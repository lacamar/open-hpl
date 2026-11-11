//! HPL2 `cGuiSet` rendering: the per-frame draw-object list, its multiset sort order, quad and
//! vertex generation, clip-region and text layout math. Widgets, materials, textures and the
//! low-level graphics calls stay in C++ and are reached through [`draw::Host`].

pub mod draw;
mod ffi;
mod ffi_skin;
mod ffi_widget;
mod ffi_widget2;
pub mod gfx;
pub mod guicore;
pub mod layout;
pub mod list;
pub mod skin;
mod sort;
pub mod widget;
pub mod widgets_base;
pub mod widgets_list;
pub mod widgets_slider;
pub mod widgets_text;

#[cfg(test)]
mod tests;
#[cfg(test)]
mod tests_skin;
#[cfg(test)]
mod tests_widget;
#[cfg(test)]
mod tests_widget2;
