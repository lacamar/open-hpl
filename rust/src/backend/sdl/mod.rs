//! SDL system, platform, input and threads (L1): the SDL calls and object ownership.
//! Decision logic lives in `crate::sdlsys` and `crate::input`.

mod clib;
pub mod fs;
mod input;
mod logw;
mod plat;
mod start;
mod sync;
mod timer;

#[cfg(test)]
mod tests;

pub use plat::{ohpl_l1_delay as delay_ms, ohpl_l1_ticks};
pub use start::game_main;
pub use timer::micros;
