//! C ABI for `string.rs`. Inputs are (pointer, length) pairs owned by the caller; outputs are
//! delivered through a caller-supplied sink callback, so Rust never hands out an allocation.

use crate::string;
use std::ffi::{c_char, c_void};

type Sink<T> = unsafe extern "C" fn(*mut c_void, *const T, usize);

/// # Safety
/// `p` points to `n` readable units, or `n` is 0.
unsafe fn units<'a, T>(p: *const T, n: usize) -> &'a [T] {
    if n == 0 {
        &[]
    } else {
        // SAFETY: caller contract.
        unsafe { std::slice::from_raw_parts(p, n) }
    }
}

/// # Safety
/// `sink` accepts `ctx` and a slice.
unsafe fn emit<T>(sink: Sink<T>, ctx: *mut c_void, v: &[T]) {
    // SAFETY: caller contract.
    unsafe { sink(ctx, v.as_ptr(), v.len()) }
}

fn widen(v: &[u8]) -> Vec<u32> {
    v.iter().copied().map(u32::from).collect()
}

macro_rules! unit_api {
    ($t:ty, $wide:expr;
     $sub:ident $file_ext:ident $file_name:ident $file_path:ident $set_file_ext:ident
     $set_file_path:ident $relative_path:ident $add_slash:ident $remove_slash:ident
     $last_char:ident $lower:ident $upper:ident $replace_char:ident $first_pos:ident
     $last_pos:ident $count_chars:ident $split:ident $numeric_suffix:ident) => {
        /// # Safety
        /// Pointers valid for their lengths; `sink` accepts `ctx`. Returns false where C++ `substr` throws.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $sub(
            s: *const $t,
            n: usize,
            start: i32,
            count: i32,
            sink: Sink<$t>,
            ctx: *mut c_void,
        ) -> bool {
            // SAFETY: caller contract.
            let s = unsafe { units(s, n) };
            match string::sub(s, start, count, $wide) {
                // SAFETY: caller contract.
                Some(r) => unsafe { emit(sink, ctx, r) },
                None => return false,
            }
            true
        }

        unit_slice_fn!($file_ext, $t, string::file_ext);
        unit_slice_fn!($file_name, $t, string::file_name);
        unit_slice_fn!($file_path, $t, string::file_path);
        unit_slice_fn!($remove_slash, $t, string::remove_slash_at_end);
        unit_slice_fn!($last_char, $t, string::last_char);
        unit_vec_fn!($lower, $t, string::to_lower);
        unit_vec_fn!($upper, $t, string::to_upper);
        unit_vec2_fn!($set_file_ext, $t, string::set_file_ext);
        unit_vec2_fn!($set_file_path, $t, string::set_file_path);
        unit_vec2_fn!($relative_path, $t, string::relative_path);

        /// # Safety
        /// Pointers valid for their lengths; `sink` accepts `ctx`.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $add_slash(
            s: *const $t,
            n: usize,
            slash: $t,
            sink: Sink<$t>,
            ctx: *mut c_void,
        ) {
            // SAFETY: caller contract.
            unsafe { emit(sink, ctx, &string::add_slash_at_end(units(s, n), slash)) }
        }

        /// # Safety
        /// Pointers valid for their lengths; `sink` accepts `ctx`.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $replace_char(
            s: *const $t,
            n: usize,
            old: *const $t,
            old_n: usize,
            new: *const $t,
            new_n: usize,
            sink: Sink<$t>,
            ctx: *mut c_void,
        ) {
            // SAFETY: caller contract.
            unsafe {
                let r = string::replace_char_to(units(s, n), units(old, old_n), units(new, new_n));
                emit(sink, ctx, &r)
            }
        }

        /// # Safety
        /// Pointers valid for their lengths.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $first_pos(
            s: *const $t,
            n: usize,
            c: *const $t,
            cn: usize,
        ) -> i32 {
            // SAFETY: caller contract.
            unsafe { string::first_pos(units(s, n), units(c, cn)) }
        }

        /// # Safety
        /// Pointers valid for their lengths.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $last_pos(s: *const $t, n: usize, c: *const $t, cn: usize) -> i32 {
            // SAFETY: caller contract.
            unsafe { string::last_pos(units(s, n), units(c, cn)) }
        }

        /// # Safety
        /// Pointers valid for their lengths.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $count_chars(
            s: *const $t,
            n: usize,
            c: *const $t,
            cn: usize,
        ) -> i32 {
            // SAFETY: caller contract.
            unsafe { string::count_chars(units(s, n), units(c, cn)) }
        }

        /// # Safety
        /// Pointers valid for their lengths (`seps` may be null for the default set);
        /// `sink` accepts `ctx`. `cr_default` adds '\r' to the default separators.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $split(
            s: *const $t,
            n: usize,
            seps: *const $t,
            seps_n: usize,
            cr_default: bool,
            sink: Sink<$t>,
            ctx: *mut c_void,
        ) {
            // SAFETY: caller contract.
            unsafe {
                let seps = (!seps.is_null()).then(|| units(seps, seps_n));
                for tok in string::split(units(s, n), seps, cr_default) {
                    emit(sink, ctx, tok);
                }
            }
        }

        /// # Safety
        /// Pointers valid for their lengths; `index` is writable.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $numeric_suffix(
            s: *const $t,
            n: usize,
            index: *mut i32,
            sink: Sink<$t>,
            ctx: *mut c_void,
        ) {
            // SAFETY: caller contract.
            unsafe {
                let (suffix, i) = string::numeric_suffix(units(s, n));
                *index = i;
                emit(sink, ctx, suffix)
            }
        }
    };
}

macro_rules! unit_slice_fn {
    ($name:ident, $t:ty, $f:path) => {
        /// # Safety
        /// Pointers valid for their lengths; `sink` accepts `ctx`.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name(s: *const $t, n: usize, sink: Sink<$t>, ctx: *mut c_void) {
            // SAFETY: caller contract.
            unsafe { emit(sink, ctx, $f(units(s, n))) }
        }
    };
}

macro_rules! unit_vec_fn {
    ($name:ident, $t:ty, $f:path) => {
        /// # Safety
        /// Pointers valid for their lengths; `sink` accepts `ctx`.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name(s: *const $t, n: usize, sink: Sink<$t>, ctx: *mut c_void) {
            // SAFETY: caller contract.
            unsafe { emit(sink, ctx, &$f(units(s, n))) }
        }
    };
}

macro_rules! unit_vec2_fn {
    ($name:ident, $t:ty, $f:path) => {
        /// # Safety
        /// Pointers valid for their lengths; `sink` accepts `ctx`.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name(
            s: *const $t,
            n: usize,
            b: *const $t,
            bn: usize,
            sink: Sink<$t>,
            ctx: *mut c_void,
        ) {
            // SAFETY: caller contract.
            unsafe { emit(sink, ctx, &$f(units(s, n), units(b, bn))) }
        }
    };
}

unit_api!(u8, false;
    ohpl_str_sub ohpl_str_file_ext ohpl_str_file_name ohpl_str_file_path ohpl_str_set_file_ext
    ohpl_str_set_file_path ohpl_str_relative_path ohpl_str_add_slash_at_end
    ohpl_str_remove_slash_at_end ohpl_str_last_char ohpl_str_to_lower ohpl_str_to_upper
    ohpl_str_replace_char_to ohpl_str_first_pos ohpl_str_last_pos ohpl_str_count_chars
    ohpl_str_split ohpl_str_numeric_suffix);

unit_api!(u32, true;
    ohpl_str_sub_w ohpl_str_file_ext_w ohpl_str_file_name_w ohpl_str_file_path_w
    ohpl_str_set_file_ext_w ohpl_str_set_file_path_w ohpl_str_relative_path_w
    ohpl_str_add_slash_at_end_w ohpl_str_remove_slash_at_end_w ohpl_str_last_char_w
    ohpl_str_to_lower_w ohpl_str_to_upper_w ohpl_str_replace_char_to_w ohpl_str_first_pos_w
    ohpl_str_last_pos_w ohpl_str_count_chars_w ohpl_str_split_w ohpl_str_numeric_suffix_w);

/// # Safety
/// Pointers valid for their lengths; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_str_replace_string_to(
    s: *const u8,
    n: usize,
    old: *const u8,
    old_n: usize,
    new: *const u8,
    new_n: usize,
    sink: Sink<u8>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    unsafe {
        let r = string::replace_string_to(units(s, n), units(old, old_n), units(new, new_n));
        emit(sink, ctx, &r)
    }
}

/// # Safety
/// Pointers valid for their lengths.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_str_matches_wildcard(
    pattern: *const u8,
    pattern_n: usize,
    name: *const u8,
    name_n: usize,
) -> bool {
    // SAFETY: caller contract.
    unsafe { string::matches_wildcard(units(pattern, pattern_n), units(name, name_n)) }
}

/// # Safety
/// `s` is NUL-terminated.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_str_to_int(s: *const c_char) -> i32 {
    // SAFETY: caller contract.
    unsafe { string::to_int(s) }
}

/// # Safety
/// `s` is NUL-terminated.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_str_to_float(s: *const c_char) -> f32 {
    // SAFETY: caller contract.
    unsafe { string::to_float(s) }
}

/// # Safety
/// `s` is NUL-terminated.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_str_to_bool(s: *const c_char) -> bool {
    // SAFETY: caller contract.
    string::to_bool(unsafe { std::ffi::CStr::from_ptr(s) }.to_bytes())
}

/// # Safety
/// `s` is NUL-terminated; `out` has room for `n` floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_str_to_floats_exact(
    s: *const c_char,
    n: usize,
    out: *mut f32,
) -> bool {
    // SAFETY: caller contract.
    unsafe {
        let s = std::ffi::CStr::from_ptr(s).to_bytes();
        string::floats_exact(s, std::slice::from_raw_parts_mut(out, n))
    }
}

/// # Safety
/// `s` is NUL-terminated; `out` has room for `n` ints.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_str_to_ints_exact(s: *const c_char, n: usize, out: *mut i32) -> bool {
    // SAFETY: caller contract.
    unsafe {
        let s = std::ffi::CStr::from_ptr(s).to_bytes();
        string::ints_exact(s, std::slice::from_raw_parts_mut(out, n))
    }
}

/// # Safety
/// `s` valid for `n` bytes; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_str_get_int_vec(
    s: *const u8,
    n: usize,
    seps: *const u8,
    seps_n: usize,
    sink: unsafe extern "C" fn(*mut c_void, i32),
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    unsafe {
        let seps = (!seps.is_null()).then(|| units(seps, seps_n));
        for tok in string::split(units(s, n), seps, true) {
            sink(ctx, string::token_to_int(tok));
        }
    }
}

/// # Safety
/// `s` valid for `n` bytes; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_str_get_float_vec(
    s: *const u8,
    n: usize,
    seps: *const u8,
    seps_n: usize,
    sink: unsafe extern "C" fn(*mut c_void, f32),
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    unsafe {
        let seps = (!seps.is_null()).then(|| units(seps, seps_n));
        for tok in string::split(units(s, n), seps, true) {
            sink(ctx, string::token_to_float(tok));
        }
    }
}

macro_rules! format_fn {
    ($narrow:ident, $wide:ident, $x:ty, $f:path, $fw:path) => {
        /// # Safety
        /// `sink` accepts `ctx`.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $narrow(x: $x, pad: i32, sink: Sink<u8>, ctx: *mut c_void) {
            // SAFETY: caller contract.
            unsafe { emit(sink, ctx, &$f(x, pad)) }
        }

        /// # Safety
        /// `sink` accepts `ctx`.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $wide(x: $x, pad: i32, sink: Sink<u32>, ctx: *mut c_void) {
            // SAFETY: caller contract.
            unsafe { emit(sink, ctx, &widen(&$fw(x, pad))) }
        }
    };
}

format_fn!(
    ohpl_str_int_to_string,
    ohpl_str_int_to_string_w,
    i32,
    string::int_to_string,
    string::int_to_string
);
format_fn!(
    ohpl_str_ulong_to_string,
    ohpl_str_ulong_to_string_w,
    u64,
    string::ulong_to_string,
    string::ulong_to_string_wide
);

/// # Safety
/// `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_str_uint_to_string(
    x: u32,
    pad: i32,
    sink: Sink<u8>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, &string::uint_to_string(x, pad)) }
}

/// # Safety
/// `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_str_float_to_string(
    x: f32,
    decimals: i32,
    remove_zeros: bool,
    sink: Sink<u8>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    unsafe {
        emit(
            sink,
            ctx,
            &string::float_to_string(x, decimals, remove_zeros),
        )
    }
}

/// # Safety
/// `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_str_float_to_string_w(
    x: f32,
    decimals: i32,
    remove_zeros: bool,
    sink: Sink<u32>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    unsafe {
        emit(
            sink,
            ctx,
            &widen(&string::float_to_string(x, decimals, remove_zeros)),
        )
    }
}

/// # Safety
/// Pointers valid for their lengths; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_str_utf8_to_wide(
    s: *const u8,
    n: usize,
    sink: Sink<u32>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, &string::utf8_to_wide(units(s, n))) }
}

/// # Safety
/// Pointers valid for their lengths; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_str_wide_to_utf8(
    s: *const u32,
    n: usize,
    sink: Sink<u8>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, &string::wide_to_utf8(units(s, n))) }
}

/// Parses a space-separated list of ints into wide units, as `Get16BitFromArray`.
///
/// # Safety
/// Pointers valid for their lengths; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_str_wide_from_int_list(
    s: *const u8,
    n: usize,
    sink: Sink<u32>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    let v: Vec<u32> = string::split(unsafe { units(s, n) }, None, true)
        .map(|t| string::token_to_int(t) as u32)
        .collect();
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, &v) }
}

/// # Safety
/// `s` valid for `n` units.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_str_hash_w(s: *const u32, n: usize) -> u32 {
    // SAFETY: caller contract.
    string::hash_wide(unsafe { units(s, n) })
}

/// Returns false (leaving the sink untouched) unless `n` is a multiple of 4.
///
/// # Safety
/// `data` valid for `n` bytes; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_str_encode_data(
    data: *const u8,
    n: usize,
    sink: Sink<u8>,
    ctx: *mut c_void,
) -> bool {
    // SAFETY: caller contract.
    match string::encode_data(unsafe { units(data, n) }) {
        Some(v) => {
            // SAFETY: caller contract.
            unsafe { emit(sink, ctx, &v) };
            true
        }
        None => false,
    }
}

/// # Safety
/// `s` valid for `n` bytes; `dest` writable for `dest_n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_str_decode_data(
    s: *const u8,
    n: usize,
    dest: *mut u8,
    dest_n: usize,
) -> bool {
    // SAFETY: caller contract.
    unsafe {
        let dest = if dest_n == 0 {
            &mut [][..]
        } else {
            std::slice::from_raw_parts_mut(dest, dest_n)
        };
        string::decode_data(units(s, n), dest)
    }
}
