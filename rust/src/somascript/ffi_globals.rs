//! C ABI for `globals.rs`. `scope` 0 is a named global variable, 1 a call argument, 2 the return value.

use super::globals::{self, Globals, Variant, get_words, set_words};
use crate::string_ffi::{Sink, emit, units};

use std::ffi::c_void;

fn with_variant<R>(scope: u32, name: &[u8], idx: i32, f: impl FnOnce(&mut Variant) -> R) -> R {
    let mut g = globals::lock();
    let g: &mut Globals = &mut g;
    match scope {
        0 => f(g.vars.entry(name.to_vec()).or_default()),
        1 => f(g.args.entry(idx).or_default()),
        _ => f(&mut g.ret),
    }
}

/// Sets a numeric field from `words` (16 raw 32-bit words) or, for the string field, from `str`.
///
/// # Safety
/// `name` valid for `name_len` bytes; `words` valid for 16 words (or null for the string field);
/// `str` valid for `str_len` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_var_set(
    scope: u32,
    name: *const u8,
    name_len: usize,
    idx: i32,
    field: u32,
    words: *const u32,
    str: *const u8,
    str_len: usize,
) {
    // SAFETY: caller contract.
    let name = unsafe { units(name, name_len) };
    if field == globals::FIELD_S {
        // SAFETY: caller contract.
        let s = unsafe { units(str, str_len) }.to_vec();
        with_variant(scope, name, idx, |v| v.s = s);
    } else {
        let mut w = [0u32; 16];
        if !words.is_null() {
            // SAFETY: caller contract.
            w = unsafe { words.cast::<[u32; 16]>().read_unaligned() };
        }
        with_variant(scope, name, idx, |v| set_words(v, field, &w));
    }
}

/// Reads a numeric field into 16 raw 32-bit words.
///
/// # Safety
/// `name` valid for `name_len` bytes; `out` has room for 16 words.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_var_get(
    scope: u32,
    name: *const u8,
    name_len: usize,
    idx: i32,
    field: u32,
    out: *mut u32,
) {
    // SAFETY: caller contract.
    let name = unsafe { units(name, name_len) };
    let w = with_variant(scope, name, idx, |v| get_words(v, field));
    // SAFETY: caller contract.
    unsafe { out.cast::<[u32; 16]>().write_unaligned(w) }
}

/// # Safety
/// `name` valid for `name_len` bytes; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_var_get_str(
    scope: u32,
    name: *const u8,
    name_len: usize,
    idx: i32,
    sink: Sink<u8>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    let name = unsafe { units(name, name_len) };
    let s = with_variant(scope, name, idx, |v| v.s.clone());
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, &s) }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs6_reset_return() {
    globals::lock().ret = Variant::default();
}

/// Drops every variable and argument and resets the return value.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs6_reset_all() {
    *globals::lock() = Globals::default();
}

/// # Safety
/// `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_serialize_vars(sink: Sink<u8>, ctx: *mut c_void) {
    let s = globals::serialize(&globals::lock());
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, &s) }
}

/// # Safety
/// `data` valid for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_deserialize_vars(data: *const u8, n: usize) {
    // SAFETY: caller contract.
    globals::deserialize(&mut globals::lock(), unsafe { units(data, n) });
}

/// `ShowSecretCode`: stores the decoded text as argument 0; false if `x` is shorter than 3 bytes.
///
/// # Safety
/// `key` points to 3 bytes; `x` valid for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_secret_code(key: *const u8, x: *const u8, n: usize) -> bool {
    // SAFETY: caller contract.
    let key = unsafe { key.cast::<[u8; 3]>().read_unaligned() };
    // SAFETY: caller contract.
    globals::secret_code(&mut globals::lock(), key, unsafe { units(x, n) })
}
