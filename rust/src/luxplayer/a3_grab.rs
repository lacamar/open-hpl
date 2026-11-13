//! `cLuxPlayerState_InteractGrab`: pick up, hold and throw a body with its attached bodies.

use super::a3_default::{act_of, reset_interact_vars, s, setup_interact_vars};
use super::a3_host::{Host, Ptr, bd, data};
use super::a3_state::{Base, Grab};
use super::states_base as sb;
use super::states_grab::{self as sg, GrabEnterIn, GrabEnterOut, GrabPost, GrabPostOut};
use super::states_vec::hmul3;
use crate::types::{Matrixf, Vector3f};

#[derive(Clone, Copy)]
pub struct BodyProps {
    pub body: Ptr,
    pub gravity: bool,
    pub collide_character: bool,
    pub mass: f32,
}

pub fn init(g: &mut Grab, h: &Host) {
    let c = |k, d| h.cfg(c"Player_Interaction", k, d);
    g.max_force = c(c"GrabMaxForce", 0.0);
    g.max_torque = c(c"GrabMaxTorque", 0.0);
    g.max_angular_speed = c(c"GrabMaxAngularSpeed", 0.0);
    g.max_leave_angular = c(c"GrabMaxLeaveAngularSpeed", 0.0);
    g.max_leave_linear = c(c"GrabMaxLeaveLinearSpeed", 0.0);
    g.min_slow_mass = c(c"GrabMinSlowPlayerMass", 0.0);
    g.max_slow_mass = c(c"GrabMaxSlowPlayerMass", 0.0);
    g.min_slow_mul = c(c"GrabMinSlowPlayerMul", 0.0);
    g.force_pid.init(20, 400.0, 0.0, 40.0);
    g.torque_pid.init(20, 40.0, 0.0, 0.4);
}

fn props(p: *mut Base) -> Vec<BodyProps> {
    let e = s!(p).ext.cast::<Vec<BodyProps>>();
    if e.is_null() {
        Vec::new()
    } else {
        // SAFETY: `ext` is the Vec made by `set_props`, freed only by `free`.
        unsafe { (*e).clone() }
    }
}

fn set_props(p: *mut Base, v: Vec<BodyProps>) {
    let e = s!(p).ext.cast::<Vec<BodyProps>>();
    if e.is_null() {
        s!(p).ext = Box::into_raw(Box::new(v)).cast();
    } else {
        // SAFETY: as in `props`.
        unsafe { *e = v };
    }
}

pub fn free(p: *mut Base) {
    let e = s!(p).ext.cast::<Vec<BodyProps>>();
    if !e.is_null() {
        // SAFETY: `ext` came from `Box::into_raw` in `set_props` and is cleared here.
        drop(unsafe { Box::from_raw(e) });
        s!(p).ext = std::ptr::null_mut();
    }
}

fn save_body_properties(h: &Host, body: Ptr, out: &mut Vec<BodyProps>) {
    out.push(BodyProps {
        body,
        gravity: h.body_flag(body, bd::FLAG_GRAVITY),
        collide_character: h.body_flag(body, bd::FLAG_COLLIDE_CHAR),
        mass: h.body_mass(body),
    });
    for i in 0..h.body_joint_num(body) {
        let joint = h.body_joint(body, i);
        let child = h.joint_child(joint);
        let parent = h.joint_parent(joint);
        if !child.is_null() && !out.iter().any(|b| b.body == child) {
            save_body_properties(h, child, out);
        }
        if !parent.is_null() && !out.iter().any(|b| b.body == parent) {
            save_body_properties(h, parent, out);
        }
    }
}

pub unsafe fn enter(p: *mut Base, h: &Host) {
    let g = p.cast::<Grab>();
    // SAFETY: caller contract.
    unsafe { setup_interact_vars(p, h) };
    let (pos, ang) = (h.cam_pos(), h.cam_angles());
    let (prop, cur) = (s!(p).cur_prop, s!(p).cur_body);
    let gd = h.prop_data(prop, data::GRAB);
    s!(g).data = gd;
    s!(g).force_pid.reset();
    s!(g).torque_pid.reset();
    let mut list = Vec::new();
    save_body_properties(h, cur, &mut list);
    let mass_mul = h.data_f(gd, data::GRAB, data::G_MASS_MUL);
    let mut mass_sum = 0.0f32;
    let mut proper = 0.0f32;
    for bp in &list {
        proper += h.body_mass(bp.body);
        if bp.body == cur {
            h.body_set_flag(bp.body, bd::FLAG_GRAVITY, false);
            h.body_set_flag(bp.body, bd::FLAG_COLLIDE_CHAR, false);
        }
        let m = h.body_mass(bp.body);
        h.body_set_mass(bp.body, m * mass_mul);
        h.body_set_v3(bp.body, bd::V3_ANG_VEL, Vector3f::default());
        h.body_set_v3(bp.body, bd::V3_LIN_VEL, Vector3f::default());
        mass_sum += h.body_mass(bp.body);
    }
    set_props(p, list);
    s!(g).mass_sum = mass_sum;
    let gi = GrabEnterIn {
        cam_pos: pos,
        cam_rot: ang,
        body_matrix: h.body_matrix(cur, bd::M_LOCAL),
        body_pos: h.body_v3(cur, bd::V3_POS),
        focus_pos: s!(p).cur_focus_pos,
        rot_offset: h.data_v3(gd, data::GRAB, data::G_ROT_OFFSET),
        use_offset: u32::from(h.data_b(gd, data::GRAB, data::G_USE_OFFSET)),
        use_depth: u32::from(h.data_b(gd, data::GRAB, data::G_USE_DEPTH)),
        depth: h.data_f(gd, data::GRAB, data::G_DEPTH),
        max_depth: h.data_f(gd, data::GRAB, data::G_MAX_DEPTH),
        proper_mass_sum: proper,
        min_slow_mass: s!(g).min_slow_mass,
        max_slow_mass: s!(g).max_slow_mass,
        min_slow_mul: s!(g).min_slow_mul,
    };
    let mut go = GrabEnterOut {
        local_offset: s!(g).local_body_offset,
        ..GrabEnterOut::default()
    };
    sg::grab_enter(&gi, &mut go);
    s!(g).body_rotation = go.body_rot;
    if !h.data_b(gd, data::GRAB, data::G_USE_OFFSET) {
        s!(g).local_body_offset = go.local_offset;
    }
    s!(g).depth = go.depth;
    s!(g).max_distance = go.max_distance;
    let mass = h.default_mass() + proper;
    h.cb_set_mass(mass);
    h.prop_set_disable_coll(prop, false);
    if go.slow != 0 {
        h.set_interact_mul(go.slow_mul);
    }
    s!(g).custom_materials = true;
}

pub unsafe fn leave(p: *mut Base, h: &Host) {
    for bp in props(p) {
        h.body_set_flag(bp.body, bd::FLAG_GRAVITY, bp.gravity);
        h.body_set_mass(bp.body, bp.mass);
    }
    h.prop_set_disable_coll(s!(p).cur_prop, true);
    if h.cb_exists() {
        let m = h.default_mass();
        h.cb_set_mass(m);
        h.set_interact_mul(1.0);
    }
    // SAFETY: caller contract.
    unsafe { reset_interact_vars(p, h) };
}

pub unsafe fn post_update(p: *mut Base, h: &Host, dt: f32) {
    let g = p.cast::<Grab>();
    let (pos, ang) = (h.cam_pos(), h.cam_angles());
    let (cur, gd) = (s!(p).cur_body, s!(g).data);
    let gp = GrabPost {
        cam_pos: pos,
        cam_rot: ang,
        body_matrix: h.body_matrix(cur, bd::M_LOCAL),
        body_pos: h.body_v3(cur, bd::V3_POS),
        body_ang_vel: h.body_v3(cur, bd::V3_ANG_VEL),
        inertia: h.body_matrix(cur, bd::M_INERTIA),
        body_rot: s!(g).body_rotation,
        local_offset: s!(g).local_body_offset,
        pos_offset: h.data_v3(gd, data::GRAB, data::G_POS_OFFSET),
        depth: s!(g).depth,
        max_distance: s!(g).max_distance,
        use_offset: u32::from(h.data_b(gd, data::GRAB, data::G_USE_OFFSET)),
        use_rotation: u32::from(h.data_b(gd, data::GRAB, data::G_USE_ROTATION)),
        mass_sum: s!(g).mass_sum,
        max_force: s!(g).max_force,
        max_torque: s!(g).max_torque,
        max_angular_speed: s!(g).max_angular_speed,
        force_mul: h.data_f(gd, data::GRAB, data::G_FORCE_MUL),
        torque_mul: h.data_f(gd, data::GRAB, data::G_TORQUE_MUL),
    };
    let mut go = GrabPostOut::default();
    let (fp, tp) = (&raw mut s!(g).force_pid, &raw mut s!(g).torque_pid);
    // SAFETY: both pids live in the grab state, are disjoint, and no host call runs meanwhile.
    sg::grab_post(&gp, unsafe { &mut *fp }, unsafe { &mut *tp }, dt, &mut go);
    if go.status == sg::GRAB_TOO_FAR {
        let prev = h.prev_state();
        h.change_state(prev);
        return;
    }
    h.body_add(cur, bd::ADD_FORCE, go.force, Vector3f::default());
    if go.status != sg::GRAB_FORCE_TORQUE {
        return;
    }
    h.body_add(cur, bd::ADD_TORQUE, go.torque, Vector3f::default());
}

pub unsafe fn do_action(p: *mut Base, h: &Host, raw: i32, pressed: bool) -> bool {
    let g = p.cast::<Grab>();
    match sb::interact_action(act_of(raw), pressed) {
        1 => {
            let (lin_max, ang_max) = (s!(g).max_leave_linear, s!(g).max_leave_angular);
            for bp in props(p) {
                let lin = h.body_v3(bp.body, bd::V3_LIN_VEL);
                if let Some(v) = sg::leave_limit(lin, lin_max) {
                    h.body_set_v3(bp.body, bd::V3_LIN_VEL, v);
                }
                let ang = h.body_v3(bp.body, bd::V3_ANG_VEL);
                if let Some(v) = sg::leave_limit(ang, ang_max) {
                    h.body_set_v3(bp.body, bd::V3_ANG_VEL, v);
                }
            }
            let prev = h.prev_state();
            h.change_state(prev);
            false
        }
        2 => {
            let cur = s!(p).cur_body;
            let impulse = h.data_f(s!(g).data, data::GRAB, data::G_THROW_IMPULSE);
            let fwd = h.cam_forward();
            h.body_set_v3(cur, bd::V3_LIN_VEL, Vector3f::default());
            h.body_set_v3(cur, bd::V3_ANG_VEL, Vector3f::default());
            h.body_add(
                cur,
                bd::ADD_IMPULSE,
                hmul3(fwd, impulse),
                Vector3f::default(),
            );
            let prev = h.prev_state();
            h.change_state(prev);
            false
        }
        _ => true,
    }
}

pub unsafe fn scroll(p: *mut Base, h: &Host, amount: f32) {
    let g = p.cast::<Grab>();
    let gd = s!(g).data;
    let (inc, min, max) = (
        h.data_f(gd, data::GRAB, data::G_DEPTH_INC),
        h.data_f(gd, data::GRAB, data::G_MIN_DEPTH),
        h.data_f(gd, data::GRAB, data::G_MAX_DEPTH),
    );
    s!(g).depth = sg::grab_scroll(s!(g).depth, amount, inc, min, max);
}

pub unsafe fn add_rotation(p: *mut Base, h: &Host, amount: f32, pitch: bool) -> bool {
    let g = p.cast::<Grab>();
    if !(h.in_rotate_triggered() && h.data_b(s!(g).data, data::GRAB, data::G_USE_ROTATION)) {
        return true;
    }
    let amount = if pitch && h.in_invert_pitch() {
        -amount
    } else {
        amount
    };
    let rot: Matrixf = sg::grab_rotate(&s!(g).body_rotation, pitch, amount);
    s!(g).body_rotation = rot;
    false
}

/// `(mass, collide_character)` saved for `body`, when it is one of the grabbed bodies.
pub fn save_body(p: *mut Base, body: Ptr) -> Option<(f32, bool)> {
    props(p)
        .iter()
        .find(|b| b.body == body)
        .map(|b| (b.mass, b.collide_character))
}

pub unsafe fn allow_buoyancy(p: *mut Base, h: &Host, body: Ptr) -> bool {
    h.body_user_data(body) != h.prop_as_entity(s!(p).cur_prop)
}
