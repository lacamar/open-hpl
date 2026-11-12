//! Amnesia game base (`LuxBase`, `LuxConfigHandler`, `LuxInputHandler`, `LuxDebugHandler`, `Main`)
//! and the AMFP scaffold: init and config decisions, XDG path plan, input action table and
//! bindings, look maths, debug message state and the headless handlers. Engine objects, module
//! creation, GUI draws and script registrations stay C++ and are reached through the host traits.

pub mod amfp;
pub mod base;
pub mod cfg;
pub mod config;
pub mod debug;
mod ffi;
pub mod headless;
pub mod input;
pub mod look;
#[cfg(test)]
mod tests;
