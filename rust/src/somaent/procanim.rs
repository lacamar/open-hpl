//! Procedural sub-mesh animations (`cProcAnim`): the playback state and the eased track offsets.

use crate::math::{PI, clamp, cos, fma, matrix_rotate, matrix_translate, sin, to_rad, v3};
use crate::types::{Matrixf, Vector3f};

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct ProcAnim {
    pub active: bool,
    pub looping: bool,
    pub time: f32,
    pub speed: f32,
    pub speed_target: f32,
    pub speed_step: f32,
    pub amount: f32,
    pub amount_step: f32,
}

impl Default for ProcAnim {
    fn default() -> Self {
        Self {
            active: false,
            looping: false,
            time: 0.0,
            speed: 0.0,
            speed_target: 0.0,
            speed_step: 0.0,
            amount: 1.0,
            amount_step: 0.0,
        }
    }
}

impl ProcAnim {
    /// `PlayProcAnimation` for one matching animation.
    pub fn play(&mut self, length: f32, looping: bool, amount_fade: f32, speed_fade: f32) {
        let speed = if length != 0.0 { 1.0f32 / length } else { 0.0 };
        if !self.active {
            self.time = if speed < 0.0 { 1.0 } else { 0.0 };
            self.amount = 0.0;
            self.speed = 0.0;
        }
        self.active = true;
        self.looping = looping;
        self.amount_step = if amount_fade > 0.0 {
            1.0f32 / amount_fade
        } else {
            0.0
        };
        if amount_fade <= 0.0 {
            self.amount = 1.0;
        }
        self.speed_target = speed;
        self.speed_step = if speed_fade > 0.0 {
            (speed - self.speed).abs() / speed_fade
        } else {
            0.0
        };
        if speed_fade <= 0.0 {
            self.speed = speed;
        }
    }

    /// One frame of an active animation; true when another frame is needed.
    pub fn step(&mut self, dt: f32) -> bool {
        let amount = fma(self.amount_step, dt, self.amount);
        self.amount = if 1.0f32 < amount { 1.0 } else { amount };
        let max_step = self.speed_step * dt;
        self.speed += clamp(self.speed_target - self.speed, -max_step, max_step);
        self.time = fma(self.speed, dt, self.time);
        self.time = if self.looping {
            self.time - self.time.floor()
        } else {
            clamp(self.time, 0.0, 1.0)
        };
        self.looping
            || self.amount < 1.0
            || self.speed != self.speed_target
            || (self.time > 0.0 && self.time < 1.0)
    }
}

pub struct Track<'a> {
    pub rotate: bool,
    pub reverse: bool,
    pub axes: Vector3f,
    pub min: f32,
    pub max: f32,
    pub cycles: i32,
    pub easing: &'a [u8],
}

/// The offset matrix a track adds to its sub-mesh at the animation's current time and amount.
pub fn track_offset(anim: &ProcAnim, t: &Track) -> Matrixf {
    let mut f = anim.time * t.cycles as f32;
    f = if !anim.looping && anim.time >= 1.0 {
        1.0
    } else {
        f - f.floor()
    };
    if t.reverse {
        f = if f < 0.5 { f * 2.0 } else { 2.0 - f * 2.0 };
    }
    match t.easing {
        b"sinein" => f = 1.0 - cos(f * PI / 2.0),
        b"sineout" => f = sin(f * PI / 2.0),
        b"sineinout" => f = fma(-0.5f32, cos(f * PI), 0.5f32),
        _ => {}
    }
    let value = fma(t.max - t.min, f, t.min) * anim.amount;
    if t.rotate {
        let r = to_rad(value);
        matrix_rotate(v3(t.axes.x * r, t.axes.y * r, t.axes.z * r), 0)
    } else {
        matrix_translate(v3(t.axes.x * value, t.axes.y * value, t.axes.z * value))
    }
}
