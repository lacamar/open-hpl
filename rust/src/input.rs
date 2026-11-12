//! Input state machines of `Keyboard`, `Mouse`, `Gamepad` (SDL2 backend) and `cAction`, plus the
//! device name tables. SDL events arrive pre-flattened as [`Event`]; the C++ shims keep every SDL call.
//!
//! Deliberate gaps, all UB in the C++: queries and pops past the end return a neutral value
//! instead of reading out of bounds; a pad event whose axis/button index exceeds the arrays is
//! ignored (arrays are sized from SDL's own maxima, so SDL never produces one).

use crate::input_tables::{KEY_LAST, KEY_NAMES, KEY_NONE, NAME_KEYS, SDL_KEYS};
use crate::string::utf8_to_wide;
use crate::types::Vector2l;
use std::collections::VecDeque;
use std::sync::atomic::{AtomicU32, Ordering};

pub mod registry;

pub const KIND_KEY: u32 = 1;
pub const KIND_TEXT: u32 = 2;
pub const KIND_MOUSE_MOTION: u32 = 3;
pub const KIND_MOUSE_BUTTON: u32 = 4;
pub const KIND_WHEEL: u32 = 5;
pub const KIND_PAD_AXIS: u32 = 6;
pub const KIND_PAD_BUTTON: u32 = 7;
pub const KIND_PAD_REMOVED: u32 = 8;

/// One `SDL_Event`, flattened. `state`: key/pad-button `state` field, or 1 for mouse-button down.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct Event {
    pub kind: u32,
    pub sym: i32,
    pub key_mod: i32,
    pub state: u8,
    pub button: u8,
    pub axis_value: i16,
    pub x: i32,
    pub y: i32,
    pub xrel: i32,
    pub yrel: i32,
    pub wheel_y: i32,
    pub which: i32,
    pub text: [u8; 32],
}

const _: () = {
    use std::mem::{align_of, offset_of, size_of};
    assert!(size_of::<Event>() == 72 && align_of::<Event>() == 4);
    assert!(offset_of!(Event, state) == 12 && offset_of!(Event, x) == 16);
    assert!(offset_of!(Event, which) == 36 && offset_of!(Event, text) == 40);
};

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct KeyPress {
    pub key: i32,
    pub unicode: i32,
    pub modifier: i32,
}

fn key_modifier(sdl_mod: i32) -> i32 {
    let mut m = 0;
    if sdl_mod & 0x00c0 != 0 {
        m |= 1;
    }
    if sdl_mod & 0x0003 != 0 {
        m |= 2;
    }
    if sdl_mod & 0x0300 != 0 {
        m |= 4;
    }
    m
}

fn sdl_to_key(sym: i32) -> u16 {
    SDL_KEYS
        .iter()
        .find(|&&(s, _)| s == sym)
        .map_or(KEY_NONE, |&(_, k)| k)
}

pub struct Keyboard {
    down: Vec<bool>,
    deferred: Vec<u16>,
    pressed: VecDeque<KeyPress>,
    released: VecDeque<KeyPress>,
}

impl Default for Keyboard {
    fn default() -> Self {
        Self::new()
    }
}

impl Keyboard {
    pub fn new() -> Self {
        Self {
            down: vec![false; usize::from(KEY_LAST)],
            deferred: Vec::new(),
            pressed: VecDeque::new(),
            released: VecDeque::new(),
        }
    }

    fn set(&mut self, key: u16, v: bool) {
        if let Some(d) = self.down.get_mut(usize::from(key)) {
            *d = v;
        }
    }

    /// A release in the same update as a press of that key keeps the key down until the next update.
    pub fn update(&mut self, events: &[Event], mod_state: i32) {
        self.pressed.clear();
        self.released.clear();
        for k in std::mem::take(&mut self.deferred) {
            self.set(k, false);
        }
        let mut now = vec![false; self.down.len()];
        for ev in events {
            match ev.kind {
                KIND_KEY => {
                    let key = sdl_to_key(ev.sym);
                    let pressed = ev.state == 1;
                    let seen = now.get(usize::from(key)).copied().unwrap_or(false);
                    if pressed {
                        self.set(key, true);
                        if let Some(n) = now.get_mut(usize::from(key)) {
                            *n = true;
                        }
                    } else if seen {
                        self.deferred.push(key);
                    } else {
                        self.set(key, false);
                    }
                    let press = KeyPress {
                        key: i32::from(key),
                        unicode: 0,
                        modifier: key_modifier(ev.key_mod),
                    };
                    if pressed {
                        self.pressed.push_back(press);
                    } else {
                        self.released.push_back(press);
                    }
                }
                KIND_TEXT => {
                    let end = ev
                        .text
                        .iter()
                        .position(|&b| b == 0)
                        .unwrap_or(ev.text.len());
                    let bytes = ev.text.get(..end).unwrap_or(&[]);
                    for ch in utf8_to_wide(bytes) {
                        self.pressed.push_back(KeyPress {
                            key: i32::from(KEY_NONE),
                            unicode: ch as i32,
                            modifier: key_modifier(mod_state),
                        });
                    }
                }
                _ => {}
            }
        }
    }

    pub fn key_is_down(&self, key: i32) -> bool {
        usize::try_from(key)
            .ok()
            .and_then(|k| self.down.get(k))
            .copied()
            .unwrap_or(false)
    }

    pub fn key_is_pressed(&self) -> bool {
        !self.pressed.is_empty()
    }

    pub fn key_is_released(&self) -> bool {
        !self.released.is_empty()
    }

    pub fn pop_pressed(&mut self) -> Option<KeyPress> {
        self.pressed.pop_front()
    }

    pub fn pop_released(&mut self) -> Option<KeyPress> {
        self.released.pop_front()
    }
}

const MOUSE_BUTTONS: usize = 9;
const MB_LEFT: usize = 0;
const MB_MIDDLE: usize = 1;
const MB_RIGHT: usize = 2;
const MB_WHEEL_UP: usize = 3;
const MB_WHEEL_DOWN: usize = 4;
const MB_BUTTON6: usize = 5;
const MB_BUTTON7: usize = 6;

pub struct Mouse {
    buttons: [bool; MOUSE_BUTTONS],
    abs: Vector2l,
    rel: Vector2l,
    first_time: bool,
}

impl Default for Mouse {
    fn default() -> Self {
        Self::new()
    }
}

impl Mouse {
    pub fn new() -> Self {
        Self {
            buttons: [false; MOUSE_BUTTONS],
            abs: Vector2l::default(),
            rel: Vector2l::default(),
            first_time: true,
        }
    }

    pub fn take_first_time(&mut self) -> bool {
        std::mem::replace(&mut self.first_time, false)
    }

    fn set(&mut self, button: usize, v: bool) {
        if let Some(b) = self.buttons.get_mut(button) {
            *b = v;
        }
    }

    /// `rel_state` is `SDL_GetRelativeMouseState`; `headless` adds the motion events' `xrel/yrel`.
    /// A wheel event ends the scan: later events of the frame are dropped. Wheel `y <= 0` counts as down.
    pub fn update(&mut self, events: &[Event], headless: bool, rel_state: Vector2l) {
        let mut wheel_up = false;
        let mut wheel_down = false;
        let mut injected = Vector2l::default();
        for ev in events {
            match ev.kind {
                KIND_MOUSE_MOTION => {
                    if headless {
                        injected.x = injected.x.wrapping_add(ev.xrel);
                        injected.y = injected.y.wrapping_add(ev.yrel);
                    }
                    self.abs = Vector2l { x: ev.x, y: ev.y };
                }
                KIND_WHEEL => {
                    if ev.wheel_y > 0 {
                        self.set(MB_WHEEL_UP, true);
                        wheel_up = true;
                    } else {
                        self.set(MB_WHEEL_DOWN, true);
                        wheel_down = true;
                    }
                    break;
                }
                KIND_MOUSE_BUTTON => {
                    let down = ev.state != 0;
                    match ev.button {
                        1 => self.set(MB_LEFT, down),
                        2 => self.set(MB_MIDDLE, down),
                        3 => self.set(MB_RIGHT, down),
                        4 => self.set(MB_BUTTON6, down),
                        5 => self.set(MB_BUTTON7, down),
                        _ => {}
                    }
                }
                _ => {}
            }
        }
        self.set(MB_WHEEL_DOWN, wheel_down);
        self.set(MB_WHEEL_UP, wheel_up);
        self.rel = Vector2l {
            x: rel_state.x.wrapping_add(injected.x),
            y: rel_state.y.wrapping_add(injected.y),
        };
    }

    pub fn button_is_down(&self, button: i32) -> bool {
        usize::try_from(button)
            .ok()
            .and_then(|b| self.buttons.get(b))
            .copied()
            .unwrap_or(false)
    }

    pub fn abs_position(&self) -> Vector2l {
        self.abs
    }

    /// Read-and-clear.
    pub fn take_rel_position(&mut self) -> Vector2l {
        std::mem::take(&mut self.rel)
    }
}

pub const INPUT_BUTTON: i32 = 0;
pub const INPUT_AXIS: i32 = 2;
pub const INPUT_NONE: i32 = 4;

/// `cGamepadInputData`.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct InputData {
    pub pad_index: i32,
    pub input_type: i32,
    pub input_id: i32,
    pub value: f32,
}

impl Default for InputData {
    fn default() -> Self {
        Self {
            pad_index: 0,
            input_type: INPUT_NONE,
            input_id: 0,
            value: 0.0,
        }
    }
}

const INV_AXIS_MAX: f32 = 1.0 / 32767.5;
const DEAD_ZONE_DEFAULT: f32 = 0.333;

/// Shared by all pads, as the C++ statics. The squared radius is never recomputed by the setter.
static DEAD_ZONE: AtomicU32 = AtomicU32::new(DEAD_ZONE_DEFAULT.to_bits());
static DEAD_ZONE_SQR: AtomicU32 = AtomicU32::new((DEAD_ZONE_DEFAULT * DEAD_ZONE_DEFAULT).to_bits());

pub fn dead_zone() -> f32 {
    f32::from_bits(DEAD_ZONE.load(Ordering::Relaxed))
}

pub fn set_dead_zone(v: f32) {
    DEAD_ZONE.store(v.to_bits(), Ordering::Relaxed);
}

fn dead_zone_sqr() -> f32 {
    f32::from_bits(DEAD_ZONE_SQR.load(Ordering::Relaxed))
}

fn axis_value(v: i16) -> f32 {
    crate::math::clamp(f32::from(v) * INV_AXIS_MAX, -1.0, 1.0)
}

pub struct Gamepad {
    index: i32,
    instance: i32,
    axes: Vec<f32>,
    buttons: Vec<bool>,
    flush_threshold: i32,
    updates: VecDeque<InputData>,
    pressed: VecDeque<InputData>,
    released: VecDeque<InputData>,
    axis_changes: VecDeque<InputData>,
    last_active: u32,
}

fn remove_all(list: &mut VecDeque<InputData>, v: &InputData) {
    list.retain(|x| x != v);
}

impl Gamepad {
    /// `buttons` and `axes` are 0 when SDL could not open the controller.
    pub fn new(
        index: i32,
        instance: i32,
        buttons: usize,
        axes: usize,
        flush_threshold: i32,
    ) -> Self {
        Self {
            index,
            instance,
            axes: vec![0.0; axes],
            buttons: vec![false; buttons],
            flush_threshold,
            updates: VecDeque::new(),
            pressed: VecDeque::new(),
            released: VecDeque::new(),
            axis_changes: VecDeque::new(),
            last_active: u32::MAX,
        }
    }

    pub fn num_buttons(&self) -> usize {
        self.buttons.len()
    }

    pub fn num_axes(&self) -> usize {
        self.axes.len()
    }

    pub fn clear_state(&mut self) {
        self.buttons.fill(false);
        self.axes.fill(0.0);
    }

    /// `now` is the application time in ms; `last_active` keeps the C++ `unsigned int` truncation.
    pub fn time_since_last_active(&self, now: u64) -> f32 {
        if self.last_active == u32::MAX {
            return f32::MAX;
        }
        now.wrapping_sub(u64::from(self.last_active)) as f32 / 1000.0
    }

    fn update_axis_pair(&mut self, a0: usize, a1: usize) {
        let (Some(&v0), Some(&v1)) = (self.axes.get(a0), self.axes.get(a1)) else {
            return;
        };
        // gcc fuses v0*v0 + v1*v1 into fmadd(v0, v0, v1*v1).
        let len = v0.mul_add(v0, v1 * v1);
        let zero = len < dead_zone_sqr();
        if let Some(a) = self.axes.get_mut(a0) {
            *a = if zero { 0.0 } else { v0 };
        }
        if let Some(a) = self.axes.get_mut(a1) {
            *a = if zero { 0.0 } else { v1 };
        }
    }

    fn update_axis_single(&mut self, axis: usize) {
        let dz = dead_zone();
        if let Some(a) = self.axes.get_mut(axis)
            && a.abs() < dz
        {
            *a = 0.0;
        }
    }

    /// `now` is the application time in ms.
    pub fn update(&mut self, events: &[Event], now: u64) {
        self.updates.clear();
        self.pressed.clear();
        self.released.clear();
        let mut removed = false;
        let mut flushed: i32 = 0;
        for ev in events {
            match ev.kind {
                KIND_PAD_REMOVED => {
                    if self.instance == ev.which {
                        removed = true;
                    }
                }
                KIND_PAD_AXIS if self.instance == ev.which => {
                    let v = axis_value(ev.axis_value);
                    if let Some(cur) = self.axes.get_mut(usize::from(ev.button)) {
                        if v != *cur {
                            let d = InputData {
                                pad_index: self.index,
                                input_type: INPUT_AXIS,
                                input_id: i32::from(ev.button),
                                value: v,
                            };
                            self.axis_changes.push_back(d);
                            self.updates.push_back(d);
                        }
                        *cur = v;
                    }
                    if ev.axis_value == 0 || ev.axis_value == 16384 {
                        flushed = flushed.wrapping_add(1);
                    }
                }
                KIND_PAD_BUTTON if self.instance == ev.which => {
                    let is_pressed = ev.state != 0;
                    let mut d = InputData {
                        pad_index: self.index,
                        input_type: INPUT_BUTTON,
                        input_id: i32::from(ev.button),
                        value: 0.0,
                    };
                    if let Some(cur) = self.buttons.get_mut(usize::from(ev.button))
                        && *cur != is_pressed
                    {
                        if is_pressed {
                            d.value = 1.0;
                            self.pressed.push_back(d);
                        } else {
                            d.value = 0.0;
                            self.released.push_back(d);
                        }
                        self.updates.push_back(d);
                        *cur = is_pressed;
                    }
                    if !is_pressed {
                        flushed = flushed.wrapping_add(1);
                    }
                }
                _ => {}
            }
        }
        if self.axes.len() >= 6 {
            self.update_axis_pair(0, 1);
            self.update_axis_pair(2, 3);
            self.update_axis_single(4);
            self.update_axis_single(5);
        }
        if removed || flushed >= self.flush_threshold {
            self.updates.clear();
            self.pressed.clear();
            self.released.clear();
            self.buttons.fill(false);
            self.axes.fill(0.0);
        }
        let half = f64::from(dead_zone()) * 0.5;
        if self
            .updates
            .iter()
            .any(|i| i.input_type != INPUT_AXIS || f64::from(i.value.abs()) > half)
        {
            self.last_active = now as u32;
        }
    }

    pub fn has_input_updates(&self) -> bool {
        !self.updates.is_empty()
    }

    pub fn pop_input_update(&mut self) -> Option<InputData> {
        let input = self.updates.pop_front()?;
        match input.input_type {
            INPUT_BUTTON if input.value == 0.0 => remove_all(&mut self.released, &input),
            INPUT_BUTTON => remove_all(&mut self.pressed, &input),
            INPUT_AXIS => remove_all(&mut self.axis_changes, &input),
            _ => {}
        }
        Some(input)
    }

    pub fn button_is_down(&self, button: i32) -> bool {
        usize::try_from(button)
            .ok()
            .and_then(|b| self.buttons.get(b))
            .copied()
            .unwrap_or(false)
    }

    pub fn button_is_pressed(&self) -> bool {
        !self.pressed.is_empty()
    }

    pub fn button_is_released(&self) -> bool {
        !self.released.is_empty()
    }

    pub fn pop_button(&mut self) -> Option<InputData> {
        let b = self.pressed.pop_front()?;
        remove_all(&mut self.updates, &b);
        Some(b)
    }

    pub fn pop_released_button(&mut self) -> Option<InputData> {
        let b = self.released.pop_front()?;
        remove_all(&mut self.updates, &b);
        Some(b)
    }

    pub fn axis_value(&self, axis: i32) -> f32 {
        usize::try_from(axis)
            .ok()
            .and_then(|a| self.axes.get(a))
            .copied()
            .unwrap_or(0.0)
    }

    pub fn axes_updated(&self) -> bool {
        !self.axis_changes.is_empty()
    }

    pub fn pop_updated_axis(&mut self) -> Option<InputData> {
        let a = self.axis_changes.pop_front()?;
        remove_all(&mut self.updates, &a);
        Some(a)
    }
}

/// `cAction` flags and double-click timer.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct ActionState {
    became_triggered: bool,
    is_triggered: bool,
    double_down: bool,
    double_triggered: bool,
    is_down: bool,
    time_count: f64,
}

impl Default for ActionState {
    fn default() -> Self {
        Self {
            became_triggered: false,
            is_triggered: false,
            double_down: false,
            double_triggered: false,
            is_down: false,
            time_count: -1.0,
        }
    }
}

impl ActionState {
    pub fn is_down(&self) -> bool {
        self.is_down
    }

    pub fn was_triggered(&mut self) -> bool {
        if self.became_triggered && !self.is_down {
            self.became_triggered = false;
            return true;
        }
        false
    }

    pub fn became_triggered(&mut self) -> bool {
        if !self.is_triggered && self.is_down {
            self.is_triggered = true;
            return true;
        }
        false
    }

    pub fn double_triggered(&mut self, limit: f32) -> bool {
        if !self.double_down && self.is_down {
            self.double_down = true;
            if self.double_triggered || self.time_count < 0.0 || self.time_count > f64::from(limit)
            {
                self.double_triggered = false;
                self.time_count = 0.0;
                return false;
            }
            self.time_count = 0.0;
            self.is_triggered = true;
            self.double_triggered = true;
            return true;
        }
        false
    }

    /// `any_down`: whether any sub action is triggered right now.
    pub fn update(&mut self, any_down: bool, time_step: f32) {
        self.is_down = any_down;
        if any_down {
            self.became_triggered = true;
        } else {
            self.is_triggered = false;
            self.double_down = false;
            if self.time_count >= 0.0 {
                self.time_count += f64::from(time_step);
            }
        }
    }

    pub fn reset_to_current_state(&mut self, any_down: bool) {
        self.update(any_down, 0.01);
        if self.is_down {
            self.is_triggered = true;
            self.double_down = true;
        } else {
            self.became_triggered = false;
        }
    }
}

pub fn key_name(key: i32) -> &'static str {
    KEY_NAMES
        .iter()
        .find(|&&(k, _)| i32::from(k) == key)
        .map_or("Unknown", |&(_, n)| n)
}

pub fn key_from_name(name: &[u8]) -> i32 {
    NAME_KEYS
        .iter()
        .find(|&&(n, _)| n.as_bytes() == name)
        .map_or(i32::from(KEY_LAST), |&(_, k)| i32::from(k))
}

pub fn modifier_name(m: i32) -> &'static str {
    match m {
        0 => "",
        1 => "CTRL",
        2 => "SHIFT",
        4 => "ALT",
        _ => "Unknown",
    }
}

const MOUSE_NAMES: [&str; MOUSE_BUTTONS] = [
    "LeftMouse",
    "MiddleMouse",
    "RightMouse",
    "WheelUp",
    "WheelDown",
    "Mouse6",
    "Mouse7",
    "Mouse8",
    "Mouse9",
];

fn indexed(list: &[&'static str], i: i32, default: &'static str) -> &'static str {
    usize::try_from(i)
        .ok()
        .and_then(|i| list.get(i))
        .copied()
        .unwrap_or(default)
}

fn position(list: &[&str], name: &[u8], default: i32) -> i32 {
    list.iter()
        .position(|n| n.as_bytes() == name)
        .map_or(default, |i| i as i32)
}

pub fn mouse_button_name(b: i32) -> &'static str {
    indexed(&MOUSE_NAMES, b, "Unknown")
}

pub fn mouse_button_from_name(name: &[u8]) -> i32 {
    position(&MOUSE_NAMES, name, MOUSE_BUTTONS as i32)
}

const PAD_BUTTONS: [&str; 15] = [
    "Button A",
    "Button B",
    "Button X",
    "Button Y",
    "Button Back",
    "Button Guide",
    "Button Start",
    "Button LeftStick",
    "Button RightStick",
    "Button LeftShoulder",
    "Button RightShoulder",
    "Button DpadUp",
    "Button DpadDown",
    "Button DpadLeft",
    "Button DpadRight",
];
const PAD_BUTTON_LAST: i32 = 30;
const PAD_AXES: [&str; 6] = [
    "Axis LeftX",
    "Axis LeftY",
    "Axis RightX",
    "Axis RightY",
    "Axis LeftTrigger",
    "Axis RightTrigger",
];
const PAD_AXIS_LAST: i32 = 10;
const PAD_HATS: [&str; 10] = [
    "Hat 0", "Hat 1", "Hat 2", "Hat 3", "Hat 4", "Hat 5", "Hat 6", "Hat 7", "Hat 8", "Hat 9",
];
const PAD_HAT_LAST: i32 = 10;
const PAD_HAT_STATES: [(i32, &str); 9] = [
    (0, "Centered"),
    (1, "Up"),
    (2, "Right"),
    (4, "Down"),
    (8, "Left"),
    (3, "RightUp"),
    (6, "RightDown"),
    (9, "LeftUp"),
    (12, "LeftDown"),
];
const PAD_HAT_STATE_LAST: i32 = 0x10;

pub fn pad_button_name(b: i32) -> &'static str {
    indexed(&PAD_BUTTONS, b, "Unknown")
}

pub fn pad_button_from_name(n: &[u8]) -> i32 {
    position(&PAD_BUTTONS, n, PAD_BUTTON_LAST)
}

pub fn pad_axis_name(a: i32) -> &'static str {
    indexed(&PAD_AXES, a, "None")
}

pub fn pad_axis_from_name(n: &[u8]) -> i32 {
    position(&PAD_AXES, n, PAD_AXIS_LAST)
}

pub fn pad_axis_range_name(r: i32) -> &'static str {
    match r {
        1 => "Positive",
        2 => "Negative",
        _ => "None",
    }
}

pub fn pad_axis_range_from_name(n: &[u8]) -> i32 {
    match n {
        b"Positive" => 1,
        b"Negative" => 2,
        _ => 3,
    }
}

pub fn pad_axis_range_from_value(v: f32) -> i32 {
    if v > 0.0 {
        1
    } else if v < 0.0 {
        2
    } else {
        0
    }
}

pub fn pad_hat_name(h: i32) -> &'static str {
    indexed(&PAD_HATS, h, "None")
}

pub fn pad_hat_from_name(n: &[u8]) -> i32 {
    position(&PAD_HATS, n, PAD_HAT_LAST)
}

pub fn pad_hat_state_name(s: i32) -> &'static str {
    PAD_HAT_STATES
        .iter()
        .find(|&&(v, _)| v == s)
        .map_or("None", |&(_, n)| n)
}

pub fn pad_hat_state_from_name(n: &[u8]) -> i32 {
    PAD_HAT_STATES
        .iter()
        .find(|&&(_, s)| s.as_bytes() == n)
        .map_or(PAD_HAT_STATE_LAST, |&(v, _)| v)
}

pub fn pad_ball_name(b: i32) -> &'static str {
    if b == 0 { "Trackball 0" } else { "None" }
}

pub fn pad_ball_from_name(n: &[u8]) -> i32 {
    if n == b"Trackball 0" { 0 } else { 1 }
}

#[cfg(test)]
#[path = "input_tests.rs"]
mod tests;
