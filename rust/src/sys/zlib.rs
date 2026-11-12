//! Raw zlib bindings: stream deflate/inflate and crc32.
#![allow(non_camel_case_types, non_snake_case, non_upper_case_globals)]

use core::ffi::{CStr, c_char, c_int, c_uint, c_ulong, c_void};

use core::mem::offset_of;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct z_stream {
    pub next_in: *const u8,
    pub avail_in: u32,
    pub total_in: c_ulong,
    pub next_out: *mut u8,
    pub avail_out: u32,
    pub total_out: c_ulong,
    pub msg: *const c_char,
    pub state: *mut c_void,
    pub zalloc: *const c_void,
    pub zfree: *const c_void,
    pub opaque: *mut c_void,
    pub data_type: c_int,
    pub adler: c_ulong,
    pub reserved: c_ulong,
}

pub const Z_OK: c_int = 0;
pub const Z_STREAM_END: c_int = 1;
pub const Z_NEED_DICT: c_int = 2;
pub const Z_ERRNO: c_int = -1;
pub const Z_STREAM_ERROR: c_int = -2;
pub const Z_DATA_ERROR: c_int = -3;
pub const Z_MEM_ERROR: c_int = -4;
pub const Z_BUF_ERROR: c_int = -5;
pub const Z_VERSION_ERROR: c_int = -6;
pub const Z_NO_FLUSH: c_int = 0;
pub const Z_SYNC_FLUSH: c_int = 2;
pub const Z_FINISH: c_int = 4;
pub const Z_DEFAULT_COMPRESSION: c_int = -1;

/// zlib only checks the major version character and the struct size.
pub const ZLIB_VERSION: &CStr = c"1.3.1";

unsafe extern "C" {
    pub fn deflateInit_(
        strm: *mut z_stream,
        level: c_int,
        version: *const c_char,
        stream_size: c_int,
    ) -> c_int;
    pub fn deflate(strm: *mut z_stream, flush: c_int) -> c_int;
    pub fn deflateEnd(strm: *mut z_stream) -> c_int;
    pub fn inflateInit_(strm: *mut z_stream, version: *const c_char, stream_size: c_int) -> c_int;
    pub fn inflate(strm: *mut z_stream, flush: c_int) -> c_int;
    pub fn inflateEnd(strm: *mut z_stream) -> c_int;
    pub fn crc32(crc: c_ulong, buf: *const u8, len: c_uint) -> c_ulong;
}

const _: () = {
    assert!(size_of::<z_stream>() == 112);
    assert!(align_of::<z_stream>() == 8);
    assert!(offset_of!(z_stream, next_in) == 0);
    assert!(offset_of!(z_stream, avail_in) == 8);
    assert!(offset_of!(z_stream, total_in) == 16);
    assert!(offset_of!(z_stream, next_out) == 24);
    assert!(offset_of!(z_stream, avail_out) == 32);
    assert!(offset_of!(z_stream, total_out) == 40);
    assert!(offset_of!(z_stream, msg) == 48);
    assert!(offset_of!(z_stream, state) == 56);
    assert!(offset_of!(z_stream, zalloc) == 64);
    assert!(offset_of!(z_stream, zfree) == 72);
    assert!(offset_of!(z_stream, opaque) == 80);
    assert!(offset_of!(z_stream, data_type) == 88);
    assert!(offset_of!(z_stream, adler) == 96);
    assert!(offset_of!(z_stream, reserved) == 104);
};

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn layout() {
        assert_eq!(size_of::<z_stream>(), 112);
        assert_eq!(align_of::<z_stream>(), 8);
        assert_eq!(offset_of!(z_stream, next_in), 0);
        assert_eq!(offset_of!(z_stream, avail_in), 8);
        assert_eq!(offset_of!(z_stream, total_in), 16);
        assert_eq!(offset_of!(z_stream, next_out), 24);
        assert_eq!(offset_of!(z_stream, avail_out), 32);
        assert_eq!(offset_of!(z_stream, total_out), 40);
        assert_eq!(offset_of!(z_stream, msg), 48);
        assert_eq!(offset_of!(z_stream, state), 56);
        assert_eq!(offset_of!(z_stream, zalloc), 64);
        assert_eq!(offset_of!(z_stream, zfree), 72);
        assert_eq!(offset_of!(z_stream, opaque), 80);
        assert_eq!(offset_of!(z_stream, data_type), 88);
        assert_eq!(offset_of!(z_stream, adler), 96);
        assert_eq!(offset_of!(z_stream, reserved), 104);
    }

    #[test]
    fn constants() {
        assert_eq!(i64::from(Z_OK), 0);
        assert_eq!(i64::from(Z_STREAM_END), 1);
        assert_eq!(i64::from(Z_NEED_DICT), 2);
        assert_eq!(i64::from(Z_ERRNO), -1);
        assert_eq!(i64::from(Z_STREAM_ERROR), -2);
        assert_eq!(i64::from(Z_DATA_ERROR), -3);
        assert_eq!(i64::from(Z_MEM_ERROR), -4);
        assert_eq!(i64::from(Z_BUF_ERROR), -5);
        assert_eq!(i64::from(Z_VERSION_ERROR), -6);
        assert_eq!(i64::from(Z_NO_FLUSH), 0);
        assert_eq!(i64::from(Z_SYNC_FLUSH), 2);
        assert_eq!(i64::from(Z_FINISH), 4);
        assert_eq!(i64::from(Z_DEFAULT_COMPRESSION), -1);
    }

    #[test]
    fn version_major() {
        assert_eq!(ZLIB_VERSION.to_bytes().first(), Some(&b'1'));
    }
}
