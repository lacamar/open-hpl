//! C ABI for `cMath` / `cQuaternion`, declared in `math/RustMath.h`.
//!
//! Aggregates (`cVector*`, `cMatrixf`, `cPlanef`, `cQuaternion`) cross as pointers: `cVector3` has
//! a user-provided copy constructor, so the C++ ABI would pass it by hidden reference anyway.
//! Unless noted, every pointer is non-null, valid and aligned for the duration of the call, and
//! output pointers do not alias inputs.

use std::slice;

use super::geom::{self, FrustumView};
use super::matrix::{self, IDENTITY};
use super::scalar;
use super::{Matrixf, Planef, Quaternion, Rect2f, Rect2l, Vector2f, Vector2l, Vector3f, tangents};

macro_rules! ffi {
    ($(fn $name:ident($($a:ident: $t:ty),*) $(-> $r:ty)? $body:block)*) => {$(
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name($($a: $t),*) $(-> $r)? {
            #[allow(unused_unsafe)]
            unsafe { $body }
        }
    )*};
}

fn slice_of<'a, T>(p: *const T, n: i32) -> &'a [T] {
    if p.is_null() || n <= 0 {
        &[]
    } else {
        // SAFETY: caller passes `n` valid elements at `p`.
        unsafe { slice::from_raw_parts(p, n as usize) }
    }
}

ffi! {
fn ohpl_math_rand_rectl(min: i32, max: i32) -> i32 { scalar::rand_rectl(min, max) }
fn ohpl_math_rand_rectf(min: f32, max: f32) -> f32 { scalar::rand_rectf(min, max) }
fn ohpl_math_fast_random_float(seed: i32) -> f32 { scalar::fast_random_float(seed) }
fn ohpl_math_rand_rect_vector2f(min: *const Vector2f, max: *const Vector2f, out: *mut Vector2f) {
    let x = scalar::rand_rectf((*min).x, (*max).x);
    let y = scalar::rand_rectf((*min).y, (*max).y);
    *out = Vector2f { x, y };
}
fn ohpl_math_rand_rect_vector3f(min: *const Vector3f, max: *const Vector3f, out: *mut Vector3f) {
    let x = scalar::rand_rectf((*min).x, (*max).x);
    let y = scalar::rand_rectf((*min).y, (*max).y);
    let z = scalar::rand_rectf((*min).z, (*max).z);
    *out = Vector3f { x, y, z };
}
fn ohpl_math_rand_rect_color(min: *const [f32; 4], max: *const [f32; 4], out: *mut [f32; 4]) {
    let (a, b) = (&*min, &*max);
    let x = scalar::rand_rectf(a[0], b[0]);
    let y = scalar::rand_rectf(a[1], b[1]);
    let z = scalar::rand_rectf(a[2], b[2]);
    let w = scalar::rand_rectf(a[3], b[3]);
    *out = [x, y, z, w];
}
fn ohpl_math_randomize(seed: i32) { scalar::randomize(seed) }
fn ohpl_math_get_flag_bit(bit: i32) -> i32 { scalar::get_flag_bit(bit) }

fn ohpl_math_expand_aabb(bmin: *mut Vector3f, bmax: *mut Vector3f, amin: *const Vector3f, amax: *const Vector3f) {
    geom::expand_aabb(&mut *bmin, &mut *bmax, *amin, *amax)
}
fn ohpl_math_check_point_in_rect(p: *const Vector2f, r: *const Rect2f) -> bool {
    geom::point_in_rect(*p, &*r)
}
fn ohpl_math_check_rect_fit_l(s: *const Rect2l, d: *const Rect2l) -> bool { geom::rect_fit_l(&*s, &*d) }
fn ohpl_math_check_rect_fit_f(s: *const Rect2f, d: *const Rect2f) -> bool { geom::rect_fit_f(&*s, &*d) }
fn ohpl_math_get_clip_rect(s: *const Rect2f, d: *const Rect2f, out: *mut Rect2f) {
    *out = geom::clip_rect(&*s, &*d)
}
fn ohpl_math_check_sphere_line_intersection(
    center: *const Vector3f, radius: f32, start: *const Vector3f, end: *const Vector3f,
    t1: *mut f32, p1: *mut Vector3f, t2: *mut f32, p2: *mut Vector3f
) -> bool {
    let (hit, r1, r2) = geom::sphere_line(*center, radius, *start, *end);
    if let Some((t, p)) = r1 {
        if !t1.is_null() { *t1 = t; }
        if let (Some(p), false) = (p, p1.is_null()) { *p1 = p; }
    }
    if let Some((t, p)) = r2 {
        if !t2.is_null() { *t2 = t; }
        if let (Some(p), false) = (p, p2.is_null()) { *p2 = p; }
    }
    hit
}
fn ohpl_math_check_aabb_intersection(a: *const Vector3f, b: *const Vector3f, c: *const Vector3f, d: *const Vector3f) -> bool {
    geom::aabb_intersection(*a, *b, *c, *d)
}
fn ohpl_math_check_aabb_inside(a: *const Vector3f, b: *const Vector3f, c: *const Vector3f, d: *const Vector3f) -> bool {
    geom::aabb_inside(*a, *b, *c, *d)
}
fn ohpl_math_check_point_in_aabb(p: *const Vector3f, min: *const Vector3f, max: *const Vector3f) -> bool {
    geom::point_in_aabb(*p, *min, *max)
}
fn ohpl_math_check_point_inside_triangle_2d(p: *const Vector2f, tri: *const Vector3f) -> bool {
    geom::point_inside_triangle_2d(*p, &*tri.cast::<[Vector3f; 3]>())
}
fn ohpl_math_check_point_same_side_of_edge_2d(p1: *const Vector3f, p2: *const Vector3f, a: *const Vector3f, b: *const Vector3f) -> bool {
    geom::point_same_side_of_edge_2d(*p1, *p2, *a, *b)
}
fn ohpl_math_get_clip_rect_from_normalized_min_max(min: *const Vector3f, max: *const Vector3f, screen: *const Vector2l, out: *mut Rect2l) {
    *out = geom::clip_rect_from_normalized_min_max(*min, *max, *screen)
}
fn ohpl_math_get_normalized_clip_rect_from_bv(
    bv_min: *const Vector3f, bv_max: *const Vector3f, view: *const Matrixf, proj: *const Matrixf,
    perspective: bool, fov: f32, near: f32, aspect: f32, tan_half_fov: f32,
    dest_min: *mut Vector3f, dest_max: *mut Vector3f
) -> bool {
    let f = FrustumView { view: &*view, proj: &*proj, perspective, fov, near, aspect };
    match geom::normalized_clip_rect_from_bv(*bv_min, *bv_max, &f, tan_half_fov) {
        Some((mn, mx)) => { *dest_min = mn; *dest_max = mx; true }
        None => false,
    }
}
fn ohpl_math_get_clip_rect_from_sphere(
    pos: *const Vector3f, radius: f32, view: *const Matrixf, fov: f32, near: f32, aspect: f32,
    screen: *const Vector2l, pos_is_screen_space: bool, tan_half_fov: f32, out: *mut Rect2l
) {
    let f = FrustumView { view: &*view, proj: &IDENTITY, perspective: true, fov, near, aspect };
    *out = geom::clip_rect_from_sphere(*pos, radius, &f, *screen, pos_is_screen_space, tan_half_fov)
}
fn ohpl_math_check_aabb_line_intersection(
    min: *const Vector3f, max: *const Vector3f, start: *const Vector3f, end: *const Vector3f,
    pos: *mut Vector3f, t: *mut f32
) -> bool {
    let mut hit_pos = None;
    let (hit, shortest) = geom::aabb_line_intersection(*min, *max, *start, *end, &mut hit_pos);
    if let (Some(p), false) = (hit_pos, pos.is_null()) { *pos = p; }
    if !t.is_null() { *t = shortest; }
    hit
}

fn ohpl_math_round_float_to_decimals(v: f32, p: i32) -> f32 { scalar::round_float_to_decimals(v, p) }
fn ohpl_math_get_fraction(v: f32) -> f32 { scalar::get_fraction(v) }
fn ohpl_math_modulus(a: f32, b: f32) -> f32 { scalar::modulus(a, b) }
fn ohpl_math_log2_to_int(x: i32) -> i32 { scalar::log2_to_int(x) }
fn ohpl_math_is_pow2(x: i32) -> bool { scalar::is_pow2(x) }
fn ohpl_math_wrap(x: f32, min: f32, max: f32) -> f32 { scalar::wrap(x, min, max) }
fn ohpl_math_clamp(x: f32, min: f32, max: f32) -> f32 { scalar::clamp(x, min, max) }
fn ohpl_math_get_angle_distance(a: f32, b: f32, max: f32) -> f32 { scalar::angle_distance(a, b, max) }
fn ohpl_math_turn_angle(a: f32, f: f32, speed: f32, max: f32) -> f32 { scalar::turn_angle(a, f, speed, max) }
fn ohpl_math_to_rad(a: f32) -> f32 { scalar::to_rad(a) }
fn ohpl_math_to_deg(a: f32) -> f32 { scalar::to_deg(a) }
fn ohpl_math_interpolate(a: f32, b: f32, t: f32) -> f32 { scalar::interpolate(a, b, t) }
fn ohpl_math_increase_to(x: f32, add: f32, dest: f32) -> f32 { scalar::increase_to(x, add, dest) }
fn ohpl_math_smooth_curve(x: f32) -> f32 { scalar::smooth_curve(x) }

fn ohpl_math_vector2_dist(a: *const Vector2f, b: *const Vector2f) -> f32 {
    scalar::vector2_dist((*a).x, (*a).y, (*b).x, (*b).y)
}
fn ohpl_math_vector2_dist_xy(a: *const Vector3f, b: *const Vector3f) -> f32 {
    scalar::vector2_dist((*a).x, (*a).y, (*b).x, (*b).y)
}
fn ohpl_math_vector2_dist_xz(a: *const Vector3f, b: *const Vector3f) -> f32 {
    scalar::vector2_dist((*a).x, (*a).z, (*b).x, (*b).z)
}
fn ohpl_math_vector2_dist_sqr(a: *const Vector2f, b: *const Vector2f) -> f32 {
    scalar::vector2_dist_sqr((*a).x, (*a).y, (*b).x, (*b).y)
}
fn ohpl_math_vector2_dist_sqr_xy(a: *const Vector3f, b: *const Vector3f) -> f32 {
    scalar::vector2_dist_sqr((*a).x, (*a).y, (*b).x, (*b).y)
}
fn ohpl_math_vector2_dist_sqr_xz(a: *const Vector3f, b: *const Vector3f) -> f32 {
    scalar::vector2_dist_sqr((*a).x, (*a).z, (*b).x, (*b).z)
}
fn ohpl_math_vector2_increase_to(x: *const Vector2f, add: *const Vector2f, dest: *const Vector2f, out: *mut Vector2f) {
    *out = Vector2f {
        x: scalar::increase_to((*x).x, (*add).x, (*dest).x),
        y: scalar::increase_to((*x).y, (*add).y, (*dest).y),
    }
}
fn ohpl_math_get_angle_from_points_2d(a: *const Vector2f, b: *const Vector2f) -> f32 {
    scalar::angle_from_points_2d(*a, *b)
}
fn ohpl_math_get_vector_from_angle_2d(angle: f32, len: f32, out: *mut Vector2f) {
    *out = scalar::vector_from_angle_2d(angle, len)
}
fn ohpl_math_get_angle_from_vector(v: *const Vector2f, angle: *mut f32, len: *mut f32) {
    let (a, l) = scalar::angle_from_vector(*v);
    *len = l;
    *angle = a;
}
fn ohpl_math_project_vector_2d(s: *const Vector2f, d: *const Vector2f, out: *mut Vector2f) {
    *out = scalar::project_vector_2d(*s, *d)
}

fn ohpl_math_vector3_max_length(v: *const Vector3f, m: f32, out: *mut Vector3f) { *out = geom::vector3_max_length(*v, m) }
fn ohpl_math_vector3_min_length(v: *const Vector3f, m: f32, out: *mut Vector3f) { *out = geom::vector3_min_length(*v, m) }
fn ohpl_math_vector3_clamp_length(v: *const Vector3f, lo: f32, hi: f32, out: *mut Vector3f) {
    *out = geom::vector3_clamp_length(*v, lo, hi)
}
fn ohpl_math_vector3_angle_distance(a: *const Vector3f, b: *const Vector3f, max: f32, out: *mut Vector3f) {
    *out = geom::vector3_angle_distance(*a, *b, max)
}
fn ohpl_math_vector3_cross(a: *const Vector3f, b: *const Vector3f, out: *mut Vector3f) {
    *out = super::cross3(*a, *b)
}
fn ohpl_math_vector3_dot(a: *const Vector3f, b: *const Vector3f) -> f32 { super::dot3(*a, *b) }
fn ohpl_math_vector3_project(s: *const Vector3f, d: *const Vector3f, out: *mut Vector3f) {
    *out = geom::vector3_project(*s, *d)
}
fn ohpl_math_vector3_reflect(v: *const Vector3f, n: *const Vector3f, out: *mut Vector3f) {
    *out = geom::vector3_reflect(*v, *n)
}
fn ohpl_math_vector3_angle(a: *const Vector3f, b: *const Vector3f) -> f32 { geom::vector3_angle(*a, *b) }
fn ohpl_math_vector3_unproject(v: *const Vector3f, rect: *const Rect2f, vp: *const Matrixf, out: *mut Vector3f) {
    *out = geom::vector3_unproject(*v, &*rect, &*vp)
}
fn ohpl_math_vector3_increase_to(x: *const Vector3f, add: *const Vector3f, dest: *const Vector3f, out: *mut Vector3f) {
    *out = geom::vector3_increase_to(*x, *add, *dest)
}
fn ohpl_math_vector3_clamp_to_length(v: *mut Vector3f, max: f32) { geom::vector3_clamp_to_length(&mut *v, max) }
fn ohpl_math_get_angle_from_points_3d(start: *const Vector3f, goal: *const Vector3f, out: *mut Vector3f) -> bool {
    let (angles, err) = geom::angle_from_points_3d(*start, *goal);
    *out = angles;
    err
}
fn ohpl_math_vector3_orthonormalize_basis(
    a1: *const Vector3f, a2: *const Vector3f, a3: *const Vector3f,
    d1: *mut Vector3f, d2: *mut Vector3f, d3: *mut Vector3f
) {
    let [r1, r2, r3] = geom::orthonormalize_basis(*a1, *a2, *a3);
    *d1 = r1;
    *d2 = r2;
    *d3 = r3;
}
fn ohpl_math_get_spherical_coords_from_point_3d(c: *const Vector3f, p: *const Vector3f, out: *mut Vector2f) {
    *out = geom::spherical_coords_from_point(*c, *p)
}
fn ohpl_math_get_point_3d_from_spherical_coords(c: *const Vector3f, r: f32, s: *const Vector2f, out: *mut Vector3f) {
    *out = geom::point_from_spherical_coords(*c, r, *s)
}

fn ohpl_math_plane_dot(a: *const Planef, b: *const Planef) -> f32 { matrix::plane_dot(&*a, &*b) }
fn ohpl_math_plane_parallel(a: *const Planef, b: *const Planef) -> bool { geom::plane_parallel(&*a, &*b) }
fn ohpl_math_transform_plane(m: *const Matrixf, p: *const Planef, out: *mut Planef) {
    *out = geom::transform_plane(&*m, &*p)
}
fn ohpl_math_plane_to_point_dist(p: *const Planef, v: *const Vector3f) -> f32 { super::plane_dist(&*p, *v) }
fn ohpl_math_plane_intersection_line(a: *const Planef, b: *const Planef, dir: *mut Vector3f, point: *mut Vector3f) {
    let (d, p) = geom::plane_intersection_line(&*a, &*b);
    *dir = d;
    (*point).x = p.x;
    (*point).y = p.y;
    (*point).z = p.z;
}
fn ohpl_math_plane_intersection_point(a: *const Planef, b: *const Planef, c: *const Planef, out: *mut Vector3f) -> bool {
    match geom::plane_intersection_point(&*a, &*b, &*c) {
        Some(p) => { *out = p; true }
        None => false,
    }
}
fn ohpl_math_check_plane_sphere_collision(p: *const Planef, c: *const Vector3f, r: f32) -> i32 {
    geom::plane_sphere_collision(&*p, *c, r)
}
fn ohpl_math_check_plane_aabb_collision(p: *const Planef, min: *const Vector3f, max: *const Vector3f) -> i32 {
    geom::plane_aabb_collision(&*p, *min, *max)
}
fn ohpl_math_check_plane_aabb_sphere_collision(
    p: *const Planef, min: *const Vector3f, max: *const Vector3f, c: *const Vector3f, r: f32
) -> i32 {
    geom::plane_aabb_sphere_collision(&*p, *min, *max, *c, r)
}
fn ohpl_math_check_plane_line_intersection(
    p: *const Planef, start: *const Vector3f, end: *const Vector3f, pos: *mut Vector3f, t: *mut f32
) -> bool {
    let (hit, r) = geom::plane_line_intersection(&*p, *start, *end);
    if let Some((rt, rp)) = r {
        if !t.is_null() { *t = rt; }
        if !pos.is_null() { *pos = rp; }
    }
    hit
}
fn ohpl_math_check_points_planes_collision(v: *const Vector3f, nv: i32, p: *const Planef, np: i32) -> i32 {
    geom::points_planes_collision(slice_of(v, nv), slice_of(p, np))
}
fn ohpl_math_check_points_aabb_planes_collision(v: *const Vector3f, nv: i32, min: *const Vector3f, max: *const Vector3f) -> i32 {
    geom::points_aabb_planes_collision(slice_of(v, nv), *min, *max)
}
fn ohpl_math_check_separating_axis_intersection(
    va: *const Vector3f, nva: i32, pa: *const Planef, npa: i32,
    vb: *const Vector3f, nvb: i32, pb: *const Planef, npb: i32
) -> bool {
    geom::separating_axis_intersection(
        [slice_of(va, nva), slice_of(vb, nvb)],
        [slice_of(pa, npa), slice_of(pb, npb)],
    )
}
fn ohpl_math_check_line_tri_mesh_intersection(
    start: *const Vector3f, end: *const Vector3f, inv: *const Matrixf,
    indices: *const u32, index_count: i32, vertices: *const f32, stride: i32,
    pos: *mut Vector3f, t: *mut f32, tri: *mut i32, skip: bool
) -> bool {
    match geom::line_tri_mesh_intersection(*start, *end, &*inv, indices, index_count, vertices, stride, skip) {
        Some(h) => {
            if !t.is_null() { *t = h.t; }
            if !pos.is_null() { *pos = h.pos; }
            if !tri.is_null() { *tri = h.tri; }
            true
        }
        None => false,
    }
}

fn ohpl_math_quaternion_dot(a: *const Quaternion, b: *const Quaternion) -> f32 { matrix::quat_dot(&*a, &*b) }
fn ohpl_math_quaternion_mul(a: *const Quaternion, b: *const Quaternion, out: *mut Quaternion) {
    *out = matrix::quat_mul(&*a, &*b)
}
fn ohpl_math_quaternion_slerp(t: f32, a: *const Quaternion, b: *const Quaternion, shortest: bool, out: *mut Quaternion) {
    *out = matrix::quat_slerp(t, &*a, &*b, shortest)
}
fn ohpl_math_matrix_slerp(t: f32, a: *const Matrixf, b: *const Matrixf, out: *mut Matrixf) {
    *out = matrix::matrix_slerp(t, &*a, &*b)
}
fn ohpl_math_matrix_mul(a: *const Matrixf, b: *const Matrixf, out: *mut Matrixf) {
    *out = matrix::matrix_mul(&*a, &*b)
}
fn ohpl_math_matrix_mul_vec(a: *const Matrixf, b: *const Vector3f, out: *mut Vector3f) {
    *out = matrix::matrix_mul_vec(&*a, *b)
}
fn ohpl_math_matrix_mul_3x3(a: *const Matrixf, b: *const Vector3f, out: *mut Vector3f) {
    *out = matrix::matrix_mul_3x3(&*a, *b)
}
fn ohpl_math_matrix_mul_divide_w(a: *const Matrixf, b: *const Vector3f, out: *mut Vector3f) {
    *out = matrix::matrix_mul_divide_w(&*a, *b)
}
fn ohpl_math_matrix_mul_scalar(a: *const Matrixf, s: f32, out: *mut Matrixf) {
    *out = matrix::matrix_mul_scalar(&*a, s)
}
fn ohpl_math_matrix_rotate(rot: *const Vector3f, order: i32, out: *mut Matrixf) {
    *out = matrix::matrix_rotate(*rot, order)
}
fn ohpl_math_matrix_rotate_x(a: f32, out: *mut Matrixf) { *out = matrix::rotate_x(a) }
fn ohpl_math_matrix_rotate_y(a: f32, out: *mut Matrixf) { *out = matrix::rotate_y(a) }
fn ohpl_math_matrix_rotate_z(a: f32, out: *mut Matrixf) { *out = matrix::rotate_z(a) }
fn ohpl_math_matrix_scale(s: *const Vector3f, out: *mut Matrixf) { *out = matrix::matrix_scale(*s) }
fn ohpl_math_matrix_translate(t: *const Vector3f, out: *mut Matrixf) { *out = matrix::matrix_translate(*t) }
fn ohpl_math_matrix_quaternion(q: *const Quaternion, out: *mut Matrixf) { *out = matrix::matrix_quaternion(&*q) }
fn ohpl_math_matrix_to_quaternion(m: *const Matrixf, out: *mut Quaternion) { *out = matrix::matrix_to_quaternion(&*m) }
fn ohpl_math_matrix_plane_mirror(p: *const Planef, out: *mut Matrixf) { *out = matrix::matrix_plane_mirror(&*p) }
fn ohpl_math_matrix_to_euler_angles(m: *const Matrixf, out: *mut Vector3f) { *out = matrix::matrix_to_euler_angles(&*m) }
fn ohpl_math_matrix_euler_angle_distance(a: *const Matrixf, b: *const Matrixf, out: *mut Vector3f) {
    *out = matrix::matrix_euler_angle_distance(&*a, &*b)
}
fn ohpl_math_matrix_unit_vectors(
    r: *const Vector3f, u: *const Vector3f, f: *const Vector3f, t: *const Vector3f, out: *mut Matrixf
) {
    *out = matrix::matrix_unit_vectors(*r, *u, *f, *t)
}
fn ohpl_math_projection_matrix_oblique_near_clip_plane(m: *const Matrixf, p: *const Planef, out: *mut Matrixf) {
    *out = matrix::oblique_near_clip_plane(&*m, &*p)
}
fn ohpl_math_matrix_perspective_projection(near: f32, far: f32, fov: f32, aspect: f32, inf: bool, out: *mut Matrixf) {
    *out = matrix::perspective_projection(near, far, fov, aspect, inf)
}
fn ohpl_math_matrix_orthographic_projection(near: f32, far: f32, view: *const Vector2f, out: *mut Matrixf) {
    *out = matrix::orthographic_projection(near, far, *view)
}
fn ohpl_math_create_tri_tangent_vectors(
    dest: *mut f32, indices: *const u32, index_count: i32, vertices: *const f32, stride: i32,
    tex: *const f32, normals: *const f32, vertex_count: i32
) {
    tangents::create_tri_tangent_vectors(dest, indices, index_count, vertices, stride, tex, normals, vertex_count)
}

fn ohpl_quaternion_normalize(q: *mut Quaternion) { matrix::quat_normalize(&mut *q) }
fn ohpl_quaternion_to_rotation_matrix(q: *const Quaternion, m: *mut Matrixf) {
    matrix::quat_to_rotation_matrix(&*q, &mut *m)
}
fn ohpl_quaternion_from_rotation_matrix(q: *mut Quaternion, m: *const Matrixf) {
    matrix::quat_from_rotation_matrix(&mut *q, &*m)
}
fn ohpl_quaternion_from_angle_axis(q: *mut Quaternion, angle: f32, axis: *const Vector3f) {
    matrix::quat_from_angle_axis(&mut *q, angle, *axis)
}
fn ohpl_quaternion_add(a: *const Quaternion, b: *const Quaternion, out: *mut Quaternion) {
    *out = matrix::quat_add(&*a, &*b)
}
fn ohpl_quaternion_sub(a: *const Quaternion, b: *const Quaternion, out: *mut Quaternion) {
    *out = matrix::quat_sub(&*a, &*b)
}
fn ohpl_quaternion_mul(a: *const Quaternion, b: *const Quaternion, out: *mut Quaternion) {
    *out = matrix::quat_mul(&*a, &*b)
}
fn ohpl_quaternion_div(a: *const Quaternion, b: *const Quaternion, out: *mut Quaternion) {
    *out = matrix::quat_div(&*a, &*b)
}
fn ohpl_quaternion_scale(a: *const Quaternion, s: f32, out: *mut Quaternion) {
    *out = matrix::quat_scale(&*a, s)
}
}
