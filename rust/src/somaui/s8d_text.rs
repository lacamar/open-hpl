//! Screen text: `$Input{Action}` expansion, button glyph icons (`cLuxScreenTextIcon`), word-wrapped
//! lines and their drawing (`cLux_ParseStringIntoScreenText`, `cLux_DrawScreenText`, `cLux_DrawHint`).

use super::s8d::{Host, ImGui, Ptr};
use super::s8d_sd::{Sd, off};
use crate::somascript::s2_api::{new_owned_script_struct, new_script_struct};
use std::cell::RefCell;

const SUB_KEYBOARD: i32 = 1;
const SUB_MOUSE: i32 = 2;
const GFX: usize = off::GFX_SIZE;
const LABEL: usize = 336;
const ALIGN_LEFT: i32 = 0;
const ALIGN_RIGHT: i32 = 1;
const ALIGN_CENTER: i32 = 2;

/// One input binding of an action: its kind, key or button, and the input's own name.
pub struct SubAction {
    pub kind: i32,
    pub code: i32,
    pub name: Vec<u8>,
}

#[repr(C)]
pub struct Icon {
    header: [u8; 16],
    timer: f32,
    x: f32,
    w: f32,
    h: f32,
    col: i32,
    chars: i32,
    layers: i32,
    gfx: [[u8; GFX]; 3],
    label: [u8; LABEL],
    text: [u32; 32],
}

const _: () = assert!(std::mem::size_of::<Icon>() <= 4096);

struct KeyLayout {
    ty: i32,
    min: i32,
    max: i32,
    gfx: [u8; GFX],
    label: [u8; LABEL],
}

impl KeyLayout {
    fn new(ty: i32, min: i32, max: i32, gfx: Sd, label: Sd) -> Self {
        let mut l = Self {
            ty,
            min,
            max,
            gfx: [0; GFX],
            label: [0; LABEL],
        };
        l.set(gfx, label);
        l
    }

    fn set(&mut self, gfx: Sd, label: Sd) {
        // SAFETY: script struct blocks hold at least `GFX` and `LABEL` bytes.
        unsafe {
            std::ptr::copy_nonoverlapping(gfx.0, self.gfx.as_mut_ptr(), GFX);
            std::ptr::copy_nonoverlapping(label.0, self.label.as_mut_ptr(), LABEL);
        }
    }
}

struct Layouts {
    keys: Vec<KeyLayout>,
    default: KeyLayout,
    mouse: Vec<Vec<u8>>,
    gfx_defaults: usize,
}

thread_local! {
    static LAYOUTS: RefCell<Layouts> = const { RefCell::new(Layouts {
        keys: Vec::new(),
        default: KeyLayout { ty: -1, min: 0, max: 0, gfx: [0; GFX], label: [0; LABEL] },
        mouse: Vec::new(),
        gfx_defaults: 0,
    }) };
}

pub fn clear_keyboard_layout() {
    LAYOUTS.with(|l| l.borrow_mut().keys.clear());
}

pub fn add_keyboard_layout(min: i32, max: i32, ty: i32, gfx: Sd, label: Sd) {
    LAYOUTS.with(|l| {
        l.borrow_mut()
            .keys
            .push(KeyLayout::new(ty, min, max, gfx, label))
    });
}

pub fn set_keyboard_defaults(gfx: Sd, label: Sd) {
    LAYOUTS.with(|l| {
        let l = &mut l.borrow_mut().default;
        l.ty = 1;
        l.set(gfx, label);
    });
}

pub fn set_mouse_layout(v: Vec<Vec<u8>>) {
    LAYOUTS.with(|l| l.borrow_mut().mouse = v);
}

fn fmax(a: f32, b: f32) -> f32 {
    if a < b { b } else { a }
}

fn widen(b: &[u8]) -> Vec<u32> {
    String::from_utf8_lossy(b).chars().map(u32::from).collect()
}

fn new_icon(h: &dyn Host, sub: &SubAction, params: Sd) -> Option<*mut Icon> {
    match sub.kind {
        SUB_KEYBOARD => {
            let ic = LAYOUTS.with(|l| {
                let l = l.borrow();
                let lay = l
                    .keys
                    .iter()
                    .find(|k| k.min <= sub.code && sub.code <= k.max)
                    .unwrap_or(&l.default);
                if lay.ty < 0 {
                    return None;
                }
                let p = new_owned_script_struct(b"cLuxScreenTextIcon").cast::<Icon>();
                // SAFETY: a fresh zeroed 4096-byte block.
                let icon = unsafe { &mut *p };
                icon.gfx[0] = lay.gfx;
                icon.label = lay.label;
                Some((p, lay.ty))
            })?;
            let (p, ty) = ic;
            // SAFETY: as above.
            let icon = unsafe { &mut *p };
            if ty != 0 {
                let key = h.key_name(sub.code);
                let mut name = h.translate_button(&key);
                if name.is_empty() {
                    name = widen(&key);
                }
                for (d, s) in icon.text.iter_mut().take(31).zip(&name) {
                    *d = *s;
                }
            }
            icon.h = params.f32(off::PARAM_ICON_KEY_H);
            Some(p)
        }
        SUB_MOUSE => {
            let files: Vec<Vec<u8>> = LAYOUTS.with(|l| {
                let l = l.borrow();
                let Some(s) = usize::try_from(sub.code).ok().and_then(|i| l.mouse.get(i)) else {
                    return Vec::new();
                };
                crate::string::split(s, Some(b","), true)
                    .map(<[u8]>::to_vec)
                    .collect()
            });
            if files.is_empty() {
                return None;
            }
            let p = new_owned_script_struct(b"cLuxScreenTextIcon").cast::<Icon>();
            let def = LAYOUTS.with(|l| {
                let mut l = l.borrow_mut();
                if l.gfx_defaults == 0 {
                    l.gfx_defaults = new_script_struct(b"cImGuiGfx") as usize;
                }
                l.gfx_defaults
            });
            // SAFETY: a fresh zeroed 4096-byte block.
            let icon = unsafe { &mut *p };
            icon.layers = i32::try_from(files.len().min(3)).unwrap_or(3) - 1;
            for (slot, file) in icon.gfx.iter_mut().zip(&files) {
                // SAFETY: the defaults block holds `GFX` bytes.
                unsafe { std::ptr::copy_nonoverlapping(def as *const u8, slot.as_mut_ptr(), GFX) };
                let g = Sd(slot.as_ptr());
                g.set_ptr(off::GFX_FILE, h.intern(file));
                g.set_i32(off::GFX_MATERIAL, 1);
                g.set_i32(off::GFX_TYPE, 0);
            }
            icon.h = params.f32(off::PARAM_ICON_MOUSE_H);
            Some(p)
        }
        _ => None,
    }
}

/// `$Input{Action}` becomes one '.' per glyph icon when `icons` is set, else translated button names.
pub fn parse_string(
    h: &dyn Host,
    text: &[u32],
    mut icons: Option<&mut Vec<*mut Icon>>,
    params: Sd,
) -> Vec<u32> {
    let pat: Vec<u32> = "$Input{".chars().map(u32::from).collect();
    let mut out: Vec<u32> = Vec::new();
    let mut pos = 0usize;
    loop {
        let start = text
            .get(pos..)
            .and_then(|t| t.windows(pat.len()).position(|w| w == pat))
            .map(|i| i + pos);
        let end = start.and_then(|s| {
            text.get(s..)?
                .iter()
                .position(|&c| c == u32::from(b'}'))
                .map(|i| i + s)
        });
        let (Some(start), Some(end)) = (start, end) else {
            break;
        };
        out.extend_from_slice(text.get(pos..start).unwrap_or_default());
        pos = end + 1;
        let name: String = text
            .get(start + 7..end)
            .unwrap_or_default()
            .iter()
            .map(|&c| char::from_u32(c).unwrap_or('?'))
            .collect();
        let Some(subs) = h.action_subs(name.as_bytes()).filter(|s| !s.is_empty()) else {
            out.extend("BADACTION".chars().map(u32::from));
            continue;
        };
        for (i, sub) in subs.iter().enumerate() {
            if i > 0 {
                out.extend(" / ".chars().map(u32::from));
            }
            if let Some(list) = icons.as_deref_mut()
                && let Some(p) = new_icon(h, sub, params)
            {
                // SAFETY: fresh icon block.
                unsafe { (*p).col = i32::try_from(out.len()).unwrap_or(0) };
                out.push(u32::from(b'.'));
                list.push(p);
                continue;
            }
            let tr = h.translate_button(&sub.name);
            out.extend(if tr.is_empty() { widen(&sub.name) } else { tr });
        }
    }
    out.extend_from_slice(text.get(pos..).unwrap_or_default());
    out
}

fn units(h: &dyn Host, a: Ptr, i: usize) -> Vec<u32> {
    h.wstr(h.array_at(a, i))
}

#[allow(clippy::too_many_arguments)]
pub fn parse_screen_text(
    h: &dyn Host,
    im: &mut ImGui,
    input: &[u32],
    params: Sd,
    lines: Ptr,
    icons_arr: Ptr,
    per_line: Ptr,
    line_height: &mut f32,
    time: &mut f32,
) {
    let font = params.at(24 + off::W_FONT);
    let mut icons: Vec<*mut Icon> = Vec::new();
    let mut text = parse_string(h, input, Some(&mut icons), params);
    let size = im.font_size(font, 1.0);
    let dot = fmax(im.font_length(h, font, 1.0, &[u32::from(b'.')]), 1e-4);
    let space = fmax(im.font_length(h, font, 1.0, &[u32::from(b' ')]), 1e-4);
    *line_height = size[1];
    let mut inserted = 0i32;
    for &p in &icons {
        // SAFETY: icons come from `new_icon` and stay alive in the script array.
        let icon = unsafe { &mut *p };
        icon.col += inserted;
        let mut g = im.gfx_size(h, Sd(icon.gfx[0].as_ptr()));
        if g[0] <= 0.0 {
            g[0] = size[0];
        }
        if g[1] <= 0.0 {
            g[1] = size[1];
        }
        if icon.h < 0.0 || icon.h > g[1] {
            icon.h = g[1];
        }
        icon.w = g[0] / g[1] * icon.h;
        if icon.text[0] != 0 {
            let t = icon_text(icon);
            let label = Sd(icon.label.as_ptr()).at(off::W_FONT);
            icon.w = fmax(icon.w, im.font_length(h, label, 1.0, &t) + 4.0 * space);
        }
        icon.chars = (icon.w / dot).ceil().max(1.0) as i32;
        let at = usize::try_from(icon.col).unwrap_or(0).min(text.len());
        let n = usize::try_from(icon.chars - 1).unwrap_or(0);
        text.splice(at..at, std::iter::repeat_n(u32::from(b'.'), n));
        inserted += icon.chars - 1;
        *line_height = fmax(*line_height, icon.h);
    }
    *time = h.string_duration(&text);
    let width = params.f32(off::PARAM_LINE_WIDTH);
    let fd = im.font(h, font);
    let mut rows = if width > 0.0 && fd != 0 {
        h.font_wrap(fd, width, size[1], size, &text)
    } else {
        vec![text.clone()]
    };
    h.array_resize(per_line, rows.len());
    let (mut start, mut k) = (0usize, 0usize);
    for (r, row) in rows.iter_mut().enumerate() {
        let end = start + row.len();
        let mut shift = 0i32;
        while let Some(&ip) = icons.get(k) {
            // SAFETY: as above.
            let icon = unsafe { &mut *ip };
            if icon.col >= i32::try_from(end).unwrap_or(i32::MAX) {
                break;
            }
            let col = icon.col - i32::try_from(start).unwrap_or(0) + shift;
            let spaces = (icon.chars as f32 * dot / space).ceil() as i32;
            let c = usize::try_from(col).unwrap_or(0).min(row.len());
            let e = (c + usize::try_from(icon.chars).unwrap_or(0)).min(row.len());
            row.splice(
                c..e,
                std::iter::repeat_n(u32::from(b' '), usize::try_from(spaces).unwrap_or(0)),
            );
            shift += spaces - icon.chars;
            let head = row.get(..c).unwrap_or_default().to_vec();
            icon.x =
                im.font_length(h, font, 1.0, &head) + (spaces as f32).mul_add(space, -icon.w) * 0.5;
            h.array_push_i32(h.array_at(per_line, r), i32::try_from(k).unwrap_or(0));
            k += 1;
        }
        start = if text
            .get(end)
            .is_some_and(|&c| c == u32::from(b' ') || c == u32::from(b'\n'))
        {
            end + 1
        } else {
            end
        };
        h.array_push_wstring(lines, row);
    }
    h.array_resize(icons_arr, icons.len());
    for (i, &p) in icons.iter().enumerate() {
        // SAFETY: the array holds `cLuxScreenTextIcon@` slots.
        unsafe { (h.array_at(icons_arr, i) as *mut *mut Icon).write(p) };
    }
}

fn icon_text(icon: &Icon) -> Vec<u32> {
    icon.text.iter().copied().take_while(|&c| c != 0).collect()
}

fn draw_icon(h: &dyn Host, im: &mut ImGui, icon: &mut Icon, dt: f32, base: [f32; 3]) {
    icon.timer += dt;
    let pos = [base[0] + icon.x, base[1] - icon.h * 0.5, base[2]];
    let size = [icon.w, icon.h];
    im.do_image(h, Sd(icon.gfx[0].as_ptr()), pos, size);
    let layers = icon.layers;
    let t = if layers > 0 {
        icon.timer % layers as f32
    } else {
        0.0
    };
    for (i, slot) in icon
        .gfx
        .iter()
        .skip(1)
        .take(usize::try_from(layers).unwrap_or(0))
        .enumerate()
    {
        let fi = i as f32;
        let a = if t > fi && t < fi + 1.0 {
            ((t - fi) * std::f32::consts::PI).sin()
        } else {
            0.0
        };
        let g = Sd(slot.as_ptr());
        g.set_col(off::GFX_COLOR, [1.0, 1.0, 1.0, a]);
        im.do_image(h, g, [pos[0], pos[1], pos[2] + 0.01], size);
    }
    if icon.text[0] != 0 {
        let t = icon_text(icon);
        im.do_label(h, &t, Sd(icon.label.as_ptr()), pos, size, 1.0);
    }
}

/// HPL3 DrawHint: first line centred, the rest left-aligned under its start.
#[allow(clippy::too_many_arguments)]
pub fn draw_screen_text(
    h: &dyn Host,
    im: &mut ImGui,
    dt: f32,
    label: Sd,
    pos: [f32; 3],
    line_width: f32,
    spacing: f32,
    lines: Ptr,
    icons: Ptr,
    per_line: Ptr,
    hint: bool,
) {
    let mut buf = [0u8; LABEL];
    // SAFETY: a `cImGuiLabelData` block holds at least `LABEL` bytes.
    unsafe { std::ptr::copy_nonoverlapping(label.0, buf.as_mut_ptr(), LABEL) };
    let l = Sd(buf.as_ptr());
    let font = l.at(off::W_FONT);
    let font_h = im.font_size(font, 1.0)[1];
    let align = l.i32(off::W_FONT_ALIGN);
    let n = h.array_size(lines);
    let hint_x = if n > 0 {
        let l0 = units(h, lines, 0);
        pos[0] + (line_width - im.font_length(h, font, 1.0, &l0)) * 0.5
    } else {
        0.0
    };
    for i in 0..n {
        let line = units(h, lines, i);
        let p = [
            if hint && i > 0 { hint_x } else { pos[0] },
            (i as f32).mul_add(spacing, pos[1]),
            pos[2],
        ];
        if hint {
            l.set_i32(
                off::W_FONT_ALIGN,
                if i == 0 { ALIGN_CENTER } else { ALIGN_LEFT },
            );
        }
        im.do_label(h, &line, l, p, [line_width, font_h], 1.0);
        if i >= h.array_size(per_line) {
            continue;
        }
        let len = im.font_length(h, font, 1.0, &line);
        let x = if hint {
            hint_x
        } else if align == ALIGN_RIGHT {
            pos[0] + line_width - len
        } else if align == ALIGN_CENTER {
            pos[0] + (line_width - len) * 0.5
        } else {
            pos[0]
        };
        let inner = h.array_at(per_line, i);
        for j in 0..h.array_size(inner) {
            // SAFETY: `array<int>` elements.
            let idx = usize::try_from(unsafe { (h.array_at(inner, j) as *const i32).read() })
                .unwrap_or(usize::MAX);
            if idx < h.array_size(icons) {
                // SAFETY: `array<cLuxScreenTextIcon@>` slot holding a live icon.
                let icon = unsafe { &mut **(h.array_at(icons, idx) as *mut *mut Icon) };
                draw_icon(h, im, icon, dt, [x, p[1] + font_h * 0.5, pos[2]]);
            }
        }
    }
}
