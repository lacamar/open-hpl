//! Amnesia `LuxArea*`, `LuxAreaNodes`, `LuxMap`, `LuxMapHandler` and `LuxMapHelper`: area trigger
//! and update decisions, map timers/callbacks bookkeeping, map-change plans and map-helper math.
//! Object creation, world loading, physics/mesh/sound/effect calls and script callbacks stay C++.

pub mod area;
mod ffi;
pub mod handler;
pub mod helper;
pub mod map;
#[cfg(test)]
mod tests;
