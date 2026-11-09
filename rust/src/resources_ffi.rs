//! C ABI for `resources.rs`, mirrored in `HPL2/core/include/resources/RustResources.h`.
//! Handles are owned by the C++ object that created them; resource and loader pointers are opaque.

use crate::resources::{
    ExtList, FileSearcher, LoaderTable, MapEntry, ResMap, ResMeta, eviction_order,
};
use crate::string_ffi::{Sink, emit, units};
use std::ffi::c_void;

type Matches = unsafe extern "C" fn(*mut c_void, *mut c_void) -> bool;
type SameAs = unsafe extern "C" fn(*mut c_void, u32) -> bool;
type UseKey = unsafe extern "C" fn(*mut c_void, *mut c_void, *mut u32, *mut u64);

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_fs_new() -> *mut FileSearcher {
    Box::into_raw(Box::default())
}

/// # Safety
/// `fs` comes from `ohpl_fs_new` and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_fs_free(fs: *mut FileSearcher) {
    // SAFETY: caller contract.
    drop(unsafe { Box::from_raw(fs) });
}

/// Returns the new entry id, or -1 when `same_as(ctx, id)` says the first entry under `key` has this path.
///
/// # Safety
/// `fs` live; `key` and `path` valid for their lengths; `same_as` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_fs_add(
    fs: *mut FileSearcher,
    key: *const u8,
    key_len: usize,
    path: *const u32,
    path_len: usize,
    same_as: SameAs,
    ctx: *mut c_void,
) -> i64 {
    // SAFETY: caller contract.
    let (fs, key, path) = unsafe { (&mut *fs, units(key, key_len), units(path, path_len)) };
    // SAFETY: caller contract.
    fs.add(key, path, |id| unsafe { same_as(ctx, id) })
        .map_or(-1, i64::from)
}

/// # Safety
/// `fs` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_fs_clear(fs: *mut FileSearcher) {
    // SAFETY: caller contract.
    unsafe { &mut *fs }.clear();
}

/// Entry id, or -1 when the name is unknown. `equal_count` (nullable) is written as C++ does.
///
/// # Safety
/// `fs` live; `name` valid for `n`; `equal_count` null or writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_fs_get(
    fs: *const FileSearcher,
    name: *const u8,
    n: usize,
    equal_count: *mut i32,
) -> i64 {
    // SAFETY: caller contract.
    let (fs, name) = unsafe { (&*fs, units(name, n)) };
    let found = fs.get(name, !equal_count.is_null());
    let Some(found) = found else {
        if !equal_count.is_null() {
            // SAFETY: caller contract.
            unsafe { *equal_count = 0 };
        }
        return -1;
    };
    if let Some(c) = found.equal_count {
        // SAFETY: only `Some` when `equal_count` is non-null.
        unsafe { *equal_count = c };
    }
    i64::from(found.id)
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

/// Emits every resource in `DestroyUnused` order; `key` fills (user count, time) for one resource.
///
/// # Safety
/// `m` live; `key` accepts `kctx` and must not touch the map; `sink` accepts `sctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_resmap_eviction_order(
    m: *const ResMap,
    key: UseKey,
    kctx: *mut c_void,
    sink: Sink<*mut c_void>,
    sctx: *mut c_void,
) {
    // SAFETY: caller contract.
    let m = unsafe { &*m };
    let order = eviction_order(m, |r| {
        let (mut users, mut time) = (0, 0);
        // SAFETY: caller contract; the out pointers are locals.
        unsafe { key(kctx, r, &mut users, &mut time) };
        (users, time)
    });
    // SAFETY: caller contract.
    unsafe { emit(sink, sctx, &order) };
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

/// First loader supporting `ext` (already lower case), or null.
///
/// # Safety
/// `t` live with live extension lists; `ext` valid for `n`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_loaders_find(
    t: *const LoaderTable,
    ext: *const u8,
    n: usize,
) -> *mut c_void {
    // SAFETY: caller contract.
    unsafe { (&*t).find(units(ext, n)) }
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
