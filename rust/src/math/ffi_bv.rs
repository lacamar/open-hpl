//! C ABI for `cBoundingVolume` / `cFrustum`, declared in `math/RustBv.h`.
//!
//! `Bv*` points at the first field of the C++ object (`m_mtxTransform`), `Frustum*` at the object.
//! Every pointer is non-null, valid and aligned; vector and matrix arguments do not alias outputs.

use super::bv::{Bv, BvObj, Frustum, Spheref};
use super::{Matrixf, Vector2f, Vector3f};

macro_rules! ffi {
    ($(fn $name:ident($($a:ident: $t:ty),*) $(-> $r:ty)? $body:block)*) => {$(
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name($($a: $t),*) $(-> $r)? {
            #[allow(unused_unsafe)]
            unsafe { $body }
        }
    )*};
}

ffi! {
fn ohpl_bv_init(bv: *mut Bv) { std::ptr::write(bv, Bv::new()) }
fn ohpl_bv_copy(dst: *mut Bv, src: *const Bv) { std::ptr::write(dst, (*src).clone()) }
fn ohpl_bv_assign(dst: *mut Bv, src: *const Bv) {
    if !std::ptr::eq(dst, src) {
        *dst = (*src).clone();
    }
}
fn ohpl_bv_drop(bv: *mut Bv) { (*bv).pending = None }
fn ohpl_bv_update_size(bv: *mut Bv) { (*bv).update_size() }
fn ohpl_bv_set_local_min_max(bv: *mut Bv, min: *const Vector3f, max: *const Vector3f) {
    (*bv).set_local_min_max(*min, *max)
}
fn ohpl_bv_set_size(bv: *mut Bv, size: *const Vector3f) { (*bv).set_size(*size) }
fn ohpl_bv_set_position(bv: *mut Bv, pos: *const Vector3f) { (*bv).set_position(*pos) }
fn ohpl_bv_set_transform(bv: *mut Bv, m: *const Matrixf) { (*bv).set_transform(&*m) }
fn ohpl_bv_local_center(bv: *const Bv, out: *mut Vector3f) { *out = (*bv).local_center() }
fn ohpl_bv_world_center(bv: *mut Bv, out: *mut Vector3f) { *out = (*bv).world_center() }
fn ohpl_bv_add_array_points(bv: *mut Bv, array: *const f32, count: i32) {
    (*bv).add_array_points(array, count)
}
fn ohpl_bv_create_from_points(bv: *mut Bv, stride: i32) { (*bv).create_from_points(stride) }

fn ohpl_frustum_init(f: *mut Frustum) { (*f).init() }
fn ohpl_frustum_setup_perspective(
    f: *mut Frustum, proj: *const Matrixf, view: *const Matrixf, far: f32, near: f32, fov: f32,
    aspect: f32, origin: *const Vector3f, inf: bool, custom_far: *const Matrixf, oblique: bool
) {
    (*f).setup_perspective(&*proj, &*view, far, near, fov, aspect, *origin, inf, custom_far.as_ref(), oblique)
}
fn ohpl_frustum_setup_ortho(
    f: *mut Frustum, proj: *const Matrixf, view: *const Matrixf, far: f32, near: f32,
    view_size: *const Vector2f, origin: *const Vector3f, inf: bool
) {
    (*f).setup_ortho(&*proj, &*view, far, near, *view_size, *origin, inf)
}
fn ohpl_frustum_collide_point(f: *mut Frustum, p: *const Vector3f) -> bool {
    (*f).sync();
    (*f).collide_point(*p)
}
fn ohpl_frustum_collide_bv(f: *const Frustum, bv: *mut Bv) -> i32 { (*f).collide_bv(&mut *bv) }
fn ohpl_frustum_collide_node(f: *const Frustum, c: *const Vector3f, r: f32, min: *const Vector3f, max: *const Vector3f) -> i32 {
    (*f).collide_node(*c, r, *min, *max)
}
fn ohpl_frustum_collide_frustum(f: *mut Frustum, g: *mut Frustum) -> i32 {
    (*f).sync();
    (*g).sync();
    (*f).collide_frustum(&*g)
}
fn ohpl_frustum_collide_frustum_sphere(f: *const Frustum, c: *const Vector3f, r: f32) -> i32 {
    (*f).collide_frustum_sphere(*c, r)
}
fn ohpl_frustum_collide_sphere(f: *const Frustum, c: *const Vector3f, r: f32, max_planes: i32) -> i32 {
    (*f).collide_sphere(*c, r, max_planes)
}
fn ohpl_frustum_collide_aabb(f: *const Frustum, min: *const Vector3f, max: *const Vector3f) -> i32 {
    (*f).collide_aabb(*min, *max)
}
fn ohpl_frustum_check_sphere_near_plane(f: *const Frustum, c: *const Vector3f, r: f32) -> bool {
    (*f).check_sphere_near_plane(*c, r)
}
fn ohpl_frustum_check_aabb_near_plane(f: *const Frustum, min: *const Vector3f, max: *const Vector3f) -> bool {
    (*f).check_aabb_near_plane(*min, *max)
}
fn ohpl_frustum_check_bv_near_plane(f: *const Frustum, bv: *mut Bv) -> bool { (*f).check_bv_near_plane(&mut *bv) }
fn ohpl_frustum_check_frustum_near_plane(f: *const Frustum, g: *const Frustum) -> bool {
    (*f).check_frustum_near_plane(&*g)
}
fn ohpl_frustum_check_line(f: *mut Frustum, start: *const Vector3f, end: *const Vector3f, out: *mut Vector3f) -> bool {
    (*f).sync();
    (*f).check_line(*start, *end, &mut *out)
}
}

const _: () = {
    use std::mem::{align_of, offset_of, size_of};
    assert!(size_of::<Bv>() == 192 && align_of::<Bv>() == 8);
    assert!(offset_of!(Bv, radius) == 172 && offset_of!(Bv, position_updated) == 176);
    assert!(offset_of!(Bv, pending) == 184);
    assert!(size_of::<BvObj>() == 200 && offset_of!(BvObj, bv) == 8);
    assert!(size_of::<Spheref>() == 16 && offset_of!(Spheref, center) == 4);
    assert!(size_of::<Frustum>() == 848 && offset_of!(Frustum, inf_far) == 24);
    assert!(offset_of!(Frustum, projection) == 28 && offset_of!(Frustum, origin) == 32);
    assert!(offset_of!(Frustum, origin_bv) == 48 && offset_of!(Frustum, proj) == 248);
    assert!(offset_of!(Frustum, plane) == 440 && offset_of!(Frustum, sphere) == 536);
    assert!(offset_of!(Frustum, bounding_volume) == 552 && offset_of!(Frustum, vertices) == 752);
};
