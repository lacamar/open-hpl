//! BMFont metric math of `cSDLFontData::CreateFromBitmapFile`. XML access, textures and glyph
//! creation stay in C++.

/// Glyph tables beyond this many entries are refused (the original asked for up to 2^31 pointers).
pub const MAX_GLYPHS: usize = 1 << 24;

#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Header {
    pub line_height: i32,
    pub base: i32,
    pub height: f32,
    pub size_ratio_x: f32,
}

/// `outline`, `line_height` and `base` are the parsed `info outline`, `common lineHeight`, `common base`.
pub fn header(outline: i32, line_height: i32, base: i32) -> Header {
    let pad = outline.wrapping_mul(2);
    let line_height = line_height.wrapping_add(pad);
    let base = base.wrapping_add(pad);
    Header {
        line_height,
        base,
        height: line_height as f32,
        size_ratio_x: base as f32 / line_height as f32,
    }
}

/// Glyph table size for the largest char id (-1 when there is none); None when it is refused.
pub fn glyph_count(largest_id: i32) -> Option<usize> {
    usize::try_from(i64::from(largest_id) + 1)
        .ok()
        .filter(|&n| n <= MAX_GLYPHS)
}

pub fn glyph_id_valid(id: i32, glyph_count: usize) -> bool {
    usize::try_from(id).is_ok_and(|i| i < glyph_count)
}

pub fn glyph_advance(xadvance: i32, outline: i32) -> i32 {
    xadvance.wrapping_add(outline)
}

pub fn page_valid(page: i32, pages: usize) -> bool {
    usize::try_from(page).is_ok_and(|p| p < pages)
}
