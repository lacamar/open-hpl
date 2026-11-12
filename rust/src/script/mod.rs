//! Script engine registration and runtime (L7).
//!
//! The engine is AngelScript (C++ library, add-ons in C++) reached through `cshim/as_shim.cpp`.
//! `engine` is the registration API for Rust code; `sqscript` is the `cSqScript` port.

pub mod engine;
mod ffi;
mod selftest;
pub mod sqscript;
pub mod sys;
