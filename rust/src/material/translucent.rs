//! `cMaterialType_Translucent`: blended transparent surfaces with refraction, fog, soft
//! particles, depth of field and light-probe or light-level modulated alpha.

use super::common::{color, fmax, fog_render_color, inv_view_rotation};
use super::probe;
use super::setup::{Feature, SetupSink, feature};
use super::types::{
    CompileOut, Globals, Host, MatView, ObjView, ProgSel, RenderView, TexSel, Uniforms, bit, blend,
    manager, mode, tex, texsel,
};
use super::vars::TranslucentVars;
use crate::math::{fma, len3, sub3};

mod var {
    pub const ALPHA: i32 = 0;
    pub const FOG_START_AND_LENGTH: i32 = 1;
    pub const ONE_MINUS_FOG_ALPHA: i32 = 2;
    pub const FALLOFF_EXP: i32 = 3;
    pub const UV: i32 = 4;
    pub const REFRACTION_SCALE: i32 = 5;
    pub const INV_VIEW_ROTATION: i32 = 6;
    pub const FRENSEL_BIAS_POW: i32 = 7;
    pub const RIM_LIGHT_MUL_POW: i32 = 8;
    pub const LIGHT_LEVEL: i32 = 9;
    pub const INV_SCREEN_SIZE: i32 = 10;
    pub const COLOR_MUL: i32 = 11;
    pub const LIGHT_PROBE: i32 = 12;
    pub const NEAR_PLANE: i32 = 13;
    pub const INV_FAR_PLANE: i32 = 14;
    pub const SOFT_PARTICLE_THICKNESS: i32 = 15;
    pub const SOFT_PARTICLE_ALPHA_BASED_THICKNESS: i32 = 16;
    pub const SOFT_PARTICLE_DEPTH_BIAS: i32 = 17;
    pub const FOG_COLOR: i32 = 18;
    pub const FOG_AREA_COLOR: i32 = 19;
    pub const SECOND_FOG_COLOR: i32 = 20;
    pub const SECOND_FOG_START_AND_LENGTH: i32 = 21;
    pub const SECOND_FALLOFF_EXP: i32 = 22;
    pub const FADE_COLOR: i32 = 23;
    pub const DEPTH_OF_FIELD_PARAMS: i32 = 24;
    pub const FADE_PROP: i32 = 25;
}

pub(crate) mod feat {
    use super::bit;
    pub const FOG: u32 = bit(0);
    pub const UV_ANIMATION: u32 = bit(1);
    pub const USE_REFRACTION: u32 = bit(2);
    pub const NORMAL_MAP: u32 = bit(3);
    pub const ENV_MAP: u32 = bit(4);
    pub const DIFFUSE_MAP: u32 = bit(5);
    pub const CUBE_MAP_ALPHA: u32 = bit(6);
    pub const USE_SCREEN_NORMAL: u32 = bit(7);
    pub const LIT: u32 = bit(8);
    pub const SOFT_PARTICLE: u32 = bit(9);
    pub const UNDERWATER_FOG: u32 = bit(10);
    pub const SECONDARY_FOG: u32 = bit(11);
    pub const FADE_COLOR: u32 = bit(12);
    pub const SRGB_DIFFUSE_MAP: u32 = bit(13);
    pub const ANGLE_FADE: u32 = bit(14);
    pub const DEPTH_OF_FIELD: u32 = bit(15);
    pub const ALPHA_BLEND_REFRACTION: u32 = 0x0001_0000;
}

const VERTEX: u32 = bit(0);
const FRAGMENT: u32 = bit(1);

static FEATURES: [Feature; 17] = [
    feature(c"UseFog", FRAGMENT | VERTEX, 0),
    feature(c"UseUvAnimation", VERTEX, 0),
    feature(c"UseRefraction", FRAGMENT | VERTEX, 0),
    feature(c"UseNormalMapping", FRAGMENT | VERTEX, 0),
    feature(c"UseEnvMap", FRAGMENT | VERTEX, 0),
    feature(c"UseDiffuseMap", FRAGMENT, 0),
    feature(c"UseCubeMapAlpha", FRAGMENT, 0),
    feature(c"UseScreenNormal", FRAGMENT, 0),
    feature(c"Lit", FRAGMENT, 0),
    feature(c"UseSoftParticle", FRAGMENT | VERTEX, 0),
    feature(c"UseUnderwaterFog", FRAGMENT, 0),
    feature(c"UseSecondaryFog", FRAGMENT, 0),
    feature(c"UseFadeColor", FRAGMENT, 0),
    feature(c"UseSRGBDiffuseMap", FRAGMENT, 0),
    feature(c"UseAngleFade", VERTEX, 0),
    feature(c"UseDepthOfField", FRAGMENT | VERTEX, 0),
    feature(c"UseAlphaBlendRefraction", FRAGMENT, 0),
];

const BLEND_DEFINES: [&str; 5] = [
    "BlendMode_Add",
    "BlendMode_Mul",
    "BlendMode_MulX2",
    "BlendMode_Alpha",
    "BlendMode_PremulAlpha",
];

const VAR_IDS: [(&str, i32); 27] = [
    ("afAlpha", var::ALPHA),
    ("avFogStartAndLength", var::FOG_START_AND_LENGTH),
    ("afOneMinusFogAlpha", var::ONE_MINUS_FOG_ALPHA),
    ("afFalloffExp", var::FALLOFF_EXP),
    ("a_mtxUV", var::UV),
    ("afRefractionScale", var::REFRACTION_SCALE),
    ("a_mtxInvViewRotation", var::INV_VIEW_ROTATION),
    ("a_mtxInvView", var::INV_VIEW_ROTATION),
    ("avFrenselBiasPow", var::FRENSEL_BIAS_POW),
    ("avRimLightMulPow", var::RIM_LIGHT_MUL_POW),
    ("afLightLevel", var::LIGHT_LEVEL),
    ("avInvScreenSize", var::INV_SCREEN_SIZE),
    ("avColorMul", var::COLOR_MUL),
    ("px_mtxLightProbe", var::LIGHT_PROBE),
    ("afNearPlane", var::NEAR_PLANE),
    ("afInvFarPlane", var::INV_FAR_PLANE),
    ("afSoftParticleThickness", var::SOFT_PARTICLE_THICKNESS),
    (
        "afSoftParticleAlphaBasedThickness",
        var::SOFT_PARTICLE_ALPHA_BASED_THICKNESS,
    ),
    ("afSoftParticleDepthBias", var::SOFT_PARTICLE_DEPTH_BIAS),
    ("avFogColor", var::FOG_COLOR),
    ("avFogAreaColor", var::FOG_AREA_COLOR),
    ("avSecondFogColor", var::SECOND_FOG_COLOR),
    (
        "avSecondFogStartAndLength",
        var::SECOND_FOG_START_AND_LENGTH,
    ),
    ("afSecondFalloffExp", var::SECOND_FALLOFF_EXP),
    ("avFadeColor", var::FADE_COLOR),
    ("avDepthOfFieldParams", var::DEPTH_OF_FIELD_PARAMS),
    ("avFadeProp", var::FADE_PROP),
];

/// `cMaterialType_Translucent::SetWorldFog`: the fog feature bits of the current world.
pub fn world_fog_flags(underwater: bool, secondary: bool) -> u32 {
    (if underwater { feat::UNDERWATER_FOG } else { 0 })
        | (if secondary { feat::SECONDARY_FOG } else { 0 })
}

/// `cMaterialType_Translucent::LoadData`
pub fn setup(s: &mut dyn SetupSink, g: &Globals) {
    for (i, blend_define) in (0i32..).zip(BLEND_DEFINES) {
        let mgr = manager::BLEND0 + i;
        s.clear_defines();
        s.define("UseUv");
        s.define("UseNormals");
        s.define("UseColor");
        s.define("UseVertexPosition");
        if g.hdr {
            s.define("UseColorMul");
        }
        s.define(blend_define);
        s.setup_program(
            mgr,
            mode::DIFFUSE,
            "Diffuse",
            "deferred_base_vtx.glsl",
            "deferred_transparent_frag.glsl",
            &FEATURES,
        );
        for (name, id) in VAR_IDS {
            s.var_id(mgr, name, id, mode::DIFFUSE);
        }
    }
}

/// `blend - 1` as a manager code; out-of-range indices are what the C++ read out of bounds.
fn blend_manager(blend_mode: i32) -> i32 {
    match blend_mode.checked_sub(1) {
        Some(i) if (0..5).contains(&i) => manager::BLEND0 + i,
        _ => manager::INVALID,
    }
}

/// `cMaterialType_Translucent::DestroyProgram`
pub fn destroy_target(render_mode: i32, blend_mode: i32) -> ProgSel {
    // These render modes always use add!!
    let m = if render_mode == mode::ILLUMINATION || render_mode == mode::ILLUMINATION_FOG {
        blend_manager(blend::ADD)
    } else {
        blend_manager(blend_mode)
    };
    ProgSel {
        manager: m,
        mode: mode::DIFFUSE,
        flags: 0,
    }
}

fn refraction_on(vars: &TranslucentVars, g: &Globals) -> bool {
    vars.refraction && g.refraction
}

pub fn texture(
    v: &MatView,
    vars: &TranslucentVars,
    g: &Globals,
    render_mode: i32,
    unit: i32,
) -> TexSel {
    let refr = refraction_on(vars, g);
    match render_mode {
        mode::DIFFUSE | mode::DIFFUSE_FOG => match unit {
            0 => TexSel::slot_if(v, tex::DIFFUSE),
            1 => TexSel::slot_if(v, tex::NMAP),
            2 if refr => TexSel::special(texsel::REFRACTION),
            3 => TexSel::slot_if(v, tex::CUBE_MAP),
            4 => TexSel::slot_if(v, tex::CUBE_MAP_ALPHA),
            7 if refr || vars.soft_particle => TexSel::special(texsel::SCENE_DEPTH),
            _ => TexSel::NONE,
        },
        mode::ILLUMINATION | mode::ILLUMINATION_FOG => match unit {
            1 => TexSel::slot_if(v, tex::NMAP),
            3 => TexSel::slot_if(v, tex::CUBE_MAP),
            4 => TexSel::slot_if(v, tex::CUBE_MAP_ALPHA),
            _ => TexSel::NONE,
        },
        _ => TexSel::NONE,
    }
}

/// `skeleton` is the program slot: 1 holds the depth of field crossfade variant.
pub fn program(
    v: &MatView,
    vars: &TranslucentVars,
    g: &Globals,
    render_mode: i32,
    skeleton: bool,
) -> ProgSel {
    let refr = refraction_on(vars, g);
    if render_mode == mode::DIFFUSE || render_mode == mode::DIFFUSE_FOG {
        let mut f = 0;
        if v.has(tex::DIFFUSE) {
            f |= feat::DIFFUSE_MAP;
        }
        if v.has(tex::DIFFUSE) && v.diffuse_srgb {
            f |= feat::SRGB_DIFFUSE_MAP;
        }
        if render_mode == mode::DIFFUSE_FOG {
            f |= feat::FOG | g.world_fog;
        }
        if v.has_uv_anim {
            f |= feat::UV_ANIMATION;
        }
        if v.has(tex::NMAP) {
            f |= feat::NORMAL_MAP;
        }
        if refr && v.has(tex::CUBE_MAP) {
            f |= feat::ENV_MAP;
            if v.has(tex::CUBE_MAP_ALPHA) {
                f |= feat::CUBE_MAP_ALPHA;
            }
        }
        if refr {
            f |= feat::USE_REFRACTION;
        }
        if vars.refraction_normals && refr {
            f |= feat::USE_SCREEN_NORMAL;
        }
        if vars.refraction_alpha_blend && refr {
            f |= feat::ALPHA_BLEND_REFRACTION;
        }
        if g.light_probes && vars.affected_by_light_level {
            f |= feat::LIT;
        }
        if vars.soft_particle {
            f |= feat::SOFT_PARTICLE;
        }
        if vars.fade_color_on {
            f |= feat::FADE_COLOR;
        }
        if vars.angle_fade {
            f |= feat::ANGLE_FADE;
        }
        if skeleton {
            if !g.depth_of_field {
                return ProgSel::NONE;
            }
            f |= feat::DEPTH_OF_FIELD;
        }
        return ProgSel {
            manager: blend_manager(v.blend_mode),
            mode: mode::DIFFUSE,
            flags: f,
        };
    }
    if (render_mode == mode::ILLUMINATION || render_mode == mode::ILLUMINATION_FOG)
        && !refr
        && v.has(tex::CUBE_MAP)
    {
        let mut f = 0;
        if render_mode == mode::ILLUMINATION_FOG {
            f |= feat::FOG | g.world_fog;
        }
        if v.has(tex::NMAP) {
            f |= feat::NORMAL_MAP;
        }
        f |= feat::ENV_MAP;
        if v.has(tex::CUBE_MAP_ALPHA) {
            f |= feat::CUBE_MAP_ALPHA;
        }
        if g.light_probes && vars.affected_by_light_level {
            f |= feat::LIT;
        }
        return ProgSel {
            manager: blend_manager(blend::ADD),
            mode: mode::DIFFUSE,
            flags: f,
        };
    }
    ProgSel::NONE
}

/// `cMaterialType_Translucent::GetRenderProgram`: which program slot the renderer uses.
pub fn render_program_slot(render_mode: i32, g: &Globals) -> i32 {
    if !g.depth_of_field || (render_mode != mode::DIFFUSE && render_mode != mode::DIFFUSE_FOG) {
        0
    } else {
        1
    }
}

pub fn compile(v: &MatView, vars: &TranslucentVars, g: &Globals) -> CompileOut {
    let mut c = CompileOut::NONE;
    for m in [
        mode::DIFFUSE,
        mode::DIFFUSE_FOG,
        mode::ILLUMINATION,
        mode::ILLUMINATION_FOG,
    ] {
        c.specific(m, true);
        c.object_specific(m, true);
    }
    let refr = refraction_on(vars, g);
    if refr {
        c.has_refraction = 1;
        c.refraction_edge = i32::from(vars.refraction_edge_check);
    }
    if v.has(tex::CUBE_MAP) && !refr {
        c.translucent_illum = 1;
    }
    c
}

pub fn setup_type(render_mode: i32, r: &RenderView, out: &mut Uniforms) {
    if render_mode == mode::DIFFUSE || render_mode == mode::DIFFUSE_FOG {
        out.vec2(
            var::INV_SCREEN_SIZE,
            1.0f32 / (r.rt_size.x as f32),
            1.0f32 / (r.rt_size.y as f32),
        );
        out.float(var::NEAR_PLANE, r.near);
        out.float(var::INV_FAR_PLANE, 1.0f32 / r.far);
    }
}

pub fn setup_material(
    render_mode: i32,
    v: &MatView,
    vars: &TranslucentVars,
    g: &Globals,
    r: &RenderView,
    host: &mut dyn Host,
    out: &mut Uniforms,
) {
    let illumination = render_mode == mode::ILLUMINATION_FOG || render_mode == mode::ILLUMINATION;
    let refr = refraction_on(vars, g);

    if v.has_uv_anim {
        out.mat(var::UV, &v.uv_matrix);
    }

    // `cube && (refr && !illum) || (!refr && illum) || probes`, with the original's precedence
    if (v.has(tex::CUBE_MAP) && (refr && !illumination))
        || (!refr && illumination)
        || (g.light_probes && vars.affected_by_light_level)
    {
        out.mat(var::INV_VIEW_ROTATION, &inv_view_rotation(&r.view));
        out.vec2(var::FRENSEL_BIAS_POW, vars.frensel_bias, vars.frensel_pow);
        out.vec2(
            var::RIM_LIGHT_MUL_POW,
            vars.rim_light_mul,
            vars.rim_light_pow,
        );
    }

    if refr && (render_mode == mode::DIFFUSE_FOG || render_mode == mode::DIFFUSE) {
        let scale = if g.tex_rect {
            r.rt_size.x as f32
        } else {
            1.0f32
        };
        out.float(var::REFRACTION_SCALE, vars.refraction_scale * scale);
    }

    if vars.fade_color_on && !illumination {
        out.color(var::FADE_COLOR, vars.fade_color);
    }

    out.color(
        var::FADE_PROP,
        color(vars.fade_in_start, vars.fade_in_end, 0.0, 0.0),
    );

    if vars.soft_particle && !illumination {
        out.float(var::SOFT_PARTICLE_THICKNESS, vars.soft_part_thickness);
        out.float(
            var::SOFT_PARTICLE_ALPHA_BASED_THICKNESS,
            vars.soft_part_alpha_based_thickness,
        );
        out.float(var::SOFT_PARTICLE_DEPTH_BIAS, vars.soft_part_depth_bias);
    }

    if render_mode == mode::DIFFUSE_FOG || render_mode == mode::ILLUMINATION_FOG {
        let Some(w) = host.world() else { return };
        let fog = w.fog_active;
        let weight = fmax(vars.fog_weight, 0.0001);
        if fog {
            out.vec2(
                var::FOG_START_AND_LENGTH,
                w.fog_start,
                (w.fog_end - w.fog_start) / weight,
            );
        } else {
            out.vec2(var::FOG_START_AND_LENGTH, 0.0, 1.0);
        }
        out.float(var::ONE_MINUS_FOG_ALPHA, 1.0 - w.fog_color.a);
        out.float(var::FALLOFF_EXP, w.fog_falloff_exp);
        out.color(
            var::FOG_COLOR,
            if fog {
                fog_render_color(w.fog_color, w.fog_brightness, g.hdr)
            } else {
                color(0.0, 0.0, 0.0, 0.0)
            },
        );
        if g.world_fog & feat::SECONDARY_FOG != 0 {
            out.color(
                var::SECOND_FOG_COLOR,
                fog_render_color(w.sec_fog_color, w.sec_fog_brightness, g.hdr),
            );
            out.vec2(
                var::SECOND_FOG_START_AND_LENGTH,
                w.sec_fog_start,
                (w.sec_fog_end - w.sec_fog_start) / weight,
            );
            out.float(var::SECOND_FALLOFF_EXP, w.sec_fog_falloff_exp);
        }
    }
}

fn max_color_value(c: crate::types::Color) -> f32 {
    fmax(fmax(c.r, c.g), c.b)
}

#[allow(clippy::too_many_arguments)]
pub fn setup_object(
    render_mode: i32,
    vars: &TranslucentVars,
    g: &Globals,
    r: &RenderView,
    o: &ObjView,
    host: &mut dyn Host,
    out: &mut Uniforms,
) {
    if g.hdr {
        out.color(var::COLOR_MUL, o.color_mul);
    }
    if g.depth_of_field {
        out.color(var::DEPTH_OF_FIELD_PARAMS, g.dof_params);
    }
    if render_mode == mode::DIFFUSE_FOG || render_mode == mode::ILLUMINATION_FOG {
        out.color(var::FOG_AREA_COLOR, r.fog_area_color);
    }

    if g.light_probes && vars.affected_by_light_level {
        out.mat(var::LIGHT_PROBE, &probe::calc_light_probe(host));
        out.float(var::ALPHA, r.temp_alpha);
        out.float(var::LIGHT_LEVEL, 1.0);
    } else if vars.affected_by_light_level {
        // Light affects Alpha
        let center = host.object_bv().world_center;
        let mut amount = 0.0f32;
        for i in 0..host.light_count() {
            let Some(l) = host.light(i, false) else {
                continue;
            };
            if !host.light_intersects(i) {
                continue;
            }
            if l.kind == super::types::light_kind::BOX {
                amount += max_color_value(l.diffuse);
            } else {
                let dist = len3(sub3(center, l.pos));
                amount = fma(
                    max_color_value(l.diffuse),
                    fmax(1.0 - (dist / l.radius), 0.0),
                    amount,
                );
            }
            if amount >= 1.0 {
                amount = 1.0;
                break;
            }
        }
        out.float(var::ALPHA, r.temp_alpha);
        out.float(var::LIGHT_LEVEL, amount);
    } else {
        out.float(var::ALPHA, r.temp_alpha);
        out.float(var::LIGHT_LEVEL, 1.0);
    }
}
