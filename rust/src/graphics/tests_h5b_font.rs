use std::path::PathBuf;

use super::font::{GlyphSet, word_wrap_rows};
use super::h5b_font::{FontTable, Glyph};

fn bits(s: &str) -> f32 {
    f32::from_bits(u32::from_str_radix(s, 16).unwrap_or(0))
}

fn text(s: &str) -> Vec<u32> {
    if s == "-" {
        return Vec::new();
    }
    s.split(',')
        .map(|u| u32::from_str_radix(u, 16).unwrap_or(0))
        .collect()
}

fn hex(t: &[u32]) -> String {
    if t.is_empty() {
        return "-".into();
    }
    t.iter()
        .map(|u| format!("{u:x}"))
        .collect::<Vec<_>>()
        .join(",")
}

fn replay(src: &str) -> String {
    let mut table = FontTable::default();
    let mut glyphs: Vec<Option<Glyph>> = Vec::new();
    let mut out = String::new();
    for line in src.lines() {
        let w: Vec<&str> = line.split_whitespace().collect();
        match w.as_slice() {
            ["font", first, last, h] => {
                table.first = first.parse().unwrap_or(0);
                table.last = last.parse().unwrap_or(0);
                table.height = bits(h);
                glyphs = vec![None; usize::from(table.last - table.first) + 1];
                out.push_str(line);
            }
            ["g", i, adv, ox, oy, sx, sy] => {
                let g = Glyph {
                    gfx: std::ptr::null_mut(),
                    offset: [bits(ox), bits(oy)],
                    size: [bits(sx), bits(sy)],
                    advance: bits(adv),
                };
                if let Some(slot) = glyphs.get_mut(i.parse::<usize>().unwrap_or(usize::MAX)) {
                    *slot = Some(g);
                }
                out.push_str(line);
            }
            ["len", size, t, "->", _] => {
                table.set_glyphs(std::mem::take(&mut glyphs));
                let r = table.length(bits(size), &text(t));
                out.push_str(&format!("len {size} {t} -> {:08x}", r.to_bits()));
                glyphs = (0..table.len()).map(|i| table.glyph(i).copied()).collect();
            }
            ["wrap", max, size, t, "->", _] => {
                table.set_glyphs(std::mem::take(&mut glyphs));
                let s = text(t);
                let rows = word_wrap_rows(&table, bits(max), bits(size), &s).map_or_else(
                    || "throw".to_string(),
                    |rows| {
                        if rows.is_empty() {
                            return "none".to_string();
                        }
                        let parts: Vec<String> = rows
                            .iter()
                            .map(|&(a, l)| hex(s.get(a..a + l).unwrap_or_default()))
                            .collect();
                        parts.join(";")
                    },
                );
                out.push_str(&format!("wrap {max} {size} {t} -> {rows}"));
                glyphs = (0..table.len()).map(|i| table.glyph(i).copied()).collect();
            }
            _ => {}
        }
        out.push('\n');
    }
    out
}

#[test]
fn font_table_matches_cpp() {
    let dir = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("tests/golden/H5");
    for s in 1..=4 {
        let want =
            std::fs::read_to_string(dir.join(format!("font_{s:03}.txt"))).unwrap_or_default();
        assert!(!want.is_empty(), "missing golden for seed {s}");
        assert_eq!(replay(&want), want, "font seed {s}");
    }
}
