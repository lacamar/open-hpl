//! C ABI of the tone mapping layer, mirrored in `soma/src/game/RustSomaUi.h`.

use super::tonemap::{self, ExposureArea, ToneState, v};
use crate::somasound::powf;

const _: () = {
    use std::mem::size_of;
    assert!(size_of::<ToneState>() == 48);
    assert!(size_of::<ExposureArea>() == 88);
};

/// # Safety
/// `s` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_tone_state_default(s: *mut ToneState) {
    // SAFETY: caller contract.
    unsafe {
        *s = ToneState {
            world_exposure: 0.0,
            world_white_cut: 3.5,
            world_fade_time: 0.5,
            exposure: 0.0,
            white_cut: 3.5,
            transition_time: 0.5,
            ..ToneState::default()
        };
        (*s).exposure_fade = tonemap::Fade::default();
        (*s).white_cut_fade = tonemap::Fade::default();
    }
}

/// # Safety
/// `areas` has `n` entries; `pos` has 3 floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_tone_area_at(
    areas: *const ExposureArea,
    n: usize,
    pos: *const f32,
) -> i32 {
    // SAFETY: caller contract.
    unsafe {
        let a: &[ExposureArea] = if n == 0 {
            &[]
        } else {
            std::slice::from_raw_parts(areas, n)
        };
        tonemap::area_at(a, v(pos.cast::<[f32; 3]>().read())).map_or(-1, |i| i as i32)
    }
}

/// # Safety
/// `s` is writable; `area` is null or valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_tone_map_loaded(
    s: *mut ToneState,
    exposure: f32,
    white_cut: f32,
    area: *const ExposureArea,
) {
    // SAFETY: caller contract.
    unsafe {
        tonemap::map_loaded(&mut *s, exposure, white_cut, area.as_ref());
    }
}

/// # Safety
/// `s` is writable; `area` is null or valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_tone_update_exposure(
    s: *mut ToneState,
    world_current: bool,
    area: *const ExposureArea,
    dt: f32,
) {
    // SAFETY: caller contract.
    unsafe { tonemap::update_exposure(&mut *s, world_current, area.as_ref(), dt) }
}

/// True when the target texture becomes current.
///
/// # Safety
/// `s` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_tone_update_grading(
    s: *mut ToneState,
    has_target: bool,
    has_texture: bool,
    dt: f32,
) -> bool {
    // SAFETY: caller contract.
    unsafe { tonemap::update_grading(&mut *s, has_target, has_texture, dt).promote_target }
}

/// # Safety
/// `prios` has `n` entries.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_tone_layer_insert_index(
    prios: *const i32,
    n: usize,
    prio: i32,
) -> usize {
    // SAFETY: caller contract.
    let p: &[i32] = if n == 0 {
        &[]
    } else {
        unsafe { std::slice::from_raw_parts(prios, n) }
    };
    tonemap::layer_insert_index(p, prio)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs7_tone_exposure_scale(exposure: f32) -> f32 {
    powf(2.0, exposure)
}
