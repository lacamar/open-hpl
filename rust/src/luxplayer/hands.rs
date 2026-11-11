//! `LuxPlayerHands.cpp`: hands animation state selection and the smoothed hands transform.

use super::hw;
use super::ops::Ops;
use crate::math::v3;
use crate::types::Vector3f;

pub mod k {
    pub const ATTACH_DRAW: u32 = 150;
    pub const DISABLE: u32 = 151;
    pub const IDLE: u32 = 152;
}

pub const DISABLED: i32 = 0;
pub const HOLSTER: i32 = 1;
pub const DRAW: i32 = 2;
pub const IDLE: i32 = 3;
pub const HAND_OBJECT: i32 = 4;

/// `Update` state machine: the new state and whether the hand objects are updated afterwards.
/// A draw or hand-object state without a current object dereferenced null in the original; it is
/// a no-op here.
pub fn hands_update(
    state: i32,
    has_object: bool,
    anim_over: bool,
    object_anim_over: bool,
    o: &mut Ops,
) -> (i32, bool) {
    match state {
        DISABLED => (state, false),
        HOLSTER => {
            if anim_over {
                if has_object {
                    o.k(k::ATTACH_DRAW);
                    return (DRAW, true);
                }
                o.k(k::DISABLE);
                return (DISABLED, true);
            }
            (state, true)
        }
        DRAW => {
            if anim_over && has_object {
                o.k(k::IDLE);
                return (IDLE, true);
            }
            (state, true)
        }
        HAND_OBJECT => {
            if has_object && anim_over && object_anim_over {
                o.k(k::IDLE);
                return (IDLE, false);
            }
            (state, true)
        }
        _ => (state, true),
    }
}

/// `DoAction`: whether the current hand object gets the action.
pub fn hands_action_gate(state: i32, has_object: bool) -> bool {
    has_object && (state == IDLE || state == HAND_OBJECT)
}

pub const PLAN_NONE: i32 = 0;
pub const PLAN_HOLSTER: i32 = 1;
pub const PLAN_DRAW: i32 = 2;

/// `SetCurrentHandObject`: what to do when switching from the current to a new object.
pub fn hands_switch_plan(has_current: bool, has_new: bool) -> i32 {
    if has_current {
        PLAN_HOLSTER
    } else if has_new {
        PLAN_DRAW
    } else {
        PLAN_NONE
    }
}

/// Whether `UpdatePlayerHandsPos` drops the oldest rotation (`size_t > int` comparison).
pub fn rotations_pop(size: usize, max: i32) -> bool {
    size as u64 > i64::from(max) as u64
}

/// Weighted camera rotation of `UpdatePlayerHandsPos`, newest samples weighing the most.
pub fn hands_final_rotation(rots: &[Vector3f]) -> Vector3f {
    let n = rots.len() as f32;
    let mut count = 1.0f32;
    let mut total = 0.0f32;
    let mut fin = v3(0.0, 0.0, 0.0);
    for r in rots {
        let mut w = count / n;
        w *= w;
        count += 1.0;
        fin.x = r.x.mul_add(w, fin.x);
        fin.y = r.y.mul_add(w, fin.y);
        fin.z = w.mul_add(r.z, fin.z);
        total += w;
    }
    v3(fin.x / total, fin.y / total, fin.z / total)
}

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct HandsPos {
    pub cam_pos: Vector3f,
    pub right: Vector3f,
    pub up: Vector3f,
    pub forward: Vector3f,
    pub bob: Vector3f,
    pub shake: Vector3f,
    pub hurt: Vector3f,
    pub pos_add_mul: f32,
}

pub fn hands_translation(p: &HandsPos) -> Vector3f {
    let mut a = v3(0.0, 0.0, 0.0);
    for v in [p.bob, p.shake, p.hurt] {
        a = v3(a.x - v.x, a.y - v.y, a.z - v.z);
    }
    let cb = |r: f32, u: f32, f: f32| a.z.mul_add(f, a.x.mul_add(r, hw::mul(a.y, u)));
    let w = v3(
        cb(p.right.x, p.up.x, p.forward.x),
        cb(p.right.y, p.up.y, p.forward.y),
        cb(p.right.z, p.up.z, p.forward.z),
    );
    v3(
        hw::msub(w.x, p.pos_add_mul, p.cam_pos.x),
        hw::msub(w.y, p.pos_add_mul, p.cam_pos.y),
        hw::msub(w.z, p.pos_add_mul, p.cam_pos.z),
    )
}
