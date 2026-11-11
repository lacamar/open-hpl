//! Image loaders and video frame math (DDS parsing, DevIL mappings, Theora YUV, BMFont metrics).
//! DevIL, theora and ogg calls stay in C++.

pub mod dds;
pub mod devil;
mod ffi;
pub mod fontdata;
pub mod theora;

#[cfg(test)]
mod tests;
