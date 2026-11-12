//! C ABI of the physics core, mirrored in `HPL2/core/include/physics/RustPhysics.h`.
//! `Vt` and the parameter structs are built on the C++ stack per call; handles stay owned by C++.

use super::body;
use super::book;
use super::controller::{self, CtrlParams};
use super::host::{H, Str, Vt};
use super::joint::{self, JointState, LimitEffect, MoveParams};
use super::pid::Pid;
use super::surface::{self, SurfaceParams};
use super::surface_xml::{self, SurfaceDef};
use super::world::{self, BodyFilter, ImpactCounts};
use crate::types::Vector3f;
use std::ffi::c_void;

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

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_phys_pid_new() -> *mut Pid {
    Box::into_raw(Box::new(Pid::new()))
}

/// # Safety
/// `p` comes from `ohpl_phys_pid_new` and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_phys_pid_free(p: *mut Pid) {
    if !p.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(p) });
    }
}

/// # Safety
/// `p` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_phys_pid_set_error_num(p: *mut Pid, n: i32) {
    // SAFETY: caller contract.
    if let Some(p) = unsafe { p.as_mut() } {
        p.set_error_num(n);
    }
}

/// # Safety
/// `p` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_phys_pid_reset(p: *mut Pid) {
    // SAFETY: caller contract.
    if let Some(p) = unsafe { p.as_mut() } {
        p.reset();
    }
}

/// # Safety
/// `vt`, `params` valid; `pid` live and not borrowed elsewhere.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_phys_ctrl_update(
    vt: *const Vt,
    params: *const CtrlParams,
    pid: *mut Pid,
    time_step: f32,
) {
    // SAFETY: caller contract.
    if let (Some(vt), Some(p)) = unsafe { (get(vt), get(params)) } {
        // SAFETY: caller contract.
        unsafe { controller::update(&vt, p, pid, time_step) };
    }
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

pub type SurfaceSink = unsafe extern "C" fn(*mut c_void, *const SurfaceDef);

/// # Safety
/// `data` points to `len` readable bytes (or `len` is 0); `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_phys_surface_parse(
    data: *const u8,
    len: usize,
    read_ok: bool,
    sink: SurfaceSink,
    ctx: *mut c_void,
) -> bool {
    // SAFETY: caller contract.
    let bytes = unsafe { Str { p: data, n: len }.bytes() };
    surface_xml::parse(bytes, read_ok, |def| {
        // SAFETY: caller contract on `sink`; `def` lives for the call.
        unsafe { sink(ctx, def) }
    })
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
/// `vt`, `params` valid; `state` is a live `JointState` the callbacks do not touch.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_phys_joint_update(
    vt: *const Vt,
    params: *const MoveParams,
    state: *mut JointState,
    book: u64,
) -> bool {
    // SAFETY: caller contract.
    match unsafe { (get(vt), get(params), state.as_mut()) } {
        (Some(vt), Some(p), Some(s)) => joint::on_physics_update_bk(&vt, p, s, book),
        _ => false,
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
/// `vt` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_phys_joint_check_breakage(
    vt: *const Vt,
    joint: H,
    breakable: bool,
    broken: bool,
    break_force: f32,
    sound: Str,
) -> bool {
    // SAFETY: caller contract.
    unsafe { get(vt) }
        .is_some_and(|vt| joint::check_breakage(&vt, joint, breakable, broken, break_force, sound))
}

/// # Safety
/// All pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_phys_joint_calc_sound_freq(
    params: *const MoveParams,
    speed: f32,
    freq: *mut f32,
    volume: *mut f32,
) {
    // SAFETY: caller contract.
    if let (Some(p), Some(f), Some(v)) = unsafe { (get(params), freq.as_mut(), volume.as_mut()) } {
        (*f, *v) = joint::calc_sound_freq(p, speed);
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

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_phys_impacts_new() -> *mut ImpactCounts {
    Box::into_raw(Box::default())
}

/// # Safety
/// `c` comes from `ohpl_phys_impacts_new` and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_phys_impacts_free(c: *mut ImpactCounts) {
    if !c.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(c) });
    }
}

/// # Safety
/// `c` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_phys_impacts_can_play(c: *const ImpactCounts, max: i32) -> bool {
    // SAFETY: caller contract.
    unsafe { get(c) }.is_some_and(|c| c.can_play(max))
}

/// # Safety
/// `c` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_phys_impacts_len(c: *const ImpactCounts) -> usize {
    // SAFETY: caller contract.
    unsafe { get(c) }.map_or(0, ImpactCounts::len)
}

/// # Safety
/// `c` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_phys_impacts_add(c: *mut ImpactCounts) {
    // SAFETY: caller contract.
    if let Some(c) = unsafe { c.as_mut() } {
        c.add();
    }
}

/// # Safety
/// `c` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_phys_impacts_update(
    c: *mut ImpactCounts,
    time_step: f32,
    duration: f32,
) {
    // SAFETY: caller contract.
    if let Some(c) = unsafe { c.as_mut() } {
        c.update(time_step, duration);
    }
}

/// # Safety
/// `f` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_phys_world_body_passes(f: *const BodyFilter) -> bool {
    // SAFETY: caller contract.
    unsafe { get(f) }.is_some_and(world::body_passes)
}

/// # Safety
/// `push` valid, `normal` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_phys_world_merge_push(
    push: *mut Vector3f,
    normal: *const Vector3f,
    depth: f32,
) {
    // SAFETY: caller contract.
    if let Some(push) = unsafe { push.as_mut() } {
        // SAFETY: caller contract.
        *push = world::merge_push(*push, unsafe { vec3(normal) }, depth);
    }
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
pub extern "C" fn ohpl_h15_joint_new() -> u64 {
    book::joint_new()
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15_joint_free(book: u64) {
    book::joint_free(book);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15_joint_limit_auto_sleep(
    book: u64,
    enabled: bool,
    sleep_dist: f32,
    num_steps: i32,
    min: f32,
    max: f32,
    dist: f32,
) -> bool {
    book::joint_with(book, |k| {
        joint::limit_auto_sleep(
            enabled,
            sleep_dist,
            num_steps,
            &mut k.limit_step_count,
            min,
            max,
            dist,
        )
    })
    .unwrap_or(false)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15_world_new() -> u64 {
    book::world_new()
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15_world_free(book: u64) {
    book::world_free(book);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15_world_update_insert(book: u64, body: usize) -> bool {
    book::world_with(book, |w| w.insert(body)).unwrap_or(false)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15_world_update_remove(book: u64, body: usize) -> bool {
    book::world_with(book, |w| w.remove(body)).unwrap_or(false)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15_world_update_clear(book: u64) {
    book::world_with(book, |w| w.clear());
}

/// # Safety
/// `ctx` is valid for the callbacks.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h15_world_update_before(
    book: u64,
    ctx: *mut c_void,
    before: world::BodyBefore,
    remove: world::BodyAfter,
) {
    // SAFETY: caller contract.
    unsafe { world::update_before(book, ctx, before, remove) };
}

/// # Safety
/// `ctx` is valid for the callback.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h15_world_update_after(
    book: u64,
    ctx: *mut c_void,
    after: world::BodyAfter,
) {
    // SAFETY: caller contract.
    unsafe { world::update_after(book, ctx, after) };
}
