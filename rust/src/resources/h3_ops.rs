//! Host op table and shared load flow for the resource managers (H3 second pass).
//! Rust owns lookup, path resolution, the load/create decision, refcounts, logging and
//! bookkeeping; C++ only constructs and initialises the concrete resource objects.

// Resource pointers are opaque host handles handed back to the host, never dereferenced here.
#![allow(clippy::not_unsafe_ptr_arg_deref)]

use super::manager::{self, Manager};
use crate::resmgr::{self, Env};
use crate::resources::ResMeta;
use crate::string;
use crate::string_ffi::{Sink, units};
use std::ffi::c_void;
use std::sync::atomic::{AtomicBool, Ordering};

pub static LOG_CREATE_AND_DELETE: AtomicBool = AtomicBool::new(false);

pub const LOG: i32 = 0;
pub const WARNING: i32 = 1;
pub const ERROR: i32 = 2;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Str {
    pub p: *const u8,
    pub n: usize,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct WStr {
    pub p: *const u32,
    pub n: usize,
}

impl Str {
    pub fn of(v: &[u8]) -> Self {
        Self {
            p: v.as_ptr(),
            n: v.len(),
        }
    }
    pub const fn empty() -> Self {
        Self {
            p: std::ptr::null(),
            n: 0,
        }
    }
}

impl WStr {
    pub fn of(v: &[u32]) -> Self {
        Self {
            p: v.as_ptr(),
            n: v.len(),
        }
    }
    pub const fn empty() -> Self {
        Self {
            p: std::ptr::null(),
            n: 0,
        }
    }
}

#[repr(C)]
pub struct Info {
    pub name: Str,
    pub path: WStr,
    pub meta: *mut ResMeta,
}

/// Object kinds the host creates (`Create::kind`).
pub mod k {
    pub const ANIMATION: i32 = 0;
    pub const ENT_FILE: i32 = 1;
    pub const MESH: i32 = 2;
    pub const SCRIPT: i32 = 3;
    pub const SOUND_DATA: i32 = 4;
    pub const SOUND_ENTITY: i32 = 5;
    pub const VIDEO: i32 = 6;
    pub const FONT_TTF: i32 = 7;
    pub const FONT_BITMAP: i32 = 8;
    pub const FONT_UNKNOWN: i32 = 9;
    pub const PS_DATA_FILE: i32 = 10;
    pub const PS_DATA_ELEMENT: i32 = 11;
    pub const PS_CREATE: i32 = 12;
    pub const TEX_SIMPLE: i32 = 13;
    pub const TEX_ANIM: i32 = 14;
    pub const TEX_CUBE: i32 = 15;
    pub const IMAGE: i32 = 16;
    pub const MATERIAL: i32 = 17;
    pub const CUSTOM_RESOLVER: i32 = 18;
    pub const IMG_BITMAP_FRAME: i32 = 19;
    pub const IMG_CUSTOM_FRAME: i32 = 20;
    pub const IMG_ADD_BITMAP: i32 = 21;
    pub const SHADER_FILE: i32 = 22;
    pub const SHADER_STRING: i32 = 23;
    pub const SHADER_PARSE: i32 = 24;
    pub const SHADER_TRANSPILE: i32 = 25;
}

/// Small host calls (`Ops::call`).
pub mod c {
    pub const DESTROY_VIRTUAL: i32 = 1;
    pub const IS_STREAM: i32 = 2;
    pub const SOUND_SET_MGR: i32 = 3;
    pub const MEM_SIZE: i32 = 4;
    pub const UPDATE: i32 = 5;
    pub const PRELOAD_SOUNDS: i32 = 6;
    pub const IMG_FRAME_TEX: i32 = 7;
    pub const IMG_FRAME_BMP: i32 = 8;
    pub const FRAME_DEC_PIC: i32 = 9;
    pub const BMPFRAME_DEC_PIC: i32 = 10;
    pub const FRAME_IS_EMPTY: i32 = 11;
    pub const FRAME_IS_CUSTOM: i32 = 12;
    pub const BMPFRAME_HANDLE: i32 = 13;
    pub const BMPFRAME_TEX: i32 = 14;
    pub const BMPFRAME_IS_FULL: i32 = 15;
    pub const BMPFRAME_IS_LOCKED: i32 = 16;
    pub const BMPFRAME_ADDITIONS: i32 = 17;
    pub const BMPFRAME_REORGANIZE: i32 = 18;
    pub const BMPFRAME_FLUSH: i32 = 19;
    pub const BMPFRAME_SET_LOCKED: i32 = 20;
    pub const BMPFRAME_DELETE: i32 = 21;
    pub const FRAME_DELETE: i32 = 22;
    pub const FRAME_HANDLE: i32 = 23;
    pub const BMP_WIDTH: i32 = 24;
    pub const BMP_HEIGHT: i32 = 25;
    pub const IMAGE_SET_PRELOADED: i32 = 26;
    pub const PHYSICS_NAME_OF: i32 = 27;
    pub const BMP_DELETE: i32 = 29;
    pub const SHADER_POST: i32 = 30;
    pub const SHADER_ERROR: i32 = 31;
    pub const MAT_COMPILE_LOOKUP: i32 = 32;
    pub const MAT_SET_FILTER: i32 = 33;
    pub const MAT_SET_ANISO: i32 = 34;
}

#[repr(C)]
pub struct Create {
    pub kind: i32,
    pub status: i32,
    pub idx: i32,
    pub name: Str,
    pub name2: Str,
    pub path: WStr,
    pub paths: *const WStr,
    pub npaths: usize,
    pub i: [i32; 4],
    pub f: [f32; 4],
    pub ptr: *mut c_void,
    pub ptr2: *mut c_void,
}

impl Create {
    pub fn new(kind: i32) -> Self {
        Self {
            kind,
            status: 0,
            idx: 0,
            name: Str::empty(),
            name2: Str::empty(),
            path: WStr::empty(),
            paths: std::ptr::null(),
            npaths: 0,
            i: [0; 4],
            f: [0.0; 4],
            ptr: std::ptr::null_mut(),
            ptr2: std::ptr::null_mut(),
        }
    }
}

#[repr(C)]
pub struct Ops {
    pub ctx: *mut c_void,
    pub log: unsafe extern "C" fn(*mut c_void, i32, *const u8, usize),
    pub now: unsafe extern "C" fn(*mut c_void) -> u64,
    pub file_path:
        unsafe extern "C" fn(*mut c_void, *const u8, usize, *mut i32, *mut usize) -> *const u32,
    pub info: unsafe extern "C" fn(*mut c_void, *mut c_void, *mut Info),
    pub destroy: unsafe extern "C" fn(*mut c_void, *mut c_void),
    pub load_done: unsafe extern "C" fn(*mut c_void, *const u8, usize, u64),
    pub create: unsafe extern "C" fn(*mut c_void, *mut Create) -> *mut c_void,
    pub call: unsafe extern "C" fn(*mut c_void, i32, *mut c_void, i64) -> i64,
    pub text: unsafe extern "C" fn(*mut c_void, i32, *mut c_void, WStr, Sink<u8>, *mut c_void),
}

pub struct ResInfo {
    pub name: Vec<u8>,
    pub path: Vec<u32>,
    pub meta: *mut ResMeta,
}

impl Ops {
    pub fn log(&self, level: i32, msg: &[u8]) {
        // SAFETY: the host contract of `Ops`.
        unsafe { (self.log)(self.ctx, level, msg.as_ptr(), msg.len()) }
    }

    pub fn err(&self, parts: &[&[u8]]) {
        self.log(ERROR, &parts.concat());
    }

    pub fn now(&self) -> u64 {
        // SAFETY: the host contract of `Ops`.
        unsafe { (self.now)(self.ctx) }
    }

    pub fn file_path(&self, name: &[u8], want_count: bool) -> (Vec<u32>, i32) {
        self.file_path_seeded(name, want_count, 0)
    }

    /// `seed` is the count the host reports when the searcher leaves it untouched.
    pub fn file_path_seeded(&self, name: &[u8], want_count: bool, seed: i32) -> (Vec<u32>, i32) {
        let mut count = seed;
        let mut len = 0;
        let counter = if want_count {
            &raw mut count
        } else {
            std::ptr::null_mut()
        };
        // SAFETY: the host returns a path valid for `len` units until its next call.
        let path = unsafe {
            let p = (self.file_path)(self.ctx, name.as_ptr(), name.len(), counter, &raw mut len);
            units(p, len).to_vec()
        };
        (path, count)
    }

    pub fn info(&self, res: *mut c_void) -> ResInfo {
        let mut i = Info {
            name: Str::empty(),
            path: WStr::empty(),
            meta: std::ptr::null_mut(),
        };
        // SAFETY: the host fills `i` with pointers into the live resource.
        unsafe {
            (self.info)(self.ctx, res, &raw mut i);
            ResInfo {
                name: units(i.name.p, i.name.n).to_vec(),
                path: units(i.path.p, i.path.n).to_vec(),
                meta: i.meta,
            }
        }
    }

    pub fn destroy(&self, res: *mut c_void) {
        // SAFETY: the host contract of `Ops`.
        unsafe { (self.destroy)(self.ctx, res) }
    }

    pub fn create(&self, c: &mut Create) -> *mut c_void {
        // SAFETY: the host contract of `Ops`.
        unsafe { (self.create)(self.ctx, c) }
    }

    pub fn call(&self, what: i32, obj: *mut c_void, i: i64) -> i64 {
        // SAFETY: the host contract of `Ops`.
        unsafe { (self.call)(self.ctx, what, obj, i) }
    }

    pub fn text(&self, what: i32, obj: *mut c_void, path: &[u32]) -> Vec<u8> {
        unsafe extern "C" fn put(ctx: *mut c_void, p: *const u8, n: usize) {
            // SAFETY: `ctx` is the `Vec` handed in below.
            unsafe { (*ctx.cast::<Vec<u8>>()).extend_from_slice(units(p, n)) }
        }
        let mut out = Vec::new();
        // SAFETY: the sink writes into `out`, alive for the call.
        unsafe {
            (self.text)(
                self.ctx,
                what,
                obj,
                WStr::of(path),
                put,
                (&raw mut out).cast(),
            );
        }
        out
    }
}

unsafe extern "C" {
    fn time(t: *mut i64) -> i64;
}

/// `time(NULL)`.
pub fn unix_time() -> u64 {
    // SAFETY: libc `time` accepts a null pointer.
    u64::try_from(unsafe { time(std::ptr::null_mut()) }).unwrap_or(0)
}

pub fn wide8(p: &[u32]) -> Vec<u8> {
    string::wide_to_utf8(p)
}

struct FlowEnv<'a> {
    ops: &'a Ops,
    m: &'a Manager,
    seed: i32,
}

impl Env for FlowEnv<'_> {
    fn file_path(&mut self, name: &[u8], want_count: bool) -> (Vec<u32>, i32) {
        self.ops.file_path_seeded(name, want_count, self.seed)
    }

    fn get_resource(&mut self, path: &[u32]) -> *mut c_void {
        self.m.get(path)
    }
}

pub fn null() -> *mut c_void {
    std::ptr::null_mut()
}

/// One manager call: the manager state plus the host, with the `iResourceManager` protected API.
pub struct Flow<'a> {
    pub m: &'a mut Manager,
    pub ops: &'a Ops,
    /// Initial equal-count a lookup reports (the caller's pointee in `FindLoadedResource`).
    pub seed: i32,
}

impl<'a> Flow<'a> {
    pub fn new(m: &'a mut Manager, ops: &'a Ops) -> Self {
        Self { m, ops, seed: 0 }
    }

    pub fn env(&self) -> impl Env + '_ {
        FlowEnv {
            ops: self.ops,
            m: self.m,
            seed: self.seed,
        }
    }

    pub fn begin(&mut self, name: &[u8]) {
        self.m.time_start = self.ops.now();
        manager::begin_load(name, self.m.time_start);
    }

    pub fn end(&mut self) {
        if let Some(o) = manager::end_load(self.ops.now()) {
            // SAFETY: the host contract of `Ops`.
            unsafe { (self.ops.load_done)(self.ops.ctx, o.name.as_ptr(), o.name.len(), o.elapsed) };
        }
    }

    pub fn tabs(&self) -> Vec<u8> {
        let n = usize::try_from(manager::tab_count()).unwrap_or(0);
        vec![b' '; 2 * n]
    }

    pub fn add(&mut self, res: *mut c_void, log: bool, to_set: bool) {
        let info = self.ops.info(res);
        if to_set {
            // SAFETY: the meta lives in the resource until `remove`.
            unsafe { self.m.add(res, &info.path, info.meta) };
        }
        if log && LOG_CREATE_AND_DELETE.load(Ordering::Relaxed) {
            let elapsed = self.ops.now().wrapping_sub(self.m.time_start);
            // `%d` of an unsigned long prints the low 32 bits as an int.
            let shown = elapsed as u32 as i32;
            let tabs = self.tabs();
            let msg = [
                tabs.as_slice(),
                b"Loaded resource ",
                &info.name,
                b" in ",
                shown.to_string().as_bytes(),
                b" ms\n",
            ]
            .concat();
            self.ops.log(LOG, &msg);
            // SAFETY: `info.meta` is the live resource's.
            if let Some(m) = unsafe { info.meta.as_mut() } {
                m.log_destruction = true;
            }
        }
    }

    pub fn inc_user(&mut self, res: *mut c_void) {
        let meta = self.ops.info(res).meta;
        // SAFETY: the meta lives in the live resource.
        if let Some(m) = unsafe { meta.as_mut() } {
            m.inc_user(unix_time());
        }
    }

    /// `ohpl_rm_destroy_standard` on a live resource.
    pub fn dec_user_is_free(&mut self, res: *mut c_void) -> bool {
        let meta = self.ops.info(res).meta;
        // SAFETY: the meta lives in the live resource.
        unsafe { meta.as_mut() }.is_some_and(resmgr::destroy_standard)
    }

    pub fn users(&self, res: *mut c_void) -> u32 {
        // SAFETY: the meta lives in the live resource.
        unsafe { self.ops.info(res).meta.as_ref() }.map_or(0, |m| m.user_count)
    }

    pub fn find_loaded(&mut self, name: &[u8], want_count: bool) -> resmgr::Lookup {
        resmgr::find_loaded(&mut self.env(), name, want_count)
    }

    pub fn destroy_all(&mut self) {
        loop {
            let r = self.m.first();
            if r.is_null() {
                break;
            }
            // SAFETY: the meta lives in the live resource.
            if let Some(m) = unsafe { self.ops.info(r).meta.as_mut() } {
                m.user_count = 0;
            }
            self.ops.call(c::DESTROY_VIRTUAL, r, 0);
        }
    }

    pub fn destroy_unused(&mut self, keep: i64) {
        for r in self.m.eviction_tail(keep) {
            if self.users(r) == 0 {
                self.m.remove(r);
                self.ops.destroy(r);
            }
        }
    }

    /// `if(destroy_standard) { RemoveResource; hplDelete }`.
    pub fn destroy_standard(&mut self, res: *mut c_void) {
        if self.dec_user_is_free(res) {
            self.m.remove(res);
            self.ops.destroy(res);
        }
    }
}
