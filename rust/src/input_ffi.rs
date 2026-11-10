//! C ABI for `input.rs`. Handles come from the matching `_new`, are owned by the C++ device object
//! and freed once with `_free`; null handles are tolerated everywhere and answer neutrally.

use crate::input::{
    self, ActionState, Event, Gamepad, InputData, KeyPress, Keyboard, Mouse, dead_zone,
    set_dead_zone,
};
use crate::string_ffi::{Sink, emit, units};
use crate::types::Vector2l;
use std::ffi::c_void;

/// # Safety
/// `p` is null or a live, unaliased handle.
unsafe fn handle<'a, T>(p: *mut T) -> Option<&'a mut T> {
    // SAFETY: caller contract.
    unsafe { p.as_mut() }
}

/// # Safety
/// `p` points to `n` events, or `n` is 0.
unsafe fn events<'a>(p: *const Event, n: usize) -> &'a [Event] {
    // SAFETY: caller contract.
    unsafe { units(p, n) }
}

/// # Safety
/// `out` is null or writable.
unsafe fn put<T>(out: *mut T, v: Option<T>) -> bool {
    // SAFETY: caller contract.
    match (unsafe { out.as_mut() }, v) {
        (Some(o), Some(v)) => {
            *o = v;
            true
        }
        _ => false,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_input_kb_new() -> *mut Keyboard {
    Box::into_raw(Box::new(Keyboard::new()))
}

/// # Safety
/// `kb` comes from `ohpl_input_kb_new` and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_input_kb_free(kb: *mut Keyboard) {
    if !kb.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(kb) });
    }
}

/// # Safety
/// `kb` is live; `ev` points to `n` events.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_input_kb_update(
    kb: *mut Keyboard,
    ev: *const Event,
    n: usize,
    mod_state: i32,
) {
    // SAFETY: caller contract.
    if let Some(k) = unsafe { handle(kb) } {
        k.update(unsafe { events(ev, n) }, mod_state);
    }
}

/// # Safety
/// `kb` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_input_kb_key_is_down(kb: *mut Keyboard, key: i32) -> bool {
    // SAFETY: caller contract.
    unsafe { handle(kb) }.is_some_and(|k| k.key_is_down(key))
}

/// # Safety
/// `kb` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_input_kb_key_is_pressed(kb: *mut Keyboard) -> bool {
    // SAFETY: caller contract.
    unsafe { handle(kb) }.is_some_and(|k| k.key_is_pressed())
}

/// # Safety
/// `kb` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_input_kb_key_is_released(kb: *mut Keyboard) -> bool {
    // SAFETY: caller contract.
    unsafe { handle(kb) }.is_some_and(|k| k.key_is_released())
}

/// Writes the oldest pressed key to `out`; false, leaving `out`, when none is queued.
///
/// # Safety
/// `kb` is live; `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_input_kb_pop_pressed(kb: *mut Keyboard, out: *mut KeyPress) -> bool {
    // SAFETY: caller contract.
    unsafe { put(out, handle(kb).and_then(|k| k.pop_pressed())) }
}

/// # Safety
/// `kb` is live; `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_input_kb_pop_released(kb: *mut Keyboard, out: *mut KeyPress) -> bool {
    // SAFETY: caller contract.
    unsafe { put(out, handle(kb).and_then(|k| k.pop_released())) }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_input_mouse_new() -> *mut Mouse {
    Box::into_raw(Box::new(Mouse::new()))
}

/// # Safety
/// `m` comes from `ohpl_input_mouse_new` and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_input_mouse_free(m: *mut Mouse) {
    if !m.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(m) });
    }
}

/// # Safety
/// `m` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_input_mouse_take_first_time(m: *mut Mouse) -> bool {
    // SAFETY: caller contract.
    unsafe { handle(m) }.is_some_and(|m| m.take_first_time())
}

/// # Safety
/// `m` is live; `ev` points to `n` events.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_input_mouse_update(
    m: *mut Mouse,
    ev: *const Event,
    n: usize,
    headless: bool,
    rel_state: Vector2l,
) {
    // SAFETY: caller contract.
    if let Some(m) = unsafe { handle(m) } {
        m.update(unsafe { events(ev, n) }, headless, rel_state);
    }
}

/// # Safety
/// `m` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_input_mouse_button_is_down(m: *mut Mouse, button: i32) -> bool {
    // SAFETY: caller contract.
    unsafe { handle(m) }.is_some_and(|m| m.button_is_down(button))
}

/// # Safety
/// `m` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_input_mouse_abs(m: *mut Mouse) -> Vector2l {
    // SAFETY: caller contract.
    unsafe { handle(m) }.map_or_else(Vector2l::default, |m| m.abs_position())
}

/// # Safety
/// `m` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_input_mouse_take_rel(m: *mut Mouse) -> Vector2l {
    // SAFETY: caller contract.
    unsafe { handle(m) }.map_or_else(Vector2l::default, |m| m.take_rel_position())
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_input_pad_new(
    index: i32,
    instance: i32,
    buttons: usize,
    axes: usize,
    flush_threshold: i32,
) -> *mut Gamepad {
    Box::into_raw(Box::new(Gamepad::new(
        index,
        instance,
        buttons,
        axes,
        flush_threshold,
    )))
}

/// # Safety
/// `g` comes from `ohpl_input_pad_new` and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_input_pad_free(g: *mut Gamepad) {
    if !g.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(g) });
    }
}

/// # Safety
/// `g` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_input_pad_num_buttons(g: *mut Gamepad) -> usize {
    // SAFETY: caller contract.
    unsafe { handle(g) }.map_or(0, |g| g.num_buttons())
}

/// # Safety
/// `g` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_input_pad_num_axes(g: *mut Gamepad) -> usize {
    // SAFETY: caller contract.
    unsafe { handle(g) }.map_or(0, |g| g.num_axes())
}

/// # Safety
/// `g` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_input_pad_clear_state(g: *mut Gamepad) {
    // SAFETY: caller contract.
    if let Some(g) = unsafe { handle(g) } {
        g.clear_state();
    }
}

/// # Safety
/// `g` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_input_pad_time_since_active(g: *mut Gamepad, now: u64) -> f32 {
    // SAFETY: caller contract.
    unsafe { handle(g) }.map_or(f32::MAX, |g| g.time_since_last_active(now))
}

/// # Safety
/// `g` is live; `ev` points to `n` events.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_input_pad_update(
    g: *mut Gamepad,
    ev: *const Event,
    n: usize,
    now: u64,
) {
    // SAFETY: caller contract.
    if let Some(g) = unsafe { handle(g) } {
        g.update(unsafe { events(ev, n) }, now);
    }
}

macro_rules! pad_flag {
    ($name:ident, $method:ident) => {
        /// # Safety
        /// `g` is live.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name(g: *mut Gamepad) -> bool {
            // SAFETY: caller contract.
            unsafe { handle(g) }.is_some_and(|g| g.$method())
        }
    };
}

macro_rules! pad_pop {
    ($name:ident, $method:ident) => {
        /// Writes the oldest entry to `out`; false, leaving `out`, when the queue is empty.
        ///
        /// # Safety
        /// `g` is live; `out` is writable.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name(g: *mut Gamepad, out: *mut InputData) -> bool {
            // SAFETY: caller contract.
            unsafe { put(out, handle(g).and_then(|g| g.$method())) }
        }
    };
}

pad_flag!(ohpl_input_pad_has_updates, has_input_updates);
pad_flag!(ohpl_input_pad_button_is_pressed, button_is_pressed);
pad_flag!(ohpl_input_pad_button_is_released, button_is_released);
pad_flag!(ohpl_input_pad_axes_updated, axes_updated);
pad_pop!(ohpl_input_pad_pop_update, pop_input_update);
pad_pop!(ohpl_input_pad_pop_button, pop_button);
pad_pop!(ohpl_input_pad_pop_released, pop_released_button);
pad_pop!(ohpl_input_pad_pop_axis, pop_updated_axis);

/// # Safety
/// `g` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_input_pad_button_is_down(g: *mut Gamepad, button: i32) -> bool {
    // SAFETY: caller contract.
    unsafe { handle(g) }.is_some_and(|g| g.button_is_down(button))
}

/// # Safety
/// `g` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_input_pad_axis_value(g: *mut Gamepad, axis: i32) -> f32 {
    // SAFETY: caller contract.
    unsafe { handle(g) }.map_or(0.0, |g| g.axis_value(axis))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_input_pad_dead_zone() -> f32 {
    dead_zone()
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_input_pad_set_dead_zone(v: f32) {
    set_dead_zone(v);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_input_action_new() -> ActionState {
    ActionState::default()
}

/// # Safety
/// `s` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_input_action_is_down(s: *mut ActionState) -> bool {
    // SAFETY: caller contract.
    unsafe { handle(s) }.is_some_and(|s| s.is_down())
}

/// # Safety
/// `s` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_input_action_was_triggered(s: *mut ActionState) -> bool {
    // SAFETY: caller contract.
    unsafe { handle(s) }.is_some_and(|s| s.was_triggered())
}

/// # Safety
/// `s` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_input_action_became_triggered(s: *mut ActionState) -> bool {
    // SAFETY: caller contract.
    unsafe { handle(s) }.is_some_and(|s| s.became_triggered())
}

/// # Safety
/// `s` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_input_action_double_triggered(
    s: *mut ActionState,
    limit: f32,
) -> bool {
    // SAFETY: caller contract.
    unsafe { handle(s) }.is_some_and(|s| s.double_triggered(limit))
}

/// # Safety
/// `s` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_input_action_update(
    s: *mut ActionState,
    any_down: bool,
    time_step: f32,
) {
    // SAFETY: caller contract.
    if let Some(s) = unsafe { handle(s) } {
        s.update(any_down, time_step);
    }
}

/// # Safety
/// `s` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_input_action_reset(s: *mut ActionState, any_down: bool) {
    // SAFETY: caller contract.
    if let Some(s) = unsafe { handle(s) } {
        s.reset_to_current_state(any_down);
    }
}

macro_rules! name_fn {
    ($name:ident, $f:path) => {
        /// # Safety
        /// `sink` accepts `ctx` and a slice valid for the call.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name(v: i32, sink: Sink<u8>, ctx: *mut c_void) {
            // SAFETY: caller contract.
            unsafe { emit(sink, ctx, $f(v).as_bytes()) }
        }
    };
}

macro_rules! from_name_fn {
    ($name:ident, $f:path) => {
        /// # Safety
        /// `p` points to `n` readable bytes, or `n` is 0.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name(p: *const u8, n: usize) -> i32 {
            // SAFETY: caller contract.
            $f(unsafe { units(p, n) })
        }
    };
}

name_fn!(ohpl_input_key_name, input::key_name);
name_fn!(ohpl_input_modifier_name, input::modifier_name);
name_fn!(ohpl_input_mouse_button_name, input::mouse_button_name);
name_fn!(ohpl_input_pad_button_name, input::pad_button_name);
name_fn!(ohpl_input_pad_axis_name, input::pad_axis_name);
name_fn!(ohpl_input_pad_axis_range_name, input::pad_axis_range_name);
name_fn!(ohpl_input_pad_hat_name, input::pad_hat_name);
name_fn!(ohpl_input_pad_hat_state_name, input::pad_hat_state_name);
name_fn!(ohpl_input_pad_ball_name, input::pad_ball_name);
from_name_fn!(ohpl_input_key_from_name, input::key_from_name);
from_name_fn!(
    ohpl_input_mouse_button_from_name,
    input::mouse_button_from_name
);
from_name_fn!(ohpl_input_pad_button_from_name, input::pad_button_from_name);
from_name_fn!(ohpl_input_pad_axis_from_name, input::pad_axis_from_name);
from_name_fn!(
    ohpl_input_pad_axis_range_from_name,
    input::pad_axis_range_from_name
);
from_name_fn!(ohpl_input_pad_hat_from_name, input::pad_hat_from_name);
from_name_fn!(
    ohpl_input_pad_hat_state_from_name,
    input::pad_hat_state_from_name
);
from_name_fn!(ohpl_input_pad_ball_from_name, input::pad_ball_from_name);

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_input_pad_axis_range_from_value(v: f32) -> i32 {
    input::pad_axis_range_from_value(v)
}
