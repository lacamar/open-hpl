//! C ABI for `strings.rs`. Inputs are (pointer, length) pairs; outputs go through sinks.

use super::cfmt;
use super::strings;
use crate::string_ffi::{Sink, emit, units};

use std::ffi::c_void;

macro_rules! out_fn {
    ($name:ident, $t:ty; ($($a:ident: $at:ty),*) => $body:expr) => {
        /// # Safety
        /// `sink` accepts `ctx`.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name($($a: $at,)* sink: Sink<$t>, ctx: *mut c_void) {
            let r: Vec<$t> = $body;
            // SAFETY: caller contract.
            unsafe { emit(sink, ctx, &r) }
        }
    };
}

out_fn!(ohpl_gs6_float_to_string, u8; (f: f32, decimals: i32, remove_zeros: bool)
    => strings::float_to_string(f, decimals, remove_zeros));
out_fn!(ohpl_gs6_int_padded, u8; (x: i32, padding: i32) => strings::int_padded(x, padding));
out_fn!(ohpl_gs6_clock, u8; (s: f32, h: bool, m: bool, sec: bool, c: bool)
    => strings::seconds_to_clock(s, h, m, sec, c));

macro_rules! slice_fn {
    ($name:ident, $t:ty; ($s:ident, $n:ident) => $body:expr) => {
        /// # Safety
        /// Pointer valid for its length; `sink` accepts `ctx`.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name(s: *const $t, n: usize, sink: Sink<$t>, ctx: *mut c_void) {
            // SAFETY: caller contract.
            let $s = unsafe { units(s, n) };
            let r: &[$t] = $body;
            // SAFETY: caller contract.
            unsafe { emit(sink, ctx, r) }
        }
    };
}

slice_fn!(ohpl_gs6_last_char, u8; (s, n) => strings::last_char(s));
slice_fn!(ohpl_gs6_numeric_suffix, u8; (s, n) => strings::numeric_suffix(s));
slice_fn!(ohpl_gs6_numeric_suffix_w, u32; (s, n) => strings::numeric_suffix(s));

/// # Safety
/// Pointer valid for its length; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_widen(s: *const u8, n: usize, sink: Sink<u32>, ctx: *mut c_void) {
    // SAFETY: caller contract.
    let w = strings::widen(unsafe { units(s, n) });
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, &w) }
}

/// # Safety
/// Pointers valid for their lengths; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_replace_w(
    s: *const u32,
    n: usize,
    old: *const u32,
    old_n: usize,
    new: *const u32,
    new_n: usize,
    sink: Sink<u32>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    let r = unsafe { strings::replace_all_wide(units(s, n), units(old, old_n), units(new, new_n)) };
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, &r) }
}

/// # Safety
/// Pointer valid for its length.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_find_char(s: *const u8, n: usize, c: u8, last: bool) -> i32 {
    // SAFETY: caller contract.
    strings::find_char(unsafe { units(s, n) }, c, last)
}

/// # Safety
/// Pointers valid for their lengths.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_find_str(
    s: *const u8,
    n: usize,
    sub: *const u8,
    m: usize,
    last: bool,
) -> i32 {
    // SAFETY: caller contract.
    unsafe { strings::find_str(units(s, n), units(sub, m), last) }
}

/// # Safety
/// Pointers valid for their lengths.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_find_str_w(
    s: *const u32,
    n: usize,
    sub: *const u32,
    m: usize,
    last: bool,
) -> i32 {
    // SAFETY: caller contract.
    unsafe { strings::find_str(units(s, n), units(sub, m), last) }
}

/// # Safety
/// Pointers valid for their lengths.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_count_chars(
    s: *const u8,
    n: usize,
    c: *const u8,
    m: usize,
) -> i32 {
    // SAFETY: caller contract.
    unsafe { strings::count_chars(units(s, n), units(c, m)) }
}

/// # Safety
/// Pointer valid for its length.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_hash(s: *const u8, n: usize) -> u32 {
    // SAFETY: caller contract.
    strings::hash(unsafe { units(s, n) })
}

/// # Safety
/// Pointer valid for its length.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_numeric_suffix_int(s: *const u8, n: usize, default: i32) -> i32 {
    // SAFETY: caller contract.
    strings::numeric_suffix_int(unsafe { units(s, n) }, default)
}

/// # Safety
/// Pointer valid for its length.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_numeric_suffix_int_w(
    s: *const u32,
    n: usize,
    default: i32,
) -> i32 {
    // SAFETY: caller contract.
    strings::numeric_suffix_int_wide(unsafe { units(s, n) }, default)
}

/// # Safety
/// Pointers valid for their lengths; `sink` accepts `ctx` once per token.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_split(
    s: *const u8,
    n: usize,
    sep: *const u8,
    m: usize,
    sink: Sink<u8>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    for t in unsafe { strings::split(units(s, n), units(sep, m)) } {
        // SAFETY: caller contract.
        unsafe { emit(sink, ctx, t) }
    }
}

/// # Safety
/// Pointers valid for their lengths; `sink` accepts `ctx` once per token.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_split_w(
    s: *const u32,
    n: usize,
    sep: *const u32,
    m: usize,
    sink: Sink<u32>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    for t in unsafe { strings::split(units(s, n), units(sep, m)) } {
        // SAFETY: caller contract.
        unsafe { emit(sink, ctx, t) }
    }
}

/// Tokens converted with `atof` / `atoi`, one sink call each.
///
/// # Safety
/// Pointers valid for their lengths; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_float_vec(
    s: *const u8,
    n: usize,
    sep: *const u8,
    m: usize,
    sink: unsafe extern "C" fn(*mut c_void, f32),
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    for t in unsafe { strings::split(units(s, n), units(sep, m)) } {
        // SAFETY: caller contract.
        unsafe { sink(ctx, cfmt::c_atof(t) as f32) }
    }
}

/// # Safety
/// Pointers valid for their lengths; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_int_vec(
    s: *const u8,
    n: usize,
    sep: *const u8,
    m: usize,
    sink: unsafe extern "C" fn(*mut c_void, i32),
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    for t in unsafe { strings::split(units(s, n), units(sep, m)) } {
        // SAFETY: caller contract.
        unsafe { sink(ctx, cfmt::c_atoi(t)) }
    }
}

/// # Safety
/// `f` and `i` valid for `nf` / `ni` elements; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs6_tostr(
    kind: u32,
    f: *const f32,
    nf: usize,
    i: *const i32,
    ni: usize,
    sink: Sink<u8>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    let r = unsafe { strings::tostr(kind, units(f, nf), units(i, ni)) };
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, &r) }
}
