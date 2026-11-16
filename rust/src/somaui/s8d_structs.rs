//! The recovered script struct defaults and SOMA's property offsets of the structs native code
//! reads, used by the script API loader (data in `s8d_defaults.rs` and `s8d_layout.rs`).

use super::s8d_defaults::{DEFAULTS, StructDefaults};
use super::s8d_layout::LAYOUT;

/// Rebirth only: spare bytes of the SOMA struct.
const EXTRA: [(&str, &str, i32); 1] = [("cImGuiGfx", "mvAlphaLimit", 160)];

pub fn find_defaults(ty: &[u8]) -> Option<&'static StructDefaults> {
    DEFAULTS.iter().find(|d| d.ty.as_bytes() == ty)
}

/// Native code and the recovered defaults use SOMA's layout for these structs in every game; a
/// property that SOMA's struct lacks gets -1.
pub fn native_offset(ty: &[u8], decl: &[u8], offset: i32) -> i32 {
    let name = decl.rsplit(|&c| c == b' ').next().unwrap_or_default();
    let mut known = false;
    for (t, n, o) in LAYOUT.iter().chain(&EXTRA) {
        if t.as_bytes() != ty {
            continue;
        }
        if n.as_bytes() == name {
            return *o;
        }
        known = true;
    }
    if known { -1 } else { offset }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn layout_follows_soma() {
        assert_eq!(native_offset(b"cImGuiGfx", b"float mfX mColor", 99), 40);
        assert_eq!(native_offset(b"cImGuiGfx", b"vec mvAlphaLimit", 99), 160);
        assert_eq!(native_offset(b"cImGuiGfx", b"int mNope", 99), -1);
        assert_eq!(native_offset(b"cOther", b"int mNope", 99), 99);
    }

    #[test]
    fn defaults_cover_the_widget_structs() {
        let b = find_defaults(b"cImGuiButtonData").unwrap();
        assert_eq!(b.size, 2352);
        assert_eq!(b.bytes.len(), b.size - 16);
        assert!(find_defaults(b"cNope").is_none());
        assert_eq!(DEFAULTS.len(), 12);
    }
}
