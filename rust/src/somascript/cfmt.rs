//! libc formatting and parsing entry points the C++ used, called directly so results match bit for bit.

use std::ffi::{c_char, c_int};

unsafe extern "C" {
    fn snprintf(buf: *mut c_char, n: usize, fmt: *const c_char, ...) -> c_int;
    fn atoi(s: *const c_char) -> c_int;
    fn atof(s: *const c_char) -> f64;
    fn strtof(s: *const c_char, end: *mut *mut c_char) -> f32;
}

/// What a `char buf[cap]` holds after `snprintf`: up to the first NUL, at most `cap - 1` bytes.
fn finish(buf: &[u8], ret: c_int) -> Vec<u8> {
    let len = usize::try_from(ret)
        .unwrap_or(0)
        .min(buf.len().saturating_sub(1));
    buf.get(..len).unwrap_or_default().to_vec()
}

pub(crate) fn fmt_f(v: f32, cap: usize) -> Vec<u8> {
    let mut buf = vec![0u8; cap];
    // SAFETY: format takes one double; the buffer bound is passed.
    let r = unsafe { snprintf(buf.as_mut_ptr().cast(), cap, c"%f".as_ptr(), f64::from(v)) };
    finish(&buf, r)
}

pub(crate) fn fmt_g(v: f32, cap: usize) -> Vec<u8> {
    let mut buf = vec![0u8; cap];
    // SAFETY: format takes one double; the buffer bound is passed.
    let r = unsafe { snprintf(buf.as_mut_ptr().cast(), cap, c"%g".as_ptr(), f64::from(v)) };
    finish(&buf, r)
}

pub(crate) fn fmt_prec_f(prec: i32, v: f32, cap: usize) -> Vec<u8> {
    let mut buf = vec![0u8; cap];
    // SAFETY: format takes (int, double); the buffer bound is passed.
    let r = unsafe {
        snprintf(
            buf.as_mut_ptr().cast(),
            cap,
            c"%.*f".as_ptr(),
            prec,
            f64::from(v),
        )
    };
    finish(&buf, r)
}

pub(crate) fn fmt_g9(v: f32) -> Vec<u8> {
    let mut buf = [0u8; 64];
    // SAFETY: format takes (int, double); the buffer bound is passed.
    let r = unsafe {
        snprintf(
            buf.as_mut_ptr().cast(),
            buf.len(),
            c"%.*g".as_ptr(),
            9 as c_int,
            f64::from(v),
        )
    };
    finish(&buf, r)
}

pub(crate) fn fmt_padded_int(width: i32, v: i32, cap: usize) -> Vec<u8> {
    let mut buf = vec![0u8; cap];
    // SAFETY: format takes (int, int); the buffer bound is passed.
    let r = unsafe { snprintf(buf.as_mut_ptr().cast(), cap, c"%0*d".as_ptr(), width, v) };
    finish(&buf, r)
}

fn nul_terminated(s: &[u8]) -> Vec<u8> {
    let mut v = Vec::with_capacity(s.len() + 1);
    v.extend_from_slice(s);
    v.push(0);
    v
}

pub(crate) fn c_atoi(s: &[u8]) -> i32 {
    let v = nul_terminated(s);
    // SAFETY: NUL terminated copy.
    unsafe { atoi(v.as_ptr().cast()) }
}

pub(crate) fn c_atof(s: &[u8]) -> f64 {
    let v = nul_terminated(s);
    // SAFETY: NUL terminated copy.
    unsafe { atof(v.as_ptr().cast()) }
}

/// `strtof` over the whole of `s`: `Some` only if every byte was consumed.
pub(crate) fn c_strtof_full(s: &[u8]) -> Option<f32> {
    let v = nul_terminated(s);
    let mut end: *mut c_char = std::ptr::null_mut();
    // SAFETY: NUL terminated copy, `end` receives a pointer into it.
    let f = unsafe { strtof(v.as_ptr().cast(), &mut end) };
    let used = (end as usize).wrapping_sub(v.as_ptr() as usize);
    (!s.is_empty() && used == s.len()).then_some(f)
}
