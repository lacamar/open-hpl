//! `cLogWriter` file handling: truncate on open, every message written through at once.

use core::ffi::{CStr, c_char};
use std::ffi::OsStr;
use std::fs::File;
use std::io::Write;
use std::os::unix::ffi::OsStrExt;

pub struct LogFile(File);

/// Null when the file cannot be created.
///
/// # Safety
/// `path` is NUL-terminated.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l1_log_open(path: *const c_char) -> *mut LogFile {
    if path.is_null() {
        return core::ptr::null_mut();
    }
    // SAFETY: caller contract.
    let p = unsafe { CStr::from_ptr(path) }.to_bytes();
    File::create(OsStr::from_bytes(p)).map_or(core::ptr::null_mut(), |f| {
        Box::into_raw(Box::new(LogFile(f)))
    })
}

/// Writes up to the first NUL, like `fprintf("%s")`.
///
/// # Safety
/// `h` is live; `msg` is NUL-terminated.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l1_log_write(h: *mut LogFile, msg: *const c_char) {
    // SAFETY: caller contract.
    if let (Some(h), false) = (unsafe { h.as_mut() }, msg.is_null()) {
        let _ = h.0.write_all(unsafe { CStr::from_ptr(msg) }.to_bytes());
    }
}

/// # Safety
/// `h` from `ohpl_l1_log_open`, closed once (or null).
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_l1_log_close(h: *mut LogFile) {
    if !h.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(h) });
    }
}
