//! `cMaterialType_Water`: refractive/reflective water surfaces with optional vertex waves.

use super::common::{color, fog_render_color, inv_view_rotation, model_or_identity};
use super::setup::{Feature, SetupSink, feature};
use super::types::{
    CompileOut, Globals, Host, MatView, ObjView, ProgSel, RenderView, TexSel, Uniforms, bit, blend,
    manager, mode, tex, texsel,
};
use super::vars::{WaterVars, v3};

mod var {
    pub const T: i32 = 0;
    pub const REFRACTION_SCALE: i32 = 1;
    pub const INV_VIEW_ROTATION: i32 = 2;
    pub const REFLECTION_MAP_SIZE_MUL: i32 = 3;
    pub const FRENSEL_BIAS_POW: i32 = 4;
    pub const REFLECTION_FADE_START_AND_LENGTH: i32 = 5;
    pub const WAVE_AMPLITUDE: i32 = 6;
    pub const WAVE_FREQ: i32 = 7;
    pub const FOG_START_AND_LENGTH: i32 = 8;
    pub const FOG_COLOR: i32 = 9;
    pub const FALLOFF_EXP: i32 = 10;
    pub const INV_SCREEN_SIZE: i32 = 11;
    pub const NEAR_PLANE: i32 = 12;
    pub const FAR_PLANE: i32 = 13;
    pub const REFLECTION_ALPHA: i32 = 14;
    pub const FADE_WHEN_SHALLOW_PROPS: i32 = 15;
    pub const MODEL: i32 = 16;
    pub const VTX_T: i32 = 17;
    pub const VTX_WAVE_AMPLITUDE: i32 = 18;
    pub const VTX_WAVE_FREQ: i32 = 19;
    pub const BLEND_MODE: i32 = 20;
    pub const FADE_WHEN_NEAR: i32 = 21;
    pub const FOG_CAMERA_POS: i32 = 22;
    pub const FOG_SETTINGS: i32 = 23;
    pub const FOG_TYPE: i32 = 24;
    pub const REFLECTION_MUL: i32 = 25;
}

mod feat {
    use super::bit;
    pub const REFLECTION: u32 = bit(0);
    pub const CUBE_MAP_REFLECTION: u32 = bit(1);
    pub const REFLECTION_FADING: u32 = bit(2);
    pub const FOG: u32 = bit(3);
    pub const FADE_WHEN_SHALLOW: u32 = bit(4);
    pub const VERTEX_WAVES: u32 = bit(5);
}

const VERTEX: u32 = bit(0);
const FRAGMENT: u32 = bit(1);

static FEATURES: [Feature; 6] = [
    feature(c"UseReflection", FRAGMENT | VERTEX, 0),
    feature(c"UseCubeMapReflection", FRAGMENT, feat::REFLECTION),
    feature(c"UseReflectionFading", FRAGMENT, feat::REFLECTION),
    feature(c"UseFog", FRAGMENT | VERTEX, 0),
    feature(c"UseFadeWhenShallow", FRAGMENT, 0),
    feature(c"UseVertexWaves", VERTEX, 0),
];

const VAR_IDS: [(&str, i32); 27] = [
    ("afT", var::T),
    ("afRefractionScale", var::REFRACTION_SCALE),
    ("a_mtxInvViewRotation", var::INV_VIEW_ROTATION),
    ("a_mtxInvView", var::INV_VIEW_ROTATION),
    ("avReflectionMapSizeMul", var::REFLECTION_MAP_SIZE_MUL),
    ("avFrenselBiasPow", var::FRENSEL_BIAS_POW),
    (
        "avReflectionFadeStartAndLength",
        var::REFLECTION_FADE_START_AND_LENGTH,
    ),
    ("afWaveAmplitude", var::WAVE_AMPLITUDE),
    ("afWaveFreq", var::WAVE_FREQ),
    ("avFogStartAndLength", var::FOG_START_AND_LENGTH),
    ("avFogColor", var::FOG_COLOR),
    ("afFalloffExp", var::FALLOFF_EXP),
    ("avInvScreenSize", var::INV_SCREEN_SIZE),
    ("afNearPlane", var::NEAR_PLANE),
    ("afFarPlane", var::FAR_PLANE),
    ("afReflectionAlpha", var::REFLECTION_ALPHA),
    ("avFadeWhenShallowProps", var::FADE_WHEN_SHALLOW_PROPS),
    ("a_mtxModel", var::MODEL),
    ("afVtxT", var::VTX_T),
    ("afVtxWaveAmplitude", var::VTX_WAVE_AMPLITUDE),
    ("afVtxWaveFreq", var::VTX_WAVE_FREQ),
    ("mlBlendMode", var::BLEND_MODE),
    ("mfFadeWhenNear", var::FADE_WHEN_NEAR),
    ("avFogCameraPos", var::FOG_CAMERA_POS),
    ("avFogSettings", var::FOG_SETTINGS),
    ("alFogType", var::FOG_TYPE),
    ("avReflectionMul", var::REFLECTION_MUL),
];

/// `cMaterialType_Water::LoadData`; `has_water_vtx` is whether `water_surface_vtx.hpsl` exists.
pub fn setup(s: &mut dyn SetupSink, g: &Globals) {
    s.clear_defines();
    for d in [
        "UseUv",
        "UseNormals",
        "UseVertexPosition",
        "UseRefractionEdgeCheck",
        "UseNormals",
        "UseNormalMapping",
    ] {
        s.define(d);
    }
    if g.refraction {
        s.define("UseRefraction");
    }
    // SOMA ships a dedicated water vertex shader (vertex waves)
    let vtx = if g.has_water_vtx {
        "water_surface_vtx.glsl"
    } else {
        "deferred_base_vtx.glsl"
    };
    s.setup_program(
        manager::OWN,
        mode::DIFFUSE,
        "Diffuse",
        vtx,
        "water_surface_frag.glsl",
        &FEATURES,
    );
    for (name, id) in VAR_IDS {
        s.var_id(manager::OWN, name, id, mode::DIFFUSE);
    }
}

/// `cMaterialType_Water::DestroyProgram`
pub fn destroy_target() -> ProgSel {
    ProgSel::own(mode::DIFFUSE, 0)
}

pub fn texture(v: &MatView, vars: &WaterVars, g: &Globals, render_mode: i32, unit: i32) -> TexSel {
    match render_mode {
        mode::Z if unit == 0 => TexSel::slot_if(v, tex::DIFFUSE),
        mode::DIFFUSE | mode::DIFFUSE_FOG => match unit {
            0 => TexSel::slot_if(v, tex::DIFFUSE),
            1 => TexSel::slot_if(v, tex::NMAP),
            7 if g.refraction || vars.fade_when_shallow_mul > 0.0 => {
                TexSel::special(texsel::SCENE_DEPTH)
            }
            2 if g.refraction => TexSel::special(texsel::REFRACTION),
            3 if g.refraction => {
                if v.has(tex::CUBE_MAP) {
                    TexSel::slot(tex::CUBE_MAP)
                } else {
                    TexSel::special(texsel::REFLECTION)
                }
            }
            _ => TexSel::NONE,
        },
        _ => TexSel::NONE,
    }
}

pub fn program(v: &MatView, vars: &WaterVars, g: &Globals, render_mode: i32) -> ProgSel {
    if render_mode != mode::DIFFUSE && render_mode != mode::DIFFUSE_FOG {
        return ProgSel::NONE;
    }
    let mut f = 0;
    if g.refraction {
        if vars.has_reflection {
            f |= feat::REFLECTION;
        }
        if v.has(tex::CUBE_MAP) {
            f |= feat::CUBE_MAP_REFLECTION;
        }
    }
    if vars.reflection_fade_end > 0.0 {
        f |= feat::REFLECTION_FADING;
    }
    if render_mode == mode::DIFFUSE_FOG {
        f |= feat::FOG;
    }
    if vars.fade_when_shallow_mul > 0.0 {
        f |= feat::FADE_WHEN_SHALLOW;
    }
    if vars.has_vertex_waves {
        f |= feat::VERTEX_WAVES;
    }
    ProgSel::own(mode::DIFFUSE, f)
}

pub fn compile(v: &MatView, vars: &WaterVars, g: &Globals) -> CompileOut {
    let mut c = CompileOut::NONE;
    c.specific(mode::DIFFUSE, true);
    c.specific(mode::DIFFUSE_FOG, true);
    c.object_specific(mode::DIFFUSE, vars.has_vertex_waves);
    c.object_specific(mode::DIFFUSE_FOG, vars.has_vertex_waves);
    c.blend_mode = if g.refraction {
        blend::NONE
    } else {
        blend::MUL
    };
    if g.refraction {
        c.has_refraction = 1;
        c.refraction_edge = 1;
    }
    if vars.has_reflection && g.refraction && !v.has(tex::CUBE_MAP) {
        c.world_reflection = 1;
    }
    c
}

pub fn setup_material(
    render_mode: i32,
    v: &MatView,
    vars: &WaterVars,
    g: &Globals,
    r: &RenderView,
    host: &mut dyn Host,
    out: &mut Uniforms,
) {
    if render_mode != mode::DIFFUSE && render_mode != mode::DIFFUSE_FOG {
        return;
    }
    out.float(var::T, r.time * vars.wave_speed);
    out.float(
        var::REFRACTION_SCALE,
        vars.refraction_scale * (r.rt_size.x as f32),
    );
    out.float(var::WAVE_AMPLITUDE, vars.wave_amplitude * 0.04f32);
    out.float(var::WAVE_FREQ, vars.wave_freq * 10.0f32);
    if vars.has_vertex_waves {
        out.float(var::VTX_T, r.time * vars.vertex_wave_speed);
        out.float(var::VTX_WAVE_AMPLITUDE, vars.vertex_wave_amplitude);
        out.float(var::VTX_WAVE_FREQ, vars.vertex_wave_freq);
    }

    let Some(w) = host.world() else { return };
    let world_fog = w.fog_active;

    // the water shader has no fog area input; an enclosing fog area replaces world fog
    let area = r.fog_area_color;
    if area.a > 0.0 {
        out.vec2(var::FOG_START_AND_LENGTH, -1.0, 1.0);
        out.color(var::FOG_COLOR, area);
        out.float(var::FALLOFF_EXP, 1.0);
        out.color(var::FOG_SETTINGS, color(-1.0, 1.0, 1.0, 0.0));
        out.int(var::FOG_TYPE, 0);
    } else {
        if world_fog {
            out.vec2(
                var::FOG_START_AND_LENGTH,
                w.fog_start,
                w.fog_end - w.fog_start,
            );
            out.color(
                var::FOG_COLOR,
                fog_render_color(w.fog_color, w.fog_brightness, g.hdr),
            );
        } else {
            out.vec2(var::FOG_START_AND_LENGTH, 0.0, 1.0);
            out.color(var::FOG_COLOR, color(0.0, 0.0, 0.0, 0.0));
        }
        out.float(var::FALLOFF_EXP, w.fog_falloff_exp);

        let exp = world_fog && w.fog_exponential;
        let height = world_fog && w.fog_height_based;
        let mut settings = if world_fog {
            color(w.fog_start, w.fog_end - w.fog_start, w.fog_falloff_exp, 0.0)
        } else {
            color(0.0, 1.0, 1.0, 0.0)
        };
        let mut cam = v3(r.origin.y, r.origin.y, r.origin.y);
        if exp || height {
            settings.b = w.fog_density * 0.03125f32;
            settings.a = w.fog_height_density;
            cam.x -= w.fog_height_horizon;
        }
        out.color(var::FOG_SETTINGS, settings);
        out.vec3(var::FOG_CAMERA_POS, cam);
        out.int(var::FOG_TYPE, i32::from(exp) | (if height { 2 } else { 0 }));
    }
    out.int(var::BLEND_MODE, v.blend_mode);
    out.color(var::REFLECTION_MUL, color(1.0, 1.0, 1.0, 1.0));

    out.vec2(
        var::INV_SCREEN_SIZE,
        1.0f32 / (r.rt_size.x as f32),
        1.0f32 / (r.rt_size.y as f32),
    );
    out.float(var::NEAR_PLANE, r.near);
    out.float(var::FAR_PLANE, r.far);
    out.float(var::FADE_WHEN_NEAR, r.near * 2.0f32);
    out.float(var::REFLECTION_ALPHA, 1.0);
    if vars.fade_when_shallow_mul > 0.0 {
        out.vec2(
            var::FADE_WHEN_SHALLOW_PROPS,
            1.0f32 / vars.fade_when_shallow_mul,
            vars.fade_when_shallow_pow,
        );
    }

    if vars.has_reflection && g.refraction {
        out.vec2(var::FRENSEL_BIAS_POW, vars.frensel_bias, vars.frensel_pow);
        if v.has(tex::CUBE_MAP) {
            out.mat(var::INV_VIEW_ROTATION, &inv_view_rotation(&r.view));
        } else {
            let s = 1.0f32 / (g.reflection_size_div as f32);
            out.vec2(var::REFLECTION_MAP_SIZE_MUL, s, s);
        }
        if vars.reflection_fade_end > 0.0 {
            let length = vars.reflection_fade_end - vars.reflection_fade_start;
            out.vec2(
                var::REFLECTION_FADE_START_AND_LENGTH,
                -vars.reflection_fade_start,
                -length,
            );
        }
    }
}

pub fn setup_object(o: &ObjView, out: &mut Uniforms) {
    out.mat(var::MODEL, &model_or_identity(o.has_model, &o.model));
}
