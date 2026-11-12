//! `FILE*` access for DevIL and the Theora reader: the original code passed C stdio handles
//! to the libraries, so the Rust side does too.

use core::ffi::{CStr, c_char, c_int, c_long, c_void};
use std::ffi::CString;

#[repr(C)]
pub struct File {
    _private: [u8; 0],
}

unsafe extern "C" {
    pub fn fopen(path: *const c_char, mode: *const c_char) -> *mut File;
    pub fn fclose(f: *mut File) -> c_int;
    pub fn fread(buf: *mut c_void, size: usize, n: usize, f: *mut File) -> usize;
    pub fn fwrite(buf: *const c_void, size: usize, n: usize, f: *mut File) -> usize;
    pub fn fseek(f: *mut File, off: c_long, whence: c_int) -> c_int;
    pub fn ftell(f: *mut File) -> c_long;
    pub fn fgetc(f: *mut File) -> c_int;
    pub fn fputc(c: c_int, f: *mut File) -> c_int;
    pub fn feof(f: *mut File) -> c_int;
}

pub const SEEK_SET: c_int = 0;
pub const SEEK_END: c_int = 2;

/// Null when the file cannot be opened (or the path holds a NUL).
pub fn open(path: &[u8], mode: &CStr) -> *mut File {
    let Ok(p) = CString::new(path) else {
        return core::ptr::null_mut();
    };
    // SAFETY: both strings are NUL-terminated.
    unsafe { fopen(p.as_ptr(), mode.as_ptr()) }
}

/// Bytes read into `buf`.
///
/// # Safety
/// `f` is an open file.
pub unsafe fn read(f: *mut File, buf: &mut [u8]) -> usize {
    // SAFETY: `buf` is writable for its length; caller contract for `f`.
    unsafe { fread(buf.as_mut_ptr().cast(), 1, buf.len(), f) }
}

/// Size by seeking to the end, -1 when that fails; leaves the position at the end.
///
/// # Safety
/// `f` is an open file.
pub unsafe fn size(f: *mut File) -> i64 {
    // SAFETY: caller contract.
    unsafe {
        if fseek(f, 0, SEEK_END) == 0 {
            ftell(f)
        } else {
            -1
        }
    }
}

/// # Safety
/// `f` is an open file.
pub unsafe fn seek_set(f: *mut File, off: i64) -> bool {
    // SAFETY: caller contract.
    unsafe { fseek(f, off as c_long, SEEK_SET) == 0 }
}
