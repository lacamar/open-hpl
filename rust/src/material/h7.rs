//! H7 state: `cMaterial` settings and UV animation, the `iMaterialType` and `iPostEffectType`
//! registries (traits, looked up by the names `cGraphics` registers them under) and the
//! `iPostEffect` base state.

use super::anim::{UvAnim, uv_matrix};
#[cfg(test)]
use super::types::Kind;
use super::types::{alpha_mode, blend};
#[cfg(test)]
use super::{VarDef, type_flags, used_textures, user_vars};
use crate::graphics::mm::Host;
use crate::math::IDENTITY;
use crate::types::Matrixf;

pub const MODES: usize = 7;
pub const TEXTURES: usize = 35;
pub const UNITS: usize = 16;

type P = *mut core::ffi::c_void;

/// `cMaterial` state. The leading fields are read in place by C++ (`OhplMmMat` in
/// `graphics/RustMaterialH7.h`); the rest is private.
#[repr(C)]
pub struct MatState {
    pub type_: P,
    pub vars: P,
    pub resources: P,
    pub programs: [[P; MODES]; 2],
    pub textures: [P; TEXTURES],
    pub in_unit: [[P; UNITS]; MODES],
    pub anim_ptr: *const UvAnim,
    pub uv: Matrixf,
    pub uv_sub: [i32; 2],
    pub max_reflection_distance: f32,
    pub decal_sort_order: i32,
    pub render_frame_count: i32,
    pub compiled_world_fog: u32,
    pub blend: i32,
    pub alpha: i32,
    pub anim_num: i32,
    pub has_specific: [bool; MODES],
    pub has_object_specific: [bool; MODES],
    pub depth_test: bool,
    pub has_uv: bool,
    pub auto_destroy_textures: bool,
    pub force_full_scale_textures: bool,
    pub has_refraction: bool,
    pub use_refraction_edge_check: bool,
    pub has_world_reflection: bool,
    pub world_reflection_occlusion_test: bool,
    pub has_translucent_illumination: bool,
    pub large_transperant_surface: bool,
    pub affected_by_depth_of_field: bool,
    pub affected_by_fog: bool,
    pub use_alpha_dissolve_filter: bool,
    translucent: bool,
    anims: Vec<UvAnim>,
    time: f32,
    pub(super) host: Option<&'static Host>,
}

const _: () = {
    assert!(core::mem::offset_of!(MatState, uv) == 1320);
    assert!(core::mem::offset_of!(MatState, anim_num) == 1416);
    assert!(core::mem::offset_of!(MatState, use_alpha_dissolve_filter) == 1446);
};

impl MatState {
    pub fn new(translucent: bool) -> Self {
        let null = core::ptr::null_mut();
        Self {
            type_: null,
            vars: null,
            resources: null,
            programs: [[null; MODES]; 2],
            textures: [null; TEXTURES],
            in_unit: [[null; UNITS]; MODES],
            anim_ptr: core::ptr::null(),
            uv: IDENTITY,
            uv_sub: [1, 1],
            max_reflection_distance: 0.0,
            decal_sort_order: 0,
            render_frame_count: -1,
            compiled_world_fog: 0,
            blend: if translucent { blend::ADD } else { blend::NONE },
            alpha: alpha_mode::SOLID,
            anim_num: 0,
            has_specific: [false; MODES],
            has_object_specific: [false; MODES],
            depth_test: true,
            has_uv: false,
            auto_destroy_textures: true,
            force_full_scale_textures: false,
            has_refraction: false,
            use_refraction_edge_check: false,
            has_world_reflection: false,
            world_reflection_occlusion_test: true,
            has_translucent_illumination: false,
            large_transperant_surface: false,
            affected_by_depth_of_field: true,
            affected_by_fog: true,
            use_alpha_dissolve_filter: false,
            translucent,
            anims: Vec::new(),
            time: 0.0,
            host: None,
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
        self.sync_anims();
    }

    fn sync_anims(&mut self) {
        self.anim_ptr = if self.anims.is_empty() {
            core::ptr::null()
        } else {
            self.anims.as_ptr()
        };
        self.anim_num = i32::try_from(self.anims.len()).unwrap_or(i32::MAX);
    }

    /// The animation time is not reset.
    pub fn clear_anims(&mut self) {
        self.anims.clear();
        self.sync_anims();
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

#[cfg(test)]
pub struct Registered(&'static str, Kind);

#[cfg(test)]
impl Registered {
    pub fn name(&self) -> &'static str {
        self.0
    }

    pub fn flags(&self) -> u32 {
        type_flags(self.1)
    }

    pub fn used_textures(&self) -> Vec<u32> {
        used_textures(self.1)
    }

    pub fn user_vars(&self) -> &'static [VarDef] {
        user_vars(self.1)
    }
}

pub const MATERIAL_TYPE_COUNT: usize = 10;

#[cfg(test)]
pub static MATERIAL_TYPES: [Registered; 10] = [
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

#[cfg(test)]
const _: () = assert!(MATERIAL_TYPES.len() == MATERIAL_TYPE_COUNT);

#[cfg(test)]
pub fn material_type(name: &[u8]) -> Option<&'static Registered> {
    MATERIAL_TYPES.iter().find(|t| t.name().as_bytes() == name)
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
