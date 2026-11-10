//! `iFontData::GetLength` and `GetWordWrapRows`.

use std::ptr;

/// Read-only view of the C++ `std::vector<cGlyph*>`; the advance is read at `advance_offset`.
#[repr(C)]
pub struct Glyphs {
    pub glyphs: *const *const u8,
    pub count: usize,
    pub first: u16,
    pub last: u16,
    pub advance_offset: usize,
}

impl Glyphs {
    fn table(&self) -> &[*const u8] {
        if self.glyphs.is_null() {
            return &[];
        }
        // SAFETY: `glyphs` holds `count` pointers (type contract) for the duration of the call.
        unsafe { std::slice::from_raw_parts(self.glyphs, self.count) }
    }

    /// Length of the chars up to the first NUL; chars outside the font or without a glyph add nothing.
    pub fn length(&self, size_x: f32, text: &[u32]) -> f32 {
        let table = self.table();
        let mut len = 0.0f32;
        for &c in text.iter().take_while(|&&c| c != 0) {
            // The C++ narrows the wide char to unsigned short.
            let g = c as u16;
            if g < self.first || g > self.last {
                continue;
            }
            let g = table
                .get(usize::from(g - self.first))
                .copied()
                .unwrap_or(ptr::null());
            if !g.is_null() {
                // SAFETY: a non-null entry is a live cGlyph with an f32 `mfAdvance` at `advance_offset`.
                let adv = unsafe { ptr::read_unaligned(g.add(self.advance_offset).cast::<f32>()) };
                len = adv.mul_add(size_x, len);
            }
        }
        len
    }
}

fn is_fullwidth(c: u32) -> bool {
    matches!(c, 12290 | 65292 | 65311 | 65281)
}

/// `wstring(s.substr(start, count).c_str())` as a `(start, len)` range; `None` where `substr` throws.
fn row(s: &[u32], start: usize, count: usize) -> Option<(usize, usize)> {
    let tail = s.get(start..)?;
    let n = count.min(tail.len());
    let body = tail.get(..n)?;
    Some((start, body.iter().position(|&c| c == 0).unwrap_or(n)))
}

/// Rows of `s` as `(start, len)` ranges; `None` where the C++ `substr` throws.
pub fn word_wrap_rows(
    glyphs: &Glyphs,
    max_length: f32,
    size_x: f32,
    s: &[u32],
) -> Option<Vec<(usize, usize)>> {
    let n = u32::try_from(s.len()).ok()?;
    // Counts are unsigned in the C++: a wrapped `pos - first` means "to the end".
    let sub = |first: u32, count: u32| row(s, first as usize, count as usize);
    let measure = |first: u32, count: u32| -> Option<f32> {
        let (a, l) = sub(first, count)?;
        Some(glyphs.length(size_x, s.get(a..a.checked_add(l)?)?))
    };

    let mut rows = 0usize;
    let mut first = 0u32;
    let mut last_space = 0u32;
    let mut list: Vec<(u32, bool)> = Vec::new();
    for pos in 0..n {
        let c = s.get(pos as usize).copied()?;
        if c == 32 || c == 10 || is_fullwidth(c) {
            let len = measure(first, pos.wrapping_sub(first))?;
            let mut nothing = true;
            if len > max_length && !is_fullwidth(c) {
                rows += 1;
                list.push((last_space, true));
                first = last_space.wrapping_add(1);
                last_space = pos;
                nothing = false;
            } else if len > max_length {
                list.push((last_space.wrapping_add(1), false));
                first = last_space.wrapping_add(1);
                last_space = pos;
                rows += 1;
                nothing = false;
            }
            if c == 10 {
                last_space = pos;
                first = last_space.wrapping_add(1);
                list.push((last_space, true));
                rows += 1;
                nothing = false;
            }
            if nothing {
                last_space = pos;
            }
        }
    }
    if measure(first, n.wrapping_sub(first))? > max_length {
        rows += 1;
        list.push((last_space, true));
    }

    let mut out = Vec::new();
    if rows == 0 {
        out.push(row(s, 0, s.len())?);
    } else {
        let mut first = 0u32;
        for (pos, incr) in list {
            out.push(sub(first, pos.wrapping_sub(first))?);
            first = pos;
            if incr {
                first = first.wrapping_add(1);
            }
        }
        out.push(sub(first, u32::MAX)?);
    }
    Some(out)
}
