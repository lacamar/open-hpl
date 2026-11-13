//! `cResources` (H2 pass 2): module init/shutdown order and logging, manager update list,
//! entity/area loader tables, resource-dir files, XML document and binary buffer ownership.

// Object pointers are opaque host handles handed back to the host.
#![allow(clippy::not_unsafe_ptr_arg_deref)]

use super::h3_ops::{ERROR, LOG, LOG_CREATE_AND_DELETE, Str, WARNING, WStr};
use crate::resmgr::{self, LoaderMap};
use crate::string;
use crate::string_ffi::units;
use std::ffi::c_void;
use std::sync::atomic::Ordering;

/// Create/destroy kinds (`ResOps::create`, `ResOps::destroy`).
pub mod w {
    pub const MESH_HANDLER: i32 = 0;
    pub const BITMAP_HANDLER: i32 = 1;
    pub const WORLD_HANDLER: i32 = 2;
    pub const VIDEO_HANDLER: i32 = 3;
    pub const IMAGE: i32 = 4;
    pub const GPU_SHADER: i32 = 5;
    pub const PARTICLE: i32 = 6;
    pub const SOUND: i32 = 7;
    pub const FONT: i32 = 8;
    pub const SCRIPT: i32 = 9;
    pub const TEXTURE: i32 = 10;
    pub const MATERIAL: i32 = 11;
    pub const MESH: i32 = 12;
    pub const SOUND_ENTITY: i32 = 13;
    pub const ANIMATION: i32 = 14;
    pub const VIDEO: i32 = 15;
    pub const ENT_FILE: i32 = 16;
    pub const FILE_SEARCHER: i32 = 17;
    pub const LANGUAGE: i32 = 18;
    pub const XML_DOC: i32 = 19;
    pub const BIN_BUFFER: i32 = 20;
    pub const ENTITY_LOADER: i32 = 21;
    pub const AREA_LOADER: i32 = 22;
    pub const LOADERS_ADD: i32 = 23;
    pub const LOW_LEVEL_SYSTEM: i32 = 24;
    pub const UPDATE_MANAGER: i32 = 25;
    pub const HAS_LANGUAGE: i32 = 26;
}

#[repr(C)]
pub struct ResOps {
    pub ctx: *mut c_void,
    pub log: unsafe extern "C" fn(*mut c_void, i32, *const u8, usize),
    pub create: unsafe extern "C" fn(*mut c_void, i32, WStr) -> *mut c_void,
    pub destroy: unsafe extern "C" fn(*mut c_void, i32, *mut c_void),
    pub call: unsafe extern "C" fn(*mut c_void, i32, *mut c_void, i64) -> i64,
    pub file_path:
        unsafe extern "C" fn(*mut c_void, *const u8, usize, *mut i32, *mut usize) -> *const u32,
    pub add_dir: unsafe extern "C" fn(*mut c_void, WStr, Str, bool),
    pub clear_dirs: unsafe extern "C" fn(*mut c_void),
    /// Parses a resource-dirs file, calling `each(ctx, path, add_sub_dirs)` per entry; false when it does not load.
    pub read_dirs: unsafe extern "C" fn(*mut c_void, Str, EachDir, *mut c_void) -> bool,
}

pub type EachDir = unsafe extern "C" fn(*mut c_void, Str, bool);

pub struct Res {
    ops: ResOps,
    managers: Vec<usize>,
    entity: LoaderMap,
    area: LoaderMap,
    docs: Vec<usize>,
    bufs: Vec<usize>,
}

fn ptr(p: usize) -> *mut c_void {
    std::ptr::with_exposed_provenance_mut(p)
}

const RULE: &[u8] = b"--------------------------------------------------------\n";

impl Res {
    pub fn new(ops: ResOps) -> Self {
        Self {
            ops,
            managers: Vec::new(),
            entity: LoaderMap::default(),
            area: LoaderMap::default(),
            docs: Vec::new(),
            bufs: Vec::new(),
        }
    }

    fn log(&self, level: i32, msg: &[u8]) {
        // SAFETY: the host contract of `ResOps`.
        unsafe { (self.ops.log)(self.ops.ctx, level, msg.as_ptr(), msg.len()) }
    }

    fn create(&self, what: i32, arg: &[u32]) -> *mut c_void {
        // SAFETY: the host contract of `ResOps`.
        unsafe { (self.ops.create)(self.ops.ctx, what, WStr::of(arg)) }
    }

    fn destroy(&self, what: i32, obj: *mut c_void) {
        // SAFETY: the host contract of `ResOps`.
        unsafe { (self.ops.destroy)(self.ops.ctx, what, obj) }
    }

    fn call(&self, what: i32, obj: *mut c_void, i: i64) -> i64 {
        // SAFETY: the host contract of `ResOps`.
        unsafe { (self.ops.call)(self.ops.ctx, what, obj, i) }
    }

    fn file_path(&self, name: &[u8]) -> Vec<u32> {
        let mut len = 0;
        // SAFETY: the host returns a path valid for `len` units until its next call.
        unsafe {
            let p = (self.ops.file_path)(
                self.ops.ctx,
                name.as_ptr(),
                name.len(),
                std::ptr::null_mut(),
                &raw mut len,
            );
            units(p, len).to_vec()
        }
    }

    /// `cResources::Init`; the host has stored the graphics/system/... arguments.
    pub fn init(&mut self) {
        self.log(LOG, b"Initializing Resources Module\n");
        self.log(LOG, RULE);
        self.call(w::LOW_LEVEL_SYSTEM, std::ptr::null_mut(), 0);
        self.log(LOG, b" Creating loader handlers \n");
        for what in w::MESH_HANDLER..=w::VIDEO_HANDLER {
            self.create(what, &[]);
        }
        self.log(LOG, b" Creating resource managers\n");
        let mut particle = 0;
        for what in w::IMAGE..=w::ENT_FILE {
            let m = self.create(what, &[]).expose_provenance();
            if what == w::PARTICLE {
                particle = m;
            }
            // The sound slot pushes the particle manager again.
            self.managers
                .push(if what == w::SOUND { particle } else { m });
        }
        self.log(LOG, b" Adding loaders to handlers \n");
        self.call(w::LOADERS_ADD, std::ptr::null_mut(), 0);
        self.log(
            LOG,
            b"--------------------------------------------------------\n\n",
        );
    }

    pub fn update(&self, dt_bits: i64) {
        for &m in &self.managers {
            self.call(w::UPDATE_MANAGER, ptr(m), dt_bits);
        }
    }

    /// `cResources::~cResources`.
    pub fn shutdown(&mut self) {
        self.log(LOG, b"Exiting Resources Module\n");
        self.log(LOG, RULE);
        for p in self.entity.values() {
            self.destroy(w::ENTITY_LOADER, p);
        }
        for p in self.area.values() {
            self.destroy(w::AREA_LOADER, p);
        }
        self.entity = LoaderMap::default();
        self.area = LoaderMap::default();
        for d in std::mem::take(&mut self.docs) {
            self.destroy(w::XML_DOC, ptr(d));
        }
        for b in std::mem::take(&mut self.bufs) {
            self.destroy(w::BIN_BUFFER, ptr(b));
        }
        for what in [
            w::FONT,
            w::SCRIPT,
            w::PARTICLE,
            w::SOUND,
            w::MESH,
            w::MATERIAL,
            w::GPU_SHADER,
            w::IMAGE,
            w::TEXTURE,
            w::SOUND_ENTITY,
            w::ANIMATION,
            w::VIDEO,
            w::ENT_FILE,
        ] {
            self.destroy(what, std::ptr::null_mut());
        }
        self.log(LOG, b" All resources deleted\n");
        self.destroy(w::FILE_SEARCHER, std::ptr::null_mut());
        for what in w::MESH_HANDLER..=w::VIDEO_HANDLER {
            self.destroy(what, std::ptr::null_mut());
        }
        if self.call(w::HAS_LANGUAGE, std::ptr::null_mut(), 0) != 0 {
            self.destroy(w::LANGUAGE, std::ptr::null_mut());
        }
        self.managers.clear();
        self.log(
            LOG,
            b"--------------------------------------------------------\n\n",
        );
    }

    pub fn add_resource_dir(&self, dir: &[u32], sub: bool, mask: &[u8]) -> bool {
        // SAFETY: the host contract of `ResOps`.
        unsafe { (self.ops.add_dir)(self.ops.ctx, WStr::of(dir), Str::of(mask), sub) };
        if LOG_CREATE_AND_DELETE.load(Ordering::Relaxed) {
            let msg = [
                b" Added resource directory '".as_slice(),
                &string::wide_to_utf8(dir),
                b"'\n",
            ]
            .concat();
            self.log(LOG, &msg);
        }
        true
    }

    pub fn clear_dirs(&self) {
        // SAFETY: the host contract of `ResOps`.
        unsafe { (self.ops.clear_dirs)(self.ops.ctx) }
    }

    pub fn add_entity_loader(&mut self, name: &[u8], loader: *mut c_void, default: bool) {
        self.entity.add(name, loader, default);
    }

    pub fn add_area_loader(&mut self, name: &[u8], loader: *mut c_void, default: bool) {
        self.area.add(name, loader, default);
    }

    pub fn entity_loader(&self, name: &[u8]) -> *mut c_void {
        self.loader(&self.entity, name, b"No loader for type '")
    }

    pub fn area_loader(&self, name: &[u8]) -> *mut c_void {
        self.loader(&self.area, name, b"No loader for area type '")
    }

    fn loader(&self, map: &LoaderMap, name: &[u8], what: &[u8]) -> *mut c_void {
        let (l, missing) = map.get(name);
        if missing {
            self.log(WARNING, &[what, name, b"' found!\n"].concat());
            if !l.is_null() {
                self.log(LOG, b"Using default loader!\n");
            }
        }
        l
    }

    /// `cResources::LoadResourceDirsFile`.
    pub fn load_resource_dirs_file(&self, file: &[u8], alt: &[u32]) -> bool {
        struct Ctx<'a> {
            res: &'a Res,
            alt: &'a [u32],
        }
        unsafe extern "C" fn each(c: *mut c_void, path: Str, sub: bool) {
            // SAFETY: `c` is the `Ctx` below; `path` valid for its length.
            let c = unsafe { &*c.cast::<Ctx>() };
            for d in resmgr::resource_dirs(unsafe { units(path.p, path.n) }, c.alt) {
                c.res.add_resource_dir(&d, sub, b"*.*");
            }
        }
        let mut ctx = Ctx { res: self, alt };
        // SAFETY: `each` reads `ctx`, alive for the call.
        let ok = unsafe {
            (self.ops.read_dirs)(self.ops.ctx, Str::of(file), each, (&raw mut ctx).cast())
        };
        if !ok {
            self.log(
                ERROR,
                &[b"Couldn't load XML file '".as_slice(), file, b"'!\n"].concat(),
            );
        }
        ok
    }

    fn load_owned(
        &mut self,
        what: i32,
        name: &[u8],
        no_path: (&[u8], &[u8]),
        no_load: (&[u8], &[u8]),
    ) -> *mut c_void {
        let path = self.file_path(name);
        if path.is_empty() {
            self.log(ERROR, &[no_path.0, name, no_path.1].concat());
            return std::ptr::null_mut();
        }
        let obj = self.create(what, &path);
        if obj.is_null() {
            self.log(
                ERROR,
                &[no_load.0, &string::wide_to_utf8(&path), no_load.1].concat(),
            );
            return obj;
        }
        if what == w::XML_DOC {
            self.docs.push(obj.expose_provenance());
        } else {
            self.bufs.push(obj.expose_provenance());
        }
        obj
    }

    pub fn load_xml_document(&mut self, name: &[u8]) -> *mut c_void {
        self.load_owned(
            w::XML_DOC,
            name,
            (b"Could not load XML document '", b"'\n"),
            (b"Could not parse/load XML from ", b"\n"),
        )
    }

    pub fn load_binary_buffer(&mut self, name: &[u8]) -> *mut c_void {
        self.load_owned(
            w::BIN_BUFFER,
            name,
            (b"Could not find filepath for binary buffer '", b"'\n"),
            (b"Could not load binary file from ", b"\n"),
        )
    }

    pub fn destroy_xml_document(&mut self, doc: *mut c_void) {
        if let Some(pos) = self.docs.iter().position(|&d| d == doc.expose_provenance()) {
            self.docs.remove(pos);
            self.destroy(w::XML_DOC, doc);
        }
    }

    pub fn destroy_binary_buffer(&mut self, buf: *mut c_void) {
        if let Some(pos) = self.bufs.iter().position(|&d| d == buf.expose_provenance()) {
            self.bufs.remove(pos);
            self.destroy(w::BIN_BUFFER, buf);
        }
    }
}
