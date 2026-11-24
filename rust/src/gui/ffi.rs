use std::ffi::c_void;

use super::layout::{self};
use super::list::{Batch, OutVertex, RenderList};
use crate::math::{Vector2f, Vector3f};

#[repr(C)]
pub struct Batches {
    batches: *const Batch,
    batch_count: usize,
    vertices: *const OutVertex,
    vertex_count: usize,
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
