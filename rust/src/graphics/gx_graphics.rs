//! `cGraphics`: owner of the GPU objects (textures, frame buffers, depth-stencil buffers, programs),
//! post effects, material types, renderers and the creators. The C++ class is a shell around one
//! `Box<Graphics>`; the objects themselves stay C++ behind `GfxHost`.

#![allow(clippy::not_unsafe_ptr_arg_deref)] // handles are opaque: only the host table dereferences them

use super::gx_gfx_host::{
    self as h, DsInfo, LlInit, OBJ_COMPOSITE, OBJ_DECAL_CREATOR, OBJ_DEPTH_STENCIL,
    OBJ_FRAME_BUFFER, OBJ_GPU_PROGRAM, OBJ_MESH_CREATOR, OBJ_POST_EFFECT, OBJ_POST_EFFECT_TYPE,
    OBJ_RENDERER, OBJ_TEXTURE, OBJ_TEXTURE_CREATOR, host, name,
};
use crate::scene::ls_host;
use crate::types::{Vector2l, Vector3l};
use std::collections::BTreeMap;
use std::ffi::c_void;
use std::ptr::null_mut;
use std::sync::atomic::{AtomicI32, Ordering};

pub const TEXTURE_TYPE_2D: i32 = 1;
pub const TEXTURE_TYPE_RECT: i32 = 2;
const USAGE_RENDER_TARGET: i32 = 1;
const SETUP_SCREEN: u32 = 1;
const RENDERERS: [i32; 3] = [0, 1, 2];
const MATERIAL_TYPES: [(i32, &str); 10] = [
    (0, "soliddiffuse"),
    (3, "translucent"),
    (4, "water"),
    (5, "decal"),
    (6, "terrainblend"),
    (8, "terraindecal"),
    (7, "undergrowth"),
    (1, "projecteduv"),
    (2, "vertexblend"),
    (0, "terrain"),
];
const SHADER_VERTEX: i32 = 0;
const SHADER_FRAGMENT: i32 = 1;

static TEMP_TEXTURE_TYPE: AtomicI32 = AtomicI32::new(TEXTURE_TYPE_RECT);

pub fn temp_texture_type() -> i32 {
    TEMP_TEXTURE_TYPE.load(Ordering::Relaxed)
}

pub fn set_temp_texture_type(t: i32) {
    TEMP_TEXTURE_TYPE.store(t, Ordering::Relaxed);
}

macro_rules! call {
    ($f:ident($($a:expr),*)) => {
        match host() {
            // SAFETY: table contract.
            Some(t) => unsafe { (t.$f)($($a),*) },
            None => Default::default(),
        }
    };
}

macro_rules! callp {
    ($f:ident($($a:expr),*)) => {
        match host() {
            // SAFETY: table contract.
            Some(t) => unsafe { (t.$f)($($a),*) },
            None => null_mut(),
        }
    };
}

fn log(error: bool, msg: &str) {
    if let Some(t) = ls_host::host() {
        // SAFETY: table contract; `msg` is valid for its length.
        unsafe { (t.log)(error, msg.as_ptr(), msg.len()) }
    }
}

fn delete(kind: i32, p: *mut c_void) {
    if !p.is_null() {
        call!(delete_obj(kind, p));
    }
}

fn delete_all(kind: i32, list: &mut Vec<*mut c_void>) {
    for p in list.drain(..) {
        delete(kind, p);
    }
}

fn find_and_delete(kind: i32, list: &mut Vec<*mut c_void>, p: *mut c_void) {
    if let Some(i) = list.iter().position(|&x| x == p) {
        list.remove(i);
        delete(kind, p);
    }
}

struct TempFb {
    fb: *mut c_void,
    size: Vector2l,
    format: i32,
    index: i32,
}

pub struct InitArgs<'a> {
    pub width: i32,
    pub height: i32,
    pub display: i32,
    pub bpp: i32,
    pub fullscreen: i32,
    pub multisampling: i32,
    pub gpu_format: i32,
    pub caption: &'a str,
    pub pos: Vector2l,
    pub flags: u32,
}

pub struct Graphics {
    me: *mut c_void,
    ll: *mut c_void,
    res: *mut c_void,
    mesh_creator: *mut c_void,
    texture_creator: *mut c_void,
    decal_creator: *mut c_void,
    temp_fbs: Vec<TempFb>,
    renderers: Vec<*mut c_void>,
    pe_types: Vec<*mut c_void>,
    frame_buffers: Vec<*mut c_void>,
    depth_stencils: Vec<*mut c_void>,
    textures: Vec<*mut c_void>,
    composites: Vec<*mut c_void>,
    programs: Vec<*mut c_void>,
    post_effects: Vec<*mut c_void>,
    material_types: BTreeMap<String, *mut c_void>,
    screen_setup: bool,
}

impl Graphics {
    pub fn new(me: *mut c_void, ll: *mut c_void) -> Graphics {
        Graphics {
            me,
            ll,
            res: null_mut(),
            mesh_creator: null_mut(),
            texture_creator: null_mut(),
            decal_creator: null_mut(),
            temp_fbs: Vec::new(),
            renderers: Vec::new(),
            pe_types: Vec::new(),
            frame_buffers: Vec::new(),
            depth_stencils: Vec::new(),
            textures: Vec::new(),
            composites: Vec::new(),
            programs: Vec::new(),
            post_effects: Vec::new(),
            material_types: BTreeMap::new(),
            screen_setup: false,
        }
    }

    pub fn init(&mut self, a: &InitArgs, res: *mut c_void) -> bool {
        log(false, "Initializing Graphics Module\n");
        log(
            false,
            "--------------------------------------------------------\n",
        );
        self.res = res;
        for (dir, add) in [
            ("core/shaders", true),
            ("core/textures", false),
            ("core/models", false),
        ] {
            let (p, n) = name(dir);
            call!(res_add_dir(res, p, n, add));
        }
        call!(res_add_compat_dir(res));
        let screen = a.flags & SETUP_SCREEN != 0;
        if screen {
            log(
                false,
                &format!(
                    "Init lowlevel graphics: {}x{} disp:{} bpp:{} fs:{} ms:{} gpufmt:{} cap:'{}' pos:({}x{})\n",
                    a.width,
                    a.height,
                    a.display,
                    a.bpp,
                    a.fullscreen,
                    a.multisampling,
                    a.gpu_format,
                    a.caption,
                    a.pos.x,
                    a.pos.y
                ),
            );
            let init = LlInit {
                width: a.width,
                height: a.height,
                display: a.display,
                bpp: a.bpp,
                fullscreen: a.fullscreen,
                multisampling: a.multisampling,
                gpu_format: a.gpu_format,
                caption: a.caption.as_ptr(),
                caption_len: a.caption.len(),
                pos: a.pos,
            };
            call!(ll_init(self.ll, &init));
        }
        self.screen_setup = screen;
        self.mesh_creator = callp!(creator_new(0, self.ll, res));
        self.texture_creator = callp!(creator_new(1, self.ll, res));
        self.decal_creator = callp!(creator_new(2, self.ll, res));
        if screen {
            call!(res_check_features(res));
            for kind in RENDERERS {
                let r = callp!(renderer_new(kind, self.me, res));
                self.renderers.push(r);
            }
            for (i, &r) in self.renderers.iter().enumerate() {
                if !r.is_null() {
                    let ok: bool = call!(renderer_load(r));
                    if !ok {
                        let msg = format!(
                            "Renderer #{i} could not be initialized! Make sure your graphic card drivers are up to date. Check log file for more information.\n"
                        );
                        call!(fatal(msg.as_ptr(), msg.len()));
                    }
                }
            }
            log(false, " Adding engine materials\n");
            for (code, n) in MATERIAL_TYPES {
                let t = callp!(mat_type_new(code, self.me, res));
                self.add_material_type(t, n);
            }
            log(false, " Adding engine post effects\n");
            let (p, n) = name("posteffect_bloom_blur_vtx.glsl");
            let has_bloom: bool = call!(res_has_file(res, p, n));
            let kinds: &[i32] = if has_bloom { &[0, 1, 2, 3] } else { &[2, 3] };
            for &kind in kinds {
                let t = callp!(pe_type_new(kind, self.me, res));
                self.add_post_effect_type(t);
            }
        }
        log(
            false,
            "--------------------------------------------------------\n\n",
        );
        true
    }

    pub fn update(&mut self, dt: f32) {
        let changed: bool = call!(ll_check_screen(self.ll));
        if changed {
            self.reload_renderer_data();
        }
        for &r in &self.renderers {
            if !r.is_null() {
                call!(renderer_update(r, dt));
            }
        }
    }

    pub fn renderer(&self, kind: i32) -> *mut c_void {
        usize::try_from(kind)
            .ok()
            .and_then(|i| self.renderers.get(i))
            .copied()
            .unwrap_or(null_mut())
    }

    pub fn reload_renderer_data(&mut self) {
        for &r in &self.renderers {
            if !r.is_null() {
                call!(renderer_setup_funcs(r, self.ll));
                call!(renderer_destroy_data(r));
                let _: bool = call!(renderer_load(r));
            }
        }
        call!(res_compile_texture_lookups(self.res));
    }

    pub fn create_frame_buffer(&mut self, n: &str) -> *mut c_void {
        let (p, len) = name(n);
        let fb = callp!(ll_create_fb(self.ll, p, len));
        if fb.is_null() {
            log(true, "Could not create a frame buffer!\n");
            return null_mut();
        }
        self.frame_buffers.push(fb);
        fb
    }

    pub fn destroy_frame_buffer(&mut self, fb: *mut c_void) {
        find_and_delete(OBJ_FRAME_BUFFER, &mut self.frame_buffers, fb);
    }

    pub fn temp_frame_buffer(&mut self, size: Vector2l, format: i32, index: i32) -> *mut c_void {
        if let Some(t) = self
            .temp_fbs
            .iter()
            .find(|t| t.size == size && t.format == format && t.index == index)
        {
            return t.fb;
        }
        let suffix = format!("{}x{}:{}", size.x, size.y, format);
        let tex = self.create_texture(
            &format!("TempBufferTexture{suffix}"),
            temp_texture_type(),
            USAGE_RENDER_TARGET,
        );
        call!(tex_setup(tex, &size, format));
        let fb = self.create_frame_buffer(&format!("TempBuffer{suffix}"));
        let ok: bool = call!(fb_attach(fb, tex));
        if !ok {
            self.destroy_frame_buffer(fb);
            self.destroy_texture(tex);
            return null_mut();
        }
        self.temp_fbs.push(TempFb {
            fb,
            size,
            format,
            index,
        });
        fb
    }

    pub fn create_depth_stencil(
        &mut self,
        size: Vector2l,
        depth: i32,
        stencil: i32,
        look_for_match: bool,
    ) -> *mut c_void {
        let mut buffer = if look_for_match {
            self.find_depth_stencil(size, depth, stencil)
        } else {
            null_mut()
        };
        if buffer.is_null() {
            buffer = callp!(ll_create_ds(self.ll, &size, depth, stencil));
            if buffer.is_null() {
                log(
                    true,
                    &format!(
                        "Could not create a depth stencil buffer size {}x{}, depthbits: {} stencilbits: {}\n",
                        size.x, size.y, depth, stencil
                    ),
                );
                return null_mut();
            }
            self.depth_stencils.push(buffer);
        }
        call!(ds_inc(buffer));
        buffer
    }

    pub fn find_depth_stencil(
        &self,
        size: Vector2l,
        min_depth: i32,
        min_stencil: i32,
    ) -> *mut c_void {
        for &b in &self.depth_stencils {
            let mut info = DsInfo::default();
            call!(ds_info(b, &mut info));
            if info.size == size && info.depth_bits >= min_depth && info.stencil_bits >= min_stencil
            {
                return b;
            }
        }
        null_mut()
    }

    pub fn destroy_depth_stencil(&mut self, b: *mut c_void) {
        let used: bool = call!(ds_dec_has_users(b));
        if !used {
            find_and_delete(OBJ_DEPTH_STENCIL, &mut self.depth_stencils, b);
        }
    }

    pub fn create_texture(&mut self, n: &str, kind: i32, usage: i32) -> *mut c_void {
        let (p, len) = name(n);
        let t = callp!(ll_create_texture(self.ll, p, len, kind, usage));
        self.textures.push(t);
        t
    }

    pub fn destroy_texture(&mut self, t: *mut c_void) {
        find_and_delete(OBJ_TEXTURE, &mut self.textures, t);
    }

    pub fn create_composite(&mut self) -> *mut c_void {
        let c = callp!(composite_new(self.me));
        self.composites.push(c);
        c
    }

    pub fn destroy_composite(&mut self, c: *mut c_void) {
        find_and_delete(OBJ_COMPOSITE, &mut self.composites, c);
    }

    pub fn add_post_effect_type(&mut self, t: *mut c_void) {
        self.pe_types.push(t);
    }

    pub fn create_post_effect(&mut self, params: *mut c_void) -> *mut c_void {
        let mut len = 0usize;
        let wanted: bool = !params.is_null();
        let pname = if wanted {
            let p = match host() {
                // SAFETY: table contract.
                Some(t) => unsafe { (t.params_name)(params, &mut len) },
                None => std::ptr::null(),
            };
            // SAFETY: the parameter object owns the name.
            unsafe { h::text(p, len) }
        } else {
            ""
        };
        let found = self.pe_types.iter().copied().find(|&t| {
            let mut n = 0usize;
            let p = match host() {
                // SAFETY: table contract.
                Some(tbl) => unsafe { (tbl.pe_type_name)(t, &mut n) },
                None => std::ptr::null(),
            };
            // SAFETY: the type owns its name.
            unsafe { h::text(p, n) == pname }
        });
        let Some(t) = found else {
            log(true, &format!("Could not find post effect type {pname}\n"));
            return null_mut();
        };
        let pe = callp!(pe_type_create(t, params));
        call!(pe_set_params(pe, params));
        self.post_effects.push(pe);
        pe
    }

    pub fn destroy_post_effect(&mut self, pe: *mut c_void) {
        find_and_delete(OBJ_POST_EFFECT, &mut self.post_effects, pe);
    }

    pub fn create_gpu_program(&mut self, n: &str) -> *mut c_void {
        let (p, len) = name(n);
        let prog = callp!(ll_create_prog(self.ll, p, len));
        call!(prog_set_res(prog, self.res));
        self.programs.push(prog);
        prog
    }

    pub fn create_gpu_program_from_shaders(
        &mut self,
        n: &str,
        vtx: &str,
        frag: &str,
        vars: *mut c_void,
    ) -> *mut c_void {
        let (p, len) = name(vtx);
        let vs = callp!(shader_create(self.res, p, len, SHADER_VERTEX, vars));
        if vs.is_null() {
            return null_mut();
        }
        let (p, len) = name(frag);
        let fs = callp!(shader_create(self.res, p, len, SHADER_FRAGMENT, vars));
        if fs.is_null() {
            call!(shader_destroy(self.res, vs));
            return null_mut();
        }
        let prog = self.create_gpu_program(n);
        call!(prog_set_shader(prog, SHADER_VERTEX, vs));
        call!(prog_set_shader(prog, SHADER_FRAGMENT, fs));
        call!(prog_link(prog));
        prog
    }

    pub fn destroy_gpu_program(&mut self, prog: *mut c_void) {
        find_and_delete(OBJ_GPU_PROGRAM, &mut self.programs, prog);
    }

    pub fn add_material_type(&mut self, t: *mut c_void, n: &str) {
        let (p, len) = name(n);
        call!(mat_type_set_name(t, p, len));
        call!(mat_type_load(t));
        self.material_types.entry(n.to_owned()).or_insert(t);
    }

    pub fn material_type(&self, n: &str) -> *mut c_void {
        self.material_types
            .get(&n.to_ascii_lowercase())
            .copied()
            .unwrap_or(null_mut())
    }

    pub fn material_type_names(&self) -> impl Iterator<Item = &String> {
        self.material_types.keys()
    }

    pub fn reload_materials(&mut self) {
        for &t in self.material_types.values() {
            call!(mat_type_reload(t));
        }
    }

    pub fn mesh_creator(&self) -> *mut c_void {
        self.mesh_creator
    }

    pub fn texture_creator(&self) -> *mut c_void {
        self.texture_creator
    }

    pub fn decal_creator(&self) -> *mut c_void {
        self.decal_creator
    }

    pub fn screen_is_setup(&self) -> bool {
        self.screen_setup
    }
}

impl Drop for Graphics {
    fn drop(&mut self) {
        log(false, "Exiting Graphics Module\n");
        log(
            false,
            "--------------------------------------------------------\n",
        );
        for &t in self.material_types.values() {
            call!(mat_type_destroy_data(t));
        }
        for &t in self.material_types.values() {
            call!(mat_type_destroy(t));
        }
        self.material_types.clear();
        call!(mat_destroy_type_specifics(false));
        delete_all(OBJ_POST_EFFECT_TYPE, &mut self.pe_types);
        for r in self.renderers.drain(..) {
            if !r.is_null() {
                call!(renderer_destroy_data(r));
                delete(OBJ_RENDERER, r);
            }
        }
        delete_all(OBJ_COMPOSITE, &mut self.composites);
        delete_all(OBJ_POST_EFFECT, &mut self.post_effects);
        delete_all(OBJ_FRAME_BUFFER, &mut self.frame_buffers);
        delete_all(OBJ_DEPTH_STENCIL, &mut self.depth_stencils);
        delete_all(OBJ_GPU_PROGRAM, &mut self.programs);
        delete_all(OBJ_TEXTURE, &mut self.textures);
        delete(OBJ_MESH_CREATOR, self.mesh_creator);
        delete(OBJ_TEXTURE_CREATOR, self.texture_creator);
        delete(OBJ_DECAL_CREATOR, self.decal_creator);
        log(
            false,
            "--------------------------------------------------------\n\n",
        );
    }
}

/// `cTextureCreator::GenerateScatterDiskMap2D/3D`: sample data from `scatter.rs`, texture upload
/// through the host.
pub fn scatter_texture(tex: *mut c_void, size: i32, samples: i32, sort: bool, three_d: bool) {
    if !crate::math::is_pow2(size) {
        fatal("OffsetTexture 2D size size non-pow2\n");
    }
    if !crate::math::is_pow2(samples) {
        fatal("OffsetTexture 2D samples size non-pow2\n");
    }
    let depth = samples.checked_div(2).unwrap_or(0);
    let len = super::scatter::data_len(size, samples).unwrap_or(0);
    let mut data = vec![0u8; len];
    super::scatter::generate(size, samples, sort, &mut data);
    let dims = if three_d {
        Vector3l {
            x: size,
            y: size,
            z: depth,
        }
    } else {
        Vector3l {
            x: size,
            y: size.wrapping_mul(depth),
            z: 1,
        }
    };
    call!(tex_fill(tex, &dims, data.as_mut_ptr()));
}

fn fatal(msg: &str) {
    call!(fatal(msg.as_ptr(), msg.len()));
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn material_lookup_is_lowercase_and_first_insert_wins() {
        let mut g = Graphics::new(null_mut(), null_mut());
        let (mut x, mut y) = (0u8, 0u8);
        let (a, b) = ((&raw mut x).cast::<c_void>(), (&raw mut y).cast::<c_void>());
        g.add_material_type(a, "water");
        g.add_material_type(b, "water");
        assert_eq!(g.material_type("WATER"), a);
        assert!(g.material_type("none").is_null());
        assert_eq!(g.material_type_names().count(), 1);
        std::mem::forget(g);
    }

    #[test]
    fn renderer_index_is_checked() {
        let g = Graphics::new(null_mut(), null_mut());
        assert!(g.renderer(-1).is_null() && g.renderer(0).is_null());
    }
}
