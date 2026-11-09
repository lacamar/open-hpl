//! Rust half of open-hpl, linked into the HPL2 static library through a C ABI.

pub mod crc;
mod ffi;
pub mod hpsl;
pub mod platform;
mod platform_ffi;
pub mod string;
mod string_ffi;
pub mod types;
