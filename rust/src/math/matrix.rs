//! Matrices and quaternions.
#![allow(
    clippy::indexing_slicing,
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::panic,
    clippy::unreachable,
    clippy::integer_division_remainder_used,
    clippy::string_slice,
    reason = "panic audit pending (open-hpl-smj.23)"
)]

use super::libm::{acos, asin, atan2, cos, sin, tan};
use super::scalar::clamp;
use super::{Matrixf, Planef, Quaternion, Vector2f, Vector3f, fma, v3};

const fn mat(m: [[f32; 4]; 4]) -> Matrixf {
    Matrixf { m }
}

pub(crate) const IDENTITY: Matrixf = mat([
    [1.0, 0.0, 0.0, 0.0],
    [0.0, 1.0, 0.0, 0.0],
    [0.0, 0.0, 1.0, 0.0],
    [0.0, 0.0, 0.0, 1.0],
]);

pub(crate) fn transpose(a: &Matrixf) -> Matrixf {
    let mut c = [[0.0f32; 4]; 4];
    for (i, row) in c.iter_mut().enumerate() {
        for (j, e) in row.iter_mut().enumerate() {
            *e = a.m[j][i];
        }
    }
    mat(c)
}

pub(crate) fn matrix_mul(a: &Matrixf, b: &Matrixf) -> Matrixf {
    let (a, b) = (&a.m, &b.m);
    let mut c = [[0.0f32; 4]; 4];
    for i in 0..4 {
        for j in 0..4 {
            c[i][j] = fma(
                a[i][3],
                b[3][j],
                fma(a[i][2], b[2][j], fma(a[i][0], b[0][j], a[i][1] * b[1][j])),
            );
        }
    }
    mat(c)
}

pub(crate) fn matrix_mul_vec(a: &Matrixf, b: Vector3f) -> Vector3f {
    let m = &a.m;
    let row = |r: usize| fma(m[r][2], b.z, fma(m[r][0], b.x, m[r][1] * b.y)) + m[r][3];
    v3(row(0), row(1), row(2))
}

pub(crate) fn matrix_mul_3x3(a: &Matrixf, b: Vector3f) -> Vector3f {
    let m = &a.m;
    let row = |r: usize| fma(m[r][2], b.z, fma(m[r][0], b.x, m[r][1] * b.y));
    v3(row(0), row(1), row(2))
}

pub(crate) fn matrix_mul_divide_w(a: &Matrixf, b: Vector3f) -> Vector3f {
    let m = &a.m;
    let inv_w = 1.0f32 / (fma(m[3][2], b.z, fma(m[3][0], b.x, m[3][1] * b.y)) + m[3][3]);
    let row = |r: usize| (fma(m[r][2], b.z, fma(m[r][0], b.x, m[r][1] * b.y)) + m[r][3]) * inv_w;
    v3(row(0), row(1), row(2))
}

pub(crate) fn matrix_mul_scalar(a: &Matrixf, s: f32) -> Matrixf {
    let mut c = a.m;
    for e in c.iter_mut().flatten() {
        *e *= s;
    }
    mat(c)
}

pub(crate) fn rotate_x(a: f32) -> Matrixf {
    let (s, c) = (sin(a), cos(a));
    mat([
        [1.0, 0.0, 0.0, 0.0],
        [0.0, c, -s, 0.0],
        [0.0, s, c, 0.0],
        [0.0, 0.0, 0.0, 1.0],
    ])
}

pub(crate) fn rotate_y(a: f32) -> Matrixf {
    let (s, c) = (sin(a), cos(a));
    mat([
        [c, 0.0, s, 0.0],
        [0.0, 1.0, 0.0, 0.0],
        [-s, 0.0, c, 0.0],
        [0.0, 0.0, 0.0, 1.0],
    ])
}

pub(crate) fn rotate_z(a: f32) -> Matrixf {
    let (s, c) = (sin(a), cos(a));
    mat([
        [c, -s, 0.0, 0.0],
        [s, c, 0.0, 0.0],
        [0.0, 0.0, 1.0, 0.0],
        [0.0, 0.0, 0.0, 1.0],
    ])
}

pub(crate) fn matrix_rotate(rot: Vector3f, order: i32) -> Matrixf {
    let axes: [u8; 3] = match order {
        0 => *b"XYZ",
        1 => *b"XZY",
        2 => *b"YXZ",
        3 => *b"YZX",
        4 => *b"ZXY",
        5 => *b"ZYX",
        _ => return IDENTITY,
    };
    let mut m = IDENTITY;
    for axis in axes {
        let r = match axis {
            b'X' => rotate_x(rot.x),
            b'Y' => rotate_y(rot.y),
            _ => rotate_z(rot.z),
        };
        m = matrix_mul(&r, &m);
    }
    m
}

pub(crate) fn matrix_scale(s: Vector3f) -> Matrixf {
    mat([
        [s.x, 0.0, 0.0, 0.0],
        [0.0, s.y, 0.0, 0.0],
        [0.0, 0.0, s.z, 0.0],
        [0.0, 0.0, 0.0, 1.0],
    ])
}

pub(crate) fn matrix_translate(t: Vector3f) -> Matrixf {
    mat([
        [1.0, 0.0, 0.0, t.x],
        [0.0, 1.0, 0.0, t.y],
        [0.0, 0.0, 1.0, t.z],
        [0.0, 0.0, 0.0, 1.0],
    ])
}

pub(crate) fn matrix_unit_vectors(r: Vector3f, u: Vector3f, f: Vector3f, t: Vector3f) -> Matrixf {
    mat([
        [r.x, u.x, f.x, t.x],
        [r.y, u.y, f.y, t.y],
        [r.z, u.z, f.z, t.z],
        [0.0, 0.0, 0.0, 1.0],
    ])
}

const PAIRS: [(usize, usize); 6] = [(0, 1), (0, 2), (0, 3), (1, 2), (1, 3), (2, 3)];

#[inline(always)]
const fn pair(a: usize, b: usize) -> usize {
    match (a, b) {
        (0, 1) => 0,
        (0, 2) => 1,
        (0, 3) => 2,
        (1, 2) => 3,
        (1, 3) => 4,
        _ => 5,
    }
}

/// 2x2 sub-determinants, indexed `[row pair][column pair]`; gcc shares them across the 16 minors.
struct Det2([[f32; 6]; 6]);

impl Det2 {
    fn new(a: &Matrixf) -> Self {
        let m = &a.m;
        let mut d = [[0.0f32; 6]; 6];
        for (row, &(r1, r2)) in d.iter_mut().zip(&PAIRS) {
            for (e, &(ca, cb)) in row.iter_mut().zip(&PAIRS) {
                *e = fma(m[r1][ca], m[r2][cb], -(m[r2][ca] * m[r1][cb]));
            }
        }
        Self(d)
    }

    #[inline(always)]
    fn minor_parts(&self, a: &Matrixf, r: [usize; 3], c: [usize; 3]) -> (f32, f32, f32) {
        let m = &a.m;
        let [r0, r1, r2] = r;
        let [c0, c1, c2] = c;
        let rp = &self.0[pair(r1, r2)];
        let (t0, t1, t2) = (rp[pair(c1, c2)], rp[pair(c0, c2)], rp[pair(c0, c1)]);
        let inner = fma(m[r0][c0], t0, -(m[r0][c1] * t1));
        (inner, m[r0][c2], t2)
    }

    #[inline(always)]
    fn minor(&self, a: &Matrixf, r: [usize; 3], c: [usize; 3]) -> f32 {
        let (inner, m2, t2) = self.minor_parts(a, r, c);
        fma(m2, t2, inner)
    }

    /// `-minor` as gcc folds the negation into the last fused op (matters for signed zeros).
    #[inline(always)]
    fn minor_neg(&self, a: &Matrixf, r: [usize; 3], c: [usize; 3]) -> f32 {
        let (inner, m2, t2) = self.minor_parts(a, r, c);
        fma(-m2, t2, -inner)
    }
}

fn adjoint(a: &Matrixf, d: &Det2) -> Matrixf {
    let mn = |r: [usize; 3], c: [usize; 3]| d.minor(a, r, c);
    let ng = |r: [usize; 3], c: [usize; 3]| d.minor_neg(a, r, c);
    let (r0, r1, r2, r3) = ([1, 2, 3], [0, 2, 3], [0, 1, 3], [0, 1, 2]);
    let (c0, c1, c2, c3) = ([1, 2, 3], [0, 2, 3], [0, 1, 3], [0, 1, 2]);
    mat([
        [mn(r0, c0), ng(r1, c0), mn(r2, c0), ng(r3, c0)],
        [ng(r0, c1), mn(r1, c1), ng(r2, c1), mn(r3, c1)],
        [mn(r0, c2), ng(r1, c2), mn(r2, c2), ng(r3, c2)],
        [ng(r0, c3), mn(r1, c3), ng(r2, c3), mn(r3, c3)],
    ])
}

fn determinant(a: &Matrixf, d: &Det2) -> f32 {
    let m = &a.m;
    let mn = |c: [usize; 3]| d.minor(a, [1, 2, 3], c);
    let s1 = fma(m[0][0], mn([1, 2, 3]), -(m[0][1] * mn([0, 2, 3])));
    let s2 = fma(m[0][2], mn([0, 1, 3]), s1);
    fma(-m[0][3], mn([0, 1, 2]), s2)
}

pub(crate) fn matrix_inverse(a: &Matrixf) -> Matrixf {
    let d = Det2::new(a);
    matrix_mul_scalar(&adjoint(a, &d), 1.0f32 / determinant(a, &d))
}

pub(crate) fn matrix_plane_mirror(p: &Planef) -> Matrixf {
    let (a, b, c, d) = (p.a, p.b, p.c, p.d);
    mat([
        [
            fma(-(2.0 * a), a, 1.0),
            -2.0 * b * a,
            -2.0 * c * a,
            -2.0 * a * d,
        ],
        [
            -2.0 * a * b,
            fma(-(2.0 * b), b, 1.0),
            -2.0 * c * b,
            -2.0 * b * d,
        ],
        [
            -2.0 * a * c,
            -2.0 * b * c,
            fma(-(2.0 * c), c, 1.0),
            -2.0 * c * d,
        ],
        [0.0, 0.0, 0.0, 1.0],
    ])
}

pub(crate) fn matrix_to_euler_angles(a: &Matrixf) -> Vector3f {
    v3(
        atan2(a.m[2][1], a.m[2][2]),
        -asin(clamp(a.m[2][0], -1.0, 1.0)),
        atan2(a.m[1][0], a.m[0][0]),
    )
}

fn normalize2(v: &mut Vector2f) {
    let length = fma(v.x, v.x, v.y * v.y).sqrt();
    if f64::from(length) > 1e-08 {
        let inv = 1.0f32 / length;
        v.x *= inv;
        v.y *= inv;
    }
}

pub(crate) fn matrix_euler_angle_distance(a: &Matrixf, b: &Matrixf) -> Vector3f {
    let local_a = matrix_mul(&transpose(b), a);
    let local_t = transpose(&local_a);
    let mut fwd = v3(local_t.m[2][0], local_t.m[2][1], local_t.m[2][2]);
    let mut up = v3(local_t.m[1][0], local_t.m[1][1], local_t.m[1][2]);
    let mut out = v3(0.0, 0.0, 0.0);

    if !(fwd.y.abs() < 0.0001 && fwd.z.abs() < 0.0001) {
        let mut yz = Vector2f { x: fwd.y, y: fwd.z };
        normalize2(&mut yz);
        out.x = acos(clamp(yz.y, -1.0, 1.0));
        if yz.x < 0.0 {
            out.x = -out.x;
        }
    }
    let rx = rotate_x(out.x);
    // gcc folds the constant rotation: only x and z are used, z's last term is not fused.
    let (sx, cx) = (rx.m[2][1], rx.m[2][2]);
    fwd = v3(
        fma(0.0, fwd.z, fma(0.0, fwd.y, fwd.x)),
        fwd.y,
        fma(0.0, fwd.x, fwd.y * sx) + fwd.z * cx,
    );

    if !(fwd.x.abs() < 0.0001 && fwd.z.abs() < 0.0001) {
        let mut xz = Vector2f { x: fwd.x, y: fwd.z };
        normalize2(&mut xz);
        out.y = acos(clamp(xz.y, -1.0, 1.0));
        if xz.x > 0.0 {
            out.y = -out.y;
        }
    }
    let rot = matrix_mul(&rotate_y(out.y), &rx);
    let row = |r: usize| {
        fma(
            rot.m[r][2],
            up.z,
            fma(rot.m[r][0], up.x, rot.m[r][1] * up.y),
        )
    };
    up = v3(row(0), row(1), up.z);

    if !(up.y.abs() < 0.0001 && up.x.abs() < 0.0001) {
        let mut xy = Vector2f { x: up.x, y: up.y };
        normalize2(&mut xy);
        out.z = acos(clamp(xy.y, -1.0, 1.0));
        if xy.x < 0.0 {
            out.z = -out.z;
        }
    }
    matrix_mul_3x3(b, out)
}

fn sign(x: f32) -> f32 {
    f32::from(i8::from(x > 0.0) - i8::from(x < 0.0))
}

pub(crate) fn plane_dot(a: &Planef, b: &Planef) -> f32 {
    fma(a.d, b.d, fma(a.c, b.c, fma(a.a, b.a, a.b * b.b)))
}

pub(crate) fn oblique_near_clip_plane(proj: &Matrixf, clip: &Planef) -> Matrixf {
    let p = &proj.m;
    let q = Planef {
        a: (sign(clip.a) + p[0][2]) / p[0][0],
        b: (sign(clip.b) + p[1][2]) / p[1][1],
        c: -1.0,
        d: (1.0f32 + p[2][2]) / p[2][3],
    };
    let k = 2.0f32 / plane_dot(clip, &q);
    let c = Planef {
        a: clip.a * k,
        b: clip.b * k,
        c: clip.c * k,
        d: clip.d * k,
    };
    let mut out = *proj;
    out.m[2][0] = c.a;
    out.m[2][1] = c.b;
    out.m[2][2] = fma(clip.c, k, 1.0f32);
    out.m[2][3] = c.d;
    out
}

pub(crate) fn perspective_projection(
    near: f32,
    far: f32,
    fov: f32,
    aspect: f32,
    inf: bool,
) -> Matrixf {
    let top = tan(fov * 0.5f32) * near;
    let bottom = -top;
    let right = aspect * top;
    let left = aspect * bottom;
    let a = (2.0f32 * near) / (right - left);
    let b = (2.0f32 * near) / (top - bottom);
    let (c, z) = if inf {
        (-2.0f32 * near, -1.0f32)
    } else {
        (
            -(2.0f32 * far * near) / (far - near),
            -(far + near) / (far - near),
        )
    };
    mat([
        [a, 0.0, 0.0, 0.0],
        [0.0, b, 0.0, 0.0],
        [0.0, 0.0, z, c],
        [0.0, 0.0, -1.0, 0.0],
    ])
}

pub(crate) fn orthographic_projection(near: f32, far: f32, view: Vector2f) -> Matrixf {
    let a = 2.0f32 / view.x;
    let b = 2.0f32 / view.y;
    let c = -2.0f32 / (far - near);
    let z = -(far + near) / (far - near);
    mat([
        [a, 0.0, 0.0, 0.0],
        [0.0, b, 0.0, 0.0],
        [0.0, 0.0, c, z],
        [0.0, 0.0, 0.0, 1.0],
    ])
}

pub(crate) fn quat_normalize(q: &mut Quaternion) {
    let len = fma(
        q.v.z,
        q.v.z,
        fma(q.v.y, q.v.y, fma(q.w, q.w, q.v.x * q.v.x)),
    );
    let f = 1.0f32 / len.sqrt();
    q.v.x *= f;
    q.v.y *= f;
    q.v.z *= f;
    q.w *= f;
}

pub(crate) fn quat_to_rotation_matrix(q: &Quaternion, m: &mut Matrixf) {
    let (x, y, z, w) = (q.v.x, q.v.y, q.v.z, q.w);
    let tx = 2.0f32 * x;
    let ty = 2.0f32 * y;
    let tz = 2.0f32 * z;
    let txy = ty * x;
    let txz = tz * x;
    let tyy = ty * y;
    let tyz = tz * y;
    let tzz = tz * z;
    m.m[0][0] = 1.0f32 - (tyy + tzz);
    m.m[0][1] = fma(-tz, w, txy);
    m.m[0][2] = fma(ty, w, txz);
    m.m[1][0] = fma(tz, w, txy);
    m.m[1][1] = 1.0f32 - fma(tx, x, tzz);
    m.m[1][2] = fma(-tx, w, tyz);
    m.m[2][0] = fma(-ty, w, txz);
    m.m[2][1] = fma(tx, w, tyz);
    m.m[2][2] = 1.0f32 - fma(tx, x, tyy);
}

pub(crate) fn quat_from_rotation_matrix(q: &mut Quaternion, a: &Matrixf) {
    let m = &a.m;
    let trace = m[0][0] + m[1][1] + m[2][2];
    if f64::from(trace) > 0.0 {
        let mut root = (trace + 1.0f32).sqrt();
        q.w = 0.5f32 * root;
        root = 0.5f32 / root;
        q.v.x = (m[2][1] - m[1][2]) * root;
        q.v.y = (m[0][2] - m[2][0]) * root;
        q.v.z = (m[1][0] - m[0][1]) * root;
    } else {
        const NEXT: [usize; 3] = [1, 2, 0];
        let mut i = 0;
        if m[1][1] > m[0][0] {
            i = 1;
        }
        if m[2][2] > m[i][i] {
            i = 2;
        }
        let j = NEXT[i];
        let k = NEXT[j];
        let mut root = (m[i][i] - m[j][j] - m[k][k] + 1.0f32).sqrt();
        let mut out = [q.v.x, q.v.y, q.v.z];
        out[i] = 0.5f32 * root;
        root = 0.5f32 / root;
        q.w = (m[k][j] - m[j][k]) * root;
        out[j] = (m[j][i] + m[i][j]) * root;
        out[k] = (m[k][i] + m[i][k]) * root;
        q.v = v3(out[0], out[1], out[2]);
    }
}

pub(crate) fn quat_from_angle_axis(q: &mut Quaternion, angle: f32, axis: Vector3f) {
    let half = 0.5f32 * angle;
    let s = sin(half);
    q.w = cos(half);
    q.v = v3(s * axis.x, s * axis.y, s * axis.z);
}

pub(crate) fn quat_add(a: &Quaternion, b: &Quaternion) -> Quaternion {
    Quaternion {
        v: v3(a.v.x + b.v.x, a.v.y + b.v.y, a.v.z + b.v.z),
        w: a.w + b.w,
    }
}

pub(crate) fn quat_sub(a: &Quaternion, b: &Quaternion) -> Quaternion {
    Quaternion {
        v: v3(a.v.x - b.v.x, a.v.y - b.v.y, a.v.z - b.v.z),
        w: a.w - b.w,
    }
}

pub(crate) fn quat_div(a: &Quaternion, b: &Quaternion) -> Quaternion {
    Quaternion {
        v: v3(a.v.x / b.v.x, a.v.y / b.v.y, a.v.z / b.v.z),
        w: a.w / b.w,
    }
}

pub(crate) fn quat_scale(a: &Quaternion, s: f32) -> Quaternion {
    Quaternion {
        v: v3(a.v.x * s, a.v.y * s, a.v.z * s),
        w: a.w * s,
    }
}

pub(crate) fn quat_mul(a: &Quaternion, b: &Quaternion) -> Quaternion {
    let (av, bv) = (a.v, b.v);
    Quaternion {
        w: fma(-av.z, bv.z, fma(-av.y, bv.y, fma(a.w, b.w, -(av.x * bv.x)))),
        v: v3(
            fma(-av.z, bv.y, fma(av.y, bv.z, fma(a.w, bv.x, av.x * b.w))),
            fma(-av.x, bv.z, fma(av.z, bv.x, fma(a.w, bv.y, av.y * b.w))),
            fma(-av.y, bv.x, fma(av.x, bv.y, fma(a.w, bv.z, av.z * b.w))),
        ),
    }
}

pub(crate) fn quat_dot(a: &Quaternion, b: &Quaternion) -> f32 {
    fma(
        a.v.z,
        b.v.z,
        fma(a.v.y, b.v.y, fma(a.w, b.w, a.v.x * b.v.x)),
    )
}

pub(crate) fn quat_slerp(t: f32, a: &Quaternion, b: &Quaternion, shortest: bool) -> Quaternion {
    let cos_a = quat_dot(a, b);
    if (cos_a - 1.0f32).abs() <= super::EPSILON {
        let mut lerp = quat_add(&quat_scale(a, 1.0f32 - t), &quat_scale(b, t));
        quat_normalize(&mut lerp);
        return lerp;
    }
    let angle = acos(cos_a);
    let s = sin(angle);
    let inv = 1.0f32 / s;
    let mut c0 = sin((1.0f32 - t) * angle) * inv;
    let c1 = sin(t * angle) * inv;
    if cos_a < 0.0 && shortest {
        c0 = -c0;
        let mut q = quat_add(&quat_scale(a, c0), &quat_scale(b, c1));
        quat_normalize(&mut q);
        q
    } else {
        quat_add(&quat_scale(a, c0), &quat_scale(b, c1))
    }
}

pub(crate) fn matrix_quaternion(q: &Quaternion) -> Matrixf {
    let mut m = IDENTITY;
    m.m[0][3] = 0.0;
    m.m[1][3] = 0.0;
    m.m[2][3] = 0.0;
    m.m[3] = [0.0, 0.0, 0.0, 1.0];
    quat_to_rotation_matrix(q, &mut m);
    m
}

pub(crate) fn matrix_to_quaternion(m: &Matrixf) -> Quaternion {
    let mut q = Quaternion::default();
    quat_from_rotation_matrix(&mut q, m);
    q
}

pub(crate) fn matrix_slerp(t: f32, a: &Matrixf, b: &Matrixf) -> Matrixf {
    if t <= 0.0 {
        return *a;
    }
    if t >= 1.0 {
        return *b;
    }
    let k = 1.0f32 - t;
    let pos = v3(
        fma(a.m[0][3], k, b.m[0][3] * t),
        fma(a.m[1][3], k, b.m[1][3] * t),
        fma(a.m[2][3], k, b.m[2][3] * t),
    );
    let qa = matrix_to_quaternion(a);
    let qb = matrix_to_quaternion(b);
    let q = quat_slerp(t, &qa, &qb, true);
    let mut m = matrix_quaternion(&q);
    m.m[0][3] = pos.x;
    m.m[1][3] = pos.y;
    m.m[2][3] = pos.z;
    m
}
