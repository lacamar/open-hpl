//! `cFrameSubImage` texture coordinates and `iFontData::CreateGlyph` ratios.

use crate::math::Rect2l;
use crate::types::{Vector2f, Vector2l};

const CONTRACT: f32 = 0.001;

/// Texture coordinates of the four quad corners (top-left, top-right, bottom-right, bottom-left)
/// for `rect` inside a frame bitmap of `src_size`, pulled in by a fixed contraction.
pub fn subimage_uvs(rect: &Rect2l, src_size: &Vector2l) -> [Vector2f; 4] {
    let (sx, sy) = (src_size.x as f32, src_size.y as f32);
    let size = Vector2f {
        x: rect.w as f32 / sx,
        y: rect.h as f32 / sy,
    };
    let pos = Vector2f {
        x: rect.x as f32 / sx,
        y: rect.y as f32 / sy,
    };
    let v = |x: f32, y: f32| Vector2f { x, y };
    [
        v(pos.x + CONTRACT, pos.y + CONTRACT),
        v(pos.x + size.x - CONTRACT, pos.y + CONTRACT),
        v(pos.x + size.x - CONTRACT, pos.y + size.y - CONTRACT),
        v(pos.x + CONTRACT, pos.y + size.y - CONTRACT),
    ]
}

pub struct GlyphMetrics {
    pub offset: Vector2f,
    pub size: Vector2f,
    pub advance: f32,
}

pub fn glyph_metrics(
    offset: &Vector2l,
    size: &Vector2l,
    font_size: &Vector2l,
    advance: i32,
    ratio: &Vector2f,
) -> GlyphMetrics {
    let (fx, fy) = (font_size.x as f32, font_size.y as f32);
    GlyphMetrics {
        size: Vector2f {
            x: size.x as f32 / fx * ratio.x,
            y: size.y as f32 / fy * ratio.y,
        },
        offset: Vector2f {
            x: offset.x as f32 / fx * ratio.x,
            y: offset.y as f32 / fy * ratio.y,
        },
        advance: advance as f32 / fx * ratio.x,
    }
}
