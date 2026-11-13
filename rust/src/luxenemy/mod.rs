//! Amnesia enemy core (`LuxEnemy`, `LuxEnemyMover`, `LuxEnemyPathfinder`, `LuxEnemy_Grunt`):
//! message queue and state scheduler, senses, mover steering and move-state animation choice,
//! path following and node picking, and the Grunt state machine. Bodies, meshes, animations,
//! sounds, raycasts and node containers stay C++ behind the host table. Commands that can
//! re-enter the enemy come back as ordered `Cmd`s that C++ replays right after the handler
//! returns.

mod a4_enemy;
mod a4_ffi;
mod a4_state;
mod ctx;
mod ffi;
mod grunt;
mod host;
mod machine;
mod manpig;
mod mover;
mod pathfinder;
mod senses;
#[cfg(test)]
mod tests;
#[cfg(test)]
mod tests_ga3b;
mod waterlurker;

pub use host::{AnimReq, Cmd, Host};

use crate::math::{Matrixf, Vector2f, Vector3f, fma, v3};

unsafe extern "C" {
    fn ohpl_math_get_angle_distance(a: f32, b: f32, max: f32) -> f32;
    fn ohpl_math_get_angle_from_points_2d(a: *const Vector2f, b: *const Vector2f) -> f32;
    fn ohpl_math_get_angle_from_points_3d(
        s: *const Vector3f,
        g: *const Vector3f,
        out: *mut Vector3f,
    ) -> bool;
    safe fn ohpl_math_matrix_rotate_y(a: f32, out: &mut Matrixf);
}

pub(crate) const K2PI: f32 = 6.283_185_5;

pub(crate) fn angle_dist_rad(a: f32, b: f32) -> f32 {
    // SAFETY: pure function on plain floats.
    unsafe { ohpl_math_get_angle_distance(a, b, K2PI) }
}

pub(crate) fn angle_from_points_2d(sx: f32, sy: f32, gx: f32, gy: f32) -> f32 {
    let (s, g) = (Vector2f { x: sx, y: sy }, Vector2f { x: gx, y: gy });
    // SAFETY: pointers to live locals.
    unsafe { ohpl_math_get_angle_from_points_2d(&s, &g) }
}

pub(crate) fn angle_from_points_3d(s: Vector3f, g: Vector3f) -> Vector3f {
    let mut o = z3();
    // SAFETY: pointers to live locals.
    unsafe { ohpl_math_get_angle_from_points_3d(&s, &g, &mut o) };
    o
}

pub(crate) fn rot_y(a: f32) -> Matrixf {
    let mut m = crate::math::IDENTITY;
    ohpl_math_matrix_rotate_y(a, &mut m);
    m
}

/// `cMath::Min`.
pub(crate) fn cmin(a: f32, b: f32) -> f32 {
    if a < b { a } else { b }
}

/// `cMath::Vector3DistSqr` as gcc contracts the inline.
pub(crate) fn dist_sqr(a: Vector3f, b: Vector3f) -> f32 {
    let (dx, dy, dz) = (b.x - a.x, b.y - a.y, b.z - a.z);
    fma(dz, dz, fma(dx, dx, dy * dy))
}

pub(crate) fn dist3(a: Vector3f, b: Vector3f) -> f32 {
    dist_sqr(a, b).sqrt()
}

pub(crate) fn normalized(mut v: Vector3f) -> Vector3f {
    crate::math::normalize3(&mut v);
    v
}

pub(crate) fn sqr_len(v: Vector3f) -> f32 {
    crate::math::dot3(v, v)
}

pub(crate) const fn z3() -> Vector3f {
    v3(0.0, 0.0, 0.0)
}
