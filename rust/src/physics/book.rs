//! Rust-owned bookkeeping of `iPhysicsBody` and `iPhysicsJoint` (`OHPL_IMPL=rust`).
//! C++ keeps an opaque token per object; the registry lock is never held across a host call.

use crate::math::{IDENTITY, v3};
use crate::obj::{Arena, Handle};
use crate::types::{Matrixf, Vector3f};
use std::sync::{LazyLock, Mutex, MutexGuard};

#[derive(Clone, Copy)]
pub struct BodyBook {
    pub has_slide: bool,
    pub has_collision: bool,
    pub static_movement: bool,
    pub disable_after_sim: bool,
    pub slide_count: i32,
    pub total_linear: Vector3f,
    pub total_angular: Vector3f,
}

impl Default for BodyBook {
    fn default() -> Self {
        Self {
            has_slide: false,
            has_collision: false,
            static_movement: false,
            disable_after_sim: false,
            slide_count: 0,
            total_linear: v3(0.0, 0.0, 0.0),
            total_angular: v3(0.0, 0.0, 0.0),
        }
    }
}

#[derive(Clone, Copy, Default)]
pub struct JointBook {
    pub prev_child: Matrixf,
    pub prev_parent: Matrixf,
    pub speed_count: i32,
    pub limit_step_count: i32,
}

#[derive(Default)]
struct Books {
    bodies: Arena<BodyBook>,
    joints: Arena<JointBook>,
}

static BOOKS: LazyLock<Mutex<Books>> = LazyLock::new(|| Mutex::new(Books::default()));

fn books() -> Option<MutexGuard<'static, Books>> {
    BOOKS.lock().ok()
}

pub fn body_new() -> u64 {
    books()
        .and_then(|mut b| b.bodies.insert(BodyBook::default()))
        .map_or(0, Handle::to_bits)
}

pub fn body_free(bits: u64) {
    if let (Some(h), Some(mut b)) = (Handle::from_bits(bits), books()) {
        b.bodies.remove(h);
    }
}

pub fn body_get(bits: u64) -> Option<BodyBook> {
    let h = Handle::from_bits(bits)?;
    books()?.bodies.get(h).copied()
}

pub fn body_with<R>(bits: u64, f: impl FnOnce(&mut BodyBook) -> R) -> Option<R> {
    let h = Handle::from_bits(bits)?;
    books()?.bodies.get_mut(h).map(f)
}

pub fn joint_new() -> u64 {
    books()
        .and_then(|mut b| {
            b.joints.insert(JointBook {
                prev_child: IDENTITY,
                prev_parent: IDENTITY,
                ..JointBook::default()
            })
        })
        .map_or(0, Handle::to_bits)
}

pub fn joint_free(bits: u64) {
    if let (Some(h), Some(mut b)) = (Handle::from_bits(bits), books()) {
        b.joints.remove(h);
    }
}

pub fn joint_get(bits: u64) -> Option<JointBook> {
    let h = Handle::from_bits(bits)?;
    books()?.joints.get(h).copied()
}

pub fn joint_with<R>(bits: u64, f: impl FnOnce(&mut JointBook) -> R) -> Option<R> {
    let h = Handle::from_bits(bits)?;
    books()?.joints.get_mut(h).map(f)
}
