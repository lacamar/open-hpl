//! Intersections, planes, clip rects and 3D vector helpers.

use super::libm::{acos, atan, atan2, cos, sin, tan};
use super::matrix::{matrix_inverse, matrix_mul_divide_w, matrix_mul_vec, rotate_y, transpose};
use super::scalar::{angle_distance, angle_from_points_2d, increase_to, wrap};
use super::{
    EPSILON, Matrixf, Planef, Rect2f, Rect2l, TWO_PI, Vector2f, Vector2l, Vector3f, cross3, div3,
    dot3, fma, len3, mul3, normalize3, plane_dist, sub3, v3,
};

pub(crate) const INSIDE: i32 = 0;
pub(crate) const OUTSIDE: i32 = 1;
pub(crate) const INTERSECT: i32 = 2;

pub(crate) fn expand_aabb(
    base_min: &mut Vector3f,
    base_max: &mut Vector3f,
    add_min: Vector3f,
    add_max: Vector3f,
) {
    if base_min.x > add_min.x {
        base_min.x = add_min.x;
    }
    if base_min.y > add_min.y {
        base_min.y = add_min.y;
    }
    if base_min.z > add_min.z {
        base_min.z = add_min.z;
    }
    if base_max.x < add_max.x {
        base_max.x = add_max.x;
    }
    if base_max.y < add_max.y {
        base_max.y = add_max.y;
    }
    if base_max.z < add_max.z {
        base_max.z = add_max.z;
    }
}

pub(crate) fn point_in_rect(p: Vector2f, r: &Rect2f) -> bool {
    !(p.x < r.x || p.x > r.x + r.w || p.y < r.y || p.y > r.y + r.h)
}

pub(crate) fn rect_fit_l(s: &Rect2l, d: &Rect2l) -> bool {
    if s.w > d.w
        || s.h > d.h
        || s.x.wrapping_add(s.w) > d.x.wrapping_add(d.w)
        || s.y.wrapping_add(s.h) > d.y.wrapping_add(d.h)
    {
        return false;
    }
    !(s.x < d.x || s.y < d.y || s.x > d.x.wrapping_add(d.w) || s.y > d.y.wrapping_add(d.h))
}

pub(crate) fn rect_fit_f(s: &Rect2f, d: &Rect2f) -> bool {
    if s.w > d.w || s.h > d.h || s.x + s.w > d.x + d.w || s.y + s.h > d.y + d.h {
        return false;
    }
    !(s.x < d.x || s.y < d.y || s.x > d.x + d.w || s.y > d.y + d.h)
}

pub(crate) fn clip_rect(src: &Rect2f, dest: &Rect2f) -> Rect2f {
    let mut o = *src;
    if o.x < dest.x {
        o.w -= dest.x - o.x;
        o.x = dest.x;
    }
    if o.y < dest.y {
        o.h -= dest.y - o.y;
        o.y = dest.y;
    }
    if o.x + o.w > dest.x + dest.w {
        o.w -= (o.x + o.w) - (dest.x + dest.w);
    }
    if o.y + o.h > dest.y + dest.h {
        o.h -= (o.y + o.h) - (dest.y + dest.h);
    }
    o
}

pub(crate) fn aabb_intersection(
    min1: Vector3f,
    max1: Vector3f,
    min2: Vector3f,
    max2: Vector3f,
) -> bool {
    !(max1.x < min2.x
        || max1.y < min2.y
        || max1.z < min2.z
        || max2.x < min1.x
        || max2.y < min1.y
        || max2.z < min1.z)
}

pub(crate) fn aabb_inside(
    in_min: Vector3f,
    in_max: Vector3f,
    out_min: Vector3f,
    out_max: Vector3f,
) -> bool {
    in_min.x >= out_min.x
        && in_min.y >= out_min.y
        && in_min.z >= out_min.z
        && in_max.x <= out_max.x
        && in_max.y <= out_max.y
        && in_max.z <= out_max.z
}

pub(crate) fn point_in_aabb(p: Vector3f, min: Vector3f, max: Vector3f) -> bool {
    !(p.x > max.x || p.y > max.y || p.z > max.z || p.x < min.x || p.y < min.y || p.z < min.z)
}

fn line_point(start: Vector3f, dir: Vector3f, t: f32) -> Vector3f {
    v3(
        fma(dir.x, t, start.x),
        fma(dir.y, t, start.y),
        fma(dir.z, t, start.z),
    )
}

/// `t1`/`t2` are `None` when gcc leaves `*apT` untouched (no real roots).
pub(crate) fn sphere_line(
    center: Vector3f,
    radius: f32,
    start: Vector3f,
    end: Vector3f,
) -> (
    bool,
    Option<(f32, Option<Vector3f>)>,
    Option<(f32, Option<Vector3f>)>,
) {
    let mut dir = sub3(end, start);
    let seg = len3(dir);
    normalize3(&mut dir);
    let sts = sub3(start, center);
    let b = 2.0f32 * dot3(dir, sts);
    let c = fma(-radius, radius, dot3(sts, sts));
    let disc = fma(b, b, -(4.0f32 * c));
    if disc < 0.0 || disc.is_nan() {
        return (false, None, None);
    }
    let sq = disc.sqrt();
    let t1 = 0.5f32 * (-b + sq);
    let t2 = 0.5f32 * (-b - sq);
    let mut hit = false;
    let mut one = |t: f32| {
        if t <= seg {
            hit = true;
            (t, Some(line_point(start, dir, t)))
        } else {
            (-1.0, None)
        }
    };
    let r1 = one(t1);
    let r2 = one(t2);
    (hit, Some(r1), Some(r2))
}

pub(crate) fn point_same_side_of_edge_2d(
    p1: Vector3f,
    p2: Vector3f,
    a: Vector3f,
    b: Vector3f,
) -> bool {
    let edge = sub3(b, a);
    let cp1 = cross3(edge, sub3(p1, a));
    let cp2 = cross3(edge, sub3(p2, a));
    dot3(cp1, cp2) >= 0.0
}

pub(crate) fn point_inside_triangle_2d(p: Vector2f, tri: &[Vector3f; 3]) -> bool {
    let pt = v3(p.x, p.y, 0.0);
    let t = tri.map(|v| v3(v.x, v.y, 0.0));
    point_same_side_of_edge_2d(pt, t[0], t[1], t[2])
        && point_same_side_of_edge_2d(pt, t[1], t[0], t[2])
        && point_same_side_of_edge_2d(pt, t[2], t[0], t[1])
}

pub(crate) fn clip_rect_from_normalized_min_max(
    min: Vector3f,
    max: Vector3f,
    screen: Vector2l,
) -> Rect2l {
    let (mut min, mut max) = (min, max);
    if min.x < -1.0 {
        min.x = -1.0;
    }
    if min.y < -1.0 {
        min.y = -1.0;
    }
    if max.x > 1.0 {
        max.x = 1.0;
    }
    if max.y > 1.0 {
        max.y = 1.0;
    }
    let hx = screen.x as f32 * 0.5f32;
    let hy = screen.y as f32 * 0.5f32;
    let tl_x = fma(min.x, hx, hx) as i32;
    let tl_y = fma(max.y, -hy, hy) as i32;
    let br_x = fma(max.x, hx, hx) as i32;
    let br_y = fma(min.y, -hy, hy) as i32;
    Rect2l {
        x: tl_x,
        y: tl_y,
        w: br_x.wrapping_sub(tl_x),
        h: br_y.wrapping_sub(tl_y),
    }
}

pub(crate) struct FrustumView<'a> {
    pub view: &'a Matrixf,
    pub proj: &'a Matrixf,
    pub perspective: bool,
    pub fov: f32,
    pub near: f32,
    pub aspect: f32,
}

fn tan_half(f: &FrustumView, tan_half_fov: f32) -> f32 {
    if tan_half_fov > 0.0 {
        tan_half_fov
    } else {
        tan(f.fov * 0.5f32)
    }
}

const BV_CONNECTIONS: [[usize; 3]; 8] = [
    [1, 2, 4],
    [0, 3, 5],
    [0, 3, 6],
    [1, 2, 7],
    [0, 5, 6],
    [1, 4, 7],
    [2, 4, 7],
    [3, 5, 6],
];

#[expect(
    clippy::indexing_slicing,
    reason = "corner indices are below 8 and at most 24 points are emitted (3 per corner)"
)]
pub(crate) fn normalized_clip_rect_from_bv(
    min: Vector3f,
    max: Vector3f,
    f: &FrustumView,
    tan_half_fov: f32,
) -> Option<(Vector3f, Vector3f)> {
    let mut corners = [
        v3(max.x, max.y, max.z),
        v3(max.x, max.y, min.z),
        v3(max.x, min.y, max.z),
        v3(max.x, min.y, min.z),
        v3(min.x, max.y, max.z),
        v3(min.x, max.y, min.z),
        v3(min.x, min.y, max.z),
        v3(min.x, min.y, min.z),
    ];
    let tan_h = tan_half(f, tan_half_fov);
    let near = f.near;
    let near_top = tan_h * near;
    let near_right = f.aspect * near_top;

    for c in &mut corners {
        *c = matrix_mul_vec(f.view, *c);
    }

    let mut pts = [Vector3f::default(); 24];
    let mut n = 0;
    let mut visible = false;
    for i in 0..8 {
        let mut pos = corners[i];
        if pos.z < 0.0 {
            visible = true;
        }
        if f.perspective {
            let mut z = pos.z;
            if z > 0.0 {
                for &conn in &BV_CONNECTIONS[i] {
                    let cp = corners[conn];
                    if cp.z >= 0.0 {
                        continue;
                    }
                    let delta = sub3(pos, cp);
                    let connect_to_near = (-near) - cp.z;
                    let t = connect_to_near / delta.z;
                    let mut sp = line_point(cp, delta, t);
                    sp.x /= near_right;
                    sp.y /= near_top;
                    pts[n] = sp;
                    n += 1;
                }
            } else {
                if z == 0.0 {
                    z = 0.0001;
                }
                pos.x = ((pos.x / z) * -near) / near_right;
                pos.y = ((pos.y / z) * -near) / near_top;
                pts[n] = pos;
                n += 1;
            }
        } else {
            pts[n] = matrix_mul_vec(f.proj, corners[i]);
            n += 1;
        }
    }
    if !visible {
        return None;
    }
    let mut dmax = v3(-10000.0, -10000.0, -10000.0);
    let mut dmin = v3(10000.0, 10000.0, 10000.0);
    for p in &pts[..n] {
        if p.x < dmin.x {
            dmin.x = p.x;
        }
        if p.x > dmax.x {
            dmax.x = p.x;
        }
        if p.y < dmin.y {
            dmin.y = p.y;
        }
        if p.y > dmax.y {
            dmax.y = p.y;
        }
    }
    Some((dmin, dmax))
}

/// One axis of the sphere projection: up to two `(screen coord, plane point on axis, light pos)` hits.
fn sphere_axis(
    pa: f32,
    pz: f32,
    sa: f32,
    sz: f32,
    r: f32,
    sqr_r: f32,
    near: f32,
    near_dim: f32,
    half: f32,
    half_sign: f32,
) -> [Option<(i32, f32)>; 2] {
    let mut out = [None, None];
    let d = fma(sqr_r, sa, -((sa + sz) * (sqr_r - sz)));
    if d >= 0.0 {
        let d = d.sqrt();
        let n1 = fma(r, pa, d) / (sa + sz);
        let n2 = fma(r, pa, -d) / (sa + sz);
        let nz1 = fma(-n1, pa, r) / pz;
        let nz2 = fma(-n2, pa, r) / pz;
        let p1 = ((sa + sz) - sqr_r) / fma(-(nz1 / n1), pa, pz);
        let p2 = ((sa + sz) - sqr_r) / fma(-(nz2 / n2), pa, pz);
        for (slot, (p, n, nz)) in out.iter_mut().zip([(p1, n1, nz1), (p2, n2, nz2)]) {
            if p < 0.0 {
                let near_a = (nz * near / n) / near_dim;
                let screen = fma(half_sign * half, near_a, half) as i32;
                let pp = -p * nz / n;
                *slot = Some((screen, pp));
            }
        }
    }
    out
}

pub(crate) fn clip_rect_from_sphere(
    position: Vector3f,
    radius: f32,
    f: &FrustumView,
    screen: Vector2l,
    position_is_screen_space: bool,
    tan_half_fov: f32,
) -> Rect2l {
    let light = if position_is_screen_space {
        position
    } else {
        matrix_mul_vec(f.view, position)
    };
    let tan_h = tan_half(f, tan_half_fov);
    let near = f.near;
    let near_top = tan_h * near;
    let near_right = f.aspect * near_top;
    let hx = screen.x as f32 * 0.5f32;
    let hy = screen.y as f32 * 0.5f32;
    let mut right = screen.x.wrapping_sub(1);
    let mut top = 0i32;
    let mut left = 0i32;
    let mut bottom = screen.y.wrapping_sub(1);
    let sqr_r = radius * radius;
    let sq = v3(light.x * light.x, light.y * light.y, light.z * light.z);

    for hit in sphere_axis(
        light.x, light.z, sq.x, sq.z, radius, sqr_r, near, near_right, hx, 1.0,
    )
    .into_iter()
    .flatten()
    {
        if hit.1 < light.x {
            left = left.max(hit.0);
        } else {
            right = right.min(hit.0);
        }
    }
    for hit in sphere_axis(
        light.y, light.z, sq.y, sq.z, radius, sqr_r, near, near_top, hy, -1.0,
    )
    .into_iter()
    .flatten()
    {
        if hit.1 > light.y {
            top = top.max(hit.0);
        } else {
            bottom = bottom.min(hit.0);
        }
    }
    Rect2l {
        x: left,
        y: top,
        w: right.wrapping_sub(left).wrapping_add(1),
        h: bottom.wrapping_sub(top).wrapping_add(1),
    }
}

pub(crate) fn spherical_coords_from_point(center: Vector3f, p: Vector3f) -> Vector2f {
    let mut polar = Vector2f { x: 0.0, y: 0.0 };
    let r = sub3(p, center);
    let len = len3(r);
    let inv = 1.0f32 / len;
    polar.x = acos(r.y * inv);
    polar.x = wrap(polar.x, 0.0, TWO_PI);
    let in_section = fma(p.x, p.x, p.z * p.z).sqrt();
    if in_section > EPSILON {
        polar.y = atan2(r.z, r.x);
        polar.y = wrap(polar.y, 0.0, TWO_PI);
    }
    polar
}

pub(crate) fn point_from_spherical_coords(center: Vector3f, radius: f32, c: Vector2f) -> Vector3f {
    let (st, ct) = (sin(c.x), cos(c.x));
    let (sp, cp) = (sin(c.y), cos(c.y));
    let dir = v3(st * cp, ct, st * sp);
    line_point(center, dir, radius)
}

pub(crate) fn plane_parallel(a: &Planef, b: &Planef) -> bool {
    fma(a.c, b.c, fma(a.a, b.a, a.b * b.b)) > 0.99999f32
}

pub(crate) fn plane_from_normal_point(n: Vector3f, p: Vector3f) -> Planef {
    Planef {
        a: n.x,
        b: n.y,
        c: n.z,
        d: -fma(n.z, p.z, fma(n.x, p.x, n.y * p.y)),
    }
}

pub(crate) fn transform_plane(m: &Matrixf, plane: &Planef) -> Planef {
    let normal = v3(plane.a, plane.b, plane.c);
    let point = mul3(normal, -plane.d);
    let mut rot = *m;
    rot.m[0][3] = 0.0;
    rot.m[1][3] = 0.0;
    rot.m[2][3] = 0.0;
    rot.m[3] = [0.0, 0.0, 0.0, 1.0];
    let nt = transpose(&matrix_inverse(&rot));
    let mut n = matrix_mul_vec(&nt, normal);
    normalize3(&mut n);
    plane_from_normal_point(n, matrix_mul_vec(m, point))
}

pub(crate) fn plane_intersection_line(pa: &Planef, pb: &Planef) -> (Vector3f, Vector3f) {
    let dir = cross3(v3(pa.a, pa.b, pa.c), v3(pb.a, pb.b, pb.c));
    let val = (-pb.b) / pa.b;
    let z = fma(pa.d, val, pb.d) / fma(pa.c, val, pb.c);
    let y = fma(-pa.c, z, pa.d) / pa.b;
    (dir, v3(0.0, y, z))
}

pub(crate) fn plane_intersection_point(p1: &Planef, p2: &Planef, p3: &Planef) -> Option<Vector3f> {
    if plane_parallel(p1, p2) || plane_parallel(p1, p3) || plane_parallel(p2, p3) {
        return None;
    }
    let n1 = v3(p1.a, p1.b, p1.c);
    let n2 = v3(p2.a, p2.b, p2.c);
    let n3 = v3(p3.a, p3.b, p3.c);
    let c12 = cross3(n1, n2);
    let c31 = cross3(n3, n1);
    let c23 = cross3(n2, n3);
    let den = dot3(n1, c23);
    let comp = |a: f32, b: f32, c: f32| fma(c, -p3.d, fma(a, -p1.d, b * -p2.d)) / den;
    Some(v3(
        comp(c23.x, c31.x, c12.x),
        comp(c23.y, c31.y, c12.y),
        comp(c23.z, c31.z, c12.z),
    ))
}

pub(crate) fn plane_sphere_collision(p: &Planef, center: Vector3f, r: f32) -> i32 {
    let d = plane_dist(p, center);
    if d.abs() <= r {
        INTERSECT
    } else if d > r {
        INSIDE
    } else {
        OUTSIDE
    }
}

pub(crate) fn plane_aabb_collision(p: &Planef, min: Vector3f, max: Vector3f) -> i32 {
    let corners = [
        v3(max.x, max.y, max.z),
        v3(max.x, max.y, min.z),
        v3(max.x, min.y, max.z),
        v3(max.x, min.y, min.z),
        v3(min.x, max.y, max.z),
        v3(min.x, max.y, min.z),
        v3(min.x, min.y, max.z),
        v3(min.x, min.y, min.z),
    ];
    let mut inside = 8;
    for c in corners {
        if plane_dist(p, c) < 0.0 {
            inside -= 1;
        }
    }
    match inside {
        0 => OUTSIDE,
        8 => INSIDE,
        _ => INTERSECT,
    }
}

pub(crate) fn plane_aabb_sphere_collision(
    p: &Planef,
    min: Vector3f,
    max: Vector3f,
    center: Vector3f,
    r: f32,
) -> i32 {
    let d = plane_dist(p, center);
    if d < -r {
        return OUTSIDE;
    }
    if d > r {
        return INSIDE;
    }
    plane_aabb_collision(p, min, max)
}

/// Returns `(hit, t, pos)`; gcc writes `t`/`pos` for every non-parallel line.
pub(crate) fn plane_line_intersection(
    p: &Planef,
    start: Vector3f,
    end: Vector3f,
) -> (bool, Option<(f32, Vector3f)>) {
    let delta = sub3(end, start);
    let neg_dist = -plane_dist(p, start);
    let mul = fma(p.c, delta.z, fma(p.a, delta.x, p.b * delta.y));
    if mul == 0.0 {
        return (false, None);
    }
    let t = neg_dist / mul;
    let pos = line_point(start, delta, t);
    (!(t < 0.0 || t > 1.0), Some((t, pos)))
}

pub(crate) fn aabb_line_intersection(
    min: Vector3f,
    max: Vector3f,
    start: Vector3f,
    end: Vector3f,
    pos_out: &mut Option<Vector3f>,
) -> (bool, f32) {
    let planes = [
        plane_from_normal_point(v3(-1.0, 0.0, 0.0), min),
        plane_from_normal_point(v3(1.0, 0.0, 0.0), max),
        plane_from_normal_point(v3(0.0, -1.0, 0.0), min),
        plane_from_normal_point(v3(0.0, 1.0, 0.0), max),
        plane_from_normal_point(v3(0.0, 0.0, -1.0), min),
        plane_from_normal_point(v3(0.0, 0.0, 1.0), max),
    ];
    let larger_min = v3(min.x - 0.001f32, min.y - 0.001f32, min.z - 0.001f32);
    let larger_max = v3(max.x + 0.001f32, max.y + 0.001f32, max.z + 0.001f32);
    let mut shortest = 1000.0f32;
    let mut hit = false;
    for p in &planes {
        if let (true, Some((t, pos))) = plane_line_intersection(p, start, end)
            && t < shortest
            && point_in_aabb(pos, larger_min, larger_max)
        {
            *pos_out = Some(pos);
            hit = true;
            shortest = t;
        }
    }
    (hit, shortest)
}

pub(crate) fn points_planes_collision(verts: &[Vector3f], planes: &[Planef]) -> i32 {
    let mut total_in = 0usize;
    for p in planes {
        let mut in_count = verts.len();
        for v in verts {
            if plane_dist(p, *v) < 0.0 {
                in_count -= 1;
            }
        }
        if in_count == 0 {
            return OUTSIDE;
        }
        if in_count == verts.len() {
            total_in += 1;
        }
    }
    if total_in == planes.len() {
        INSIDE
    } else {
        INTERSECT
    }
}

pub(crate) fn points_aabb_planes_collision(
    verts: &[Vector3f],
    min: Vector3f,
    max: Vector3f,
) -> i32 {
    let p = |a, b, c, d| Planef { a, b, c, d };
    let planes = [
        p(-1.0, 0.0, 0.0, max.x),
        p(1.0, 0.0, 0.0, -min.x),
        p(0.0, -1.0, 0.0, max.y),
        p(0.0, 1.0, 0.0, -min.y),
        p(0.0, 0.0, -1.0, max.z),
        p(0.0, 0.0, 1.0, -min.z),
    ];
    points_planes_collision(verts, &planes)
}

pub(crate) fn separating_axis_intersection(
    verts: [&[Vector3f]; 2],
    planes: [&[Planef]; 2],
) -> bool {
    let mut vmin = [0.0f32; 2];
    let mut vmax = [0.0f32; 2];
    for set in planes {
        for p in set {
            let normal = v3(p.a, p.b, p.c);
            for ((vs, mn), mx) in verts.iter().zip(&mut vmin).zip(&mut vmax) {
                let Some((&v0, rest)) = vs.split_first() else {
                    continue;
                };
                let first = dot3(v0, normal);
                *mn = first;
                *mx = first;
                for v in rest {
                    let d = dot3(*v, normal);
                    if *mn > d {
                        *mn = d;
                    } else if *mx < d {
                        *mx = d;
                    }
                }
            }
            if vmin[0] > vmax[1] || vmin[1] > vmax[0] {
                return false;
            }
        }
    }
    true
}

pub(crate) fn line_triangle_intersection(
    start: Vector3f,
    end: Vector3f,
    p0: Vector3f,
    p1: Vector3f,
    p2: Vector3f,
    skip_backfacing: bool,
) -> Option<f32> {
    let delta = sub3(end, start);
    let e1 = sub3(p1, p0);
    let e2 = sub3(p2, p0);
    if skip_backfacing {
        let n = cross3(e1, e2);
        if dot3(n, delta) < 0.0 {
            return None;
        }
    }
    let dxe = cross3(delta, e2);
    let edotn = dot3(e1, dxe);
    if edotn.abs() < 0.00001f32 {
        return None;
    }
    let f = 1.0f32 / edotn;
    let tts = sub3(start, p0);
    let u = f * dot3(tts, dxe);
    if u < 0.0 || u > 1.0 {
        return None;
    }
    let q = cross3(tts, e1);
    let v = f * dot3(delta, q);
    if v < 0.0 || u + v > 1.0 {
        return None;
    }
    let t = f * dot3(e2, q);
    if t < 0.0 || t > 1.0 {
        return None;
    }
    Some(t)
}

pub(crate) struct TriMeshHit {
    pub t: f32,
    pub pos: Vector3f,
    pub tri: i32,
}

/// # Safety
/// `indices` has `index_count` readable entries; each `index * stride + 2` addresses `vertices`.
pub(crate) unsafe fn line_tri_mesh_intersection(
    start: Vector3f,
    end: Vector3f,
    inv_mesh: &Matrixf,
    indices: *const u32,
    index_count: i32,
    vertices: *const f32,
    stride: i32,
    skip_backfacing: bool,
) -> Option<TriMeshHit> {
    let local_start = matrix_mul_vec(inv_mesh, start);
    let local_end = matrix_mul_vec(inv_mesh, end);
    let mut min_t = 99999.0f32;
    let mut tri = -1i32;
    let vtx = |idx: i32| {
        // SAFETY: caller contract.
        unsafe {
            let i = *indices.offset(idx as isize);
            let p = vertices.offset(i.wrapping_mul(stride as u32) as i32 as isize);
            v3(*p, *p.add(1), *p.add(2))
        }
    };
    let mut idx = 0i32;
    while idx < index_count {
        let (p0, p1, p2) = (vtx(idx), vtx(idx + 1), vtx(idx + 2));
        if let Some(t) =
            line_triangle_intersection(local_start, local_end, p0, p1, p2, skip_backfacing)
            && t < min_t
        {
            min_t = t;
            tri = idx;
        }
        idx = idx.wrapping_add(3);
    }
    if min_t < 0.0 || min_t > 1.0 {
        return None;
    }
    let pos = line_point(start, sub3(end, start), min_t);
    Some(TriMeshHit { t: min_t, pos, tri })
}

pub(crate) fn vector3_max_length(v: Vector3f, max: f32) -> Vector3f {
    let l = len3(v);
    if l > max { mul3(div3(v, l), max) } else { v }
}

pub(crate) fn vector3_min_length(v: Vector3f, min: f32) -> Vector3f {
    let l = len3(v);
    if l < min { mul3(div3(v, l), min) } else { v }
}

pub(crate) fn vector3_clamp_length(v: Vector3f, min: f32, max: f32) -> Vector3f {
    let l = len3(v);
    if l < min {
        mul3(div3(v, l), min)
    } else if l > max {
        mul3(div3(v, l), max)
    } else {
        v
    }
}

pub(crate) fn vector3_angle_distance(a: Vector3f, b: Vector3f, max: f32) -> Vector3f {
    v3(
        angle_distance(a.x, b.x, max),
        angle_distance(a.y, b.y, max),
        angle_distance(a.z, b.z, max),
    )
}

pub(crate) fn vector3_project(src: Vector3f, dest: Vector3f) -> Vector3f {
    mul3(dest, dot3(src, dest) / dot3(dest, dest))
}

pub(crate) fn vector3_reflect(v: Vector3f, n: Vector3f) -> Vector3f {
    let d = dot3(v, n);
    let r = |v: f32, n: f32| fma(n * d, -2.0, v);
    v3(r(v.x, n.x), r(v.y, n.y), r(v.z, n.z))
}

pub(crate) fn vector3_angle(a: Vector3f, b: Vector3f) -> f32 {
    let c = dot3(a, b);
    if (c - 1.0f32).abs() <= EPSILON {
        return 0.0;
    }
    acos(c)
}

pub(crate) fn vector3_unproject(v: Vector3f, screen: &Rect2f, view_proj: &Matrixf) -> Vector3f {
    let inv = matrix_inverse(view_proj);
    let n = v3(
        ((v.x - screen.x) * 2.0f32 / screen.w) - 1.0f32,
        -(((v.y - screen.y) * 2.0f32 / screen.h) - 1.0f32),
        fma(2.0f32, v.z, -1.0f32),
    );
    let n = matrix_mul_divide_w(&inv, n);
    mul3(n, -1.0)
}

pub(crate) fn vector3_increase_to(x: Vector3f, add: Vector3f, dest: Vector3f) -> Vector3f {
    v3(
        increase_to(x.x, add.x, dest.x),
        increase_to(x.y, add.y, dest.y),
        increase_to(x.z, add.z, dest.z),
    )
}

pub(crate) fn vector3_clamp_to_length(v: &mut Vector3f, max: f32) {
    if max <= 0.0 {
        return;
    }
    let l = len3(*v);
    if l > max {
        *v = mul3(div3(*v, l), max);
    }
}

/// Returns `(angles, error)`; `error` mirrors gcc's "Error in GetAngle 3D code" branch.
pub(crate) fn angle_from_points_3d(start: Vector3f, goal: Vector3f) -> (Vector3f, bool) {
    let y = -angle_from_points_2d(
        Vector2f {
            x: start.x,
            y: start.z,
        },
        Vector2f {
            x: goal.x,
            y: goal.z,
        },
    );
    let rot = rotate_y(-y);
    let delta = sub3(goal, start);
    let g = matrix_mul_vec(&rot, delta);
    let mut d = sub3(v3(g.z, g.y, 0.0), v3(0.0, 0.0, 0.0));
    if d.x == 0.0 {
        d.x = -EPSILON;
    }
    if d.y == 0.0 {
        d.y = EPSILON;
    }
    let (x, err) = if (d.y >= 0.0 && d.x <= 0.0) || (d.y < 0.0 && d.x <= 0.0) {
        (-atan(d.y / d.x), false)
    } else {
        (-1000.0, true)
    };
    (v3(wrap(x, 0.0, TWO_PI), y, 0.0), err)
}

pub(crate) fn orthonormalize_basis(a1: Vector3f, a2: Vector3f, a3: Vector3f) -> [Vector3f; 3] {
    let mut d1 = a1;
    normalize3(&mut d1);
    let sub_proj = |v: Vector3f, from: Vector3f, onto: Vector3f| {
        let t = dot3(from, onto) / dot3(onto, onto);
        v3(v.x - onto.x * t, v.y - onto.y * t, v.z - onto.z * t)
    };
    let u2 = sub_proj(a2, a2, a1);
    let mut d2 = u2;
    normalize3(&mut d2);
    let u3 = sub_proj(sub_proj(a3, a3, a1), a3, u2);
    let mut d3 = u3;
    normalize3(&mut d3);
    [d1, d2, d3]
}
