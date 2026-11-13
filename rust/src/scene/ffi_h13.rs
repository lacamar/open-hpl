//! C ABI for `h13_world.rs`, declared in `HPL2/core/include/scene/RustSceneH13.h`.
//!
//! `reg` is a live registry from `ohpl_h13_reg_new`. Entity addresses are opaque. Accessor
//! callbacks only read an entity's name or id and never reach the registry.

use super::h13_world::{AREA, BodyItem, EntityRef, MeshItem, Reg, plan_bodies, plan_meshes};
use crate::obj::{DropFn, Foreign, IdFn as FIdFn, NameFn as FNameFn};
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

/// Registers the C++ shell's virtuals for `kind` (`AREA` for the area map; only `drop` is used).
///
/// # Safety
/// `reg` live; the thunks stay valid for the registry's life.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h13_reg_set_foreign(
    reg: *mut Reg,
    kind: u32,
    name: Option<FNameFn>,
    id: Option<FIdFn>,
    drop: Option<DropFn>,
) {
    // SAFETY: caller contract.
    unsafe { (*reg).set_foreign(kind as usize, Foreign { name, id, drop }) }
}

/// # Safety
/// `reg` live; `key` valid; the kind's `name` thunk registered when the list is non-empty.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h13_reg_find_name_f(
    reg: *const Reg,
    kind: u32,
    key: Name,
    last: bool,
) -> *mut c_void {
    // SAFETY: caller contract; the thunk only reads a name and never reaches the registry.
    unsafe {
        let f = (*reg).foreign(kind as usize);
        (*reg).find_name(kind as usize, bytes(key), last, |p| f.name_of(p)) as *mut c_void
    }
}

/// # Safety
/// As `ohpl_h13_reg_find_name_f` for the `id` thunk.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h13_reg_find_id_f(
    reg: *const Reg,
    kind: u32,
    id: i32,
) -> *mut c_void {
    // SAFETY: caller contract.
    unsafe {
        let f = (*reg).foreign(kind as usize);
        (*reg).find_id(kind as usize, id, |p| f.id_of(p)) as *mut c_void
    }
}

/// # Safety
/// As `ohpl_h13_reg_find_name_f`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h13_reg_first_by_name_f(reg: *const Reg, kind: u32) -> *mut c_void {
    // SAFETY: caller contract.
    unsafe {
        let f = (*reg).foreign(kind as usize);
        (*reg).first_by_name(kind as usize, |p| f.name_of(p)) as *mut c_void
    }
}

/// `STLFindAndDelete` (`only_if_found` false) or erase-then-delete-if-found: unlinks the first
/// match, then runs the destructor with no registry borrow held. Returns whether it was linked.
///
/// # Safety
/// `reg` live; `p` a live object of the kind (or, with `only_if_found`, possibly dangling).
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h13_reg_destroy(
    reg: *mut Reg,
    kind: u32,
    p: *mut c_void,
    only_if_found: bool,
) -> bool {
    // SAFETY: caller contract; each registry call ends before the destructor runs.
    unsafe {
        let found = (*reg).remove_first(kind as usize, p as usize);
        if found || !only_if_found {
            (*reg).foreign(kind as usize).destroy(p as usize);
        }
        found
    }
}

/// `STLDeleteAll`: destructors in creation order while the list is still populated, then clear.
///
/// # Safety
/// `reg` live; every linked object of the kind is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h13_reg_destroy_all(reg: *mut Reg, kind: u32) {
    // SAFETY: caller contract; destructors may re-enter the registry, so only the snapshot and
    // the copied vtable are held across them.
    unsafe {
        let f = (*reg).foreign(kind as usize);
        for p in (*reg).snapshot(kind as usize) {
            f.destroy(p);
        }
        (*reg).clear(kind as usize);
    }
}

/// `STLMapDeleteAll` over the area map (key order).
///
/// # Safety
/// `reg` live; every area is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h13_area_destroy_all(reg: *mut Reg) {
    // SAFETY: caller contract.
    unsafe {
        let f = (*reg).foreign(AREA);
        for (_, p) in (*reg).areas() {
            f.destroy(p);
        }
        (*reg).area_clear();
    }
}

/// # Safety
/// `reg` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h13_area_len(reg: *const Reg) -> usize {
    // SAFETY: caller contract.
    unsafe { (*reg).areas().len() }
}

/// The `i`-th area in key order; the key bytes stay valid until the map changes.
///
/// # Safety
/// `reg` live; `key` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h13_area_at(
    reg: *const Reg,
    i: usize,
    key: *mut Name,
) -> *mut c_void {
    // SAFETY: caller contract.
    unsafe {
        (*reg).area_at(i, |k| {
            *key = Name {
                ptr: k.as_ptr(),
                len: k.len(),
            }
        }) as *mut c_void
    }
}

/// First position of the kind's list as cursor bits; 0 when empty.
///
/// # Safety
/// `reg` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h13_cur_begin(reg: *const Reg, kind: u32) -> u64 {
    // SAFETY: caller contract.
    unsafe { (*reg).begin(kind as usize).map_or(0, EntityRef::to_bits) }
}

/// The entity at a cursor; null for 0, an erased or a cleared position.
///
/// # Safety
/// `reg` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h13_cur_get(reg: *const Reg, kind: u32, cur: u64) -> *mut c_void {
    // SAFETY: caller contract.
    unsafe {
        EntityRef::from_bits(kind as usize, cur).map_or(0, |at| (*reg).resolve(at)) as *mut c_void
    }
}

/// # Safety
/// `reg` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h13_cur_next(reg: *const Reg, kind: u32, cur: u64) -> u64 {
    // SAFETY: caller contract.
    unsafe {
        EntityRef::from_bits(kind as usize, cur)
            .and_then(|at| (*reg).next(at))
            .map_or(0, EntityRef::to_bits)
    }
}

/// Unlinks the entity at a cursor without destroying it; returns the next cursor.
///
/// # Safety
/// `reg` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h13_cur_erase(reg: *mut Reg, kind: u32, cur: u64) -> u64 {
    // SAFETY: caller contract.
    unsafe {
        EntityRef::from_bits(kind as usize, cur)
            .and_then(|at| (*reg).erase(at))
            .map_or(0, EntityRef::to_bits)
    }
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
