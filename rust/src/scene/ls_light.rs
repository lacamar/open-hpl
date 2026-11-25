//! LS: `iLight` and its point, spot and box subclasses. The pods and their pure logic live in
//! `sl.rs`/`light.rs`; this module is what the C++ shells did around them: textures, billboards and
//! particle systems that follow the light, XML loading, flicker side effects and the spot matrices.

use super::h10::EntPub;
use super::h11::{self, Caster};
use super::light::{self, LightHost};
use super::ls_host::{self as lh, Ent, Xml, XmlError};
use super::rnd::Rnd;
use super::sl::{self, BoxPod, KIND_BOX, KIND_POINT, KIND_SPOT, LightPod, SpotPod};
use crate::math::bv::{Bv, Frustum};
use crate::math::{IDENTITY, matrix_inverse};
use crate::types::{Color, Matrixf, Vector3f};
use std::ffi::c_void;
use std::sync::atomic::{AtomicU8, Ordering};

const TEX_1D: i32 = 0;
const TEX_2D: i32 = 1;
const TEX_ANIM: i32 = 2;
const ANIM_NONE: i32 = 0;
const ANIM_LOOP: i32 = 1;
const ANIM_OSCILLATE: i32 = 2;

static LIGHT_FALLOFF: AtomicU8 = AtomicU8::new(0);
static SPOT_FALLOFF: AtomicU8 = AtomicU8::new(0);

fn lp<'a>(p: *mut LightPod) -> &'a mut LightPod {
    // SAFETY: every light pointer handed to C++ is a live `LightPod`.
    unsafe { &mut *p }
}

fn spot<'a>(p: *mut LightPod) -> &'a mut SpotPod {
    // SAFETY: spot lights are allocated as `SpotObj`, whose first field is the `SpotPod`.
    unsafe { &mut *p.cast() }
}

fn box_pod<'a>(p: *mut LightPod) -> &'a mut BoxPod {
    // SAFETY: box lights are allocated as `BoxObj`, whose first field is the `BoxPod`.
    unsafe { &mut *p.cast() }
}

fn ent(p: &LightPod) -> Ent {
    Ent {
        ctx: p.ctx as *mut c_void,
        e: p.ent as *mut EntPub,
        bv: p.bv as *mut Bv,
    }
}

fn rnd<'a>(p: &LightPod) -> &'a mut Rnd {
    // SAFETY: the renderable pod outlives the light.
    unsafe { &mut *(p.rnd as *mut Rnd) }
}

fn visible(p: &LightPod) -> bool {
    rnd(p).visible
}

fn rgb(c: Color) -> Color {
    Color {
        r: c.r,
        g: c.g,
        b: c.b,
        a: 1.0,
    }
}

/// The 1D falloff texture; `cell` is the function-local `bFalloffMissing` of the C++ constructor.
fn falloff_texture(res: usize, cell: &AtomicU8) -> usize {
    if cell.load(Ordering::Relaxed) == 0 {
        let missing = sl::ohpl_sl_hpl3_visibility();
        cell.store(if missing { 1 } else { 2 }, Ordering::Relaxed);
    }
    let missing = cell.load(Ordering::Relaxed) == 1;
    let tex = if missing {
        0
    } else {
        lh::tex_create(res, TEX_1D, b"core_falloff_linear")
    };
    cell.store(if tex == 0 { 1 } else { 2 }, Ordering::Relaxed);
    lh::tex_clamp(tex, false);
    tex
}

/// # Safety
/// `ctx` is the light shell, `e` its entity pod, `bv` its bounding volume, `rnd` its renderable pod.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_light_new(
    kind: i32,
    ctx: *mut c_void,
    e: *mut EntPub,
    bv: *mut Bv,
    rnd: *mut Rnd,
    res: usize,
) -> *mut LightPod {
    let p = sl::ohpl_sl_light_new(kind);
    let pod = lp(p);
    pod.res = res;
    pod.ctx = ctx as usize;
    pod.ent = e as usize;
    pod.bv = bv as usize;
    pod.rnd = rnd as usize;
    // SAFETY: caller contract.
    unsafe { (*e).apply_bv = false };
    pod.falloff_map = falloff_texture(res, &LIGHT_FALLOFF);
    pod.tracker = lh::host().map_or(0, |h| {
        // SAFETY: table contract.
        unsafe { (h.tracker_new)() as usize }
    });
    if kind == KIND_SPOT {
        spot(p).frustum = lh::host().map_or(0, |h| {
            // SAFETY: table contract.
            unsafe { (h.frustum_new)() as usize }
        });
        spot(p).spot_falloff_map = falloff_texture(res, &SPOT_FALLOFF);
    }
    if kind <= KIND_BOX {
        update_bounding_volume(p);
    }
    p
}

/// # Safety
/// `p` from `ohpl_ls_light_new`, not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_light_free(p: *mut LightPod) {
    let (kind, res) = (lp(p).light_type, lp(p).res);
    if kind == KIND_SPOT {
        lh::tex_destroy(res, spot(p).spot_falloff_map);
        if let (Some(h), true) = (lh::host(), spot(p).frustum != 0) {
            // SAFETY: table contract.
            unsafe { (h.frustum_delete)(spot(p).frustum as *mut Frustum) }
        }
    }
    // SAFETY: `p` is live; the counts are read again after each host call.
    let n = unsafe { sl::ohpl_sl_light_ps_count(p) };
    for i in 0..n {
        // SAFETY: as above.
        let ps = unsafe { sl::ohpl_sl_light_ps_at(p, i) };
        if let Some(h) = lh::host() {
            // SAFETY: table contract.
            unsafe { (h.ps_set_light)(ps as *mut c_void, std::ptr::null_mut()) }
        }
    }
    let pod = lp(p);
    if let (Some(h), true) = (lh::host(), pod.tracker != 0) {
        // SAFETY: table contract.
        unsafe { (h.tracker_delete)(pod.tracker as *mut c_void) }
    }
    lh::tex_destroy(res, pod.falloff_map);
    lh::tex_destroy(res, pod.gobo);
    // SAFETY: caller contract.
    unsafe { sl::ohpl_sl_light_free(p) };
}

fn bb_each(p: *mut LightPod, f: impl Fn(usize, Color)) {
    // SAFETY: `p` is live; each entry is read afresh because `f` may re-enter.
    let n = unsafe { sl::ohpl_sl_light_billboard_count(p) };
    for i in 0..n {
        let mut base = Color::default();
        // SAFETY: as above.
        let bb = unsafe { sl::ohpl_sl_light_billboard_at(p, i, &mut base) };
        f(bb, base);
    }
}

fn ps_each(p: *mut LightPod, f: impl Fn(usize)) {
    // SAFETY: `p` is live; each entry is read afresh because `f` may re-enter.
    let n = unsafe { sl::ohpl_sl_light_ps_count(p) };
    for i in 0..n {
        // SAFETY: as above.
        f(unsafe { sl::ohpl_sl_light_ps_at(p, i) });
    }
}

/// # Safety
/// `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_light_on_change_visible(p: *mut LightPod) {
    let vis = visible(lp(p));
    let Some(h) = lh::host() else { return };
    // SAFETY: table contract.
    bb_each(p, |bb, _| unsafe { (h.bb_visible)(bb as *mut c_void, vis) });
    // SAFETY: table contract.
    ps_each(p, |ps| unsafe { (h.ps_visible)(ps as *mut c_void, vis) });
}

fn on_set_diffuse(p: *mut LightPod) {
    let Some(h) = lh::host() else { return };
    let d = lp(p).diffuse;
    bb_each(p, |bb, base| {
        let c = Color {
            r: base.r * d.r,
            g: base.g * d.g,
            b: base.b * d.b,
            a: base.a,
        };
        // SAFETY: table contract.
        unsafe { (h.bb_color)(bb as *mut c_void, &c) }
    });
    let c = rgb(d);
    // SAFETY: table contract.
    ps_each(p, |ps| unsafe { (h.ps_light_color)(ps as *mut c_void, &c) });
}

fn notify_visible_change(pod: &LightPod, lit_changed: bool) {
    let r = rnd(pod);
    if !(r.visible && lit_changed && !r.callback.is_null()) {
        return;
    }
    if let Some(h) = lh::host() {
        // SAFETY: table contract.
        unsafe { (h.visible_change)(r.callback, pod.ctx as *mut c_void) }
    }
}

fn set_diffuse(p: *mut LightPod, c: &Color) {
    // SAFETY: `p` valid.
    let changed = unsafe { sl::ohpl_sl_light_set_diffuse(p, c) };
    notify_visible_change(lp(p), changed);
    on_set_diffuse(p);
}

fn set_brightness(p: *mut LightPod, b: f32) {
    // SAFETY: `p` valid.
    let changed = unsafe { sl::ohpl_sl_light_set_brightness(p, b) };
    if let Some(h) = lh::host() {
        // SAFETY: table contract.
        ps_each(p, |ps| unsafe {
            (h.ps_light_brightness)(ps as *mut c_void, b)
        });
    }
    notify_visible_change(lp(p), changed);
}

fn set_radius(p: *mut LightPod, r: f32) {
    let pod = lp(p);
    let e = ent(pod);
    if pod.light_type == KIND_SPOT {
        pod.radius = r;
        update_bounding_volume(p);
        e.updated();
        spot(p).ctl.projection_updated = true;
        return;
    }
    if pod.radius == r {
        return;
    }
    pod.radius = r;
    // SAFETY: the entity pod outlives the light.
    unsafe { (*e.e).update_bv = true };
    e.updated();
}

/// # Safety
/// `p` and `c` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_light_set_diffuse(p: *mut LightPod, c: *const Color) {
    // SAFETY: caller contract.
    set_diffuse(p, unsafe { &*c });
}

/// # Safety
/// `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_light_set_brightness(p: *mut LightPod, b: f32) {
    set_brightness(p, b);
}

/// # Safety
/// `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_light_set_radius(p: *mut LightPod, r: f32) {
    set_radius(p, r);
}

struct Updates {
    p: *mut LightPod,
}

impl LightHost for Updates {
    fn ease(&mut self, t: f32) -> f32 {
        // SAFETY: table contract.
        lh::host().map_or(t, |h| unsafe { (h.ease)(lp(self.p).ctx as *mut c_void, t) })
    }

    fn set_brightness(&mut self, v: f32) {
        set_brightness(self.p, v);
    }

    fn set_radius(&mut self, v: f32) {
        set_radius(self.p, v);
    }

    fn set_diffuse(&mut self, c: Color) {
        set_diffuse(self.p, &c);
    }

    fn assign_diffuse(&mut self, c: Color) {
        lp(self.p).diffuse = c;
    }

    fn raw_radius(&mut self, v: f32) {
        lp(self.p).radius = v;
    }

    fn flicker_sound(&mut self, on: bool) {
        let pod = lp(self.p);
        let mut len = 0;
        // SAFETY: `p` is live; the string lives until the next `set_flicker`.
        let file = unsafe { sl::ohpl_sl_light_string(self.p, if on { 0 } else { 2 }, &mut len) };
        if let Some(h) = lh::host() {
            // SAFETY: table contract.
            unsafe {
                (h.world_sound)(
                    pod.world as *mut c_void,
                    pod.ctx as *mut c_void,
                    on,
                    file,
                    len,
                )
            }
        }
    }

    fn flicker_event(&mut self, on: bool) {
        let pod = lp(self.p);
        let mut len = 0;
        // SAFETY: as `flicker_sound`.
        let ps = unsafe { sl::ohpl_sl_light_string(self.p, if on { 1 } else { 3 }, &mut len) };
        if len == 0 {
            return;
        }
        if let Some(h) = lh::host() {
            // SAFETY: table contract.
            unsafe { (h.world_particle)(pod.world as *mut c_void, pod.ctx as *mut c_void, ps, len) }
        }
    }
}

pub(crate) unsafe fn ohpl_ls_light_update_light(p: *mut LightPod, dt: f32) {
    unsafe {
        let mut live = (*p).live();
        let (on, off) = {
            let s = &sl::rest(p).strings;
            (
                s.first().is_some_and(|v| !v.is_empty()),
                s.get(2).is_some_and(|v| !v.is_empty()),
            )
        };
        light::update_light(&mut (*p).anim, &mut live, on, off, &mut Updates { p }, dt);
    }
}

/// # Safety
/// `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_light_update_logic(p: *mut LightPod, dt: f32) {
    super::ls_dump::dump_light(p);
    // SAFETY: `p` valid.
    unsafe { ohpl_ls_light_update_light(p, dt) };
    let pod = lp(p);
    if pod.anim.fade_time > 0.0 || pod.anim.flickering {
        // SAFETY: the entity pod outlives the light.
        unsafe { (*ent(pod).e).update_bv = true };
    }
    if pod.light_type == KIND_BOX {
        // SAFETY: box lights are `BoxPod`s.
        unsafe { sl::ohpl_sl_box_fade_step(p.cast(), dt) };
    }
}

fn update_bounding_volume(p: *mut LightPod) {
    let pod = lp(p);
    let e = ent(pod);
    match pod.light_type {
        KIND_POINT => {
            let r = pod.radius * 2.0;
            e.bv().set_size(Vector3f { x: r, y: r, z: r });
            e.bv().set_position(e.pos());
        }
        KIND_BOX => {
            e.bv().set_size(box_pod(p).size);
            e.bv().set_position(e.pos());
        }
        KIND_SPOT => {
            let f = frustum(p);
            // SAFETY: the spot frustum is live while the light is.
            *e.bv() = unsafe { (*f).bounding_volume.bv.clone() };
        }
        _ => {}
    }
}

/// `GetBoundingVolume`: refreshes the volume (and clips it to the mask box) when stale.
///
/// # Safety
/// `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_light_bounds(p: *mut LightPod) {
    let pod = lp(p);
    let e = ent(pod);
    // SAFETY: the entity pod outlives the light.
    if !unsafe { (*e.e).update_bv } {
        return;
    }
    update_bounding_volume(p);
    if pod.mask_box {
        let bv = e.bv();
        bv.update_size();
        let (min, max) =
            h11::light_mask_bounds(bv.world_min, bv.world_max, pod.mask_center, pod.mask_size);
        bv.set_local_min_max(min, max);
        bv.set_transform(&IDENTITY);
    }
    // SAFETY: as above.
    unsafe { (*e.e).update_bv = false };
}

fn set_texture(res: usize, slot: &mut usize, tex: usize) {
    lh::tex_destroy(res, *slot);
    *slot = tex;
}

pub(crate) unsafe fn ohpl_ls_light_set_falloff_map(p: *mut LightPod, tex: usize) {
    let pod = lp(p);
    set_texture(pod.res, &mut pod.falloff_map, tex);
    lh::tex_clamp(tex, false);
}

/// # Safety
/// `p` valid; `tex` null or a texture the light now owns.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_light_set_gobo(p: *mut LightPod, tex: usize) {
    let pod = lp(p);
    set_texture(pod.res, &mut pod.gobo, tex);
    lh::tex_clamp(tex, true);
}

pub(crate) unsafe fn ohpl_ls_spot_set_falloff_map(p: *mut LightPod, tex: usize) {
    let res = lp(p).res;
    set_texture(res, &mut spot(p).spot_falloff_map, tex);
    lh::tex_clamp(tex, false);
}

fn casters(objs: *const usize, n: i32) -> Vec<Caster> {
    let n = usize::try_from(n).unwrap_or(0);
    let list = if n == 0 {
        &[]
    } else {
        // SAFETY: caller contract: `n` object addresses.
        unsafe { std::slice::from_raw_parts(objs, n) }
    };
    list.iter()
        .map(|&object| {
            let (mut count, mut is_static) = (0, false);
            if let Some(h) = lh::host() {
                // SAFETY: table contract.
                unsafe { (h.renderable_info)(object as *mut c_void, &mut count, &mut is_static) }
            }
            Caster {
                object,
                count,
                is_static,
            }
        })
        .collect()
}

/// # Safety
/// `p` valid; `objs` holds `n` renderable addresses.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_light_casters_unchanged(
    p: *mut LightPod,
    objs: *const usize,
    n: i32,
) -> bool {
    let list = casters(objs, n);
    // SAFETY: `p` valid; `list` holds `list.len()` entries.
    unsafe { sl::ohpl_sl_light_cache_unchanged(p, list.as_ptr(), list.len() as i32) }
}

/// # Safety
/// `p` valid; `objs` holds `n` renderable addresses.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_light_casters_set(p: *mut LightPod, objs: *const usize, n: i32) {
    let list = casters(objs, n);
    // SAFETY: `p` valid; `list` holds `list.len()` entries.
    unsafe { sl::ohpl_sl_light_cache_set(p, list.as_ptr(), list.len() as i32) };
}

/// # Safety
/// `p` valid; `strings`/`lens` hold the four flicker strings.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_light_set_flicker(
    p: *mut LightPod,
    off_color: *const Color,
    off_radius: f32,
    on_min: f32,
    on_max: f32,
    off_min: f32,
    off_max: f32,
    fade: bool,
    on_fade_min: f32,
    on_fade_max: f32,
    off_fade_min: f32,
    off_fade_max: f32,
    strings: *const [*const u8; 4],
    lens: *const [usize; 4],
) {
    let cfg = light::FlickerCfg {
        // SAFETY: caller contract.
        off_color: unsafe { *off_color },
        off_radius,
        on_min_length: on_min,
        on_max_length: on_max,
        off_min_length: off_min,
        off_max_length: off_max,
        on_fade_min_length: on_fade_min,
        on_fade_max_length: on_fade_max,
        off_fade_min_length: off_fade_min,
        off_fade_max_length: off_fade_max,
        fade,
    };
    // SAFETY: caller contract.
    unsafe { sl::ohpl_sl_light_set_flicker(p, &cfg, strings, lens) };
}

/// # Safety
/// `p` valid; `bb` a billboard; `base` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_light_attach_billboard(
    p: *mut LightPod,
    bb: usize,
    base: *const Color,
) {
    let pod = lp(p);
    // SAFETY: caller contract.
    let base = unsafe { *base };
    let d = pod.diffuse;
    let color = Color {
        r: base.r * d.r,
        g: base.g * d.g,
        b: base.b * d.b,
        a: base.a,
    };
    let lit = if pod.light_type == KIND_BOX {
        pod.is_lit() && visible(pod)
    } else {
        // SAFETY: `p` valid.
        unsafe { sl::ohpl_sl_light_is_visible(p, visible(pod)) }
    };
    if let Some(h) = lh::host() {
        // SAFETY: table contract.
        unsafe {
            (h.bb_color)(bb as *mut c_void, &color);
            (h.bb_visible)(bb as *mut c_void, lit);
        }
    }
    // SAFETY: `p` valid.
    unsafe { sl::ohpl_sl_light_attach_billboard(p, bb, &base) };
}

/// # Safety
/// `p` valid; `ps` a particle system.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_light_attach_ps(p: *mut LightPod, ps: usize) {
    let pod = lp(p);
    let c = rgb(pod.diffuse);
    let (brightness, vis, ctx) = (pod.brightness, visible(pod), pod.ctx);
    if let Some(h) = lh::host() {
        // SAFETY: table contract; `leave_light` re-enters `ohpl_ls_light_remove_ps` of the old light.
        unsafe {
            (h.ps_light_color)(ps as *mut c_void, &c);
            (h.ps_light_brightness)(ps as *mut c_void, brightness);
            (h.ps_visible)(ps as *mut c_void, vis);
            (h.ps_leave_light)(ps as *mut c_void);
            (h.ps_set_light)(ps as *mut c_void, ctx as *mut c_void);
        }
    }
    // SAFETY: `p` valid.
    unsafe { sl::ohpl_sl_light_attach_ps(p, ps) };
}

/// # Safety
/// `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_light_remove_ps(p: *mut LightPod, ps: usize) {
    // SAFETY: `p` valid.
    if !unsafe { sl::ohpl_sl_light_remove_ps(p, ps) } {
        return;
    }
    if let Some(h) = lh::host() {
        // SAFETY: table contract.
        unsafe { (h.ps_set_light)(ps as *mut c_void, std::ptr::null_mut()) }
    }
}

/// # Safety
/// `p` valid; `file` addresses `len` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_light_load(p: *mut LightPod, file: *const u8, len: usize) {
    let pod = lp(p);
    // SAFETY: caller contract.
    let file = unsafe { std::slice::from_raw_parts(file, len) };
    let name = String::from_utf8_lossy(file).into_owned();
    let xml = match Xml::open(pod.res, file) {
        Ok(x) => x,
        Err(XmlError::Unreadable) => return,
        Err(XmlError::Load) => return lh::log(true, &format!("Couldn't load file '{name}'\n")),
        Err(XmlError::NoMain) => {
            return lh::log(true, &format!("Cannot find main element in {name}\n"));
        }
        Err(XmlError::NotFound) => return lh::log(true, &format!("Couldn't find file '{name}'\n")),
    };
    pod.cast_shadows = xml.boolean("CastsShadows", pod.cast_shadows);
    pod.diffuse.a = xml.float("Specular", pod.diffuse.a);
    let tex = lh::tex_create(pod.res, TEX_1D, &xml.string("FalloffImage"));
    if tex != 0 {
        // SAFETY: `p` valid; the light takes the texture.
        unsafe { ohpl_ls_light_set_falloff_map(p, tex) };
    }
    if pod.light_type != KIND_SPOT {
        return;
    }
    let image = xml.string("ProjectionImage");
    let mode = match xml
        .string_or("ProjectionAnimMode", "None")
        .to_ascii_lowercase()
        .as_slice()
    {
        b"loop" => ANIM_LOOP,
        b"oscillate" => ANIM_OSCILLATE,
        _ => ANIM_NONE,
    };
    let frame_time = xml.float("ProjectionFrameTime", 1.0);
    let gobo = if mode != ANIM_NONE {
        let t = lh::tex_create(pod.res, TEX_ANIM, &image);
        if let (Some(h), true) = (lh::host(), t != 0) {
            // SAFETY: table contract.
            unsafe { (h.tex_anim)(t as *mut c_void, mode, frame_time) }
        }
        t
    } else {
        lh::tex_create(pod.res, TEX_2D, &image)
    };
    if gobo != 0 {
        // SAFETY: `p` valid; the light takes the texture.
        unsafe { ohpl_ls_light_set_gobo(p, gobo) };
    }
    let s = spot(p);
    s.aspect = xml.float("Aspect", s.aspect);
    s.near_clip = xml.float("NearClipPlane", s.near_clip);
}

/// # Safety
/// `p` valid box light.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_box_fade_set(
    p: *mut LightPod,
    name: *const u8,
    len: usize,
    time: f32,
) {
    // SAFETY: caller contract.
    if unsafe { sl::ohpl_sl_box_fade_set(p.cast(), name, len, time) } {
        // SAFETY: `p` valid.
        unsafe { ohpl_ls_light_update_logic(p, 0.0) };
    }
}

fn view<'a>(p: *mut LightPod) -> &'a Matrixf {
    let s = spot(p);
    let e = ent(lp(p));
    let count = e.count();
    if s.ctl.view_stale(count) {
        s.ctl.view_count = count;
        // SAFETY: the entity matrix is live.
        s.mtx_view = matrix_inverse(unsafe { &*e.world() });
    }
    &s.mtx_view
}

fn projection<'a>(p: *mut LightPod) -> &'a Matrixf {
    let s = spot(p);
    if s.ctl.projection_stale() {
        s.mtx_projection = light::spot_projection(
            s.fov,
            s.aspect,
            s.near_clip,
            sl::ohpl_sl_spot_shadow_near(),
            lp(p).radius,
        );
    }
    &s.mtx_projection
}

pub(super) fn view_proj<'a>(p: *mut LightPod) -> &'a Matrixf {
    let s = spot(p);
    let count = ent(lp(p)).count();
    if s.ctl.view_proj_stale(count) {
        let (proj, v) = (*projection(p), *view(p));
        s.mtx_view_proj = light::spot_view_proj(&proj, &v);
        s.ctl.view_proj_done(count);
    }
    &s.mtx_view_proj
}

fn frustum(p: *mut LightPod) -> *mut Frustum {
    let s = spot(p);
    let e = ent(lp(p));
    let count = e.count();
    let f = s.frustum as *mut Frustum;
    if s.ctl.frustum_stale(count) {
        let (proj, v) = (*projection(p), *view(p));
        let near = light::spot_near(s.near_clip, sl::ohpl_sl_spot_shadow_near());
        // SAFETY: the spot frustum is live while the light is.
        unsafe {
            (*f).setup_perspective(
                &proj,
                &v,
                lp(p).radius,
                near,
                s.fov,
                s.aspect,
                e.pos(),
                false,
                None,
                false,
            );
        }
        s.ctl.frustum_done(count);
    }
    f
}

/// # Safety
/// `p` valid spot light.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_spot_view(p: *mut LightPod) -> *const Matrixf {
    view(p)
}

/// # Safety
/// `p` valid spot light.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_spot_projection(p: *mut LightPod) -> *const Matrixf {
    projection(p)
}

/// # Safety
/// `p` valid spot light.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_spot_view_proj(p: *mut LightPod) -> *const Matrixf {
    view_proj(p)
}

/// # Safety
/// `p` valid spot light.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_spot_frustum(p: *mut LightPod) -> *mut Frustum {
    frustum(p)
}

/// # Safety
/// `p` valid spot light.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_spot_set_fov(p: *mut LightPod, fov: f32) {
    let s = spot(p);
    s.fov = fov;
    s.ctl.projection_updated = true;
    let (t, c) = light::spot_half_fov(fov);
    s.tan_half_fov = t;
    s.cos_half_fov = c;
}

/// # Safety
/// `p` valid spot light; `other` a volume.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_spot_collides_bv(p: *mut LightPod, other: *mut Bv) -> bool {
    // SAFETY: caller contract.
    unsafe { ohpl_ls_light_bounds(p) };
    let e = ent(lp(p));
    let (mine, theirs) = (e.bv(), unsafe { &mut *other });
    mine.update_size();
    theirs.update_size();
    let (a, b) = (&*mine, &*theirs);
    let apart = a.world_max.x < b.world_min.x
        || a.world_max.y < b.world_min.y
        || a.world_max.z < b.world_min.z
        || b.world_max.x < a.world_min.x
        || b.world_max.y < a.world_min.y
        || b.world_max.z < a.world_min.z;
    if apart {
        return false;
    }
    // SAFETY: the spot frustum is live while the light is.
    unsafe { (*frustum(p)).collide_bv(theirs) != 1 }
}

/// # Safety
/// `p` valid spot light; `other` a frustum.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_spot_collides_frustum(
    p: *mut LightPod,
    other: *mut Frustum,
) -> bool {
    // SAFETY: caller contract; the spot frustum is live while the light is.
    unsafe { (*other).collide_frustum(&*frustum(p)) != 1 }
}

pub(super) fn spot_view_proj<'a>(p: *mut LightPod) -> &'a Matrixf {
    view_proj(p)
}

pub(super) fn is_visible(p: *mut LightPod) -> bool {
    let pod = lp(p);
    if pod.light_type == KIND_BOX {
        pod.is_lit() && visible(pod)
    } else {
        // SAFETY: `p` valid.
        unsafe { sl::ohpl_sl_light_is_visible(p, visible(pod)) }
    }
}

pub(super) fn entity_bv<'a>(p: *mut LightPod) -> &'a mut Bv {
    ent(lp(p)).bv()
}

pub(super) fn entity_count(p: *mut LightPod) -> i32 {
    ent(lp(p)).count()
}
