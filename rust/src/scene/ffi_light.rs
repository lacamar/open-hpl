//! C ABI for `light.rs`, `lensflare.rs`, `beam.rs` and `rope.rs`, declared in
//! `HPL2/core/include/scene/RustLight.h`.
//!
//! Pointers are non-null, valid and aligned for the call; outputs do not alias inputs. Float
//! buffers have the capacity stated by the accompanying count.

use super::beam;
use super::lensflare::{self, EntIn, FlareIn, Kind, ViewIn, VisState};
use super::light::{FlickerCfg, LightAnim, LightLive};
use super::rope::RopeParams;
use crate::types::{Color, Matrixf, Vector3f};

const _: () = {
    use std::mem::{offset_of, size_of};
    assert!(size_of::<LightAnim>() == 144 && offset_of!(LightAnim, flickering) == 140);
    assert!(size_of::<LightLive>() == 24 && size_of::<FlickerCfg>() == 56);
    assert!(size_of::<VisState>() == 16 && size_of::<RopeParams>() == 28);
    assert!(size_of::<FlareIn>() == 152 && offset_of!(FlareIn, brightness) == 144);
    assert!(offset_of!(FlareIn, color) == 4 && size_of::<EntIn>() == 24);
    assert!(size_of::<ViewIn>() == 144);
};

/// # Safety
/// `p` valid for `n` writes when `n > 0`.
unsafe fn slice_mut<'a, T>(p: *mut T, n: i32) -> &'a mut [T] {
    match usize::try_from(n) {
        // SAFETY: caller contract.
        Ok(n) if n > 0 && !p.is_null() => unsafe { std::slice::from_raw_parts_mut(p, n) },
        _ => &mut [],
    }
}

/// # Safety
/// All pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_flare_color(
    f: *const FlareIn,
    e: *const EntIn,
    v: *const ViewIn,
    kind: i32,
    vis: f32,
    out: *mut Color,
) {
    let Some(kind) = Kind::from_i32(kind) else {
        return;
    };
    // SAFETY: caller contract.
    unsafe { *out = lensflare::color_and_brightness(&*f, &*e, &*v, kind, vis) }
}

/// # Safety
/// All pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_flare_glare(
    f: *const FlareIn,
    e: *const EntIn,
    v: *const ViewIn,
    scale: f32,
    out: *mut Color,
    screen: *mut Vector3f,
) {
    // SAFETY: caller contract.
    unsafe {
        let (c, s) = lensflare::glare(&*f, &*e, &*v, scale);
        *out = c;
        *screen = s;
    }
}

/// # Safety
/// All pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_flare_size_rotation(
    f: *const FlareIn,
    e: *const EntIn,
    v: *const ViewIn,
    kind: i32,
    vis: f32,
    out: *mut Matrixf,
) {
    let Some(kind) = Kind::from_i32(kind) else {
        return;
    };
    // SAFETY: caller contract.
    unsafe { *out = lensflare::size_and_rotation(&*f, &*e, &*v, kind, vis) }
}

/// # Safety
/// All pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_flare_billboard(
    f: *const FlareIn,
    e: *const EntIn,
    v: *const ViewIn,
    out: *mut Matrixf,
) {
    // SAFETY: caller contract.
    unsafe { *out = lensflare::billboard_matrix(&*f, &*e, &*v) }
}

/// # Safety
/// All pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_flare_type_color(
    f: *const FlareIn,
    e: *const EntIn,
    v: *const ViewIn,
    kind: i32,
    vis: f32,
    out: *mut Color,
) -> bool {
    let Some(kind) = Kind::from_i32(kind) else {
        return false;
    };
    // SAFETY: caller contract.
    match unsafe { lensflare::type_color(&*f, &*e, &*v, kind, vis) } {
        Some(c) => {
            // SAFETY: caller contract.
            unsafe { *out = c };
            true
        }
        None => false,
    }
}

/// Returns false when the flare is out of range; otherwise writes the distance-scaled box.
///
/// # Safety
/// All pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_flare_collide_box(
    f: *const FlareIn,
    v: *const ViewIn,
    world_center: *const Vector3f,
    local_min: *const Vector3f,
    local_max: *const Vector3f,
    local_center: *const Vector3f,
    out_min: *mut Vector3f,
    out_max: *mut Vector3f,
) -> bool {
    // SAFETY: caller contract.
    unsafe {
        match lensflare::collide_box(
            &*f,
            &*v,
            *world_center,
            *local_min,
            *local_max,
            *local_center,
        ) {
            Some((a, b)) => {
                *out_min = a;
                *out_max = b;
                true
            }
            None => false,
        }
    }
}

/// Fills up to `quads` quads; returns how many were written.
///
/// # Safety
/// All pointers valid; `pos`, `col` hold `quads * 16` floats, `uv` `quads * 12`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_flare_multi_iris(
    f: *const FlareIn,
    e: *const EntIn,
    v: *const ViewIn,
    vis: f32,
    pos: *mut f32,
    col: *mut f32,
    uv: *mut f32,
    quads: i32,
) -> i32 {
    let q = quads.max(0);
    // SAFETY: caller contract.
    let n = unsafe {
        lensflare::multi_iris(
            &*f,
            &*e,
            &*v,
            vis,
            slice_mut(pos, q.saturating_mul(16)),
            slice_mut(col, q.saturating_mul(16)),
            slice_mut(uv, q.saturating_mul(12)),
        )
    };
    i32::try_from(n).unwrap_or(i32::MAX)
}

/// Writes xyz of 4 positions (stride 4) and xy of 4 uvs (stride 3), as `UpdateGraphicsForFrame`.
///
/// # Safety
/// All pointers valid; `pos` holds 16 floats, `uv` 12.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_beam_geometry(
    start: *const Vector3f,
    end: *const Vector3f,
    size_x: f32,
    size_y: f32,
    tile_height: bool,
    pos: *mut f32,
    uv: *mut f32,
    axis: *mut Vector3f,
    mid: *mut Vector3f,
) {
    // SAFETY: caller contract.
    unsafe {
        let g = beam::geometry(*start, *end, [size_x, size_y], tile_height);
        for (dst, src) in slice_mut(pos, 16)
            .as_chunks_mut::<4>()
            .0
            .iter_mut()
            .zip(g.pos)
        {
            for (d, s) in dst.iter_mut().zip(src) {
                *d = s;
            }
        }
        for (dst, src) in slice_mut(uv, 12)
            .as_chunks_mut::<3>()
            .0
            .iter_mut()
            .zip(g.uv)
        {
            for (d, s) in dst.iter_mut().zip(src) {
                *d = s;
            }
        }
        *axis = g.axis;
        *mid = g.mid;
    }
}
