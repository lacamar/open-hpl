//! Backend-independent physics core: surface effects, joints, controllers, body bookkeeping.
//! The `iPhysics*` objects stay C++ and are reached through [`host::Host`] callbacks.

pub mod body;
pub mod book;
mod char_aux;
pub mod character;
pub mod controller;
mod ffi;
mod ffi_character;
mod ffi_h14;
mod ffi_h15p;
mod ffi_h15w;
pub mod host;
pub mod joint;
pub(crate) mod newton_body;
mod newton_ffi;
pub(crate) mod newton_material;
pub mod pid;
pub mod preg;
pub mod rope;
pub mod surf;
pub mod surface;
pub mod surface_xml;
pub mod world;
pub mod wreg;

#[cfg(test)]
mod tests;
#[cfg(test)]
mod tests_character;
#[cfg(test)]
mod tests_h14;
#[cfg(test)]
mod tests_h15;
