//! HPL2 `cMath` and `cQuaternion` out-of-line functions.
//!
//! Results are bit-identical to the gcc build, which contracts `a*b + c` into fused
//! multiply-adds (aarch64 `-ffp-contract=fast`): every `fma` below marks a site gcc fused.
//! Transcendentals call the same libm entry points gcc emits.

// Negated/split comparisons keep the C++ NaN behaviour; argument lists mirror the C++ signatures.
#![allow(
    clippy::manual_range_contains,
    clippy::neg_cmp_op_on_partial_ord,
    clippy::too_many_arguments,
    clippy::type_complexity
)]

mod ffi;
mod geom;
mod libm;
mod matrix;
mod scalar;
mod tangents;

pub use crate::types::{Matrixf, Quaternion, Vector2f, Vector2l, Vector3f};
pub(crate) use geom::expand_aabb;
pub(crate) use geom::rect_fit_l;
pub(crate) use geom::vector3_reflect;
pub(crate) use geom::{
    aabb_inside, aabb_line_intersection, plane_aabb_collision, plane_aabb_sphere_collision,
};
pub(crate) use geom::{point_in_aabb, points_aabb_planes_collision};
pub(crate) use libm::{acos, asin, cos, sin};
pub(crate) use libm::{atan2, tan};
#[cfg(test)]
pub(crate) use libm::{c_rand, c_srand};
pub(crate) use matrix::{
    IDENTITY, matrix_inverse, matrix_mul, matrix_mul_3x3, matrix_mul_vec, matrix_rotate,
    matrix_scale, rotate_z,
};
pub(crate) use matrix::{matrix_mul_divide_w, perspective_projection};
pub(crate) use matrix::{
    matrix_quaternion, matrix_unit_vectors, quat_dot, quat_from_angle_axis,
    quat_from_rotation_matrix,
};
pub(crate) use matrix::{matrix_translate, orthographic_projection, transpose};
pub(crate) use matrix::{quat_normalize, quat_scale, quat_slerp};
pub(crate) use scalar::angle_from_points_2d;
pub(crate) use scalar::{clamp, rand_rectf, round_float_to_decimals, to_rad};
pub(crate) use scalar::{fast_random_float, modulus, rand_rectl};
pub(crate) use scalar::{increase_to, wrap};

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Planef {
    pub a: f32,
    pub b: f32,
    pub c: f32,
    pub d: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Rect2<T> {
    pub x: T,
    pub y: T,
    pub w: T,
    pub h: T,
}

pub type Rect2f = Rect2<f32>;
pub type Rect2l = Rect2<i32>;

const _: () = {
    use std::mem::{offset_of, size_of};
    assert!(size_of::<Planef>() == 16 && offset_of!(Planef, d) == 12);
    assert!(size_of::<Rect2f>() == 16 && size_of::<Rect2l>() == 16);
};

pub(crate) const EPSILON: f32 = 0.0001;
pub(crate) const PI: f32 = std::f32::consts::PI;
pub(crate) const PI2: f32 = std::f32::consts::FRAC_PI_2;
pub(crate) const TWO_PI: f32 = std::f32::consts::TAU;

#[inline(always)]
pub(crate) fn fma(a: f32, b: f32, c: f32) -> f32 {
    a.mul_add(b, c)
}

pub(crate) const fn v3(x: f32, y: f32, z: f32) -> Vector3f {
    Vector3f { x, y, z }
}

pub(crate) fn sub3(a: Vector3f, b: Vector3f) -> Vector3f {
    v3(a.x - b.x, a.y - b.y, a.z - b.z)
}

pub(crate) fn mul3(a: Vector3f, s: f32) -> Vector3f {
    v3(a.x * s, a.y * s, a.z * s)
}

pub(crate) fn div3(a: Vector3f, s: f32) -> Vector3f {
    v3(a.x / s, a.y / s, a.z / s)
}

/// `a.x*b.x + a.y*b.y + a.z*b.z` as gcc contracts it.
#[inline(always)]
pub(crate) fn dot3(a: Vector3f, b: Vector3f) -> f32 {
    fma(a.z, b.z, fma(a.x, b.x, a.y * b.y))
}

#[inline(always)]
pub(crate) fn len3(v: Vector3f) -> f32 {
    dot3(v, v).sqrt()
}

pub(crate) fn normalize3(v: &mut Vector3f) -> f32 {
    let length = len3(*v);
    if f64::from(length) > 1e-08 {
        let inv = 1.0f32 / length;
        v.x *= inv;
        v.y *= inv;
        v.z *= inv;
    }
    length
}

pub(crate) fn cross3(a: Vector3f, b: Vector3f) -> Vector3f {
    v3(
        fma(a.y, b.z, -(a.z * b.y)),
        fma(a.z, b.x, -(a.x * b.z)),
        fma(a.x, b.y, -(a.y * b.x)),
    )
}

#[inline(always)]
pub(crate) fn plane_dist(p: &Planef, v: Vector3f) -> f32 {
    fma(p.c, v.z, fma(p.a, v.x, p.b * v.y)) + p.d
}

#[cfg(test)]
mod tests;
