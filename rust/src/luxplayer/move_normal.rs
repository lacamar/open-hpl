//! `iLuxMoveState`, `cLuxMoveState_Normal` and `cLuxMoveState_ClimbLedge`: speeds, acceleration
//! limits, crouch/run/jump gates, head bob, ground bounce, fall damage and the ledge pitch fade.

use super::states_vec::nmul;
use crate::math::{cos, fma, increase_to, len3, sin, to_rad, wrap};
use crate::types::{Vector2f, Vector3f};
use std::hint::black_box;

const PI: f32 = std::f32::consts::PI;
const TWO_PI: f32 = std::f32::consts::TAU;
const PI4: f32 = std::f32::consts::FRAC_PI_4;

fn smooth_curve(x: f32) -> f32 {
    x * x * fma(-x, 2.0, 3.0)
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct MoveMulIn {
    pub interaction: f32,
    pub script: f32,
    pub event: f32,
    pub hurt: f32,
    pub collapse: f32,
    pub water: f32,
    pub in_water: u32,
}

/// `GetMoveSpeedMul`.
pub fn move_speed_mul(i: &MoveMulIn) -> f32 {
    let m = i.interaction * i.script * i.event * i.hurt * i.collapse;
    if i.in_water != 0 { m * i.water } else { m }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct SpeedCfg {
    pub crouch_fwd: f32,
    pub crouch_back: f32,
    pub crouch_side: f32,
    pub run_fwd: f32,
    pub run_back: f32,
    pub run_side: f32,
    pub air_fwd: f32,
    pub air_back: f32,
    pub air_side: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct SpeedMuls {
    pub fwd: f32,
    pub back: f32,
    pub side: f32,
    pub default_fwd: f32,
}

/// `UpdateSpeedMultipliers`, with `GetMoveSpeedMul` and `GetRunSpeedMul` already evaluated.
pub fn speed_multipliers(
    c: &SpeedCfg,
    move_mul: f32,
    run_mul: f32,
    on_ground: bool,
    crouching: bool,
    running: bool,
) -> SpeedMuls {
    let mut o = SpeedMuls {
        fwd: move_mul,
        back: move_mul,
        side: move_mul,
        default_fwd: 1.0,
    };
    if on_ground {
        if crouching {
            o.fwd *= c.crouch_fwd;
            o.back *= c.crouch_back;
            o.side *= c.crouch_side;
            o.default_fwd *= c.crouch_fwd;
        } else if running {
            o.fwd *= fma(c.run_fwd - 1.0, run_mul, 1.0);
            o.back *= fma(c.run_back - 1.0, run_mul, 1.0);
            o.side *= fma(c.run_side - 1.0, run_mul, 1.0);
            o.default_fwd *= c.run_fwd;
        }
    } else {
        o = SpeedMuls {
            fwd: c.air_fwd,
            back: c.air_back,
            side: c.air_side,
            default_fwd: c.air_fwd,
        };
    }
    o
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct MoveLimits {
    pub fwd_pos: f32,
    pub fwd_neg: f32,
    pub side_pos: f32,
    pub side_neg: f32,
}

/// `iLuxMoveState::Update` limits sent to the character body.
pub fn move_limits(max_fwd: f32, max_back: f32, max_side: f32, m: &SpeedMuls) -> MoveLimits {
    MoveLimits {
        fwd_pos: max_fwd * m.fwd,
        fwd_neg: nmul(max_back, m.back),
        side_pos: max_side * m.side,
        side_neg: nmul(max_side, m.side),
    }
}

/// `UpdateMovement`; returns whether to stand up. `was_moving` is the member flag.
pub fn movement_update(
    on_ground: bool,
    pressed_move: bool,
    was_moving: &mut bool,
    running: bool,
    crouch_disabled: bool,
    collapse_active: bool,
) -> bool {
    if on_ground && pressed_move && !*was_moving {
        *was_moving = true;
        return running && !crouch_disabled && !collapse_active;
    }
    *was_moving = false;
    false
}

/// `OnRun(true)`: stand up while running on the ground.
pub fn run_pressed_stands(
    crouch_disabled: bool,
    collapse_active: bool,
    on_ground: bool,
    pressed_move: bool,
) -> bool {
    !crouch_disabled && !collapse_active && on_ground && pressed_move
}

/// `OnJump`: whether a jump starts.
pub fn jump_starts(pressed: bool, jumping: bool, on_ground: bool) -> bool {
    pressed && !jumping && on_ground
}

/// `Jump` start force along y.
pub fn jump_force(
    crouching: bool,
    crouch_force: f32,
    force: f32,
    script_mul: f32,
    mass: f32,
) -> f32 {
    let f = if crouching { crouch_force } else { force };
    (f * script_mul) * mass
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct JumpState {
    pub count: f32,
    pub max: f32,
    pub jumping: u32,
}

/// First half of the held-jump update; true when gravity compensation applies this frame.
pub fn jump_tick(s: &mut JumpState, dt: f32) -> bool {
    if s.jumping == 0 {
        return false;
    }
    s.count += dt;
    if s.count >= s.max {
        s.count = s.max;
        s.jumping = 0;
    }
    true
}

/// Gravity compensation y force while jumping.
pub fn jump_gravity_force(count: f32, max: f32, gravity_y: f32, mass: f32) -> f32 {
    let mul = fma(0.5, 1.0 - count / max, 0.4);
    (-gravity_y * mass) * mul
}

/// `mbWasOnGround` update.
pub fn ground_check(on_ground: bool, force_vel_y: f32, was: &mut bool) {
    if on_ground && force_vel_y <= 0.0 {
        *was = true;
    } else if !on_ground {
        *was = false;
    }
}

/// Gravity-collide bounce start.
pub fn bounce_starts(normal_y: f32, force_vel_y: f32, min_speed: f32, active: bool) -> bool {
    normal_y > 0.001 && force_vel_y < -min_speed && !active
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct FallCfg {
    pub speed_min: f32,
    pub damage_min: f32,
    pub speed_med: f32,
    pub damage_med: f32,
    pub speed_max: f32,
    pub damage_max: f32,
}

/// `FallDamage`: `(tier, damage)`, tier 0 none, 1 min, 2 med, 3 max.
pub fn fall_damage(c: &FallCfg, y_speed: f32) -> (i32, f32) {
    let (tier, damage) = if y_speed < -c.speed_max {
        (3, c.damage_max)
    } else if y_speed < -c.speed_med {
        (2, c.damage_med)
    } else if y_speed < -c.speed_min {
        (1, c.damage_min)
    } else {
        (0, 0.0)
    };
    if damage > 0.0 {
        (tier, damage)
    } else {
        (0, damage)
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct BobCfg {
    pub walk_max: Vector2f,
    pub run_max: Vector2f,
    pub crouch_max: Vector2f,
    pub walk_min_speed: f32,
    pub walk_max_speed: f32,
    pub run_min_speed: f32,
    pub run_max_speed: f32,
    pub crouch_min_speed: f32,
    pub crouch_max_speed: f32,
    pub max_forward_speed: f32,
    pub bounce_size: f32,
    pub bounce_speed: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct BobState {
    pub head_bob: Vector3f,
    pub prev_count: f32,
    pub count: f32,
    pub bobbing: u32,
    pub ground_bounce_active: u32,
    pub ground_bounce: f32,
    pub cur_max: Vector2f,
    pub max_goal: Vector2f,
    pub bounce_size_mul: f32,
    pub bounce_speed_mul: f32,
    pub default_fwd_mul: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct BobIn {
    pub velocity: Vector3f,
    pub crouching: u32,
    pub running: u32,
    pub on_ground: u32,
    pub moved_last_update: u32,
}

fn crosses_zero(cos_now: f32, cos_prev: f32) -> bool {
    (cos_now <= 0.0 && cos_prev >= 0.0) || (cos_now >= 0.0 && cos_prev <= 0.0)
}

/// `UpdateHeadBob` minus the sound and the head-offset write; true when a footstep is due.
pub fn head_bob(s: &mut BobState, c: &BobCfg, i: &BobIn, dt: f32) -> bool {
    let mut goal = c.walk_max;
    let (mut min_speed, mut max_speed) = (c.walk_min_speed, c.walk_max_speed);
    if i.crouching != 0 {
        goal = c.crouch_max;
        min_speed = c.crouch_min_speed;
        max_speed = c.crouch_max_speed;
    } else if i.running != 0 {
        goal = c.run_max;
        min_speed = c.run_min_speed;
        max_speed = c.run_max_speed;
    }
    let player_speed = len3(i.velocity);
    let max_player_speed = c.max_forward_speed * s.default_fwd_mul;
    let bob_speed = fma(
        player_speed / max_player_speed,
        max_speed - min_speed,
        min_speed,
    );
    let moving = i.on_ground != 0 && i.moved_last_update != 0;
    if !moving {
        goal = Vector2f { x: 0.0, y: 0.0 };
    }
    s.max_goal = goal;
    let step = 0.1f32 * dt;
    s.cur_max = Vector2f {
        x: increase_to(s.cur_max.x, step, goal.x),
        y: increase_to(s.cur_max.y, step, goal.y),
    };
    let mut footstep = false;
    if !moving {
        if s.bobbing != 0 {
            let x = wrap(s.count, to_rad(-90.0), to_rad(270.0));
            let dir = black_box(if x <= to_rad(90.0) { 1.0f32 } else { -1.0f32 });
            let prev_cos = cos(s.count);
            s.prev_count = s.count;
            s.count = fma(dt * TWO_PI * dir, 3.1, s.count);
            let now_cos = cos(s.count);
            let now_sin = cos(s.count);
            if now_sin > 0.0 && crosses_zero(now_cos, prev_cos) {
                s.bobbing = 0;
                s.head_bob.x = 0.0;
                s.head_bob.y = 0.0;
            }
        }
    } else {
        s.bobbing = 1;
        s.prev_count = s.count;
        s.count = fma(bob_speed * dt, TWO_PI, s.count);
        let prev_cos = cos(s.prev_count);
        let now_cos = cos(s.count);
        let now_sin = sin(s.count);
        if now_sin < 0.0 && crosses_zero(now_cos, prev_cos) {
            footstep = true;
        }
    }
    if s.bobbing != 0 {
        s.head_bob.y = fma(sin(s.count), s.cur_max.y, -s.cur_max.y);
        s.head_bob.x = sin(fma(s.count, 0.5, -PI4)) * s.cur_max.x;
    } else {
        s.head_bob = Vector3f::default();
    }
    if s.ground_bounce_active != 0 {
        s.ground_bounce = fma(dt * c.bounce_speed, s.bounce_speed_mul, s.ground_bounce);
        if s.ground_bounce < 0.5 {
            s.head_bob.y = fma(
                sin(fma(s.ground_bounce, PI, PI)) * c.bounce_size,
                s.bounce_size_mul,
                s.head_bob.y,
            );
        } else {
            s.head_bob.y = fma(
                (smooth_curve((s.ground_bounce - 0.5) * 2.0) - 1.0) * c.bounce_size,
                s.bounce_size_mul,
                s.head_bob.y,
            );
        }
        if s.ground_bounce >= 1.0 {
            s.ground_bounce_active = 0;
            s.ground_bounce = 0.0;
        }
    }
    footstep
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct LedgeCfg {
    pub give_control_dist: f32,
    pub pitch_factor: f32,
    pub max_pitch_speed: f32,
}

/// `cLuxMoveState_ClimbLedge::OnUpdate`: `None` when control goes back to the normal move state,
/// otherwise the pitch delta to add.
pub fn ledge_update(c: &LedgeCfg, head_add: Vector3f, pitch: f32, dt: f32) -> Option<f32> {
    if len3(head_add) <= c.give_control_dist {
        return None;
    }
    let mut add = nmul(pitch, c.pitch_factor);
    if add < -c.max_pitch_speed {
        add = -c.max_pitch_speed;
    }
    if add > c.max_pitch_speed {
        add = c.max_pitch_speed;
    }
    Some(super::hw::mul(add, dt))
}
