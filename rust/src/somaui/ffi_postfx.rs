//! C ABI of the post-effect plans, mirrored in `soma/src/game/RustSomaUi.h`.

use super::postfx::{self, BlurPlan, Chromatic, LevelSize, PARAMS, TentacleFades};
use crate::math::v3;

const _: () = {
    use std::mem::size_of;
    assert!(size_of::<Chromatic>() == 72);
    assert!(size_of::<LevelSize>() == 12);
    assert!(size_of::<postfx::BlurOp>() == 36);
    assert!(size_of::<postfx::BlurWeights>() == 140);
    assert!(size_of::<BlurPlan>() == 4040);
};

/// # Safety
/// `params` has 9 floats; `clear` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_pfx_set_activity(
    ty: i32,
    params: *const f32,
    was_active: bool,
    clear: *mut bool,
) -> bool {
    // SAFETY: caller contract.
    unsafe {
        let p = params.cast::<[f32; PARAMS]>().read();
        let (active, c) =
            postfx::set_activity(usize::try_from(ty).unwrap_or(usize::MAX), &p, was_active);
        *clear = c;
        active
    }
}

/// # Safety
/// `params` has 9 floats; `tex` has 2; `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_pfx_chromatic(
    params: *const f32,
    screen_h: f32,
    tex: *const f32,
    out: *mut Chromatic,
) {
    // SAFETY: caller contract.
    unsafe {
        *out = postfx::chromatic(
            &params.cast::<[f32; PARAMS]>().read(),
            screen_h,
            tex.cast::<[f32; 2]>().read(),
        );
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs7_pfx_trail_alpha(amount: f32, frame_time: f32, clear: bool) -> f32 {
    postfx::trail_alpha(amount, frame_time, clear)
}

/// Fills `out` (room for `max`) with the level sizes and returns their count.
///
/// # Safety
/// `out` has room for `max` entries.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_pfx_blur_levels(
    w: i32,
    h: i32,
    out: *mut LevelSize,
    max: usize,
) -> usize {
    let levels = postfx::blur_levels([w, h]);
    let n = levels.len().min(max);
    // SAFETY: caller contract; `n <= max`.
    unsafe { std::ptr::copy_nonoverlapping(levels.as_ptr(), out, n) };
    n
}

/// # Safety
/// `levels` has `n` entries; `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_pfx_blur_plan(
    amount: f32,
    screen_h: i32,
    levels: *const LevelSize,
    n: usize,
    out: *mut BlurPlan,
) {
    // SAFETY: caller contract.
    unsafe {
        let l: &[LevelSize] = if n == 0 {
            &[]
        } else {
            std::slice::from_raw_parts(levels, n)
        };
        *out = postfx::blur_plan(amount, screen_h, l);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs7_pfx_tentacles_new() -> *mut TentacleFades {
    Box::into_raw(Box::default())
}

/// # Safety
/// `f` came from `ohpl_gs7_pfx_tentacles_new` and is not used again.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_pfx_tentacles_free(f: *mut TentacleFades) {
    if !f.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(f) });
    }
}

/// # Safety
/// `f` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_pfx_tentacles_clear(f: *mut TentacleFades) {
    // SAFETY: caller contract.
    unsafe { (*f).clear() }
}

/// # Safety
/// `f` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_pfx_tentacles_set_material(
    f: *mut TentacleFades,
    tentacles: usize,
    materials: usize,
    idx: i32,
    time: f32,
) {
    // SAFETY: caller contract.
    unsafe { (*f).set_material(tentacles, materials, idx, time) }
}

/// Current material.
///
/// # Safety
/// `f` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_pfx_tentacles_material(f: *const TentacleFades) -> i32 {
    // SAFETY: caller contract.
    unsafe { (*f).mat }
}

/// # Safety
/// `f` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_pfx_tentacles_advance(f: *mut TentacleFades, dt: f32) {
    // SAFETY: caller contract.
    unsafe { (*f).advance(dt) }
}

/// Second material (-1 for none) and its alpha.
///
/// # Safety
/// `f` is live; `alpha` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_pfx_tentacles_blend(
    f: *const TentacleFades,
    alpha: *mut f32,
) -> i32 {
    // SAFETY: caller contract.
    unsafe {
        let (m, a) = (*f).blend();
        *alpha = a;
        m.unwrap_or(-1)
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs7_pfx_tentacle_time(time: f32, dt: f32, speed: f32) -> f32 {
    postfx::tentacle_time(time, dt, speed)
}

/// Fills `out` with the playback speed of each of `groups` groups.
///
/// # Safety
/// `out` has `groups` floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_pfx_tentacle_speeds(
    groups: usize,
    min: f32,
    max: f32,
    out: *mut f32,
) {
    let v = postfx::tentacle_speeds(groups, min, max);
    // SAFETY: caller contract.
    unsafe { std::ptr::copy_nonoverlapping(v.as_ptr(), out, v.len()) };
}

/// # Safety
/// `out` has 2 ints.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_pfx_tentacle_target(w: i32, h: i32, out: *mut i32) {
    // SAFETY: caller contract.
    unsafe { out.cast::<[i32; 2]>().write(postfx::tentacle_target(w, h)) }
}

/// # Safety
/// `rot` has 3 floats; `out` has 16.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_pfx_tentacle_model(
    w: i32,
    h: i32,
    rot: *const f32,
    out: *mut f32,
) {
    // SAFETY: caller contract.
    unsafe {
        let r = rot.cast::<[f32; 3]>().read();
        let m = postfx::tentacle_model([w, h], v3(r[0], r[1], r[2]));
        out.cast::<crate::types::Matrixf>().write(m);
    }
}

/// # Safety
/// Vectors have 3 floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_pfx_tentacle_pos(pos: *const f32, spread: f32, out: *mut f32) {
    // SAFETY: caller contract.
    unsafe {
        let p = pos.cast::<[f32; 3]>().read();
        let r = postfx::tentacle_pos(v3(p[0], p[1], p[2]), spread);
        out.cast::<[f32; 3]>().write([r.x, r.y, r.z]);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs7_pfx_tentacle_aspect(w: i32, h: i32) -> f32 {
    postfx::tentacle_aspect([w, h])
}
