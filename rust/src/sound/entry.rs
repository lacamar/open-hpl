//! `cSoundEntry`: one playing sound with volume/speed fades, 3D attenuation and block fading.

use super::channel::{Channel, EntryHost};
use crate::math::{fma, matrix_mul_vec};
use crate::types::Vector3f;

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EntryState {
    pub normal_volume: f32,
    pub volume_mul: f32,
    pub volume_fade_dest: f32,
    pub volume_fade_speed: f32,
    pub normal_speed: f32,
    pub speed_mul: f32,
    pub speed_fade_dest: f32,
    pub speed_fade_speed: f32,
    pub block_mul: f32,
    pub block_fade_dest: f32,
    pub block_fade_speed: f32,
    pub entry_type: u32,
    pub stop_after_fade_out: bool,
    pub first_time: bool,
    pub stop_disabled: bool,
    pub is_3d: bool,
    pub stream: bool,
}

#[derive(Debug, PartialEq, Eq)]
pub(super) enum Step {
    Alive,
    /// The channel ended; `release` asks for the priority-release callback before the final stop.
    Finished {
        release: bool,
    },
}

pub(super) fn correct_sign_of_speed(current: f32, dest: f32, speed: f32) -> f32 {
    if dest < current && speed > 0.0 {
        return -speed;
    }
    if dest > current && speed < 0.0 {
        return -speed;
    }
    speed
}

/// `cMath::Vector3DistSqr(start, end)` as gcc contracts it.
fn dist_sqr(start: Vector3f, end: Vector3f) -> f32 {
    let dx = end.x - start.x;
    let dy = end.y - start.y;
    let dz = end.z - start.z;
    fma(dz, dz, fma(dx, dx, dy * dy))
}

impl EntryState {
    pub fn new(volume: f32, entry_type: u32, is_3d: bool, stream: bool) -> Self {
        Self {
            normal_volume: volume,
            volume_mul: 1.0,
            volume_fade_dest: 1.0,
            volume_fade_speed: 0.0,
            normal_speed: 1.0,
            speed_mul: 1.0,
            speed_fade_dest: 1.0,
            speed_fade_speed: 0.0,
            block_mul: 1.0,
            block_fade_dest: 1.0,
            block_fade_speed: 0.0,
            entry_type,
            stop_after_fade_out: false,
            first_time: true,
            stop_disabled: false,
            is_3d,
            stream,
        }
    }

    pub(super) fn update<C: Channel, H: EntryHost>(
        &mut self,
        dt: f32,
        ch: &mut C,
        host: &mut H,
    ) -> Step {
        self.update_volume_fade(dt, ch);
        self.update_speed_fade(dt);

        let final_speed = self.normal_speed * self.speed_mul * host.global_speed(self.entry_type);
        if ch.speed() != final_speed {
            ch.set_speed(final_speed);
        }

        if self.block_mul != self.block_fade_dest {
            self.block_mul = fma(self.block_fade_speed, dt, self.block_mul);
            if self.block_fade_speed < 0.0 {
                if self.block_mul < self.block_fade_dest {
                    self.block_mul = self.block_fade_dest;
                }
            } else if self.block_mul > self.block_fade_dest {
                self.block_mul = self.block_fade_dest;
            }
        }

        if ch.position_is_relative() {
            let pos = matrix_mul_vec(&host.listener_matrix(), ch.rel_position());
            ch.set_position(pos);
        }

        if self.is_3d {
            self.update_3d(ch, host);
        } else {
            let volume = self.normal_volume * self.volume_mul * host.global_volume(self.entry_type);
            if ch.volume() != volume {
                ch.set_volume(volume);
            }
        }

        if self.first_time && !host.silent() {
            ch.play();
            self.first_time = false;
        }

        if !ch.is_playing() && !ch.paused() {
            let release = !ch.stop_used() && ch.looping() && self.volume_fade_dest != 0.0;
            return Step::Finished { release };
        }
        Step::Alive
    }

    fn update_volume_fade<C: Channel>(&mut self, dt: f32, ch: &mut C) {
        if self.volume_mul != self.volume_fade_dest {
            self.volume_mul = fma(self.volume_fade_speed, dt, self.volume_mul);
            if (self.volume_fade_speed < 0.0 && self.volume_mul <= self.volume_fade_dest)
                || (self.volume_fade_speed >= 0.0 && self.volume_mul >= self.volume_fade_dest)
            {
                self.volume_mul = self.volume_fade_dest;
            }
        }
        if self.stop_after_fade_out && self.volume_mul.abs() < 0.001 {
            ch.stop();
            self.stop_after_fade_out = false;
        }
    }

    fn update_speed_fade(&mut self, dt: f32) {
        if self.speed_mul != self.speed_fade_dest {
            self.speed_mul = fma(self.speed_fade_speed, dt, self.speed_mul);
            if (self.speed_fade_speed < 0.0 && self.speed_mul <= self.speed_fade_dest)
                || (self.speed_fade_speed >= 0.0 && self.speed_mul >= self.speed_fade_dest)
            {
                self.speed_mul = self.speed_fade_dest;
            }
        }
    }

    fn update_3d<C: Channel, H: EntryHost>(&mut self, ch: &mut C, host: &mut H) {
        let listener = host.listener_position();
        let sqr = dist_sqr(ch.position(), listener);
        if sqr >= ch.max_distance() * ch.max_distance() {
            ch.set_volume(0.0);
            ch.set_priority(0);
            return;
        }

        if host.is_blocked(ch.position()) {
            self.block_fade_dest = 0.0;
            self.block_fade_speed = -1.0 / 0.55;
            if self.first_time {
                self.block_mul = 0.0;
            }
        } else {
            self.block_fade_dest = 1.0;
            self.block_fade_speed = 1.0 / 0.2;
            if self.first_time {
                self.block_mul = 1.0;
            }
        }

        let dist_volume_mul = if sqr < ch.min_distance() * ch.min_distance() {
            ch.set_priority(100);
            1.0
        } else {
            ch.set_priority(10);
            let dist = dist_sqr(ch.position(), listener).sqrt();
            let delta = dist - ch.min_distance();
            let max_delta = ch.max_distance() - ch.min_distance();
            1.0 - (delta / max_delta)
        };

        let block_volume = ch.block_volume_mul();
        let block = fma(self.block_mul, 1.0 - block_volume, block_volume);
        ch.set_volume(
            self.normal_volume
                * self.volume_mul
                * block
                * dist_volume_mul
                * host.global_volume(self.entry_type),
        );
    }

    pub(super) fn stop<C: Channel>(&mut self, ch: &mut C) {
        if self.stop_disabled {
            return;
        }
        if self.first_time {
            ch.play();
            self.first_time = false;
        }
        ch.set_paused(false);
        ch.stop();
    }

    pub fn fade_volume_mul_to(&mut self, dest: f32, speed: f32) {
        self.volume_fade_dest = dest;
        self.volume_fade_speed = correct_sign_of_speed(self.volume_mul, dest, speed);
    }

    pub fn fade_speed_mul_to(&mut self, dest: f32, speed: f32) {
        self.speed_fade_dest = dest;
        self.speed_fade_speed = correct_sign_of_speed(self.speed_mul, dest, speed);
    }

    pub fn fade_out(&mut self, speed: f32) {
        self.volume_fade_dest = 0.0;
        self.volume_fade_speed = correct_sign_of_speed(self.volume_mul, 0.0, speed);
        self.stop_after_fade_out = true;
    }

    pub fn fade_in(&mut self, volume_mul: f32, speed: f32) {
        self.volume_fade_dest = volume_mul;
        self.volume_mul = 0.0;
        self.volume_fade_speed = correct_sign_of_speed(0.0, volume_mul, speed);
    }
}
