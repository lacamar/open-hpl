//! Script engine registration and runtime (L7).
//!
//! The engine is AngelScript (C++ library) reached through `cshim/as_shim.cpp`; the add-ons are in `addons`.
//! `engine` is the registration API for Rust code; `sqscript` is the `cSqScript` port.

pub mod addons;
pub mod engine;
mod ffi;
mod selftest;
pub mod sqscript;
pub mod sys;
