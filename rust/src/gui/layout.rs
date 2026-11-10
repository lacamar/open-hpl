//! Pure geometry of `cGuiSet`: clip regions, scissor rects, tooltip and popup placement.

use crate::math::{Rect2f, Vector2f, Vector3f};
use crate::types::Vector2l;

/// `cMath::GetClipRect` (kept private to `math`).
fn clip_rect(src: &Rect2f, dest: &Rect2f) -> Rect2f {
    let mut o = *src;
    if o.x < dest.x {
        o.w -= dest.x - o.x;
        o.x = dest.x;
    }
    if o.y < dest.y {
        o.h -= dest.y - o.y;
        o.y = dest.y;
    }
    if o.x + o.w > dest.x + dest.w {
        o.w -= (o.x + o.w) - (dest.x + dest.w);
    }
    if o.y + o.h > dest.y + dest.h {
        o.h -= (o.y + o.h) - (dest.y + dest.h);
    }
    o
}

/// `cGuiClipRegion::CreateChild`: a negative parent width means unclipped.
pub fn child_clip_rect(parent: &Rect2f, pos: Vector3f, size: Vector2f) -> Rect2f {
    let rect = Rect2f {
        x: pos.x,
        y: pos.y,
        w: size.x,
        h: size.y,
    };
    if parent.w < 0.0 {
        return rect;
    }
    let mut clipped = clip_rect(&rect, parent);
    if clipped.w < 0.0 {
        clipped.w = 0.0;
    }
    if clipped.h < 0.0 {
        clipped.h = 0.0;
    }
    clipped
}

/// Scissor `(pos, size)` in target pixels for a clip rect in virtual coordinates. The float to
/// int casts saturate where the C++ conversion is undefined (matching the aarch64 `fcvtzs`).
pub fn scissor(
    rect: &Rect2f,
    target: Vector2l,
    virtual_size: Vector2f,
    virtual_offset: Vector2f,
    flip_y: bool,
) -> (Vector2l, Vector2l) {
    let scale = Vector2f {
        x: target.x as f32 / virtual_size.x,
        y: target.y as f32 / virtual_size.y,
    };
    let mut pos = Vector2l {
        x: ((rect.x + virtual_offset.x) * scale.x) as i32,
        y: ((rect.y + virtual_offset.y) * scale.y) as i32,
    };
    let size = Vector2l {
        x: (rect.w * scale.x) as i32,
        y: (rect.h * scale.y) as i32,
    };
    if flip_y {
        pos.y = target.y.wrapping_sub(pos.y).wrapping_sub(size.y);
    }
    (pos, size)
}

pub struct Tooltip {
    pub pos: Vector3f,
    pub size: Vector2f,
    pub background_size: Vector2f,
}

pub struct TooltipInput {
    pub mouse: Vector2f,
    pub mouse_z: f32,
    pub pointer_size: Vector2f,
    pub virtual_size: Vector2f,
    /// `GetLength` of the tip text, before the label padding.
    pub text_length: f32,
    pub font_height: f32,
    pub rows: i32,
}

/// Placement of the tooltip frame in `cGuiSet::UpdateToolTip`.
pub fn tooltip(i: &TooltipInput) -> Tooltip {
    let text_length = i.text_length + 3.0 + 3.0;
    let max_length = i.virtual_size.x * 0.4;
    let mut pos = Vector3f {
        x: i.mouse.x + i.pointer_size.x,
        y: i.mouse.y + i.pointer_size.y,
        z: i.mouse_z - 2.0,
    };
    let size = Vector2f {
        x: if text_length < max_length {
            text_length
        } else {
            max_length
        },
        y: (i.rows as f32).mul_add(i.font_height + 2.0, -2.0) + 3.0 + 3.0,
    };
    if pos.x + size.x > i.virtual_size.x {
        pos.x = i.virtual_size.x - size.x;
    }
    if pos.y + size.y > i.virtual_size.y {
        pos.y = i.virtual_size.y - size.y;
    }
    Tooltip {
        pos,
        size,
        background_size: Vector2f {
            x: size.x - 2.0,
            y: size.y - 2.0,
        },
    }
}

/// `cGuiSet::PositionWidgetInsideBounds`.
pub fn position_inside_bounds(mut pos: Vector3f, size: Vector2f, set: Vector2f) -> Vector3f {
    if pos.x < 0.0 {
        pos.x = 0.0;
    }
    if pos.x + size.x > set.x {
        pos.x = set.x - size.x;
    }
    if pos.y < 0.0 {
        pos.y = 0.0;
    }
    if pos.y + size.y > set.y {
        pos.y = set.y - size.y;
    }
    pos
}

/// `cGuiSet::IncPopUpZ`.
pub fn next_popup_z(z: f32) -> f32 {
    let z = z + 5.0;
    if z >= 500.0 { 20.0 } else { z }
}

/// `cGuiSet::SetVirtualSize`.
pub fn virtual_size(size: Vector2f, global_scale: f32, ignore_global_scale: bool) -> Vector2f {
    if global_scale != 1.0 && !ignore_global_scale {
        Vector2f {
            x: size.x / global_scale,
            y: size.y / global_scale,
        }
    } else {
        size
    }
}
