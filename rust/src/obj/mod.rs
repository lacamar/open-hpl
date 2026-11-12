//! Generational handles and arenas for objects owned by Rust after the cut-over.

mod arena;

pub use arena::{Arena, Handle};
