//! SOMA/Rebirth/Bunker UI and visual game layer: critters (`SomaCritter.cpp`), tone mapping,
//! gamma screen, splash, loader decisions, post effects and the ImGui core. Object creation, GL,
//! script registrations and callbacks stay C++ and are reached through the tables in the `ffi_*`
//! modules.

pub mod critter;
mod ffi_critter;
mod ffi_loaders;
mod ffi_screens;
mod hw;
pub mod imgui;
pub mod imgui_widgets;
pub mod imgui_window;
pub mod loaders;
pub mod postfx;
pub mod screens;
#[cfg(test)]
mod tests;
pub mod tonemap;

mod ffi_s8;
pub mod s8;
#[cfg(test)]
mod s8_tests;
mod s8_trace;

mod ffi_s8b;
pub mod s8b_tone;

mod ffi_s8c;
pub mod s8c_pfx;
mod ffi_s8d;
pub mod s8d;
mod s8d_defaults;
mod s8d_hud;
mod s8d_layout;
mod s8d_natives;
mod s8d_sd;
pub mod s8d_structs;
#[cfg(test)]
mod s8d_tests;
mod s8d_text;
