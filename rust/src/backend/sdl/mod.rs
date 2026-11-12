//! SDL system, platform, input and threads (L1): the SDL calls and object ownership behind
//! `OHPL_IMPL=rust`. Decision logic lives in `crate::sdlsys` and `crate::input`.

mod clib;
mod fs;
mod input;
mod logw;
mod plat;
mod start;
mod sync;
mod timer;

#[cfg(test)]
mod tests;

pub use timer::micros;
