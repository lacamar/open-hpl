//! C ABI for `api.rs`.

/// (pointer, length) of a byte string owned by the caller.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct OhplGs6Str {
    pub ptr: *const u8,
    pub len: usize,
}

impl OhplGs6Str {}
