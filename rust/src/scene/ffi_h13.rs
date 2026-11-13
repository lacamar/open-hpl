//! C ABI for `h13_world.rs`, declared in `HPL2/core/include/scene/RustSceneH13.h`.
//!
//! `reg` is a live registry from `ohpl_h13_reg_new`. Entity addresses are opaque. Accessor
//! callbacks only read an entity's name or id and never reach the registry.

use super::h13_world::{BodyItem, MeshItem, Reg, plan_bodies, plan_meshes};
use std::ffi::c_void;

const _: () = {
    use std::mem::size_of;
    assert!(size_of::<Name>() == 16);
    assert!(size_of::<MeshItem>() == 16 && size_of::<BodyItem>() == 16);
};

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Name {
    ptr: *const u8,
    len: usize,
}

type NameFn = unsafe extern "C" fn(*const c_void, *mut usize) -> *const u8;
type IdFn = unsafe extern "C" fn(*const c_void) -> i32;

/// # Safety
/// `p` valid for `n` reads when `n > 0`.
unsafe fn slice<'a, T>(p: *const T, n: usize) -> &'a [T] {
    if n == 0 || p.is_null() {
        return &[];
    }
    // SAFETY: caller contract.
    unsafe { std::slice::from_raw_parts(p, n) }
}

/// # Safety
/// `n.ptr` valid for `n.len` reads.
unsafe fn bytes<'a>(n: Name) -> &'a [u8] {
    // SAFETY: caller contract.
    unsafe { slice(n.ptr, n.len) }
}

/// # Safety
/// `f` valid; `p` is an entity of the registry's kind.
unsafe fn name_of<'a>(f: NameFn, p: usize) -> &'a [u8] {
    let mut len = 0usize;
    // SAFETY: caller contract; the name outlives the lookup.
    unsafe {
        let ptr = f(p as *const c_void, &mut len);
        slice(ptr, len)
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h13_reg_new() -> *mut Reg {
    Box::into_raw(Box::default())
}

/// # Safety
/// `reg` from `ohpl_h13_reg_new`, not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h13_reg_free(reg: *mut Reg) {
    if !reg.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(reg) });
    }
}

/// # Safety
/// `reg` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h13_reg_add(reg: *mut Reg, kind: u32, p: *mut c_void) {
    // SAFETY: caller contract.
    unsafe { (*reg).add(kind as usize, p as usize) }
}

/// # Safety
/// `reg` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h13_reg_remove(reg: *mut Reg, kind: u32, p: *mut c_void) -> bool {
    // SAFETY: caller contract.
    unsafe { (*reg).remove_first(kind as usize, p as usize) }
}

/// # Safety
/// `reg` live; `set` valid for `n` reads.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h13_reg_remove_set(
    reg: *mut Reg,
    kind: u32,
    set: *const usize,
    n: usize,
) {
    // SAFETY: caller contract.
    unsafe { (*reg).remove_set(kind as usize, slice(set, n)) }
}

/// # Safety
/// `reg` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h13_reg_make_dynamic(reg: *mut Reg, p: *mut c_void) {
    // SAFETY: caller contract.
    unsafe { (*reg).make_dynamic(p as usize) }
}

/// # Safety
/// `reg` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h13_reg_clear(reg: *mut Reg, kind: u32) {
    // SAFETY: caller contract.
    unsafe { (*reg).clear(kind as usize) }
}

/// # Safety
/// `reg` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h13_reg_len(reg: *const Reg, kind: u32) -> usize {
    // SAFETY: caller contract.
    unsafe { (*reg).len(kind as usize) }
}

/// # Safety
/// `reg` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h13_reg_get(reg: *const Reg, kind: u32, i: usize) -> *mut c_void {
    // SAFETY: caller contract.
    unsafe { (*reg).get(kind as usize, i) as *mut c_void }
}

/// # Safety
/// `reg` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h13_reg_index(reg: *const Reg, kind: u32, p: *const c_void) -> i32 {
    // SAFETY: caller contract.
    unsafe { (*reg).index_of(kind as usize, p as usize) }
}

/// # Safety
/// `reg` live; `key` valid; `f` valid for every entity of the kind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h13_reg_find_name(
    reg: *const Reg,
    kind: u32,
    key: Name,
    last: bool,
    f: NameFn,
) -> *mut c_void {
    // SAFETY: caller contract.
    unsafe { (*reg).find_name(kind as usize, bytes(key), last, |p| name_of(f, p)) as *mut c_void }
}

/// # Safety
/// `reg` live; `f` valid for every entity of the kind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h13_reg_find_id(
    reg: *const Reg,
    kind: u32,
    id: i32,
    f: IdFn,
) -> *mut c_void {
    // SAFETY: caller contract.
    unsafe { (*reg).find_id(kind as usize, id, |p| f(p as *const c_void)) as *mut c_void }
}

/// # Safety
/// `reg` live; `f` valid for every entity of the kind.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h13_reg_first_by_name(
    reg: *const Reg,
    kind: u32,
    f: NameFn,
) -> *mut c_void {
    // SAFETY: caller contract.
    unsafe { (*reg).first_by_name(kind as usize, |p| name_of(f, p)) as *mut c_void }
}

/// # Safety
/// `reg` live; `name` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h13_area_insert(reg: *mut Reg, name: Name, p: *mut c_void) -> bool {
    // SAFETY: caller contract.
    unsafe { (*reg).area_insert(bytes(name), p as usize) }
}

/// # Safety
/// `reg` live; `name` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h13_area_get(reg: *const Reg, name: Name) -> *mut c_void {
    // SAFETY: caller contract.
    unsafe { (*reg).area_get(bytes(name)) as *mut c_void }
}

/// # Safety
/// `reg` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h13_area_clear(reg: *mut Reg) {
    // SAFETY: caller contract.
    unsafe { (*reg).area_clear() }
}

/// Writes at most `cap` `[first, last]` pairs to `out`; returns the pair count.
///
/// # Safety
/// `items` valid for `n` reads; `out` for `2 * cap` writes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h13_plan_meshes(
    items: *const MeshItem,
    n: usize,
    out: *mut i32,
    cap: usize,
) -> usize {
    // SAFETY: caller contract.
    unsafe { write_plan(plan_meshes(slice(items, n)), out, cap) }
}

/// # Safety
/// `items` valid for `n` reads; `out` for `2 * cap` writes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h13_plan_bodies(
    items: *const BodyItem,
    n: usize,
    mesh_count: usize,
    out: *mut i32,
    cap: usize,
) -> usize {
    // SAFETY: caller contract.
    unsafe { write_plan(plan_bodies(slice(items, n), mesh_count), out, cap) }
}

/// # Safety
/// `out` valid for `2 * cap` writes.
unsafe fn write_plan(plan: Vec<(i32, i32)>, out: *mut i32, cap: usize) -> usize {
    let n = plan.len().min(cap);
    for (k, &(a, b)) in plan.iter().take(n).enumerate() {
        // SAFETY: caller contract, `k < cap`.
        unsafe {
            *out.add(2 * k) = a;
            *out.add(2 * k + 1) = b;
        }
    }
    n
}
