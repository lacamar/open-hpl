//! `iResourceLoaderHandler` and its mesh/bitmap/video/world handlers (H2 pass 2): loader dispatch
//! by file extension and the per-handler load/save flows. Loaders and the loaded objects stay C++.

// Loader and object pointers are opaque host handles handed back to the host.
#![allow(clippy::not_unsafe_ptr_arg_deref)]

use super::h3_ops::{ERROR, WStr};
use crate::resources::LoaderTable;
use crate::string;
use crate::string_ffi::units;
use std::ffi::c_void;

/// `HandlerOps::call` / `HandlerOps::query` codes.
pub mod h {
    pub const MESH_LOAD: i32 = 0;
    pub const MESH_SAVE: i32 = 1;
    pub const ANIMATION_LOAD: i32 = 2;
    pub const BITMAP_LOAD: i32 = 3;
    pub const BITMAP_SAVE: i32 = 4;
    pub const VIDEO_LOAD: i32 = 5;
    pub const WORLD_LOAD: i32 = 6;
    pub const MESH_SET_FULL_PATH: i32 = 7;
    pub const BITMAP_SET_FILE_NAME: i32 = 8;
    /// Force-cache loading is on and the file exists.
    pub const MESH_CACHE_USABLE: i32 = 9;
}

#[repr(C)]
pub struct HandlerOps {
    pub ctx: *mut c_void,
    pub log: unsafe extern "C" fn(*mut c_void, i32, *const u8, usize),
    pub call:
        unsafe extern "C" fn(*mut c_void, i32, *mut c_void, WStr, i64, *mut c_void) -> *mut c_void,
}

impl HandlerOps {
    fn call(
        &self,
        what: i32,
        obj: *mut c_void,
        file: &[u32],
        a: i64,
        b: *mut c_void,
    ) -> *mut c_void {
        // SAFETY: the host contract of `HandlerOps`.
        unsafe { (self.call)(self.ctx, what, obj, WStr::of(file), a, b) }
    }
}

/// `GetLoaderForFile(tString)`: the loader for the lowercase extension, null with an error logged.
///
/// # Safety
/// Every extension list registered in `t` is live.
pub unsafe fn find(t: &LoaderTable, ops: &HandlerOps, file: &[u8]) -> *mut c_void {
    let ext = string::to_lower(string::file_ext(file));
    // SAFETY: caller contract.
    let l = unsafe { t.find(&ext) };
    if l.is_null() {
        let msg = [
            b"No loader for file extension '".as_slice(),
            &ext,
            b"' found!\n",
        ]
        .concat();
        // SAFETY: the host contract of `HandlerOps`.
        unsafe { (ops.log)(ops.ctx, ERROR, msg.as_ptr(), msg.len()) };
    }
    l
}

/// One handler call: find the loader for `file`, then run `what`; null when no loader.
///
/// # Safety
/// As `find`.
pub unsafe fn run(
    t: &LoaderTable,
    ops: &HandlerOps,
    what: i32,
    file: &[u32],
    a: i64,
    b: *mut c_void,
) -> *mut c_void {
    if what == h::MESH_LOAD {
        // SAFETY: caller contract.
        return unsafe { load_mesh(t, ops, file, a) };
    }
    // SAFETY: caller contract.
    let l = unsafe { find(t, ops, &string::wide_to_utf8(file)) };
    if l.is_null() {
        return l;
    }
    let r = ops.call(what, l, file, a, b);
    if what == h::BITMAP_LOAD && !r.is_null() {
        let name = string::file_name(file);
        ops.call(h::BITMAP_SET_FILE_NAME, r, name, 0, std::ptr::null_mut());
    }
    r
}

/// `cMeshLoaderHandler::LoadMesh`: under force-cache a shipped `.msh` replaces the source model.
///
/// # Safety
/// As `find`.
unsafe fn load_mesh(t: &LoaderTable, ops: &HandlerOps, file: &[u32], flags: i64) -> *mut c_void {
    let msh = string::set_file_ext(file, &string::utf8_to_wide(b"msh"));
    if msh != file
        && !ops
            .call(
                h::MESH_CACHE_USABLE,
                std::ptr::null_mut(),
                &msh,
                0,
                std::ptr::null_mut(),
            )
            .is_null()
    {
        // SAFETY: caller contract.
        let m = unsafe { load_mesh(t, ops, &msh, flags) };
        if !m.is_null() {
            ops.call(h::MESH_SET_FULL_PATH, m, file, 0, std::ptr::null_mut());
            return m;
        }
    }
    // SAFETY: caller contract.
    let l = unsafe { find(t, ops, &string::wide_to_utf8(file)) };
    if l.is_null() {
        l
    } else {
        ops.call(h::MESH_LOAD, l, file, flags, std::ptr::null_mut())
    }
}

/// # Safety
/// `t`, `ops` live; every extension list registered in `t` is live; `name` valid for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h4_handler_find(
    t: *const LoaderTable,
    ops: *const HandlerOps,
    name: *const u8,
    n: usize,
) -> *mut c_void {
    // SAFETY: caller contract.
    unsafe { find(&*t, &*ops, units(name, n)) }
}

/// # Safety
/// `t`, `ops` live; every extension list registered in `t` is live; `file` valid for `n` wide units.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h4_handler_run(
    t: *const LoaderTable,
    ops: *const HandlerOps,
    what: i32,
    file: *const u32,
    n: usize,
    a: i64,
    b: *mut c_void,
) -> *mut c_void {
    // SAFETY: caller contract.
    unsafe { run(&*t, &*ops, what, units(file, n), a, b) }
}
