//! C ABI of the body and joint shells, mirrored in `RustPhysBook.h`.

use super::host::Str;
use super::shell::{self, BodyState, JointStateC};
use super::wreg::WorldOp;
use crate::types::Vector3f;

/// # Safety
/// `state` is writable and embedded in the body `me`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h15b_new(
    op: WorldOp,
    me: usize,
    state: *mut BodyState,
    world: usize,
    shape: usize,
) -> u64 {
    // SAFETY: caller contract.
    unsafe { shell::body_new(op, me, state, world, shape) }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15b_free(b: u64) {
    shell::body_free(b);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15b_add_joint(b: u64, j: usize) {
    shell::body_add_joint(b, j);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15b_joint_at(b: u64, i: i32) -> usize {
    shell::body_joint_at(b, i)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15b_joint_num(b: u64) -> i32 {
    shell::body_joint_num(b)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15b_remove_joint(b: u64, j: usize) {
    shell::body_remove_joint(b, j);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15b_add_callback(b: u64, cb: usize) {
    shell::body_add_callback(b, cb);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15b_remove_callback(b: u64, cb: usize) {
    shell::body_remove_callback(b, cb);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15b_on_aabb(b: u64, me: usize, other: usize) -> bool {
    shell::body_on_aabb(b, me, other)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15b_on_collide(b: u64, me: usize, other: usize, contact: usize) {
    shell::body_on_collide(b, me, other, contact);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15b_add_char(b: u64, c: usize) {
    shell::body_add_char(b, c);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15b_remove_char(b: u64, c: usize) {
    shell::body_remove_char(b, c);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15b_add_verlet(b: u64, c: usize) {
    shell::body_add_verlet(b, c);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15b_remove_verlet(b: u64, c: usize) {
    shell::body_remove_verlet(b, c);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15b_destroy(b: u64, me: usize) {
    shell::body_destroy(b, me);
}

/// # Safety
/// `state` is writable and embedded in the joint `me`; `vt` is the static physics host table.
#[unsafe(no_mangle)]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ohpl_h15j_new(
    op: WorldOp,
    me: usize,
    state: *mut JointStateC,
    vt: usize,
    parent: usize,
    child: usize,
    world: usize,
    pivot: Vector3f,
    pin: Vector3f,
) -> u64 {
    // SAFETY: caller contract.
    unsafe { shell::joint_new(op, me, state, vt, parent, child, world, pivot, pin) }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15j_drop(j: u64, me: usize) {
    shell::joint_drop(j, me);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15j_add_controller(j: u64, me: usize, ctrl: usize) {
    shell::joint_add_controller(j, me, ctrl);
}

/// # Safety
/// `name` points to `n` readable bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h15j_get_controller(j: u64, name: *const u8, n: usize) -> usize {
    // SAFETY: caller contract.
    shell::joint_get_controller(j, unsafe { Str { p: name, n }.bytes() })
}

/// # Safety
/// `name` points to `n` readable bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h15j_change_controller(j: u64, name: *const u8, n: usize) -> bool {
    // SAFETY: caller contract.
    shell::joint_change_controller(j, unsafe { Str { p: name, n }.bytes() })
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15j_set_all_paused(j: u64, paused: bool) {
    shell::joint_set_all_paused(j, paused);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15j_controller_len(j: u64) -> usize {
    shell::joint_controller_len(j)
}

/// # Safety
/// `name` and `obj` are writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h15j_controller_at(
    j: u64,
    i: usize,
    name: *mut Str,
    obj: *mut usize,
) -> bool {
    let Some((p, n, h)) = shell::joint_controller_at(j, i) else {
        return false;
    };
    // SAFETY: caller contract.
    unsafe {
        if let Some(o) = name.as_mut() {
            *o = Str { p, n };
        }
        if let Some(o) = obj.as_mut() {
            *o = h;
        }
    }
    true
}

/// # Safety
/// The shell's state and host table are live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h15j_update(j: u64, me: usize) -> bool {
    // SAFETY: caller contract.
    unsafe { shell::joint_update(j, me) }
}

/// # Safety
/// The shell's state and host table are live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h15j_check_breakage(j: u64, me: usize) -> bool {
    // SAFETY: caller contract.
    unsafe { shell::joint_check_breakage(j, me) }
}
