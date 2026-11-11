//! `cSDLTexture` decisions: size and mip planning, upload parameters, filter and property
//! choices, animation clock. The C++ caller issues the GL calls and owns every buffer.

use super::c;
use super::tables;
use crate::graphics::formats;
use crate::math::fma;
use crate::types::Vector3l;

pub const TYPE_1D: i32 = 0;
pub const TYPE_2D: i32 = 1;
pub const TYPE_RECT: i32 = 2;
pub const TYPE_CUBE: i32 = 3;
pub const TYPE_3D: i32 = 4;
pub const FILTER_NEAREST: i32 = 0;
pub const FILTER_BILINEAR: i32 = 1;
pub const FILTER_TRILINEAR: i32 = 2;
pub const FORMAT_RGTC2: i32 = 13;
pub const USAGE_RENDER_TARGET: i32 = 1;
pub const ANIM_LOOP: i32 = 1;

fn is_pow2(x: i32) -> bool {
    matches!(
        x,
        0 | 1 | 2 | 4 | 8 | 16 | 32 | 64 | 128 | 256 | 512 | 1024 | 2048 | 4096 | 8192
    )
}

fn shr(v: i32, n: i32) -> i32 {
    v.wrapping_shr(u32::try_from(n).unwrap_or(0))
}

fn clamp1(v: Vector3l) -> Vector3l {
    let f = |x: i32| if x == 0 { 1 } else { x };
    Vector3l {
        x: f(v.x),
        y: f(v.y),
        z: f(v.z),
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct CreateIn {
    pub tex_type: i32,
    pub format: i32,
    pub size: Vector3l,
    pub min_size: Vector3l,
    pub num_mips: i32,
    pub down_level: u32,
    pub generate_mips: bool,
    pub check_resize: bool,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct CreatePlan {
    pub warn_pow2: bool,
    pub compressed: bool,
    /// Rescale the top level with `gluScaleImage` into `resize_bytes` bytes.
    pub resize: bool,
    pub size: Vector3l,
    pub resize_bytes: i32,
    pub start_level: i32,
    pub end_level: i32,
    /// `GL_TEXTURE_MAX_LEVEL` value, -1 for none.
    pub max_level: i32,
    /// Build the chain from the top level (`GenerateMipMaps`).
    pub build_mips: bool,
}

/// `CreateTexture` up to the upload loop.
pub fn plan_create(i: &CreateIn) -> CreatePlan {
    let compressed = formats::is_compressed(i.format);
    let mut p = CreatePlan {
        warn_pow2: (!is_pow2(i.size.x) || !is_pow2(i.size.y)) && i.tex_type != TYPE_RECT,
        compressed,
        size: i.size,
        ..CreatePlan::default()
    };
    let down = i32::try_from(i.down_level).unwrap_or(i32::MAX);
    if i.down_level > 0
        && i.check_resize
        && (!compressed || i.num_mips > 1)
        && !formats::is_depth(i.format)
    {
        if i.num_mips > 1 {
            p.start_level = if i.num_mips - 1 < down {
                i.num_mips - 1
            } else {
                down
            };
            p.size.x = shr(p.size.x, p.start_level);
            p.size.y = shr(p.size.y, p.start_level);
            p.size.z = shr(p.size.z, p.start_level);
            p.size = clamp1(p.size);
        } else if (i.tex_type == TYPE_2D || i.tex_type == TYPE_CUBE)
            && p.size.x > i.min_size.x
            && p.size.y > i.min_size.y
        {
            let mut new = p.size;
            for _ in 0..down.max(0) {
                new.x = shr(new.x, 1);
                new.y = shr(new.y, 1);
                if new.x <= i.min_size.x || p.size.y <= i.min_size.y {
                    break;
                }
            }
            p.resize = true;
            p.resize_bytes = new
                .x
                .wrapping_mul(new.y)
                .wrapping_mul(formats::bytes_per_pixel(i.format));
            p.size = new;
        }
    }
    p.end_level = if i.generate_mips {
        i.num_mips
    } else {
        p.start_level.wrapping_add(1)
    };
    if i.generate_mips && i.num_mips > 1 {
        p.max_level = p.end_level.wrapping_sub(p.start_level).wrapping_sub(1);
    } else {
        p.max_level = -1;
    }
    p.build_mips = i.generate_mips && i.num_mips <= 1;
    p
}

/// Size of upload level `count` (0 is the first uploaded level): halve per level, never 0.
pub fn level_size(size: Vector3l, count: i32) -> Vector3l {
    let mut v = size;
    for _ in 0..count.max(0) {
        v = clamp1(v);
        v.x = shr(v.x, 1);
        v.y = shr(v.y, 1);
        v.z = shr(v.z, 1);
    }
    clamp1(v)
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Upload {
    /// 0 none, 1-3 `glCompressedTexImage1D-3D`, 4-6 `glTexImage1D-3D`.
    pub kind: u32,
    pub target: u32,
    pub level: i32,
    pub internal: u32,
    pub size: Vector3l,
    pub format: u32,
    pub gl_type: u32,
    /// Follow with `glTexParameteriv(.., TEXTURE_SWIZZLE_RGBA, {R,R,R,G})` on the plain target.
    pub swizzle: bool,
    pub bytes: i32,
}

/// `CopyTextureDataToGL`: which upload call to make with what arguments.
pub fn plan_upload(
    tex_type: i32,
    srgb: bool,
    format: i32,
    level: i32,
    data_size: i32,
    size: Vector3l,
    face: i32,
) -> Upload {
    let target = tables::texture_target(tex_type);
    let face_target = if tex_type == TYPE_CUBE {
        c::TEXTURE_CUBE_MAP_POSITIVE_X_ARB.wrapping_add_signed(face)
    } else {
        target
    };
    let mut u = Upload {
        target,
        level,
        size,
        bytes: data_size,
        ..Upload::default()
    };
    if formats::is_compressed(format) {
        let mut comp = tables::compression_format(format);
        if srgb {
            comp = tables::srgb(comp);
        }
        u.internal = comp;
        match tex_type {
            TYPE_1D => u.kind = 1,
            TYPE_2D | TYPE_CUBE => {
                u.kind = 2;
                u.target = face_target;
                u.swizzle = format == FORMAT_RGTC2;
            }
            TYPE_3D => u.kind = 3,
            _ => {}
        }
    } else {
        u.format = tables::pixel_format(format);
        u.gl_type = if formats::is_floating_point(format) {
            c::FLOAT
        } else {
            c::UNSIGNED_BYTE
        };
        let mut internal = tables::pixel_internal_format(format);
        if srgb {
            internal = tables::srgb(internal);
        }
        u.internal = internal;
        match tex_type {
            TYPE_1D => u.kind = 4,
            TYPE_2D | TYPE_RECT | TYPE_CUBE => {
                u.kind = 5;
                u.target = face_target;
            }
            TYPE_3D => u.kind = 6,
            _ => {}
        }
    }
    u
}

pub const MIP_DISABLE: u32 = 0;
pub const MIP_GENERATE: u32 = 1;
pub const MIP_BUILD_1D: u32 = 2;
pub const MIP_BUILD_2D: u32 = 3;
pub const MIP_NONE: u32 = 4;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct MipPlan {
    pub action: u32,
    pub target: u32,
    pub internal: u32,
    pub format: u32,
    /// Also clear the texture's mip flag.
    pub disable_mips: bool,
    pub memory: i32,
}

/// `GenerateMipMaps`.
pub fn plan_mips(
    tex_type: i32,
    usage: i32,
    fbo_ext: bool,
    srgb: bool,
    format: i32,
    size: Vector3l,
    face: i32,
) -> MipPlan {
    let mut p = MipPlan {
        action: MIP_NONE,
        target: tables::texture_target(tex_type),
        format: tables::pixel_format(format),
        ..MipPlan::default()
    };
    let mut internal = tables::pixel_internal_format(format);
    if srgb {
        internal = tables::srgb(internal);
    }
    p.internal = internal;
    if usage == USAGE_RENDER_TARGET {
        if fbo_ext {
            p.action = MIP_GENERATE;
        } else {
            p.disable_mips = true;
        }
    } else if !formats::is_compressed(format) {
        if tex_type == TYPE_1D {
            p.action = MIP_BUILD_1D;
        } else if tex_type == TYPE_2D || tex_type == TYPE_CUBE {
            p.action = MIP_BUILD_2D;
            if tex_type == TYPE_CUBE {
                p.target = c::TEXTURE_CUBE_MAP_POSITIVE_X_ARB.wrapping_add_signed(face);
            }
        }
        let bpp = formats::bytes_per_pixel(format);
        let mut v = size;
        while v.x > 1 || v.y > 1 || v.z > 1 {
            v.x = shr(v.x, 1);
            v.y = shr(v.y, 1);
            v.z = shr(v.z, 1);
            v = clamp1(v);
            p.memory = p
                .memory
                .wrapping_add(v.x.wrapping_mul(v.y).wrapping_mul(v.z).wrapping_mul(bpp));
        }
    } else {
        p.disable_mips = true;
    }
    p
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Props {
    pub min_filter: u32,
    pub rect: bool,
    pub wrap: [u32; 3],
}

/// `SetupProperties` filter and wrap choice; `wrap` is the texture's S/T/R mode, replaced for
/// rectangles.
pub fn plan_props(tex_type: i32, use_mips: bool, filter: i32, wrap: [i32; 3]) -> Props {
    let min_filter = if use_mips && tex_type != TYPE_RECT {
        if filter == FILTER_BILINEAR {
            c::LINEAR_MIPMAP_NEAREST
        } else {
            c::LINEAR_MIPMAP_LINEAR
        }
    } else {
        c::LINEAR
    };
    let rect = tex_type == TYPE_RECT;
    let wrap = if rect {
        [c::CLAMP_TO_EDGE; 3]
    } else {
        wrap.map(tables::wrap)
    };
    Props {
        min_filter,
        rect,
        wrap,
    }
}

/// Whether the driver's maximum anisotropy covers `degree`.
pub fn aniso_fits(max: i32, degree: f32) -> bool {
    max as f32 >= degree
}

/// `SetFilter` min and mag filters; (0, 0) when the filter value matches no case.
pub fn filter_params(filter: i32, use_mips: bool, tex_type: i32) -> [u32; 2] {
    if use_mips && tex_type != TYPE_RECT {
        match filter {
            FILTER_NEAREST => [c::NEAREST_MIPMAP_NEAREST, c::NEAREST],
            FILTER_BILINEAR => [c::LINEAR_MIPMAP_NEAREST, c::LINEAR],
            FILTER_TRILINEAR => [c::LINEAR_MIPMAP_LINEAR, c::LINEAR],
            _ => [0, 0],
        }
    } else {
        match filter {
            FILTER_NEAREST => [c::NEAREST, c::NEAREST],
            FILTER_BILINEAR | FILTER_TRILINEAR => [c::LINEAR, c::LINEAR],
            _ => [0, 0],
        }
    }
}

/// `SetAnisotropyDegree`: whether to apply `x`.
#[expect(clippy::float_cmp, reason = "C++ compares the degree exactly")]
pub fn aniso_apply(has_data: bool, cap: bool, max: i32, x: f32, current: f32) -> bool {
    let below = x < 1.0;
    let above = x > max as f32;
    has_data && cap && !below && !above && current != x
}

/// Clock state shared by `Update`, `NextFrame` and `PrevFrame`.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Clock {
    pub count: f32,
    pub dir: f32,
}

impl Clock {
    pub fn update(&mut self, handles: usize, step: f32, frame_time: f32, mode: i32) {
        if handles > 1 {
            let max = handles as f32;
            self.count = fma(step * (1.0f32 / frame_time), self.dir, self.count);
            self.wrap(max, mode, self.dir > 0.0);
        }
    }

    pub fn next_frame(&mut self, handles: usize, mode: i32) {
        self.count += self.dir;
        self.wrap(handles as f32, mode, self.dir > 0.0);
    }

    pub fn prev_frame(&mut self, handles: usize, mode: i32) {
        self.count -= self.dir;
        self.wrap(handles as f32, mode, self.dir < 0.0);
    }

    /// `forward` selects the upper-bound branch; `PrevFrame` tests the opposite direction.
    fn wrap(&mut self, max: f32, mode: i32, forward: bool) {
        if forward {
            if self.count >= max {
                if mode == ANIM_LOOP {
                    self.count = 0.0;
                } else {
                    self.count = max - 1.0;
                    self.dir = -1.0;
                }
            }
        } else if self.count < 0.0 {
            self.count = 1.0;
            self.dir = 1.0;
        }
    }
}

/// `GetTextureHandle` slot: `None` is handle 0. A frame outside the handle list (C++ read past
/// the vector) also gives `None`.
pub fn handle_slot(has_data: bool, handles: usize, count: f32) -> Option<usize> {
    if !has_data || handles == 0 {
        return None;
    }
    if handles == 1 {
        return Some(0);
    }
    usize::try_from(count as i32).ok().filter(|&i| i < handles)
}

/// `CreateFromRawData` size clamp and byte count.
pub fn raw_size(size: Vector3l, format: i32) -> (Vector3l, i32) {
    let s = Vector3l {
        x: size.x.max(1),
        y: size.y.max(1),
        z: size.z.max(1),
    };
    let bytes = size
        .x
        .wrapping_mul(size.y)
        .wrapping_mul(size.z)
        .wrapping_mul(formats::bytes_per_pixel(format));
    (s, bytes)
}
