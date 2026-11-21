//! `OHPL_SL_DUMP`: state of lights, billboards, beams, lens flares and fog areas after a world loads,
//! plus a scripted fade/flicker drive of each light. Compared against the recorded C++ output by the
//! gate, so every number is written as the bit pattern of the float.

use super::ls_host::{self as lh, DumpInfo};
use super::ls_light;
use super::sl::{self, BbPod, BeamPod, FlarePod, FogPod, KIND_BOX, KIND_SPOT, LightPod, SpotPod};
use crate::math::bv::Bv;
use crate::math::libm::c_srand;
use crate::types::{Color, Matrixf, Vector3f};
use std::collections::HashSet;
use std::ffi::{OsString, c_void};
use std::io::Write;
use std::sync::{Mutex, OnceLock};

const LIGHT: i32 = 0;
const BILLBOARD: i32 = 1;
const BEAM: i32 = 2;
const FOG: i32 = 3;
const FLARE: i32 = 4;
const NAME_TEXTURE: i32 = 0;
const NAME_MATERIAL: i32 = 1;
const NAME_WORLD: i32 = 2;

#[derive(Default)]
struct Seen {
    worlds: HashSet<usize>,
    driven: HashSet<usize>,
}

static PATH: OnceLock<Option<OsString>> = OnceLock::new();
static SEEN: OnceLock<Mutex<Seen>> = OnceLock::new();

struct Out(Vec<u8>);

impl Out {
    fn f(&mut self, x: f32) {
        let _ = write!(self.0, " {:08x}", x.to_bits());
    }

    fn s(&mut self, text: &str) {
        self.0.extend_from_slice(text.as_bytes());
    }

    fn raw(&mut self, bytes: &[u8]) {
        self.0.extend_from_slice(bytes);
    }

    fn i(&mut self, x: i32) {
        let _ = write!(self.0, " {x}");
    }

    fn col(&mut self, c: Color) {
        for v in [c.r, c.g, c.b, c.a] {
            self.f(v);
        }
    }

    fn v3(&mut self, v: Vector3f) {
        for x in [v.x, v.y, v.z] {
            self.f(x);
        }
    }

    fn mtx(&mut self, m: &Matrixf) {
        for x in m.m.as_flattened() {
            self.f(*x);
        }
    }
}

fn name_of(kind: i32, obj: usize) -> Vec<u8> {
    if obj == 0 {
        return Vec::new();
    }
    let Some(h) = lh::host() else {
        return Vec::new();
    };
    let mut len = 0;
    // SAFETY: table contract; the name lives as long as its owner.
    let p = unsafe { (h.obj_name)(kind, obj as *mut c_void, &mut len) };
    if p.is_null() {
        return Vec::new();
    }
    // SAFETY: the host returns `len` readable bytes.
    unsafe { std::slice::from_raw_parts(p, len) }.to_vec()
}

fn info(kind: i32, obj: usize) -> DumpInfo {
    let mut out = DumpInfo::default();
    if let Some(h) = lh::host() {
        // SAFETY: table contract.
        unsafe { (h.dump_info)(kind, obj as *mut c_void, &mut out) }
    }
    out
}

fn bounds(kind: i32, obj: usize) -> (Vector3f, Vector3f) {
    let bv = lh::host().map_or(std::ptr::null_mut::<Bv>(), |h| {
        // SAFETY: table contract.
        unsafe { (h.obj_bv)(kind, obj as *mut c_void) }
    });
    if bv.is_null() {
        return (Vector3f::default(), Vector3f::default());
    }
    // SAFETY: the host returns the live bounding volume.
    let bv = unsafe { &mut *bv };
    bv.update_size();
    (bv.world_min, bv.world_max)
}

fn light_live(out: &mut Out, tag: &str, step: i32, p: *mut LightPod, name: &[u8]) {
    // SAFETY: `p` is the live light pod.
    let pod = unsafe { &*p };
    out.s("ll ");
    out.raw(name);
    out.s(&format!(" {tag} {step}"));
    out.col(pod.diffuse);
    out.f(pod.radius);
    // The format string is " %08x %d %d %d %d": the float comes without a leading space issue.
    out.i(i32::from(pod.anim.fade_time != 0.0));
    out.i(i32::from(pod.anim.flickering));
    out.i(i32::from(ls_light::is_visible(p)));
    out.i(0);
    out.f(pod.brightness);
    out.s("\n");
}

fn dump_light_state(out: &mut Out, p: *mut LightPod, obj: usize) {
    // SAFETY: `p` is the live light pod.
    let pod = unsafe { &*p };
    let inf = info(LIGHT, obj);
    let name = name_of_info(&inf);
    let dyn_ = pod.anim.fade_time != 0.0 || pod.anim.flickering;
    out.s("light ");
    out.raw(&name);
    let vis = if dyn_ {
        -1
    } else {
        i32::from(ls_light::is_visible(p))
    };
    out.s(&format!(
        " {} type {} vis {vis} dyn {}",
        inf.uid,
        pod.light_type,
        i32::from(dyn_)
    ));
    if dyn_ {
        out.s(" -");
    } else {
        out.col(pod.diffuse);
    }
    out.col(pod.default_diffuse);
    out.col(pod.specular);
    out.s(&format!(
        " cs {} aff {} res {}",
        i32::from(pod.cast_shadows),
        pod.shadow_casters_affected,
        pod.shadow_res
    ));
    out.s(&format!(
        " {:08x} {}",
        pod.shadow_blur.to_bits(),
        i32::from(pod.occlusion_cull)
    ));
    out.f(pod.shadow_bias_mul);
    out.f(pod.shadow_slope_bias_mul);
    out.f(pod.shadow_distance);
    out.s(&format!(
        " {:08x}",
        if dyn_ { 0 } else { pod.radius.to_bits() }
    ));
    out.f(pod.falloff_pow);
    out.f(pod.spot_falloff_pow);
    out.f(pod.translucency);
    out.s(&format!(
        " {:08x}",
        if dyn_ { 0 } else { pod.brightness.to_bits() }
    ));
    out.f(pod.default_brightness);
    out.s(&format!(
        " mask {} {} {} {} {}",
        pod.mask_id,
        i32::from(pod.gobo_specular),
        i32::from(pod.cast_diffuse),
        i32::from(pod.cast_specular),
        i32::from(pod.mask_box)
    ));
    out.v3(pod.mask_center);
    out.v3(pod.mask_size);
    out.f(pod.source_radius);
    // SAFETY: `p` is live; the strings live until the next `set_flicker`.
    let strings: Vec<Vec<u8>> = (0..4)
        .map(|i| {
            let mut len = 0;
            // SAFETY: as above.
            let s = unsafe { sl::ohpl_sl_light_string(p, i, &mut len) };
            // SAFETY: the string has `len` bytes.
            unsafe { std::slice::from_raw_parts(s, len) }.to_vec()
        })
        .collect();
    out.s(&format!(" flick {} '", i32::from(pod.anim.flickering)));
    for (n, i) in [0usize, 2, 1, 3].into_iter().enumerate() {
        if n > 0 {
            out.s(" '");
        }
        out.raw(strings.get(i).map_or(&[][..], Vec::as_slice));
        out.s("'");
    }
    let a = &pod.anim;
    out.s(" ");
    for (n, x) in [
        a.flicker_on_min_length,
        a.flicker_off_min_length,
        a.flicker_on_max_length,
        a.flicker_off_max_length,
        a.flicker_off_radius,
    ]
    .into_iter()
    .enumerate()
    {
        out.s(&format!(
            "{}{:08x}",
            if n > 0 { " " } else { "" },
            x.to_bits()
        ));
    }
    out.i(i32::from(a.flicker_fade));
    out.f(a.flicker_on_fade_min_length);
    out.f(a.flicker_on_fade_max_length);
    out.f(a.flicker_off_fade_min_length);
    // GetFlickerOffFadeMaxLength returns the "on" value.
    out.f(a.flicker_on_fade_max_length);
    out.col(a.flicker_off_color);
    out.col(a.flicker_on_color);
    out.f(a.flicker_on_radius);
    out.col(a.dest_col);
    out.s(&format!(" {:08x} '", a.dest_radius.to_bits()));
    out.raw(&name_of(NAME_TEXTURE, pod.falloff_map));
    out.s("' '");
    out.raw(&name_of(NAME_TEXTURE, pod.gobo));
    out.s("'");
    out.s(&format!(" tracker {}", i32::from(pod.tracker != 0)));
    if !dyn_ {
        // SAFETY: `p` is live.
        unsafe { ls_light::ohpl_ls_light_bounds(p) };
        let bv = ls_light::entity_bv(p);
        bv.update_size();
        out.v3(bv.world_min);
        out.v3(bv.world_max);
    }
    if pod.light_type == KIND_SPOT {
        // SAFETY: spot lights are `SpotPod`s.
        let s: &SpotPod = unsafe { &*p.cast() };
        out.s(&format!(" spot {:08x}", s.fov.to_bits()));
        out.f(s.aspect);
        out.f(s.near_clip);
        out.f(s.tan_half_fov);
        out.f(s.cos_half_fov);
        out.s(" '");
        out.raw(&name_of(NAME_TEXTURE, s.spot_falloff_map));
        out.s("'");
        if !dyn_ {
            let m = *ls_light::spot_view_proj(p);
            out.mtx(&m);
        }
    } else if pod.light_type == KIND_BOX {
        // SAFETY: box lights are `BoxPod`s.
        let b: &sl::BoxPod = unsafe { &*p.cast() };
        out.s(&format!(
            " box {} {} {:08x}",
            b.blend,
            b.prio,
            b.weight.to_bits()
        ));
        out.f(b.bevel);
        out.f(b.falloff_pow);
        out.i(i32::from(b.use_sh));
        out.v3(b.size);
        out.v3(b.probe_offset);
        out.col(b.ambient_sky);
        out.col(b.ambient_ground);
        for v in b.bands {
            out.v3(v);
        }
    }
    out.s("\n");
}

fn name_of_info(i: &DumpInfo) -> Vec<u8> {
    if i.name.is_null() {
        return Vec::new();
    }
    // SAFETY: the host returns `name_len` readable bytes owned by the entity.
    unsafe { std::slice::from_raw_parts(i.name, i.name_len) }.to_vec()
}

fn drive_light(out: &mut Out, p: *mut LightPod, obj: usize) {
    // SAFETY: `p` is the live light pod.
    let pod = unsafe { &mut *p };
    if pod.anim.fade_time != 0.0 || pod.anim.flickering {
        return;
    }
    let name = name_of_info(&info(LIGHT, obj));
    let (diffuse, radius, brightness) = (pod.diffuse, pod.radius, pod.brightness);
    c_srand(12345);
    light_live(out, "start", 0, p, &name);
    let target = Color {
        r: 0.2,
        g: 0.4,
        b: 0.6,
        a: 0.1,
    };
    // SAFETY: `p` is live for every call below.
    unsafe { sl::ohpl_sl_light_fade_to(p, &target, radius * 0.5 + 1.0, 1.0) };
    for i in 1..=12 {
        // SAFETY: as above.
        unsafe { ls_light::ohpl_ls_light_update_light(p, 0.1) };
        light_live(out, "fade", i, p, &name);
    }
    // SAFETY: as above.
    unsafe { sl::ohpl_sl_light_fade_brightness_to(p, 0.25, 0.5) };
    for i in 1..=6 {
        // SAFETY: as above.
        unsafe { ls_light::ohpl_ls_light_update_light(p, 0.1) };
        light_live(out, "bright", i, p, &name);
    }
    set_flicker(p, true, [0.05, 0.1, 0.05, 0.1]);
    // SAFETY: `p` live.
    unsafe { (*p).anim.flickering = true };
    for i in 1..=40 {
        // SAFETY: as above.
        unsafe { ls_light::ohpl_ls_light_update_light(p, 0.05) };
        light_live(out, "flicker", i, p, &name);
    }
    set_flicker(p, false, [0.0; 4]);
    for i in 1..=20 {
        // SAFETY: as above.
        unsafe { ls_light::ohpl_ls_light_update_light(p, 0.05) };
        light_live(out, "flicker2", i, p, &name);
    }

    // SAFETY: as above.
    unsafe {
        (*p).anim.flickering = false;
        (*p).anim.fade_time = 0.0;
        ls_light::ohpl_ls_light_set_diffuse(p, &diffuse);
        ls_light::ohpl_ls_light_set_radius(p, radius);
        ls_light::ohpl_ls_light_set_brightness(p, brightness);
        sl::ohpl_sl_light_cache_clear(p);
    }
    let count = ls_light::entity_count(p);
    // SAFETY: `p` live.
    unsafe { sl::ohpl_sl_light_cache_add(p, obj, count) };
    // SAFETY: `p` live.
    let valid = unsafe { sl::ohpl_sl_light_cache_valid(p, obj, count) };
    let list = [obj];
    // SAFETY: `list` holds one renderable address.
    let first = unsafe { ls_light::ohpl_ls_light_casters_unchanged(p, list.as_ptr(), 1) };
    // SAFETY: as above.
    unsafe { ls_light::ohpl_ls_light_casters_set(p, list.as_ptr(), 1) };
    // SAFETY: as above.
    let second = unsafe { ls_light::ohpl_ls_light_casters_unchanged(p, list.as_ptr(), 1) };
    out.s("cache ");
    out.raw(&name);
    out.s(&format!(
        " {} {} {}\n",
        i32::from(valid),
        i32::from(first),
        i32::from(second)
    ));
    // SAFETY: `p` live.
    unsafe { sl::ohpl_sl_light_cache_clear(p) };
}

fn set_flicker(p: *mut LightPod, fade: bool, f: [f32; 4]) {
    let off = Color::default();
    let empty = [b"".as_ptr(); 4];
    let lens = [0usize; 4];
    // SAFETY: `p` live; the strings are empty.
    unsafe {
        ls_light::ohpl_ls_light_set_flicker(
            p, &off, 0.5, 0.2, 0.5, 0.1, 0.3, fade, f[0], f[1], f[2], f[3], &empty, &lens,
        );
    }
}

fn list(world: usize, kind: i32) -> Vec<usize> {
    let mut buf = vec![0usize; 4096];
    let n = lh::host().map_or(0, |h| {
        // SAFETY: table contract; `buf` holds `cap` entries.
        unsafe { (h.dump_list)(world as *mut c_void, kind, buf.as_mut_ptr(), buf.len()) }
    });
    buf.truncate(n.min(4096));
    buf
}

fn dump_world(out: &mut Out, world: usize) {
    out.s("world ");
    out.raw(&name_of(NAME_WORLD, world));
    out.s("\n");

    for obj in list(world, LIGHT) {
        let p = info(LIGHT, obj).pod as *mut LightPod;
        dump_light_state(out, p, obj);
    }

    for obj in list(world, BILLBOARD) {
        let inf = info(BILLBOARD, obj);
        // SAFETY: the host returns the live billboard pod.
        let b: &BbPod = unsafe { &*(inf.pod as *const BbPod) };
        out.s("billboard ");
        out.raw(&name_of_info(&inf));
        out.s(&format!(
            " {} type {} vis {}",
            inf.uid,
            b.kind,
            i32::from(inf.visible)
        ));
        out.f(b.size.x);
        out.f(b.size.y);
        out.v3(b.axis);
        out.col(b.color);
        out.f(b.forward_offset);
        out.f(b.brightness);
        out.f(b.halo_alpha);
        out.s(&format!(" halo {}", i32::from(b.is_halo)));
        out.v3(b.halo_source_size);
        out.s(" mat '");
        out.raw(&name_of(NAME_MATERIAL, b.material));
        out.s(&format!("' oq {}", i32::from(b.is_halo)));
        let (min, max) = bounds(BILLBOARD, obj);
        out.v3(min);
        out.v3(max);
        out.s("\n");
    }

    for obj in list(world, BEAM) {
        let inf = info(BEAM, obj);
        let p = inf.pod as *mut BeamPod;
        // SAFETY: the host returns the live beam pod.
        unsafe { super::ls_bb::ohpl_ls_beam_update_frame(p) };
        // SAFETY: as above.
        let b: &BeamPod = unsafe { &*p };
        let mut len = 0;
        // SAFETY: as above; the file name lives until the next `set_file`.
        let file = unsafe {
            let s = sl::ohpl_sl_beam_file(p, &mut len);
            std::slice::from_raw_parts(s, len).to_vec()
        };
        out.s("beam ");
        out.raw(&name_of_info(&inf));
        out.s(&format!(" {} file '", inf.uid));
        out.raw(&file);
        out.s(&format!("' vis {}", i32::from(inf.visible)));
        out.f(b.size.x);
        out.f(b.size.y);
        out.col(b.color);
        out.col(super::ls_bb::beam_end_color(p));
        out.i(i32::from(b.tile_height));
        out.i(i32::from(b.multiply_alpha));
        out.v3(b.axis);
        out.v3(b.mid);
        out.s(" mat '");
        out.raw(&name_of(NAME_MATERIAL, b.material));
        out.s("'");
        let (min, max) = bounds(BEAM, obj);
        out.v3(min);
        out.v3(max);
        let vb = lh::Vb(b.vtx as *mut c_void);
        for elem in [lh::E_POS, lh::E_COLOR0] {
            // SAFETY: both arrays hold four vertices of four floats.
            for x in unsafe { vb.floats(elem, 16) }.iter() {
                out.f(*x);
            }
        }
        out.s("\n");
    }

    for obj in list(world, FOG) {
        let inf = info(FOG, obj);
        // SAFETY: the host returns the live fog pod.
        let pod = inf.pod as *mut FogPod;
        let p: &FogPod = unsafe { &*pod };
        out.s("fogarea ");
        out.raw(&name_of_info(&inf));
        out.s(&format!(" {}", inf.uid));
        out.col(p.color);
        out.v3(p.size);
        out.s(&format!(" {:08x}", p.brightness.to_bits()));
        out.f(p.start);
        out.f(p.end);
        out.f(p.falloff_exp);
        out.i(i32::from(p.backside_outside));
        out.i(i32::from(p.backside_inside));
        out.i(i32::from(p.underwater));
        out.i(i32::from(p.skybox));
        out.f(p.noise_strength);
        out.f(p.noise_size);
        out.v3(p.noise_turbulence);
        out.i(i32::from(p.lighten));
        out.i(i32::from(p.height_based));
        out.i(i32::from(p.exponential));
        out.f(p.density);
        out.f(p.height_density);
        out.f(p.height_horizon);
        let world_mtx = lh::ent_world_of(obj);
        // SAFETY: the pod is live and the world matrix is the entity's.
        let m = unsafe { *sl::ohpl_sl_fog_model(pod, &world_mtx) };
        out.mtx(&m);
        out.s("\n");
    }

    for obj in list(world, FLARE) {
        let inf = info(FLARE, obj);
        // SAFETY: the host returns the live flare pod.
        let p: &FlarePod = unsafe { &*(inf.pod as *const FlarePod) };
        out.s("lensflare ");
        out.raw(&name_of_info(&inf));
        out.s(&format!(" {} vis {}", inf.uid, i32::from(inf.visible)));
        out.v3(p.source_size);
        out.s(&format!(" {:08x}", p.outer_fov.to_bits()));
        out.f(p.inner_fov);
        out.s(&format!(
            " {} {} {} {}",
            p.atlas_grid.x, p.atlas_grid.y, p.multi_iris_seed, p.multi_iris_count
        ));
        for x in [
            p.range_max.x,
            p.range_max.y,
            p.range_min.x,
            p.range_min.y,
            p.glare_brightness,
            p.glare_fov,
            p.glare_range.x,
            p.glare_range.y,
            p.size_by_dist,
            p.brightness,
        ] {
            out.f(x);
        }
        out.i(i32::from(p.use_parent_mesh));
        out.i(i32::from(p.shrink_when_occluded));
        out.i(i32::from(p.mul_multi_iris));
        for t in 0..3 {
            let active = p.active.get(t).copied().unwrap_or(false);
            out.s(&format!(
                " t{t} {} {}",
                i32::from(active),
                i32::from(p.is_active(t))
            ));
            let s = p.size.get(t).copied().unwrap_or_default();
            out.f(s.x);
            out.f(s.y);
            out.col(p.color.get(t).copied().unwrap_or_default());
        }
        // SAFETY: `p` is a live flare pod.
        let (first, any) = unsafe {
            (
                sl::ohpl_sl_flare_first_active(p),
                sl::ohpl_sl_flare_any_active(p),
            )
        };
        out.s(&format!(" first {first} any {}\n", i32::from(any)));
    }
}

/// Called at the start of every light's `UpdateLogic`; dumps the world once and drives each light once.
pub fn dump_light(p: *mut LightPod) {
    let path = PATH.get_or_init(|| std::env::var_os("OHPL_SL_DUMP"));
    let Some(path) = path else { return };
    // SAFETY: `p` is the live light pod.
    let (world, obj) = unsafe { ((*p).world, (*p).ctx) };
    if world == 0 {
        return;
    }
    let Ok(mut file) = std::fs::OpenOptions::new()
        .append(true)
        .create(true)
        .open(path)
    else {
        return;
    };
    let seen = SEEN.get_or_init(|| Mutex::new(Seen::default()));
    let (new_world, new_light) = match seen.lock() {
        Ok(mut s) => (s.worlds.insert(world), s.driven.insert(p as usize)),
        Err(_) => return,
    };
    let mut out = Out(Vec::new());
    if new_world {
        dump_world(&mut out, world);
    }
    if new_light {
        drive_light(&mut out, p, obj);
    }
    let _ = file.write_all(&out.0);
}
