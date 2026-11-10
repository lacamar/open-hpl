//! Render plumbing: render list sorting, `iRenderFunctions` state dedup, program combo cache.

pub mod combo;
pub mod ffi;
pub mod funcs;
pub mod list;

#[cfg(test)]
mod tests;
