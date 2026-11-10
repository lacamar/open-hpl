//! C ABI of `character.rs` and `rope.rs`, mirrored in `HPL2/core/include/physics/RustCharacter.h`.

use super::character::{self, CharHost, CharState, S};
use super::rope::{self, Body, P, Particle, R, RopeHost, RopeState};
use crate::types::{Matrixf, Vector3f};
use std::ffi::c_void;

type Ctx = *mut c_void;

#[repr(C)]
pub struct CharVt {
    pub ctx: Ctx,
    pub world_gravity: unsafe extern "C" fn(Ctx) -> Vector3f,
    pub shape_radius: unsafe extern "C" fn(Ctx) -> f32,
    pub shape_size_y: unsafe extern "C" fn(Ctx, i32) -> f32,
    pub check_collision: unsafe extern "C" fn(Ctx, *mut Vector3f, Vector3f, i32, i32) -> bool,
    pub cast_ray: unsafe extern "C" fn(Ctx, Vector3f, Vector3f, *mut f32, *mut Vector3f) -> bool,
    pub on_hit_ground: unsafe extern "C" fn(Ctx),
    pub pre_connection: unsafe extern "C" fn(Ctx, f32),
    pub clear_gravity_attachment: unsafe extern "C" fn(Ctx),
    pub update_gravity_attachment: unsafe extern "C" fn(Ctx),
    pub update_body: unsafe extern "C" fn(Ctx),
    pub update_character_connection: unsafe extern "C" fn(Ctx, f32),
    pub update_body_connection: unsafe extern "C" fn(Ctx, f32),
    pub update_camera: unsafe extern "C" fn(Ctx),
    pub update_entity: unsafe extern "C" fn(Ctx),
    pub post_connection: unsafe extern "C" fn(Ctx, f32),
    pub update_safe_positions: unsafe extern "C" fn(Ctx, f32),
}

// SAFETY (every forwarder): the C++ side fills `CharVt` with live function pointers and `ctx`.
impl CharHost for &CharVt {
    fn world_gravity(&self) -> Vector3f {
        unsafe { (self.world_gravity)(self.ctx) }
    }
    fn shape_radius(&self) -> f32 {
        unsafe { (self.shape_radius)(self.ctx) }
    }
    fn shape_size_y(&self, idx: i32) -> f32 {
        unsafe { (self.shape_size_y)(self.ctx, idx) }
    }
    fn check_collision(&self, push: &mut Vector3f, pos: Vector3f, callback: i32, idx: i32) -> bool {
        unsafe { (self.check_collision)(self.ctx, push, pos, callback, idx) }
    }
    fn cast_ray(&self, start: Vector3f, end: Vector3f, d: &mut f32, n: &mut Vector3f) -> bool {
        unsafe { (self.cast_ray)(self.ctx, start, end, d, n) }
    }
    fn on_hit_ground(&self) {
        unsafe { (self.on_hit_ground)(self.ctx) }
    }
    fn pre_connection(&self, dt: f32) {
        unsafe { (self.pre_connection)(self.ctx, dt) }
    }
    fn clear_gravity_attachment(&self) {
        unsafe { (self.clear_gravity_attachment)(self.ctx) }
    }
    fn update_gravity_attachment(&self) {
        unsafe { (self.update_gravity_attachment)(self.ctx) }
    }
    fn update_body(&self) {
        unsafe { (self.update_body)(self.ctx) }
    }
    fn update_character_connection(&self, dt: f32) {
        unsafe { (self.update_character_connection)(self.ctx, dt) }
    }
    fn update_body_connection(&self, dt: f32) {
        unsafe { (self.update_body_connection)(self.ctx, dt) }
    }
    fn update_camera(&self) {
        unsafe { (self.update_camera)(self.ctx) }
    }
    fn update_entity(&self) {
        unsafe { (self.update_entity)(self.ctx) }
    }
    fn post_connection(&self, dt: f32) {
        unsafe { (self.post_connection)(self.ctx, dt) }
    }
    fn update_safe_positions(&self, dt: f32) {
        unsafe { (self.update_safe_positions)(self.ctx, dt) }
    }
}

/// # Safety
/// `vt` valid for the call; `st` live and not borrowed by Rust elsewhere (C++ callbacks may write it).
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_char_update(
    vt: *const CharVt,
    st: *mut CharState,
    dt: f32,
    hpl3: bool,
) {
    // SAFETY: caller contract.
    if let Some(vt) = unsafe { vt.as_ref() } {
        // SAFETY: caller contract.
        let s = unsafe { S::new(st) };
        character::update(&vt, &s, dt, hpl3);
    }
}

/// # Safety
/// `st` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_char_update_move_matrix(st: *mut CharState) {
    // SAFETY: caller contract.
    let s = unsafe { S::new(st) };
    character::update_move_matrix(&s);
}

/// # Safety
/// `vt` valid; `push` valid for writes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_char_check_fits(
    vt: *const CharVt,
    pos: Vector3f,
    feet: bool,
    idx: i32,
    epsilon: f32,
    push: *mut Vector3f,
) -> bool {
    // SAFETY: caller contract.
    let (Some(vt), Some(push)) = (unsafe { vt.as_ref() }, unsafe { push.as_mut() }) else {
        return false;
    };
    let (fits, p) = character::check_character_fits(&vt, pos, feet, idx, epsilon);
    *push = p;
    fits
}

#[repr(C)]
pub struct RopeVt {
    pub ctx: Ctx,
    pub particles: unsafe extern "C" fn(Ctx, *mut usize) -> *const *mut Particle,
    pub check_specific_sleeping: unsafe extern "C" fn(Ctx) -> bool,
    pub check_specific_awake: unsafe extern "C" fn(Ctx) -> bool,
    pub set_sleeping: unsafe extern "C" fn(Ctx, bool),
    pub set_total_length: unsafe extern "C" fn(Ctx, f32),
    pub update_motor_sound: unsafe extern "C" fn(Ctx, f32),
    pub cast_particle_ray:
        unsafe extern "C" fn(Ctx, Vector3f, Vector3f, *mut Vector3f, *mut Vector3f) -> bool,
    pub body_local_matrix: unsafe extern "C" fn(Body, *mut Matrixf),
    pub body_local_position: unsafe extern "C" fn(Body) -> Vector3f,
    pub body_mass_centre: unsafe extern "C" fn(Body) -> Vector3f,
    pub body_inertia_matrix: unsafe extern "C" fn(Body, *mut Matrixf),
    pub body_mass: unsafe extern "C" fn(Body) -> f32,
    pub body_add_force: unsafe extern "C" fn(Body, Vector3f),
    pub body_add_torque: unsafe extern "C" fn(Body, Vector3f),
    pub pid_output: unsafe extern "C" fn(Ctx, i32, f32, f32, Vector3f, f32) -> Vector3f,
}

// SAFETY (every forwarder): the C++ side fills `RopeVt` with live function pointers and `ctx`.
impl RopeHost for &RopeVt {
    fn particles(&self) -> (*const *mut Particle, usize) {
        let mut n = 0usize;
        let p = unsafe { (self.particles)(self.ctx, &mut n) };
        (p, n)
    }
    fn check_specific_sleeping(&self) -> bool {
        unsafe { (self.check_specific_sleeping)(self.ctx) }
    }
    fn check_specific_awake(&self) -> bool {
        unsafe { (self.check_specific_awake)(self.ctx) }
    }
    fn set_sleeping(&self, sleeping: bool) {
        unsafe { (self.set_sleeping)(self.ctx, sleeping) }
    }
    fn set_total_length(&self, len: f32) {
        unsafe { (self.set_total_length)(self.ctx, len) }
    }
    fn update_motor_sound(&self, vel: f32) {
        unsafe { (self.update_motor_sound)(self.ctx, vel) }
    }
    fn cast_particle_ray(
        &self,
        start: Vector3f,
        end: Vector3f,
        pos: &mut Vector3f,
        normal: &mut Vector3f,
    ) -> bool {
        unsafe { (self.cast_particle_ray)(self.ctx, start, end, pos, normal) }
    }
    fn body_local_matrix(&self, b: Body) -> Matrixf {
        let mut m = Matrixf::default();
        unsafe { (self.body_local_matrix)(b, &mut m) };
        m
    }
    fn body_local_position(&self, b: Body) -> Vector3f {
        unsafe { (self.body_local_position)(b) }
    }
    fn body_mass_centre(&self, b: Body) -> Vector3f {
        unsafe { (self.body_mass_centre)(b) }
    }
    fn body_inertia_matrix(&self, b: Body) -> Matrixf {
        let mut m = Matrixf::default();
        unsafe { (self.body_inertia_matrix)(b, &mut m) };
        m
    }
    fn body_mass(&self, b: Body) -> f32 {
        unsafe { (self.body_mass)(b) }
    }
    fn body_add_force(&self, b: Body, v: Vector3f) {
        unsafe { (self.body_add_force)(b, v) }
    }
    fn body_add_torque(&self, b: Body, v: Vector3f) {
        unsafe { (self.body_add_torque)(b, v) }
    }
    fn pid_output(&self, idx: i32, p: f32, d: f32, error: Vector3f, dt: f32) -> Vector3f {
        unsafe { (self.pid_output)(self.ctx, idx, p, d, error, dt) }
    }
}

/// # Safety
/// `vt` valid; `st` live and not borrowed by Rust elsewhere; particle list live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rope_update_before(vt: *const RopeVt, st: *mut RopeState, dt: f32) {
    // SAFETY: caller contract.
    if let Some(vt) = unsafe { vt.as_ref() } {
        // SAFETY: caller contract.
        let r = unsafe { R::new(st) };
        rope::update_before_simulate(&vt, &r, dt);
    }
}

/// # Safety
/// As `ohpl_rope_update_before`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rope_update_after(vt: *const RopeVt, st: *mut RopeState) {
    // SAFETY: caller contract.
    if let Some(vt) = unsafe { vt.as_ref() } {
        // SAFETY: caller contract.
        let r = unsafe { R::new(st) };
        rope::update_after_simulate(&vt, &r);
    }
}

/// # Safety
/// As `ohpl_rope_update_before`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rope_update_constraints(vt: *const RopeVt, st: *mut RopeState) {
    // SAFETY: caller contract.
    if let Some(vt) = unsafe { vt.as_ref() } {
        // SAFETY: caller contract.
        let r = unsafe { R::new(st) };
        rope::update_constraints_public(&vt, &r);
    }
}

/// # Safety
/// `p` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_verlet_update_movement(
    p: *mut Particle,
    damping_mul: f32,
    gravity: Vector3f,
    dt: f32,
) {
    rope::particle_update_movement(P(p), damping_mul, gravity, dt);
}

/// # Safety
/// `p1`, `p2` live and distinct.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_verlet_length_constraint(
    p1: *mut Particle,
    p2: *mut Particle,
    length: f32,
    stiffness: f32,
) {
    rope::length_constraint(P(p1), P(p2), length, stiffness);
}

/// # Safety
/// `p1`, `p2` live and distinct.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_verlet_length_constraint_range(
    p1: *mut Particle,
    p2: *mut Particle,
    min: f32,
    max: f32,
    stiffness: f32,
) {
    rope::length_constraint_range(P(p1), P(p2), min, max, stiffness);
}

/// # Safety
/// `p1`, `p2` live and distinct.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_verlet_length_constraint_stretch(
    p1: *mut Particle,
    p2: *mut Particle,
    length: f32,
    stiffness: f32,
) {
    rope::length_constraint_stretch(P(p1), P(p2), length, stiffness);
}

type CastFn = unsafe extern "C" fn(Ctx, Vector3f, Vector3f, *mut Vector3f, *mut Vector3f) -> bool;

/// # Safety
/// `cast` and `ctx` valid for the call; `st` live and not borrowed by Rust elsewhere; `part` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_verlet_collision_constraint(
    cast: CastFn,
    ctx: Ctx,
    st: *mut RopeState,
    part: *mut Particle,
    prev: Vector3f,
    radius: f32,
) {
    // SAFETY: caller contract.
    let r = unsafe { R::new(st) };
    // SAFETY: caller contract.
    let cast = |s, e, p: &mut Vector3f, n: &mut Vector3f| unsafe { cast(ctx, s, e, p, n) };
    rope::collision_constraint(&cast, &r, P(part), prev, radius);
}
