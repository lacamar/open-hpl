//! `iPhysicsController::Update`.

use super::body::rotation;
use super::host::{H, Host, Str};
use super::pid::Pid;
use crate::math::{fma, matrix_inverse, matrix_mul_vec, mul3, v3};
use crate::types::Vector3f;

const INPUT_JOINT_ANGLE: i32 = 0;
const INPUT_JOINT_DIST: i32 = 1;
const INPUT_LINEAR_SPEED: i32 = 2;
const INPUT_ANGULAR_SPEED: i32 = 3;
const OUTPUT_FORCE: i32 = 0;
const OUTPUT_TORQUE: i32 = 1;
const TYPE_PID: i32 = 0;
const END_ON_DEST: i32 = 1;
const EPSILON: f32 = 0.0001;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct CtrlParams {
    pub this: H,
    pub body: H,
    pub joint: H,
    pub active: bool,
    pub paused: bool,
    pub log: bool,
    pub mul_mass: bool,
    pub kind: i32,
    pub input: i32,
    pub input_axis: i32,
    pub output: i32,
    pub output_axis: i32,
    pub end: i32,
    pub a: f32,
    pub b: f32,
    pub c: f32,
    pub dest: f32,
    pub max_output: f32,
    pub next: Str,
}

fn axis(a: i32, v: Vector3f) -> f32 {
    match a {
        0 => v.x,
        1 => v.y,
        2 => v.z,
        _ => 0.0,
    }
}

fn input_value<Ho: Host>(h: &Ho, p: &CtrlParams) -> Vector3f {
    let scalar = |f: f32| v3(f, f, f);
    match p.input {
        INPUT_ANGULAR_SPEED => h.body_angular_velocity(p.body),
        INPUT_LINEAR_SPEED => h.body_linear_velocity(p.body),
        INPUT_JOINT_ANGLE if p.joint != 0 => scalar(h.joint_angle(p.joint)),
        INPUT_JOINT_DIST if p.joint != 0 => scalar(h.joint_distance(p.joint)),
        _ => scalar(0.0),
    }
}

fn add_output<Ho: Host>(h: &Ho, p: &CtrlParams, value: f32) {
    let mut v = match p.output_axis {
        0 => v3(value, 0.0, 0.0),
        1 => v3(0.0, value, 0.0),
        2 => v3(0.0, 0.0, value),
        _ => v3(0.0, 0.0, 0.0),
    };
    if p.mul_mass {
        v = mul3(v, h.body_mass(p.body));
    }
    v = matrix_mul_vec(&rotation(&h.body_local_matrix(p.body)), v);
    match p.output {
        OUTPUT_TORQUE => h.body_add_torque(p.body, v),
        OUTPUT_FORCE => h.body_add_force(p.body, v),
        _ => {}
    }
}

/// `pid` is dereferenced only while computing the output, never across a host call that can reach
/// another controller.
///
/// # Safety
/// `pid` is a live `Pid` not otherwise borrowed for the duration of the call.
pub unsafe fn update<Ho: Host>(h: &Ho, p: &CtrlParams, pid: *mut Pid, time_step: f32) {
    if !p.active || p.paused {
        return;
    }
    if p.body == 0 {
        return;
    }

    let input = input_value(h, p);
    let inverse = matrix_inverse(&rotation(&h.body_local_matrix(p.body)));
    let input = matrix_mul_vec(&inverse, input);

    let value = axis(p.input_axis, input);
    let error = p.dest - value;

    let mut output = if p.kind == TYPE_PID {
        // SAFETY: caller contract.
        match unsafe { pid.as_mut() } {
            Some(pid) => {
                pid.p = p.a;
                pid.i = p.b;
                pid.d = p.c;
                pid.output(error, time_step)
            }
            None => 0.0,
        }
    } else {
        fma(error, p.a, -(value * p.b))
    };

    if p.max_output > 0.0 {
        output = if output > 0.0 {
            if output < p.max_output {
                output
            } else {
                p.max_output
            }
        } else if output > -p.max_output {
            output
        } else {
            -p.max_output
        };
    }

    if p.log {
        h.ctrl_log(p.this, value, p.dest, error, output);
    }

    add_output(h, p, output);

    if p.end == END_ON_DEST && p.joint != 0 && (value - p.dest).abs() < EPSILON {
        h.ctrl_deactivate(p.this);
        let next = h.joint_get_controller(p.joint, p.next);
        if next != 0 {
            h.ctrl_set_active(next, true);
        }
    }
}
