//! C ABI of `aupd.rs`.

use super::a7_area::A7Area;
use super::a7_ffi::Ctx;
use super::aupd;
use std::ffi::c_void;

/// # Safety
/// `a` live; `shell` the C++ area of `a`; `ctx` the map context.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_area_update(
    a: *mut A7Area,
    shell: *mut c_void,
    ctx: *mut Ctx,
    step: f32,
) {
    // SAFETY: caller contract.
    unsafe { aupd::update(a, shell as usize, ctx, step) };
}

/// # Safety
/// `a` a live sticky block; `body` a live physics body.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_area_sticky_attach_body(a: *mut A7Area, body: *mut c_void) {
    // SAFETY: caller contract.
    unsafe { aupd::attach_body(a, body as usize) };
}

/// # Safety
/// `a` a live sticky block; `shell` its C++ area; `ctx` the map context.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a7_area_sticky_detach(
    a: *mut A7Area,
    shell: *mut c_void,
    ctx: *mut Ctx,
) {
    // SAFETY: caller contract.
    unsafe { aupd::detach_body(a, shell as usize, ctx) };
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a7_sticky_allow(set: bool, value: bool) -> bool {
    if set {
        aupd::set_allow_attachment(value);
    }
    aupd::allow_attachment()
}
