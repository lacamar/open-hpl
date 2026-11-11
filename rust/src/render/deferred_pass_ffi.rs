//! C ABI of the light pass plans, declared in `graphics/RustRendererDeferred.h` (`ohpl_dr3_*`).

use std::slice;

use super::deferred::Plan;
use super::deferred_pass::{self as dp, PassIn, PassLight};

/// # Safety
/// `i` is null or valid; `lights` points at `n` valid elements.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dr3_plan_light_pass(
    i: *const PassIn,
    lights: *const PassLight,
    n: usize,
) -> *mut Plan {
    // SAFETY: the caller's contract.
    let Some(i) = (unsafe { i.as_ref() }) else {
        return std::ptr::null_mut();
    };
    let lights: &[PassLight] = if lights.is_null() || n == 0 {
        &[]
    } else {
        // SAFETY: the caller's contract.
        unsafe { slice::from_raw_parts(lights, n) }
    };
    Box::into_raw(Box::new(dp::plan_light_pass(*i, lights)))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_dr3_plan_render_lights(gbuffer_textures: i32) -> *mut Plan {
    Box::into_raw(Box::new(dp::plan_render_lights(gbuffer_textures)))
}

const _: () = {
    use std::mem::size_of;
    assert!(size_of::<PassIn>() == 32 && size_of::<PassLight>() == 40);
};
