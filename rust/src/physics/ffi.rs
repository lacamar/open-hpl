//! C ABI of the physics core, mirrored in `HPL2/core/include/physics/RustPhysics.h`.
//! `Vt` and the parameter structs are built on the C++ stack per call; handles stay owned by C++.

use super::body;
use super::book;
use super::controller::CtrlParams;
use super::host::{H, Vt};
use super::joint::{self, JointState, LimitEffect, MoveParams};
use super::surface::{self, SurfaceParams};
use super::world::BodyFilter;
use crate::types::Vector3f;

/// # Safety
/// `p` is null or a live reference for the call.
unsafe fn get<'a, T>(p: *const T) -> Option<&'a T> {
    // SAFETY: caller contract.
    unsafe { p.as_ref() }
}

/// # Safety
/// `p` is null or a valid `Vector3f`.
unsafe fn vec3(p: *const Vector3f) -> Vector3f {
    // SAFETY: caller contract.
    unsafe { get(p) }.copied().unwrap_or_default()
}

/// # Safety
/// `vt`, `params`, `pos` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_phys_surface_on_impact(
    vt: *const Vt,
    params: *const SurfaceParams,
    speed: f32,
    pos: *const Vector3f,
    body: H,
) {
    // SAFETY: caller contract.
    if let (Some(vt), Some(p)) = unsafe { (get(vt), get(params)) } {
        // SAFETY: caller contract.
        surface::on_impact(&vt, p, speed, unsafe { vec3(pos) }, body);
    }
}

/// # Safety
/// `vt`, `params`, `pos` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_phys_surface_on_slide(
    vt: *const Vt,
    params: *const SurfaceParams,
    speed: f32,
    pos: *const Vector3f,
    contacts: i32,
    body: H,
    against: H,
) {
    // SAFETY: caller contract.
    if let (Some(vt), Some(p)) = unsafe { (get(vt), get(params)) } {
        // SAFETY: caller contract.
        surface::on_slide(&vt, p, speed, unsafe { vec3(pos) }, contacts, body, against);
    }
}

/// # Safety
/// `vt`, `params`, `pos` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_phys_surface_create_impact_effect(
    vt: *const Vt,
    params: *const SurfaceParams,
    speed: f32,
    pos: *const Vector3f,
    second: H,
    physics_world: H,
) {
    // SAFETY: caller contract.
    if let (Some(vt), Some(p)) = unsafe { (get(vt), get(params)) } {
        // SAFETY: caller contract.
        surface::create_impact_effect(&vt, p, speed, unsafe { vec3(pos) }, second, physics_world);
    }
}

/// # Safety
/// `vt`, `params` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_phys_surface_update_roll_effect(
    vt: *const Vt,
    params: *const SurfaceParams,
    body: H,
) {
    // SAFETY: caller contract.
    if let (Some(vt), Some(p)) = unsafe { (get(vt), get(params)) } {
        surface::update_roll_effect(&vt, p, body);
    }
}

/// # Safety
/// `vt`, `pos`, `out` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_phys_body_velocity_at_position(
    vt: *const Vt,
    b: H,
    pos: *const Vector3f,
    out: *mut Vector3f,
) {
    // SAFETY: caller contract.
    if let (Some(vt), Some(out)) = unsafe { (get(vt), out.as_mut()) } {
        // SAFETY: caller contract.
        *out = body::velocity_at_position(&vt, b, unsafe { vec3(pos) });
    }
}

/// # Safety
/// `vt`, `force`, `pos`, `out` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_phys_body_torque_from_force(
    vt: *const Vt,
    b: H,
    force: *const Vector3f,
    pos: *const Vector3f,
    out: *mut Vector3f,
) {
    // SAFETY: caller contract.
    if let (Some(vt), Some(out)) = unsafe { (get(vt), out.as_mut()) } {
        // SAFETY: caller contract.
        *out = body::torque_from_force_at_position(&vt, b, unsafe { vec3(force) }, unsafe {
            vec3(pos)
        });
    }
}

/// # Safety
/// `vt` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_phys_body_update_before(vt: *const Vt, b: H, book: u64) -> bool {
    // SAFETY: caller contract.
    unsafe { get(vt) }.is_some_and(|vt| body::update_before_simulate_bk(&vt, b, book))
}

/// # Safety
/// `vt` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_phys_body_update_after(
    vt: *const Vt,
    b: H,
    time_step: f32,
    book: u64,
) {
    // SAFETY: caller contract.
    if let Some(vt) = unsafe { get(vt) } {
        body::update_after_simulate_bk(&vt, b, time_step, book);
    }
}

/// # Safety
/// `vt`, `v` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_phys_body_static_linear_move(
    vt: *const Vt,
    b: H,
    v: *const Vector3f,
    book: u64,
) {
    // SAFETY: caller contract.
    if let Some(vt) = unsafe { get(vt) } {
        // SAFETY: caller contract.
        body::static_linear_move_bk(&vt, b, unsafe { vec3(v) }, book);
    }
}

/// # Safety
/// `vt`, `v` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_phys_body_static_angular_move(
    vt: *const Vt,
    b: H,
    v: *const Vector3f,
    book: u64,
) {
    // SAFETY: caller contract.
    if let Some(vt) = unsafe { get(vt) } {
        // SAFETY: caller contract.
        body::static_angular_move_bk(&vt, b, unsafe { vec3(v) }, book);
    }
}

/// # Safety
/// `vt`, `effect` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_phys_joint_on_limit(
    vt: *const Vt,
    joint: H,
    is_max: bool,
    effect: *const LimitEffect,
) {
    // SAFETY: caller contract.
    if let (Some(vt), Some(e)) = unsafe { (get(vt), get(effect)) } {
        joint::on_limit(&vt, joint, is_max, e);
    }
}

/// # Safety
/// `step_count` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_phys_joint_limit_auto_sleep(
    enabled: bool,
    sleep_dist: f32,
    num_steps: i32,
    step_count: *mut i32,
    min: f32,
    max: f32,
    dist: f32,
) -> bool {
    // SAFETY: caller contract.
    unsafe { step_count.as_mut() }
        .is_some_and(|c| joint::limit_auto_sleep(enabled, sleep_dist, num_steps, c, min, max, dist))
}

const _: () = {
    use std::mem::size_of;
    assert!(size_of::<CtrlParams>() == 88 && size_of::<SurfaceParams>() == 112);
    assert!(size_of::<JointState>() == 216 && size_of::<MoveParams>() == 56);
    assert!(size_of::<LimitEffect>() == 24 && size_of::<BodyFilter>() == 24);
};

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15_body_new() -> u64 {
    book::body_new()
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15_body_free(book: u64) {
    book::body_free(book);
}

/// Field ids: 0 has_slide, 1 has_collision, 2 static_movement, 3 disable_after_sim.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15_body_flag_get(book: u64, id: i32) -> bool {
    book::body_get(book).is_some_and(|k| match id {
        0 => k.has_slide,
        1 => k.has_collision,
        2 => k.static_movement,
        _ => k.disable_after_sim,
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15_body_flag_set(book: u64, id: i32, v: bool) {
    book::body_with(book, |k| match id {
        0 => k.has_slide = v,
        1 => k.has_collision = v,
        2 => k.static_movement = v,
        _ => k.disable_after_sim = v,
    });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15_world_free(book: u64) {
    book::world_free(book);
}
