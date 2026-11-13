//! H11: control flow of `cMeshEntity` (animation update, skeleton-physics fade, graphics gate),
//! the `cSubMeshEntity` gates, light visibility and shadow-caster cache, spot and fog state.
//!
//! The C++ keeps the node, bone, body and track objects; this module decides the order of the
//! calls and owns the flags. Every call into C++ goes through `MeshHost`: the C++ side may call
//! back (animation callbacks, `SetSkeletonPhysicsActive`), so no reference into the host's state
//! or into `MeshCtl` is held across a host call; the animation cores are re-fetched after each.

use std::collections::BTreeMap;

use super::animstate::{self, AnimCore};
use crate::math::{
    IDENTITY, angle_from_points_2d, cos, fma, matrix_inverse, matrix_mul, matrix_quaternion,
    matrix_scale, sin,
};
use crate::types::{Matrixf, Quaternion, Vector2f, Vector3f};

/// The `cMeshEntity` members the control flow reads and writes.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct MeshCtl {
    pub physics_fade_speed: f32,
    pub physics_weight: f32,
    pub bone_matrices_transform_count: i32,
    pub bone_matrices_version: i32,
    pub inv_world_transform_count: i32,
    pub physics: bool,
    pub fading: bool,
    pub sleeping: bool,
    pub can_sleep: bool,
    pub colliders: bool,
    pub normalize: bool,
    pub updated_bones: bool,
    pub has_updated_animation: bool,
    pub matrices_need_update: bool,
}

impl MeshCtl {
    pub const fn new() -> Self {
        Self {
            physics_fade_speed: 1.0,
            physics_weight: 1.0,
            bone_matrices_transform_count: -1,
            bone_matrices_version: 0,
            inv_world_transform_count: -1,
            physics: false,
            fading: false,
            sleeping: false,
            can_sleep: true,
            colliders: false,
            normalize: true,
            updated_bones: false,
            has_updated_animation: true,
            matrices_need_update: true,
        }
    }
}

impl Default for MeshCtl {
    fn default() -> Self {
        Self::new()
    }
}

/// Raw view of the C++-owned `MeshCtl`. Host calls may write the struct, so fields are read and
/// written one at a time and no reference to it exists.
#[derive(Clone, Copy)]
pub struct Ctl(*mut MeshCtl);

macro_rules! ctl_field {
    ($($get:ident, $set:ident: $f:ident: $t:ty;)*) => {$(
        fn $get(self) -> $t {
            // SAFETY: `Ctl::new` contract: valid for the whole call; no reference is formed.
            unsafe { (*self.0).$f }
        }
        fn $set(self, v: $t) {
            // SAFETY: as above.
            unsafe { (*self.0).$f = v }
        }
    )*};
}

impl Ctl {
    /// # Safety
    /// `p` is valid and aligned for reads and writes for as long as the `Ctl` is used.
    pub unsafe fn new(p: *mut MeshCtl) -> Self {
        Self(p)
    }

    ctl_field! {
        fade_speed, set_fade_speed: physics_fade_speed: f32;
        weight, set_weight: physics_weight: f32;
        transform_count, set_transform_count: bone_matrices_transform_count: i32;
        inv_count, set_inv_count: inv_world_transform_count: i32;
        physics, set_physics: physics: bool;
        fading, set_fading: fading: bool;
        sleeping, set_sleeping: sleeping: bool;
        updated_bones, set_updated_bones: updated_bones: bool;
        has_updated_animation, set_has_updated_animation: has_updated_animation: bool;
        need_update, set_need_update: matrices_need_update: bool;
    }

    fn colliders(self) -> bool {
        // SAFETY: `Ctl::new` contract.
        unsafe { (*self.0).colliders }
    }

    fn normalize(self) -> bool {
        // SAFETY: `Ctl::new` contract.
        unsafe { (*self.0).normalize }
    }
}

/// Calls into the C++ entity. `cores` is in state order, synced with the states.
pub trait MeshHost {
    fn cores(&mut self) -> &[*mut AnimCore];
    fn state_count(&mut self) -> usize;
    fn bone_count(&mut self) -> usize;
    fn has_skeleton(&mut self) -> bool;
    fn has_rest_pose(&mut self) -> bool;
    fn bone_reset(&mut self, bone: usize, map_pose: bool, reset_temp: bool);
    fn physics_from_bodies(&mut self);
    fn physics_mix(&mut self, bone: usize, weight: f32);
    fn physics_deactivate(&mut self);
    fn skeleton_apply(&mut self, state: usize, weight: f32);
    fn node_apply(&mut self, state: usize, weight: f32);
    fn bone_nodes_update(&mut self);
    fn bone_entity_children(&mut self);
    fn colliders_update(&mut self);
    fn sub_transform_updated(&mut self);
    fn node_reset(&mut self);
    fn node_update(&mut self);
    fn node_restore(&mut self);
    fn after_anim(&mut self, dt: f32);
    fn sub_update_logic(&mut self, dt: f32);
    fn event_count(&mut self, state: usize) -> usize;
    fn event_time(&mut self, state: usize, event: usize) -> f32;
    fn handle_event(&mut self, state: usize, event: usize);
}

fn core_at<H: MeshHost>(h: &mut H, i: usize) -> Option<AnimCore> {
    let p = *h.cores().get(i)?;
    // SAFETY: the host returns valid core pointers; read at once, before any other host call.
    unsafe { p.as_ref() }.copied()
}

fn weight_mul<H: MeshHost>(h: &mut H, normalize: bool) -> f32 {
    // SAFETY: valid core pointers (host contract), no host call while the iterator lives.
    let states = h.cores().iter().filter_map(|&p| unsafe { p.as_ref() });
    animstate::weight_mul(states, normalize)
}

fn update_anims<H: MeshHost>(h: &mut H, dt: f32) {
    // SAFETY: valid, distinct core pointers (host contract), no host call while the iterator lives.
    let states = h.cores().iter().filter_map(|&p| unsafe { p.as_mut() });
    super::meshentity::update_active(states, dt);
}

/// `cMeshEntity::UpdateLogic`.
pub fn update_logic<H: MeshHost>(h: &mut H, c: Ctl, is_static: bool, dt: f32) {
    if is_static {
        return;
    }
    if c.fading() && c.physics() {
        c.set_weight(fma(-dt, c.fade_speed(), c.weight()));
        if c.weight() <= 0.0 {
            c.set_fading(false);
            c.set_weight(1.0);
            h.physics_deactivate();
        }
    }
    c.set_sleeping(false);
    if h.state_count() != 0 || c.physics() {
        animate(h, c, dt);
    }
    h.sub_update_logic(dt);
    events(h);
    if h.has_skeleton() {
        c.set_need_update(true);
    }
}

fn animate<H: MeshHost>(h: &mut H, c: Ctl, dt: f32) {
    let (mut any, mut base) = (false, false);
    for i in 0..h.state_count() {
        if let Some(s) = core_at(h, i).filter(|s| s.active) {
            any = true;
            base |= !s.layer;
        }
    }
    let map_pose = h.has_rest_pose() && !base;
    if h.has_skeleton() {
        animate_skeleton(h, c, dt, any, map_pose);
    } else {
        animate_nodes(h, c, dt, any);
    }
    h.after_anim(dt);
}

fn animate_skeleton<H: MeshHost>(h: &mut H, c: Ctl, dt: f32, any: bool, map_pose: bool) {
    let mut update_transform = false;
    if any || !c.updated_bones() || (c.physics() && !c.sleeping()) {
        for bone in 0..h.bone_count() {
            h.bone_reset(bone, map_pose, c.physics() && c.weight() != 1.0);
        }
        update_transform = true;
    }
    if c.physics() && (!c.sleeping() || !c.updated_bones()) {
        c.set_updated_bones(true);
        h.physics_from_bodies();
        if c.weight() != 1.0 {
            for bone in 0..h.bone_count() {
                h.physics_mix(bone, c.weight());
            }
        }
    }
    let mul = weight_mul(h, c.normalize());
    for i in 0..h.state_count() {
        if let Some(s) = core_at(h, i).filter(|s| s.active) {
            h.skeleton_apply(i, s.weight * if s.layer { 1.0 } else { mul });
        }
    }
    update_anims(h, dt);
    if any {
        h.bone_nodes_update();
    }
    if any || c.physics() {
        h.bone_entity_children();
    }
    if c.colliders() && !c.physics() {
        h.colliders_update();
    }
    if update_transform {
        h.sub_transform_updated();
    }
}

fn animate_nodes<H: MeshHost>(h: &mut H, c: Ctl, dt: f32, any: bool) {
    if any {
        h.node_reset();
        let mul = weight_mul(h, c.normalize());
        for i in 0..h.state_count() {
            if let Some(s) = core_at(h, i).filter(|s| s.active) {
                h.node_apply(i, s.weight * mul);
            }
        }
        update_anims(h, dt);
        h.node_update();
        c.set_has_updated_animation(true);
    } else if c.has_updated_animation() {
        h.node_restore();
        c.set_has_updated_animation(false);
    }
}

fn events<H: MeshHost>(h: &mut H) {
    let mut i = 0;
    while i < h.state_count() {
        if core_at(h, i).is_some_and(|s| s.active && !s.paused) {
            let mut j = 0;
            while j < h.event_count(i) {
                let time = h.event_time(i, j);
                if core_at(h, i).is_some_and(|s| time >= s.prev_time_pos && time < s.time_pos) {
                    h.handle_event(i, j);
                }
                j += 1;
            }
        }
        i += 1;
    }
}

/// # Safety
/// `states` are valid, distinct core pointers.
unsafe fn core_mut<'a>(states: &[*mut AnimCore], i: usize) -> Option<&'a mut AnimCore> {
    // SAFETY: caller contract.
    unsafe { states.get(i)?.as_mut() }
}

/// `cMeshEntity::Stop`.
///
/// # Safety
/// As `core_mut`.
pub unsafe fn stop(states: &[*mut AnimCore]) {
    for i in 0..states.len() {
        // SAFETY: caller contract.
        if let Some(s) = unsafe { core_mut(states, i) } {
            s.set_active(false);
            s.set_time_position(0.0);
        }
    }
}

/// `cMeshEntity::Play`; an index outside the states is ignored (the C++ reads past the vector
/// for a negative one).
///
/// # Safety
/// As `core_mut`.
pub unsafe fn play(states: &[*mut AnimCore], index: i32, looping: bool, stop_prev: bool) {
    if stop_prev {
        // SAFETY: caller contract.
        unsafe { stop(states) };
    }
    let Ok(i) = usize::try_from(index) else {
        return;
    };
    // SAFETY: caller contract.
    if let Some(s) = unsafe { core_mut(states, i) } {
        s.set_active(true);
        s.set_time_position(0.0);
        s.looping = looping;
        s.weight = 1.0;
    }
}

/// `cMeshEntity::PlayFadeTo`; an index outside the states fades the others out only.
///
/// # Safety
/// As `core_mut`.
pub unsafe fn play_fade_to(states: &[*mut AnimCore], index: i32, looping: bool, time: f32) {
    for i in 0..states.len() {
        // SAFETY: caller contract.
        if let Some(s) = unsafe { core_mut(states, i) }.filter(|s| s.active) {
            s.fade_out(time);
        }
    }
    let Ok(i) = usize::try_from(index) else {
        return;
    };
    // SAFETY: caller contract.
    if let Some(s) = unsafe { core_mut(states, i) } {
        if !s.active {
            s.weight = 0.0;
        }
        s.set_active(true);
        s.set_time_position(0.0);
        s.looping = looping;
        s.fade_in(time);
    }
}

/// `SetSkeletonPhysicsActive`, the flag part.
pub fn skeleton_physics_set(c: Ctl, active: bool) {
    c.set_physics(active);
    c.set_updated_bones(false);
    c.set_fading(false);
    c.set_weight(1.0);
}

/// `FadeSkeletonPhysicsWeight`, the flag part; true when the bodies are to be deactivated.
pub fn skeleton_physics_fade(c: Ctl, time: f32) -> bool {
    if !c.physics() {
        return false;
    }
    c.set_fading(true);
    c.set_fade_speed(1.0 / time);
    true
}

/// `UpdateGraphicsForFrame` gate: bit 0 = proceed, bit 1 = the inverse world matrix is stale.
pub fn graphics_gate(c: Ctl, has_skeleton: bool, transform_count: i32) -> u32 {
    if !c.need_update() && c.transform_count() == transform_count {
        return 0;
    }
    c.set_transform_count(transform_count);
    c.set_need_update(false);
    let mut r = 1;
    if has_skeleton && c.inv_count() != transform_count {
        c.set_inv_count(transform_count);
        r |= 2;
    }
    r
}

/// `cSubMeshEntity::UpdateGraphicsForFrame` skin gate; true when the vertices are to be skinned.
pub fn sub_skin_gate(
    graphics_updated: &mut bool,
    skinned_version: &mut i32,
    mesh_version: i32,
    sleeping: bool,
) -> bool {
    if *graphics_updated && (sleeping || *skinned_version == mesh_version) {
        return false;
    }
    *graphics_updated = true;
    *skinned_version = mesh_version;
    true
}

/// `cSubMeshEntity::GetModelMatrix` for a static sub mesh: true when no matrix is returned.
pub fn static_null_matrix(count: &mut i32, world: &Matrixf) -> bool {
    if *count == 0 {
        *count = if *world == IDENTITY { 1 } else { -1 };
        return *count == 1;
    }
    true
}

fn min(a: f32, b: f32) -> f32 {
    if a < b { a } else { b }
}

fn row_length(r: &[f32; 4]) -> f32 {
    let [x, y, z, _] = *r;
    fma(z, z, fma(x, x, y * y)).sqrt()
}

/// `SetBoneRestPoseFromMapValues`: 7 floats per bone (quaternion w,x,y,z then translation).
/// `None` when the count does not match.
pub fn rest_pose_from_map(
    vals: &[f32],
    locals: &[Matrixf],
    unit_scales: &[f32],
    out: &mut [Matrixf],
) -> bool {
    if vals.len() != locals.len().saturating_mul(7) || out.len() < locals.len() {
        return false;
    }
    for (((v, m), unit), o) in vals
        .chunks_exact(7)
        .zip(locals)
        .zip(unit_scales)
        .zip(out.iter_mut())
    {
        let [w, x, y, z, tx, ty, tz] = *v else {
            return false;
        };
        let s = min(
            min(row_length(&m.m[0]), row_length(&m.m[1])),
            row_length(&m.m[2]),
        );
        let q = Quaternion {
            v: Vector3f { x, y, z },
            w,
        };
        let mut l = matrix_mul(
            &matrix_quaternion(&q),
            &matrix_scale(Vector3f { x: s, y: s, z: s }),
        );
        l.m[0][3] = tx * s * unit;
        l.m[1][3] = ty * s * unit;
        l.m[2][3] = tz * s * unit;
        *o = l;
    }
    true
}

/// `CalculateTransformFromSkeleton`: root bone state world matrix, root bone inverse world
/// transform. Returns the matrix, the position and the Y angle.
pub fn transform_from_skeleton(
    state_world: &Matrixf,
    bone_inv_world: &Matrixf,
) -> (Matrixf, Vector3f, f32) {
    let inv_bone = matrix_inverse(state_world);
    let angle = |m: &Matrixf| {
        let f = Vector2f {
            x: -m.m[2][0],
            y: -m.m[2][2],
        };
        -angle_from_points_2d(Vector2f::default(), f)
    };
    let y = angle(&inv_bone) - angle(bone_inv_world);
    let (s, c) = (sin(y), cos(y));
    let mut m = Matrixf {
        m: [
            [c, 0.0, s, 0.0],
            [0.0, 1.0, 0.0, 0.0],
            [-s, 0.0, c, 0.0],
            [0.0, 0.0, 0.0, 1.0],
        ],
    };
    let pos = Vector3f {
        x: state_world.m[0][3],
        y: state_world.m[1][3],
        z: state_world.m[2][3],
    };
    m.m[0][3] = pos.x;
    m.m[1][3] = pos.y;
    m.m[2][3] = pos.z;
    (m, pos, y)
}

/// Union of the sub mesh bounding boxes (`cMeshEntity::GetBoundingVolume`, no skeleton).
pub fn bounds_union(mins: &[Vector3f], maxs: &[Vector3f]) -> Option<(Vector3f, Vector3f)> {
    let (mut lo, mut hi) = (*mins.first()?, *maxs.first()?);
    for (a, b) in mins.iter().zip(maxs).skip(1) {
        if lo.x > a.x {
            lo.x = a.x
        }
        if hi.x < b.x {
            hi.x = b.x
        }
        if lo.y > a.y {
            lo.y = a.y
        }
        if hi.y < b.y {
            hi.y = b.y
        }
        if lo.z > a.z {
            lo.z = a.z
        }
        if hi.z < b.z {
            hi.z = b.z
        }
    }
    Some((lo, hi))
}

/// `iLight::IsLit`.
pub fn light_is_lit(r: f32, g: f32, b: f32, a: f32, brightness: f32, hpl3: bool) -> bool {
    if hpl3 {
        return (r > 0.0 || g > 0.0 || b > 0.0) && brightness != 0.0;
    }
    r > 0.0 || g > 0.0 || b > 0.0 || a > 0.0
}

/// `iLight::IsVisible`.
pub fn light_is_visible(lit: bool, radius: f32, visible: bool) -> bool {
    lit && radius > 0.0 && visible
}

/// Mask box clamp of `iLight::GetBoundingVolume`: the volume `min`/`max` clipped to the mask box.
pub fn light_mask_bounds(
    min: Vector3f,
    max: Vector3f,
    center: Vector3f,
    size: Vector3f,
) -> (Vector3f, Vector3f) {
    let half = Vector3f {
        x: size.x * 0.5,
        y: size.y * 0.5,
        z: size.z * 0.5,
    };
    let hi = |a: f32, b: f32| if a > b { a } else { b };
    let lo = |a: f32, b: f32| if a < b { a } else { b };
    (
        Vector3f {
            x: hi(min.x, center.x - half.x),
            y: hi(min.y, center.y - half.y),
            z: hi(min.z, center.z - half.z),
        },
        Vector3f {
            x: lo(max.x, center.x + half.x),
            y: lo(max.y, center.y + half.y),
            z: lo(max.z, center.z + half.z),
        },
    )
}

/// Dirty flags and matrix-update counters of `cLightSpot`.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct SpotCtl {
    pub view_proj_count: i32,
    pub view_count: i32,
    pub frustum_count: i32,
    pub projection_updated: bool,
    pub view_proj_updated: bool,
    pub frustum_updated: bool,
}

impl SpotCtl {
    pub const fn new() -> Self {
        Self {
            view_proj_count: -1,
            view_count: -1,
            frustum_count: -1,
            projection_updated: true,
            view_proj_updated: true,
            frustum_updated: true,
        }
    }

    /// `GetViewMatrix`: true when the view matrix is to be recomputed.
    pub fn view_stale(&mut self, transform_count: i32) -> bool {
        let stale = self.view_count != transform_count;
        self.view_count = transform_count;
        stale
    }

    /// `GetProjectionMatrix`: true when the projection is to be recomputed.
    pub fn projection_stale(&mut self) -> bool {
        let stale = self.projection_updated;
        if stale {
            self.projection_updated = false;
            self.view_proj_updated = true;
            self.frustum_updated = true;
        }
        stale
    }

    /// `GetViewProjMatrix` entry check.
    pub fn view_proj_stale(&self, transform_count: i32) -> bool {
        self.view_proj_count != transform_count || self.view_proj_updated || self.projection_updated
    }

    /// `GetViewProjMatrix` after the recompute.
    pub fn view_proj_done(&mut self, transform_count: i32) {
        self.view_proj_count = transform_count;
        self.view_proj_updated = false;
    }

    /// `GetFrustum` entry check.
    pub fn frustum_stale(&self, transform_count: i32) -> bool {
        self.frustum_count != transform_count || self.frustum_updated || self.projection_updated
    }

    /// `GetFrustum` after the rebuild.
    pub fn frustum_done(&mut self, transform_count: i32) {
        self.frustum_updated = false;
        self.frustum_count = transform_count;
    }
}

impl Default for SpotCtl {
    fn default() -> Self {
        Self::new()
    }
}

/// `iLight` shadow-caster cache: object address to its transform update count.
#[derive(Default)]
pub struct ShadowCache {
    map: BTreeMap<usize, i32>,
}

/// One object of a shadow-caster list.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct Caster {
    pub object: usize,
    pub count: i32,
    pub is_static: bool,
}

impl ShadowCache {
    /// `AddShadowCaster`: keeps the count of an object already cached, as `std::map::insert`.
    pub fn add(&mut self, object: usize, count: i32) {
        self.map.entry(object).or_insert(count);
    }

    pub fn is_valid(&self, object: usize, count: i32) -> bool {
        self.map.get(&object) == Some(&count)
    }

    pub fn unchanged(&self, list: &[Caster]) -> bool {
        let mut dynamic = 0usize;
        for c in list.iter().filter(|c| !c.is_static) {
            dynamic += 1;
            if !self.is_valid(c.object, c.count) {
                return false;
            }
        }
        dynamic == self.map.len()
    }

    pub fn set_from(&mut self, list: &[Caster]) {
        self.map.clear();
        for c in list.iter().filter(|c| !c.is_static) {
            self.add(c.object, c.count);
        }
    }

    pub fn clear(&mut self) {
        self.map.clear();
    }

    pub fn len(&self) -> usize {
        self.map.len()
    }
}

/// `cFogArea::SetSize` bounding volume size.
pub fn fog_bounds_size(size: Vector3f) -> Vector3f {
    Vector3f {
        x: size.x,
        y: size.y,
        z: size.x,
    }
}
