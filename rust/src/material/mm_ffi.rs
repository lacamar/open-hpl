//! C ABI of the `cMaterial` lifecycle, declared in `graphics/RustMaterialH7.h`.

use core::sync::atomic::Ordering;

use super::h7::MatState;
use super::mm::DESTROY_TYPE_SPECIFICS;
use crate::graphics::mm::Host;

type P = *mut core::ffi::c_void;

/// `host` stays valid for the process; `type_` is null or a live iMaterialType.
///
/// # Safety
/// `host` is a valid table.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mm_mat_new(
    host: *const Host,
    owner: P,
    type_: P,
    resources: P,
) -> *mut MatState {
    // SAFETY: caller contract.
    let Some(host) = (unsafe { host.as_ref() }) else {
        return core::ptr::null_mut();
    };
    // SAFETY: `type_` is null or live.
    let translucent = !type_.is_null() && unsafe { (host.type_translucent)(type_) };
    let mut m = MatState::new(translucent);
    m.host = Some(host);
    m.owner = owner;
    m.resources = resources;
    m.set_type(type_);
    Box::into_raw(Box::new(m))
}

/// # Safety
/// `m` is null or from `ohpl_mm_mat_new`, freed once.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mm_mat_free(m: *mut MatState) {
    if !m.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(m) });
    }
}

/// # Safety
/// `m` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mm_mat_set_type(m: *mut MatState, t: P) {
    // SAFETY: caller contract.
    if let Some(m) = unsafe { m.as_mut() } {
        m.set_type(t);
    }
}

/// # Safety
/// `m` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mm_mat_compile(m: *mut MatState) {
    // SAFETY: caller contract.
    if let Some(m) = unsafe { m.as_mut() } {
        m.compile();
    }
}

/// # Safety
/// `m` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mm_mat_compile_texture_lookup(m: *mut MatState) {
    // SAFETY: caller contract.
    if let Some(m) = unsafe { m.as_mut() } {
        m.compile_texture_lookup();
    }
}

/// # Safety
/// `m` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mm_mat_vars_object(m: *const MatState) -> P {
    // SAFETY: caller contract.
    unsafe { m.as_ref() }.map_or(core::ptr::null_mut(), MatState::vars_object)
}

/// # Safety
/// `m` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mm_mat_load_vars(m: *const MatState, obj: P) {
    // SAFETY: caller contract.
    if let Some(m) = unsafe { m.as_ref() } {
        m.load_vars(obj);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_mm_mat_set_destroy_type_specifics(on: bool) {
    DESTROY_TYPE_SPECIFICS.store(on, Ordering::Relaxed);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_mm_mat_destroy_type_specifics() -> bool {
    DESTROY_TYPE_SPECIFICS.load(Ordering::Relaxed)
}
