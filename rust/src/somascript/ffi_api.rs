//! C ABI for `api.rs`.

use super::api::Enum;

/// (pointer, length) of a byte string owned by the caller.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct OhplGs6Str {
    pub ptr: *const u8,
    pub len: usize,
}

impl OhplGs6Str {}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs6_enums_new() -> *mut Vec<Enum> {
    Box::into_raw(Box::default())
}
