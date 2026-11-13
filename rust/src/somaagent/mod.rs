//! SOMA/Rebirth/Bunker AI agents (`SomaAgent.cpp`): senses, door checks, auto-disable, mesh
//! sync, sound hearing, proximity and bark timing. Bodies, meshes, raycasts and scripts stay C++
//! behind the host table; script-visible effects come back as ordered `Event`s that C++ replays
//! after the step.

mod agent;
mod charmover;
mod ffi;
mod host;
mod misc;
mod pathfinder;
pub mod s4;
#[cfg(test)]
mod s4_tests;
mod statemachine;
#[cfg(test)]
mod tests;

use crate::math::{Vector3f, v3};

unsafe extern "C" {
    #[link_name = "acosf"]
    fn c_acosf(x: f32) -> f32;
}

fn acos(x: f32) -> f32 {
    // SAFETY: pure libm function on a plain float.
    unsafe { c_acosf(x) }
}

/// `cMath::Min(a, b)`.
pub(crate) fn cmin(a: f32, b: f32) -> f32 {
    if a < b { a } else { b }
}

/// `cMath::Max(a, b)`.
pub(crate) fn cmax(a: f32, b: f32) -> f32 {
    if a > b { a } else { b }
}

/// `std::min(a, b)`.
pub(crate) fn std_min(a: f32, b: f32) -> f32 {
    if b < a { b } else { a }
}

pub(crate) fn add3(a: Vector3f, b: Vector3f) -> Vector3f {
    v3(a.x + b.x, a.y + b.y, a.z + b.z)
}

/// `cMath::Vector3Dist(a, b)`.
pub(crate) fn dist3(a: Vector3f, b: Vector3f) -> f32 {
    crate::math::len3(crate::math::sub3(b, a))
}

/// `cMath::Vector3Angle`.
pub(crate) fn angle3(a: Vector3f, b: Vector3f) -> f32 {
    let c = crate::math::dot3(a, b);
    if (c - 1.0f32).abs() <= crate::math::EPSILON {
        return 0.0;
    }
    acos(c)
}

pub(crate) fn normalized(mut v: Vector3f) -> Vector3f {
    crate::math::normalize3(&mut v);
    v
}
