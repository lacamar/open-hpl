//! SOMA/Rebirth/Bunker script layer (GS6): the logic around the AngelScript engine calls.
//! Narrow strings are bytes, wide strings `u32` units (Linux `wchar_t`).

mod api;
mod builder;
mod cfmt;
mod check;
mod ffi_api;
mod ffi_builder;
mod ffi_globals;
mod ffi_math;
mod ffi_runtime;
mod ffi_strings;
mod globals;
mod math;
mod patches;
mod runtime;
mod stream;
mod strings;
#[cfg(test)]
mod tests;
