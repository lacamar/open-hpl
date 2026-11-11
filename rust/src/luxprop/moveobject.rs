//! `cLuxProp_MoveObject`: a body that slides or turns between a closed and an open transform.

use super::base::{
    BaseState, euler_angle_distance, matrix_slerp, rotation, set_translation, translation,
};
use super::ops::Ops;
use super::vars::{Vars, put_var, zero3};
use crate::math::{fma, matrix_inverse, matrix_mul, matrix_mul_vec, matrix_rotate, v3};
use crate::types::{Matrixf, Vector3f};

pub const SLOT_ANGULAR_OFFSET_AREA: i32 = 90;

const PI2F: f32 = crate::math::PI2;

pub const LINEAR: i32 = 0;
pub const ANGULAR: i32 = 1;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct MoveParams {
    pub move_type: i32,
    pub bad_type: bool,
    pub open_amount: f32,
    pub move_axis: i32,
    pub bad_axis: bool,
    pub open_acc: f32,
    pub open_speed: f32,
    pub close_acc: f32,
    pub close_speed: f32,
    pub auto_move: bool,
    pub auto_move_state_goal: f32,
    pub auto_move_acc: f32,
    pub auto_move_speed: f32,
    pub auto_move_slowdown_dist: f32,
}

pub fn load(v: &dyn Vars, out: &mut MoveParams) {
    let low = crate::string::to_lower(&v.string("MoveObjectType", b"Linear"));
    out.bad_type = false;
    out.move_type = match low.as_slice() {
        b"linear" => LINEAR,
        b"angular" => ANGULAR,
        _ => {
            out.bad_type = true;
            LINEAR
        }
    };
    out.open_amount = v.float("OpenAmount", 1.0);
    let axis = crate::string::to_lower(&v.string("MoveAxis", b"y"));
    out.bad_axis = false;
    out.move_axis = match axis.as_slice() {
        b"x" => 0,
        b"y" => 1,
        b"z" => 2,
        _ => {
            out.bad_axis = true;
            0
        }
    };
    out.open_acc = v.float("OpenAcc", 1.0);
    out.open_speed = v.float("OpenSpeed", 0.8);
    out.close_acc = v.float("CloseAcc", 9.0);
    out.close_speed = v.float("CloseSpeed", 5.0);
    out.auto_move = v.boolean("AutoMove", false);
    out.auto_move_state_goal = v.float("AutoMoveStateGoal", 0.0);
    out.auto_move_acc = v.float("AutoMoveAcc", 9.0);
    out.auto_move_speed = v.float("AutoMoveSpeed", 4.0);
    out.auto_move_slowdown_dist = v.float("AutoMoveSlowdownDist", 0.1);
}

pub fn load_instance(v: &dyn Vars) {
    put_var(v, SLOT_ANGULAR_OFFSET_AREA, "AngularOffsetArea");
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct MoveObjState {
    pub angular_offset_pos: Vector3f,
    pub use_angular_local_offset: bool,
    pub auto_move_reached_goal: bool,
    pub closed: Matrixf,
    pub open: Matrixf,
}

fn axis_vec(axis: i32) -> Vector3f {
    match axis {
        0 => v3(1.0, 0.0, 0.0),
        1 => v3(0.0, 1.0, 0.0),
        _ => v3(0.0, 0.0, 1.0),
    }
}

pub fn calculate_open_rotate(s: &mut MoveObjState, p: &MoveParams) {
    let a = axis_vec(p.move_axis);
    let rot = matrix_rotate(
        v3(
            a.x * PI2F * p.open_amount,
            a.y * PI2F * p.open_amount,
            a.z * PI2F * p.open_amount,
        ),
        0,
    );
    let mut open = matrix_mul(&rotation(&s.closed), &rot);
    set_translation(&mut open, translation(&s.closed));
    s.open = open;
}

/// `OnSetupAfterLoad`: `body` is the main body's matrix and `local` its local bounding box.
pub fn setup(
    s: &mut MoveObjState,
    p: &MoveParams,
    body: &Matrixf,
    local_min: Vector3f,
    local_max: Vector3f,
) {
    s.closed = *body;
    if p.move_type == LINEAR {
        let size = v3(
            local_max.x - local_min.x,
            local_max.y - local_min.y,
            local_max.z - local_min.z,
        );
        let m = axis_vec(p.move_axis);
        let t = translation(&s.closed);
        let mut open = rotation(&s.closed);
        set_translation(
            &mut open,
            v3(
                fma(size.x * m.x, p.open_amount, t.x),
                fma(size.y * m.y, p.open_amount, t.y),
                fma(size.z * m.z, p.open_amount, t.z),
            ),
        );
        s.open = open;
    } else {
        calculate_open_rotate(s, p);
    }
}

pub fn set_angular_offset(s: &mut MoveObjState, p: &MoveParams, pos: Vector3f) {
    s.angular_offset_pos = pos;
    s.use_angular_local_offset = true;
    if p.move_type == ANGULAR {
        calculate_open_rotate(s, p);
    }
}

pub fn get_move_state(s: &MoveObjState, p: &MoveParams, body: &Matrixf) -> f32 {
    let (delta, t);
    if p.move_type == LINEAR {
        let cur = translation(body);
        let closed = translation(&s.closed);
        let open = translation(&s.open);
        delta = v3(cur.x - closed.x, cur.y - closed.y, cur.z - closed.z);
        t = v3(
            delta.x / (open.x - closed.x),
            delta.y / (open.y - closed.y),
            delta.z / (open.z - closed.z),
        );
    } else {
        delta = euler_angle_distance(&s.closed, body);
        let full = euler_angle_distance(&s.closed, &s.open);
        t = v3(delta.x / full.x, delta.y / full.y, delta.z / full.z);
    }
    if delta.x != 0.0 {
        t.x
    } else if delta.y != 0.0 {
        t.y
    } else if delta.z != 0.0 {
        t.z
    } else {
        0.0
    }
}

#[expect(clippy::too_many_arguments, reason = "mirrors MoveToState")]
pub fn move_to_state(
    s: &MoveObjState,
    p: &MoveParams,
    base: &mut BaseState,
    state: f32,
    acc: f32,
    max_speed: f32,
    slowdown: f32,
    reset: bool,
    ops: &mut Ops,
) {
    if p.move_type == LINEAR {
        let k = 1.0f32 - state;
        let (c, o) = (translation(&s.closed), translation(&s.open));
        let wanted = v3(
            fma(c.x, k, o.x * state),
            fma(c.y, k, o.y * state),
            fma(c.z, k, o.z * state),
        );
        base.move_linear_to(wanted, acc, max_speed, slowdown, reset, ops);
    } else {
        let wanted = matrix_slerp(state, &s.closed, &s.open);
        let inv = matrix_inverse(&s.closed);
        let local = matrix_mul_vec(&inv, s.angular_offset_pos);
        if s.use_angular_local_offset {
            base.move_angular_to(
                &wanted,
                acc,
                max_speed,
                slowdown,
                reset,
                true,
                s.angular_offset_pos,
                local,
                ops,
            );
        } else {
            base.move_angular_to(
                &wanted,
                acc,
                max_speed,
                slowdown,
                reset,
                false,
                zero3(),
                zero3(),
                ops,
            );
        }
    }
}

/// `MoveToState(float)`: picks the close or open motion by the current position.
pub fn move_to_state_auto(
    s: &MoveObjState,
    p: &MoveParams,
    base: &mut BaseState,
    body: &Matrixf,
    state: f32,
    ops: &mut Ops,
) {
    let current = get_move_state(s, p, body);
    if state < current {
        move_to_state(
            s,
            p,
            base,
            state,
            p.close_acc,
            p.close_speed,
            0.3f32,
            true,
            ops,
        );
    } else {
        move_to_state(
            s,
            p,
            base,
            state,
            p.open_acc,
            p.open_speed,
            0.3f32,
            true,
            ops,
        );
    }
}

pub fn update_auto_move(
    s: &mut MoveObjState,
    p: &MoveParams,
    base: &mut BaseState,
    body: &Matrixf,
    ops: &mut Ops,
) {
    if !p.auto_move || base.moving || s.auto_move_reached_goal {
        return;
    }
    if (get_move_state(s, p, body) - p.auto_move_state_goal).abs() < 0.001f32 {
        s.auto_move_reached_goal = true;
    } else {
        move_to_state(
            s,
            p,
            base,
            p.auto_move_state_goal,
            p.auto_move_acc,
            p.auto_move_speed,
            p.auto_move_slowdown_dist,
            true,
            ops,
        );
    }
}

pub fn on_connection_state(
    s: &MoveObjState,
    p: &MoveParams,
    base: &mut BaseState,
    body: &Matrixf,
    state: i32,
    ops: &mut Ops,
) {
    if state > 0 {
        move_to_state_auto(s, p, base, body, 1.0, ops);
    }
    if state < 0 {
        move_to_state_auto(s, p, base, body, 0.0, ops);
    }
}
