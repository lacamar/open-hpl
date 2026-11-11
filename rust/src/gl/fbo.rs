//! `cFrameBufferGL` and `cDepthStencilBufferGL` decisions: renderbuffer formats, attachment
//! validation and slots, draw-buffer setup, completeness status mapping.

use super::c;
use super::tables;
use super::texture::{TYPE_2D, TYPE_3D, TYPE_CUBE, TYPE_RECT};
use crate::types::Vector2l;

pub const MAX_COLOR_BUFFERS: usize = 5;

pub const DS_NONE: u32 = 0;
pub const DS_PACKED: u32 = 1;
pub const DS_ERR_NO_PACKED: u32 = 2;
pub const DS_ERR_PACKED_BITS: u32 = 3;
pub const DS_DEPTH: u32 = 4;
pub const DS_ERR_DEPTH: u32 = 5;
pub const DS_STENCIL: u32 = 6;
pub const DS_ERR_STENCIL: u32 = 7;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct DsPlan {
    pub kind: u32,
    pub format: u32,
}

/// `cDepthStencilBufferGL` constructor choice.
pub fn plan_depth_stencil(depth_bits: i32, stencil_bits: i32, packed_ext: bool) -> DsPlan {
    let (kind, format) = if depth_bits > 0 && stencil_bits > 0 {
        if !packed_ext {
            (DS_ERR_NO_PACKED, 0)
        } else if depth_bits != 24 || stencil_bits != 8 {
            (DS_ERR_PACKED_BITS, 0)
        } else {
            (DS_PACKED, c::DEPTH24_STENCIL8_EXT)
        }
    } else if depth_bits > 0 {
        match tables::depth_renderbuffer(depth_bits) {
            0 => (DS_ERR_DEPTH, 0),
            f => (DS_DEPTH, f),
        }
    } else if stencil_bits > 0 {
        match tables::stencil_renderbuffer(stencil_bits) {
            0 => (DS_ERR_STENCIL, 0),
            f => (DS_STENCIL, f),
        }
    } else {
        (DS_NONE, 0)
    };
    DsPlan { kind, format }
}

pub const ENTRY_2D: i32 = 0;
pub const ENTRY_3D: i32 = 1;
pub const ENTRY_CUBE: i32 = 2;

/// Texture types an entry point (`SetTexture2D`, `SetTexture3D`, `SetTextureCubeMap`, and the
/// depth variants) accepts.
pub fn entry_accepts(entry: i32, tex_type: i32) -> bool {
    match entry {
        ENTRY_2D => tex_type == TYPE_2D || tex_type == TYPE_RECT,
        ENTRY_3D => tex_type == TYPE_3D,
        ENTRY_CUBE => tex_type == TYPE_CUBE,
        _ => false,
    }
}

pub const CALL_NONE: u32 = 0;
pub const CALL_TEXTURE_2D: u32 = 1;
pub const CALL_TEXTURE_3D: u32 = 2;

pub const SLOT_COLOR: u32 = 0;
pub const SLOT_DEPTH: u32 = 1;
pub const SLOT_STENCIL: u32 = 2;
pub const SLOT_NONE: u32 = 3;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Attach {
    pub call: u32,
    /// `attachment + index`.
    pub attachment: u32,
    /// Target of the 2D call (cube face resolved) or of the 3D call.
    pub target: u32,
    pub slot: u32,
    /// Color slot index when it is inside the buffer array, else -1.
    pub color_index: i32,
    pub explicit_mip: bool,
    /// Size this attachment contributes when it is the first.
    pub first_size: Vector2l,
}

/// `AttachTexture` call selection, buffer slot and first-size candidate.
pub fn plan_attach(
    attachment: u32,
    index: i32,
    tex_type: i32,
    mip: i32,
    extra: i32,
    tex_size: Vector2l,
) -> Attach {
    let target = tables::texture_target(tex_type);
    let (call, gl_target) = match tex_type {
        TYPE_2D | TYPE_RECT => (CALL_TEXTURE_2D, target),
        TYPE_CUBE => (
            CALL_TEXTURE_2D,
            c::TEXTURE_CUBE_MAP_POSITIVE_X_ARB.wrapping_add_signed(extra),
        ),
        TYPE_3D => (CALL_TEXTURE_3D, target),
        _ => (CALL_NONE, 0),
    };
    let slot = match attachment {
        c::COLOR_ATTACHMENT0_EXT => SLOT_COLOR,
        c::DEPTH_ATTACHMENT_EXT => SLOT_DEPTH,
        c::STENCIL_ATTACHMENT_EXT => SLOT_STENCIL,
        _ => SLOT_NONE,
    };
    let shift = u32::try_from(mip).unwrap_or(0);
    Attach {
        call,
        attachment: attachment.wrapping_add_signed(index),
        target: gl_target,
        slot,
        color_index: usize::try_from(index)
            .ok()
            .filter(|&i| i < MAX_COLOR_BUFFERS)
            .map_or(-1, |i| i as i32),
        explicit_mip: mip > 0,
        first_size: Vector2l {
            x: tex_size.x.wrapping_shr(shift).max(1),
            y: tex_size.y.wrapping_shr(shift).max(1),
        },
    }
}

pub const DRAW_NONE: u32 = 0;
pub const DRAW_SINGLE: u32 = 1;
pub const DRAW_MULTI: u32 = 2;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct DrawPlan {
    pub mode: u32,
    pub count: i32,
    pub buffers: [u32; MAX_COLOR_BUFFERS],
}

/// `CompileAndValidate` draw/read buffer setup from the occupied color slots (bit per slot).
pub fn plan_draw_buffers(occupied: u32) -> DrawPlan {
    let mut p = DrawPlan::default();
    for i in 0..MAX_COLOR_BUFFERS {
        if occupied & 1u32.wrapping_shl(i as u32) != 0 {
            if let Some(b) = p.buffers.get_mut(usize::try_from(p.count).unwrap_or(0)) {
                *b = c::COLOR_ATTACHMENT0_EXT.wrapping_add(i as u32);
            }
            p.count += 1;
        }
    }
    p.mode = match p.count {
        0 => DRAW_NONE,
        1 => DRAW_SINGLE,
        _ => DRAW_MULTI,
    };
    p
}

pub const STATUS_COMPLETE: u32 = 0;
pub const STATUS_UNSUPPORTED: u32 = 1;
pub const STATUS_ATTACHMENT: u32 = 2;
pub const STATUS_MISSING: u32 = 3;
pub const STATUS_DIMENSIONS: u32 = 4;
pub const STATUS_FORMATS: u32 = 5;
pub const STATUS_DRAW_BUFFER: u32 = 6;
pub const STATUS_READ_BUFFER: u32 = 7;
pub const STATUS_UNKNOWN: u32 = 8;

/// `glCheckFramebufferStatus` result to the message class `CompileAndValidate` reports.
pub fn status_class(status: u32) -> u32 {
    match status {
        c::FRAMEBUFFER_COMPLETE_EXT => STATUS_COMPLETE,
        c::FRAMEBUFFER_UNSUPPORTED_EXT => STATUS_UNSUPPORTED,
        c::FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT => STATUS_ATTACHMENT,
        c::FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT => STATUS_MISSING,
        c::FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT => STATUS_DIMENSIONS,
        c::FRAMEBUFFER_INCOMPLETE_FORMATS_EXT => STATUS_FORMATS,
        c::FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT => STATUS_DRAW_BUFFER,
        c::FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT => STATUS_READ_BUFFER,
        _ => STATUS_UNKNOWN,
    }
}
