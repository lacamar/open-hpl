//! `cLuxProp_Wheel`: accumulated angle, limits, stuck states, spin direction and auto-rotation.

use super::base::{cmax, cmin};
use super::object::{MoveData, load_rotate};
use super::ops::{Op, Ops, Probe, op};
use super::vars::{Vars, put_var};
use crate::math::{dot3, fma, to_rad, v3};
use crate::types::Vector3f;

pub const SLOT_MIN_LIMIT_SOUND: i32 = 50;
pub const SLOT_MAX_LIMIT_SOUND: i32 = 51;
pub const SLOT_STUCK_SOUND: i32 = 52;

pub(crate) const PIF: f32 = crate::math::PI;
pub(crate) const TWO_PIF: f32 = crate::math::TWO_PI;

unsafe extern "C" {
    fn ohpl_math_get_angle_distance(a: f32, b: f32, max: f32) -> f32;
}

pub(crate) fn angle_distance_rad(a: f32, b: f32) -> f32 {
    // SAFETY: pure function on plain floats.
    unsafe { ohpl_math_get_angle_distance(a, b, TWO_PIF) }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct WheelParams {
    pub data: MoveData,
    pub can_interact_static: bool,
    pub min_limit: f32,
    pub max_limit: f32,
    pub min_limit_range: f32,
    pub max_limit_range: f32,
    pub min_limit_stuck: bool,
    pub max_limit_stuck: bool,
    pub spin_dir: i32,
    pub slow_down_rotation: bool,
    pub skip_middle_state: bool,
}

pub(crate) fn to_spin_dir(s: &[u8]) -> i32 {
    match s {
        b"BothWays" => 0,
        b"ToMax" => 1,
        b"ToMin" => -1,
        _ => 0,
    }
}

pub(crate) fn to_stuck_state(s: &[u8]) -> i32 {
    match s {
        b"None" => 0,
        b"Min" => -1,
        b"Max" => 1,
        _ => 0,
    }
}

pub fn load(v: &dyn Vars, out: &mut WheelParams) {
    load_rotate(v, &mut out.data);
    out.can_interact_static = v.boolean("CanInteractWithStaticBody", true);
    out.min_limit = to_rad(v.float("MinLimit", -360.0));
    out.max_limit = to_rad(v.float("MaxLimit", 360.0));
    out.min_limit_range = to_rad(v.float("MinLimitRange", 5.0));
    out.max_limit_range = to_rad(v.float("MaxLimitRange", 5.0));
    out.min_limit_stuck = v.boolean("MinLimitStuck", false);
    out.max_limit_stuck = v.boolean("MaxLimitStuck", false);
    out.spin_dir = to_spin_dir(&v.string("SpinDir", b"BothWays"));
    out.slow_down_rotation = v.boolean("SlowDownRotation", true);
    out.skip_middle_state = v.boolean("SkipMiddleState", true);
    put_var(v, SLOT_MIN_LIMIT_SOUND, "MinLimitSound");
    put_var(v, SLOT_MAX_LIMIT_SOUND, "MaxLimitSound");
    put_var(v, SLOT_STUCK_SOUND, "StuckSound");
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct WheelState {
    pub angle: f32,
    pub prev_angle: f32,
    pub stuck_sound_timer: f32,
    pub last_to_max: f32,
    pub last_to_min: f32,
    pub current_state: i32,
    pub stuck_state: i32,
    pub joint_angle: f32,
    pub prev_joint_angle: f32,
    pub auto_moving: bool,
    pub auto_move_goal: f32,
    pub interaction_disables_stuck: bool,
}

pub fn can_interact(p: &WheelParams, mass: f32) -> bool {
    !(mass == 0.0 && !p.can_interact_static)
}

pub fn reset_auto_move(s: &mut WheelState, ops: &mut Ops) {
    s.auto_moving = false;
    ops.push(Op::new(op::PID_RESET));
}

/// The hinge starts unlimited (min = max = 0); a two-way wheel also sticks to its limits.
pub fn setup(p: &WheelParams, ops: &mut Ops) {
    if p.spin_dir == 0 {
        ops.push(Op::new(op::HINGE_STICKY).i(0).j(1));
    }
    ops.push(Op::new(op::HINGE_MIN).i(0).f(0.0));
    ops.push(Op::new(op::HINGE_MAX).i(0).f(0.0));
}

fn change_state(s: &mut WheelState, p: &WheelParams, state: i32, effects: bool, ops: &mut Ops) {
    if s.current_state == state {
        return;
    }
    if p.skip_middle_state && state == 0 {
        return;
    }
    s.current_state = state;
    if state == 1 || state == -1 {
        ops.event(Op::new(op::CONN_STATE).i(state), s);
        ops.push(Op::new(op::LIMIT_SOUND).i(state).j(i32::from(effects)));
    }
    ops.event(Op::new(op::CONN_LIMIT).i(state), s);
}

pub fn set_stuck_state(
    s: &mut WheelState,
    p: &WheelParams,
    state: i32,
    effects: bool,
    probe: &mut dyn Probe,
    ops: &mut Ops,
) {
    if s.stuck_state == state {
        return;
    }
    s.stuck_state = state;
    s.stuck_sound_timer = 0.0;
    if state == 1 || state == -1 {
        s.angle = if state == 1 {
            p.max_limit - p.max_limit_range / 2.0f32
        } else {
            p.min_limit + p.min_limit_range / 2.0f32
        };
        s.prev_angle = s.angle;
        let a = probe.hinge(0).angle;
        let max = cmin(a + to_rad(1.0), to_rad(180.0));
        let min = cmax(a - to_rad(1.0), to_rad(-180.0));
        ops.push(Op::new(op::HINGE_MAX).i(0).f(max));
        ops.push(Op::new(op::HINGE_MIN).i(0).f(min));
        change_state(s, p, state, effects, ops);
        reset_auto_move(s, ops);
    } else {
        ops.push(Op::new(op::HINGE_MIN).i(0).f(0.0));
        ops.push(Op::new(op::HINGE_MAX).i(0).f(0.0));
    }
}

pub fn set_angle(s: &mut WheelState, p: &WheelParams, x: f32, auto_move: bool, ops: &mut Ops) {
    if x == s.angle {
        return;
    }
    let mut wanted = x;
    if wanted < p.min_limit {
        wanted = p.min_limit;
    }
    if wanted > p.max_limit {
        wanted = p.max_limit;
    }
    if auto_move {
        if wanted == s.angle {
            return;
        }
    } else if (wanted - s.angle).abs() < 0.0001f32 {
        return;
    }
    if auto_move {
        s.auto_moving = true;
        s.auto_move_goal = wanted;
        ops.push(Op::new(op::PID_RESET));
    } else {
        s.angle = wanted;
        s.prev_angle = s.angle;
        reset_auto_move(s, ops);
    }
}

pub fn on_connection_state(
    s: &mut WheelState,
    p: &WheelParams,
    state: i32,
    probe: &mut dyn Probe,
    ops: &mut Ops,
) {
    set_stuck_state(s, p, if state > 0 { -1 } else { 0 }, true, probe, ops);
}

/// `OnInteract` before the player state change; returns whether to show the hint.
pub fn on_interact(
    s: &mut WheelState,
    p: &WheelParams,
    show_hints: bool,
    disables_stuck: bool,
    probe: &mut dyn Probe,
    ops: &mut Ops,
) -> bool {
    reset_auto_move(s, ops);
    if s.stuck_state != 0 && disables_stuck {
        set_stuck_state(s, p, 0, true, probe, ops);
    }
    s.stuck_state == 0 && show_hints
}

fn set_max_joint(s: &WheelState, max_angle: f32, ops: &mut Ops) {
    let dist = max_angle - s.angle;
    let hinge_max = |ops: &mut Ops, v: f32| ops.push(Op::new(op::HINGE_MAX).i(0).f(v));
    if dist > PIF {
        hinge_max(ops, TWO_PIF);
        return;
    }
    let joint_max = s.joint_angle + dist;
    if (s.joint_angle >= 0.0 && joint_max <= PIF) || (s.joint_angle < 0.0 && joint_max < 0.0) {
        hinge_max(ops, joint_max);
    } else {
        hinge_max(ops, TWO_PIF);
    }
}

fn set_min_joint(s: &WheelState, min_angle: f32, ops: &mut Ops) {
    let dist = s.angle - min_angle;
    let hinge_min = |ops: &mut Ops, v: f32| ops.push(Op::new(op::HINGE_MIN).i(0).f(v));
    if dist > PIF {
        hinge_min(ops, -TWO_PIF);
        return;
    }
    let joint_min = s.joint_angle - dist;
    if (s.joint_angle >= 0.0 && joint_min >= 0.0) || (s.joint_angle < 0.0 && joint_min > -PIF) {
        hinge_min(ops, joint_min);
    } else {
        hinge_min(ops, -TWO_PIF);
    }
}

fn update_angle(
    s: &mut WheelState,
    p: &WheelParams,
    dt: f32,
    interacted: bool,
    probe: &mut dyn Probe,
    ops: &mut Ops,
) {
    let last_angle = s.angle;
    s.joint_angle = probe.hinge(0).angle;
    let add = angle_distance_rad(s.prev_joint_angle, s.joint_angle);
    s.prev_angle = s.angle;
    s.angle += add;
    s.prev_joint_angle = s.joint_angle;
    let _ = last_angle;

    if s.stuck_state != 0 && interacted && s.stuck_sound_timer <= 0.0 && add.abs() > 0.01f32 * dt {
        ops.push(Op::new(op::PLAY_SOUND).i(op::SND_WHEEL_STUCK));
        s.stuck_sound_timer = 1.5;
    }

    let mut spin_val = 0.0f32;
    if p.spin_dir == 1 && s.angle > s.last_to_max {
        s.last_to_max = s.angle;
        spin_val = probe.hinge(0).angle;
    }
    if p.spin_dir == -1 && s.angle < s.last_to_min {
        s.last_to_min = s.angle;
        spin_val = probe.hinge(0).angle;
    }

    if p.max_limit == 0.0 && p.min_limit == 0.0 {
        return;
    }
    if s.stuck_state == 0 {
        if p.spin_dir == 1 {
            set_max_joint(s, p.max_limit, ops);
            ops.push(Op::new(op::HINGE_MIN).i(0).f(spin_val - to_rad(0.5)));
        } else if p.spin_dir == -1 {
            set_min_joint(s, p.min_limit, ops);
            ops.push(Op::new(op::HINGE_MAX).i(0).f(spin_val + to_rad(0.5)));
        } else {
            set_max_joint(s, p.max_limit, ops);
            set_min_joint(s, p.min_limit, ops);
        }
    }
    let turn = Op {
        f: [s.angle, s.prev_angle, p.min_limit, p.max_limit],
        ..Op::new(op::CONN_TURN)
    };
    ops.event(turn, s);
}

fn update_check_limit(s: &mut WheelState, p: &WheelParams, probe: &mut dyn Probe, ops: &mut Ops) {
    if s.stuck_state != 0 || (p.max_limit == 0.0 && p.min_limit == 0.0) {
        return;
    }
    if s.angle > p.max_limit - p.max_limit_range {
        change_state(s, p, 1, true, ops);
        if p.max_limit_stuck {
            set_stuck_state(s, p, 1, false, probe, ops);
        }
    } else if s.angle < p.min_limit + p.min_limit_range {
        change_state(s, p, -1, true, ops);
        if p.min_limit_stuck {
            set_stuck_state(s, p, -1, false, probe, ops);
        }
    } else {
        change_state(s, p, 0, true, ops);
    }
}

fn sub(a: Vector3f, b: Vector3f) -> Vector3f {
    v3(a.x - b.x, a.y - b.y, a.z - b.z)
}

fn update_auto_rotation(
    s: &mut WheelState,
    p: &WheelParams,
    dt: f32,
    interacted: bool,
    probe: &mut dyn Probe,
    ops: &mut Ops,
) {
    if s.auto_moving {
        if (s.angle - s.auto_move_goal).abs() < to_rad(0.1f32) {
            reset_auto_move(s, ops);
        } else {
            let wanted_speed = fma(
                s.angle - s.auto_move_goal,
                10.0f32,
                (s.prev_angle - s.angle) * dt,
            );
            let h = probe.hinge(0);
            let dir = h.pin_dir;
            let k = dot3(dir, h.child_ang_vel);
            let err = v3(
                fma(-dir.x, k, dir.x * wanted_speed),
                fma(-dir.y, k, dir.y * wanted_speed),
                fma(-dir.z, k, dir.z * wanted_speed),
            );
            ops.push(Op::new(op::PID_TORQUE).v(err).f(dt));
        }
    }
    if interacted {
        return;
    }
    if !s.auto_moving && p.slow_down_rotation {
        let h = probe.hinge(0);
        ops.push(
            Op::new(op::PID_TORQUE)
                .v(sub(v3(0.0, 0.0, 0.0), h.child_ang_vel))
                .f(dt),
        );
    }
}

pub fn update(
    s: &mut WheelState,
    p: &WheelParams,
    dt: f32,
    interacted: bool,
    probe: &mut dyn Probe,
    ops: &mut Ops,
) {
    if s.stuck_sound_timer > 0.0 {
        s.stuck_sound_timer -= dt;
    }
    update_angle(s, p, dt, interacted, probe, ops);
    update_check_limit(s, p, probe, ops);
    update_auto_rotation(s, p, dt, interacted, probe, ops);
}

/// `LoadInstanceVariables`.
pub fn load_instance(
    v: &dyn Vars,
    s: &mut WheelState,
    p: &mut WheelParams,
    probe: &mut dyn Probe,
    ops: &mut Ops,
) {
    if v.boolean("OverrideDefaults", false) {
        p.min_limit = to_rad(v.float("MinLimit", -360.0));
        p.max_limit = to_rad(v.float("MaxLimit", 360.0));
        p.min_limit_stuck = v.boolean("MinLimitStuck", false);
        p.max_limit_stuck = v.boolean("MaxLimitStuck", false);
        p.spin_dir = to_spin_dir(&v.string("SpinDir", b"BothWays"));
    }
    let stuck = to_stuck_state(&v.string("StuckState", b""));
    set_stuck_state(s, p, stuck, false, probe, ops);
    s.interaction_disables_stuck = v.boolean("InteractionDisablesStuck", false);
}
