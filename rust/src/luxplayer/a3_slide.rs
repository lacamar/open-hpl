//! `cLuxPlayerState_InteractSlide`: drag a body along its slider joint with the mouse.

use super::a3_default::{act_of, reset_interact_vars, s, setup_interact_vars};
use super::a3_host::{Host, bd, data};
use super::a3_state::{Base, Slide};
use super::states_base as sb;
use super::states_grab::{self as sg, SlideIn, SlideOut};
use super::states_rotate::move_to_mouse;
use super::states_vec::{dist3, mat_vec};
use crate::math::matrix_inverse;
use crate::types::{Vector2f, Vector3f};

pub fn init(p: &mut Slide, h: &Host) {
    let c = |k, d| h.cfg(c"Player_Interaction", k, d);
    p.move_to_mouse_factor = c(c"MoveToMouseAddFactor", 0.01);
    p.max_force = c(c"SlideMaxForce", 1000.0);
    p.force_pid.init(10, 6.0, 0.0, 0.1);
}

pub unsafe fn enter(b: *mut Base, h: &Host) {
    let p = b.cast::<Slide>();
    // SAFETY: caller contract.
    unsafe { setup_interact_vars(b, h) };
    let (prop, cur) = (s!(b).cur_prop, s!(b).cur_body);
    s!(p).data = h.prop_data(prop, data::SLIDE);
    s!(p).joint = h.body_joint(cur, 0);
    s!(p).force_pid.reset();
    s!(p).mouse_add = Vector2f::default();
    s!(p).last_mouse_add = Vector2f::default();
    s!(p).slide_speed = 0.0;
    s!(p).has_gravity = h.body_flag(cur, bd::FLAG_GRAVITY);
    h.body_set_flag(cur, bd::FLAG_GRAVITY, false);
    let inv = matrix_inverse(&h.body_matrix(cur, bd::M_LOCAL));
    let focus = s!(b).cur_focus_pos;
    s!(p).local_interact_pos = mat_vec(&inv, focus);
    s!(p).max_distance = dist3(h.cam_pos(), focus) * 1.4;
}

pub unsafe fn leave(b: *mut Base, h: &Host) {
    let p = b.cast::<Slide>();
    h.body_set_flag(s!(b).cur_body, bd::FLAG_GRAVITY, s!(p).has_gravity);
    // SAFETY: caller contract.
    unsafe { reset_interact_vars(b, h) };
}

pub unsafe fn update(b: *mut Base, h: &Host, dt: f32) {
    let p = b.cast::<Slide>();
    let (cur, sd, joint) = (s!(b).cur_body, s!(p).data, s!(p).joint);
    let si = SlideIn {
        cam_pos: h.cam_pos(),
        up: h.cam_up(),
        right: h.cam_right(),
        body_matrix: h.body_matrix(cur, bd::M_LOCAL),
        local_interact: s!(p).local_interact_pos,
        mouse: s!(p).mouse_add,
        pin: h.joint_pin(joint),
        vel: h.body_v3(cur, bd::V3_LIN_VEL),
        mass: h.body_mass(cur),
        max_distance: s!(p).max_distance,
        slow_down_factor: h.data_f(sd, data::SLIDE, data::S_SLOW_DOWN),
        speed_factor: h.data_f(sd, data::SLIDE, data::S_SPEED_FACTOR),
        max_speed: h.data_f(sd, data::SLIDE, data::S_MAX_SPEED),
        max_force: s!(p).max_force,
    };
    let mut so = SlideOut::default();
    let (speed, pid) = (&raw mut s!(p).slide_speed, &raw mut s!(p).force_pid);
    // SAFETY: disjoint fields of the slide state, and no host call runs meanwhile.
    sg::slide_update(
        &si,
        unsafe { &mut *speed },
        unsafe { &mut *pid },
        dt,
        &mut so,
    );
    if so.out_of_range != 0 {
        let prev = h.prev_state();
        h.change_state(prev);
    }
    h.body_add(cur, bd::ADD_FORCE, so.force, Vector3f::default());
    s!(p).last_force = so.force;
    s!(p).last_mouse_add = s!(p).mouse_add;
    s!(p).mouse_add = Vector2f::default();
}

pub unsafe fn do_action(b: *mut Base, h: &Host, raw: i32, pressed: bool) -> bool {
    let p = b.cast::<Slide>();
    match sb::interact_action(act_of(raw), pressed) {
        1 => {
            let prev = h.prev_state();
            h.change_state(prev);
            false
        }
        2 => {
            let pin = h.joint_pin(s!(p).joint);
            let fwd = h.cam_forward();
            let impulse = sg::slide_throw(
                pin,
                fwd,
                h.data_f(s!(p).data, data::SLIDE, data::S_THROW_IMPULSE),
            );
            h.body_add(
                s!(b).cur_body,
                bd::ADD_IMPULSE,
                impulse,
                Vector3f::default(),
            );
            false
        }
        _ => true,
    }
}

pub unsafe fn add_yaw(b: *mut Base, amount: f32) {
    s!(b.cast::<Slide>()).mouse_add.x += amount;
}

pub unsafe fn add_pitch(b: *mut Base, h: &Host, amount: f32) {
    let amount = if h.in_invert_pitch() { -amount } else { amount };
    s!(b.cast::<Slide>()).mouse_add.y += amount;
}

pub unsafe fn on_move(b: *mut Base, dir: i32, mul: f32) {
    let p = b.cast::<Slide>();
    let factor = s!(p).move_to_mouse_factor;
    move_to_mouse(dir == 0, mul, factor, &mut s!(p).mouse_add);
}
