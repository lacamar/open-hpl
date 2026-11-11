//! `SomaLuxPlayer.cpp`: camera fades and moves, scripted camera rotation and auto-move, move-state
//! speed settings, state changes, health and damage bookkeeping, mouse look.

use crate::math::{Vector3f, atan2, dot3, len3, normalize3, sub3, v3};

use std::f32::consts::{PI, TAU as TWO_PI};

fn fmin(a: f32, b: f32) -> f32 {
    if b < a { b } else { a }
}

fn fmax(a: f32, b: f32) -> f32 {
    if a < b { b } else { a }
}

/// `cSomaLuxPlayer::cFadeValue`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Fade {
    pub value: f32,
    pub goal: f32,
    pub speed: f32,
    pub max_speed: f32,
    pub speed_mul: f32,
}

/// `cSomaLuxPlayer::cMoveVector`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct MoveVec {
    pub value: Vector3f,
    pub goal: Vector3f,
    pub speed: f32,
    pub acc: f32,
    pub max_speed: f32,
    pub slowdown_dist: f32,
    pub moving: bool,
}

pub fn fade_towards(x: &mut Fade, dt: f32) -> f32 {
    let diff = x.goal - x.value;
    let mut speed = if x.speed_mul > 0.0 {
        diff.abs() * x.speed_mul
    } else {
        x.speed
    };
    if x.max_speed > 0.0 {
        speed = fmin(speed, x.max_speed);
    }
    if speed <= 0.0 || diff.abs() <= fmax(speed * dt, 0.0001) {
        x.value = x.goal;
    } else {
        x.value += if diff > 0.0 { speed * dt } else { -speed * dt };
    }
    x.value
}

pub fn angle_delta(from: f32, to: f32) -> f32 {
    let mut d = (to - from + PI) % TWO_PI;
    if d < 0.0 {
        d += TWO_PI;
    }
    d - PI
}

pub fn step_move_vec(m: &mut MoveVec, dt: f32) {
    if !m.moving {
        return;
    }
    let delta = sub3(m.goal, m.value);
    let dist = len3(delta);
    m.speed = fmin(m.acc.mul_add(dt, m.speed), m.max_speed);
    let mut speed = m.speed;
    if m.slowdown_dist > 0.0 && dist < m.slowdown_dist {
        speed *= fmax(dist / m.slowdown_dist, 0.1);
    }
    if dist <= speed * dt || dist < 0.0001 {
        m.value = m.goal;
        m.moving = false;
        m.speed = 0.0;
    } else {
        let s = speed * dt / dist;
        m.value.x = delta.x.mul_add(s, m.value.x);
        m.value.y = delta.y.mul_add(s, m.value.y);
        m.value.z = delta.z.mul_add(s, m.value.z);
    }
}

/// `GetFOVMul`: the base multiplier times every keyed multiplier, in key order.
pub fn fov_mul(base: f32, keyed: &[f32]) -> f32 {
    keyed.iter().fold(base, |f, &v| f * v)
}

pub fn visibility_range_mul(values: &[f32]) -> f32 {
    values.iter().fold(1.0, |f, &v| f * v)
}

pub fn visibility_max_range(values: &[f32]) -> f32 {
    let mut f = -1.0;
    for &v in values {
        if v >= 0.0 {
            f = if f < 0.0 { v } else { fmin(f, v) };
        }
    }
    f
}

#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct CameraRotate {
    pub speed: f32,
    pub acc: f32,
    pub speed_mul: f32,
    pub max_speed: f32,
    pub local: bool,
    pub target: Vector3f,
}

#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct RotateStep {
    pub speed: f32,
    /// `Some((yaw, pitch))` to add to the body yaw and the camera pitch.
    pub add: Option<(f32, f32)>,
}

pub fn step_camera_rotate(
    r: &CameraRotate,
    cam_pos: Vector3f,
    body_yaw: f32,
    cam_pitch: f32,
    dt: f32,
) -> RotateStep {
    let target = if r.local {
        v3(
            cam_pos.x + r.target.x,
            cam_pos.y + r.target.y,
            cam_pos.z + r.target.z,
        )
    } else {
        r.target
    };
    let dir = sub3(target, cam_pos);
    let yaw = atan2(-dir.x, -dir.z);
    let pitch = atan2(dir.y, dir.x.mul_add(dir.x, dir.z * dir.z).sqrt());
    let d_yaw = angle_delta(body_yaw, yaw);
    let d_pitch = pitch - cam_pitch;
    let speed = fmin(r.acc.mul_add(dt, r.speed), r.max_speed);
    let len = d_yaw.mul_add(d_yaw, d_pitch * d_pitch).sqrt();
    let max = fmin(speed, len * r.speed_mul) * dt;
    let add = (len > 0.0001).then(|| {
        let f = fmin(max / len, 1.0);
        (d_yaw * f, d_pitch * f)
    });
    RotateStep { speed, add }
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub enum Automove {
    Arrived,
    Move { forward: f32, right: f32 },
}

pub fn step_automove(
    target: Vector3f,
    feet: Vector3f,
    forward: Vector3f,
    right: Vector3f,
    speed_mul: f32,
) -> Automove {
    let mut delta = sub3(target, feet);
    delta.y = 0.0;
    let dist = len3(delta);
    if dist < 0.05 {
        return Automove::Arrived;
    }
    let (mut f, mut r) = (forward, right);
    f.y = 0.0;
    r.y = 0.0;
    normalize3(&mut f);
    normalize3(&mut r);
    let mul = fmin(1.0, dist * speed_mul);
    Automove::Move {
        forward: dot3(delta, f) / dist * mul,
        right: dot3(delta, r) / dist * mul,
    }
}

pub fn update_average_move(avg_speed: &mut f32, avg_dir: &mut Vector3f, vel: Vector3f) {
    let mut v = vel;
    v.y = 0.0;
    let speed = len3(v);
    *avg_speed = (*avg_speed).mul_add(0.9, speed * 0.1);
    if speed > 0.001 {
        avg_dir.x = avg_dir.x.mul_add(0.9, v.x / speed * 0.1);
        avg_dir.y = avg_dir.y.mul_add(0.9, v.y / speed * 0.1);
        avg_dir.z = avg_dir.z.mul_add(0.9, v.z / speed * 0.1);
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct MoveSpeeds {
    pub max_forward: f32,
    pub max_backward: f32,
    pub max_sideway: f32,
    pub forward_acc: f32,
    pub sideway_acc: f32,
    pub forward_deacc: f32,
    pub sideway_deacc: f32,
    pub forward_opposite_mul: f32,
    pub sideway_opposite_mul: f32,
    pub max_forward_mul: f32,
    pub max_backward_mul: f32,
    pub max_sideway_mul: f32,
}

/// Values for the character body, in call order: forward max +/-, right max +/-, acc, deacc and
/// opposite-direction multiplier for forward then right.
pub fn move_speed_settings(s: &MoveSpeeds) -> [f32; 10] {
    [
        s.max_forward * s.max_forward_mul,
        -(s.max_backward * s.max_backward_mul),
        s.max_sideway * s.max_sideway_mul,
        -(s.max_sideway * s.max_sideway_mul),
        s.forward_acc,
        s.sideway_acc,
        s.forward_deacc,
        s.sideway_deacc,
        s.forward_opposite_mul,
        s.sideway_opposite_mul,
    ]
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum StateChange {
    Missing,
    Same,
    Switch,
}

pub fn state_change(exists: bool, is_current: bool) -> StateChange {
    if !exists {
        StateChange::Missing
    } else if is_current {
        StateChange::Same
    } else {
        StateChange::Switch
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum MessageKind {
    Other,
    Reset,
    MapEnter,
    DestroyWorldEntities,
}

pub fn message_kind(decl: &[u8]) -> MessageKind {
    match decl {
        b"void Reset()" => MessageKind::Reset,
        b"void OnMapEnter(cLuxMap @apMap)" => MessageKind::MapEnter,
        b"void DestroyWorldEntities(cLuxMap @apMap)" => MessageKind::DestroyWorldEntities,
        _ => MessageKind::Other,
    }
}

/// The map is gone on `Reset()`, so a state other than id 0 must not run `OnLeaveState`.
pub fn drops_state_on_reset(kind: MessageKind, has_state: bool, state_id: i32) -> bool {
    kind == MessageKind::Reset && has_state && state_id != 0
}

pub fn clears_camera_adds(is_transfer: bool) -> bool {
    !is_transfer
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Place {
    pub set_feet_and_yaw: bool,
}

/// `PlaceAtStart` without a character body does nothing (`None`).
pub fn place_plan(has_body: bool, script_handled: bool) -> Option<Place> {
    has_body.then_some(Place {
        set_feet_and_yaw: !script_handled,
    })
}

pub fn is_dead(health: f32, can_die: bool) -> bool {
    health <= 0.0 && can_die
}

/// `(new health, just died)`.
pub fn set_health(health: f32, max: f32, can_die: bool, x: f32) -> (f32, bool) {
    let was_dead = is_dead(health, can_die);
    let h = fmin(x, max);
    (h, !was_dead && is_dead(h, can_die))
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum DamageId {
    /// Id -1: not remembered.
    Untracked,
    Reject,
    Push,
    /// Drop the oldest id first.
    EvictPush,
}

/// An attack instance hurts once; the last eight ids are remembered.
pub fn damage_id(ids: &[i32], id: i32) -> DamageId {
    if id == -1 {
        DamageId::Untracked
    } else if ids.contains(&id) {
        DamageId::Reject
    } else if ids.len() > 7 {
        DamageId::EvictPush
    } else {
        DamageId::Push
    }
}

pub fn health_after_damage(health: f32, amount: f32, min_health: f32) -> f32 {
    fmax(health - amount, min_health)
}

/// `GiveDamage(float, int, int, float, const tString&)` native: never raises, never below zero.
pub fn health_after_script_damage(health: f32, amount: f32, min_health: f32) -> f32 {
    fmax(fmin(health, fmax(health - amount, min_health)), 0.0)
}

pub fn add_health(health: f32, x: f32, min_health: f32) -> f32 {
    fmax(health + x, min_health)
}

/// Mouse motion in screen heights per frame.
pub fn mouse_vector(rel_x: i32, rel_y: i32, sensitivity: f32, height: f32) -> (f32, f32) {
    (
        rel_x as f32 * sensitivity / height,
        rel_y as f32 * sensitivity / height,
    )
}

/// Weighted average of the recent samples (newest first), each older one weighted by `prev_mul`
/// times the one before.
pub fn smooth_mouse(samples: &[(f32, f32)], prev_mul: f32) -> (f32, f32) {
    let (mut sx, mut sy) = (0.0f32, 0.0f32);
    let (mut mul, mut total) = (1.0f32, 0.0f32);
    for &(x, y) in samples {
        sx += x * mul;
        sy += y * mul;
        total += mul;
        mul *= prev_mul;
    }
    (sx / total, sy / total)
}

/// The smoothing window drops its oldest sample once it holds more than `max`.
pub fn pops_oldest_sample(len: usize, max: i32) -> bool {
    (len as i64) > i64::from(max)
}
