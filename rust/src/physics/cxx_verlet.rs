//! `cVerletParticle`, `cVerletParticleRayCallback`, `iVerletParticleContainer` and `iPhysicsJoint::ControllerAt`
//! members defined in Rust under their Itanium symbols. Offsets are asserted in VerletParticle.cpp and RustPhysicsShells.cpp.

use super::cxx::val_member;
use super::rope::{self, P, Particle};
use super::shell;
use crate::types::Vector3f;

const CONTAINER_STATE: usize = 8;
const CONTAINER_LIST: usize = 96;
const CONTAINER_DAMPING: usize = 124;
const STATE_GRAVITY: usize = 64;
const STATE_DAMPING_MUL: usize = 76;
const CB_HIT: usize = 16;
const CB_POS: usize = 20;
const CB_NORMAL: usize = 32;
const CB_CLOSEST: usize = 44;
const RAY_DIST: usize = 4;
const RAY_NORMAL: usize = 8;
const RAY_POINT: usize = 20;
const JOINT_STATE: usize = 8;

/// # Safety
/// `p` is a live `cVerletParticle` whose container is live.
#[unsafe(export_name = "_ZN3hpl15cVerletParticle14UpdateMovementEf")]
unsafe extern "C" fn update_movement(p: *mut Particle, dt: f32) {
    // SAFETY: C++ calls with a live particle; the container owns its state block.
    unsafe {
        let c: *const u8 = val_member(p.cast::<u8>(), 0);
        let s: *const u8 = val_member(c, CONTAINER_STATE);
        let g: Vector3f = val_member(s, STATE_GRAVITY);
        rope::particle_update_movement(P(p), val_member(s, STATE_DAMPING_MUL), g, dt);
    }
}

macro_rules! constraint {
    ($($sym:literal $name:ident($($a:ident),*) => $f:path;)*) => {$(
        #[unsafe(export_name = $sym)]
        unsafe extern "C" fn $name(_this: *const u8, p1: *mut Particle, p2: *mut Particle, $($a: f32),*) {
            $f(P(p1), P(p2), $($a),*);
        }
    )*};
}

constraint! {
    "_ZN3hpl24iVerletParticleContainer22UpdateLengthConstraintEPNS_15cVerletParticleES2_ff" length(l, s) => rope::length_constraint;
    "_ZN3hpl24iVerletParticleContainer22UpdateLengthConstraintEPNS_15cVerletParticleES2_fff" range(lo, hi, s) => rope::length_constraint_range;
    "_ZN3hpl24iVerletParticleContainer29UpdateLengthConstraintStretchEPNS_15cVerletParticleES2_ff" stretch(l, s) => rope::length_constraint_stretch;
}

/// # Safety
/// `this` is a live `iVerletParticleContainer`.
#[unsafe(export_name = "_ZN3hpl24iVerletParticleContainer10SetDampingEf")]
unsafe extern "C" fn set_damping(this: *const u8, x: f32) {
    let x = x.clamp(0.0, 1.0);
    // SAFETY: C++ calls with a live container.
    unsafe {
        *this.add(CONTAINER_DAMPING).cast_mut().cast::<f32>() = x;
        let s: *mut u8 = val_member(this, CONTAINER_STATE);
        *s.add(STATE_DAMPING_MUL).cast::<f32>() = 1.0 - x;
    }
}

/// # Safety
/// `this` is a live `iVerletParticleContainer`.
#[unsafe(export_name = "_ZN3hpl24iVerletParticleContainer11GetParticleEi")]
unsafe extern "C" fn get_particle(this: *const u8, idx: i32) -> *mut u8 {
    // SAFETY: C++ calls with a live container; libstdc++ list: header {next, prev, size}, node {next, prev, value}.
    unsafe {
        let list = this.add(CONTAINER_LIST);
        let n: usize = val_member(list, 16);
        let Ok(i) = usize::try_from(idx) else {
            return std::ptr::null_mut();
        };
        if i >= n {
            return std::ptr::null_mut();
        }
        let mut node: *const u8 = val_member(list, 0);
        for _ in 0..i {
            node = val_member(node, 0);
        }
        val_member(node, 16)
    }
}

/// # Safety
/// `this` is a live `cVerletParticleRayCallback`.
#[unsafe(export_name = "_ZN3hpl26cVerletParticleRayCallback5ResetEv")]
unsafe extern "C" fn cb_reset(this: *mut u8) {
    // SAFETY: C++ calls with a live callback.
    unsafe {
        *this.add(CB_CLOSEST).cast::<f32>() = 9_999_999.0;
        *this.add(CB_HIT) = 0;
    }
}

/// # Safety
/// `this` is a live `cVerletParticleRayCallback`; `params` a live `cPhysicsRayParams`.
#[unsafe(export_name = "_ZN3hpl26cVerletParticleRayCallback11OnIntersectEPNS_12iPhysicsBodyEPNS_17cPhysicsRayParamsE")]
unsafe extern "C" fn cb_on_intersect(this: *mut u8, _body: *const u8, params: *const u8) -> bool {
    // SAFETY: C++ calls with live arguments.
    unsafe {
        *this.add(CB_HIT) = 1;
        let dist: f32 = val_member(params, RAY_DIST);
        if dist < val_member::<f32>(this, CB_CLOSEST) {
            *this.add(CB_CLOSEST).cast::<f32>() = dist;
            *this.add(CB_POS).cast::<Vector3f>() = val_member(params, RAY_POINT);
            *this.add(CB_NORMAL).cast::<Vector3f>() = val_member(params, RAY_NORMAL);
        }
    }
    true
}

/// # Safety
/// `this` is a live `iPhysicsJoint`.
#[unsafe(export_name = "_ZN3hpl13iPhysicsJoint12ControllerAtEi")]
unsafe extern "C" fn controller_at(this: *const u8, idx: i32) -> usize {
    // SAFETY: C++ calls with a live joint whose state block starts with the shell handle.
    unsafe {
        let s: *const u8 = val_member(this, JOINT_STATE);
        let shell_h: u64 = val_member(s, 0);
        usize::try_from(idx)
            .ok()
            .and_then(|i| shell::joint_controller_at(shell_h, i))
            .map_or(0, |(_, _, h)| h)
    }
}
