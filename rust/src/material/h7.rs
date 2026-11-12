//! H7 state: `cMaterial` settings and UV animation, the `iMaterialType` and `iPostEffectType`
//! registries (traits, looked up by the names `cGraphics` registers them under) and the
//! `iPostEffect` base state.

use super::anim::{UvAnim, uv_matrix};
use super::types::{Kind, alpha_mode, blend};
use super::{VarDef, type_flags, used_textures, user_vars};
use crate::math::IDENTITY;
use crate::types::Matrixf;

/// The part of `cMaterial` that is plain state.
pub struct MatState {
    translucent: bool,
    pub blend: i32,
    pub alpha: i32,
    pub depth_test: bool,
    anims: Vec<UvAnim>,
    pub has_uv: bool,
    pub uv: Matrixf,
    time: f32,
}

impl MatState {
    pub fn new(translucent: bool) -> Self {
        Self {
            translucent,
            blend: if translucent { blend::ADD } else { blend::NONE },
            alpha: alpha_mode::SOLID,
            depth_test: true,
            anims: Vec::new(),
            has_uv: false,
            uv: IDENTITY,
            time: 0.0,
        }
    }

    /// `SetType`: only the translucency gate follows the type; the settings stay.
    pub fn set_translucent(&mut self, t: bool) {
        self.translucent = t;
    }

    pub fn set_blend(&mut self, mode: i32) {
        if self.translucent {
            self.blend = mode;
        }
    }

    pub fn set_alpha(&mut self, mode: i32) {
        self.alpha = mode;
    }

    pub fn set_depth_test(&mut self, on: bool) {
        if self.translucent {
            self.depth_test = on;
        }
    }

    pub fn add_anim(&mut self, a: UvAnim) {
        self.anims.push(a);
        self.has_uv = true;
    }

    /// The animation time is not reset.
    pub fn clear_anims(&mut self) {
        self.anims.clear();
        self.has_uv = false;
        self.uv = IDENTITY;
    }

    pub fn anim_count(&self) -> usize {
        self.anims.len()
    }

    /// `UpdateBeforeRendering`
    pub fn update(&mut self, dt: f32) {
        if self.has_uv {
            self.uv = uv_matrix(&self.anims, self.time);
            self.time += dt;
        }
    }
}

/// `iMaterialType` data that does not touch the GPU.
pub trait MaterialType: Sync {
    fn name(&self) -> &'static str;
    fn kind(&self) -> Kind;

    fn flags(&self) -> u32 {
        type_flags(self.kind())
    }

    fn used_textures(&self) -> Vec<u32> {
        used_textures(self.kind())
    }

    fn user_vars(&self) -> &'static [VarDef] {
        user_vars(self.kind())
    }
}

struct Registered(&'static str, Kind);

impl MaterialType for Registered {
    fn name(&self) -> &'static str {
        self.0
    }
    fn kind(&self) -> Kind {
        self.1
    }
}

/// The `AddMaterialType` calls of `cGraphics`; terrain decal and "terrain" reuse the C++
/// classes of the kinds below.
static MATERIAL_TYPES: [Registered; 10] = [
    Registered("soliddiffuse", Kind::SolidDiffuse),
    Registered("translucent", Kind::Translucent),
    Registered("water", Kind::Water),
    Registered("decal", Kind::Decal),
    Registered("terrainblend", Kind::TerrainBlend),
    Registered("terraindecal", Kind::Decal),
    Registered("undergrowth", Kind::Undergrowth),
    Registered("projecteduv", Kind::ProjectedUv),
    Registered("vertexblend", Kind::VertexBlend),
    Registered("terrain", Kind::SolidDiffuse),
];

pub fn material_types() -> impl Iterator<Item = &'static dyn MaterialType> {
    MATERIAL_TYPES.iter().map(|r| r as &dyn MaterialType)
}

pub fn material_type(name: &[u8]) -> Option<&'static dyn MaterialType> {
    material_types().find(|t| t.name().as_bytes() == name)
}

/// `iPostEffectType`: the name `SetParams`/`GetParams` check the params against.
pub trait PostEffectType: Sync {
    fn name(&self) -> &'static str;
}

struct PeName(&'static str);

impl PostEffectType for PeName {
    fn name(&self) -> &'static str {
        self.0
    }
}

static POST_EFFECT_TYPES: [PeName; 4] = [
    PeName("Bloom"),
    PeName("ImageTrail"),
    PeName("RadialBlur"),
    PeName("ColorConvTex"),
];

pub fn post_effect_type(name: &[u8]) -> Option<&'static dyn PostEffectType> {
    POST_EFFECT_TYPES
        .iter()
        .map(|t| t as &dyn PostEffectType)
        .find(|t| t.name().as_bytes() == name)
}

/// `iPostEffect` base state.
pub struct PeState {
    active: bool,
    disabled: bool,
    ty: Option<Vec<u8>>,
    final_used: bool,
}

impl PeState {
    /// `ty` is the name of the effect's `iPostEffectType`, `None` without one. Any name is
    /// accepted, not just the registered ones, as the C++ compares strings.
    pub fn new(ty: Option<&[u8]>) -> Self {
        Self {
            active: true,
            disabled: false,
            ty: ty.map(<[u8]>::to_vec),
            final_used: false,
        }
    }

    pub fn set_disabled(&mut self, d: bool) {
        self.disabled = d;
    }

    pub fn is_disabled(&self) -> bool {
        self.disabled
    }

    pub fn is_active(&self) -> bool {
        !self.disabled && self.active
    }

    /// True when `OnSetActive` runs.
    pub fn set_active(&mut self, a: bool) -> bool {
        if self.active == a {
            return false;
        }
        self.active = a;
        true
    }

    /// True when `SetParams`/`GetParams` accept params named `params`.
    pub fn accepts(&self, params: &[u8]) -> bool {
        self.ty.as_deref() == Some(params)
    }

    pub fn begin_render(&mut self) {
        self.final_used = false;
    }

    pub fn mark_final_used(&mut self) {
        self.final_used = true;
    }

    /// True when `Render` copies the output to the render target.
    pub fn needs_final_copy(&self, last: bool) -> bool {
        last && !self.final_used
    }
}
