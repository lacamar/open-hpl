//! `iEntity3D`, `cNode3D` and `cCamera` state logic. The C++ classes keep their vtables, lists and
//! callbacks and answer the `Host` queries; transforms, dirty flags and traversal order live here.
//!
//! No reference to C++ state is held across a host call: every access is a short raw-pointer scope,
//! because host calls reach virtual methods that call back into these functions.

use super::node::{add_rotation, ohpl_math_quaternion_mul, update_matrix, world_to_local};
use crate::math::{IDENTITY, fma, matrix_mul as mul_raw, sub3};
use crate::types::{Matrixf, Quaternion, Vector2f, Vector3f};
use std::hint::black_box;

// Opaque operands keep LLVM from folding constant entries, which changes the NaN payload an fma
// propagates.
fn mm(a: &Matrixf, b: &Matrixf) -> Matrixf {
    mul_raw(black_box(a), black_box(b))
}

fn translation_of(m: &Matrixf) -> Vector3f {
    Vector3f {
        x: m.m[0][3],
        y: m.m[1][3],
        z: m.m[2][3],
    }
}

/// Raw views of one `iEntity3D`; null parents are 0.
#[repr(C)]
pub struct EntView {
    pub local: *mut Matrixf,
    pub world: *mut Matrixf,
    pub transform_updated: *mut bool,
    pub update_bv: *mut bool,
    pub apply_bv: *mut bool,
    pub count: *mut i32,
    pub parent_node: usize,
    pub parent_entity: usize,
}

/// State of one `cNode3D`.
#[repr(C)]
pub struct NodeState {
    pub local: Matrixf,
    pub world: Matrixf,
    pub pre: Matrixf,
    pub post: Matrixf,
    pub rotation: Quaternion,
    pub scale: Vector3f,
    pub translation: Vector3f,
    pub parent: usize,
    pub entity_parent: usize,
    pub transform_updated: bool,
    pub use_pre: bool,
    pub use_post: bool,
}

type ListFn = unsafe extern "C" fn(usize, *mut usize, usize) -> usize;

/// C++ answers. Objects are `iEntity3D*` / `cNode3D*` as `usize`; list functions return the item
/// count and fill up to `cap` entries.
#[repr(C)]
pub struct Host {
    pub ent_view: unsafe extern "C" fn(usize, *mut EntView),
    pub ent_children: ListFn,
    pub ent_node_children: ListFn,
    pub ent_on_updated: unsafe extern "C" fn(usize),
    pub ent_callbacks: unsafe extern "C" fn(usize),
    pub node_state: unsafe extern "C" fn(usize) -> *mut NodeState,
    pub node_children: ListFn,
    pub node_entities: ListFn,
}

/// A list snapshot, so the C++ list may change during the traversal that follows.
fn snapshot(f: ListFn, obj: usize) -> Vec<usize> {
    // SAFETY: host contract: `f` fills at most `cap` entries.
    let n = unsafe { f(obj, std::ptr::null_mut(), 0) };
    if n == 0 {
        return Vec::new();
    }
    let mut v = vec![0usize; n];
    // SAFETY: `v` holds `n` entries.
    unsafe { f(obj, v.as_mut_ptr(), n) };
    v
}

fn view(h: &Host, e: usize) -> EntView {
    let mut v = EntView {
        local: std::ptr::null_mut(),
        world: std::ptr::null_mut(),
        transform_updated: std::ptr::null_mut(),
        update_bv: std::ptr::null_mut(),
        apply_bv: std::ptr::null_mut(),
        count: std::ptr::null_mut(),
        parent_node: 0,
        parent_entity: 0,
    };
    // SAFETY: host contract.
    unsafe { (h.ent_view)(e, &mut v) };
    v
}

/// # Safety
/// `h` and `e` valid; the host views point at live C++ members.
pub unsafe fn ent_update_world(h: &Host, e: usize) {
    let v = view(h, e);
    // SAFETY: view pointers are live members of the entity.
    unsafe {
        if !*v.transform_updated {
            return;
        }
        *v.transform_updated = false;
        if v.parent_node != 0 {
            node_update_world(h, v.parent_node);
            let pw = (*(h.node_state)(v.parent_node)).world;
            let local = *v.local;
            *v.world = mm(&pw, &local);
        } else if v.parent_entity != 0 {
            ent_update_world(h, v.parent_entity);
            let pw = *view(h, v.parent_entity).world;
            let local = *v.local;
            *v.world = mm(&pw, &local);
        } else {
            *v.world = *v.local;
        }
    }
}

/// `iEntity3D::SetTransformUpdated`.
///
/// # Safety
/// As `ent_update_world`.
pub unsafe fn ent_set_transform_updated(h: &Host, e: usize, callbacks: bool) {
    let v = view(h, e);
    // SAFETY: view pointers are live members of the entity.
    unsafe {
        *v.transform_updated = true;
        *v.count = (*v.count).wrapping_add(1);
        *v.update_bv = true;
        (h.ent_on_updated)(e);
        for c in snapshot(h.ent_children, e) {
            ent_set_transform_updated(h, c, true);
        }
        for n in snapshot(h.ent_node_children, e) {
            node_set_world_transform_updated(h, n);
        }
        if callbacks {
            (h.ent_callbacks)(e);
        }
    }
}

/// # Safety
/// As `ent_update_world`.
pub unsafe fn ent_set_matrix(h: &Host, e: usize, m: &Matrixf) {
    let v = view(h, e);
    // SAFETY: live member.
    unsafe { *v.local = *m };
    // SAFETY: as above.
    unsafe { ent_set_transform_updated(h, e, true) };
}

/// # Safety
/// As `ent_update_world`.
pub unsafe fn ent_set_position(h: &Host, e: usize, p: Vector3f) {
    let v = view(h, e);
    // SAFETY: live member.
    unsafe {
        (*v.local).m[0][3] = p.x;
        (*v.local).m[1][3] = p.y;
        (*v.local).m[2][3] = p.z;
        ent_set_transform_updated(h, e, true);
    }
}

/// # Safety
/// As `ent_update_world`.
pub unsafe fn ent_set_world_position(h: &Host, e: usize, p: Vector3f) {
    let v = view(h, e);
    if v.parent_entity == 0 {
        // SAFETY: as above.
        unsafe { ent_set_position(h, e, p) };
        return;
    }
    // SAFETY: as above.
    unsafe {
        ent_update_world(h, v.parent_entity);
        let pw = translation_of(&*view(h, v.parent_entity).world);
        ent_set_position(h, e, sub3(p, pw));
    }
}

/// # Safety
/// As `ent_update_world`.
pub unsafe fn ent_set_world_matrix(h: &Host, e: usize, m: &Matrixf) {
    let v = view(h, e);
    if v.parent_entity == 0 {
        // SAFETY: as above.
        unsafe { ent_set_matrix(h, e, m) };
        return;
    }
    // SAFETY: as above.
    unsafe {
        ent_update_world(h, v.parent_entity);
        let pw = *view(h, v.parent_entity).world;
        ent_set_matrix(h, e, &world_to_local(&pw, m));
    }
}

/// `iEntity3D::GetBoundingVolume` flag step: true when the C++ side must apply the world matrix
/// to its bounding volume.
///
/// # Safety
/// As `ent_update_world`.
pub unsafe fn ent_bv_dirty(h: &Host, e: usize) -> bool {
    let v = view(h, e);
    // SAFETY: live members.
    unsafe {
        if *v.apply_bv && *v.update_bv {
            ent_update_world(h, e);
            *v.update_bv = false;
            return true;
        }
    }
    false
}

fn nst(h: &Host, n: usize) -> *mut NodeState {
    // SAFETY: host contract.
    unsafe { (h.node_state)(n) }
}

/// # Safety
/// `h` and `n` valid.
pub unsafe fn node_update_world(h: &Host, n: usize) {
    let s = nst(h, n);
    // SAFETY: `s` is the live state of the node; copies end before each recursion.
    unsafe {
        if !(*s).transform_updated {
            return;
        }
        (*s).transform_updated = false;
        let (parent, eparent) = ((*s).parent, (*s).entity_parent);
        if parent != 0 {
            node_update_world(h, parent);
            let pw = (*nst(h, parent)).world;
            let local = (*s).local;
            (*s).world = mm(&pw, &local);
        } else if eparent != 0 {
            ent_update_world(h, eparent);
            let pw = *view(h, eparent).world;
            let local = (*s).local;
            (*s).world = mm(&pw, &local);
        } else {
            (*s).world = (*s).local;
        }
    }
}

/// `cNode3D::SetWorldTransformUpdated`.
///
/// # Safety
/// As `node_update_world`.
pub unsafe fn node_set_world_transform_updated(h: &Host, n: usize) {
    // SAFETY: live state.
    unsafe { (*nst(h, n)).transform_updated = true };
    for e in snapshot(h.node_entities, n) {
        // SAFETY: as above.
        unsafe { ent_set_transform_updated(h, e, true) };
    }
    for c in snapshot(h.node_children, n) {
        // SAFETY: as above.
        unsafe { node_set_world_transform_updated(h, c) };
    }
}

/// # Safety
/// As `node_update_world`.
pub unsafe fn node_set_matrix(h: &Host, n: usize, m: &Matrixf, children: bool) {
    // SAFETY: live state.
    unsafe {
        (*nst(h, n)).local = *m;
        if children {
            node_set_world_transform_updated(h, n);
        } else {
            (*nst(h, n)).transform_updated = true;
        }
    }
}

/// # Safety
/// As `node_update_world`.
pub unsafe fn node_set_position(h: &Host, n: usize, p: Vector3f) {
    // SAFETY: live state.
    unsafe {
        let s = nst(h, n);
        (*s).local.m[0][3] = p.x;
        (*s).local.m[1][3] = p.y;
        (*s).local.m[2][3] = p.z;
        node_set_world_transform_updated(h, n);
    }
}

/// # Safety
/// As `node_update_world`.
pub unsafe fn node_set_world_position(h: &Host, n: usize, p: Vector3f) {
    // SAFETY: live state.
    unsafe {
        let parent = (*nst(h, n)).parent;
        if parent == 0 {
            node_set_position(h, n, p);
            return;
        }
        node_update_world(h, parent);
        let pw = translation_of(&(*nst(h, parent)).world);
        node_set_position(h, n, sub3(p, pw));
    }
}

/// # Safety
/// As `node_update_world`.
pub unsafe fn node_set_world_matrix(h: &Host, n: usize, m: &Matrixf) {
    // SAFETY: live state.
    unsafe {
        let parent = (*nst(h, n)).parent;
        if parent == 0 {
            node_set_matrix(h, n, m, true);
            return;
        }
        node_update_world(h, parent);
        let pw = (*nst(h, parent)).world;
        node_set_matrix(h, n, &world_to_local(&pw, m), true);
    }
}

/// # Safety
/// `s` valid.
pub unsafe fn node_add_rotation_euler(s: *mut NodeState, euler: Vector3f, order: i32) {
    // SAFETY: caller contract.
    unsafe { (*s).rotation = add_rotation(euler, order, &(*s).rotation) };
}

/// # Safety
/// `s` valid.
pub unsafe fn node_add_rotation(s: *mut NodeState, q: &Quaternion) {
    let mut out = Quaternion::default();
    // SAFETY: caller contract.
    unsafe {
        let cur = (*s).rotation;
        ohpl_math_quaternion_mul(q, &cur, &mut out);
        (*s).rotation = out;
    }
}

const ONE: Vector3f = Vector3f {
    x: 1.0,
    y: 1.0,
    z: 1.0,
};

/// # Safety
/// `s` valid.
pub unsafe fn node_add_scale(s: *mut NodeState, v: Vector3f) {
    // SAFETY: caller contract.
    unsafe {
        (*s).scale.x *= v.x;
        (*s).scale.y *= v.y;
        (*s).scale.z *= v.z;
    }
}

/// # Safety
/// `s` valid.
pub unsafe fn node_add_translation(s: *mut NodeState, v: Vector3f) {
    // SAFETY: caller contract.
    unsafe {
        (*s).translation.x += v.x;
        (*s).translation.y += v.y;
        (*s).translation.z += v.z;
    }
}

const IDENTITY_Q: Quaternion = Quaternion {
    v: Vector3f {
        x: 0.0,
        y: 0.0,
        z: 0.0,
    },
    w: 1.0,
};

/// `cNode3D::UpdateMatrix`.
///
/// # Safety
/// As `node_update_world`.
pub unsafe fn node_update_matrix(h: &Host, n: usize, children: bool) {
    // SAFETY: live state.
    unsafe {
        let s = nst(h, n);
        let m = update_matrix(&(*s).local, &(*s).rotation, (*s).translation);
        node_set_matrix(h, n, &m, children);
        let s = nst(h, n);
        (*s).rotation = IDENTITY_Q;
        (*s).scale = ONE;
        (*s).translation = Vector3f::default();
    }
}

/// `cNode3D::ApplyPreAnimTransform` / `ApplyPostAnimTransform`.
///
/// # Safety
/// As `node_update_world`.
pub unsafe fn node_apply_anim(h: &Host, n: usize, post: bool, children: bool) {
    // SAFETY: live state.
    unsafe {
        let s = nst(h, n);
        let (on, local, t) = if post {
            ((*s).use_post, (*s).local, (*s).post)
        } else {
            ((*s).use_pre, (*s).local, (*s).pre)
        };
        if !on {
            return;
        }
        let m = if post { mm(&t, &local) } else { mm(&local, &t) };
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
