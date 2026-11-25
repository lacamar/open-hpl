//! SOMA/Rebirth/Bunker AI agents (`SomaAgent.cpp`): senses, door checks, auto-disable, mesh
//! sync, sound hearing, proximity and bark timing. Bodies, meshes, raycasts and scripts stay C++
//! behind the host table; script-visible effects come back as ordered `Event`s that C++ replays
//! after the step.

mod agent;
mod charmover;
mod gridnav;
mod host;
mod misc;
mod pathfinder;
pub mod s4;
#[cfg(test)]
mod s4_tests;
mod statemachine;
#[cfg(test)]
mod tests;

pub(crate) use crate::math::{
    add3, cmath_max as cmax, cmath_min as cmin, dist3, normalized, std_min, vector3_angle as angle3,
};

pub mod s4b;
