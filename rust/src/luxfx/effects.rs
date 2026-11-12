//! `LuxEffectHandler.cpp` effect state machines: fades, flashes, sepia, radial blur, image trail,
//! emotion flash and screen shake. Each step edits a POD copy of the C++ members and returns ops.

use super::hw::{madd, msub, mul};
use super::ops::Ops;
use crate::math::rand_rectf;

pub mod k {
    pub const AMOUNT_PARAMS: u32 = 1;
    pub const AMOUNT_OFF: u32 = 2;
    pub const EMO_FOV_OUT: u32 = 3;
    pub const EMO_UNPAUSE: u32 = 4;
    pub const EMO_RADIAL_OUT: u32 = 5;
    pub const SHAKE_HEAD: u32 = 6;
    pub const SHAKE_END: u32 = 7;
}

/// Sepia, image trail and radial blur share one fade-to-goal state.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Amount {
    pub amount: f32,
    pub goal: f32,
    pub speed: f32,
    pub active: u32,
}

/// `Update` of the three amount effects. Ops: params with the new amount, then deactivate the post
/// effect when the amount is not above zero.
pub fn amount_update(s: &mut Amount, dt: f32, o: &mut Ops) {
    // gcc computes the product once for both branches, so neither add is fused
    let step = mul(s.speed, dt);
    if s.goal < s.amount {
        s.amount -= step;
        if s.amount <= s.goal {
            s.amount = s.goal;
            s.active = 0;
        }
    } else {
        s.amount += step;
        if s.amount >= s.goal {
            s.amount = s.goal;
            s.active = 0;
        }
    }
    o.kf(k::AMOUNT_PARAMS, s.amount);
    if s.amount <= 0.0 {
        o.k(k::AMOUNT_OFF);
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Fade {
    pub goal: f32,
    pub alpha: f32,
    pub speed: f32,
    pub active: u32,
}

pub fn fade_in(s: &mut Fade, time: f32) {
    s.goal = 0.0;
    if time <= 0.0 {
        s.alpha = 0.0;
    } else {
        s.speed = 1.0 / time;
    }
    s.active = 1;
}

pub fn fade_out(s: &mut Fade, time: f32) {
    s.goal = 1.0;
    if time <= 0.0 {
        s.alpha = 1.0;
    } else {
        s.speed = 1.0 / time;
    }
    s.active = 1;
}

pub fn fade_direct(s: &mut Fade, x: f32) {
    s.active = u32::from(
        x.partial_cmp(&0.0) != Some(std::cmp::Ordering::Less)
            && x.partial_cmp(&0.0) != Some(std::cmp::Ordering::Equal),
    );
    s.goal = x;
    s.alpha = x;
}

pub fn fade_is_fading(s: &Fade) -> bool {
    (s.goal == 0.0 && s.alpha > 0.0) || (s.goal == 1.0 && s.alpha < 1.0)
}

pub fn fade_update(s: &mut Fade, dt: f32) {
    if s.goal == 0.0 && s.alpha > 0.0 {
        s.alpha = msub(s.speed, dt, s.alpha);
        if s.alpha < 0.0 {
            s.alpha = 0.0;
            s.active = 0;
        }
    } else if s.goal == 1.0 && s.alpha < 1.0 {
        s.alpha = madd(s.speed, dt, s.alpha);
        if s.alpha > 1.0 {
            s.alpha = 1.0;
        }
    }
}

pub fn fade_reset(s: &mut Fade) {
    s.goal = 0.0;
    s.alpha = 0.0;
    s.speed = 1.0;
}

/// Flash and sanity-gain flash: fade in, hold white, fade out.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Flash {
    pub alpha: f32,
    pub step: i32,
    pub count: f32,
    pub in_speed: f32,
    pub white_speed: f32,
    pub out_speed: f32,
    pub active: u32,
}

pub fn flash_start(s: &mut Flash, fade_in: f32, white: f32, fade_out: f32) {
    s.active = 1;
    s.step = 0;
    s.alpha = 0.0;
    let fix = |v: f32| if v == 0.0 { 0.000001f32 } else { v };
    s.in_speed = 1.0 / fix(fade_in);
    s.white_speed = 1.0 / fix(white);
    s.out_speed = 1.0 / fix(fade_out);
}

/// Sanity-gain start: the speeds come from the config times, unguarded against zero.
pub fn sanity_flash_start(s: &mut Flash, fade_in_time: f32, fade_out_time: f32) {
    s.active = 1;
    s.step = 0;
    s.alpha = 0.0;
    s.in_speed = 1.0 / fade_in_time;
    s.white_speed = 1.0 / 0.05f32;
    s.out_speed = 1.0 / fade_out_time;
}

pub fn flash_reset(s: &mut Flash) {
    s.step = 0;
    s.alpha = 0.0;
}

pub fn flash_update(s: &mut Flash, dt: f32) {
    if s.step == 0 {
        s.alpha = madd(s.in_speed, dt, s.alpha);
        if s.alpha >= 1.0 {
            s.alpha = 1.0;
            s.step = 1;
            s.count = 1.0;
        }
    } else if s.step == 1 {
        s.count = msub(s.white_speed, dt, s.count);
        if s.count <= 0.0 {
            s.step = 2;
        }
    } else if s.step == 2 {
        s.alpha = msub(s.out_speed, dt, s.alpha);
        if s.alpha <= 0.0 {
            s.active = 0;
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Emotion {
    pub alpha: f32,
    pub step: i32,
    pub count: f32,
    pub text_alpha: f32,
    pub text_time: f32,
    pub active: u32,
}

pub fn emotion_start(s: &mut Emotion, text_len: usize) {
    s.active = 1;
    s.step = 0;
    s.alpha = 0.0;
    s.text_time = madd(0.15f32, text_len as f32, 3.0f32);
    s.text_alpha = 0.0;
}

pub fn emotion_update(s: &mut Emotion, dt: f32, o: &mut Ops) {
    if s.step == 0 {
        s.alpha = madd(0.5f32, dt, s.alpha);
        if s.alpha >= 1.0 {
            s.alpha = 1.0;
            s.step = 1;
            s.count = 1.0;
        }
    } else if s.step == 1 {
        s.text_alpha = madd(dt, 3.0f32, s.text_alpha);
        if s.text_alpha > 1.0 {
            s.text_alpha = 1.0;
        }
        s.text_time -= dt;
        if s.text_time < 0.0 {
            o.k(k::EMO_FOV_OUT);
            o.k(k::EMO_UNPAUSE);
            o.k(k::EMO_RADIAL_OUT);
            s.step = 2;
        }
    } else if s.step == 2 {
        s.text_alpha -= dt;
        if s.text_alpha < 0.0 {
            s.text_alpha = 0.0;
        }
        s.alpha = msub(dt, 0.33f32, s.alpha);
        if s.alpha <= 0.0 {
            s.active = 0;
        }
    }
}

pub fn emotion_action(s: &mut Emotion, pressed: bool) {
    if !pressed {
        return;
    }
    if s.step == 1 {
        s.text_time = 0.0;
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Shake {
    pub max_size: f32,
    pub size: f32,
    pub time: f32,
    pub fade_in_time: f32,
    pub max_fade_in_time: f32,
    pub fade_out_time: f32,
    pub max_fade_out_time: f32,
}

/// Updates the shakes in place, dropping finished ones; returns the remaining count and whether the
/// effect stays active. Ops: head offset with the random shake, or the end marker when the list
/// empties mid-update (no random numbers drawn then).
pub fn shake_update(shakes: &mut [Shake], dt: f32, o: &mut Ops) -> (usize, bool) {
    let mut largest = 0.0f32;
    let n = shakes.len();
    let mut kept = 0usize;
    for i in 0..n {
        let Some(mut s) = shakes.get(i).copied() else {
            break;
        };
        if s.fade_in_time > 0.0 {
            s.fade_in_time -= dt;
            if s.fade_in_time < 0.0 {
                s.fade_in_time = 0.0;
            }
            let t = s.fade_in_time / s.max_fade_in_time;
            s.size = (1.0 - t) * s.max_size;
        } else if s.time > 0.0 {
            s.time -= dt;
            if s.time < 0.0 {
                s.time = 0.0;
            }
            s.size = s.max_size;
        } else {
            s.fade_out_time -= dt;
            if s.fade_out_time < 0.0 {
                s.fade_out_time = 0.0;
            }
            let t = s.fade_out_time / s.max_fade_out_time;
            s.size = t * s.max_size;
        }
        if largest < s.size {
            largest = s.size;
        }
        let done = s.time <= 0.0 && s.fade_out_time <= 0.0 && s.fade_in_time <= 0.0;
        if done {
            if kept == 0 && i + 1 == n {
                o.k(k::SHAKE_END);
                return (0, false);
            }
        } else {
            if let Some(slot) = shakes.get_mut(kept) {
                *slot = s;
            }
            kept += 1;
        }
        if done {
            // The erased element is gone; later ones are still iterated.
            continue;
        }
    }
    let x = rand_rectf(-largest, largest);
    let y = rand_rectf(-largest, largest);
    let z = rand_rectf(-largest, largest);
    o.kfff(k::SHAKE_HEAD, x, y, z);
    (kept, true)
}
