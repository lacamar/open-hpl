//! C ABI for `cGuiSet`, declared in `gui/RustGui.h`.
//!
//! Pointers are non-null, valid and aligned for the call unless noted. `list` is a handle from
//! `ohpl_gui_list_new`, used from one thread.

use std::ffi::c_void;

use super::draw::{self, Frame, Gfx, Host};
use super::layout::{self};
use super::list::{Batch, OutVertex, RenderList};
use crate::math::{Vector2f, Vector3f};
use crate::types::Color;

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
pub unsafe extern "C" fn ohpl_gui_position_inside_bounds(
    pos: *const Vector3f,
    size: *const Vector2f,
    set_size: *const Vector2f,
    out: *mut Vector3f,
) {
    // SAFETY: caller contract.
    unsafe { *out = layout::position_inside_bounds(*pos, *size, *set_size) };
}
