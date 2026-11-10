//! `cAnimationState` scalar state machine: speed fade, time position, weight fade.

use crate::math::{clamp, fma, increase_to, wrap};

/// The scalar members of `cAnimationState`, shared with C++ by layout.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct AnimCore {
    pub length: f32,
    pub weight: f32,
    pub speed: f32,
    pub speed_dest: f32,
    pub speed_fade_rate: f32,
    pub time_pos: f32,
    pub prev_time_pos: f32,
    pub base_speed: f32,
    pub special_event_time: f32,
    pub fade_step: f32,
    pub active: bool,
    pub looping: bool,
    pub paused: bool,
    pub layer: bool,
}

impl AnimCore {
    pub fn new(length: f32) -> Self {
        Self {
            length,
            weight: 1.0,
            speed: 1.0,
            speed_dest: 1.0,
            speed_fade_rate: 0.0,
            time_pos: 0.0,
            prev_time_pos: 0.0,
            base_speed: 1.0,
            special_event_time: 0.0,
            fade_step: 0.0,
            active: false,
            looping: false,
            paused: false,
            layer: false,
        }
    }

    pub fn update(&mut self, dt: f32) {
        if self.speed != self.speed_dest {
            self.speed = increase_to(self.speed, self.speed_fade_rate * dt, self.speed_dest);
        }
        self.add_time_position(dt);
        if self.fade_step != 0.0 {
            self.weight = fma(self.fade_step, dt, self.weight);
            if self.fade_step < 0.0 && self.weight <= 0.0 {
                self.weight = 0.0;
                self.active = false;
                self.fade_step = 0.0;
            } else if self.fade_step > 0.0 && self.weight >= 1.0 {
                self.weight = 1.0;
                self.fade_step = 0.0;
            }
        }
    }

    pub fn is_over(&self) -> bool {
        !self.looping && self.time_pos >= self.length
    }

    pub fn fade_in(&mut self, time: f32) {
        self.fade_step = 1.0 / time.abs();
    }

    pub fn fade_out(&mut self, time: f32) {
        self.fade_step = -1.0 / time.abs();
    }

    pub fn fade_speed(&mut self, dest: f32, time: f32) {
        self.speed_dest = dest;
        let t = if time > 0.0001 { time } else { 0.0001 };
        self.speed_fade_rate = (dest - self.speed).abs() / t;
    }

    pub fn set_time_position(&mut self, pos: f32) {
        self.time_pos = if self.looping {
            wrap(pos, 0.0, self.length)
        } else {
            clamp(pos, 0.0, self.length)
        };
    }

    pub fn set_relative_time_position(&mut self, pos: f32) {
        self.set_time_position(pos * self.length);
    }

    pub fn relative_time_position(&self) -> f32 {
        self.time_pos / self.length
    }

    pub fn set_active(&mut self, active: bool) {
        if self.active == active {
            return;
        }
        self.active = active;
        self.paused = false;
        self.fade_step = 0.0;
    }

    pub fn add_time_position(&mut self, add: f32) {
        if self.paused {
            return;
        }
        self.prev_time_pos = self.time_pos;
        self.time_pos = fma(add * self.speed, self.base_speed, self.time_pos);
        self.set_time_position(self.time_pos);
    }
}

/// `cMeshEntity::GetAnimationWeightMul` over the states in order.
pub fn weight_mul<'a>(states: impl Iterator<Item = &'a AnimCore>, normalize: bool) -> f32 {
    if !normalize {
        return 1.0;
    }
    let mut num = 0.0f32;
    let mut total = 0.0f32;
    for s in states {
        if s.active && !s.layer {
            num += 1.0;
            total += s.weight;
        }
    }
    if num > 0.0 && total > 0.0 {
        1.0 / total
    } else {
        1.0
    }
}
