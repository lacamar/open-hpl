//! C ABI for `cGuiSet`, declared in `gui/RustGui.h`.
//!
//! Pointers are non-null, valid and aligned for the call unless noted. `list` is a handle from
//! `ohpl_gui_list_new`, used from one thread.

use std::ffi::c_void;

use super::draw::{self, Frame, Gfx, GlyphTable, Host, Text};
use super::layout::{self, Tooltip, TooltipInput};
use super::list::{Batch, OutVertex, RenderList};
use crate::math::{Rect2f, Vector2f, Vector3f};
use crate::types::{Color, Vector2l};

#[repr(C)]
pub struct Batches {
    batches: *const Batch,
    batch_count: usize,
    vertices: *const OutVertex,
    vertex_count: usize,
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gui_list_new() -> *mut c_void {
    Box::into_raw(Box::<RenderList>::default()).cast()
}

/// # Safety
/// `list` is from `ohpl_gui_list_new`, freed once, or null.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gui_list_free(list: *mut c_void) {
    if !list.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(list.cast::<RenderList>()) });
    }
}

/// # Safety
/// `list` is a live list.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gui_list_clear(list: *mut c_void) {
    // SAFETY: caller contract.
    if let Some(list) = unsafe { list.cast::<RenderList>().as_mut() } {
        list.clear();
    }
}

/// `cGuiSet::DrawGfx`. `pivot` is null unless `abUseCustomPivot`; `material` is `eGuiMaterial`.
///
/// # Safety
/// `list` is live; `host` callbacks are valid; `gfx` is a live gfx element for them; `clip_rect`
/// is readable when `clip` is non-null; the other pointers are readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gui_draw_gfx(
    list: *mut c_void,
    host: *const Host,
    frame: *const Frame,
    gfx: *mut c_void,
    pos: *const Vector3f,
    size: *const Vector2f,
    color: *const Color,
    material: i32,
    angle: f32,
    pivot: *const Vector3f,
) {
    // SAFETY: caller contract.
    let (Some(list), Some(host), Some(frame)) = (unsafe {
        (
            list.cast::<RenderList>().as_mut(),
            host.as_ref(),
            frame.as_ref(),
        )
    }) else {
        return;
    };
    // SAFETY: caller contract.
    let (pos, size, color, pivot) = unsafe { (*pos, *size, *color, pivot.as_ref()) };
    let draw = Gfx {
        pos,
        size,
        color,
        material,
        angle,
        pivot: pivot.map(|p| Vector2f { x: p.x, y: p.y }),
        gfx,
        host,
    };
    draw::draw_gfx(list, frame, &draw);
}

/// `cGuiSet::DrawTextFromCharArry`; `text` is `len` wide chars.
///
/// # Safety
/// As `ohpl_gui_draw_gfx`; `table` describes the live font's glyphs; `text` has `len` chars.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gui_draw_text(
    list: *mut c_void,
    host: *const Host,
    frame: *const Frame,
    table: *const GlyphTable,
    text: *const u32,
    len: usize,
    size: *const Vector2f,
    pos: *const Vector3f,
    color: *const Color,
    material: i32,
    align: i32,
) {
    // SAFETY: caller contract.
    let (Some(list), Some(host), Some(frame), Some(table)) = (unsafe {
        (
            list.cast::<RenderList>().as_mut(),
            host.as_ref(),
            frame.as_ref(),
            table.as_ref(),
        )
    }) else {
        return;
    };
    let text: &[u32] = if text.is_null() {
        &[]
    } else {
        // SAFETY: caller contract.
        unsafe { std::slice::from_raw_parts(text, len) }
    };
    // SAFETY: caller contract.
    let (size, pos, color) = unsafe { (*size, *pos, *color) };
    let t = Text {
        text,
        size,
        pos,
        color,
        material,
        align,
        host,
    };
    draw::draw_text(list, frame, table, &t);
}

/// Sorts the list and returns the batches to emit; the arrays live until the next build, clear or free.
///
/// # Safety
/// `list` is live and `out` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gui_build(list: *mut c_void, out: *mut Batches) {
    // SAFETY: caller contract.
    let (Some(list), Some(out)) = (unsafe { (list.cast::<RenderList>().as_mut(), out.as_mut()) })
    else {
        return;
    };
    list.build();
    *out = Batches {
        batches: list.batches.as_ptr(),
        batch_count: list.batches.len(),
        vertices: list.vertices.as_ptr(),
        vertex_count: list.vertices.len(),
    };
}

/// # Safety
/// All pointers readable/writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gui_clip_child_rect(
    parent: *const Rect2f,
    pos: *const Vector3f,
    size: *const Vector2f,
    out: *mut Rect2f,
) {
    // SAFETY: caller contract.
    unsafe { *out = layout::child_clip_rect(&*parent, *pos, *size) };
}

/// # Safety
/// All pointers readable/writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gui_scissor(
    rect: *const Rect2f,
    target: *const Vector2l,
    virtual_size: *const Vector2f,
    virtual_offset: *const Vector2f,
    flip_y: bool,
    out_pos: *mut Vector2l,
    out_size: *mut Vector2l,
) {
    // SAFETY: caller contract.
    unsafe {
        let (pos, size) = layout::scissor(&*rect, *target, *virtual_size, *virtual_offset, flip_y);
        *out_pos = pos;
        *out_size = size;
    }
}

#[repr(C)]
pub struct TooltipOut {
    pos: Vector3f,
    size: Vector2f,
    background_size: Vector2f,
}

/// # Safety
/// All pointers readable/writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gui_tooltip(
    mouse: *const Vector2f,
    mouse_z: f32,
    pointer_size: *const Vector2f,
    virtual_size: *const Vector2f,
    text_length: f32,
    font_height: f32,
    rows: i32,
    out: *mut TooltipOut,
) {
    // SAFETY: caller contract.
    unsafe {
        let Tooltip {
            pos,
            size,
            background_size,
        } = layout::tooltip(&TooltipInput {
            mouse: *mouse,
            mouse_z,
            pointer_size: *pointer_size,
            virtual_size: *virtual_size,
            text_length,
            font_height,
            rows,
        });
        *out = TooltipOut {
            pos,
            size,
            background_size,
        };
    }
}

/// # Safety
/// All pointers readable/writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gui_position_inside_bounds(
    pos: *const Vector3f,
    size: *const Vector2f,
    set_size: *const Vector2f,
    out: *mut Vector3f,
) {
    // SAFETY: caller contract.
    unsafe { *out = layout::position_inside_bounds(*pos, *size, *set_size) };
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gui_next_popup_z(z: f32) -> f32 {
    layout::next_popup_z(z)
}

/// # Safety
/// `size` readable, `out` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gui_virtual_size(
    size: *const Vector2f,
    global_scale: f32,
    ignore_global_scale: bool,
    out: *mut Vector2f,
) {
    // SAFETY: caller contract.
    unsafe { *out = layout::virtual_size(*size, global_scale, ignore_global_scale) };
}
