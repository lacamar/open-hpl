//! H8: `cRenderList::AddObject` decisions and the `iRenderer::GetClipRectFromObject` padding.
//! C ABI declared in `graphics/RustRenderH8.h`.

use crate::math::{Matrixf, aabb_line_intersection, matrix_mul_vec};
use crate::types::Vector3f;

const RT_LIGHT: i32 = 2;
const RT_GUI_SET: i32 = 5;
const RT_FOG_AREA: i32 = 7;

pub const GATE_MATERIAL_UPDATE: u32 = 1;
pub const GATE_OBJECT_UPDATE: u32 = 2;
pub const GATE_VIEWPORT_UPDATE: u32 = 4;

pub const LIST_OCCLUSION: u32 = 1;
pub const LIST_LIGHT: u32 = 2;
pub const LIST_FOG: u32 = 4;
pub const LIST_SOLID: u32 = 8;
pub const LIST_TRANS: u32 = 16;
pub const LIST_DECAL: u32 = 32;
pub const LIST_ILLUM: u32 = 64;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct GateIn {
    pub has_material: bool,
    pub has_type: bool,
    pub translucent: bool,
    pub decal: bool,
    pub frame: i32,
    pub material_frame: i32,
    pub object_frame: i32,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct PostIn {
    pub render_type: i32,
    pub has_material: bool,
    pub has_type: bool,
    pub translucent: bool,
    pub decal: bool,
    pub has_illum_texture: bool,
    pub uses_occlusion_query: bool,
    pub illumination: f32,
    pub bv_min: Vector3f,
    pub bv_max: Vector3f,
    pub bv_center: Vector3f,
    pub origin: Vector3f,
    pub view: Matrixf,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct PostOut {
    pub view_z: f32,
    pub lists: u32,
}

const _: () =
    assert!(size_of::<GateIn>() == 16 && size_of::<PostIn>() == 128 && size_of::<PostOut>() == 8);

pub fn gates(i: &GateIn) -> u32 {
    let mut g = 0;
    if i.has_material && i.material_frame != i.frame {
        g |= GATE_MATERIAL_UPDATE;
    }
    if i.object_frame != i.frame {
        g |= GATE_OBJECT_UPDATE;
    }
    if !i.has_type || !i.translucent || i.decal {
        g |= GATE_VIEWPORT_UPDATE;
    }
    g
}

pub fn post(i: &PostIn) -> PostOut {
    let center = i.bv_center;
    let target = if i.has_type && i.translucent && !i.decal {
        let mut hit = None;
        aabb_line_intersection(i.bv_min, i.bv_max, i.origin, center, &mut hit);
        hit.unwrap_or(center)
    } else {
        center
    };
    let view_z = matrix_mul_vec(&i.view, target).z;

    let mut lists = 0;
    if i.uses_occlusion_query {
        lists |= LIST_OCCLUSION;
    }
    if i.render_type == RT_LIGHT {
        lists |= LIST_LIGHT;
    }
    // A light also takes the normal branch, as in the C++.
    if i.render_type == RT_FOG_AREA {
        lists |= LIST_FOG;
    } else if i.render_type != RT_GUI_SET && i.has_material {
        if i.has_type && i.translucent {
            lists |= if i.decal { LIST_DECAL } else { LIST_TRANS };
        } else {
            lists |= LIST_SOLID;
            if i.has_illum_texture && i.illumination > 0.0 {
                lists |= LIST_ILLUM;
            }
        }
    }
    PostOut { view_z, lists }
}

#[repr(C)]
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub struct Rect2l {
    pub x: i32,
    pub y: i32,
    pub w: i32,
    pub h: i32,
}

#[expect(clippy::cast_possible_truncation, reason = "C++ (int) cast of a float")]
pub fn clip_pad(r: Rect2l, screen: (i32, i32), half_fov_tan: f32) -> Rect2l {
    let inc_x = (r.w as f32 * half_fov_tan) as i32;
    let inc_y = (r.h as f32 * half_fov_tan) as i32;
    let x = (r.x - inc_x).max(0);
    let y = (r.y - inc_y).max(0);
    Rect2l {
        x,
        y,
        w: (r.w + inc_x * 2).min(screen.0 - x),
        h: (r.h + inc_y * 2).min(screen.1 - y),
    }
}

/// # Safety
/// `i` is null or valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h8_rl_gates(i: *const GateIn) -> u32 {
    // SAFETY: by the caller's contract.
    unsafe { i.as_ref() }.map_or(0, gates)
}

/// # Safety
/// `i` and `o` are null or valid and distinct.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h8_rl_post(i: *const PostIn, o: *mut PostOut) {
    // SAFETY: by the caller's contract.
    if let (Some(i), Some(o)) = (unsafe { i.as_ref() }, unsafe { o.as_mut() }) {
        *o = post(i);
    }
}

/// # Safety
/// `r` is null or valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h8_clip_pad(r: *mut Rect2l, sw: i32, sh: i32, half_fov_tan: f32) {
    // SAFETY: by the caller's contract.
    if let Some(r) = unsafe { r.as_mut() } {
        *r = clip_pad(*r, (sw, sh), half_fov_tan);
    }
}
