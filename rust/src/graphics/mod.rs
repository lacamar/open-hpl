//! HPL2 graphics data: pixel formats, bitmap blit/clear, atlas rect packing, font layout and the
//! scatter-disk texture data. GPU objects stay in C++; this module only sees plain buffers.

pub mod bitmap;
mod ffi;
pub mod font;
pub mod formats;
pub mod packer;
pub mod scatter;
mod stdsort;
pub mod uv;

#[cfg(test)]
mod tests;
