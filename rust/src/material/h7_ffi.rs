//! C ABI of the H7 state, declared in `graphics/RustMaterialH7.h` (`OHPL_IMPL=rust`).

use super::anim::UvAnim;
use super::h7::{MatState, PeState, material_type, material_types, post_effect_type};
use super::types::Kind;
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
    if p.is_null() || n == 0 {
        &[]
    } else {
        // SAFETY: callers pass `n` readable bytes at `p`.
        unsafe { std::slice::from_raw_parts(p, n) }
    }
}

/// # Safety
/// `out` is null or valid for one write.
unsafe fn put_matrix(m: &MatState, out: *mut Matrixf) {
    if !out.is_null() {
        // SAFETY: caller contract.
        unsafe { out.write(m.uv) };
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h7_mat_new(translucent: bool) -> *mut MatState {
    own(MatState::new(translucent))
}

/// # Safety
/// `m` from `ohpl_h7_mat_new`, freed once.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h7_mat_free(m: *mut MatState) {
    // SAFETY: caller contract.
    unsafe { release(m) }
}

/// # Safety
/// `m` is null or live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h7_mat_set_translucent(m: *mut MatState, translucent: bool) {
    // SAFETY: caller contract.
    if let Some(m) = unsafe { m.as_mut() } {
        m.set_translucent(translucent);
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

/// # Safety
/// `m` is null or live; `out` is null or valid for one write.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h7_mat_clear_uv_anims(m: *mut MatState, out: *mut Matrixf) {
    // SAFETY: caller contract.
    if let Some(m) = unsafe { m.as_mut() } {
        m.clear_anims();
        // SAFETY: caller contract.
        unsafe { put_matrix(m, out) };
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

/// Kind of the type registered as `name`, or -1.
///
/// # Safety
/// `name` is valid for `len` reads.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h7_type_kind(name: *const u8, len: usize) -> i32 {
    material_type(bytes(name, len)).map_or(-1, |t| t.kind() as i32)
}

/// Number of `AddMaterialType` registrations.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h7_type_count() -> usize {
    material_types().count()
}

/// `mbIsTranslucent`, `mbIsDecal` and `mbHasTypeSpecifics` bits of the first type of `kind`.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h7_type_flags(kind: i32) -> u32 {
    Kind::from_i32(kind).map_or(0, |k| {
        material_types()
            .find(|t| t.kind() == k)
            .map_or(0, |t| t.flags())
    })
}

/// `AddUsedTexture` list of the first type of `kind`; returns its length.
///
/// # Safety
/// `out` is null or valid for `cap` writes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h7_type_used_textures(kind: i32, out: *mut i32, cap: usize) -> usize {
    let list = Kind::from_i32(kind)
        .and_then(|k| material_types().find(|t| t.kind() == k))
        .map(|t| t.used_textures())
        .unwrap_or_default();
    if !out.is_null() {
        for (i, s) in list.iter().take(cap).enumerate() {
            // SAFETY: caller contract, `i < cap`.
            unsafe { out.add(i).write(i32::try_from(*s).unwrap_or(0)) };
        }
    }
    list.len()
}

/// Post effect type known by `name`.
///
/// # Safety
/// `name` is valid for `len` reads.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h7_pe_type_known(name: *const u8, len: usize) -> bool {
    post_effect_type(bytes(name, len)).is_some()
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

/// # Safety
/// `p` is null or live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h7_pe_is_disabled(p: *const PeState) -> bool {
    // SAFETY: caller contract.
    unsafe { p.as_ref() }.is_some_and(PeState::is_disabled)
}

/// # Safety
/// `p` is null or live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h7_pe_is_active(p: *const PeState) -> bool {
    // SAFETY: caller contract.
    unsafe { p.as_ref() }.is_some_and(PeState::is_active)
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
