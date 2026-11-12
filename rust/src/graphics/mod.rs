//! HPL2 graphics data: pixel formats, bitmap blit/clear, atlas rect packing, font layout, the
//! scatter-disk texture data, `cMeshCreator` geometry, `cAnimationTrack` key frames and
//! `cSubMesh`/`cMesh` bone data. GPU objects and vertex buffers stay in C++; these modules only
//! see plain buffers.

pub mod anim;
pub mod atlas;
pub mod bitmap;
pub mod combo;
pub mod decal;
mod ffi;
mod ffi_decal;
mod ffi_h5;
mod ffi_mesh;
pub mod font;
pub mod formats;
pub mod meshgen;
pub mod packer;
pub mod scatter;
pub(crate) mod stdsort;
pub mod submesh;
pub mod uv;

#[cfg(test)]
mod tests;
#[cfg(test)]
mod tests_decal;
#[cfg(test)]
mod tests_h5;
#[cfg(test)]
mod tests_mesh;
