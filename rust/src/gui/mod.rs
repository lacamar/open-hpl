//! HPL2 `cGuiSet` rendering: the per-frame draw-object list, its multiset sort order, quad and
//! vertex generation, clip-region and text layout math. Widgets, materials, textures and the
//! low-level graphics calls stay in C++ and are reached through [`draw::Host`].

pub mod draw;
mod ffi;
mod ffi_set;
mod ffi_skin;
mod ffi_widget;
mod ffi_widget2;
mod ffi_widget3;
pub mod gfx;
pub mod guicore;
mod h19_ffi;
pub mod h19_widget;
pub mod h20_combo;
mod h20_ffi;
pub mod h20_host;
pub mod h20_list;
pub mod h20_menu;
pub mod h20_tab;
pub mod h20_text;
pub mod layout;
pub mod list;
pub mod popup;
pub mod set;
pub mod skin;
mod sort;
pub mod widget;
pub mod widgets_base;
pub mod widgets_list;
pub mod widgets_menu;
pub mod widgets_slider;
pub mod widgets_text;

#[cfg(test)]
mod h19_tests;
#[cfg(test)]
mod h20_tests;
#[cfg(test)]
mod tests;
#[cfg(test)]
mod tests_set;
#[cfg(test)]
mod tests_skin;
#[cfg(test)]
mod tests_widget;
#[cfg(test)]
mod tests_widget2;
#[cfg(test)]
mod tests_widget3;
