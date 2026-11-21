//! `cSubMesh` and `cMesh` state (stage MM). The C++ classes hold one box pointer each and read the
//! `repr(C)` prefix (`SubPub`) in place; everything that touches a still-C++ object (material
//! manager, vertex buffer, skeleton, animation, node, mesh name, log) goes through `Host`, which
//! `RustMeshMm.cpp` fills and which goes away with those classes.

use core::ffi::c_void;

use super::animdata::MeshInfo;
use super::submesh::{self, VertexBonePair};
use crate::math::IDENTITY;
use crate::types::{Matrixf, Vector3f};

pub type P = *mut c_void;

/// `iVertexBuffer` facts the sub-mesh and mesh passes read.
#[repr(C)]
pub struct VbView {
    pub index_num: usize,
    pub indices: *const u32,
    pub vertex_num: usize,
    pub positions: *const f32,
    pub stride: i32,
}

/// Thunks over C++ objects. Field order is mirrored by `OhplMmHost` in `RustMeshMm.h`.
#[repr(C)]
pub struct Host {
    pub vars_delete: unsafe extern "C" fn(P),
    pub texture_destroy: unsafe extern "C" fn(P, P),
    pub material_destroy: unsafe extern "C" fn(P, P),
    pub vb_delete: unsafe extern "C" fn(P),
    pub vb_view: unsafe extern "C" fn(P, *mut VbView),
    pub warning: unsafe extern "C" fn(*const u8, usize),
    pub mesh_name: unsafe extern "C" fn(P, *mut *const u8, *mut usize),
    pub sub_delete: unsafe extern "C" fn(P),
    pub skeleton_bone_num: unsafe extern "C" fn(P) -> i32,
    pub skeleton_bind_pos: unsafe extern "C" fn(P, i32, *mut Vector3f),
    pub skeleton_delete: unsafe extern "C" fn(P),
    pub animation_delete: unsafe extern "C" fn(P),
    pub node_new: unsafe extern "C" fn() -> P,
    pub node_delete: unsafe extern "C" fn(P),
    pub node_name: unsafe extern "C" fn(P, *mut *const u8, *mut usize),
    pub shape_box: unsafe extern "C" fn(P, *const Vector3f, *mut Matrixf) -> P,
    pub shape_sphere: unsafe extern "C" fn(P, *const Vector3f, *mut Matrixf) -> P,
    pub shape_cylinder: unsafe extern "C" fn(P, f32, f32, *mut Matrixf) -> P,
    pub shape_capsule: unsafe extern "C" fn(P, f32, f32, *mut Matrixf) -> P,
    pub shape_compound: unsafe extern "C" fn(P, *const P, usize) -> P,
}

pub(crate) fn name_of(f: unsafe extern "C" fn(P, *mut *const u8, *mut usize), p: P) -> Vec<u8> {
    let (mut ptr, mut len) = (core::ptr::null::<u8>(), 0usize);
    // SAFETY: the thunk writes a pointer into a live string and its length.
    unsafe { f(p, &mut ptr, &mut len) };
    if ptr.is_null() {
        return Vec::new();
    }
    // SAFETY: the thunk returned `len` readable bytes valid until the next engine call.
    unsafe { core::slice::from_raw_parts(ptr, len) }.to_vec()
}

fn view(host: &Host, vb: P) -> VbView {
    let mut v = VbView {
        index_num: 0,
        indices: core::ptr::null(),
        vertex_num: 0,
        positions: core::ptr::null(),
        stride: 0,
    };
    // SAFETY: `vb` is a live iVertexBuffer; the thunk fills `v`.
    unsafe { (host.vb_view)(vb, &mut v) };
    v
}

fn positions<'a>(v: &VbView) -> &'a [f32] {
    let n = v
        .vertex_num
        .saturating_mul(usize::try_from(v.stride).unwrap_or(0));
    if v.positions.is_null() || n == 0 {
        &[]
    } else {
        // SAFETY: the buffer holds `vertex_num * stride` floats.
        unsafe { core::slice::from_raw_parts(v.positions, n) }
    }
}

fn indices<'a>(v: &VbView) -> &'a [u32] {
    if v.indices.is_null() || v.index_num == 0 {
        &[]
    } else {
        // SAFETY: the buffer holds `index_num` indices.
        unsafe { core::slice::from_raw_parts(v.indices, v.index_num) }
    }
}

/// Mirror of `cMeshCollider`.
#[repr(C)]
pub struct Collider {
    pub kind: i32,
    pub size: Vector3f,
    pub offset: Matrixf,
    pub char_collider: bool,
}

const SHAPE_BOX: i32 = 1;
const SHAPE_SPHERE: i32 = 2;
const SHAPE_CYLINDER: i32 = 3;
const SHAPE_CAPSULE: i32 = 4;

/// `cSubMesh::CreateCollideShapeFromCollider`; a null `offset` takes the collider's own.
pub(crate) fn collide_shape(
    host: &Host,
    c: &mut Collider,
    world: P,
    mul: Vector3f,
    offset: *mut Matrixf,
) -> P {
    let off = if offset.is_null() {
        &raw mut c.offset
    } else {
        offset
    };
    let size = Vector3f {
        x: c.size.x * mul.x,
        y: c.size.y * mul.y,
        z: c.size.z * mul.z,
    };
    // SAFETY: `world` is a live iPhysicsWorld and `off` a live matrix.
    unsafe {
        match c.kind {
            SHAPE_BOX => (host.shape_box)(world, &size, off),
            SHAPE_SPHERE => (host.shape_sphere)(world, &size, off),
            SHAPE_CYLINDER => (host.shape_cylinder)(world, size.x, size.y, off),
            SHAPE_CAPSULE => (host.shape_capsule)(world, size.x, size.y, off),
            _ => core::ptr::null_mut(),
        }
    }
}

/// Fields the C++ `cSubMesh` reads in place (`OhplMmSub`).
#[repr(C)]
pub struct SubPub {
    pub material: P,
    pub vtx: P,
    pub weights: *mut f32,
    pub bones: *mut u8,
    pub local: Matrixf,
    pub model_scale: Vector3f,
    pub one_sided_normal: Vector3f,
    pub one_sided_point: Vector3f,
    pub double_sided: bool,
    pub collide_shape: bool,
    pub one_sided: bool,
}

#[repr(C)]
pub struct Sub {
    pub p: SubPub,
    host: &'static Host,
    name: Vec<u8>,
    manager: P,
    parent: P,
    pairs: Vec<VertexBonePair>,
    #[allow(clippy::vec_box, reason = "collider addresses are handed to C++")]
    colliders: Vec<Box<Collider>>,
    weights: Vec<f32>,
    bones: Vec<u8>,
}

impl Sub {
    pub fn new(host: &'static Host, manager: P, name: &[u8]) -> Self {
        let zero = Vector3f::default();
        Self {
            p: SubPub {
                material: core::ptr::null_mut(),
                vtx: core::ptr::null_mut(),
                weights: core::ptr::null_mut(),
                bones: core::ptr::null_mut(),
                local: IDENTITY,
                model_scale: zero,
                one_sided_normal: zero,
                one_sided_point: zero,
                double_sided: false,
                collide_shape: false,
                one_sided: false,
            },
            host,
            name: name.to_vec(),
            manager,
            parent: core::ptr::null_mut(),
            pairs: Vec::new(),
            colliders: Vec::new(),
            weights: Vec::new(),
            bones: Vec::new(),
        }
    }

    pub fn set_parent(&mut self, parent: P) {
        self.parent = parent;
    }

    pub fn set_material(&mut self, m: P) {
        if !self.p.material.is_null() {
            // SAFETY: the manager and material are live engine objects.
            unsafe { (self.host.material_destroy)(self.manager, self.p.material) };
        }
        self.p.material = m;
    }

    pub fn set_vertex_buffer(&mut self, vb: P) {
        self.p.vtx = vb;
    }

    pub fn pairs(&self) -> &[VertexBonePair] {
        &self.pairs
    }

    pub fn pair_mut(&mut self, i: usize) -> Option<&mut VertexBonePair> {
        self.pairs.get_mut(i)
    }

    pub fn resize_pairs(&mut self, n: usize) {
        self.pairs.resize(n, VertexBonePair::default());
    }

    pub fn add_pair(&mut self, p: VertexBonePair) {
        self.pairs.push(p);
    }

    pub fn clear_pairs(&mut self) {
        self.pairs.clear();
    }

    pub fn create_collider(&mut self, kind: i32) -> *mut Collider {
        self.colliders.push(Box::new(Collider {
            kind,
            size: Vector3f::default(),
            offset: Matrixf::default(),
            char_collider: false,
        }));
        self.colliders
            .last_mut()
            .map_or(core::ptr::null_mut(), |c| &raw mut **c)
    }

    pub fn collider(&mut self, i: usize) -> *mut Collider {
        self.colliders
            .get_mut(i)
            .map_or(core::ptr::null_mut(), |c| &raw mut **c)
    }

    pub fn collider_count(&self) -> usize {
        self.colliders.len()
    }

    /// `cSubMesh::CreateCollideShape`
    pub(crate) fn create_collide_shape(&mut self, world: P) -> P {
        let one = Vector3f {
            x: 1.0,
            y: 1.0,
            z: 1.0,
        };
        let host = self.host;
        let null = core::ptr::null_mut::<Matrixf>();
        match self.colliders.as_mut_slice() {
            [] => core::ptr::null_mut(),
            [c] => collide_shape(host, c, world, one, null),
            all => {
                let shapes: Vec<P> = all
                    .iter_mut()
                    .map(|c| collide_shape(host, c, world, one, null))
                    .collect();
                // SAFETY: the thunk reads `shapes.len()` pointers.
                unsafe { (host.shape_compound)(world, shapes.as_ptr(), shapes.len()) }
            }
        }
    }

    pub fn compile(&mut self) {
        self.check_one_sided();
        self.compile_bone_pairs();
    }

    fn check_one_sided(&mut self) {
        if self.p.vtx.is_null() {
            return;
        }
        let v = view(self.host, self.p.vtx);
        if v.index_num > 400 * 3 {
            return;
        }
        if let Some((n, p)) = submesh::one_sided(indices(&v), positions(&v), v.stride) {
            self.p.one_sided = true;
            self.p.one_sided_normal = n;
            self.p.one_sided_point = p;
        }
    }

    fn compile_bone_pairs(&mut self) {
        if self.pairs.is_empty() || self.p.vtx.is_null() {
            return;
        }
        let n = view(self.host, self.p.vtx).vertex_num;
        self.weights = vec![0.0; n.saturating_mul(4)];
        self.bones = vec![0; n.saturating_mul(4)];
        let flags = submesh::compile_bone_pairs(&self.pairs, n, &mut self.weights, &mut self.bones);
        self.p.weights = self.weights.as_mut_ptr();
        self.p.bones = self.bones.as_mut_ptr();
        let mesh = name_of(self.host.mesh_name, self.parent);
        for (bit, head, tail) in [
            (
                1,
                &b"More than 4 bones on a vertex in submesh '"[..],
                &b"' !\n"[..],
            ),
            (
                2,
                &b"Some vertices in sub mesh '"[..],
                &b"' are not connected to a bone!\n"[..],
            ),
        ] {
            if flags & bit == 0 {
                continue;
            }
            let msg = [head, &self.name, &b"' in mesh '"[..], &mesh, tail].concat();
            // SAFETY: the thunk reads `msg.len()` bytes.
            unsafe { (self.host.warning)(msg.as_ptr(), msg.len()) };
        }
    }
}

impl Drop for Sub {
    fn drop(&mut self) {
        if !self.p.material.is_null() {
            // SAFETY: the manager and material are live engine objects.
            unsafe { (self.host.material_destroy)(self.manager, self.p.material) };
        }
        if !self.p.vtx.is_null() {
            // SAFETY: the sub-mesh owns its vertex buffer.
            unsafe { (self.host.vb_delete)(self.p.vtx) };
        }
    }
}

pub struct Mesh {
    host: &'static Host,
    owner: P,
    manager: P,
    info: MeshInfo,
    subs: Vec<(P, *mut Sub)>,
    skeleton: P,
    anims: Vec<P>,
    radii: Vec<f32>,
    root: P,
    nodes: Vec<P>,
}

impl Mesh {
    pub fn new(host: &'static Host, owner: P, manager: P) -> Self {
        // SAFETY: the thunk allocates a node the mesh then owns.
        let root = unsafe { (host.node_new)() };
        Self {
            host,
            owner,
            manager,
            info: MeshInfo::default(),
            subs: Vec::new(),
            skeleton: core::ptr::null_mut(),
            anims: Vec::new(),
            radii: Vec::new(),
            root,
            nodes: Vec::new(),
        }
    }

    pub fn manager(&self) -> P {
        self.manager
    }

    pub fn owner(&self) -> P {
        self.owner
    }

    pub(crate) fn add_sub(&mut self, shell: P, sub: *mut Sub, name: &[u8]) {
        // SAFETY: `sub` is the live box of `shell`.
        if let Some(s) = unsafe { sub.as_mut() } {
            s.set_parent(self.owner);
        }
        self.subs.push((shell, sub));
        self.info.add_sub(name);
    }

    pub fn sub(&self, i: usize) -> P {
        self.subs.get(i).map_or(core::ptr::null_mut(), |s| s.0)
    }

    pub fn sub_count(&self) -> usize {
        self.subs.len()
    }

    pub fn sub_index(&self, name: &[u8]) -> i32 {
        self.info.sub_index(name)
    }

    pub fn sub_by_name(&self, name: &[u8]) -> P {
        usize::try_from(self.info.sub_by_name(name)).map_or(core::ptr::null_mut(), |i| self.sub(i))
    }

    pub fn triangle_count(&self) -> i32 {
        let mut n = 0usize;
        for (_, s) in &self.subs {
            // SAFETY: subs are live while the mesh is.
            if let Some(s) = unsafe { s.as_ref() }
                && !s.p.vtx.is_null()
            {
                n += view(self.host, s.p.vtx).index_num.wrapping_div(3);
            }
        }
        i32::try_from(n).unwrap_or(i32::MAX)
    }

    pub fn set_skeleton(&mut self, s: P) {
        self.skeleton = s;
    }

    pub fn skeleton(&self) -> P {
        self.skeleton
    }

    pub fn add_animation(&mut self, a: P, name: &[u8]) {
        self.anims.push(a);
        self.info.add_animation(name);
    }

    pub fn animation(&self, i: usize) -> P {
        self.anims.get(i).copied().unwrap_or(core::ptr::null_mut())
    }

    pub fn animation_count(&self) -> usize {
        self.anims.len()
    }

    pub fn animation_index(&self, name: &[u8]) -> i32 {
        self.info.animation_index(name)
    }

    pub fn clear_animations(&mut self, delete_all: bool) {
        if delete_all {
            for a in &self.anims {
                // SAFETY: the mesh owns its animations.
                unsafe { (self.host.animation_delete)(*a) };
            }
        }
        self.anims.clear();
        self.info.clear_animations();
    }

    pub fn radius(&self, i: usize) -> f32 {
        self.radii.get(i).copied().unwrap_or(0.0)
    }

    pub fn root(&self) -> P {
        self.root
    }

    pub fn add_node(&mut self, n: P) {
        self.nodes.push(n);
    }

    pub fn node_count(&self) -> usize {
        self.nodes.len()
    }

    pub fn node(&self, i: usize) -> P {
        self.nodes.get(i).copied().unwrap_or(core::ptr::null_mut())
    }

    pub fn node_by_name(&self, name: &[u8]) -> P {
        self.nodes
            .iter()
            .copied()
            .find(|n| name_of(self.host.node_name, *n) == name)
            .unwrap_or(core::ptr::null_mut())
    }

    /// `CompileBonesAndSubMeshes`
    pub fn compile_bones(&mut self) {
        if self.skeleton.is_null() {
            return;
        }
        // SAFETY: the skeleton is live.
        let bones =
            usize::try_from(unsafe { (self.host.skeleton_bone_num)(self.skeleton) }).unwrap_or(0);
        self.radii.resize(bones, 0.0);
        let mut bind = vec![Vector3f::default(); bones];
        for (i, b) in bind.iter_mut().enumerate() {
            // SAFETY: `i < bones`; the thunk writes one vector.
            unsafe {
                (self.host.skeleton_bind_pos)(self.skeleton, i32::try_from(i).unwrap_or(0), b);
            }
        }
        for (_, s) in &self.subs {
            // SAFETY: subs are live while the mesh is.
            let Some(s) = (unsafe { s.as_ref() }) else {
                continue;
            };
            if s.p.vtx.is_null() {
                continue;
            }
            let v = view(self.host, s.p.vtx);
            submesh::bone_radii(&bind, &mut self.radii, s.pairs(), positions(&v), v.stride);
        }
    }
}

impl Drop for Mesh {
    fn drop(&mut self) {
        for (shell, _) in &self.subs {
            // SAFETY: the mesh owns its sub-mesh shells.
            unsafe { (self.host.sub_delete)(*shell) };
        }
        if !self.skeleton.is_null() {
            // SAFETY: the mesh owns its skeleton.
            unsafe { (self.host.skeleton_delete)(self.skeleton) };
        }
        for a in &self.anims {
            // SAFETY: the mesh owns its animations.
            unsafe { (self.host.animation_delete)(*a) };
        }
        if !self.root.is_null() {
            // SAFETY: the mesh owns its root node.
            unsafe { (self.host.node_delete)(self.root) };
        }
    }
}
