//! C ABI of the part E plans (`ohpl_dr5_*`), declared in `graphics/RustRendererDeferred.h`.

use std::slice;

use super::deferred::{Feed, Plan};
use super::deferred_obj::{
    DecalIn, SkyIn, TransIn, plan_decals, plan_gbuffer, plan_gbuffer_content, plan_illumination,
    plan_reflection_content, plan_skybox, plan_translucent, plan_z,
};

fn boxed(p: Plan) -> *mut Plan {
    Box::into_raw(Box::new(p))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_dr5_plan_z(n: i32) -> *mut Plan {
    boxed(plan_z(n))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_dr5_plan_gbuffer(n: i32) -> *mut Plan {
    boxed(plan_gbuffer(n))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_dr5_plan_illumination(n: i32) -> *mut Plan {
    boxed(plan_illumination(n))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_dr5_plan_gbuffer_content(w: f32, h: f32, num_textures: i32) -> *mut Plan {
    boxed(plan_gbuffer_content([w, h], num_textures))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_dr5_plan_reflection_content(w: f32, h: f32, div: i32) -> *mut Plan {
    boxed(plan_reflection_content([w, h], div))
}

/// # Safety
/// `i` is null or valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dr5_plan_decals(i: *const DecalIn) -> *mut Plan {
    // SAFETY: the caller's contract.
    unsafe { i.as_ref() }.map_or(std::ptr::null_mut(), |i| boxed(plan_decals(*i)))
}

/// # Safety
/// `i` is null or valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dr5_plan_skybox(i: *const SkyIn) -> *mut Plan {
    // SAFETY: the caller's contract.
    unsafe { i.as_ref() }.map_or(std::ptr::null_mut(), |i| boxed(plan_skybox(*i)))
}

/// # Safety
/// `i` is null or valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dr5_plan_translucent(i: *const TransIn) -> *mut Plan {
    // SAFETY: the caller's contract.
    unsafe { i.as_ref() }.map_or(std::ptr::null_mut(), |i| boxed(plan_translucent(*i)))
}

/// Answers an `ASK_TRANS_INFO`, `ASK_CLIP_RECT_OBJ` or `ASK_REFL_TARGET` op.
///
/// # Safety
/// `plan` is null or live; `v` points at `n` words.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dr5_feed_words(plan: *mut Plan, v: *const u32, n: usize) {
    let words = if v.is_null() || n == 0 {
        Vec::new()
    } else {
        // SAFETY: the caller's contract.
        unsafe { slice::from_raw_parts(v, n) }.to_vec()
    };
    // SAFETY: the caller's contract.
    if let Some(p) = unsafe { plan.as_mut() } {
        p.feed(Feed::Words(words));
    }
}
