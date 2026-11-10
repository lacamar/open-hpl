//! `cDecalCreator` geometry: clipping a mesh's triangles against the decal volume and the
//! decal texture coordinates. Vertex buffers stay in C++; this sees plain arrays.
//!
//! Float expressions mirror the gcc build (`fma` marks a contracted `a*b + c`).

use crate::math::{
    EPSILON, Matrixf, Planef, clamp, cross3, dot3, fma, matrix_mul_3x3, matrix_mul_vec, normalize3,
    plane_dist, sub3, v3,
};
use crate::types::{Vector2l, Vector3f};

#[repr(C)]
pub struct ClipParams {
    pub planes: [Planef; 6],
    pub world: Matrixf,
    pub normal_rot: Matrixf,
    pub offset: f32,
    pub max_tris: i32,
}

pub struct MeshView<'a> {
    pub positions: &'a [f32],
    pub pos_stride: usize,
    pub normals: &'a [f32],
    pub nrm_stride: usize,
    pub indices: &'a [u32],
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Counts {
    pub vertices: i32,
    pub triangles: i32,
}

#[derive(Default)]
pub struct DecalOut {
    pub positions: Vec<Vector3f>,
    pub normals: Vec<Vector3f>,
    pub indices: Vec<u32>,
}

#[derive(Default)]
struct Poly {
    v: Vec<Vector3f>,
    n: Vec<Vector3f>,
}

impl Poly {
    fn push(&mut self, v: Vector3f, n: Vector3f) {
        self.v.push(v);
        self.n.push(n);
    }

    fn clear(&mut self) {
        self.v.clear();
        self.n.clear();
    }

    fn at(&self, i: usize) -> (Vector3f, Vector3f) {
        (
            self.v.get(i).copied().unwrap_or_default(),
            self.n.get(i).copied().unwrap_or_default(),
        )
    }
}

fn lerp(a: Vector3f, b: Vector3f, t: f32) -> Vector3f {
    let s = 1.0 - t;
    v3(
        fma(a.x, s, b.x * t),
        fma(a.y, s, b.y * t),
        fma(a.z, s, b.z * t),
    )
}

fn edge_point(
    plane: &Planef,
    v1: Vector3f,
    n1: Vector3f,
    v2: Vector3f,
    n2: Vector3f,
) -> (Vector3f, Vector3f) {
    let normal = v3(plane.a, plane.b, plane.c);
    let t = plane_dist(plane, v1) / dot3(normal, sub3(v1, v2));
    let mut n = lerp(n1, n2, t);
    normalize3(&mut n);
    (lerp(v1, v2, t), n)
}

fn clip_against_plane(plane: &Planef, src: &Poly, dst: &mut Poly) -> usize {
    dst.clear();
    let len = src.v.len();
    let negative: Vec<bool> = src
        .v
        .iter()
        .map(|&v| plane_dist(plane, v) < EPSILON)
        .collect();
    if negative.iter().filter(|&&b| b).count() == len {
        return 0;
    }
    for i in 0..len {
        let j = if i != 0 { i - 1 } else { len - 1 };
        let neg_i = negative.get(i).copied().unwrap_or(false);
        let neg_j = negative.get(j).copied().unwrap_or(false);
        let (vi, ni) = src.at(i);
        let (vj, nj) = src.at(j);
        if neg_i {
            if !neg_j {
                let (v, n) = edge_point(plane, vj, nj, vi, ni);
                dst.push(v, n);
            }
        } else {
            if neg_j {
                let (v, n) = edge_point(plane, vi, ni, vj, nj);
                dst.push(v, n);
            }
            dst.push(vi, ni);
        }
    }
    dst.v.len()
}

fn clip_polygon(planes: &[Planef; 6], poly: &mut Poly, tmp: &mut Poly) -> usize {
    let mut count = poly.v.len();
    for plane in planes {
        if count == 0 {
            break;
        }
        count = clip_against_plane(plane, poly, tmp);
        std::mem::swap(poly, tmp);
    }
    count
}

fn read3(a: &[f32], base: usize) -> Option<Vector3f> {
    Some(v3(
        *a.get(base)?,
        *a.get(base.checked_add(1)?)?,
        *a.get(base.checked_add(2)?)?,
    ))
}

fn read_vertex(a: &[f32], stride: usize, idx: u32) -> Option<Vector3f> {
    read3(a, (idx as usize).checked_mul(stride)?)
}

/// Clips every front-facing triangle of one sub-mesh and appends the polygons to `out`.
/// Stops at the first polygon that would exceed the triangle limit; an index past the vertex
/// arrays also stops, and a trailing partial triangle is ignored (the C++ read out of bounds).
pub fn clip_mesh(p: &ClipParams, mesh: &MeshView, counts: &mut Counts, out: &mut DecalOut) {
    let up_plane = p.planes.get(2).copied().unwrap_or_default();
    let up = v3(up_plane.a, up_plane.b, up_plane.c);
    let mut poly = Poly::default();
    let mut tmp = Poly::default();
    for tri in mesh.indices.chunks_exact(3) {
        poly.clear();
        for &idx in tri {
            let pos = read_vertex(mesh.positions, mesh.pos_stride, idx);
            let nrm = read_vertex(mesh.normals, mesh.nrm_stride, idx);
            let (Some(pos), Some(nrm)) = (pos, nrm) else {
                return;
            };
            poly.push(pos, nrm);
        }
        let (v0, _) = poly.at(0);
        let (v1, _) = poly.at(1);
        let (v2, _) = poly.at(2);
        let mut tri_normal = cross3(sub3(v2, v0), sub3(v1, v0));
        normalize3(&mut tri_normal);
        if dot3(up, tri_normal) <= EPSILON {
            continue;
        }
        let count = clip_polygon(&p.planes, &mut poly, &mut tmp);
        if count != 0 && !add_polygon(p, &poly, count, counts, out) {
            break;
        }
    }
}

fn add_polygon(
    p: &ClipParams,
    poly: &Poly,
    count: usize,
    counts: &mut Counts,
    out: &mut DecalOut,
) -> bool {
    let first = counts.vertices;
    let tris = (count as i32).wrapping_sub(2);
    if counts.triangles.wrapping_add(tris) > p.max_tris {
        return false;
    }
    counts.triangles = counts.triangles.wrapping_add(tris);
    for i in 0..tris {
        out.indices.push(first as u32);
        out.indices
            .push(first.wrapping_add(i).wrapping_add(1) as u32);
        out.indices
            .push(first.wrapping_add(i).wrapping_add(2) as u32);
    }
    for (&v, &n) in poly.v.iter().zip(&poly.n) {
        let local = v3(
            fma(n.x, p.offset, v.x),
            fma(n.y, p.offset, v.y),
            fma(n.z, p.offset, v.z),
        );
        out.positions.push(matrix_mul_vec(&p.world, local));
        let mut normal = matrix_mul_3x3(&p.normal_rot, n);
        normalize3(&mut normal);
        out.normals.push(normal);
    }
    counts.vertices = first.wrapping_add(count as i32);
    true
}

#[repr(C)]
pub struct UvParams {
    pub position: Vector3f,
    pub right: Vector3f,
    pub forward: Vector3f,
    pub size_x: f32,
    pub size_z: f32,
    pub sub_div: Vector2l,
    pub current: i32,
}

/// Texture coordinates `(u, v, 0)` per vertex. A zero sub-division count crashed the C++
/// (integer divide); here it selects cell 0.
pub fn uvs(
    positions: &[f32],
    stride: usize,
    vertex_num: usize,
    p: &UvParams,
    out: &mut Vec<Vector3f>,
) {
    let inv_w = 1.0 / p.size_x;
    let inv_h = 1.0 / p.size_z;
    let inv_x = 1.0 / p.sub_div.x as f32;
    let inv_y = 1.0 / p.sub_div.y as f32;
    let start_u = p.current.checked_rem(p.sub_div.x).unwrap_or(0) as f32;
    let start_v = p.current.checked_div(p.sub_div.x).unwrap_or(0) as f32;
    for i in 0..vertex_num {
        let point = read_vertex(positions, stride, i as u32).unwrap_or_default();
        let rel = sub3(point, p.position);
        let u = inv_x * (fma(dot3(rel, p.right), inv_w, start_u) + 0.5);
        let v = inv_y * (fma(dot3(rel, p.forward), inv_h, start_v) + 0.5);
        let u = clamp(u, 0.0, 1.0);
        let v = 1.0 - clamp(v, 0.0, 1.0);
        out.push(v3(u, v, 0.0));
    }
}
