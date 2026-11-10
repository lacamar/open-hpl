//! Backend-independent physics core: surface effects, joints, controllers, body bookkeeping.
//! The `iPhysics*` objects stay C++ and are reached through [`host::Host`] callbacks.

pub mod body;
pub mod controller;
mod ffi;
pub mod host;
pub mod joint;
mod newton_body;
mod newton_ffi;
mod newton_material;
pub mod pid;
pub mod surface;
pub mod surface_xml;
pub mod world;

#[cfg(test)]
mod tests;
