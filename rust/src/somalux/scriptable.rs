//! `SomaLuxScriptable.cpp`: id hashing, easing, timer and fader stepping.

use crate::math::{cos, sin};

pub fn hash64(s: &[u8]) -> u64 {
    if s.is_empty() {
        return 0;
    }
    let mut h = 0x1465_0fb0_739d_0383u64;
    for &c in s.iter().rev() {
        h = (i64::from(c as i8) as u64 ^ h).wrapping_mul(0x0000_0100_0000_01b3);
    }
    h
}

unsafe extern "C" {
    fn powf(x: f32, y: f32) -> f32;
}

fn pw(x: f32, y: f32) -> f32 {
    // SAFETY: pure libm function on plain floats.
    unsafe { powf(x, y) }
}

fn bounce_out(mut x: f32) -> f32 {
    if x < 1.0 / 2.75 {
        return 7.5625 * x * x;
    }
    if x < 2.0 / 2.75 {
        x -= 1.5 / 2.75;
        return (7.5625 * x).mul_add(x, 0.75);
    }
    if x < 2.5 / 2.75 {
        x -= 2.25 / 2.75;
        return (7.5625 * x).mul_add(x, 0.9375);
    }
    x -= 2.625 / 2.75;
    (7.5625 * x).mul_add(x, 0.984_375)
}

pub fn easing(kind: i32, t: f32) -> f32 {
    const PI: f32 = std::f32::consts::PI;
    let c1: f32 = 1.701_58;
    let c2 = c1 * 1.525;
    let c3 = c1 + 1.0;
    let c4 = (2.0 * PI) / 3.0;
    let c5 = (2.0 * PI) / 4.5;
    match kind {
        1 => 1.0 - cos(t * PI / 2.0),
        2 => sin(t * PI / 2.0),
        3 => -(cos(PI * t) - 1.0) / 2.0,
        4 => t * t,
        5 => (-(1.0 - t)).mul_add(1.0 - t, 1.0),
        6 => {
            if t < 0.5 {
                2.0 * t * t
            } else {
                1.0 - pw(-2.0 * t + 2.0, 2.0) / 2.0
            }
        }
        7 => t * t * t,
        8 => 1.0 - pw(1.0 - t, 3.0),
        9 => {
            if t < 0.5 {
                4.0 * t * t * t
            } else {
                1.0 - pw(-2.0 * t + 2.0, 3.0) / 2.0
            }
        }
        10 => t * t * t * t,
        11 => 1.0 - pw(1.0 - t, 4.0),
        12 => {
            if t < 0.5 {
                8.0 * pw(t, 4.0)
            } else {
                1.0 - pw(-2.0 * t + 2.0, 4.0) / 2.0
            }
        }
        13 => pw(t, 5.0),
        14 => 1.0 - pw(1.0 - t, 5.0),
        15 => {
            if t < 0.5 {
                16.0 * pw(t, 5.0)
            } else {
                1.0 - pw(-2.0 * t + 2.0, 5.0) / 2.0
            }
        }
        16 => {
            if t == 0.0 {
                0.0
            } else {
                pw(2.0, 10.0f32.mul_add(t, -10.0))
            }
        }
        17 => {
            if t == 1.0 {
                1.0
            } else {
                1.0 - pw(2.0, -10.0 * t)
            }
        }
        18 => {
            if t == 0.0 {
                0.0
            } else if t == 1.0 {
                1.0
            } else if t < 0.5 {
                pw(2.0, 20.0f32.mul_add(t, -10.0)) / 2.0
            } else {
                (2.0 - pw(2.0, (-20.0f32).mul_add(t, 10.0))) / 2.0
            }
        }
        19 => 1.0 - (-t).mul_add(t, 1.0).sqrt(),
        20 => (-(t - 1.0)).mul_add(t - 1.0, 1.0).sqrt(),
        21 => {
            if t < 0.5 {
                (1.0 - (-(4.0 * t)).mul_add(t, 1.0).sqrt()) / 2.0
            } else {
                ({
                    let x = (-2.0f32).mul_add(t, 2.0);
                    (1.0 - x * x).sqrt()
                } + 1.0)
                    / 2.0
            }
        }
        22 => (c3 * t * t).mul_add(t, -(c1 * t * t)),
        23 => c1.mul_add((t - 1.0) * (t - 1.0), c3.mul_add(pw(t - 1.0, 3.0), 1.0)),
        24 => {
            if t < 0.5 {
                (pw(2.0 * t, 2.0) * ((c2 + 1.0) * 2.0).mul_add(t, -c2)) / 2.0
            } else {
                (pw(2.0 * t - 2.0, 2.0).mul_add((c2 + 1.0).mul_add(t * 2.0 - 2.0, c2), 2.0)) / 2.0
            }
        }
        25 => {
            if t == 0.0 {
                0.0
            } else if t == 1.0 {
                1.0
            } else {
                -pw(2.0, 10.0f32.mul_add(t, -10.0)) * sin(t.mul_add(10.0, -10.75) * c4)
            }
        }
        26 => {
            if t == 0.0 {
                0.0
            } else if t == 1.0 {
                1.0
            } else {
                pw(2.0, -10.0 * t).mul_add(sin((t * 10.0 - 0.75) * c4), 1.0)
            }
        }
        27 => {
            if t == 0.0 {
                0.0
            } else if t == 1.0 {
                1.0
            } else if t < 0.5 {
                -(pw(2.0, 20.0f32.mul_add(t, -10.0)) * sin(20.0f32.mul_add(t, -11.125) * c5)) / 2.0
            } else {
                (pw(2.0, (-20.0f32).mul_add(t, 10.0)) * sin(20.0f32.mul_add(t, -11.125) * c5)) / 2.0
                    + 1.0
            }
        }
        28 => 1.0 - bounce_out(1.0 - t),
        29 => bounce_out(t),
        30 => {
            if t < 0.5 {
                (1.0 - bounce_out(1.0 - 2.0 * t)) / 2.0
            } else {
                (1.0 + bounce_out(2.0 * t - 1.0)) / 2.0
            }
        }
        _ => t,
    }
}

/// Prefix of `cSomaLuxScriptable::cTimer`; the script function name follows in C++.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Timer {
    pub id: u64,
    pub length: f32,
    pub time_left: f32,
    pub repeat: bool,
    pub paused: bool,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Fader {
    pub id: u64,
    pub value: f32,
    pub start: f32,
    pub goal: f32,
    pub speed: f32,
    pub reverse_at_end: bool,
    pub paused: bool,
}

/// Counts one timer down; `(due, keep)` where a one-shot timer that fired is not kept.
pub fn step_timer(t: &mut Timer, dt: f32) -> (bool, bool) {
    if !t.paused {
        t.time_left -= dt;
    }
    if t.paused || t.time_left > 0.0 || t.time_left.is_nan() {
        return (false, true);
    }
    if t.repeat {
        t.time_left += t.length;
        (true, true)
    } else {
        (true, false)
    }
}

pub fn update_faders(faders: &mut [Fader], dt: f32) {
    for f in faders {
        if f.paused || f.value == f.goal {
            continue;
        }
        let step = f.speed * dt;
        if (f.goal - f.value).abs() <= step {
            f.value = f.goal;
            if f.reverse_at_end {
                std::mem::swap(&mut f.start, &mut f.goal);
                f.reverse_at_end = false;
            }
        } else {
            f.value += if f.goal > f.value { step } else { -step };
        }
    }
}

pub fn fade_speed(value: f32, goal: f32, time: f32) -> f32 {
    if time > 0.0 {
        (goal - value).abs() / time
    } else {
        1e9
    }
}

fn clamp01(t: f32) -> f32 {
    t.clamp(0.0, 1.0)
}

fn eased(t: f32, min: f32, max: f32, kind: i32, abs: bool) -> f32 {
    let v = (max - min).mul_add(easing(kind, clamp01(t)), min);
    if abs { v.abs() } else { v }
}

/// `timer` is `(length, time_left)` of the found timer.
pub fn timer_value(timer: Option<(f32, f32)>, min: f32, max: f32, kind: i32, abs: bool) -> f32 {
    let t = match timer {
        Some((length, left)) if length > 0.0 => 1.0 - left / length,
        _ => 1.0,
    };
    eased(t, min, max, kind, abs)
}

pub fn timer_time_has_passed(timer: Option<(f32, f32)>, length: f32) -> bool {
    match timer {
        None => true,
        Some((l, left)) => l - left >= length,
    }
}

pub fn fader_value(value: Option<f32>, min: f32, max: f32, kind: i32, abs: bool) -> f32 {
    eased(value.unwrap_or(0.0), min, max, kind, abs)
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum TimerAdd {
    Skip,
    Push,
    Overwrite,
}

pub fn timer_add(exists: bool, create_if_exist: bool) -> TimerAdd {
    match (exists, create_if_exist) {
        (true, false) => TimerAdd::Skip,
        (true, true) => TimerAdd::Overwrite,
        (false, _) => TimerAdd::Push,
    }
}
