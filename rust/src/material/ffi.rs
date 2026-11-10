//! C ABI of the material port, declared in `HPL2/core/include/graphics/RustMaterial.h`.
//!
//! Pointers are valid and aligned for the call unless documented as nullable; outputs do not
//! alias inputs. `vars` pointers address the `Ohpl*Vars` base of the C++ `*_Vars` class of the
//! `kind` and are nullable. Callback tables have every entry optional; a missing entry answers
//! with the neutral value.

use super::Vars;
use super::anim::{UvAnim, uv_matrix};
use super::setup::{Feature, SetupSink};
use super::types::{
    Bv, CompileOut, ForceFieldInfo, Globals, Host, Kind, LightInfo, MatView, ObjView, ProgSel,
    RenderView, TexSel, Uniform, Uniforms, WorldInfo,
};
use super::vars::{
    LoadOut, ProjectedUvVars, SolidVars, TerrainBlendVars, TranslucentVars, UndergrowthVars,
    VarDefault, VarSink, VarSource, VertexBlendVars, WaterVars,
};
use crate::types::{Color, Matrixf, Vector2f, Vector3f};
use std::ffi::{c_char, c_void};

const _: () = {
    use std::mem::size_of;
    assert!(size_of::<SolidVars>() == 172);
    assert!(size_of::<ProjectedUvVars>() == 24);
    assert!(size_of::<VertexBlendVars>() == 88);
    assert!(size_of::<TranslucentVars>() == 72);
    assert!(size_of::<WaterVars>() == 60);
    assert!(size_of::<TerrainBlendVars>() == 136);
    assert!(size_of::<UndergrowthVars>() == 44);
    assert!(size_of::<LoadOut>() == 20);
};

/// # Safety
/// `p` is null or points to a valid `T`.
unsafe fn read<'a, T>(p: *const T) -> Option<&'a T> {
    // SAFETY: caller contract.
    unsafe { p.as_ref() }
}

/// # Safety
/// `p` is null or points to a valid, initialised vars struct of `kind`.
unsafe fn vars_of<'a>(kind: Kind, p: *const c_void) -> Vars<'a> {
    // SAFETY: caller contract; the pointee type is the one the kind names.
    unsafe {
        match kind {
            Kind::SolidDiffuse => read(p.cast::<SolidVars>()).map_or(Vars::None, Vars::Solid),
            Kind::ProjectedUv => {
                read(p.cast::<ProjectedUvVars>()).map_or(Vars::None, Vars::ProjectedUv)
            }
            Kind::VertexBlend => {
                read(p.cast::<VertexBlendVars>()).map_or(Vars::None, Vars::VertexBlend)
            }
            Kind::Translucent => {
                read(p.cast::<TranslucentVars>()).map_or(Vars::None, Vars::Translucent)
            }
            Kind::Water => read(p.cast::<WaterVars>()).map_or(Vars::None, Vars::Water),
            Kind::TerrainBlend => {
                read(p.cast::<TerrainBlendVars>()).map_or(Vars::None, Vars::TerrainBlend)
            }
            Kind::Undergrowth => {
                read(p.cast::<UndergrowthVars>()).map_or(Vars::None, Vars::Undergrowth)
            }
            Kind::Decal => Vars::None,
        }
    }
}

/// `cResourceVarsObject` accessors, supplied by the shim.
#[repr(C)]
pub struct VarsVt {
    ctx: *mut c_void,
    get_bool: Option<unsafe extern "C" fn(*mut c_void, *const u8, usize, bool) -> bool>,
    get_float: Option<unsafe extern "C" fn(*mut c_void, *const u8, usize, f32) -> f32>,
    get_vec2:
        Option<unsafe extern "C" fn(*mut c_void, *const u8, usize, *const Vector2f, *mut Vector2f)>,
    get_vec3:
        Option<unsafe extern "C" fn(*mut c_void, *const u8, usize, *const Vector3f, *mut Vector3f)>,
    get_color:
        Option<unsafe extern "C" fn(*mut c_void, *const u8, usize, *const Color, *mut Color)>,
    /// copies the value (or the default) to `out` and returns its full length
    get_string: Option<
        unsafe extern "C" fn(
            *mut c_void,
            *const u8,
            usize,
            *const u8,
            usize,
            *mut u8,
            usize,
        ) -> usize,
    >,
    add_bool: Option<unsafe extern "C" fn(*mut c_void, *const u8, usize, bool)>,
    add_float: Option<unsafe extern "C" fn(*mut c_void, *const u8, usize, f32)>,
}

impl VarSource for VarsVt {
    fn boolean(&mut self, name: &str, def: bool) -> bool {
        match self.get_bool {
            // SAFETY: table contract.
            Some(f) => unsafe { f(self.ctx, name.as_ptr(), name.len(), def) },
            None => def,
        }
    }

    fn float(&mut self, name: &str, def: f32) -> f32 {
        match self.get_float {
            // SAFETY: table contract.
            Some(f) => unsafe { f(self.ctx, name.as_ptr(), name.len(), def) },
            None => def,
        }
    }

    fn vec2(&mut self, name: &str, def: Vector2f) -> Vector2f {
        let mut out = def;
        if let Some(f) = self.get_vec2 {
            // SAFETY: table contract; `def` and `out` are live locals.
            unsafe { f(self.ctx, name.as_ptr(), name.len(), &def, &mut out) };
        }
        out
    }

    fn vec3(&mut self, name: &str, def: Vector3f) -> Vector3f {
        let mut out = def;
        if let Some(f) = self.get_vec3 {
            // SAFETY: table contract; `def` and `out` are live locals.
            unsafe { f(self.ctx, name.as_ptr(), name.len(), &def, &mut out) };
        }
        out
    }

    fn color(&mut self, name: &str, def: Color) -> Color {
        let mut out = def;
        if let Some(f) = self.get_color {
            // SAFETY: table contract; `def` and `out` are live locals.
            unsafe { f(self.ctx, name.as_ptr(), name.len(), &def, &mut out) };
        }
        out
    }

    fn string(&mut self, name: &str, def: &str) -> Vec<u8> {
        let Some(f) = self.get_string else {
            return def.as_bytes().to_vec();
        };
        let mut buf = [0u8; 64];
        // SAFETY: table contract; `buf` is a live local of the stated capacity.
        let len = unsafe {
            f(
                self.ctx,
                name.as_ptr(),
                name.len(),
                def.as_ptr(),
                def.len(),
                buf.as_mut_ptr(),
                buf.len(),
            )
        };
        buf.get(..len.min(buf.len())).unwrap_or(&[]).to_vec()
    }
}

impl VarSink for VarsVt {
    fn add_bool(&mut self, name: &str, v: bool) {
        if let Some(f) = self.add_bool {
            // SAFETY: table contract.
            unsafe { f(self.ctx, name.as_ptr(), name.len(), v) };
        }
    }

    fn add_float(&mut self, name: &str, v: f32) {
        if let Some(f) = self.add_float {
            // SAFETY: table contract.
            unsafe { f(self.ctx, name.as_ptr(), name.len(), v) };
        }
    }
}

/// `cProgramComboFeature` as the shim reads it.
#[repr(C)]
pub struct FeatureC {
    name: *const c_char,
    stage: u32,
    required: u32,
}

/// `cParserVarContainer` / `cProgramComboManager` calls of a `LoadData`.
#[repr(C)]
pub struct SetupVt {
    ctx: *mut c_void,
    clear_defines: Option<unsafe extern "C" fn(*mut c_void)>,
    define: Option<unsafe extern "C" fn(*mut c_void, *const u8, usize)>,
    define_value: Option<unsafe extern "C" fn(*mut c_void, *const u8, usize, f32)>,
    setup_program: Option<
        unsafe extern "C" fn(
            *mut c_void,
            i32,
            i32,
            *const u8,
            usize,
            *const u8,
            usize,
            *const u8,
            usize,
            *const FeatureC,
            usize,
        ),
    >,
    var_id: Option<unsafe extern "C" fn(*mut c_void, i32, *const u8, usize, i32, i32)>,
}

impl SetupSink for SetupVt {
    fn clear_defines(&mut self) {
        if let Some(f) = self.clear_defines {
            // SAFETY: table contract.
            unsafe { f(self.ctx) };
        }
    }

    fn define(&mut self, name: &str) {
        if let Some(f) = self.define {
            // SAFETY: table contract.
            unsafe { f(self.ctx, name.as_ptr(), name.len()) };
        }
    }

    fn define_value(&mut self, name: &str, value: f32) {
        if let Some(f) = self.define_value {
            // SAFETY: table contract.
            unsafe { f(self.ctx, name.as_ptr(), name.len(), value) };
        }
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
        let Some(f) = self.setup_program else { return };
        let feats: Vec<FeatureC> = features
            .iter()
            .map(|x| FeatureC {
                name: x.name.as_ptr(),
                stage: x.stage,
                required: x.required,
            })
            .collect();
        // SAFETY: table contract; `feats` and the strings outlive the call.
        unsafe {
            f(
                self.ctx,
                manager,
                mode,
                name.as_ptr(),
                name.len(),
                vtx.as_ptr(),
                vtx.len(),
                frag.as_ptr(),
                frag.len(),
                feats.as_ptr(),
                feats.len(),
            );
        }
    }

    fn var_id(&mut self, manager: i32, name: &str, id: i32, mode: i32) {
        if let Some(f) = self.var_id {
            // SAFETY: table contract.
            unsafe { f(self.ctx, manager, name.as_ptr(), name.len(), id, mode) };
        }
    }
}

/// Lazy queries into the engine.
#[repr(C)]
pub struct HostVt {
    ctx: *mut c_void,
    coverage: Option<unsafe extern "C" fn(*mut c_void) -> f32>,
    /// fills up to four entries, returns how many
    force_fields: Option<unsafe extern "C" fn(*mut c_void, *mut ForceFieldInfo) -> i32>,
    fields_in_box:
        Option<unsafe extern "C" fn(*mut c_void, *const Vector3f, *const Vector3f) -> i32>,
    world: Option<unsafe extern "C" fn(*mut c_void, *mut WorldInfo) -> bool>,
    object_bv: Option<unsafe extern "C" fn(*mut c_void, *mut Bv)>,
    light_count: Option<unsafe extern "C" fn(*mut c_void) -> i32>,
    light: Option<unsafe extern "C" fn(*mut c_void, i32, bool, *mut LightInfo) -> bool>,
    light_intersects: Option<unsafe extern "C" fn(*mut c_void, i32) -> bool>,
    /// writes min then max to the two vectors at `out`
    spot_clip: Option<unsafe extern "C" fn(*mut c_void, i32, *mut Vector3f) -> bool>,
}

impl Host for HostVt {
    fn coverage(&mut self) -> f32 {
        match self.coverage {
            // SAFETY: table contract.
            Some(f) => unsafe { f(self.ctx) },
            None => 1.0,
        }
    }

    fn force_fields(&mut self, out: &mut [ForceFieldInfo; 4]) -> usize {
        match self.force_fields {
            // SAFETY: table contract; `out` has room for four.
            Some(f) => usize::try_from(unsafe { f(self.ctx, out.as_mut_ptr()) }).unwrap_or(0),
            None => 0,
        }
    }

    fn fields_in_box(&mut self, min: Vector3f, max: Vector3f) -> usize {
        match self.fields_in_box {
            // SAFETY: table contract.
            Some(f) => usize::try_from(unsafe { f(self.ctx, &min, &max) }).unwrap_or(0),
            None => 0,
        }
    }

    fn world(&mut self) -> Option<WorldInfo> {
        let f = self.world?;
        let mut w = WorldInfo::default();
        // SAFETY: table contract.
        unsafe { f(self.ctx, &mut w) }.then_some(w)
    }

    fn object_bv(&mut self) -> Bv {
        let mut b = Bv::default();
        if let Some(f) = self.object_bv {
            // SAFETY: table contract.
            unsafe { f(self.ctx, &mut b) };
        }
        b
    }

    fn light_count(&mut self) -> usize {
        match self.light_count {
            // SAFETY: table contract.
            Some(f) => usize::try_from(unsafe { f(self.ctx) }).unwrap_or(0),
            None => 0,
        }
    }

    fn light(&mut self, i: usize, detail: bool) -> Option<LightInfo> {
        let f = self.light?;
        let mut l = LightInfo::default();
        // SAFETY: table contract.
        unsafe { f(self.ctx, i32::try_from(i).ok()?, detail, &mut l) }.then_some(l)
    }

    fn light_intersects(&mut self, i: usize) -> bool {
        match (self.light_intersects, i32::try_from(i)) {
            // SAFETY: table contract.
            (Some(f), Ok(i)) => unsafe { f(self.ctx, i) },
            _ => false,
        }
    }

    fn spot_clip(&mut self, i: usize) -> Option<(Vector3f, Vector3f)> {
        let f = self.spot_clip?;
        let mut out = [Vector3f::default(); 2];
        // SAFETY: table contract; `out` holds two vectors.
        unsafe { f(self.ctx, i32::try_from(i).ok()?, out.as_mut_ptr()) }.then(|| (out[0], out[1]))
    }
}

/// `AddVar*` entry as the shim reads it.
#[repr(C)]
pub struct VarDefC {
    name: *const u8,
    name_len: usize,
    desc: *const u8,
    desc_len: usize,
    is_float: bool,
    bool_value: bool,
    float_value: f32,
}

/// # Safety
/// `out` is null or valid for `cap` writes.
unsafe fn out_slice<'a, T>(out: *mut T, cap: usize) -> &'a mut [T] {
    if out.is_null() {
        return &mut [];
    }
    // SAFETY: caller contract.
    unsafe { std::slice::from_raw_parts_mut(out, cap) }
}

/// # Safety
/// `p` is null or valid for one write.
unsafe fn write<T>(p: *mut T, v: T) {
    if !p.is_null() {
        // SAFETY: caller contract.
        unsafe { p.write(v) };
    }
}

/// `mbIsTranslucent` (bit 0), `mbIsDecal` (bit 1) and `mbHasTypeSpecifics[mode]` (bit `8 + mode`).
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_mat_type_flags(kind: i32) -> u32 {
    Kind::from_i32(kind).map_or(0, super::type_flags)
}

/// Writes the `AddUsedTexture` list into `out` (up to `cap`) and returns its length.
///
/// # Safety
/// `out` is valid for `cap` writes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mat_used_textures(kind: i32, out: *mut i32, cap: usize) -> usize {
    let list = Kind::from_i32(kind)
        .map(super::used_textures)
        .unwrap_or_default();
    // SAFETY: caller contract.
    let out = unsafe { out_slice(out, cap) };
    for (d, s) in out.iter_mut().zip(&list) {
        *d = i32::try_from(*s).unwrap_or(0);
    }
    list.len()
}

/// The `i`th `AddVar*` call of the type; false past the end.
///
/// # Safety
/// `out` is valid for one write.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mat_user_var(kind: i32, i: usize, out: *mut VarDefC) -> bool {
    let Some(def) = Kind::from_i32(kind)
        .map(super::user_vars)
        .and_then(|v| v.get(i))
    else {
        return false;
    };
    let (is_float, b, f) = match def.default {
        VarDefault::Bool(b) => (false, b, 0.0),
        VarDefault::Float(f) => (true, false, f),
    };
    // SAFETY: caller contract.
    unsafe {
        write(
            out,
            VarDefC {
                name: def.name.as_ptr(),
                name_len: def.name.len(),
                desc: def.desc.as_ptr(),
                desc_len: def.desc.len(),
                is_float,
                bool_value: b,
                float_value: f,
            },
        );
    }
    true
}

/// Writes the constructor defaults of the type's vars into `vars`.
///
/// # Safety
/// `vars` is valid for a write of the vars struct of `kind`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mat_default_vars(kind: i32, vars: *mut c_void) {
    let Some(kind) = Kind::from_i32(kind) else {
        return;
    };
    // SAFETY: caller contract; the pointee type is the one the kind names.
    unsafe {
        match kind {
            Kind::SolidDiffuse => write(vars.cast(), super::default_solid()),
            Kind::ProjectedUv => write(vars.cast(), super::default_projected_uv()),
            Kind::VertexBlend => write(vars.cast(), super::default_vertex_blend()),
            Kind::Translucent => write(vars.cast(), super::default_translucent()),
            Kind::Water => write(vars.cast(), super::default_water()),
            Kind::TerrainBlend => write(vars.cast(), super::default_terrain_blend()),
            Kind::Undergrowth => write(vars.cast(), super::default_undergrowth()),
            Kind::Decal => {}
        }
    }
}

/// `LoadVariables`: reads the `.mat` variables into `vars` (created by the shim) and reports the
/// `cMaterial` settings some types take.
///
/// # Safety
/// `vars` is valid for reads and writes of the vars struct of `kind`; `api` and `out` are valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mat_load_vars(
    kind: i32,
    vars: *mut c_void,
    api: *mut VarsVt,
    out: *mut LoadOut,
) {
    let (Some(kind), false, false) = (Kind::from_i32(kind), vars.is_null(), api.is_null()) else {
        return;
    };
    let mut lo = LoadOut::NONE;
    // SAFETY: caller contract.
    unsafe {
        let api = &mut *api;
        match kind {
            Kind::SolidDiffuse => super::load_solid(&mut *vars.cast(), api),
            Kind::ProjectedUv => super::load_projected_uv(&mut *vars.cast(), api),
            Kind::VertexBlend => super::load_vertex_blend(&mut *vars.cast(), api),
            Kind::Translucent => super::load_translucent(&mut *vars.cast(), api, &mut lo),
            Kind::Water => super::load_water(&mut *vars.cast(), api, &mut lo),
            Kind::Decal | Kind::TerrainBlend | Kind::Undergrowth => {}
        }
        write(out, lo);
    }
}

/// `GetVariableValues`; the two flags are the material's (`cMaterial`) values of water.
///
/// # Safety
/// `vars` is null or valid; `api` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mat_var_values(
    kind: i32,
    vars: *const c_void,
    affected_by_dof: bool,
    occlusion_test: bool,
    large_surface: bool,
    api: *mut VarsVt,
) {
    let Some(kind) = Kind::from_i32(kind) else {
        return;
    };
    // SAFETY: caller contract.
    unsafe {
        let Some(api) = api.as_mut() else { return };
        match vars_of(kind, vars) {
            Vars::Solid(v) => super::solid_values(v, api),
            Vars::Translucent(v) => super::translucent_values(v, affected_by_dof, api),
            Vars::Water(v) => super::water_values(v, occlusion_test, large_surface, api),
            _ => {}
        }
    }
}

/// `LoadData`'s program manager setup (`global`: the solid types' shared Z programs).
///
/// # Safety
/// `g` and `sink` are valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mat_setup_programs(
    kind: i32,
    global: bool,
    g: *const Globals,
    sink: *mut SetupVt,
) {
    // SAFETY: caller contract.
    let (Some(kind), Some(g), Some(sink)) = (Kind::from_i32(kind), unsafe { read(g) }, unsafe {
        sink.as_mut()
    }) else {
        return;
    };
    super::setup_programs(kind, global, g, sink);
}

/// # Safety
/// `view`, `g` and `out` are valid; `vars` is null or valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mat_texture_unit(
    kind: i32,
    view: *const MatView,
    vars: *const c_void,
    g: *const Globals,
    render_mode: i32,
    unit: i32,
    out: *mut TexSel,
) {
    // SAFETY: caller contract.
    unsafe {
        let (Some(kind), Some(view), Some(g)) = (Kind::from_i32(kind), read(view), read(g)) else {
            return write(out, TexSel::NONE);
        };
        write(
            out,
            super::texture_unit(kind, view, vars_of(kind, vars), g, render_mode, unit),
        );
    }
}

/// # Safety
/// `view`, `g` and `out` are valid; `vars` is null or valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mat_program(
    kind: i32,
    view: *const MatView,
    vars: *const c_void,
    g: *const Globals,
    render_mode: i32,
    skeleton: i32,
    out: *mut ProgSel,
) {
    // SAFETY: caller contract.
    unsafe {
        let (Some(kind), Some(view), Some(g)) = (Kind::from_i32(kind), read(view), read(g)) else {
            return write(out, ProgSel::NONE);
        };
        write(
            out,
            super::program(
                kind,
                view,
                vars_of(kind, vars),
                g,
                render_mode,
                skeleton != 0,
            ),
        );
    }
}

/// # Safety
/// `out` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mat_destroy_target(
    kind: i32,
    render_mode: i32,
    blend_mode: i32,
    out: *mut ProgSel,
) {
    let sel = Kind::from_i32(kind).map_or(ProgSel::NONE, |k| {
        super::destroy_target(k, render_mode, blend_mode)
    });
    // SAFETY: caller contract.
    unsafe { write(out, sel) };
}

/// # Safety
/// `g` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mat_render_program_slot(
    kind: i32,
    render_mode: i32,
    g: *const Globals,
) -> i32 {
    // SAFETY: caller contract.
    match (Kind::from_i32(kind), unsafe { read(g) }) {
        (Some(k), Some(g)) => super::render_program_slot(k, render_mode, g),
        _ => 0,
    }
}

/// Undergrowth's `GetRenderProgram`; `fields` receives the number of force fields in range.
///
/// # Safety
/// `vars`, `r`, `host`, `out` and `fields` are valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mat_undergrowth_render_program(
    vars: *const UndergrowthVars,
    render_mode: i32,
    r: *const RenderView,
    host: *mut HostVt,
    out: *mut ProgSel,
    fields: *mut usize,
) {
    // SAFETY: caller contract.
    unsafe {
        let (Some(vars), Some(r), Some(host)) = (read(vars), read(r), host.as_mut()) else {
            write(out, ProgSel::NONE);
            return write(fields, 0);
        };
        let (sel, n) = super::undergrowth_render_program(vars, render_mode, r, host);
        write(out, sel);
        write(fields, n);
    }
}

/// # Safety
/// `view`, `g` and `out` are valid; `vars` is null or valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mat_compile(
    kind: i32,
    view: *const MatView,
    vars: *const c_void,
    g: *const Globals,
    out: *mut CompileOut,
) {
    // SAFETY: caller contract.
    unsafe {
        let (Some(kind), Some(view), Some(g)) = (Kind::from_i32(kind), read(view), read(g)) else {
            return write(out, CompileOut::NONE);
        };
        write(out, super::compile(kind, view, vars_of(kind, vars), g));
    }
}

/// # Safety
/// `r` is valid; `out` is valid for `cap` writes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mat_setup_type(
    kind: i32,
    render_mode: i32,
    r: *const RenderView,
    out: *mut Uniform,
    cap: usize,
) -> usize {
    // SAFETY: caller contract.
    let (Some(kind), Some(r), buf) = (Kind::from_i32(kind), unsafe { read(r) }, unsafe {
        out_slice(out, cap)
    }) else {
        return 0;
    };
    let mut u = Uniforms::new(buf);
    super::setup_type(kind, render_mode, r, &mut u);
    u.len()
}

/// # Safety
/// `view`, `g`, `r` and `host` are valid; `vars` is null or valid; `out` is valid for `cap`
/// writes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mat_setup_material(
    kind: i32,
    render_mode: i32,
    view: *const MatView,
    vars: *const c_void,
    g: *const Globals,
    r: *const RenderView,
    field_num: usize,
    host: *mut HostVt,
    out: *mut Uniform,
    cap: usize,
) -> usize {
    // SAFETY: caller contract.
    unsafe {
        let (Some(kind), Some(view), Some(g), Some(r), Some(host)) = (
            Kind::from_i32(kind),
            read(view),
            read(g),
            read(r),
            host.as_mut(),
        ) else {
            return 0;
        };
        let mut u = Uniforms::new(out_slice(out, cap));
        super::setup_material(
            kind,
            render_mode,
            view,
            vars_of(kind, vars),
            g,
            r,
            field_num,
            host,
            &mut u,
        );
        u.len()
    }
}

/// # Safety
/// `view`, `g`, `r`, `obj` and `host` are valid; `vars` is null or valid; `out` is valid for
/// `cap` writes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mat_setup_object(
    kind: i32,
    render_mode: i32,
    view: *const MatView,
    vars: *const c_void,
    g: *const Globals,
    r: *const RenderView,
    obj: *const ObjView,
    host: *mut HostVt,
    out: *mut Uniform,
    cap: usize,
) -> usize {
    // SAFETY: caller contract.
    unsafe {
        let (Some(kind), Some(view), Some(g), Some(r), Some(obj), Some(host)) = (
            Kind::from_i32(kind),
            read(view),
            read(g),
            read(r),
            read(obj),
            host.as_mut(),
        ) else {
            return 0;
        };
        let mut u = Uniforms::new(out_slice(out, cap));
        super::setup_object(
            kind,
            render_mode,
            view,
            vars_of(kind, vars),
            g,
            r,
            obj,
            host,
            &mut u,
        );
        u.len()
    }
}

/// The UV matrix of `n` animations at `time`.
///
/// # Safety
/// `anims` is valid for `n` reads when `n > 0`; `out` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mat_uv_matrix(
    anims: *const UvAnim,
    n: usize,
    time: f32,
    out: *mut Matrixf,
) {
    let list: &[UvAnim] = if anims.is_null() || n == 0 {
        &[]
    } else {
        // SAFETY: caller contract.
        unsafe { std::slice::from_raw_parts(anims, n) }
    };
    // SAFETY: caller contract.
    unsafe { write(out, uv_matrix(list, time)) };
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_mat_world_fog_flags(underwater: bool, secondary: bool) -> u32 {
    super::world_fog_flags(underwater, secondary)
}
