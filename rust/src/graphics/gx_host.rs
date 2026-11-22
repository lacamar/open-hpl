//! GX: C++ engine objects behind the Rust-owned graphics classes. One table, registered once by
//! `RustGxHost.cpp`; every entry works on the C++ object it is given.

#![allow(clippy::not_unsafe_ptr_arg_deref)] // handles are opaque: only the host table dereferences them

use crate::scene::ls_host::{self, Vb};
use crate::types::Matrixf;
use std::ffi::c_void;
use std::sync::atomic::{AtomicPtr, Ordering};

#[repr(C)]
pub struct SubView {
    pub world: Matrixf,
    pub bv: *mut c_void,
    pub pos: *const f32,
    pub pos_len: usize,
    pub pos_stride: u32,
    pub nrm: *const f32,
    pub nrm_len: usize,
    pub nrm_stride: u32,
    pub idx: *const u32,
    pub idx_num: u32,
}

#[repr(C)]
pub struct Host {
    pub vb_create: unsafe extern "C" fn(ll: *mut c_void) -> *mut c_void,
    pub vb_compile: unsafe extern "C" fn(vb: *mut c_void, flags: i32) -> bool,
    pub sub_view: unsafe extern "C" fn(sub: *mut c_void, out: *mut SubView),
    pub make_mesh:
        unsafe extern "C" fn(res: *mut c_void, vb: *mut c_void, mat: *mut c_void) -> *mut c_void,
}

static HOST: AtomicPtr<Host> = AtomicPtr::new(std::ptr::null_mut());

/// # Safety
/// `h` points at a static table that lives for the whole run.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gx_set_host(h: *const Host) {
    HOST.store(h.cast_mut(), Ordering::Release);
}

fn host() -> Option<&'static Host> {
    // SAFETY: set once from a static table.
    unsafe { HOST.load(Ordering::Acquire).as_ref() }
}

pub fn vb_create(ll: *mut c_void) -> Vb {
    // SAFETY: table contract.
    Vb(host().map_or(std::ptr::null_mut(), |h| unsafe { (h.vb_create)(ll) }))
}

pub fn vb_compile(vb: Vb, flags: i32) -> bool {
    // SAFETY: table contract.
    host().is_some_and(|h| unsafe { (h.vb_compile)(vb.0, flags) })
}

pub fn vb_vertex_num(vb: Vb) -> i32 {
    // SAFETY: table contract.
    ls_host::host().map_or(0, |h| unsafe { (h.vb_vertex_num)(vb.0) })
}

pub fn sub_view(sub: *mut c_void) -> Option<SubView> {
    let h = host()?;
    let mut out = std::mem::MaybeUninit::<SubView>::zeroed();
    // SAFETY: the thunk fills every field; the zeroed value is valid for all of them.
    unsafe {
        (h.sub_view)(sub, out.as_mut_ptr());
        Some(out.assume_init())
    }
}

pub fn mat_create(res: *mut c_void, name: &str) -> *mut c_void {
    // SAFETY: `name` is valid for its length.
    ls_host::host().map_or(std::ptr::null_mut(), |h| unsafe {
        (h.mat_create)(res, name.as_ptr(), name.len())
    })
}

pub fn make_mesh(res: *mut c_void, vb: Vb, mat: *mut c_void) -> *mut c_void {
    // SAFETY: table contract.
    host().map_or(std::ptr::null_mut(), |h| unsafe {
        (h.make_mesh)(res, vb.0, mat)
    })
}

pub fn floats<'a>(p: *const f32, n: usize) -> &'a [f32] {
    if p.is_null() {
        &[]
    } else {
        // SAFETY: the vertex buffer holds `n` floats at `p` while the sub-mesh entity lives.
        unsafe { std::slice::from_raw_parts(p, n) }
    }
}

pub fn indices<'a>(p: *const u32, n: u32) -> &'a [u32] {
    if p.is_null() {
        &[]
    } else {
        // SAFETY: as `floats`.
        unsafe { std::slice::from_raw_parts(p, n as usize) }
    }
}
