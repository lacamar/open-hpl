//! C ABI of the map helper, mirrored in `amnesia/src/game/RustLuxMapA13.h`.

use super::mhelper::{self, DamageArgs, Ray};
use super::mhost::{self, OpFn};
use crate::string_ffi::units;
use crate::types::Vector3f;

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a13_set_host(f: Option<OpFn>) {
    mhost::set_host(f);
}

/// # Safety
/// `ctx` is the [`Ray`] the running cast was started with.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_ray_before(ctx: *mut Ray, body: usize) -> bool {
    // SAFETY: caller contract.
    unsafe { mhelper::ray_before(ctx, body) }
}

/// # Safety
/// `ctx` is the [`Ray`] the running cast was started with; `normal` is readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_ray_hit(
    ctx: *mut Ray,
    body: usize,
    dist: f32,
    normal: *const Vector3f,
) -> bool {
    // SAFETY: caller contract.
    unsafe { mhelper::ray_hit(ctx, body, dist, *normal) }
}

/// # Safety
/// Vectors are readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_check_los(
    start: *const Vector3f,
    end: *const Vector3f,
    shadows: bool,
) -> bool {
    // SAFETY: caller contract.
    unsafe { mhelper::check_line_of_sight(*start, *end, shadows) }
}

/// # Safety
/// Vectors are readable; out pointers are null or writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_closest_entity(
    start: *const Vector3f,
    dir: *const Vector3f,
    len: f32,
    dist: *mut f32,
    body: *mut usize,
    entity: *mut usize,
) -> bool {
    // SAFETY: caller contract.
    let Some(c) = (unsafe { mhelper::closest_entity(*start, *dir, len) }) else {
        return false;
    };
    // SAFETY: caller contract.
    unsafe {
        put(dist, c.dist);
        put(body, c.body);
        put(entity, c.entity);
    }
    c.entity != 0
}

/// # Safety
/// Vectors are readable; out pointers are null or writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_closest_char(
    start: *const Vector3f,
    dir: *const Vector3f,
    len: f32,
    dist: *mut f32,
    normal: *mut Vector3f,
    body: *mut usize,
) -> bool {
    // SAFETY: caller contract.
    let Some(c) = (unsafe { mhelper::closest_char_collider(*start, *dir, len) }) else {
        return false;
    };
    // SAFETY: caller contract.
    unsafe {
        put(dist, c.dist);
        put(normal, c.normal);
        put(body, c.body);
    }
    c.body != 0
}

/// # Safety
/// `pos` readable; `skip` readable for `n` entries when `has_skip`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_light_level(
    pos: *const Vector3f,
    skip: *const usize,
    n: usize,
    has_skip: bool,
    radius_add: f32,
) -> f32 {
    // SAFETY: caller contract.
    let skip = has_skip.then(|| unsafe { units(skip, n) });
    // SAFETY: caller contract.
    mhelper::light_level_at_pos(unsafe { *pos }, skip, radius_add)
}

/// # Safety
/// `args` readable; `hit_player` null or writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_shape_damage(
    args: *const DamageArgs,
    hit_player: *mut bool,
) -> bool {
    // SAFETY: caller contract.
    let (hit, player) = mhelper::shape_damage(unsafe { &*args });
    // SAFETY: caller contract.
    unsafe { put(hit_player, player) };
    hit
}

/// # Safety
/// `p` is null or writable.
unsafe fn put<T>(p: *mut T, v: T) {
    if !p.is_null() {
        // SAFETY: caller contract.
        unsafe { *p = v };
    }
}
