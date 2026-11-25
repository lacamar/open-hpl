//! `cBoundingVolume` and `cFrustum` state and logic.
//!
//! `Bv` and `Frustum` mirror the C++ field layout (`math/RustBv.h` asserts it); C++ keeps the
//! objects and forwards every method here.

// Frustum planes, vertices and corner tables are fixed-size arrays indexed by construction.
#![allow(clippy::indexing_slicing)]

use super::geom::{
    INSIDE, INTERSECT, OUTSIDE, plane_aabb_collision, plane_from_normal_point,
    plane_intersection_point, plane_line_intersection, plane_sphere_collision, point_in_aabb,
    points_aabb_planes_collision, points_planes_collision,
};
use super::matrix::{IDENTITY, matrix_mul, matrix_mul_vec};
use super::{
    Matrixf, Planef, Vector2f, Vector3f, cross3, dot3, fma, len3, mul3, plane_dist, sub3, tan, v3,
};
use crate::math::{add3, translation};

pub(crate) struct Pending {
    ptr: *const f32,
    len: i32,
}

#[repr(C)]
pub(crate) struct Bv {
    pub mtx: Matrixf,
    pub local_max: Vector3f,
    pub local_min: Vector3f,
    pub max: Vector3f,
    pub min: Vector3f,
    pub pivot: Vector3f,
    pub world_max: Vector3f,
    pub world_min: Vector3f,
    pub position: Vector3f,
    pub size: Vector3f,
    pub radius: f32,
    pub position_updated: bool,
    pub size_updated: bool,
    // One thin pointer: the C++ mirror stores it as `void*`.
    #[allow(clippy::box_collection)]
    pub pending: Option<Box<Vec<Pending>>>,
}

impl Clone for Bv {
    fn clone(&self) -> Self {
        let pending = self.pending.as_ref().map(|p| {
            Box::new(
                p.iter()
                    .map(|a| Pending {
                        ptr: a.ptr,
                        len: a.len,
                    })
                    .collect(),
            )
        });
        Bv {
            mtx: self.mtx,
            local_max: self.local_max,
            local_min: self.local_min,
            max: self.max,
            min: self.min,
            pivot: self.pivot,
            world_max: self.world_max,
            world_min: self.world_min,
            position: self.position,
            size: self.size,
            radius: self.radius,
            position_updated: self.position_updated,
            size_updated: self.size_updated,
            pending,
        }
    }
}

const ZERO: Vector3f = v3(0.0, 0.0, 0.0);

fn corners(min: Vector3f, max: Vector3f) -> [Vector3f; 8] {
    [
        v3(max.x, max.y, max.z),
        v3(max.x, max.y, min.z),
        v3(max.x, min.y, max.z),
        v3(max.x, min.y, min.z),
        v3(min.x, max.y, max.z),
        v3(min.x, max.y, min.z),
        v3(min.x, min.y, max.z),
        v3(min.x, min.y, min.z),
    ]
}

impl Bv {
    pub fn new() -> Bv {
        Bv {
            mtx: IDENTITY,
            local_max: ZERO,
            local_min: ZERO,
            max: ZERO,
            min: ZERO,
            pivot: ZERO,
            world_max: ZERO,
            world_min: ZERO,
            position: ZERO,
            size: ZERO,
            radius: 0.0,
            position_updated: true,
            size_updated: true,
            pending: None,
        }
    }

    pub fn update_size(&mut self) {
        if self.size_updated {
            let m = &self.mtx.m;
            let rot = Matrixf {
                m: [
                    [m[0][0], m[0][1], m[0][2], 0.0],
                    [m[1][0], m[1][1], m[1][2], 0.0],
                    [m[2][0], m[2][1], m[2][2], 0.0],
                    [0.0, 0.0, 0.0, 1.0],
                ],
            };
            let c = corners(self.local_min, self.local_max).map(|v| matrix_mul_vec(&rot, v));
            let (mut mx, mut mn) = (c[0], c[0]);
            for v in &c[1..] {
                if v.x < mn.x {
                    mn.x = v.x;
                } else if v.x > mx.x {
                    mx.x = v.x;
                }
                if v.y < mn.y {
                    mn.y = v.y;
                } else if v.y > mx.y {
                    mx.y = v.y;
                }
                if v.z < mn.z {
                    mn.z = v.z;
                } else if v.z > mx.z {
                    mx.z = v.z;
                }
            }
            self.max = mx;
            self.min = mn;
            self.size = sub3(mx, mn);
            self.pivot = sub3(mx, mul3(self.size, 0.5));
            let d = sub3(mx, self.pivot);
            self.radius = dot3(d, d).sqrt();
            self.size_updated = false;
            self.position_updated = true;
        }
        if self.position_updated {
            let t = translation(&self.mtx);
            self.world_max = add3(t, self.max);
            self.world_min = add3(t, self.min);
            self.position_updated = false;
        }
    }

    pub fn set_local_min_max(&mut self, min: Vector3f, max: Vector3f) {
        self.local_max = max;
        self.local_min = min;
        self.size_updated = true;
    }

    pub fn set_size(&mut self, size: Vector3f) {
        self.local_max = mul3(size, 0.5);
        self.local_min = mul3(size, -0.5);
        self.size_updated = true;
    }

    pub fn set_position(&mut self, pos: Vector3f) {
        self.mtx.m[0][3] = pos.x;
        self.mtx.m[1][3] = pos.y;
        self.mtx.m[2][3] = pos.z;
        self.position_updated = true;
    }

    pub fn set_transform(&mut self, m: &Matrixf) {
        self.mtx = *m;
        self.size_updated = true;
    }

    pub fn local_center(&self) -> Vector3f {
        let (a, b) = (self.local_min, self.local_max);
        v3((a.x + b.x) / 2.0, (a.y + b.y) / 2.0, (a.z + b.z) / 2.0)
    }

    pub fn world_center(&mut self) -> Vector3f {
        self.update_size();
        add3(translation(&self.mtx), self.pivot)
    }

    pub fn world_min_max(&mut self) -> (Vector3f, Vector3f) {
        self.update_size();
        (self.world_min, self.world_max)
    }

    pub fn radius(&mut self) -> f32 {
        self.update_size();
        self.radius
    }

    /// # Safety
    /// `array` must stay valid for `count` vectors until `create_from_points`.
    pub unsafe fn add_array_points(&mut self, array: *const f32, count: i32) {
        self.pending
            .get_or_insert_with(Default::default)
            .push(Pending {
                ptr: array,
                len: count,
            });
    }

    /// # Safety
    /// Every pending array holds `len` vectors of `stride` floats.
    pub unsafe fn create_from_points(&mut self, stride: i32) {
        let mut mx = v3(-100000.0, -100000.0, -100000.0);
        let mut mn = v3(100000.0, 100000.0, 100000.0);
        if let Some(list) = self.pending.take() {
            for a in list.iter() {
                let mut p = a.ptr;
                for _ in 0..a.len.max(0) {
                    // SAFETY: caller contract above.
                    let (x, y, z) = unsafe { (*p, *p.add(1), *p.add(2)) };
                    if x < mn.x {
                        mn.x = x;
                    }
                    if x > mx.x {
                        mx.x = x;
                    }
                    if y < mn.y {
                        mn.y = y;
                    }
                    if y > mx.y {
                        mx.y = y;
                    }
                    if z < mn.z {
                        mn.z = z;
                    }
                    if z > mx.z {
                        mx.z = z;
                    }
                    p = p.wrapping_offset(stride as isize);
                }
            }
        }
        self.local_max = mx;
        self.local_min = mn;
        self.position_updated = true;
        self.size_updated = true;
    }
}

impl Default for Bv {
    fn default() -> Self {
        Bv::new()
    }
}

/// Both volumes must be up to date.
pub(crate) fn bv_intersect(a: &Bv, b: &Bv) -> bool {
    let (mn1, mx1, mn2, mx2) = (a.world_min, a.world_max, b.world_min, b.world_max);
    !(mx1.x < mn2.x
        || mx1.y < mn2.y
        || mx1.z < mn2.z
        || mx2.x < mn1.x
        || mx2.y < mn1.y
        || mx2.z < mn1.z)
}

/// `cBoundingVolume` object as C++ lays it out: vtable pointer, then the fields.
#[repr(C)]
pub(crate) struct BvObj {
    pub vptr: usize,
    pub bv: Bv,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub(crate) struct Spheref {
    pub r: f32,
    pub center: Vector3f,
}

#[repr(C)]
pub(crate) struct Frustum {
    pub far_plane: f32,
    pub near_plane: f32,
    pub aspect: f32,
    pub fov: f32,
    pub view_size: Vector2f,
    pub inf_far: bool,
    pub inverts_cull: bool,
    pub oblique: bool,
    pub projection: u32,
    pub origin: Vector3f,
    pub origin_bv: BvObj,
    pub proj: Matrixf,
    pub view_proj: Matrixf,
    pub view: Matrixf,
    pub plane: [Planef; 6],
    pub sphere: Spheref,
    pub bounding_volume: BvObj,
    pub vertices: [Vector3f; 8],
}

const LEFT: usize = 0;
const RIGHT: usize = 1;
const BOTTOM: usize = 2;
const TOP: usize = 3;
const NEAR: usize = 4;
const FAR: usize = 5;
pub(crate) const PERSPECTIVE: u32 = 0;
pub(crate) const ORTHOGRAPHIC: u32 = 1;

const PLANE_ORDER: [[usize; 2]; 4] = [[RIGHT, TOP], [RIGHT, BOTTOM], [LEFT, BOTTOM], [LEFT, TOP]];

fn plane_normalize(p: &mut Planef) {
    let mag = dot3(v3(p.a, p.b, p.c), v3(p.a, p.b, p.c)).sqrt();
    p.a /= mag;
    p.b /= mag;
    p.c /= mag;
    p.d /= mag;
}

fn plane_normal(p: &Planef) -> Vector3f {
    v3(p.a, p.b, p.c)
}

fn intersect_into(a: &Planef, b: &Planef, c: &Planef, out: &mut Vector3f) {
    if let Some(p) = plane_intersection_point(a, b, c) {
        *out = p;
    }
}

impl Frustum {
    /// C++ leaves everything but `inverts_cull` uninitialised; zero is the deterministic choice.
    pub fn init(&mut self) {
        self.far_plane = 0.0;
        self.near_plane = 0.0;
        self.aspect = 0.0;
        self.fov = 0.0;
        self.view_size = Vector2f { x: 0.0, y: 0.0 };
        self.inf_far = false;
        self.inverts_cull = false;
        self.oblique = false;
        self.projection = 0;
        self.origin = ZERO;
        self.proj = Matrixf::default();
        self.view_proj = Matrixf::default();
        self.view = Matrixf::default();
        self.plane = [Planef::default(); 6];
        self.sphere = Spheref::default();
        self.vertices = [ZERO; 8];
    }

    #[allow(clippy::too_many_arguments)]
    pub fn setup_perspective(
        &mut self,
        proj: &Matrixf,
        view: &Matrixf,
        far: f32,
        near: f32,
        fov: f32,
        aspect: f32,
        origin: Vector3f,
        inf: bool,
        custom_far: Option<&Matrixf>,
        oblique: bool,
    ) {
        self.projection = PERSPECTIVE;
        self.fov = fov;
        self.aspect = aspect;
        self.oblique = oblique;
        self.setup(proj, view, far, near, origin, inf, custom_far);
    }

    #[allow(clippy::too_many_arguments)]
    pub fn setup_ortho(
        &mut self,
        proj: &Matrixf,
        view: &Matrixf,
        far: f32,
        near: f32,
        view_size: Vector2f,
        origin: Vector3f,
        inf: bool,
    ) {
        self.projection = ORTHOGRAPHIC;
        self.view_size = view_size;
        self.setup(proj, view, far, near, origin, inf, None);
    }

    #[allow(clippy::too_many_arguments)]
    fn setup(
        &mut self,
        proj: &Matrixf,
        view: &Matrixf,
        far: f32,
        near: f32,
        origin: Vector3f,
        inf: bool,
        custom_far: Option<&Matrixf>,
    ) {
        self.view_proj = matrix_mul(proj, view);
        self.view = *view;
        self.proj = *proj;
        self.far_plane = far;
        self.near_plane = near;
        self.origin = origin;
        self.inf_far = inf;
        let n2 = near * 2.0;
        self.origin_bv.bv.set_size(v3(n2, n2, n2));
        self.origin_bv.bv.set_position(origin);
        self.update_planes(custom_far);
        self.update_sphere();
        self.update_vertices();
        self.update_bv();
    }

    fn update_planes(&mut self, custom_far: Option<&Matrixf>) {
        let vp = self.view_proj.m;
        let row = |m: &[[f32; 4]; 4], r: usize, sign: f32| Planef {
            a: m[3][0] + sign * m[r][0],
            b: m[3][1] + sign * m[r][1],
            c: m[3][2] + sign * m[r][2],
            d: m[3][3] + sign * m[r][3],
        };
        let pl = &mut self.plane;
        pl[LEFT] = row(&vp, 0, 1.0);
        pl[RIGHT] = row(&vp, 0, -1.0);
        pl[BOTTOM] = row(&vp, 1, 1.0);
        pl[TOP] = row(&vp, 1, -1.0);
        pl[NEAR] = row(&vp, 2, 1.0);
        pl[FAR] = match custom_far {
            Some(c) => row(&matrix_mul(c, &self.view).m, 2, -1.0),
            None => row(&vp, 2, -1.0),
        };
        for p in pl.iter_mut() {
            plane_normalize(p);
        }
    }

    fn update_sphere(&mut self) {
        let view_len = self.far_plane - self.near_plane;
        let radius = if self.projection == PERSPECTIVE {
            let top = self.far_plane * tan(self.fov * 0.5);
            let right = top * self.aspect;
            let p = v3(0.0, 0.0, fma(view_len, 0.5, self.near_plane));
            len3(sub3(p, v3(right, top, view_len)))
        } else {
            len3(v3(
                self.view_size.x * 0.5,
                self.view_size.y * 0.5,
                view_len * 0.5,
            ))
        };
        let f = v3(self.view.m[2][0], self.view.m[2][1], self.view.m[2][2]);
        let look = mul3(f, -1.0);
        let t = fma(view_len, 0.5, self.near_plane);
        let o = self.origin;
        let center = v3(
            fma(look.x, t, o.x),
            fma(look.y, t, o.y),
            fma(look.z, t, o.z),
        );
        self.sphere = Spheref { center, r: radius };
    }

    fn update_vertices(&mut self) {
        let p = self.plane;
        let v = &mut self.vertices;
        for (i, (a, b, c)) in [
            (NEAR, RIGHT, TOP),
            (NEAR, RIGHT, BOTTOM),
            (NEAR, LEFT, BOTTOM),
            (NEAR, LEFT, TOP),
            (FAR, RIGHT, TOP),
            (FAR, RIGHT, BOTTOM),
            (FAR, LEFT, BOTTOM),
            (FAR, LEFT, TOP),
        ]
        .into_iter()
        .enumerate()
        {
            intersect_into(&p[a], &p[b], &p[c], &mut v[i]);
        }
        if !self.oblique {
            return;
        }
        let (near, far) = (p[NEAR], p[FAR]);
        let near_cross_far = cross3(plane_normal(&far), plane_normal(&near));
        let fake_near = plane_from_normal_point(mul3(plane_normal(&far), -1.0), self.origin);
        for i in 0..4 {
            if plane_dist(&fake_near, v[i]) < 0.0 || plane_dist(&far, v[i]) < 0.0 {
                let [p0, p1] = PLANE_ORDER[i];
                intersect_into(&far, &p[p0], &p[p1], &mut v[i]);
                let split = plane_from_normal_point(near_cross_far, v[i]);
                intersect_into(&far, &near, &split, &mut v[i]);
            }
        }
        for vtx in &mut v[4..8] {
            if plane_dist(&near, *vtx) < 0.0 {
                let split = plane_from_normal_point(near_cross_far, *vtx);
                intersect_into(&far, &near, &split, vtx);
            }
        }
    }

    fn update_bv(&mut self) {
        let v = &self.vertices;
        let (mut mn, mut mx) = (v[0], v[0]);
        for p in &v[1..] {
            if mx.x < p.x {
                mx.x = p.x;
            } else if mn.x > p.x {
                mn.x = p.x;
            }
            if mx.y < p.y {
                mx.y = p.y;
            } else if mn.y > p.y {
                mn.y = p.y;
            }
            if mx.z < p.z {
                mx.z = p.z;
            } else if mn.z > p.z {
                mn.z = p.z;
            }
        }
        self.bounding_volume.bv.set_local_min_max(mn, mx);
    }

    fn side_planes(&self) -> &[Planef] {
        &self.plane[..if self.inf_far { 5 } else { 6 }]
    }

    /// Brings the frustum's bounding volume up to date; call before the `&self` queries.
    pub fn sync(&mut self) {
        self.bounding_volume.bv.update_size();
    }

    pub fn collide_point(&self, p: Vector3f) -> bool {
        let bv = &self.bounding_volume.bv;
        if !point_in_aabb(p, bv.world_min, bv.world_max) {
            return false;
        }
        !self.side_planes().iter().any(|pl| plane_dist(pl, p) < 0.0)
    }

    pub fn collide_bv(&self, bv: &mut Bv) -> i32 {
        let (c, r) = (bv.world_center(), bv.radius());
        if self.collide_frustum_sphere(c, r) == OUTSIDE {
            return OUTSIDE;
        }
        let ret = self.collide_sphere(c, r, 6);
        if ret == INTERSECT {
            let (mn, mx) = bv.world_min_max();
            return self.collide_aabb(mn, mx);
        }
        ret
    }

    pub fn collide_node(&self, c: Vector3f, r: f32, mn: Vector3f, mx: Vector3f) -> i32 {
        if self.collide_frustum_sphere(c, r) == OUTSIDE {
            return OUTSIDE;
        }
        let ret = self.collide_sphere(c, r, 6);
        if ret == INTERSECT {
            return self.collide_aabb(mn, mx);
        }
        ret
    }

    pub fn collide_frustum(&self, other: &Frustum) -> i32 {
        if !bv_intersect(&self.bounding_volume.bv, &other.bounding_volume.bv) {
            return OUTSIDE;
        }
        let c = points_planes_collision(&other.vertices, self.side_planes());
        if c == OUTSIDE || c == INSIDE {
            return c;
        }
        let c = points_planes_collision(&self.vertices, other.side_planes());
        if c == OUTSIDE {
            return OUTSIDE;
        }
        INTERSECT
    }

    pub fn check_sphere_near_plane(&self, center: Vector3f, r: f32) -> bool {
        if plane_sphere_collision(&self.plane[NEAR], center, r) != INTERSECT {
            return false;
        }
        let v = &self.vertices;
        for p in &v[..4] {
            let d = sub3(*p, center);
            if dot3(d, d) <= r * r {
                return true;
            }
        }
        for p in &v[..4] {
            let mut to = sub3(*p, center);
            super::normalize3(&mut to);
            let point = v3(
                fma(to.x, r, center.x),
                fma(to.y, r, center.y),
                fma(to.z, r, center.z),
            );
            let plane = plane_from_normal_point(mul3(to, -1.0), point);
            if points_planes_collision(&v[..4], &[plane]) == OUTSIDE {
                return false;
            }
        }
        true
    }

    pub fn check_aabb_near_plane(&self, mn: Vector3f, mx: Vector3f) -> bool {
        if plane_aabb_collision(&self.plane[NEAR], mn, mx) != INTERSECT {
            return false;
        }
        let v = &self.vertices;
        if v[..4].iter().any(|p| point_in_aabb(*p, mn, mx)) {
            return true;
        }
        points_aabb_planes_collision(&v[..4], mn, mx) != OUTSIDE
    }

    pub fn check_bv_near_plane(&self, bv: &mut Bv) -> bool {
        let (mn, mx) = bv.world_min_max();
        if plane_aabb_collision(&self.plane[NEAR], mn, mx) != INTERSECT {
            return false;
        }
        if self.vertices[..4].iter().any(|p| point_in_aabb(*p, mn, mx)) {
            return true;
        }
        let (c, r) = (bv.world_center(), bv.radius());
        if self.collide_sphere(c, r, 4) == INSIDE {
            return true;
        }
        let (mn, mx) = bv.world_min_max();
        points_aabb_planes_collision(&self.vertices[..4], mn, mx) != OUTSIDE
    }

    pub fn check_frustum_near_plane(&self, other: &Frustum) -> bool {
        let near = &self.plane[NEAR];
        if plane_sphere_collision(near, other.sphere.center, other.sphere.r) != INTERSECT {
            return false;
        }
        if !other.vertices.iter().any(|v| plane_dist(near, *v) > 0.0) {
            return false;
        }
        points_planes_collision(&self.vertices[..4], &other.plane[..4]) != OUTSIDE
    }

    /// `out` is written only on a hit; returns the hit flag.
    pub fn check_line(&self, start: Vector3f, end: Vector3f, out: &mut Vector3f) -> bool {
        let mut closest = -1.0f32;
        for i in 0..self.side_planes().len() {
            let plane = self.plane[i];
            let (true, Some((t, pos))) = plane_line_intersection(&plane, start, end) else {
                continue;
            };
            let n = plane_normal(&plane);
            let probe = add3(pos, mul3(n, 0.001));
            if (t < closest || closest < 0.0) && self.collide_point(probe) {
                *out = pos;
                closest = t;
            }
        }
        closest >= 0.0
    }

    pub fn collide_frustum_sphere(&self, c: Vector3f, r: f32) -> i32 {
        let sum = self.sphere.r + r;
        let d = sub3(self.sphere.center, c);
        if dot3(d, d) > sum * sum {
            OUTSIDE
        } else {
            INTERSECT
        }
    }

    pub fn collide_sphere(&self, c: Vector3f, r: f32, max_planes: i32) -> i32 {
        let n = (if self.inf_far { 5 } else { 6 }).max(max_planes).min(6) as usize;
        for p in &self.plane[..n] {
            let d = plane_dist(p, c);
            if d < -r {
                return OUTSIDE;
            }
            if d.abs() < r {
                return INTERSECT;
            }
        }
        INSIDE
    }

    pub fn collide_aabb(&self, mn: Vector3f, mx: Vector3f) -> i32 {
        let c = corners(mn, mx);
        let r = points_planes_collision(&c, self.side_planes());
        if r == OUTSIDE || r == INSIDE {
            return r;
        }
        if points_aabb_planes_collision(&self.vertices, mn, mx) == OUTSIDE {
            return OUTSIDE;
        }
        INTERSECT
    }
}
