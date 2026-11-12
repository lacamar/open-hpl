//! C ABI for `h10.rs`, declared in `HPL2/core/include/scene/RustSceneH10.h`.
//!
//! Pointers are non-null, valid and aligned for the call; vectors and matrices cross as pointers.

use super::h10::{self, CamState, EntView, Host, NodeState};
use crate::types::{Matrixf, Quaternion, Vector2f, Vector3f};

const _: () = {
    use std::mem::{offset_of, size_of};
    assert!(size_of::<EntView>() == 64 && offset_of!(EntView, parent_node) == 48);
    assert!(size_of::<NodeState>() == 320 && offset_of!(NodeState, parent) == 296);
    assert!(size_of::<CamState>() == 468 && offset_of!(CamState, rotate_mode) == 68);
};

/// # Safety
/// Host table and `e` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h10_ent_update_world(h: *const Host, e: usize) {
    // SAFETY: caller contract.
    unsafe { h10::ent_update_world(&*h, e) }
}

/// # Safety
/// As `ohpl_h10_ent_update_world`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h10_ent_set_transform_updated(h: *const Host, e: usize, cb: bool) {
    // SAFETY: caller contract.
    unsafe { h10::ent_set_transform_updated(&*h, e, cb) }
}

/// # Safety
/// As `ohpl_h10_ent_update_world`; `m` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h10_ent_set_matrix(h: *const Host, e: usize, m: *const Matrixf) {
    // SAFETY: caller contract.
    unsafe { h10::ent_set_matrix(&*h, e, &*m) }
}

/// # Safety
/// As `ohpl_h10_ent_set_matrix`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h10_ent_set_world_matrix(
    h: *const Host,
    e: usize,
    m: *const Matrixf,
) {
    // SAFETY: caller contract.
    unsafe { h10::ent_set_world_matrix(&*h, e, &*m) }
}

/// # Safety
/// As `ohpl_h10_ent_update_world`; `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h10_ent_set_position(h: *const Host, e: usize, p: *const Vector3f) {
    // SAFETY: caller contract.
    unsafe { h10::ent_set_position(&*h, e, *p) }
}

/// # Safety
/// As `ohpl_h10_ent_set_position`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h10_ent_set_world_position(
    h: *const Host,
    e: usize,
    p: *const Vector3f,
) {
    // SAFETY: caller contract.
    unsafe { h10::ent_set_world_position(&*h, e, *p) }
}

/// # Safety
/// As `ohpl_h10_ent_update_world`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h10_ent_bv_dirty(h: *const Host, e: usize) -> bool {
    // SAFETY: caller contract.
    unsafe { h10::ent_bv_dirty(&*h, e) }
}

/// # Safety
/// Host table and `n` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h10_node_update_world(h: *const Host, n: usize) {
    // SAFETY: caller contract.
    unsafe { h10::node_update_world(&*h, n) }
}

/// # Safety
/// As `ohpl_h10_node_update_world`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h10_node_set_world_transform_updated(h: *const Host, n: usize) {
    // SAFETY: caller contract.
    unsafe { h10::node_set_world_transform_updated(&*h, n) }
}

/// # Safety
/// As `ohpl_h10_node_update_world`; `m` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h10_node_set_matrix(
    h: *const Host,
    n: usize,
    m: *const Matrixf,
    children: bool,
) {
    // SAFETY: caller contract.
    unsafe { h10::node_set_matrix(&*h, n, &*m, children) }
}

/// # Safety
/// As `ohpl_h10_node_set_matrix`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h10_node_set_world_matrix(
    h: *const Host,
    n: usize,
    m: *const Matrixf,
) {
    // SAFETY: caller contract.
    unsafe { h10::node_set_world_matrix(&*h, n, &*m) }
}

/// # Safety
/// As `ohpl_h10_node_update_world`; `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h10_node_set_position(h: *const Host, n: usize, p: *const Vector3f) {
    // SAFETY: caller contract.
    unsafe { h10::node_set_position(&*h, n, *p) }
}

/// # Safety
/// As `ohpl_h10_node_set_position`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h10_node_set_world_position(
    h: *const Host,
    n: usize,
    p: *const Vector3f,
) {
    // SAFETY: caller contract.
    unsafe { h10::node_set_world_position(&*h, n, *p) }
}

/// # Safety
/// `s` and `euler` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h10_node_add_rotation_euler(
    s: *mut NodeState,
    euler: *const Vector3f,
    order: i32,
) {
    // SAFETY: caller contract.
    unsafe { h10::node_add_rotation_euler(s, *euler, order) }
}

/// # Safety
/// `s` and `q` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h10_node_add_rotation(s: *mut NodeState, q: *const Quaternion) {
    // SAFETY: caller contract.
    unsafe { h10::node_add_rotation(s, &*q) }
}

/// # Safety
/// `s` and `v` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h10_node_add_scale(s: *mut NodeState, v: *const Vector3f) {
    // SAFETY: caller contract.
    unsafe { h10::node_add_scale(s, *v) }
}

/// # Safety
/// `s` and `v` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h10_node_add_translation(s: *mut NodeState, v: *const Vector3f) {
    // SAFETY: caller contract.
    unsafe { h10::node_add_translation(s, *v) }
}

/// # Safety
/// As `ohpl_h10_node_update_world`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h10_node_update_matrix(h: *const Host, n: usize, children: bool) {
    // SAFETY: caller contract.
    unsafe { h10::node_update_matrix(&*h, n, children) }
}

/// # Safety
/// As `ohpl_h10_node_update_world`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h10_node_apply_anim(
    h: *const Host,
    n: usize,
    post: bool,
    children: bool,
) {
    // SAFETY: caller contract.
    unsafe { h10::node_apply_anim(&*h, n, post, children) }
}

macro_rules! cam_fns {
    ($($name:ident($s:ident, $($arg:ident: $ty:ty),*) $body:block)*) => {$(
        /// # Safety
        /// `s` and pointer arguments valid.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name(s: *mut CamState, $($arg: $ty),*) {
            // SAFETY: caller contract.
            let $s = unsafe { &mut *s };
            $body
        }
    )*};
}

cam_fns! {
    ohpl_h10_cam_set_position(s, p: *const Vector3f) { s.set_position(unsafe { *p }) }
    ohpl_h10_cam_set_pitch(s, a: f32) { s.set_pitch(a) }
    ohpl_h10_cam_set_yaw(s, a: f32) { s.set_yaw(a) }
    ohpl_h10_cam_set_roll(s, a: f32) { s.set_roll(a) }
    ohpl_h10_cam_add_pitch(s, a: f32) { s.add_pitch(a) }
    ohpl_h10_cam_add_yaw(s, a: f32) { s.add_yaw(a) }
    ohpl_h10_cam_add_roll(s, a: f32) { s.add_roll(a) }
    ohpl_h10_cam_move(s, axis: i32, d: f32) { s.move_along(axis, d) }
    ohpl_h10_cam_set_fov(s, v: f32) { s.set_fov(v) }
    ohpl_h10_cam_set_aspect(s, v: f32) { s.set_aspect(v) }
    ohpl_h10_cam_set_far(s, v: f32) { s.set_far(v) }
    ohpl_h10_cam_set_near(s, v: f32) { s.set_near(v) }
    ohpl_h10_cam_set_projection_type(s, t: i32) { s.set_projection_type(t) }
    ohpl_h10_cam_set_ortho_view_size(s, v: *const Vector2f) { s.set_ortho_view_size(unsafe { *v }) }
    ohpl_h10_cam_set_inf_far(s, v: bool) { s.set_inf_far(v) }
    ohpl_h10_cam_set_rotate_mode(s, m: i32) { s.set_rotate_mode(m) }
    ohpl_h10_cam_set_move_mode(s, m: i32) { s.set_move_mode(m) }
    ohpl_h10_cam_reset_rotation(s,) { s.reset_rotation() }
    ohpl_h10_cam_set_rotation_row(s, row: i32, v: *const Vector3f) {
        s.set_rotation_row(usize::try_from(row).unwrap_or(0), unsafe { *v })
    }
    ohpl_h10_cam_set_rotation_matrix(s, m: *const Matrixf) { s.set_rotation_matrix(unsafe { &*m }) }
    ohpl_h10_cam_frustum_end(s, was_inf: bool) { s.frustum_end(was_inf) }
}

/// # Safety
/// `s` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h10_cam_frustum_begin(s: *mut CamState) -> u32 {
    // SAFETY: caller contract.
    unsafe { (*s).frustum_begin() }
}

/// Rebuilds when dirty and returns the stored matrix: `which` 0 view, 1 projection, 2 move.
///
/// # Safety
/// `s` valid; the result lives as long as the camera.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h10_cam_matrix(s: *mut CamState, which: i32) -> *const Matrixf {
    // SAFETY: caller contract.
    let s = unsafe { &mut *s };
    match which {
        0 => s.view_matrix(),
        1 => s.projection_matrix(),
        _ => {
            s.update_move();
            &s.mv
        }
    }
}

/// `GetForward` (row 2, negated), `GetRight` (0), `GetUp` (1).
///
/// # Safety
/// `s` and `out` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h10_cam_view_row(
    s: *mut CamState,
    row: i32,
    negate: bool,
    out: *mut Vector3f,
) {
    // SAFETY: caller contract.
    unsafe { *out = (*s).view_row(usize::try_from(row).unwrap_or(0), negate) }
}
