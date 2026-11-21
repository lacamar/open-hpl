//! `iCharacterBody` accessors defined in Rust under their Itanium symbols. `mS` (offset 8) and `mE`
//! (offset 16) point to the Rust `CharState` and `CharExt`; offsets asserted in CharacterBody.cpp.

use super::char_conn::CharExt;
use super::character::CharState;
use super::cxx::ptr_member;
use crate::types::{Matrixf, Vector3f};

const MS_OFFSET: usize = 8;
const ME_OFFSET: usize = 16;

struct Char<'a> {
    s: &'a mut CharState,
    e: &'a mut CharExt,
}

/// # Safety
/// `this` is a live `iCharacterBody` whose constructor has run.
unsafe fn state<'a>(this: *const u8) -> Char<'a> {
    // SAFETY: caller contract.
    unsafe {
        Char {
            s: ptr_member(this, MS_OFFSET),
            e: ptr_member(this, ME_OFFSET),
        }
    }
}

fn at(a: &[f32; 2], dir: i32) -> f32 {
    a.get(dir as usize).copied().unwrap_or_default()
}

super::cxx::cxx! {
    state;
    "_ZN3hpl14iCharacterBody7SetMassEf" set_mass[this, c](v: f32) => c.s.mass = v;
    "_ZN3hpl14iCharacterBody23GetMaxPositiveMoveSpeedENS_8eCharDirE" max_pos[this, c](d: i32) -> f32 => at(&c.s.max_pos_move_speed, d);
    "_ZN3hpl14iCharacterBody23GetMaxNegativeMoveSpeedENS_8eCharDirE" max_neg[this, c](d: i32) -> f32 => at(&c.s.max_neg_move_speed, d);
    "_ZN3hpl14iCharacterBody12GetMoveSpeedENS_8eCharDirE" move_speed[this, c](d: i32) -> f32 => at(&c.s.move_speed, d);
    "_ZN3hpl14iCharacterBody10GetMoveAccENS_8eCharDirE" move_acc[this, c](d: i32) -> f32 => at(&c.s.move_acc, d);
    "_ZN3hpl14iCharacterBody12GetMoveDeaccENS_8eCharDirE" move_deacc[this, c](d: i32) -> f32 => at(&c.s.move_deacc, d);
    "_ZN3hpl14iCharacterBody11GetPositionEv" position[this, c]() -> *const Vector3f => &raw const c.s.position;
    "_ZN3hpl14iCharacterBody8GetRightEv" right[this, c]() -> *const Vector3f => &raw const c.s.right;
    "_ZN3hpl14iCharacterBody18GetMaxGravitySpeedEv" max_gravity_speed[this, c]() -> f32 => c.s.max_gravity_speed;
    "_ZN3hpl14iCharacterBody9GetEntityEv" entity[this, c]() -> usize => c.e.entity;
    "_ZN3hpl14iCharacterBody15GetEntityOffsetEv" entity_offset[this, c]() -> *const Matrixf => &raw const c.e.entity_offset;
    "_ZN3hpl14iCharacterBody19GetEntityPostOffsetEv" entity_post_offset[this, c]() -> *const Matrixf => &raw const c.e.entity_post_offset;
}
