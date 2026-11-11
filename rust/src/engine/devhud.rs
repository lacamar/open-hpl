//! `cDevHud` decisions: overlay layout, bug-report text wrapping and editing, key routing, the
//! system-info lines and the report JSON. GUI calls, SDL and file I/O stay in C++.

use crate::string::{float_to_string, int_to_string, replace_string_to};
use crate::system::json_escape;
use std::ffi::{c_char, c_int};

unsafe extern "C" {
    fn snprintf(buf: *mut c_char, n: usize, fmt: *const c_char, ...) -> c_int;
}

fn fmax(a: f32, b: f32) -> f32 {
    if a > b { a } else { b }
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct HudLayout {
    pub font: f32,
    pub pad: f32,
    pub line_h: f32,
    pub bg_w: f32,
    pub bg_h: f32,
    pub text_x: f32,
    pub text_y: f32,
}

pub fn hud_font_size(screen_y: i32) -> f32 {
    fmax(14.0, screen_y as f32 / 60.0)
}

/// Stats overlay geometry for `lines` text lines whose widest is `max_width`.
pub fn hud_layout(font: f32, max_width: f32, lines: usize) -> HudLayout {
    let pad = font * 0.5;
    let line_h = font * 1.25;
    HudLayout {
        font,
        pad,
        line_h,
        bg_w: max_width + pad * 2.0,
        bg_h: line_h.mul_add(lines as f32, pad * 2.0),
        text_x: pad + pad,
        text_y: pad + pad,
    }
}

/// `Render`'s stats line, formatted by the C library like the original `swprintf`.
pub fn stats_line(w: i32, h: i32, fps: f32, frame_ms: f32) -> Vec<u8> {
    let mut buf = [0u8; 256];
    // SAFETY: format matches (int, int, double, double) varargs; the buffer bound is passed.
    let n = unsafe {
        snprintf(
            buf.as_mut_ptr().cast(),
            buf.len(),
            c"%dx%d  |  %.0f FPS  %.1f ms".as_ptr(),
            w,
            h,
            f64::from(fps),
            f64::from(frame_ms),
        )
    };
    let len = usize::try_from(n).unwrap_or(0).min(buf.len() - 1);
    buf.get(..len).unwrap_or_default().to_vec()
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct BugMetrics {
    pub font: f32,
    pub pad: f32,
    pub line_h: f32,
    pub box_w: f32,
    pub text_w: f32,
}

pub fn bug_metrics(sx: i32, sy: i32) -> BugMetrics {
    let font = fmax(16.0, sy as f32 / 45.0);
    let pad = font * 0.75;
    let box_w = sx as f32 * 0.6;
    BugMetrics {
        font,
        pad,
        line_h: font * 1.25,
        box_w,
        text_w: box_w - pad * 2.0,
    }
}

/// The saved-toast box widens to fit the path line.
pub fn toast_box_w(m: &BugMetrics, path_width: f32) -> f32 {
    fmax(m.box_w, path_width + m.pad * 2.0)
}

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct BugBox {
    pub x: f32,
    pub y: f32,
    pub w: f32,
    pub h: f32,
    pub text_x: f32,
    pub text_y: f32,
}

pub fn bug_box(m: &BugMetrics, box_w: f32, lines: usize, sx: i32, sy: i32) -> BugBox {
    let rows = i32::try_from(lines).unwrap_or(i32::MAX).max(4);
    let x = (sx as f32 - box_w) * 0.5;
    let y = sy as f32 * 0.25;
    BugBox {
        x,
        y,
        w: box_w,
        h: m.line_h.mul_add(rows as f32, m.pad * 2.0),
        text_x: x + m.pad,
        text_y: y + m.pad,
    }
}

/// The saved toast shows for 4 s after the save time (0 = never saved).
pub fn toast_active(saved_ms: u64, now_ms: u64) -> bool {
    saved_ms != 0 && now_ms.wrapping_sub(saved_ms) < 4000
}

pub fn should_render(hud: bool, bug_open: bool, toast: bool) -> bool {
    hud || bug_open || toast
}

/// Blink character: underscore on odd half-seconds.
pub fn cursor_char(now_ms: u64) -> u32 {
    if now_ms.checked_div(500).unwrap_or(0) & 1 == 1 {
        u32::from(b'_')
    } else {
        u32::from(b' ')
    }
}

/// Wraps the bug text into lines no wider than `text_w` per `measure`, cutting at the last space.
pub fn wrap_bug_text(
    text: &[u32],
    text_w: f32,
    measure: &mut dyn FnMut(&[u32]) -> f32,
) -> Vec<Vec<u32>> {
    let mut out = Vec::new();
    let mut line: Vec<u32> = Vec::new();
    for &c in text {
        if c == u32::from(b'\n') {
            out.push(std::mem::take(&mut line));
            continue;
        }
        line.push(c);
        if measure(&line) > text_w {
            let cut = match line.iter().rposition(|&x| x == u32::from(b' ')) {
                None | Some(0) => line.len().saturating_sub(1),
                Some(p) => p + 1,
            };
            let rest = line.split_off(cut);
            out.push(std::mem::replace(&mut line, rest));
        }
    }
    out.push(line);
    out
}

/// Ctrl+Backspace: drops the last word. All-blank text is resized to one unit, which pads an empty
/// text with a NUL (kept).
pub fn ctrl_backspace(text: &mut Vec<u32>) {
    let blank = |c: &u32| *c == u32::from(b' ') || *c == u32::from(b'\n');
    let new_len = match text.iter().rposition(|c| !blank(c)) {
        None => 1,
        Some(end) => text
            .iter()
            .take(end + 1)
            .rposition(blank)
            .map_or(0, |p| p + 1),
    };
    text.resize(new_len, 0);
}

pub const SDLK_F9: i32 = 0x4000_0042;
const SDLK_ESCAPE: i32 = 27;
const SDLK_RETURN: i32 = 13;
const SDLK_KP_ENTER: i32 = 0x4000_0058;
const SDLK_BACKSPACE: i32 = 8;
const SDLK_V: i32 = 118;

pub const EV_KEYDOWN: u32 = 0x300;
pub const EV_TEXTINPUT: u32 = 0x303;
pub const EV_MOUSEMOTION: u32 = 0x400;
pub const EV_MOUSEBUTTONDOWN: u32 = 0x401;
pub const EV_MOUSEBUTTONUP: u32 = 0x402;
pub const EV_MOUSEWHEEL: u32 = 0x403;
pub const EV_CONTROLLERAXISMOTION: u32 = 0x650;
pub const EV_CONTROLLERBUTTONDOWN: u32 = 0x651;

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum KeyAction {
    Nothing,
    Close,
    NewLine,
    Save,
    CtrlBackspace,
    Backspace,
    PasteIfClipboard,
}

/// Key routing while the bug report is open.
pub fn key_action(key: i32, ctrl: bool, shift: bool, text_empty: bool) -> KeyAction {
    let enter = key == SDLK_RETURN || key == SDLK_KP_ENTER;
    if key == SDLK_ESCAPE {
        KeyAction::Close
    } else if enter && shift {
        KeyAction::NewLine
    } else if enter {
        KeyAction::Save
    } else if key == SDLK_BACKSPACE && ctrl {
        KeyAction::CtrlBackspace
    } else if key == SDLK_BACKSPACE && !text_empty {
        KeyAction::Backspace
    } else if key == SDLK_V && ctrl {
        KeyAction::PasteIfClipboard
    } else {
        KeyAction::Nothing
    }
}

/// True for events the open bug report swallows without acting on them.
pub fn swallowed_event(ty: u32) -> bool {
    matches!(
        ty,
        EV_MOUSEMOTION
            | EV_MOUSEBUTTONDOWN
            | EV_MOUSEBUTTONUP
            | EV_MOUSEWHEEL
            | EV_CONTROLLERBUTTONDOWN
            | EV_CONTROLLERAXISMOTION
    )
}

/// F9 opens the report while it is closed.
pub fn opens_report(ty: u32, key: i32, repeat: bool) -> bool {
    ty == EV_KEYDOWN && key == SDLK_F9 && !repeat
}

/// `ReadFirstLine`: first line, cut at an embedded NUL.
pub fn first_line(file: &[u8]) -> &[u8] {
    let line = file.split(|&b| b == b'\n').next().unwrap_or(&[]);
    line.split(|&b| b == 0).next().unwrap_or(&[])
}

/// `OsName`: `PRETTY_NAME` without quotes, else "Linux".
pub fn os_name(os_release: &[u8]) -> Vec<u8> {
    let mut lines: Vec<&[u8]> = os_release.split(|&b| b == b'\n').collect();
    if os_release.last() == Some(&b'\n') || os_release.is_empty() {
        lines.pop();
    }
    lines
        .into_iter()
        .find_map(|l| l.strip_prefix(b"PRETTY_NAME="))
        .map_or_else(|| b"Linux".to_vec(), |v| replace_string_to(v, b"\"", b""))
}

/// Physical memory in GB, rounded.
pub fn mem_gb(pages: i64, page_size: i64) -> i64 {
    pages.wrapping_mul(page_size).wrapping_add(1 << 29) >> 30
}

pub struct SystemInfo<'a> {
    pub version: &'a [u8],
    pub device: &'a [u8],
    pub machine: &'a [u8],
    pub cores: i32,
    pub mem_gb: i32,
    pub os: &'a [u8],
    pub kernel: &'a [u8],
}

/// First three system lines; the renderer description is appended by the host.
pub fn system_lines(i: &SystemInfo<'_>) -> [Vec<u8>; 3] {
    let cat = |parts: &[&[u8]]| parts.concat();
    [
        cat(&[b"Open HPL ", i.version]),
        cat(&[
            i.device,
            b"  |  ",
            i.machine,
            b", ",
            &int_to_string(i.cores, 0),
            b" cores, ",
            &int_to_string(i.mem_gb, 0),
            b" GB",
        ]),
        cat(&[i.os, b"  |  Linux ", i.kernel]),
    ]
}

/// First free report directory name: `base`, then `base-2`, `base-3`, ...
pub fn unique_name(base: &[u8], exists: &mut dyn FnMut(&[u8]) -> bool) -> Vec<u8> {
    let mut name = base.to_vec();
    let mut i = 2i32;
    while exists(&name) {
        name = [base, b"-", &int_to_string(i, 0)].concat();
        i = i.wrapping_add(1);
    }
    name
}

pub fn files_json(screenshot: bool, log: bool) -> Vec<u8> {
    let mut out = Vec::new();
    if screenshot {
        out.extend_from_slice(b"\"screenshot\":\"screenshot.png\"");
    }
    if log {
        if !out.is_empty() {
            out.push(b',');
        }
        out.extend_from_slice(b"\"log\":\"hpl.log\"");
    }
    out
}

/// `OPENHPL_`/`OHPL_` variable as (name, value). Without `=` both halves are the whole string.
pub fn env_pair(var: &[u8]) -> Option<(&[u8], &[u8])> {
    if !var.starts_with(b"OPENHPL_") && !var.starts_with(b"OHPL_") {
        return None;
    }
    Some(match var.iter().position(|&b| b == b'=') {
        Some(p) => (
            var.get(..p).unwrap_or_default(),
            var.get(p + 1..).unwrap_or_default(),
        ),
        None => (var, var),
    })
}

/// Splits `/proc/self/cmdline` on NUL like `getline(.., '\0')`.
pub fn cmdline_args(raw: &[u8]) -> Vec<&[u8]> {
    let mut v: Vec<&[u8]> = raw.split(|&b| b == 0).collect();
    if raw.last() == Some(&0) || raw.is_empty() {
        v.pop();
    }
    v
}

fn jstr(out: &mut Vec<u8>, s: &[u8]) {
    out.push(b'"');
    out.extend(json_escape(s));
    out.push(b'"');
}

fn fnum(x: f32) -> Vec<u8> {
    float_to_string(x, 20, false)
}

fn jvec(v: [f32; 3]) -> Vec<u8> {
    let mut out = vec![b'['];
    for (i, x) in v.into_iter().enumerate() {
        if i > 0 {
            out.push(b',');
        }
        out.extend(fnum(x));
    }
    out.push(b']');
    out
}

fn join_jstr<'a>(items: impl Iterator<Item = &'a [u8]>) -> Vec<u8> {
    let mut out = Vec::new();
    for (i, s) in items.enumerate() {
        if i > 0 {
            out.push(b',');
        }
        jstr(&mut out, s);
    }
    out
}

pub struct Scene<'a> {
    pub map: &'a [u8],
    pub pos: [f32; 3],
    pub forward: [f32; 3],
    pub world_json: &'a [u8],
    pub render_json: &'a [u8],
}

pub struct Report<'a> {
    pub id: &'a [u8],
    pub time: &'a [u8],
    pub description: &'a [u8],
    pub version: &'a [u8],
    pub exe: &'a [u8],
    pub cwd: &'a [u8],
    pub args: &'a [&'a [u8]],
    pub env: &'a [(&'a [u8], &'a [u8])],
    pub system: &'a [&'a [u8]],
    pub resolution: (i32, i32),
    pub fps: f32,
    pub frame_ms: f32,
    pub game_time: f32,
    pub scene: Option<Scene<'a>>,
    pub files: &'a [u8],
}

pub fn report_json(r: &Report<'_>) -> Vec<u8> {
    let mut o = b"{\"id\":".to_vec();
    jstr(&mut o, r.id);
    o.extend_from_slice(b",\"time\":");
    jstr(&mut o, r.time);
    o.extend_from_slice(b",\"description\":");
    jstr(&mut o, r.description);
    o.extend_from_slice(b",\"version\":");
    jstr(&mut o, r.version);
    o.extend_from_slice(b",\"exe\":");
    jstr(&mut o, r.exe);
    o.extend_from_slice(b",\"cwd\":");
    jstr(&mut o, r.cwd);
    o.extend_from_slice(b",\"args\":[");
    o.extend(join_jstr(r.args.iter().copied()));
    o.extend_from_slice(b"],\"env\":{");
    for (i, (k, v)) in r.env.iter().enumerate() {
        if i > 0 {
            o.push(b',');
        }
        jstr(&mut o, k);
        o.push(b':');
        jstr(&mut o, v);
    }
    o.extend_from_slice(b"},\"system\":[");
    o.extend(join_jstr(r.system.iter().copied()));
    o.extend_from_slice(b"],\"resolution\":[");
    o.extend(int_to_string(r.resolution.0, 0));
    o.push(b',');
    o.extend(int_to_string(r.resolution.1, 0));
    o.extend_from_slice(b"],\"fps\":");
    o.extend(fnum(r.fps));
    o.extend_from_slice(b",\"frame_ms\":");
    o.extend(fnum(r.frame_ms));
    o.extend_from_slice(b",\"game_time\":");
    o.extend(fnum(r.game_time));
    if let Some(s) = &r.scene {
        o.extend_from_slice(b",\"map\":");
        jstr(&mut o, s.map);
        o.extend_from_slice(b",\"camera\":{\"pos\":");
        o.extend(jvec(s.pos));
        o.extend_from_slice(b",\"forward\":");
        o.extend(jvec(s.forward));
        o.extend_from_slice(b"},\"world\":");
        o.extend_from_slice(s.world_json);
        o.extend_from_slice(b",\"render\":");
        o.extend_from_slice(s.render_json);
    }
    o.extend_from_slice(b",\"files\":{");
    o.extend_from_slice(r.files);
    o.extend_from_slice(b"}}");
    o
}

/// One `index.jsonl` line, newline included.
pub fn index_line(id: &[u8], map: &[u8], description: &[u8], dir: &[u8]) -> Vec<u8> {
    let mut o = b"{\"id\":".to_vec();
    jstr(&mut o, id);
    o.extend_from_slice(b",\"map\":");
    jstr(&mut o, map);
    o.extend_from_slice(b",\"description\":");
    jstr(&mut o, description);
    o.extend_from_slice(b",\"dir\":");
    jstr(&mut o, dir);
    o.extend_from_slice(b"}\n");
    o
}
