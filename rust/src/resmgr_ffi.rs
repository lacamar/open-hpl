//! C ABI for `resmgr.rs`, mirrored in `HPL2/core/include/resources/RustResMgr.h`.

use crate::resmgr::{self, CustomMap};
use crate::string_ffi::{Sink, emit, units};
use std::ffi::c_void;

#[repr(C)]
pub struct RmOut {
    ctx: *mut c_void,
    name: Sink<u8>,
    path: Sink<u32>,
    item: Sink<u32>,
    res: *mut c_void,
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_rm_custom_new() -> *mut CustomMap {
    Box::into_raw(Box::default())
}

/// # Safety
/// `m` from `ohpl_rm_custom_new`, not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rm_custom_free(m: *mut CustomMap) {
    // SAFETY: caller contract.
    drop(unsafe { Box::from_raw(m) });
}

macro_rules! parse_fn {
    ($name:ident, $f:path) => {
        /// # Safety
        /// `s` valid for `n`.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name(s: *const u8, n: usize) -> i32 {
            // SAFETY: caller contract.
            $f(unsafe { units(s, n) })
        }
    };
}

parse_fn!(ohpl_rm_mat_texture_type, resmgr::mat::texture_type);
parse_fn!(ohpl_rm_mat_wrap, resmgr::mat::wrap);
parse_fn!(ohpl_rm_mat_anim_mode, resmgr::mat::anim_mode);
parse_fn!(ohpl_rm_world_billboard_type, resmgr::world::billboard_type);
parse_fn!(
    ohpl_rm_world_shadow_resolution,
    resmgr::world::shadow_resolution
);

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_rm_mat_texture_is_srgb(t: i32) -> bool {
    resmgr::mat::texture_is_srgb(t)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_rm_mat_anisotropy_accepted(
    x: f32,
    caps: i32,
    max: i32,
    current: f32,
) -> bool {
    resmgr::mat::anisotropy_accepted(x, caps, max, current)
}

/// Packs decal vertex data into `verts * 4` `[x, y, z, w]` rows (one per array).
/// False when an array is shorter than the vertex count needs; nothing is written. A NULL `out` only checks.
///
/// # Safety
/// Each `arrays[i]` valid for `lens[i]` floats; `out` writable for `verts * 16` floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rm_world_decal_vertices(
    arrays: *const *const f32,
    lens: *const usize,
    verts: usize,
    out: *mut f32,
) -> bool {
    // SAFETY: caller contract.
    unsafe {
        let a = |i: usize| units(*arrays.add(i), *lens.add(i));
        let Some(rows) = resmgr::world::decal_vertices([a(0), a(1), a(2), a(3)], verts) else {
            return false;
        };
        if !out.is_null() {
            for (i, r) in rows.iter().enumerate() {
                std::ptr::copy_nonoverlapping(r.as_ptr(), out.add(i * 4), 4);
            }
        }
    }
    true
}

/// `out.path` is the file path; `out.item` each directory prefix to create, shortest first.
///
/// # Safety
/// Pointers valid for their lengths; `out` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rm_cache_file(
    home: *const u32,
    hn: usize,
    file: *const u32,
    fnn: usize,
    ext: *const u32,
    en: usize,
    out: *mut RmOut,
) {
    // SAFETY: caller contract.
    unsafe {
        let (p, dirs) = resmgr::cache_file(units(home, hn), units(file, fnn), units(ext, en));
        emit((*out).path, (*out).ctx, &p);
        for d in dirs {
            emit((*out).item, (*out).ctx, p.get(..d).unwrap_or_default());
        }
    }
}
