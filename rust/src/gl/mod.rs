//! GL backend decisions (`LowLevelGraphicsSDL`, `SDLTexture`, `VertexBufferOGL*`, `FrameBufferGL`,
//! `GLSLProgram`). No GL symbol is linked here: enum tables, state filters and upload plans
//! come back as values or op lists and the C++ side issues every `gl*` call.

pub mod consts;
pub mod fbo;
pub mod ffi;
pub mod glsl;
pub mod state;
pub mod tables;
#[cfg(test)]
mod tests;
pub mod texture;
pub mod vbuf;

pub use consts as c;
