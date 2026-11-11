//! Engine core: updater order, frame-loop state, FPS counter, setup variables, multiple-settings
//! blending, save-object registry and the DevHud layout/JSON. Object creation, vtable calls, SDL
//! and file I/O stay in C++.

pub mod devhud;
mod ffi;
mod ffi_devhud;
pub mod frame;
pub mod multi_settings;
pub mod save;
pub mod setup_vars;
pub mod updater;

#[cfg(test)]
mod tests;
