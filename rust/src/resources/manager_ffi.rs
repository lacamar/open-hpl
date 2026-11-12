//! C ABI for the H3 manager core, mirrored in `HPL2/core/include/resources/RustResourcesH3.h`.

use super::manager::{self, Manager};
use crate::resources::{ResMap, ResMeta};
use crate::string_ffi::{emit, units};
use std::ffi::c_void;

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h3_mgr_new() -> *mut Manager {
    Box::into_raw(Box::default())
}

/// # Safety
/// `m` comes from `ohpl_h3_mgr_new` and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h3_mgr_free(m: *mut Manager) {
    // SAFETY: caller contract.
    drop(unsafe { Box::from_raw(m) });
}

/// Borrowed view for `cResourceMap`; valid while `m` lives. Read-only for C++.
///
/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h3_mgr_map(m: *mut Manager) -> *mut ResMap {
    // SAFETY: caller contract.
    unsafe { &mut *m }.map_mut()
}

/// # Safety
/// `m` live; `path` valid for `n` wide units; `meta` stays valid until `ohpl_h3_mgr_remove`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h3_mgr_add(
    m: *mut Manager,
    res: *mut c_void,
    path: *const u32,
    n: usize,
    meta: *const ResMeta,
) {
    // SAFETY: caller contract.
    unsafe { (&mut *m).add(res, units(path, n), meta) };
}

/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h3_mgr_remove(m: *mut Manager, res: *mut c_void) -> bool {
    // SAFETY: caller contract.
    unsafe { &mut *m }.remove(res)
}

/// # Safety
/// `m` live; `path` valid for `n` wide units.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h3_mgr_get(
    m: *const Manager,
    path: *const u32,
    n: usize,
) -> *mut c_void {
    // SAFETY: caller contract.
    unsafe { (&*m).get(units(path, n)) }
}

/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h3_mgr_len(m: *const Manager) -> usize {
    // SAFETY: caller contract.
    unsafe { &*m }.len()
}

/// First resource in map order, null when empty.
///
/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h3_mgr_first(m: *mut Manager) -> *mut c_void {
    // SAFETY: caller contract.
    unsafe { &mut *m }.first()
}

/// Sends the eviction candidates past `keep` (empty within budget).
///
/// # Safety
/// `m` live; `sink` accepts `ctx` and a pointer slice.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h3_mgr_eviction_tail(
    m: *const Manager,
    keep: i64,
    sink: unsafe extern "C" fn(*mut c_void, *const *mut c_void, usize),
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    let v = unsafe { &*m }.eviction_tail(keep);
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, &v) };
}

/// # Safety
/// `name` valid for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h3_begin_load(name: *const u8, n: usize, now: u64) {
    // SAFETY: caller contract.
    manager::begin_load(unsafe { units(name, n) }, now);
}

/// True when the outermost load ended; the name goes to `sink`, the elapsed time to `elapsed`.
///
/// # Safety
/// `sink` accepts `ctx`; `elapsed` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h3_end_load(
    now: u64,
    sink: unsafe extern "C" fn(*mut c_void, *const u8, usize),
    ctx: *mut c_void,
    elapsed: *mut u64,
) -> bool {
    let Some(o) = manager::end_load(now) else {
        return false;
    };
    // SAFETY: caller contract.
    unsafe {
        emit(sink, ctx, &o.name);
        *elapsed = o.elapsed;
    }
    true
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h3_tab_count() -> i32 {
    manager::tab_count()
}
