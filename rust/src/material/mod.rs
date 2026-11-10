//! HPL2 material types: variable loading and defaults, program feature selection, texture unit
//! assignment, `CompileMaterialSpecifics` settings and per-pass uniform values
//! (`iMaterialType` and its subclasses, `cMaterial` UV animation).
//!
//! The `iMaterialType` classes stay C++ shims (the renderer calls their virtuals). A shim passes
//! plain views of the material, renderer and object in and applies what comes back: a program
//! choice, a texture choice or a list of `iGpuProgram::Set*` calls. Queries that cost something
//! (force fields, lights, coverage, world fog) go through [`Host`] and run only when the original
//! code ran them. Floats match the gcc build bit for bit, including its fused multiply-adds.

mod anim;
mod common;
mod decal;
mod ffi;
mod probe;
mod setup;
mod solid;
mod translucent;
mod types;
mod vars;
mod water;

#[cfg(test)]
mod tests;

pub use anim::{UvAnim, uv_matrix};
pub use setup::{Feature, SetupSink};
pub use types::{
    Bv, CompileOut, ForceFieldInfo, Globals, Host, Kind, LightInfo, MatView, ObjView, ProgSel,
    RenderView, TexSel, Uniform, Uniforms, WorldInfo,
};
pub use vars::{
    LoadOut, ProjectedUvVars, SolidVars, TerrainBlendVars, TranslucentVars, UndergrowthVars,
    VarDef, VarDefault, VarSink, VarSource, VertexBlendVars, WaterVars,
};

use types::{mode, tex};

/// A material's type-specific variables as the shim hands them in.
#[derive(Clone, Copy)]
pub enum Vars<'a> {
    None,
    Solid(&'a SolidVars),
    ProjectedUv(&'a ProjectedUvVars),
    VertexBlend(&'a VertexBlendVars),
    Translucent(&'a TranslucentVars),
    Water(&'a WaterVars),
    TerrainBlend(&'a TerrainBlendVars),
    Undergrowth(&'a UndergrowthVars),
}

const TRANSLUCENT_FLAG: u32 = 1;
const DECAL_FLAG: u32 = 2;
const TYPE_SPECIFICS_SHIFT: u32 = 8;

/// `mbIsTranslucent`, `mbIsDecal` and `mbHasTypeSpecifics[mode]` (bit `8 + mode`).
pub fn type_flags(kind: Kind) -> u32 {
    let specifics = |modes: &[i32]| {
        modes.iter().fold(0u32, |a, m| {
            a | (1 << (TYPE_SPECIFICS_SHIFT + m.unsigned_abs()))
        })
    };
    match kind {
        Kind::SolidDiffuse | Kind::ProjectedUv | Kind::VertexBlend => specifics(&[mode::DIFFUSE]),
        Kind::Translucent => {
            TRANSLUCENT_FLAG
                | specifics(&[
                    mode::DIFFUSE,
                    mode::DIFFUSE_FOG,
                    mode::ILLUMINATION,
                    mode::ILLUMINATION_FOG,
                ])
        }
        Kind::Water => TRANSLUCENT_FLAG | specifics(&[mode::DIFFUSE, mode::DIFFUSE_FOG]),
        Kind::Decal | Kind::TerrainBlend => {
            TRANSLUCENT_FLAG | DECAL_FLAG | specifics(&[mode::DIFFUSE])
        }
        Kind::Undergrowth => specifics(&[mode::Z, mode::DIFFUSE]),
    }
}

/// `AddUsedTexture` calls of the type's constructor, in order.
pub fn used_textures(kind: Kind) -> Vec<u32> {
    match kind {
        Kind::SolidDiffuse => vec![
            tex::DIFFUSE,
            tex::NMAP,
            tex::ALPHA,
            tex::SPECULAR,
            tex::HEIGHT,
            tex::ILLUMINATION,
            tex::DISSOLVE_ALPHA,
            tex::CUBE_MAP,
            tex::CUBE_MAP_ALPHA,
            tex::DETAIL_DIFFUSE,
            tex::DETAIL_NMAP,
            tex::TRANSLUCENCY,
            tex::LIQUID,
        ],
        Kind::ProjectedUv => (tex::DIFFUSE_SIDE..=tex::SPECULAR_BOTTOM).collect(),
        Kind::VertexBlend => (tex::DIFFUSE_R..=tex::HEIGHT_B).collect(),
        Kind::Translucent => vec![tex::DIFFUSE, tex::NMAP, tex::CUBE_MAP, tex::CUBE_MAP_ALPHA],
        Kind::Water => vec![tex::DIFFUSE, tex::NMAP, tex::CUBE_MAP],
        Kind::Decal => vec![tex::DIFFUSE],
        Kind::TerrainBlend | Kind::Undergrowth => Vec::new(),
    }
}

pub use vars::user_vars;

/// `LoadData`: tells the type's program managers what to generate.
/// `global` selects the shared Z programs of the solid types instead.
pub fn setup_programs(kind: Kind, global: bool, g: &Globals, s: &mut dyn SetupSink) {
    if global {
        solid::setup_global(s);
        return;
    }
    match kind {
        Kind::SolidDiffuse => solid::setup_diffuse(s, g),
        Kind::ProjectedUv => solid::setup_projected_uv(s),
        Kind::VertexBlend => solid::setup_vertex_blend(s),
        Kind::Translucent => translucent::setup(s, g),
        Kind::Water => water::setup(s, g),
        Kind::Decal => decal::setup_decal(s),
        Kind::TerrainBlend => decal::setup_terrain_blend(s),
        Kind::Undergrowth => decal::setup_undergrowth(s),
    }
}

/// The bits `cMaterialType_Translucent::SetWorldFog` stores.
pub fn world_fog_flags(underwater: bool, secondary: bool) -> u32 {
    translucent::world_fog_flags(underwater, secondary)
}

/// Where a program made for `render_mode` is destroyed: manager and main mode.
pub fn destroy_target(kind: Kind, render_mode: i32, blend_mode: i32) -> ProgSel {
    match kind {
        Kind::SolidDiffuse | Kind::ProjectedUv | Kind::VertexBlend => {
            solid::destroy_target(render_mode)
        }
        Kind::Translucent => translucent::destroy_target(render_mode, blend_mode),
        Kind::Water => water::destroy_target(),
        Kind::Decal | Kind::TerrainBlend | Kind::Undergrowth => decal::destroy_target(render_mode),
    }
}

pub fn texture_unit(
    kind: Kind,
    v: &MatView,
    vars: Vars,
    g: &Globals,
    render_mode: i32,
    unit: i32,
) -> TexSel {
    match (kind, vars) {
        (Kind::SolidDiffuse, Vars::Solid(x)) => solid::diffuse_texture(v, x, g, render_mode, unit),
        (Kind::ProjectedUv, _) => solid::projected_uv_texture(v, render_mode, unit),
        (Kind::VertexBlend, _) => solid::vertex_blend_texture(v, render_mode, unit),
        (Kind::Translucent, Vars::Translucent(x)) => {
            translucent::texture(v, x, g, render_mode, unit)
        }
        (Kind::Water, Vars::Water(x)) => water::texture(v, x, g, render_mode, unit),
        (Kind::Decal, _) => decal::decal_texture(v, render_mode, unit),
        (Kind::TerrainBlend, _) => decal::terrain_blend_texture(v, render_mode, unit),
        (Kind::Undergrowth, _) => decal::undergrowth_texture(v, render_mode, unit),
        _ => TexSel::NONE,
    }
}

/// `skeleton` is the program slot (0, or 1 for the depth of field variant of translucent types).
pub fn program(
    kind: Kind,
    v: &MatView,
    vars: Vars,
    g: &Globals,
    render_mode: i32,
    skeleton: bool,
) -> ProgSel {
    match (kind, vars) {
        (Kind::SolidDiffuse, Vars::Solid(x)) => solid::diffuse_program(v, x, g, render_mode),
        (Kind::ProjectedUv, Vars::ProjectedUv(x)) => solid::projected_uv_program(v, x, render_mode),
        (Kind::VertexBlend, Vars::VertexBlend(x)) => solid::vertex_blend_program(v, x, render_mode),
        (Kind::Translucent, Vars::Translucent(x)) => {
            translucent::program(v, x, g, render_mode, skeleton)
        }
        (Kind::Water, Vars::Water(x)) => water::program(v, x, g, render_mode),
        (Kind::Decal, _) => decal::decal_program(v, render_mode),
        (Kind::TerrainBlend, _) => decal::terrain_blend_program(v, render_mode),
        (Kind::Undergrowth, Vars::Undergrowth(x)) => decal::undergrowth_program(x, render_mode),
        _ => ProgSel::NONE,
    }
}

/// The program slot `GetRenderProgram` reads for translucent types; the others use slot 0.
pub fn render_program_slot(kind: Kind, render_mode: i32, g: &Globals) -> i32 {
    match kind {
        Kind::Translucent => translucent::render_program_slot(render_mode, g),
        _ => 0,
    }
}

/// Undergrowth's `GetRenderProgram`: program and the number of force fields in range.
pub fn undergrowth_render_program(
    vars: &UndergrowthVars,
    render_mode: i32,
    r: &RenderView,
    host: &mut dyn Host,
) -> (ProgSel, usize) {
    decal::undergrowth_render_program(vars, render_mode, r, host)
}

/// `CompileMaterialSpecifics`
pub fn compile(kind: Kind, v: &MatView, vars: Vars, g: &Globals) -> CompileOut {
    match (kind, vars) {
        (Kind::SolidDiffuse, Vars::Solid(x)) => solid::diffuse_compile(v, x, g),
        (Kind::ProjectedUv, _) | (Kind::VertexBlend, _) => solid::blend_compile(v),
        (Kind::Translucent, Vars::Translucent(x)) => translucent::compile(v, x, g),
        (Kind::Water, Vars::Water(x)) => water::compile(v, x, g),
        (Kind::Decal, _) => decal::decal_compile(v),
        (Kind::TerrainBlend, _) => decal::terrain_blend_compile(),
        (Kind::Undergrowth, _) => decal::undergrowth_compile(),
        _ => CompileOut::NONE,
    }
}

pub fn setup_type(kind: Kind, render_mode: i32, r: &RenderView, out: &mut Uniforms) {
    match kind {
        Kind::SolidDiffuse => solid::diffuse_setup_type(render_mode, r, out),
        Kind::ProjectedUv => solid::projected_uv_setup_type(render_mode, r, out),
        Kind::VertexBlend => solid::vertex_blend_setup_type(render_mode, r, out),
        Kind::Translucent => translucent::setup_type(render_mode, r, out),
        Kind::Undergrowth => decal::undergrowth_setup_type(r, out),
        Kind::Water | Kind::Decal | Kind::TerrainBlend => {}
    }
}

/// `field_num` is the undergrowth type's cached force field count.
#[allow(clippy::too_many_arguments)]
pub fn setup_material(
    kind: Kind,
    render_mode: i32,
    v: &MatView,
    vars: Vars,
    g: &Globals,
    r: &RenderView,
    field_num: usize,
    host: &mut dyn Host,
    out: &mut Uniforms,
) {
    match (kind, vars) {
        (Kind::SolidDiffuse, Vars::Solid(x)) => {
            solid::diffuse_setup_material(render_mode, v, x, g, r, out)
        }
        (Kind::ProjectedUv, Vars::ProjectedUv(x)) => {
            solid::projected_uv_setup_material(render_mode, x, out)
        }
        (Kind::VertexBlend, Vars::VertexBlend(x)) => {
            solid::vertex_blend_setup_material(render_mode, v, x, r, out)
        }
        (Kind::Translucent, Vars::Translucent(x)) => {
            translucent::setup_material(render_mode, v, x, g, r, host, out)
        }
        (Kind::Water, Vars::Water(x)) => water::setup_material(render_mode, v, x, g, r, host, out),
        (Kind::Decal, _) => decal::decal_setup_material(render_mode, v, out),
        (Kind::TerrainBlend, Vars::TerrainBlend(x)) => decal::terrain_blend_setup_material(x, out),
        (Kind::Undergrowth, Vars::Undergrowth(x)) => {
            decal::undergrowth_setup_material(x, field_num, host, out)
        }
        _ => {}
    }
}

#[allow(clippy::too_many_arguments)]
pub fn setup_object(
    kind: Kind,
    render_mode: i32,
    v: &MatView,
    vars: Vars,
    g: &Globals,
    r: &RenderView,
    o: &ObjView,
    host: &mut dyn Host,
    out: &mut Uniforms,
) {
    match (kind, vars) {
        (Kind::SolidDiffuse, Vars::Solid(x)) => {
            solid::diffuse_setup_object(render_mode, v, x, g, r, o, host, out)
        }
        (Kind::ProjectedUv, Vars::ProjectedUv(x)) => {
            solid::projected_uv_setup_object(render_mode, x, r, o, host, out)
        }
        (Kind::VertexBlend, _) => solid::vertex_blend_setup_object(render_mode, o, host, out),
        (Kind::Translucent, Vars::Translucent(x)) => {
            translucent::setup_object(render_mode, x, g, r, o, host, out)
        }
        (Kind::Water, _) => water::setup_object(o, out),
        _ => {}
    }
}

/// `user_vars` defaults that are not part of the per-type tables.
pub fn default_solid() -> SolidVars {
    SolidVars::default()
}

pub use vars::{
    default_projected_uv, default_terrain_blend, default_translucent, default_undergrowth,
    default_vertex_blend, default_water, load_projected_uv, load_solid, load_translucent,
    load_vertex_blend, load_water, solid_values, translucent_values, water_values,
};
