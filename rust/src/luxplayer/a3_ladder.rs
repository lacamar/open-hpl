//! `cLuxPlayerState_Ladder`: attach to a ladder area, climb with the move keys, leave at either end.

use super::a3_default::{act_of, s};
use super::a3_host::{Host, cb};
use super::a3_state::{Base, Ladder, MOVE_NORMAL, ps};
use super::states_base as sb;
use super::states_ladder::{
    self as sl, LadderAttachOut, LadderEnterIn, LadderEnterOut, LadderState,
};
use crate::types::Vector2f;

const SOUND_ATTACH: i32 = 0;
const SOUND_UP: i32 = 1;
const SOUND_DOWN: i32 = 2;
const SOUND_DETTACH: i32 = 3;

const LADDER_MAX_Y: i32 = 0;
const LADDER_MIN_Y: i32 = 1;
const LADDER_FORWARD: i32 = 2;
const LADDER_START_ROT: i32 = 3;

pub fn init(l: &mut Ladder, h: &Host) {
    l.ladder = std::ptr::null_mut();
    l.up_speed = h.cfg(c"Player_Ladder", c"UpSpeed", 0.0);
    l.down_speed = h.cfg(c"Player_Ladder", c"DownSpeed", 0.0);
    l.step_length = h.cfg(c"Player_Ladder", c"StepLength", 0.0);
    l.step_count = 0.0;
    l.state = 0;
}

fn to_rust(l: &Ladder) -> LadderState {
    LadderState {
        state: l.state,
        time_count: l.time_count,
        step_count: l.step_count,
        move_mul: l.move_mul,
        leave_at_top: l.leave_at_top,
        played_sound: u32::from(l.played_sound),
        char_pos: l.char_position,
        pos_add: l.pos_add,
        rot_add: Vector2f {
            x: l.rot_add.x,
            y: l.rot_add.y,
        },
    }
}

fn from_rust(l: &mut Ladder, s: &LadderState) {
    l.time_count = s.time_count;
    l.step_count = s.step_count;
    l.move_mul = s.move_mul;
    l.leave_at_top = s.leave_at_top;
    l.played_sound = s.played_sound != 0;
    l.char_position = s.char_pos;
}

fn play(h: &Host, l: *mut Ladder, code: i32) {
    match code {
        sl::SOUND_UP => h.ladder_sound(s!(l).ladder, SOUND_UP),
        sl::SOUND_DOWN => h.ladder_sound(s!(l).ladder, SOUND_DOWN),
        _ => {}
    }
}

fn setup_head_turn_limits(h: &Host) {
    let a = h.cam_angles();
    let lim = sl::ladder_head_limits(a.y, a.x);
    h.cam_set_pitch_limits(lim[0], lim[1]);
    h.cam_set_yaw_limits(lim[2], lim[3]);
}

pub unsafe fn enter(b: *mut Base, h: &Host) {
    let l = b.cast::<Ladder>();
    let (ladder, start) = h.vars_ladder();
    s!(l).ladder = ladder;
    s!(l).start_position = start;
    h.change_move_state(MOVE_NORMAL);
    h.cb_flag(cb::FLAG_GRAVITY, false);
    h.cb_flag(cb::FLAG_TEST_COLLISION, false);
    h.cb_cmd(cb::STOP_MOVEMENT);
    h.cb_cmd(cb::RESET_CLIMBING);
    h.move_normal_uncrouch();
    h.move_normal_reset_jumping();
    s!(l).lantern_drawn = h.lantern_active();
    h.lantern_set(false);
    s!(l).state = 0;
    s!(l).step_count = 0.0;
    s!(l).played_sound = false;
    s!(l).move_mul = 0.0;
    let (min, max) = h.cam_pitch_limits();
    s!(l).pitch_min_limit = min;
    s!(l).pitch_max_limit = max;
    let ang = h.cam_angles();
    let start_rot = h.ladder_v3(ladder, LADDER_START_ROT);
    let li = LadderEnterIn {
        pos: h.cb_v3(cb::V3_POS),
        start,
        max_y: h.ladder_y(ladder, LADDER_MAX_Y),
        start_yaw: start_rot.y,
        pitch: ang.x,
        yaw: ang.y,
    };
    let mut lo = LadderEnterOut::default();
    sl::ladder_enter(&li, &mut lo);
    s!(l).time_count = lo.time;
    s!(l).goal_pos = start;
    s!(l).pos_add = lo.pos_add;
    s!(l).char_position = h.cb_v3(cb::V3_POS);
    s!(l).goal_rot.x = 0.0;
    s!(l).goal_rot.y = start_rot.y;
    s!(l).rot_add.x = lo.rot_add.x;
    s!(l).rot_add.y = lo.rot_add.y;
    h.ladder_sound(ladder, SOUND_ATTACH);
}

pub unsafe fn leave(b: *mut Base, h: &Host) {
    let l = b.cast::<Ladder>();
    if h.cb_exists() {
        h.cb_flag(cb::FLAG_GRAVITY, true);
        h.cb_flag(cb::FLAG_TEST_COLLISION, true);
    }
    h.cam_set_pitch_limits(s!(l).pitch_min_limit, s!(l).pitch_max_limit);
    h.cam_set_yaw_limits(0.0, 0.0);
    if s!(l).lantern_drawn {
        h.lantern_set(true);
    }
}

pub unsafe fn update(b: *mut Base, h: &Host, dt: f32) {
    let l = b.cast::<Ladder>();
    match s!(l).state {
        0 => {
            let mut ls = to_rust(s!(l));
            let mut ao = LadderAttachOut::default();
            sl::ladder_attach(&mut ls, dt, &mut ao);
            from_rust(s!(l), &ls);
            h.cb_set_pos(s!(l).char_position);
            s!(l).char_position = h.cb_v3(cb::V3_POS);
            h.cam_add_pitch(ao.pitch_add);
            h.cam_add_yaw(ao.yaw_add);
            h.cb_set_yaw(h.cam_angles().y);
            if ao.done != 0 {
                h.cb_flag(cb::FLAG_TEST_COLLISION, true);
                setup_head_turn_limits(h);
                s!(l).state += 1;
            }
        }
        1 => {
            let pos = h.cb_v3(cb::V3_POS);
            let (up, down) = (s!(l).up_speed, s!(l).down_speed);
            let mut ls = to_rust(s!(l));
            let sound = sl::ladder_move_pre(&mut ls, pos, up, down, dt);
            from_rust(s!(l), &ls);
            play(h, l, sound);
            if !h.cb_fits(s!(l).char_position) {
                s!(l).move_mul = 0.0;
                return;
            }
            let mut ls = to_rust(s!(l));
            let sound = sl::ladder_move_post(&mut ls, up, down, s!(l).step_length, dt);
            from_rust(s!(l), &ls);
            play(h, l, sound);
            h.cb_set_pos(s!(l).char_position);
            let feet = h.cb_v3(cb::V3_FEET);
            let (ladder, size) = (s!(l).ladder, h.cb_v3(cb::V3_SIZE));
            let rays = sl::ladder_ground_rays(feet, h.ladder_v3(ladder, LADDER_FORWARD), size.x);
            let (mm, y) = (s!(l).move_mul, s!(l).char_position.y);
            if sl::ladder_at_top(mm, y, size.y, h.ladder_y(ladder, LADDER_MAX_Y)) {
                s!(l).state = 2;
                s!(l).leave_at_top = 2.0;
                h.ladder_sound(ladder, SOUND_DETTACH);
            } else {
                let bottom = sl::ladder_at_bottom(mm, y, size.y, h.ladder_y(ladder, LADDER_MIN_Y));
                let [r0, r1, r2, r3] = rays;
                if bottom == sl::BOTTOM_YES
                    || (bottom == sl::BOTTOM_CHECK_RAYS && (h.cb_ray(r0, r1) || h.cb_ray(r2, r3)))
                {
                    h.change_state(ps::NORMAL);
                }
            }
            s!(l).move_mul = 0.0;
        }
        2 => {
            h.cb_move(0, 1.0);
            let feet = h.cb_v3(cb::V3_FEET);
            let [r0, r1] = sl::ladder_top_rays(feet);
            let timeout = sl::ladder_leave_tick(&mut s!(l).leave_at_top, dt);
            if h.cb_ray(r0, r1) || timeout {
                h.change_state(ps::NORMAL);
            }
        }
        _ => {}
    }
}

pub unsafe fn on_move(b: *mut Base, dir: i32, mul: f32) {
    let l = b.cast::<Ladder>();
    let state = s!(l).state;
    sl::ladder_on_move(state, dir == 1, mul, &mut s!(l).move_mul);
}

pub unsafe fn camera_free(b: *mut Base) -> bool {
    sl::ladder_camera_free(s!(b.cast::<Ladder>()).state)
}

pub unsafe fn on_jump(h: &Host, pressed: bool) {
    if pressed {
        h.change_state(ps::NORMAL);
        h.move_normal_jump();
    }
}

pub unsafe fn do_action(h: &Host, raw: i32, pressed: bool) -> bool {
    let (release, pass) = sb::ladder_action(act_of(raw), pressed);
    if release {
        h.change_state(ps::NORMAL);
    }
    pass
}
