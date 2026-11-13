//! `iFontData` glyph table: metrics and `cGuiGfxElement` pointers per character, and the string
//! measuring built on it. The C++ `cGlyph` objects stay (the GUI readers use their layout); the table
//! is mirrored here by font handle so Rust code can measure and read glyphs without the C++ type.

use core::ffi::c_void;
use std::sync::{Mutex, MutexGuard, PoisonError};

use super::font::{self, GlyphSet};
use crate::obj::{Arena, Handle};
use crate::string_ffi::{Sink, emit, units};

#[derive(Clone, Copy, Debug, PartialEq)]
#[repr(C)]
pub struct Glyph {
    /// The glyph's `cGuiGfxElement`, owned by the C++ `cGlyph`.
    pub gfx: *mut c_void,
    pub offset: [f32; 2],
    pub size: [f32; 2],
    pub advance: f32,
}

#[derive(Default)]
pub struct FontTable {
    pub first: u16,
    pub last: u16,
    pub height: f32,
    pub size_ratio: [f32; 2],
    glyphs: Vec<Option<Glyph>>,
}

impl FontTable {
    pub fn set_glyphs(&mut self, g: Vec<Option<Glyph>>) {
        self.glyphs = g;
    }

    pub fn glyph(&self, i: usize) -> Option<&Glyph> {
        self.glyphs.get(i).and_then(Option::as_ref)
    }

    pub fn len(&self) -> usize {
        self.glyphs.len()
    }

    pub fn is_empty(&self) -> bool {
        self.glyphs.is_empty()
    }
}

impl GlyphSet for FontTable {
    fn length(&self, size_x: f32, text: &[u32]) -> f32 {
        let mut len = 0.0f32;
        for &c in text.iter().take_while(|&&c| c != 0) {
            // The C++ narrows the wide char to unsigned short.
            let g = c as u16;
            if g < self.first || g > self.last {
                continue;
            }
            if let Some(gl) = self.glyph(usize::from(g - self.first)) {
                len = gl.advance.mul_add(size_x, len);
            }
        }
        len
    }
}

static FONTS: Mutex<Option<Arena<FontTable>>> = Mutex::new(None);

fn fonts() -> MutexGuard<'static, Option<Arena<FontTable>>> {
    FONTS.lock().unwrap_or_else(PoisonError::into_inner)
}

fn with<R>(h: u64, f: impl FnOnce(&mut FontTable) -> R) -> Option<R> {
    let mut g = fonts();
    let t = g.as_mut()?.get_mut(Handle::from_bits(h)?)?;
    Some(f(t))
}

// SAFETY: the table is only touched under the lock and never dereferences the `gfx` pointers.
unsafe impl Send for FontTable {}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h5b_font_new() -> u64 {
    let mut g = fonts();
    g.get_or_insert_with(Arena::default)
        .insert(FontTable::default())
        .map_or(0, Handle::to_bits)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h5b_font_free(h: u64) {
    if let (Some(a), Some(h)) = (fonts().as_mut(), Handle::from_bits(h)) {
        drop(a.remove(h));
    }
}

/// Header of `ohpl_h5b_font_set`.
#[repr(C)]
pub struct FontHdr {
    pub first: u16,
    pub last: u16,
    pub height: f32,
    pub ratio: [f32; 2],
}

/// Replaces the table with `n` entries; `present[i] == 0` marks an absent glyph.
///
/// # Safety
/// `hdr` is readable; `glyphs` and `present` hold `n` readable entries.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h5b_font_set(
    h: u64,
    hdr: *const FontHdr,
    glyphs: *const Glyph,
    present: *const u8,
    n: usize,
) {
    let (gs, ps): (&[Glyph], &[u8]) = if n == 0 {
        (&[], &[])
    } else {
        // SAFETY: caller contract.
        unsafe {
            (
                core::slice::from_raw_parts(glyphs, n),
                core::slice::from_raw_parts(present, n),
            )
        }
    };
    let table = gs
        .iter()
        .zip(ps)
        .map(|(g, &p)| (p != 0).then_some(*g))
        .collect();
    // SAFETY: caller contract.
    let hdr = unsafe { &*hdr };
    with(h, |t| {
        t.first = hdr.first;
        t.last = hdr.last;
        t.height = hdr.height;
        t.size_ratio = hdr.ratio;
        t.set_glyphs(table);
    });
}

/// Writes glyph `i` to `out`; false where absent.
///
/// # Safety
/// `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h5b_font_glyph(h: u64, i: usize, out: *mut Glyph) -> bool {
    with(h, |t| t.glyph(i).copied()).flatten().is_some_and(|g| {
        // SAFETY: caller contract.
        unsafe { out.write(g) };
        true
    })
}

/// # Safety
/// `s` has `n` wide chars.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h5b_font_length(h: u64, size_x: f32, s: *const u32, n: usize) -> f32 {
    // SAFETY: caller contract.
    let text = unsafe { units(s, n) };
    with(h, |t| t.length(size_x, text)).unwrap_or(0.0)
}

/// Calls `sink` once per row; false where the C++ `substr` throws or the handle is stale.
///
/// # Safety
/// `s` has `n` wide chars; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h5b_font_word_wrap(
    h: u64,
    max_length: f32,
    size_x: f32,
    s: *const u32,
    n: usize,
    sink: Sink<u32>,
    ctx: *mut c_void,
) -> bool {
    // SAFETY: caller contract.
    let text = unsafe { units(s, n) };
    let Some(Some(rows)) = with(h, |t| font::word_wrap_rows(t, max_length, size_x, text)) else {
        return false;
    };
    for (start, len) in rows {
        if let Some(r) = text.get(start..start + len) {
            // SAFETY: caller contract.
            unsafe { emit(sink, ctx, r) };
        }
    }
    true
}
