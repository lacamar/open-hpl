//! C ABI of the H7 state, declared in `graphics/RustMaterialH7.h` (`OHPL_IMPL=rust`).

use super::anim::UvAnim;
use super::h7::{MATERIAL_TYPE_COUNT, MatState, PeState};
use crate::types::Matrixf;

fn own<T>(v: T) -> *mut T {
    Box::into_raw(Box::new(v))
}

/// # Safety
/// `p` is null or came from the matching `*_new` and is not used again.
unsafe fn release<T>(p: *mut T) {
    if !p.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(p) });
    }
}

fn bytes<'a>(p: *const u8, n: usize) -> &'a [u8] {
    unsafe { crate::ffi::slice(p, n) }
}

/// # Safety
/// `out` is null or valid for one write.
unsafe fn put_matrix(m: &MatState, out: *mut Matrixf) {
    if !out.is_null() {
        // SAFETY: caller contract.
        unsafe { out.write(m.uv) };
    }
}

/// Returns the blend mode now in force.
///
/// # Safety
/// `m` is null or live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h7_mat_set_blend(m: *mut MatState, mode: i32) -> i32 {
    // SAFETY: caller contract.
    unsafe { m.as_mut() }.map_or(mode, |m| {
        m.set_blend(mode);
        m.blend
    })
}

/// # Safety
/// `m` is null or live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h7_mat_set_alpha(m: *mut MatState, mode: i32) -> i32 {
    // SAFETY: caller contract.
    unsafe { m.as_mut() }.map_or(mode, |m| {
        m.set_alpha(mode);
        m.alpha
    })
}

/// Returns the depth test now in force.
///
/// # Safety
/// `m` is null or live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h7_mat_set_depth_test(m: *mut MatState, on: bool) -> bool {
    // SAFETY: caller contract.
    unsafe { m.as_mut() }.map_or(on, |m| {
        m.set_depth_test(on);
        m.depth_test
    })
}

/// # Safety
/// `m` is null or live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h7_mat_add_uv_anim(
    m: *mut MatState,
    kind: i32,
    speed: f32,
    amp: f32,
    axis: i32,
) {
    // SAFETY: caller contract.
    if let Some(m) = unsafe { m.as_mut() } {
        m.add_anim(UvAnim {
            kind,
            speed,
            amp,
            axis,
        });
    }
}

/// `UpdateBeforeRendering`; writes the UV matrix when animations run.
///
/// # Safety
/// `m` is null or live; `out` is null or valid for one write.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h7_mat_update(m: *mut MatState, dt: f32, out: *mut Matrixf) {
    // SAFETY: caller contract.
    if let Some(m) = unsafe { m.as_mut() }
        && m.has_uv
    {
        m.update(dt);
        // SAFETY: caller contract.
        unsafe { put_matrix(m, out) };
    }
}

/// Number of `AddMaterialType` registrations.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h7_type_count() -> usize {
    MATERIAL_TYPE_COUNT
}

/// # Safety
/// `name` is valid for `len` reads.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h7_pe_new(
    has_type: bool,
    name: *const u8,
    len: usize,
) -> *mut PeState {
    own(PeState::new(has_type.then(|| bytes(name, len))))
}

/// # Safety
/// `p` from `ohpl_h7_pe_new`, freed once.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h7_pe_free(p: *mut PeState) {
    // SAFETY: caller contract.
    unsafe { release(p) }
}

/// # Safety
/// `p` is null or live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h7_pe_set_disabled(p: *mut PeState, d: bool) {
    // SAFETY: caller contract.
    if let Some(p) = unsafe { p.as_mut() } {
        p.set_disabled(d);
    }
}

/// True when the caller runs `OnSetActive`.
///
/// # Safety
/// `p` is null or live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h7_pe_set_active(p: *mut PeState, a: bool) -> bool {
    // SAFETY: caller contract.
    unsafe { p.as_mut() }.is_some_and(|p| p.set_active(a))
}

/// True when params named `name` are accepted by `SetParams`/`GetParams`.
///
/// # Safety
/// `p` is null or live; `name` is valid for `len` reads.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h7_pe_accepts(
    p: *const PeState,
    name: *const u8,
    len: usize,
) -> bool {
    // SAFETY: caller contract.
    unsafe { p.as_ref() }.is_some_and(|p| p.accepts(bytes(name, len)))
}

/// # Safety
/// `p` is null or live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h7_pe_begin_render(p: *mut PeState) {
    // SAFETY: caller contract.
    if let Some(p) = unsafe { p.as_mut() } {
        p.begin_render();
    }
}

/// # Safety
/// `p` is null or live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h7_pe_mark_final_used(p: *mut PeState) {
    // SAFETY: caller contract.
    if let Some(p) = unsafe { p.as_mut() } {
        p.mark_final_used();
    }
}

/// # Safety
/// `p` is null or live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h7_pe_needs_final_copy(p: *const PeState, last: bool) -> bool {
    // SAFETY: caller contract.
    unsafe { p.as_ref() }.is_some_and(|p| p.needs_final_copy(last))
}
