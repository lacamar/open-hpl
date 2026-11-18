//! C ABI for the widget logic, declared in `gui/RustWidget.h`.
//!
//! Pointers are valid, aligned and non-null for the call unless noted; a null input makes the call
//! a no-op that returns the zero value. Op buffers are `cap` entries long and the call returns the
//! number written.

use super::widgets_base as wb;

/// # Safety
/// `p` is null or valid for writes.
unsafe fn wr<T>(p: *mut T, v: T) {
    // SAFETY: caller contract.
    if let Some(r) = unsafe { p.as_mut() } {
        *r = v;
    }
}

/// # Safety
/// `p` is null or points to `n` readable elements.
unsafe fn items<'a, T>(p: *const T, n: usize) -> &'a [T] {
    if p.is_null() {
        &[]
    } else {
        // SAFETY: caller contract.
        unsafe { std::slice::from_raw_parts(p, n) }
    }
}

// ---------------------------------------------------------------- iWidget

// ---------------------------------------------------------------- image, dummy, group

/// `eFileBrowserFileType` of a lowercase extension, or -1 for none.
///
/// # Safety
/// `ext` has `len` wide chars.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_file_type(ext: *const u32, len: usize) -> i32 {
    // SAFETY: caller contract.
    wb::file_type_of_extension(unsafe { items(ext, len) }).unwrap_or(-1)
}

/// Returns the scaled size; `unit` receives the index into bytes/KB/MB/GB.
///
/// # Safety
/// `unit` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_human_size(size: u64, unit: *mut usize) -> u64 {
    let (v, u) = wb::human_readable_size(size);
    // SAFETY: caller contract.
    unsafe { wr(unit, u) };
    v
}

/// # Safety
/// `out` has `cap` writable chars.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_drive_letters(mask: u64, out: *mut u32, cap: usize) -> usize {
    if out.is_null() {
        return 0;
    }
    // SAFETY: caller contract.
    wb::drive_letters(mask, unsafe { std::slice::from_raw_parts_mut(out, cap) })
}

/// # Safety
/// `path` has `len` wide chars.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_navigate_target(path: *const u32, len: usize) -> i32 {
    // SAFETY: caller contract.
    wb::navigate_target(unsafe { items(path, len) }) as i32
}

/// # Safety
/// `piece` has `len` wide chars.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_path_piece(piece: *const u32, len: usize, depth: usize) -> i32 {
    // SAFETY: caller contract.
    wb::path_piece(unsafe { items(piece, len) }, depth) as i32
}
