//! Shared types of the material port: enum values of `GraphicsTypes.h`, the views the C++ shims
//! hand in, the host callbacks and the uniform command buffer handed back.
//! `#[repr(C)]` layouts are asserted here and in `graphics/RustMaterial.h`.

use crate::types::{Color, Matrixf, Vector2l, Vector3f};

pub(crate) mod mode {
    pub const Z: i32 = 0;
    pub const Z_DISSOLVE: i32 = 1;
    pub const DIFFUSE: i32 = 2;
    pub const DIFFUSE_FOG: i32 = 3;
    pub const ILLUMINATION: i32 = 5;
    pub const ILLUMINATION_FOG: i32 = 6;
}

/// `eMaterialTexture` values.
pub(crate) mod tex {
    pub const DIFFUSE: u32 = 0;
    pub const NMAP: u32 = 1;
    pub const SPECULAR: u32 = 2;
    pub const ALPHA: u32 = 3;
    pub const HEIGHT: u32 = 4;
    pub const ILLUMINATION: u32 = 5;
    pub const CUBE_MAP: u32 = 6;
    pub const DISSOLVE_ALPHA: u32 = 7;
    pub const CUBE_MAP_ALPHA: u32 = 8;
    pub const DETAIL_DIFFUSE: u32 = 9;
    pub const DETAIL_NMAP: u32 = 10;
    pub const TRANSLUCENCY: u32 = 11;
    pub const DIFFUSE_SIDE: u32 = 12;
    pub const NMAP_SIDE: u32 = 15;
    pub const SPECULAR_SIDE: u32 = 18;
    pub const DIFFUSE_BOTTOM: u32 = 14;
    pub const SPECULAR_BOTTOM: u32 = 20;
    pub const DIFFUSE_R: u32 = 21;
    pub const HEIGHT_B: u32 = 32;
    pub const LIQUID: u32 = 33;
}

/// `eMaterialBlendMode` values.
pub(crate) mod blend {
    pub const NONE: i32 = 0;
    pub const ADD: i32 = 1;
    pub const MUL: i32 = 2;
}

pub(crate) mod alpha_mode {
    pub const SOLID: i32 = 0;
    pub const TRANS: i32 = 1;
}

/// The material types, in `ohpl_mat_*` `kind` order.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Kind {
    SolidDiffuse,
    ProjectedUv,
    VertexBlend,
    Translucent,
    Water,
    Decal,
    TerrainBlend,
    Undergrowth,
}

impl Kind {
    pub fn from_i32(v: i32) -> Option<Self> {
        Some(match v {
            0 => Self::SolidDiffuse,
            1 => Self::ProjectedUv,
            2 => Self::VertexBlend,
            3 => Self::Translucent,
            4 => Self::Water,
            5 => Self::Decal,
            6 => Self::TerrainBlend,
            7 => Self::Undergrowth,
            _ => return None,
        })
    }
}

/// `tFlag` bit `n`, as `eFlagBit_n`.
pub(crate) const fn bit(n: u32) -> u32 {
    1u32 << n
}

/// Renderer/engine statics read through getters in the C++ code.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct Globals {
    pub hdr: bool,
    pub parallax_enabled: bool,
    pub parallax_low: bool,
    pub shader_model_3: bool,
    pub gbuffer_32bit: bool,
    pub gbuffer_four_targets: bool,
    pub hpsl: bool,
    pub refraction: bool,
    pub light_probes: bool,
    pub depth_of_field: bool,
    pub tex_rect: bool,
    pub has_water_vtx: bool,
    pub world_fog: u32,
    pub reflection_size_div: i32,
    pub virtual_position_add_scale: f32,
    pub dof_params: Color,
}

/// What the shim reads off a `cMaterial`.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct MatView {
    /// bit `n` set when `GetTexture(n)` is non-null
    pub tex_mask: u64,
    pub uv_matrix: Matrixf,
    pub liquid_size: Vector2l,
    pub blend_mode: i32,
    pub has_uv_anim: bool,
    pub diffuse_srgb: bool,
}

impl MatView {
    pub(crate) fn has(&self, slot: u32) -> bool {
        slot < 64 && (self.tex_mask >> slot) & 1 != 0
    }
}

/// What the shim reads off the current frustum and renderer.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct RenderView {
    pub view: Matrixf,
    pub origin: Vector3f,
    pub time: f32,
    pub near: f32,
    pub far: f32,
    pub temp_alpha: f32,
    pub rt_size: Vector2l,
    pub fog_area_color: Color,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct ObjView {
    pub model: Matrixf,
    pub color_mul: Color,
    pub illum_color: Color,
    pub illum_amount: f32,
    pub liquid_amount: f32,
    pub has_model: bool,
    pub is_static: bool,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct ForceFieldInfo {
    pub pos: Vector3f,
    pub falloff_start: f32,
    pub radius: f32,
    pub force: f32,
    pub t: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct WorldInfo {
    pub fog_start: f32,
    pub fog_end: f32,
    pub fog_falloff_exp: f32,
    pub fog_brightness: f32,
    pub fog_color: Color,
    pub fog_density: f32,
    pub fog_height_density: f32,
    pub fog_height_horizon: f32,
    pub sec_fog_start: f32,
    pub sec_fog_end: f32,
    pub sec_fog_falloff_exp: f32,
    pub sec_fog_brightness: f32,
    pub sec_fog_color: Color,
    pub fog_active: bool,
    pub fog_exponential: bool,
    pub fog_height_based: bool,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct Bv {
    pub min: Vector3f,
    pub max: Vector3f,
    pub world_center: Vector3f,
}

pub(crate) mod light_kind {
    pub const BOX: i32 = 2;
    pub const SPOT: i32 = 1;
}

pub(crate) mod box_blend {
    pub const REPLACE: i32 = 0;
    pub const ADD: i32 = 1;
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct LightInfo {
    pub kind: i32,
    pub min: Vector3f,
    pub max: Vector3f,
    pub pos: Vector3f,
    pub radius: f32,
    pub falloff_pow: f32,
    pub brightness: f32,
    pub diffuse: Color,
    pub near_clip: f32,
    pub blend_func: i32,
    pub weight: f32,
    pub bevel: f32,
    pub bands: [Vector3f; 4],
    pub use_sh: bool,
}

/// Queries the shim answers lazily, in the order the C++ code made them.
pub trait Host {
    fn coverage(&mut self) -> f32;
    fn force_fields(&mut self, out: &mut [ForceFieldInfo; 4]) -> usize;
    /// Force fields touching the box, cached for later `force_fields` calls.
    fn fields_in_box(&mut self, min: Vector3f, max: Vector3f) -> usize;
    fn world(&mut self) -> Option<WorldInfo>;
    fn object_bv(&mut self) -> Bv;
    fn light_count(&mut self) -> usize;
    /// `detail` asks for the bounds and the box/spot specifics too.
    fn light(&mut self, i: usize, detail: bool) -> Option<LightInfo>;
    fn light_intersects(&mut self, i: usize) -> bool;
    fn spot_clip(&mut self, i: usize) -> Option<(Vector3f, Vector3f)>;
}

pub(crate) mod ukind {
    pub const FLOAT: u32 = 0;
    pub const INT: u32 = 1;
    pub const VEC2: u32 = 2;
    pub const VEC3: u32 = 3;
    pub const VEC4: u32 = 4;
    pub const MAT: u32 = 5;
}

/// One `iGpuProgram::Set*` call.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct Uniform {
    pub kind: u32,
    pub id: i32,
    /// `SetFloat` result is checked and reported as an error
    pub check: u32,
    pub int_value: i32,
    pub v: [f32; 16],
}

impl Uniform {
    pub const ZERO: Uniform = Uniform {
        kind: 0,
        id: 0,
        check: 0,
        int_value: 0,
        v: [0.0; 16],
    };
}

/// Bounded command buffer: writes past the end are dropped and counted, never a panic.
pub struct Uniforms<'a> {
    buf: &'a mut [Uniform],
    len: usize,
    dropped: usize,
}

impl<'a> Uniforms<'a> {
    pub fn new(buf: &'a mut [Uniform]) -> Self {
        Self {
            buf,
            len: 0,
            dropped: 0,
        }
    }

    pub fn len(&self) -> usize {
        self.len
    }

    pub fn is_empty(&self) -> bool {
        self.len == 0
    }

    pub fn dropped(&self) -> usize {
        self.dropped
    }

    pub fn as_slice(&self) -> &[Uniform] {
        self.buf.get(..self.len).unwrap_or(&[])
    }

    fn push(&mut self, u: Uniform) {
        match self.buf.get_mut(self.len) {
            Some(slot) => {
                *slot = u;
                self.len += 1;
            }
            None => self.dropped += 1,
        }
    }

    fn make(kind: u32, id: i32, vals: &[f32]) -> Uniform {
        let mut u = Uniform {
            kind,
            id,
            ..Uniform::ZERO
        };
        for (d, s) in u.v.iter_mut().zip(vals) {
            *d = *s;
        }
        u
    }

    pub fn float(&mut self, id: i32, x: f32) {
        self.push(Self::make(ukind::FLOAT, id, &[x]));
    }

    /// `SetFloat` whose failure the C++ code reports.
    pub fn float_checked(&mut self, id: i32, x: f32) {
        let mut u = Self::make(ukind::FLOAT, id, &[x]);
        u.check = 1;
        self.push(u);
    }

    pub fn int(&mut self, id: i32, x: i32) {
        let mut u = Self::make(ukind::INT, id, &[]);
        u.int_value = x;
        self.push(u);
    }

    pub fn vec2(&mut self, id: i32, x: f32, y: f32) {
        self.push(Self::make(ukind::VEC2, id, &[x, y]));
    }

    pub fn vec3(&mut self, id: i32, v: Vector3f) {
        self.push(Self::make(ukind::VEC3, id, &[v.x, v.y, v.z]));
    }

    pub fn vec4(&mut self, id: i32, x: f32, y: f32, z: f32, w: f32) {
        self.push(Self::make(ukind::VEC4, id, &[x, y, z, w]));
    }

    pub fn color(&mut self, id: i32, c: Color) {
        self.vec4(id, c.r, c.g, c.b, c.a);
    }

    pub fn mat(&mut self, id: i32, m: &Matrixf) {
        let mut u = Self::make(ukind::MAT, id, &[]);
        for (d, s) in u.v.iter_mut().zip(m.m.iter().flatten()) {
            *d = *s;
        }
        self.push(u);
    }
}

/// Which `cProgramComboManager` a program comes from.
pub(crate) mod manager {
    /// the material type's own `mpProgramManager`
    pub const OWN: i32 = 0;
    /// the shared Z-pass manager
    pub const GLOBAL: i32 = 1;
    /// `mpBlendProgramManager[i]` is `BLEND0 + i`
    pub const BLEND0: i32 = 2;
    pub const NONE: i32 = -1;
    /// index the C++ code would have read out of bounds
    pub const INVALID: i32 = -2;
}

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct ProgSel {
    pub manager: i32,
    pub mode: i32,
    pub flags: u32,
}

impl ProgSel {
    pub const NONE: ProgSel = ProgSel {
        manager: manager::NONE,
        mode: 0,
        flags: 0,
    };

    pub(crate) const fn own(mode: i32, flags: u32) -> Self {
        Self {
            manager: manager::OWN,
            mode,
            flags,
        }
    }

    pub(crate) const fn global(mode: i32, flags: u32) -> Self {
        Self {
            manager: manager::GLOBAL,
            mode,
            flags,
        }
    }
}

pub(crate) mod texsel {
    pub const NONE: i32 = 0;
    pub const SLOT: i32 = 1;
    pub const DISSOLVE: i32 = 2;
    pub const REFRACTION: i32 = 3;
    pub const SCENE_DEPTH: i32 = 4;
    pub const REFLECTION: i32 = 5;
    pub const WHITE: i32 = 6;
}

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct TexSel {
    pub kind: i32,
    pub slot: i32,
}

impl TexSel {
    pub const NONE: TexSel = TexSel {
        kind: texsel::NONE,
        slot: 0,
    };

    pub(crate) const fn special(kind: i32) -> Self {
        Self { kind, slot: 0 }
    }

    pub(crate) fn slot(slot: u32) -> Self {
        Self {
            kind: texsel::SLOT,
            slot: i32::try_from(slot).unwrap_or(0),
        }
    }

    /// The material's texture in `slot` when it has one.
    pub(crate) fn slot_if(v: &MatView, slot: u32) -> Self {
        if v.has(slot) {
            Self::slot(slot)
        } else {
            Self::NONE
        }
    }
}

/// Settings a material type's `CompileMaterialSpecifics` applies to its `cMaterial`.
/// `-1` in an `i32` field means "leave as is".
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct CompileOut {
    pub spec_set: u32,
    pub spec_val: u32,
    pub obj_set: u32,
    pub obj_val: u32,
    pub alpha_mode: i32,
    pub dissolve_filter: i32,
    pub blend_mode: i32,
    pub has_refraction: i32,
    pub refraction_edge: i32,
    pub world_reflection: i32,
    pub translucent_illum: i32,
}

impl CompileOut {
    pub const NONE: CompileOut = CompileOut {
        spec_set: 0,
        spec_val: 0,
        obj_set: 0,
        obj_val: 0,
        alpha_mode: -1,
        dissolve_filter: -1,
        blend_mode: -1,
        has_refraction: -1,
        refraction_edge: -1,
        world_reflection: -1,
        translucent_illum: -1,
    };

    pub(crate) fn specific(&mut self, mode: i32, v: bool) {
        self.spec_set |= 1 << mode;
        if v {
            self.spec_val |= 1 << mode;
        }
    }

    pub(crate) fn object_specific(&mut self, mode: i32, v: bool) {
        self.obj_set |= 1 << mode;
        if v {
            self.obj_val |= 1 << mode;
        }
    }
}

const _: () = {
    use std::mem::{offset_of, size_of};
    assert!(size_of::<Globals>() == 40 && offset_of!(Globals, world_fog) == 12);
    assert!(size_of::<MatView>() == 88 && offset_of!(MatView, uv_matrix) == 8);
    assert!(offset_of!(MatView, blend_mode) == 80 && offset_of!(MatView, has_uv_anim) == 84);
    assert!(size_of::<RenderView>() == 116 && offset_of!(RenderView, rt_size) == 92);
    assert!(size_of::<ObjView>() == 108 && offset_of!(ObjView, has_model) == 104);
    assert!(size_of::<ForceFieldInfo>() == 28);
    assert!(size_of::<WorldInfo>() == 80 && offset_of!(WorldInfo, fog_active) == 76);
    assert!(size_of::<Bv>() == 36);
    assert!(size_of::<LightInfo>() == 136 && offset_of!(LightInfo, bands) == 84);
    assert!(size_of::<Uniform>() == 80 && offset_of!(Uniform, v) == 16);
    assert!(size_of::<ProgSel>() == 12 && size_of::<TexSel>() == 8);
    assert!(size_of::<CompileOut>() == 44);
};
