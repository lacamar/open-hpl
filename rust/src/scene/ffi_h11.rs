//! C ABI for `h11.rs`, declared in `HPL2/core/include/scene/RustSceneH11.h`.
//!
//! Pointers are non-null, valid and aligned for the call.

use super::h11::{self, Caster, Ctl, MeshCtl, ShadowCache, SpotCtl};
use crate::types::{Color, Matrixf, Vector3f};

const _: () = {
    use std::mem::{offset_of, size_of};
    assert!(size_of::<MeshCtl>() == 32 && offset_of!(MeshCtl, physics) == 20);
    assert!(size_of::<SpotCtl>() == 16 && offset_of!(SpotCtl, projection_updated) == 12);
    assert!(size_of::<Caster>() == 16 && offset_of!(Caster, is_static) == 12);
};

fn count(n: i32) -> usize {
    usize::try_from(n).unwrap_or(0)
}

/// # Safety
/// `p` valid for `n` reads when `n > 0`.
unsafe fn slice<'a, T>(p: *const T, n: i32) -> &'a [T] {
    match usize::try_from(n) {
        // SAFETY: caller contract.
        Ok(n) if n > 0 && !p.is_null() => unsafe { std::slice::from_raw_parts(p, n) },
        _ => &[],
    }
}

/// # Safety
/// `ctl` valid for a write.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h11_mesh_ctl_init(ctl: *mut MeshCtl) {
    // SAFETY: caller contract.
    unsafe { ctl.write(MeshCtl::new()) }
}

/// # Safety
/// `ctl` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h11_mesh_physics_set(ctl: *mut MeshCtl, active: bool) {
    // SAFETY: caller contract.
    h11::skeleton_physics_set(unsafe { Ctl::new(ctl) }, active);
}

/// # Safety
/// `ctl` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h11_mesh_physics_fade(ctl: *mut MeshCtl, time: f32) -> bool {
    // SAFETY: caller contract.
    h11::skeleton_physics_fade(unsafe { Ctl::new(ctl) }, time)
}

/// # Safety
/// `ctl` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h11_mesh_graphics_gate(
    ctl: *mut MeshCtl,
    has_skeleton: bool,
    transform_count: i32,
) -> u32 {
    // SAFETY: caller contract.
    h11::graphics_gate(unsafe { Ctl::new(ctl) }, has_skeleton, transform_count)
}

/// # Safety
/// All pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h11_sub_skin_gate(
    graphics_updated: *mut bool,
    skinned_version: *mut i32,
    mesh_version: i32,
    sleeping: bool,
) -> bool {
    // SAFETY: caller contract.
    unsafe {
        h11::sub_skin_gate(
            &mut *graphics_updated,
            &mut *skinned_version,
            mesh_version,
            sleeping,
        )
    }
}

/// # Safety
/// All pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h11_sub_static_null(count: *mut i32, world: *const Matrixf) -> bool {
    // SAFETY: caller contract.
    unsafe { h11::static_null_matrix(&mut *count, &*world) }
}

/// # Safety
/// `vals` holds `nvals` floats, `locals`/`unit`/`out` hold `n` entries; `out` does not alias.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h11_rest_pose_from_map(
    vals: *const f32,
    nvals: i32,
    locals: *const Matrixf,
    unit: *const f32,
    out: *mut Matrixf,
    n: i32,
) -> bool {
    let len = count(n);
    let out = if out.is_null() || len == 0 {
        &mut []
    } else {
        // SAFETY: caller contract.
        unsafe { std::slice::from_raw_parts_mut(out, len) }
    };
    // SAFETY: caller contract.
    let (vals, locals, unit) = unsafe { (slice(vals, nvals), slice(locals, n), slice(unit, n)) };
    h11::rest_pose_from_map(vals, locals, unit, out)
}

/// # Safety
/// All pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h11_transform_from_skeleton(
    state_world: *const Matrixf,
    bone_inv_world: *const Matrixf,
    out: *mut Matrixf,
    pos: *mut Vector3f,
    angle_y: *mut f32,
) {
    // SAFETY: caller contract.
    unsafe {
        let (m, p, y) = h11::transform_from_skeleton(&*state_world, &*bone_inv_world);
        *out = m;
        *pos = p;
        *angle_y = y;
    }
}

/// # Safety
/// `mins`/`maxs` hold `n` vectors; `out_min`/`out_max` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h11_bounds_union(
    mins: *const Vector3f,
    maxs: *const Vector3f,
    n: i32,
    out_min: *mut Vector3f,
    out_max: *mut Vector3f,
) -> bool {
    // SAFETY: caller contract.
    let Some((lo, hi)) = h11::bounds_union(unsafe { slice(mins, n) }, unsafe { slice(maxs, n) })
    else {
        return false;
    };
    // SAFETY: caller contract.
    unsafe {
        *out_min = lo;
        *out_max = hi;
    }
    true
}

/// # Safety
/// `diffuse` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h11_light_is_lit(
    diffuse: *const Color,
    brightness: f32,
    hpl3: bool,
) -> bool {
    // SAFETY: caller contract.
    let c = unsafe { &*diffuse };
    h11::light_is_lit(c.r, c.g, c.b, c.a, brightness, hpl3)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h11_light_is_visible(lit: bool, radius: f32, visible: bool) -> bool {
    h11::light_is_visible(lit, radius, visible)
}

/// # Safety
/// All pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h11_light_mask_bounds(
    min: *const Vector3f,
    max: *const Vector3f,
    center: *const Vector3f,
    size: *const Vector3f,
    out_min: *mut Vector3f,
    out_max: *mut Vector3f,
) {
    // SAFETY: caller contract.
    unsafe {
        let (lo, hi) = h11::light_mask_bounds(*min, *max, *center, *size);
        *out_min = lo;
        *out_max = hi;
    }
}

/// # Safety
/// `s` valid for a write.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h11_spot_init(s: *mut SpotCtl) {
    // SAFETY: caller contract.
    unsafe { s.write(SpotCtl::new()) }
}

/// # Safety
/// `s` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h11_spot_view_stale(s: *mut SpotCtl, count: i32) -> bool {
    // SAFETY: caller contract.
    unsafe { (*s).view_stale(count) }
}

/// # Safety
/// `s` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h11_spot_projection_stale(s: *mut SpotCtl) -> bool {
    // SAFETY: caller contract.
    unsafe { (*s).projection_stale() }
}

/// # Safety
/// `s` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h11_spot_view_proj_stale(s: *const SpotCtl, count: i32) -> bool {
    // SAFETY: caller contract.
    unsafe { (*s).view_proj_stale(count) }
}

/// # Safety
/// `s` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h11_spot_view_proj_done(s: *mut SpotCtl, count: i32) {
    // SAFETY: caller contract.
    unsafe { (*s).view_proj_done(count) }
}

/// # Safety
/// `s` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h11_spot_frustum_stale(s: *const SpotCtl, count: i32) -> bool {
    // SAFETY: caller contract.
    unsafe { (*s).frustum_stale(count) }
}

/// # Safety
/// `s` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h11_spot_frustum_done(s: *mut SpotCtl, count: i32) {
    // SAFETY: caller contract.
    unsafe { (*s).frustum_done(count) }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h11_shadow_cache_new() -> *mut ShadowCache {
    Box::into_raw(Box::default())
}

/// # Safety
/// `c` from `ohpl_h11_shadow_cache_new`, not used afterwards; null is ignored.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h11_shadow_cache_free(c: *mut ShadowCache) {
    if !c.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(c) });
    }
}

/// # Safety
/// `c` from `ohpl_h11_shadow_cache_new`, live and exclusive.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h11_shadow_cache_add(c: *mut ShadowCache, object: usize, count: i32) {
    // SAFETY: caller contract.
    if let Some(c) = unsafe { c.as_mut() } {
        c.add(object, count);
    }
}

/// # Safety
/// `c` from `ohpl_h11_shadow_cache_new`, live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h11_shadow_cache_valid(
    c: *const ShadowCache,
    object: usize,
    count: i32,
) -> bool {
    // SAFETY: caller contract.
    unsafe { c.as_ref() }.is_some_and(|c| c.is_valid(object, count))
}

/// # Safety
/// `c` live; `list` holds `n` casters.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h11_shadow_cache_unchanged(
    c: *const ShadowCache,
    list: *const Caster,
    n: i32,
) -> bool {
    // SAFETY: caller contract.
    unsafe { c.as_ref() }.is_some_and(|c| c.unchanged(unsafe { slice(list, n) }))
}

/// # Safety
/// `c` live and exclusive; `list` holds `n` casters.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h11_shadow_cache_set(
    c: *mut ShadowCache,
    list: *const Caster,
    n: i32,
) {
    // SAFETY: caller contract.
    if let Some(c) = unsafe { c.as_mut() } {
        c.set_from(unsafe { slice(list, n) });
    }
}

/// # Safety
/// `c` live and exclusive.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h11_shadow_cache_clear(c: *mut ShadowCache) {
    // SAFETY: caller contract.
    if let Some(c) = unsafe { c.as_mut() } {
        c.clear();
    }
}

/// # Safety
/// `c` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h11_shadow_cache_len(c: *const ShadowCache) -> i32 {
    // SAFETY: caller contract.
    unsafe { c.as_ref() }.map_or(0, |c| i32::try_from(c.len()).unwrap_or(i32::MAX))
}

/// # Safety
/// `size` and `out` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h11_fog_bounds_size(size: *const Vector3f, out: *mut Vector3f) {
    // SAFETY: caller contract.
    unsafe { *out = h11::fog_bounds_size(*size) }
}
