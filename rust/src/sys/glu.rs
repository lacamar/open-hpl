//! Raw GLU bindings (CPU mip filtering).
#![allow(non_camel_case_types, non_snake_case, non_upper_case_globals)]

use core::ffi::c_void;

unsafe extern "C" {
    pub fn gluBuild1DMipmaps(
        target: u32,
        internalFormat: i32,
        width: i32,
        format: u32,
        type_: u32,
        data: *const c_void,
    ) -> i32;
    pub fn gluBuild2DMipmaps(
        target: u32,
        internalFormat: i32,
        width: i32,
        height: i32,
        format: u32,
        type_: u32,
        data: *const c_void,
    ) -> i32;
    pub fn gluScaleImage(
        format: u32,
        wIn: i32,
        hIn: i32,
        typeIn: u32,
        dataIn: *const c_void,
        wOut: i32,
        hOut: i32,
        typeOut: u32,
        dataOut: *mut c_void,
    ) -> i32;
}
