//! C ABI of `renderer.rs`, declared in `HPL2/core/include/graphics/RustRenderer.h`.
//!
//! Pointer arguments are non-null, valid and aligned for the call; `vt` follows
//! `scene::boxtree_dyn_ffi`. A null pointer makes a call a no-op returning the zero value.

use std::ffi::c_void;
use std::slice;

use super::renderer::{
    Chc, Checks, FogParams, FrustumData, LightKey, ShadowCache, ShadowQuery, ShadowSlot, Tracker,
    ViewCheck, add_visible_to_list, chc_traverse, delayed_culled, distance_fade_amount,
    fetch_delayed_results, fog_inside_near_plane, fog_sort_order, fog_visibility,
    has_delayed_queries, node_visible, object_visible, pick_shadow_map, render_delayed_queries,
    shadow_cache_valid, shadow_casters, shadow_map_resolution, sort_shadow_casters,
};
use crate::math::{Matrixf, Planef};
use crate::scene::boxtree_dyn::Handle;
use crate::scene::boxtree_dyn_ffi::{FfiHost, HostVt};
use crate::types::Vector3f;

#[repr(C)]
pub struct ChecksC {
    pub occlusion_planes_active: bool,
    pub planes: *const Planef,
    pub plane_count: usize,
    pub fog_cull_plane_active: bool,
    pub fog_cull_plane: Planef,
    pub distance_cull_active: bool,
    pub dc_origin: Vector3f,
    pub dc_min_range_sqr: f32,
    pub dc_screen_size_sqr: f32,
    pub dc_random_size: f32,
    pub dc_fade_time: f32,
    pub dc_fade_time_inv: f32,
    pub dc_max_range_sqr: f32,
    pub dc_time: f32,
}

/// # Safety
/// `c` is null or valid and `planes` points at `plane_count` planes (or is null with count 0).
unsafe fn checks<'a>(c: *const ChecksC) -> Option<Checks<'a>> {
    // SAFETY: caller contract.
    let c = unsafe { c.as_ref() }?;
    let planes: &[Planef] = if c.planes.is_null() || c.plane_count == 0 {
        &[]
    } else {
        // SAFETY: caller contract.
        unsafe { slice::from_raw_parts(c.planes, c.plane_count) }
    };
    Some(Checks {
        occlusion_planes_active: c.occlusion_planes_active,
        planes,
        fog_cull_plane_active: c.fog_cull_plane_active,
        fog_cull_plane: c.fog_cull_plane,
        distance_cull_active: c.distance_cull_active,
        dc_origin: c.dc_origin,
        dc_min_range_sqr: c.dc_min_range_sqr,
        dc_screen_size_sqr: c.dc_screen_size_sqr,
        dc_random_size: c.dc_random_size,
        dc_fade_time: c.dc_fade_time,
        dc_fade_time_inv: c.dc_fade_time_inv,
        dc_max_range_sqr: c.dc_max_range_sqr,
        dc_time: c.dc_time,
    })
}

macro_rules! host {
    ($vt:expr, $ctx:expr) => {
        // SAFETY: the caller's contract on `vt`.
        match unsafe { FfiHost::new($vt, $ctx) } {
            Some(h) => h,
            None => return Default::default(),
        }
    };
}

macro_rules! checks {
    ($c:expr) => {
        // SAFETY: the caller's contract on `c`.
        match unsafe { checks($c) } {
            Some(c) => c,
            None => return Default::default(),
        }
    };
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_rn_tracker_new() -> *mut Tracker {
    Box::into_raw(Box::default())
}

/// # Safety
/// `t` came from `ohpl_rn_tracker_new` and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rn_tracker_free(t: *mut Tracker) {
    if !t.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(t) });
    }
}

/// # Safety
/// `t` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rn_tracker_switch(t: *mut Tracker) {
    // SAFETY: caller contract.
    if let Some(t) = unsafe { t.as_mut() } {
        t.switch_and_clear();
    }
}

/// # Safety
/// `t` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rn_tracker_set_visible(t: *mut Tracker, node: Handle) {
    // SAFETY: caller contract.
    if let Some(t) = unsafe { t.as_mut() } {
        t.set_visible(node);
    }
}

/// # Safety
/// `t` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rn_tracker_was_visible(t: *const Tracker, node: Handle) -> bool {
    // SAFETY: caller contract.
    unsafe { t.as_ref() }.is_some_and(|t| t.was_visible(node))
}

/// Writes up to `cap` pending queries to `out` and returns how many there are.
///
/// # Safety
/// `t` is valid; `out` has room for `cap` handles.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rn_tracker_pending(
    t: *const Tracker,
    out: *mut Handle,
    cap: usize,
) -> usize {
    // SAFETY: caller contract.
    let Some(t) = (unsafe { t.as_ref() }) else {
        return 0;
    };
    let all = t.pending_queries();
    if !out.is_null() {
        for (i, q) in all.iter().enumerate().take(cap) {
            // SAFETY: caller contract, `i < cap`.
            unsafe { out.add(i).write(*q) };
        }
    }
    all.len()
}

/// # Safety
/// `t` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rn_tracker_reset(t: *mut Tracker) {
    // SAFETY: caller contract.
    if let Some(t) = unsafe { t.as_mut() } {
        t.reset();
    }
}

/// # Safety
/// See the module contract.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rn_object_visible(
    vt: *const HostVt,
    ctx: *mut c_void,
    c: *const ChecksC,
    obj: Handle,
    needed: u32,
) -> bool {
    let mut h = host!(vt, ctx);
    let ck = checks!(c);
    object_visible(&mut h, &ck, obj, needed)
}

/// # Safety
/// See the module contract.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rn_node_visible(
    vt: *const HostVt,
    ctx: *mut c_void,
    c: *const ChecksC,
    node: Handle,
) -> bool {
    let mut h = host!(vt, ctx);
    let ck = checks!(c);
    node_visible(&mut h, &ck, node)
}

/// # Safety
/// See the module contract.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rn_distance_fade_amount(c: *const ChecksC, fade: f32) -> f32 {
    let ck = checks!(c);
    distance_fade_amount(&ck, fade)
}

/// Returns the new `mlOnlyRenderPrevVisibleOcclusionObjectsFrameCount`.
///
/// # Safety
/// See the module contract; `roots` points at `n` handles.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rn_chc_traverse(
    vt: *const HostVt,
    ctx: *mut c_void,
    c: *const ChecksC,
    tracker: *mut Tracker,
    chc: *const Chc,
    roots: *const Handle,
    n: usize,
) -> i32 {
    let mut h = host!(vt, ctx);
    let ck = checks!(c);
    // SAFETY: caller contract.
    let (Some(t), Some(chc)) = (unsafe { tracker.as_mut() }, unsafe { chc.as_ref() }) else {
        return 0;
    };
    let roots: &[Handle] = if roots.is_null() || n == 0 {
        &[]
    } else {
        // SAFETY: caller contract.
        unsafe { slice::from_raw_parts(roots, n) }
    };
    chc_traverse(&mut h, &ck, t, chc, roots)
}

/// # Safety
/// See the module contract.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rn_add_visible_to_list(
    vt: *const HostVt,
    ctx: *mut c_void,
    c: *const ChecksC,
    tracker: *mut Tracker,
    frustum: Handle,
    use_delayed: bool,
    needed: u32,
    root: Handle,
) {
    let mut h = host!(vt, ctx);
    let ck = checks!(c);
    // SAFETY: caller contract.
    if let Some(t) = unsafe { tracker.as_mut() } {
        add_visible_to_list(&mut h, &ck, t, frustum, use_delayed, needed, root);
    }
}

/// # Safety
/// See the module contract.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rn_fetch_delayed(
    vt: *const HostVt,
    ctx: *mut c_void,
    t: *mut Tracker,
    limit: i32,
) {
    let mut h = host!(vt, ctx);
    // SAFETY: caller contract.
    if let Some(t) = unsafe { t.as_mut() } {
        fetch_delayed_results(&mut h, t, limit);
    }
}

/// # Safety
/// `t` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rn_has_delayed_queries(t: *const Tracker) -> bool {
    // SAFETY: caller contract.
    unsafe { t.as_ref() }.is_some_and(has_delayed_queries)
}

/// # Safety
/// See the module contract.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rn_render_delayed(
    vt: *const HostVt,
    ctx: *mut c_void,
    t: *mut Tracker,
) {
    let mut h = host!(vt, ctx);
    // SAFETY: caller contract.
    if let Some(t) = unsafe { t.as_mut() } {
        render_delayed_queries(&mut h, t);
    }
}

/// # Safety
/// See the module contract.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rn_delayed_culled(
    vt: *const HostVt,
    ctx: *mut c_void,
    t: *mut Tracker,
    frustum: Handle,
    node: Handle,
) -> bool {
    let mut h = host!(vt, ctx);
    // SAFETY: caller contract.
    match unsafe { t.as_mut() } {
        Some(t) => delayed_culled(&mut h, t, frustum, node),
        None => false,
    }
}

#[repr(C)]
pub struct ShadowQueryC {
    pub light: Handle,
    pub light_origin: Vector3f,
    pub min_radius: f32,
    pub caster_flag: u32,
    pub has_view_check: bool,
    pub view_check: ViewCheck,
}

/// # Safety
/// `view`, `light` and `out` are valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rn_view_check(
    view: *const FrustumData,
    light: *const FrustumData,
    out: *mut ViewCheck,
) {
    // SAFETY: caller contract.
    let (v, l, o) = unsafe { (view.as_ref(), light.as_ref(), out.as_mut()) };
    if let (Some(v), Some(l), Some(o)) = (v, l, o) {
        *o = ViewCheck::new(v, l);
    }
}

/// Reports each caster through `vt.caster` in order.
///
/// # Safety
/// See the module contract.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rn_shadow_casters(
    vt: *const HostVt,
    ctx: *mut c_void,
    c: *const ChecksC,
    q: *const ShadowQueryC,
    root: Handle,
) {
    let mut h = host!(vt, ctx);
    let ck = checks!(c);
    // SAFETY: caller contract.
    let Some(q) = (unsafe { q.as_ref() }) else {
        return;
    };
    let query = ShadowQuery {
        ck: &ck,
        light: q.light,
        light_origin: q.light_origin,
        min_radius: q.min_radius,
        caster_flag: q.caster_flag,
        view_check: q.has_view_check.then_some(q.view_check),
    };
    let mut out = Vec::new();
    shadow_casters(&mut h, &query, root, &mut out);
    for o in out {
        h.caster(o);
    }
}

/// # Safety
/// See the module contract; `casters` points at `n` handles.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rn_sort_shadow_casters(
    vt: *const HostVt,
    ctx: *mut c_void,
    casters: *mut Handle,
    n: usize,
) {
    let mut h = host!(vt, ctx);
    if casters.is_null() || n == 0 {
        return;
    }
    // SAFETY: caller contract.
    let s = unsafe { slice::from_raw_parts_mut(casters, n) };
    sort_shadow_casters(&mut h, s);
}

/// # Safety
/// `inv` valid; `verts` points at 4 vertices.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rn_fog_inside_near_plane(
    inv: *const Matrixf,
    verts: *const Vector3f,
) -> bool {
    // SAFETY: caller contract.
    let Some(m) = (unsafe { inv.as_ref() }) else {
        return false;
    };
    if verts.is_null() {
        return false;
    }
    // SAFETY: caller contract.
    let v = unsafe { slice::from_raw_parts(verts, 4) };
    match <&[Vector3f; 4]>::try_from(v) {
        Ok(v) => fog_inside_near_plane(m, v),
        Err(_) => false,
    }
}

/// # Safety
/// All pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rn_fog_visibility(
    f: *const FogParams,
    inv: *const Matrixf,
    box_origin: *const Vector3f,
    frustum_origin: *const Vector3f,
    object_pos: *const Vector3f,
) -> f32 {
    // SAFETY: caller contract.
    let (f, m, b, o, p) = unsafe {
        (
            f.as_ref(),
            inv.as_ref(),
            box_origin.as_ref(),
            frustum_origin.as_ref(),
            object_pos.as_ref(),
        )
    };
    match (f, m, b, o, p) {
        (Some(f), Some(m), Some(b), Some(o), Some(p)) => fog_visibility(f, m, *b, *o, *p),
        _ => 1.0,
    }
}

/// # Safety
/// `view_z` and `out` point at `n` elements.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rn_fog_sort_order(view_z: *const f32, n: usize, out: *mut u32) {
    if view_z.is_null() || out.is_null() || n == 0 {
        return;
    }
    // SAFETY: caller contract.
    let (z, o) = unsafe {
        (
            slice::from_raw_parts(view_z, n),
            slice::from_raw_parts_mut(out, n),
        )
    };
    fog_sort_order(z, o);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_rn_shadow_map_resolution(wanted: i32, max: i32) -> i32 {
    shadow_map_resolution(wanted, max)
}

/// Returns the chosen slot or -1; `update` says whether to store the frame count in it.
///
/// # Safety
/// `slots` points at `n` slots (or is null with `n` 0); `update` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rn_pick_shadow_map(
    slots: *const ShadowSlot,
    n: usize,
    light: Handle,
    frame: i32,
    update: *mut bool,
) -> i64 {
    let slots: &[ShadowSlot] = if slots.is_null() || n == 0 {
        &[]
    } else {
        // SAFETY: caller contract.
        unsafe { slice::from_raw_parts(slots, n) }
    };
    let (best, upd) = pick_shadow_map(slots, light, frame);
    // SAFETY: caller contract.
    if let Some(u) = unsafe { update.as_mut() } {
        *u = upd;
    }
    best.map_or(-1, |i| i as i64)
}

/// # Safety
/// Both pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rn_shadow_cache_valid(
    c: *const ShadowCache,
    l: *const LightKey,
) -> bool {
    // SAFETY: caller contract.
    match unsafe { (c.as_ref(), l.as_ref()) } {
        (Some(c), Some(l)) => shadow_cache_valid(c, l),
        _ => false,
    }
}

const _: () = {
    use super::renderer::{MatView, ObjCheck};
    use std::mem::size_of;
    assert!(size_of::<FrustumData>() == 120);
    assert!(size_of::<ChecksC>() == 88 && size_of::<Chc>() == 104);
    assert!(size_of::<ViewCheck>() == 152 && size_of::<ShadowQueryC>() == 184);
    assert!(size_of::<FogParams>() == 20);
    assert!(
        size_of::<ShadowSlot>() == 16
            && size_of::<ShadowCache>() == 32
            && size_of::<LightKey>() == 32
    );
    assert!(size_of::<ObjCheck>() == 36 && size_of::<MatView>() == 24);
};
