//! `cLuxPlayer` camera fades, head offsets, head spin, terror, focus text and average speed.

use super::hw;
use super::ops::Ops;
use crate::math::{len3, v3};
use crate::types::{Vector2f, Vector3f};

pub mod k {
    pub const SET_FOV: u32 = 20;
    pub const SET_ASPECT: u32 = 21;
    pub const SET_PITCH: u32 = 22;
    pub const SET_ROLL: u32 = 23;
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct CamState {
    pub fov: f32,
    pub aspect: f32,
    pub fov_mul: f32,
    pub fov_mul_goal: f32,
    pub fov_mul_speed: f32,
    pub aspect_mul: f32,
    pub aspect_mul_goal: f32,
    pub aspect_mul_speed: f32,
    pub roll: f32,
    pub roll_goal: f32,
    pub roll_speed_mul: f32,
    pub roll_max_speed: f32,
    pub lean_roll: f32,
    pub lean_roll_goal: f32,
    pub lean_roll_speed_mul: f32,
    pub lean_roll_max_speed: f32,
    pub pitch_goal: f32,
    pub pitch_speed_mul: f32,
    pub pitch_max_speed: f32,
    pub cam_anim_pos: Vector3f,
    pub cam_anim_pos_goal: Vector3f,
    pub cam_anim_speed_mul: f32,
    pub cam_anim_max_speed: f32,
    pub fading_pitch: u32,
    pub free_cam: u32,
    pub cam_pitch: f32,
    pub pitch_min: f32,
    pub pitch_max: f32,
}

fn cam_set_pitch(p: f32, min: f32, max: f32) -> f32 {
    let mut p = p;
    if min != 0.0 || max != 0.0 {
        if p > max {
            p = max;
        }
        if p < min {
            p = min;
        }
    }
    p
}

fn fade_clamped(speed: f32, max: f32) -> f32 {
    let mut s = speed;
    if s > max {
        s = max;
    }
    if s < -max {
        s = -max;
    }
    s
}

pub fn update_camera(s: &mut CamState, dt: f32, o: &mut Ops) {
    if s.free_cam != 0 {
        return;
    }
    if s.fov_mul != s.fov_mul_goal {
        let speed = (s.fov_mul_goal - s.fov_mul) * s.fov_mul_speed;
        s.fov_mul = speed.mul_add(dt, s.fov_mul);
        if (s.fov_mul_goal - s.fov_mul).abs() < 0.004 {
            s.fov_mul = s.fov_mul_goal;
        }
        o.kf(k::SET_FOV, s.fov * s.fov_mul);
    }
    if s.aspect_mul != s.aspect_mul_goal {
        let speed = (s.aspect_mul_goal - s.aspect_mul) * s.aspect_mul_speed;
        s.aspect_mul = dt.mul_add(speed, s.aspect_mul);
        if (s.aspect_mul_goal - s.aspect_mul).abs() < 0.004 {
            s.aspect_mul = s.aspect_mul_goal;
        }
        o.kf(k::SET_ASPECT, s.aspect * s.aspect_mul);
    }
    let mut updated_roll = false;
    if s.roll != s.roll_goal {
        let speed = fade_clamped((s.roll_goal - s.roll) * s.roll_speed_mul, s.roll_max_speed);
        s.roll = dt.mul_add(speed, s.roll);
        if (s.roll_goal - s.roll).abs() < 0.004 {
            s.roll = s.roll_goal;
        }
        updated_roll = true;
    }
    if s.fading_pitch != 0 {
        let speed = fade_clamped(
            (s.pitch_goal - s.cam_pitch) * s.pitch_speed_mul,
            s.pitch_max_speed,
        );
        let raw = dt.mul_add(speed, s.cam_pitch);
        o.kf(k::SET_PITCH, raw);
        s.cam_pitch = cam_set_pitch(raw, s.pitch_min, s.pitch_max);
        if (s.pitch_goal - s.cam_pitch).abs() < 0.004 {
            o.kf(k::SET_PITCH, s.pitch_goal);
            s.cam_pitch = cam_set_pitch(s.pitch_goal, s.pitch_min, s.pitch_max);
            s.fading_pitch = 0;
        }
    }
    if s.lean_roll != s.lean_roll_goal {
        let speed = fade_clamped(
            (s.lean_roll_goal - s.lean_roll) * s.lean_roll_speed_mul,
            s.lean_roll_max_speed,
        );
        s.lean_roll = dt.mul_add(speed, s.lean_roll);
        if (s.lean_roll_goal - s.lean_roll).abs() < 0.004 {
            s.lean_roll = s.lean_roll_goal;
        }
        updated_roll = true;
    }
    if updated_roll {
        o.kf(k::SET_ROLL, s.roll + s.lean_roll);
    }
    if s.cam_anim_pos != s.cam_anim_pos_goal {
        let mut dir = v3(
            s.cam_anim_pos_goal.x - s.cam_anim_pos.x,
            s.cam_anim_pos_goal.y - s.cam_anim_pos.y,
            s.cam_anim_pos_goal.z - s.cam_anim_pos.z,
        );
        let mut speed = len3(dir) * s.cam_anim_speed_mul;
        if speed > s.cam_anim_max_speed {
            speed = s.cam_anim_max_speed;
        }
        crate::math::normalize3(&mut dir);
        s.cam_anim_pos.x = dir.x.mul_add(speed, s.cam_anim_pos.x);
        s.cam_anim_pos.y = dir.y.mul_add(speed, s.cam_anim_pos.y);
        s.cam_anim_pos.z = dir.z.mul_add(speed, s.cam_anim_pos.z);
    }
}

/// `FadePitchTo`: the goal clamped to non-zero camera pitch limits.
pub fn clamp_pitch_goal(x: f32, min: f32, max: f32) -> f32 {
    let mut x = x;
    if min != 0.0 || max != 0.0 {
        if x > max {
            x = max;
        }
        if x < min {
            x = min;
        }
    }
    x
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct HeadPosAdd {
    pub moving: u32,
    pub goal: Vector3f,
    pub speed: f32,
    pub slowdown_dist: f32,
    pub add: Vector3f,
}

/// `UpdateHeadPosAdd`: moves every offset towards its goal and returns `(camera add, sum)`.
pub fn update_head_pos_add(
    adds: &mut [HeadPosAdd],
    camera_pos_add: Vector3f,
    dt: f32,
) -> (Vector3f, Vector3f) {
    let mut total = camera_pos_add;
    for a in adds.iter_mut() {
        if a.moving != 0 {
            let mut dir = v3(a.goal.x - a.add.x, a.goal.y - a.add.y, a.goal.z - a.add.z);
            let dist = len3(dir);
            dir = v3(dir.x / dist, dir.y / dist, dir.z / dist);
            let mut speed = a.speed;
            if dist < a.slowdown_dist {
                speed *= dist / a.slowdown_dist;
            }
            a.add.x = (dir.x * speed).mul_add(dt, a.add.x);
            a.add.y = (dir.y * speed).mul_add(dt, a.add.y);
            a.add.z = (dir.z * speed).mul_add(dt, a.add.z);
            if dist < 0.001 {
                a.moving = 0;
                a.add = a.goal;
            }
        }
        total = v3(total.x + a.add.x, total.y + a.add.y, total.z + a.add.z);
    }
    let sum = v3(
        total.x - camera_pos_add.x,
        total.y - camera_pos_add.y,
        total.z - camera_pos_add.z,
    );
    (total, sum)
}

fn norm2(v: &mut Vector2f) {
    let length = v.x.mul_add(v.x, v.y * v.y).sqrt();
    if f64::from(length) > 1e-08 {
        let inv = 1.0f32 / length;
        v.x *= inv;
        v.y *= inv;
    }
}

/// `SpinHead` from the two draws, `rx` for yaw and `ry` for pitch.
pub fn spin_head(rx: f32, ry: f32, speed: f32) -> Vector2f {
    let mut v = Vector2f { x: rx, y: ry };
    if v.x == 0.0 && v.y == 0.0 {
        v.x = 1.0;
    } else {
        norm2(&mut v);
    }
    Vector2f {
        x: v.x * speed,
        y: v.y * speed,
    }
}

/// `UpdateHeadSpin`: `None` when idle, else the new spin and the `(pitch, yaw)` camera adds.
pub fn update_head_spin(spin: Vector2f, deacc: f32, dt: f32) -> Option<(Vector2f, f32, f32)> {
    if spin.x == 0.0 && spin.y == 0.0 {
        return None;
    }
    let add_pitch = hw::mul(dt, spin.y);
    let add_yaw = hw::mul(dt, spin.x);
    let mut length = spin.x.mul_add(spin.x, spin.y * spin.y).sqrt();
    let mut dir = spin;
    norm2(&mut dir);
    length = hw::msub(dt, deacc, length);
    if length < 0.0 {
        length = 0.0;
    }
    Some((
        Vector2f {
            x: dir.x * length,
            y: dir.y * length,
        },
        add_pitch,
        add_yaw,
    ))
}

/// `UpdateTerror` value step; `enemies` is "terror enemies present and player alive".
pub fn terror_step(terror: f32, enemies: bool, inc: f32, dec: f32, dt: f32) -> f32 {
    let mut t = terror;
    if !enemies {
        t = hw::msub(dec, dt, t);
        if t < 0.0 {
            t = 0.0;
        }
    } else {
        t = inc.mul_add(dt, t);
        if t > 1.0 {
            t = 1.0;
        }
    }
    t
}

pub fn update_focus_alpha(alpha: f32, hide: bool, dt: f32) -> f32 {
    let mut a = alpha;
    if hide {
        a = hw::msub(dt, 2.0, a);
        if a < 0.0 {
            a = 0.0;
        }
    } else {
        a = dt.mul_add(2.0, a);
        if a > 1.0 {
            a = 1.0;
        }
    }
    a
}

/// Whether `UpdateAvgSpeed` drops the oldest sample (`size` already counts the new one).
pub fn prev_speeds_pop(size: usize, max: i32) -> bool {
    (size as i32) > max
}

pub fn avg_speed(speeds: &[f32]) -> f32 {
    let mut sum = 0.0f32;
    for s in speeds {
        sum += *s;
    }
    sum / speeds.len() as f32
}
