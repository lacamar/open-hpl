//! C ABI for `sdlsys`, declared in `HPL2/core/include/impl/RustImpl.h`.

use super::{looks_like_steam_install, renderer_is_vulkan};
use crate::string_ffi::units;
use std::ffi::{CStr, c_char};

/// # Safety
/// `p` is null or NUL-terminated.
unsafe fn cstr<'a>(p: *const c_char) -> Option<&'a [u8]> {
    // SAFETY: caller contract.
    unsafe { p.as_ref() }.map(|_| unsafe { CStr::from_ptr(p) }.to_bytes())
}

/// # Safety
/// `path` valid for `len` reads; `cwd` null or NUL-terminated.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sys_looks_like_steam_install(
    path: *const u8,
    len: usize,
    cwd: *const c_char,
) -> bool {
    // SAFETY: caller contract.
    let path = unsafe { units(path, len) };
    // SAFETY: caller contract.
    looks_like_steam_install(path, unsafe { cstr(cwd) })
}

/// # Safety
/// `name` null or NUL-terminated.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sys_renderer_is_vulkan(name: *const c_char) -> bool {
    // SAFETY: caller contract.
    unsafe { cstr(name) }.is_some_and(renderer_is_vulkan)
}
