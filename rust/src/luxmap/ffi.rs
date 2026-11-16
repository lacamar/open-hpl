//! C ABI of `luxmap`, mirrored in `amnesia/src/game/RustLuxMap.h`.
//! Texts are (pointer, length) in; results leave through sinks.

use super::area::{
    self, BuoyIn, BuoyOut, ExamineIn, ExamineOut, LadderSetup, PlayerStart, StickyAttachIn,
    StickyAttachOut,
};
use super::handler;
use crate::math::Planef;
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

pure!(ohpl_ga5_body_skipped(collide: bool, active: bool, mass: f32, is_char: bool) -> bool => area::body_skipped(collide, active, mass, is_char));
pure!(ohpl_ga5_liquid_surface_y(pos_y: f32, size_y: f32) -> f32 => area::liquid_surface_y(pos_y, size_y));
pure!(ohpl_ga5_wave_surface_y(time: f32, freq: f32, px: f32, pz: f32, amp: f32, surface_y: f32) -> f32 => area::wave_surface_y(time, freq, px, pz, amp, surface_y));
pure!(ohpl_ga5_liquid_char(is_player: bool, has_enemy: bool, inside: bool, in_water: bool) -> i32 => area::liquid_char(is_player, has_enemy, inside, in_water));
pure!(ohpl_ga5_sticky_inside(inside: bool, is_last: bool) -> i32 => area::sticky_inside(inside, is_last));
pure!(ohpl_ga5_sticky_after_callback(has_callback: bool, allow: bool) -> bool => area::sticky_after_callback(has_callback, allow));
pure!(ohpl_ga5_rope_after_world(created: bool, nothing_linked: bool) -> i32 => area::rope_after_world(created, nothing_linked));
pure!(ohpl_ga5_rope_max_segments(max_len: f32, segment: f32) -> i32 => area::rope_max_segments(max_len, segment));

/// # Safety
/// `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga5_wave_plane(surface_y: f32, out: *mut Planef) {
    // SAFETY: caller contract.
    unsafe { wr(out, area::wave_plane(surface_y)) }
}

/// # Safety
/// `i` is readable, `out` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga5_liquid_body(
    i: *const BuoyIn,
    dist_sqr: f32,
    max_dist_sqr: f32,
    out: *mut BuoyOut,
) {
    // SAFETY: caller contract.
    unsafe { wr(out, area::liquid_body(rd(i), dist_sqr, max_dist_sqr)) }
}

/// Returns skip; `allow` receives the new `mbAllowAttachment`.
///
/// # Safety
/// Texts valid for their length, `allow` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga5_sticky_name(
    attach: *const u8,
    attach_n: usize,
    body: *const u8,
    body_n: usize,
    allow: *mut bool,
) -> bool {
    // SAFETY: caller contract.
    let (skip, a) = area::sticky_name(unsafe { units(attach, attach_n) }, unsafe {
        units(body, body_n)
    });
    // SAFETY: caller contract.
    unsafe { wr(allow, a) };
    skip
}

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
