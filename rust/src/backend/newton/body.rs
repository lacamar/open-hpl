//! `cPhysicsBodyNewton`: Newton body lifetime, accessors and the force/torque and transform callbacks.

use super::host::{BodyState, host};
use super::reg::{self, BodyRec, reg};
use super::transposed;
use crate::math::{cross3, matrix_mul_vec, sub3, v3};
use crate::obj::Handle;
use crate::physics::newton_body::clamp_speed;
use crate::sys::newton::{
    NewtonBody, NewtonBodyAddBuoyancyForce, NewtonBodyAddForce, NewtonBodyAddImpulse,
    NewtonBodyAddTorque, NewtonBodyGetAngularDamping, NewtonBodyGetAutoSleep,
    NewtonBodyGetCentreOfMass, NewtonBodyGetContinuousCollisionMode, NewtonBodyGetLinearDamping,
    NewtonBodyGetMassMatrix, NewtonBodyGetOmega, NewtonBodyGetSleepState, NewtonBodyGetUserData,
    NewtonBodyGetVelocity, NewtonBodySetAngularDamping, NewtonBodySetAutoSleep,
    NewtonBodySetCentreOfMass, NewtonBodySetContinuousCollisionMode,
    NewtonBodySetForceAndTorqueCallback, NewtonBodySetFreezeState, NewtonBodySetLinearDamping,
    NewtonBodySetMassMatrix, NewtonBodySetMaterialGroupID, NewtonBodySetMatrix, NewtonBodySetOmega,
    NewtonBodySetTransformCallback, NewtonBodySetUserData, NewtonBodySetVelocity, NewtonCollision,
    NewtonConvexCollisionCalculateInertialMatrix, NewtonCreateBody, NewtonDestroyBody,
};
use crate::types::{Matrixf, Vector3f};
use std::ffi::{c_int, c_void};
use std::ptr;

const ZERO: Vector3f = v3(0.0, 0.0, 0.0);

fn fp(v: &Vector3f) -> *const f32 {
    ptr::from_ref(v).cast()
}

/// Record behind a Newton body's user data.
pub(super) fn of_newton(nb: *const NewtonBody) -> Option<(Handle<BodyRec>, BodyRec)> {
    // SAFETY: `nb` is a live Newton body handed to a callback.
    let bits = unsafe { NewtonBodyGetUserData(nb) } as usize;
    reg::body(u64::try_from(bits).ok()?)
}

unsafe extern "C" fn on_transform(nb: *const NewtonBody, matrix: *const f32, _thread: i32) {
    if let (Some(host), Some((_, rec))) = (host(), of_newton(nb)) {
        // SAFETY: host callbacks are live for the process; `matrix` has 16 floats.
        unsafe { (host.body_set_transform)(rec.cpp, matrix) };
    }
}

unsafe extern "C" fn buoyancy_plane(
    _id: i32,
    context: *mut c_void,
    _matrix: *const f32,
    plane: *mut f32,
) -> c_int {
    // SAFETY: `context` is the four-float plane passed to `NewtonBodyAddBuoyancyForce`.
    unsafe { ptr::copy_nonoverlapping(context.cast::<f32>(), plane, 4) };
    1
}

fn is_enabled(nb: *mut NewtonBody) -> bool {
    // SAFETY: `nb` is a live Newton body.
    unsafe { NewtonBodyGetSleepState(nb) == 0 }
}

unsafe extern "C" fn on_update(nb: *mut NewtonBody, time_step: f32, _thread: i32) {
    let (Some(host), Some((h, rec))) = (host(), of_newton(nb)) else {
        return;
    };
    let mut st = BodyState::default();
    // SAFETY: host callbacks are live for the process; `st` is a valid out-pointer.
    unsafe { (host.body_state)(rec.cpp, &mut st) };
    if !st.active {
        return;
    }
    let mut freeze = st.freeze_count;
    if is_enabled(nb) {
        if !st.in_update_list {
            // SAFETY: host callback.
            unsafe { (host.body_add_to_update_list)(rec.cpp) };
        }
        if freeze > 0 {
            freeze -= 1;
            // SAFETY: host callback.
            unsafe { (host.body_set_freeze_count)(rec.cpp, freeze) };
        }
    }
    if rec.gravity && freeze <= 0 {
        let g = st.gravity;
        let f = v3(rec.mass * g.x, rec.mass * g.y, rec.mass * g.z);
        // SAFETY: Newton body, valid vector.
        unsafe { NewtonBodyAddForce(nb, fp(&f)) };
    }
    if st.buoyancy_active && st.density_mul > 0.0 && freeze <= 0 {
        if st.hpl3 {
            let (mut f, mut t) = (rec.buoyancy_force, rec.buoyancy_torque);
            let mut calc = false;
            if is_enabled(nb) {
                // SAFETY: host callback with valid out-pointers.
                calc = unsafe {
                    (host.body_hpl3_buoyancy)(rec.cpp, &st.gravity, time_step, &mut f, &mut t)
                };
                if calc {
                    reg::update_body(h, |b| {
                        b.buoyancy_force = f;
                        b.buoyancy_torque = t;
                    });
                }
            }
            if !is_enabled(nb) || calc {
                // SAFETY: Newton body, valid vectors.
                unsafe {
                    NewtonBodyAddForce(nb, fp(&f));
                    NewtonBodyAddTorque(nb, fp(&t));
                }
            }
        } else {
            let mut plane = st.surface;
            // SAFETY: `plane` outlives the synchronous call.
            unsafe {
                NewtonBodyAddBuoyancyForce(
                    nb,
                    st.density * st.density_mul,
                    st.linear_viscosity,
                    st.angular_viscosity,
                    fp(&st.gravity),
                    Some(buoyancy_plane),
                    plane.as_mut_ptr().cast(),
                );
            }
        }
    }
    // SAFETY: Newton body, valid vectors.
    unsafe {
        NewtonBodyAddForce(nb, fp(&rec.force));
        NewtonBodyAddTorque(nb, fp(&rec.torque));
    }
    if rec.max_linear > 0.0 {
        let mut v = ZERO;
        // SAFETY: Newton body, valid vector.
        unsafe { NewtonBodyGetVelocity(nb, ptr::from_mut(&mut v).cast()) };
        if let Some(c) = clamp_speed(v, rec.max_linear) {
            // SAFETY: as above.
            unsafe { NewtonBodySetVelocity(nb, fp(&c)) };
        }
    }
    if rec.max_angular > 0.0 {
        let mut v = ZERO;
        // SAFETY: as above.
        unsafe { NewtonBodyGetOmega(nb, ptr::from_mut(&mut v).cast()) };
        if let Some(c) = clamp_speed(v, rec.max_angular) {
            // SAFETY: as above.
            unsafe { NewtonBodySetOmega(nb, fp(&c)) };
        }
    }
}

/// # Safety
/// `coll` is a live collision of the world; `out_nb` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ntb_create(
    world: u64,
    coll: *mut NewtonCollision,
    cpp: usize,
    out_nb: *mut *mut NewtonBody,
) -> u64 {
    let Some((wh, w)) = reg::world(world) else {
        return 0;
    };
    let identity = transposed(&Matrixf {
        m: [
            [1.0, 0.0, 0.0, 0.0],
            [0.0, 1.0, 0.0, 0.0],
            [0.0, 0.0, 1.0, 0.0],
            [0.0, 0.0, 0.0, 1.0],
        ],
    });
    // SAFETY: plain Newton call.
    let nb = unsafe { NewtonCreateBody(w.nw, coll, identity.as_ptr()) };
    let bits = {
        let Some(mut r) = reg() else { return 0 };
        r.bodies
            .insert(BodyRec {
                nb,
                nw: w.nw,
                cpp,
                world: wh,
                mass: 0.0,
                gravity: true,
                max_linear: 0.0,
                max_angular: 0.0,
                force: ZERO,
                torque: ZERO,
                forced: false,
                buoyancy_force: ZERO,
                buoyancy_torque: ZERO,
            })
            .map_or(0, Handle::to_bits)
    };
    // SAFETY: `nb` was just created; the user data is the arena handle, never an address.
    unsafe {
        NewtonBodySetForceAndTorqueCallback(nb, Some(on_update));
        NewtonBodySetTransformCallback(nb, Some(on_transform));
        NewtonBodySetUserData(nb, bits as usize as *mut c_void);
        if let Some(o) = out_nb.as_mut() {
            *o = nb;
        }
    }
    bits
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ntb_destroy(b: u64) {
    let Some(h) = Handle::from_bits(b) else {
        return;
    };
    let rec = {
        let Some(mut r) = reg() else { return };
        r.forced.retain(|(_, bh)| *bh != h);
        r.bodies.remove(h)
    };
    if let Some(rec) = rec {
        // SAFETY: the record left the registry first, so callbacks no longer resolve it.
        unsafe { NewtonDestroyBody(rec.nw, rec.nb) };
    }
}

fn with_body(b: u64, f: impl FnOnce(&BodyRec)) {
    if let Some((_, rec)) = reg::body(b) {
        f(&rec);
    }
}

/// # Safety
/// `m` is a valid matrix.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ntb_set_matrix(b: u64, m: *const Matrixf) {
    // SAFETY: caller contract.
    if let Some(m) = unsafe { m.as_ref() } {
        let mut t = transposed(m);
        // SAFETY: Newton body, 16 floats.
        with_body(b, |r| unsafe { NewtonBodySetMatrix(r.nb, t.as_mut_ptr()) });
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ntb_set_material(b: u64, id: i32) {
    // SAFETY: Newton body.
    with_body(b, |r| unsafe { NewtonBodySetMaterialGroupID(r.nb, id) });
}

/// # Safety
/// `v` is a valid vector.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ntb_set_velocity(b: u64, v: *const Vector3f) {
    // SAFETY: Newton body, caller contract.
    with_body(b, |r| unsafe { NewtonBodySetVelocity(r.nb, v.cast()) });
}

/// # Safety
/// `out` is a valid vector.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ntb_velocity(b: u64, out: *mut Vector3f) {
    // SAFETY: Newton body, caller contract.
    with_body(b, |r| unsafe { NewtonBodyGetVelocity(r.nb, out.cast()) });
}

/// # Safety
/// `v` is a valid vector.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ntb_set_omega(b: u64, v: *const Vector3f) {
    // SAFETY: Newton body, caller contract.
    with_body(b, |r| unsafe { NewtonBodySetOmega(r.nb, v.cast()) });
}

/// # Safety
/// `out` is a valid vector.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ntb_omega(b: u64, out: *mut Vector3f) {
    // SAFETY: Newton body, caller contract.
    with_body(b, |r| unsafe { NewtonBodyGetOmega(r.nb, out.cast()) });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ntb_set_linear_damping(b: u64, d: f32) {
    let d = if d < 0.001 { 0.001 } else { d };
    // SAFETY: Newton body.
    with_body(b, |r| unsafe { NewtonBodySetLinearDamping(r.nb, d) });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ntb_linear_damping(b: u64) -> f32 {
    // SAFETY: Newton body.
    reg::body(b).map_or(0.0, |(_, r)| unsafe { NewtonBodyGetLinearDamping(r.nb) })
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ntb_set_angular_damping(b: u64, d: f32) {
    let d = if d < 0.001 { 0.001 } else { d };
    let damp = [d, d, d];
    // SAFETY: Newton body, three floats.
    with_body(b, |r| unsafe {
        NewtonBodySetAngularDamping(r.nb, damp.as_ptr())
    });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ntb_angular_damping(b: u64) -> f32 {
    let mut damp = [0.0f32; 3];
    // SAFETY: Newton body, three floats.
    with_body(b, |r| unsafe {
        NewtonBodyGetAngularDamping(r.nb, damp.as_mut_ptr())
    });
    damp[0]
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ntb_set_max_linear(b: u64, s: f32) {
    if let Some((h, _)) = reg::body(b) {
        reg::update_body(h, |r| r.max_linear = s);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ntb_max_linear(b: u64) -> f32 {
    reg::body(b).map_or(0.0, |(_, r)| r.max_linear)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ntb_set_max_angular(b: u64, s: f32) {
    if let Some((h, _)) = reg::body(b) {
        reg::update_body(h, |r| r.max_angular = s);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ntb_max_angular(b: u64) -> f32 {
    reg::body(b).map_or(0.0, |(_, r)| r.max_angular)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ntb_set_gravity(b: u64, on: bool) {
    if let Some((h, _)) = reg::body(b) {
        reg::update_body(h, |r| r.gravity = on);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ntb_gravity(b: u64) -> bool {
    reg::body(b).is_some_and(|(_, r)| r.gravity)
}

/// # Safety
/// `out` is a valid vector.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ntb_inertia(b: u64, out: *mut Vector3f) {
    let (mut m, mut x, mut y, mut z) = (0.0f32, 0.0f32, 0.0f32, 0.0f32);
    // SAFETY: Newton body, valid out-pointers.
    with_body(b, |r| unsafe {
        NewtonBodyGetMassMatrix(r.nb, &mut m, &mut x, &mut y, &mut z)
    });
    // SAFETY: caller contract.
    if let Some(o) = unsafe { out.as_mut() } {
        *o = v3(x, y, z);
    }
}

/// # Safety
/// `coll` is the body's live collision.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ntb_set_mass(b: u64, coll: *const NewtonCollision, mass: f32) {
    let Some((h, rec)) = reg::body(b) else { return };
    let (mut inertia, mut offset) = (ZERO, ZERO);
    // SAFETY: valid collision and out-pointers.
    unsafe {
        NewtonConvexCollisionCalculateInertialMatrix(
            coll,
            ptr::from_mut(&mut inertia).cast(),
            ptr::from_mut(&mut offset).cast(),
        );
    }
    let i = v3(inertia.x * mass, inertia.y * mass, inertia.z * mass);
    // SAFETY: Newton body.
    unsafe {
        NewtonBodySetCentreOfMass(rec.nb, fp(&offset));
        NewtonBodySetMassMatrix(rec.nb, mass, i.x, i.y, i.z);
    }
    reg::update_body(h, |r| r.mass = mass);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ntb_mass(b: u64) -> f32 {
    reg::body(b).map_or(0.0, |(_, r)| r.mass)
}

/// # Safety
/// `v` is a valid vector.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ntb_set_mass_centre(b: u64, v: *const Vector3f) {
    // SAFETY: Newton body, caller contract.
    with_body(b, |r| unsafe { NewtonBodySetCentreOfMass(r.nb, v.cast()) });
}

/// # Safety
/// `out` is a valid vector.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ntb_mass_centre(b: u64, out: *mut Vector3f) {
    // SAFETY: Newton body, caller contract.
    with_body(b, |r| unsafe {
        NewtonBodyGetCentreOfMass(r.nb, out.cast())
    });
}

/// `MarkForced` plus accumulate; the caller then runs `Enable()`.
fn accumulate(b: u64, force: Option<Vector3f>, torque: Option<Vector3f>) {
    let Some((h, _)) = reg::body(b) else { return };
    let Some(mut r) = reg() else { return };
    let Some(rec) = r.bodies.get_mut(h) else {
        return;
    };
    let newly = !rec.forced;
    rec.forced = true;
    if let Some(f) = force {
        rec.force = v3(rec.force.x + f.x, rec.force.y + f.y, rec.force.z + f.z);
    }
    if let Some(t) = torque {
        rec.torque = v3(rec.torque.x + t.x, rec.torque.y + t.y, rec.torque.z + t.z);
    }
    let world = rec.world;
    if newly {
        r.forced.push((world, h));
    }
}

/// # Safety
/// `f` is a valid vector.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ntb_add_force(b: u64, f: *const Vector3f) {
    // SAFETY: caller contract.
    if let Some(f) = unsafe { f.as_ref() } {
        accumulate(b, Some(*f), None);
    }
}

/// # Safety
/// All pointers are valid; `local` is the body's local matrix.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ntb_add_force_at_position(
    b: u64,
    f: *const Vector3f,
    pos: *const Vector3f,
    local: *const Matrixf,
) {
    // SAFETY: caller contract.
    let (Some(f), Some(pos), Some(local), Some((_, rec))) = (
        unsafe { f.as_ref() },
        unsafe { pos.as_ref() },
        unsafe { local.as_ref() },
        reg::body(b),
    ) else {
        return;
    };
    let mut local_pos = sub3(*pos, v3(local.m[0][3], local.m[1][3], local.m[2][3]));
    let mut centre = ZERO;
    // SAFETY: Newton body, valid out-pointer.
    unsafe { NewtonBodyGetCentreOfMass(rec.nb, ptr::from_mut(&mut centre).cast()) };
    if centre != ZERO {
        let mut rot = *local;
        for row in &mut rot.m {
            row[3] = 0.0;
        }
        rot.m[3] = [0.0, 0.0, 0.0, 1.0];
        centre = matrix_mul_vec(&rot, centre);
        local_pos = sub3(local_pos, centre);
    }
    accumulate(b, Some(*f), Some(cross3(local_pos, *f)));
}

/// # Safety
/// `t` is a valid vector.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ntb_add_torque(b: u64, t: *const Vector3f) {
    // SAFETY: caller contract.
    if let Some(t) = unsafe { t.as_ref() } {
        accumulate(b, None, Some(*t));
    }
}

/// Impulse at the mass centre; `world` is the body's world matrix.
///
/// # Safety
/// Pointers are valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ntb_add_impulse(
    b: u64,
    impulse: *const Vector3f,
    world: *const Matrixf,
) {
    // SAFETY: caller contract.
    let (Some(w), Some((_, rec))) = (unsafe { world.as_ref() }, reg::body(b)) else {
        return;
    };
    let mut centre = ZERO;
    // SAFETY: Newton body, valid out-pointer.
    unsafe { NewtonBodyGetCentreOfMass(rec.nb, ptr::from_mut(&mut centre).cast()) };
    let mut pos = v3(w.m[0][3], w.m[1][3], w.m[2][3]);
    if centre != ZERO {
        let mut rot = *w;
        for row in &mut rot.m {
            row[3] = 0.0;
        }
        rot.m[3] = [0.0, 0.0, 0.0, 1.0];
        let off = matrix_mul_vec(&rot, centre);
        pos = v3(pos.x + off.x, pos.y + off.y, pos.z + off.z);
    }
    // SAFETY: Newton body, valid vectors.
    unsafe { NewtonBodyAddImpulse(rec.nb, impulse.cast(), fp(&pos)) };
}

/// # Safety
/// Pointers are valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ntb_add_impulse_at(
    b: u64,
    impulse: *const Vector3f,
    pos: *const Vector3f,
) {
    // SAFETY: Newton body, caller contract.
    with_body(b, |r| unsafe {
        NewtonBodyAddImpulse(r.nb, impulse.cast(), pos.cast())
    });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ntb_set_freeze(b: u64, state: i32) {
    // SAFETY: Newton body.
    with_body(b, |r| unsafe { NewtonBodySetFreezeState(r.nb, state) });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ntb_enabled(b: u64) -> bool {
    reg::body(b).is_some_and(|(_, r)| is_enabled(r.nb))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ntb_set_auto_sleep(b: u64, on: bool) {
    // SAFETY: Newton body.
    with_body(b, |r| unsafe {
        NewtonBodySetAutoSleep(r.nb, c_int::from(on))
    });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ntb_auto_sleep(b: u64) -> bool {
    // SAFETY: Newton body.
    reg::body(b).is_some_and(|(_, r)| unsafe { NewtonBodyGetAutoSleep(r.nb) } != 0)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ntb_set_continuous(b: u64, on: bool) {
    // SAFETY: Newton body.
    with_body(b, |r| unsafe {
        NewtonBodySetContinuousCollisionMode(r.nb, u32::from(on))
    });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ntb_continuous(b: u64) -> bool {
    // SAFETY: Newton body.
    reg::body(b).is_some_and(|(_, r)| unsafe { NewtonBodyGetContinuousCollisionMode(r.nb) } == 1)
}
