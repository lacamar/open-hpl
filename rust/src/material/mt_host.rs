//! Engine calls of the material types and program combo managers (stage MT): GPU programs and
//! shaders, textures and the renderer/object views. Mirrored by `OhplMtHost` in
//! `graphics/RustMt.h`; filled by `graphics/RustMtHost.cpp` over the classes still in C++.

use core::ffi::c_void;

use super::ffi::{HostVt, VarsVt};
use super::types::{Globals, ObjView, RenderView, Uniform};

pub type P = *mut c_void;

#[repr(C)]
pub struct KV {
    pub name: *const u8,
    pub name_len: usize,
    pub value: *const u8,
    pub value_len: usize,
}

/// `HostCtx` of the C++ lazy queries.
#[repr(C)]
pub struct HostCtx {
    pub renderer: P,
    pub object: P,
    pub fields: *mut P,
    pub field_num: *mut i32,
}

#[repr(C)]
pub struct MtHost {
    pub prog_create: unsafe extern "C" fn(P, *const u8, usize) -> P,
    pub prog_link: unsafe extern "C" fn(P) -> bool,
    pub prog_delete: unsafe extern "C" fn(P),
    pub prog_set_shaders: unsafe extern "C" fn(P, P, P),
    pub prog_set_owned: unsafe extern "C" fn(P, P),
    pub prog_shader: unsafe extern "C" fn(P, i32) -> P,
    pub prog_set_user_id: unsafe extern "C" fn(P, u32),
    pub prog_user_id: unsafe extern "C" fn(P) -> u32,
    pub prog_name: unsafe extern "C" fn(P, *mut *const u8, *mut usize),
    pub prog_var_id: unsafe extern "C" fn(P, *const u8, usize, i32),
    pub prog_apply: unsafe extern "C" fn(P, *const Uniform, usize),
    pub shader_create: unsafe extern "C" fn(P, *const u8, usize, i32, *const KV, usize, bool) -> P,
    pub shader_destroy: unsafe extern "C" fn(P, P),
    pub shader_set_user_id: unsafe extern "C" fn(P, u32),
    pub shader_user_id: unsafe extern "C" fn(P) -> u32,
    pub shader_name: unsafe extern "C" fn(P, *mut *const u8, *mut usize),
    pub error: unsafe extern "C" fn(*const u8, usize),
    pub globals: unsafe extern "C" fn(P, f32, *mut Globals),
    pub render_view: unsafe extern "C" fn(P, *mut RenderView),
    /// fills the view and returns the object's material state (null when none)
    pub obj_view: unsafe extern "C" fn(P, *mut ObjView) -> P,
    pub host_vt: unsafe extern "C" fn(*mut HostCtx, *mut HostVt),
    pub vars_vt: unsafe extern "C" fn(P, *mut VarsVt),
    pub vars_new: unsafe extern "C" fn(i32) -> P,
    pub vars_pod: unsafe extern "C" fn(i32, P) -> P,
    pub vars_object_new: unsafe extern "C" fn() -> P,
    pub tex_create_2d: unsafe extern "C" fn(P, *const u8, usize, bool) -> P,
    pub tex_destroy: unsafe extern "C" fn(P, P),
    pub tex_white_new: unsafe extern "C" fn(P) -> P,
    pub tex_white_free: unsafe extern "C" fn(P, P),
    pub tex_info: unsafe extern "C" fn(P, *mut i32, *mut i32, *mut bool),
    pub special_tex: unsafe extern "C" fn(P, i32) -> P,
    pub file_exists: unsafe extern "C" fn(P, *const u8, usize) -> bool,
}
