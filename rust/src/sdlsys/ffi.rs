//! C ABI for `sdlsys`, declared in `HPL2/core/include/impl/RustImpl.h`.

use super::{
    Arg, EnvInputs, classify_arg, engine_env, looks_like_steam_install, remount_flags,
    renderer_is_vulkan, script_chunks, script_message, steam_root, vdf_path,
};
use crate::string_ffi::{Sink, emit, units};
use std::ffi::{CStr, c_char, c_void};

/// # Safety
/// `p` is null or NUL-terminated.
unsafe fn cstr<'a>(p: *const c_char) -> Option<&'a [u8]> {
    // SAFETY: caller contract.
    unsafe { p.as_ref() }.map(|_| unsafe { CStr::from_ptr(p) }.to_bytes())
}

/// Emits the root through `sink`; false when `real` is not under steamapps/common.
///
/// # Safety
/// `real` NUL-terminated; `sink` accepts `ctx` and a slice valid for the call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sys_steam_root(
    real: *const c_char,
    sink: Sink<u8>,
    ctx: *mut c_void,
) -> bool {
    // SAFETY: caller contract.
    let Some(root) = unsafe { cstr(real) }.and_then(steam_root) else {
        return false;
    };
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, &root) };
    true
}

/// Emits the `path` value of a libraryfolders.vdf line; false when the line has none.
///
/// # Safety
/// `line` valid for `len` reads; `sink` accepts `ctx` and a slice valid for the call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sys_vdf_path(
    line: *const u8,
    len: usize,
    sink: Sink<u8>,
    ctx: *mut c_void,
) -> bool {
    // SAFETY: caller contract.
    let line = unsafe { units(line, len) };
    let Some(path) = vdf_path(line) else {
        return false;
    };
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, path) };
    true
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

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_sys_remount_flags(st_flag: u64) -> u64 {
    remount_flags(st_flag)
}

/// 0 `-cwd`, 1 skip, 2 game dir candidate, 3 command line word.
///
/// # Safety
/// `arg` NUL-terminated.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sys_classify_arg(arg: *const c_char, got_game_dir: bool) -> u32 {
    // SAFETY: caller contract.
    match classify_arg(unsafe { cstr(arg) }.unwrap_or_default(), got_game_dir) {
        Arg::Cwd => 0,
        Arg::Skip => 1,
        Arg::GameDirCandidate => 2,
        Arg::Command => 3,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_sys_engine_env(
    headless: bool,
    alsoft_set: bool,
    video: bool,
    videodriver_set: bool,
    display_set: bool,
) -> u32 {
    engine_env(&EnvInputs {
        headless,
        alsoft_set,
        video,
        videodriver_set,
        display_set,
    })
}

/// # Safety
/// `name` null or NUL-terminated.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sys_renderer_is_vulkan(name: *const c_char) -> bool {
    // SAFETY: caller contract.
    unsafe { cstr(name) }.is_some_and(renderer_is_vulkan)
}

/// # Safety
/// `section` and `message` null or NUL-terminated; `sink` accepts `ctx` and a slice valid for the call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sys_script_message(
    section: *const c_char,
    row: i32,
    col: i32,
    kind: u32,
    message: *const c_char,
    sink: Sink<u8>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract; glibc prints a null %s as "(null)".
    let (section, message) = unsafe {
        (
            cstr(section).unwrap_or(b"(null)"),
            cstr(message).unwrap_or(b"(null)"),
        )
    };
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, &script_message(section, row, col, kind, message)) };
}

/// Emits each piece `cScriptOutput::Display` logs.
///
/// # Safety
/// `msg` valid for `len` reads; `sink` accepts `ctx` and a slice valid for the call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sys_script_chunks(
    msg: *const u8,
    len: usize,
    sink: Sink<u8>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    let msg = unsafe { units(msg, len) };
    for chunk in script_chunks(msg) {
        // SAFETY: caller contract.
        unsafe { emit(sink, ctx, chunk) };
    }
}
