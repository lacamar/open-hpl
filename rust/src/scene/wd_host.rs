//! WD: C++ engine objects behind the Rust-owned `cWorld`. One table, registered by `RustWd.cpp`.
//! Every entry takes the C++ object it works on; Rust never holds a reference across a call.

use super::wd::Pod;
use crate::types::{Color, Vector2f, Vector3f};
use std::ffi::c_void;
use std::sync::atomic::{AtomicPtr, Ordering};

#[repr(C)]
pub struct CreateArgs {
    pub name: *const u8,
    pub name_len: usize,
    pub size2: Vector2f,
    pub ty: i32,
    pub which: i32,
    pub flag: bool,
    pub id: i32,
    pub a: *mut c_void,
    pub b: *mut c_void,
}

#[repr(C)]
pub struct DumpItem {
    pub name: *const u8,
    pub name_len: usize,
    pub kind_name: *const u8,
    pub kind_len: usize,
    pub uid: i32,
    pub crouch: bool,
    pub mtx: [f32; 16],
    pub size: [f32; 3],
}

impl Default for DumpItem {
    fn default() -> Self {
        Self {
            name: std::ptr::null(),
            name_len: 0,
            kind_name: std::ptr::null(),
            kind_len: 0,
            uid: 0,
            crouch: false,
            mtx: [0.0; 16],
            size: [0.0; 3],
        }
    }
}

/// A field of `cForceField` as `GetForceFields` reads it.
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct FieldOut {
    pub min: Vector3f,
    pub max: Vector3f,
    pub radius: f32,
    pub usable: bool,
}

#[repr(C)]
pub struct Host {
    pub reg_foreign: unsafe extern "C" fn(reg: *mut c_void),
    pub root_new: unsafe extern "C" fn() -> *mut c_void,
    pub root_delete: unsafe extern "C" fn(node: *mut c_void),
    pub cont_new: unsafe extern "C" fn(dynamic: bool) -> *mut c_void,
    pub cont_delete: unsafe extern "C" fn(cont: *mut c_void),
    pub cont_add: unsafe extern "C" fn(cont: *mut c_void, rend: *mut c_void),
    pub cont_remove: unsafe extern "C" fn(cont: *mut c_void, rend: *mut c_void),
    pub cont_remove_set: unsafe extern "C" fn(cont: *mut c_void, rends: *const usize, n: usize),
    pub cont_compile: unsafe extern "C" fn(cont: *mut c_void),
    pub cont_bounds: unsafe extern "C" fn(cont: *mut c_void, out: *mut Vector3f),
    pub rend_of: unsafe extern "C" fn(kind: u32, p: *mut c_void) -> *mut c_void,
    pub is_static: unsafe extern "C" fn(kind: u32, p: *mut c_void) -> bool,
    pub set_static: unsafe extern "C" fn(kind: u32, p: *mut c_void, value: bool),
    pub set_world: unsafe extern "C" fn(kind: u32, p: *mut c_void, world: *mut c_void),
    pub create:
        unsafe extern "C" fn(kind: u32, pod: *mut Pod, args: *const CreateArgs) -> *mut c_void,
    pub light_gobo: unsafe extern "C" fn(
        res: *mut c_void,
        spot: bool,
        light: *mut c_void,
        name: *const u8,
        len: usize,
    ),
    pub bb_material: unsafe extern "C" fn(bb: *mut c_void, mat: *mut c_void),
    pub flare_halo: unsafe extern "C" fn(flare: *mut c_void, size: *const Vector2f),
    pub flare_material: unsafe extern "C" fn(flare: *mut c_void, mat: *mut c_void),
    pub mesh_subs: unsafe extern "C" fn(
        mesh: *mut c_void,
        out: *mut *mut c_void,
        cap: usize,
        skip_collide: bool,
    ) -> usize,
    pub mesh_draw_bb:
        unsafe extern "C" fn(gfx: *mut c_void, mesh: *mut c_void, color: *const Color),
    pub ps_create: unsafe extern "C" fn(
        res: *mut c_void,
        mode: i32,
        name: *const u8,
        name_len: usize,
        ty: *const u8,
        ty_len: usize,
        elem: *mut c_void,
        size: *const Vector3f,
    ) -> *mut c_void,
    pub ps_emitters:
        unsafe extern "C" fn(ps: *mut c_void, out: *mut *mut c_void, cap: usize) -> usize,
    pub ps_remove_when_dead: unsafe extern "C" fn(ps: *mut c_void, value: bool),
    pub update: unsafe extern "C" fn(kind: u32, p: *mut c_void, dt: f32) -> i32,
    pub phys_update: unsafe extern "C" fn(pw: *mut c_void, dt: f32),
    pub phys_attach: unsafe extern "C" fn(pw: *mut c_void, world: *mut c_void),
    pub phys_destroy: unsafe extern "C" fn(physics: *mut c_void, pw: *mut c_void),
    pub phys_set_size: unsafe extern "C" fn(pw: *mut c_void, bounds: *const Vector3f),
    pub phys_clear_sounds: unsafe extern "C" fn(pw: *mut c_void),
    pub sound_silent: unsafe extern "C" fn(sound: *mut c_void, silent: bool),
    pub sound_data:
        unsafe extern "C" fn(res: *mut c_void, name: *const u8, len: usize) -> *mut c_void,
    pub sound_creation_id: unsafe extern "C" fn(p: *mut c_void) -> i32,
    pub sky_vb_new: unsafe extern "C" fn(gfx: *mut c_void) -> *mut c_void,
    pub dirlight_new: unsafe extern "C" fn(res: *mut c_void) -> *mut c_void,
    pub dirlight_delete: unsafe extern "C" fn(light: *mut c_void),
    pub terrain_delete: unsafe extern "C" fn(terrain: *mut c_void),
    pub entfile_create:
        unsafe extern "C" fn(res: *mut c_void, name: *const u8, len: usize) -> *mut c_void,
    pub entfile_destroy: unsafe extern "C" fn(res: *mut c_void, ef: *mut c_void),
    pub entfile_info: unsafe extern "C" fn(
        ef: *mut c_void,
        ty: *mut *const u8,
        ty_len: *mut usize,
        name: *mut *const u8,
        name_len: *mut usize,
        has_vars: *mut bool,
    ),
    pub loader_get:
        unsafe extern "C" fn(res: *mut c_void, ty: *const u8, len: usize) -> *mut c_void,
    pub loader_static: unsafe extern "C" fn(loader: *mut c_void) -> bool,
    pub loader_load: unsafe extern "C" fn(
        loader: *mut c_void,
        ef: *mut c_void,
        world: *mut c_void,
        args: *const LoadArgs,
    ) -> *mut c_void,
    pub entity_source: unsafe extern "C" fn(ent: *mut c_void, ef: *mut c_void),
    pub temp_nodes_clear: unsafe extern "C" fn(world: *mut c_void),
    pub dump_item: unsafe extern "C" fn(kind: u32, p: *mut c_void, out: *mut DumpItem),
    pub field_info: unsafe extern "C" fn(p: *mut c_void, out: *mut FieldOut),
    pub log: unsafe extern "C" fn(error: bool, msg: *const u8, len: usize),
}

#[repr(C)]
pub struct LoadArgs {
    pub name: *const u8,
    pub name_len: usize,
    pub id: i32,
    pub active: bool,
    pub mtx: *const crate::types::Matrixf,
    pub scale: *const Vector3f,
    pub vars: *mut c_void,
}

static HOST: AtomicPtr<Host> = AtomicPtr::new(std::ptr::null_mut());

/// # Safety
/// `h` outlives the process and every entry is non-null.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_wd_set_host(h: *const Host) {
    HOST.store(h.cast_mut(), Ordering::Release);
}

pub fn host() -> Option<&'static Host> {
    // SAFETY: set once from a static table.
    unsafe { HOST.load(Ordering::Acquire).as_ref() }
}
