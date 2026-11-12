//! System layer: save-game reflection walk, headless-control protocol, diagnostics JSON and the
//! logic timer. Threads, mutexes, the memory tracker and the socket I/O stay in C++.

mod diag;
mod ffi;
pub mod h1;
pub mod headless;
pub mod logic_timer;
pub mod serialize;

pub use diag::{frame_stats_json, gamma_lut, json_escape, num, pixel_stats_json};

#[cfg(test)]
mod tests;
