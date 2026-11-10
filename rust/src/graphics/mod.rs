//! `cMeshCreator` geometry, `cAnimationTrack` key-frame lookup and `cSubMesh`/`cMesh` bone data.
//!
//! Vertex buffers stay in C++ (`iVertexBuffer` is a virtual interface); these modules work on
//! plain arrays and the C++ shims move the data in and out.

pub mod anim;
mod ffi;
pub mod meshgen;
pub mod submesh;
#[cfg(test)]
mod tests;
