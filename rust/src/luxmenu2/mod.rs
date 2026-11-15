//! Amnesia menu code (GA7b, A10b). GA7b: main menu window stack, fades and exit flow, top menu plan,
//! start/load game and custom story windows; widgets, draws and engine calls stay in C++ there.
//! A10b: the pre-menu, credits, demo end, load screen and progress log classes run here in full;
//! their C++ classes are shells that forward to `a10b_ffi`, and engine objects are reached through
//! `host`.

pub mod credits;
mod ffi;
pub mod loadscreen;
pub mod menu;
pub mod pages;
pub mod premenu;
#[cfg(test)]
mod tests;

mod a10b;
mod a10b_cr;
mod a10b_ffi;
mod a10b_log;
mod a10b_ls;
mod a10b_pm;
pub mod host;
#[cfg(test)]
mod tests_a10b;
