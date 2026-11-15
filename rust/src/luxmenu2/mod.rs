//! Amnesia menu code (GA7b, A10b). GA7b: main menu fades, exit flow, window stack and top menu plan
//! rules (driven by `luxmenu::mainmenu`).
//! A10b: the pre-menu, credits, demo end, load screen and progress log classes run here in full;
//! their C++ classes are shells that forward to `a10b_ffi`, and engine objects are reached through
//! `host`.

pub mod credits;
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
