//! `cLuxProp_Lever`: min/max/middle states, stuck states and the spring back to a goal angle.

use super::object::{MoveData, load_rotate};
use super::ops::{Op, Ops, Probe, op};
use super::vars::{Vars, put_var};
use super::wheel::to_stuck_state;
use crate::math::{dot3, fma, to_rad, v3};
use crate::types::Vector3f;

pub const SLOT_MIN_LIMIT_SOUND: i32 = 60;
pub const SLOT_MAX_LIMIT_SOUND: i32 = 61;
pub const SLOT_STUCK_SOUND: i32 = 62;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct LeverParams {
    pub data: MoveData,
    pub can_interact_static: bool,
    pub min_limit_range: f32,
    pub max_limit_range: f32,
    pub min_limit_stuck: bool,
    pub max_limit_stuck: bool,
    pub middle_angle_amount: f32,
    pub auto_move_to_angle: bool,
    pub auto_move_goal: i32,
    pub auto_move_speed_factor: f32,
    pub auto_move_max_speed: f32,
}

fn to_auto_move_goal(s: &[u8]) -> i32 {
    match s {
        b"Middle" => 0,
        b"Max" => 1,
        b"Min" => -1,
        _ => 0,
    }
}

pub fn load(v: &dyn Vars, out: &mut LeverParams) {
    load_rotate(v, &mut out.data);
    out.can_interact_static = v.boolean("CanInteractWithStaticBody", true);
    out.min_limit_range = to_rad(v.float("MinLimitRange", 5.0));
    out.max_limit_range = to_rad(v.float("MaxLimitRange", 5.0));
    out.min_limit_stuck = v.boolean("MinLimitStuck", false);
    out.max_limit_stuck = v.boolean("MaxLimitStuck", false);
    out.middle_angle_amount = v.float("MiddleAngleAmount", 0.0);
    out.auto_move_to_angle = v.boolean("AutoMoveToAngle", true);
    out.auto_move_goal = to_auto_move_goal(&v.string("AutoMoveGoal", b"Middle"));
    out.auto_move_speed_factor = v.float("AutoMoveSpeedFactor", 2.0);
    out.auto_move_max_speed = v.float("AutoMoveMaxSpeed", 8.0);
    put_var(v, SLOT_MIN_LIMIT_SOUND, "MinLimitSound");
    put_var(v, SLOT_MAX_LIMIT_SOUND, "MaxLimitSound");
    put_var(v, SLOT_STUCK_SOUND, "StuckSound");
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct LeverState {
    pub current_state: i32,
    pub stuck_state: i32,
    pub interaction_disables_stuck: bool,
    pub stuck_sound_timer: f32,
    pub default_min_angle: f32,
    pub default_max_angle: f32,
    pub middle_angle: f32,
}

pub fn can_interact(p: &LeverParams, mass: f32) -> bool {
    !(mass == 0.0 && !p.can_interact_static)
}

pub fn calculate_middle_angle(s: &mut LeverState, p: &LeverParams) {
    let t = (p.middle_angle_amount + 1.0f32) / 2.0f32;
    s.middle_angle = fma(
        s.default_max_angle - s.default_min_angle,
        t,
        s.default_min_angle,
    );
}

/// `OnSetupAfterLoad` once the hinge exists: its limits become the lever's range.
pub fn setup(s: &mut LeverState, p: &LeverParams, min: f32, max: f32, ops: &mut Ops) {
    ops.push(Op::new(op::HINGE_STICKY).i(0).j(1));
    s.default_min_angle = min;
    s.default_max_angle = max;
    calculate_middle_angle(s, p);
}

fn change_state(s: &mut LeverState, state: i32, effects: bool, ops: &mut Ops) {
    if s.current_state == state {
        return;
    }
    s.current_state = state;
    ops.event(Op::new(op::CONN_STATE).i(state), s);
    if state == 1 || state == -1 {
        ops.push(Op::new(op::LIMIT_SOUND).i(state).j(i32::from(effects)));
    }
    ops.event(Op::new(op::CONN_LIMIT).i(state), s);
}

pub fn set_stuck_state(
    s: &mut LeverState,
    p: &LeverParams,
    state: i32,
    effects: bool,
    ops: &mut Ops,
) {
    if s.stuck_state == state {
        return;
    }
    s.stuck_state = state;
    s.stuck_sound_timer = 0.0;
    let hinge = |ops: &mut Ops, min: f32, max: f32| {
        ops.push(Op::new(op::HINGE_MIN).i(0).f(min));
        ops.push(Op::new(op::HINGE_MAX).i(0).f(max));
    };
    if state == 1 {
        change_state(s, 1, effects, ops);
        hinge(
            ops,
            s.default_max_angle - p.max_limit_range,
            s.default_max_angle,
        );
    } else if state == -1 {
        change_state(s, -1, effects, ops);
        hinge(
            ops,
            s.default_min_angle,
            s.default_min_angle + p.min_limit_range,
        );
    } else {
        hinge(ops, s.default_min_angle, s.default_max_angle);
    }
}

pub fn on_connection_state(s: &mut LeverState, p: &LeverParams, state: i32, ops: &mut Ops) {
    set_stuck_state(s, p, if state > 0 { -1 } else { 0 }, true, ops);
}

/// `OnInteract` before the player state change; returns whether to show the hint.
pub fn on_interact(s: &mut LeverState, p: &LeverParams, show_hints: bool, ops: &mut Ops) -> bool {
    if s.stuck_state != 0 && s.interaction_disables_stuck {
        set_stuck_state(s, p, 0, true, ops);
    }
    s.stuck_state == 0 && show_hints
}

fn update_check_limit(s: &mut LeverState, p: &LeverParams, angle: f32, ops: &mut Ops) {
    if s.stuck_state != 0 {
        return;
    }
    let (dmax, dmin) = (s.default_max_angle, s.default_min_angle);
    if angle > dmax - p.max_limit_range {
        change_state(s, 1, true, ops);
        if p.max_limit_stuck {
            set_stuck_state(s, p, 1, false, ops);
        }
    } else if angle < dmin + p.min_limit_range {
        change_state(s, -1, true, ops);
        if p.min_limit_stuck {
            set_stuck_state(s, p, -1, false, ops);
        }
    } else if angle < (dmax - p.max_limit_range) - to_rad(5.0)
        || angle > (dmin + p.min_limit_range) + to_rad(5.0)
    {
        change_state(s, 0, true, ops);
    }
}

fn update_auto_move(
    s: &LeverState,
    p: &LeverParams,
    angle: f32,
    dt: f32,
    interacted: bool,
    probe: &mut dyn Probe,
    ops: &mut Ops,
) {
    if interacted || !p.auto_move_to_angle || s.stuck_state != 0 {
        return;
    }
    let goal = match p.auto_move_goal {
        0 => s.middle_angle,
        -1 => s.default_min_angle + p.min_limit_range / 2.0f32,
        1 => s.default_max_angle - p.max_limit_range / 2.0f32,
        _ => 0.0,
    };
    let mut wanted = p.auto_move_speed_factor * (angle - goal);
    if wanted > p.auto_move_max_speed {
        wanted = p.auto_move_max_speed;
    }
    if wanted < -p.auto_move_max_speed {
        wanted = -p.auto_move_max_speed;
    }
    let h = probe.hinge(0);
    let dir = h.pin_dir;
    let k = dot3(dir, h.child_ang_vel);
    let wanted_vel = v3(dir.x * wanted, dir.y * wanted, dir.z * wanted);
    ops.push(Op::new(op::BODY_ANG_VEL).v(wanted_vel));
    let err: Vector3f = v3(
        fma(-dir.x, k, wanted_vel.x),
        fma(-dir.y, k, wanted_vel.y),
        fma(-dir.z, k, wanted_vel.z),
    );
    ops.push(Op::new(op::PID_TORQUE).v(err).f(dt));
}

pub fn update(
    s: &mut LeverState,
    p: &LeverParams,
    dt: f32,
    interacted: bool,
    probe: &mut dyn Probe,
    ops: &mut Ops,
) {
    let h = probe.hinge(0);
    let angle = h.angle;
    if s.stuck_state != 0 && interacted {
        if s.stuck_sound_timer > 0.0 {
            s.stuck_sound_timer -= dt;
        } else if super::base::sqr_len(h.child_ang_vel) > 0.01f32 {
            ops.push(Op::new(op::PLAY_SOUND).i(op::SND_LEVER_STUCK));
            s.stuck_sound_timer = 1.5;
        }
    }
    update_check_limit(s, p, angle, ops);
    update_auto_move(s, p, angle, dt, interacted, probe, ops);
}

/// `LoadInstanceVariables`.
pub fn load_instance(v: &dyn Vars, s: &mut LeverState, p: &mut LeverParams, ops: &mut Ops) {
    let stuck = to_stuck_state(&v.string("StuckState", b""));
    set_stuck_state(s, p, stuck, false, ops);
    s.interaction_disables_stuck = v.boolean("InteractionDisablesStuck", false);
    if v.boolean("OverrideDefaults", false) {
        p.middle_angle_amount = v.float("MiddleAngleAmount", 0.0);
        p.auto_move_to_angle = v.boolean("AutoMoveToAngle", true);
        p.auto_move_goal = to_auto_move_goal(&v.string("AutoMoveGoal", b"Middle"));
        calculate_middle_angle(s, p);
    }
}
