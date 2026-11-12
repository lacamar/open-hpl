//! FLTK launcher logic (`amnesia/src/launcher`): card database and quality rating, preset tables,
//! init/config path and language plans, game exe choice, resolution and sound device rules.
//! Widgets, config file objects and the process exec stay in C++.

mod ffi;
pub mod flow;
pub mod modes;
pub mod preset;
pub mod quality;
mod stdsort;
#[cfg(test)]
mod tests;
