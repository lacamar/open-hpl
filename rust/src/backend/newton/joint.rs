//! `iPhysicsJointNewton` and the Ball/Hinge/Slider/Screw joints: creation, accessors, and the
//! constraint-submit and limit callbacks.

use super::host::JointOps;
use super::reg::{self, JointKind, JointRec, reg};
use crate::math::{IDENTITY, atan2, cross3, dot3, fma, matrix_mul, sub3, to_rad, transpose, v3};
use crate::obj::Handle;
use crate::sys::newton::{
    NewtonBallGetJointAngle, NewtonBallGetJointForce, NewtonBallGetJointOmega,
    NewtonBallSetConeLimits, NewtonBody, NewtonConstraintCreateBall,
    NewtonConstraintCreateCorkscrew, NewtonConstraintCreateSlider, NewtonConstraintCreateUserJoint,
    NewtonCorkscrewCalculateStopAccel, NewtonCorkscrewGetJointAngle, NewtonCorkscrewGetJointForce,
    NewtonCorkscrewGetJointPosit, NewtonCorkscrewGetJointVeloc, NewtonCorkscrewSetUserCallback,
    NewtonDestroyJoint, NewtonHingeSliderUpdateDesc, NewtonJoint, NewtonJointGetCollisionState,
    NewtonJointGetStiffness, NewtonJointGetUserData, NewtonJointRecord,
    NewtonJointSetCollisionState, NewtonJointSetStiffness, NewtonJointSetUserData,
    NewtonSliderCalculateStopAccel, NewtonSliderGetJointForce, NewtonSliderGetJointPosit,
    NewtonSliderGetJointVeloc, NewtonSliderSetUserCallback, NewtonUserJointAddAngularRow,
    NewtonUserJointAddLinearRow, NewtonUserJointGetRowForce, NewtonUserJointSetRowMaximumFriction,
    NewtonUserJointSetRowMinimumFriction, NewtonUserJointSetRowStiffness,
};
use crate::types::{Matrixf, Vector3f};
use std::ffi::{c_int, c_uint, c_void};
use std::ptr;

const EVENT_MIN: i32 = 0;
const EVENT_MAX: i32 = 1;

fn joint_of(j: *const NewtonJoint) -> Option<(Handle<JointRec>, JointRec)> {
    // SAFETY: `j` is a live joint handed to a callback.
    let bits = unsafe { NewtonJointGetUserData(j) } as usize;
    reg::joint(u64::try_from(bits).ok()?)
}

fn fp(v: &Vector3f) -> *const f32 {
    ptr::from_ref(v).cast()
}

fn row(m: &Matrixf, r: usize) -> Vector3f {
    let [x, y, z, _] = m.m.get(r).copied().unwrap_or_default();
    v3(x, y, z)
}

fn translation(m: &Matrixf) -> Vector3f {
    let [r0, r1, r2, _] = m.m;
    v3(r0[3], r1[3], r2[3])
}

/// `a + dir * s` with the product fused, as gcc emits it.
fn offset(a: Vector3f, dir: Vector3f, s: f32) -> Vector3f {
    v3(fma(dir.x, s, a.x), fma(dir.y, s, a.y), fma(dir.z, s, a.z))
}

fn insert(rec: JointRec) -> u64 {
    let Some(mut r) = reg() else { return 0 };
    r.joints.insert(rec).map_or(0, Handle::to_bits)
}

fn blank(nj: *mut NewtonJoint, cpp: usize, ops: *const JointOps, kind: JointKind) -> JointRec {
    JointRec {
        nj,
        cpp,
        ops,
        kind,
        local0: IDENTITY,
        local1: IDENTITY,
        min: 0.0,
        max: 0.0,
        previous: 0.0,
    }
}

fn nw_of(w: u64) -> Option<*mut crate::sys::newton::NewtonWorld> {
    reg::world(w).map(|(_, r)| r.nw)
}

/// # Safety
/// `pivot` is a valid vector; bodies are live (parent may be null).
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ntj_create_ball(
    w: u64,
    pivot: *const Vector3f,
    child: *mut NewtonBody,
    parent: *mut NewtonBody,
    out_nj: *mut *mut NewtonJoint,
) -> u64 {
    let Some(nw) = nw_of(w) else { return 0 };
    // SAFETY: caller contract.
    let nj = unsafe { NewtonConstraintCreateBall(nw, pivot.cast(), child, parent) };
    // SAFETY: caller contract.
    if let Some(o) = unsafe { out_nj.as_mut() } {
        *o = nj;
    }
    insert(blank(nj, 0, ptr::null(), JointKind::Ball))
}

unsafe extern "C" fn get_info(_j: *const NewtonJoint, _info: *mut NewtonJointRecord) {}

/// # Safety
/// `ops` outlives the joint; `local0`/`local1` are valid matrices.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ntj_create_hinge(
    w: u64,
    child: *mut NewtonBody,
    parent: *mut NewtonBody,
    cpp: usize,
    ops: *const JointOps,
    local0: *const Matrixf,
    local1: *const Matrixf,
    out_nj: *mut *mut NewtonJoint,
) -> u64 {
    let Some(nw) = nw_of(w) else { return 0 };
    // SAFETY: plain Newton call.
    let nj = unsafe {
        NewtonConstraintCreateUserJoint(nw, 6, Some(submit_hinge), Some(get_info), child, parent)
    };
    let mut rec = blank(nj, cpp, ops, JointKind::Hinge);
    // SAFETY: caller contract.
    unsafe {
        rec.local0 = local0.as_ref().copied().unwrap_or(IDENTITY);
        rec.local1 = local1.as_ref().copied().unwrap_or(IDENTITY);
        if let Some(o) = out_nj.as_mut() {
            *o = nj;
        }
    }
    let bits = insert(rec);
    // SAFETY: the user data is the arena handle, never an address.
    unsafe { NewtonJointSetUserData(nj, bits as usize as *mut c_void) };
    bits
}

fn create_limited(
    kind: JointKind,
    nj: *mut NewtonJoint,
    cpp: usize,
    ops: *const JointOps,
    out_nj: *mut *mut NewtonJoint,
) -> u64 {
    // SAFETY: caller contract of the public creators.
    if let Some(o) = unsafe { out_nj.as_mut() } {
        *o = nj;
    }
    let bits = insert(blank(nj, cpp, ops, kind));
    // SAFETY: the user data is the arena handle, never an address.
    unsafe { NewtonJointSetUserData(nj, bits as usize as *mut c_void) };
    bits
}

/// # Safety
/// Vectors are valid, bodies are live (parent may be null), `ops` outlives the joint.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ntj_create_slider(
    w: u64,
    pivot: *const Vector3f,
    pin: *const Vector3f,
    child: *mut NewtonBody,
    parent: *mut NewtonBody,
    cpp: usize,
    ops: *const JointOps,
    out_nj: *mut *mut NewtonJoint,
) -> u64 {
    let Some(nw) = nw_of(w) else { return 0 };
    // SAFETY: caller contract.
    let nj = unsafe { NewtonConstraintCreateSlider(nw, pivot.cast(), pin.cast(), child, parent) };
    let bits = create_limited(JointKind::Slider, nj, cpp, ops, out_nj);
    // SAFETY: `nj` is live.
    unsafe { NewtonSliderSetUserCallback(nj, Some(slider_limit)) };
    bits
}

/// # Safety
/// Vectors are valid, bodies are live (parent may be null), `ops` outlives the joint.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ntj_create_screw(
    w: u64,
    pivot: *const Vector3f,
    pin: *const Vector3f,
    child: *mut NewtonBody,
    parent: *mut NewtonBody,
    cpp: usize,
    ops: *const JointOps,
    out_nj: *mut *mut NewtonJoint,
) -> u64 {
    let Some(nw) = nw_of(w) else { return 0 };
    // SAFETY: caller contract.
    let nj =
        unsafe { NewtonConstraintCreateCorkscrew(nw, pivot.cast(), pin.cast(), child, parent) };
    let bits = create_limited(JointKind::Screw, nj, cpp, ops, out_nj);
    // SAFETY: `nj` is live.
    unsafe { NewtonCorkscrewSetUserCallback(nj, Some(screw_limit)) };
    bits
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ntj_destroy(w: u64, j: u64) {
    let Some(h) = Handle::from_bits(j) else {
        return;
    };
    let rec = reg().and_then(|mut r| r.joints.remove(h));
    if let (Some(rec), Some(nw)) = (rec, nw_of(w)) {
        // SAFETY: the record left the registry first, so callbacks no longer resolve it.
        unsafe { NewtonDestroyJoint(nw, rec.nj) };
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ntj_set_collide(j: u64, on: bool) {
    if let Some((_, r)) = reg::joint(j) {
        // SAFETY: live joint.
        unsafe { NewtonJointSetCollisionState(r.nj, c_int::from(on)) };
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ntj_collide(j: u64) -> bool {
    // SAFETY: live joint.
    reg::joint(j).is_some_and(|(_, r)| unsafe { NewtonJointGetCollisionState(r.nj) } != 0)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ntj_set_stiffness(j: u64, s: f32) {
    if let Some((_, r)) = reg::joint(j) {
        // SAFETY: live joint.
        unsafe { NewtonJointSetStiffness(r.nj, s) };
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ntj_stiffness(j: u64) -> f32 {
    // SAFETY: live joint.
    reg::joint(j).map_or(0.0, |(_, r)| unsafe { NewtonJointGetStiffness(r.nj) })
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ntj_set_min(j: u64, v: f32) {
    if let Some((h, _)) = reg::joint(j) {
        reg::update_joint(h, |r| r.min = v);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ntj_set_max(j: u64, v: f32) {
    if let Some((h, _)) = reg::joint(j) {
        reg::update_joint(h, |r| r.max = v);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ntj_min(j: u64) -> f32 {
    reg::joint(j).map_or(0.0, |(_, r)| r.min)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ntj_max(j: u64) -> f32 {
    reg::joint(j).map_or(0.0, |(_, r)| r.max)
}

/// # Safety
/// `pin` is a valid vector.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ntj_ball_cone_limits(
    j: u64,
    pin: *const Vector3f,
    cone: f32,
    twist: f32,
) {
    if let Some((_, r)) = reg::joint(j) {
        // SAFETY: live joint, caller contract.
        unsafe { NewtonBallSetConeLimits(r.nj, pin.cast(), cone, twist) };
    }
}

/// # Safety
/// `out` is a valid vector.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ntj_ball_angles(j: u64, out: *mut Vector3f) {
    if let Some((_, r)) = reg::joint(j) {
        // SAFETY: live joint, caller contract.
        unsafe { NewtonBallGetJointAngle(r.nj, out.cast()) };
    }
}

/// # Safety
/// `out` is a valid vector.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ntj_ball_omega(j: u64, out: *mut Vector3f) {
    if let Some((_, r)) = reg::joint(j) {
        // SAFETY: live joint, caller contract.
        unsafe { NewtonBallGetJointOmega(r.nj, out.cast()) };
    }
}

/// # Safety
/// `out` is a valid vector.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ntj_ball_force(j: u64, out: *mut Vector3f) {
    if let Some((_, r)) = reg::joint(j) {
        // SAFETY: live joint, caller contract.
        unsafe { NewtonBallGetJointForce(r.nj, out.cast()) };
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ntj_hinge_force_size(j: u64) -> f32 {
    let Some((_, r)) = reg::joint(j) else {
        return 0.0;
    };
    let mut size = 0.0f32;
    for i in 0..3 {
        // SAFETY: live user joint.
        size += unsafe { NewtonUserJointGetRowForce(r.nj, i) }.abs();
    }
    size
}

/// Position, velocity or angle of a slider/screw: 0 posit, 1 veloc, 2 angle (screw only).
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ntj_axis(j: u64, what: i32) -> f32 {
    let Some((_, r)) = reg::joint(j) else {
        return 0.0;
    };
    // SAFETY: live joint of the matching kind.
    unsafe {
        match (r.kind, what) {
            (JointKind::Slider, 0) => NewtonSliderGetJointPosit(r.nj),
            (JointKind::Slider, 1) => NewtonSliderGetJointVeloc(r.nj),
            (JointKind::Screw, 0) => NewtonCorkscrewGetJointPosit(r.nj),
            (JointKind::Screw, 1) => NewtonCorkscrewGetJointVeloc(r.nj),
            (JointKind::Screw, 2) => NewtonCorkscrewGetJointAngle(r.nj),
            _ => 0.0,
        }
    }
}

/// # Safety
/// `out` is a valid vector.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ntj_force(j: u64, out: *mut Vector3f) {
    let Some((_, r)) = reg::joint(j) else { return };
    // SAFETY: live joint of the matching kind, caller contract.
    unsafe {
        match r.kind {
            JointKind::Slider => NewtonSliderGetJointForce(r.nj, out.cast()),
            JointKind::Screw => NewtonCorkscrewGetJointForce(r.nj, out.cast()),
            _ => {}
        }
    }
}

/// Row geometry of a hinge submit, computed with the float patterns of the gcc build.
#[derive(Clone, Copy, Debug, PartialEq)]
pub(super) struct HingeGeometry {
    pub p0: Vector3f,
    pub p1: Vector3f,
    pub right0: Vector3f,
    pub up0: Vector3f,
    pub forward0: Vector3f,
    pub q0: Vector3f,
    pub q1: Vector3f,
    pub angle: f32,
    pub dist_sqr: f32,
}

pub(super) fn hinge_geometry(
    child: &Matrixf,
    parent: &Matrixf,
    local0: &Matrixf,
    local1: &Matrixf,
) -> HingeGeometry {
    let m0 = matrix_mul(child, local0);
    let m1 = matrix_mul(parent, local1);
    let (p0, p1) = (translation(&m0), translation(&m1));
    let (i0, i1) = (transpose(&m0), transpose(&m1));
    let (right0, up0, forward0) = (row(&i0, 0), row(&i0, 1), row(&i0, 2));
    let (right1, up1) = (row(&i1, 0), row(&i1, 1));
    let d = sub3(p1, p0);
    HingeGeometry {
        p0,
        p1,
        right0,
        up0,
        forward0,
        q0: offset(p0, up0, 50.0),
        q1: offset(p1, up1, 50.0),
        angle: atan2(dot3(cross3(right0, right1), up0), dot3(right0, right1)),
        dist_sqr: dot3(d, d),
    }
}

unsafe extern "C" fn submit_hinge(j: *const NewtonJoint, _time_step: f32, _thread: c_int) {
    let Some((h, rec)) = joint_of(j) else { return };
    // SAFETY: `ops` outlives the joint; out-pointers are valid.
    let Some(ops) = (unsafe { rec.ops.as_ref() }) else {
        return;
    };
    let (mut child, mut parent) = (IDENTITY, IDENTITY);
    // SAFETY: host callback with valid out-pointers.
    unsafe { (ops.matrices)(rec.cpp, &mut child, &mut parent) };
    let g = hinge_geometry(&child, &parent, &rec.local0, &rec.local1);
    let (p0, p1, right0, up0, fwd0) = (g.p0, g.p1, g.right0, g.up0, g.forward0);
    // SAFETY: `j` is the live user joint being submitted; vectors are valid for each call.
    unsafe {
        NewtonUserJointAddLinearRow(j, fp(&p0), fp(&p1), fp(&right0));
        NewtonUserJointAddLinearRow(j, fp(&p0), fp(&p1), fp(&up0));
        NewtonUserJointAddLinearRow(j, fp(&p0), fp(&p1), fp(&fwd0));
    }
    let (q0, q1) = (g.q0, g.q1);
    // SAFETY: as above.
    unsafe {
        NewtonUserJointAddLinearRow(j, fp(&q0), fp(&q1), fp(&right0));
        NewtonUserJointAddLinearRow(j, fp(&q0), fp(&q1), fp(&fwd0));
    }
    if rec.min != 0.0 || rec.max != 0.0 {
        let angle = g.angle;
        // SAFETY: host callbacks and Newton row calls on the live joint.
        unsafe {
            (ops.check_auto_sleep)(rec.cpp, rec.min, rec.max, angle);
            let skip = (rec.previous - angle).abs() > to_rad(300.0);
            if angle < rec.min && !skip {
                (ops.limit_event)(rec.cpp, EVENT_MIN);
                NewtonUserJointAddAngularRow(j, angle - rec.min, fp(&up0));
                NewtonUserJointSetRowStiffness(j, 1.0);
                NewtonUserJointSetRowMaximumFriction(j, 0.0);
            } else if angle > rec.max && !skip {
                (ops.limit_event)(rec.cpp, EVENT_MAX);
                NewtonUserJointAddAngularRow(j, angle - rec.max, fp(&up0));
                NewtonUserJointSetRowStiffness(j, 1.0);
                NewtonUserJointSetRowMinimumFriction(j, 0.0);
            } else {
                (ops.no_limit)(rec.cpp, rec.previous, rec.min, rec.max, skip);
            }
        }
        reg::update_joint(h, |r| r.previous = angle);
    }
    // SAFETY: host callback.
    unsafe { (ops.position_check)(rec.cpp, g.dist_sqr) };
}

/// Shared slider/screw limit step; `store_on_limit` is whether a limit hit also stores the position.
fn axis_limit(
    j: *const NewtonJoint,
    desc: *mut NewtonHingeSliderUpdateDesc,
    posit: f32,
    stop_accel: unsafe fn(*const NewtonJoint, *const NewtonHingeSliderUpdateDesc, f32) -> f32,
    store_on_limit: bool,
) -> c_uint {
    let Some((h, rec)) = joint_of(j) else {
        return 0;
    };
    // SAFETY: `ops` outlives the joint; `desc` is Newton's live descriptor.
    let (Some(ops), Some(d)) = (unsafe { rec.ops.as_ref() }, unsafe { desc.as_mut() }) else {
        return 0;
    };
    if rec.min == 0.0 && rec.max == 0.0 {
        return 0;
    }
    // SAFETY: host callbacks and Newton stop-accel calls on the live joint.
    unsafe {
        (ops.check_auto_sleep)(rec.cpp, rec.min, rec.max, posit);
        if posit < rec.min {
            (ops.limit_event)(rec.cpp, EVENT_MIN);
            d.m_accel = stop_accel(j, d, rec.min);
            d.m_minFriction = 0.0;
            if store_on_limit {
                reg::update_joint(h, |r| r.previous = posit);
            }
            return 1;
        }
        if posit > rec.max {
            (ops.limit_event)(rec.cpp, EVENT_MAX);
            d.m_accel = stop_accel(j, d, rec.max);
            d.m_maxFriction = 0.0;
            if store_on_limit {
                reg::update_joint(h, |r| r.previous = posit);
            }
            return 1;
        }
        (ops.no_limit)(rec.cpp, rec.previous, rec.min, rec.max, false);
    }
    reg::update_joint(h, |r| r.previous = posit);
    0
}

unsafe fn slider_stop(j: *const NewtonJoint, d: *const NewtonHingeSliderUpdateDesc, p: f32) -> f32 {
    // SAFETY: forwarded contract.
    unsafe { NewtonSliderCalculateStopAccel(j, d, p) }
}

unsafe fn screw_stop(j: *const NewtonJoint, d: *const NewtonHingeSliderUpdateDesc, p: f32) -> f32 {
    // SAFETY: forwarded contract.
    unsafe { NewtonCorkscrewCalculateStopAccel(j, d, p) }
}

unsafe extern "C" fn slider_limit(
    j: *const NewtonJoint,
    desc: *mut NewtonHingeSliderUpdateDesc,
) -> c_uint {
    // SAFETY: Newton passes the live slider joint.
    let posit = unsafe { NewtonSliderGetJointPosit(j) };
    axis_limit(j, desc, posit, slider_stop, true)
}

unsafe extern "C" fn screw_limit(
    j: *const NewtonJoint,
    desc: *mut NewtonHingeSliderUpdateDesc,
) -> c_uint {
    // SAFETY: Newton passes the live corkscrew joint.
    let posit = unsafe { NewtonCorkscrewGetJointPosit(j) };
    axis_limit(j, desc, posit, screw_stop, false)
}
