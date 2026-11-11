//! Rust half of open-hpl, linked into the HPL2 static library through a C ABI.
//!
//! panic=abort: a panic kills the game, so non-test code may not contain a panic path.
#![cfg_attr(
    not(test),
    deny(
        clippy::indexing_slicing,
        clippy::unwrap_used,
        clippy::expect_used,
        clippy::panic,
        clippy::unreachable,
        clippy::todo,
        clippy::integer_division_remainder_used,
        clippy::string_slice
    )
)]

pub mod ai;
pub mod binary_buffer;
pub mod crc;
pub mod engine;
pub mod entity_loader;
mod ffi;
pub mod fsb5;
mod fsb5_ffi;
pub mod graphics;
pub mod gui;
pub mod hplmap;
pub mod hpsl;
pub mod imageload;
pub mod input;
mod input_ffi;
mod input_tables;
pub mod material;
pub mod math;
pub mod meshload;
pub mod physics;
pub mod platform;
mod platform_ffi;
pub mod preprocess;
pub mod render;
pub mod resources;
mod resources_ffi;
pub mod scene;
pub mod sdlsys;
pub mod shader;
pub mod sound;
pub mod string;
mod string_ffi;
pub mod system;
pub mod types;
pub mod world_hpm;
pub mod xml;
