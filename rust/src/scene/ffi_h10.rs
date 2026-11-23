//! C ABI for `h10.rs`, declared in `HPL2/core/include/scene/RustSceneH10.h`.
//!
//! Pointers are non-null, valid and aligned for the call; vectors and matrices cross as pointers.
//! Pod pointers are `EntBox` / `NodeBox` addresses, which are also the `OhplEntPod` / `OhplNodePod`
//! addresses the C++ shells read.

use super::h10::{self, CamState, EntBox, EntPub, Host, NodeBox, NodePub};
use crate::types::{Matrixf, Quaternion, Vector2f, Vector3f};
use std::mem::{offset_of, size_of};

const _: () = {
    assert!(size_of::<EntPub>() == 176 && offset_of!(EntPub, owner) == 128);
    assert!(offset_of!(EntPub, count) == 152 && offset_of!(EntPub, transform_updated) == 164);
    assert!(size_of::<NodePub>() == 336 && offset_of!(NodePub, owner) == 296);
    assert!(
        offset_of!(NodePub, custom_flags) == 320 && offset_of!(NodePub, transform_updated) == 324
    );
    assert!(size_of::<CamState>() == 468 && offset_of!(CamState, rotate_mode) == 68);
};

fn at<T: Copy>(v: &[T], i: usize) -> Option<T> {
    v.get(i).copied()
}

macro_rules! ent_ops {
    ($($name:ident => $f:ident($($arg:ident: $ty:ty),*);)*) => {$(
        /// # Safety
        /// Host table and pod pointers valid.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name(h: *const Host, e: *mut EntBox, $($arg: $ty),*) {
            // SAFETY: caller contract.
            unsafe { h10::$f(&*h, e, $($arg),*) }
        }
    )*};
}

ent_ops! {
    ohpl_h10_ent_set_transform_updated => ent_set_transform_updated(cb: bool);
    ohpl_ent_add_child => ent_add_child(c: *mut EntBox);
    ohpl_ent_add_node_child => ent_add_node_child(n: *mut NodeBox);
}

/// # Safety
/// As `ohpl_h10_ent_set_transform_updated`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h10_ent_update_world(h: *const Host, e: *mut EntBox) {
    // SAFETY: caller contract.
    unsafe { h10::ent_update_world(&*h, e) }
}

/// # Safety
/// As `ohpl_h10_ent_update_world`; `m` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h10_ent_set_matrix(
    h: *const Host,
    e: *mut EntBox,
    m: *const Matrixf,
) {
    // SAFETY: caller contract.
    unsafe { h10::ent_set_matrix(&*h, e, &*m) }
}

/// # Safety
/// As `ohpl_h10_ent_set_matrix`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h10_ent_set_world_matrix(
    h: *const Host,
    e: *mut EntBox,
    m: *const Matrixf,
) {
    // SAFETY: caller contract.
    unsafe { h10::ent_set_world_matrix(&*h, e, &*m) }
}

/// # Safety
/// As `ohpl_h10_ent_update_world`; `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h10_ent_set_position(
    h: *const Host,
    e: *mut EntBox,
    p: *const Vector3f,
) {
    // SAFETY: caller contract.
    unsafe { h10::ent_set_position(&*h, e, *p) }
}

/// # Safety
/// As `ohpl_h10_ent_set_position`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h10_ent_set_world_position(
    h: *const Host,
    e: *mut EntBox,
    p: *const Vector3f,
) {
    // SAFETY: caller contract.
    unsafe { h10::ent_set_world_position(&*h, e, *p) }
}

/// # Safety
/// As `ohpl_h10_ent_update_world`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h10_ent_bv_dirty(h: *const Host, e: *mut EntBox) -> bool {
    // SAFETY: caller contract.
    unsafe { h10::ent_bv_dirty(&*h, e) }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ent_new(owner: usize) -> *mut EntBox {
    h10::ent_new(owner)
}

/// # Safety
/// `e` from `ohpl_ent_new`, freed once.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ent_free(e: *mut EntBox) {
    // SAFETY: caller contract.
    unsafe { h10::ent_free(e) }
}

/// # Safety
/// Pod pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ent_remove_child(e: *mut EntBox, c: *mut EntBox) {
    // SAFETY: caller contract.
    unsafe { h10::ent_remove_child(e, c) }
}

/// # Safety
/// Pod pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ent_remove_node_child(e: *mut EntBox, n: *mut NodeBox) {
    // SAFETY: caller contract.
    unsafe { h10::ent_remove_node_child(e, n) }
}

/// # Safety
/// `e` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ent_is_child(e: *const EntBox, c: *const EntBox) -> bool {
    // SAFETY: caller contract.
    unsafe { (*e).kids.iter().any(|&k| std::ptr::eq(k, c)) }
}

/// # Safety
/// `e` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ent_is_node_child(e: *const EntBox, n: *const NodeBox) -> bool {
    // SAFETY: caller contract.
    unsafe { (*e).nodes.iter().any(|&k| std::ptr::eq(k, n)) }
}

/// # Safety
/// `e` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ent_child_count(e: *const EntBox) -> usize {
    // SAFETY: caller contract.
    unsafe { (*e).kids.len() }
}

/// Owner (`iEntity3D*`) of child `i`, 0 when out of range.
///
/// # Safety
/// `e` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ent_child_at(e: *const EntBox, i: usize) -> usize {
    // SAFETY: caller contract; listed pods are live.
    unsafe { at(&(*e).kids, i).map_or(0, |c| (*c).s.owner) }
}

/// # Safety
/// `e` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ent_node_child_count(e: *const EntBox) -> usize {
    // SAFETY: caller contract.
    unsafe { (*e).nodes.len() }
}

/// Owner (`cNode3D*`) of node child `i`, 0 when out of range.
///
/// # Safety
/// `e` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ent_node_child_at(e: *const EntBox, i: usize) -> usize {
    // SAFETY: caller contract; listed pods are live.
    unsafe { at(&(*e).nodes, i).map_or(0, |c| (*c).s.owner) }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_node_new(owner: usize, auto_delete: bool) -> *mut NodeBox {
    h10::node_new(owner, auto_delete)
}

/// # Safety
/// `n` from `ohpl_node_new`, freed once; `h` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_node_free(h: *const Host, n: *mut NodeBox) {
    // SAFETY: caller contract.
    unsafe { h10::node_free(&*h, n) }
}

/// # Safety
/// Pod pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_node_add_entity(n: *mut NodeBox, e: *mut EntBox) -> bool {
    // SAFETY: caller contract.
    unsafe { h10::node_add_entity(n, e) }
}

/// # Safety
/// Pod pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_node_remove_entity(n: *mut NodeBox, e: *mut EntBox) -> bool {
    // SAFETY: caller contract.
    unsafe { h10::node_remove_entity(n, e) }
}

/// # Safety
/// `n` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_node_clear_entities(n: *mut NodeBox) {
    // SAFETY: caller contract.
    unsafe { (*n).ents.clear() }
}

/// # Safety
/// `n` valid; `p` null or valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_node_set_parent(n: *mut NodeBox, p: *mut NodeBox) {
    // SAFETY: caller contract.
    unsafe { h10::node_set_parent(n, p) }
}

/// # Safety
/// `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_node_add_child(p: *mut NodeBox, c: *mut NodeBox) {
    // SAFETY: caller contract.
    unsafe { (*p).kids.push(c) }
}

/// # Safety
/// `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_node_remove_child(p: *mut NodeBox, c: *mut NodeBox) {
    // SAFETY: caller contract.
    unsafe { h10::node_remove_child(p, c) }
}

/// # Safety
/// `n` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_node_child_count(n: *const NodeBox) -> usize {
    // SAFETY: caller contract.
    unsafe { (*n).kids.len() }
}

/// Owner (`cNode3D*`) of child `i`, 0 when out of range.
///
/// # Safety
/// `n` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_node_child_at(n: *const NodeBox, i: usize) -> usize {
    // SAFETY: caller contract; listed pods are live.
    unsafe { at(&(*n).kids, i).map_or(0, |c| (*c).s.owner) }
}

/// # Safety
/// `n` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_node_entity_count(n: *const NodeBox) -> usize {
    // SAFETY: caller contract.
    unsafe { (*n).ents.len() }
}

/// Owner (`iEntity3D*`) of entity `i`, 0 when out of range.
///
/// # Safety
/// `n` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_node_entity_at(n: *const NodeBox, i: usize) -> usize {
    // SAFETY: caller contract; listed pods are live.
    unsafe { at(&(*n).ents, i).map_or(0, |c| (*c).s.owner) }
}

/// # Safety
/// As `ohpl_node_free`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_node_update_entity_children(h: *const Host, n: *mut NodeBox) {
    // SAFETY: caller contract.
    unsafe { h10::node_update_entity_children(&*h, n) }
}

/// # Safety
/// Host table and `n` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h10_node_update_world(h: *const Host, n: *mut NodeBox) {
    // SAFETY: caller contract.
    unsafe { h10::node_update_world(&*h, n) }
}

/// # Safety
/// As `ohpl_h10_node_update_world`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h10_node_set_world_transform_updated(
    h: *const Host,
    n: *mut NodeBox,
) {
    // SAFETY: caller contract.
    unsafe { h10::node_set_world_transform_updated(&*h, n) }
}

/// # Safety
/// As `ohpl_h10_node_update_world`; `m` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h10_node_set_matrix(
    h: *const Host,
    n: *mut NodeBox,
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
    n: *mut NodeBox,
    m: *const Matrixf,
) {
    // SAFETY: caller contract.
    unsafe { h10::node_set_world_matrix(&*h, n, &*m) }
}

/// # Safety
/// As `ohpl_h10_node_update_world`; `p` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h10_node_set_position(
    h: *const Host,
    n: *mut NodeBox,
    p: *const Vector3f,
) {
    // SAFETY: caller contract.
    unsafe { h10::node_set_position(&*h, n, *p) }
}

/// # Safety
/// As `ohpl_h10_node_set_position`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h10_node_set_world_position(
    h: *const Host,
    n: *mut NodeBox,
    p: *const Vector3f,
) {
    // SAFETY: caller contract.
    unsafe { h10::node_set_world_position(&*h, n, *p) }
}

/// # Safety
/// `s` and `euler` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h10_node_add_rotation_euler(
    s: *mut NodePub,
    euler: *const Vector3f,
    order: i32,
) {
    // SAFETY: caller contract.
    unsafe { h10::node_add_rotation_euler(s, *euler, order) }
}

/// # Safety
/// `s` and `q` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h10_node_add_rotation(s: *mut NodePub, q: *const Quaternion) {
    // SAFETY: caller contract.
    unsafe { h10::node_add_rotation(s, &*q) }
}

/// # Safety
/// `s` and `v` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h10_node_add_scale(s: *mut NodePub, v: *const Vector3f) {
    // SAFETY: caller contract.
    unsafe { h10::node_add_scale(s, *v) }
}

/// # Safety
/// `s` and `v` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h10_node_add_translation(s: *mut NodePub, v: *const Vector3f) {
    // SAFETY: caller contract.
    unsafe { h10::node_add_translation(s, *v) }
}

/// # Safety
/// As `ohpl_h10_node_update_world`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h10_node_update_matrix(
    h: *const Host,
    n: *mut NodeBox,
    children: bool,
) {
    // SAFETY: caller contract.
    unsafe { h10::node_update_matrix(&*h, n, children) }
}

/// # Safety
/// As `ohpl_h10_node_update_world`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h10_node_apply_anim(
    h: *const Host,
    n: *mut NodeBox,
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
