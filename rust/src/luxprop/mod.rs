//! Amnesia prop core (`LuxProp`, `_Object`, `_SwingDoor`, `_Wheel`, `_Lever`, `_Lamp`,
//! `_MultiSlider`, `_MoveObject`): `.ent` variable parsing, state machines, limits, health and
//! break decisions, effect timing and move maths. Joints, bodies, meshes, sounds, particles and
//! script calls stay C++: Rust issues them as ordered `Op`s that C++ applies immediately. A script
//! that re-enters the prop rewrites its POD state, which Rust re-reads after each such op.

mod base;
mod ffi;
mod lamp;
mod lever;
mod moveobject;
mod multislider;
mod object;
mod ops;
mod swingdoor;
#[cfg(test)]
mod tests;
mod vars;
mod wheel;

pub use ops::{Op, Ops, Pod, Probe, op};
pub use vars::Vars;
