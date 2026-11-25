//! `cDecalCreator`: the decal state, setters, clip-volume build, compile and mesh creation.
//! The C++ class is an inline shell around one `Box<Decal>`; clipping and texture coordinates are
//! in `decal.rs`. Vertex buffers, sub-mesh entities, materials and `cMesh` stay C++ behind `GxHost`.

#![allow(clippy::not_unsafe_ptr_arg_deref)] // handles are opaque: only the host table dereferences them

use super::decal::{self, ClipParams, Counts, DecalOut, MeshView, UvParams};
use super::gx_host;
use crate::math::bv::{Bv, bv_intersect};
use crate::math::geom::{orthonormalize_basis, plane_from_normal_point, transform_plane};
use crate::math::{EPSILON, Matrixf, Planef, matrix_inverse, matrix_unit_vectors, transpose, v3};
use crate::scene::ls_host::{self, E_COLOR0, E_NORMAL, E_POS, E_TEX0, Vb};
use crate::types::{Color, Vector2l, Vector3f};
use std::ffi::c_void;
use std::ptr::null_mut;

const POS_COMPS: i32 = 4;
const CREATE_TANGENTS: i32 = 1;

pub struct Decal {
    ll: *mut c_void,
    res: *mut c_void,
    meshes: Vec<*mut c_void>,
    compiled: bool,
    updated: bool,
    bv_updated: bool,
    bv: Bv,
    position: Vector3f,
    up: Vector3f,
    right: Vector3f,
    forward: Vector3f,
    offset: f32,
    size: Vector3f,
    vertex_count: i32,
    triangle_count: i32,
    max_tris: i32,
    clip_planes: Vec<Planef>,
    material_name: String,
    sub_div: Vector2l,
    current_sub_div: i32,
    color: Color,
    material: *mut c_void,
    vb: Vb,
}

fn rotation(m: &Matrixf) -> Matrixf {
    let r = &m.m;
    Matrixf {
        m: [
            [r[0][0], r[0][1], r[0][2], 0.0],
            [r[1][0], r[1][1], r[1][2], 0.0],
            [r[2][0], r[2][1], r[2][2], 0.0],
            [0.0, 0.0, 0.0, 1.0],
        ],
    }
}

use crate::math::normalized;

impl Decal {
    pub fn new(ll: *mut c_void, res: *mut c_void) -> Decal {
        let zero = v3(0.0, 0.0, 0.0);
        Decal {
            ll,
            res,
            meshes: Vec::new(),
            compiled: false,
            updated: true,
            bv_updated: true,
            bv: Bv::new(),
            position: zero,
            up: v3(0.0, 1.0, 0.0),
            right: v3(1.0, 0.0, 0.0),
            forward: v3(0.0, 0.0, 1.0),
            offset: 0.0,
            size: v3(1.0, 1.0, 1.0),
            vertex_count: 0,
            triangle_count: 0,
            max_tris: 250,
            clip_planes: Vec::new(),
            material_name: String::new(),
            sub_div: Vector2l { x: 1, y: 1 },
            current_sub_div: 0,
            color: Color {
                r: 1.0,
                g: 1.0,
                b: 1.0,
                a: 1.0,
            },
            material: null_mut(),
            vb: Vb(null_mut()),
        }
    }

    pub fn add_submesh(&mut self, sub: *mut c_void) -> bool {
        if sub.is_null() {
            return false;
        }
        let Some(view) = gx_host::sub_view(sub) else {
            return false;
        };
        // SAFETY: the sub-mesh entity owns its bounding volume.
        let mesh_bv = unsafe { &mut *view.bv.cast::<Bv>() };
        let decal_bv = self.decal_bv();
        mesh_bv.update_size();
        decal_bv.update_size();
        if bv_intersect(mesh_bv, decal_bv) {
            self.add_affected(sub);
            return true;
        }
        false
    }

    pub fn add_affected(&mut self, sub: *mut c_void) {
        if sub.is_null() {
            return;
        }
        self.meshes.push(sub);
        self.updated = true;
    }

    pub fn clear_meshes(&mut self) {
        self.meshes.clear();
        self.updated = true;
    }

    pub fn set_max_tris(&mut self, n: i32) {
        self.max_tris = n;
    }

    pub fn set_position(&mut self, p: Vector3f) {
        if self.position == p {
            return;
        }
        self.position = p;
        self.updated = true;
        self.bv_updated = true;
    }

    fn basis_changed(&mut self, compute: bool) {
        if compute {
            let [up, right, forward] = orthonormalize_basis(self.up, self.right, self.forward);
            self.up = up;
            self.right = right;
            self.forward = forward;
        }
        self.updated = true;
        self.bv_updated = true;
    }

    pub fn set_up(&mut self, v: Vector3f, compute: bool) {
        if self.up == v {
            return;
        }
        self.up = normalized(v);
        self.basis_changed(compute);
    }

    pub fn set_right(&mut self, v: Vector3f, compute: bool) {
        if self.right == v {
            return;
        }
        self.right = normalized(v);
        self.basis_changed(compute);
    }

    pub fn set_forward(&mut self, v: Vector3f, compute: bool) {
        if self.forward == v {
            return;
        }
        self.forward = normalized(v);
        self.basis_changed(compute);
    }

    pub fn set_offset(&mut self, o: f32) {
        if self.offset != o {
            self.offset = o;
            self.updated = true;
        }
    }

    pub fn set_size(&mut self, s: Vector3f) {
        if self.size != s {
            self.size = s;
            self.updated = true;
        }
    }

    pub fn set_material(&mut self, name: &str) {
        if self.material_name == name {
            return;
        }
        let created = gx_host::mat_create(self.res, name);
        if !created.is_null() {
            if self.material == created {
                ls_host::mat_destroy(self.res as usize, created as usize);
            } else {
                ls_host::mat_destroy(self.res as usize, self.material as usize);
                self.material = created;
                self.material_name = name.to_owned();
            }
        } else {
            ls_host::mat_destroy(self.res as usize, self.material as usize);
            self.material = null_mut();
            self.material_name.clear();
        }
        self.updated = true;
    }

    pub fn set_color(&mut self, c: Color) {
        if self.color != c {
            self.color = c;
            self.updated = true;
        }
    }

    pub fn set_sub_div(&mut self, s: Vector2l) {
        if self.sub_div != s {
            self.sub_div = s;
            self.updated = true;
        }
    }

    pub fn set_current_sub_div(&mut self, i: i32) {
        if self.current_sub_div != i {
            self.current_sub_div = i;
            self.updated = true;
        }
    }

    pub fn is_updated(&self) -> bool {
        self.updated
    }

    pub fn material(&self) -> *mut c_void {
        self.material
    }

    fn decal_bv(&mut self) -> &mut Bv {
        if self.bv_updated {
            self.bv_updated = false;
            let basis = matrix_unit_vectors(self.right, self.up, self.forward, v3(0.0, 0.0, 0.0));
            self.bv.set_transform(&matrix_inverse(&transpose(&basis)));
            self.bv.set_position(self.position);
            self.bv.set_size(self.size);
        }
        &mut self.bv
    }

    fn build_planes(&mut self) {
        let half = v3(self.size.x * 0.5, self.size.y * 0.5, self.size.z * 0.5);
        let axes = [
            (self.right, half.x),
            (self.up, half.y),
            (self.forward, half.z),
        ];
        self.clip_planes.clear();
        for (axis, extent) in axes {
            for sign in [1.0f32, -1.0] {
                let add = v3(axis.x * extent, axis.y * extent, axis.z * extent);
                let normal = v3(axis.x * sign, axis.y * sign, axis.z * sign);
                let point = v3(
                    self.position.x - add.x * sign,
                    self.position.y - add.y * sign,
                    self.position.z - add.z * sign,
                );
                self.clip_planes
                    .push(plane_from_normal_point(normal, point));
            }
        }
    }

    pub fn compile(&mut self) -> bool {
        if !self.updated {
            return self.compiled;
        }
        self.compiled = false;
        if self.meshes.is_empty() || self.size.x.abs() < EPSILON || self.size.y.abs() < EPSILON {
            return false;
        }
        self.updated = false;
        self.vertex_count = 0;
        self.triangle_count = 0;
        self.build_planes();
        self.vb.free();
        self.vb = gx_host::vb_create(self.ll);
        self.vb.elem(E_POS, POS_COMPS);
        self.vb.elem(E_COLOR0, 4);
        self.vb.elem(E_TEX0, 3);
        self.vb.elem(E_NORMAL, 3);
        for i in 0..self.meshes.len() {
            if let Some(&sub) = self.meshes.get(i) {
                self.clip_mesh(sub);
            }
        }
        let vtx_num = gx_host::vb_vertex_num(self.vb);
        let count = usize::try_from(vtx_num).unwrap_or(0);
        let stride = POS_COMPS as usize;
        let params = UvParams {
            position: self.position,
            right: self.right,
            forward: self.forward,
            size_x: self.size.x,
            size_z: self.size.z,
            sub_div: self.sub_div,
            current: self.current_sub_div,
        };
        let mut uvs = Vec::with_capacity(count);
        // SAFETY: the buffer holds `count` positions of `stride` floats.
        let positions: &[f32] = unsafe { self.vb.floats(E_POS, count * stride) };
        decal::uvs(positions, stride, count, &params, &mut uvs);
        for uv in &uvs {
            self.vb.add(E_TEX0, &[uv.x, uv.y, uv.z]);
        }
        self.compiled = gx_host::vb_compile(self.vb, CREATE_TANGENTS);
        self.compiled
    }

    fn clip_mesh(&mut self, sub: *mut c_void) {
        let Some(view) = gx_host::sub_view(sub) else {
            return;
        };
        let inverse = matrix_inverse(&view.world);
        let mut planes = [Planef::default(); 6];
        for (dst, src) in planes.iter_mut().zip(&self.clip_planes) {
            *dst = transform_plane(&inverse, src);
        }
        let clip = ClipParams {
            planes,
            world: view.world,
            normal_rot: transpose(&rotation(&inverse)),
            offset: self.offset,
            max_tris: self.max_tris,
        };
        let mesh = MeshView {
            positions: gx_host::floats(view.pos, view.pos_len),
            pos_stride: view.pos_stride as usize,
            normals: gx_host::floats(view.nrm, view.nrm_len),
            nrm_stride: view.nrm_stride as usize,
            indices: gx_host::indices(view.idx, view.idx_num),
        };
        let mut counts = Counts {
            vertices: self.vertex_count,
            triangles: self.triangle_count,
        };
        let mut out = DecalOut::default();
        decal::clip_mesh(&clip, &mesh, &mut counts, &mut out);
        self.vertex_count = counts.vertices;
        self.triangle_count = counts.triangles;
        for &i in &out.indices {
            self.vb.index(i as i32);
        }
        for (p, n) in out.positions.iter().zip(&out.normals) {
            self.vb.add(E_POS, &[p.x, p.y, p.z]);
            self.vb.add(E_NORMAL, &[n.x, n.y, n.z]);
            self.vb.color(E_COLOR0, self.color);
        }
    }

    pub fn can_create(&mut self) -> bool {
        self.compile()
    }

    pub fn create_mesh(&mut self) -> *mut c_void {
        if !self.can_create() {
            return null_mut();
        }
        let mat = gx_host::mat_create(self.res, &self.material_name);
        if mat.is_null() {
            return null_mut();
        }
        gx_host::make_mesh(self.res, self.vb, mat)
    }
}

impl Drop for Decal {
    fn drop(&mut self) {
        self.vb.free();
        ls_host::mat_destroy(self.res as usize, self.material as usize);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn basis_orthonormalizes_and_flags() {
        let mut d = Decal::new(null_mut(), null_mut());
        d.updated = false;
        d.set_up(v3(0.0, 2.0, 0.0), true);
        assert!(d.updated && d.bv_updated);
        assert_eq!(d.up, v3(0.0, 1.0, 0.0));
        d.updated = false;
        d.set_up(v3(0.0, 1.0, 0.0), true);
        assert!(!d.updated);
    }

    #[test]
    fn planes_face_inward() {
        let mut d = Decal::new(null_mut(), null_mut());
        d.size = v3(2.0, 4.0, 6.0);
        d.position = v3(1.0, 0.0, 0.0);
        d.build_planes();
        assert_eq!(d.clip_planes.len(), 6);
        let (p0, p1) = (d.clip_planes[0], d.clip_planes[1]);
        assert_eq!((p0.a, p0.d), (1.0, -(1.0 - 1.0)));
        assert_eq!((p1.a, p1.d), (-1.0, 1.0 + 1.0));
    }
}
