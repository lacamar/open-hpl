//! `decal`/`terraindecal`, `terrainblend` and `undergrowth`: the small material types.

use super::common::{FORCE_FIELD_NAMES, force_field_vars};
use super::setup::{Feature, SetupSink, feature};
use super::types::{
    CompileOut, Host, MatView, ProgSel, RenderView, TexSel, Uniforms, bit, manager, mode, tex,
    texsel,
};
use super::vars::{TerrainBlendVars, UndergrowthVars};
use crate::types::{Vector2f, Vector3f};

const VERTEX: u32 = bit(0);
const FRAGMENT: u32 = bit(1);

mod decal_var {
    pub const UV: i32 = 0;
}

static DECAL_FEATURES: [Feature; 1] = [feature(c"UseUvAnimation", VERTEX, 0)];

/// `cMaterialType_Decal::LoadData`
pub fn setup_decal(s: &mut dyn SetupSink) {
    s.clear_defines();
    s.define("UseUv");
    s.define("UseColor");
    s.define("MaterialType_Decal");
    s.setup_program(
        manager::OWN,
        mode::DIFFUSE,
        "Diffuse",
        "deferred_base_vtx.glsl",
        "deferred_decal_frag.glsl",
        &DECAL_FEATURES,
    );
    s.var_id(manager::OWN, "a_mtxUV", decal_var::UV, mode::DIFFUSE);
}

/// Programs of the decal-like types are destroyed in the mode they were made for.
pub fn destroy_target(render_mode: i32) -> ProgSel {
    ProgSel::own(render_mode, 0)
}

pub fn decal_texture(v: &MatView, render_mode: i32, unit: i32) -> TexSel {
    if render_mode == mode::DIFFUSE && unit == 0 {
        TexSel::slot_if(v, tex::DIFFUSE)
    } else {
        TexSel::NONE
    }
}

pub fn decal_program(v: &MatView, render_mode: i32) -> ProgSel {
    if render_mode == mode::DIFFUSE {
        ProgSel::own(mode::DIFFUSE, if v.has_uv_anim { bit(0) } else { 0 })
    } else {
        ProgSel::NONE
    }
}

pub fn decal_setup_material(render_mode: i32, v: &MatView, out: &mut Uniforms) {
    if render_mode == mode::DIFFUSE && v.has_uv_anim {
        out.mat(decal_var::UV, &v.uv_matrix);
    }
}

pub fn decal_compile(v: &MatView) -> CompileOut {
    let mut c = CompileOut::NONE;
    if v.has_uv_anim {
        c.specific(mode::DIFFUSE, true);
    }
    c
}

static TERRAIN_FEATURES: [Feature; 2] = [
    feature(c"UseBaseTexture", FRAGMENT, 0),
    feature(c"UseVertexPosition", VERTEX | FRAGMENT, 0),
];

/// `cMaterialType_TerrainBlend::LoadData` without the white texture.
pub fn setup_terrain_blend(s: &mut dyn SetupSink) {
    s.clear_defines();
    s.define("UseUv");
    s.define("UseBlendMul");
    s.setup_program(
        manager::OWN,
        mode::DIFFUSE,
        "Diffuse",
        "deferred_base_vtx.glsl",
        "cache_terrain_diffuse_frag.glsl",
        &TERRAIN_FEATURES,
    );
    let d = mode::DIFFUSE;
    let o = manager::OWN;
    s.var_id(o, "avTextureCoordScale", 0, d);
    s.var_id(o, "avOneMinusFadeStart", 1, d);
    s.var_id(o, "afBaseTextureCoordScale", 2, d);
    s.var_id(o, "avDetailMapScale", 3, d);
    s.var_id(o, "avDetailFade", 4, d);
    s.var_id(o, "avBaseDetailAmount", 5, d);
    for (i, name) in [
        "avDetailAmount0",
        "avDetailAmount1",
        "avDetailAmount2",
        "avDetailAmount3",
    ]
    .into_iter()
    .enumerate()
    {
        s.var_id(o, name, 6 + i32::try_from(i).unwrap_or(0), d);
    }
    s.var_id(o, "avBlendMul", 10, d);
}

pub fn terrain_blend_texture(v: &MatView, render_mode: i32, unit: i32) -> TexSel {
    let Ok(slot) = u32::try_from(unit) else {
        return TexSel::NONE;
    };
    if render_mode != mode::DIFFUSE || unit > 12 {
        return TexSel::NONE;
    }
    if v.has(slot) {
        TexSel::slot(slot)
    } else {
        TexSel::special(texsel::WHITE)
    }
}

pub fn terrain_blend_program(v: &MatView, render_mode: i32) -> ProgSel {
    if render_mode != mode::DIFFUSE {
        return ProgSel::NONE;
    }
    let mut f = 0;
    if v.has(tex::ILLUMINATION) {
        f |= bit(0);
    }
    if v.has(tex::DETAIL_NMAP) {
        f |= bit(1);
    }
    ProgSel::own(mode::DIFFUSE, f)
}

pub fn terrain_blend_setup_material(vars: &TerrainBlendVars, out: &mut Uniforms) {
    let [s0, s1, s2, s3] = vars.texture_coord_scale;
    let [f0, f1, f2, f3] = vars.one_minus_fade_start;
    out.vec4(0, s0, s1, s2, s3);
    out.vec4(1, f0, f1, f2, f3);
    out.float(2, vars.base_texture_coord_scale);
    out.vec3(3, vars.detail_map_scale);
    out.vec3(4, vars.detail_fade);
    out.vec3(5, vars.base_detail_amount);
    for (id, a) in (6..).zip(vars.detail_amount) {
        out.vec3(id, a);
    }
    let [b0, b1, b2, b3] = vars.blend_mul;
    out.vec4(10, b0, b1, b2, b3);
}

pub fn terrain_blend_compile() -> CompileOut {
    let mut c = CompileOut::NONE;
    c.specific(mode::DIFFUSE, true);
    c
}

mod ug {
    pub const INV_FAR_PLANE: i32 = 0;
    pub const T: i32 = 1;
    pub const DISSOLVE: i32 = 2;
    pub const WIND_PROPERTIES: i32 = 3;
    pub const WIND_OCTAVES_MUL: i32 = 4;
    pub const FORCE_FIELD_FIRST: i32 = 5;
    pub const FORCE_FIELD_POS: i32 = 14;
    pub const FORCE_FIELD_PROP: i32 = 15;
}

static UNDERGROWTH_FEATURES: [Feature; 3] = [
    feature(c"UseWind", VERTEX, 0),
    feature(c"UseSingleForceField", VERTEX, 0),
    feature(c"UseFourForceFields", VERTEX, 0),
];

/// `cMaterialType_Undergrowth::LoadData`
pub fn setup_undergrowth(s: &mut dyn SetupSink) {
    s.clear_defines();
    for (m, name) in [(mode::Z, "Z"), (mode::DIFFUSE, "Diffuse")] {
        s.setup_program(
            manager::OWN,
            m,
            name,
            "deferred_undergrowth_gbuffer_vtx.glsl",
            "deferred_undergrowth_gbuffer_frag.glsl",
            &UNDERGROWTH_FEATURES,
        );
        let o = manager::OWN;
        s.var_id(o, "afInvFarPlane", ug::INV_FAR_PLANE, m);
        s.var_id(o, "afT", ug::T, m);
        s.var_id(o, "avDissolveStartSizeDepth", ug::DISSOLVE, m);
        s.var_id(o, "avWindProperties", ug::WIND_PROPERTIES, m);
        s.var_id(o, "avWindOctavesMul", ug::WIND_OCTAVES_MUL, m);
        for (id, name) in (ug::FORCE_FIELD_FIRST..).zip(FORCE_FIELD_NAMES) {
            s.var_id(o, name, id, m);
        }
        s.var_id(o, "avForceFieldPos", ug::FORCE_FIELD_POS, m);
        s.var_id(o, "avForceFieldProp", ug::FORCE_FIELD_PROP, m);
    }
}

fn undergrowth_mode(render_mode: i32) -> bool {
    render_mode == mode::Z || render_mode == mode::DIFFUSE
}

pub fn undergrowth_texture(v: &MatView, render_mode: i32, unit: i32) -> TexSel {
    if undergrowth_mode(render_mode) && unit == 0 {
        TexSel::slot_if(v, tex::DIFFUSE)
    } else {
        TexSel::NONE
    }
}

pub fn undergrowth_program(vars: &UndergrowthVars, render_mode: i32) -> ProgSel {
    if !undergrowth_mode(render_mode) {
        return ProgSel::NONE;
    }
    ProgSel::own(render_mode, if vars.wind { bit(0) } else { 0 })
}

/// Rebirth's SetupUndergrowth: single/four field variant from the fields within fade range of
/// the camera. Returns the program and the number of fields the host found.
pub fn undergrowth_render_program(
    vars: &UndergrowthVars,
    render_mode: i32,
    r: &RenderView,
    host: &mut dyn Host,
) -> (ProgSel, usize) {
    if !undergrowth_mode(render_mode) {
        return (ProgSel::NONE, 0);
    }
    let mut n = 0;
    if vars.force_field_mul > 0.0 {
        let cam = r.origin;
        let range = vars.dissolve.x + vars.dissolve.y;
        let min = Vector3f {
            x: cam.x - range,
            y: cam.y - range,
            z: cam.z - range,
        };
        let max = Vector3f {
            x: cam.x + range,
            y: cam.y + range,
            z: cam.z + range,
        };
        n = host.fields_in_box(min, max);
    }
    let field_bits = if n == 1 {
        bit(1)
    } else if n > 1 {
        bit(2)
    } else {
        0
    };
    let flags = (if vars.wind { bit(0) } else { 0 }) | field_bits;
    (ProgSel::own(render_mode, flags), n)
}

pub fn undergrowth_setup_type(r: &RenderView, out: &mut Uniforms) {
    out.float(ug::INV_FAR_PLANE, 1.0f32 / r.far);
    out.float(ug::T, r.time);
}

pub fn undergrowth_setup_material(
    vars: &UndergrowthVars,
    field_num: usize,
    host: &mut dyn Host,
    out: &mut Uniforms,
) {
    let Vector2f { x, y } = vars.dissolve;
    out.vec2(ug::DISSOLVE, x, y);
    out.vec3(ug::WIND_PROPERTIES, vars.wind_vec);
    out.vec3(ug::WIND_OCTAVES_MUL, vars.wind_octaves);
    let mut fields = [Default::default(); 4];
    let n = host.force_fields(&mut fields).min(field_num).min(4);
    let fields = fields.get(..n).unwrap_or(&[]);
    if field_num == 1 {
        if let Some(f) = fields.first() {
            out.vec4(ug::FORCE_FIELD_POS, f.pos.x, f.pos.y, f.pos.z, f.t);
            out.vec4(
                ug::FORCE_FIELD_PROP,
                f.falloff_start,
                f.radius - f.falloff_start,
                f.force * vars.force_field_mul,
                vars.force_field_mul,
            );
        }
    } else if field_num > 1 {
        force_field_vars(
            out,
            ug::FORCE_FIELD_FIRST,
            fields,
            vars.force_field_mul,
            vars.max_force_field_force,
        );
    }
}

pub fn undergrowth_compile() -> CompileOut {
    let mut c = CompileOut::NONE;
    c.specific(mode::Z, true);
    c.specific(mode::DIFFUSE, true);
    c
}
