//! C ABI for the H3 manager core, mirrored in `HPL2/core/include/resources/RustResourcesH3.h`.

use super::manager::Manager;
use crate::resources::ResMap;
use crate::string_ffi::units;
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
