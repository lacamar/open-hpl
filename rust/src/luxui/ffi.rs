//! C ABI of the GA8 UI layer, mirrored in `amnesia/src/game/RustLuxUi.h`. Functions that produce
//! events write them to a caller array and return the count; the strings inside stay valid until
//! the next call on the same thread, so the caller copies them before running any event.

use super::commentary;
use super::event::Str;
use std::ffi::c_void;

type Ctx = *mut c_void;
type Sink = unsafe extern "C" fn(Ctx, *const u8, usize);

fn bytes<'a>(s: Str) -> &'a [u8] {
    if s.n == 0 || s.p.is_null() {
        return &[];
    }
    // SAFETY: the caller passes a live `n`-byte buffer.
    unsafe { std::slice::from_raw_parts(s.p, s.n) }
}

fn pb<'a>(p: *const u8, n: usize) -> &'a [u8] {
    bytes(Str { p, n })
}

fn sink(s: Sink, c: Ctx, v: &[u8]) {
    // SAFETY: `s` accepts `c`.
    unsafe { s(c, v.as_ptr(), v.len()) }
}

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
    sink(s, c, &commentary::focus_text(pb(talker, tn), pb(topic, pn)));
}

/// # Safety
/// Buffers are live; `s` accepts `c`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_comm_file(file: *const u8, n: usize, s: Sink, c: Ctx) {
    sink(s, c, &commentary::comment_file(pb(file, n)));
}
