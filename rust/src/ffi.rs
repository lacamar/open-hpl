//! C ABI seam called from HPL2 C++. Every pointer here is owned by the C++ side unless noted.

use crate::crc::Crc;

/// Returns an owned `Crc`; free with `ohpl_crc_free`.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_crc_new(key: u32) -> *mut Crc {
    Box::into_raw(Box::new(Crc::new(key)))
}

/// # Safety
/// `crc` comes from `ohpl_crc_new` and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_crc_free(crc: *mut Crc) {
    // SAFETY: caller contract above; Box::from_raw on the pointer Box::into_raw produced.
    drop(unsafe { Box::from_raw(crc) });
}

/// # Safety
/// `crc` is live; `data` points to `len` readable bytes (or `len` is 0).
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_crc_put_data(crc: *mut Crc, data: *const u8, len: usize) {
    // SAFETY: caller contract above.
    let crc = unsafe { &mut *crc };
    if len > 0 {
        // SAFETY: caller contract above.
        crc.put_data(unsafe { std::slice::from_raw_parts(data, len) });
    }
}

/// # Safety
/// `crc` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_crc_done(crc: *mut Crc) -> u32 {
    // SAFETY: caller contract above.
    unsafe { &mut *crc }.done()
}

/// Heap bytes owned by Rust; free with `ohpl_bytes_free`.
#[repr(C)]
pub struct OhplBytes {
    ptr: *mut u8,
    len: usize,
}

impl From<Vec<u8>> for OhplBytes {
    fn from(v: Vec<u8>) -> Self {
        let b = v.into_boxed_slice();
        let len = b.len();
        Self {
            ptr: Box::into_raw(b).cast(),
            len,
        }
    }
}

/// Writes the GLSL (returns true) or the error message (returns false) to `out`.
///
/// # Safety
/// `src` points to `len` readable bytes (or `len` is 0); `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_hpsl_transpile(
    src: *const u8,
    len: usize,
    shader_type: i32,
    out: *mut OhplBytes,
) -> bool {
    let src: &[u8] = if len == 0 {
        &[]
    } else {
        // SAFETY: caller contract above.
        unsafe { std::slice::from_raw_parts(src, len) }
    };
    let result = crate::hpsl::transpile(src, crate::hpsl::ShaderType::from_raw(shader_type));
    let ok = result.is_ok();
    // SAFETY: caller contract above.
    unsafe { out.write(result.unwrap_or_else(|e| e).into()) };
    ok
}

/// # Safety
/// `bytes` was filled by `ohpl_hpsl_transpile` and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_bytes_free(bytes: OhplBytes) {
    // SAFETY: ptr/len come from the boxed slice leaked in `From<Vec<u8>>`.
    drop(unsafe { Box::from_raw(std::ptr::slice_from_raw_parts_mut(bytes.ptr, bytes.len)) });
}
