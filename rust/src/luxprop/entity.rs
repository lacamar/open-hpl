//! `iLuxEntity`: connection state filtering and the player look-at test. Scripts, callbacks and
//! the line-of-sight ray stay in C++; the look-at test asks for them through `Los`.

use crate::math::{aabb_line_intersection, dot3, fma, normalize3, v3};
use crate::types::Vector3f;

/// `ConnectionStateChange` for one connection: the state to send, or `None` when it is skipped.
pub fn conn_state(state: i32, used: i32, invert: bool) -> Option<i32> {
    if used != 0 && state != used {
        return None;
    }
    Some(if invert { state.wrapping_neg() } else { state })
}

/// `UpdatePlayerLookAt` timer: true when this frame runs the test.
pub fn look_timer(count: &mut f32, dt: f32) -> bool {
    if *count > 0.0 {
        *count -= dt;
        return false;
    }
    *count = 0.3;
    true
}

/// The line-of-sight query the look-at test makes.
pub trait Los {
    fn line_of_sight(&mut self, start: Vector3f, end: Vector3f) -> bool;
}

/// The camera and the bounding volume of one body.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct LookIn {
    pub cam_pos: Vector3f,
    pub cam_fwd: Vector3f,
    pub cam_up: Vector3f,
    pub cam_right: Vector3f,
    pub bv_min: Vector3f,
    pub bv_max: Vector3f,
    pub bv_center: Vector3f,
    pub bv_radius: f32,
}

fn sub(a: Vector3f, b: Vector3f) -> Vector3f {
    v3(a.x - b.x, a.y - b.y, a.z - b.z)
}

fn add_scaled(a: Vector3f, b: Vector3f, s: f32) -> Vector3f {
    v3(fma(b.x, s, a.x), fma(b.y, s, a.y), fma(b.z, s, a.z))
}

fn sub_scaled(a: Vector3f, b: Vector3f, s: f32) -> Vector3f {
    v3(fma(-b.x, s, a.x), fma(-b.y, s, a.y), fma(-b.z, s, a.z))
}

/// Is this body looked at (after the frustum test passed)?
pub fn look_body(i: &LookIn, los: &mut dyn Los) -> bool {
    let start = i.cam_pos;
    let end = i.bv_center;
    let mut dir = sub(end, start);
    let sqr = dot3(dir, dir);
    if sqr > 50.0f32 * 50.0f32 {
        return false;
    }
    let centre_end = add_scaled(start, i.cam_fwd, 50.0);
    let mut hit_pos = None;
    if !aabb_line_intersection(i.bv_min, i.bv_max, start, centre_end, &mut hit_pos).0 {
        return false;
    }
    let sqr_radius = fma(i.bv_radius, i.bv_radius, 0.05);
    if sqr < sqr_radius {
        return true;
    }
    normalize3(&mut dir);
    let half = i.bv_radius * 0.5f32;
    let p0 = sub_scaled(end, dir, i.bv_radius);
    let pts = [
        p0,
        add_scaled(p0, i.cam_up, half),
        sub_scaled(p0, i.cam_up, half),
        add_scaled(p0, i.cam_right, half),
        sub_scaled(p0, i.cam_right, half),
    ];
    pts.iter().any(|&p| los.line_of_sight(start, p))
}
