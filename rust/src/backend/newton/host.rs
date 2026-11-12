//! C++ callbacks of the Newton backend, installed once with `ohpl_nt_set_host`.

use crate::physics::newton_material::{ContactAgg, GateBody};
use crate::types::{Matrixf, Vector3f};
use std::ffi::c_void;
use std::ptr;
use std::sync::atomic::{AtomicPtr, Ordering};

/// Snapshot of the `iPhysicsBody` state the force callback reads.
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct BodyState {
    pub active: bool,
    pub in_update_list: bool,
    pub buoyancy_active: bool,
    pub hpl3: bool,
    pub freeze_count: i32,
    pub density_mul: f32,
    pub density: f32,
    pub linear_viscosity: f32,
    pub angular_viscosity: f32,
    pub surface: [f32; 4],
    pub gravity: Vector3f,
}

/// Per-joint-class operations on the C++ joint (`j` is the C++ joint address).
#[repr(C)]
pub struct JointOps {
    /// kind: 0 min limit, 1 max limit, 2 no limit.
    pub limit_event: unsafe extern "C" fn(j: usize, kind: i32),
    pub check_auto_sleep: unsafe extern "C" fn(j: usize, min: f32, max: f32, value: f32),
    /// Sticky-limit hold (unless `skip`) then `OnNoLimit`.
    pub no_limit: unsafe extern "C" fn(j: usize, prev: f32, min: f32, max: f32, skip: bool),
    pub matrices: unsafe extern "C" fn(j: usize, child: *mut Matrixf, parent: *mut Matrixf),
    pub position_check: unsafe extern "C" fn(j: usize, dist_sqr: f32),
}

#[repr(C)]
pub struct Host {
    pub body_state: unsafe extern "C" fn(cpp: usize, out: *mut BodyState),
    pub body_add_to_update_list: unsafe extern "C" fn(cpp: usize),
    pub body_set_freeze_count: unsafe extern "C" fn(cpp: usize, n: i32),
    pub body_hpl3_buoyancy: unsafe extern "C" fn(
        cpp: usize,
        gravity: *const Vector3f,
        time_step: f32,
        force: *mut Vector3f,
        torque: *mut Vector3f,
    ) -> bool,
    /// `m` is Newton's column-major matrix.
    pub body_set_transform: unsafe extern "C" fn(cpp: usize, m: *const f32),
    pub body_gate: unsafe extern "C" fn(cpp: usize, gate: *mut GateBody, flags: *mut u32),
    pub collide_flags_match: unsafe extern "C" fn(a: u32, b: u32) -> bool,
    pub body_on_aabb_collision: unsafe extern "C" fn(cpp: usize, other: usize) -> bool,
    pub body_is_active: unsafe extern "C" fn(cpp: usize) -> bool,
    pub body_aabb: unsafe extern "C" fn(cpp: usize, min: *mut Vector3f, max: *mut Vector3f),
    pub contact_save_enabled: unsafe extern "C" fn(cpp: usize) -> bool,
    pub contact_push_point:
        unsafe extern "C" fn(cpp: usize, position: *const Vector3f, normal: *const Vector3f),
    pub contact_dispatch:
        unsafe extern "C" fn(cpp1: usize, cpp2: usize, agg: *const ContactAgg, count: i32),
    pub ray_before_intersect: unsafe extern "C" fn(cb: usize, body: usize) -> bool,
    /// `dist`, `normal` and `point` are null when the caller did not ask for them.
    pub ray_on_intersect: unsafe extern "C" fn(
        cb: usize,
        body: usize,
        t: f32,
        dist: *const f32,
        normal: *const Vector3f,
        point: *const Vector3f,
    ) -> bool,
    pub body_vec_push: unsafe extern "C" fn(vec: usize, cpp: usize),
    pub draw_line:
        unsafe extern "C" fn(low: usize, a: *const Vector3f, b: *const Vector3f, color: usize),
    pub buf_add: unsafe extern "C" fn(buf: usize, data: *const c_void, count: usize),
    pub buf_get: unsafe extern "C" fn(buf: usize, data: *mut c_void, count: usize),
}

static HOST: AtomicPtr<Host> = AtomicPtr::new(ptr::null_mut());

/// # Safety
/// `h` points to a `Host` that lives for the rest of the process.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_nt_set_host(h: *const Host) {
    HOST.store(h.cast_mut(), Ordering::Release);
}

pub fn host() -> Option<&'static Host> {
    // SAFETY: `ohpl_nt_set_host` stores a pointer valid for the process lifetime.
    unsafe { HOST.load(Ordering::Acquire).as_ref() }
}
