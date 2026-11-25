//! `cLuxPlayer` dispatch (A2c): input routing, state switching, per-update order, map enter/leave, reset and the crosshair rule.
//! The flag/multiplier members live here (C++ keeps references into `Disp`); player and move state virtuals, helpers,
//! body, camera and GUI calls go through one host call. No reference is held across a host call: a state update may
//! call back (`ChangeState`, `Jump`, ...).

use super::a2_core::{
    Core, Host, update_camera, update_focus_alpha, update_head_spin, update_terror, vitals_run,
};
use super::camera;
use super::ffi::VITALS_SET_HEALTH;
use crate::types::Vector3f;
use std::collections::LinkedList;
use std::ffi::c_void;

pub mod k {
    pub const MS_ENTER: u32 = 1;
    pub const MS_LEAVE: u32 = 2;
    pub const MS_UPDATE: u32 = 3;
    pub const MS_ON_MOVE: u32 = 4;
    pub const MS_ON_YAW: u32 = 5;
    pub const MS_ON_PITCH: u32 = 6;
    pub const MS_ON_RUN: u32 = 7;
    pub const MS_ON_JUMP: u32 = 8;
    pub const MS_ON_CROUCH: u32 = 9;
    pub const MS_MAP_ENTER: u32 = 10;
    pub const MS_ON_DRAW: u32 = 11;
    pub const ST_ENTER: u32 = 20;
    pub const ST_LEAVE: u32 = 21;
    pub const ST_SET_PREV: u32 = 22;
    pub const ST_UPDATE: u32 = 23;
    pub const ST_POST_UPDATE: u32 = 24;
    pub const ST_ON_MOVE: u32 = 25;
    pub const ST_ON_YAW: u32 = 26;
    pub const ST_ON_PITCH: u32 = 27;
    pub const ST_ON_SCROLL: u32 = 28;
    pub const ST_ON_ACTION: u32 = 29;
    pub const ST_ON_RUN: u32 = 30;
    pub const ST_ON_JUMP: u32 = 31;
    pub const ST_ON_CROUCH: u32 = 32;
    pub const ST_MAP_ENTER: u32 = 33;
    pub const ST_MAP_LEAVE: u32 = 34;
    pub const ST_ON_DRAW: u32 = 35;
    pub const FREE_MOVE: u32 = 40;
    pub const BODY_MOVE: u32 = 41;
    pub const FREE_YAW: u32 = 42;
    pub const FREE_PITCH: u32 = 43;
    pub const YAW: u32 = 44;
    pub const PITCH: u32 = 45;
    pub const LANTERN_TOGGLE: u32 = 46;
    pub const COLLAPSE_ACTIVE: u32 = 47;
    pub const HEAD_POS_ADD: u32 = 48;
    pub const HELPERS_UPDATE: u32 = 49;
    pub const HELPERS_POST: u32 = 50;
    pub const HELPERS_DRAW: u32 = 51;
    pub const HELPERS_LUX_MSG: u32 = 52;
    pub const COLLISION_CB: u32 = 53;
    pub const BODY_Y: u32 = 54;
    pub const BODY_SPEED: u32 = 55;
    pub const CLEAR_FOCUS_TEXT: u32 = 56;
    pub const NULL_FOCUS: u32 = 57;
    pub const DESTROY_CB: u32 = 58;
    pub const CLEAR_TERROR: u32 = 59;
    pub const CAM_MAP_LEAVE: u32 = 60;
    pub const PAUSE_ACTIVE: u32 = 61;
    pub const FOCUS_CAN_INTERACT: u32 = 62;
    pub const DRAW_CROSSHAIR: u32 = 63;
    pub const FOCUS_TEXT_STATE: u32 = 64;
    pub const DRAW_FOCUS_TEXT: u32 = 65;
    pub const FREECAM_CAMERA: u32 = 66;
    pub const SHOW_CROSSHAIR: u32 = 67;
}

pub const ST_NORMAL: i32 = 0;
pub const ST_USE_ITEM: i32 = 2;
pub const ST_GRAB: i32 = 3;
pub const ST_PUSH: i32 = 4;
pub const ST_LEVER: i32 = 6;
pub const ST_WHEEL: i32 = 7;
pub const ST_SLIDE: i32 = 8;
pub const ST_LAST: i32 = 10;
pub const MS_NORMAL: i32 = 0;
pub const MS_LAST: i32 = 2;
pub(super) const ONE: f32 = 1.0;

#[repr(C)]
pub struct DHost {
    pub ctx: *mut c_void,
    pub call: extern "C" fn(*mut c_void, u32, i32, f32, f32, *mut c_void) -> f32,
}

impl DHost {
    pub(super) fn c(&self, kind: u32, i: i32, a: f32, b: f32) -> f32 {
        (self.call)(self.ctx, kind, i, a, b, std::ptr::null_mut())
    }

    pub(super) fn cb(&self, kind: u32, i: i32, a: f32, b: f32) -> bool {
        self.c(kind, i, a, b) != 0.0
    }

    pub(super) fn cp(&self, kind: u32, i: i32, p: *mut c_void) {
        (self.call)(self.ctx, kind, i, 0.0, 0.0, p);
    }
}

#[repr(C)]
pub struct Disp {
    ring: *mut LinkedList<f32>,
    pub state: i32,
    pub move_state: i32,
    pub coins: i32,
    pub tinderboxes: i32,
    pub max_prev_speeds: i32,
    pub water_speed_mul: f32,
    pub interaction_move_speed_mul: f32,
    pub look_speed_mul: f32,
    pub event_move_speed_mul: f32,
    pub event_run_speed_mul: f32,
    pub script_move_speed_mul: f32,
    pub script_run_speed_mul: f32,
    pub hurt_move_speed_mul: f32,
    pub insanity_collapse_speed_mul: f32,
    pub script_jump_force_mul: f32,
    pub current_focus_distance: f32,
    pub free_cam_speed: f32,
    pub avg_speed: f32,
    pub auto_kill_y: f32,
    pub head_pos_add_sum: Vector3f,
    pub active: bool,
    pub no_fall_damage: bool,
    pub uses_drag_footsteps: bool,
    pub pressed_move: bool,
    pub pressing_run: bool,
    pub pressing_jump: bool,
    pub is_in_water: bool,
    pub jump_disabled: bool,
    pub crouch_disabled: bool,
    pub sanity_drain_disabled: bool,
    pub free_cam: bool,
    pub script_show_focus: bool,
}

const _: () = assert!(std::mem::size_of::<Disp>() == 112);

/// # Safety
/// `d` valid.
pub(super) unsafe fn ring<'a, T>(slot: &mut *mut LinkedList<T>) -> Option<&'a mut LinkedList<T>> {
    if slot.is_null() {
        *slot = Box::into_raw(Box::default());
    }
    unsafe { slot.as_mut() }
}

/// # Safety
/// `d` valid, zeroed or initialised by a previous call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2d_init(d: *mut Disp, max_prev_speeds: i32, auto_kill_y: f32) {
    // SAFETY: caller contract.
    unsafe {
        (*d).max_prev_speeds = max_prev_speeds;
        (*d).auto_kill_y = auto_kill_y;
        (*d).state = ST_NORMAL;
        (*d).move_state = MS_NORMAL;
    }
}

/// # Safety
/// `d` was initialised and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2d_free(d: *mut Disp) {
    // SAFETY: caller contract.
    unsafe {
        if !(*d).ring.is_null() {
            drop(Box::from_raw((*d).ring));
            (*d).ring = std::ptr::null_mut();
        }
    }
}

/// # Safety
/// All pointers valid; `h`/`dh` may call back into the player.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2d_reset(c: *mut Core, d: *mut Disp, dh: *const DHost) {
    // SAFETY: caller contract; fields are written one at a time, no reference lives across a host call.
    unsafe {
        let dh = &*dh;
        (*d).active = true;
        (*d).move_state = MS_NORMAL;
        dh.c(k::MS_ENTER, MS_NORMAL, MS_LAST as f32, 0.0);
        (*d).state = ST_NORMAL;
        dh.c(k::ST_ENTER, ST_NORMAL, ST_LAST as f32, 0.0);
        (*d).pressed_move = false;
        (*d).pressing_run = false;
        (*d).pressing_jump = false;
        (*d).jump_disabled = false;
        (*d).crouch_disabled = false;
        (*d).sanity_drain_disabled = false;
        (*d).is_in_water = false;
        (*d).water_speed_mul = ONE;
        (*c).head_spin = Default::default();
        (*d).interaction_move_speed_mul = ONE;
        (*d).look_speed_mul = ONE;
        (*d).event_move_speed_mul = ONE;
        (*d).event_run_speed_mul = ONE;
        (*d).script_move_speed_mul = ONE;
        (*d).script_run_speed_mul = ONE;
        (*d).hurt_move_speed_mul = ONE;
        (*d).insanity_collapse_speed_mul = ONE;
        (*d).script_jump_force_mul = ONE;
        (*d).current_focus_distance = 0.0;
        (*c).health = 100.0;
        (*c).sanity = 100.0;
        (*c).lamp_oil = 100.0;
        (*d).tinderboxes = 0;
        (*d).head_pos_add_sum = Vector3f::default();
        (*c).terror = 0.0;
        (*d).coins = 0;
        (*d).avg_speed = 0.0;
        if let Some(r) = (*d).ring.as_mut() {
            r.clear();
        }
        (*c).focus_alpha = 0.0;
        let m = &mut (*c).cam;
        m.aspect_mul = ONE;
        m.fov_mul = ONE;
        m.aspect_mul_goal = ONE;
        m.fov_mul_goal = ONE;
        m.aspect_mul_speed = 0.0;
        m.fov_mul_speed = 0.0;
        m.roll = 0.0;
        m.roll_goal = 0.0;
        m.roll_speed_mul = 0.0;
        m.roll_max_speed = 0.0;
        m.lean_roll = 0.0;
        m.lean_roll_goal = 0.0;
        m.lean_roll_speed_mul = 0.0;
        m.lean_roll_max_speed = 0.0;
        m.cam_anim_pos = Vector3f::default();
        m.cam_anim_pos_goal = Vector3f::default();
        m.fading_pitch = 0;
        m.pitch_goal = 0.0;
        m.pitch_speed_mul = 0.0;
        m.pitch_max_speed = 0.0;
        (*d).uses_drag_footsteps = false;
        (*d).no_fall_damage = false;
        (*d).script_show_focus = true;
        (*d).free_cam = false;
        (*d).free_cam_speed = 0.1;
    }
}

/// # Safety
/// As `ohpl_a2d_reset`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2d_update(
    c: *mut Core,
    d: *mut Disp,
    h: *const Host,
    dh: *const DHost,
    dt: f32,
) {
    // SAFETY: caller contract.
    unsafe {
        let (h, dh) = (&*h, &*dh);
        dh.c(k::MS_UPDATE, (*d).move_state, dt, 0.0);
        dh.c(k::ST_UPDATE, (*d).state, dt, 0.0);
        dh.c(k::HEAD_POS_ADD, 0, dt, 0.0);
        update_head_spin(c, h, dt);
        dh.c(k::HELPERS_UPDATE, 0, dt, 0.0);
        (*d).pressed_move = false;
        if dh.c(k::BODY_Y, 0, 0.0, 0.0) < (*d).auto_kill_y {
            vitals_run(c, h, VITALS_SET_HEALTH, 0.0, 0, 0);
        }
        dh.c(k::COLLISION_CB, 0, 0.0, 0.0);
        update_camera(c, h, dt);
        update_terror(c, h, dt);
        update_focus_alpha(c, h, dt);
        let speed = dh.c(k::BODY_SPEED, 0, dt, 0.0);
        if let Some(r) = ring(&mut (*d).ring) {
            r.push_back(speed);
            if camera::prev_speeds_pop(r.len(), (*d).max_prev_speeds) {
                r.pop_front();
            }
            let v: Vec<f32> = r.iter().copied().collect();
            (*d).avg_speed = camera::avg_speed(&v);
        }
    }
}

/// # Safety
/// As `ohpl_a2d_reset`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2d_post_update(d: *mut Disp, dh: *const DHost, dt: f32) {
    // SAFETY: caller contract.
    unsafe {
        let dh = &*dh;
        dh.c(k::HELPERS_POST, 0, dt, 0.0);
        dh.c(k::ST_POST_UPDATE, (*d).state, dt, 0.0);
    }
}
