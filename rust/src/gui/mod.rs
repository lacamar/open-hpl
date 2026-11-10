//! HPL2 `cGuiSet` rendering: the per-frame draw-object list, its multiset sort order, quad and
//! vertex generation, clip-region and text layout math. Widgets, materials, textures and the
//! low-level graphics calls stay in C++ and are reached through [`draw::Host`].

pub mod draw;
mod ffi;
mod ffi_widget;
pub mod layout;
pub mod list;
mod sort;
pub mod widget;
pub mod widgets_base;

#[cfg(test)]
mod tests;
#[cfg(test)]
mod tests_widget;
