//! C ABI of `luxmap`, mirrored in `amnesia/src/game/RustLuxMap.h`.
//! Texts are (pointer, length) in; results leave through sinks.

use super::area::{
    self, BuoyIn, BuoyOut, ExamineIn, ExamineOut, LadderSetup, PlayerStart, StickyAttachIn,
    StickyAttachOut,
};
use super::handler;
use crate::string_ffi::{Sink, emit, units};
use crate::types::{Matrixf, Vector3f};
use std::ffi::c_void;

const _: () = {
    use std::mem::{offset_of, size_of};
    assert!(size_of::<BuoyIn>() == 4 && size_of::<BuoyOut>() == 3);
    assert!(size_of::<ExamineIn>() == 5 && size_of::<ExamineOut>() == 4);
    assert!(size_of::<StickyAttachIn>() == 180 && offset_of!(StickyAttachIn, move_body) == 176);
    assert!(size_of::<StickyAttachOut>() == 88 && offset_of!(StickyAttachOut, mode) == 80);
    assert!(size_of::<LadderSetup>() == 20 && size_of::<PlayerStart>() == 16);
    assert!(size_of::<super::helper::SubMesh>() == 4);
};

/// # Safety
/// `p` is valid for writes.
unsafe fn wr<T>(p: *mut T, v: T) {
    // SAFETY: caller contract.
    unsafe { p.write(v) }
}

/// # Safety
/// `p` is valid for reads.
unsafe fn rd<T: Copy>(p: *const T) -> T {
    // SAFETY: caller contract.
    unsafe { *p }
}

macro_rules! pure {
    ($(#[$m:meta])* $name:ident ($($a:ident: $at:ty),*) -> $r:ty => $body:expr) => {
        $(#[$m])*
        #[unsafe(no_mangle)]
        pub extern "C" fn $name($($a: $at),*) -> $r {
            $body
        }
    };
}

macro_rules! sink {
    ($name:ident ($($a:ident: $at:ty),*; $($s:ident, $n:ident),*) => $body:expr) => {
        /// # Safety
        /// Each text is valid for its length; `sink` accepts `ctx`.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name($($a: $at,)* $($s: *const u8, $n: usize,)* sink: Sink<u8>, ctx: *mut c_void) {
            // SAFETY: caller contract.
            $(let $s = unsafe { units($s, $n) };)*
            let r: Vec<u8> = $body;
            // SAFETY: caller contract.
            unsafe { emit(sink, ctx, &r) }
        }
    };
}

// area

pure!(ohpl_ga5_rope_after_world(created: bool, nothing_linked: bool) -> i32 => area::rope_after_world(created, nothing_linked));
pure!(ohpl_ga5_rope_max_segments(max_len: f32, segment: f32) -> i32 => area::rope_max_segments(max_len, segment));

pub type FitsFn = unsafe extern "C" fn(*mut c_void, *const Vector3f) -> bool;

/// Returns whether a position was found; `start` holds it.
///
/// # Safety
/// `start` is exclusively borrowed; `fits` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga5_ladder_find_start(
    start: *mut Vector3f,
    max_y: f32,
    fits: FitsFn,
    ctx: *mut c_void,
) -> bool {
    // SAFETY: caller contract.
    let s = unsafe { rd(start) };
    // SAFETY: `fits` accepts `ctx` and a readable position.
    let r = area::ladder_find_start(s, max_y, &mut |p| unsafe { fits(ctx, &p) });
    match r {
        Some(p) => {
            // SAFETY: caller contract.
            unsafe { wr(start, p) };
            true
        }
        None => false,
    }
}

/// # Safety
/// `m` readable, `out` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga5_player_start(m: *const Matrixf, out: *mut PlayerStart) {
    // SAFETY: caller contract.
    unsafe { wr(out, area::player_start(&rd(m))) }
}

/// # Safety
/// `t` readable, `out` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga5_node_up(t: *const Vector3f, out: *mut Vector3f) {
    // SAFETY: caller contract.
    unsafe { wr(out, area::node_up(rd(t))) }
}

sink!(ohpl_ga5_call1(; func, fn_n, name, name_n) => area::call1(func, name));
sink!(ohpl_ga5_call2(; func, fn_n, a, a_n, b, b_n) => area::call2(func, a, b));

// handler

sink!(ohpl_ga5_map_name(; file, file_n) => handler::map_name(file));
