//! Backend-independent physics core: surface effects, joints, controllers, body bookkeeping.
//! The `iPhysics*` objects stay C++ and are reached through [`host::Host`] callbacks.

pub mod body;
pub mod book;
pub mod character;
pub mod controller;
mod ffi;
mod ffi_character;
pub mod host;
pub mod joint;
pub(crate) mod newton_body;
mod newton_ffi;
pub(crate) mod newton_material;
pub mod pid;
pub mod rope;
pub mod surface;
pub mod surface_xml;
pub mod world;

#[cfg(test)]
mod tests;
#[cfg(test)]
mod tests_character;
