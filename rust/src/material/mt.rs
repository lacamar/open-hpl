//! `iMaterialType` and its subclasses (stage MT): the type objects, their program combo managers
//! and the renderer-facing calls, over the pure decisions of this module. The engine side
//! (renderer, objects, GPU programs, textures) comes through `MtHost`.

use core::ffi::c_void;
use core::sync::atomic::{AtomicBool, AtomicPtr, AtomicU32, Ordering};
use std::collections::BTreeMap;

use super::ffi::{HostVt, VarsVt, vars_of};
use super::h7::{MODES, MatState};
use super::mt_host::{HostCtx, MtHost, P};
use super::pcm::Pcm;
use super::setup::{Feature, SetupSink};
use super::types::{
    Globals, Kind, MatView, ObjView, ProgSel, RenderView, TexSel, Uniform, Uniforms, blend,
    manager, tex, texsel,
};
use super::vars::{
    LoadOut, ProjectedUvVars, SolidVars, TranslucentVars, VertexBlendVars, WaterVars,
};
use crate::types::{Color, Vector2l};

pub const CODE_TERRAIN_DECAL: i32 = 8;
pub const CODE_BLANK: i32 = 9;

const MAX_UNIFORMS: usize = 64;
const UG_SLOTS: usize = 8;
const BLEND_MANAGERS: usize = 5;
const BLEND_ALPHA: i32 = 4;
const VIRTUAL_POSITION_ADD_SCALE: f32 = 0.03;

static LIGHT_PROBES: AtomicBool = AtomicBool::new(false);
static DEPTH_OF_FIELD: AtomicBool = AtomicBool::new(false);
static WORLD_FOG: AtomicU32 = AtomicU32::new(0);
static DOF_PARAMS: [AtomicU32; 4] = [
    AtomicU32::new(0),
    AtomicU32::new(0),
    AtomicU32::new(0),
    AtomicU32::new(0),
];
static GLOBAL_CREATED: AtomicBool = AtomicBool::new(false);
static GLOBAL_MGR: AtomicPtr<Pcm> = AtomicPtr::new(core::ptr::null_mut());

pub fn world_fog() -> u32 {
    WORLD_FOG.load(Ordering::Relaxed)
}

pub struct Mt {
    host: &'static MtHost,
    code: i32,
    kind: Option<Kind>,
    terrain_decal: bool,
    graphics: P,
    resources: P,
    name: Vec<u8>,
    translucent: bool,
    decal: bool,
    specifics: [bool; MODES],
    used: Vec<i32>,
    own: *mut Pcm,
    blend: [*mut Pcm; BLEND_MANAGERS],
    dissolve: P,
    white: P,
    creator: bool,
    ug_programs: [[P; UG_SLOTS]; MODES],
    ug_fields: [P; 4],
    ug_field_num: i32,
}

fn zeroed<T: Copy>() -> T {
    // SAFETY: only used for repr(C) views of plain numbers and bools, valid when zero.
    unsafe { core::mem::zeroed() }
}

fn pcm_new(host: &'static MtHost, name: &[u8], g: P, r: P) -> *mut Pcm {
    Box::into_raw(Box::new(Pcm::new(host, name, g, r, MODES)))
}

struct Setup {
    own: *mut Pcm,
    blend: [*mut Pcm; BLEND_MANAGERS],
    defs: BTreeMap<Vec<u8>, Vec<u8>>,
}

impl Setup {
    fn manager(&self, code: i32) -> *mut Pcm {
        match usize::try_from(code - manager::BLEND0) {
            Ok(i) if code >= manager::BLEND0 => {
                self.blend.get(i).copied().unwrap_or(core::ptr::null_mut())
            }
            _ => self.own,
        }
    }
}

impl SetupSink for Setup {
    fn clear_defines(&mut self) {
        self.defs.clear();
    }

    fn define(&mut self, name: &str) {
        self.defs.insert(name.as_bytes().to_vec(), Vec::new());
    }

    fn define_value(&mut self, name: &str, value: f32) {
        self.defs
            .insert(name.as_bytes().to_vec(), format!("{value:.6}").into_bytes());
    }

    fn setup_program(
        &mut self,
        manager: i32,
        mode: i32,
        name: &str,
        vtx: &str,
        frag: &str,
        features: &[Feature],
    ) {
        let m = self.manager(manager);
        let feats: Vec<(u32, Vec<u8>)> = features
            .iter()
            .map(|f| (f.stage, f.name.to_bytes().to_vec()))
            .collect();
        // SAFETY: the managers belong to the type being set up.
        if let Some(p) = unsafe { m.as_mut() } {
            p.setup_mode(
                mode,
                name.as_bytes(),
                vtx.as_bytes(),
                frag.as_bytes(),
                &feats,
                &self.defs,
            );
        }
    }

    fn var_id(&mut self, manager: i32, name: &str, id: i32, mode: i32) {
        let m = self.manager(manager);
        // SAFETY: as above.
        if let Some(p) = unsafe { m.as_mut() } {
            p.add_variable_id(name.as_bytes(), id, mode);
        }
    }
}

impl Mt {
    pub fn new(host: &'static MtHost, code: i32, graphics: P, resources: P) -> Self {
        let kind = match code {
            CODE_TERRAIN_DECAL => Some(Kind::Decal),
            CODE_BLANK => None,
            c => Kind::from_i32(c),
        };
        let flags = kind.map_or(0, super::type_flags);
        let null: P = core::ptr::null_mut();
        let has_gpu = !graphics.is_null() && !resources.is_null() && kind.is_some();
        let own = if has_gpu {
            pcm_new(host, b"", graphics, resources)
        } else {
            core::ptr::null_mut()
        };
        let mut blend = [core::ptr::null_mut(); BLEND_MANAGERS];
        if kind == Some(Kind::Translucent) {
            for (i, b) in blend.iter_mut().enumerate() {
                *b = pcm_new(host, format!("Blend{i}").as_bytes(), graphics, resources);
            }
        }
        let mut specifics = [false; MODES];
        for (i, s) in specifics.iter_mut().enumerate() {
            *s = flags & (1 << (8 + i)) != 0;
        }
        Self {
            host,
            code,
            kind,
            terrain_decal: code == CODE_TERRAIN_DECAL,
            graphics,
            resources,
            name: Vec::new(),
            translucent: flags & 1 != 0,
            decal: flags & 2 != 0,
            specifics,
            used: kind
                .map(super::used_textures)
                .unwrap_or_default()
                .into_iter()
                .map(|t| i32::try_from(t).unwrap_or(0))
                .collect(),
            own,
            blend,
            dissolve: null,
            white: null,
            creator: false,
            ug_programs: [[null; UG_SLOTS]; MODES],
            ug_fields: [null; 4],
            ug_field_num: 0,
        }
    }

    pub fn is_translucent(&self) -> bool {
        self.translucent
    }

    pub fn set_name(&mut self, name: &[u8]) {
        self.name = name.to_vec();
        // SAFETY: `own` is this type's manager or null.
        if let Some(p) = unsafe { self.own.as_mut() } {
            p.set_name(name);
        }
    }

    fn pcm(&self, code: i32) -> *mut Pcm {
        match code {
            manager::OWN => self.own,
            manager::GLOBAL => GLOBAL_MGR.load(Ordering::Relaxed),
            c if c >= manager::BLEND0 => usize::try_from(c - manager::BLEND0)
                .ok()
                .and_then(|i| self.blend.get(i))
                .copied()
                .unwrap_or(core::ptr::null_mut()),
            _ => core::ptr::null_mut(),
        }
    }

    fn globals(&self, with_graphics: bool, vps: f32) -> Globals {
        let mut g: Globals = zeroed();
        let gfx = if with_graphics {
            self.graphics
        } else {
            core::ptr::null_mut()
        };
        // SAFETY: the host fills a valid `Globals`.
        unsafe { (self.host.globals)(gfx, vps, &mut g) };
        g.light_probes = LIGHT_PROBES.load(Ordering::Relaxed);
        g.depth_of_field = DEPTH_OF_FIELD.load(Ordering::Relaxed);
        g.world_fog = WORLD_FOG.load(Ordering::Relaxed);
        let [r, gr, b, a] = &DOF_PARAMS;
        g.dof_params = Color {
            r: f32::from_bits(r.load(Ordering::Relaxed)),
            g: f32::from_bits(gr.load(Ordering::Relaxed)),
            b: f32::from_bits(b.load(Ordering::Relaxed)),
            a: f32::from_bits(a.load(Ordering::Relaxed)),
        };
        g
    }

    fn tex_info(&self, t: P) -> (i32, i32, bool) {
        let (mut w, mut h, mut srgb) = (0, 0, false);
        // SAFETY: `t` is a live texture.
        unsafe { (self.host.tex_info)(t, &mut w, &mut h, &mut srgb) };
        (w, h, srgb)
    }

    fn view(&self, m: &MatState) -> MatView {
        let mut mask = 0u64;
        for (i, t) in m.textures.iter().enumerate() {
            if !t.is_null() {
                mask |= 1u64 << i;
            }
        }
        let tex_at = |slot: u32| {
            m.textures
                .get(slot as usize)
                .copied()
                .filter(|t| !t.is_null())
        };
        let liquid = tex_at(tex::LIQUID).map_or(Vector2l { x: 0, y: 0 }, |t| {
            let (w, h, _) = self.tex_info(t);
            Vector2l { x: w, y: h }
        });
        MatView {
            tex_mask: mask,
            uv_matrix: m.uv,
            liquid_size: liquid,
            blend_mode: m.blend,
            has_uv_anim: m.has_uv,
            diffuse_srgb: tex_at(tex::DIFFUSE).is_some_and(|t| self.tex_info(t).2),
        }
    }

    fn pod(&self, m: &MatState) -> *const c_void {
        match (self.kind, m.vars.is_null()) {
            (Some(_), false) => {
                // SAFETY: `vars` is the iMaterialVars object of this type.
                unsafe { (self.host.vars_pod)(self.code, m.vars) }
            }
            _ => core::ptr::null(),
        }
    }

    fn vars<'a>(&self, kind: Kind, m: &MatState) -> super::Vars<'a> {
        let p = self.pod(m);
        // SAFETY: the pod points at the vars struct of `kind` or is null.
        unsafe { vars_of(kind, p) }
    }

    fn pick_texture(&self, sel: TexSel, m: &MatState) -> P {
        match sel.kind {
            texsel::SLOT => usize::try_from(sel.slot)
                .ok()
                .and_then(|i| m.textures.get(i))
                .copied()
                .unwrap_or(core::ptr::null_mut()),
            texsel::DISSOLVE => self.dissolve,
            texsel::REFRACTION | texsel::SCENE_DEPTH | texsel::REFLECTION => {
                // SAFETY: the graphics' main renderer answers.
                unsafe { (self.host.special_tex)(self.graphics, sel.kind) }
            }
            texsel::WHITE => self.white,
            _ => core::ptr::null_mut(),
        }
    }

    fn pick_program(&self, sel: ProgSel) -> P {
        // SAFETY: managers are live while the type is.
        match unsafe { self.pcm(sel.manager).as_mut() } {
            Some(p) if sel.manager != manager::NONE => {
                p.generate_program(sel.mode, sel.flags as i32)
            }
            _ => core::ptr::null_mut(),
        }
    }

    pub fn texture_for_unit(&self, m: &MatState, mode: i32, unit: i32) -> P {
        let Some(kind) = self.kind else {
            return core::ptr::null_mut();
        };
        let g = self.globals(false, 0.0);
        let sel = super::texture_unit(kind, &self.view(m), self.vars(kind, m), &g, mode, unit);
        self.pick_texture(sel, m)
    }

    pub fn gpu_program(&self, m: &MatState, mode: i32, skeleton: i32) -> P {
        let Some(kind) = self.kind else {
            return core::ptr::null_mut();
        };
        let g = self.globals(false, 0.0);
        let sel = super::program(
            kind,
            &self.view(m),
            self.vars(kind, m),
            &g,
            mode,
            skeleton != 0,
        );
        self.pick_program(sel)
    }

    pub fn destroy_program(&self, blend_mode: i32, mode: i32, prog: P) {
        let Some(kind) = self.kind else { return };
        let bm = if kind == Kind::Translucent {
            blend_mode
        } else {
            0
        };
        let sel = super::destroy_target(kind, mode, bm);
        // SAFETY: managers are live while the type is.
        if let Some(p) = unsafe { self.pcm(sel.manager).as_mut() } {
            p.destroy_generated_program(sel.mode, prog);
        }
    }

    fn render_view(&self, renderer: P) -> RenderView {
        let mut r: RenderView = zeroed();
        // SAFETY: the host fills a valid view of a live renderer.
        unsafe { (self.host.render_view)(renderer, &mut r) };
        r
    }

    fn host_vt(&mut self, renderer: P, object: P, own_fields: bool) -> (HostCtx, HostVt) {
        let ctx = HostCtx {
            renderer,
            object,
            fields: if own_fields {
                self.ug_fields.as_mut_ptr()
            } else {
                core::ptr::null_mut()
            },
            field_num: if own_fields {
                &mut self.ug_field_num
            } else {
                core::ptr::null_mut()
            },
        };
        (ctx, HostVt::empty())
    }

    fn apply(&self, prog: P, u: &Uniforms) {
        let s = u.as_slice();
        if !s.is_empty() {
            // SAFETY: the program is live; `s` outlives the call.
            unsafe { (self.host.prog_apply)(prog, s.as_ptr(), s.len()) };
        }
    }

    pub fn setup_type(&self, mode: i32, prog: P, renderer: P) {
        let Some(kind) = self.kind else { return };
        let r = self.render_view(renderer);
        let mut buf = [Uniform::ZERO; MAX_UNIFORMS];
        let mut u = Uniforms::new(&mut buf);
        super::setup_type(kind, mode, &r, &mut u);
        self.apply(prog, &u);
    }

    pub fn setup_material(&mut self, mode: i32, prog: P, m: &MatState, renderer: P) {
        let Some(kind) = self.kind else { return };
        let ug = kind == Kind::Undergrowth;
        let v = self.view(m);
        let g = self.globals(false, 0.0);
        let r = self.render_view(renderer);
        let vars = self.vars(kind, m);
        let fields = if ug {
            usize::try_from(self.ug_field_num).unwrap_or(0)
        } else {
            0
        };
        let (mut ctx, mut vt) = self.host_vt(renderer, core::ptr::null_mut(), ug);
        // SAFETY: the host fills the table with thunks over `ctx`, which outlives the call.
        unsafe { (self.host.host_vt)(&mut ctx, &mut vt) };
        let mut buf = [Uniform::ZERO; MAX_UNIFORMS];
        let mut u = Uniforms::new(&mut buf);
        super::setup_material(kind, mode, &v, vars, &g, &r, fields, &mut vt, &mut u);
        self.apply(prog, &u);
    }

    pub fn setup_object(&mut self, mode: i32, prog: P, object: P, renderer: P) {
        let Some(kind) = self.kind else { return };
        let mut o: ObjView = zeroed();
        // SAFETY: the host fills the view of a live object and returns its material state.
        let ms = unsafe { (self.host.obj_view)(object, &mut o) }.cast::<MatState>();
        // SAFETY: the state belongs to the live material of the object.
        let Some(m) = (unsafe { ms.as_ref() }) else {
            return;
        };
        let v = self.view(m);
        let g = self.globals(false, 0.0);
        let r = self.render_view(renderer);
        let vars = self.vars(kind, m);
        let (mut ctx, mut vt) = self.host_vt(renderer, object, false);
        // SAFETY: as in `setup_material`.
        unsafe { (self.host.host_vt)(&mut ctx, &mut vt) };
        let mut buf = [Uniform::ZERO; MAX_UNIFORMS];
        let mut u = Uniforms::new(&mut buf);
        super::setup_object(kind, mode, &v, vars, &g, &r, &o, &mut vt, &mut u);
        self.apply(prog, &u);
    }

    pub fn has_type_specifics(&self, mode: i32) -> bool {
        usize::try_from(mode)
            .ok()
            .and_then(|i| self.specifics.get(i))
            .copied()
            .unwrap_or(false)
    }

    /// `GetRenderProgram`
    pub fn render_program(&mut self, m: &mut MatState, mode: i32, renderer: P) -> P {
        let Ok(mi) = usize::try_from(mode) else {
            return core::ptr::null_mut();
        };
        let slot0 = |m: &MatState| {
            m.programs
                .first()
                .and_then(|r| r.get(mi))
                .copied()
                .unwrap_or(core::ptr::null_mut())
        };
        match self.kind {
            Some(Kind::Translucent) => {
                let g = self.globals(false, 0.0);
                if super::render_program_slot(Kind::Translucent, mode, &g) == 0 {
                    return slot0(m);
                }
                let have = m.programs.get(1).and_then(|r| r.get(mi)).copied();
                if have.is_none_or(<*mut c_void>::is_null) {
                    let p = self.gpu_program(m, mode, 1);
                    if let Some(s) = m.programs.get_mut(1).and_then(|r| r.get_mut(mi)) {
                        *s = p;
                    }
                }
                m.programs
                    .get(1)
                    .and_then(|r| r.get(mi))
                    .copied()
                    .unwrap_or(core::ptr::null_mut())
            }
            Some(Kind::Undergrowth) => self.undergrowth_render_program(m, mode, mi, renderer),
            _ => slot0(m),
        }
    }

    fn undergrowth_render_program(&mut self, m: &MatState, mode: i32, mi: usize, renderer: P) -> P {
        let r = self.render_view(renderer);
        let (mut ctx, mut vt) = self.host_vt(renderer, core::ptr::null_mut(), true);
        // SAFETY: as in `setup_material`.
        unsafe { (self.host.host_vt)(&mut ctx, &mut vt) };
        let Vars::Undergrowth(vars) = self.vars(Kind::Undergrowth, m) else {
            return core::ptr::null_mut();
        };
        let (sel, n) = super::undergrowth_render_program(vars, mode, &r, &mut vt);
        if sel.manager == manager::NONE {
            return core::ptr::null_mut();
        }
        self.ug_field_num = i32::try_from(n).unwrap_or(0);
        let own = self.own;
        let slot = usize::try_from(sel.flags)
            .ok()
            .and_then(|f| self.ug_programs.get_mut(mi).and_then(|r| r.get_mut(f)));
        match slot {
            Some(p) => {
                if p.is_null() {
                    // SAFETY: `own` is this type's manager.
                    *p = unsafe { own.as_mut() }.map_or(core::ptr::null_mut(), |c| {
                        c.generate_program(sel.mode, sel.flags as i32)
                    });
                }
                *p
            }
            None => core::ptr::null_mut(),
        }
    }

    /// `CompileMaterialSpecifics`
    pub fn compile_specifics(&self, m: &mut MatState) {
        let Some(kind) = self.kind else { return };
        let g = self.globals(false, 0.0);
        let out = super::compile(kind, &self.view(m), self.vars(kind, m), &g);
        for (mode, (spec, obj)) in m
            .has_specific
            .iter_mut()
            .zip(m.has_object_specific.iter_mut())
            .enumerate()
        {
            if out.spec_set >> mode & 1 != 0 {
                *spec = out.spec_val >> mode & 1 != 0;
            }
            if out.obj_set >> mode & 1 != 0 {
                *obj = out.obj_val >> mode & 1 != 0;
            }
        }
        if out.alpha_mode >= 0 {
            m.set_alpha(out.alpha_mode);
        }
        if out.dissolve_filter >= 0 {
            m.use_alpha_dissolve_filter = out.dissolve_filter != 0;
        }
        if out.blend_mode >= 0 {
            m.set_blend(out.blend_mode);
        }
        if out.has_refraction >= 0 {
            m.has_refraction = out.has_refraction != 0;
        }
        if out.refraction_edge >= 0 {
            m.use_refraction_edge_check = out.refraction_edge != 0;
        }
        if out.world_reflection >= 0 {
            m.has_world_reflection = out.world_reflection != 0;
        }
        if out.translucent_illum >= 0 {
            m.has_translucent_illumination = out.translucent_illum != 0;
        }
        if self.terrain_decal {
            m.set_blend(BLEND_ALPHA);
        }
    }

    pub fn create_vars(&self) -> P {
        // SAFETY: plain allocation of the vars class of the type.
        unsafe { (self.host.vars_new)(self.code) }
    }

    fn vars_vt(&self, obj: P) -> VarsVt {
        let mut vt = VarsVt::empty();
        // SAFETY: the host fills a table over the live vars object.
        unsafe { (self.host.vars_vt)(obj, &mut vt) };
        vt
    }

    /// `LoadVariables`
    pub fn load_variables(&self, m: &mut MatState, obj: P) {
        let Some(kind) = self.kind else { return };
        if matches!(kind, Kind::TerrainBlend | Kind::Undergrowth) {
            return;
        }
        if m.vars.is_null() {
            m.vars = self.create_vars();
        }
        if kind == Kind::Decal {
            return;
        }
        let pod = self.pod(m).cast_mut();
        if pod.is_null() {
            return;
        }
        let mut api = self.vars_vt(obj);
        let mut lo = LoadOut::NONE;
        // SAFETY: the pod is the vars struct of the kind and no other reference exists.
        unsafe {
            match kind {
                Kind::SolidDiffuse => super::load_solid(&mut *pod.cast::<SolidVars>(), &mut api),
                Kind::ProjectedUv => {
                    super::load_projected_uv(&mut *pod.cast::<ProjectedUvVars>(), &mut api)
                }
                Kind::VertexBlend => {
                    super::load_vertex_blend(&mut *pod.cast::<VertexBlendVars>(), &mut api)
                }
                Kind::Translucent => {
                    super::load_translucent(&mut *pod.cast::<TranslucentVars>(), &mut api, &mut lo)
                }
                Kind::Water => super::load_water(&mut *pod.cast::<WaterVars>(), &mut api, &mut lo),
                Kind::Decal | Kind::TerrainBlend | Kind::Undergrowth => {}
            }
        }
        if lo.affected_by_dof >= 0 {
            m.affected_by_depth_of_field = lo.affected_by_dof != 0;
        }
        if lo.world_reflection_occlusion_test >= 0 {
            m.world_reflection_occlusion_test = lo.world_reflection_occlusion_test != 0;
        }
        if lo.has_max_reflection_distance {
            m.max_reflection_distance = lo.max_reflection_distance;
        }
        if lo.large_transparent_surface >= 0 {
            m.large_transperant_surface = lo.large_transparent_surface != 0;
        }
    }

    /// `GetVariableValues` into a new vars object.
    pub fn var_values(&self, m: &MatState) -> P {
        // SAFETY: plain allocation.
        let obj = unsafe { (self.host.vars_object_new)() };
        let Some(kind) = self.kind else { return obj };
        let mut api = self.vars_vt(obj);
        match self.vars(kind, m) {
            Vars::Solid(v) => super::solid_values(v, &mut api),
            Vars::Translucent(v) => {
                super::translucent_values(v, m.affected_by_depth_of_field, &mut api)
            }
            Vars::Water(v) => super::water_values(
                v,
                m.world_reflection_occlusion_test,
                m.large_transperant_surface,
                &mut api,
            ),
            _ => {}
        }
        obj
    }

    fn run_setup(&self, kind: Kind, global: bool, g: &Globals) {
        let mut s = Setup {
            own: self.own,
            blend: self.blend,
            defs: BTreeMap::new(),
        };
        super::setup_programs(kind, global, g, &mut s);
    }

    fn solid(&self) -> bool {
        matches!(
            self.kind,
            Some(Kind::SolidDiffuse | Kind::ProjectedUv | Kind::VertexBlend)
        )
    }

    pub fn load_data(&mut self) {
        let Some(kind) = self.kind else { return };
        if self.own.is_null() {
            return;
        }
        match kind {
            Kind::SolidDiffuse | Kind::ProjectedUv | Kind::VertexBlend => {
                if !GLOBAL_CREATED.swap(true, Ordering::Relaxed) {
                    self.creator = true;
                    let g = self.globals(false, 0.0);
                    self.run_setup(Kind::SolidDiffuse, true, &g);
                    GLOBAL_MGR.store(self.own, Ordering::Relaxed);
                }
                let n = b"core_dissolve.tga";
                // SAFETY: the resources' texture manager creates the texture.
                self.dissolve =
                    unsafe { (self.host.tex_create_2d)(self.resources, n.as_ptr(), n.len(), true) };
                let g = if kind == Kind::SolidDiffuse {
                    self.globals(true, VIRTUAL_POSITION_ADD_SCALE)
                } else {
                    self.globals(false, 0.0)
                };
                self.run_setup(kind, false, &g);
            }
            Kind::Water => {
                let mut g = self.globals(false, 0.0);
                let n = b"water_surface_vtx.hpsl";
                // SAFETY: the resources' file searcher answers.
                g.has_water_vtx =
                    unsafe { (self.host.file_exists)(self.resources, n.as_ptr(), n.len()) };
                self.run_setup(kind, false, &g);
            }
            Kind::TerrainBlend => {
                let g = self.globals(false, 0.0);
                self.run_setup(kind, false, &g);
                // SAFETY: the graphics creates a 1x1 white texture.
                self.white = unsafe { (self.host.tex_white_new)(self.graphics) };
            }
            Kind::Translucent | Kind::Decal | Kind::Undergrowth => {
                let g = self.globals(false, 0.0);
                self.run_setup(kind, false, &g);
            }
        }
    }

    pub fn destroy_data(&mut self) {
        let Some(kind) = self.kind else { return };
        if self.solid() {
            if !self.dissolve.is_null() {
                // SAFETY: the texture manager destroys what it created.
                unsafe { (self.host.tex_destroy)(self.resources, self.dissolve) };
                self.dissolve = core::ptr::null_mut();
            }
            if self.creator {
                GLOBAL_CREATED.store(false, Ordering::Relaxed);
            }
        }
        if kind == Kind::Translucent {
            for b in self.blend {
                // SAFETY: the blend managers are live.
                if let Some(p) = unsafe { b.as_mut() } {
                    p.destroy_shaders_and_programs();
                }
            }
            return;
        }
        // SAFETY: `own` is live.
        if let Some(p) = unsafe { self.own.as_mut() } {
            p.destroy_shaders_and_programs();
        }
        if kind == Kind::TerrainBlend && !self.white.is_null() {
            // SAFETY: the graphics destroys the texture it created.
            unsafe { (self.host.tex_white_free)(self.graphics, self.white) };
            self.white = core::ptr::null_mut();
        }
        if kind == Kind::Undergrowth {
            self.ug_programs = [[core::ptr::null_mut(); UG_SLOTS]; MODES];
        }
    }

    pub fn reload(&mut self) {
        self.destroy_data();
        self.load_data();
    }
}

impl Drop for Mt {
    fn drop(&mut self) {
        for p in core::iter::once(self.own).chain(self.blend) {
            if !p.is_null() {
                // SAFETY: allocated by `pcm_new`, freed once.
                drop(unsafe { Box::from_raw(p) });
            }
        }
    }
}

use super::Vars;

/// # Safety
/// `t` is a live type from `ohpl_mt_new`.
unsafe fn mt<'a>(t: *mut c_void) -> Option<&'a mut Mt> {
    // SAFETY: caller contract.
    unsafe { t.cast::<Mt>().as_mut() }
}

/// # Safety
/// `m` is null or a live material state.
unsafe fn state<'a>(m: *mut c_void) -> Option<&'a mut MatState> {
    // SAFETY: caller contract.
    unsafe { m.cast::<MatState>().as_mut() }
}

/// # Safety
/// `host` is a valid table living for the process.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mt_new(
    host: *const MtHost,
    code: i32,
    graphics: P,
    resources: P,
) -> *mut c_void {
    // SAFETY: caller contract.
    let Some(host) = (unsafe { host.as_ref() }) else {
        return core::ptr::null_mut();
    };
    Box::into_raw(Box::new(Mt::new(host, code, graphics, resources))).cast()
}

/// # Safety
/// `t` is from `ohpl_mt_new`, freed once.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mt_free(t: *mut c_void) {
    if !t.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(t.cast::<Mt>()) });
    }
}

/// # Safety
/// `t` is live; `name` is readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mt_set_name(t: *mut c_void, name: *const u8, n: usize) {
    // SAFETY: caller contract.
    if let Some(m) = unsafe { mt(t) } {
        let s = if name.is_null() {
            &[][..]
        } else {
            // SAFETY: caller contract.
            unsafe { core::slice::from_raw_parts(name, n) }
        };
        m.set_name(s);
    }
}

/// # Safety
/// `t` is live; `out`/`n` are valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mt_name(t: *mut c_void, out: *mut *const u8, n: *mut usize) {
    // SAFETY: caller contract.
    if let (Some(m), Some(out), Some(n)) = (unsafe { mt(t) }, unsafe { out.as_mut() }, unsafe {
        n.as_mut()
    }) {
        *out = m.name.as_ptr();
        *n = m.name.len();
    }
}

/// # Safety
/// `t` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mt_is_translucent(t: *mut c_void) -> bool {
    // SAFETY: caller contract.
    unsafe { mt(t) }.is_some_and(|m| m.translucent)
}

/// # Safety
/// `t` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mt_is_decal(t: *mut c_void) -> bool {
    // SAFETY: caller contract.
    unsafe { mt(t) }.is_some_and(|m| m.decal)
}

/// # Safety
/// `t` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mt_has_type_specifics(t: *mut c_void, mode: i32) -> bool {
    // SAFETY: caller contract.
    unsafe { mt(t) }.is_some_and(|m| m.has_type_specifics(mode))
}

/// # Safety
/// `t` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mt_used_texture_num(t: *mut c_void) -> i32 {
    // SAFETY: caller contract.
    unsafe { mt(t) }.map_or(0, |m| i32::try_from(m.used.len()).unwrap_or(0))
}

/// # Safety
/// `t` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mt_used_texture(t: *mut c_void, i: i32) -> i32 {
    // SAFETY: caller contract.
    unsafe { mt(t) }
        .and_then(|m| usize::try_from(i).ok().and_then(|i| m.used.get(i)))
        .copied()
        .unwrap_or(0)
}

/// The `Kind` value of the type, or -1 for the blank type.
///
/// # Safety
/// `t` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mt_code(t: *mut c_void) -> i32 {
    // SAFETY: caller contract.
    unsafe { mt(t) }.map_or(-1, |m| m.code)
}

/// # Safety
/// `t` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mt_load_data(t: *mut c_void) {
    // SAFETY: caller contract.
    if let Some(m) = unsafe { mt(t) } {
        m.load_data();
    }
}

/// # Safety
/// `t` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mt_destroy_data(t: *mut c_void) {
    // SAFETY: caller contract.
    if let Some(m) = unsafe { mt(t) } {
        m.destroy_data();
    }
}

/// # Safety
/// `t` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mt_reload(t: *mut c_void) {
    // SAFETY: caller contract.
    if let Some(m) = unsafe { mt(t) } {
        m.reload();
    }
}

/// # Safety
/// `t` and `m` are live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mt_render_program(
    t: *mut c_void,
    m: *mut c_void,
    mode: i32,
    renderer: P,
) -> P {
    // SAFETY: caller contract.
    match (unsafe { mt(t) }, unsafe { state(m) }) {
        (Some(t), Some(m)) => t.render_program(m, mode, renderer),
        _ => core::ptr::null_mut(),
    }
}

/// # Safety
/// `t` is live; `prog` is a live program.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mt_setup_type(t: *mut c_void, mode: i32, prog: P, renderer: P) {
    // SAFETY: caller contract.
    if let Some(t) = unsafe { mt(t) } {
        t.setup_type(mode, prog, renderer);
    }
}

/// # Safety
/// `t` and `m` are live; `prog` is a live program.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mt_setup_material(
    t: *mut c_void,
    mode: i32,
    prog: P,
    m: *mut c_void,
    renderer: P,
) {
    // SAFETY: caller contract.
    if let (Some(t), Some(m)) = (unsafe { mt(t) }, unsafe { state(m) }) {
        t.setup_material(mode, prog, m, renderer);
    }
}

/// # Safety
/// `t` is live; `prog` and `object` are live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mt_setup_object(
    t: *mut c_void,
    mode: i32,
    prog: P,
    object: P,
    renderer: P,
) {
    // SAFETY: caller contract.
    if let Some(t) = unsafe { mt(t) } {
        t.setup_object(mode, prog, object, renderer);
    }
}

/// # Safety
/// `t` and `m` are live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mt_load_variables(t: *mut c_void, m: *mut c_void, obj: P) {
    // SAFETY: caller contract.
    if let (Some(t), Some(m)) = (unsafe { mt(t) }, unsafe { state(m) }) {
        t.load_variables(m, obj);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_mt_set_light_probes(on: bool) {
    LIGHT_PROBES.store(on, Ordering::Relaxed);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_mt_set_world_fog(underwater: bool, secondary: bool) {
    WORLD_FOG.store(
        super::world_fog_flags(underwater, secondary),
        Ordering::Relaxed,
    );
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_mt_get_world_fog() -> u32 {
    world_fog()
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_mt_set_depth_of_field(on: bool, r: f32, g: f32, b: f32, a: f32) {
    DEPTH_OF_FIELD.store(on, Ordering::Relaxed);
    for (s, v) in DOF_PARAMS.iter().zip([r, g, b, a]) {
        s.store(v.to_bits(), Ordering::Relaxed);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_mt_get_light_probes() -> bool {
    LIGHT_PROBES.load(Ordering::Relaxed)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_mt_get_depth_of_field() -> bool {
    DEPTH_OF_FIELD.load(Ordering::Relaxed)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_mt_get_dof_params(out: *mut f32) {
    if out.is_null() {
        return;
    }
    for (i, s) in DOF_PARAMS.iter().enumerate() {
        // SAFETY: the caller passes room for four floats.
        unsafe { out.add(i).write(f32::from_bits(s.load(Ordering::Relaxed))) };
    }
}

const _: () = {
    assert!(blend::ADD == 1);
    assert!(manager::OWN == 0 && manager::GLOBAL == 1);
};
