//! C ABI of the light/SSAO plans, declared in `graphics/RustRendererDeferred.h` (`ohpl_dl_*`).
//!
//! Plans are replayed through `ohpl_dr_plan_next`; these constructors and result getters
//! complete the `deferred_ffi` surface. Null pointers make a call a no-op returning the zero value.

use std::slice;

use super::deferred::Plan;
use super::deferred_light::{self as dl, EdgeIn, LightIn, LightOut, LightState, SsaoIn};
use super::deferred_lightlist::{self as ll, InitIn, InitLight, PairIn, SetupIn};
use crate::math::Matrixf;

fn boxed(plan: Option<Plan>) -> *mut Plan {
    plan.map_or(std::ptr::null_mut(), |p| Box::into_raw(Box::new(p)))
}

/// # Safety
/// `p` is null or points at `n` valid elements.
unsafe fn items<'a, T>(p: *const T, n: usize) -> &'a [T] {
    if p.is_null() || n == 0 {
        &[]
    } else {
        // SAFETY: the caller's contract.
        unsafe { slice::from_raw_parts(p, n) }
    }
}

/// Null when the pass is skipped.
///
/// # Safety
/// `i` is null or valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dl_plan_ssao(i: *const SsaoIn) -> *mut Plan {
    // SAFETY: the caller's contract.
    boxed(unsafe { i.as_ref() }.and_then(dl::plan_ssao))
}

/// Null when the pass is skipped.
///
/// # Safety
/// `i` is null or valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dl_plan_edge_smooth(i: *const EdgeIn) -> *mut Plan {
    // SAFETY: the caller's contract.
    boxed(unsafe { i.as_ref() }.and_then(dl::plan_edge_smooth))
}

/// # Safety
/// `s` is null or valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dl_plan_light_variables(s: *const LightState) -> *mut Plan {
    // SAFETY: the caller's contract.
    boxed(unsafe { s.as_ref() }.map(dl::plan_light_variables))
}

/// # Safety
/// `s` is null or valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dl_plan_light_program(
    s: *const LightState,
    extra_flags: u32,
    mode: i32,
) -> *mut Plan {
    // SAFETY: the caller's contract.
    boxed(unsafe { s.as_ref() }.map(|s| dl::plan_light_program(s, extra_flags, mode)))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_dl_plan_shadow_map(gbuffer_textures: i32) -> *mut Plan {
    boxed(Some(dl::plan_light_shadow_map(gbuffer_textures)))
}

/// # Safety
/// `i` is null or valid; `lights` and `pairs` point at `n` and `np` valid elements.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dl_plan_setup_lights(
    i: *const SetupIn,
    lights: *const LightIn,
    n: usize,
    pairs: *const PairIn,
    np: usize,
) -> *mut Plan {
    // SAFETY: the caller's contract.
    let (i, lights, pairs) = unsafe { (i.as_ref(), items(lights, n), items(pairs, np)) };
    boxed(i.map(|i| ll::plan_setup_lights(*i, lights.to_vec(), pairs)))
}

/// # Safety
/// `i` is null or valid; `lights` points at `n` valid elements.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dl_plan_init_lights(
    i: *const InitIn,
    lights: *const InitLight,
    n: usize,
) -> *mut Plan {
    // SAFETY: the caller's contract.
    let (i, lights) = unsafe { (i.as_ref(), items(lights, n)) };
    boxed(i.map(|i| ll::plan_init_lights(*i, lights.to_vec())))
}

/// What `SetupLights` decided for light `idx`; false when there is no such light.
///
/// # Safety
/// `plan` is null or a live handle; `out` is null or valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dl_plan_light_out(
    plan: *const Plan,
    idx: i32,
    out: *mut LightOut,
) -> bool {
    // SAFETY: the caller's contract.
    let (Some(p), Some(out)) = (unsafe { plan.as_ref() }, unsafe { out.as_mut() }) else {
        return false;
    };
    let found = usize::try_from(idx)
        .ok()
        .and_then(|i| p.res.lights.lights.get(i));
    found.is_some_and(|l| {
        *out = *l;
        true
    })
}

/// `m_mtxInvView` computed by `InitLights`.
///
/// # Safety
/// `plan` is null or a live handle; `out` is null or valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dl_plan_inv_view(plan: *const Plan, out: *mut Matrixf) {
    // SAFETY: the caller's contract.
    if let (Some(p), Some(out)) = (unsafe { plan.as_ref() }, unsafe { out.as_mut() }) {
        *out = p.res.lights.inv_view;
    }
}

/// Sorted light indices of list `which`; the pointer stays valid until the plan is freed.
///
/// # Safety
/// `plan` is null or a live handle; `out` is null or valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dl_plan_list(
    plan: *const Plan,
    which: i32,
    out: *mut *const u32,
) -> usize {
    // SAFETY: the caller's contract.
    let (Some(p), Some(out)) = (unsafe { plan.as_ref() }, unsafe { out.as_mut() }) else {
        return 0;
    };
    let list = usize::try_from(which)
        .ok()
        .and_then(|i| p.res.lights.lists.get(i));
    *out = list.map_or(std::ptr::null(), |l| l.as_ptr());
    list.map_or(0, Vec::len)
}

/// `mlNumberOfLightsRendered` after `InitLights`.
///
/// # Safety
/// `plan` is null or a live handle.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dl_plan_rendered(plan: *const Plan) -> i32 {
    // SAFETY: the caller's contract.
    unsafe { plan.as_ref() }.map_or(0, |p| p.res.lights.rendered)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_dl_light_shape(kind: i32, quality: i32) -> i32 {
    dl::light_shape(kind, quality)
}

const _: () = {
    use super::deferred_ffi::OpC;
    use super::deferred_lightlist::FrustumIn;
    use std::mem::{offset_of, size_of};
    assert!(size_of::<OpC>() == 72);
    assert!(size_of::<LightIn>() == 320 && offset_of!(LightIn, id) == 312);
    assert!(offset_of!(LightIn, shadow_res) == 48 && offset_of!(LightIn, world) == 80);
    assert!(offset_of!(LightIn, view_proj) == 208 && offset_of!(LightIn, box_size) == 272);
    assert!(offset_of!(LightIn, falloff_map) == 288);
    assert!(size_of::<LightOut>() == 156);
    assert!(size_of::<LightState>() == 576 && offset_of!(LightState, inv_view) == 448);
    assert!(offset_of!(LightState, shadow_size) == 516 && offset_of!(LightState, fog_color) == 528);
    assert!(
        offset_of!(LightState, depth_texture_idx) == 564
            && offset_of!(LightState, program_ok) == 568
    );
    assert!(
        size_of::<SsaoIn>() == 68
            && offset_of!(SsaoIn, far) == 12
            && offset_of!(SsaoIn, scatter_mul) == 48
    );
    assert!(size_of::<EdgeIn>() == 40);
    assert!(size_of::<SetupIn>() == 196 && offset_of!(SetupIn, frustum) == 40);
    assert!(size_of::<PairIn>() == 16);
    assert!(size_of::<InitLight>() == 328 && offset_of!(InitLight, area) == 324);
    assert!(size_of::<InitIn>() == 104);
    assert!(size_of::<FrustumIn>() == 156);
};
