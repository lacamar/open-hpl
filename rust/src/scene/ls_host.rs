//! LS: C++ engine objects behind the Rust-owned light, billboard, beam, flare, rope and
//! environment-particle shells. One table, registered once by `RustLsHost.cpp`; every entry takes the
//! C++ object it works on and never holds a Rust reference across the call.

use crate::math::bv::{Bv, Frustum};
use crate::scene::h10::EntPub;
use crate::scene::sl::FlarePod;
use crate::types::{Color, Matrixf, Vector3f};
use std::ffi::{c_char, c_void};
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
    pub ent_updated: unsafe extern "C" fn(ctx: *mut c_void, callbacks: bool),
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
    pub hdr: unsafe extern "C" fn() -> bool,
    pub ent_pos: unsafe extern "C" fn(ctx: *mut c_void, out: *mut Vector3f),
    pub occ_assign: unsafe extern "C" fn(
        renderer: *mut c_void,
        ctx: *mut c_void,
        idx: i32,
        vb: *mut c_void,
        m: *const Matrixf,
        flag: bool,
    ),
    pub occ_samples: unsafe extern "C" fn(renderer: *mut c_void, ctx: *mut c_void, idx: i32) -> i32,
    pub renderer_frustum: unsafe extern "C" fn(renderer: *mut c_void) -> *mut Frustum,
    pub log: unsafe extern "C" fn(error: bool, msg: *const u8, len: usize),
    pub xml_open: unsafe extern "C" fn(
        res: *mut c_void,
        file: *const u8,
        len: usize,
        status: *mut i32,
    ) -> *mut c_void,
    pub xml_attr:
        unsafe extern "C" fn(doc: *mut c_void, name: *const u8, len: usize) -> *const c_char,
    pub xml_close: unsafe extern "C" fn(doc: *mut c_void),
    pub mat_create:
        unsafe extern "C" fn(res: *mut c_void, name: *const u8, len: usize) -> *mut c_void,
    pub ent_bv: unsafe extern "C" fn(ctx: *mut c_void) -> *mut Bv,
    pub flare_type_new:
        unsafe extern "C" fn(ctx: *mut c_void, flare: *mut FlarePod, kind: i32) -> *mut c_void,
    pub flare_type_delete: unsafe extern "C" fn(ty: *mut c_void),
    pub flare_parent_mesh: unsafe extern "C" fn(
        ctx: *mut c_void,
        vb: *mut *mut c_void,
        mtx: *mut *const Matrixf,
    ) -> bool,
    pub vb_vertex_num: unsafe extern "C" fn(vb: *mut c_void) -> i32,
    pub tex_create: unsafe extern "C" fn(
        res: *mut c_void,
        kind: i32,
        name: *const u8,
        len: usize,
    ) -> *mut c_void,
    pub tex_clamp: unsafe extern "C" fn(tex: *mut c_void, str_axis: bool),
    pub tex_anim: unsafe extern "C" fn(tex: *mut c_void, mode: i32, frame_time: f32),
    pub tracker_new: unsafe extern "C" fn() -> *mut c_void,
    pub tracker_delete: unsafe extern "C" fn(tracker: *mut c_void),
    pub frustum_new: unsafe extern "C" fn() -> *mut Frustum,
    pub frustum_delete: unsafe extern "C" fn(frustum: *mut Frustum),
    pub bb_visible: unsafe extern "C" fn(bb: *mut c_void, visible: bool),
    pub bb_color: unsafe extern "C" fn(bb: *mut c_void, color: *const Color),
    pub ps_visible: unsafe extern "C" fn(ps: *mut c_void, visible: bool),
    pub ps_set_light: unsafe extern "C" fn(ps: *mut c_void, light: *mut c_void),
    pub ps_leave_light: unsafe extern "C" fn(ps: *mut c_void),
    pub ps_light_color: unsafe extern "C" fn(ps: *mut c_void, color: *const Color),
    pub ps_light_brightness: unsafe extern "C" fn(ps: *mut c_void, brightness: f32),
    pub visible_change: unsafe extern "C" fn(callback: *mut c_void, ctx: *mut c_void),
    pub ease: unsafe extern "C" fn(ctx: *mut c_void, t: f32) -> f32,
    pub world_sound: unsafe extern "C" fn(
        world: *mut c_void,
        ctx: *mut c_void,
        on: bool,
        file: *const u8,
        len: usize,
    ),
    pub world_particle:
        unsafe extern "C" fn(world: *mut c_void, ctx: *mut c_void, ps: *const u8, len: usize),
    pub renderable_info:
        unsafe extern "C" fn(obj: *mut c_void, count: *mut i32, is_static: *mut bool),
    pub obj_name: unsafe extern "C" fn(kind: i32, obj: *mut c_void, len: *mut usize) -> *const u8,
    pub dump_info: unsafe extern "C" fn(kind: i32, obj: *mut c_void, out: *mut DumpInfo),
    pub obj_bv: unsafe extern "C" fn(kind: i32, obj: *mut c_void) -> *mut Bv,
    pub dump_list:
        unsafe extern "C" fn(world: *mut c_void, kind: i32, out: *mut usize, cap: usize) -> usize,
}

/// What the `OHPL_SL_DUMP` hook reads from an entity that its pod does not hold.
#[repr(C)]
pub struct DumpInfo {
    pub name: *const u8,
    pub name_len: usize,
    pub pod: usize,
    pub uid: i32,
    pub visible: bool,
}

impl Default for DumpInfo {
    fn default() -> Self {
        Self {
            name: std::ptr::null(),
            name_len: 0,
            pod: 0,
            uid: 0,
            visible: false,
        }
    }
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

/// The entity half of a shell: the C++ object, its `OhplEntPod` and its bounding volume.
#[derive(Clone, Copy)]
pub struct Ent {
    pub ctx: *mut c_void,
    pub e: *mut EntPub,
    pub bv: *mut Bv,
}

impl Ent {
    pub fn world(&self) -> *const Matrixf {
        // SAFETY: table contract.
        host().map_or(std::ptr::null(), |h| unsafe { (h.ent_world)(self.ctx) })
    }

    pub fn pos(&self) -> Vector3f {
        pos_of(self.ctx)
    }

    pub fn updated(&self) {
        if let Some(h) = host() {
            // SAFETY: table contract.
            unsafe { (h.ent_updated)(self.ctx, true) }
        }
    }

    pub fn count(&self) -> i32 {
        // SAFETY: the pod outlives the shell.
        unsafe { (*self.e).count }
    }

    pub fn bv<'a>(self) -> &'a mut Bv {
        // SAFETY: the bounding volume is a member of the shell's base class.
        unsafe { &mut *self.bv }
    }
}

pub fn pos_of(ctx: *mut c_void) -> Vector3f {
    let mut out = Vector3f::default();
    if let Some(h) = host() {
        // SAFETY: table contract.
        unsafe { (h.ent_pos)(ctx, &mut out) }
    }
    out
}

pub fn hdr() -> bool {
    // SAFETY: table contract.
    host().is_some_and(|h| unsafe { (h.hdr)() })
}

pub fn log(error: bool, msg: &str) {
    if let Some(h) = host() {
        // SAFETY: table contract.
        unsafe { (h.log)(error, msg.as_ptr(), msg.len()) }
    }
}

/// The `MAIN` element of an XML file found through `cFileSearcher`.
pub struct Xml(*mut c_void);

pub enum XmlError {
    NotFound,
    Unreadable,
    Load,
    NoMain,
}

impl Xml {
    pub fn open(res: usize, file: &[u8]) -> Result<Xml, XmlError> {
        let mut status = 0;
        let doc = host().map_or(std::ptr::null_mut(), |h| {
            // SAFETY: table contract.
            unsafe { (h.xml_open)(res as *mut c_void, file.as_ptr(), file.len(), &mut status) }
        });
        match status {
            0 if !doc.is_null() => Ok(Xml(doc)),
            1 => Err(XmlError::NotFound),
            2 => Err(XmlError::Unreadable),
            4 => Err(XmlError::NoMain),
            _ => Err(XmlError::Load),
        }
    }

    /// The attribute as a NUL-terminated string, or null.
    pub fn attr(&self, name: &str) -> *const c_char {
        // SAFETY: table contract.
        host().map_or(std::ptr::null(), |h| unsafe {
            (h.xml_attr)(self.0, name.as_ptr(), name.len())
        })
    }

    pub fn string(&self, name: &str) -> Vec<u8> {
        let p = self.attr(name);
        if p.is_null() {
            return Vec::new();
        }
        // SAFETY: the host returns a NUL-terminated string that lives until `drop`.
        unsafe { std::ffi::CStr::from_ptr(p) }.to_bytes().to_vec()
    }

    pub fn string_or(&self, name: &str, default: &str) -> Vec<u8> {
        if self.attr(name).is_null() {
            default.as_bytes().to_vec()
        } else {
            self.string(name)
        }
    }

    pub fn float(&self, name: &str, default: f32) -> f32 {
        let p = self.attr(name);
        if p.is_null() {
            return default;
        }
        // SAFETY: as `string`.
        unsafe { crate::string::to_float(p) }
    }

    pub fn boolean(&self, name: &str, default: bool) -> bool {
        let p = self.attr(name);
        if p.is_null() {
            return default;
        }
        // SAFETY: as `string`.
        crate::string::to_bool(unsafe { std::ffi::CStr::from_ptr(p) }.to_bytes())
    }

    pub fn floats<const N: usize>(&self, name: &str, default: [f32; N]) -> [f32; N] {
        let p = self.attr(name);
        let mut v = [0.0; N];
        // SAFETY: as `string`.
        let ok = !p.is_null()
            && crate::string::floats_exact(
                unsafe { std::ffi::CStr::from_ptr(p) }.to_bytes(),
                &mut v,
            );
        if ok { v } else { default }
    }
}

impl Drop for Xml {
    fn drop(&mut self) {
        if let Some(h) = host() {
            // SAFETY: table contract.
            unsafe { (h.xml_close)(self.0) }
        }
    }
}

pub fn mat_create(res: usize, name: &[u8]) -> usize {
    // SAFETY: table contract.
    host().map_or(0, |h| unsafe {
        (h.mat_create)(res as *mut c_void, name.as_ptr(), name.len()) as usize
    })
}

pub fn tex_create(res: usize, kind: i32, name: &[u8]) -> usize {
    // SAFETY: table contract.
    host().map_or(0, |h| unsafe {
        (h.tex_create)(res as *mut c_void, kind, name.as_ptr(), name.len()) as usize
    })
}

pub fn tex_destroy(res: usize, tex: usize) {
    if let (Some(h), true) = (host(), tex != 0) {
        // SAFETY: table contract.
        unsafe { (h.tex_destroy)(res as *mut c_void, tex as *mut c_void) }
    }
}

pub fn tex_clamp(tex: usize, str_axis: bool) {
    if let (Some(h), true) = (host(), tex != 0) {
        // SAFETY: table contract.
        unsafe { (h.tex_clamp)(tex as *mut c_void, str_axis) }
    }
}

pub fn ent_world_of(ctx: usize) -> Matrixf {
    // SAFETY: table contract; the matrix is copied at once.
    host().map_or_else(Matrixf::default, |h| unsafe {
        *(h.ent_world)(ctx as *mut c_void)
    })
}
