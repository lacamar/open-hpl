//! `iPhysicsRope::UpdateBeforeSimulate` / `UpdateAfterSimulate` and the verlet container they drive.
//!
//! Particles and the shared state stay C++-owned (list of heap particles, camera-style callbacks),
//! so both are reached through raw pointers and every host call may rewrite them.

use super::body::rotation;
use crate::math::{cross3, div3, dot3, fma, len3, matrix_mul_vec, mul3, v3};
use crate::types::{Matrixf, Vector3f};
use std::ffi::c_void;

pub type Body = usize;

const ZERO: Vector3f = v3(0.0, 0.0, 0.0);

/// `hpl::cVerletParticle`.
#[repr(C)]
pub struct Particle {
    pub container: *mut c_void,
    pub position: Vector3f,
    pub prev_position: Vector3f,
    pub smooth_position: Vector3f,
    pub force: Vector3f,
    pub inv_mass: f32,
}

/// `hpl::cPhysicsRopeAttachment`.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Attachment {
    pub body: Body,
    pub particle: *mut Particle,
    pub body_local_pos: Vector3f,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct RopeState {
    pub attached: [Attachment; 2],
    pub gravity_force: Vector3f,
    pub damping_mul: f32,
    pub particle_radius: f32,
    pub slide_amount: f32,
    pub sleep_check_count: f32,
    pub sleep_check_time: f32,
    pub sleep_check_sqr_limit: f32,
    pub motor_wanted_length: f32,
    pub motor_speed_mul: f32,
    pub motor_max_speed: f32,
    pub motor_min_speed: f32,
    pub auto_move_acc: f32,
    pub auto_move_max_speed: f32,
    pub auto_move_speed: f32,
    pub total_length: f32,
    pub min_total_length: f32,
    pub max_total_length: f32,
    pub segment_length: f32,
    pub first_segment_length: f32,
    pub strength: f32,
    pub stiffness: f32,
    pub sleep_count: i32,
    pub sleep_max_count: i32,
    pub update_count: i32,
    pub max_iterations: i32,
    pub sleeping: bool,
    pub collide: bool,
    pub physics_active: bool,
    pub motor_active: bool,
    pub auto_move_active: bool,
    pub collide_attachments: bool,
    pub has_updated: bool,
}

/// Engine calls made while updating a rope; `ctx` is the C++ `iPhysicsRope`.
pub trait RopeHost {
    /// Current particle list, front to back; invalidated by `set_total_length`.
    fn particles(&self) -> (*const *mut Particle, usize);
    fn check_specific_sleeping(&self) -> bool;
    fn check_specific_awake(&self) -> bool;
    fn set_sleeping(&self, sleeping: bool);
    fn set_total_length(&self, len: f32);
    fn update_motor_sound(&self, vel: f32);
    /// Casts the particle ray; returns whether it intersected, with the callback's last hit data.
    fn cast_particle_ray(
        &self,
        start: Vector3f,
        end: Vector3f,
        pos: &mut Vector3f,
        normal: &mut Vector3f,
    ) -> bool;
    fn body_local_matrix(&self, b: Body) -> Matrixf;
    fn body_local_position(&self, b: Body) -> Vector3f;
    fn body_mass_centre(&self, b: Body) -> Vector3f;
    fn body_inertia_matrix(&self, b: Body) -> Matrixf;
    fn body_mass(&self, b: Body) -> f32;
    fn body_add_force(&self, b: Body, v: Vector3f);
    fn body_add_torque(&self, b: Body, v: Vector3f);
    fn pid_output(&self, idx: i32, p: f32, d: f32, error: Vector3f, dt: f32) -> Vector3f;
}

pub struct R(*mut RopeState);

macro_rules! fields {
    ($($f:ident, $set:ident : $t:ty;)*) => {
        #[allow(dead_code)] // accessors only some fields need
        impl R {
            $(
                fn $f(&self) -> $t {
                    // SAFETY: `R::new` caller guarantees a live state; raw read, no reference.
                    unsafe { (*self.0).$f }
                }
                fn $set(&self, v: $t) {
                    // SAFETY: as above; raw write, no reference.
                    unsafe { (*self.0).$f = v }
                }
            )*
        }
    };
}

fields! {
    attached, set_attached: [Attachment; 2];
    gravity_force, set_gravity_force: Vector3f;
    damping_mul, set_damping_mul: f32;
    particle_radius, set_particle_radius: f32;
    slide_amount, set_slide_amount: f32;
    sleep_check_count, set_sleep_check_count: f32;
    sleep_check_time, set_sleep_check_time: f32;
    sleep_check_sqr_limit, set_sleep_check_sqr_limit: f32;
    motor_wanted_length, set_motor_wanted_length: f32;
    motor_speed_mul, set_motor_speed_mul: f32;
    motor_max_speed, set_motor_max_speed: f32;
    motor_min_speed, set_motor_min_speed: f32;
    auto_move_acc, set_auto_move_acc: f32;
    auto_move_max_speed, set_auto_move_max_speed: f32;
    auto_move_speed, set_auto_move_speed: f32;
    total_length, set_total_length_field: f32;
    min_total_length, set_min_total_length: f32;
    max_total_length, set_max_total_length: f32;
    segment_length, set_segment_length: f32;
    first_segment_length, set_first_segment_length: f32;
    strength, set_strength: f32;
    stiffness, set_stiffness: f32;
    sleep_count, set_sleep_count: i32;
    sleep_max_count, set_sleep_max_count: i32;
    update_count, set_update_count: i32;
    max_iterations, set_max_iterations: i32;
    sleeping, set_sleeping_field: bool;
    collide, set_collide: bool;
    physics_active, set_physics_active: bool;
    motor_active, set_motor_active: bool;
    auto_move_active, set_auto_move_active: bool;
    collide_attachments, set_collide_attachments: bool;
    has_updated, set_has_updated: bool;
}

impl R {
    /// # Safety
    /// `p` points to a live `RopeState` for the lifetime of the returned value; C++ callbacks may
    /// write it between calls, so no reference to it may be held.
    pub unsafe fn new(p: *mut RopeState) -> Self {
        R(p)
    }
}

/// Raw view of one C++ particle.
#[derive(Clone, Copy, PartialEq)]
pub struct P(pub *mut Particle);

macro_rules! particle_fields {
    ($($f:ident, $set:ident : $t:ty;)*) => {
        #[allow(dead_code)] // accessors only some fields need
        impl P {
            $(
                fn $f(&self) -> $t {
                    // SAFETY: `P` is only built from live list entries; raw read, no reference.
                    unsafe { (*self.0).$f }
                }
                fn $set(&self, v: $t) {
                    // SAFETY: as above; raw write, no reference.
                    unsafe { (*self.0).$f = v }
                }
            )*
        }
    };
}

particle_fields! {
    position, set_position: Vector3f;
    prev_position, set_prev_position: Vector3f;
    smooth_position, set_smooth_position: Vector3f;
    force, set_force: Vector3f;
    inv_mass, set_inv_mass: f32;
}

fn add3(a: Vector3f, b: Vector3f) -> Vector3f {
    v3(a.x + b.x, a.y + b.y, a.z + b.z)
}

fn sub3(a: Vector3f, b: Vector3f) -> Vector3f {
    v3(a.x - b.x, a.y - b.y, a.z - b.z)
}

fn max(a: f32, b: f32) -> f32 {
    if a > b { a } else { b }
}

fn list<H: RopeHost>(h: &H) -> Vec<P> {
    let (p, n) = h.particles();
    if p.is_null() {
        return Vec::new();
    }
    // SAFETY: the C++ cache holds `n` live particle pointers until the next list mutation.
    unsafe { std::slice::from_raw_parts(p, n) }
        .iter()
        .map(|&q| P(q))
        .collect()
}

pub fn particle_update_movement(p: P, damping_mul: f32, gravity: Vector3f, dt: f32) {
    let f = p.force();
    let im = p.inv_mass();
    let acc = if im == 0.0 {
        ZERO
    } else {
        v3(
            fma(f.x, im, gravity.x),
            fma(f.y, im, gravity.y),
            fma(f.z, im, gravity.z),
        )
    };
    p.set_force(ZERO);
    let temp = p.position();
    let pos = temp;
    let prev = p.prev_position();
    let step = |pos: f32, prev: f32, acc: f32| {
        let damped = fma(pos, damping_mul, -(prev * damping_mul));
        let mut t = fma(acc * dt, dt, damped);
        t += pos;
        t
    };
    p.set_position(v3(
        step(pos.x, prev.x, acc.x),
        step(pos.y, prev.y, acc.y),
        step(pos.z, prev.z, acc.z),
    ));
    p.set_prev_position(temp);
}

pub fn length_constraint(p1: P, p2: P, length: f32, stiffness: f32) {
    let delta = sub3(p2.position(), p1.position());
    let dist = len3(delta);
    let diff = stiffness * (dist - length) / (dist * max(0.0001, p1.inv_mass() + p2.inv_mass()));
    let (i1, i2) = (p1.inv_mass(), p2.inv_mass());
    let a = p1.position();
    p1.set_position(v3(
        fma(delta.x * diff, i1, a.x),
        fma(delta.y * diff, i1, a.y),
        fma(delta.z * diff, i1, a.z),
    ));
    let b = p2.position();
    p2.set_position(v3(
        fma(-(delta.x * diff), i2, b.x),
        fma(-(delta.y * diff), i2, b.y),
        fma(-(delta.z * diff), i2, b.z),
    ));
}

pub fn length_constraint_range(p1: P, p2: P, min: f32, max: f32, stiffness: f32) {
    let dist = len3(sub3(p2.position(), p1.position()));
    if dist < min {
        length_constraint(p1, p2, min, stiffness);
    } else if dist > max {
        length_constraint(p1, p2, max, stiffness);
    }
}

pub fn length_constraint_stretch(p1: P, p2: P, length: f32, stiffness: f32) {
    let delta = sub3(p1.position(), p2.position());
    let dist = len3(delta);
    let diff = (dist - length) / (dist * max(0.0001, p2.inv_mass()));
    let i2 = p2.inv_mass();
    let b = p2.position();
    p2.set_position(v3(
        fma(delta.x * diff * i2, stiffness, b.x),
        fma(delta.y * diff * i2, stiffness, b.y),
        fma(delta.z * diff * i2, stiffness, b.z),
    ));
}

pub type Cast<'a> = &'a dyn Fn(Vector3f, Vector3f, &mut Vector3f, &mut Vector3f) -> bool;

pub fn collision_constraint(cast: Cast, r: &R, part: P, prev: Vector3f, radius: f32) {
    if !r.collide() {
        return;
    }
    let pos = part.position();
    let diff = sub3(pos, prev);
    if diff.x == 0.0 && diff.y == 0.0 && diff.z == 0.0 {
        return;
    }
    let length = len3(diff);
    let dir = div3(diff, length);
    // gcc shares dir*radius between start, end and the hit position, so none of them fuse
    let off = v3(dir.x * radius, dir.y * radius, dir.z * radius);
    let start = sub3(prev, off);
    let end = add3(pos, off);
    let (mut hit, mut normal) = (ZERO, ZERO);
    if cast(start, end, &mut hit, &mut normal) {
        let p = sub3(hit, off);
        part.set_position(p);
        let k = length * r.slide_amount();
        part.set_prev_position(v3(
            fma(normal.x, k, p.x),
            fma(normal.y, k, p.y),
            fma(normal.z, k, p.z),
        ));
    }
}

fn pre_update<H: RopeHost>(h: &H, r: &R, dt: f32) {
    if r.sleeping() && h.check_specific_awake() {
        h.set_sleeping(false);
    }
    if r.sleeping() {
        return;
    }
    r.set_update_count(r.update_count().wrapping_add(1));
    r.set_sleep_check_count(r.sleep_check_count() + dt);
    if r.sleep_check_count() < r.sleep_check_time() {
        return;
    }
    let mut all_sleeping = true;
    for p in list(h) {
        let d = sub3(p.prev_position(), p.position());
        if dot3(d, d) > r.sleep_check_sqr_limit() {
            all_sleeping = false;
            break;
        }
    }
    if all_sleeping {
        all_sleeping = h.check_specific_sleeping();
    }
    if all_sleeping {
        r.set_sleep_count(r.sleep_count().wrapping_add(1));
        if r.sleep_max_count() <= r.sleep_count() {
            h.set_sleeping(true);
        }
    } else {
        r.set_sleep_count(0);
    }
}

fn update_motor_and_auto_move<H: RopeHost>(h: &H, r: &R, dt: f32) {
    let mut vel = 0.0;
    if r.motor_active() {
        let mut total = r.total_length();
        let dir = r.motor_wanted_length() - r.total_length();
        vel = dir * r.motor_speed_mul();
        if vel > r.motor_max_speed() {
            vel = r.motor_max_speed();
        }
        if vel < r.motor_min_speed() {
            vel = r.motor_min_speed();
        }
        total = fma(vel, dt, total);
        h.set_total_length(total);
        if (r.motor_wanted_length() - total).abs() < 0.02 {
            r.set_motor_active(false);
        }
    } else if r.auto_move_active() {
        let mut total = r.total_length();
        r.set_auto_move_speed(fma(r.auto_move_acc(), dt, r.auto_move_speed()));
        if r.auto_move_speed() > r.auto_move_max_speed() {
            r.set_auto_move_speed(r.auto_move_max_speed());
        }
        if r.auto_move_speed() < -r.auto_move_max_speed() {
            r.set_auto_move_speed(-r.auto_move_max_speed());
        }
        total = fma(r.auto_move_speed(), dt, total);
        let mut stop = false;
        if total > r.max_total_length() {
            total = r.max_total_length();
            stop = true;
        }
        if total < r.min_total_length() {
            total = r.min_total_length();
            stop = true;
        }
        if stop {
            r.set_auto_move_speed(0.0);
        }
        vel = r.auto_move_speed();
        h.set_total_length(total);
    }
    h.update_motor_sound(vel);
}

fn update_attached_particle_positions<H: RopeHost>(h: &H, r: &R) {
    for a in r.attached() {
        if a.body == 0 {
            continue;
        }
        let m = h.body_local_matrix(a.body);
        let pos = matrix_mul_vec(&m, a.body_local_pos);
        let part = P(a.particle);
        part.set_position(pos);
        h.set_sleeping(false);
    }
}

fn update_attached_bodies<H: RopeHost>(h: &H, r: &R, dt: f32) {
    for (i, a) in r.attached().into_iter().enumerate() {
        if a.body == 0 {
            continue;
        }
        let body = a.body;
        let wanted = P(a.particle).position();
        let current = matrix_mul_vec(&h.body_local_matrix(body), a.body_local_pos);
        let error = sub3(wanted, current);
        let strength = r.strength();
        let d = strength * 0.1 * r.stiffness();
        let idx = i32::try_from(i).unwrap_or(0);
        let force = h.pid_output(idx, strength, d, error, dt);

        let mut local = sub3(current, h.body_local_position(body));
        let mut centre = h.body_mass_centre(body);
        if centre != ZERO {
            centre = matrix_mul_vec(&rotation(&h.body_local_matrix(body)), centre);
            local = sub3(local, centre);
        }
        let torque = cross3(local, force);
        h.body_add_torque(body, matrix_mul_vec(&h.body_inertia_matrix(body), torque));
        h.body_add_force(body, mul3(force, h.body_mass(body)));
    }
}

fn update_constraints<H: RopeHost>(h: &H, r: &R) {
    let parts = list(h);
    let mut prev: Option<P> = None;
    for (count, &part) in parts.iter().enumerate() {
        if let Some(pp) = prev {
            let len = if count == 1 {
                r.first_segment_length()
            } else {
                r.segment_length()
            };
            length_constraint(pp, part, len, 1.0);
        }
        prev = Some(part);
    }
    if !r.collide() {
        return;
    }
    let (first, last) = (parts.first().copied(), parts.last().copied());
    for &part in &parts {
        if !r.collide_attachments() && (Some(part) == first || Some(part) == last) {
            continue;
        }
        if part.inv_mass() != 0.0 {
            let cast = |s, e, p: &mut Vector3f, n: &mut Vector3f| h.cast_particle_ray(s, e, p, n);
            collision_constraint(&cast, r, part, part.prev_position(), r.particle_radius());
        }
    }
}

pub fn update_constraints_public<H: RopeHost>(h: &H, r: &R) {
    update_constraints(h, r);
}

pub fn update_before_simulate<H: RopeHost>(h: &H, r: &R, dt: f32) {
    r.set_has_updated(true);
    if r.physics_active() {
        pre_update(h, r, dt);
        if r.sleeping() {
            return;
        }
    }
    update_motor_and_auto_move(h, r, dt);
    if r.physics_active() {
        for p in list(h) {
            particle_update_movement(p, r.damping_mul(), r.gravity_force(), dt);
        }
    }
    update_attached_particle_positions(h, r);
    if r.physics_active() {
        for _ in 0..r.max_iterations() {
            update_constraints(h, r);
        }
    }
    update_attached_bodies(h, r, dt);
}

pub fn update_after_simulate<H: RopeHost>(h: &H, r: &R) {
    if r.sleeping() {
        return;
    }
    let parts = list(h);
    let (Some(&first), Some(&last)) = (parts.first(), parts.last()) else {
        return;
    };
    let base = [first.position(), last.position()];
    let mut add = [ZERO; 2];
    for ((slot, a), b) in add.iter_mut().zip(r.attached()).zip(base) {
        if a.body != 0 {
            let wanted = matrix_mul_vec(&h.body_local_matrix(a.body), a.body_local_pos);
            *slot = sub3(wanted, b);
        }
    }
    let max_count = parts.len() as f32 - 1.0;
    let [a0, a1] = add;
    let mut count = 0.0f32;
    for &part in &parts {
        let t = count / max_count;
        let omt = 1.0 - t;
        let add = v3(
            fma(a0.x, omt, a1.x * t),
            fma(a0.y, omt, a1.y * t),
            fma(a0.z, omt, a1.z * t),
        );
        part.set_smooth_position(add3(part.position(), add));
        count += 1.0;
    }
}

/// `iPhysicsRope::BuildRopeParticles` sizing: particles between the end points and the first segment length.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct RopePlan {
    pub wanted: i32,
    pub first_segment: f32,
}

pub fn rope_plan(total: f32, segment: f32) -> RopePlan {
    if segment > 0.0 {
        let first = crate::math::modulus(total, segment);
        RopePlan {
            wanted: ((total / segment) + 0.999999f32) as i32 - 1,
            first_segment: if first == 0.0 { segment } else { first },
        }
    } else {
        RopePlan {
            wanted: 0,
            first_segment: total,
        }
    }
}

/// `start + dir * length` as gcc fuses it.
pub fn rope_particle_pos(start: Vector3f, dir: Vector3f, length: f32) -> Vector3f {
    v3(
        fma(dir.x, length, start.x),
        fma(dir.y, length, start.y),
        fma(dir.z, length, start.z),
    )
}

impl RopeState {
    /// The values `iVerletParticleContainer` and `iPhysicsRope` constructors give a new rope.
    pub fn initial(gravity: Vector3f, start: Vector3f, end: Vector3f) -> Self {
        let total = len3(sub3(end, start));
        Self {
            attached: [Attachment {
                body: 0,
                particle: std::ptr::null_mut(),
                body_local_pos: v3(0.0, 0.0, 0.0),
            }; 2],
            gravity_force: gravity,
            damping_mul: 1.0f32 - 0.001f32,
            particle_radius: 0.05,
            slide_amount: 0.3,
            sleep_check_count: 0.0,
            sleep_check_time: 1.0f32 / 3.0f32,
            sleep_check_sqr_limit: 0.0015f32 * 0.0015f32,
            motor_wanted_length: total,
            motor_speed_mul: 1.0,
            motor_max_speed: 1.0,
            motor_min_speed: -1.0,
            auto_move_acc: 1.0,
            auto_move_max_speed: 1.0,
            auto_move_speed: 0.0,
            total_length: total,
            min_total_length: total,
            max_total_length: total,
            segment_length: 0.0,
            first_segment_length: total,
            strength: 100.0,
            stiffness: 1.0,
            sleep_count: 0,
            sleep_max_count: 3,
            update_count: 0,
            max_iterations: 3,
            sleeping: false,
            collide: false,
            physics_active: true,
            motor_active: false,
            auto_move_active: false,
            collide_attachments: false,
            has_updated: false,
        }
    }
}
