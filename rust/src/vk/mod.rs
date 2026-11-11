//! Vulkan backend decisions (`LowLevelGraphicsVulkan`, `VulkanResources`, `GlslToVulkan`). No
//! Vulkan symbol is linked here: tables, plans and the GLSL rewriter come back as values and the
//! C++ side issues every `vk*` call.

pub mod ffi;
pub mod glsl_rewrite;
pub mod tables;
#[cfg(test)]
mod tests;
