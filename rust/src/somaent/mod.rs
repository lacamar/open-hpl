//! SOMA Lux entity layer (`SomaLuxEntity.cpp`): move/rotate state machines, effect and liquid
//! fades, procedural animations, instance-variable parsing, damage and break decisions and the
//! oriented-box geometry. Object creation, physics, mesh and script calls stay C++, which replays
//! the ops and plans returned here.

pub mod effects;
mod ffi;
pub mod geom;
pub mod logic;
pub mod motion;
pub mod procanim;
pub mod props;
pub mod s3;
#[cfg(test)]
mod s3_tests;
#[cfg(test)]
mod tests;
