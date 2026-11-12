//! Registry of Rust-side records behind Newton user data.

use super::host::JointOps;
use crate::obj::{Arena, Handle};
use crate::sys::newton::{NewtonBody, NewtonJoint, NewtonWorld};
use crate::types::{Matrixf, Vector3f};
use std::sync::{LazyLock, Mutex, MutexGuard};

#[derive(Clone, Copy)]
pub struct WorldRec {
    pub nw: *mut NewtonWorld,
    pub max_step: f32,
}

#[derive(Clone, Copy)]
pub struct BodyRec {
    pub nb: *mut NewtonBody,
    pub nw: *mut NewtonWorld,
    pub cpp: usize,
    pub world: Handle<WorldRec>,
    pub mass: f32,
    pub gravity: bool,
    pub max_linear: f32,
    pub max_angular: f32,
    pub force: Vector3f,
    pub torque: Vector3f,
    pub forced: bool,
    pub buoyancy_force: Vector3f,
    pub buoyancy_torque: Vector3f,
}

#[derive(Clone, Copy, PartialEq, Eq)]
pub enum JointKind {
    Ball,
    Hinge,
    Slider,
    Screw,
}

#[derive(Clone, Copy)]
pub struct JointRec {
    pub nj: *mut NewtonJoint,
    pub cpp: usize,
    pub ops: *const JointOps,
    pub kind: JointKind,
    pub local0: Matrixf,
    pub local1: Matrixf,
    pub min: f32,
    pub max: f32,
    pub previous: f32,
}

#[derive(Default)]
pub struct Reg {
    pub worlds: Arena<WorldRec>,
    pub bodies: Arena<BodyRec>,
    pub joints: Arena<JointRec>,
    /// Bodies with accumulated forces, per world, in `MarkForced` order.
    pub forced: Vec<(Handle<WorldRec>, Handle<BodyRec>)>,
}

// SAFETY: the raw pointers are Newton handles used only through the Newton API, which does its
// own locking; the registry itself is behind a mutex.
unsafe impl Send for Reg {}

static REG: LazyLock<Mutex<Reg>> = LazyLock::new(|| Mutex::new(Reg::default()));

/// Hold the guard only for a copy-in/copy-out; never across Newton or host calls.
pub fn reg() -> Option<MutexGuard<'static, Reg>> {
    REG.lock().ok()
}

pub fn body(bits: u64) -> Option<(Handle<BodyRec>, BodyRec)> {
    let h = Handle::from_bits(bits)?;
    let rec = *reg()?.bodies.get(h)?;
    Some((h, rec))
}

pub fn world(bits: u64) -> Option<(Handle<WorldRec>, WorldRec)> {
    let h = Handle::from_bits(bits)?;
    let rec = *reg()?.worlds.get(h)?;
    Some((h, rec))
}

pub fn joint(bits: u64) -> Option<(Handle<JointRec>, JointRec)> {
    let h = Handle::from_bits(bits)?;
    let rec = *reg()?.joints.get(h)?;
    Some((h, rec))
}

pub fn update_body(h: Handle<BodyRec>, f: impl FnOnce(&mut BodyRec)) {
    if let Some(mut r) = reg()
        && let Some(b) = r.bodies.get_mut(h)
    {
        f(b);
    }
}

pub fn update_joint(h: Handle<JointRec>, f: impl FnOnce(&mut JointRec)) {
    if let Some(mut r) = reg()
        && let Some(j) = r.joints.get_mut(h)
    {
        f(j);
    }
}
