//! C ABI for `soundentity.rs`, `world.rs` and `scenemgr.rs`, declared in
//! `HPL2/core/include/scene/RustWorld.h`.
//!
//! Pointers are non-null, valid and aligned for the call; outputs do not alias inputs. Arrays
//! are valid for the stated count. Vectors and colours cross as pointers.

use super::scenemgr::ViewportIn;
use super::world::{self, FieldIn};
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
