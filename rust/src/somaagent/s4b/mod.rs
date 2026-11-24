//! SOMA agent objects owned by Rust: registry, components, update driver, natives; engine objects
//! (bodies, meshes, physics, scripts, nodes) are reached through the host ops in `io`.

mod api;
mod comps;
mod core;
mod dump;
mod ffi;
mod host;
pub mod io;
mod model;
mod natives;
mod natives_c;
mod pod;

#[cfg(test)]
mod tests;
