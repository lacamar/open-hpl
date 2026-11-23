//! C ABI for `runtime.rs` and `check.rs`.

use super::ffi_api::OhplGs6Str;
use super::runtime::{self, ApiLookup};
use crate::string_ffi::{Sink, emit, units};

use std::ffi::c_void;

/// A nullable string: a null pointer means "absent".
unsafe fn opt<'a>(p: *const u8, n: usize) -> Option<&'a [u8]> {
    // SAFETY: caller contract.
    (!p.is_null()).then(|| unsafe { units(p, n) })
}

/// # Safety
/// Each pointer is null or valid for its length; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_find_api_file(
    game: OhplGs6Str,
    current_game: OhplGs6Str,
    env: OhplGs6Str,
    exe: OhplGs6Str,
    source_dir: OhplGs6Str,
    sink: Sink<u8>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    let q = unsafe {
        ApiLookup {
            game: units(game.ptr, game.len),
            current_game: units(current_game.ptr, current_game.len),
            env: opt(env.ptr, env.len),
            exe: opt(exe.ptr, exe.len),
            source_dir: opt(source_dir.ptr, source_dir.len),
        }
    };
    let r = runtime::find_api_file(&q);
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, &r) }
}

/// Returns true if `decl` named a global (`$`) function; `sink` gets the declaration without the marker.
///
/// # Safety
/// `decl` valid for `n` bytes; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_split_global_marker(
    decl: *const u8,
    n: usize,
    sink: Sink<u8>,
    ctx: *mut c_void,
) -> bool {
    // SAFETY: caller contract.
    let (global, d) = runtime::split_global_marker(unsafe { units(decl, n) });
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, &d) };
    global
}

/// Returns true for a `$Func` name; the sinks get the function name and its two declarations.
///
/// # Safety
/// `name` valid for `n` bytes; sinks accept `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_callback_decls(
    name: *const u8,
    n: usize,
    func: Sink<u8>,
    with_arg: Sink<u8>,
    no_arg: Sink<u8>,
    ctx: *mut c_void,
) -> bool {
    // SAFETY: caller contract.
    let d = runtime::callback_decls(unsafe { units(name, n) });
    // SAFETY: caller contract.
    unsafe {
        emit(func, ctx, &d.func);
        emit(with_arg, ctx, &d.with_arg);
        emit(no_arg, ctx, &d.no_arg);
    }
    d.global
}

/// # Safety
/// `code` valid for `n` bytes; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_exec_source(
    code: *const u8,
    n: usize,
    sink: Sink<u8>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    let r = runtime::exec_source(unsafe { units(code, n) });
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, &r) }
}

/// # Safety
/// `msg` valid for `n` bytes; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_exec_message(
    msg: *const u8,
    n: usize,
    row: i32,
    col: i32,
    sink: Sink<u8>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    let r = runtime::exec_message(unsafe { units(msg, n) }, row, col);
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, &r) }
}

/// # Safety
/// `keys` holds `n` valid strings; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_script_files_error(
    keys: *const OhplGs6Str,
    n: usize,
    sink: Sink<u8>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    let keys: Vec<&[u8]> = unsafe { units(keys, n) }
        .iter()
        // SAFETY: caller contract.
        .map(|k| unsafe { units(k.ptr, k.len) })
        .collect();
    let r = runtime::script_files_error(&keys);
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, &r) }
}

/// Index of the first loaded module whose key contains `needle`, or -1.
///
/// # Safety
/// `keys` and `loaded` hold `n` elements; `needle` valid for `needle_len` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_pick_module(
    keys: *const OhplGs6Str,
    loaded: *const bool,
    n: usize,
    needle: *const u8,
    needle_len: usize,
) -> isize {
    // SAFETY: caller contract.
    let (keys, loaded, needle) =
        unsafe { (units(keys, n), units(loaded, n), units(needle, needle_len)) };
    let mods: Vec<(&[u8], bool)> = keys
        .iter()
        .zip(loaded)
        // SAFETY: caller contract.
        .map(|(k, &l)| (unsafe { units(k.ptr, k.len) }, l))
        .collect();
    runtime::pick_module(&mods, needle).map_or(-1, |i| i as isize)
}

/// Report lines for the stub call counts: the total, then the `top` most called.
///
/// # Safety
/// `names` and `counts` hold `n` elements; `sink` accepts `ctx` once per line.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_stub_report(
    names: *const OhplGs6Str,
    counts: *const i32,
    n: usize,
    top: i32,
    sink: Sink<u8>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    let (names, counts) = unsafe { (units(names, n), units(counts, n)) };
    let v: Vec<(&[u8], i32)> = names
        .iter()
        .zip(counts)
        // SAFETY: caller contract.
        .map(|(s, &c)| (unsafe { units(s.ptr, s.len) }, c))
        .collect();
    for line in runtime::stub_report(&v, top) {
        // SAFETY: caller contract.
        unsafe { emit(sink, ctx, &line) }
    }
}
