//! C ABI for `particles.rs` / `billboard.rs`, declared in `HPL2/core/include/scene/RustParticles.h`.
//!
//! Vectors and matrices cross as pointers (`cVector3` has a user-provided copy constructor).
//! Pointers are non-null (unless noted), valid and aligned for the call; an emitter handle is
//! freed exactly once with `ohpl_pe_free`.

use super::billboard::{self, ModelIn};
use super::particles::{
    Emitter, EnvVecs, EnvVelIn, Gfx, GfxIn, Params, Particle, env_setup, env_update,
    env_velocity_vectors,
};
use crate::types::{Color, Matrixf, Vector2f, Vector2l, Vector3f};
use std::ffi::c_void;
use std::slice;

pub type CollideFn = unsafe extern "C" fn(
    ctx: *mut c_void,
    start: *const Vector3f,
    end: *const Vector3f,
    normal: *mut Vector3f,
    point: *mut Vector3f,
) -> bool;

fn slice_mut<'a>(p: *mut f32, n: usize) -> &'a mut [f32] {
    if p.is_null() || n == 0 {
        &mut []
    } else {
        // SAFETY: caller passes `n` writable floats at `p`.
        unsafe { slice::from_raw_parts_mut(p, n) }
    }
}

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

/// Returns 0 hidden (alpha <= 0), 1 beam (nothing written), 2 drawn.
///
/// # Safety
/// `e`, `g` live; each array pointer addresses its stated float count (`tex` may be null).
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_pe_build_vertices(
    e: *const Emitter,
    g: *const GfxIn,
    pos: *mut f32,
    pos_len: usize,
    col: *mut f32,
    col_len: usize,
    tex: *mut f32,
    tex_len: usize,
) -> i32 {
    // SAFETY: caller contract.
    let (e, g) = unsafe { (&*e, &*g) };
    let out = e.build_vertices(
        g,
        slice_mut(pos, pos_len),
        slice_mut(col, col_len),
        slice_mut(tex, tex_len),
    );
    match out {
        Gfx::Hidden => 0,
        Gfx::Beam => 1,
        Gfx::Drawn => 2,
    }
}

/// # Safety
/// All pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_pe_bounds(
    e: *const Emitter,
    world_pos: *const Vector3f,
    right: *const Vector3f,
    forward: *const Vector3f,
    max_draw: *const Vector2f,
    out_min: *mut Vector3f,
    out_max: *mut Vector3f,
) {
    // SAFETY: caller contract.
    unsafe {
        let (mn, mx) = (*e).bounds(*world_pos, *right, *forward, *max_draw);
        *out_min = mn;
        *out_max = mx;
    }
}

/// # Safety
/// `e` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_pe_num_particles(e: *const Emitter) -> u32 {
    // SAFETY: caller contract.
    unsafe { &*e }.num_particles()
}

/// # Safety
/// `e` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_pe_is_dying(e: *const Emitter) -> bool {
    // SAFETY: caller contract.
    unsafe { &*e }.is_dying()
}

/// # Safety
/// `e` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_pe_is_dead(e: *const Emitter) -> bool {
    // SAFETY: caller contract.
    unsafe { &*e }.is_dead()
}

/// # Safety
/// `e` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_pe_kill(e: *mut Emitter) {
    // SAFETY: caller contract.
    unsafe { &mut *e }.kill();
}

/// # Safety
/// `e` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_pe_kill_instantly(e: *mut Emitter) {
    // SAFETY: caller contract.
    unsafe { &mut *e }.kill_instantly();
}

/// # Safety
/// `e` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_pe_frame(e: *const Emitter) -> f32 {
    // SAFETY: caller contract.
    unsafe { &*e }.frame()
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

/// # Safety
/// `inp` and `out` valid, not aliasing.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_bb_model_matrix(inp: *const ModelIn, out: *mut Matrixf) -> bool {
    // SAFETY: caller contract.
    let (m, warn) = billboard::model_matrix(unsafe { &*inp });
    unsafe { *out = m };
    warn
}

/// Fills 4 vertex colours (16 floats).
///
/// # Safety
/// `color` valid; `out` addresses 16 floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_bb_vertex_colors(
    color: *const Color,
    brightness: f32,
    halo_alpha: f32,
    hdr: bool,
    out: *mut f32,
) {
    // SAFETY: caller contract.
    unsafe { billboard::vertex_colors(*color, brightness, halo_alpha, hdr, &mut *out.cast()) };
}

/// Fills 4 vertex positions (16 floats, stride 4).
///
/// # Safety
/// `size` valid; `out` addresses 16 floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_bb_vertex_positions(size: *const Vector2f, out: *mut f32) {
    // SAFETY: caller contract.
    unsafe { billboard::vertex_positions(*size, &mut *out.cast()) };
}

/// # Safety
/// `inp` valid; each array addresses `ENV_MAX_ITER` vectors; arrays do not alias.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_env_velocity_vectors(
    inp: *const EnvVelIn,
    grav: *mut EnvVecs,
    wind: *mut EnvVecs,
    rot: *mut EnvVecs,
) {
    // SAFETY: caller contract.
    unsafe { env_velocity_vectors(&*inp, &mut *grav, &mut *wind, &mut *rot) };
}

/// # Safety
/// Each array addresses `ENV_MAX_ITER` vectors; the mutable ones do not alias the others.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_env_update(
    n: i32,
    dt: f32,
    grav: *const EnvVecs,
    wind: *const EnvVecs,
    rotv: *const EnvVecs,
    offset: *mut EnvVecs,
    rotation: *mut EnvVecs,
) {
    // SAFETY: caller contract.
    unsafe {
        env_update(
            usize::try_from(n).unwrap_or(0),
            dt,
            [&*grav, &*wind, &*rotv],
            &mut *offset,
            &mut *rotation,
        );
    }
}

/// Fills `num` particles: `pos` 16 floats each, `uv0` and `uv1` 12 floats each.
///
/// # Safety
/// `particle_size` and `sub_div` valid; arrays address `num` particles' floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_env_setup(
    box_size: f32,
    num: i32,
    particle_size: *const Vector2f,
    sub_div: *const Vector2l,
    pos: *mut f32,
    uv0: *mut f32,
    uv1: *mut f32,
) {
    let n = usize::try_from(num).unwrap_or(0);
    // SAFETY: caller contract.
    unsafe {
        env_setup(
            box_size,
            *particle_size,
            *sub_div,
            slice_mut(pos, n * 16),
            slice_mut(uv0, n * 12),
            slice_mut(uv1, n * 12),
        );
    }
}

/// # Safety
/// `e` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_pe_sub_div_count(e: *const Emitter) -> usize {
    // SAFETY: caller contract.
    unsafe { &*e }.sub_div_count()
}

/// Writes time, create/collide/pause counters, paused, max, dying (8 floats) for diagnostics.
///
/// # Safety
/// `e` live; `out` addresses 8 floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_pe_debug_state(e: *const Emitter, out: *mut [f32; 8]) {
    // SAFETY: caller contract.
    unsafe { *out = (*e).debug_state() };
}
