//! `DrawDebug` of the state family: which rows the debug HUD shows for each state and the y they advance.

use super::a3_default::s;
use super::a3_host::{Host, Ptr, bd};
use super::a3_state::{Base, Kind, Push, Rotate, Slide};
use super::states_vec::{dot, hmul3};
use crate::types::Vector3f;

const FOCUS_BODY: i32 = 0;
const CURRENT_VEL: i32 = 1;
const WANTED_VEL: i32 = 2;
const SLIDE_SPEED: i32 = 3;
const ROT_SPEED: i32 = 4;
const ROT_ANGLE: i32 = 5;
const ANGULAR_VEL: i32 = 6;
const PROP_DEBUG: i32 = 7;

pub unsafe fn draw_debug(p: *mut Base, h: &Host, kind: Kind, set: Ptr, font: Ptr, y: f32) -> f32 {
    let none = std::ptr::null_mut();
    let zero = Vector3f::default();
    let row = |row: i32, y: f32, obj: Ptr, v: Vector3f, f: f32| {
        h.debug_row((set, font), row, y, obj, v, f)
    };
    let mut y = y;
    match kind {
        Kind::Normal | Kind::HandObject | Kind::UseItem => {
            row(
                FOCUS_BODY,
                y,
                s!(p).body_in_focus,
                zero,
                s!(p).focus_distance,
            );
            y += 15.0;
        }
        Kind::Push => {
            let vel = h.body_v3(s!(p).cur_body, bd::V3_LIN_VEL);
            let dir = s!(p.cast::<Push>()).push_dir;
            let wanted = hmul3(dir, dot(dir, vel));
            row(CURRENT_VEL, y, none, vel, 0.0);
            y += 13.0;
            row(WANTED_VEL, y, none, wanted, 0.0);
            y += 13.0;
        }
        Kind::Slide => {
            let sl = p.cast::<Slide>();
            row(SLIDE_SPEED, y, none, s!(sl).last_force, s!(sl).slide_speed);
            y += 13.0;
        }
        Kind::SwingDoor | Kind::Lever | Kind::Wheel => {
            let (r, cur) = (p.cast::<Rotate>(), s!(p).cur_body);
            row(ROT_SPEED, y, none, zero, s!(r).rot_speed);
            y += 13.0;
            row(ROT_ANGLE, y, s!(r).joint, zero, 0.0);
            y += 13.0;
            row(ANGULAR_VEL, y, none, h.body_v3(cur, bd::V3_ANG_VEL), 0.0);
            y += 13.0;
            y = row(PROP_DEBUG, y, s!(p).cur_prop, zero, 0.0);
        }
        Kind::Grab | Kind::Ladder => {}
    }
    y
}
