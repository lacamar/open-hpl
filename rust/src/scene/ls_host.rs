//! LS: C++ engine objects behind the Rust-owned light, billboard, beam, flare, rope and
//! environment-particle shells. One table, registered once by `RustLsHost.cpp`; every entry takes the
//! C++ object it works on and never holds a Rust reference across the call.

use crate::math::bv::Bv;
use crate::types::{Color, Matrixf, Vector3f};
use std::ffi::c_void;
use std::sync::atomic::{AtomicPtr, Ordering};

pub const E_NORMAL: i32 = 0;
pub const E_POS: i32 = 1;
pub const E_COLOR0: i32 = 2;
pub const E_TANGENT: i32 = 4;
pub const E_TEX0: i32 = 5;
pub const E_TEX1: i32 = 6;
pub const F_NORMAL: i32 = 1;
pub const F_POS: i32 = 2;
pub const F_COLOR0: i32 = 4;
pub const F_TEX0: i32 = 0x10;
pub const F_TEX1: i32 = 0x20;
pub const USAGE_STATIC: i32 = 0;
pub const USAGE_DYNAMIC: i32 = 1;

#[repr(C)]
pub struct Host {
    pub vb_create:
        unsafe extern "C" fn(gfx: *mut c_void, usage: i32, verts: i32, idx: i32) -> *mut c_void,
    pub vb_elem: unsafe extern "C" fn(vb: *mut c_void, elem: i32, comps: i32),
    pub vb_add: unsafe extern "C" fn(vb: *mut c_void, elem: i32, comps: i32, v: *const f32),
    pub vb_index: unsafe extern "C" fn(vb: *mut c_void, i: i32),
    pub vb_compile: unsafe extern "C" fn(vb: *mut c_void, flags: i32),
    pub vb_array: unsafe extern "C" fn(vb: *mut c_void, elem: i32) -> *mut f32,
    pub vb_update: unsafe extern "C" fn(vb: *mut c_void, flags: i32),
    pub vb_count: unsafe extern "C" fn(vb: *mut c_void, n: i32),
    pub vb_free: unsafe extern "C" fn(vb: *mut c_void),
    pub mat_destroy: unsafe extern "C" fn(res: *mut c_void, mat: *mut c_void),
    pub rope_info: unsafe extern "C" fn(rope: *mut c_void, out: *mut RopeInfo),
    pub rope_points:
        unsafe extern "C" fn(rope: *mut c_void, out: *mut Vector3f, take: i32, smooth: bool) -> i32,
    pub ent_world: unsafe extern "C" fn(ctx: *mut c_void) -> *const Matrixf,
    pub ent_updated: unsafe extern "C" fn(ctx: *mut c_void),
    pub tex_destroy: unsafe extern "C" fn(res: *mut c_void, tex: *mut c_void),
    pub prog_create: unsafe extern "C" fn(
        gfx: *mut c_void,
        name: *const u8,
        len: usize,
        vars: *const Var,
        n: usize,
    ) -> *mut c_void,
    pub prog_destroy: unsafe extern "C" fn(gfx: *mut c_void, prog: *mut c_void),
    pub prog_var: unsafe extern "C" fn(prog: *mut c_void, name: *const u8, len: usize, id: i32),
    pub prog_set: unsafe extern "C" fn(prog: *mut c_void, id: i32, comps: i32, v: *const f32),
    pub ent_clip: unsafe extern "C" fn(ent: *mut c_void, out: *mut ClipInfo),
    pub spot_info: unsafe extern "C" fn(spot: *mut c_void, out: *mut SpotInfo),
    pub light_level: unsafe extern "C" fn(pos: *const Vector3f, out: *mut f32) -> bool,
}

/// A preprocessor variable of a GPU program; `has_value` picks the integer overload.
#[repr(C)]
pub struct Var {
    pub name: *const u8,
    pub len: usize,
    pub value: i32,
    pub has_value: bool,
}

#[repr(C)]
pub struct ClipInfo {
    pub active: bool,
    pub bv: *mut Bv,
}

#[repr(C)]
#[derive(Default)]
pub struct SpotInfo {
    pub color: Color,
    pub brightness: f32,
    pub radius: f32,
    pub falloff_pow: f32,
    pub spot_falloff_pow: f32,
    pub pos: Vector3f,
    pub visible: bool,
    pub view_proj: Matrixf,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct RopeInfo {
    pub update_count: i32,
    pub particles: i32,
    pub segment_length: f32,
    pub radius: f32,
}

static HOST: AtomicPtr<Host> = AtomicPtr::new(std::ptr::null_mut());

/// # Safety
/// `h` outlives the process and every entry is non-null.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ls_set_host(h: *const Host) {
    HOST.store(h.cast_mut(), Ordering::Release);
}

pub fn host() -> Option<&'static Host> {
    // SAFETY: set once from a static table.
    unsafe { HOST.load(Ordering::Acquire).as_ref() }
}

/// Handle of a C++ `iVertexBuffer`.
#[derive(Clone, Copy)]
pub struct Vb(pub *mut c_void);

impl Vb {
    pub fn create(gfx: *mut c_void, usage: i32, verts: i32, idx: i32) -> Vb {
        // SAFETY: table contract.
        Vb(host().map_or(std::ptr::null_mut(), |h| unsafe {
            (h.vb_create)(gfx, usage, verts, idx)
        }))
    }

    pub fn elem(self, elem: i32, comps: i32) {
        if let Some(h) = host() {
            // SAFETY: table contract.
            unsafe { (h.vb_elem)(self.0, elem, comps) }
        }
    }

    pub fn add(self, elem: i32, v: &[f32]) {
        if let Some(h) = host() {
            // SAFETY: `v` holds the component count.
            unsafe { (h.vb_add)(self.0, elem, v.len() as i32, v.as_ptr()) }
        }
    }

    pub fn color(self, elem: i32, c: Color) {
        self.add(elem, &[c.r, c.g, c.b, c.a]);
    }

    pub fn index(self, i: i32) {
        if let Some(h) = host() {
            // SAFETY: table contract.
            unsafe { (h.vb_index)(self.0, i) }
        }
    }

    pub fn compile(self, flags: i32) {
        if let Some(h) = host() {
            // SAFETY: table contract.
            unsafe { (h.vb_compile)(self.0, flags) }
        }
    }

    pub fn array(self, elem: i32) -> *mut f32 {
        // SAFETY: table contract.
        host().map_or(std::ptr::null_mut(), |h| unsafe {
            (h.vb_array)(self.0, elem)
        })
    }

    /// The first `n` floats of an element array.
    ///
    /// # Safety
    /// The array holds at least `n` floats and no other reference to it lives.
    pub unsafe fn floats<'a>(self, elem: i32, n: usize) -> &'a mut [f32] {
        let p = self.array(elem);
        if p.is_null() {
            &mut []
        } else {
            // SAFETY: caller contract.
            unsafe { std::slice::from_raw_parts_mut(p, n) }
        }
    }

    pub fn update(self, flags: i32) {
        if let Some(h) = host() {
            // SAFETY: table contract.
            unsafe { (h.vb_update)(self.0, flags) }
        }
    }

    pub fn set_count(self, n: i32) {
        if let Some(h) = host() {
            // SAFETY: table contract.
            unsafe { (h.vb_count)(self.0, n) }
        }
    }

    pub fn free(self) {
        if let (Some(h), false) = (host(), self.0.is_null()) {
            // SAFETY: table contract.
            unsafe { (h.vb_free)(self.0) }
        }
    }
}

pub fn mat_destroy(res: usize, mat: usize) {
    if let (Some(h), true) = (host(), mat != 0) {
        // SAFETY: table contract.
        unsafe { (h.mat_destroy)(res as *mut c_void, mat as *mut c_void) }
    }
}
