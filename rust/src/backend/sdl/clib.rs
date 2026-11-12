//! The libc entry points L1 needs (no libc crate); layouts are asserted in `cshim/layout_check.cpp`.
#![allow(non_camel_case_types)]

use core::ffi::{CStr, c_char, c_int, c_long, c_uint, c_ulong, c_void};

pub const LC_CTYPE: c_int = 0;
pub const CODESET: c_int = 14;
pub const CLONE_NEWNS: c_int = 0x0002_0000;
pub const CLONE_NEWUSER: c_int = 0x1000_0000;
pub const MS_REC: c_ulong = 16384;
pub const MS_PRIVATE: c_ulong = 1 << 18;
pub const MS_BIND: c_ulong = 4096;
pub const W_OK: c_int = 2;
pub const S_IFMT: c_uint = 0o170_000;
pub const S_IFDIR: c_uint = 0o040_000;

#[repr(C)]
pub struct timeval {
    pub tv_sec: c_long,
    pub tv_usec: c_long,
}

#[repr(C)]
pub struct statvfs {
    pub f_bsize: c_ulong,
    pub f_frsize: c_ulong,
    pub f_blocks: u64,
    pub f_bfree: u64,
    pub f_bavail: u64,
    pub f_files: u64,
    pub f_ffree: u64,
    pub f_favail: u64,
    pub f_fsid: c_ulong,
    pub f_flag: c_ulong,
    pub f_namemax: c_ulong,
    pub spare: [c_int; 6],
}

#[repr(C)]
pub struct tm {
    pub tm_sec: c_int,
    pub tm_min: c_int,
    pub tm_hour: c_int,
    pub tm_mday: c_int,
    pub tm_mon: c_int,
    pub tm_year: c_int,
    pub tm_wday: c_int,
    pub tm_yday: c_int,
    pub tm_isdst: c_int,
    pub tm_gmtoff: c_long,
    pub tm_zone: *const c_char,
}

#[repr(C)]
pub struct stat {
    pub st_dev: u64,
    pub st_ino: u64,
    pub st_mode: c_uint,
    pub st_nlink: c_uint,
    pub st_uid: c_uint,
    pub st_gid: c_uint,
    pub st_rdev: u64,
    pub pad1: u64,
    pub st_size: i64,
    pub st_blksize: c_int,
    pub pad2: c_int,
    pub st_blocks: i64,
    pub st_atime: i64,
    pub st_atime_nsec: i64,
    pub st_mtime: i64,
    pub st_mtime_nsec: i64,
    pub st_ctime: i64,
    pub st_ctime_nsec: i64,
    pub unused: [c_int; 2],
}

impl stat {
    pub const fn zeroed() -> Self {
        Self {
            st_dev: 0,
            st_ino: 0,
            st_mode: 0,
            st_nlink: 0,
            st_uid: 0,
            st_gid: 0,
            st_rdev: 0,
            pad1: 0,
            st_size: 0,
            st_blksize: 0,
            pad2: 0,
            st_blocks: 0,
            st_atime: 0,
            st_atime_nsec: 0,
            st_mtime: 0,
            st_mtime_nsec: 0,
            st_ctime: 0,
            st_ctime_nsec: 0,
            unused: [0; 2],
        }
    }
}

impl tm {
    pub const fn zeroed() -> Self {
        Self {
            tm_sec: 0,
            tm_min: 0,
            tm_hour: 0,
            tm_mday: 0,
            tm_mon: 0,
            tm_year: 0,
            tm_wday: 0,
            tm_yday: 0,
            tm_isdst: 0,
            tm_gmtoff: 0,
            tm_zone: core::ptr::null(),
        }
    }
}

unsafe extern "C" {
    pub fn gettimeofday(tv: *mut timeval, tz: *mut c_void) -> c_int;
    pub fn setlocale(category: c_int, locale: *const c_char) -> *mut c_char;
    pub fn nl_langinfo(item: c_int) -> *mut c_char;
    pub fn strcasecmp(a: *const c_char, b: *const c_char) -> c_int;
    pub fn strerror(errnum: c_int) -> *const c_char;
    pub fn __errno_location() -> *mut c_int;
    pub fn unshare(flags: c_int) -> c_int;
    pub fn mount(
        source: *const c_char,
        target: *const c_char,
        fstype: *const c_char,
        flags: c_ulong,
        data: *const c_void,
    ) -> c_int;
    pub fn statvfs(path: *const c_char, buf: *mut statvfs) -> c_int;
    pub fn access(path: *const c_char, mode: c_int) -> c_int;
    pub fn chdir(path: *const c_char) -> c_int;
    pub fn getuid() -> c_uint;
    pub fn getgid() -> c_uint;
    pub fn setenv(name: *const c_char, value: *const c_char, overwrite: c_int) -> c_int;
    pub fn stat(path: *const c_char, buf: *mut stat) -> c_int;
    pub fn lstat(path: *const c_char, buf: *mut stat) -> c_int;
    pub fn unlink(path: *const c_char) -> c_int;
    pub fn mkdir(path: *const c_char, mode: c_uint) -> c_int;
    pub fn gmtime_r(t: *const i64, out: *mut tm) -> *mut tm;
    pub fn localtime_r(t: *const i64, out: *mut tm) -> *mut tm;
    pub fn time(t: *mut i64) -> i64;
    pub fn mbstowcs(dst: *mut u32, src: *const c_char, n: usize) -> usize;
    pub fn towupper(c: u32) -> u32;
}

pub fn errno_text() -> String {
    // SAFETY: errno is thread-local; strerror returns a NUL-terminated string or null.
    unsafe {
        let p = strerror(*__errno_location());
        if p.is_null() {
            String::new()
        } else {
            CStr::from_ptr(p).to_string_lossy().into_owned()
        }
    }
}

pub fn warn(msg: &str) {
    use std::io::Write;
    let _ = writeln!(std::io::stderr(), "{msg}");
}
