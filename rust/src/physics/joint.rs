//! `iPhysicsJoint` move sounds, limit effects, breakage and limit auto-sleep.

use super::book;
use super::host::{H, Host, Str};
use crate::math::{fma, len3, matrix_mul_3x3, matrix_mul_vec, sub3, v3};
use crate::types::{Matrixf, Vector3f};

const ZERO: Vector3f = v3(0.0, 0.0, 0.0);
const SPEED_LINEAR: i32 = 0;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct MoveParams {
    pub min_speed: f32,
    pub min_freq: f32,
    pub min_freq_speed: f32,
    pub min_volume: f32,
    pub max_freq: f32,
    pub max_freq_speed: f32,
    pub max_volume: f32,
    pub middle_speed: f32,
    pub middle_volume: f32,
    pub speed_type: i32,
    pub move_sound: Str,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct JointState {
    pub parent: H,
    pub child: H,
    pub world: H,
    pub pivot: Vector3f,
    pub pin_dir: Vector3f,
    pub local_pivot: Vector3f,
    pub local_pin_dir: Vector3f,
    pub prev_child: Matrixf,
    pub prev_parent: Matrixf,
    pub sound: H,
    pub sound_id: i32,
    pub speed_count: i32,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct LimitEffect {
    pub sound: Str,
    pub min_speed: f32,
    pub max_speed: f32,
}

pub fn calc_sound_freq(p: &MoveParams, speed: f32) -> (f32, f32) {
    let abs = speed.abs();
    if abs >= p.middle_speed {
        if abs >= p.max_freq_speed {
            (p.max_freq, p.max_volume)
        } else {
            let t = (abs - p.middle_speed) / (p.max_freq_speed - p.middle_speed);
            (
                fma(t, p.max_freq, 1.0 - t),
                fma(p.middle_volume, 1.0 - t, t * p.max_volume),
            )
        }
    } else if abs <= p.min_freq_speed {
        (p.min_freq, p.min_volume)
    } else {
        let t = (p.middle_speed - abs) / (p.middle_speed - p.min_freq_speed);
        (
            fma(t, p.min_freq, 1.0 - t),
            fma(p.middle_volume, 1.0 - t, t * p.min_volume),
        )
    }
}

fn max(a: f32, b: f32) -> f32 {
    if a > b { a } else { b }
}

/// `OnPhysicsUpdate` after the pending position reset; false when the joint is frozen.
pub fn on_physics_update<Ho: Host>(h: &Ho, p: &MoveParams, s: &mut JointState) -> bool {
    on_physics_update_bk(h, p, s, 0)
}

/// With a non-zero `book` the previous matrices and speed count live in the Rust-owned book.
pub fn on_physics_update_bk<Ho: Host>(
    h: &Ho,
    p: &MoveParams,
    s: &mut JointState,
    book: u64,
) -> bool {
    let Some(k) = book::joint_get(book) else {
        return step(h, p, s);
    };
    (s.prev_child, s.prev_parent, s.speed_count) = (k.prev_child, k.prev_parent, k.speed_count);
    let updated = step(h, p, s);
    book::joint_with(book, |k| {
        (k.prev_child, k.prev_parent, k.speed_count) = (s.prev_child, s.prev_parent, s.speed_count);
    });
    updated
}

fn step<Ho: Host>(h: &Ho, p: &MoveParams, s: &mut JointState) -> bool {
    let frozen = !((s.parent != 0 && h.body_enabled(s.parent)) || h.body_enabled(s.child));
    if frozen && s.sound == 0 {
        return false;
    }

    if s.parent != 0 {
        let m = h.body_local_matrix(s.parent);
        s.pivot = matrix_mul_vec(&m, s.local_pivot);
        s.pin_dir = matrix_mul_3x3(&h.body_local_matrix(s.parent), s.local_pin_dir);
    }

    let world = h.pworld_world(s.world);
    if world == 0 {
        return true;
    }
    if p.move_sound.n == 0 {
        return true;
    }
    if h.world_silent(h.pworld_world(s.world)) {
        return true;
    }

    let mut vel = if p.speed_type == SPEED_LINEAR {
        if s.parent != 0 {
            sub3(
                h.body_linear_velocity(s.child),
                h.body_linear_velocity(s.parent),
            )
        } else {
            h.body_linear_velocity(s.child)
        }
    } else if s.parent != 0 {
        sub3(
            h.body_angular_velocity(s.child),
            h.body_angular_velocity(s.parent),
        )
    } else {
        h.body_angular_velocity(s.child)
    };

    if s.parent != 0 {
        if s.prev_child == h.body_local_matrix(s.child)
            && s.prev_parent == h.body_local_matrix(s.parent)
        {
            vel = ZERO;
        }
        s.prev_child = h.body_local_matrix(s.child);
        s.prev_parent = h.body_local_matrix(s.parent);
    } else {
        if s.prev_child == h.body_local_matrix(s.child) {
            vel = ZERO;
        }
        s.prev_child = h.body_local_matrix(s.child);
    }

    let speed = len3(vel);

    if !h.world_sound_exists(world, s.sound, s.sound_id) {
        s.sound = 0;
    }

    if s.sound != 0 {
        let fmin = max(p.min_speed - 0.2, 0.1);
        if speed <= fmin {
            h.ent_fade_out(s.sound, 4.3);
            s.sound = 0;
        } else {
            let entry = h.ent_main_entry(s.sound);
            if entry != 0 {
                let (freq, volume) = calc_sound_freq(p, speed);
                h.entry_set_speed_mul(entry, freq);
                h.entry_set_volume_mul(entry, volume);
            }
            h.ent_set_position(s.sound, s.pivot);
        }
    } else if speed > p.min_speed {
        if s.speed_count >= 3 {
            s.speed_count = 0;
            s.sound = h.world_create_sound(world, Str::new(b"MoveSound"), p.move_sound);
            if s.sound != 0 {
                s.sound_id = h.ent_creation_id(s.sound);
                h.ent_set_saved_false(s.sound);
                h.ent_fade_in(s.sound, 3.3);
                h.ent_set_position(s.sound, s.pivot);
            }
        } else {
            s.speed_count += 1;
        }
    } else {
        s.speed_count = 0;
    }
    true
}

fn limit_effect<Ho: Host>(h: &Ho, joint: H, e: &LimitEffect) {
    let world = h.joint_world_world(joint);
    if world != 0 && e.sound.n != 0 {
        let parent = h.joint_parent(joint);
        let child = h.joint_child(joint);
        let vel = if parent != 0 {
            sub3(
                h.body_linear_velocity(child),
                h.body_linear_velocity(parent),
            )
        } else {
            h.body_linear_velocity(child)
        };
        let mut speed = len3(vel);
        if speed > e.max_speed {
            speed = e.max_speed;
        }
        if speed >= e.min_speed && !h.joint_has_collided(joint) {
            let volume = (speed - e.min_speed) / (e.max_speed - e.min_speed);
            let sound = h.world_create_sound(world, Str::new(b"LimitSound"), e.sound);
            if sound != 0 {
                h.ent_set_saved_false(sound);
                h.ent_set_volume(sound, volume);
                h.ent_set_position(sound, h.body_local_position(child));
            }
        }
    }
    h.joint_set_has_collided(joint, true);
}

pub const END_ON_MIN: i32 = 2;
pub const END_ON_MAX: i32 = 3;

pub fn on_limit<Ho: Host>(h: &Ho, joint: H, is_max: bool, e: &LimitEffect) {
    if !h.joint_has_collided(joint) {
        if is_max {
            h.joint_callback_max(joint);
        } else {
            h.joint_callback_min(joint);
        }
    }
    if !h.joint_has_collided(joint) {
        let end = if is_max { END_ON_MAX } else { END_ON_MIN };
        for i in 0..h.joint_controller_count(joint) {
            let c = h.joint_controller_at(joint, i);
            if h.ctrl_is_active(c) && h.ctrl_end_type(c) == end {
                h.ctrl_set_active(c, false);
                let next = h.joint_get_controller(joint, h.ctrl_next_name(c));
                if next != 0 {
                    h.ctrl_set_active(next, true);
                } else {
                    h.joint_warn_missing_controller(joint, c);
                }
            }
        }
    }
    limit_effect(h, joint, e);
}

/// `fForcesSize >= mfBreakForce || mbBroken`; the break sound plays when it holds.
pub fn check_breakage<Ho: Host>(
    h: &Ho,
    joint: H,
    breakable: bool,
    broken: bool,
    break_force: f32,
    sound: Str,
) -> bool {
    if !breakable {
        return false;
    }
    if h.joint_force_size(joint) >= break_force || broken {
        if sound.n != 0 {
            let world = h.joint_world_world(joint);
            if world != 0 {
                let e = h.world_create_sound(world, Str::new(b"BreakSound"), sound);
                if e != 0 {
                    h.ent_set_saved_false(e);
                    h.ent_set_position(e, h.joint_pivot(joint));
                }
            }
        }
        return true;
    }
    false
}

/// True when the child body should be disabled after the simulation step.
pub fn limit_auto_sleep(
    enabled: bool,
    sleep_dist: f32,
    num_steps: i32,
    step_count: &mut i32,
    min: f32,
    max: f32,
    dist: f32,
) -> bool {
    if !enabled {
        return false;
    }
    let min_diff = (min - dist).abs();
    let max_diff = (max - dist).abs();
    if max_diff < sleep_dist || min_diff < sleep_dist {
        if *step_count >= num_steps {
            return true;
        }
        *step_count += 1;
    } else {
        *step_count = 0;
    }
    false
}
