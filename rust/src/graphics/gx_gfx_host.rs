//! GX: engine objects behind the Rust-owned `cGraphics`. One table, registered once by
//! `RustGxHost.cpp`; every entry works on the C++ object it is given.

#![allow(clippy::not_unsafe_ptr_arg_deref)] // handles are opaque: only the host table dereferences them

use crate::types::{Vector2l, Vector3l};
use std::ffi::{c_char, c_void};
use std::sync::atomic::{AtomicPtr, Ordering};

pub const OBJ_TEXTURE: i32 = 0;
pub const OBJ_FRAME_BUFFER: i32 = 1;
pub const OBJ_DEPTH_STENCIL: i32 = 2;
pub const OBJ_GPU_PROGRAM: i32 = 3;
pub const OBJ_POST_EFFECT: i32 = 4;
pub const OBJ_COMPOSITE: i32 = 5;
pub const OBJ_POST_EFFECT_TYPE: i32 = 6;
pub const OBJ_RENDERER: i32 = 7;
pub const OBJ_MESH_CREATOR: i32 = 8;
pub const OBJ_TEXTURE_CREATOR: i32 = 9;
pub const OBJ_DECAL_CREATOR: i32 = 10;

#[repr(C)]
#[derive(Default)]
pub struct DsInfo {
    pub size: Vector2l,
    pub depth_bits: i32,
    pub stencil_bits: i32,
}

#[repr(C)]
pub struct LlInit {
    pub width: i32,
    pub height: i32,
    pub display: i32,
    pub bpp: i32,
    pub fullscreen: i32,
    pub multisampling: i32,
    pub gpu_format: i32,
    pub caption: *const u8,
    pub caption_len: usize,
    pub pos: Vector2l,
}

type Name = (*const u8, usize);

#[repr(C)]
pub struct GfxHost {
    pub delete_obj: unsafe extern "C" fn(kind: i32, p: *mut c_void),
    pub fatal: unsafe extern "C" fn(msg: *const u8, len: usize),
    pub ll_init: unsafe extern "C" fn(ll: *mut c_void, init: *const LlInit),
    pub ll_check_screen: unsafe extern "C" fn(ll: *mut c_void) -> bool,
    pub ll_create_texture: unsafe extern "C" fn(
        ll: *mut c_void,
        name: *const u8,
        len: usize,
        kind: i32,
        usage: i32,
    ) -> *mut c_void,
    pub ll_create_fb:
        unsafe extern "C" fn(ll: *mut c_void, name: *const u8, len: usize) -> *mut c_void,
    pub ll_create_ds: unsafe extern "C" fn(
        ll: *mut c_void,
        size: *const Vector2l,
        depth: i32,
        stencil: i32,
    ) -> *mut c_void,
    pub ll_create_prog:
        unsafe extern "C" fn(ll: *mut c_void, name: *const u8, len: usize) -> *mut c_void,
    pub creator_new:
        unsafe extern "C" fn(kind: i32, ll: *mut c_void, res: *mut c_void) -> *mut c_void,
    pub composite_new: unsafe extern "C" fn(gfx: *mut c_void) -> *mut c_void,
    pub res_add_dir: unsafe extern "C" fn(res: *mut c_void, path: *const u8, len: usize, add: bool),
    pub res_add_compat_dir: unsafe extern "C" fn(res: *mut c_void),
    pub res_check_features: unsafe extern "C" fn(res: *mut c_void),
    pub res_has_file: unsafe extern "C" fn(res: *mut c_void, name: *const u8, len: usize) -> bool,
    pub res_compile_texture_lookups: unsafe extern "C" fn(res: *mut c_void),
    pub renderer_new:
        unsafe extern "C" fn(kind: i32, gfx: *mut c_void, res: *mut c_void) -> *mut c_void,
    pub renderer_load: unsafe extern "C" fn(r: *mut c_void) -> bool,
    pub renderer_destroy_data: unsafe extern "C" fn(r: *mut c_void),
    pub renderer_update: unsafe extern "C" fn(r: *mut c_void, dt: f32),
    pub renderer_setup_funcs: unsafe extern "C" fn(r: *mut c_void, ll: *mut c_void),
    pub mat_type_new:
        unsafe extern "C" fn(code: i32, gfx: *mut c_void, res: *mut c_void) -> *mut c_void,
    pub mat_type_set_name: unsafe extern "C" fn(t: *mut c_void, name: *const u8, len: usize),
    pub mat_type_load: unsafe extern "C" fn(t: *mut c_void),
    pub mat_type_destroy_data: unsafe extern "C" fn(t: *mut c_void),
    pub mat_type_destroy: unsafe extern "C" fn(t: *mut c_void),
    pub mat_type_reload: unsafe extern "C" fn(t: *mut c_void),
    pub mat_destroy_type_specifics: unsafe extern "C" fn(on: bool),
    pub pe_type_new:
        unsafe extern "C" fn(kind: i32, gfx: *mut c_void, res: *mut c_void) -> *mut c_void,
    pub pe_type_name: unsafe extern "C" fn(t: *mut c_void, len: *mut usize) -> *const c_char,
    pub pe_type_create: unsafe extern "C" fn(t: *mut c_void, params: *mut c_void) -> *mut c_void,
    pub pe_set_params: unsafe extern "C" fn(pe: *mut c_void, params: *mut c_void),
    pub params_name: unsafe extern "C" fn(params: *mut c_void, len: *mut usize) -> *const c_char,
    pub tex_setup: unsafe extern "C" fn(tex: *mut c_void, size: *const Vector2l, format: i32),
    pub fb_attach: unsafe extern "C" fn(fb: *mut c_void, tex: *mut c_void) -> bool,
    pub ds_info: unsafe extern "C" fn(ds: *mut c_void, out: *mut DsInfo),
    pub ds_inc: unsafe extern "C" fn(ds: *mut c_void),
    pub ds_dec_has_users: unsafe extern "C" fn(ds: *mut c_void) -> bool,
    pub prog_set_res: unsafe extern "C" fn(prog: *mut c_void, res: *mut c_void),
    pub shader_create: unsafe extern "C" fn(
        res: *mut c_void,
        name: *const u8,
        len: usize,
        kind: i32,
        vars: *mut c_void,
    ) -> *mut c_void,
    pub shader_destroy: unsafe extern "C" fn(res: *mut c_void, shader: *mut c_void),
    pub prog_set_shader: unsafe extern "C" fn(prog: *mut c_void, kind: i32, shader: *mut c_void),
    pub prog_link: unsafe extern "C" fn(prog: *mut c_void),
    pub tex_fill: unsafe extern "C" fn(tex: *mut c_void, size: *const Vector3l, data: *mut u8),
}

static HOST: AtomicPtr<GfxHost> = AtomicPtr::new(std::ptr::null_mut());

/// # Safety
/// `h` points at a static table that lives for the whole run.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gx_set_gfx_host(h: *const GfxHost) {
    HOST.store(h.cast_mut(), Ordering::Release);
}

pub fn host() -> Option<&'static GfxHost> {
    // SAFETY: set once from a static table.
    unsafe { HOST.load(Ordering::Acquire).as_ref() }
}

pub fn name(s: &str) -> Name {
    (s.as_ptr(), s.len())
}

/// A C++ string handed back as pointer and length.
///
/// # Safety
/// `p` is null or points at `len` bytes that outlive the returned value.
pub unsafe fn text<'a>(p: *const c_char, len: usize) -> &'a str {
    if p.is_null() {
        ""
    } else {
        // SAFETY: caller contract.
        std::str::from_utf8(unsafe { std::slice::from_raw_parts(p.cast::<u8>(), len) })
            .unwrap_or("")
    }
}
