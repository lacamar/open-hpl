//! Per-type material variables: their `#[repr(C)]` layouts (the `Ohpl*Vars` bases of the C++
//! `*_Vars` classes), `.mat` `SpecificVariables` loading with defaults, the user-variable tables
//! the editor lists, and `GetVariableValues`.

use super::Kind;
use crate::types::{Color, Vector2f, Vector3f};

/// `cResourceVarsObject::GetVar*` with the value strings parsed by the C++ side.
pub trait VarSource {
    fn boolean(&mut self, name: &str, def: bool) -> bool;
    fn float(&mut self, name: &str, def: f32) -> f32;
    fn vec2(&mut self, name: &str, def: Vector2f) -> Vector2f;
    fn vec3(&mut self, name: &str, def: Vector3f) -> Vector3f;
    fn color(&mut self, name: &str, def: Color) -> Color;
    fn string(&mut self, name: &str, def: &str) -> Vec<u8>;
}

/// `cResourceVarsObject::AddVar*`
pub trait VarSink {
    fn add_bool(&mut self, name: &str, v: bool);
    fn add_float(&mut self, name: &str, v: f32);
}

pub(crate) const fn v3(x: f32, y: f32, z: f32) -> Vector3f {
    Vector3f { x, y, z }
}

const fn col(r: f32, g: f32, b: f32, a: f32) -> Color {
    Color { r, g, b, a }
}

pub(crate) const LIQUID_BLEND_OTHER: i32 = 0;
pub(crate) const LIQUID_BLEND_ALPHA: i32 = 1;
pub(crate) const LIQUID_BLEND_MUL: i32 = 2;

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct SolidVars {
    pub height_map_scale: f32,
    pub height_map_bias: f32,
    pub frensel_bias: f32,
    pub frensel_pow: f32,
    pub alpha_dissolve_filter: bool,
    pub sway_active: bool,
    pub sway_single_dir: bool,
    pub sway_properties: Vector3f,
    pub sway_octave_muls: Vector3f,
    pub sway_single_dir_vec: Vector3f,
    pub sway_single_sample_dir: Vector3f,
    pub sway_y_freq_mul: f32,
    pub sway_force_field_affected: bool,
    pub sway_force_field_mul: f32,
    pub sway_force_field_max: f32,
    pub detail_properties: [f32; 4],
    pub detail_weights: Vector3f,
    pub illumination_brightness: f32,
    pub liquid_color: Color,
    pub liquid_specular: Color,
    pub liquid_loop_fade: bool,
    pub liquid_fade_speed: Vector2f,
    pub liquid_edge_size: f32,
    pub liquid_dryness: f32,
    pub liquid_blend_mode: i32,
}

impl Default for SolidVars {
    fn default() -> Self {
        Self {
            height_map_scale: 0.05,
            height_map_bias: 0.0,
            frensel_bias: 0.0,
            frensel_pow: 0.0,
            alpha_dissolve_filter: false,
            sway_active: false,
            sway_single_dir: false,
            sway_properties: v3(0.0, 0.0, 0.0),
            sway_octave_muls: v3(0.0, 0.0, 0.0),
            sway_single_dir_vec: v3(0.0, 0.0, 0.0),
            sway_single_sample_dir: v3(0.0, 0.0, 0.0),
            sway_y_freq_mul: 0.0,
            sway_force_field_affected: false,
            sway_force_field_mul: 0.0,
            sway_force_field_max: 0.0,
            detail_properties: [0.0; 4],
            detail_weights: v3(0.0, 0.0, 0.0),
            illumination_brightness: 1.0,
            liquid_color: col(0.0, 0.0, 0.0, 0.0),
            liquid_specular: col(0.0, 0.0, 0.0, 0.0),
            liquid_loop_fade: false,
            liquid_fade_speed: Vector2f { x: 0.0, y: 0.0 },
            liquid_edge_size: 0.5,
            liquid_dryness: 0.5,
            liquid_blend_mode: LIQUID_BLEND_OTHER,
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct ProjectedUvVars {
    pub blend_hardness: f32,
    pub normal_map_blend_impact: f32,
    pub texture_scale: Vector3f,
    pub dynamic_object_support: bool,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct VertexBlendVars {
    pub normal_blend_hardness: f32,
    pub color_blend_hardness: f32,
    pub texture_scale: Vector3f,
    pub height_map_scale: Vector3f,
    pub directional_weight: [Color; 3],
    pub auto_uv: [bool; 3],
    pub alpha_mask: bool,
    pub overlay_normal: bool,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct TranslucentVars {
    pub refraction: bool,
    pub refraction_edge_check: bool,
    pub refraction_normals: bool,
    pub refraction_alpha_blend: bool,
    pub fog_weight: f32,
    pub refraction_scale: f32,
    pub frensel_bias: f32,
    pub frensel_pow: f32,
    pub rim_light_mul: f32,
    pub rim_light_pow: f32,
    pub affected_by_light_level: bool,
    pub soft_particle: bool,
    pub soft_part_thickness: f32,
    pub soft_part_alpha_based_thickness: f32,
    pub soft_part_depth_bias: f32,
    pub fade_in_start: f32,
    pub fade_in_end: f32,
    pub fade_color_on: bool,
    pub angle_fade: bool,
    pub fade_color: Color,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct WaterVars {
    pub has_reflection: bool,
    pub refraction_scale: f32,
    pub frensel_bias: f32,
    pub frensel_pow: f32,
    pub reflection_fade_start: f32,
    pub reflection_fade_end: f32,
    pub wave_speed: f32,
    pub wave_amplitude: f32,
    pub wave_freq: f32,
    pub fade_when_shallow_mul: f32,
    pub fade_when_shallow_pow: f32,
    pub has_vertex_waves: bool,
    pub vertex_wave_speed: f32,
    pub vertex_wave_amplitude: f32,
    pub vertex_wave_freq: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct TerrainBlendVars {
    pub texture_coord_scale: [f32; 4],
    pub one_minus_fade_start: [f32; 4],
    pub blend_mul: [f32; 4],
    pub base_texture_coord_scale: f32,
    pub detail_map_scale: Vector3f,
    pub detail_fade: Vector3f,
    pub base_detail_amount: Vector3f,
    pub detail_amount: [Vector3f; 4],
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct UndergrowthVars {
    pub wind: bool,
    pub wind_vec: Vector3f,
    pub wind_octaves: Vector3f,
    pub dissolve: Vector2f,
    pub force_field_mul: f32,
    pub max_force_field_force: f32,
}

const fn vec2(x: f32, y: f32) -> Vector2f {
    Vector2f { x, y }
}

/// `cMaterialType_SolidDiffuse::LoadVariables`
pub fn load_solid(v: &mut SolidVars, s: &mut dyn VarSource) {
    v.height_map_scale = s.float("HeightMapScale", 0.1);
    v.height_map_bias = s.float("HeightMapBias", 0.0);
    v.frensel_bias = s.float("FrenselBias", 0.2);
    v.frensel_pow = s.float("FrenselPow", 8.0);
    v.alpha_dissolve_filter = s.boolean("AlphaDissolveFilter", false);
    v.sway_active = s.boolean("SwayActive", false);
    v.sway_single_dir = s.boolean("SwaySingleDir", false);
    v.sway_properties = v3(
        s.float("SwayFreq", 1.0),
        s.float("SwayAmplitude", 0.1),
        s.float("SwaySpeed", 1.0),
    );
    v.sway_octave_muls = s.vec3("SwayOctaveMuls", v3(0.125, 0.25, 1.0));
    v.sway_y_freq_mul = s.float("SwayYFreqMul", 0.0);
    v.sway_single_dir_vec = s.vec3("SwaySingleDirVector", v3(0.0, 0.0, 1.0));
    v.sway_single_sample_dir = s.vec3("SwaySingleSampleVector", v3(1.0, 0.0, 0.0));
    v.sway_force_field_affected = s.boolean("SwayForceFieldAffected", false);
    v.sway_force_field_mul = s.float("SwayForceFieldMul", 0.0);
    v.sway_force_field_max = s.float("SwayForceFieldMax", 0.0);
    let fade_start = s.float("DetailFadeStart", 5.0);
    let uv_mul = s.vec2("DetailUvMul", vec2(4.0, 4.0));
    v.detail_properties = [
        fade_start,
        s.float("DetailFadeEnd", 10.0) - fade_start,
        uv_mul.x,
        uv_mul.y,
    ];
    v.detail_weights = v3(
        s.float("DetailWeight_Diffuse", 1.0),
        s.float("DetailWeight_Specular", 1.0),
        s.float("DetailWeight_Normal", 1.0),
    );
    v.illumination_brightness = s.float("IlluminationBrightness", 1.0);
    v.liquid_color = s.color("LiquidTrickleColor", col(0.0, 0.0, 0.0, 1.0));
    v.liquid_specular = s.color("LiquidTrickleSpecular", col(0.0, 0.0, 0.0, 0.0));
    v.liquid_loop_fade = s.boolean("LiquidTrickleLoopFade", false);
    v.liquid_fade_speed = s.vec2("LiquidTrickleFadeSpeed", vec2(0.5, 0.5));
    v.liquid_edge_size = s.float("LiquidTrickleEdgeSize", 0.5);
    v.liquid_dryness = s.float("LiquidTrickleDryness", 0.5);
    v.liquid_blend_mode = match s.string("LiquidTrickleBlendMode", "Alpha").as_slice() {
        b"Alpha" => LIQUID_BLEND_ALPHA,
        b"Mul" => LIQUID_BLEND_MUL,
        _ => LIQUID_BLEND_OTHER,
    };
}

pub fn solid_values(v: &SolidVars, k: &mut dyn VarSink) {
    k.add_float("HeightMapScale", v.height_map_scale);
    k.add_float("HeightMapBias", v.height_map_bias);
    k.add_float("FrenselBias", v.frensel_bias);
    k.add_float("FrenselPow", v.frensel_pow);
    k.add_bool("AlphaDissolveFilter", v.alpha_dissolve_filter);
}

/// `std::max`-free clamp as `cMath::Clamp`.
fn clamp(x: f32, lo: f32, hi: f32) -> f32 {
    crate::math::clamp(x, lo, hi)
}

pub fn load_projected_uv(v: &mut ProjectedUvVars, s: &mut dyn VarSource) {
    v.blend_hardness = clamp(1.0 - s.float("BlendSmoothness", 0.5), 0.0, 0.99);
    v.normal_map_blend_impact = clamp(s.float("NormalMapBlendImpact", 0.0), 0.0, 1.0);
    v.texture_scale = v3(
        1.0 / s.float("TextureUVScaleSide", 1.0),
        1.0 / s.float("TextureUVScaleTop", 1.0),
        1.0 / s.float("TextureUVScaleBottom", 1.0),
    );
    v.dynamic_object_support = s.boolean("DynamicObjectSupport", false);
}

pub fn load_vertex_blend(v: &mut VertexBlendVars, s: &mut dyn VarSource) {
    let smoothness = s.float("BlendSmoothness", 0.5);
    v.normal_blend_hardness = clamp(1.0 - smoothness, 0.0, 0.99);
    v.color_blend_hardness = clamp(1.0 - s.float("ColorBlendSmoothness", smoothness), 0.0, 0.99);
    let names = ["R", "G", "B"];
    let [sx, sy, sz] = names.map(|c| s.float(&format!("TextureUVScale{c}"), 1.0));
    let [hx, hy, hz] = names.map(|c| s.float(&format!("HeightMapScale{c}"), 0.1));
    v.texture_scale = v3(sx, sy, sz);
    v.height_map_scale = v3(hx, hy, hz);
    v.auto_uv = names.map(|c| s.boolean(&format!("AutoUv{c}"), true));
    v.directional_weight =
        names.map(|c| s.color(&format!("DirectionalWeight{c}"), col(0.0, 0.0, 0.0, 1.0)));
    v.alpha_mask = s.boolean("AlphaMask", true);
    v.overlay_normal = s.boolean("OverlayNormal", false);
}

/// Settings `cMaterial` takes from a translucent `.mat`.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct LoadOut {
    pub affected_by_dof: i32,
    pub world_reflection_occlusion_test: i32,
    pub max_reflection_distance: f32,
    pub large_transparent_surface: i32,
    pub has_max_reflection_distance: bool,
}

impl LoadOut {
    pub const NONE: LoadOut = LoadOut {
        affected_by_dof: -1,
        world_reflection_occlusion_test: -1,
        max_reflection_distance: 0.0,
        large_transparent_surface: -1,
        has_max_reflection_distance: false,
    };
}

pub fn load_translucent(v: &mut TranslucentVars, s: &mut dyn VarSource, out: &mut LoadOut) {
    v.refraction = s.boolean("Refraction", false);
    v.refraction_edge_check = s.boolean("RefractionEdgeCheck", true);
    v.refraction_normals = s.boolean("RefractionNormals", true);
    v.refraction_alpha_blend = s.boolean("RefractionAlphaBlend", false);
    v.fog_weight = clamp(s.float("FogWeight", 1.0), 0.0, 1.0);
    out.affected_by_dof = i32::from(s.boolean("AffectedByDepthOfField", true));
    v.refraction_scale = s.float("RefractionScale", 1.0);
    v.frensel_bias = s.float("FrenselBias", 0.2);
    v.frensel_pow = s.float("FrenselPow", 8.0);
    v.rim_light_mul = s.float("RimLightMul", 0.0);
    v.rim_light_pow = s.float("RimLightPow", 8.0);
    v.affected_by_light_level = s.boolean("AffectedByLightLevel", false);
    v.soft_particle = s.boolean("SoftParticleActive", false);
    v.angle_fade = s.boolean("AngleFadeActive", false);
    v.soft_part_thickness = s.float("SoftPartThickness", 1.0);
    v.soft_part_alpha_based_thickness = s.float("SoftPartAlphaBasedThickness", 0.0);
    v.soft_part_depth_bias = s.float("SoftPartDepthBias", 0.0);
    v.fade_in_start = s.float("FadeInStart", 0.0);
    v.fade_in_end = s.float("FadeInEnd", 0.0);
}

pub fn translucent_values(v: &TranslucentVars, affected_by_dof: bool, k: &mut dyn VarSink) {
    k.add_bool("Refraction", v.refraction);
    k.add_bool("RefractionEdgeCheck", v.refraction_edge_check);
    k.add_bool("RefractionNormals", v.refraction_normals);
    k.add_bool("RefractionAlphaBlend", v.refraction_alpha_blend);
    k.add_float("FogWeight", v.fog_weight);
    k.add_bool("AffectedByDepthOfField", affected_by_dof);
    k.add_float("RefractionScale", v.refraction_scale);
    k.add_float("FrenselBias", v.frensel_bias);
    k.add_float("FrenselPow", v.frensel_pow);
    k.add_float("RimLightMul", v.rim_light_mul);
    k.add_float("RimLightPow", v.rim_light_pow);
    k.add_bool("AffectedByLightLevel", v.affected_by_light_level);
}

pub fn load_water(v: &mut WaterVars, s: &mut dyn VarSource, out: &mut LoadOut) {
    v.has_reflection = s.boolean("HasReflection", true);
    v.refraction_scale = s.float("RefractionScale", 0.1);
    v.frensel_bias = s.float("FrenselBias", 0.2);
    v.frensel_pow = s.float("FrenselPow", 8.0);
    v.reflection_fade_start = s.float("ReflectionFadeStart", 0.0);
    v.reflection_fade_end = s.float("ReflectionFadeEnd", 0.0);
    v.wave_speed = s.float("WaveSpeed", 1.0);
    v.wave_amplitude = s.float("WaveAmplitude", 1.0);
    v.wave_freq = s.float("WaveFreq", 1.0);
    v.fade_when_shallow_mul = s.float("FadeWhenShallowMul", 0.0);
    v.fade_when_shallow_pow = s.float("FadeWhenShallowPow", 1.0);
    v.has_vertex_waves = s.boolean("HasVertexWaves", false);
    v.vertex_wave_speed = s.float("VertexWaveSpeed", 1.0);
    v.vertex_wave_amplitude = s.float("VertexWaveAmplitude", 1.0);
    v.vertex_wave_freq = s.float("VertexWaveFreq", 1.0);
    out.world_reflection_occlusion_test =
        i32::from(s.boolean("OcclusionCullWorldReflection", true));
    out.max_reflection_distance = s.float("ReflectionFadeEnd", 0.0);
    out.has_max_reflection_distance = true;
    out.large_transparent_surface = i32::from(s.boolean("LargeSurface", false));
}

/// `world_reflection_occlusion_test` and `large_transparent_surface` come from the material.
pub fn water_values(v: &WaterVars, occlusion_test: bool, large_surface: bool, k: &mut dyn VarSink) {
    k.add_bool("HasReflection", v.has_reflection);
    k.add_float("RefractionScale", v.refraction_scale);
    k.add_float("FrenselBias", v.frensel_bias);
    k.add_float("FrenselPow", v.frensel_pow);
    k.add_float("ReflectionFadeStart", v.reflection_fade_start);
    k.add_float("ReflectionFadeEnd", v.reflection_fade_end);
    k.add_float("WaveSpeed", v.wave_speed);
    k.add_float("WaveAmplitude", v.wave_amplitude);
    k.add_float("WaveFreq", v.wave_freq);
    k.add_float("FadeWhenShallowMul", v.fade_when_shallow_mul);
    k.add_float("FadeWhenShallowPow", v.fade_when_shallow_pow);
    k.add_bool("HasVertexWaves", v.has_vertex_waves);
    k.add_float("VertexWaveSpeed", v.vertex_wave_speed);
    k.add_float("VertexWaveAmplitude", v.vertex_wave_amplitude);
    k.add_float("VertexWaveFreq", v.vertex_wave_freq);
    k.add_bool("OcclusionCullWorldReflection", occlusion_test);
    k.add_bool("LargeSurface", large_surface);
}

pub fn default_translucent() -> TranslucentVars {
    TranslucentVars {
        refraction: false,
        refraction_edge_check: true,
        refraction_normals: false,
        refraction_alpha_blend: false,
        fog_weight: 1.0,
        refraction_scale: 0.1,
        frensel_bias: 0.2,
        frensel_pow: 8.0,
        rim_light_mul: 0.0,
        rim_light_pow: 0.0,
        affected_by_light_level: false,
        soft_particle: false,
        soft_part_thickness: 0.0,
        soft_part_alpha_based_thickness: 0.0,
        soft_part_depth_bias: 0.0,
        fade_in_start: 0.0,
        fade_in_end: 0.0,
        fade_color_on: false,
        angle_fade: false,
        fade_color: col(0.0, 0.0, 0.0, 0.0),
    }
}

pub fn default_water() -> WaterVars {
    WaterVars {
        has_reflection: true,
        refraction_scale: 0.1,
        frensel_bias: 0.2,
        frensel_pow: 8.0,
        reflection_fade_start: 0.0,
        reflection_fade_end: 0.0,
        wave_speed: 0.0,
        wave_amplitude: 0.0,
        wave_freq: 0.0,
        fade_when_shallow_mul: 0.0,
        fade_when_shallow_pow: 1.0,
        has_vertex_waves: false,
        vertex_wave_speed: 1.0,
        vertex_wave_amplitude: 1.0,
        vertex_wave_freq: 1.0,
    }
}

pub fn default_terrain_blend() -> TerrainBlendVars {
    let zero = v3(0.0, 0.0, 0.0);
    TerrainBlendVars {
        texture_coord_scale: [1.0; 4],
        one_minus_fade_start: [1.0; 4],
        blend_mul: [1.0; 4],
        base_texture_coord_scale: 1.0,
        detail_map_scale: zero,
        detail_fade: zero,
        base_detail_amount: zero,
        detail_amount: [zero; 4],
    }
}

pub fn default_undergrowth() -> UndergrowthVars {
    UndergrowthVars {
        wind: false,
        wind_vec: v3(0.0, 0.0, 0.0),
        wind_octaves: v3(0.0, 0.0, 0.0),
        dissolve: vec2(0.0, 0.0),
        force_field_mul: 0.0,
        max_force_field_force: 0.0,
    }
}

pub fn default_projected_uv() -> ProjectedUvVars {
    ProjectedUvVars {
        blend_hardness: 0.0,
        normal_map_blend_impact: 0.0,
        texture_scale: v3(0.0, 0.0, 0.0),
        dynamic_object_support: false,
    }
}

pub fn default_vertex_blend() -> VertexBlendVars {
    VertexBlendVars {
        normal_blend_hardness: 0.0,
        color_blend_hardness: 0.0,
        texture_scale: v3(0.0, 0.0, 0.0),
        height_map_scale: v3(0.0, 0.0, 0.0),
        directional_weight: [col(0.0, 0.0, 0.0, 0.0); 3],
        auto_uv: [false; 3],
        alpha_mask: false,
        overlay_normal: false,
    }
}

/// A variable of the editor-visible user variable list.
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum VarDefault {
    Bool(bool),
    Float(f32),
}

#[derive(Clone, Copy, Debug)]
pub struct VarDef {
    pub name: &'static str,
    pub default: VarDefault,
    pub desc: &'static str,
}

const fn vb(name: &'static str, b: bool, desc: &'static str) -> VarDef {
    VarDef {
        name,
        default: VarDefault::Bool(b),
        desc,
    }
}

const fn vf(name: &'static str, f: f32, desc: &'static str) -> VarDef {
    VarDef {
        name,
        default: VarDefault::Float(f),
        desc,
    }
}

static SOLID_VARS: [VarDef; 5] = [
    vf("HeightMapScale", 0.05, ""),
    vf("HeightMapBias", 0.0, ""),
    vf(
        "FrenselBias",
        0.2,
        "Bias for Fresnel term. values: 0-1. Higher means that more of reflection is seen when looking straight at object.",
    ),
    vf(
        "FrenselPow",
        8.0,
        "The higher the 'sharper' the reflection is, meaning that it is only clearly seen at sharp angles.",
    ),
    vb(
        "AlphaDissolveFilter",
        false,
        "If alpha values between 0 and 1 should be used and dissolve the texture. This can be useful for things like hair.",
    ),
];

const REFRACTION_NORMALS: &str =
    "If normals should be used when refracting. If no NMap is set this is forced true!";

// "RefractionNormals" is declared twice, as in the original.
static TRANSLUCENT_VARS: [VarDef; 10] = [
    vb(
        "Refraction",
        false,
        "If the material has refraction (distortion of bg). Uses NMap and/or normals of mesh",
    ),
    vb(
        "RefractionEdgeCheck",
        true,
        "If true, there is no bleeding with foreground objects, but takes some extra power.",
    ),
    vb("RefractionNormals", false, REFRACTION_NORMALS),
    vb("RefractionNormals", false, REFRACTION_NORMALS),
    vf(
        "RefractionScale",
        0.1,
        "The amount refraction offsets the background",
    ),
    vf(
        "FrenselBias",
        0.2,
        "Bias for Fresnel term. values: 0-1. Higher means that more of reflection is seen when looking straight at the surface.",
    ),
    vf(
        "FrenselPow",
        8.0,
        "The higher the 'sharper' the reflection is, meaning that it is only clearly seen at sharp angles.",
    ),
    vf(
        "RimLightMul",
        0.0,
        "The amount of rim light based on the reflection. This gives an edge to the object. Values: 0 - inf (although 1.0f should be used for max)",
    ),
    vf("RimLightPow", 8.0, "The sharpness of the rim lighting."),
    vb(
        "AffectedByLightLevel",
        false,
        "The the material alpha is affected by the light level.",
    ),
];

static WATER_VARS: [VarDef; 13] = [
    vf(
        "RefractionScale",
        0.1,
        "The amount reflection and refraction is offset by ripples in water.",
    ),
    vf(
        "FrenselBias",
        0.2,
        "Bias for Fresnel term. values: 0-1. Higher means that more of reflection is seen when looking straight at water.",
    ),
    vf(
        "FrenselPow",
        8.0,
        "The higher the 'sharper' the reflection is, meaning that it is only clearly seen at sharp angles.",
    ),
    vf("WaveSpeed", 1.0, "The speed of the waves."),
    vf("WaveAmplitude", 1.0, "The size of the waves."),
    vf("WaveFreq", 1.0, "The frequency of the waves."),
    vf(
        "ReflectionFadeStart",
        0.0,
        "Where the reflection starts fading.",
    ),
    vf(
        "ReflectionFadeEnd",
        0.0,
        "Where the reflection stops fading. 0 or less means no fading.",
    ),
    vf(
        "FadeWhenShallowMul",
        0.0,
        "Depth over which the water fades in. 0 means no fading.",
    ),
    vf("FadeWhenShallowPow", 1.0, "Exponent of the shallow fade."),
    vb(
        "HasReflection",
        true,
        "If a reflection should be shown or not.",
    ),
    vb(
        "OcclusionCullWorldReflection",
        true,
        "If occlusion culling should be used on reflection.",
    ),
    vb(
        "LargeSurface",
        true,
        "If the water will cover a large surface and will need special sorting when rendering other transperant objects.",
    ),
];

/// `AddVar*` calls of the type's constructor, in order.
pub fn user_vars(kind: Kind) -> &'static [VarDef] {
    match kind {
        Kind::SolidDiffuse => &SOLID_VARS,
        Kind::Translucent => &TRANSLUCENT_VARS,
        Kind::Water => &WATER_VARS,
        _ => &[],
    }
}
