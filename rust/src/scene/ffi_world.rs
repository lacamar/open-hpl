//! C ABI for `soundentity.rs`, `world.rs` and `scenemgr.rs`, declared in
//! `HPL2/core/include/scene/RustWorld.h`.
//!
//! Pointers are non-null, valid and aligned for the call; outputs do not alias inputs. Arrays
//! are valid for the stated count. Vectors and colours cross as pointers.

use super::scenemgr::{self, ViewportIn};
use super::world::{self, FieldIn};
use crate::types::{Color, Vector3f};
use std::ffi::c_void;

const _: () = {
    use std::mem::size_of;
    assert!(size_of::<FieldIn>() == 28 && size_of::<ViewportIn>() == 6);
    assert!(size_of::<Str>() == 16);
};

/// A `std::string` view.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Str {
    ptr: *const u8,
    len: usize,
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
/// `s.ptr` valid for `s.len` reads.
unsafe fn bytes<'a>(s: Str) -> &'a [u8] {
    if s.len == 0 || s.ptr.is_null() {
        return &[];
    }
    // SAFETY: caller contract.
    unsafe { std::slice::from_raw_parts(s.ptr, s.len) }
}

/// # Safety
/// `names` valid for `n` entries, each readable for its length; `key` for `key_len` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_world_find_name(names: *const Str, n: i32, key: Str) -> i32 {
    // SAFETY: caller contract.
    unsafe { world::find_by_name(slice(names, n).iter().map(|&s| bytes(s)), bytes(key)) }
}

/// # Safety
/// As `ohpl_world_find_name`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_world_find_last_name(names: *const Str, n: i32, key: Str) -> i32 {
    // SAFETY: caller contract.
    unsafe { world::find_last_by_name(slice(names, n).iter().map(|&s| bytes(s)), bytes(key)) }
}

/// # Safety
/// `names` valid for `n` entries, each readable for its length.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_world_first_by_name(names: *const Str, n: i32) -> i32 {
    // SAFETY: caller contract.
    unsafe { world::first_by_name(slice(names, n).iter().map(|&s| bytes(s))) }
}

/// # Safety
/// `ids` valid for `n` reads.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_world_find_id(ids: *const i32, n: i32, id: i32) -> i32 {
    // SAFETY: caller contract.
    world::find_by_id(unsafe { slice(ids, n) }.iter().copied(), id)
}

/// # Safety
/// `ptrs` valid for `n` reads.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_world_find_ptr(
    ptrs: *const *const c_void,
    n: i32,
    p: *const c_void,
) -> i32 {
    // SAFETY: caller contract.
    world::find_ptr(
        unsafe { slice(ptrs, n) }.iter().map(|&q| q as usize),
        p as usize,
    )
}

/// # Safety
/// `fields` valid for `n` reads; `min`, `max` valid; `out` valid for 4 writes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_world_force_fields(
    fields: *const FieldIn,
    n: i32,
    min: *const Vector3f,
    max: *const Vector3f,
    out: *mut u32,
) -> i32 {
    let mut idx = [0u32; 4];
    // SAFETY: caller contract.
    let num = unsafe { world::force_fields(slice(fields, n), *min, *max, &mut idx) };
    // SAFETY: caller contract: `out` holds 4 entries.
    unsafe { std::ptr::copy_nonoverlapping(idx.as_ptr(), out, 4) };
    num
}

/// # Safety
/// `sky` and `out` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_world_skybox_color(
    sky: *const Color,
    hpl3: bool,
    brightness: f32,
    out: *mut Color,
) {
    // SAFETY: caller contract.
    unsafe { *out = world::skybox_color(*sky, hpl3, brightness) }
}

/// # Safety
/// All pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_world_physics_bounds(
    min: *const Vector3f,
    max: *const Vector3f,
    out_min: *mut Vector3f,
    out_max: *mut Vector3f,
) {
    // SAFETY: caller contract.
    unsafe {
        let (a, b) = world::physics_bounds(*min, *max);
        *out_min = a;
        *out_max = b;
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_world_pre_update_steps(total: f32, step: f32) -> u32 {
    world::pre_update_steps(total, step)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_world_destroy_plan(flags: u32) -> u32 {
    world::destroy_plan(flags)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_scene_wants_post_effects(flags: u32, has_composite: bool) -> bool {
    scenemgr::wants_post_effects(flags, has_composite)
}

/// # Safety
/// `v` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_scene_plan_viewport(flags: u32, v: *const ViewportIn) -> u32 {
    // SAFETY: caller contract.
    scenemgr::plan_viewport(flags, unsafe { &*v })
}

/// # Safety
/// `prios` valid for `n` reads; `out` for `n` writes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_scene_gui_order(prios: *const i32, n: i32, out: *mut u32) -> i32 {
    // SAFETY: caller contract.
    let out = unsafe {
        match usize::try_from(n) {
            Ok(n) if n > 0 => std::slice::from_raw_parts_mut(out, n),
            _ => &mut [],
        }
    };
    // SAFETY: caller contract.
    let count = scenemgr::gui_order(unsafe { slice(prios, n) }, out);
    i32::try_from(count).unwrap_or(0)
}
