//! Registry of Rust-side records behind Newton user data.

use super::host::JointOps;
use crate::math::Planef;
use crate::obj::{Arena, Handle};
use crate::sys::newton::{NewtonBody, NewtonJoint, NewtonWorld};
use crate::types::{Matrixf, Vector3f};
use std::sync::{LazyLock, Mutex, MutexGuard};

#[derive(Clone, Copy)]
pub struct WorldRec {
    pub nw: *mut NewtonWorld,
    pub max_step: f32,
    pub size_min: Vector3f,
    pub size_max: Vector3f,
    pub gravity: Vector3f,
    pub accuracy: i32,
    pub log_debug: bool,
    pub save_contacts: bool,
}

/// `cPhysicsBody_Buoyancy`; layout mirrored by `OhplL4Buoy` in `RustNewtonBackend.h`.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Buoy {
    pub active: bool,
    pub no_prev_vel: bool,
    pub density: f32,
    pub linear_viscosity: f32,
    pub angular_viscosity: f32,
    pub drag_density_mul: f32,
    pub surface: Planef,
    pub prev_linear_vel: Vector3f,
    pub prev_angular_vel: Vector3f,
}

const _: () = assert!(std::mem::size_of::<Buoy>() == 60);

impl Default for Buoy {
    fn default() -> Self {
        let z = Vector3f {
            x: 0.0,
            y: 0.0,
            z: 0.0,
        };
        Self {
            active: false,
            no_prev_vel: true,
            density: 1.0,
            linear_viscosity: 1.0,
            angular_viscosity: 1.0,
            drag_density_mul: 1.0,
            surface: Planef::default(),
            prev_linear_vel: z,
            prev_angular_vel: z,
        }
    }
}

#[derive(Clone, Copy)]
pub struct AutoDisable {
    pub linear: f32,
    pub angular: f32,
    pub steps: i32,
}

#[derive(Clone, Copy)]
pub struct BodyRec {
    pub buoy: Buoy,
    pub auto_disable: AutoDisable,
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
    pub cone: f32,
    pub twist: f32,
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

pub fn update_world(h: Handle<WorldRec>, f: impl FnOnce(&mut WorldRec)) {
    if let Some(mut r) = reg()
        && let Some(w) = r.worlds.get_mut(h)
    {
        f(w);
    }
}

pub fn update_joint(h: Handle<JointRec>, f: impl FnOnce(&mut JointRec)) {
    if let Some(mut r) = reg()
        && let Some(j) = r.joints.get_mut(h)
    {
        f(j);
    }
}
