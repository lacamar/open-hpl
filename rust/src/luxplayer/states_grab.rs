//! `cLuxPlayerState_InteractGrab`, `_InteractPush` and `_InteractSlide` math: grab distance,
//! slow-down, goal matrix, force and torque, push direction, limits and forces, slide speed.

use super::hw;
use super::states_vec::{
    Pid3, cross, dist3, dot, fmad, hmul3, madd, mat_vec, max_length, msub3, nmul, normalize,
};
use crate::math::{
    acos, cos, div3, fma, len3, matrix_inverse, matrix_mul, matrix_rotate, matrix_translate, sin,
    sub3, v3,
};
use crate::types::{Matrixf, Vector2f, Vector3f};

fn angle(a: Vector3f, b: Vector3f) -> f32 {
    let c = dot(a, b);
    if (c - 1.0f32).abs() <= 0.0001 {
        return 0.0;
    }
    acos(c)
}

pub(super) fn rotation(m: &Matrixf) -> Matrixf {
    let r = &m.m;
    let row = |i: usize| -> [f32; 4] {
        let g = |j: usize| r.get(i).and_then(|x| x.get(j)).copied().unwrap_or(0.0);
        [g(0), g(1), g(2), 0.0]
    };
    Matrixf {
        m: [row(0), row(1), row(2), [0.0, 0.0, 0.0, 1.0]],
    }
}

pub(super) fn row3(m: &Matrixf, i: usize) -> Vector3f {
    let g = |j: usize| m.m.get(i).and_then(|x| x.get(j)).copied().unwrap_or(0.0);
    v3(g(0), g(1), g(2))
}

pub(super) fn translation(m: &Matrixf) -> Vector3f {
    let g = |i: usize| m.m.get(i).and_then(|x| x.get(3)).copied().unwrap_or(0.0);
    v3(g(0), g(1), g(2))
}

pub(super) fn rotate_x(a: f32) -> Matrixf {
    let (s, c) = (sin(a), cos(a));
    Matrixf {
        m: [
            [1.0, 0.0, 0.0, 0.0],
            [0.0, c, -s, 0.0],
            [0.0, s, c, 0.0],
            [0.0, 0.0, 0.0, 1.0],
        ],
    }
}

pub(super) fn rotate_y(a: f32) -> Matrixf {
    let (s, c) = (sin(a), cos(a));
    Matrixf {
        m: [
            [c, 0.0, s, 0.0],
            [0.0, 1.0, 0.0, 0.0],
            [-s, 0.0, c, 0.0],
            [0.0, 0.0, 0.0, 1.0],
        ],
    }
}

const XYZ: i32 = 0;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct GrabEnterIn {
    pub cam_pos: Vector3f,
    pub cam_rot: Vector3f,
    pub body_matrix: Matrixf,
    pub body_pos: Vector3f,
    pub focus_pos: Vector3f,
    pub rot_offset: Vector3f,
    pub use_offset: u32,
    pub use_depth: u32,
    pub depth: f32,
    pub max_depth: f32,
    pub proper_mass_sum: f32,
    pub min_slow_mass: f32,
    pub max_slow_mass: f32,
    pub min_slow_mul: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct GrabEnterOut {
    pub body_rot: Matrixf,
    pub local_offset: Vector3f,
    pub depth: f32,
    pub max_distance: f32,
    pub slow: u32,
    pub slow_mul: f32,
}

/// `OnEnterState` math; `local_offset` is only written when the grab data has no offset.
pub fn grab_enter(i: &GrabEnterIn, o: &mut GrabEnterOut) {
    if i.use_offset != 0 {
        o.body_rot = matrix_rotate(i.rot_offset, XYZ);
    } else {
        let cam = matrix_rotate(i.cam_rot, XYZ);
        let inv = matrix_inverse(&cam);
        o.body_rot = matrix_mul(&inv, &rotation(&i.body_matrix));
        o.local_offset = mat_vec(&inv, sub3(i.body_pos, i.focus_pos));
    }
    o.depth = if i.use_depth != 0 {
        i.depth
    } else {
        dist3(i.focus_pos, i.cam_pos) - 0.08
    };
    o.slow = 0;
    o.slow_mul = 0.0;
    if i.proper_mass_sum >= i.min_slow_mass {
        o.slow = 1;
        if i.proper_mass_sum >= i.max_slow_mass {
            o.slow_mul = i.min_slow_mul;
        } else {
            let m = (i.proper_mass_sum - i.min_slow_mass) / (i.max_slow_mass - i.min_slow_mass);
            o.slow_mul = fma(m, i.min_slow_mul, 1.0 - m);
        }
    }
    let d = dist3(i.cam_pos, i.body_pos);
    let d = if d > i.max_depth { d } else { i.max_depth };
    o.max_distance = fma(d, 1.1, 0.2);
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct GrabPost {
    pub cam_pos: Vector3f,
    pub cam_rot: Vector3f,
    pub body_matrix: Matrixf,
    pub body_pos: Vector3f,
    pub body_ang_vel: Vector3f,
    pub inertia: Matrixf,
    pub body_rot: Matrixf,
    pub local_offset: Vector3f,
    pub pos_offset: Vector3f,
    pub depth: f32,
    pub max_distance: f32,
    pub use_offset: u32,
    pub use_rotation: u32,
    pub mass_sum: f32,
    pub max_force: f32,
    pub max_torque: f32,
    pub max_angular_speed: f32,
    pub force_mul: f32,
    pub torque_mul: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct GrabPostOut {
    pub status: i32,
    pub force: Vector3f,
    pub torque: Vector3f,
}

pub const GRAB_TOO_FAR: i32 = 0;
pub const GRAB_FORCE: i32 = 1;
pub const GRAB_FORCE_TORQUE: i32 = 2;

fn angle_axis(a: Vector3f, b: Vector3f, w: Vector3f) -> Vector3f {
    let axis = cross(a, b);
    let err = angle(b, a);
    v3(
        fma(axis.x * err, 100.0, w.x),
        fma(axis.y * err, 100.0, w.y),
        fma(axis.z * err, 100.0, w.z),
    )
}

/// `PostUpdate` math.
pub fn grab_post(
    i: &GrabPost,
    force_pid: &mut Pid3,
    torque_pid: &mut Pid3,
    dt: f32,
    o: &mut GrabPostOut,
) {
    if dist3(i.cam_pos, i.body_pos) > i.max_distance {
        o.status = GRAB_TOO_FAR;
        return;
    }
    let mut cam_t = matrix_rotate(i.cam_rot, XYZ);
    cam_t.m[0][3] = i.cam_pos.x;
    cam_t.m[1][3] = i.cam_pos.y;
    cam_t.m[2][3] = i.cam_pos.z;
    let base = if i.use_offset != 0 {
        v3(
            i.pos_offset.x + 0.0,
            i.pos_offset.y + 0.0,
            i.pos_offset.z + -i.depth,
        )
    } else {
        v3(
            0.0 + i.local_offset.x,
            0.0 + i.local_offset.y,
            -i.depth + i.local_offset.z,
        )
    };
    let goal = matrix_mul(&matrix_translate(base), &i.body_rot);
    let goal = matrix_mul(&cam_t, &goal);
    let wanted_pos = translation(&goal);
    let err = sub3(wanted_pos, i.body_pos);
    let f = hmul3(force_pid.output(err, dt), i.mass_sum);
    o.force = hmul3(max_length(f, i.max_force), i.force_mul);
    o.status = GRAB_FORCE;
    if i.use_rotation == 0 {
        return;
    }
    let goal_inv = matrix_inverse(&goal);
    let (w_up, w_right) = (row3(&goal_inv, 1), row3(&goal_inv, 0));
    let body_inv = matrix_inverse(&i.body_matrix);
    let (up, right) = (row3(&body_inv, 1), row3(&body_inv, 0));
    let mut wanted = Vector3f::default();
    wanted = angle_axis(up, w_up, wanted);
    wanted = angle_axis(right, w_right, wanted);
    let speed = len3(wanted);
    if speed > i.max_angular_speed {
        wanted = hmul3(div3(wanted, speed), 6.0);
    }
    let rot_err = sub3(wanted, i.body_ang_vel);
    let t = torque_pid.output(rot_err, dt);
    let t = mat_vec(&i.inertia, t);
    o.torque = hmul3(max_length(t, i.max_torque), i.torque_mul);
    o.status = GRAB_FORCE_TORQUE;
}

/// Leave-speed cap: the new velocity when `v` is faster than `max`.
pub fn leave_limit(v: Vector3f, max: f32) -> Option<Vector3f> {
    let s = len3(v);
    (s > max).then(|| hmul3(div3(v, s), max))
}

/// `OnScroll` depth.
pub fn grab_scroll(depth: f32, amount: f32, inc: f32, min: f32, max: f32) -> f32 {
    let mut d = fma(amount, inc, depth);
    if d < min {
        d = min;
    }
    if d > max {
        d = max;
    }
    d
}

/// `OnAddYaw`/`OnAddPitch` rotation of the grabbed body, `pitch` picks the x axis.
pub fn grab_rotate(rot: &Matrixf, pitch: bool, amount: f32) -> Matrixf {
    let r = if pitch {
        rotate_x(amount * -3.2)
    } else {
        rotate_y(amount * -3.2)
    };
    matrix_mul(&r, rot)
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct PushEnterIn {
    pub pitch: f32,
    pub yaw: f32,
    pub pitch_min_range: f32,
    pub pitch_max_range: f32,
    pub yaw_range: f32,
    pub at_point: u32,
    pub body_matrix: Matrixf,
    pub body_pos: Vector3f,
    pub feet: Vector3f,
    pub interact_pos: Vector3f,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct PushEnterOut {
    pub pitch_min: f32,
    pub pitch_max: f32,
    pub yaw_min: f32,
    pub yaw_max: f32,
    pub local_attach: Vector3f,
    pub max_attach_dist: f32,
}

/// `OnEnterState` math; `local_attach` is only written when pushing at a point.
pub fn push_enter(i: &PushEnterIn, o: &mut PushEnterOut) {
    o.pitch_min = i.pitch - i.pitch_min_range;
    o.pitch_max = i.pitch + i.pitch_max_range;
    o.yaw_min = i.yaw - i.yaw_range;
    o.yaw_max = i.yaw + i.yaw_range;
    let d = if i.at_point != 0 {
        o.local_attach = mat_vec(&matrix_inverse(&i.body_matrix), i.interact_pos);
        dist3(i.feet, i.interact_pos)
    } else {
        dist3(i.feet, i.body_pos)
    };
    o.max_attach_dist = fma(d, 1.2, 0.3);
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct PushBegin {
    pub at_point: u32,
    pub body_matrix: Matrixf,
    pub local_attach: Vector3f,
    pub body_pos: Vector3f,
    pub feet: Vector3f,
    pub max_attach_dist: f32,
    pub forward: Vector3f,
    pub right: Vector3f,
    pub moving: [u32; 2],
    pub total_move_mul: [f32; 2],
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct PushBeginOut {
    pub too_far: u32,
    pub attach: Vector3f,
    pub push_dir: Vector3f,
    pub no_movement: u32,
}

/// First half of `Update`: range check and push direction (normalised unless zero).
pub fn push_begin(i: &PushBegin, o: &mut PushBeginOut) {
    o.attach = Vector3f::default();
    if i.at_point != 0 {
        o.attach = mat_vec(&i.body_matrix, i.local_attach);
    }
    let d = if i.at_point != 0 {
        dist3(i.feet, o.attach)
    } else {
        dist3(i.feet, i.body_pos)
    };
    o.too_far = u32::from(d > i.max_attach_dist);
    if o.too_far != 0 {
        return;
    }
    let mut dir = Vector3f::default();
    let dirs = [i.forward, i.right];
    for (k, d) in dirs.iter().enumerate() {
        if i.moving.get(k).copied().unwrap_or(0) != 0 {
            dir = madd(*d, i.total_move_mul.get(k).copied().unwrap_or(0.0), dir);
        }
    }
    o.no_movement = u32::from(super::states_vec::is_zero(dir));
    if o.no_movement == 0 {
        normalize(&mut dir);
    }
    o.push_dir = dir;
}

/// Zero-input branch: the braking force.
pub fn push_stop(pid: &mut Pid3, vel: Vector3f, mass: f32, max_force: f32, dt: f32) -> Vector3f {
    let mut e = v3(0.0 - vel.x, 0.0 - vel.y, 0.0 - vel.z);
    e.y = 0.0;
    max_length(hmul3(pid.output(e, dt), mass), max_force)
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct PushDrive {
    pub push_dir: Vector3f,
    pub vel: Vector3f,
    pub mass: f32,
    pub max_force: f32,
    pub max_speed: f32,
    pub push_force: f32,
    pub force_mul: f32,
    pub body_matrix: Matrixf,
    pub forward: Vector3f,
    pub right: Vector3f,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct PushDriveOut {
    pub correct_force: Vector3f,
    pub has_push_force: u32,
    pub push_force: Vector3f,
    pub next_transform: Matrixf,
    pub max_speed: [f32; 2],
}

/// Second half of `Update` for a moving push; the `max_speed` pair is applied unless the body is
/// about to hit the player.
pub fn push_drive(pid: &mut Pid3, i: &PushDrive, dt: f32, o: &mut PushDriveOut) {
    let dir = i.push_dir;
    let vel_in_dir = hmul3(dir, dot(dir, i.vel));
    pid.p = 12.0;
    pid.i = 0.0;
    pid.d = 0.2;
    let mut e = sub3(vel_in_dir, i.vel);
    e.y = 0.0;
    o.correct_force = max_length(hmul3(pid.output(e, dt), i.mass), i.max_force);
    o.has_push_force = 0;
    if len3(vel_in_dir) < i.max_speed || dot(vel_in_dir, dir) < 0.0 {
        o.has_push_force = 1;
        o.push_force = max_length(hmul3(hmul3(dir, i.push_force), i.force_mul), i.max_force);
    }
    let max_player = if dot(vel_in_dir, dir) > 0.0 {
        vel_in_dir
    } else {
        Vector3f::default()
    };
    o.next_transform = matrix_mul(&matrix_translate(hmul3(dir, 0.1)), &i.body_matrix);
    o.max_speed = [
        dot(i.forward, max_player).abs(),
        dot(i.right, max_player).abs(),
    ];
}

/// Max speed and force for the push, by the normal move state's running and crouching flags.
pub fn push_pick(
    normal_move: bool,
    running: bool,
    crouching: bool,
    walk: (f32, f32),
    run: (f32, f32),
    crouch: (f32, f32),
) -> (f32, f32) {
    if normal_move {
        if running {
            return run;
        }
        if crouching {
            return crouch;
        }
    }
    walk
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct SlideIn {
    pub cam_pos: Vector3f,
    pub up: Vector3f,
    pub right: Vector3f,
    pub body_matrix: Matrixf,
    pub local_interact: Vector3f,
    pub mouse: Vector2f,
    pub pin: Vector3f,
    pub vel: Vector3f,
    pub mass: f32,
    pub max_distance: f32,
    pub slow_down_factor: f32,
    pub speed_factor: f32,
    pub max_speed: f32,
    pub max_force: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct SlideOut {
    pub out_of_range: u32,
    pub force: Vector3f,
}

/// `cLuxPlayerState_InteractSlide::Update`; `speed` is the member slide speed.
pub fn slide_update(i: &SlideIn, speed: &mut f32, pid: &mut Pid3, dt: f32, o: &mut SlideOut) {
    let world = mat_vec(&i.body_matrix, i.local_interact);
    o.out_of_range = u32::from(dist3(i.cam_pos, world) > i.max_distance);
    let m = i.mouse;
    let dir = v3(
        fma(i.up.x, m.y, nmul(m.x, i.right.x)),
        fma(i.up.y, m.y, nmul(m.x, i.right.y)),
        fma(i.up.z, m.y, nmul(m.x, i.right.z)),
    );
    let speed_add = dot(dir, i.pin);
    let slow = hw::mul(hw::mul(speed.abs(), 3.0), i.slow_down_factor);
    if *speed > 0.0 {
        *speed = hw::msub(slow, dt, *speed);
        if *speed < 0.0 {
            *speed = 0.0;
        }
    }
    if *speed < 0.0 {
        *speed = fmad(slow, dt, *speed);
        if *speed > 0.0 {
            *speed = 0.0;
        }
    }
    if m.x == 0.0 && m.y == 0.0 {
        *speed = 0.0;
    }
    *speed = fmad(
        hw::mul(hw::mul(speed_add, 850.0), dt),
        i.speed_factor,
        *speed,
    );
    if *speed > i.max_speed {
        *speed = i.max_speed;
    }
    if *speed < -i.max_speed {
        *speed = -i.max_speed;
    }
    let d = dot(i.pin, i.vel);
    let wanted = hmul3(i.pin, *speed);
    let f = hmul3(pid.output(msub3(d, i.pin, wanted), dt), i.mass);
    o.force = max_length(f, i.max_force);
}

/// Throw impulse for the slide state.
pub fn slide_throw(pin: Vector3f, cam_fwd: Vector3f, impulse: f32) -> Vector3f {
    let mut v = hmul3(pin, dot(pin, cam_fwd));
    normalize(&mut v);
    hmul3(v, impulse)
}
