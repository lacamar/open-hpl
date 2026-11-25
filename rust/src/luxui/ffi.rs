//! C ABI of the GA8 UI layer, mirrored in `amnesia/src/game/RustLuxUi.h`. Functions that produce
//! events write them to a caller array and return the count; the strings inside stay valid until
//! the next call on the same thread, so the caller copies them before running any event.

use super::commentary;
use crate::luxmap::mhost::slice_of;
use crate::string_ffi::emit;
use std::ffi::c_void;

type Ctx = *mut c_void;
type Sink = unsafe extern "C" fn(Ctx, *const u8, usize);

// ---- commentary icon ------------------------------------------------------------------------

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga8_comm_spin(y: f32, playing: i32, dt: f32) -> f32 {
    commentary::spin(y, playing != 0, dt)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga8_comm_set_playing(current: i32, next: i32) -> i32 {
    commentary::set_playing(current != 0, next != 0)
}

/// # Safety
/// Buffers are live; `s` accepts `c`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_comm_focus_text(
    talker: *const u8,
    tn: usize,
    topic: *const u8,
    pn: usize,
    s: Sink,
    c: Ctx,
) {
    unsafe {
        emit(
            s,
            c,
            &commentary::focus_text(slice_of(talker, tn), slice_of(topic, pn)),
        )
    };
}

/// # Safety
/// Buffers are live; `s` accepts `c`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_comm_file(file: *const u8, n: usize, s: Sink, c: Ctx) {
    unsafe { emit(s, c, &commentary::comment_file(slice_of(file, n))) };
}
