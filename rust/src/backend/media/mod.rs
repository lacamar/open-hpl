//! OpenAL, DevIL, fonts and video (L5): the library calls and object ownership behind
//! `OHPL_IMPL=rust`. Decision logic lives in `crate::imageload`, `crate::sound` and `crate::xml`;
//! the retained C shims are `cshim/{oal,theora,vorbis}_shim`.

pub mod audio;
pub mod bitmap;
mod cfile;
pub mod env;
pub mod font;
pub mod log;
mod media_ffi;
pub mod oal;
pub mod snd;
mod snd_ffi;
pub mod video;

#[cfg(test)]
mod tests;
