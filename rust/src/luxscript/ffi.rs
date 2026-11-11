//! C ABI of `luxscript`. Inputs are (pointer, length); outputs go through sinks.

use super::help;
use super::lookup::{self, Wild};
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

/// # Safety
/// `name` valid for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga1_has_wildcard(name: *const u8, n: usize) -> bool {
    // SAFETY: caller contract.
    lookup::has_wildcard(unsafe { units(name, n) })
}

/// # Safety
/// `name` valid for `n` bytes. Free the result with `ohpl_ga1_wild_free`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga1_wild_new(name: *const u8, n: usize) -> *mut Wild {
    // SAFETY: caller contract.
    Box::into_raw(Box::new(Wild::new(unsafe { units(name, n) })))
}

/// # Safety
/// `w` is null or from `ohpl_ga1_wild_new`, freed once.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga1_wild_free(w: *mut Wild) {
    if !w.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(w) });
    }
}

/// # Safety
/// `w` from `ohpl_ga1_wild_new`; `name` valid for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga1_wild_matches(w: *const Wild, name: *const u8, n: usize) -> bool {
    // SAFETY: caller contract.
    unsafe { w.as_ref() }.is_some_and(|w| w.matches(unsafe { units(name, n) }))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga1_type_prep(entity_type: i32, want_type: i32, want_sub: i32) -> i32 {
    lookup::type_prep(entity_type, want_type, want_sub)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga1_type_sub(got: i32, want: i32) -> bool {
    got == want
}

/// # Safety
/// Pointers valid for their lengths.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga1_body_matches(
    body: *const u8,
    body_n: usize,
    entity_name_len: i32,
    want: *const u8,
    want_n: usize,
) -> bool {
    // SAFETY: caller contract.
    let (body, want) = unsafe { (units(body, body_n), units(want, want_n)) };
    lookup::body_matches(body, entity_name_len, want)
}

/// # Safety
/// `want` valid for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga1_searches_bodies(
    want: *const u8,
    n: usize,
    body_num: i32,
) -> bool {
    // SAFETY: caller contract.
    lookup::searches_bodies(unsafe { units(want, n) }, body_num)
}

// variables and strings

sink_fn!(ohpl_ga1_var_from_int, (x: i32) => vars::from_int(x));
sink_fn!(ohpl_ga1_var_from_float, (x: f32) => vars::from_float(x));

/// # Safety
/// `cur` valid for `n` bytes; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga1_var_add_int(
    cur: *const u8,
    n: usize,
    d: i32,
    sink: Sink<u8>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, &vars::add_int(units(cur, n), d)) }
}

/// # Safety
/// `cur` valid for `n` bytes; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga1_var_add_float(
    cur: *const u8,
    n: usize,
    d: f32,
    sink: Sink<u8>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, &vars::add_float(units(cur, n), d)) }
}

text_fn!(ohpl_ga1_var_get_int, (), s => vars::get_int(s));
text_fn!(ohpl_ga1_string_to_int, (), s => string::token_to_int(s));

/// # Safety
/// `s` valid for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga1_var_get_float(s: *const u8, n: usize) -> f32 {
    // SAFETY: caller contract.
    vars::get_float(unsafe { units(s, n) })
}

/// # Safety
/// `s` valid for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga1_string_to_float(s: *const u8, n: usize) -> f32 {
    // SAFETY: caller contract.
    string::token_to_float(unsafe { units(s, n) })
}

/// # Safety
/// `s` valid for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga1_string_to_bool(s: *const u8, n: usize) -> bool {
    // SAFETY: caller contract.
    string::to_bool(unsafe { units(s, n) })
}

/// # Safety
/// Pointers valid for their lengths.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga1_string_contains(
    s: *const u8,
    n: usize,
    sub: *const u8,
    m: usize,
) -> bool {
    // SAFETY: caller contract.
    vars::contains(unsafe { units(s, n) }, unsafe { units(sub, m) })
}

// tables

text_fn!(ohpl_ga1_progress_level, (), s => tables::progress_level(s));
text_fn!(ohpl_ga1_damage_type, (), s => tables::damage_type(s));
text_fn!(ohpl_ga1_crosshair, (), s => tables::crosshair(s));
text_fn!(ohpl_ga1_pose, (), s => tables::pose(s));
text_fn!(ohpl_ga1_achievement, (), s => tables::achievement(s));
text_fn!(ohpl_ga1_quest_achievement, (), s => tables::quest_achievement(s));

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga1_achievement_hardmode(id: i32) -> bool {
    tables::achievement_hardmode(id)
}

/// # Safety
/// `s` valid for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga1_sound_is_entity(s: *const u8, n: usize) -> bool {
    // SAFETY: caller contract.
    tables::sound_is_entity(unsafe { units(s, n) })
}

/// # Safety
/// `s` valid for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga1_coord_is_local(s: *const u8, n: usize) -> bool {
    // SAFETY: caller contract.
    tables::coord_is_local(unsafe { units(s, n) })
}

scalar!(ohpl_ga1_sanity_boost, tables::sanity_boost; (s));
scalar!(ohpl_ga1_sanity_boost_small, tables::sanity_boost_small; (s));
scalar!(ohpl_ga1_fade_speed, tables::fade_speed; (t));
scalar!(ohpl_ga1_inverse, tables::inverse; (t));

/// # Safety
/// `f`, `r`, `u`, `out` each valid for 3 floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga1_local_force(
    f: *const f32,
    r: *const f32,
    u: *const f32,
    y: f32,
    z: f32,
    out: *mut f32,
) {
    // SAFETY: caller contract.
    unsafe {
        let v = tables::local_force(
            f.cast::<[f32; 3]>().read_unaligned(),
            r.cast::<[f32; 3]>().read_unaligned(),
            u.cast::<[f32; 3]>().read_unaligned(),
            y,
            z,
        );
        out.cast::<[f32; 3]>().write_unaligned(v);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga1_rope_segments(total: f32, segment: f32) -> i32 {
    tables::rope_segments(total, segment)
}

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
