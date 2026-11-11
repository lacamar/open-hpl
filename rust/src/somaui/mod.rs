//! SOMA/Rebirth/Bunker UI and visual game layer: critters (`SomaCritter.cpp`), tone mapping,
//! gamma screen, splash, loader decisions, post effects and the ImGui core. Object creation, GL,
//! script registrations and callbacks stay C++ and are reached through the tables in the `ffi_*`
//! modules.

pub mod critter;
mod ffi_critter;
mod ffi_imgui;
mod ffi_loaders;
mod ffi_postfx;
mod ffi_screens;
mod ffi_tonemap;
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
