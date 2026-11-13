//! `iPhysicsRope` attachment, sleep and motor logic on the embedded `RopeState` (`OHPL_IMPL=rust`).
//! Bodies, particles and the rope itself are reached through one host op; the state is re-read
//! through its raw pointer around every op because ops re-enter C++.

use super::host::H;
use super::rope::{Attachment, RopeState};
use super::wreg::{Wa, WorldOp};
use crate::math::{IDENTITY, dot3, matrix_inverse, matrix_mul_vec, v3};
use crate::types::{Matrixf, Vector3f};

pub const RO_BODY_ANGULAR_VELOCITY: i32 = 601;
pub const RO_BODY_LINEAR_VELOCITY: i32 = 602;
pub const RO_BODY_ENABLED: i32 = 603;
pub const RO_BODY_ENABLE: i32 = 604;
pub const RO_BODY_ADD_CONTAINER: i32 = 605;
pub const RO_BODY_REMOVE_CONTAINER: i32 = 606;
pub const RO_SET_SLEEPING: i32 = 607;
pub const RO_BODY_LOCAL_MATRIX: i32 = 608;
pub const RO_PARTICLE_POS: i32 = 609;
pub const RO_RESET_PID: i32 = 610;

const SLEEP_SQR_LIMIT: f32 = 0.001f32 * 0.001f32;

fn call(op: WorldOp, code: i32, p: [usize; 6], i0: i32) -> Wa {
    let mut a = Wa {
        p,
        f: [0.0; 2],
        i: [i0, 0],
        s: std::ptr::null(),
        n: 0,
        rf: 0.0,
        rv: [0.0; 3],
    };
    // SAFETY: the op belongs to the live rope and `a` is a live block.
    a.p[5] = unsafe { op(code, &mut a) };
    a
}

fn vec(a: &Wa) -> Vector3f {
    v3(a.rv[0], a.rv[1], a.rv[2])
}

fn att(st: *mut RopeState, i: usize) -> Option<Attachment> {
    // SAFETY: caller contract of every entry point: `st` is the live embedded state.
    unsafe { st.as_ref() }.and_then(|s| s.attached.get(i).copied())
}

/// `CheckParticleBodyCollision`.
///
/// # Safety
/// `st` is the live embedded state.
pub unsafe fn check_particle_body_collision(st: *const RopeState, body: H) -> bool {
    // SAFETY: caller contract.
    let Some(s) = (unsafe { st.as_ref() }) else {
        return true;
    };
    s.collide_attachments || !s.attached.iter().any(|a| a.body == body)
}

/// `CheckSpecificDataSleeping`: motor or any attached body still moving keeps the rope awake.
///
/// # Safety
/// `st` is the live embedded state of the rope.
pub unsafe fn check_specific_sleeping(op: WorldOp, st: *mut RopeState) -> bool {
    // SAFETY: caller contract.
    if unsafe { st.as_ref() }.is_some_and(|s| s.motor_active) {
        return false;
    }
    for i in 0..2 {
        let Some(a) = att(st, i) else { return true };
        if a.body != 0 {
            let w = vec(&call(
                op,
                RO_BODY_ANGULAR_VELOCITY,
                [a.body, 0, 0, 0, 0, 0],
                0,
            ));
            if dot3(w, w) > SLEEP_SQR_LIMIT {
                return false;
            }
            let v = vec(&call(
                op,
                RO_BODY_LINEAR_VELOCITY,
                [a.body, 0, 0, 0, 0, 0],
                0,
            ));
            if dot3(v, v) > SLEEP_SQR_LIMIT {
                return false;
            }
        }
    }
    true
}

/// `CheckSpecificDataAwake`.
///
/// # Safety
/// `st` is the live embedded state of the rope.
pub unsafe fn check_specific_awake(op: WorldOp, st: *mut RopeState) -> bool {
    // SAFETY: caller contract.
    if unsafe { st.as_ref() }.is_some_and(|s| s.motor_active) {
        return true;
    }
    (0..2).any(|i| {
        att(st, i).is_some_and(|a| {
            a.body != 0 && call(op, RO_BODY_ENABLED, [a.body, 0, 0, 0, 0, 0], 0).p[5] != 0
        })
    })
}

/// `SetSpecificDataSleeping`: waking the rope enables its attached bodies.
///
/// # Safety
/// `st` is the live embedded state of the rope.
pub unsafe fn set_specific_sleeping(op: WorldOp, st: *mut RopeState, sleeping: bool) {
    for i in 0..2 {
        if let Some(a) = att(st, i)
            && a.body != 0
            && !sleeping
        {
            call(op, RO_BODY_ENABLE, [a.body, 0, 0, 0, 0, 0], 0);
        }
    }
}

/// `SetMotorActive`.
///
/// # Safety
/// `st` is the live embedded state of rope `me`.
pub unsafe fn set_motor_active(op: WorldOp, me: H, st: *mut RopeState, on: bool) {
    // SAFETY: caller contract.
    let Some(s) = (unsafe { st.as_mut() }) else {
        return;
    };
    if s.motor_active == on {
        return;
    }
    s.motor_active = on;
    if on {
        s.auto_move_speed = 0.0;
        call(op, RO_SET_SLEEPING, [me, 0, 0, 0, 0, 0], 0);
    }
}

/// `SetAutoMoveActive`.
///
/// # Safety
/// `st` is the live embedded state of rope `me`.
pub unsafe fn set_auto_move_active(op: WorldOp, me: H, st: *mut RopeState, on: bool) {
    // SAFETY: caller contract.
    if let Some(s) = unsafe { st.as_mut() } {
        s.auto_move_active = on;
    }
    if on {
        call(op, RO_SET_SLEEPING, [me, 0, 0, 0, 0, 0], 0);
    }
}

/// `RemoveAttachedBody`.
///
/// # Safety
/// `st` is the live embedded state of rope `me`.
pub unsafe fn remove_attached_body(
    op: WorldOp,
    me: H,
    st: *mut RopeState,
    body: H,
    remove_container: bool,
) {
    // SAFETY: caller contract.
    if let Some(s) = unsafe { st.as_mut() } {
        for a in &mut s.attached {
            if a.body == body {
                a.body = 0;
            }
        }
    }
    if remove_container {
        call(op, RO_BODY_REMOVE_CONTAINER, [body, me, 0, 0, 0, 0], 0);
    }
}

/// `SetAttachedBody`: detaches the previous body, attaches `body` and stores the particle's body-local position.
///
/// # Safety
/// `st` is the live embedded state of rope `me`.
pub unsafe fn set_attached_body(
    op: WorldOp,
    me: H,
    st: *mut RopeState,
    idx: usize,
    particle: H,
    body: H,
) {
    call(op, RO_RESET_PID, [me, idx, 0, 0, 0, 0], 0);
    if let Some(a) = att(st, idx)
        && a.body != 0
    {
        call(op, RO_BODY_REMOVE_CONTAINER, [a.body, me, 0, 0, 0, 0], 0);
    }
    call(op, RO_BODY_ADD_CONTAINER, [body, me, 0, 0, 0, 0], 0);

    let mut m: Matrixf = IDENTITY;
    call(
        op,
        RO_BODY_LOCAL_MATRIX,
        [body, &mut m as *mut Matrixf as usize, 0, 0, 0, 0],
        0,
    );
    let pos = vec(&call(op, RO_PARTICLE_POS, [particle, 0, 0, 0, 0, 0], 0));
    let local = matrix_mul_vec(&matrix_inverse(&m), pos);
    // SAFETY: caller contract.
    if let Some(s) = unsafe { st.as_mut() }
        && let Some(a) = s.attached.get_mut(idx)
    {
        a.body = body;
        a.particle = particle as *mut super::rope::Particle;
        a.body_local_pos = local;
    }
}
