//! C ABI of the GA8 UI layer, mirrored in `amnesia/src/game/RustLuxUi.h`. Functions that produce
//! events write them to a caller array and return the count; the strings inside stay valid until
//! the next call on the same thread, so the caller copies them before running any event.

use super::event::Str;
use super::{commentary, keyentry};
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

// ---- key entry ------------------------------------------------------------------------------

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga8_key_verify(c: u8) -> i32 {
    i32::from(keyentry::verify_char(c))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga8_key_route(keyboard: i32, paste: i32, tab: i32) -> i32 {
    match keyentry::route(keyboard != 0, paste != 0, tab != 0) {
        keyentry::Route::Base => 0,
        keyentry::Route::Key => 1,
        keyentry::Route::Paste => 2,
    }
}

/// # Safety
/// The out pointers are writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_key_action(
    composed: i32,
    len: i32,
    pos: i32,
    mark: i32,
    ascii: u8,
    out_pos: *mut i32,
    out_epos: *mut i32,
    out_ch: *mut u8,
) -> i32 {
    match keyentry::key_action(composed != 0, len, pos, mark, ascii) {
        keyentry::KeyAct::Base => 0,
        keyentry::KeyAct::Swallow => 1,
        keyentry::KeyAct::Insert { pos, epos, ch } => {
            // SAFETY: writable per the contract.
            unsafe {
                out_pos.write(pos);
                out_epos.write(epos);
                out_ch.write(ch);
            }
            2
        }
    }
}

/// # Safety
/// `p` is `n` readable bytes, `out` is `cap` writable bytes (17 suffice).
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga8_key_paste(
    p: *const u8,
    n: usize,
    out: *mut u8,
    cap: usize,
) -> usize {
    let v = keyentry::paste_filter(pb(p, n));
    let k = v.len().min(cap);
    for (i, c) in v.iter().take(k).enumerate() {
        // SAFETY: i < cap.
        unsafe { out.add(i).write(*c) };
    }
    k
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
