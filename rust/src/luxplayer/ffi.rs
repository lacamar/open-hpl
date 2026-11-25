//! C ABI for the player core, mirrored in `amnesia/src/game/RustLuxPlayer.h`.

use super::camera::{self, CamState, HeadPosAdd};
use super::helpers::{CamDir, Collapse, Darkness, Death, Flashback, Hurt, Lantern, LookAt};
use super::ops::{Op, Ops};
use super::sanity::Sanity;
use super::vitals::Vitals;
use crate::luxprop::ops::{rd, rw};
use crate::types::Vector3f;

const _: () = {
    use std::mem::{offset_of, size_of};
    assert!(size_of::<Op>() == 24 && offset_of!(Op, a) == 8);
    assert!(size_of::<Ops>() == 388 && offset_of!(Ops, ops) == 4);
    assert!(size_of::<Vitals>() == 16);
    assert!(size_of::<HeadPosAdd>() == 36 && offset_of!(HeadPosAdd, add) == 24);
    assert!(size_of::<Collapse>() == 60);
    assert!(size_of::<CamDir>() == 32);
    assert!(size_of::<Hurt>() == 56);
    assert!(size_of::<LookAt>() == 20);
    assert!(size_of::<Flashback>() == 28);
    assert!(size_of::<Lantern>() == 32);
    assert!(size_of::<Darkness>() == 36);
    assert!(size_of::<Death>() == 64);
    assert!(size_of::<Sanity>() == 128);
    assert!(size_of::<CamState>() == 128);
};

pub const VITALS_GIVE_DAMAGE: i32 = 0;
pub const VITALS_SET_HEALTH: i32 = 1;
pub const VITALS_SET_SANITY: i32 = 2;
pub const VITALS_LOWER_SANITY: i32 = 3;
pub const VITALS_ADD_HEALTH: i32 = 4;
pub const VITALS_ADD_SANITY: i32 = 5;
pub const VITALS_ADD_LAMP_OIL: i32 = 6;

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga2_clamp_pitch_goal(x: f32, min: f32, max: f32) -> f32 {
    camera::clamp_pitch_goal(x, min, max)
}

/// # Safety
/// `adds` points to `n` exclusively borrowed entries; the other pointers are valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga2_update_head_pos_add(
    adds: *mut HeadPosAdd,
    n: usize,
    camera_pos_add: *const Vector3f,
    dt: f32,
    total: *mut Vector3f,
    sum: *mut Vector3f,
) {
    let a: &mut [HeadPosAdd] = if n == 0 {
        &mut []
    } else {
        // SAFETY: caller contract.
        unsafe { std::slice::from_raw_parts_mut(adds, n) }
    };
    let (t, s) = camera::update_head_pos_add(a, *rd(camera_pos_add), dt);
    *rw(total) = t;
    *rw(sum) = s;
}
