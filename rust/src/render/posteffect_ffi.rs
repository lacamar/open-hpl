//! C ABI for the post effect port, declared in `graphics/RustPostEffect.h`.
//!
//! Plan handles are boxed; the shim frees each exactly once. Pointers are valid for the call, and
//! a null pointer makes the call a no-op returning the zero value.

use std::slice;

use super::posteffect::{self as pe, Op, Plan};
use crate::types::{Vector2f, Vector2l};

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct OpC {
    pub kind: u32,
    pub a: i32,
    pub b: i32,
    pub c: i32,
    pub f: [f32; 3],
}

const fn op(kind: u32, a: i32, b: i32, c: i32, f: [f32; 3]) -> OpC {
    OpC { kind, a, b, c, f }
}

impl From<Op> for OpC {
    fn from(o: Op) -> Self {
        let z = [0.0; 3];
        match o {
            Op::FlatProjection => op(0, 0, 0, 0, z),
            Op::BlendNone => op(1, 0, 0, 0, z),
            Op::BlendAlpha => op(2, 0, 0, 0, z),
            Op::ChannelRgba => op(3, 0, 0, 0, z),
            Op::TextureRangeNull { first } => op(4, first, 0, 0, z),
            Op::CompFb { fb } => op(5, fb, 0, 0, z),
            Op::CompFbPos { fb } => op(6, fb, 0, 0, z),
            Op::FinalFb => op(7, 0, 0, 0, z),
            Op::BaseFb { fb } => op(8, fb, 0, 0, z),
            Op::Program { prog } => op(9, prog, 0, 0, z),
            Op::ClearColor => op(10, 0, 0, 0, z),
            Op::Tex { unit, tex } => op(11, unit, tex, 0, z),
            Op::Float { prog, id, v } => op(12, prog, id, 0, [v, 0.0, 0.0]),
            Op::Vec2 { prog, id, v } => op(13, prog, id, 0, [v[0], v[1], 0.0]),
            Op::Vec3 { prog, id, v } => op(14, prog, id, 0, v),
            Op::Quad1 { tex, flip } => op(15, tex, i32::from(flip), 0, z),
            Op::Quad2 {
                t0,
                t1,
                flip0,
                flip1,
            } => op(16, t0, t1, i32::from(flip0) | (i32::from(flip1) << 1), z),
        }
    }
}

fn boxed(plan: Plan) -> *mut Plan {
    Box::into_raw(Box::new(plan))
}

#[repr(C)]
pub struct BloomInC {
    pub blur_size: f32,
    pub iterations: i32,
    pub rgb_to_intensity: [f32; 3],
    pub programs_ok: bool,
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_pe_plan_bloom(p: BloomInC) -> *mut Plan {
    boxed(pe::plan_bloom(&pe::BloomIn {
        blur_size: p.blur_size,
        iterations: p.iterations,
        rgb_to_intensity: p.rgb_to_intensity,
        programs_ok: p.programs_ok,
    }))
}

/// # Safety
/// `clear` is null or points at the effect's valid `mbClearFrameBuffer`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_pe_plan_image_trail(
    amount: f32,
    frame_time: f32,
    program_ok: bool,
    clear: *mut bool,
) -> *mut Plan {
    // SAFETY: the caller's contract.
    let Some(clear) = (unsafe { clear.as_mut() }) else {
        return std::ptr::null_mut();
    };
    boxed(pe::plan_image_trail(
        &pe::ImageTrailIn {
            amount,
            frame_time,
            program_ok,
        },
        clear,
    ))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_pe_plan_radial_blur(
    size: f32,
    blur_start_dist: f32,
    target_size: Vector2l,
    program_ok: bool,
) -> *mut Plan {
    boxed(pe::plan_radial_blur(&pe::RadialBlurIn {
        size,
        blur_start_dist,
        target_size,
        program_ok,
    }))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_pe_plan_color_conv(
    fade_alpha: f32,
    plain_ok: bool,
    fade_ok: bool,
) -> *mut Plan {
    boxed(pe::plan_color_conv(&pe::ColorConvIn {
        fade_alpha,
        programs_ok: [plain_ok, fade_ok],
    }))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_pe_plan_final_copy() -> *mut Plan {
    boxed(pe::plan_final_copy())
}

/// Writes the next op to `out`; false when the plan is exhausted.
///
/// # Safety
/// `plan` is null or a live handle from an `ohpl_pe_plan_*` call; `out` is null or valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_pe_plan_next(plan: *mut Plan, out: *mut OpC) -> bool {
    // SAFETY: the caller's contract.
    let (Some(plan), Some(out)) = (unsafe { plan.as_mut() }, unsafe { out.as_mut() }) else {
        return false;
    };
    match plan.next_op() {
        Some(o) => {
            *out = o.into();
            true
        }
        None => false,
    }
}

/// Slot of the texture the effect returns.
///
/// # Safety
/// `plan` is null or a live handle.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_pe_plan_result(plan: *const Plan) -> i32 {
    // SAFETY: the caller's contract.
    unsafe { plan.as_ref() }.map_or(pe::NONE, |p| p.result)
}

/// # Safety
/// `plan` is null or a live handle, freed exactly once.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_pe_plan_free(plan: *mut Plan) {
    if !plan.is_null() {
        // SAFETY: the caller's contract; the pointer came from `Box::into_raw`.
        drop(unsafe { Box::from_raw(plan) });
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_pe_uv_pos_size(
    tex_size: Vector2f,
    screen: Vector2f,
    rt_pos: Vector2l,
    rt_size: Vector2l,
    out_pos: &mut Vector2f,
    out_size: &mut Vector2f,
) {
    (*out_pos, *out_size) = pe::uv_pos_size(tex_size, screen, rt_pos, rt_size);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_pe_round_target(
    uv_pos: Vector2f,
    uv_size: Vector2f,
    out_pos: &mut Vector2l,
    out_size: &mut Vector2l,
) {
    (*out_pos, *out_size) = pe::round_target(uv_pos, uv_size);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_pe_quad_uv(
    tex_size: Vector2f,
    rect: bool,
    uv_pos: Vector2f,
    uv_size: Vector2f,
    out_min: &mut Vector2f,
    out_max: &mut Vector2f,
) {
    (*out_min, *out_max) = pe::quad_uv(tex_size, rect, uv_pos, uv_size);
}

/// # Safety
/// `prios` is null or points at `n` valid ints.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_pe_insert_pos(prios: *const i32, n: usize, prio: i32) -> usize {
    let prios: &[i32] = if prios.is_null() || n == 0 {
        &[]
    } else {
        // SAFETY: the caller's contract.
        unsafe { slice::from_raw_parts(prios, n) }
    };
    pe::insert_pos(prios, prio)
}

/// Fills `out` (room for `n` steps) with the active effects' steps; returns how many.
///
/// # Safety
/// `active` points at `n` valid bools and `out` at room for `n` steps, or either is null.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_pe_chain(active: *const bool, n: usize, out: *mut pe::Step) -> usize {
    if active.is_null() || out.is_null() || n == 0 {
        return 0;
    }
    // SAFETY: the caller's contract.
    let (active, out) = unsafe {
        (
            slice::from_raw_parts(active, n),
            slice::from_raw_parts_mut(out, n),
        )
    };
    let steps = pe::chain(active);
    for (dst, src) in out.iter_mut().zip(&steps) {
        *dst = *src;
    }
    steps.len()
}
