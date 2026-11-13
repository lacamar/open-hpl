//! H11 pass 2: `cMeshEntity` object glue. The C++ keeps the node, bone, body, sub-mesh and
//! animation-state objects; everything that decides what to call on them lives here and reaches
//! them through one host op (`HostCall`, ops in `RustSceneH11.h`). Name lookups are owned here.
//!
//! Matrix pointers a host op returns are copied at once; no reference into C++ state is held
//! across a host call (the C++ side may call back).

use std::collections::{BTreeMap, BTreeSet};
use std::ffi::c_void;

use super::h11::{self, MeshCtl};
use super::skinning;
use crate::math::{IDENTITY, matrix_inverse, matrix_mul};
use crate::types::{Matrixf, Vector3f};

pub type CallFn = unsafe extern "C" fn(*mut c_void, i32, usize, usize, i32, f32) -> usize;

/// `OhplH11Call`: the entity-side op table.
#[repr(C)]
pub struct HostCall {
    pub ctx: *mut c_void,
    pub call: CallFn,
}

#[derive(Clone, Copy)]
#[repr(i32)]
pub enum Op {
    NodeActive = 0,
    NodeUpdateMatrix,
    NodeAnimXform,
    NodeChildCount,
    NodeChild,
    NodeWorld,
    NodeUpdateWorld,
    NodeSetMatrix,
    BoneBody,
    BoneCollider,
    BoneInvBody,
    BoneBodyMatrix,
    BodyWorld,
    BodySetActive,
    BodyEnable,
    BodyZeroVel,
    BodySetMatrix,
    BoneState,
    BoneCount,
    Root,
    ResetGfx,
    EntWorld,
    EntParentWorld,
    EntSetMatrix,
    Collide,
    PushPos,
    PushNum,
    SubCount,
    SubVisible,
    SubStatic,
    SubIllum,
    SubCoverage,
    SubLiquid,
    SubAutoLiquid,
    SubRenderFlag,
    SubColorMul,
    SubIllumColor,
    SubBvMin,
    SubBvMax,
    BvFlag,
    BvClearFlag,
    BvFromVertices,
    BvSetLocal,
    BvSetIdentity,
    BoneRadius,
    HasSkeleton,
    Play,
    PlayFade,
    Warn,
    AnimIsOver,
    EvCallback,
    EvValueEmpty,
    EvType,
    EvPlaySound,
    ListCount,
    ListAt,
}

/// `cMeshEntity::*` host ops, one wrapper per op.
pub struct Host<'a>(pub(super) &'a HostCall);

impl Host<'_> {
    fn c(&self, op: Op, a: usize, b: usize, i: i32, f: f32) -> usize {
        // SAFETY: the table is valid for the call (ffi contract).
        unsafe { (self.0.call)(self.0.ctx, op as i32, a, b, i, f) }
    }

    fn op(&self, op: Op, a: usize) -> usize {
        self.c(op, a, 0, 0, 0.0)
    }

    fn mat(&self, op: Op, a: usize) -> Matrixf {
        let p = self.op(op, a) as *const Matrixf;
        // SAFETY: the op returns a valid matrix, live until the next host call; copied at once.
        unsafe { p.as_ref() }.copied().unwrap_or(IDENTITY)
    }

    fn bones(&self) -> usize {
        self.op(Op::BoneCount, 0)
    }
}

/// `SetBoneMatrixFromBodyRec`.
fn bone_from_body_rec(h: &Host, parent_world: &Matrixf, node: usize) {
    let world = if h.op(Op::BoneBody, node) != 0 {
        let body = h.op(Op::BoneBody, node);
        let bone_world = matrix_mul(&h.mat(Op::BodyWorld, body), &h.mat(Op::BoneInvBody, node));
        let parent_inv = matrix_inverse(parent_world);
        let local = matrix_mul(&parent_inv, &bone_world);
        h.c(Op::NodeSetMatrix, node, &raw const local as usize, 0, 0.0);
        bone_world
    } else {
        h.op(Op::NodeUpdateWorld, node);
        h.mat(Op::NodeWorld, node)
    };
    for i in 0..h.c(Op::NodeChildCount, node, 0, 0, 0.0) {
        let child = h.c(Op::NodeChild, node, 0, i as i32, 0.0);
        bone_from_body_rec(h, &world, child);
    }
}

/// Bone-state children of the root, for `UpdateLogic` and `SetBoneMatrixFromBodyRec` callers.
fn children_of(h: &Host, node: usize, mut f: impl FnMut(&Host, usize)) {
    for i in 0..h.c(Op::NodeChildCount, node, 0, 0, 0.0) {
        let child = h.c(Op::NodeChild, node, 0, i as i32, 0.0);
        f(h, child);
    }
}

/// `UpdateNodeMatrixRec`.
fn update_node_rec(h: &Host, node: usize) {
    if h.op(Op::NodeActive, node) != 0 {
        h.op(Op::NodeUpdateMatrix, node);
    }
    h.op(Op::NodeAnimXform, node);
    children_of(h, node, update_node_rec);
}

pub(super) fn physics_from_bodies(h: &Host) {
    let root = h.op(Op::Root, 0);
    let world = h.mat(Op::NodeWorld, root);
    children_of(h, root, |h, n| bone_from_body_rec(h, &world, n));
}

pub(super) fn bone_nodes_update(h: &Host) {
    let root = h.op(Op::Root, 0);
    children_of(h, root, update_node_rec);
}

pub(super) fn node_update(h: &Host) {
    for i in 0..h.op(Op::ListCount, 0) {
        update_node_rec(h, h.c(Op::ListAt, 0, 0, i as i32, 0.0));
    }
}

fn bone_count(h: &Host) -> usize {
    h.bones()
}

/// `SetSkeletonPhysicsActive`.
pub(super) fn physics_active(h: &Host, c: *mut MeshCtl, active: bool) {
    // SAFETY: `c` is the entity's `MeshCtl`, valid for the call; no reference is formed.
    h11::skeleton_physics_set(unsafe { h11::Ctl::new(c) }, active);
    h.op(Op::ResetGfx, 0);
    for bone in 0..bone_count(h) {
        let state = h.c(Op::BoneState, 0, 0, bone as i32, 0.0);
        let body = h.op(Op::BoneBody, state);
        let collider = h.op(Op::BoneCollider, state);
        if body == 0 {
            continue;
        }
        h.c(Op::BodySetActive, body, 0, i32::from(active), 0.0);
        h.op(Op::BodyEnable, body);
        if !active {
            h.op(Op::BodyZeroVel, body);
        }
        // SAFETY: as above.
        if unsafe { (*c).colliders } {
            h.c(Op::BodySetActive, collider, 0, i32::from(!active), 0.0);
        }
    }
}

/// `FadeSkeletonPhysicsWeight`.
pub(super) fn physics_fade(h: &Host, c: *mut MeshCtl, time: f32) {
    // SAFETY: as in `physics_active`.
    if !h11::skeleton_physics_fade(unsafe { h11::Ctl::new(c) }, time) {
        return;
    }
    for bone in 0..bone_count(h) {
        let state = h.c(Op::BoneState, 0, 0, bone as i32, 0.0);
        let body = h.op(Op::BoneBody, state);
        if body != 0 {
            h.c(Op::BodySetActive, body, 0, 0, 0.0);
        }
    }
}

/// `SetSkeletonCollidersActive`.
pub(super) fn colliders_active(h: &Host, c: *mut MeshCtl, active: bool) {
    // SAFETY: as in `physics_active`.
    unsafe { (*c).colliders = active };
    // SAFETY: as above.
    let physics = unsafe { (*c).physics };
    for bone in 0..bone_count(h) {
        let state = h.c(Op::BoneState, 0, 0, bone as i32, 0.0);
        let collider = h.op(Op::BoneCollider, state);
        if collider == 0 {
            continue;
        }
        if active && !physics {
            h.c(Op::BodySetActive, collider, 0, 1, 0.0);
        } else if !active {
            h.c(Op::BodySetActive, collider, 0, 0, 0.0);
        }
    }
}

/// `AlignBodiesToSkeleton`.
pub(super) fn align_bodies(h: &Host) {
    let n = bone_count(h);
    let mut mats = vec![IDENTITY; n];
    for (bone, m) in mats.iter_mut().enumerate() {
        let state = h.c(Op::BoneState, 0, 0, bone as i32, 0.0);
        if h.op(Op::BoneBody, state) != 0 {
            *m = matrix_mul(
                &h.mat(Op::NodeWorld, state),
                &h.mat(Op::BoneBodyMatrix, state),
            );
        }
    }
    let entity_world = h.mat(Op::EntWorld, 0);
    let mut done = BTreeSet::new();
    for (bone, m) in mats.iter().enumerate() {
        let state = h.c(Op::BoneState, 0, 0, bone as i32, 0.0);
        let body = h.op(Op::BoneBody, state);
        if body != 0 && done.insert(body) {
            h.c(
                Op::BodySetMatrix,
                body,
                m as *const Matrixf as usize,
                0,
                0.0,
            );
        }
    }
    let parent = h.op(Op::EntParentWorld, 0) as *const Matrixf;
    // SAFETY: the op returns null or a valid matrix, live until the next host call; copied here.
    if let Some(parent) = unsafe { parent.as_ref() }.copied() {
        let local = matrix_mul(&matrix_inverse(&parent), &entity_world);
        h.c(Op::EntSetMatrix, 0, &raw const local as usize, 0, 0.0);
    }
}

/// `CheckColliderShapeCollision`; `args` is the C++ query record the collide/push ops read.
pub(super) fn check_colliders(h: &Host, args: usize, has_pos: bool, has_num: bool) -> bool {
    let mut hit = false;
    for i in 0..bone_count(h) {
        let state = h.c(Op::BoneState, 0, 0, i as i32, 0.0);
        let body = h.op(Op::BoneCollider, state);
        if body == 0 {
            continue;
        }
        let m = matrix_mul(
            &h.mat(Op::NodeWorld, state),
            &h.mat(Op::BoneBodyMatrix, state),
        );
        h.c(Op::BodySetMatrix, body, &raw const m as usize, 0, 0.0);
        if h.c(Op::Collide, body, args, 0, 0.0) == 0 {
            continue;
        }
        hit = true;
        if !has_pos && !has_num {
            break;
        }
        if has_pos {
            h.op(Op::PushPos, args);
        }
        if has_num {
            h.c(Op::PushNum, args, 0, i as i32, 0.0);
        }
    }
    hit
}

/// `GetBoundingVolume` after the skeleton and sub-mesh cases split.
pub(super) fn bounding_volume(h: &Host) {
    if h.op(Op::BvFlag, 0) == 0 {
        return;
    }
    if h.op(Op::HasSkeleton, 0) != 0 {
        update_bv_from_skeleton(h);
        h.op(Op::BvClearFlag, 0);
        return;
    }
    h.op(Op::BvClearFlag, 0);
    let n = h.op(Op::SubCount, 0);
    let (mut mins, mut maxs) = (Vec::new(), Vec::new());
    for i in 0..n {
        let rd = |op| {
            let p = h.c(op, 0, 0, i as i32, 0.0) as *const Vector3f;
            // SAFETY: the op returns a valid vector, copied at once.
            unsafe { p.as_ref() }.copied().unwrap_or_default()
        };
        mins.push(rd(Op::SubBvMin));
        maxs.push(rd(Op::SubBvMax));
    }
    if let Some((lo, hi)) = h11::bounds_union(&mins, &maxs) {
        h.c(
            Op::BvSetLocal,
            &raw const lo as usize,
            &raw const hi as usize,
            0,
            0.0,
        );
    }
}

/// `UpdateBVFromSkeleton`.
fn update_bv_from_skeleton(h: &Host) {
    if h.op(Op::HasSkeleton, 0) == 0 {
        return;
    }
    let n = bone_count(h);
    if n == 0 {
        h.op(Op::BvFromVertices, 0);
        return;
    }
    let mats: Vec<Matrixf> = (0..n)
        .map(|i| h.mat(Op::NodeWorld, h.c(Op::BoneState, 0, 0, i as i32, 0.0)))
        .collect();
    let radii: Vec<f32> = (0..n)
        .map(|i| f32::from_bits(h.c(Op::BoneRadius, 0, 0, i as i32, 0.0) as u32))
        .collect();
    let refs: Vec<&Matrixf> = mats.iter().collect();
    let (lo, hi) = skinning::aabb_from_bones(&refs, &radii);
    h.op(Op::BvSetIdentity, 0);
    h.c(
        Op::BvSetLocal,
        &raw const lo as usize,
        &raw const hi as usize,
        0,
        0.0,
    );
}

/// `HandleAnimationEvent`.
pub(super) fn animation_event(h: &Host, event: usize) {
    if h.op(Op::EvCallback, event) != 0 || h.op(Op::EvValueEmpty, event) != 0 {
        return;
    }
    if h.op(Op::EvType, event) != 0 {
        h.op(Op::EvPlaySound, event);
    }
}

/// Fan-out of the entity setters to the sub-mesh entities.
pub(super) fn fan_out(h: &Host, op: Op, a: usize, b: usize, f: f32) {
    for i in 0..h.op(Op::SubCount, 0) {
        h.c(op, a, b, i as i32, f);
    }
}

/// Name tables of one entity: `std::map::insert` semantics (the first insert of a name wins).
#[derive(Default)]
pub struct Names {
    maps: [BTreeMap<Vec<u8>, i32>; 4],
}

impl Names {
    pub(super) fn insert(&mut self, kind: usize, name: &[u8], idx: i32) {
        if let Some(m) = self.maps.get_mut(kind) {
            m.entry(name.to_vec()).or_insert(idx);
        }
    }

    pub(super) fn find(&self, kind: usize, name: &[u8]) -> i32 {
        self.maps
            .get(kind)
            .and_then(|m| m.get(name))
            .copied()
            .unwrap_or(-1)
    }
}

/// `PlayName` / `PlayFadeToName` / `AnimationIsOver`: look the state up, warn when missing.
pub(super) fn play_name(
    h: &Host,
    n: &Names,
    name: &[u8],
    looping: bool,
    arg: f32,
    mode: i32,
    stop_prev: bool,
) {
    let idx = n.find(0, name);
    if idx < 0 {
        h.c(Op::Warn, name.as_ptr() as usize, name.len(), mode, 0.0);
        return;
    }
    if mode == 0 {
        h.c(
            Op::Play,
            usize::from(looping),
            usize::from(stop_prev),
            idx,
            0.0,
        );
    } else {
        h.c(Op::PlayFade, usize::from(looping), 0, idx, arg);
    }
}

pub(super) fn anim_is_over(h: &Host, n: &Names, name: &[u8]) -> bool {
    let idx = n.find(0, name);
    if idx < 0 {
        h.c(Op::Warn, name.as_ptr() as usize, name.len(), 2, 0.0);
        return false;
    }
    h.c(Op::AnimIsOver, 0, 0, idx, 0.0) != 0
}
