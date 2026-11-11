//! `MoveLinearTo` / `MoveAngularTo` / `RotateAtSpeed` and their per-frame updates.

use super::geom::{rotation, set_translation, translation};
use crate::math::{
    acos, clamp, dot3, fma, matrix_inverse, matrix_mul, matrix_mul_3x3, matrix_quaternion,
    normalize3, quat_dot, quat_from_angle_axis, quat_from_rotation_matrix, quat_normalize,
    quat_scale, quat_slerp, v3,
};
use crate::types::{Matrixf, Quaternion, Vector3f};

/// `cMath::Min` / `cMath::Max` (float overloads): the second operand wins on NaN.
pub(crate) fn cmin(a: f32, b: f32) -> f32 {
    if a < b { a } else { b }
}

pub(crate) fn cmax(a: f32, b: f32) -> f32 {
    if a > b { a } else { b }
}

fn sqr_len(v: Vector3f) -> f32 {
    fma(v.z, v.z, fma(v.x, v.x, v.y * v.y))
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Move {
    pub moving: bool,
    pub goal: Vector3f,
    pub acc: f32,
    pub max_speed: f32,
    pub slowdown: f32,
    pub speed: f32,
}

impl Move {
    pub fn start(&mut self, goal: Vector3f, acc: f32, max_speed: f32, slowdown: f32, reset: bool) {
        self.moving = true;
        self.goal = goal;
        self.acc = acc;
        self.max_speed = max_speed;
        self.slowdown = slowdown;
        if reset {
            self.speed = 0.0;
        }
    }

    /// The new translation of the entity while moving, and whether the goal was reached.
    pub fn step(&mut self, dt: f32, pos: Vector3f) -> Option<(Vector3f, bool)> {
        if !self.moving {
            return None;
        }
        let delta = v3(
            self.goal.x - pos.x,
            self.goal.y - pos.y,
            self.goal.z - pos.z,
        );
        let dist = sqr_len(delta).sqrt();
        self.speed = cmin(fma(self.acc, dt, self.speed), self.max_speed);
        let mut speed = self.speed;
        if self.slowdown > 0.0 && dist < self.slowdown {
            speed = cmin(
                speed,
                cmax(
                    self.max_speed * dist / self.slowdown,
                    self.max_speed * 0.05f32,
                ),
            );
        }
        let step = speed * dt;
        let done = step >= dist;
        let f = step / dist;
        let to = if done {
            self.goal
        } else {
            v3(
                fma(delta.x, f, pos.x),
                fma(delta.y, f, pos.y),
                fma(delta.z, f, pos.z),
            )
        };
        if done {
            self.moving = false;
            self.speed = 0.0;
        }
        Some((to, done))
    }

    pub fn stop(&mut self) {
        self.moving = false;
        self.speed = 0.0;
    }
}

pub const ROTATE_NONE: i32 = 0;
pub const ROTATE_ALIGN: i32 = 1;
pub const ROTATE_SPEED: i32 = 2;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Rotate {
    pub mode: i32,
    pub goal: Matrixf,
    pub axis: Vector3f,
    pub pivot: Vector3f,
    pub acc: f32,
    pub max_speed: f32,
    pub slowdown: f32,
    pub speed: f32,
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct RotateOut {
    pub matrix: Matrixf,
    pub done: bool,
}

impl Rotate {
    pub fn align_to(
        &mut self,
        goal: &Matrixf,
        acc: f32,
        max: f32,
        slowdown: f32,
        reset: bool,
        pivot: Vector3f,
    ) {
        self.mode = ROTATE_ALIGN;
        self.goal = rotation(goal);
        self.acc = acc;
        self.max_speed = max;
        self.slowdown = slowdown;
        if reset {
            self.speed = 0.0;
        }
        self.pivot = pivot;
    }

    pub fn at_speed(
        &mut self,
        acc: f32,
        goal_speed: f32,
        axis: Vector3f,
        reset: bool,
        pivot: Vector3f,
    ) {
        self.mode = ROTATE_SPEED;
        self.acc = acc;
        self.max_speed = goal_speed;
        self.axis = axis;
        if reset {
            self.speed = 0.0;
        }
        self.pivot = pivot;
    }

    pub fn stop(&mut self) {
        self.mode = ROTATE_NONE;
        self.speed = 0.0;
    }

    /// `UpdateRotate`: the matrix to set, or none when the update does nothing this frame.
    pub fn step(&mut self, dt: f32, m: &Matrixf) -> Option<RotateOut> {
        let rot = rotation(m);
        let t = translation(m);
        let off = matrix_mul_3x3(&rot, self.pivot);
        let pivot = v3(t.x + off.x, t.y + off.y, t.z + off.z);
        let mut done = false;
        let step_mtx;
        if self.mode == ROTATE_SPEED {
            let diff = self.max_speed - self.speed;
            self.speed += clamp(diff, -self.acc * dt, self.acc * dt);
            let mut axis = self.axis;
            if sqr_len(axis) < 1e-8f32 || self.speed == 0.0 {
                return None;
            }
            let mut q = Quaternion::default();
            normalize3(&mut axis);
            quat_from_angle_axis(&mut q, self.speed * dt, axis);
            step_mtx = matrix_quaternion(&q);
        } else {
            let mut q_cur = Quaternion::default();
            let mut q_goal = Quaternion::default();
            quat_from_rotation_matrix(&mut q_cur, &rot);
            quat_from_rotation_matrix(&mut q_goal, &self.goal);
            if quat_dot(&q_cur, &q_goal) < 0.0 {
                q_goal = quat_scale(&q_goal, -1.0);
            }
            let angle = 2.0f32 * acos(cmin(quat_dot(&q_cur, &q_goal), 1.0f32));
            self.speed = cmin(fma(self.acc, dt, self.speed), self.max_speed);
            let mut speed = self.speed;
            if self.slowdown > 0.0 && angle < self.slowdown {
                speed = cmin(
                    speed,
                    cmax(
                        self.max_speed * angle / self.slowdown,
                        self.max_speed * 0.05f32,
                    ),
                );
            }
            let step = speed * dt;
            done = step >= angle;
            let mut q_new = if done {
                q_goal
            } else {
                quat_slerp(step / angle, &q_cur, &q_goal, true)
            };
            quat_normalize(&mut q_new);
            step_mtx = matrix_mul(&matrix_quaternion(&q_new), &matrix_inverse(&rot));
        }
        let mut new = matrix_mul(&step_mtx, &rot);
        let rel = v3(t.x - pivot.x, t.y - pivot.y, t.z - pivot.z);
        let r = matrix_mul_3x3(&step_mtx, rel);
        set_translation(&mut new, v3(pivot.x + r.x, pivot.y + r.y, pivot.z + r.z));
        if done {
            self.mode = ROTATE_NONE;
            self.speed = 0.0;
        }
        Some(RotateOut { matrix: new, done })
    }
}

pub const MOVE_SOUND_KEEP: i32 = 0;
pub const MOVE_SOUND_REPOSITION: i32 = 1;
pub const MOVE_SOUND_SWITCH: i32 = 2;

/// `UpdateStaticMoveSound`: false when nothing can change (state matches and no loop is held).
pub fn static_move_enter(moving: bool, flag: bool, loop_present: bool) -> bool {
    moving != flag || loop_present
}

/// `UpdateStaticMoveSound` once the world is known: `REPOSITION` moves the live loop, `KEEP` does
/// nothing, `SWITCH` flips the flag and starts/stops sounds.
pub fn static_move_plan(moving: bool, flag: bool, loop_alive: bool) -> i32 {
    if moving != flag {
        MOVE_SOUND_SWITCH
    } else if loop_alive {
        MOVE_SOUND_REPOSITION
    } else {
        MOVE_SOUND_KEEP
    }
}

pub fn is_moving(moving: bool, rotate_mode: i32, rotate_max_speed: f32) -> bool {
    moving
        || rotate_mode == ROTATE_ALIGN
        || (rotate_mode == ROTATE_SPEED && rotate_max_speed != 0.0)
}

/// `UpdateCharCollision`, "until stopped" branch: per body, whether to clear its character
/// collision; returns whether every body is done.
pub fn char_stopped_step(
    collide: &[bool],
    default_collide: &[bool],
    lin_sqr: &[f32],
    ang_sqr: &[f32],
    clear: &mut [bool],
) -> bool {
    let mut done = 0usize;
    for (i, out) in clear.iter_mut().enumerate() {
        *out = false;
        let c = collide.get(i).copied().unwrap_or(false);
        let d = default_collide.get(i).copied().unwrap_or(false);
        if c && !d
            && lin_sqr.get(i).copied().unwrap_or(0.0) >= 0.01f32
            && ang_sqr.get(i).copied().unwrap_or(0.0) >= 0.01f32
        {
            continue;
        }
        if c && !d {
            *out = true;
        }
        done += 1;
    }
    done == clear.len()
}

/// `UpdateCharCollision`, "until outside the player" branch: the impulse pushing a body that
/// still overlaps the player out, if any.
pub fn outside_push(
    body_pos: Vector3f,
    player_pos: Vector3f,
    lin_vel: Vector3f,
) -> Option<Vector3f> {
    let mut dir = v3(
        body_pos.x - player_pos.x,
        body_pos.y - player_pos.y,
        body_pos.z - player_pos.z,
    );
    dir.y *= 0.1f32;
    normalize3(&mut dir);
    if dot3(lin_vel, dir).abs() < 0.5f32 {
        Some(v3(dir.x * 0.5f32, dir.y * 0.5f32, dir.z * 0.5f32))
    } else {
        None
    }
}
