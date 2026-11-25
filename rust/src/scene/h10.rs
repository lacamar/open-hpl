//! `iEntity3D`, `cNode3D` and `cCamera` state. The entity and node pods are box-pinned and owned
//! here (the C++ classes keep a pointer, their vtables, strings and callback lists); transforms,
//! dirty flags, parent links and child lists live here.
//!
//! No reference into a pod is held across a host call: every access is a short raw-pointer scope,
//! because host calls reach virtual methods that call back into these functions.

use super::node::{
    add_rotation, matrix_mul, ohpl_math_quaternion_mul, update_matrix, world_to_local,
};
use crate::math::{IDENTITY, fma, sub3};
use crate::types::{Matrixf, Quaternion, Vector2f, Vector3f};
use std::ptr::null_mut;

fn translation_of(m: &Matrixf) -> Vector3f {
    Vector3f {
        x: m.m[0][3],
        y: m.m[1][3],
        z: m.m[2][3],
    }
}

/// C++-visible part of an `iEntity3D` pod (`OhplEntPod`).
#[repr(C)]
pub struct EntPub {
    pub local: Matrixf,
    pub world: Matrixf,
    pub owner: usize,
    pub parent_node: *mut NodeBox,
    pub parent_entity: *mut EntBox,
    pub count: i32,
    pub unique_id: i32,
    pub iterator_count: i32,
    pub transform_updated: bool,
    pub update_bv: bool,
    pub apply_bv: bool,
    pub active: bool,
    pub saved: bool,
}

/// An `iEntity3D` pod; `s` is first so the box address is the `EntPub` address.
#[repr(C)]
pub struct EntBox {
    pub s: EntPub,
    pub kids: Vec<*mut EntBox>,
    pub nodes: Vec<*mut NodeBox>,
}

/// C++-visible part of a `cNode3D` pod (`OhplNodePod`).
#[repr(C)]
pub struct NodePub {
    pub local: Matrixf,
    pub world: Matrixf,
    pub pre: Matrixf,
    pub post: Matrixf,
    pub rotation: Quaternion,
    pub scale: Vector3f,
    pub translation: Vector3f,
    pub owner: usize,
    pub parent: *mut NodeBox,
    pub entity_parent: *mut EntBox,
    pub custom_flags: i32,
    pub transform_updated: bool,
    pub use_pre: bool,
    pub use_post: bool,
    pub active: bool,
    pub auto_delete: bool,
}

/// A `cNode3D` pod.
#[repr(C)]
pub struct NodeBox {
    pub s: NodePub,
    pub kids: Vec<*mut NodeBox>,
    pub ents: Vec<*mut EntBox>,
}

/// C++ virtuals: objects are `iEntity3D*` / `cNode3D*` as `usize`.
#[repr(C)]
pub struct Host {
    pub ent_on_updated: unsafe extern "C" fn(usize),
    pub ent_callbacks: unsafe extern "C" fn(usize),
    pub node_delete: unsafe extern "C" fn(usize),
}

const ONE: Vector3f = Vector3f {
    x: 1.0,
    y: 1.0,
    z: 1.0,
};

const IDENTITY_Q: Quaternion = Quaternion {
    v: Vector3f {
        x: 0.0,
        y: 0.0,
        z: 0.0,
    },
    w: 1.0,
};

pub fn ent_new(owner: usize) -> *mut EntBox {
    Box::into_raw(Box::new(EntBox {
        s: EntPub {
            local: IDENTITY,
            world: IDENTITY,
            owner,
            parent_node: null_mut(),
            parent_entity: null_mut(),
            count: 0,
            unique_id: -1,
            iterator_count: -1,
            transform_updated: true,
            update_bv: true,
            apply_bv: true,
            active: true,
            saved: true,
        },
        kids: Vec::new(),
        nodes: Vec::new(),
    }))
}

pub fn node_new(owner: usize, auto_delete: bool) -> *mut NodeBox {
    Box::into_raw(Box::new(NodeBox {
        s: NodePub {
            local: IDENTITY,
            world: IDENTITY,
            pre: IDENTITY,
            post: IDENTITY,
            rotation: IDENTITY_Q,
            scale: ONE,
            translation: Vector3f::default(),
            owner,
            parent: null_mut(),
            entity_parent: null_mut(),
            custom_flags: 0,
            transform_updated: true,
            use_pre: false,
            use_post: false,
            active: true,
            auto_delete,
        },
        kids: Vec::new(),
        ents: Vec::new(),
    }))
}

/// `iEntity3D::~iEntity3D`: unlink from the parent, orphan children, free the pod.
///
/// # Safety
/// `e` from `ent_new`, freed once; linked pods live.
pub unsafe fn ent_free(e: *mut EntBox) {
    // SAFETY: caller contract; every linked pod is live until its own free unlinks it.
    unsafe {
        if !(*e).s.parent_node.is_null() {
            node_remove_entity((*e).s.parent_node, e);
        } else if !(*e).s.parent_entity.is_null() {
            ent_remove_child((*e).s.parent_entity, e);
        }
        for &c in &(*e).kids {
            (*c).s.parent_entity = null_mut();
        }
        for &n in &(*e).nodes {
            (*n).s.entity_parent = null_mut();
        }
        drop(Box::from_raw(e));
    }
}

/// `cNode3D::~cNode3D`: unlink from the parents, orphan entities and children, delete the
/// children when auto-deleting, free the pod.
///
/// # Safety
/// `n` from `node_new`, freed once; linked pods live.
pub unsafe fn node_free(h: &Host, n: *mut NodeBox) {
    // SAFETY: as `ent_free`.
    unsafe {
        if !(*n).s.entity_parent.is_null() {
            ent_remove_node_child((*n).s.entity_parent, n);
        } else if !(*n).s.parent.is_null() {
            node_remove_child((*n).s.parent, n);
        }
        for &e in &(*n).ents {
            (*e).s.parent_node = null_mut();
        }
        let kids = (*n).kids.clone();
        for &c in &kids {
            (*c).s.parent = null_mut();
        }
        if (*n).s.auto_delete {
            for &c in &kids {
                (h.node_delete)((*c).s.owner);
            }
        }
        drop(Box::from_raw(n));
    }
}

/// # Safety
/// `e` and its parents live.
pub unsafe fn ent_update_world(h: &Host, e: *mut EntBox) {
    // SAFETY: caller contract; copies end before each recursion.
    unsafe {
        let s = &mut (*e).s;
        if !s.transform_updated {
            return;
        }
        s.transform_updated = false;
        let (pn, pe) = (s.parent_node, s.parent_entity);
        if !pn.is_null() {
            node_update_world(h, pn);
            let pw = (*pn).s.world;
            let local = (*e).s.local;
            (*e).s.world = matrix_mul(&pw, &local);
        } else if !pe.is_null() {
            ent_update_world(h, pe);
            let pw = (*pe).s.world;
            let local = (*e).s.local;
            (*e).s.world = matrix_mul(&pw, &local);
        } else {
            (*e).s.world = (*e).s.local;
        }
    }
}

/// `iEntity3D::SetTransformUpdated`.
///
/// # Safety
/// As `ent_update_world`.
pub unsafe fn ent_set_transform_updated(h: &Host, e: *mut EntBox, callbacks: bool) {
    // SAFETY: caller contract.
    unsafe {
        let s = &mut (*e).s;
        s.transform_updated = true;
        s.count = s.count.wrapping_add(1);
        s.update_bv = true;
        let owner = s.owner;
        (h.ent_on_updated)(owner);
        for c in (*e).kids.clone() {
            ent_set_transform_updated(h, c, true);
        }
        for n in (*e).nodes.clone() {
            node_set_world_transform_updated(h, n);
        }
        if callbacks {
            (h.ent_callbacks)(owner);
        }
    }
}

/// # Safety
/// As `ent_update_world`.
pub unsafe fn ent_set_matrix(h: &Host, e: *mut EntBox, m: &Matrixf) {
    // SAFETY: caller contract.
    unsafe {
        (*e).s.local = *m;
        ent_set_transform_updated(h, e, true);
    }
}

/// # Safety
/// As `ent_update_world`.
pub unsafe fn ent_set_position(h: &Host, e: *mut EntBox, p: Vector3f) {
    // SAFETY: caller contract.
    unsafe {
        let l = &mut (*e).s.local;
        l.m[0][3] = p.x;
        l.m[1][3] = p.y;
        l.m[2][3] = p.z;
        ent_set_transform_updated(h, e, true);
    }
}

/// # Safety
/// As `ent_update_world`.
pub unsafe fn ent_set_world_position(h: &Host, e: *mut EntBox, p: Vector3f) {
    // SAFETY: caller contract.
    unsafe {
        let pe = (*e).s.parent_entity;
        if pe.is_null() {
            ent_set_position(h, e, p);
            return;
        }
        ent_update_world(h, pe);
        let pw = translation_of(&(*pe).s.world);
        ent_set_position(h, e, sub3(p, pw));
    }
}

/// # Safety
/// As `ent_update_world`.
pub unsafe fn ent_set_world_matrix(h: &Host, e: *mut EntBox, m: &Matrixf) {
    // SAFETY: caller contract.
    unsafe {
        let pe = (*e).s.parent_entity;
        if pe.is_null() {
            ent_set_matrix(h, e, m);
            return;
        }
        ent_update_world(h, pe);
        let pw = (*pe).s.world;
        ent_set_matrix(h, e, &world_to_local(&pw, m));
    }
}

/// `iEntity3D::GetBoundingVolume` flag step: true when the C++ side must apply the world matrix
/// to its bounding volume.
///
/// # Safety
/// As `ent_update_world`.
pub unsafe fn ent_bv_dirty(h: &Host, e: *mut EntBox) -> bool {
    // SAFETY: caller contract.
    unsafe {
        if (*e).s.apply_bv && (*e).s.update_bv {
            ent_update_world(h, e);
            (*e).s.update_bv = false;
            return true;
        }
    }
    false
}

/// `iEntity3D::AddChild`.
///
/// # Safety
/// As `ent_update_world`; `c` may be null.
pub unsafe fn ent_add_child(h: &Host, e: *mut EntBox, c: *mut EntBox) {
    if c.is_null() {
        return;
    }
    // SAFETY: caller contract.
    unsafe {
        if !(*c).s.parent_entity.is_null() {
            ent_remove_child((*c).s.parent_entity, c);
        }
        (*e).kids.push(c);
        (*c).s.parent_entity = e;
        ent_set_transform_updated(h, c, true);
    }
}

/// `iEntity3D::RemoveChild`: drops every occurrence.
///
/// # Safety
/// `e` live; `c` is compared only.
pub unsafe fn ent_remove_child(e: *mut EntBox, c: *mut EntBox) {
    // SAFETY: caller contract.
    unsafe {
        (*e).kids.retain(|&k| {
            if k == c {
                (*k).s.parent_entity = null_mut();
                false
            } else {
                true
            }
        });
    }
}

/// `iEntity3D::AddNodeChild`.
///
/// # Safety
/// As `ent_update_world`.
pub unsafe fn ent_add_node_child(h: &Host, e: *mut EntBox, n: *mut NodeBox) {
    // SAFETY: caller contract.
    unsafe {
        if !(*n).s.entity_parent.is_null() {
            ent_remove_node_child((*n).s.entity_parent, n);
        }
        (*e).nodes.push(n);
        (*n).s.entity_parent = e;
        node_set_world_transform_updated(h, n);
    }
}

/// `iEntity3D::RemoveNodeChild`: drops every occurrence.
///
/// # Safety
/// `e` live; `n` is compared only.
pub unsafe fn ent_remove_node_child(e: *mut EntBox, n: *mut NodeBox) {
    // SAFETY: caller contract.
    unsafe {
        (*e).nodes.retain(|&k| {
            if k == n {
                (*k).s.entity_parent = null_mut();
                false
            } else {
                true
            }
        });
    }
}

/// `cNode3D::AddEntity`.
///
/// # Safety
/// `n` and `e` live.
pub unsafe fn node_add_entity(n: *mut NodeBox, e: *mut EntBox) -> bool {
    // SAFETY: caller contract.
    unsafe {
        if !(*e).s.parent_node.is_null() {
            return false;
        }
        (*n).ents.push(e);
        (*e).s.parent_node = n;
    }
    true
}

/// `cNode3D::RemoveEntity`.
///
/// # Safety
/// `n` and `e` live.
pub unsafe fn node_remove_entity(n: *mut NodeBox, e: *mut EntBox) -> bool {
    // SAFETY: caller contract.
    unsafe {
        let Some(i) = (*n).ents.iter().position(|&x| x == e) else {
            return false;
        };
        (*e).s.parent_node = null_mut();
        (*n).ents.remove(i);
    }
    true
}

/// `cNode3D::SetParent`: links `n` below `p` (child list push when `p` is set).
///
/// # Safety
/// `n` live; `p` null or live.
pub unsafe fn node_set_parent(n: *mut NodeBox, p: *mut NodeBox) {
    // SAFETY: caller contract.
    unsafe {
        (*n).s.parent = p;
        if !p.is_null() {
            (*p).kids.push(n);
        }
    }
}

/// `cNode3D::RemoveChild`: erases the first occurrence only.
///
/// # Safety
/// `p` live.
pub unsafe fn node_remove_child(p: *mut NodeBox, c: *mut NodeBox) {
    // SAFETY: caller contract.
    unsafe {
        if let Some(i) = (*p).kids.iter().position(|&x| x == c) {
            (*p).kids.remove(i);
        }
    }
}

/// `cNode3D::UpdateEntityChildren`.
///
/// # Safety
/// As `ent_update_world`.
pub unsafe fn node_update_entity_children(h: &Host, n: *mut NodeBox) {
    // SAFETY: caller contract.
    unsafe {
        for e in (*n).ents.clone() {
            ent_set_transform_updated(h, e, true);
        }
    }
}

/// # Safety
/// `h` and `n` valid.
pub unsafe fn node_update_world(h: &Host, n: *mut NodeBox) {
    // SAFETY: caller contract; copies end before each recursion.
    unsafe {
        let s = &mut (*n).s;
        if !s.transform_updated {
            return;
        }
        s.transform_updated = false;
        let (parent, eparent) = (s.parent, s.entity_parent);
        if !parent.is_null() {
            node_update_world(h, parent);
            let pw = (*parent).s.world;
            let local = (*n).s.local;
            (*n).s.world = matrix_mul(&pw, &local);
        } else if !eparent.is_null() {
            ent_update_world(h, eparent);
            let pw = (*eparent).s.world;
            let local = (*n).s.local;
            (*n).s.world = matrix_mul(&pw, &local);
        } else {
            (*n).s.world = (*n).s.local;
        }
    }
}

/// `cNode3D::SetWorldTransformUpdated`.
///
/// # Safety
/// As `node_update_world`.
pub unsafe fn node_set_world_transform_updated(h: &Host, n: *mut NodeBox) {
    // SAFETY: caller contract.
    unsafe {
        (*n).s.transform_updated = true;
        for e in (*n).ents.clone() {
            ent_set_transform_updated(h, e, true);
        }
        for c in (*n).kids.clone() {
            node_set_world_transform_updated(h, c);
        }
    }
}

/// # Safety
/// As `node_update_world`.
pub unsafe fn node_set_matrix(h: &Host, n: *mut NodeBox, m: &Matrixf, children: bool) {
    // SAFETY: caller contract.
    unsafe {
        (*n).s.local = *m;
        if children {
            node_set_world_transform_updated(h, n);
        } else {
            (*n).s.transform_updated = true;
        }
    }
}

/// # Safety
/// As `node_update_world`.
pub unsafe fn node_set_position(h: &Host, n: *mut NodeBox, p: Vector3f) {
    // SAFETY: caller contract.
    unsafe {
        let l = &mut (*n).s.local;
        l.m[0][3] = p.x;
        l.m[1][3] = p.y;
        l.m[2][3] = p.z;
        node_set_world_transform_updated(h, n);
    }
}

/// # Safety
/// As `node_update_world`.
pub unsafe fn node_set_world_position(h: &Host, n: *mut NodeBox, p: Vector3f) {
    // SAFETY: caller contract.
    unsafe {
        let parent = (*n).s.parent;
        if parent.is_null() {
            node_set_position(h, n, p);
            return;
        }
        node_update_world(h, parent);
        let pw = translation_of(&(*parent).s.world);
        node_set_position(h, n, sub3(p, pw));
    }
}

/// # Safety
/// As `node_update_world`.
pub unsafe fn node_set_world_matrix(h: &Host, n: *mut NodeBox, m: &Matrixf) {
    // SAFETY: caller contract.
    unsafe {
        let parent = (*n).s.parent;
        if parent.is_null() {
            node_set_matrix(h, n, m, true);
            return;
        }
        node_update_world(h, parent);
        let pw = (*parent).s.world;
        node_set_matrix(h, n, &world_to_local(&pw, m), true);
    }
}

/// # Safety
/// `s` valid.
pub unsafe fn node_add_rotation_euler(s: *mut NodePub, euler: Vector3f, order: i32) {
    // SAFETY: caller contract.
    unsafe { (*s).rotation = add_rotation(euler, order, &(*s).rotation) };
}

/// # Safety
/// `s` valid.
pub unsafe fn node_add_rotation(s: *mut NodePub, q: &Quaternion) {
    let mut out = Quaternion::default();
    // SAFETY: caller contract.
    unsafe {
        let cur = (*s).rotation;
        ohpl_math_quaternion_mul(q, &cur, &mut out);
        (*s).rotation = out;
    }
}

/// # Safety
/// `s` valid.
pub unsafe fn node_add_scale(s: *mut NodePub, v: Vector3f) {
    // SAFETY: caller contract.
    unsafe {
        (*s).scale.x *= v.x;
        (*s).scale.y *= v.y;
        (*s).scale.z *= v.z;
    }
}

/// # Safety
/// `s` valid.
pub unsafe fn node_add_translation(s: *mut NodePub, v: Vector3f) {
    // SAFETY: caller contract.
    unsafe {
        (*s).translation.x += v.x;
        (*s).translation.y += v.y;
        (*s).translation.z += v.z;
    }
}

/// `cNode3D::UpdateMatrix`.
///
/// # Safety
/// As `node_update_world`.
pub unsafe fn node_update_matrix(h: &Host, n: *mut NodeBox, children: bool) {
    // SAFETY: caller contract.
    unsafe {
        let s = &(*n).s;
        let m = update_matrix(&s.local, &s.rotation, s.translation);
        node_set_matrix(h, n, &m, children);
        let s = &mut (*n).s;
        s.rotation = IDENTITY_Q;
        s.scale = ONE;
        s.translation = Vector3f::default();
    }
}

/// `cNode3D::ApplyPreAnimTransform` / `ApplyPostAnimTransform`.
///
/// # Safety
/// As `node_update_world`.
pub unsafe fn node_apply_anim(h: &Host, n: *mut NodeBox, post: bool, children: bool) {
    // SAFETY: caller contract.
    unsafe {
        let s = &(*n).s;
        let (on, local, t) = if post {
            (s.use_post, s.local, s.post)
        } else {
            (s.use_pre, s.local, s.pre)
        };
        if !on {
            return;
        }
        let m = if post {
            matrix_mul(&t, &local)
        } else {
            matrix_mul(&local, &t)
        };
        node_set_matrix(h, n, &m, children);
    }
}

const PROJ_ORTHO: i32 = 1;

/// State of one `cCamera` (the node and frustum stay C++ members).
#[repr(C)]
pub struct CamState {
    pub position: Vector3f,
    pub fov: f32,
    pub aspect: f32,
    pub far: f32,
    pub near: f32,
    pub view_size: Vector2f,
    pub projection_type: i32,
    pub pitch: f32,
    pub yaw: f32,
    pub roll: f32,
    pub pitch_min: f32,
    pub pitch_max: f32,
    pub yaw_min: f32,
    pub yaw_max: f32,
    pub rotate_mode: i32,
    pub move_mode: i32,
    pub view: Matrixf,
    pub projection: Matrixf,
    pub mv: Matrixf,
    pub prev_view: Matrixf,
    pub prev_projection: Matrixf,
    pub rotation: Matrixf,
    pub inf_far: bool,
    pub view_updated: bool,
    pub projection_updated: bool,
    pub move_updated: bool,
    pub frustum_updated: bool,
}

impl CamState {
    fn dirty_view(&mut self) {
        self.view_updated = true;
        self.move_updated = true;
        self.frustum_updated = true;
    }

    fn dirty_projection(&mut self) {
        self.projection_updated = true;
        self.frustum_updated = true;
    }

    pub fn set_position(&mut self, p: Vector3f) {
        self.position = p;
        self.view_updated = true;
        self.frustum_updated = true;
    }

    pub fn set_pitch(&mut self, a: f32) {
        self.pitch = a;
        if self.pitch_min != 0.0 || self.pitch_max != 0.0 {
            if self.pitch > self.pitch_max {
                self.pitch = self.pitch_max;
            }
            if self.pitch < self.pitch_min {
                self.pitch = self.pitch_min;
            }
        }
        self.dirty_view();
    }

    pub fn set_yaw(&mut self, a: f32) {
        self.yaw = a;
        if self.yaw_min != 0.0 || self.yaw_max != 0.0 {
            if self.yaw > self.yaw_max {
                self.yaw = self.yaw_max;
            }
            if self.yaw < self.yaw_min {
                self.yaw = self.yaw_min;
            }
        }
        self.dirty_view();
    }

    pub fn set_roll(&mut self, a: f32) {
        self.roll = a;
        self.dirty_view();
    }

    pub fn add_pitch(&mut self, a: f32) {
        self.pitch += a;
        self.set_pitch(self.pitch);
    }

    pub fn add_yaw(&mut self, a: f32) {
        self.yaw += a;
        self.set_yaw(self.yaw);
    }

    pub fn add_roll(&mut self, a: f32) {
        self.roll += a;
        self.dirty_view();
    }

    /// `MoveForward` (`axis` 0, moves along the negated forward row), `MoveRight` (1), `MoveUp` (2).
    pub fn move_along(&mut self, axis: i32, dist: f32) {
        self.update_move();
        let d = if axis == 0 { -dist } else { dist };
        let (x, y, z) = match axis {
            0 => (self.mv.m[2][0], self.mv.m[2][1], self.mv.m[2][2]),
            1 => (self.mv.m[0][0], self.mv.m[0][1], self.mv.m[0][2]),
            _ => (self.mv.m[1][0], self.mv.m[1][1], self.mv.m[1][2]),
        };
        self.position.x = fma(x, d, self.position.x);
        self.position.y = fma(y, d, self.position.y);
        self.position.z = fma(z, d, self.position.z);
        self.view_updated = true;
        self.frustum_updated = true;
    }

    pub fn set_fov(&mut self, v: f32) {
        if self.fov != v {
            self.fov = v;
            self.dirty_projection();
        }
    }

    pub fn set_aspect(&mut self, v: f32) {
        if self.aspect != v {
            self.aspect = v;
            self.dirty_projection();
        }
    }

    pub fn set_far(&mut self, v: f32) {
        if self.far != v {
            self.far = v;
            self.dirty_projection();
        }
    }

    pub fn set_near(&mut self, v: f32) {
        if self.near != v {
            self.near = v;
            self.dirty_projection();
        }
    }

    pub fn set_projection_type(&mut self, t: i32) {
        if self.projection_type != t {
            self.projection_type = t;
            self.dirty_projection();
        }
    }

    pub fn set_ortho_view_size(&mut self, s: Vector2f) {
        self.view_size = s;
        if self.projection_type == PROJ_ORTHO {
            self.dirty_projection();
        }
    }

    pub fn set_inf_far(&mut self, v: bool) {
        self.inf_far = v;
        self.projection_updated = true;
    }

    pub fn set_rotate_mode(&mut self, m: i32) {
        self.rotate_mode = m;
        self.dirty_view();
    }

    pub fn set_move_mode(&mut self, m: i32) {
        self.move_mode = m;
        self.move_updated = true;
    }

    pub fn reset_rotation(&mut self) {
        self.view_updated = false;
        self.move_updated = false;
        self.frustum_updated = true;
        self.mv = IDENTITY;
        self.view = IDENTITY;
        self.roll = 0.0;
        self.yaw = 0.0;
        self.pitch = 0.0;
    }

    /// `SetForward` (`row` 2), `SetRight` (0), `SetUp` (1) on the rotation matrix.
    pub fn set_rotation_row(&mut self, row: usize, v: Vector3f) {
        if let Some(r) = self.rotation.m.get_mut(row) {
            r[0] = v.x;
            r[1] = v.y;
            r[2] = v.z;
        }
        self.view_updated = true;
        self.frustum_updated = true;
    }

    pub fn set_rotation_matrix(&mut self, m: &Matrixf) {
        self.rotation = *m;
        self.view_updated = true;
        self.frustum_updated = true;
    }

    pub fn view_matrix(&mut self) -> &Matrixf {
        if self.view_updated {
            self.view = super::camera::view_matrix(&super::camera::ViewParams {
                rotate_mode: self.rotate_mode,
                position: self.position,
                yaw: self.yaw,
                pitch: self.pitch,
                roll: self.roll,
                rotation: self.rotation,
            });
            self.view_updated = false;
        }
        &self.view
    }

    pub fn projection_matrix(&mut self) -> &Matrixf {
        if self.projection_updated {
            self.projection = super::camera::projection_matrix(&super::camera::ProjectionParams {
                kind: self.projection_type,
                near: self.near,
                far: self.far,
                fov: self.fov,
                aspect: self.aspect,
                inf_far: self.inf_far,
                view_size: self.view_size,
            });
            self.projection_updated = false;
        }
        &self.projection
    }

    pub fn update_move(&mut self) {
        if self.move_updated {
            if let Some(m) =
                super::camera::move_matrix(self.rotate_mode, self.move_mode, self.yaw, self.pitch)
            {
                self.mv = m;
            }
            self.move_updated = false;
        }
    }

    /// `GetForward` (`row` 2, negated), `GetRight` (0), `GetUp` (1) of the view matrix.
    pub fn view_row(&mut self, row: usize, negate: bool) -> Vector3f {
        let m = *self.view_matrix();
        let r = m.m.get(row).copied().unwrap_or([0.0; 4]);
        let s = if negate { -1.0 } else { 1.0 };
        Vector3f {
            x: r[0] * s,
            y: r[1] * s,
            z: r[2] * s,
        }
    }

    /// Start of `GetFrustum`: bit 0 when the frustum must be rebuilt, bit 1 when the infinite far
    /// plane was switched off for it (restore with `frustum_end`).
    pub fn frustum_begin(&mut self) -> u32 {
        if !self.frustum_updated {
            return 0;
        }
        if self.inf_far {
            self.set_inf_far(false);
            return 3;
        }
        1
    }

    pub fn frustum_end(&mut self, was_inf: bool) {
        if was_inf {
            self.set_inf_far(true);
        }
        self.frustum_updated = false;
    }
}
