//! `cLuxPlayerState_Ladder`: attach timing, climb steps and sounds, top and bottom detection,
//! ground and top rays and the head-turn limits.

use super::states_vec::madd;
use crate::math::{div3, fma, to_rad, v3, wrap};
use crate::types::{Vector2f, Vector3f};

const TWO_PI: f32 = std::f32::consts::TAU;

pub const SOUND_NONE: i32 = 0;
pub const SOUND_UP: i32 = 1;
pub const SOUND_DOWN: i32 = 2;

fn angle_distance(a1: f32, a2: f32, max_angle: f32) -> f32 {
    let a1 = wrap(a1, 0.0, max_angle);
    let a2 = wrap(a2, 0.0, max_angle);
    if a1 == a2 {
        return 0.0;
    }
    let d1 = a2 - a1;
    let mut d2 = max_angle - d1.abs();
    if d1 > 0.0 {
        d2 = -d2;
    }
    if d1.abs() < d2.abs() { d1 } else { d2 }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct LadderEnterIn {
    pub pos: Vector3f,
    pub start: Vector3f,
    pub max_y: f32,
    pub start_yaw: f32,
    pub pitch: f32,
    pub yaw: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct LadderEnterOut {
    pub time: f32,
    pub pos_add: Vector3f,
    pub rot_add: Vector2f,
}

pub fn ladder_enter(i: &LadderEnterIn, o: &mut LadderEnterOut) {
    let time = if i.pos.y > i.max_y { 1.2f32 } else { 0.5f32 };
    o.time = time;
    o.pos_add = div3(
        v3(
            i.start.x - i.pos.x,
            i.start.y - i.pos.y,
            i.start.z - i.pos.z,
        ),
        time,
    );
    o.rot_add = Vector2f {
        x: angle_distance(i.pitch, 0.0, TWO_PI) / time,
        y: angle_distance(i.yaw, i.start_yaw, TWO_PI) / time,
    };
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct LadderState {
    pub state: i32,
    pub time_count: f32,
    pub step_count: f32,
    pub move_mul: f32,
    pub leave_at_top: f32,
    pub played_sound: u32,
    pub char_pos: Vector3f,
    pub pos_add: Vector3f,
    pub rot_add: Vector2f,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct LadderAttachOut {
    pub pitch_add: f32,
    pub yaw_add: f32,
    pub done: u32,
}

/// State 0 step: the new character position is `s.char_pos`.
pub fn ladder_attach(s: &mut LadderState, dt: f32, o: &mut LadderAttachOut) {
    s.time_count -= dt;
    s.char_pos = madd(s.pos_add, dt, s.char_pos);
    o.pitch_add = s.rot_add.x * dt;
    o.yaw_add = s.rot_add.y * dt;
    o.done = u32::from(s.time_count <= 0.0);
}

/// State 1 before the fit test: moves `char_pos` in y and picks the idle step sound.
pub fn ladder_move_pre(s: &mut LadderState, pos: Vector3f, up: f32, down: f32, dt: f32) -> i32 {
    s.char_pos = pos;
    let mut sound = SOUND_NONE;
    if s.move_mul > 0.0 {
        s.char_pos.y = fma(s.move_mul * up, dt, s.char_pos.y);
        if s.step_count < 0.0 {
            s.step_count = 0.0;
        }
    } else if s.move_mul < 0.0 {
        s.char_pos.y = fma(s.move_mul * down, dt, s.char_pos.y);
        if s.step_count > 0.0 {
            s.step_count = 0.0;
        }
    } else {
        if s.played_sound == 0 {
            if s.step_count > 0.0 {
                sound = SOUND_UP;
            }
            if s.step_count < 0.0 {
                sound = SOUND_DOWN;
            }
        }
        s.step_count = 0.0;
        s.played_sound = 0;
    }
    sound
}

/// State 1 after the fit test passed: step counting and the climb sound.
pub fn ladder_move_post(s: &mut LadderState, up: f32, down: f32, step_len: f32, dt: f32) -> i32 {
    if s.move_mul > 0.0 {
        s.step_count = fma(s.move_mul * up, dt, s.step_count);
        if s.step_count >= step_len {
            s.step_count = 0.0;
            s.played_sound = 1;
            return SOUND_UP;
        }
    } else if s.move_mul < 0.0 {
        s.step_count = fma(s.move_mul * down, dt, s.step_count);
        if s.step_count <= -step_len {
            s.step_count = 0.0;
            s.played_sound = 1;
            return SOUND_DOWN;
        }
    }
    SOUND_NONE
}

/// Ground rays `[start1, end1, start2, end2]` used when climbing down.
pub fn ladder_ground_rays(feet: Vector3f, ladder_fwd: Vector3f, size_x: f32) -> [Vector3f; 4] {
    let lift = v3(feet.x + 0.0, feet.y + 0.05, feet.z + 0.0);
    let s1 = madd(
        v3(
            ladder_fwd.x * size_x,
            ladder_fwd.y * size_x,
            ladder_fwd.z * size_x,
        ),
        0.7,
        lift,
    );
    let e1 = v3(s1.x - 0.0, s1.y - 0.1, s1.z - 0.0);
    let e2 = v3(lift.x - 0.0, lift.y - 0.1, lift.z - 0.0);
    [s1, e1, lift, e2]
}

pub fn ladder_at_top(move_mul: f32, y: f32, size_y: f32, max_y: f32) -> bool {
    move_mul > 0.0 && (y - size_y * 0.5) > max_y
}

pub const BOTTOM_NO: i32 = 0;
pub const BOTTOM_YES: i32 = 1;
pub const BOTTOM_CHECK_RAYS: i32 = 2;

/// Bottom test; the two ray queries run only for `BOTTOM_CHECK_RAYS`.
pub fn ladder_at_bottom(move_mul: f32, y: f32, size_y: f32, min_y: f32) -> i32 {
    if move_mul < 0.0 {
        if (y - size_y / 2.0) < min_y {
            BOTTOM_YES
        } else {
            BOTTOM_CHECK_RAYS
        }
    } else {
        BOTTOM_NO
    }
}

/// Rays `[start, end]` checked while leaving at the top.
pub fn ladder_top_rays(feet: Vector3f) -> [Vector3f; 2] {
    [
        v3(feet.x + 0.0, feet.y + 0.1, feet.z + 0.0),
        v3(feet.x - 0.0, feet.y - 0.3, feet.z - 0.0),
    ]
}

/// Leave-at-top countdown; true when it ran out.
pub fn ladder_leave_tick(count: &mut f32, dt: f32) -> bool {
    *count -= dt;
    *count <= 0.0
}

/// `OnMove`: only forward/back moves while climbing set the multiplier. Always consumed.
pub fn ladder_on_move(state: i32, right: bool, mul: f32, move_mul: &mut f32) {
    if state == 0 || state == 2 || right {
        return;
    }
    *move_mul = mul;
}

/// `OnAddYaw`/`OnAddPitch`: the camera is frozen only while attaching.
pub fn ladder_camera_free(state: i32) -> bool {
    state != 0
}

/// `SetupHeadTurnLimits`: `[pitch_min, pitch_max, yaw_min, yaw_max]`.
pub fn ladder_head_limits(yaw: f32, pitch: f32) -> [f32; 4] {
    let (xmax, ymax) = (to_rad(120.0), to_rad(79.0));
    let (xmin, ymin) = (to_rad(-120.0), to_rad(-60.0));
    [pitch + ymin, pitch + ymax, yaw + xmin, yaw + xmax]
}
