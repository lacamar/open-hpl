//! Engine core: updater order, frame-loop state, FPS counter, setup variables, multiple-settings
//! blending, save-object registry and the DevHud layout/JSON. Object creation, vtable calls, SDL
//! and file I/O stay in C++.

pub mod devhud;
mod ffi;
mod ffi_devhud;
pub mod frame;
pub mod h22_engine;
mod h22_engine_ffi;
pub mod h22_hc;
mod h22_hc_ffi;
pub mod multi_settings;
pub mod save;
pub mod setup_vars;
pub mod updater;

#[cfg(test)]
mod h22_tests;
#[cfg(test)]
mod tests;
