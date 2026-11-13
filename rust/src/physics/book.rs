//! Rust-owned bookkeeping of `iPhysicsBody` and `iPhysicsJoint` (`OHPL_IMPL=rust`).
//! C++ keeps an opaque token per object; the registry lock is never held across a host call.

use crate::math::{IDENTITY, v3};
use crate::obj::{Arena, Handle};
use crate::types::{Matrixf, Vector3f};
use std::collections::{BTreeMap, BTreeSet};
use std::ops::Bound;
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

/// Object lists of one `iPhysicsWorld`, in C++ `std::list` insertion order (materials: `std::map` key order).
#[derive(Default)]
pub struct Lists {
    pub shapes: Vec<usize>,
    pub bodies: Vec<usize>,
    pub chars: Vec<usize>,
    pub joints: Vec<usize>,
    pub ctrls: Vec<usize>,
    pub ropes: Vec<usize>,
    pub mats: BTreeMap<Vec<u8>, usize>,
}

/// Per-world set of bodies updated every step, ordered by address like `std::set<iPhysicsBody*>`.
#[derive(Default)]
pub struct WorldBook {
    update: BTreeSet<usize>,
    pub lists: Lists,
    pub op: Option<super::wreg::WorldOp>,
    pub me: usize,
}

#[derive(Default)]
struct Books {
    bodies: Arena<BodyBook>,
    joints: Arena<JointBook>,
    worlds: Arena<WorldBook>,
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

pub fn world_new() -> u64 {
    books()
        .and_then(|mut b| b.worlds.insert(WorldBook::default()))
        .map_or(0, Handle::to_bits)
}

pub fn world_free(bits: u64) {
    if let (Some(h), Some(mut b)) = (Handle::from_bits(bits), books()) {
        b.worlds.remove(h);
    }
}

pub fn world_with<R>(bits: u64, f: impl FnOnce(&mut WorldBook) -> R) -> Option<R> {
    let h = Handle::from_bits(bits)?;
    books()?.worlds.get_mut(h).map(f)
}

impl WorldBook {
    pub fn insert(&mut self, body: usize) -> bool {
        self.update.insert(body)
    }

    pub fn remove(&mut self, body: usize) -> bool {
        self.update.remove(&body)
    }

    pub fn clear(&mut self) {
        self.update.clear();
    }

    /// Smallest member above `after` (the first member when `None`): the `std::set` iterator step.
    pub fn next_after(&self, after: Option<usize>) -> Option<usize> {
        let lo = after.map_or(Bound::Unbounded, Bound::Excluded);
        self.update.range((lo, Bound::Unbounded)).next().copied()
    }
}
