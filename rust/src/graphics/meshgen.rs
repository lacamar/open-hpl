//! `cMeshCreator` shape generators. Float expressions mirror the gcc build (see `math`).

use crate::math::{asin, clamp, cos, fma, normalize3, round_float_to_decimals, sin, sub3, v3};
use crate::types::{Vector2f, Vector3f};

const PI: f32 = std::f32::consts::PI;
const TWO_PI: f32 = std::f32::consts::TAU;

#[derive(Debug, Default, Clone, PartialEq)]
pub struct MeshData {
    pub positions: Vec<Vector3f>,
    pub normals: Vec<Vector3f>,
    pub texcoords: Vec<Vector3f>,
    pub indices: Vec<u32>,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PlaneError {
    Coincident,
    NotCoplanar,
}

const ZERO: Vector3f = v3(0.0, 0.0, 0.0);

fn add3(a: Vector3f, b: Vector3f) -> Vector3f {
    v3(a.x + b.x, a.y + b.y, a.z + b.z)
}

fn mul3v(a: Vector3f, b: Vector3f) -> Vector3f {
    v3(a.x * b.x, a.y * b.y, a.z * b.z)
}

fn at<T: Copy + Default>(a: &[T], i: usize) -> T {
    a.get(i).copied().unwrap_or_default()
}

fn comp(v: Vector3f, axis: i32) -> f32 {
    match axis {
        0 => v.x,
        1 => v.y,
        _ => v.z,
    }
}

fn set_comp(v: &mut Vector3f, axis: i32, f: f32) {
    match axis {
        0 => v.x = f,
        1 => v.y = f,
        _ => v.z = f,
    }
}

impl MeshData {
    fn vertex(&mut self, pos: Vector3f, normal: Vector3f) {
        self.positions.push(pos);
        self.normals.push(normal);
        self.texcoords.push(ZERO);
    }

    fn vertex_count(&self) -> i32 {
        self.positions.len() as i32
    }

    fn index(&mut self, i: i32) {
        self.indices.push(i as u32);
    }

    fn normalized_vertex(&mut self, pos: Vector3f) {
        let mut n = pos;
        normalize3(&mut n);
        self.vertex(pos, n);
    }

    fn wrap_sides(&mut self, start: i32, sections: i32) {
        let mut i = 0i32;
        while i < sections.wrapping_sub(1) {
            let p0 = start.wrapping_add(i);
            let p1 = p0.wrapping_add(1);
            let p2 = p0.wrapping_add(sections);
            let p3 = p2.wrapping_add(1);
            for p in [p0, p2, p1, p1, p2, p3] {
                self.index(p);
            }
            i += 1;
        }
        let p0 = start.wrapping_add(sections).wrapping_sub(1);
        let p1 = start;
        let p2 = p0.wrapping_add(sections);
        let p3 = p0.wrapping_add(1);
        for p in [p0, p2, p1, p1, p2, p3] {
            self.index(p);
        }
    }

    fn wrap_upper_cap(&mut self, center: i32, start: i32, sections: i32) {
        let mut i = 0i32;
        while i < sections.wrapping_sub(1) {
            let base = start.wrapping_add(i);
            for p in [center, base, base.wrapping_add(1)] {
                self.index(p);
            }
            i += 1;
        }
        for p in [center, start.wrapping_add(sections).wrapping_sub(1), start] {
            self.index(p);
        }
    }

    fn wrap_lower_cap(&mut self, center: i32, start: i32, sections: i32) {
        let mut i = 0i32;
        while i < sections.wrapping_sub(1) {
            let base = start.wrapping_add(i);
            for p in [center, base.wrapping_add(1), base] {
                self.index(p);
            }
            i += 1;
        }
        for p in [center, start, start.wrapping_add(sections).wrapping_sub(1)] {
            self.index(p);
        }
    }

    fn last_cap(&mut self, sections: i32) {
        let last = self.vertex_count().wrapping_sub(1);
        self.wrap_lower_cap(last, last.wrapping_sub(sections), sections);
    }
}

/// A non-positive step makes the C++ loop run until memory is exhausted (or not at all for
/// infinity); this yields nothing for it.
fn circumference(radius: f32, step: f32, height: f32) -> Vec<Vector3f> {
    let mut out = Vec::new();
    let end = TWO_PI - step;
    let mut angle = 0.0f32;
    while angle <= end && step > 0.0 {
        out.push(v3(
            round_float_to_decimals(radius * cos(angle), 6),
            round_float_to_decimals(height, 6),
            round_float_to_decimals(radius * sin(angle), 6),
        ));
        let next = angle + step;
        if next == angle {
            break;
        }
        angle = next;
    }
    out
}

fn pole(m: &mut MeshData, y: f32, ny: f32) {
    m.vertex(v3(0.0, y, 0.0), v3(0.0, ny, 0.0));
}

pub fn sphere(radius: f32, sections: i32, slices: i32) -> MeshData {
    let mut m = MeshData::default();
    let inv_slices = 1.0f32 / slices as f32;
    let section_step = TWO_PI / sections as f32;
    pole(&mut m, radius, 1.0);
    let pi_over_slices = PI * inv_slices;
    let half_pi = PI * 0.5f32;
    let mut i = 0i32;
    while i < slices.wrapping_sub(1) {
        let arg = fma(pi_over_slices, i.wrapping_add(1) as f32, -half_pi);
        let height = fma(-radius, 1.0f32 + sin(arg), radius);
        let angle = asin(height / radius);
        for v in circumference(radius * cos(angle), section_step, height) {
            m.normalized_vertex(v);
        }
        i += 1;
    }
    pole(&mut m, -radius, -1.0);
    m.wrap_upper_cap(0, 1, sections);
    let mut i = 0i32;
    while i < slices.wrapping_sub(2) {
        m.wrap_sides(1i32.wrapping_add(i.wrapping_mul(sections)), sections);
        i += 1;
    }
    m.last_cap(sections);
    m
}

pub fn cylinder(size: Vector2f, sections: i32) -> MeshData {
    let mut m = MeshData::default();
    let step = TWO_PI / sections as f32;
    let half = size.y * 0.5f32;
    pole(&mut m, half, 1.0);
    let ring = circumference(size.x, step, 0.0);
    for h in [half, -half] {
        for v in &ring {
            m.normalized_vertex(v3(v.x, h, v.z));
        }
    }
    pole(&mut m, -half, -1.0);
    m.wrap_upper_cap(0, 1, sections);
    m.wrap_sides(1, sections);
    m.last_cap(sections);
    m
}

pub fn capsule(size: Vector2f, sections: i32, slices: i32) -> MeshData {
    let mut m = MeshData::default();
    let radius = size.x;
    let half = size.y * 0.5f32;
    let cyl_half = clamp(half - radius, 0.0, half - radius);
    let inv_slices = 1.0f32 / slices as f32;
    let section_step = TWO_PI / sections as f32;
    let top = if half > radius { half } else { radius };
    pole(&mut m, top, 1.0);
    let half_pi = PI * 0.5f32;
    let half_pi_over_slices = half_pi * inv_slices;
    let dome = |i: i32| {
        let arg = fma(half_pi_over_slices, i.wrapping_add(1) as f32, -half_pi);
        let height = fma(-radius, 1.0f32 + sin(arg), cyl_half + radius);
        let angle = asin((height - cyl_half) / radius);
        (height, angle)
    };
    let dome_vertex = |m: &mut MeshData, v: Vector3f| {
        let mut n = v3(v.x - cyl_half, v.y - cyl_half, v.z - cyl_half);
        normalize3(&mut n);
        m.vertex(v, n);
    };
    let mut i = 0i32;
    while i < slices.wrapping_sub(1) {
        let (height, angle) = dome(i);
        for v in circumference(size.x * cos(angle), section_step, height) {
            dome_vertex(&mut m, v);
        }
        i += 1;
    }
    let ring = circumference(size.x, section_step, 0.0);
    for h in [cyl_half, -cyl_half] {
        for v in &ring {
            m.normalized_vertex(v3(v.x, h, v.z));
        }
    }
    let mut i = slices.wrapping_sub(2);
    while i >= 0 {
        let (height, angle) = dome(i);
        for v in circumference(size.x * cos(angle), section_step, -height) {
            dome_vertex(&mut m, v);
        }
        i -= 1;
    }
    let bottom = if -half < -radius { -half } else { -radius };
    pole(&mut m, bottom, -1.0);
    m.wrap_upper_cap(0, 1, sections);
    let mut i = 0i32;
    while i < slices.wrapping_mul(2).wrapping_sub(1) {
        m.wrap_sides(1i32.wrapping_add(i.wrapping_mul(sections)), sections);
        i += 1;
    }
    m.last_cap(sections);
    m
}

pub fn cone(size: Vector2f, sections: i32) -> MeshData {
    let mut m = MeshData::default();
    let step = TWO_PI / sections as f32;
    let half = size.y * 0.5f32;
    pole(&mut m, half, 1.0);
    for v in circumference(size.x, step, -half) {
        m.normalized_vertex(v);
    }
    pole(&mut m, -half, -1.0);
    m.wrap_upper_cap(0, 1, sections);
    m.last_cap(sections);
    m
}

const FACES: [(i32, i32, i32); 6] = [
    (-1, 0, 0),
    (0, -1, 0),
    (0, 0, -1),
    (0, 0, 1),
    (0, 1, 0),
    (1, 0, 0),
];

fn face(x: i32, y: i32, z: i32) -> (Vector3f, [Vector3f; 4]) {
    let mut dir = ZERO;
    if x != 0 {
        dir.x = x as f32;
        (
            dir,
            [
                v3(0.0, 1.0, 1.0),
                v3(0.0, -1.0, 1.0),
                v3(0.0, -1.0, -1.0),
                v3(0.0, 1.0, -1.0),
            ],
        )
    } else if y != 0 {
        dir.y = y as f32;
        (
            dir,
            [
                v3(1.0, 0.0, 1.0),
                v3(1.0, 0.0, -1.0),
                v3(-1.0, 0.0, -1.0),
                v3(-1.0, 0.0, 1.0),
            ],
        )
    } else {
        dir.z = z as f32;
        (
            dir,
            [
                v3(1.0, 1.0, 0.0),
                v3(-1.0, 1.0, 0.0),
                v3(-1.0, -1.0, 0.0),
                v3(1.0, -1.0, 0.0),
            ],
        )
    }
}

fn box_tex(i: usize, x: i32, y: i32, z: i32, add: &[Vector3f; 4]) -> (f32, f32) {
    let a = at(add, i);
    let (mut tx, mut ty) = if x != 0 {
        (a.z, a.y)
    } else if y != 0 {
        (a.x, a.z)
    } else {
        (a.x, a.y)
    };
    if x + y + z < 0 {
        tx = -tx;
        ty = -ty;
    }
    (tx, ty)
}

pub fn box_mesh(size: Vector3f) -> MeshData {
    let mut m = MeshData::default();
    let half = v3(size.x * 0.5f32, size.y * 0.5f32, size.z * 0.5f32);
    let mut base = 0i32;
    for (x, y, z) in FACES {
        let (dir, add) = face(x, y, z);
        for i in 0..4usize {
            let k = if x + y + z > 0 { 3 - i } else { i };
            let (tx, ty) = box_tex(i, x, y, z, &add);
            m.positions.push(mul3v(add3(dir, at(&add, k)), half));
            m.normals.push(dir);
            m.texcoords
                .push(v3((tx + 1.0f32) * 0.5f32, (ty + 1.0f32) * 0.5f32, 0.0));
        }
        for o in [0, 1, 2, 2, 3, 0] {
            m.index(base + o);
        }
        base += 4;
    }
    m
}

/// No normals; texture coordinates are the unscaled cube corners.
pub fn sky_box(size: f32) -> MeshData {
    let mut m = MeshData::default();
    for (x, y, z) in FACES {
        let (dir, add) = face(x, y, z);
        for i in 0..4usize {
            let k = if x + y + z < 0 { 3 - i } else { i };
            let corner = add3(dir, at(&add, k));
            m.positions
                .push(v3(corner.x * size, corner.y * size, corner.z * size));
            m.texcoords.push(corner);
        }
    }
    for i in 0..24 {
        m.index(i);
    }
    m
}

pub fn plane(c1: Vector3f, c2: Vector3f, uv: [Vector2f; 4]) -> Result<MeshData, PlaneError> {
    if c1 == c2 {
        return Err(PlaneError::Coincident);
    }
    let diff = sub3(c2, c1);
    let mut axes: Vec<i32> = Vec::new();
    let mut normal_axis = -1i32;
    let mut same = 0;
    for i in 0..3 {
        if comp(diff, i) == 0.0 {
            normal_axis = i;
            same += 1;
        } else {
            axes.push(i);
        }
    }
    if normal_axis < 0 || same > 1 {
        return Err(PlaneError::NotCoplanar);
    }
    let center = v3(
        (c1.x + c2.x) * 0.5f32,
        (c1.y + c2.y) * 0.5f32,
        (c1.z + c2.z) * 0.5f32,
    );
    let a1 = at(&axes, 0);
    let a2 = at(&axes, 1);
    let mut temp = [c1, c2, ZERO, ZERO];
    for i in 2..4usize {
        let t0 = at(&temp, 0);
        let prev = at(&temp, i - 2);
        let other = at(&temp, 3 - i);
        let mut t = at(&temp, i);
        set_comp(&mut t, normal_axis, comp(t0, normal_axis));
        set_comp(&mut t, a1, comp(prev, a1));
        set_comp(&mut t, a2, comp(other, a2));
        if let Some(slot) = temp.get_mut(i) {
            *slot = t;
        }
    }
    let mut coords = [ZERO; 4];
    for t in temp {
        let c = sub3(t, center);
        let k = if comp(c, a1) <= 0.0 {
            if comp(c, a2) > 0.0 { 0 } else { 1 }
        } else if comp(c, a2) > 0.0 {
            3
        } else {
            2
        };
        if let Some(slot) = coords.get_mut(k) {
            *slot = t;
        }
    }
    let first = at(&coords, 1);
    let mut m = MeshData::default();
    for (i, c) in coords.iter().enumerate() {
        m.positions.push(sub3(*c, first));
        let t = at(&uv, i);
        m.texcoords.push(v3(t.x, t.y, 0.0));
        m.normals.push(v3(0.0, 1.0, 0.0));
    }
    for i in [0, 1, 2, 2, 3, 0] {
        m.index(i);
    }
    Ok(m)
}

/// Each triangle becomes three line segments; a trailing partial triangle is dropped.
pub fn wireframe_indices(tris: &[u32]) -> Vec<u32> {
    let mut out = Vec::with_capacity(tris.len().saturating_mul(2));
    for t in tris.chunks_exact(3) {
        for i in 0..3usize {
            out.push(at(t, i));
            out.push(at(t, if i + 1 >= 3 { 0 } else { i + 1 }));
        }
    }
    out
}
