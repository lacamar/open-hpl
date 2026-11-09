//! C ABI for `platform.rs`.

use crate::platform::{Kind, SystemEnv, special_path};
use std::ffi::c_void;

type Sink = unsafe extern "C" fn(*mut c_void, *const u8, usize);

/// Resolves `kind` (0 personal, 1 data, 2 config, 3 cache, 4 state, 5 pictures) from the process
/// environment and passes the bytes to `sink`. Returns false, emitting nothing, for an unknown
/// kind or when HOME is needed but unset.
///
/// # Safety
/// `sink` accepts `ctx` and a slice valid for the call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_platform_special_path(
    kind: u32,
    sink: Sink,
    ctx: *mut c_void,
) -> bool {
    let Some(p) = Kind::from_u32(kind).and_then(|k| special_path(&SystemEnv, k)) else {
        return false;
    };
    // SAFETY: caller contract.
    unsafe { sink(ctx, p.as_ptr(), p.len()) };
    true
}
