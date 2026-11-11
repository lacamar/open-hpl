//! Render plumbing: render list sorting, `iRenderFunctions` state dedup, program combo cache.

pub mod combo;
pub mod deferred;
pub mod deferred_ffi;
pub mod deferred_light;
pub mod deferred_light_ffi;
#[cfg(test)]
mod deferred_light_tests;
pub mod deferred_lightlist;
pub mod deferred_pass;
pub mod deferred_pass_ffi;
#[cfg(test)]
mod deferred_pass_tests;
pub mod deferred_post;
#[cfg(test)]
mod deferred_tests;
pub mod ffi;
pub mod funcs;
pub mod list;
pub mod posteffect;
pub mod posteffect_ffi;
#[cfg(test)]
mod posteffect_tests;
pub mod renderer;
mod renderer_ffi;
pub mod renderer_simple;
#[cfg(test)]
mod renderer_tests;

#[cfg(test)]
mod tests;
