//! C ABI for `script`, declared in `HPL2/core/include/impl/RustL7.h`.

use super::engine::ScriptEngine;
use super::selftest;
use super::sqscript::{BuildError, Script};
use crate::string_ffi::{Sink, emit, units};
use std::ffi::{CStr, c_char, c_void};

fn text<'a>(p: *const c_char) -> &'a str {
    if p.is_null() {
        return "";
    }
    // SAFETY: callers pass NUL-terminated strings.
    unsafe { CStr::from_ptr(p) }.to_str().unwrap_or("")
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_l7_engine_new() -> *mut ScriptEngine {
    ScriptEngine::new().map_or(std::ptr::null_mut(), |e| Box::into_raw(Box::new(e)))
}

/// # Safety
/// `e` from `ohpl_l7_engine_new`, freed once, after all its scripts.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l7_engine_free(e: *mut ScriptEngine) {
    if !e.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(e) });
    }
}

/// The `asIScriptEngine*` for C++ code that registers the game API.
///
/// # Safety
/// `e` from `ohpl_l7_engine_new`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l7_engine_raw(e: *const ScriptEngine) -> *mut c_void {
    // SAFETY: caller contract.
    unsafe { e.as_ref() }.map_or(std::ptr::null_mut(), |e| e.engine().as_raw())
}

/// # Safety
/// `e` from `ohpl_l7_engine_new`; `decl` NUL-terminated.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l7_add_func(
    e: *const ScriptEngine,
    decl: *const c_char,
    f: *const c_void,
) -> bool {
    // SAFETY: caller contract.
    unsafe { e.as_ref() }.is_some_and(|e| unsafe { e.add_func(text(decl), f) })
}

/// # Safety
/// `e` from `ohpl_l7_engine_new`; `decl` NUL-terminated.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l7_add_var(
    e: *const ScriptEngine,
    decl: *const c_char,
    p: *mut c_void,
) -> bool {
    // SAFETY: caller contract.
    unsafe { e.as_ref() }.is_some_and(|e| unsafe { e.add_var(text(decl), p) })
}

/// # Safety
/// `e` from `ohpl_l7_engine_new`, outliving the script.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l7_script_new(
    e: *const ScriptEngine,
    handle: i32,
    rand: i32,
) -> *mut Script {
    // SAFETY: caller contract; the `Output` box has a stable address.
    let Some(e) = (unsafe { e.as_ref() }) else {
        return std::ptr::null_mut();
    };
    // SAFETY: the engine owner outlives its scripts.
    let s = unsafe { Script::new(e.engine(), std::ptr::from_ref(&*e.output), handle, rand) };
    Box::into_raw(Box::new(s))
}

/// # Safety
/// `s` from `ohpl_l7_script_new`, freed once.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l7_script_free(s: *mut Script) {
    if !s.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(s) });
    }
}

/// 0 ok, 1 `AddScriptSection` failed, 2 build failed (compiler output chunks go to `chunk_sink`).
/// `msg_sink`, when set, receives the compile messages as `apCompileMessages` does.
///
/// # Safety
/// `s` live; `src` valid for `len`; sinks accept their `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l7_script_build(
    s: *mut Script,
    src: *const u8,
    len: usize,
    msg_sink: Option<Sink<u8>>,
    msg_ctx: *mut c_void,
    chunk_sink: Sink<u8>,
    chunk_ctx: *mut c_void,
) -> i32 {
    // SAFETY: caller contract.
    let (Some(s), src) = (unsafe { s.as_mut() }, unsafe { units(src, len) }) else {
        return 1;
    };
    let (msgs, res) = s.build(src);
    if let Some(sink) = msg_sink {
        // SAFETY: caller contract.
        unsafe { emit(sink, msg_ctx, &msgs) };
    }
    match res {
        Ok(()) => 0,
        Err(BuildError::Section) => 1,
        Err(BuildError::Build { chunks }) => {
            for c in &chunks {
                // SAFETY: caller contract.
                unsafe { emit(chunk_sink, chunk_ctx, c) };
            }
            2
        }
    }
}

/// # Safety
/// `s` live; `name` NUL-terminated.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l7_script_func_handle(s: *mut Script, name: *const c_char) -> i32 {
    // SAFETY: caller contract.
    unsafe { s.as_mut() }.map_or(-1, |s| s.func_handle(text(name)))
}

/// # Safety
/// `s` live; `line` NUL-terminated.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l7_script_run_line(s: *mut Script, line: *const c_char) -> bool {
    // SAFETY: caller contract.
    unsafe { s.as_mut() }.is_some_and(|s| s.run_line(text(line)))
}

/// # Safety
/// `s` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l7_script_run_handle(s: *mut Script, handle: i32) -> bool {
    // SAFETY: caller contract.
    unsafe { s.as_mut() }.is_some_and(|s| s.run_handle(handle))
}

/// Registers a function, property, enum, value type with a method, ref type and strings through the Rust
/// API on a fresh engine, runs a script against them. 0 on success, else a bitmask of failed checks.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_l7_selftest() -> u32 {
    selftest::run()
}
