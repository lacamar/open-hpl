//! Resolution list, language list and sound device rules of the launcher dialog.

use crate::luxbase::cfg::Cfg;
use crate::string;

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Mode {
    pub display: i32,
    pub width: i32,
    pub height: i32,
}

/// `GetAvailableVideoModes` after the platform list: `true` when the configured custom size is
/// missing from `modes` and must be appended; an unset size is rewritten to -1/-1/0.
pub fn fix_config_size(cfg: &mut dyn Cfg, modes: &[Mode]) -> bool {
    let w = cfg.int(b"Screen", b"Width", -1);
    let h = cfg.int(b"Screen", b"Height", -1);
    let d = cfg.int(b"Screen", b"Display", 0);
    if w == -1 || h == -1 {
        cfg.set_int(b"Screen", b"Width", -1);
        cfg.set_int(b"Screen", b"Height", -1);
        cfg.set_int(b"Screen", b"Display", 0);
        return false;
    }
    !modes
        .iter()
        .any(|m| m.display == d && m.width == w && m.height == h)
}

/// The configured mode, read as `ReselectResolution` and `PopulateResolutions` do.
pub fn configured(cfg: &mut dyn Cfg) -> Mode {
    let width = cfg.int(b"Screen", b"Width", -1);
    let height = cfg.int(b"Screen", b"Height", -1);
    let display = cfg.int(b"Screen", b"Display", 0);
    Mode {
        display,
        width,
        height,
    }
}

/// Last mode equal to `cur` (display and size).
pub fn last_match(modes: &[Mode], cur: Mode) -> i32 {
    modes
        .iter()
        .rposition(|m| *m == cur)
        .map_or(-1, |p| p as i32)
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Custom {
    Reject,
    Select(i32),
    Add,
}

/// `AddCustomResolution` after the config write: sizes below 640x480 are rejected; otherwise the
/// last mode of that size (any display) is selected, or the size is added.
pub fn custom_resolution(modes: &[Mode], w: i32, h: i32) -> Custom {
    if w < 640 || h < 480 {
        return Custom::Reject;
    }
    modes
        .iter()
        .rposition(|m| m.width == w && m.height == h)
        .map_or(Custom::Add, |p| Custom::Select(p as i32))
}

/// Dialog row to select after `PopulateResolutions`.
pub fn populate_selection(last_match: i32, w: i32, h: i32) -> i32 {
    if last_match == -1 && (w == -1 || h == -1) {
        0
    } else {
        last_match
    }
}

pub fn multi_display(modes: &[Mode]) -> bool {
    let mut seen: Vec<i32> = modes.iter().map(|m| m.display).collect();
    seen.sort_unstable();
    seen.dedup();
    seen.len() > 1
}

pub struct Label<'a> {
    pub fullscreen_desktop: bool,
    pub fullscreen_desktop_text: &'a [u8],
    pub display_name: &'a [u8],
    pub width: i32,
    pub height: i32,
    pub custom: bool,
    pub custom_text: &'a [u8],
    pub multi_display: bool,
}

/// `AspectRatioForResolution`; a zero side gives 0:0 and 8:5 is shown as 16:10.
pub fn aspect_ratio(w: i32, h: i32) -> (i32, i32) {
    if w == 0 || h == 0 {
        return (0, 0);
    }
    let (mut a, mut b) = (w, h);
    while a.checked_rem(b).unwrap_or(0) != 0 {
        let r = a.checked_rem(b).unwrap_or(0);
        a = b;
        b = r;
    }
    let (rw, rh) = (w.wrapping_div(b), h.wrapping_div(b));
    if (rw, rh) == (8, 5) {
        (16, 10)
    } else {
        (rw, rh)
    }
}

/// Text of one resolution row.
pub fn label(l: &Label) -> Vec<u8> {
    let mut s = if l.fullscreen_desktop {
        [l.fullscreen_desktop_text, b" ", l.display_name].concat()
    } else {
        let (rw, rh) = aspect_ratio(l.width, l.height);
        let mut s = [
            &string::int_to_string(l.width, 0)[..],
            b"x",
            &string::int_to_string(l.height, 0),
            b"  [",
            &string::int_to_string(rw, 0),
            b":",
            &string::int_to_string(rh, 0),
            b"]",
        ]
        .concat();
        if l.custom {
            s.extend_from_slice(b" (");
            s.extend_from_slice(l.custom_text);
            s.push(b')');
        }
        s
    };
    if l.multi_display {
        s.extend_from_slice(b"  (");
        s.extend_from_slice(&string::replace_char_to(l.display_name, b"/", b"-"));
        s.push(b')');
    }
    s
}

/// Language name shown for a `.lang` file: lowercase, extension cut at its last dot.
pub fn language_name(file: &[u8]) -> Vec<u8> {
    let cut = string::last_pos(file, b".");
    string::to_lower(string::sub(file, 0, cut, false).unwrap_or(&[]))
}

/// `PopulateLanguages` row: the last file equal to the configured language, else the first
/// equal to the default.
pub fn language_index(files: &[&[u8]], default: &[u8], current: &[u8]) -> i32 {
    let cur = files.iter().rposition(|f| *f == current);
    let def = files.iter().position(|f| *f == default);
    cur.or(def).map_or(-1, |p| p as i32)
}

pub fn current_language(cfg: &mut dyn Cfg, default: &[u8]) -> Vec<u8> {
    cfg.string(b"Main", b"StartLanguage", default)
}

/// Language folder searched for `.lang` files: the user resource copy when it exists.
pub fn languages_dir(folder: &[u8], user_dir: &[u8], user_exists: bool) -> Vec<u8> {
    if user_exists {
        [user_dir, folder].concat()
    } else {
        folder.to_vec()
    }
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Device {
    pub is_default: bool,
    pub is_current: bool,
}

/// `PopulateSoundDevices` row; `true` when the unsupported-current entry is appended after the
/// listed devices.
pub fn sound_selection(hint: i32, devs: &[Device]) -> (i32, bool) {
    let mut index = -1;
    let mut supported = false;
    for (i, d) in devs.iter().enumerate() {
        if hint == -1 && index == -1 && d.is_default {
            index = i as i32;
        }
        if d.is_current {
            supported = true;
            index = i as i32;
        }
    }
    if !supported && hint != -1 {
        (devs.len() as i32, true)
    } else {
        (index, false)
    }
}

pub fn sound_hint(cfg: &mut dyn Cfg) -> i32 {
    cfg.int(b"Sound", b"Device", -1)
}

/// FLTK menu text: a slash would start a submenu.
pub fn escape_device(name: &[u8]) -> Vec<u8> {
    string::replace_string_to(name, b"/", b"\\/")
}
