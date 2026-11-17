//! C ABI for `cGui`, `cGuiSkin`, `cGuiGfxElement` and the GUI materials, declared in
//! `gui/RustGuiSkin.h`.

use std::ffi::c_void;

use super::guicore::{self, SkinKind};
use crate::string_ffi::units;

fn kind(k: i32) -> Option<SkinKind> {
    match k {
        0 => Some(SkinKind::Font),
        1 => Some(SkinKind::Attribute),
        2 => Some(SkinKind::Gfx),
        _ => None,
    }
}

/// `cGui::GetSkin{Font,Attribute,Gfx}FromString`: the table length (`LastEnum`) when unknown.
///
/// # Safety
/// `name` is readable for `len` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gui_skin_from_name(k: i32, name: *const u8, len: usize) -> u32 {
    let Some(kind) = kind(k) else {
        return 0;
    };
    // SAFETY: caller contract.
    kind.from_name(unsafe { units(name, len) })
}

/// `cGui::GetSkin{Font,Attribute,Gfx}String`, delivered through `sink`.
///
/// # Safety
/// `sink` accepts `ctx` and a slice.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gui_skin_name(
    k: i32,
    index: u32,
    sink: unsafe extern "C" fn(*mut c_void, *const u8, usize),
    ctx: *mut c_void,
) {
    let name = kind(k).map_or("Unknown", |kind| kind.name(index));
    // SAFETY: caller contract.
    unsafe { sink(ctx, name.as_ptr(), name.len()) }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gui_material_slot(ty: u32) -> u32 {
    guicore::material_slot(ty)
}

/// The blend factors of a GUI material; false for an unknown material.
///
/// # Safety
/// `src` and `dst` are writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gui_material_blend(ty: u32, src: *mut i32, dst: *mut i32) -> bool {
    let Some((s, d)) = guicore::material_blend(ty) else {
        return false;
    };
    // SAFETY: caller contract.
    unsafe {
        *src = s;
        *dst = d;
    }
    true
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gui_mouse_to_mouse(button: i32) -> i32 {
    guicore::gui_mouse_to_mouse(button)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gui_mouse_to_gui(button: i32) -> i32 {
    guicore::mouse_to_gui_mouse(button)
}
