//! C ABI for `resources.rs`, mirrored in `HPL2/core/include/resources/RustResources.h`.
//! Handles are owned by the C++ object that created them; resource and loader pointers are opaque.

use crate::resources::{ExtList, FileSearcher, LoaderTable, MapEntry, ResMap, ResMeta};
use crate::string_ffi::units;
use std::ffi::c_void;

type Matches = unsafe extern "C" fn(*mut c_void, *mut c_void) -> bool;

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_fs_new() -> *mut FileSearcher {
    Box::into_raw(Box::default())
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_resmap_new() -> *mut ResMap {
    Box::into_raw(Box::default())
}

/// # Safety
/// `m` comes from `ohpl_resmap_new` and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_resmap_free(m: *mut ResMap) {
    // SAFETY: caller contract.
    drop(unsafe { Box::from_raw(m) });
}

/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_resmap_len(m: *const ResMap) -> usize {
    // SAFETY: caller contract.
    unsafe { &*m }.len()
}

/// Entry `i`, valid until the next insert or remove; null past the end.
///
/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_resmap_at(m: *mut ResMap, i: usize) -> *mut MapEntry {
    // SAFETY: caller contract.
    unsafe { &mut *m }.at(i)
}

/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_resmap_insert(m: *mut ResMap, hash: u32, res: *mut c_void) {
    // SAFETY: caller contract.
    unsafe { &mut *m }.insert(hash, res);
}

/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_resmap_remove(m: *mut ResMap, hash: u32, res: *mut c_void) -> bool {
    // SAFETY: caller contract.
    unsafe { &mut *m }.remove(hash, res)
}

/// First resource under `hash` for which `matches(ctx, res)` holds, or null.
///
/// # Safety
/// `m` live; `matches` accepts `ctx` and must not touch the map.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_resmap_find(
    m: *const ResMap,
    hash: u32,
    matches: Matches,
    ctx: *mut c_void,
) -> *mut c_void {
    // SAFETY: caller contract.
    unsafe { &*m }.find(hash, |r| unsafe { matches(ctx, r) })
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_exts_new() -> *mut ExtList {
    Box::into_raw(Box::default())
}

/// # Safety
/// `e` comes from `ohpl_exts_new` and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_exts_free(e: *mut ExtList) {
    // SAFETY: caller contract.
    drop(unsafe { Box::from_raw(e) });
}

/// # Safety
/// `e` live; `s` valid for `n`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_exts_add(e: *mut ExtList, s: *const u8, n: usize) {
    // SAFETY: caller contract.
    unsafe { &mut *e }.0.push(unsafe { units(s, n) }.to_vec());
}

/// # Safety
/// `e` live; `s` valid for `n`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_exts_supports(e: *const ExtList, s: *const u8, n: usize) -> bool {
    // SAFETY: caller contract.
    unsafe { &*e }.supports(unsafe { units(s, n) })
}

/// # Safety
/// `e` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_exts_len(e: *const ExtList) -> usize {
    // SAFETY: caller contract.
    unsafe { &*e }.0.len()
}

/// Extension `i` as (pointer, length), valid until the list changes.
///
/// # Safety
/// `e` live; `n` writable. Out-of-range `i` yields length 0.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_exts_at(e: *const ExtList, i: usize, n: *mut usize) -> *const u8 {
    // SAFETY: caller contract.
    let s = unsafe { &*e }.0.get(i).map_or(&[][..], Vec::as_slice);
    // SAFETY: caller contract.
    unsafe { *n = s.len() };
    s.as_ptr()
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_loaders_new() -> *mut LoaderTable {
    Box::into_raw(Box::default())
}

/// # Safety
/// `t` comes from `ohpl_loaders_new` and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_loaders_free(t: *mut LoaderTable) {
    // SAFETY: caller contract.
    drop(unsafe { Box::from_raw(t) });
}

/// # Safety
/// `t` live; `exts` outlives the table's lookups.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_loaders_add(
    t: *mut LoaderTable,
    loader: *mut c_void,
    exts: *const ExtList,
) {
    // SAFETY: caller contract.
    unsafe { (&mut *t).add(loader, exts) };
}

/// # Safety
/// `t` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_loaders_len(t: *const LoaderTable) -> usize {
    // SAFETY: caller contract.
    unsafe { &*t }.len()
}

/// # Safety
/// `t` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_loaders_at(t: *const LoaderTable, i: usize) -> *mut c_void {
    // SAFETY: caller contract.
    unsafe { &*t }.at(i)
}

/// # Safety
/// `m` is writable storage for a `ResMeta`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_resmeta_init(m: *mut ResMeta, prio: u32, now: u64) {
    // SAFETY: caller contract.
    unsafe { m.write(ResMeta::new(prio, now)) };
}

/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_resmeta_inc_user(m: *mut ResMeta, now: u64) {
    // SAFETY: caller contract.
    unsafe { &mut *m }.inc_user(now);
}

/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_resmeta_dec_user(m: *mut ResMeta) {
    // SAFETY: caller contract.
    unsafe { &mut *m }.dec_user();
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn exts_at_out_of_range_is_empty() {
        let e = ohpl_exts_new();
        let mut n = 7;
        // SAFETY: `e` is live and `n` is a local.
        unsafe {
            ohpl_exts_add(e, b"png".as_ptr(), 3);
            assert!(!ohpl_exts_at(e, 0, &mut n).is_null());
            assert_eq!(n, 3);
            ohpl_exts_at(e, 1, &mut n);
            assert_eq!(n, 0);
            ohpl_exts_free(e);
        }
    }
}
