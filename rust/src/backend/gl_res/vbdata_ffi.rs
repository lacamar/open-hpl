//! C ABI of the vertex buffer data side, declared in `HPL2/core/include/impl/RustVertexBuffer.h`.
//! Handles are `VBS` ids; a stale or zero handle does nothing and reads as empty.

use core::ffi::c_void;

use super::vb::VBS;
use super::vbdata::ArrayInfo;
use crate::types::{Matrixf, Vector3f};

fn size(n: i32) -> usize {
    usize::try_from(n).unwrap_or(0)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_vb_new(vbo: bool, reserve_vtx: i32, reserve_idx: i32) -> u64 {
    VBS.insert(super::vb::Vb::with_reserve(vbo, reserve_vtx, reserve_idx))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_vb_free(h: u64) {
    drop(VBS.remove(h));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_vb_create_array(
    h: u64,
    kind: i32,
    format: i32,
    element_num: i32,
    program_var: i32,
) -> u32 {
    VBS.with(h, |v| {
        v.create_array(kind, format, element_num, program_var)
    })
    .unwrap_or(0)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_vb_add_vec3(h: u64, kind: i32, x: f32, y: f32, z: f32) {
    VBS.with(h, |v| v.add_vec3(kind, Vector3f { x, y, z }));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_vb_add_vec4(h: u64, kind: i32, x: f32, y: f32, z: f32, w: f32) {
    VBS.with(h, |v| v.add_vec4(kind, [x, y, z, w]));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_vb_add_color(h: u64, kind: i32, r: f32, g: f32, b: f32, a: f32) {
    VBS.with(h, |v| v.add_color(kind, [r, g, b, a]));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_vb_add_index(h: u64, i: u32) {
    VBS.with(h, |v| v.add_index(i));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_vb_fill_tangents(h: u64) {
    VBS.with(h, |v| v.fill_tangents());
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_vb_shadow_double(h: u64) -> bool {
    VBS.with(h, |v| v.shadow_double()).unwrap_or(false)
}

/// # Safety
/// `m` is a 4x4 matrix.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_vb_transform(h: u64, m: *const Matrixf) -> u32 {
    // SAFETY: caller contract.
    VBS.with(h, |v| v.transform(unsafe { &*m })).unwrap_or(0)
}

/// Copies `src` into the freshly created `dst`; returns the element flags created there.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_vb_copy_into(src: u64, dst: u64, flags: u32) -> u32 {
    let Some((snap, _)) = VBS.with(src, |v| v.snapshot(flags)) else {
        return 0;
    };
    VBS.with(dst, |v| v.fill_from(snap)).unwrap_or(0)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_vb_array_ptr(h: u64, kind: i32) -> *mut c_void {
    VBS.with(h, |v| v.array_ptr(kind))
        .unwrap_or(core::ptr::null_mut())
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_vb_indices(h: u64) -> *mut u32 {
    VBS.with(h, |v| v.index_ptr())
        .unwrap_or(core::ptr::null_mut())
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_vb_index_num(h: u64) -> i32 {
    VBS.with(h, |v| v.index_num()).unwrap_or(0)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_vb_vertex_num(h: u64) -> i32 {
    VBS.with(h, |v| v.vertex_count()).unwrap_or(0)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_vb_array_count(h: u64) -> usize {
    VBS.with(h, |v| v.array_count()).unwrap_or(0)
}

/// # Safety
/// `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_vb_array_at(h: u64, i: usize, out: *mut ArrayInfo) -> bool {
    match VBS.with(h, |v| v.array_info(i)).flatten() {
        Some(info) => {
            // SAFETY: caller contract.
            unsafe { out.write(info) };
            true
        }
        None => false,
    }
}

/// Element count, format and program variable of an element (`(0, LastEnum, 0)` when absent).
///
/// # Safety
/// `out` holds three ints.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_vb_element_desc(h: u64, kind: i32, out: *mut i32) {
    let (n, f, p) = VBS.with(h, |v| v.element_desc(kind)).unwrap_or((0, 3, 0));
    // SAFETY: caller contract.
    unsafe {
        out.write(n);
        out.add(1).write(f);
        out.add(2).write(p);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_vb_resize_array(h: u64, kind: i32, n: i32) {
    VBS.with(h, |v| v.resize_array(kind, size(n)));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_vb_resize_indices(h: u64, n: i32) {
    VBS.with(h, |v| v.resize_indices(size(n)));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_vb_bounding_check(h: u64) -> i32 {
    VBS.with(h, |v| v.bounding_check()).unwrap_or(1)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_vb_compile(h: u64, usage: i32) {
    VBS.with(h, |v| v.compile_gl(usage));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_vb_update(h: u64, usage: i32, types: u32, with_indices: bool) {
    VBS.with(h, |v| v.update_gl(usage, types, with_indices));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_vb_bind(h: u64, usage: i32) {
    VBS.with(h, |v| v.bind_gl(usage));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_vb_unbind(h: u64) {
    VBS.with(h, |v| v.unbind());
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_vb_draw(h: u64, own: i32, requested: i32, element_num: i32) {
    VBS.with(h, |v| v.draw_gl(own, requested, element_num));
}
