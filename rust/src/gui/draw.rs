//! `cGuiSet::DrawGfx` and `DrawTextFromCharArry`.

use std::ffi::c_void;
use std::ptr;

use super::list::{GfxVertex, Object, RenderList, Rotation};
use crate::graphics::font::Glyphs;
use crate::math::Rect2f;
use crate::types::{Color, Vector2f, Vector3f};

/// `eGuiMaterial_LastEnum`: no custom material.
pub const MATERIAL_NONE: i32 = 6;
pub const ALIGN_LEFT: i32 = 0;
pub const ALIGN_RIGHT: i32 = 1;
pub const ALIGN_CENTER: i32 = 2;

/// What the C++ reads from a `cGuiGfxElement`.
#[repr(C)]
pub struct GfxInfo {
    pub offset: Vector3f,
    pub image_size: Vector2f,
    pub material: *const *const c_void,
    pub texture0: *const *const c_void,
    pub vtx: *const GfxVertex,
}

/// Callbacks into the C++ gui set; `ctx` is passed back unchanged.
#[repr(C)]
pub struct Host {
    pub ctx: *mut c_void,
    pub gfx_info: unsafe extern "C" fn(*mut c_void, *mut c_void, *mut GfxInfo),
    pub gfx_flush: unsafe extern "C" fn(*mut c_void, *mut c_void),
    pub material: unsafe extern "C" fn(*mut c_void, i32) -> *mut c_void,
}

/// State of the gui set at the call: current clip region (null: draw nothing), its rect and the draw offset.
#[repr(C)]
pub struct Frame {
    pub clip: *const c_void,
    pub clip_rect: Rect2f,
    pub draw_offset: Vector3f,
}

/// `cMath::CheckRectIntersection(cRect2f, cRect2f)`.
pub fn rects_intersect(mut a: Rect2f, mut b: Rect2f) -> bool {
    for r in [&mut a, &mut b] {
        if r.w < 0.0 {
            r.w *= -1.0;
            r.x -= r.w;
        }
        if r.h < 0.0 {
            r.h *= -1.0;
            r.y -= r.h;
        }
    }
    !(a.x > b.x + b.w || b.x > a.x + a.w || a.y > b.y + b.h || b.y > a.y + a.h)
}

pub struct Gfx<'a> {
    pub pos: Vector3f,
    pub size: Vector2f,
    pub color: Color,
    pub material: i32,
    pub angle: f32,
    pub pivot: Option<Vector2f>,
    pub gfx: *mut c_void,
    pub host: &'a Host,
}

pub fn draw_gfx(list: &mut RenderList, frame: &Frame, d: &Gfx) {
    if frame.clip.is_null() {
        return;
    }
    let rect = frame.clip_rect;
    if rect.w == 0.0 || rect.h == 0.0 {
        return;
    }
    let host = d.host;
    let mut info = GfxInfo {
        offset: Vector3f::default(),
        image_size: Vector2f::default(),
        material: ptr::null(),
        texture0: ptr::null(),
        vtx: ptr::null(),
    };
    // SAFETY: host contract; `gfx` is a live cGuiGfxElement.
    unsafe { (host.gfx_info)(host.ctx, d.gfx, &mut info) };

    let abs = Vector3f {
        x: d.pos.x + info.offset.x + frame.draw_offset.x,
        y: d.pos.y + info.offset.y + frame.draw_offset.y,
        z: d.pos.z + info.offset.z + frame.draw_offset.z,
    };
    let size = if d.size.x < 0.0 {
        info.image_size
    } else {
        d.size
    };
    if rect.w > 0.0 {
        let gfx_rect = Rect2f {
            x: abs.x,
            y: abs.y,
            w: size.x,
            h: size.y,
        };
        if !rects_intersect(rect, gfx_rect) {
            return;
        }
    }
    // SAFETY: as above.
    unsafe { (host.gfx_flush)(host.ctx, d.gfx) };

    let custom_material = if d.material != MATERIAL_NONE {
        // SAFETY: as above.
        unsafe { (host.material)(host.ctx, d.material) }.cast_const()
    } else {
        ptr::null()
    };
    let rotation = (d.angle != 0.0).then(|| Rotation {
        angle: d.angle,
        pivot: d.pivot.unwrap_or(Vector2f {
            x: size.x * 0.5,
            y: size.y * 0.5,
        }),
    });
    list.push(Object {
        vtx: info.vtx,
        gfx_material: info.material,
        gfx_texture: info.texture0,
        custom_material,
        clip: frame.clip,
        pos: abs,
        size,
        color: d.color,
        rotation,
    });
}

/// Layout of `cGlyph` and the font's glyph table.
#[repr(C)]
pub struct GlyphTable {
    pub glyphs: *const *const u8,
    pub count: usize,
    pub first: u16,
    pub last: u16,
    pub off_gfx: usize,
    pub off_offset: usize,
    pub off_size: usize,
    pub off_advance: usize,
}

impl GlyphTable {
    fn table(&self) -> &[*const u8] {
        if self.glyphs.is_null() {
            return &[];
        }
        // SAFETY: `glyphs` holds `count` pointers for the duration of the call (type contract).
        unsafe { std::slice::from_raw_parts(self.glyphs, self.count) }
    }

    fn read<T: Copy>(glyph: *const u8, offset: usize) -> T {
        // SAFETY: a non-null table entry is a live cGlyph with a `T` at `offset` (type contract).
        unsafe { ptr::read_unaligned(glyph.add(offset).cast::<T>()) }
    }
}

pub struct Text<'a> {
    pub text: &'a [u32],
    pub size: Vector2f,
    pub pos: Vector3f,
    pub color: Color,
    pub material: i32,
    pub align: i32,
    pub host: &'a Host,
}

pub fn draw_text(list: &mut RenderList, frame: &Frame, table: &GlyphTable, t: &Text) {
    let mut pos = t.pos;
    if t.align == ALIGN_CENTER || t.align == ALIGN_RIGHT {
        let view = Glyphs {
            glyphs: table.glyphs,
            count: table.count,
            first: table.first,
            last: table.last,
            advance_offset: table.off_advance,
        };
        let len = view.length(t.size.x, t.text);
        pos.x -= if t.align == ALIGN_CENTER {
            len / 2.0
        } else {
            len
        };
    }
    let glyphs = table.table();
    for &c in t.text.iter().take_while(|&&c| c != 0) {
        if c < u32::from(table.first) || c > u32::from(table.last) {
            continue;
        }
        let Some(&glyph) = usize::try_from(c - u32::from(table.first))
            .ok()
            .and_then(|n| glyphs.get(n))
        else {
            continue;
        };
        if glyph.is_null() {
            continue;
        }
        let offset: Vector2f = GlyphTable::read(glyph, table.off_offset);
        let size: Vector2f = GlyphTable::read(glyph, table.off_size);
        let advance: f32 = GlyphTable::read(glyph, table.off_advance);
        let gfx: *mut c_void = GlyphTable::read(glyph, table.off_gfx);
        let draw = Gfx {
            pos: Vector3f {
                x: offset.x.mul_add(t.size.x, pos.x),
                y: offset.y.mul_add(t.size.y, pos.y),
                z: pos.z,
            },
            size: Vector2f {
                x: size.x * t.size.x,
                y: size.y * t.size.y,
            },
            color: t.color,
            material: t.material,
            angle: 0.0,
            pivot: None,
            gfx,
            host: t.host,
        };
        draw_gfx(list, frame, &draw);
        pos.x = advance.mul_add(t.size.x, pos.x);
    }
}
