//! C ABI of `lang.rs`, mirrored in `HPL2/core/include/resources/RustResourcesH2.h`.

use super::lang::{Lang, LangOps};
use crate::string_ffi::units;
use std::ffi::c_void;

/// # Safety
/// `ops` is a valid table whose callbacks accept its `ctx` for the life of the returned handle.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h2_lang_new(ops: *const LangOps) -> *mut Lang {
    // SAFETY: caller contract; the table is plain data.
    let ops = unsafe { std::ptr::read(ops) };
    Box::into_raw(Box::new(Lang::new(ops)))
}

/// # Safety
/// `l` comes from `ohpl_h2_lang_new` and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h2_lang_free(l: *mut Lang) {
    // SAFETY: caller contract.
    drop(unsafe { Box::from_raw(l) });
}

/// # Safety
/// `l` live; pointers valid for their lengths.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h2_lang_add_file(
    l: *mut Lang,
    file: *const u32,
    n: usize,
    add_dirs: bool,
    alt: *const u32,
    an: usize,
) -> bool {
    // SAFETY: caller contract.
    unsafe { (&mut *l).add_from_file(units(file, n), add_dirs, units(alt, an)) }
}

/// The host text, or null with the warning logged.
///
/// # Safety
/// `l` live; pointers valid for their lengths.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h2_lang_translate(
    l: *const Lang,
    cat: *const u8,
    cn: usize,
    name: *const u8,
    nn: usize,
) -> *mut c_void {
    // SAFETY: caller contract.
    unsafe { (&*l).translate(units(cat, cn), units(name, nn)) }
}

/// # Safety
/// `l` live; pointers valid for their lengths.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h2_lang_has(
    l: *const Lang,
    cat: *const u8,
    cn: usize,
    name: *const u8,
    nn: usize,
) -> bool {
    // SAFETY: caller contract.
    unsafe { (&*l).has_entry(units(cat, cn), units(name, nn)) }
}

/// # Safety
/// `l` live; pointers valid for their lengths.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h2_lang_set(
    l: *mut Lang,
    cat: *const u8,
    cn: usize,
    name: *const u8,
    nn: usize,
    text: *const i32,
    tn: usize,
) {
    // SAFETY: caller contract.
    unsafe { (&mut *l).set_entry(units(cat, cn), units(name, nn), units(text, tn)) }
}

type EachFn = unsafe extern "C" fn(*mut c_void, *const u8, usize, *const u8, usize, *mut c_void);

/// # Safety
/// `l` live; `f` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h2_lang_each(l: *const Lang, f: EachFn, ctx: *mut c_void) {
    // SAFETY: caller contract.
    unsafe { &*l }.for_each(|c, e, t| {
        // SAFETY: caller contract.
        unsafe { f(ctx, c.as_ptr(), c.len(), e.as_ptr(), e.len(), t) }
    });
}
