//! GL context and low-level graphics (L2): window and context creation, capability flags from the
//! extension string, and every `gl*` call of `cLowLevelGraphicsSDL` and `cOcclusionQueryOGL`.
//! Decisions (state filters, enum tables, op lists) live in `crate::gl`.

pub mod caps;
pub mod ctx;
pub mod ffi;
pub mod oq;
#[cfg(test)]
pub(crate) mod tests;
