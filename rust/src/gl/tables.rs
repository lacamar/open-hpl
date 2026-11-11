//! Enum to `GLenum` tables. Out-of-range input takes the C++ switch fall-through value.

use super::c;

fn pick(table: &[u32], v: i32, default: u32) -> u32 {
    usize::try_from(v)
        .ok()
        .and_then(|i| table.get(i))
        .copied()
        .unwrap_or(default)
}

pub fn vertex_array(element: i32) -> u32 {
    pick(
        &[
            c::NORMAL_ARRAY,
            c::VERTEX_ARRAY,
            c::COLOR_ARRAY,
            c::SECONDARY_COLOR_ARRAY,
            c::TEXTURE_COORD_ARRAY,
            c::TEXTURE_COORD_ARRAY,
            c::TEXTURE_COORD_ARRAY,
            c::TEXTURE_COORD_ARRAY,
            c::TEXTURE_COORD_ARRAY,
            c::TEXTURE_COORD_ARRAY,
            0,
            0,
            0,
            0,
        ],
        element,
        0,
    )
}

pub fn vertex_type(format: i32) -> u32 {
    pick(&[c::FLOAT, c::INT, c::UNSIGNED_BYTE], format, 0)
}

pub fn draw_mode(draw: i32) -> u32 {
    pick(
        &[
            c::TRIANGLES,
            c::TRIANGLE_STRIP,
            c::TRIANGLE_FAN,
            c::QUADS,
            c::QUAD_STRIP,
            c::LINES,
            c::LINE_LOOP,
            c::LINE_STRIP,
        ],
        draw,
        0,
    )
}

pub fn usage(usage: i32) -> u32 {
    match usage {
        1 => c::DYNAMIC_DRAW_ARB,
        2 => c::STREAM_DRAW_ARB,
        _ => c::STATIC_DRAW_ARB,
    }
}

pub fn wrap(mode: i32) -> u32 {
    pick(
        &[c::REPEAT, c::CLAMP, c::CLAMP_TO_EDGE, c::CLAMP_TO_BORDER],
        mode,
        c::REPEAT,
    )
}

pub fn pixel_format(f: i32) -> u32 {
    pick(
        &[
            0,
            c::ALPHA,
            c::LUMINANCE,
            c::LUMINANCE_ALPHA,
            c::RGB,
            c::RGBA,
            c::BGR_EXT,
            c::BGRA_EXT,
            0,
            0,
            0,
            0,
            0,
            0,
            c::DEPTH_COMPONENT,
            c::DEPTH_COMPONENT,
            c::DEPTH_COMPONENT,
            c::ALPHA,
            c::LUMINANCE,
            c::LUMINANCE_ALPHA,
            c::RGB,
            c::RGBA,
            c::ALPHA,
            c::LUMINANCE,
            c::LUMINANCE_ALPHA,
            c::RGB,
            c::RGBA,
        ],
        f,
        0,
    )
}

pub fn pixel_internal_format(f: i32) -> u32 {
    pick(
        &[
            0,
            c::ALPHA,
            c::LUMINANCE,
            c::LUMINANCE_ALPHA,
            c::RGB,
            c::RGBA,
            c::RGB,
            c::RGBA,
            0,
            0,
            0,
            0,
            0,
            0,
            c::DEPTH_COMPONENT16,
            c::DEPTH_COMPONENT24,
            c::DEPTH_COMPONENT32,
            c::ALPHA16F_ARB,
            c::LUMINANCE16F_ARB,
            c::LUMINANCE_ALPHA16F_ARB,
            c::RGB16F_ARB,
            c::RGBA16F_ARB,
            c::ALPHA32F_ARB,
            c::LUMINANCE32F_ARB,
            c::LUMINANCE_ALPHA32F_ARB,
            c::RGB32F_ARB,
            c::RGBA32F_ARB,
        ],
        f,
        0,
    )
}

pub fn compression_format(f: i32) -> u32 {
    match f {
        8 => c::COMPRESSED_RGBA_S3TC_DXT1_EXT,
        10 => c::COMPRESSED_RGBA_S3TC_DXT3_EXT,
        12 => c::COMPRESSED_RGBA_S3TC_DXT5_EXT,
        13 => c::COMPRESSED_RG_RGTC2,
        27 => c::COMPRESSED_RED_RGTC1,
        28 => c::COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT,
        29 => c::COMPRESSED_RGBA_BPTC_UNORM,
        _ => 0,
    }
}

/// `TextureTypeToGLTarget` and `GetGLTextureTargetEnum` are the same table.
pub fn texture_target(t: i32) -> u32 {
    pick(
        &[
            c::TEXTURE_1D,
            c::TEXTURE_2D,
            c::TEXTURE_RECTANGLE_NV,
            c::TEXTURE_CUBE_MAP_ARB,
            c::TEXTURE_3D,
        ],
        t,
        0,
    )
}

pub fn blend(f: i32) -> u32 {
    pick(
        &[
            c::ZERO,
            c::ONE,
            c::SRC_COLOR,
            c::ONE_MINUS_SRC_COLOR,
            c::DST_COLOR,
            c::ONE_MINUS_DST_COLOR,
            c::SRC_ALPHA,
            c::ONE_MINUS_SRC_ALPHA,
            c::DST_ALPHA,
            c::ONE_MINUS_DST_ALPHA,
            c::SRC_ALPHA_SATURATE,
        ],
        f,
        0,
    )
}

pub fn texture_param(p: i32) -> u32 {
    pick(
        &[
            c::COMBINE_RGB_ARB,
            c::COMBINE_ALPHA_ARB,
            c::SOURCE0_RGB_ARB,
            c::SOURCE1_RGB_ARB,
            c::SOURCE2_RGB_ARB,
            c::SOURCE0_ALPHA_ARB,
            c::SOURCE1_ALPHA_ARB,
            c::SOURCE2_ALPHA_ARB,
            c::OPERAND0_RGB_ARB,
            c::OPERAND1_RGB_ARB,
            c::OPERAND2_RGB_ARB,
            c::OPERAND0_ALPHA_ARB,
            c::OPERAND1_ALPHA_ARB,
            c::OPERAND2_ALPHA_ARB,
            c::RGB_SCALE_ARB,
            c::ALPHA_SCALE,
        ],
        p,
        0,
    )
}

pub fn texture_op(o: i32) -> u32 {
    pick(
        &[
            c::SRC_COLOR,
            c::ONE_MINUS_SRC_COLOR,
            c::SRC_ALPHA,
            c::ONE_MINUS_SRC_ALPHA,
        ],
        o,
        0,
    )
}

pub fn texture_source(s: i32) -> u32 {
    pick(
        &[
            c::TEXTURE,
            c::CONSTANT_ARB,
            c::PRIMARY_COLOR_ARB,
            c::PREVIOUS_ARB,
        ],
        s,
        0,
    )
}

pub fn compare_mode(m: i32) -> u32 {
    pick(&[c::NONE, c::COMPARE_R_TO_TEXTURE], m, 0)
}

pub fn compare_func(f: i32) -> u32 {
    pick(&[c::LEQUAL, c::GEQUAL], f, 0)
}

pub fn texture_func(f: i32) -> u32 {
    pick(
        &[
            c::MODULATE,
            c::REPLACE,
            c::ADD,
            c::SUBTRACT_ARB,
            c::ADD_SIGNED_ARB,
            c::INTERPOLATE_ARB,
            c::DOT3_RGB_ARB,
            c::DOT3_RGBA_ARB,
        ],
        f,
        0,
    )
}

/// Shared by `eDepthTestFunc`, `eAlphaTestFunc` and `eStencilFunc` (same order).
pub fn compare(f: i32) -> u32 {
    pick(
        &[
            c::NEVER,
            c::LESS,
            c::LEQUAL,
            c::GREATER,
            c::GEQUAL,
            c::EQUAL,
            c::NOTEQUAL,
            c::ALWAYS,
        ],
        f,
        0,
    )
}

pub fn stencil_op(o: i32) -> u32 {
    pick(
        &[
            c::KEEP,
            c::ZERO,
            c::REPLACE,
            c::INCR,
            c::DECR,
            c::INVERT,
            c::INCR_WRAP_EXT,
            c::DECR_WRAP_EXT,
        ],
        o,
        0,
    )
}

pub fn srgb(f: u32) -> u32 {
    match f {
        c::RGB => c::SRGB8,
        c::RGBA => c::SRGB8_ALPHA8,
        c::COMPRESSED_RGBA_S3TC_DXT1_EXT => c::COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT,
        c::COMPRESSED_RGBA_S3TC_DXT3_EXT => c::COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT,
        c::COMPRESSED_RGBA_S3TC_DXT5_EXT => c::COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT,
        c::COMPRESSED_RGBA_BPTC_UNORM => c::COMPRESSED_SRGB_ALPHA_BPTC_UNORM,
        other => other,
    }
}

pub fn shader_type(t: i32) -> u32 {
    if t == 1 {
        c::FRAGMENT_SHADER
    } else {
        c::VERTEX_SHADER
    }
}

/// `GL_*_BINDING` that `AutoGenerateMipmaps` reads back for a target.
pub fn texture_binding(target: u32) -> u32 {
    match target {
        c::TEXTURE_CUBE_MAP => c::TEXTURE_BINDING_CUBE_MAP,
        c::TEXTURE_3D => c::TEXTURE_BINDING_3D,
        c::TEXTURE_RECTANGLE => c::TEXTURE_BINDING_RECTANGLE,
        c::TEXTURE_1D => c::TEXTURE_BINDING_1D,
        _ => c::TEXTURE_BINDING_2D,
    }
}

pub fn depth_renderbuffer(bits: i32) -> u32 {
    match bits {
        16 => c::DEPTH_COMPONENT16,
        24 => c::DEPTH_COMPONENT24,
        32 => c::DEPTH_COMPONENT32,
        _ => 0,
    }
}

pub fn stencil_renderbuffer(bits: i32) -> u32 {
    match bits {
        1 => c::STENCIL_INDEX1_EXT,
        4 => c::STENCIL_INDEX4_EXT,
        8 => c::STENCIL_INDEX8_EXT,
        16 => c::STENCIL_INDEX16_EXT,
        _ => 0,
    }
}

/// Component count `DumpUniforms` prints for an active-uniform type.
pub fn uniform_components(t: u32) -> i32 {
    match t {
        c::FLOAT_VEC2 => 2,
        c::FLOAT_VEC3 => 3,
        c::FLOAT_VEC4 => 4,
        c::FLOAT_MAT4 => 16,
        _ => 1,
    }
}
