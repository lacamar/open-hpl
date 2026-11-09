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
