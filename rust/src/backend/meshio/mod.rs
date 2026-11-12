//! Mesh loaders (L6): Rust reads, parses and decides; the C++ builder callbacks
//! (`HPL2/core/sources/impl/MeshLoaderRustBuilder.cpp`) create the engine objects. Active with
//! `OHPL_IMPL=rust`; the retained C++ shim is `cshim/assimp_shim.cpp`.

pub mod abi;
mod assimp;
mod collada;
mod ffi;
pub mod msh;

#[cfg(test)]
mod tests;
