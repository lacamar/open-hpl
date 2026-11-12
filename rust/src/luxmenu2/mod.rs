//! Amnesia main menu (GA7b): window stack, fades and exit flow, top menu plan, start/load game and
//! custom story windows, premenu sequencing, credits, demo end and load screen timing. Widgets,
//! draws, fonts, sound and engine calls stay in C++ and run at their original points.

pub mod credits;
mod ffi;
pub mod loadscreen;
pub mod menu;
pub mod pages;
pub mod premenu;
#[cfg(test)]
mod tests;
