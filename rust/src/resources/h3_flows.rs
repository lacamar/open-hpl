//! Per-manager load/destroy flows (`cAnimationManager` ... `cVideoManager`), driven through [`Ops`].

use super::h3_ops::{Create, Flow, Str, WStr, c, k, null, wide8};
use crate::resmgr::{self, AnimErr, AnimPlan, CubePre, FontKind, Model, Simple};
use std::ffi::c_void;

pub const SHUTDOWN_LINES: [&[u8]; 13] = [
    b" Done with animations\n",
    b" Done with ent files\n",
    b" Done with meshes\n",
    b" Done with scripts\n",
    b" Done with sound entities\n",
    b" Done with sounds\n",
    b" Done with fonts\n",
    b" Destroyed all textures\n",
    b" Done with images\n",
    b" Done with materials\n",
    b" Done with Gpu programs\n",
    b" Done with particles\n",
    b"",
];

pub mod shutdown {
    pub const ANIMATION: i32 = 0;
    pub const ENT_FILE: i32 = 1;
    pub const MESH: i32 = 2;
    pub const SCRIPT: i32 = 3;
    pub const SOUND_ENTITY: i32 = 4;
    pub const SOUND: i32 = 5;
    pub const FONT: i32 = 6;
    pub const TEXTURE: i32 = 7;
    pub const IMAGE: i32 = 8;
    pub const MATERIAL: i32 = 9;
    pub const GPU_SHADER: i32 = 10;
    pub const PARTICLE: i32 = 11;
}

fn ptr_text(p: *mut c_void) -> Vec<u8> {
    if p.is_null() {
        b"(nil)".to_vec()
    } else {
        format!("{p:p}").into_bytes()
    }
}

impl Flow<'_> {
    fn finish(&mut self, obj: *mut c_void, missing: &[&[u8]]) -> *mut c_void {
        if obj.is_null() {
            self.ops.err(missing);
        } else {
            self.inc_user(obj);
        }
        self.end();
        obj
    }

    /// `~cXManager`: destroy everything, then the closing log line.
    pub fn shutdown(&mut self, which: i32) {
        if which == shutdown::PARTICLE {
            for r in self.m.resources() {
                // SAFETY: the meta lives in the live resource.
                if let Some(m) = unsafe { self.ops.info(r).meta.as_mut() } {
                    m.user_count = 0;
                }
            }
            self.destroy_unused(0);
        } else if which == shutdown::SOUND {
            self.sound_destroy_all();
        } else if usize::try_from(which).is_ok_and(|w| w < SHUTDOWN_LINES.len() - 1) {
            self.destroy_all();
        }
        if let Some(line) = usize::try_from(which)
            .ok()
            .and_then(|w| SHUTDOWN_LINES.get(w))
            .filter(|l| !l.is_empty())
        {
            self.ops.log(super::h3_ops::LOG, line);
        }
    }

    pub fn model(&mut self, mesh: bool, name: &[u8], exts: &[&[u8]], flag: i32) -> *mut c_void {
        self.begin(name);
        let Model::Found(new_name, lk) = resmgr::resolve_model(&mut self.env(), name, exts) else {
            let what: &[u8] = if mesh { b"mesh" } else { b"animation" };
            self.ops.err(&[
                b"Couldn't find ",
                what,
                b" file '",
                name,
                b"' in any supported format!\n",
            ]);
            self.end();
            return null();
        };
        let mut obj = lk.res;
        if obj.is_null() && !lk.path.is_empty() {
            let mut cr = Create::new(if mesh { k::MESH } else { k::ANIMATION });
            cr.path = WStr::of(&lk.path);
            cr.i[0] = flag;
            obj = self.ops.create(&mut cr);
            if mesh && obj.is_null() {
                self.end();
                return null();
            }
            if !obj.is_null() {
                self.add(obj, true, true);
            }
        }
        let what: &[u8] = if mesh {
            b"Couldn't load mesh '"
        } else {
            b"Couldn't create animation '"
        };
        self.finish(obj, &[what, &new_name, b"'\n"])
    }

    fn resolve_begin(&mut self, kind: Simple, name: &[u8]) -> Option<(Vec<u8>, resmgr::Lookup)> {
        let r = resmgr::resolve_simple(&mut self.env(), kind, name);
        if r.is_some() {
            self.begin(name);
        }
        r
    }

    pub fn ent_file(&mut self, name: &[u8]) -> *mut c_void {
        let Some((nn, lk)) = self.resolve_begin(Simple::EntFile, name) else {
            return null();
        };
        let mut obj = lk.res;
        if obj.is_null() && !lk.path.is_empty() {
            let mut cr = Create::new(k::ENT_FILE);
            cr.name = Str::of(&nn);
            cr.path = WStr::of(&lk.path);
            obj = self.ops.create(&mut cr);
            if obj.is_null() {
                self.ops
                    .err(&[b"Couldn't load entity file '", &wide8(&lk.path), b"'!\n"]);
                self.end();
                return null();
            }
            self.add(obj, true, true);
        }
        self.finish(obj, &[b"Couldn't create ent file '", &nn, b"'\n"])
    }

    pub fn script(&mut self, name: &[u8], compile_msgs: *mut c_void) -> *mut c_void {
        let Some((nn, lk)) = self.resolve_begin(Simple::Script, name) else {
            return null();
        };
        let mut obj = lk.res;
        if obj.is_null() && !lk.path.is_empty() {
            let mut cr = Create::new(k::SCRIPT);
            cr.name = Str::of(&nn);
            cr.path = WStr::of(&lk.path);
            cr.ptr = compile_msgs;
            obj = self.ops.create(&mut cr);
            if obj.is_null() {
                self.end();
                return null();
            }
            self.add(obj, true, true);
        }
        self.finish(obj, &[b"Couldn't create script '", &nn, b"'\n"])
    }

    pub fn font(&mut self, name: &[u8], size: i32, first: i32, last: i32) -> *mut c_void {
        let Some((nn, lk)) = self.resolve_begin(Simple::Font, name) else {
            return null();
        };
        let mut obj = lk.res;
        if obj.is_null() && !lk.path.is_empty() {
            let kind = match resmgr::font_kind(name) {
                FontKind::TrueType => k::FONT_TTF,
                FontKind::Bitmap => k::FONT_BITMAP,
                FontKind::Unknown => k::FONT_UNKNOWN,
            };
            let mut cr = Create::new(kind);
            cr.name = Str::of(&nn);
            cr.path = WStr::of(&lk.path);
            cr.i = [size, first, last, 0];
            obj = self.ops.create(&mut cr);
            if kind == k::FONT_UNKNOWN {
                self.ops
                    .err(&[b"Font '", name, b"' has an unkown extension!\n"]);
                if !obj.is_null() {
                    self.ops.destroy(obj);
                }
                self.end();
                return null();
            }
            if obj.is_null() {
                self.end();
                return null();
            }
            self.add(obj, true, true);
        }
        self.finish(obj, &[b"Couldn't create font '", &nn, b"'\n"])
    }

    pub fn material(&mut self, name: &[u8]) -> *mut c_void {
        let Some((nn, lk)) = self.resolve_begin(Simple::Material, name) else {
            return null();
        };
        let mut obj = lk.res;
        if obj.is_null() && !lk.path.is_empty() {
            let mut cr = Create::new(k::MATERIAL);
            cr.name = Str::of(&nn);
            cr.path = WStr::of(&lk.path);
            obj = self.ops.create(&mut cr);
            if obj.is_null() {
                self.ops.err(&[b"Couldn't load material '", &nn, b"'\n"]);
                self.end();
                return null();
            }
            self.add(obj, true, true);
        }
        self.finish(obj, &[b"Couldn't create material '", &nn, b"'\n"])
    }

    pub fn material_custom(&mut self, obj: *mut c_void) {
        self.inc_user(obj);
        self.add(obj, true, true);
    }

    pub fn material_physics_name(&mut self, name: &[u8]) -> Vec<u8> {
        let Some((_, lk)) = resmgr::resolve_simple(&mut self.env(), Simple::PhysicsMaterial, name)
        else {
            return Vec::new();
        };
        if lk.res.is_null() {
            if lk.path.is_empty() {
                Vec::new()
            } else {
                self.ops.text(c::PHYSICS_NAME_OF, null(), &lk.path)
            }
        } else {
            self.ops.text(c::PHYSICS_NAME_OF, lk.res, &[])
        }
    }

    pub fn sound_entity(&mut self, name: &[u8], has_resolver: bool) -> *mut c_void {
        if let Some(stored) = self.m.custom.get(name) {
            if !stored.is_null() {
                self.inc_user(stored);
            }
            return stored;
        }
        if resmgr::sound_entity_needs_resolver(&mut self.env(), name, has_resolver) {
            let mut cr = Create::new(k::CUSTOM_RESOLVER);
            cr.name = Str::of(name);
            let custom = self.ops.create(&mut cr);
            self.m.custom.set(name, custom);
            if !custom.is_null() {
                self.inc_user(custom);
                self.inc_user(custom);
            }
            return custom;
        }
        let Some((nn, lk)) = self.resolve_begin(Simple::SoundEntity, name) else {
            return null();
        };
        let mut obj = lk.res;
        if obj.is_null() && !lk.path.is_empty() {
            let mut cr = Create::new(k::SOUND_ENTITY);
            cr.name = Str::of(&nn);
            cr.path = WStr::of(&lk.path);
            obj = self.ops.create(&mut cr);
            if !obj.is_null() {
                self.add(obj, true, true);
            }
        }
        self.finish(obj, &[b"Couldn't create SoundEntity '", &nn, b"'\n"])
    }

    pub fn sound_entity_add_custom(&mut self, name: &[u8], data: *mut c_void) {
        self.inc_user(data);
        self.m.custom.set(name, data);
    }

    pub fn sound_entity_preload(&mut self, name: &[u8], has_resolver: bool) {
        let obj = self.sound_entity(name, has_resolver);
        if obj.is_null() {
            self.ops.log(
                super::h3_ops::WARNING,
                &[b"Couldn't preload sound entity '".as_slice(), name, b"'\n"].concat(),
            );
        } else {
            self.ops.call(c::PRELOAD_SOUNDS, obj, 0);
        }
    }

    pub fn video(&mut self, name: &[u8]) -> *mut c_void {
        self.begin(name);
        let path = self.ops.file_path(name, false).0;
        if path.is_empty() {
            self.end();
            self.ops
                .err(&[b"Video file '", name, b"' could not be found!\n"]);
            return null();
        }
        let mut cr = Create::new(k::VIDEO);
        cr.path = WStr::of(&path);
        let obj = self.ops.create(&mut cr);
        if obj.is_null() {
            self.end();
            self.ops.err(&[b"Could not load video '", name, b"'\n"]);
            return null();
        }
        self.add(obj, true, true);
        self.end();
        obj
    }

    pub fn for_each(&mut self, what: i32, arg: i64) {
        for r in self.m.resources() {
            self.ops.call(what, r, arg);
        }
    }

    pub fn video_destroy(&mut self, obj: *mut c_void) {
        if !obj.is_null() {
            self.m.remove(obj);
            self.ops.destroy(obj);
        }
    }

    pub fn video_update(&mut self, dt_bits: i64) {
        for r in self.m.resources() {
            self.ops.call(c::UPDATE, r, dt_bits);
        }
    }

    pub fn sound(
        &mut self,
        name: &[u8],
        stream: bool,
        looped: bool,
        formats: &[&[u8]],
    ) -> *mut c_void {
        self.begin(name);
        let mut obj = null();
        let mut cr = Create::new(k::SOUND_DATA);
        cr.name = Str::of(name);
        cr.i = [i32::from(stream), i32::from(looped), 0, 0];
        if stream {
            let path = resmgr::find_stream_path(&mut self.env(), name, formats);
            if !path.is_empty() {
                cr.path = WStr::of(&path);
                obj = self.ops.create(&mut cr);
                if !obj.is_null() {
                    self.add(obj, true, false);
                    self.m.streams.push(obj.expose_provenance());
                    self.ops.call(c::SOUND_SET_MGR, obj, 0);
                }
            }
        } else {
            let lk = resmgr::find_sample(&mut self.env(), name, formats);
            obj = lk.res;
            if obj.is_null() && !lk.path.is_empty() {
                cr.path = WStr::of(&lk.path);
                obj = self.ops.create(&mut cr);
                if !obj.is_null() {
                    self.add(obj, true, true);
                    self.ops.call(c::SOUND_SET_MGR, obj, 0);
                }
            }
        }
        self.end();
        obj
    }

    pub fn sound_destroy(&mut self, obj: *mut c_void) {
        let is_stream = self.ops.call(c::IS_STREAM, obj, 0) != 0;
        let meta = self.ops.info(obj).meta;
        // SAFETY: the meta lives in the live resource.
        let gone = unsafe { meta.as_mut() }.is_some_and(|m| resmgr::destroy_sound(m, is_stream));
        if gone {
            let key = obj.expose_provenance();
            if let Some(pos) = self.m.streams.iter().position(|&s| s == key) {
                self.m.streams.remove(pos);
                self.ops.destroy(obj);
            }
        }
    }

    pub fn sound_destroy_all(&mut self) {
        for s in std::mem::take(&mut self.m.streams) {
            let p: *mut c_void = std::ptr::with_exposed_provenance_mut(s);
            let info = self.ops.info(p);
            self.ops.log(
                super::h3_ops::LOG,
                &[b"  destroying ".as_slice(), &ptr_text(p), b"\n"].concat(),
            );
            self.ops.log(
                super::h3_ops::LOG,
                &[b"   file '".as_slice(), &wide8(&info.path), b"'\n"].concat(),
            );
            self.ops.destroy(p);
        }
        loop {
            let r = self.m.first();
            if r.is_null() {
                break;
            }
            self.m.remove(r);
            self.ops.destroy(r);
        }
    }

    fn particle_data_file(&mut self, file: &[u8], path: &[u32]) -> *mut c_void {
        let mut cr = Create::new(k::PS_DATA_FILE);
        cr.name = Str::of(file);
        cr.path = WStr::of(path);
        let pd = self.ops.create(&mut cr);
        if pd.is_null() {
            self.ops.err(&[
                b"Can't load data from particle system file '",
                &wide8(path),
                b"'\n",
            ]);
        }
        pd
    }

    fn particle_make(
        &mut self,
        data: *mut c_void,
        name: &[u8],
        data_name: &[u8],
        size: [f32; 3],
    ) -> *mut c_void {
        self.inc_user(data);
        let mut cr = Create::new(k::PS_CREATE);
        cr.name = Str::of(name);
        cr.name2 = Str::of(data_name);
        cr.f = [size[0], size[1], size[2], 0.0];
        cr.ptr = data;
        let ps = self.ops.create(&mut cr);
        self.end();
        ps
    }

    pub fn particle_by_type(&mut self, name: &[u8], ty: &[u8], size: [f32; 3]) -> *mut c_void {
        let (file, path, res) = resmgr::particle_by_file(&mut self.env(), ty);
        self.begin(name);
        let mut data = res;
        if data.is_null() {
            if path.is_empty() {
                self.ops
                    .err(&[b"Couldn't find particle system file '", &file, b"'\n"]);
                self.end();
                return null();
            }
            data = self.particle_data_file(&file, &path);
            if data.is_null() {
                self.end();
                return null();
            }
            self.add(data, true, true);
        }
        self.particle_make(data, name, ty, size)
    }

    pub fn particle_by_element(
        &mut self,
        name: &[u8],
        data_name: &[u8],
        element: *mut c_void,
        size: [f32; 3],
    ) -> *mut c_void {
        self.begin(name);
        let mut data = resmgr::particle_by_data_name(&mut self.env(), data_name);
        if data.is_null() {
            let mut cr = Create::new(k::PS_DATA_ELEMENT);
            cr.name = Str::of(data_name);
            cr.ptr = element;
            data = self.ops.create(&mut cr);
            if data.is_null() {
                self.ops
                    .err(&[b"Can't load particle system data '", data_name, b"'\n"]);
                self.end();
                return null();
            }
            self.add(data, true, true);
        }
        self.particle_make(data, name, data_name, size)
    }

    pub fn particle_preload(&mut self, name: &[u8]) {
        let (file, path, res) = resmgr::particle_by_file(&mut self.env(), name);
        if !res.is_null() {
            return;
        }
        if path.is_empty() {
            self.ops
                .err(&[b"Couldn't find particle system file '", &file, b"'\n"]);
            return;
        }
        let pd = self.particle_data_file(&file, &path);
        if !pd.is_null() {
            self.add(pd, true, true);
        }
    }

    pub fn particle_destroy(&mut self, obj: *mut c_void) {
        // SAFETY: the meta lives in the live resource.
        if let Some(m) = unsafe { self.ops.info(obj).meta.as_mut() } {
            resmgr::destroy_particle(m);
        }
    }

    pub fn gpu_destroy(&mut self, obj: *mut c_void) {
        // SAFETY: the meta lives in the live resource.
        if let Some(m) = unsafe { self.ops.info(obj).meta.as_mut() } {
            m.dec_user();
        }
        if self.users(obj) == 0 {
            self.m.remove(obj);
            self.ops.destroy(obj);
        }
    }

    pub fn texture_destroy(&mut self, obj: *mut c_void) {
        if self.dec_user_is_free(obj) {
            let size = self.ops.call(c::MEM_SIZE, obj, 0);
            self.m.mem_usage = self.m.mem_usage.wrapping_sub(size as i32);
            self.m.anim.remove(&obj.expose_provenance());
            self.m.remove(obj);
            self.ops.destroy(obj);
        }
    }

    pub fn texture_update(&mut self, dt_bits: i64) {
        let all: Vec<usize> = self.m.anim.iter().copied().collect();
        for p in all {
            self.ops
                .call(c::UPDATE, std::ptr::with_exposed_provenance_mut(p), dt_bits);
        }
    }

    /// Logs a face/frame bitmap that failed to load; true when that was the failure.
    fn bitmap_failed(&self, cr: &Create, paths: &[Vec<u32>]) -> bool {
        if cr.status != 1 {
            return false;
        }
        let shown = paths
            .get(usize::try_from(cr.idx).unwrap_or(0))
            .map_or(Vec::new(), |p| wide8(p));
        self.ops.err(&[b"Couldn't load bitmap '", &shown, b"'!\n"]);
        true
    }

    pub fn texture_simple(
        &mut self,
        name: &[u8],
        mip: bool,
        usage: i32,
        ty: i32,
        level: i32,
        exts: &[&[u8]],
    ) -> *mut c_void {
        self.begin(name);
        let lk = resmgr::find_with_formats(&mut self.env(), name, exts);
        let mut tex = lk.res;
        if tex.is_null() && !lk.path.is_empty() {
            let mut cr = Create::new(k::TEX_SIMPLE);
            cr.name = Str::of(name);
            cr.path = WStr::of(&lk.path);
            cr.i = [i32::from(mip), usage, ty, level];
            tex = self.ops.create(&mut cr);
            if tex.is_null() {
                if cr.status == 1 {
                    self.ops.err(&[
                        b"Texture manager Couldn't load bitmap '",
                        &wide8(&lk.path),
                        b"'\n",
                    ]);
                }
                self.end();
                return null();
            }
            let size = self.ops.call(c::MEM_SIZE, tex, 0);
            self.m.mem_usage = self.m.mem_usage.wrapping_add(size as i32);
            self.add(tex, true, true);
        }
        self.finish(tex, &[b"Couldn't texture '", name, b"'\n"])
    }

    pub fn texture_anim(
        &mut self,
        name: &[u8],
        mip: bool,
        ty: i32,
        usage: i32,
        level: i32,
        exts: &[&[u8]],
    ) -> *mut c_void {
        let first = resmgr::anim_first_frame(&mut self.env(), name, exts);
        self.begin(&first);
        let plan = resmgr::anim_plan(&mut self.env(), &first);
        let tex;
        match plan {
            Err(AnimErr::No01) => {
                self.ops.err(&[
                    b"First frame of animation '",
                    &first,
                    b"' must contain '01'!\n",
                ]);
                return null();
            }
            Err(AnimErr::No01BeforeExt) => {
                self.ops.err(&[
                    b"First frame of animation '",
                    &first,
                    b"' must contain '01' before extension!\n",
                ]);
                return null();
            }
            Err(AnimErr::FirstFrameMissing) => {
                self.ops.err(&[
                    b"First frame of animation '",
                    &first,
                    b"' could not be found!\n",
                ]);
                return null();
            }
            Err(AnimErr::NoFrames(base)) => {
                self.ops
                    .err(&[b"No textures found for animation ", &base, b"\n"]);
                self.end();
                return null();
            }
            Ok(AnimPlan::Loaded(r)) => tex = r,
            Ok(AnimPlan::Frames {
                base_name,
                fake_path,
                paths,
            }) => {
                let ws: Vec<WStr> = paths.iter().map(|p| WStr::of(p)).collect();
                let mut cr = Create::new(k::TEX_ANIM);
                cr.name = Str::of(&base_name);
                cr.path = WStr::of(&fake_path);
                cr.paths = ws.as_ptr();
                cr.npaths = ws.len();
                cr.i = [i32::from(mip), usage, ty, level];
                let made = self.ops.create(&mut cr);
                if made.is_null() {
                    if !self.bitmap_failed(&cr, &paths) {
                        self.ops
                            .err(&[b"Couldn't create animated texture '", &base_name, b"'!\n"]);
                    }
                    self.end();
                    return null();
                }
                tex = made;
                let size = self.ops.call(c::MEM_SIZE, tex, 0);
                self.m.mem_usage = self.m.mem_usage.wrapping_add(size as i32);
                self.add(tex, true, true);
                self.m.anim.insert(tex.expose_provenance());
            }
        }
        self.finish(tex, &[b"Couldn't texture '", &first, b"'\n"])
    }

    pub fn texture_cube(
        &mut self,
        path_name: &[u8],
        mip: bool,
        usage: i32,
        level: i32,
        cube_type: i32,
        exts: &[&[u8]],
    ) -> *mut c_void {
        let CubePre::Faces {
            name,
            fake_path,
            res,
        } = resmgr::cube_pre(&mut self.env(), path_name)
        else {
            return self.texture_simple(path_name, mip, usage, cube_type, level, exts);
        };
        self.begin(path_name);
        let mut tex = res;
        if tex.is_null() {
            let paths = match resmgr::cube_faces(&mut self.env(), &name, exts) {
                Ok(p) => p,
                Err(i) => {
                    let suffix = resmgr::CUBE_SUFFIXES.get(i).copied().unwrap_or_default();
                    let new_name = [name.as_slice(), suffix].concat();
                    self.ops.err(&[
                        b"Couldn't find ",
                        i.to_string().as_bytes(),
                        b"-face '",
                        &new_name,
                        b"', for cubemap '",
                        &name,
                        b"' in path: '",
                        path_name,
                        b"'\n",
                    ]);
                    return null();
                }
            };
            let ws: Vec<WStr> = paths.iter().map(|p| WStr::of(p)).collect();
            let mut cr = Create::new(k::TEX_CUBE);
            cr.name = Str::of(&name);
            cr.path = WStr::of(&fake_path);
            cr.paths = ws.as_ptr();
            cr.npaths = ws.len();
            cr.i = [i32::from(mip), usage, cube_type, level];
            tex = self.ops.create(&mut cr);
            if tex.is_null() {
                if !self.bitmap_failed(&cr, &paths) {
                    self.ops
                        .err(&[b"Couldn't create cubemap '", &name, b"'!\n"]);
                }
                self.end();
                return null();
            }
            let size = self.ops.call(c::MEM_SIZE, tex, 0);
            self.m.mem_usage = self.m.mem_usage.wrapping_add(size as i32);
            self.add(tex, true, true);
        }
        self.finish(tex, &[b"Couldn't texture '", &name, b"'\n"])
    }
}
