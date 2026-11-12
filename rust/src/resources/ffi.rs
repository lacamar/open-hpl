//! C ABI for the opaque H2 handles, mirrored in `HPL2/core/include/resources/RustResourcesH2.h`.

use super::config::Config;
use super::searcher::Searcher;
use crate::string;
use crate::string_ffi::{Sink, emit, units};
use std::ffi::c_void;

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h2_fs_new() -> *mut Searcher {
    Box::into_raw(Box::default())
}

/// # Safety
/// `fs` comes from `ohpl_h2_fs_new` and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h2_fs_free(fs: *mut Searcher) {
    // SAFETY: caller contract.
    drop(unsafe { Box::from_raw(fs) });
}

/// # Safety
/// `fs` live; `dir` valid for `dir_len` wide units; `mask` for `mask_len` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h2_fs_add_dir(
    fs: *mut Searcher,
    dir: *const u32,
    dir_len: usize,
    mask: *const u8,
    mask_len: usize,
    sub_dirs: bool,
) {
    // SAFETY: caller contract.
    let (fs, dir, mask) = unsafe { (&mut *fs, units(dir, dir_len), units(mask, mask_len)) };
    fs.add_directory(dir, mask, sub_dirs);
}

/// # Safety
/// `fs` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h2_fs_clear(fs: *mut Searcher) {
    // SAFETY: caller contract.
    unsafe { &mut *fs }.clear();
}

/// Path id, or -1 when unknown. `equal_count` (nullable) is written as the C++ does.
///
/// # Safety
/// `fs` live; `name` valid for `n`; `equal_count` null or writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h2_fs_get(
    fs: *const Searcher,
    name: *const u8,
    n: usize,
    equal_count: *mut i32,
) -> i64 {
    // SAFETY: caller contract.
    let (fs, name) = unsafe { (&*fs, units(name, n)) };
    let Some((id, count)) = fs.get(name, !equal_count.is_null()) else {
        if !equal_count.is_null() {
            // SAFETY: caller contract.
            unsafe { *equal_count = 0 };
        }
        return -1;
    };
    if let Some(c) = count
        && !equal_count.is_null()
    {
        // SAFETY: caller contract.
        unsafe { *equal_count = c };
    }
    i64::from(id)
}

/// Wide path of an id, valid until the next `add_dir` or `clear`.
///
/// # Safety
/// `fs` live; `len` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h2_fs_path(
    fs: *const Searcher,
    id: u32,
    len: *mut usize,
) -> *const u32 {
    // SAFETY: caller contract.
    let p = unsafe { &*fs }.path(id);
    // SAFETY: caller contract.
    unsafe { *len = p.len() };
    p.as_ptr()
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h2_cfg_new() -> *mut Config {
    Box::into_raw(Box::default())
}

/// # Safety
/// `cfg` comes from `ohpl_h2_cfg_new` and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h2_cfg_free(cfg: *mut Config) {
    // SAFETY: caller contract.
    drop(unsafe { Box::from_raw(cfg) });
}

/// 0 ok, 1 open failed, 2 CRC check failed, 3 parse failed.
///
/// # Safety
/// `cfg` live; `file` valid for `n` wide units.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h2_cfg_load(
    cfg: *mut Config,
    file: *const u32,
    n: usize,
    use_crc: bool,
) -> i32 {
    // SAFETY: caller contract.
    let (cfg, file) = unsafe { (&mut *cfg, units(file, n)) };
    cfg.load(&string::wide_to_utf8(file), use_crc) as i32
}

/// 0 ok, 1 open or write failed.
///
/// # Safety
/// `cfg` live; `file` valid for `n` wide units.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h2_cfg_save(
    cfg: *const Config,
    file: *const u32,
    n: usize,
    use_crc: bool,
) -> i32 {
    // SAFETY: caller contract.
    let (cfg, file) = unsafe { (&*cfg, units(file, n)) };
    cfg.save(&string::wide_to_utf8(file), use_crc) as i32
}

/// # Safety
/// `cfg` live; byte arguments valid for their lengths.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h2_cfg_set(
    cfg: *mut Config,
    level: *const u8,
    ln: usize,
    name: *const u8,
    nn: usize,
    val: *const u8,
    vn: usize,
) {
    // SAFETY: caller contract.
    let (cfg, level, name, val) =
        unsafe { (&mut *cfg, units(level, ln), units(name, nn), units(val, vn)) };
    cfg.set(level, name, val);
}

/// # Safety
/// `cfg` live; byte arguments valid for their lengths; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h2_cfg_get(
    cfg: *const Config,
    level: *const u8,
    ln: usize,
    name: *const u8,
    nn: usize,
    sink: Sink<u8>,
    ctx: *mut c_void,
) -> bool {
    // SAFETY: caller contract.
    let (cfg, level, name) = unsafe { (&*cfg, units(level, ln), units(name, nn)) };
    let Some(v) = cfg.get(level, name) else {
        return false;
    };
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, v) };
    true
}
