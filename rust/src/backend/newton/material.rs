//! `cPhysicsMaterialNewton`: material pair setup and the AABB-overlap and contact callbacks.

use super::body::of_newton;
use super::host::host;
use super::reg;
use crate::physics::newton_material::{
    Contact, GateBody, MatProps, aabb_overlap_gate, aggregate_contacts, pair_params,
};
use crate::sys::newton::{
    NewtonBody, NewtonBodyGetWorld, NewtonContactGetMaterial, NewtonContactJointGetFirstContact,
    NewtonContactJointGetNextContact, NewtonJoint, NewtonJointGetBody0, NewtonJointGetBody1,
    NewtonMaterial, NewtonMaterialCreateGroupID, NewtonMaterialGetContactForce,
    NewtonMaterialGetContactNormalSpeed, NewtonMaterialGetContactPositionAndNormal,
    NewtonMaterialGetContactTangentSpeed, NewtonMaterialSetCollisionCallback,
    NewtonMaterialSetContinuousCollisionMode, NewtonMaterialSetDefaultElasticity,
    NewtonMaterialSetDefaultFriction, NewtonWorld, NewtonWorldCriticalSectionLock,
    NewtonWorldCriticalSectionUnlock,
};
use crate::types::Vector3f;
use std::ffi::c_int;
use std::ptr;

/// Newton world critical section held for the guard's lifetime.
struct Section(*mut NewtonWorld);

impl Section {
    fn of_body(body: *const NewtonBody) -> Self {
        // SAFETY: `body` is a live Newton body inside a callback.
        let w = unsafe { NewtonBodyGetWorld(body) };
        // SAFETY: `w` is the body's live world.
        unsafe { NewtonWorldCriticalSectionLock(w) };
        Self(w)
    }
}

impl Drop for Section {
    fn drop(&mut self) {
        // SAFETY: paired with the lock taken in `of_body`.
        unsafe { NewtonWorldCriticalSectionUnlock(self.0) };
    }
}

unsafe extern "C" fn aabb_overlap(
    _material: *const NewtonMaterial,
    body0: *const NewtonBody,
    body1: *const NewtonBody,
    _thread: i32,
) -> c_int {
    let (Some(host), Some((_, r1)), Some((_, r2))) = (host(), of_newton(body0), of_newton(body1))
    else {
        return 0;
    };
    let (mut g1, mut g2) = (GateBody::default(), GateBody::default());
    let (mut f1, mut f2) = (0u32, 0u32);
    // SAFETY: host callbacks are live for the process; out-pointers are valid.
    let flags_match = unsafe {
        (host.body_gate)(r1.cpp, &mut g1, &mut f1);
        (host.body_gate)(r2.cpp, &mut g2, &mut f2);
        (host.collide_flags_match)(f1, f2)
    };
    if !aabb_overlap_gate(flags_match, &g1, &g2) {
        return 0;
    }
    let _lock1 = Section::of_body(body0);
    let _lock2 = Section::of_body(body1);
    // SAFETY: host callbacks.
    unsafe {
        if !(host.body_on_aabb_collision)(r1.cpp, r2.cpp) {
            return 0;
        }
        if !(host.body_on_aabb_collision)(r2.cpp, r1.cpp) {
            return 0;
        }
    }
    1
}

unsafe extern "C" fn contacts_process(joint: *const NewtonJoint, _time_step: f32, _thread: i32) {
    // SAFETY: `joint` is the live contact joint of this callback.
    let (b0, b1) = unsafe { (NewtonJointGetBody0(joint), NewtonJointGetBody1(joint)) };
    let (Some(host), Some((_, r1)), Some((_, r2))) = (host(), of_newton(b0), of_newton(b1)) else {
        return;
    };
    let mut contacts: Vec<Contact> = Vec::new();
    // SAFETY: the contact list belongs to `joint` and is only walked here.
    let mut node = unsafe { NewtonContactJointGetFirstContact(joint) };
    while !node.is_null() {
        // SAFETY: `node` is a live contact of `joint`.
        let mat = unsafe { NewtonContactGetMaterial(node) };
        let mut c = Contact::default();
        // SAFETY: `mat` belongs to the contact; out-pointers are valid.
        unsafe {
            c.normal_speed = NewtonMaterialGetContactNormalSpeed(mat);
            c.tangent_speed0 = NewtonMaterialGetContactTangentSpeed(mat, 0);
            c.tangent_speed1 = NewtonMaterialGetContactTangentSpeed(mat, 1);
            NewtonMaterialGetContactForce(mat, b0, ptr::from_mut(&mut c.force).cast());
            NewtonMaterialGetContactPositionAndNormal(
                mat,
                b0,
                ptr::from_mut(&mut c.position).cast(),
                ptr::from_mut(&mut c.normal).cast(),
            );
        }
        contacts.push(c);
        // SAFETY: host callbacks; the section guard unlocks on scope end.
        unsafe {
            if (host.contact_save_enabled)(r1.cpp) {
                let _section = Section::of_body(b0);
                let (p, n): (Vector3f, Vector3f) = (c.position, c.normal);
                (host.contact_push_point)(r1.cpp, &p, &n);
            }
        }
        // SAFETY: `node` is a live contact of `joint`.
        node = unsafe { NewtonContactJointGetNextContact(joint, node) };
    }
    let agg = aggregate_contacts(&contacts);
    let _section0 = Section::of_body(b0);
    let _section1 = Section::of_body(b1);
    // SAFETY: host callback; `agg` is valid for the call.
    unsafe {
        (host.contact_dispatch)(
            r1.cpp,
            r2.cpp,
            &agg,
            i32::try_from(contacts.len()).unwrap_or(0),
        );
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ntm_create_group(w: u64) -> i32 {
    // SAFETY: plain Newton call.
    reg::world(w).map_or(0, |(_, r)| unsafe { NewtonMaterialCreateGroupID(r.nw) })
}

/// Blend `a` with `b` and install the pair's defaults and callbacks.
///
/// # Safety
/// `a` and `b` are valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ntm_update_pair(
    w: u64,
    id_a: i32,
    id_b: i32,
    a: *const MatProps,
    b: *const MatProps,
    same: bool,
) {
    // SAFETY: caller contract.
    let (Some((_, r)), Some(a), Some(b)) =
        (reg::world(w), unsafe { a.as_ref() }, unsafe { b.as_ref() })
    else {
        return;
    };
    let p = pair_params(a, b, same);
    // SAFETY: plain Newton calls on a live world.
    unsafe {
        NewtonMaterialSetDefaultElasticity(r.nw, id_a, id_b, p.elasticity);
        NewtonMaterialSetDefaultFriction(r.nw, id_a, id_b, p.static_friction, p.kinetic_friction);
        NewtonMaterialSetContinuousCollisionMode(r.nw, id_a, id_b, 1);
        NewtonMaterialSetCollisionCallback(
            r.nw,
            id_a,
            id_b,
            ptr::null_mut(),
            Some(aabb_overlap),
            Some(contacts_process),
        );
    }
}
