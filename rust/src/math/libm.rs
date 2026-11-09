//! The libm/libc entry points gcc emits for `<cmath>` float calls.

use std::ffi::{c_int, c_long};

unsafe extern "C" {
    fn sinf(x: f32) -> f32;
    fn cosf(x: f32) -> f32;
    fn tanf(x: f32) -> f32;
    fn acosf(x: f32) -> f32;
    fn asinf(x: f32) -> f32;
    fn atanf(x: f32) -> f32;
    fn atan2f(y: f32, x: f32) -> f32;
    fn logf(x: f32) -> f32;
    fn pow(x: f64, y: f64) -> f64;
    fn rand() -> c_int;
    fn srand(seed: u32);
    fn time(t: *mut c_long) -> c_long;
}

macro_rules! safe {
    ($name:ident, $raw:ident, ($($a:ident),*)) => {
        pub(crate) fn $name($($a: f32),*) -> f32 {
            // SAFETY: pure libm function on plain floats.
            unsafe { $raw($($a),*) }
        }
    };
}

safe!(sin, sinf, (x));
safe!(cos, cosf, (x));
safe!(tan, tanf, (x));
safe!(acos, acosf, (x));
safe!(asin, asinf, (x));
safe!(atan, atanf, (x));
safe!(atan2, atan2f, (y, x));
safe!(ln, logf, (x));

pub(crate) fn pow_f64(x: f64, y: f64) -> f64 {
    // SAFETY: pure libm function on plain doubles.
    unsafe { pow(x, y) }
}

pub(crate) fn c_rand() -> i32 {
    // SAFETY: libc rand has no preconditions; shares the C++ side's generator state.
    unsafe { rand() }
}

pub(crate) fn c_srand(seed: u32) {
    // SAFETY: libc srand has no preconditions.
    unsafe { srand(seed) }
}

pub(crate) fn c_time() -> c_long {
    // SAFETY: null is an accepted argument for time().
    unsafe { time(std::ptr::null_mut()) }
}
