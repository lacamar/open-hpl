//! C ABI for `animstate.rs`, `meshentity.rs` and `skinning.rs`, declared in
//! `HPL2/core/include/scene/RustMeshEntity.h`.
//!
//! Pointers are non-null, valid and aligned for the call; a null array or a count `<= 0` is an
//! empty slice. Output arrays do not alias inputs.

use super::animstate::AnimCore;
use super::skinning::{self, SkinJob};
use crate::types::Matrixf;

const _: () = {
    use std::mem::{offset_of, size_of};
    assert!(size_of::<AnimCore>() == 44 && offset_of!(AnimCore, active) == 40);
};

/// # Safety
/// `p` valid for `n` reads when `n > 0`.
unsafe fn slice<'a, T>(p: *const T, n: i32) -> &'a [T] {
    match usize::try_from(n) {
        // SAFETY: caller contract.
        Ok(n) if n > 0 && !p.is_null() => unsafe { std::slice::from_raw_parts(p, n) },
        _ => &[],
    }
}

/// # Safety
/// `p` valid for `n` writes when `n > 0`.
unsafe fn slice_mut<'a, T>(p: *mut T, n: i32) -> &'a mut [T] {
    match usize::try_from(n) {
        // SAFETY: caller contract.
        Ok(n) if n > 0 && !p.is_null() => unsafe { std::slice::from_raw_parts_mut(p, n) },
        _ => &mut [],
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_anim_core_init(core: *mut AnimCore, length: f32) {
    if !core.is_null() {
        // SAFETY: non-null, valid for a write of `AnimCore` (caller contract).
        unsafe { core.write(AnimCore::new(length)) }
    }
}

/// # Safety
/// `core` valid and exclusive.
unsafe fn with_core(core: *mut AnimCore, f: impl FnOnce(&mut AnimCore)) {
    // SAFETY: caller contract.
    if let Some(c) = unsafe { core.as_mut() } {
        f(c);
    }
}

/// # Safety
/// `core` valid and exclusive.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_anim_update(core: *mut AnimCore, dt: f32) {
    // SAFETY: caller contract.
    unsafe { with_core(core, |c| c.update(dt)) }
}

/// # Safety
/// `core` valid and exclusive.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_anim_add_time(core: *mut AnimCore, add: f32) {
    // SAFETY: caller contract.
    unsafe { with_core(core, |c| c.add_time_position(add)) }
}

/// # Safety
/// `core` valid and exclusive.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_anim_set_time(core: *mut AnimCore, pos: f32) {
    // SAFETY: caller contract.
    unsafe { with_core(core, |c| c.set_time_position(pos)) }
}

/// # Safety
/// `core` valid and exclusive.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_anim_set_relative_time(core: *mut AnimCore, pos: f32) {
    // SAFETY: caller contract.
    unsafe { with_core(core, |c| c.set_relative_time_position(pos)) }
}

/// # Safety
/// `core` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_anim_relative_time(core: *const AnimCore) -> f32 {
    // SAFETY: caller contract.
    unsafe { core.as_ref() }.map_or(0.0, AnimCore::relative_time_position)
}

/// # Safety
/// `core` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_anim_is_over(core: *const AnimCore) -> bool {
    // SAFETY: caller contract.
    unsafe { core.as_ref() }.is_some_and(AnimCore::is_over)
}

/// # Safety
/// `core` valid and exclusive.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_anim_fade_in(core: *mut AnimCore, time: f32) {
    // SAFETY: caller contract.
    unsafe { with_core(core, |c| c.fade_in(time)) }
}

/// # Safety
/// `core` valid and exclusive.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_anim_fade_out(core: *mut AnimCore, time: f32) {
    // SAFETY: caller contract.
    unsafe { with_core(core, |c| c.fade_out(time)) }
}

/// # Safety
/// `core` valid and exclusive.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_anim_fade_speed(core: *mut AnimCore, dest: f32, time: f32) {
    // SAFETY: caller contract.
    unsafe { with_core(core, |c| c.fade_speed(dest, time)) }
}

/// # Safety
/// `core` valid and exclusive.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_anim_set_active(core: *mut AnimCore, active: bool) {
    // SAFETY: caller contract.
    unsafe { with_core(core, |c| c.set_active(active)) }
}

/// Skins one sub mesh. `tangent` pointers may be null.
///
/// # Safety
/// `bind_*` / `skin_*` hold `vertex_count` vertices at the strides `pos_stride`, 3 and 4 floats;
/// `weights` and `bones` hold 4 entries per vertex; `matrices` holds `matrix_count` matrices.
/// Skin arrays do not alias bind arrays.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_submesh_skin(
    bind_pos: *const f32,
    bind_normal: *const f32,
    bind_tangent: *const f32,
    skin_pos: *mut f32,
    skin_normal: *mut f32,
    skin_tangent: *mut f32,
    weights: *const f32,
    bones: *const u8,
    matrices: *const Matrixf,
    vertex_count: i32,
    pos_stride: i32,
    matrix_count: i32,
) {
    let v = i64::from(vertex_count);
    let len = |per: i64| i32::try_from(v.saturating_mul(per)).unwrap_or(0);
    let stride = i64::from(pos_stride.max(1));
    // SAFETY: caller contract.
    let job = unsafe {
        SkinJob {
            bind_pos: slice(bind_pos, len(stride)),
            bind_normal: slice(bind_normal, len(3)),
            bind_tangent: slice(bind_tangent, len(4)),
            skin_pos: slice_mut(skin_pos, len(stride)),
            skin_normal: slice_mut(skin_normal, len(3)),
            skin_tangent: slice_mut(skin_tangent, len(4)),
            weights: slice(weights, len(4)),
            bones: slice(bones, len(4)),
            matrices: slice(matrices, matrix_count),
            pos_stride: usize::try_from(stride).unwrap_or(1),
        }
    };
    skinning::skin(job);
}
