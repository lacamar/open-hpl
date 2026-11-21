//! LS: `cLensFlare` and `cLensFlareType` shells over the `sl.rs` flare pods. The type objects stay
//! C++ `iRenderable`s (the renderer reaches them as renderables); everything they decide is here.

use super::h10::EntPub;
use super::h12b_ent;
use super::lensflare::{EntIn, ViewIn};
use super::ls_host::{self as lh, Ent, Vb};
use super::rnd::Rnd;
use super::sl::{self, FlarePod, FlareTypePod};
use crate::math::bv::{Bv, Frustum, PERSPECTIVE};
use crate::math::geom::{FrustumView, normalized_clip_rect_from_bv};
use crate::math::libm::c_rand;
use crate::math::{matrix_mul, matrix_scale};
use crate::types::{Color, Matrixf, Vector2f, Vector3f};
use std::ffi::c_void;

const MULTI_IRIS: usize = 2;
const COLLISION_OUTSIDE: i32 = 1;

#[repr(C)]
struct FlareBox {
    pod: FlarePod,
    ent: Ent,
}

fn flare<'a>(p: *mut FlarePod) -> &'a mut FlareBox {
    // SAFETY: every `FlarePod` handed to C++ is the first field of a `FlareBox`.
    unsafe { &mut *p.cast() }
}

fn view_in(f: &Frustum) -> ViewIn {
    ViewIn {
        origin: f.origin,
        view: f.view,
        proj: f.proj,
        aspect: f.aspect,
    }
}

fn frustum_view(f: &Frustum) -> FrustumView<'_> {
    FrustumView {
        view: &f.view,
        proj: &f.proj,
        perspective: f.projection == PERSPECTIVE,
        fov: f.fov,
        near: f.near_plane,
        aspect: f.aspect,
    }
}

impl FlareBox {
    fn ent_in(&self) -> EntIn {
        let pos = self.ent.pos();
        // SAFETY: the entity pod and its world matrix outlive the shell.
        let (m, e) = unsafe { (&*self.ent.world(), &*self.ent.e) };
        let axis = if e.parent_entity.is_null() && e.parent_node.is_null() {
            Vector3f {
                x: m.m[2][0],
                y: m.m[2][1],
                z: m.m[2][2],
            }
        } else {
            Vector3f {
                x: m.m[0][2],
                y: m.m[1][2],
                z: m.m[2][2],
            }
        };
        EntIn { pos, axis }
    }

    fn update_bv(&self) {
        // SAFETY: `pod` valid.
        let r = unsafe { sl::ohpl_sl_flare_bv_radius(&self.pod) };
        self.ent.bv().set_local_min_max(
            Vector3f {
                x: -r,
                y: -r,
                z: -r,
            },
            Vector3f { x: r, y: r, z: r },
        );
        self.ent.updated();
    }

    fn any_active(&self) -> bool {
        // SAFETY: `pod` valid.
        unsafe { sl::ohpl_sl_flare_any_active(&self.pod) }
    }

    fn billboard(&mut self, view: &ViewIn) -> *mut Matrixf {
        let ent = self.ent_in();
        // SAFETY: all pointers valid.
        unsafe { sl::ohpl_sl_flare_billboard(&mut self.pod, &ent, view) }
    }

    fn size_rotation(&mut self, kind: i32, view: &ViewIn, vis: f32) -> *mut Matrixf {
        let ent = self.ent_in();
        // SAFETY: all pointers valid.
        unsafe { sl::ohpl_sl_flare_size_rotation(&mut self.pod, &ent, view, kind, vis) }
    }
}

/// # Safety
/// `ctx` is the shell, `e` its entity pod, `bv` its bounding volume, `res`/`gfx` the engine services.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_flare_new(
    ctx: *mut c_void,
    e: *mut EntPub,
    bv: *mut Bv,
    res: usize,
    gfx: usize,
) -> *mut FlarePod {
    // SAFETY: `ohpl_sl_flare_new` returns a live pod that is copied out and released.
    let mut pod = unsafe {
        let p = sl::ohpl_sl_flare_new(0);
        let pod = *p;
        sl::ohpl_sl_flare_free(p);
        pod
    };
    pod.mat_mgr = res;
    pod.low_level = gfx;
    let ent = Ent { ctx, e, bv };
    let b = Box::into_raw(Box::new(FlareBox { pod, ent }));
    // SAFETY: just allocated.
    let b = unsafe { &mut *b };

    for kind in 0..3 {
        let ty = lh::host().map_or(0, |h| {
            // SAFETY: table contract; `b.pod` outlives the call.
            unsafe { (h.flare_type_new)(ctx, &mut b.pod, kind as i32) as usize }
        });
        if let Some(slot) = b.pod.types.get_mut(kind) {
            *slot = ty;
        }
    }
    b.pod.multi_iris_seed = c_rand();

    let vb = Vb::create(gfx as *mut c_void, lh::USAGE_STATIC, 4, 6);
    b.pod.vtx = vb.0 as usize;
    vb.elem(lh::E_POS, 4);
    vb.elem(lh::E_NORMAL, 3);
    vb.elem(lh::E_COLOR0, 4);
    vb.elem(lh::E_TEX0, 3);
    let hdr = lh::hdr();
    for (x, y) in [(1.0f32, -1.0f32), (-1.0, -1.0), (-1.0, 1.0), (1.0, 1.0)] {
        let v = if hdr { -y } else { y };
        vb.add(lh::E_POS, &[x * 0.5, y * 0.5, 0.0]);
        vb.color(
            lh::E_COLOR0,
            Color {
                r: 1.0,
                g: 1.0,
                b: 1.0,
                a: 1.0,
            },
        );
        vb.add(lh::E_TEX0, &[(x + 1.0) / 2.0, (v + 1.0) / 2.0, 0.0]);
        vb.add(lh::E_NORMAL, &[0.0, 0.0, 1.0]);
    }
    for i in [0, 1, 2, 2, 3, 0] {
        vb.index(i);
    }
    vb.compile(1);

    let mut occlusion = Box::new(Bv::new());
    occlusion.set_size(b.pod.source_size);
    b.pod.occlusion_bv = Box::into_raw(occlusion) as usize;
    b.update_bv();
    &mut b.pod
}

/// # Safety
/// `p` from `ohpl_ls_flare_new`, not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_flare_free(p: *mut FlarePod) {
    // SAFETY: caller contract.
    let b = unsafe { Box::from_raw(p.cast::<FlareBox>()) };
    for (mat, ty) in b.pod.material.iter().zip(b.pod.types) {
        lh::mat_destroy(b.pod.mat_mgr, *mat);
        if let (Some(h), true) = (lh::host(), ty != 0) {
            // SAFETY: table contract.
            unsafe { (h.flare_type_delete)(ty as *mut c_void) }
        }
    }
    Vb(b.pod.vtx as *mut c_void).free();
    Vb(b.pod.multi_iris_vtx as *mut c_void).free();
    // SAFETY: the occlusion volume is the `Box<Bv>` made by `ohpl_ls_flare_new`.
    drop(unsafe { Box::from_raw(b.pod.occlusion_bv as *mut Bv) });
}

/// # Safety
/// `p` and `size` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_flare_set_source_size(p: *mut FlarePod, size: *const Vector3f) {
    let b = flare(p);
    // SAFETY: caller contract; the occlusion volume is a `Box<Bv>`.
    unsafe {
        b.pod.source_size = *size;
        (*(b.pod.occlusion_bv as *mut Bv)).set_size(b.pod.source_size);
    }
}

/// # Safety
/// `p` and `size` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_flare_set_size(
    p: *mut FlarePod,
    kind: i32,
    size: *const Vector2f,
) {
    let b = flare(p);
    if let Some(s) = usize::try_from(kind)
        .ok()
        .and_then(|k| b.pod.size.get_mut(k))
    {
        // SAFETY: caller contract.
        *s = unsafe { *size };
    }
    b.update_bv();
}

/// # Safety
/// `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_flare_set_active(p: *mut FlarePod, kind: i32, active: bool) {
    let b = flare(p);
    if let Some(a) = usize::try_from(kind)
        .ok()
        .and_then(|k| b.pod.active.get_mut(k))
    {
        *a = active;
    }
    b.update_bv();
}

/// # Safety
/// `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_flare_set_material(p: *mut FlarePod, kind: i32, mat: usize) {
    let b = flare(p);
    let res = b.pod.mat_mgr;
    if let Some(m) = usize::try_from(kind)
        .ok()
        .and_then(|k| b.pod.material.get_mut(k))
    {
        lh::mat_destroy(res, *m);
        *m = mat;
    }
}

/// # Safety
/// `p`, `frustum` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_flare_color(
    p: *mut FlarePod,
    frustum: *const Frustum,
    kind: i32,
    vis: f32,
    out: *mut Color,
) {
    let b = flare(p);
    let (ent, view) = (
        b.ent_in(),
        view_in(
            // SAFETY: caller contract.
            unsafe { &*frustum },
        ),
    );
    // SAFETY: all pointers valid.
    unsafe { sl::ohpl_sl_flare_color(&b.pod, &ent, &view, kind, vis, out) };
}

/// # Safety
/// `p`, `frustum`, `screen` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_flare_glare(
    p: *mut FlarePod,
    frustum: *const Frustum,
    screen: *mut Vector3f,
    scale: f32,
    out: *mut Color,
) {
    let b = flare(p);
    let (ent, view) = (
        b.ent_in(),
        view_in(
            // SAFETY: caller contract.
            unsafe { &*frustum },
        ),
    );
    // SAFETY: all pointers valid.
    unsafe { sl::ohpl_sl_flare_glare(&b.pod, &ent, &view, scale, out, screen) };
}

/// # Safety
/// `p` valid; `frustum` null or valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_flare_model(
    p: *mut FlarePod,
    frustum: *const Frustum,
) -> *mut Matrixf {
    let b = flare(p);
    if frustum.is_null() {
        return b.ent.world().cast_mut();
    }
    // SAFETY: caller contract.
    let view = view_in(unsafe { &*frustum });
    b.billboard(&view)
}

/// # Safety
/// `p`, `frustum` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_flare_collides(p: *mut FlarePod, frustum: *const Frustum) -> bool {
    let b = flare(p);
    if !b.any_active() {
        return false;
    }
    // SAFETY: caller contract.
    let frustum = unsafe { &*frustum };
    let live = lh::host().map_or(std::ptr::null_mut(), |h| {
        // SAFETY: table contract.
        unsafe { (h.ent_bv)(b.ent.ctx) }
    });
    if live.is_null() {
        return false;
    }
    // SAFETY: the host returns the live bounding volume.
    let mut bv = unsafe { (*live).clone() };
    let view = view_in(frustum);
    let (wc, lmin, lmax, lc) = (
        bv.world_center(),
        bv.local_min,
        bv.local_max,
        bv.local_center(),
    );
    let (mut nmin, mut nmax) = (Vector3f::default(), Vector3f::default());
    // SAFETY: all pointers valid.
    if !unsafe {
        sl::ohpl_sl_flare_collide_box(&b.pod, &view, &wc, &lmin, &lmax, &lc, &mut nmin, &mut nmax)
    } {
        return false;
    }
    bv.set_local_min_max(nmin, nmax);
    frustum.collide_bv(&mut bv) != COLLISION_OUTSIDE
}

/// # Safety
/// `p` valid; `renderer` is the `iRenderer` that queries.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_flare_assign_occlusion(p: *mut FlarePod, renderer: *mut c_void) {
    let b = flare(p);
    if !b.any_active() {
        return;
    }
    // SAFETY: the entity matrix is live.
    b.pod.occluder = matrix_mul(unsafe { &*b.ent.world() }, &matrix_scale(b.pod.source_size));
    let (mut vb, mut mtx): (*mut c_void, *const Matrixf) = (std::ptr::null_mut(), &b.pod.occluder);
    let Some(h) = lh::host() else { return };
    if b.pod.use_parent_mesh {
        // SAFETY: table contract; both out pointers valid.
        unsafe { (h.flare_parent_mesh)(b.ent.ctx, &mut vb, &mut mtx) };
    }
    // SAFETY: table contract.
    unsafe {
        (h.occ_assign)(renderer, b.ent.ctx, 0, vb, mtx, true);
        (h.occ_assign)(renderer, b.ent.ctx, 1, vb, mtx, false);
    }
}

/// # Safety
/// `p` valid; `renderer` is the `iRenderer` that queries.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_flare_retrieve_occlusion(
    p: *mut FlarePod,
    renderer: *mut c_void,
) -> bool {
    let b = flare(p);
    b.pod.vis.target = 0.0;
    if !b.any_active() {
        return false;
    }
    let Some(h) = lh::host() else { return false };
    // SAFETY: table contract.
    let (vis, total) = unsafe {
        (
            (h.occ_samples)(renderer, b.ent.ctx, 0) as f32,
            (h.occ_samples)(renderer, b.ent.ctx, 1) as f32,
        )
    };
    if vis <= 0.0 {
        return false;
    }
    // SAFETY: the occlusion volume is a `Box<Bv>`; the renderer frustum is live.
    let (occlusion, frustum) = unsafe {
        (
            &mut *(b.pod.occlusion_bv as *mut Bv),
            &*(h.renderer_frustum)(renderer),
        )
    };
    // SAFETY: the entity matrix is live.
    occlusion.set_transform(unsafe { &*b.ent.world() });
    occlusion.update_size();
    let Some((min, max)) = normalized_clip_rect_from_bv(
        occlusion.world_min,
        occlusion.world_max,
        &frustum_view(frustum),
        0.0,
    ) else {
        return true;
    };
    // SAFETY: all pointers valid.
    unsafe { sl::ohpl_sl_flare_set_target(&mut b.pod, vis, total, &min, &max) };
    true
}

/// # Safety
/// `p`, `frustum` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_flare_multi_iris(
    p: *mut FlarePod,
    frustum: *const Frustum,
    vis: f32,
) -> *mut c_void {
    let b = flare(p);
    let count = b.pod.multi_iris_count;
    let mut vb = Vb(b.pod.multi_iris_vtx as *mut c_void);
    let existing = if vb.0.is_null() {
        -1
    } else {
        lh::host().map_or(-1, |h| {
            // SAFETY: table contract.
            unsafe { (h.vb_vertex_num)(vb.0) }
        })
    };
    let plan = h12b_ent::iris_plan(b.pod.is_active(MULTI_IRIS), count, existing);
    if plan == h12b_ent::IRIS_NONE {
        return std::ptr::null_mut();
    }
    if plan == h12b_ent::IRIS_RECREATE {
        vb.free();
        vb = Vb(std::ptr::null_mut());
    }
    if vb.0.is_null() {
        vb = Vb::create(
            b.pod.low_level as *mut c_void,
            lh::USAGE_DYNAMIC,
            count.wrapping_mul(4),
            count.wrapping_mul(6),
        );
        vb.elem(lh::E_POS, 4);
        vb.elem(lh::E_COLOR0, 4);
        vb.elem(lh::E_TEX0, 3);
        for _ in 0..count.wrapping_mul(4) {
            vb.add(lh::E_POS, &[0.0, 0.0, 0.0, 1.0]);
            vb.color(lh::E_COLOR0, Color::default());
            vb.add(lh::E_TEX0, &[0.0; 3]);
        }
        for i in 0..count {
            for j in [0, 1, 2, 0, 2, 3] {
                vb.index(i * 4 + j);
            }
        }
        vb.compile(0);
    }
    b.pod.multi_iris_vtx = vb.0 as usize;

    let (pos, col, uv) = (
        vb.array(lh::E_POS),
        vb.array(lh::E_COLOR0),
        vb.array(lh::E_TEX0),
    );
    let ent = b.ent_in();
    // SAFETY: caller contract; the arrays hold `count` quads.
    let view = view_in(unsafe { &*frustum });
    // SAFETY: all pointers valid.
    unsafe { sl::ohpl_sl_flare_multi_iris(&b.pod, &ent, &view, vis, pos, col, uv, count) };
    vb.update(lh::F_POS | lh::F_COLOR0 | lh::F_TEX0);
    vb.0
}

#[repr(C)]
struct TypeBox {
    pod: FlareTypePod,
    flare: *mut FlareBox,
    rnd: *mut Rnd,
}

/// # Safety
/// `parent` is the `cLensFlare`, `rnd` the type's `OhplRnd`, `flare` the flare pod.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_ft_new(
    parent: usize,
    rnd: *mut Rnd,
    flare: *mut FlarePod,
    kind: i32,
) -> *mut FlareTypePod {
    Box::into_raw(Box::new(TypeBox {
        pod: FlareTypePod {
            parent,
            kind,
            model: Matrixf::default(),
        },
        flare: flare.cast(),
        rnd,
    }))
    .cast()
}

/// # Safety
/// `p` from `ohpl_ls_ft_new`, not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_ft_free(p: *mut FlareTypePod) {
    if !p.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(p.cast::<TypeBox>()) });
    }
}

/// # Safety
/// `p` valid; `frustum` null or valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_ft_model(
    p: *mut FlareTypePod,
    frustum: *const Frustum,
) -> *mut Matrixf {
    // SAFETY: caller contract; the parent flare outlives its types.
    let (t, flare) = unsafe {
        (
            &mut *p.cast::<TypeBox>(),
            &mut *(*p.cast::<TypeBox>()).flare,
        )
    };
    if frustum.is_null() {
        return flare.ent.world().cast_mut();
    }
    // SAFETY: caller contract.
    let view = view_in(unsafe { &*frustum });
    let vis = flare.pod.vis.vis;
    let size_rot = flare.size_rotation(t.pod.kind, &view, vis);
    let bill = flare.billboard(&view);
    if bill.is_null() || size_rot.is_null() {
        return std::ptr::null_mut();
    }
    // SAFETY: both matrices live in the parent pod.
    t.pod.model = matrix_mul(unsafe { &*bill }, unsafe { &*size_rot });
    &mut t.pod.model
}

/// # Safety
/// `p` valid; `frustum` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_ft_update_viewport(
    p: *mut FlareTypePod,
    frustum: *const Frustum,
) -> bool {
    // SAFETY: caller contract; the parent flare outlives its types.
    let (t, flare) = unsafe {
        (
            &mut *p.cast::<TypeBox>(),
            &mut *(*p.cast::<TypeBox>()).flare,
        )
    };
    // SAFETY: caller contract.
    let view = view_in(unsafe { &*frustum });
    let ent = flare.ent_in();
    let mut c = Color::default();
    // SAFETY: all pointers valid.
    if !unsafe {
        sl::ohpl_sl_flare_type_color(
            &flare.pod,
            &ent,
            &view,
            t.pod.kind,
            flare.pod.vis.vis,
            &mut c,
        )
    } {
        return false;
    }
    // SAFETY: the renderable pod outlives the type.
    unsafe { (*t.rnd).color_mul = c };
    true
}

/// The parent flare's material and vertex buffer for the type renderable.
///
/// # Safety
/// `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_ft_material(p: *const FlareTypePod) -> usize {
    // SAFETY: caller contract.
    let t = unsafe { &*p.cast::<TypeBox>() };
    // SAFETY: the parent flare outlives its types.
    let flare = unsafe { &*t.flare };
    usize::try_from(t.pod.kind)
        .ok()
        .and_then(|k| flare.pod.material.get(k).copied())
        .unwrap_or(0)
}

/// # Safety
/// `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_ft_vertex_buffer(p: *const FlareTypePod) -> usize {
    // SAFETY: caller contract; the parent flare outlives its types.
    unsafe { (*(*p.cast::<TypeBox>()).flare).pod.vtx }
}

/// # Safety
/// `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_ft_update_count(p: *const FlareTypePod) -> i32 {
    // SAFETY: caller contract; the parent flare outlives its types.
    unsafe { (*(*p.cast::<TypeBox>()).flare).ent.count() }
}
