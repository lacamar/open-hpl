//! C ABI of `h4_res.rs`, mirrored in `HPL2/core/include/resources/RustResourcesH4.h`.

use super::h4_res::{Res, ResOps};
use crate::string_ffi::units;
use std::ffi::c_void;

/// # Safety
/// `ops` is a valid table whose callbacks accept its `ctx` for the life of the returned handle.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h4_res_new(ops: *const ResOps) -> *mut Res {
    // SAFETY: caller contract; the table is plain data.
    Box::into_raw(Box::new(Res::new(unsafe { std::ptr::read(ops) })))
}

/// # Safety
/// `r` comes from `ohpl_h4_res_new` and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h4_res_free(r: *mut Res) {
    // SAFETY: caller contract.
    drop(unsafe { Box::from_raw(r) });
}

/// # Safety
/// `r` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h4_res_init(r: *mut Res) {
    // SAFETY: caller contract.
    unsafe { &mut *r }.init();
}

/// # Safety
/// `r` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h4_res_update(r: *const Res, dt_bits: i64) {
    // SAFETY: caller contract.
    unsafe { &*r }.update(dt_bits);
}

/// # Safety
/// `r` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h4_res_shutdown(r: *mut Res) {
    // SAFETY: caller contract.
    unsafe { &mut *r }.shutdown();
}

/// # Safety
/// `r` live; pointers valid for their lengths.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h4_res_add_dir(
    r: *const Res,
    dir: *const u32,
    dn: usize,
    sub: bool,
    mask: *const u8,
    mn: usize,
) -> bool {
    // SAFETY: caller contract.
    unsafe { &*r }.add_resource_dir(unsafe { units(dir, dn) }, sub, unsafe { units(mask, mn) })
}

/// # Safety
/// `r` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h4_res_clear_dirs(r: *const Res) {
    // SAFETY: caller contract.
    unsafe { &*r }.clear_dirs();
}

/// # Safety
/// `r` live; `name` valid for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h4_res_add_loader(
    r: *mut Res,
    area: bool,
    name: *const u8,
    n: usize,
    loader: *mut c_void,
    set_default: bool,
) {
    // SAFETY: caller contract.
    let (r, name) = unsafe { (&mut *r, units(name, n)) };
    if area {
        r.add_area_loader(name, loader, set_default);
    } else {
        r.add_entity_loader(name, loader, set_default);
    }
}

/// # Safety
/// `r` live; `name` valid for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h4_res_get_loader(
    r: *const Res,
    area: bool,
    name: *const u8,
    n: usize,
) -> *mut c_void {
    // SAFETY: caller contract.
    let (r, name) = unsafe { (&*r, units(name, n)) };
    if area {
        r.area_loader(name)
    } else {
        r.entity_loader(name)
    }
}

/// # Safety
/// `r` live; pointers valid for their lengths.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h4_res_dirs_file(
    r: *const Res,
    file: *const u8,
    n: usize,
    alt: *const u32,
    an: usize,
) -> bool {
    // SAFETY: caller contract.
    unsafe { &*r }.load_resource_dirs_file(unsafe { units(file, n) }, unsafe { units(alt, an) })
}

/// # Safety
/// `r` live; `name` valid for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h4_res_load_xml(
    r: *mut Res,
    name: *const u8,
    n: usize,
) -> *mut c_void {
    // SAFETY: caller contract.
    unsafe { &mut *r }.load_xml_document(unsafe { units(name, n) })
}

/// # Safety
/// `r` live; `name` valid for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h4_res_load_bin(
    r: *mut Res,
    name: *const u8,
    n: usize,
) -> *mut c_void {
    // SAFETY: caller contract.
    unsafe { &mut *r }.load_binary_buffer(unsafe { units(name, n) })
}

/// # Safety
/// `r` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h4_res_destroy_xml(r: *mut Res, doc: *mut c_void) {
    // SAFETY: caller contract.
    unsafe { &mut *r }.destroy_xml_document(doc);
}

/// # Safety
/// `r` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h4_res_destroy_bin(r: *mut Res, buf: *mut c_void) {
    // SAFETY: caller contract.
    unsafe { &mut *r }.destroy_binary_buffer(buf);
}
