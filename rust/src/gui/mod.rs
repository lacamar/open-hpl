//! HPL2 `cGuiSet` rendering: the per-frame draw-object list, its multiset sort order, quad and
//! vertex generation, clip-region and text layout math. Widgets, materials, textures and the
//! low-level graphics calls stay in C++ and are reached through [`draw::Host`].

pub mod draw;
mod ffi;
pub mod layout;
pub mod list;
mod sort;

#[cfg(test)]
mod tests;
