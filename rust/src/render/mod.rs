//! Render plumbing: render list sorting, `iRenderFunctions` state dedup, program combo cache.

pub mod combo;
pub mod ffi;
pub mod funcs;
pub mod list;
pub mod posteffect;
pub mod posteffect_ffi;
#[cfg(test)]
mod posteffect_tests;

#[cfg(test)]
mod tests;
