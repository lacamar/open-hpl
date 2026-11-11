//! C ABI for `node.rs` / `camera.rs`, declared in `HPL2/core/include/scene/RustNode.h`.
//!
//! Vectors cross as pointers (`cVector3` has a user-provided copy constructor). Pointers are
//! non-null, valid and aligned for the call; outputs do not alias inputs.

use super::camera::{
    CameraBasis, ProjectionParams, ViewParams, move_matrix, projection_matrix, unproject,
    view_matrix,
};
use super::node;
use crate::types::{Matrixf, Quaternion, Vector2f, Vector3f};

/// # Safety
/// All pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_scene_world_to_local(
    parent_world: *const Matrixf,
    world: *const Matrixf,
    out: *mut Matrixf,
) {
    // SAFETY: caller contract.
    unsafe { *out = node::world_to_local(&*parent_world, &*world) }
}

/// # Safety
/// All pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_scene_node_update_matrix(
    local: *const Matrixf,
    rotation: *const Quaternion,
    translation: *const Vector3f,
    out: *mut Matrixf,
) {
    // SAFETY: caller contract.
    unsafe { *out = node::update_matrix(&*local, &*rotation, *translation) }
}

/// # Safety
/// All pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_scene_node_add_rotation(
    euler: *const Vector3f,
    order: i32,
    current: *const Quaternion,
    out: *mut Quaternion,
) {
    // SAFETY: caller contract.
    unsafe { *out = node::add_rotation(*euler, order, &*current) }
}

/// # Safety
/// All pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_scene_scaled_world(
    world: *const Matrixf,
    size: *const Vector3f,
    out: *mut Matrixf,
) {
    // SAFETY: caller contract.
    unsafe { *out = node::scaled_world(&*world, *size) }
}

/// # Safety
/// All pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_scene_inverse(m: *const Matrixf, out: *mut Matrixf) {
    // SAFETY: caller contract.
    unsafe { *out = node::inverse(&*m) }
}

/// # Safety
/// All pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_scene_calc_scale(m: *const Matrixf, out: *mut Vector3f) {
    // SAFETY: caller contract.
    unsafe { *out = node::calc_scale(&*m) }
}

/// # Safety
/// All pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_scene_camera_view_matrix(
    rotate_mode: i32,
    position: *const Vector3f,
    yaw: f32,
    pitch: f32,
    roll: f32,
    rotation: *const Matrixf,
    out: *mut Matrixf,
) {
    // SAFETY: caller contract.
    unsafe {
        let p = ViewParams {
            rotate_mode,
            position: *position,
            yaw,
            pitch,
            roll,
            rotation: *rotation,
        };
        *out = view_matrix(&p)
    }
}

/// Writes `out` and returns true unless the rotate mode leaves the move matrix untouched.
///
/// # Safety
/// `out` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_scene_camera_move_matrix(
    rotate_mode: i32,
    move_mode: i32,
    yaw: f32,
    pitch: f32,
    out: *mut Matrixf,
) -> bool {
    match move_matrix(rotate_mode, move_mode, yaw, pitch) {
        Some(m) => {
            // SAFETY: caller contract.
            unsafe { *out = m };
            true
        }
        None => false,
    }
}

/// # Safety
/// All pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_scene_camera_projection(
    kind: i32,
    near: f32,
    far: f32,
    fov: f32,
    aspect: f32,
    inf_far: bool,
    view_size: *const Vector2f,
    out: *mut Matrixf,
) {
    // SAFETY: caller contract.
    unsafe {
        let p = ProjectionParams {
            kind,
            near,
            far,
            fov,
            aspect,
            inf_far,
            view_size: *view_size,
        };
        *out = projection_matrix(&p)
    }
}

/// Writes `position` and `direction` and returns true unless the projection type is unknown.
///
/// # Safety
/// All pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_scene_camera_unproject(
    kind: i32,
    near: f32,
    fov: f32,
    aspect: f32,
    view_size: *const Vector2f,
    screen: *const Vector2f,
    virtual_size: *const Vector2f,
    rotation: *const Matrixf,
    camera_pos: *const Vector3f,
    forward: *const Vector3f,
    up: *const Vector3f,
    right: *const Vector3f,
    position: *mut Vector3f,
    direction: *mut Vector3f,
) -> bool {
    // SAFETY: caller contract.
    unsafe {
        let p = ProjectionParams {
            kind,
            near,
            far: 0.0,
            fov,
            aspect,
            inf_far: false,
            view_size: *view_size,
        };
        let c = CameraBasis {
            rotation: *rotation,
            position: *camera_pos,
            forward: *forward,
            up: *up,
            right: *right,
        };
        match unproject(&p, *screen, *virtual_size, &c) {
            Some((pos, dir)) => {
                *position = pos;
                *direction = dir;
                true
            }
            None => false,
        }
    }
}
