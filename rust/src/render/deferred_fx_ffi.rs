//! C ABI of the part D plans (`ohpl_dr4_*`), declared in `graphics/RustRendererDeferred.h`.

use std::slice;

use super::deferred::{Feed, Plan};
use super::deferred_box::plan_box_weighted;
use super::deferred_dir::{BoxIn, BoxLightIn, DirIn, plan_directional};
use super::deferred_fog::{FogAreaIn, FogParams, FogWorld, plan_fog, plan_full_screen_fog};
use super::deferred_fx::{
    DofIn, Hpl3In, NightIn, plan_apply_hpl3_ssao, plan_depth_of_field, plan_fog_depth,
    plan_night_vision,
};
use super::deferred_misc::{
    EnvIn, FlareUpdateIn, GlareFlare, GlareIn, IrisFlare, plan_environment_particles,
    plan_multi_iris_and_glare, plan_update_lens_flares,
};

fn boxed(p: Plan) -> *mut Plan {
    Box::into_raw(Box::new(p))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_dr4_plan_fog_depth(
    depth_in_normal_alpha: bool,
    linear_idx: i32,
    bind: bool,
    unit: i32,
) -> *mut Plan {
    boxed(plan_fog_depth(
        depth_in_normal_alpha,
        linear_idx,
        bind,
        unit,
    ))
}

/// # Safety
/// `i` is null or valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dr4_plan_night_vision(i: *const NightIn) -> *mut Plan {
    // SAFETY: the caller's contract.
    unsafe { i.as_ref() }.map_or(std::ptr::null_mut(), |i| boxed(plan_night_vision(i)))
}

/// # Safety
/// `i` is null or valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dr4_plan_hpl3_ssao(i: *const Hpl3In) -> *mut Plan {
    // SAFETY: the caller's contract.
    unsafe { i.as_ref() }.map_or(std::ptr::null_mut(), |i| boxed(plan_apply_hpl3_ssao(i)))
}

/// # Safety
/// `i` is null or valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dr4_plan_dof(i: *const DofIn) -> *mut Plan {
    // SAFETY: the caller's contract.
    unsafe { i.as_ref() }.map_or(std::ptr::null_mut(), |i| boxed(plan_depth_of_field(*i)))
}

/// # Safety
/// `w` and `p` are null or valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dr4_plan_full_screen_fog(
    w: *const FogWorld,
    p: *const FogParams,
) -> *mut Plan {
    // SAFETY: the caller's contract.
    match (unsafe { w.as_ref() }, unsafe { p.as_ref() }) {
        (Some(w), Some(p)) => boxed(plan_full_screen_fog(*w, *p)),
        _ => std::ptr::null_mut(),
    }
}

/// # Safety
/// `w` is null or valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dr4_plan_fog(w: *const FogWorld) -> *mut Plan {
    // SAFETY: the caller's contract.
    unsafe { w.as_ref() }.map_or(std::ptr::null_mut(), |w| boxed(plan_fog(*w)))
}

/// Answers the `AskFogs` op with the fog render data.
///
/// # Safety
/// `plan` is null or live; `areas` points at `n` valid elements.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dr4_feed_fogs(plan: *mut Plan, areas: *const FogAreaIn, n: usize) {
    let areas = if areas.is_null() || n == 0 {
        Vec::new()
    } else {
        // SAFETY: the caller's contract.
        unsafe { slice::from_raw_parts(areas, n) }.to_vec()
    };
    // SAFETY: the caller's contract.
    if let Some(p) = unsafe { plan.as_mut() } {
        p.feed(Feed::Fogs(areas));
    }
}

/// # Safety
/// `i` is null or valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dr4_plan_directional(i: *const DirIn) -> *mut Plan {
    // SAFETY: the caller's contract.
    unsafe { i.as_ref() }.map_or(std::ptr::null_mut(), |i| boxed(plan_directional(*i)))
}

/// # Safety
/// `i` is null or valid; `lights` points at `n` valid elements.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dr4_plan_box_weighted(
    i: *const BoxIn,
    lights: *const BoxLightIn,
    n: usize,
) -> *mut Plan {
    // SAFETY: the caller's contract.
    let Some(i) = (unsafe { i.as_ref() }) else {
        return std::ptr::null_mut();
    };
    let lights: &[BoxLightIn] = if lights.is_null() || n == 0 {
        &[]
    } else {
        // SAFETY: the caller's contract.
        unsafe { slice::from_raw_parts(lights, n) }
    };
    boxed(plan_box_weighted(i, lights))
}

/// The plan's `handled` result (`RenderLights_BoxWeighted`'s return value).
///
/// # Safety
/// `plan` is null or live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dr4_plan_handled(plan: *const Plan) -> bool {
    // SAFETY: the caller's contract.
    unsafe { plan.as_ref() }.is_some_and(|p| p.res.handled)
}

/// # Safety
/// `i` is null or valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dr4_plan_env(i: *const EnvIn) -> *mut Plan {
    // SAFETY: the caller's contract.
    unsafe { i.as_ref() }.map_or(std::ptr::null_mut(), |i| {
        boxed(plan_environment_particles(*i))
    })
}

/// # Safety
/// `i` is null or valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dr4_plan_update_flares(i: *const FlareUpdateIn) -> *mut Plan {
    // SAFETY: the caller's contract.
    unsafe { i.as_ref() }.map_or(std::ptr::null_mut(), |i| boxed(plan_update_lens_flares(*i)))
}

fn copied<T: Copy>(p: *const T, n: usize) -> Vec<T> {
    if p.is_null() || n == 0 {
        Vec::new()
    } else {
        // SAFETY: callers pass `n` valid elements.
        unsafe { slice::from_raw_parts(p, n) }.to_vec()
    }
}

/// # Safety
/// `i` is null or valid; each pointer addresses its count of valid elements.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dr4_plan_glare(
    i: *const GlareIn,
    flares: *const GlareFlare,
    nflares: usize,
    iris: *const IrisFlare,
    niris: usize,
    pos: *const f32,
    npos: usize,
) -> *mut Plan {
    // SAFETY: the caller's contract.
    let Some(i) = (unsafe { i.as_ref() }) else {
        return std::ptr::null_mut();
    };
    boxed(plan_multi_iris_and_glare(
        *i,
        copied(flares, nflares),
        copied(iris, niris),
        copied(pos, npos),
    ))
}

/// Answers an `AskGlare` op.
///
/// # Safety
/// `plan` is null or live; `v` points at `n` floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dr4_feed_floats(plan: *mut Plan, v: *const f32, n: usize) {
    // SAFETY: the caller's contract.
    if let Some(p) = unsafe { plan.as_mut() } {
        p.feed(Feed::Floats(copied(v, n)));
    }
}

const _: () = {
    use std::mem::size_of;
    assert!(
        size_of::<NightIn>() == 48
            && size_of::<Hpl3In>() == 28
            && size_of::<DofIn>() == 44
            && size_of::<FogWorld>() == 272
            && size_of::<FogParams>() == 88
            && size_of::<FogAreaIn>() == 168
            && size_of::<DirIn>() == 344
            && size_of::<BoxLightIn>() == 240
            && size_of::<BoxIn>() == 128
            && size_of::<EnvIn>() == 88
            && size_of::<FlareUpdateIn>() == 16
            && size_of::<GlareIn>() == 52
            && size_of::<GlareFlare>() == 8
            && size_of::<IrisFlare>() == 24
    );
};
