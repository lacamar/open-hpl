//! SOMA/Rebirth/Bunker save layer and base boot decisions (`SomaSave.cpp`, `SomaBase.cpp`,
//! `SomaConfig.cpp`, `Main.cpp`): the save byte format, slot naming and rotation, game detection,
//! settings and path decisions. Engine objects, script registrations and file I/O stay C++.

pub mod base;
pub mod codec;
mod ffi;
pub mod header;
pub mod names;
pub mod rec;
pub mod script;
#[cfg(test)]
mod tests;
pub mod world;
