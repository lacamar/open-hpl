//! C ABI of the material port, declared in `HPL2/core/include/graphics/RustMaterial.h`.
//!
//! Pointers are valid and aligned for the call unless documented as nullable; outputs do not
//! alias inputs. `vars` pointers address the `Ohpl*Vars` base of the C++ `*_Vars` class of the
//! `kind` and are nullable. Callback tables have every entry optional; a missing entry answers
//! with the neutral value.

use super::Vars;
use super::types::{Bv, ForceFieldInfo, Host, Kind, LightInfo, WorldInfo};
use super::vars::{
    LoadOut, ProjectedUvVars, SolidVars, TerrainBlendVars, TranslucentVars, UndergrowthVars,
    VarSource, VertexBlendVars, WaterVars,
};
use crate::types::{Color, Vector2f, Vector3f};
use std::ffi::c_void;

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
pub(super) unsafe fn vars_of<'a>(kind: Kind, p: *const c_void) -> Vars<'a> {
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

impl VarsVt {
    pub(super) fn empty() -> Self {
        Self {
            ctx: core::ptr::null_mut(),
            get_bool: None,
            get_float: None,
            get_vec2: None,
            get_vec3: None,
            get_color: None,
            get_string: None,
            add_bool: None,
            add_float: None,
        }
    }
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

impl VarsVt {
    pub fn add_bool(&mut self, name: &str, v: bool) {
        if let Some(f) = self.add_bool {
            // SAFETY: table contract.
            unsafe { f(self.ctx, name.as_ptr(), name.len(), v) };
        }
    }

    pub fn add_float(&mut self, name: &str, v: f32) {
        if let Some(f) = self.add_float {
            // SAFETY: table contract.
            unsafe { f(self.ctx, name.as_ptr(), name.len(), v) };
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

impl HostVt {
    pub(super) fn empty() -> Self {
        Self {
            ctx: core::ptr::null_mut(),
            coverage: None,
            force_fields: None,
            fields_in_box: None,
            world: None,
            object_bv: None,
            light_count: None,
            light: None,
            light_intersects: None,
            spot_clip: None,
        }
    }
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

/// # Safety
/// `p` is null or valid for one write.
unsafe fn write<T>(p: *mut T, v: T) {
    if !p.is_null() {
        // SAFETY: caller contract.
        unsafe { p.write(v) };
    }
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
