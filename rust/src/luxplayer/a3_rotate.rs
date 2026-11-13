//! `iLuxPlayerState_InteractRotateBase` with the swing door, lever and wheel states: spin a hinged body.

use super::a3_default::{act_of, reset_interact_vars, s, setup_interact_vars};
use super::a3_host::{Host, bd, cb, data};
use super::a3_state::{Base, Kind, Rotate};
use super::states_base as sb;
use super::states_rotate::{self as sr, JointGeom, RotateTorqueIn};
use crate::types::{Vector2f, Vector3f};
use std::collections::VecDeque;

pub fn init(p: &mut Rotate, h: &Host, kind: Kind) {
    let c = |k, d| h.cfg(c"Player_Interaction", k, d);
    p.move_to_mouse_factor = c(c"MoveToMouseAddFactor", 0.01);
    p.max_torque = c(c"MoveMaxForce", 1000.0);
    p.rotate_pid.init(10, 10.0, 0.0, 1.0);
    if kind == Kind::Wheel {
        p.rotate_pid.p = 5.0;
        p.rotate_pid.i = 0.0;
        p.rotate_pid.d = 0.1;
    }
}

fn history(b: *mut Base) -> *mut VecDeque<Vector2f> {
    if s!(b).ext.is_null() {
        s!(b).ext = Box::into_raw(Box::new(VecDeque::<Vector2f>::new())).cast();
    }
    s!(b).ext.cast()
}

pub fn free(b: *mut Base) {
    let e = s!(b).ext.cast::<VecDeque<Vector2f>>();
    if !e.is_null() {
        // SAFETY: `ext` came from `Box::into_raw` in `history` and is cleared here.
        drop(unsafe { Box::from_raw(e) });
        s!(b).ext = std::ptr::null_mut();
    }
}

pub unsafe fn enter(b: *mut Base, h: &Host, kind: Kind) {
    let p = b.cast::<Rotate>();
    if !h.in_smooth_mouse() {
        h.in_reset_smooth();
    }
    // SAFETY: caller contract.
    unsafe { setup_interact_vars(b, h) };
    let (prop, cur) = (s!(b).cur_prop, s!(b).cur_body);
    s!(p).data = h.prop_data(prop, data::MOVE_BASE);
    let joint = h.body_joint(cur, 0);
    s!(p).joint = joint;
    s!(p).rotate_pid.reset();
    s!(p).rot_speed = 0.0;
    s!(p).mouse_add = Vector2f::default();
    s!(p).has_gravity = h.body_flag(cur, bd::FLAG_GRAVITY);
    h.body_set_flag(cur, bd::FLAG_GRAVITY, false);
    let (feet, pivot) = (h.cb_v3(cb::V3_FEET), h.joint_pivot(joint));
    s!(p).max_distance = sr::rotate_max_distance(feet, pivot);
    if kind == Kind::Wheel {
        // SAFETY: `history` returns the live deque of this state.
        unsafe { (*history(b)).clear() };
    }
}

pub unsafe fn leave(b: *mut Base, h: &Host) {
    let p = b.cast::<Rotate>();
    h.body_set_flag(s!(b).cur_body, bd::FLAG_GRAVITY, s!(p).has_gravity);
    // SAFETY: caller contract.
    unsafe { reset_interact_vars(b, h) };
}

fn joint_geom(b: *mut Base, h: &Host) -> JointGeom {
    let p = b.cast::<Rotate>();
    let (cur, joint) = (s!(b).cur_body, s!(p).joint);
    JointGeom {
        body_matrix: h.body_matrix(cur, bd::M_LOCAL),
        mass_centre: h.body_v3(cur, bd::V3_MASS_CENTRE),
        pivot: h.joint_pivot(joint),
        pin: h.joint_pin(joint),
        up: h.cam_up(),
        right: h.cam_right(),
        forward: h.cam_forward(),
        mouse: s!(p).mouse_add,
    }
}

unsafe fn speed_add(b: *mut Base, h: &Host, kind: Kind) -> f32 {
    let p = b.cast::<Rotate>();
    match kind {
        Kind::Lever => sr::lever_speed_add(&joint_geom(b, h)),
        Kind::SwingDoor => sr::swing_door_speed_add(&joint_geom(b, h)),
        _ => {
            let mouse = s!(p).mouse_add;
            let hist = history(b);
            // SAFETY: `history` returns the live deque of this state; no host call while it is borrowed.
            let list: Vec<Vector2f> = unsafe {
                (*hist).push_back(mouse);
                if sr::wheel_pop((*hist).len()) {
                    (*hist).pop_front();
                }
                (*hist).iter().copied().collect()
            };
            let pin = h.joint_pin(s!(p).joint);
            let fwd = h.cam_forward();
            sr::wheel_speed_add(&list, mouse, pin, fwd)
        }
    }
}

pub unsafe fn update(b: *mut Base, h: &Host, kind: Kind, dt: f32) {
    let p = b.cast::<Rotate>();
    if !h.in_smooth_mouse() {
        let m = s!(p).mouse_add;
        s!(p).mouse_add = h.in_smooth_pos(m);
    }
    let (cur, joint, md) = (s!(b).cur_body, s!(p).joint, s!(p).data);
    let (feet, pivot) = (h.cb_v3(cb::V3_FEET), h.joint_pivot(joint));
    if sr::rotate_far(feet, pivot, s!(p).max_distance) {
        let prev = h.prev_state();
        h.change_state(prev);
    }
    let slow = h.data_f(md, data::MOVE_BASE, data::M_SLOW_DOWN);
    let mouse = s!(p).mouse_add;
    sr::rotate_slow(&mut s!(p).rot_speed, mouse, slow, dt);
    // SAFETY: caller contract.
    let add = unsafe { speed_add(b, h, kind) };
    let ti = RotateTorqueIn {
        pin: h.joint_pin(joint),
        ang_vel: h.body_v3(cur, bd::V3_ANG_VEL),
        inertia: h.body_matrix(cur, bd::M_INERTIA),
        speed_add: add,
        speed_factor: h.data_f(md, data::MOVE_BASE, data::M_SPEED_FACTOR),
        max_speed: h.data_f(md, data::MOVE_BASE, data::M_MAX_SPEED),
        max_torque: s!(p).max_torque,
    };
    let (speed, pid) = (&raw mut s!(p).rot_speed, &raw mut s!(p).rotate_pid);
    // SAFETY: disjoint fields of the rotate state, and no host call runs meanwhile.
    let torque = sr::rotate_torque(&ti, unsafe { &mut *speed }, unsafe { &mut *pid }, dt);
    h.body_add(cur, bd::ADD_TORQUE, torque, Vector3f::default());
    s!(p).last_mouse_add = s!(p).mouse_add;
    s!(p).mouse_add = Vector2f::default();
}

unsafe fn on_throw(b: *mut Base, h: &Host, kind: Kind) {
    if kind != Kind::SwingDoor {
        return;
    }
    let p = b.cast::<Rotate>();
    let g = joint_geom(b, h);
    let impulse = h.data_f(s!(p).data, data::MOVE_BASE, data::M_THROW_IMPULSE);
    let (forward, v) = sr::swing_door_throw(&g, impulse);
    s!(p).joint_forward = forward;
    h.body_add(s!(b).cur_body, bd::ADD_IMPULSE, v, Vector3f::default());
}

pub unsafe fn do_action(b: *mut Base, h: &Host, kind: Kind, raw: i32, pressed: bool) -> bool {
    match sb::interact_action(act_of(raw), pressed) {
        1 => {
            let prev = h.prev_state();
            h.change_state(prev);
            false
        }
        2 => {
            // SAFETY: caller contract.
            unsafe { on_throw(b, h, kind) };
            let prev = h.prev_state();
            h.change_state(prev);
            false
        }
        _ => true,
    }
}

pub unsafe fn on_move(b: *mut Base, dir: i32, mul: f32) {
    let p = b.cast::<Rotate>();
    let factor = s!(p).move_to_mouse_factor;
    sr::move_to_mouse(dir == 0, mul, factor, &mut s!(p).mouse_add);
}

pub unsafe fn add_yaw(b: *mut Base, amount: f32) {
    s!(b.cast::<Rotate>()).mouse_add.x += amount;
}

pub unsafe fn add_pitch(b: *mut Base, h: &Host, amount: f32) {
    let amount = if h.in_invert_pitch() { -amount } else { amount };
    s!(b.cast::<Rotate>()).mouse_add.y += amount;
}
