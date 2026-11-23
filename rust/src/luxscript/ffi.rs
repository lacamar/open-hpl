//! C ABI of `luxscript`. Inputs are (pointer, length); outputs go through sinks.

use super::help;
use super::lookup;
use super::tables;
use super::types::{self, AlphaFader, CollideStep};
use super::vars;
use crate::math;
use crate::string;
use crate::string_ffi::{Sink, emit, units};

use std::ffi::c_void;

macro_rules! text_fn {
    ($name:ident, ($($a:ident: $at:ty),*), $s:ident => $body:expr) => {
        /// # Safety
        /// `$s` valid for its length.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name($($a: $at,)* $s: *const u8, n: usize) -> i32 {
            // SAFETY: caller contract.
            let $s = unsafe { units($s, n) };
            $body
        }
    };
}

macro_rules! sink_fn {
    ($name:ident, ($($a:ident: $at:ty),*) => $body:expr) => {
        /// # Safety
        /// `sink` accepts `ctx`.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name($($a: $at,)* sink: Sink<u8>, ctx: *mut c_void) {
            let r: Vec<u8> = $body;
            // SAFETY: caller contract.
            unsafe { emit(sink, ctx, &r) }
        }
    };
}

macro_rules! scalar {
    ($name:ident, $f:expr; ($($a:ident),*)) => {
        #[unsafe(no_mangle)]
        pub extern "C" fn $name($($a: f32),*) -> f32 {
            $f($($a),*)
        }
    };
}

// lookup

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga1_type_prep(entity_type: i32, want_type: i32, want_sub: i32) -> i32 {
    lookup::type_prep(entity_type, want_type, want_sub)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga1_type_sub(got: i32, want: i32) -> bool {
    got == want
}

// variables and strings

sink_fn!(ohpl_ga1_var_from_int, (x: i32) => vars::from_int(x));
sink_fn!(ohpl_ga1_var_from_float, (x: f32) => vars::from_float(x));

text_fn!(ohpl_ga1_var_get_int, (), s => vars::get_int(s));
text_fn!(ohpl_ga1_string_to_int, (), s => string::token_to_int(s));

// tables

text_fn!(ohpl_ga1_progress_level, (), s => tables::progress_level(s));
text_fn!(ohpl_ga1_damage_type, (), s => tables::damage_type(s));
text_fn!(ohpl_ga1_crosshair, (), s => tables::crosshair(s));
text_fn!(ohpl_ga1_pose, (), s => tables::pose(s));

scalar!(ohpl_ga1_sanity_boost, tables::sanity_boost; (s));
scalar!(ohpl_ga1_sanity_boost_small, tables::sanity_boost_small; (s));
scalar!(ohpl_ga1_fade_speed, tables::fade_speed; (t));
scalar!(ohpl_ga1_inverse, tables::inverse; (t));

// script math

scalar!(ohpl_ga1_sin, math::sin; (x));
scalar!(ohpl_ga1_cos, math::cos; (x));
scalar!(ohpl_ga1_tan, math::tan; (x));
scalar!(ohpl_ga1_asin, math::asin; (x));
scalar!(ohpl_ga1_acos, math::acos; (x));
scalar!(ohpl_ga1_atan, f32::atan; (x));
scalar!(ohpl_ga1_atan2, math::atan2; (x, y));
scalar!(ohpl_ga1_sqrt, f32::sqrt; (x));
scalar!(ohpl_ga1_pow, f32::powf; (b, e));
scalar!(ohpl_ga1_min, tables::min; (a, b));
scalar!(ohpl_ga1_max, tables::max; (a, b));
scalar!(ohpl_ga1_clamp, math::clamp; (x, lo, hi));
scalar!(ohpl_ga1_abs, f32::abs; (x));

// help funcs

/// # Safety
/// `input` valid for `n` units; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga1_parse_plan(
    input: *const u32,
    n: usize,
    sink: Sink<u32>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, &help::parse_plan(units(input, n))) }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga1_string_duration(start: f32, min: f32, per_char: f32, len: usize) -> f32 {
    help::string_duration(start, min, per_char, len)
}

// types

/// # Safety
/// `f` points to a live `cLuxAlphaFader`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga1_alpha_reset(f: *mut AlphaFader) {
    // SAFETY: caller contract.
    if let Some(f) = unsafe { f.as_mut() } {
        f.reset();
    }
}

/// # Safety
/// `f` points to a live `cLuxAlphaFader`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga1_alpha_update(f: *mut AlphaFader, dt: f32) {
    // SAFETY: caller contract.
    if let Some(f) = unsafe { f.as_mut() } {
        f.update(dt);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga1_collide_step(
    colliding: bool,
    states: i32,
    collide: bool,
) -> CollideStep {
    types::collide_step(colliding, states, collide)
}

/// # Safety
/// Pointers valid for their lengths; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga1_collide_command(
    func: *const u8,
    func_n: usize,
    name: *const u8,
    name_n: usize,
    entity: *const u8,
    entity_n: usize,
    state: i32,
    sink: Sink<u8>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    unsafe {
        let c = types::collide_command(
            units(func, func_n),
            units(name, name_n),
            units(entity, entity_n),
            state,
        );
        emit(sink, ctx, &c);
    }
}

/// # Safety
/// `count` points to a live float.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga1_enemy_sound_update(count: *mut f32, dt: f32) {
    // SAFETY: caller contract.
    if let Some(c) = unsafe { count.as_mut() } {
        types::enemy_sound_update(c, dt);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga1_enemy_sound_allowed(count: f32) -> bool {
    types::enemy_sound_allowed(count)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga1_enemy_sound_cooldown() -> f32 {
    types::ENEMY_SOUND_COOLDOWN
}
