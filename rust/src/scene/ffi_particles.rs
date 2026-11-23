//! C ABI for `particles.rs` / `billboard.rs`, declared in `HPL2/core/include/scene/RustParticles.h`.
//!
//! Vectors and matrices cross as pointers (`cVector3` has a user-provided copy constructor).
//! Pointers are non-null (unless noted), valid and aligned for the call; an emitter handle is
//! freed exactly once with `ohpl_pe_free`.

use super::particles::{Emitter, Params, Particle};
use crate::types::{Matrixf, Vector2l, Vector3f};
use std::ffi::c_void;

pub type CollideFn = unsafe extern "C" fn(
    ctx: *mut c_void,
    start: *const Vector3f,
    end: *const Vector3f,
    normal: *mut Vector3f,
    point: *mut Vector3f,
) -> bool;

/// # Safety
/// `params` and `sub_div` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_pe_new(
    params: *const Params,
    max_particles: u32,
    respawn: bool,
    sub_div: *const Vector2l,
    materials: u32,
) -> *mut Emitter {
    // SAFETY: caller contract.
    let (p, sd) = unsafe { (*params, *sub_div) };
    Box::into_raw(Box::new(Emitter::new(
        p,
        max_particles,
        respawn,
        sd,
        materials as usize,
    )))
}

/// # Safety
/// `e` from `ohpl_pe_new` (or null), not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_pe_free(e: *mut Emitter) {
    if !e.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(e) });
    }
}

/// `collide` may be null (no collision).
///
/// # Safety
/// `e` live; `start` valid; `collide` is called with `ctx` and valid pointers and must not
/// re-enter `e`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_pe_update_motion(
    e: *mut Emitter,
    dt: f32,
    start: *const Matrixf,
    collide: Option<CollideFn>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    let (e, start) = unsafe { (&mut *e, &*start) };
    let mut cb = |s: &Vector3f, t: &Vector3f| {
        let f = collide?;
        let mut n = Vector3f::default();
        let mut p = Vector3f::default();
        // SAFETY: caller contract on `collide`; the pointers are to live locals.
        unsafe { f(ctx, s, t, &mut n, &mut p) }.then_some((n, p))
    };
    e.update_motion(dt, start, &mut cb);
}

/// # Safety
/// `e` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_pe_num_particles(e: *const Emitter) -> u32 {
    // SAFETY: caller contract.
    unsafe { &*e }.num_particles()
}

/// Copies pool slot `i` to `out`; false when out of range.
///
/// # Safety
/// `e` live; `out` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_pe_particle(e: *const Emitter, i: u32, out: *mut Particle) -> bool {
    // SAFETY: caller contract.
    match unsafe { &*e }.particle(i as usize) {
        Some(p) => {
            // SAFETY: `out` valid.
            unsafe { *out = *p };
            true
        }
        None => false,
    }
}
