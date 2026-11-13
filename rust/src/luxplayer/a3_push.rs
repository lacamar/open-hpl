//! `cLuxPlayerState_InteractPush`: push a body along the ground with the player's movement keys.

use super::a3_default::{act_of, reset_interact_vars, s, setup_interact_vars};
use super::a3_host::{Host, bd, cb, data};
use super::a3_state::{Base, MOVE_NORMAL, Push};
use super::states_base as sb;
use super::states_grab::{
    self as sg, PushBegin, PushBeginOut, PushDrive, PushDriveOut, PushEnterIn, PushEnterOut,
};
use super::states_vec::{hmul3, mat_vec};
use crate::math::to_rad;
use crate::types::Vector3f;

pub fn init(p: &mut Push, h: &Host) {
    let c = |k, d| h.cfg(c"Player_Interaction", k, d);
    p.walk_max_speed = c(c"PushMaxSpeedWalk", 0.0);
    p.walk_force = c(c"PushForceWalk", 0.0);
    p.run_max_speed = c(c"PushMaxSpeedRun", 0.0);
    p.run_force = c(c"PushForceRun", 0.0);
    p.crouch_max_speed = c(c"PushMaxSpeedCrouch", 0.0);
    p.crouch_force = c(c"PushForceCrouch", 0.0);
    p.yaw_range = to_rad(c(c"PushYawRange", 0.0));
    p.pitch_min_range = to_rad(c(c"PushPitchMinRange", 0.0));
    p.pitch_max_range = to_rad(c(c"PushPitchMaxRange", 0.0));
    p.max_force = c(c"PushMaxForce", 0.0);
    p.velocity_pid.init(20, 15.0, 0.0, 0.1);
    p.stop_pid.init(20, 10.0, 0.0, 0.1);
}

fn clear_move_vars(p: &mut Push) {
    p.total_move_mul = [0.0; 2];
    p.moving = [false; 2];
}

pub unsafe fn enter(b: *mut Base, h: &Host) {
    let p = b.cast::<Push>();
    // SAFETY: caller contract.
    unsafe { setup_interact_vars(b, h) };
    let (prop, cur) = (s!(b).cur_prop, s!(b).cur_body);
    let interact_pos = s!(b).cur_focus_pos;
    let pd = h.prop_data(prop, data::PUSH);
    s!(p).data = pd;
    clear_move_vars(s!(p));
    s!(p).velocity_pid.reset();
    s!(p).stop_pid.reset();
    s!(p).push_dir = Vector3f::default();
    h.cb_flag(cb::FLAG_DIAG_BOOST, false);
    let (min, max) = h.cam_pitch_limits();
    s!(p).prev_min_pitch = min;
    s!(p).prev_max_pitch = max;
    let ang = h.cam_angles();
    let pi = PushEnterIn {
        pitch: ang.x,
        yaw: ang.y,
        pitch_min_range: s!(p).pitch_min_range,
        pitch_max_range: s!(p).pitch_max_range,
        yaw_range: s!(p).yaw_range,
        at_point: u32::from(h.data_b(pd, data::PUSH, data::P_AT_POINT)),
        body_matrix: h.body_matrix(cur, bd::M_LOCAL),
        body_pos: h.body_v3(cur, bd::V3_POS),
        feet: h.cb_v3(cb::V3_FEET),
        interact_pos,
    };
    let mut po = PushEnterOut {
        local_attach: s!(p).local_attach,
        ..PushEnterOut::default()
    };
    sg::push_enter(&pi, &mut po);
    h.cam_set_pitch_limits(po.pitch_min, po.pitch_max);
    h.cam_set_yaw_limits(po.yaw_min, po.yaw_max);
    s!(p).local_attach = po.local_attach;
    s!(p).max_attach_dist = po.max_attach_dist;
}

pub unsafe fn leave(b: *mut Base, h: &Host) {
    let p = b.cast::<Push>();
    if h.cb_exists() {
        h.cb_flag(cb::FLAG_DIAG_BOOST, true);
    }
    h.cam_set_pitch_limits(s!(p).prev_min_pitch, s!(p).prev_max_pitch);
    h.cam_set_yaw_limits(0.0, 0.0);
    // SAFETY: caller contract.
    unsafe { reset_interact_vars(b, h) };
}

fn add_force(h: &Host, body: super::a3_host::Ptr, at_point: bool, f: Vector3f, at: Vector3f) {
    if at_point {
        h.body_add(body, bd::ADD_FORCE_AT, f, at);
    } else {
        h.body_add(body, bd::ADD_FORCE, f, Vector3f::default());
    }
}

pub unsafe fn update(b: *mut Base, h: &Host, dt: f32) {
    let p = b.cast::<Push>();
    let (cur, pd) = (s!(b).cur_body, s!(p).data);
    let at_point = h.data_b(pd, data::PUSH, data::P_AT_POINT);
    let pb = PushBegin {
        at_point: u32::from(at_point),
        body_matrix: h.body_matrix(cur, bd::M_LOCAL),
        local_attach: s!(p).local_attach,
        body_pos: h.body_v3(cur, bd::V3_POS),
        feet: h.cb_v3(cb::V3_FEET),
        max_attach_dist: s!(p).max_attach_dist,
        forward: h.cb_v3(cb::V3_FORWARD),
        right: h.cb_v3(cb::V3_RIGHT),
        moving: s!(p).moving.map(u32::from),
        total_move_mul: s!(p).total_move_mul,
    };
    let mut pbo = PushBeginOut::default();
    sg::push_begin(&pb, &mut pbo);
    if pbo.too_far != 0 {
        let prev = h.prev_state();
        h.change_state(prev);
        return;
    }
    let attach = pbo.attach;
    s!(p).push_dir = pbo.push_dir;
    let vel = if at_point {
        h.body_vel_at(cur, attach)
    } else {
        h.body_v3(cur, bd::V3_LIN_VEL)
    };
    let max_force = s!(p).max_force;
    if pbo.no_movement != 0 {
        let mass = h.body_mass(cur);
        let force = sg::push_stop(&mut s!(p).stop_pid, vel, mass, max_force, dt);
        add_force(h, cur, at_point, force, attach);
        return;
    }
    let normal = h.cur_move_state() == MOVE_NORMAL;
    let (mut running, mut crouching) = (false, false);
    if normal {
        running = h.move_normal_running();
        crouching = h.move_normal_crouching();
    }
    let (walk, run, crouch) = (
        (s!(p).walk_max_speed, s!(p).walk_force),
        (s!(p).run_max_speed, s!(p).run_force),
        (s!(p).crouch_max_speed, s!(p).crouch_force),
    );
    let pick = sg::push_pick(normal, running, crouching, walk, run, crouch);
    let drive = PushDrive {
        push_dir: s!(p).push_dir,
        vel,
        mass: h.body_mass(cur),
        max_force,
        max_speed: pick.0,
        push_force: pick.1,
        force_mul: h.data_f(pd, data::PUSH, data::P_FORCE_MUL),
        body_matrix: h.body_matrix(cur, bd::M_LOCAL),
        forward: h.cb_v3(cb::V3_FORWARD),
        right: h.cb_v3(cb::V3_RIGHT),
    };
    let mut pdo = PushDriveOut::default();
    sg::push_drive(&mut s!(p).velocity_pid, &drive, dt, &mut pdo);
    add_force(h, cur, at_point, pdo.correct_force, attach);
    if pdo.has_push_force != 0 {
        add_force(h, cur, at_point, pdo.push_force, attach);
    }
    let collide = h.cb_shape_collide(cur, h.cb_v3(cb::V3_POS), &pdo.next_transform);
    if !collide {
        for (i, v) in (0..).zip(pdo.max_speed) {
            h.cb_max_speed(i, v);
        }
    }
    for (i, mul) in (0..).zip(s!(p).total_move_mul) {
        if s!(p).moving.get(i as usize).copied().unwrap_or(false) {
            h.cb_move(i, mul);
        }
    }
}

pub unsafe fn post_update(b: *mut Base) {
    clear_move_vars(s!(b.cast::<Push>()));
}

pub unsafe fn do_action(b: *mut Base, h: &Host, raw: i32, pressed: bool) -> bool {
    let p = b.cast::<Push>();
    match sb::interact_action(act_of(raw), pressed) {
        1 => {
            let prev = h.prev_state();
            h.change_state(prev);
            false
        }
        2 => {
            let cur = s!(b).cur_body;
            let pd = s!(p).data;
            let at_point = h.data_b(pd, data::PUSH, data::P_AT_POINT);
            let impulse = h.data_f(pd, data::PUSH, data::P_IMPULSE);
            let push = hmul3(s!(p).push_dir, impulse);
            if at_point {
                let attach = mat_vec(&h.body_matrix(cur, bd::M_LOCAL), s!(p).local_attach);
                h.body_add(cur, bd::ADD_IMPULSE_AT, push, attach);
            } else {
                h.body_add(cur, bd::ADD_IMPULSE, push, Vector3f::default());
            }
            let prev = h.prev_state();
            h.change_state(prev);
            false
        }
        _ => true,
    }
}

pub unsafe fn on_move(b: *mut Base, dir: i32, mul: f32) {
    let p = s!(b.cast::<Push>());
    let i = usize::try_from(dir).unwrap_or(0);
    if let Some(v) = p.total_move_mul.get_mut(i) {
        *v += mul;
    }
    if let Some(v) = p.moving.get_mut(i) {
        *v = true;
    }
}
