//! C ABI of the H15 world registry, mirrored in `HPL2/core/include/physics/RustPhysBook.h`.

use super::host::Str;
use super::wreg::{self, Query, WorldOp};
use crate::types::{Matrixf, Vector3f};

/// # Safety
/// `p` points to `n` readable bytes, or `n` is 0.
unsafe fn bytes<'a>(p: *const u8, n: usize) -> &'a [u8] {
    // SAFETY: caller contract.
    unsafe { Str { p, n }.bytes() }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15w_new(op: WorldOp, me: usize) -> u64 {
    wreg::new(op, me)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15w_add(book: u64, kind: usize, h: usize) {
    wreg::add(book, kind, h);
}

/// # Safety
/// `name` points to `n` readable bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h15w_add_material(book: u64, name: *const u8, n: usize, h: usize) {
    // SAFETY: caller contract.
    wreg::add_material(book, unsafe { bytes(name, n) }, h);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15w_len(book: u64, kind: usize) -> usize {
    wreg::len(book, kind)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15w_at(book: u64, kind: usize, i: usize) -> usize {
    wreg::at(book, kind, i)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15w_material_len(book: u64) -> usize {
    wreg::material_len(book)
}

/// # Safety
/// `name` and `obj` are writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h15w_material_at(
    book: u64,
    i: usize,
    name: *mut Str,
    obj: *mut usize,
) -> bool {
    let Some((p, n, h)) = wreg::material_at(book, i) else {
        return false;
    };
    // SAFETY: caller contract.
    unsafe {
        if let Some(o) = name.as_mut() {
            *o = Str { p, n };
        }
        if let Some(o) = obj.as_mut() {
            *o = h;
        }
    }
    true
}

/// # Safety
/// `name` points to `n` readable bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h15w_find_by_name(
    book: u64,
    kind: usize,
    name: *const u8,
    n: usize,
) -> usize {
    // SAFETY: caller contract.
    wreg::find_by_name(book, kind, unsafe { bytes(name, n) })
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15w_find_and_delete(book: u64, kind: usize, h: usize) {
    wreg::find_and_delete(book, kind, h);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15w_contains(book: u64, kind: usize, h: usize) -> bool {
    wreg::contains(book, kind, h)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15w_destroy_shape(book: u64, shape: usize) {
    wreg::destroy_shape(book, shape);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15w_destroy_body(book: u64, body: usize) {
    wreg::destroy_body(book, body);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15w_destroy_all(book: u64) {
    wreg::destroy_all(book);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15w_update_add(book: u64, body: usize) {
    wreg::update_add(book, body);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15w_update_remove(book: u64, body: usize) {
    wreg::update_remove(book, body);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15w_update(book: u64, dt: f32) {
    wreg::update(book, dt);
}

/// # Safety
/// `name` points to `n` readable bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h15w_get_material(book: u64, name: *const u8, n: usize) -> usize {
    // SAFETY: caller contract.
    wreg::get_material(book, unsafe { bytes(name, n) })
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15w_rope_from_unique_id(book: u64, id: i32) -> usize {
    wreg::rope_from_unique_id(book, id)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15w_enable_bodies_in_bv(book: u64, bv: usize) {
    wreg::enable_bodies_in_bv(book, bv);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15w_fadeout_all_loop_sounds(book: u64, speed: f32) {
    wreg::fadeout_all_loop_sounds(book, speed);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h15w_render_contacts(book: u64, low: usize, colors: usize) {
    wreg::render_contacts(book, low, colors);
}

/// # Safety
/// `push` is null or writable, `mtx` and `q` are valid for the call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h15w_shape_world_collision(
    book: u64,
    push: *mut Vector3f,
    shape: usize,
    mtx: *const Matrixf,
    q: *const Query,
    callback: usize,
) -> bool {
    // SAFETY: caller contract.
    let Some(q) = (unsafe { q.as_ref() }) else {
        return false;
    };
    // SAFETY: caller contract.
    unsafe { wreg::shape_world_collision(book, push, shape, mtx, q, callback) }
}
