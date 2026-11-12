//! `cLuxHandObject_LightSource` (fade and sway physics) and `cLuxHandObject_Melee` (charge and
//! swing state machine). Hand state lives in `cLuxPlayerHands`; C++ copies the fields in and out.

use crate::math::{clamp, fma, matrix_rotate, normalize3, sin, to_rad, v3};
use crate::types::{Matrixf, Vector2f, Vector3f};

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct LightAttrs {
    pub fade_in_time: f32,
    pub fade_out_time: f32,
    pub has_sway: bool,
    pub max_sway_vel: f32,
    pub sway_limits: Vector2f,
    pub sway_down_limits: Vector2f,
    pub gravity: f32,
    pub friction: f32,
    pub pin_dir: Vector3f,
    pub player_speed_mul: f32,
    pub camera_roll_mul: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct LightParams {
    pub fade_in_speed: f32,
    pub fade_out_speed: f32,
    pub has_sway: bool,
    pub max_sway_vel: f32,
    pub sway_limits: Vector2f,
    pub sway_down_limits: Vector2f,
    pub gravity: f32,
    pub friction: f32,
    pub pin_dir: Vector3f,
    pub player_speed_mul: f32,
    pub camera_roll_mul: f32,
}

fn rad2(v: Vector2f) -> Vector2f {
    Vector2f {
        x: to_rad(v.x),
        y: to_rad(v.y),
    }
}

/// `LoadImplementedVars`: a zero fade time becomes 0.001, angles go to radians.
pub fn light_load(a: &LightAttrs) -> LightParams {
    let mut fade_in = a.fade_in_time;
    let mut fade_out = a.fade_out_time;
    if fade_in == 0.0 {
        fade_in = 0.001;
    }
    if fade_out == 0.0 {
        fade_out = 0.001;
    }
    let mut pin = a.pin_dir;
    normalize3(&mut pin);
    LightParams {
        fade_in_speed: 1.0f32 / fade_in,
        fade_out_speed: 1.0f32 / fade_out,
        has_sway: a.has_sway,
        max_sway_vel: a.max_sway_vel,
        sway_limits: rad2(a.sway_limits),
        sway_down_limits: rad2(a.sway_down_limits),
        gravity: a.gravity,
        friction: a.friction,
        pin_dir: pin,
        player_speed_mul: a.player_speed_mul,
        camera_roll_mul: a.camera_roll_mul,
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Fade {
    pub alpha: f32,
    pub update: bool,
    pub done: bool,
}

/// The alpha part of `Update`: fade out while holstered, fade in until fully visible.
pub fn light_fade(p: &LightParams, holster: bool, alpha: f32, dt: f32) -> Fade {
    let mut out = Fade {
        alpha,
        update: false,
        done: false,
    };
    if holster {
        out.alpha = fma(-p.fade_out_speed, dt, out.alpha);
        if out.alpha < 0.0 {
            out.alpha = 0.0;
        }
        out.update = true;
    } else if out.alpha < 1.0 {
        out.alpha = fma(p.fade_in_speed, dt, out.alpha);
        if out.alpha > 1.0 {
            out.alpha = 1.0;
            out.done = true;
        }
        out.update = true;
    }
    out
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Sway {
    pub angle: f32,
    pub vel: f32,
}

/// `UpdateSwayPhysics` up to the sub mesh matrices; returns the sway rotation.
pub fn sway_step(
    p: &LightParams,
    s: &mut Sway,
    pitch: f32,
    roll: f32,
    player_speed: f32,
    moving_back: bool,
    dt: f32,
) -> Matrixf {
    #[allow(clippy::neg_multiply, reason = "x * -1.0 as compiled, not a negation")]
    let mut down = pitch * -1.0f32;
    down = fma(roll, p.camera_roll_mul, down);
    down = clamp(down, p.sway_down_limits.x, p.sway_down_limits.y);
    let angle = s.angle - down;
    let force = -sin(angle) * p.gravity;
    s.vel = fma(force, dt, s.vel);
    let speed = if moving_back {
        -player_speed
    } else {
        player_speed
    };
    s.vel = fma(-speed, p.player_speed_mul, s.vel);
    if s.vel > p.max_sway_vel {
        s.vel = p.max_sway_vel;
    }
    if s.vel < -p.max_sway_vel {
        s.vel = -p.max_sway_vel;
    }
    s.vel = fma(-(s.vel * p.friction), dt, s.vel);
    s.angle = fma(s.vel, dt, s.angle);
    if s.angle < p.sway_limits.x {
        s.vel = 0.0;
        s.angle = p.sway_limits.x;
    }
    if s.angle > p.sway_limits.y {
        s.vel = 0.0;
        s.angle = p.sway_limits.y;
    }
    matrix_rotate(
        v3(
            p.pin_dir.x * s.angle,
            p.pin_dir.y * s.angle,
            p.pin_dir.z * s.angle,
        ),
        0,
    )
}

pub const HANDS_IDLE: i32 = 3;
pub const HANDS_HAND_OBJECT: i32 = 4;

/// The fields of `cLuxPlayerHands` the melee weapon drives.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Hands {
    pub obj_state: i32,
    pub attack_down: bool,
    pub charge_count: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct MeleeAct {
    /// `DoAction` returns true.
    pub ret: bool,
    /// Run `Charge()`.
    pub charge: bool,
    /// Run `Swing()`.
    pub swing: bool,
    /// `SetInteractionMoveSpeedMul(charge speed mul)`.
    pub charge_speed: bool,
}

/// `DoAction` for the attack action.
pub fn melee_do_action(hands_state: i32, pressed: bool, h: &mut Hands) -> MeleeAct {
    let mut out = MeleeAct::default();
    if hands_state == HANDS_IDLE {
        if pressed {
            out.charge = true;
            out.ret = true;
        }
    } else if hands_state == HANDS_HAND_OBJECT && !pressed && h.obj_state != 2 {
        h.attack_down = false;
        if h.obj_state != 0 {
            out.swing = true;
        }
    }
    out
}

/// `AnimationIsOver`.
pub fn melee_anim_over(h: &mut Hands) -> MeleeAct {
    let mut out = MeleeAct::default();
    if h.obj_state == 0 {
        if !h.attack_down {
            out.swing = true;
        } else {
            out.charge_speed = true;
            h.obj_state = 1;
        }
    } else if h.obj_state == 2 {
        h.obj_state = 0;
        out.ret = true;
    }
    out
}

/// `UpdateCharge`: true on the update that completes the charge.
pub fn melee_update_charge(h: &mut Hands, charge_time: f32, dt: f32) -> bool {
    if !h.attack_down || h.charge_count >= 1.0 {
        return false;
    }
    h.charge_count = fma(dt, 1.0f32 / charge_time, h.charge_count);
    if h.charge_count >= 1.0 {
        h.charge_count = 1.0;
        return true;
    }
    false
}

/// `Swing()` state change; true when the charged swing is used.
pub fn melee_swing(h: &mut Hands) -> bool {
    h.obj_state = 2;
    h.charge_count >= 1.0
}

/// `Charge()` state change.
pub fn melee_charge(h: &mut Hands) {
    h.attack_down = true;
    h.charge_count = 0.0;
    h.obj_state = 0;
}

/// Damage multiplier of a charged hit.
pub fn melee_damage_mul(h: &Hands) -> f32 {
    if h.charge_count >= 1.0 { 2.0 } else { 1.0 }
}
