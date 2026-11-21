//! `cMeshEntity`: sub entities, animation states, bone and node states, name tables, bone
//! matrices and the animation/physics/graphics control flow. The C++ entity is a thin shell;
//! only the objects whose classes are still C++ (`cNode3D`/`cBoneState`, `cSubMeshEntity`,
//! `cAnimationState`, physics bodies, the world) are reached through the `Fx` thunk table, one
//! typed call per effect. No reference into `MeshEnt` or `MeshCtl` is held across a thunk call:
//! the thunks may re-enter the entity (animation callbacks, destructors).

use std::collections::BTreeMap;
use std::ptr;

use super::animstate::AnimCore;
use super::h11::{self, Ctl, MeshCtl, MeshHost};
use super::skinning;
use super::wm2_sub::Sub;
use crate::graphics::mm::{Mesh, P};
use crate::graphics::skel::Skeleton;
use crate::math::{IDENTITY, matrix_inverse, matrix_mul, matrix_slerp};
use crate::types::{Matrixf, Vector3f};

#[cfg(test)]
pub trait Nil {
    fn nil() -> Self;
}

#[cfg(test)]
macro_rules! nil_impl {
    ($($t:ty => $v:expr),*) => {$(impl Nil for $t { fn nil() -> Self { $v } })*};
}

#[cfg(test)]
nil_impl!(bool => false, usize => 0, i32 => 0, f32 => 0.0, P => ptr::null_mut(), *const Matrixf => ptr::null(),
    *const u8 => ptr::null(), *mut AnimCore => ptr::null_mut());

macro_rules! fx_table {
    ($($name:ident($($t:ty),*) $(-> $r:ty)?;)*) => {
        /// `OhplMeFx` (`RustMeshEntity.h`); `ctx` arguments are the `cMeshEntity`.
        #[repr(C)]
        pub struct Fx {
            $(pub $name: unsafe extern "C" fn($($t),*) $(-> $r)?,)*
        }

        impl Fx {
            #[cfg(test)]
            pub fn noop() -> Self {
                Self {$($name: {
                    unsafe extern "C" fn f($(_: $t),*) $(-> $r)? { $(<$r as Nil>::nil())? }
                    f
                },)*}
            }
        }
    };
}

fx_table! {
    name(i32, P, *mut usize) -> *const u8;
    n_active(P) -> bool;
    n_update_matrix(P);
    n_anim_xform(P);
    n_world(P) -> *const Matrixf;
    n_update_world(P);
    n_set_matrix(P, *const Matrixf, bool);
    n_local(P) -> *const Matrixf;
    n_parent(P) -> P;
    n_child_count(P) -> usize;
    n_child_at(P, usize) -> P;
    n_set_parent(P, P);
    n_add_child(P, P);
    n_ent_children(P);
    new_state(P, *const u8, usize, *const Matrixf) -> P;
    new_root(P) -> P;
    delete_node(P);
    bs_body(P) -> P;
    bs_collider(P) -> P;
    bs_inv_body(P) -> *const Matrixf;
    bs_body_matrix(P) -> *const Matrixf;
    body_world(P) -> *const Matrixf;
    body_set_active(P, bool);
    body_enable(P);
    body_zero_vel(P);
    body_set_matrix(P, *const Matrixf);
    collide(P, usize) -> bool;
    push_pos(usize);
    push_num(usize, i32);
    ent_world(P) -> *const Matrixf;
    ent_parent_world(P) -> *const Matrixf;
    ent_set_matrix(P, *const Matrixf);
    ent_transform_updated(P);
    ent_count(P) -> i32;
    ent_add_node_child(P, P);
    ent_node_child_count(P) -> usize;
    ent_node_child_at(P, usize) -> P;
    ent_add_child(P, P);
    ent_remove_child(P, P);
    bv_flag(P) -> bool;
    bv_clear(P);
    bv_from_vertices(P);
    bv_set_local(P, *const Vector3f, *const Vector3f);
    bv_set_identity(P);
    new_sub(P, i32, *mut *mut Sub) -> P;
    delete_sub(P, P);
    sub_set_matrix(P, *const Matrixf);
    sub_visible(P, bool);
    sub_static(P, bool);
    sub_illum(P, f32);
    sub_coverage(P, f32);
    sub_liquid(P, f32);
    sub_auto_liquid(P);
    sub_render_flag(P, i32, bool);
    sub_color_mul(P, P);
    sub_illum_color(P, P);
    sub_bv(P, *mut Vector3f, *mut Vector3f);
    sub_update_logic(P, f32);
    sub_transform_updated(P);
    sub_set_local_node(P, P);
    new_anim_state(P, P) -> P;
    add_anim_state(P, P, *const u8, usize) -> P;
    anim_core(P) -> *mut AnimCore;
    delete_state(P);
    state_anim(P) -> P;
    anim_track_count(P) -> i32;
    anim_track(P, i32) -> P;
    track_name(P, *mut usize) -> *const u8;
    track_unit(P) -> f32;
    track_apply(P, P, f32, f32, bool, f32);
    ev_count(P) -> i32;
    ev_at(P, i32) -> P;
    ev_time(P) -> f32;
    ev_is_sound(P) -> bool;
    ev_value_empty(P) -> bool;
    ev_callback(P, P) -> bool;
    ev_play_sound(P, P);
    after_anim(P, f32);
    mesh_destroy(P);
    report(P, i32, *const u8, usize);
}

#[derive(Default)]
pub struct Names {
    maps: [BTreeMap<Vec<u8>, i32>; 4],
}

const ANIM: usize = 0;
const BONE: usize = 1;
const NODE: usize = 2;
const SUB: usize = 3;

impl Names {
    fn insert(&mut self, kind: usize, name: &[u8], idx: i32) {
        if let Some(m) = self.maps.get_mut(kind) {
            m.entry(name.to_vec()).or_insert(idx);
        }
    }

    fn find(&self, kind: usize, name: &[u8]) -> i32 {
        self.maps
            .get(kind)
            .and_then(|m| m.get(name))
            .copied()
            .unwrap_or(-1)
    }
}

pub struct MeshEnt {
    ctx: P,
    fx: &'static Fx,
    ctl: Ctl,
    mesh: *const Mesh,
    skel: *mut Skeleton,
    file_units: *const bool,
    names: Names,
    subs: Vec<P>,
    pods: Vec<*mut Sub>,
    states: Vec<P>,
    cores: Vec<*mut AnimCore>,
    track_idx: Vec<Vec<i32>>,
    bones: Vec<P>,
    temp: Vec<P>,
    nodes: Vec<P>,
    root: P,
    rest: Vec<Matrixf>,
    mats: Vec<Matrixf>,
    inv_world: Matrixf,
}

/// Raw handle to the entity; every access forms a short-lived reference.
#[derive(Clone, Copy)]
pub struct Me(pub *mut MeshEnt);

fn mat(p: *const Matrixf) -> Matrixf {
    // SAFETY: the thunks return null or a matrix live until the next thunk call; copied at once.
    unsafe { p.as_ref() }.copied().unwrap_or(IDENTITY)
}

fn text(p: *const u8, n: usize) -> Vec<u8> {
    if p.is_null() {
        return Vec::new();
    }
    // SAFETY: the thunk returns `n` readable bytes; copied at once.
    unsafe { std::slice::from_raw_parts(p, n) }.to_vec()
}

impl Me {
    fn m<T>(self, f: impl FnOnce(&mut MeshEnt) -> T) -> T {
        // SAFETY: the handle is the live entity; the reference ends with `f`, which never calls
        // a thunk (callers copy what they need first).
        f(unsafe { &mut *self.0 })
    }

    fn fx(self) -> (&'static Fx, P) {
        self.m(|m| (m.fx, m.ctx))
    }

    fn ctl(self) -> Ctl {
        self.m(|m| m.ctl)
    }

    fn mesh(&self) -> &Mesh {
        // SAFETY: the mesh outlives the entity (the destructor releases it last).
        unsafe { &*self.m(|m| m.mesh) }
    }

    fn has_skel(self) -> bool {
        self.m(|m| !m.skel.is_null())
    }

    fn with_skel<T>(self, f: impl FnOnce(&mut Skeleton) -> T) -> Option<T> {
        // SAFETY: the skeleton is owned by the mesh, which outlives the entity; the reference
        // ends with `f`, which calls no thunk.
        unsafe { self.m(|m| m.skel).as_mut() }.map(f)
    }

    fn name_of(self, kind: i32, obj: P) -> Vec<u8> {
        let (fx, _) = self.fx();
        let mut n = 0;
        // SAFETY: thunk contract.
        let p = unsafe { (fx.name)(kind, obj, &mut n) };
        text(p, n)
    }

    fn bones(self) -> Vec<P> {
        self.m(|m| m.bones.clone())
    }

    fn bone_name(self, i: usize) -> Vec<u8> {
        self.with_skel(|s| Some(s.name(s.by_index(i as i32)?).to_vec()))
            .flatten()
            .unwrap_or_default()
    }

    fn bone_local(self, i: usize) -> Matrixf {
        self.with_skel(|s| Some(*s.local(s.by_index(i as i32)?)?))
            .flatten()
            .unwrap_or(IDENTITY)
    }

    fn world(self, n: P) -> Matrixf {
        let (fx, _) = self.fx();
        // SAFETY: thunk contract.
        mat(unsafe { (fx.n_world)(n) })
    }

    fn local(self, n: P) -> Matrixf {
        let (fx, _) = self.fx();
        // SAFETY: thunk contract.
        mat(unsafe { (fx.n_local)(n) })
    }

    fn set_matrix(self, n: P, m: &Matrixf, children: bool) {
        let (fx, _) = self.fx();
        // SAFETY: thunk contract.
        unsafe { (fx.n_set_matrix)(n, m, children) }
    }

    fn children(self, n: P) -> Vec<P> {
        let (fx, _) = self.fx();
        // SAFETY: thunk contract.
        let count = unsafe { (fx.n_child_count)(n) };
        (0..count)
            // SAFETY: as above.
            .map(|i| unsafe { (fx.n_child_at)(n, i) })
            .collect()
    }

    fn state_bone(self, i: i32) -> P {
        self.m(|m| {
            usize::try_from(i)
                .ok()
                .and_then(|i| m.bones.get(i))
                .copied()
        })
        .unwrap_or(ptr::null_mut())
    }

    fn state_node(self, i: i32) -> P {
        self.m(|m| {
            usize::try_from(i)
                .ok()
                .and_then(|i| m.nodes.get(i))
                .copied()
        })
        .unwrap_or(ptr::null_mut())
    }

    fn report(self, kind: i32, name: &[u8]) {
        let (fx, ctx) = self.fx();
        // SAFETY: thunk contract.
        unsafe { (fx.report)(ctx, kind, name.as_ptr(), name.len()) }
    }

    /// `SetBoneMatrixFromBodyRec`.
    fn bone_from_body(self, parent_world: &Matrixf, node: P) {
        let (fx, _) = self.fx();
        // SAFETY (all thunk calls below): thunk contract.
        let body = unsafe { (fx.bs_body)(node) };
        let world = if body.is_null() {
            unsafe { (fx.n_update_world)(node) };
            self.world(node)
        } else {
            let bone_world = matrix_mul(
                &mat(unsafe { (fx.body_world)(body) }),
                &mat(unsafe { (fx.bs_inv_body)(node) }),
            );
            let local = matrix_mul(&matrix_inverse(parent_world), &bone_world);
            self.set_matrix(node, &local, false);
            bone_world
        };
        for child in self.children(node) {
            self.bone_from_body(&world, child);
        }
    }

    /// `UpdateNodeMatrixRec`.
    fn update_node(self, node: P) {
        let (fx, _) = self.fx();
        // SAFETY: thunk contract.
        unsafe {
            if (fx.n_active)(node) {
                (fx.n_update_matrix)(node);
            }
            (fx.n_anim_xform)(node);
        }
        for child in self.children(node) {
            self.update_node(child);
        }
    }

    fn entity_children(self) -> Vec<P> {
        let (fx, ctx) = self.fx();
        // SAFETY: thunk contract.
        let count = unsafe { (fx.ent_node_child_count)(ctx) };
        // SAFETY: as above.
        (0..count)
            .map(|i| unsafe { (fx.ent_node_child_at)(ctx, i) })
            .collect()
    }

    fn reset_gfx(self) {
        self.m(|m| {
            for &pod in &m.pods {
                // SAFETY: the pod is the live box of a sub entity this entity owns.
                unsafe { (*pod).graphics_updated = false };
            }
        });
        self.ctl().set_updated_bones(false);
    }

    fn physics_active(self, active: bool) {
        let (fx, _) = self.fx();
        let c = self.ctl();
        h11::skeleton_physics_set(c, active);
        self.reset_gfx();
        for state in self.bones() {
            // SAFETY: thunk contract.
            let (body, collider) = unsafe { ((fx.bs_body)(state), (fx.bs_collider)(state)) };
            if body.is_null() {
                continue;
            }
            // SAFETY: as above.
            unsafe {
                (fx.body_set_active)(body, active);
                (fx.body_enable)(body);
                if !active {
                    (fx.body_zero_vel)(body);
                }
                if c.colliders() && !collider.is_null() {
                    (fx.body_set_active)(collider, !active);
                }
            }
        }
    }

    fn physics_fade(self, time: f32) {
        let (fx, _) = self.fx();
        if !h11::skeleton_physics_fade(self.ctl(), time) {
            return;
        }
        for state in self.bones() {
            // SAFETY: thunk contract.
            unsafe {
                let body = (fx.bs_body)(state);
                if !body.is_null() {
                    (fx.body_set_active)(body, false);
                }
            }
        }
    }

    fn colliders_active(self, active: bool) {
        let (fx, _) = self.fx();
        let c = self.ctl();
        c.set_colliders(active);
        for state in self.bones() {
            // SAFETY: thunk contract.
            let collider = unsafe { (fx.bs_collider)(state) };
            if collider.is_null() {
                continue;
            }
            if active && !c.physics() {
                // SAFETY: as above.
                unsafe { (fx.body_set_active)(collider, true) };
            } else if !active {
                // SAFETY: as above.
                unsafe { (fx.body_set_active)(collider, false) };
            }
        }
    }

    /// `AlignBodiesToSkeleton`.
    fn align_bodies(self) {
        let (fx, ctx) = self.fx();
        let bones = self.bones();
        let mats: Vec<Matrixf> = bones
            .iter()
            .map(|&state| {
                // SAFETY: thunk contract.
                if unsafe { (fx.bs_body)(state) }.is_null() {
                    IDENTITY
                } else {
                    matrix_mul(
                        &self.world(state),
                        &mat(unsafe { (fx.bs_body_matrix)(state) }),
                    )
                }
            })
            .collect();
        // SAFETY: as above.
        let entity_world = mat(unsafe { (fx.ent_world)(ctx) });
        let mut done = std::collections::BTreeSet::new();
        for (state, m) in bones.iter().zip(&mats) {
            // SAFETY: as above.
            let body = unsafe { (fx.bs_body)(*state) };
            if !body.is_null() && done.insert(body as usize) {
                // SAFETY: as above.
                unsafe { (fx.body_set_matrix)(body, m) };
            }
        }
        // SAFETY: as above.
        let parent = unsafe { (fx.ent_parent_world)(ctx) };
        // SAFETY: null or a matrix live until the next thunk call; copied here.
        if let Some(parent) = unsafe { parent.as_ref() }.copied() {
            let local = matrix_mul(&matrix_inverse(&parent), &entity_world);
            // SAFETY: as above.
            unsafe { (fx.ent_set_matrix)(ctx, &local) };
        }
    }

    /// `CheckColliderShapeCollision`; `args` is the C++ query record the collide thunks read.
    fn check_colliders(self, args: usize, has_pos: bool, has_num: bool) -> bool {
        let (fx, _) = self.fx();
        let mut hit = false;
        for (i, state) in self.bones().into_iter().enumerate() {
            // SAFETY: thunk contract (all calls in this loop).
            let body = unsafe { (fx.bs_collider)(state) };
            if body.is_null() {
                continue;
            }
            let m = matrix_mul(
                &self.world(state),
                &mat(unsafe { (fx.bs_body_matrix)(state) }),
            );
            unsafe { (fx.body_set_matrix)(body, &m) };
            if !unsafe { (fx.collide)(body, args) } {
                continue;
            }
            hit = true;
            if !has_pos && !has_num {
                break;
            }
            if has_pos {
                unsafe { (fx.push_pos)(args) };
            }
            if has_num {
                unsafe { (fx.push_num)(args, i as i32) };
            }
        }
        hit
    }

    /// `GetBoundingVolume` after the skeleton and sub-mesh cases split.
    fn bounding_volume(self) {
        let (fx, ctx) = self.fx();
        // SAFETY: thunk contract (all calls below).
        if !unsafe { (fx.bv_flag)(ctx) } {
            return;
        }
        if self.has_skel() {
            self.bv_from_skeleton();
            unsafe { (fx.bv_clear)(ctx) };
            return;
        }
        unsafe { (fx.bv_clear)(ctx) };
        let (mut mins, mut maxs) = (Vec::new(), Vec::new());
        for sub in self.m(|m| m.subs.clone()) {
            let (mut lo, mut hi) = (Vector3f::default(), Vector3f::default());
            unsafe { (fx.sub_bv)(sub, &mut lo, &mut hi) };
            mins.push(lo);
            maxs.push(hi);
        }
        if let Some((lo, hi)) = h11::bounds_union(&mins, &maxs) {
            unsafe { (fx.bv_set_local)(ctx, &lo, &hi) };
        }
    }

    /// `UpdateBVFromSkeleton`.
    fn bv_from_skeleton(self) {
        let (fx, ctx) = self.fx();
        let bones = self.bones();
        if bones.is_empty() {
            // SAFETY: thunk contract.
            unsafe { (fx.bv_from_vertices)(ctx) };
            return;
        }
        let mats: Vec<Matrixf> = bones.iter().map(|&b| self.world(b)).collect();
        let radii: Vec<f32> = (0..bones.len()).map(|i| self.mesh().radius(i)).collect();
        let refs: Vec<&Matrixf> = mats.iter().collect();
        let (lo, hi) = skinning::aabb_from_bones(&refs, &radii);
        // SAFETY: as above.
        unsafe {
            (fx.bv_set_identity)(ctx);
            (fx.bv_set_local)(ctx, &lo, &hi);
        }
    }

    /// `HandleAnimationEvent`.
    fn animation_event(self, ev: P) {
        let (fx, ctx) = self.fx();
        // SAFETY: thunk contract.
        unsafe {
            if (fx.ev_callback)(ctx, ev) || (fx.ev_value_empty)(ev) {
                return;
            }
            if (fx.ev_is_sound)(ev) {
                (fx.ev_play_sound)(ctx, ev);
            }
        }
    }

    fn fan_out(self, f: impl Fn(&'static Fx, P)) {
        let (fx, _) = self.fx();
        for sub in self.m(|m| m.subs.clone()) {
            f(fx, sub);
        }
    }

    fn core_play_args(self) -> Vec<*mut AnimCore> {
        self.m(|m| m.cores.clone())
    }

    /// `PlayName` / `PlayFadeToName` / `AnimationIsOver`: look the state up, warn when missing.
    fn state_index(self, name: &[u8], mode: i32) -> Option<usize> {
        let idx = self.m(|m| m.names.find(ANIM, name));
        match usize::try_from(idx) {
            Ok(i) => Some(i),
            Err(_) => {
                self.report(mode, name);
                None
            }
        }
    }

    /// `cMeshEntity::CreateNodes`.
    fn create_nodes(self) {
        let (fx, ctx) = self.fx();
        let mesh_nodes = self.mesh().node_count();
        if self.m(|m| !m.nodes.is_empty()) || self.has_skel() || mesh_nodes == 0 {
            return;
        }
        for sub in self.m(|m| m.subs.clone()) {
            // SAFETY: thunk contract.
            unsafe { (fx.ent_remove_child)(ctx, sub) };
            unsafe { (fx.sub_set_matrix)(sub, &IDENTITY) };
        }
        for i in 0..mesh_nodes {
            let mesh_node = self.mesh().node(i);
            let name = self.name_of(2, mesh_node);
            let init = self.local(mesh_node);
            // SAFETY: as above.
            let state = unsafe { (fx.new_state)(ctx, name.as_ptr(), name.len(), &init) };
            let sub = self.m(|m| {
                m.nodes.push(state);
                m.names.insert(NODE, &name, i as i32);
                let idx = m.names.find(SUB, &name);
                usize::try_from(idx)
                    .ok()
                    .and_then(|s| m.subs.get(s).copied())
            });
            if let Some(sub) = sub {
                unsafe { (fx.sub_set_local_node)(sub, state) };
            }
        }
        for i in 0..mesh_nodes {
            let mesh_node = self.mesh().node(i);
            let node = self.state_node(i as i32);
            // SAFETY: as above.
            let parent = unsafe { (fx.n_parent)(mesh_node) };
            let parent_idx = if parent.is_null() {
                -1
            } else {
                let name = self.name_of(2, parent);
                self.m(|m| m.names.find(NODE, &name))
            };
            if parent_idx >= 0 {
                unsafe { (fx.n_set_parent)(node, self.state_node(parent_idx)) };
            } else {
                unsafe { (fx.ent_add_node_child)(ctx, node) };
            }
            for child in self.children(mesh_node) {
                let name = self.name_of(2, child);
                let idx = self.m(|m| m.names.find(NODE, &name));
                // the original adds a null child (crash) for an unknown name
                if idx >= 0 {
                    unsafe { (fx.n_add_child)(node, self.state_node(idx)) };
                }
            }
        }
        unsafe { (fx.ent_transform_updated)(ctx) };
    }

    /// The constructor of `cMeshEntity`: sub entities, animation states, skeleton or node states.
    fn build(self) {
        let (fx, ctx) = self.fx();
        for i in 0..self.mesh().sub_count() {
            let mut pod = ptr::null_mut();
            let mesh_sub = self.mesh().sub(i);
            // SAFETY: thunk contract.
            let sub = unsafe { (fx.new_sub)(ctx, i as i32, &mut pod) };
            let name = self.name_of(0, mesh_sub);
            self.m(|m| {
                m.subs.push(sub);
                m.pods.push(pod);
                m.names.insert(SUB, &name, i as i32);
            });
        }
        for i in 0..self.mesh().animation_count() {
            let anim = self.mesh().animation(i);
            // SAFETY: as above.
            let state = unsafe { (fx.new_anim_state)(ctx, anim) };
            let core = unsafe { (fx.anim_core)(state) };
            let name = self.name_of(1, anim);
            self.m(|m| {
                m.states.push(state);
                m.cores.push(core);
                m.track_idx.push(Vec::new());
                m.names.insert(ANIM, &name, i as i32);
            });
        }
        if let Some(parents) = self.with_skel(|skel| {
            (0..skel.num())
                .map(|i| {
                    let id = skel.by_index(i)?;
                    Some(skel.name(skel.parent(id)?).to_vec())
                })
                .collect::<Vec<_>>()
        }) {
            let root = unsafe { (fx.new_root)(ctx) };
            self.m(|m| m.root = root);
            let n = parents.len();
            for i in 0..n {
                let name = self.bone_name(i);
                let init = self.bone_local(i);
                let state = unsafe { (fx.new_state)(ctx, name.as_ptr(), name.len(), &init) };
                self.m(|m| {
                    m.bones.push(state);
                    m.names.insert(BONE, &name, i as i32);
                });
            }
            for (i, parent) in parents.iter().enumerate() {
                let parent = parent
                    .as_ref()
                    .map_or(-1, |p| self.m(|m| m.names.find(BONE, p)));
                let target = if parent < 0 {
                    root
                } else {
                    self.state_bone(parent)
                };
                unsafe { (fx.n_set_parent)(self.state_bone(i as i32), target) };
            }
            self.m(|m| m.mats = vec![Matrixf::default(); n]);
            for i in 0..n {
                self.set_matrix(self.state_bone(i as i32), &self.bone_local(i), true);
            }
            for i in 0..n {
                let name = self.bone_name(i);
                let state = unsafe { (fx.new_state)(ctx, name.as_ptr(), name.len(), ptr::null()) };
                self.m(|m| m.temp.push(state));
            }
            for sub in self.m(|m| m.subs.clone()) {
                unsafe { (fx.ent_add_child)(ctx, sub) };
            }
        } else if self.m(|m| !m.states.is_empty()) {
            self.create_nodes();
        } else {
            for (i, sub) in self.m(|m| m.subs.clone()).into_iter().enumerate() {
                if self.mesh().node_count() > 0 {
                    self.sub_from_mesh_node(i, sub);
                }
                unsafe { (fx.ent_add_child)(ctx, sub) };
            }
        }
    }

    fn sub_from_mesh_node(self, i: usize, sub: P) {
        let (fx, _) = self.fx();
        let name = self.name_of(0, self.mesh().sub(i));
        let node = self.mesh().node_by_name(&name);
        if node.is_null() {
            self.report(3, &name);
        } else {
            let world = self.world(node);
            // SAFETY: thunk contract.
            unsafe { (fx.sub_set_matrix)(sub, &world) };
        }
    }

    /// The destructor of `cMeshEntity`.
    fn destroy(self) {
        let (fx, ctx) = self.fx();
        let (subs, root) = self.m(|m| (std::mem::take(&mut m.subs), m.root));
        // SAFETY: thunk contract (all calls below).
        for sub in subs {
            unsafe { (fx.delete_sub)(ctx, sub) };
        }
        if !root.is_null() {
            unsafe { (fx.delete_node)(root) };
        }
        unsafe { (fx.mesh_destroy)(ctx) };
        for nodes in [
            self.m(|m| std::mem::take(&mut m.nodes)),
            self.m(|m| std::mem::take(&mut m.bones)),
            self.m(|m| std::mem::take(&mut m.temp)),
        ] {
            for node in nodes {
                unsafe { (fx.delete_node)(node) };
            }
        }
        self.m(|m| m.cores.clear());
        for state in self.m(|m| std::mem::take(&mut m.states)) {
            unsafe { (fx.delete_state)(state) };
        }
    }

    /// Animation, core and per-track target index (bone or node state) of animation state
    /// `state`; the indices are cached per entity because tracks are shared between meshes.
    fn track_nodes(self, state: usize, bones: bool) -> Option<(P, *mut AnimCore, Vec<i32>)> {
        let (fx, _) = self.fx();
        let (st, core) = self.m(|m| Some((*m.states.get(state)?, *m.cores.get(state)?)))?;
        // SAFETY: thunk contract (all calls below).
        let anim = unsafe { (fx.state_anim)(st) };
        let tracks = usize::try_from(unsafe { (fx.anim_track_count)(anim) }).unwrap_or(0);
        let cached = self.m(|m| {
            m.track_idx
                .get(state)
                .filter(|v| v.len() == tracks)
                .cloned()
        });
        let idx = cached.unwrap_or_else(|| {
            let idx: Vec<i32> = (0..tracks)
                .map(|i| {
                    let mut n = 0;
                    let name = text(
                        unsafe { (fx.track_name)((fx.anim_track)(anim, i as i32), &mut n) },
                        n,
                    );
                    if bones {
                        self.with_skel(|s| s.index_by_name(&name)).unwrap_or(-1)
                    } else {
                        self.m(|m| m.names.find(NODE, &name))
                    }
                })
                .collect();
            self.m(|m| {
                if let Some(slot) = m.track_idx.get_mut(state) {
                    slot.clone_from(&idx);
                }
            });
            idx
        });
        Some((anim, core, idx))
    }

    fn state_at(self, state: usize) -> P {
        self.m(|m| m.states.get(state).copied())
            .unwrap_or(ptr::null_mut())
    }

    fn update_graphics(self) {
        let (fx, ctx) = self.fx();
        let c = self.ctl();
        // SAFETY: thunk contract (all calls below).
        let count = unsafe { (fx.ent_count)(ctx) };
        let gate = h11::graphics_gate(c, self.has_skel(), count);
        if gate == 0 {
            return;
        }
        if !self.has_skel() {
            return;
        }
        if gate & 2 != 0 {
            let inv = matrix_inverse(&mat(unsafe { (fx.ent_world)(ctx) }));
            self.m(|m| m.inv_world = inv);
        }
        let world: Vec<Matrixf> = self.bones().iter().map(|&b| self.world(b)).collect();
        let inv_bind: Vec<Matrixf> = self
            .with_skel(|skel| {
                (0..skel.num())
                    .map(|i| {
                        skel.by_index(i)
                            .and_then(|id| skel.inv_bind(id).copied())
                            .unwrap_or_default()
                    })
                    .collect()
            })
            .unwrap_or_default();
        let changed = self.m(|m| {
            let w: Vec<&Matrixf> = world.iter().collect();
            let ib: Vec<&Matrixf> = inv_bind.iter().collect();
            skinning::bone_matrices(&m.inv_world, &w, &ib, &mut m.mats)
        });
        c.add_version(changed as i32);
    }
}

impl MeshHost for Me {
    fn cores(&mut self) -> &[*mut AnimCore] {
        // SAFETY: the vector lives as long as the entity; the borrow of `self` prevents a thunk
        // call while the slice is used (`h11` contract), and the cores never change meanwhile.
        unsafe { &*ptr::addr_of!((*self.0).cores) }
    }

    fn state_count(&mut self) -> usize {
        self.m(|m| m.states.len())
    }

    fn bone_count(&mut self) -> usize {
        self.m(|m| m.bones.len())
    }

    fn has_skeleton(&mut self) -> bool {
        self.has_skel()
    }

    fn has_rest_pose(&mut self) -> bool {
        self.m(|m| !m.rest.is_empty())
    }

    fn bone_reset(&mut self, bone: usize, map_pose: bool, reset_temp: bool) {
        let (fx, _) = self.fx();
        let m = if map_pose {
            self.m(|m| m.rest.get(bone).copied()).unwrap_or(IDENTITY)
        } else {
            self.bone_local(bone)
        };
        let state = self.state_bone(bone as i32);
        // SAFETY: thunk contract.
        if unsafe { (fx.n_active)(state) } {
            self.set_matrix(state, &m, false);
        }
        if reset_temp {
            let temp = self
                .m(|m| m.temp.get(bone).copied())
                .unwrap_or(ptr::null_mut());
            self.set_matrix(temp, &m, false);
        }
    }

    fn physics_from_bodies(&mut self) {
        let root = self.m(|m| m.root);
        let world = self.world(root);
        for child in self.children(root) {
            self.bone_from_body(&world, child);
        }
    }

    fn physics_mix(&mut self, bone: usize, weight: f32) {
        let temp = self
            .m(|m| m.temp.get(bone).copied())
            .unwrap_or(ptr::null_mut());
        let state = self.state_bone(bone as i32);
        let mixed = matrix_slerp(weight, &self.local(temp), &self.local(state));
        self.set_matrix(state, &mixed, false);
    }

    fn physics_deactivate(&mut self) {
        self.physics_active(false);
    }

    fn skeleton_apply(&mut self, state: usize, weight: f32) {
        let Some((anim, core, idx)) = self.track_nodes(state, true) else {
            return;
        };
        let (fx, _) = self.fx();
        // SAFETY: valid until the entity is destroyed.
        let file_units = unsafe { self.m(|m| m.file_units).as_ref() }
            .copied()
            .unwrap_or(false);
        for (i, &bone) in idx.iter().enumerate() {
            let node = self.state_bone(bone);
            // SAFETY: thunk contract (all calls below).
            if node.is_null() || !unsafe { (fx.n_active)(node) } {
                continue;
            }
            let track = unsafe { (fx.anim_track)(anim, i as i32) };
            let scale = if file_units {
                1.0
            } else {
                unsafe { (fx.track_unit)(track) }
            };
            // SAFETY: the core is valid while the state lives; read at once.
            let c = unsafe { *core };
            unsafe { (fx.track_apply)(track, node, c.time_pos, weight, c.looping, scale) };
        }
    }

    fn node_apply(&mut self, state: usize, weight: f32) {
        let Some((anim, core, idx)) = self.track_nodes(state, false) else {
            return;
        };
        let (fx, _) = self.fx();
        for (i, &at) in idx.iter().enumerate() {
            if at < 0 {
                continue;
            }
            let node = self.state_node(at);
            // SAFETY: thunk contract (all calls below).
            if unsafe { (fx.n_active)(node) } {
                let track = unsafe { (fx.anim_track)(anim, i as i32) };
                // SAFETY: as in `skeleton_apply`.
                let c = unsafe { *core };
                let scale = unsafe { (fx.track_unit)(track) };
                unsafe { (fx.track_apply)(track, node, c.time_pos, weight, true, scale) };
            }
        }
    }

    fn bone_nodes_update(&mut self) {
        let root = self.m(|m| m.root);
        for child in self.children(root) {
            self.update_node(child);
        }
    }

    fn bone_entity_children(&mut self) {
        let (fx, _) = self.fx();
        for state in self.bones() {
            // SAFETY: thunk contract.
            unsafe { (fx.n_ent_children)(state) };
        }
    }

    fn colliders_update(&mut self) {
        let (fx, _) = self.fx();
        for state in self.bones() {
            // SAFETY: thunk contract (all calls below).
            let collider = unsafe { (fx.bs_collider)(state) };
            if !collider.is_null() {
                let m = matrix_mul(
                    &self.world(state),
                    &mat(unsafe { (fx.bs_body_matrix)(state) }),
                );
                unsafe { (fx.body_set_matrix)(collider, &m) };
            }
        }
    }

    fn sub_transform_updated(&mut self) {
        self.fan_out(|fx, sub| {
            // SAFETY: thunk contract.
            unsafe { (fx.sub_transform_updated)(sub) }
        });
    }

    fn node_reset(&mut self) {
        let (fx, _) = self.fx();
        for node in self.m(|m| m.nodes.clone()) {
            // SAFETY: thunk contract.
            if unsafe { (fx.n_active)(node) } {
                self.set_matrix(node, &IDENTITY, true);
            }
        }
    }

    fn node_update(&mut self) {
        for node in self.entity_children() {
            self.update_node(node);
        }
    }

    fn node_restore(&mut self) {
        let (fx, _) = self.fx();
        for (i, node) in self.m(|m| m.nodes.clone()).into_iter().enumerate() {
            // SAFETY: thunk contract.
            if unsafe { (fx.n_active)(node) } {
                let local = self.local(self.mesh().node(i));
                self.set_matrix(node, &local, true);
            }
        }
    }

    fn after_anim(&mut self, dt: f32) {
        let (fx, ctx) = self.fx();
        // SAFETY: thunk contract.
        unsafe { (fx.after_anim)(ctx, dt) };
    }

    fn sub_update_logic(&mut self, dt: f32) {
        self.fan_out(|fx, sub| {
            // SAFETY: thunk contract.
            unsafe { (fx.sub_update_logic)(sub, dt) }
        });
    }

    fn event_count(&mut self, state: usize) -> usize {
        let (fx, _) = self.fx();
        let s = self.state_at(state);
        // SAFETY: thunk contract.
        usize::try_from(unsafe { (fx.ev_count)(s) }).unwrap_or(0)
    }

    fn event_time(&mut self, state: usize, event: usize) -> f32 {
        let (fx, _) = self.fx();
        let s = self.state_at(state);
        // SAFETY: thunk contract.
        unsafe { (fx.ev_time)((fx.ev_at)(s, event as i32)) }
    }

    fn handle_event(&mut self, state: usize, event: usize) {
        let (fx, _) = self.fx();
        let s = self.state_at(state);
        // SAFETY: thunk contract.
        let ev = unsafe { (fx.ev_at)(s, event as i32) };
        self.animation_event(ev);
    }
}

// ---- C ABI (declared in RustMeshEntity.h) ----

macro_rules! me {
    ($p:expr) => {
        Me($p)
    };
}

/// # Safety
/// `fx` is a static table; `ctl`, `mesh` and `file_units` outlive the entity; `skel` is null or
/// the mesh's skeleton state.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_me_new(
    ctx: P,
    fx: &'static Fx,
    ctl: *mut MeshCtl,
    mesh: *const Mesh,
    skel: *mut Skeleton,
    file_units: *const bool,
) -> *mut MeshEnt {
    let m = Box::into_raw(Box::new(MeshEnt {
        ctx,
        fx,
        // SAFETY: caller contract.
        ctl: unsafe { Ctl::new(ctl) },
        mesh,
        skel,
        file_units,
        names: Names::default(),
        subs: Vec::new(),
        pods: Vec::new(),
        states: Vec::new(),
        cores: Vec::new(),
        track_idx: Vec::new(),
        bones: Vec::new(),
        temp: Vec::new(),
        nodes: Vec::new(),
        root: ptr::null_mut(),
        rest: Vec::new(),
        mats: Vec::new(),
        inv_world: IDENTITY,
    }));
    me!(m).build();
    m
}

/// # Safety
/// `m` from `ohpl_me_new`, freed once.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_me_free(m: *mut MeshEnt) {
    if m.is_null() {
        return;
    }
    me!(m).destroy();
    // SAFETY: caller contract.
    drop(unsafe { Box::from_raw(m) });
}

/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_me_update_logic(m: *mut MeshEnt, is_static: bool, dt: f32) {
    let mut me = me!(m);
    let c = me.ctl();
    h11::update_logic(&mut me, c, is_static, dt);
}

/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_me_update_graphics(m: *mut MeshEnt) {
    me!(m).update_graphics();
}

/// Bone matrices for skinning; valid until the entity changes.
///
/// # Safety
/// `m` live, `n` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_me_bone_matrices(m: *mut MeshEnt, n: *mut i32) -> *const Matrixf {
    let (p, len) = me!(m).m(|m| (m.mats.as_ptr(), m.mats.len()));
    // SAFETY: caller contract.
    unsafe { *n = len as i32 };
    p
}

macro_rules! getter {
    ($($name:ident($($a:ident: $t:ty),*) -> $r:ty => |$m:ident| $body:expr;)*) => {$(
        /// # Safety
        /// `m` live; pointers readable.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name(m: *mut MeshEnt $(, $a: $t)*) -> $r {
            me!(m).m(|$m| $body)
        }
    )*};
}

fn at(v: &[P], i: i64) -> P {
    usize::try_from(i)
        .ok()
        .and_then(|i| v.get(i))
        .copied()
        .unwrap_or(ptr::null_mut())
}

getter! {
    ohpl_me_sub(i: u32) -> P => |m| at(&m.subs, i64::from(i));
    ohpl_me_sub_num() -> i32 => |m| m.subs.len() as i32;
    ohpl_me_anim_state(i: i32) -> P => |m| at(&m.states, i64::from(i));
    ohpl_me_anim_num() -> i32 => |m| m.states.len() as i32;
    ohpl_me_bone(i: i32) -> P => |m| at(&m.bones, i64::from(i));
    ohpl_me_bone_num() -> i32 => |m| m.bones.len() as i32;
    ohpl_me_node(i: i32) -> P => |m| at(&m.nodes, i64::from(i));
    ohpl_me_node_num() -> i32 => |m| m.nodes.len() as i32;
    ohpl_me_root() -> P => |m| m.root;
    ohpl_me_bone_index_of(p: P) -> i32 => |m| m.bones.iter().position(|&b| b == p).map_or(-1, |i| i as i32);
}

macro_rules! by_name {
    ($($name:ident, $kind:expr;)*) => {$(
        /// Index, -1 when unknown.
        ///
        /// # Safety
        /// `m` live; `p` readable for `n` bytes.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name(m: *mut MeshEnt, p: *const u8, n: usize) -> i32 {
            let name = text(p, n);
            me!(m).m(|m| m.names.find($kind, &name))
        }
    )*};
}

by_name! {
    ohpl_me_anim_index, ANIM;
    ohpl_me_bone_index, BONE;
    ohpl_me_node_index, NODE;
    ohpl_me_sub_index, SUB;
}

/// # Safety
/// `m` live; name readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_me_add_animation(
    m: *mut MeshEnt,
    anim: P,
    p: *const u8,
    n: usize,
    speed: f32,
) -> P {
    let me = me!(m);
    me.create_nodes();
    let name = text(p, n);
    let (fx, ctx) = me.fx();
    // SAFETY: thunk contract.
    let state = unsafe { (fx.add_anim_state)(ctx, anim, name.as_ptr(), name.len()) };
    // SAFETY: as above.
    let core = unsafe { (fx.anim_core)(state) };
    // SAFETY: the core lives with the state.
    unsafe { (*core).base_speed = speed };
    me.m(|m| {
        m.states.push(state);
        m.cores.push(core);
        m.track_idx.push(Vec::new());
        let idx = m.states.len() as i32 - 1;
        m.names.insert(ANIM, &name, idx);
    });
    state
}

/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_me_clear_animations(m: *mut MeshEnt) {
    let me = me!(m);
    let (fx, _) = me.fx();
    me.m(|m| m.cores.clear());
    me.m(|m| m.track_idx.clear());
    for state in me.m(|m| std::mem::take(&mut m.states)) {
        // SAFETY: thunk contract.
        unsafe { (fx.delete_state)(state) };
    }
}

/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_me_play(m: *mut MeshEnt, index: i32, looping: bool, stop_prev: bool) {
    let cores = me!(m).core_play_args();
    // SAFETY: the cores are live distinct states of this entity.
    unsafe { h11::play(&cores, index, looping, stop_prev) };
}

/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_me_play_fade_to(
    m: *mut MeshEnt,
    index: i32,
    looping: bool,
    time: f32,
) {
    let cores = me!(m).core_play_args();
    // SAFETY: as in `ohpl_me_play`.
    unsafe { h11::play_fade_to(&cores, index, looping, time) };
}

/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_me_stop(m: *mut MeshEnt) {
    let cores = me!(m).core_play_args();
    // SAFETY: as in `ohpl_me_play`.
    unsafe { h11::stop(&cores) };
}

/// # Safety
/// `m` live; name readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_me_play_name(
    m: *mut MeshEnt,
    p: *const u8,
    n: usize,
    looping: bool,
    time: f32,
    fade: bool,
    stop_prev: bool,
) {
    let me = me!(m);
    let name = text(p, n);
    let Some(i) = me.state_index(&name, i32::from(fade)) else {
        return;
    };
    // SAFETY: as in `ohpl_me_play`.
    unsafe {
        if fade {
            ohpl_me_play_fade_to(m, i as i32, looping, time);
        } else {
            ohpl_me_play(m, i as i32, looping, stop_prev);
        }
    }
}

/// # Safety
/// `m` live; name readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_me_anim_is_over(m: *mut MeshEnt, p: *const u8, n: usize) -> bool {
    let me = me!(m);
    let Some(i) = me.state_index(&text(p, n), 2) else {
        return false;
    };
    let core = me.m(|m| m.cores.get(i).copied()).unwrap_or(ptr::null_mut());
    // SAFETY: the core lives with its state.
    unsafe { core.as_ref() }.is_some_and(AnimCore::is_over)
}

/// # Safety
/// `m` live; `p` readable for `n` matrices.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_me_set_rest_pose(m: *mut MeshEnt, p: *const Matrixf, n: usize) {
    let me = me!(m);
    if n != me.m(|m| m.bones.len()) {
        return;
    }
    // SAFETY: caller contract.
    let rest = unsafe { std::slice::from_raw_parts(p, n) }.to_vec();
    me.m(|m| m.rest.clone_from(&rest));
    for (state, local) in me.bones().into_iter().zip(&rest) {
        me.set_matrix(state, local, true);
    }
    me.ctl().set_updated_bones(false);
}

macro_rules! unit {
    ($($name:ident($($a:ident: $t:ty),*) => $f:ident;)*) => {$(
        /// # Safety
        /// `m` live.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name(m: *mut MeshEnt $(, $a: $t)*) {
            me!(m).$f($($a),*)
        }
    )*};
}

unit! {
    ohpl_me_physics_active(active: bool) => physics_active;
    ohpl_me_physics_fade(time: f32) => physics_fade;
    ohpl_me_colliders_active(active: bool) => colliders_active;
    ohpl_me_align_bodies() => align_bodies;
    ohpl_me_reset_graphics() => reset_gfx;
    ohpl_me_bounding_volume() => bounding_volume;
}

/// # Safety
/// `m` live; `args` is the C++ query record.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_me_check_colliders(
    m: *mut MeshEnt,
    args: usize,
    has_pos: bool,
    has_num: bool,
) -> bool {
    me!(m).check_colliders(args, has_pos, has_num)
}

/// `CalculateTransformFromSkeleton`.
///
/// # Safety
/// `m` live; outputs writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_me_transform_from_skeleton(
    m: *mut MeshEnt,
    out: *mut Matrixf,
    pos: *mut Vector3f,
    angle: *mut f32,
) {
    let me = me!(m);
    let (fx, root) = (me.fx().0, me.m(|m| m.root));
    // SAFETY: thunk contract.
    let first = if root.is_null() || unsafe { (fx.n_child_count)(root) } == 0 {
        ptr::null_mut()
    } else {
        unsafe { (fx.n_child_at)(root, 0) }
    };
    let inv = me
        .with_skel(|s| {
            let id = s.first_child(0)?;
            s.inv_world(id).copied()
        })
        .flatten();
    let (Some(inv), false) = (inv, first.is_null()) else {
        return;
    };
    let (mtx, p, y) = h11::transform_from_skeleton(&me.world(first), &inv);
    // SAFETY: caller contract.
    unsafe {
        *out = mtx;
        *pos = p;
        *angle = y;
    }
}

macro_rules! fan {
    ($($name:ident($($a:ident: $t:ty),*) => |$fx:ident, $s:ident| $body:expr;)*) => {$(
        /// # Safety
        /// `m` live.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name(m: *mut MeshEnt $(, $a: $t)*) {
            // SAFETY: thunk contract.
            me!(m).fan_out(|$fx, $s| unsafe { $body })
        }
    )*};
}

fan! {
    ohpl_me_fan_visible(v: bool) => |fx, s| (fx.sub_visible)(s, v);
    ohpl_me_fan_static(v: bool) => |fx, s| (fx.sub_static)(s, v);
    ohpl_me_fan_illum(v: f32) => |fx, s| (fx.sub_illum)(s, v);
    ohpl_me_fan_coverage(v: f32) => |fx, s| (fx.sub_coverage)(s, v);
    ohpl_me_fan_liquid(v: f32) => |fx, s| (fx.sub_liquid)(s, v);
    ohpl_me_fan_auto_liquid() => |fx, s| (fx.sub_auto_liquid)(s);
    ohpl_me_fan_render_flag(bit: i32, v: bool) => |fx, s| (fx.sub_render_flag)(s, bit, v);
    ohpl_me_fan_color_mul(c: P) => |fx, s| (fx.sub_color_mul)(s, c);
    ohpl_me_fan_illum_color(c: P) => |fx, s| (fx.sub_illum_color)(s, c);
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::cell::RefCell;

    #[derive(Default)]
    struct Node {
        children: Vec<usize>,
        world: Matrixf,
        body: usize,
        collider: usize,
        inv_body: Matrixf,
    }

    #[derive(Default)]
    struct Mock {
        nodes: Vec<Node>,
        bodies: Vec<Matrixf>,
        log: Vec<String>,
    }

    thread_local!(static MOCK: RefCell<Mock> = RefCell::new(Mock::default()));

    fn tr(x: f32, y: f32, z: f32) -> Matrixf {
        let mut m = IDENTITY;
        m.m[0][3] = x;
        m.m[1][3] = y;
        m.m[2][3] = z;
        m
    }

    fn id(p: P) -> usize {
        (p as usize).wrapping_sub(1)
    }

    fn with<T>(f: impl FnOnce(&mut Mock) -> T) -> T {
        MOCK.with(|m| f(&mut m.borrow_mut()))
    }

    fn log(s: String) {
        with(|m| m.log.push(s));
    }

    unsafe extern "C" fn n_child_count(n: P) -> usize {
        with(|m| m.nodes.get(id(n)).map_or(0, |n| n.children.len()))
    }

    unsafe extern "C" fn n_child_at(n: P, i: usize) -> P {
        with(|m| m.nodes[id(n)].children[i] + 1) as P
    }

    unsafe extern "C" fn n_world(n: P) -> *const Matrixf {
        with(|m| &raw const m.nodes[id(n)].world)
    }

    unsafe extern "C" fn n_update_world(n: P) {
        log(format!("uw {}", n as usize));
    }

    unsafe extern "C" fn n_set_matrix(n: P, m: *const Matrixf, _children: bool) {
        // SAFETY: the module passes a live matrix.
        let v = unsafe { *m };
        log(format!("set {} {:?}", n as usize, v.m[0]));
        with(|mock| mock.nodes[id(n)].world = v);
    }

    unsafe extern "C" fn bs_body(n: P) -> P {
        with(|m| m.nodes[id(n)].body) as P
    }

    unsafe extern "C" fn bs_collider(n: P) -> P {
        with(|m| m.nodes[id(n)].collider) as P
    }

    unsafe extern "C" fn bs_inv_body(n: P) -> *const Matrixf {
        with(|m| &raw const m.nodes[id(n)].inv_body)
    }

    unsafe extern "C" fn body_world(b: P) -> *const Matrixf {
        with(|m| &raw const m.bodies[id(b)])
    }

    unsafe extern "C" fn body_set_active(b: P, on: bool) {
        log(format!("active {} {}", b as usize, i32::from(on)));
    }

    unsafe extern "C" fn body_enable(b: P) {
        log(format!("enable {}", b as usize));
    }

    unsafe extern "C" fn body_zero_vel(b: P) {
        log(format!("zero {}", b as usize));
    }

    static FX: std::sync::OnceLock<Fx> = std::sync::OnceLock::new();

    fn fx() -> &'static Fx {
        FX.get_or_init(|| Fx {
            n_child_count,
            n_child_at,
            n_world,
            n_update_world,
            n_set_matrix,
            bs_body,
            bs_collider,
            bs_inv_body,
            body_world,
            body_set_active,
            body_enable,
            body_zero_vel,
            ..Fx::noop()
        })
    }

    // root(1) -> bone 2 (body 1) -> bone 3 (no body); bone 4 (body 2, collider 3) under root
    fn entity(ctl: &mut MeshCtl) -> MeshEnt {
        with(|m| {
            *m = Mock::default();
            for _ in 0..4 {
                m.nodes.push(Node {
                    world: IDENTITY,
                    inv_body: tr(0.0, -1.0, 0.0),
                    ..Node::default()
                });
            }
            m.nodes[0].children = vec![1, 3];
            m.nodes[1].children = vec![2];
            m.nodes[1].body = 1;
            m.nodes[3].body = 2;
            m.nodes[3].collider = 3;
            m.bodies = vec![tr(1.0, 2.0, 3.0), tr(4.0, 5.0, 6.0)];
        });
        MeshEnt {
            ctx: ptr::null_mut(),
            fx: fx(),
            // SAFETY: `ctl` outlives the entity in each test.
            ctl: unsafe { Ctl::new(ctl) },
            mesh: ptr::null(),
            skel: ptr::null_mut(),
            file_units: ptr::null(),
            names: Names::default(),
            subs: Vec::new(),
            pods: Vec::new(),
            states: Vec::new(),
            cores: Vec::new(),
            track_idx: Vec::new(),
            bones: vec![2 as P, 3 as P, 4 as P],
            temp: Vec::new(),
            nodes: Vec::new(),
            root: 1 as P,
            rest: Vec::new(),
            mats: Vec::new(),
            inv_world: IDENTITY,
        }
    }

    #[test]
    fn bone_from_body_sets_local_from_body_and_recurses() {
        let mut ctl = MeshCtl::new();
        let mut e = entity(&mut ctl);
        let mut me = Me(&raw mut e);
        me.physics_from_bodies();
        let bone_world = matrix_mul(&tr(1.0, 2.0, 3.0), &tr(0.0, -1.0, 0.0));
        let want = matrix_mul(&matrix_inverse(&IDENTITY), &bone_world);
        with(|m| {
            assert_eq!(m.nodes[1].world, want);
            assert!(m.log.contains(&"uw 3".to_string()));
            assert_eq!(m.log.iter().filter(|l| l.starts_with("set")).count(), 2);
        });
    }

    #[test]
    fn physics_active_and_fade_drive_bodies() {
        let mut ctl = MeshCtl::new();
        ctl.colliders = true;
        let mut e = entity(&mut ctl);
        let me = Me(&raw mut e);
        me.physics_active(true);
        assert!(ctl.physics && !ctl.updated_bones);
        with(|m| {
            assert!(m.log.contains(&"active 3 0".to_string()));
            assert!(m.log.contains(&"active 2 1".to_string()));
            assert!(!m.log.iter().any(|l| l.starts_with("zero")));
            m.log.clear();
        });
        me.physics_fade(0.5);
        assert!(ctl.fading && ctl.physics_fade_speed == 2.0);
        with(|m| {
            assert_eq!(m.log, ["active 1 0", "active 2 0"]);
            m.log.clear();
        });
        me.physics_active(false);
        with(|m| assert_eq!(m.log.iter().filter(|l| l.starts_with("zero")).count(), 2));
    }

    #[test]
    fn names_keep_first_insert() {
        let mut n = Names::default();
        n.insert(ANIM, b"idle", 0);
        n.insert(ANIM, b"idle", 5);
        assert_eq!((n.find(ANIM, b"idle"), n.find(ANIM, b"run")), (0, -1));
    }
}
