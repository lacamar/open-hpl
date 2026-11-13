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
mod s1_gen;
#[cfg(test)]
mod s1_tests;
mod s2_api;
mod s2_as;
mod s2_builder;
mod s2_check;
mod s2_entry;
mod s2_globals;
mod s2_host;
mod s2_math;
mod s2_strings;
#[cfg(test)]
mod s2_tests;
mod stream;
mod strings;
#[cfg(test)]
mod tests;
