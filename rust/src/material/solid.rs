//! `iMaterialType_SolidBase` and its three types: `soliddiffuse`/`terrain`, `projecteduv`,
//! `vertexblend`.

use super::common::{
    FORCE_FIELD_NAMES, color_mul, color_scale, fmax, fmod, force_field_vars, inv_view_rotation,
    model_or_identity, view_mul,
};
use super::setup::{Feature, SetupSink, feature};
use super::types::{
    CompileOut, Globals, Host, MatView, ObjView, ProgSel, RenderView, TexSel, Uniforms, alpha_mode,
    bit, manager, mode, tex, texsel,
};
use super::vars::{
    LIQUID_BLEND_ALPHA, LIQUID_BLEND_MUL, ProjectedUvVars, SolidVars, VertexBlendVars,
};
use crate::math::{IDENTITY, matrix_inverse, matrix_mul_3x3, v3};
use crate::types::Vector3f;

mod var {
    pub const INV_FAR_PLANE: i32 = 0;
    pub const HEIGHT_MAP_SCALE_AND_BIAS: i32 = 1;
    pub const UV: i32 = 2;
    pub const COLOR_MUL_F: i32 = 3;
    pub const DISSOLVE_AMOUNT: i32 = 4;
    pub const FRENSEL_BIAS_POW: i32 = 5;
    pub const INV_VIEW_ROTATION: i32 = 6;
    pub const COLOR_MUL: i32 = 7;
    pub const ILLUMINATION_MUL: i32 = 8;
    pub const T: i32 = 9;
    pub const SWAY_PROPERTIES: i32 = 10;
    pub const SWAY_OCTAVES_MUL: i32 = 11;
    pub const SWAY_Y_FREQ_MUL: i32 = 12;
    pub const SWAY_SINGLE_DIRECTION: i32 = 13;
    pub const SWAY_SINGLE_SAMPLE_DIRECTION: i32 = 14;
    pub const MODEL: i32 = 15;
    pub const FAR_PLANE: i32 = 16;
    pub const DETAIL_PROPERTIES: i32 = 17;
    pub const DETAIL_WEIGHTS: i32 = 18;
    pub const NORMAL_FRAG: i32 = 19;
    pub const BLEND_HARDNESS: i32 = 20;
    pub const BLEND_HARDNESS_VTX: i32 = 21;
    pub const NORMAL_MAP_BLEND_IMPACT: i32 = 22;
    pub const TEXTURE_SCALE: i32 = 23;
    pub const FORCE_FIELD_POS0: i32 = 24;
    pub const HEIGHTMAP_SCALE: i32 = 40;
    pub const NORMAL_BLEND_HARDNESS: i32 = 41;
    pub const COLOR_BLEND_HARDNESS: i32 = 42;
    pub const BLEND_VEC_X: i32 = 43;
    pub const BLEND_MASK: i32 = 46;
    pub const DIFFUSE_MASK: i32 = 47;
    pub const SPECULAR_MASK: i32 = 48;
    pub const NORMAL_MASK: i32 = 49;
    pub const BLEND_BAND: i32 = 50;
    pub const INV_VIEW: i32 = 51;
    pub const LIQUID_COLOR: i32 = 52;
    pub const LIQUID_SPECULAR: i32 = 53;
    pub const LIQUID_INV_TEXTURE_SIZE: i32 = 54;
    pub const LIQUID_FADE: i32 = 55;
}

mod diffuse {
    use super::bit;
    pub const NORMAL_MAPS: u32 = bit(0);
    pub const SPECULAR: u32 = bit(1);
    pub const PARALLAX: u32 = bit(2);
    pub const UV_ANIMATION: u32 = bit(3);
    pub const ENV_MAP: u32 = bit(5);
    pub const CUBE_MAP_ALPHA: u32 = bit(6);
    pub const SWAY: u32 = bit(7);
    pub const SWAY_SINGLE_DIR: u32 = bit(8);
    pub const SWAY_MAP: u32 = bit(9);
    pub const DETAIL_DIFFUSE: u32 = bit(10);
    pub const DETAIL_NORMAL: u32 = bit(11);
    pub const TRANSLUCENCY: u32 = bit(12);
    pub const FORCE_FIELDS: u32 = bit(13);
    pub const LIQUID: u32 = bit(14);
    pub const LIQUID_ALPHA: u32 = bit(15);
    pub const LIQUID_MUL: u32 = bit(16);
}

mod zpass {
    use super::bit;
    pub const USE_ALPHA: u32 = bit(0);
    pub const UV_ANIMATION: u32 = bit(1);
    pub const DISSOLVE: u32 = bit(2);
    pub const DISSOLVE_ALPHA: u32 = bit(3);
    pub const ALPHA_DISSOLVE_FILTER: u32 = bit(4);
    pub const SWAY: u32 = bit(5);
    pub const SWAY_SINGLE_DIR: u32 = bit(6);
    pub const SWAY_MAP: u32 = bit(7);
    pub const FORCE_FIELDS: u32 = bit(8);
}

mod illum {
    use super::bit;
    pub const UV_ANIMATION: u32 = bit(0);
}

mod puv {
    use super::bit;
    pub const NORMAL_MAPS: u32 = bit(0);
    pub const SPECULAR: u32 = bit(1);
    pub const BOTTOM_TEXTURE: u32 = bit(2);
    pub const DIFFUSE_MAP: u32 = bit(3);
    pub const NORMAL_MAP_BLEND_WEIGHT: u32 = bit(4);
}

mod vblend {
    pub const NORMAL_UV_R: u32 = 1 << 12;
    pub const ALPHA_MASK: u32 = 1 << 15;
    pub const OVERLAY_NORMAL: u32 = 1 << 16;
    pub const TANGENTS: u32 = 1 << 17;
}

const VERTEX: u32 = bit(0);
const FRAGMENT: u32 = bit(1);

static DIFFUSE_FEATURES: [Feature; 17] = [
    feature(c"UseNormalMapping", VERTEX | FRAGMENT, 0),
    feature(c"UseSpecular", FRAGMENT, 0),
    feature(c"UseParallax", VERTEX | FRAGMENT, diffuse::NORMAL_MAPS),
    feature(c"UseUvAnimation", VERTEX, 0),
    feature(c"UseSkeleton", VERTEX, 0),
    feature(c"UseEnvMap", VERTEX | FRAGMENT, 0),
    feature(c"UseCubeMapAlpha", FRAGMENT, 0),
    feature(c"UseSway", VERTEX, 0),
    feature(c"UseSwaySingleDir", VERTEX, 0),
    feature(c"UseSwayMap", VERTEX, 0),
    feature(c"UseDetailDiffuse", FRAGMENT, 0),
    feature(c"UseDetailNormal", FRAGMENT, diffuse::NORMAL_MAPS),
    feature(c"UseTranslucency", FRAGMENT, 0),
    feature(c"UseFourForceFields", VERTEX, 0),
    feature(c"UseLiquid", VERTEX | FRAGMENT, 0),
    feature(c"LiquidBlendMode_Alpha", FRAGMENT, 0),
    feature(c"LiquidBlendMode_Mul", FRAGMENT, 0),
];

static ILLUM_FEATURES: [Feature; 2] = [
    feature(c"UseUvAnimation", VERTEX, 0),
    feature(c"UseSkeleton", VERTEX, 0),
];

static Z_FEATURES: [Feature; 9] = [
    feature(c"UseAlphaMap", FRAGMENT, 0),
    feature(c"UseUvAnimation", VERTEX, 0),
    feature(c"UseDissolve", FRAGMENT, 0),
    feature(c"UseDissolveAlphaMap", FRAGMENT, 0),
    feature(c"UseAlphaUseDissolveFilter", FRAGMENT, 0),
    feature(c"UseSway", VERTEX, 0),
    feature(c"UseSwaySingleDir", VERTEX, 0),
    feature(c"UseSwayMap", VERTEX, 0),
    feature(c"UseFourForceFields", VERTEX, 0),
];

static PUV_FEATURES: [Feature; 5] = [
    feature(c"UseNormalMapping", VERTEX | FRAGMENT, 0),
    feature(c"UseSpecular", FRAGMENT, 0),
    feature(c"UseBottomTexture", FRAGMENT, 0),
    feature(c"UseDiffuseMap", FRAGMENT, 0),
    feature(c"UseNormalMapBlendWeight", FRAGMENT, puv::NORMAL_MAPS),
];

static VBLEND_FEATURES: [Feature; 18] = [
    feature(c"UseDiffuse_R", FRAGMENT, 0),
    feature(c"UseDiffuse_G", FRAGMENT, 0),
    feature(c"UseDiffuse_B", FRAGMENT, 0),
    feature(c"UseSpecular_R", FRAGMENT, 0),
    feature(c"UseSpecular_G", FRAGMENT, 0),
    feature(c"UseSpecular_B", FRAGMENT, 0),
    feature(c"UseNormalMap_R", VERTEX | FRAGMENT, 0),
    feature(c"UseNormalMap_G", VERTEX | FRAGMENT, 0),
    feature(c"UseNormalMap_B", VERTEX | FRAGMENT, 0),
    feature(c"UseParallax_R", VERTEX | FRAGMENT, 0),
    feature(c"UseParallax_G", VERTEX | FRAGMENT, 0),
    feature(c"UseParallax_B", VERTEX | FRAGMENT, 0),
    feature(c"UseNormalUv_R", VERTEX | FRAGMENT, 0),
    feature(c"UseNormalUv_G", VERTEX | FRAGMENT, 0),
    feature(c"UseNormalUv_B", VERTEX | FRAGMENT, 0),
    feature(c"UseAlphaMask", FRAGMENT, 0),
    feature(c"OverlayNormal", FRAGMENT, 0),
    // the vertex shader only declares the tangent input with this
    feature(c"UseNormalMapping", VERTEX, 0),
];

fn add_sway_variable_ids(s: &mut dyn SetupSink, mode: i32) {
    s.var_id(manager::OWN, "afT", var::T, mode);
    s.var_id(manager::OWN, "avSwayProperties", var::SWAY_PROPERTIES, mode);
    s.var_id(
        manager::OWN,
        "avSwayOctavesMul",
        var::SWAY_OCTAVES_MUL,
        mode,
    );
    s.var_id(manager::OWN, "afSwayYFreqMul", var::SWAY_Y_FREQ_MUL, mode);
    s.var_id(
        manager::OWN,
        "avSwaySingleDirection",
        var::SWAY_SINGLE_DIRECTION,
        mode,
    );
    s.var_id(
        manager::OWN,
        "avSwaySinglesampleDirection",
        var::SWAY_SINGLE_SAMPLE_DIRECTION,
        mode,
    );
    s.var_id(manager::OWN, "a_mtxModel", var::MODEL, mode);
    for (id, name) in (var::FORCE_FIELD_POS0..).zip(FORCE_FIELD_NAMES) {
        s.var_id(manager::OWN, name, id, mode);
    }
}

/// `iMaterialType_SolidBase::CreateGlobalPrograms`, the Z programs every solid type shares.
pub fn setup_global(s: &mut dyn SetupSink) {
    s.clear_defines();
    s.define("UseUv");
    s.setup_program(
        manager::OWN,
        mode::Z,
        "Z",
        "deferred_base_vtx.glsl",
        "deferred_base_frag.glsl",
        &Z_FEATURES,
    );
    s.var_id(manager::OWN, "a_mtxUV", var::UV, mode::Z);
    s.var_id(
        manager::OWN,
        "afDissolveAmount",
        var::DISSOLVE_AMOUNT,
        mode::Z,
    );
    add_sway_variable_ids(s, mode::Z);
}

/// `cMaterialType_SolidDiffuse::LoadSpecificData`
pub fn setup_diffuse(s: &mut dyn SetupSink, g: &Globals) {
    s.clear_defines();
    s.define("UseUv");
    s.define("UseNormals");
    s.define("UseDepth");
    s.define_value("VirtualPositionAddScale", g.virtual_position_add_scale);
    // selects the constant buffers of shaders that dropped the plain-uniform path
    s.define("MaterialType_SolidDiffuse");
    s.define(if g.gbuffer_32bit {
        "Deferred_32bit"
    } else {
        "Deferred_64bit"
    });
    s.define(if g.gbuffer_four_targets {
        "RenderTargets_4"
    } else {
        "RenderTargets_3"
    });
    if !g.parallax_low && g.shader_model_3 {
        s.define("ParallaxMethod_Relief");
    } else {
        s.define("ParallaxMethod_Simple");
    }
    if g.hdr {
        s.define("UseColor");
        s.define("UseColorMul");
    }
    s.setup_program(
        manager::OWN,
        mode::DIFFUSE,
        "Diffuse",
        "deferred_base_vtx.glsl",
        "deferred_gbuffer_solid_frag.glsl",
        &DIFFUSE_FEATURES,
    );

    s.clear_defines();
    s.define("UseUv");
    s.define("UseColor");
    s.setup_program(
        manager::OWN,
        mode::ILLUMINATION,
        "Illum",
        "deferred_base_vtx.glsl",
        "deferred_illumination_frag.glsl",
        &ILLUM_FEATURES,
    );

    let d = mode::DIFFUSE;
    let own = manager::OWN;
    s.var_id(own, "afInvFarPlane", var::INV_FAR_PLANE, d);
    s.var_id(
        own,
        "avHeightMapScaleAndBias",
        var::HEIGHT_MAP_SCALE_AND_BIAS,
        d,
    );
    s.var_id(own, "a_mtxUV", var::UV, d);
    s.var_id(own, "avFrenselBiasPow", var::FRENSEL_BIAS_POW, d);
    s.var_id(own, "a_mtxInvViewRotation", var::INV_VIEW_ROTATION, d);
    s.var_id(own, "a_mtxInvView", var::INV_VIEW_ROTATION, d);
    s.var_id(own, "avColorMul", var::COLOR_MUL, d);
    s.var_id(own, "afFarPlane", var::FAR_PLANE, d);
    s.var_id(own, "avDetailProperties", var::DETAIL_PROPERTIES, d);
    s.var_id(own, "avDetailWeights", var::DETAIL_WEIGHTS, d);
    s.var_id(own, "avLiquidColor", var::LIQUID_COLOR, d);
    s.var_id(own, "avLiquidSpecular", var::LIQUID_SPECULAR, d);
    s.var_id(
        own,
        "afLiquidInvTextureSize",
        var::LIQUID_INV_TEXTURE_SIZE,
        d,
    );
    s.var_id(own, "afInvTextureSize", var::LIQUID_INV_TEXTURE_SIZE, d);
    s.var_id(own, "px_vLiquidFade", var::LIQUID_FADE, d);
    s.var_id(own, "avLiquidFade", var::LIQUID_FADE, d);
    add_sway_variable_ids(s, d);

    let i = mode::ILLUMINATION;
    s.var_id(own, "a_mtxUV", var::UV, i);
    s.var_id(own, "afColorMul", var::COLOR_MUL_F, i);
    s.var_id(own, "avIlluminationMul", var::ILLUMINATION_MUL, i);
}

/// `cMaterialType_ProjectedUV::LoadSpecificData`
pub fn setup_projected_uv(s: &mut dyn SetupSink) {
    s.clear_defines();
    for d in [
        "UseUv",
        "UseNormals",
        "UseDepth",
        "UseColor",
        "UseColorMul",
        "MaterialType_ProjectedUV",
    ] {
        s.define(d);
    }
    s.setup_program(
        manager::OWN,
        mode::DIFFUSE,
        "Diffuse",
        "deferred_projected_uv_vtx.glsl",
        "deferred_projected_uv_frag.glsl",
        &PUV_FEATURES,
    );
    let d = mode::DIFFUSE;
    let own = manager::OWN;
    s.var_id(own, "afInvFarPlane", var::INV_FAR_PLANE, d);
    s.var_id(own, "a_mtxUV", var::UV, d);
    s.var_id(own, "avColorMul", var::COLOR_MUL, d);
    s.var_id(own, "a_mtxNormalFrag", var::NORMAL_FRAG, d);
    s.var_id(own, "afBlendHardness", var::BLEND_HARDNESS, d);
    s.var_id(own, "afBlendHardnessVtx", var::BLEND_HARDNESS_VTX, d);
    s.var_id(
        own,
        "afNormalMapBlendImpact",
        var::NORMAL_MAP_BLEND_IMPACT,
        d,
    );
    s.var_id(own, "avTextureScale", var::TEXTURE_SCALE, d);
}

/// `cMaterialType_VertexBlend::LoadSpecificData`
pub fn setup_vertex_blend(s: &mut dyn SetupSink) {
    s.clear_defines();
    for d in [
        "UseUv",
        "UseNormals",
        "UseDepth",
        "UseColor",
        "UseColorMul",
        "MaterialType_BlendWeight",
    ] {
        s.define(d);
    }
    s.setup_program(
        manager::OWN,
        mode::DIFFUSE,
        "Diffuse",
        "deferred_projected_uv_vtx.glsl",
        "deferred_projected_blendweight_frag.glsl",
        &VBLEND_FEATURES,
    );
    let d = mode::DIFFUSE;
    let own = manager::OWN;
    s.var_id(own, "afInvFarPlane", var::INV_FAR_PLANE, d);
    s.var_id(own, "a_mtxModel", var::MODEL, d);
    s.var_id(own, "a_mtxInvView", var::INV_VIEW, d);
    s.var_id(own, "avColorMul", var::COLOR_MUL, d);
    s.var_id(own, "avHeightmapScale", var::HEIGHTMAP_SCALE, d);
    s.var_id(own, "afNormalBlendHardness", var::NORMAL_BLEND_HARDNESS, d);
    s.var_id(own, "afColorBlendHardness", var::COLOR_BLEND_HARDNESS, d);
    s.var_id(own, "avTextureScale", var::TEXTURE_SCALE, d);
    s.var_id(own, "avBlendVecX", var::BLEND_VEC_X, d);
    s.var_id(own, "avBlendVecY", var::BLEND_VEC_X + 1, d);
    s.var_id(own, "avBlendVecZ", var::BLEND_VEC_X + 2, d);
    s.var_id(own, "avBlendMask", var::BLEND_MASK, d);
    s.var_id(own, "avDiffuseMask", var::DIFFUSE_MASK, d);
    s.var_id(own, "avSpecularMask", var::SPECULAR_MASK, d);
    s.var_id(own, "avNormalMask", var::NORMAL_MASK, d);
    s.var_id(own, "a_mtxBlendBand", var::BLEND_BAND, d);
}

/// `iMaterialType_SolidBase::DestroyProgram`: which manager and main mode a program leaves.
pub fn destroy_target(render_mode: i32) -> ProgSel {
    if render_mode == mode::Z || render_mode == mode::Z_DISSOLVE {
        ProgSel::global(mode::Z, 0)
    } else {
        ProgSel::own(render_mode, 0)
    }
}

/// `SwayFlags`
fn sway_flags(v: &SolidVars, m: &MatView, sway: u32, single_dir: u32, map: u32, ff: u32) -> u32 {
    if !v.sway_active {
        return 0;
    }
    let mut f = sway;
    if v.sway_single_dir {
        f |= single_dir;
    }
    if v.sway_force_field_affected {
        f |= ff;
    }
    if m.has(tex::HEIGHT) {
        f |= map;
    }
    f
}

pub fn diffuse_program(v: &MatView, vars: &SolidVars, g: &Globals, render_mode: i32) -> ProgSel {
    let sway = |s, d, m, f| sway_flags(vars, v, s, d, m, f);
    match render_mode {
        mode::Z => {
            let mut f = 0;
            if v.has(tex::ALPHA) {
                f |= zpass::USE_ALPHA;
            }
            if v.has_uv_anim {
                f |= zpass::UV_ANIMATION;
            }
            if vars.alpha_dissolve_filter {
                f |= zpass::ALPHA_DISSOLVE_FILTER;
            }
            f |= sway(
                zpass::SWAY,
                zpass::SWAY_SINGLE_DIR,
                zpass::SWAY_MAP,
                zpass::FORCE_FIELDS,
            );
            ProgSel::global(mode::Z, f)
        }
        mode::Z_DISSOLVE => {
            let mut f = zpass::DISSOLVE;
            if v.has(tex::ALPHA) {
                f |= zpass::USE_ALPHA;
            }
            if v.has(tex::DISSOLVE_ALPHA) {
                f |= zpass::DISSOLVE_ALPHA;
            }
            if v.has_uv_anim {
                f |= zpass::UV_ANIMATION;
            }
            if vars.alpha_dissolve_filter {
                f |= zpass::ALPHA_DISSOLVE_FILTER;
            }
            f |= sway(
                zpass::SWAY,
                zpass::SWAY_SINGLE_DIR,
                zpass::SWAY_MAP,
                zpass::FORCE_FIELDS,
            );
            ProgSel::global(mode::Z, f)
        }
        mode::DIFFUSE => {
            let mut f = 0;
            if v.has(tex::NMAP) {
                f |= diffuse::NORMAL_MAPS;
            }
            if v.has(tex::SPECULAR) {
                f |= diffuse::SPECULAR;
            }
            if v.has(tex::HEIGHT) && g.parallax_enabled && !vars.sway_active {
                f |= diffuse::PARALLAX;
            }
            if v.has(tex::CUBE_MAP) {
                f |= diffuse::ENV_MAP;
                if v.has(tex::CUBE_MAP_ALPHA) {
                    f |= diffuse::CUBE_MAP_ALPHA;
                }
            }
            if v.has_uv_anim {
                f |= diffuse::UV_ANIMATION;
            }
            if v.has(tex::DETAIL_DIFFUSE) {
                f |= diffuse::DETAIL_DIFFUSE;
            }
            if v.has(tex::DETAIL_NMAP) {
                f |= diffuse::DETAIL_NORMAL;
            }
            if v.has(tex::TRANSLUCENCY) {
                f |= diffuse::TRANSLUCENCY;
            }
            if v.has(tex::LIQUID) && g.hpsl {
                f |= diffuse::LIQUID;
                if vars.liquid_blend_mode == LIQUID_BLEND_ALPHA {
                    f |= diffuse::LIQUID_ALPHA;
                } else if vars.liquid_blend_mode == LIQUID_BLEND_MUL {
                    f |= diffuse::LIQUID_MUL;
                }
            }
            f |= sway(
                diffuse::SWAY,
                diffuse::SWAY_SINGLE_DIR,
                diffuse::SWAY_MAP,
                diffuse::FORCE_FIELDS,
            );
            ProgSel::own(mode::DIFFUSE, f)
        }
        mode::ILLUMINATION => {
            let f = if v.has_uv_anim {
                illum::UV_ANIMATION
            } else {
                0
            };
            ProgSel::own(mode::ILLUMINATION, f)
        }
        _ => ProgSel::NONE,
    }
}

pub fn diffuse_texture(
    v: &MatView,
    vars: &SolidVars,
    g: &Globals,
    render_mode: i32,
    unit: i32,
) -> TexSel {
    let sway_height = || {
        if vars.sway_active {
            TexSel::slot_if(v, tex::HEIGHT)
        } else {
            TexSel::NONE
        }
    };
    match render_mode {
        mode::Z => match unit {
            0 => TexSel::slot_if(v, tex::ALPHA),
            1 => TexSel::special(texsel::DISSOLVE),
            3 => sway_height(),
            _ => TexSel::NONE,
        },
        mode::Z_DISSOLVE => match unit {
            0 => TexSel::slot_if(v, tex::ALPHA),
            1 => TexSel::special(texsel::DISSOLVE),
            2 => TexSel::slot_if(v, tex::DISSOLVE_ALPHA),
            3 => sway_height(),
            _ => TexSel::NONE,
        },
        mode::DIFFUSE => {
            if unit > 3 && g.hpsl {
                return match unit {
                    5 => TexSel::slot_if(v, tex::CUBE_MAP),
                    6 => TexSel::slot_if(v, tex::CUBE_MAP_ALPHA),
                    8 => TexSel::slot_if(v, tex::DETAIL_DIFFUSE),
                    9 if v.has(tex::NMAP) => TexSel::slot_if(v, tex::DETAIL_NMAP),
                    10 => TexSel::slot_if(v, tex::TRANSLUCENCY),
                    11 => TexSel::slot_if(v, tex::LIQUID),
                    _ => TexSel::NONE,
                };
            }
            match unit {
                0 => TexSel::slot_if(v, tex::DIFFUSE),
                1 => TexSel::slot_if(v, tex::NMAP),
                2 => TexSel::slot_if(v, tex::SPECULAR),
                3 => TexSel::slot_if(v, tex::HEIGHT),
                4 => TexSel::slot_if(v, tex::CUBE_MAP),
                5 => TexSel::slot_if(v, tex::CUBE_MAP_ALPHA),
                _ => TexSel::NONE,
            }
        }
        mode::ILLUMINATION if unit == 0 => TexSel::slot_if(v, tex::ILLUMINATION),
        _ => TexSel::NONE,
    }
}

/// `iMaterialType_SolidBase::CompileMaterialSpecifics` plus the type's `CompileSolidSpecifics`.
pub fn diffuse_compile(v: &MatView, vars: &SolidVars, g: &Globals) -> CompileOut {
    let mut c = base_compile(v);
    c.object_specific(mode::Z_DISSOLVE, true);
    if g.hdr || v.has(tex::LIQUID) {
        c.object_specific(mode::DIFFUSE, true);
    }
    if v.has(tex::LIQUID) {
        c.specific(mode::DIFFUSE, true);
    }
    c.dissolve_filter = i32::from(vars.alpha_dissolve_filter);
    if vars.sway_active {
        for m in [mode::Z, mode::Z_DISSOLVE, mode::DIFFUSE] {
            c.specific(m, true);
            c.object_specific(m, true);
        }
    }
    if v.has(tex::NMAP) && v.has(tex::HEIGHT) {
        c.specific(mode::DIFFUSE, true);
    }
    if v.has_uv_anim {
        c.specific(mode::Z, true);
        c.specific(mode::DIFFUSE, true);
        c.specific(mode::ILLUMINATION, true);
    }
    if v.has(tex::CUBE_MAP) || v.has(tex::DETAIL_DIFFUSE) || v.has(tex::DETAIL_NMAP) {
        c.specific(mode::DIFFUSE, true);
    }
    if v.has(tex::ILLUMINATION) {
        c.object_specific(mode::ILLUMINATION, true);
    }
    c
}

fn base_compile(v: &MatView) -> CompileOut {
    let mut c = CompileOut::NONE;
    c.alpha_mode = if v.has(tex::ALPHA) {
        alpha_mode::TRANS
    } else {
        alpha_mode::SOLID
    };
    c
}

pub fn diffuse_setup_type(render_mode: i32, r: &RenderView, out: &mut Uniforms) {
    if render_mode == mode::DIFFUSE {
        out.float(var::INV_FAR_PLANE, 1.0f32 / r.far);
        out.float(var::FAR_PLANE, r.far);
    }
}

pub fn diffuse_setup_material(
    render_mode: i32,
    v: &MatView,
    vars: &SolidVars,
    g: &Globals,
    r: &RenderView,
    out: &mut Uniforms,
) {
    if !matches!(
        render_mode,
        mode::DIFFUSE | mode::Z | mode::Z_DISSOLVE | mode::ILLUMINATION
    ) {
        return;
    }
    if v.has_uv_anim {
        out.mat(var::UV, &v.uv_matrix);
    }
    if vars.sway_active && render_mode != mode::ILLUMINATION {
        out.float(var::T, r.time);
        out.vec3(var::SWAY_PROPERTIES, vars.sway_properties);
        out.vec3(var::SWAY_OCTAVES_MUL, vars.sway_octave_muls);
        out.float(var::SWAY_Y_FREQ_MUL, vars.sway_y_freq_mul);
        out.vec3(var::SWAY_SINGLE_DIRECTION, vars.sway_single_dir_vec);
        out.vec3(
            var::SWAY_SINGLE_SAMPLE_DIRECTION,
            vars.sway_single_sample_dir,
        );
    }
    if render_mode != mode::DIFFUSE {
        return;
    }
    if v.has(tex::HEIGHT) && g.parallax_enabled && !vars.sway_active {
        out.vec2(
            var::HEIGHT_MAP_SCALE_AND_BIAS,
            vars.height_map_scale,
            vars.height_map_bias,
        );
    }
    if v.has(tex::CUBE_MAP) {
        out.vec2(var::FRENSEL_BIAS_POW, vars.frensel_bias, vars.frensel_pow);
        out.mat(var::INV_VIEW_ROTATION, &inv_view_rotation(&r.view));
    }
    if v.has(tex::DETAIL_DIFFUSE) || v.has(tex::DETAIL_NMAP) {
        let [p0, p1, p2, p3] = vars.detail_properties;
        out.vec4(var::DETAIL_PROPERTIES, p0, p1, p2, p3);
        out.vec3(var::DETAIL_WEIGHTS, vars.detail_weights);
    }
    if v.has(tex::LIQUID) {
        out.color(var::LIQUID_COLOR, vars.liquid_color);
        out.color(var::LIQUID_SPECULAR, vars.liquid_specular);
        let size = if v.liquid_size.x > v.liquid_size.y {
            v.liquid_size.x
        } else {
            v.liquid_size.y
        };
        out.float(var::LIQUID_INV_TEXTURE_SIZE, 1.0f32 / (size as f32));
    }
}

#[allow(clippy::too_many_arguments)]
pub fn diffuse_setup_object(
    render_mode: i32,
    v: &MatView,
    vars: &SolidVars,
    g: &Globals,
    r: &RenderView,
    o: &ObjView,
    host: &mut dyn Host,
    out: &mut Uniforms,
) {
    if vars.sway_active && render_mode != mode::ILLUMINATION {
        out.mat(var::MODEL, &model_or_identity(o.has_model, &o.model));
        if vars.sway_force_field_affected {
            let mut fields = [Default::default(); 4];
            let n = host.force_fields(&mut fields).min(4);
            force_field_vars(
                out,
                var::FORCE_FIELD_POS0,
                fields.get(..n).unwrap_or(&[]),
                vars.sway_force_field_mul,
                vars.sway_force_field_max,
            );
        }
    }
    match render_mode {
        mode::Z_DISSOLVE => out.float_checked(var::DISSOLVE_AMOUNT, host.coverage()),
        mode::ILLUMINATION => {
            out.float(var::COLOR_MUL_F, o.illum_amount);
            let mut illum =
                color_scale(o.illum_color, o.illum_amount * vars.illumination_brightness);
            if g.hpsl {
                illum = color_scale(color_mul(illum, o.color_mul), o.color_mul.a);
            }
            out.color(var::ILLUMINATION_MUL, illum);
        }
        mode::DIFFUSE => {
            out.color(var::COLOR_MUL, o.color_mul);
            if v.has(tex::LIQUID) {
                liquid_fade(vars, r, o, out);
            }
        }
        _ => {}
    }
}

/// official SetupObjectInstanceData 0xdbd050
fn liquid_fade(vars: &SolidVars, r: &RenderView, o: &ObjView, out: &mut Uniforms) {
    let mut t = o.liquid_amount;
    if t <= -1.0 {
        t += 1.0 + r.time;
    }
    let fade_in = vars.liquid_fade_speed.x;
    let mut fade_out = vars.liquid_fade_speed.y;
    if vars.liquid_loop_fade {
        t = fmod(t, (fade_in + fade_out) * 1.1f32);
    }
    if fade_out <= 0.0 {
        fade_out = t + 1e6f32;
    }
    out.vec4(
        var::LIQUID_FADE,
        t / fade_in - 1.0,
        (fade_out - t) / fade_in,
        1.0f32 / (vars.liquid_edge_size + 0.01f32),
        2.0f32 * vars.liquid_dryness,
    );
}

fn has_projected_texture(v: &MatView, first: u32, step: u32) -> bool {
    (0..3).any(|i| v.has(first + i * step))
}

pub fn projected_uv_texture(v: &MatView, render_mode: i32, unit: i32) -> TexSel {
    if render_mode == mode::Z || render_mode == mode::Z_DISSOLVE {
        return if unit == 1 {
            TexSel::special(texsel::DISSOLVE)
        } else {
            TexSel::NONE
        };
    }
    let Ok(unit) = u32::try_from(unit) else {
        return TexSel::NONE;
    };
    if render_mode != mode::DIFFUSE || unit > 8 {
        return TexSel::NONE;
    }
    // Units 0-8: Diffuse, NMap, Specular x Side, Top, Bottom; a missing one falls back to another of its kind
    let first = tex::DIFFUSE_SIDE + unit.div_euclid(3) * 3;
    let start = unit.rem_euclid(3);
    (0..3)
        .map(|i| first + (start + i).rem_euclid(3))
        .find(|s| v.has(*s))
        .map_or(TexSel::NONE, TexSel::slot)
}

pub fn projected_uv_program(v: &MatView, vars: &ProjectedUvVars, render_mode: i32) -> ProgSel {
    match render_mode {
        mode::Z => ProgSel::global(mode::Z, 0),
        mode::Z_DISSOLVE => ProgSel::global(mode::Z, zpass::DISSOLVE),
        mode::DIFFUSE => {
            let mut f = 0;
            if has_projected_texture(v, tex::DIFFUSE_SIDE, 1) {
                f |= puv::DIFFUSE_MAP;
            }
            if has_projected_texture(v, tex::NMAP_SIDE, 1) {
                f |= puv::NORMAL_MAPS;
            }
            if has_projected_texture(v, tex::SPECULAR_SIDE, 1) {
                f |= puv::SPECULAR;
            }
            if has_projected_texture(v, tex::DIFFUSE_BOTTOM, 3) {
                f |= puv::BOTTOM_TEXTURE;
            }
            if vars.normal_map_blend_impact > 0.0 {
                f |= puv::NORMAL_MAP_BLEND_WEIGHT;
            }
            ProgSel::own(mode::DIFFUSE, f)
        }
        _ => ProgSel::NONE,
    }
}

/// Compile for the two types that share `CompileSolidSpecifics`.
pub fn blend_compile(v: &MatView) -> CompileOut {
    let mut c = base_compile(v);
    c.specific(mode::DIFFUSE, true);
    c.object_specific(mode::DIFFUSE, true);
    c.object_specific(mode::Z_DISSOLVE, true);
    c
}

pub fn projected_uv_setup_type(render_mode: i32, r: &RenderView, out: &mut Uniforms) {
    if render_mode == mode::DIFFUSE {
        out.float(var::INV_FAR_PLANE, 1.0f32 / r.far);
    }
}

pub fn projected_uv_setup_material(render_mode: i32, vars: &ProjectedUvVars, out: &mut Uniforms) {
    if render_mode != mode::DIFFUSE {
        return;
    }
    out.float(var::BLEND_HARDNESS, vars.blend_hardness);
    out.float(var::BLEND_HARDNESS_VTX, vars.blend_hardness);
    out.float(var::NORMAL_MAP_BLEND_IMPACT, vars.normal_map_blend_impact);
    out.vec3(var::TEXTURE_SCALE, vars.texture_scale);
}

pub fn projected_uv_setup_object(
    render_mode: i32,
    vars: &ProjectedUvVars,
    r: &RenderView,
    o: &ObjView,
    host: &mut dyn Host,
    out: &mut Uniforms,
) {
    if render_mode == mode::Z_DISSOLVE {
        out.float(var::DISSOLVE_AMOUNT, host.coverage());
    } else if render_mode == mode::DIFFUSE {
        // Official static batches are in world space; other objects only project in world space with DynamicObjectSupport
        let world = o.has_model && (o.is_static || vars.dynamic_object_support);
        out.mat(var::UV, &model_or_identity(world, &o.model));
        let normal = if world || !o.has_model {
            r.view
        } else {
            view_mul(r, &o.model)
        };
        out.mat(var::NORMAL_FRAG, &normal);
        out.color(var::COLOR_MUL, o.color_mul);
    }
}

fn mask_of(v: &MatView, kind: u32) -> Vector3f {
    let m = |i: u32| {
        if v.has(tex::DIFFUSE_R + i * 4 + kind) {
            1.0f32
        } else {
            0.0f32
        }
    };
    v3(m(0), m(1), m(2))
}

fn vec3_max(a: Vector3f, b: Vector3f) -> Vector3f {
    v3(fmax(a.x, b.x), fmax(a.y, b.y), fmax(a.z, b.z))
}

/// Which of R, G, B has a diffuse, specular or normal map.
fn blend_mask(v: &MatView) -> Vector3f {
    vec3_max(mask_of(v, 0), vec3_max(mask_of(v, 1), mask_of(v, 2)))
}

pub fn vertex_blend_texture(v: &MatView, render_mode: i32, unit: i32) -> TexSel {
    if render_mode == mode::Z || render_mode == mode::Z_DISSOLVE {
        return if unit == 1 {
            TexSel::special(texsel::DISSOLVE)
        } else {
            TexSel::NONE
        };
    }
    match u32::try_from(unit) {
        Ok(u) if render_mode == mode::DIFFUSE && u <= 11 => TexSel::slot_if(v, tex::DIFFUSE_R + u),
        _ => TexSel::NONE,
    }
}

pub fn vertex_blend_program(v: &MatView, vars: &VertexBlendVars, render_mode: i32) -> ProgSel {
    match render_mode {
        mode::Z => return ProgSel::global(mode::Z, 0),
        mode::Z_DISSOLVE => return ProgSel::global(mode::Z, zpass::DISSOLVE),
        mode::DIFFUSE => {}
        _ => return ProgSel::NONE,
    }
    let mask = blend_mask(v);
    let all_auto = vars.auto_uv.iter().all(|a| *a);
    let mask_of_channel = [mask.x, mask.y, mask.z];
    let height_scale = [
        vars.height_map_scale.x,
        vars.height_map_scale.y,
        vars.height_map_scale.z,
    ];
    let mut flags = 0u32;
    for (i, ((auto_uv, m), hs)) in
        (0u32..).zip(vars.auto_uv.iter().zip(mask_of_channel).zip(height_scale))
    {
        for kind in 0u32..4 {
            if !v.has(tex::DIFFUSE_R + i * 4 + kind) {
                continue;
            }
            if kind == 3 && hs <= 0.0 {
                continue;
            }
            flags |= bit(0) << (kind * 3 + i);
        }
        if !all_auto && (!*auto_uv || m == 0.0) {
            flags |= vblend::NORMAL_UV_R << i;
        }
    }
    if vars.alpha_mask {
        flags |= vblend::ALPHA_MASK;
    }
    if vars.overlay_normal {
        flags |= vblend::OVERLAY_NORMAL;
    }
    if (flags & (vblend::NORMAL_UV_R * 7)) != 0 && (flags & (bit(6) * 63)) != 0 {
        flags |= vblend::TANGENTS;
    }
    ProgSel::own(mode::DIFFUSE, flags)
}

pub fn vertex_blend_setup_type(render_mode: i32, r: &RenderView, out: &mut Uniforms) {
    if render_mode != mode::DIFFUSE {
        return;
    }
    out.float(var::INV_FAR_PLANE, 1.0f32 / r.far);
    out.mat(var::INV_VIEW, &matrix_inverse(&r.view));
}

pub fn vertex_blend_setup_material(
    render_mode: i32,
    v: &MatView,
    vars: &VertexBlendVars,
    r: &RenderView,
    out: &mut Uniforms,
) {
    if render_mode != mode::DIFFUSE {
        return;
    }
    out.vec3(var::HEIGHTMAP_SCALE, vars.height_map_scale);
    out.float(var::NORMAL_BLEND_HARDNESS, vars.normal_blend_hardness);
    out.float(var::COLOR_BLEND_HARDNESS, vars.color_blend_hardness);
    out.vec3(var::TEXTURE_SCALE, vars.texture_scale);
    out.vec3(var::BLEND_MASK, blend_mask(v));
    out.vec3(var::DIFFUSE_MASK, mask_of(v, 0));
    out.vec3(var::SPECULAR_MASK, mask_of(v, 1));
    out.vec3(var::NORMAL_MASK, mask_of(v, 2));

    // World axes and directional weights in view space, like the normal
    let mut band = IDENTITY;
    let axes = [v3(1.0, 0.0, 0.0), v3(0.0, 1.0, 0.0), v3(0.0, 0.0, 1.0)];
    for ((i, axis), (dir, row)) in (0i32..)
        .zip(axes)
        .zip(vars.directional_weight.iter().zip(band.m.iter_mut()))
    {
        out.vec3(var::BLEND_VEC_X + i, matrix_mul_3x3(&r.view, axis));
        let d = matrix_mul_3x3(&r.view, v3(dir.r, dir.g, dir.b));
        *row = [d.x, d.y, d.z, dir.a];
    }
    out.mat(var::BLEND_BAND, &band);
}

pub fn vertex_blend_setup_object(
    render_mode: i32,
    o: &ObjView,
    host: &mut dyn Host,
    out: &mut Uniforms,
) {
    if render_mode == mode::Z_DISSOLVE {
        out.float(var::DISSOLVE_AMOUNT, host.coverage());
    } else if render_mode == mode::DIFFUSE {
        out.mat(var::MODEL, &model_or_identity(o.has_model, &o.model));
        out.color(var::COLOR_MUL, o.color_mul);
    }
}
