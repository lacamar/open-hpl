//! HPL2 graphics data: pixel formats, bitmap blit/clear, atlas rect packing, font layout, the
//! scatter-disk texture data, `cMeshCreator` geometry, `cAnimationTrack` key frames and
//! `cSubMesh`/`cMesh` bone data, and the `cSkeleton`/`cBone`, `cAnimationTrack` and `cMesh` index state
//! behind `OHPL_IMPL=rust` (`skel`, `animdata`). GPU objects and vertex buffers stay in C++; these modules only
//! see plain buffers.

pub mod anim;
pub mod animdata;
pub mod atlas;
pub mod bitmap;
pub mod combo;
pub mod decal;
mod ffi;
mod ffi_decal;
mod ffi_h5;
mod ffi_h6;
mod ffi_mesh;
pub mod font;
pub mod formats;
pub mod h5b_ffi;
pub mod h5b_font;
pub mod h5b_foreign;
pub mod h5b_gfx;
pub mod h5b_gl;
pub mod h5b_imm;
pub mod h5b_ops;
pub mod h5b_scn;
pub mod h5b_trace;
pub mod meshgen;
pub mod packer;
pub mod scatter;
pub mod skel;
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
mod tests_h5b;
#[cfg(test)]
mod tests_h5b_font;
#[cfg(test)]
mod tests_h5b_imm;
#[cfg(test)]
mod tests_h6;
#[cfg(test)]
mod tests_mesh;
