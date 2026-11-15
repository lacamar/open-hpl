//! `cLuxHintHandler`: the object behind the C++ shell. Owns the hint state, the given and blocked
//! name sets (the save layer reads them), the current text and the gamepad icons.

use super::event::Out;
use super::hint::{self, HintState};
use super::host::{ALIGN_CENTER, ALIGN_LEFT, GFX_IMAGE_SIZE, MAT_ALPHA, MAT_FONT_NORMAL, Slot, Ui};
use crate::math::log2_to_int;
use crate::string::split;
use std::collections::BTreeSet;

const BUTTONS: usize = 30;
const HATS: usize = 4;
const AXES: usize = 20;
const BUTTON_LAST: i32 = 30;
const HAT_LAST: i32 = 10;
const AXIS_LAST: i32 = 10;
const ROW_LIMIT: usize = 1022;

fn ratio(a: f32, b: f32) -> f32 {
    a / b
}

fn spaces(n: usize) -> Vec<u32> {
    vec![u32::from(b' '); n]
}

#[derive(Clone, Copy, Default)]
pub struct Icon {
    gfx: usize,
    pos: [f32; 3],
    size: [f32; 2],
    char_pos: i32,
}

#[derive(Clone, Copy)]
struct IconTables {
    button: [usize; BUTTONS],
    hat: [usize; HATS],
    axis: [usize; AXES],
}

pub struct HintHandler {
    pub s: HintState,
    pub font: usize,
    text: Vec<u32>,
    given: BTreeSet<Vec<u8>>,
    blocked: BTreeSet<Vec<u8>>,
    icons: Vec<Icon>,
    tables: IconTables,
}

impl HintHandler {
    pub fn new(ui: &dyn Ui) -> Self {
        let mut s = HintState::default();
        let size = ui.menu_vec2(b"Hints", b"FontSize", 0.0);
        let y = ui.menu_f32(b"Hints", b"YPos", 0.0);
        let mul = ui.menu_f32(b"Hints", b"TextDurationMul", 0.0);
        hint::init(&mut s, y, size, mul);
        let tex = |f: &str| ui.gfx_texture(f.as_bytes());
        let mut t = IconTables {
            button: [0; BUTTONS],
            hat: [0; HATS],
            axis: [0; AXES],
        };
        for (i, f) in [
            (0, "a"),
            (1, "b"),
            (2, "x"),
            (3, "y"),
            (9, "lb"),
            (10, "rb"),
            (4, "select"),
            (6, "start"),
            (7, "ls"),
            (8, "rs"),
            (11, "dpad_u"),
            (14, "dpad_r"),
            (13, "dpad_l"),
            (12, "dpad_d"),
        ] {
            if let Some(slot) = t.button.get_mut(i) {
                *slot = tex(&format!("gamepad_{f}.tga"));
            }
        }
        for (i, f) in [(0, "u"), (1, "r"), (3, "l"), (2, "d")] {
            if let Some(slot) = t.hat.get_mut(i) {
                *slot = tex(&format!("gamepad_dpad_{f}.tga"));
            }
        }
        t.axis[8] = tex("gamepad_lt.tga");
        t.axis[10] = tex("gamepad_rt.tga");
        Self {
            s,
            font: 0,
            text: Vec::new(),
            given: BTreeSet::new(),
            blocked: BTreeSet::new(),
            icons: Vec::new(),
            tables: t,
        }
    }

    pub fn reset(&mut self) {
        hint::reset(&mut self.s);
        self.text.clear();
        self.given.clear();
        self.blocked.clear();
    }

    pub fn set_active(&mut self, on: bool) {
        if hint::set_active(&mut self.s, on) {
            self.text.clear();
        }
    }

    pub fn names(&self, blocked: bool) -> Vec<Vec<u8>> {
        let set = if blocked { &self.blocked } else { &self.given };
        set.iter().cloned().collect()
    }

    pub fn clear_names(&mut self, blocked: bool) {
        if blocked {
            &mut self.blocked
        } else {
            &mut self.given
        }
        .clear();
    }

    pub fn add_name(&mut self, blocked: bool, name: &[u8]) {
        if blocked {
            &mut self.blocked
        } else {
            &mut self.given
        }
        .insert(name.to_vec());
    }

    pub fn remove(&mut self, name: &[u8]) -> bool {
        self.given.remove(name)
    }

    pub fn block(&mut self, name: &[u8]) {
        self.blocked.insert(name.to_vec());
    }

    pub fn unblock(&mut self, name: &[u8]) {
        self.blocked.remove(name);
    }
}

struct Snap {
    font: usize,
    size: [f32; 2],
    y_pos: f32,
    tables: IconTables,
}

fn icon_at(ui: &dyn Ui, t: &IconTables, cmd: &[u32], pos: i32, out: &mut Vec<Icon>) -> Vec<u32> {
    let narrow: Vec<u8> = cmd.iter().map(|&c| c as u8).collect();
    let parts: Vec<&[u8]> = split(&narrow, Some(b"."), true).collect();
    let part = |i: usize| parts.get(i).copied().unwrap_or_default();
    let mut icon = |gfx: usize| {
        out.push(Icon {
            gfx,
            char_pos: pos,
            ..Icon::default()
        })
    };
    let name = |a: &[u8], b: &[u8]| {
        let mut k = a.to_vec();
        k.push(b'.');
        k.extend_from_slice(b);
        ui.translate(b"ButtonNames", &k)
    };
    if part(0) == b"GamepadButton" {
        let b = crate::input::pad_button_from_name(part(1));
        if b >= BUTTON_LAST {
            return spaces(1);
        }
        return match usize::try_from(b)
            .ok()
            .and_then(|i| t.button.get(i))
            .copied()
        {
            Some(g) if g != 0 => {
                icon(g);
                spaces(if ui.chinese() { 16 } else { 4 })
            }
            _ => ui.translate(b"ButtonNames", part(1)),
        };
    }
    let hat = crate::input::pad_hat_from_name(part(1));
    let axis = crate::input::pad_axis_from_name(part(1));
    if hat != HAT_LAST {
        let state = crate::input::pad_hat_state_from_name(part(2));
        let g = usize::try_from(log2_to_int(state))
            .ok()
            .and_then(|i| t.hat.get(i))
            .copied()
            .unwrap_or(0);
        if g != 0 {
            icon(g);
            return spaces(4);
        }
        return name(part(1), part(2));
    }
    if axis != AXIS_LAST {
        let range = crate::input::pad_axis_range_from_name(part(2));
        let g = usize::try_from(axis * 2 + range - 1)
            .ok()
            .and_then(|i| t.axis.get(i))
            .copied()
            .unwrap_or(0);
        if g != 0 {
            icon(g);
            return spaces(6);
        }
        return name(part(1), part(2));
    }
    spaces(1)
}

/// Wrapped rows and glyph walk that place each icon at its character position.
fn place_icons(ui: &dyn Ui, sn: &Snap, text: &[u32], icons: &mut [Icon]) {
    let rows = ui.wrap_rows(
        sn.font,
        hint::WRAP_WIDTH,
        hint::WRAP_LINE_HEIGHT,
        sn.size,
        text,
    );
    let start_y = sn.y_pos + sn.size[1] + 3.0;
    let (first, last) = ui.font_range(sn.font);
    let (mut idx, mut position) = (0usize, 0i32);
    for (i, row) in rows.iter().enumerate() {
        let mut line = vec![u32::from(b' ')];
        line.extend_from_slice(row);
        line.truncate(ROW_LIMIT);
        let mut pos = if rows.len() == 1 {
            [
                400.0 - ui.font_len(sn.font, sn.size, &line) / 2.0,
                start_y,
                20.0,
            ]
        } else {
            [
                400.0 - hint::WRAP_WIDTH * 0.5,
                (i as f32).mul_add(sn.size[1] + 2.0, start_y),
                20.0,
            ]
        };
        for &c in &line {
            if c < first || c > last {
                continue;
            }
            if let Some((_, adv)) = ui.font_glyph(sn.font, c - first) {
                pos[0] = adv.mul_add(sn.size[0], pos[0]);
            }
            let Some(icon) = icons.get_mut(idx) else {
                return;
            };
            if position == icon.char_pos {
                icon.pos = pos;
                let img = ui.gfx_size(icon.gfx, GFX_IMAGE_SIZE);
                icon.size = [
                    ratio(img[0], img[1]) * sn.size[0],
                    ratio(img[1], img[1]) * sn.size[1],
                ];
                idx += 1;
                if idx == icons.len() {
                    return;
                }
            }
            position += 1;
        }
    }
}

pub struct Hint<'a> {
    pub ui: &'a dyn Ui,
    pub c: Slot<'a, HintHandler>,
}

impl Hint<'_> {
    pub fn update(&self, dt: f32) {
        self.c.with(|h| hint::update(&mut h.s, dt));
    }

    pub fn add(&self, name: &[u8], message: &[u32], time: f32) -> bool {
        let Some(true) = self.c.with(|h| {
            let ok = hint::can_add(&h.s, h.blocked.contains(name), h.given.contains(name));
            if ok {
                h.given.insert(name.to_vec());
            }
            ok
        }) else {
            return false;
        };
        let text = self.ui.parse_string(message);
        let duration = if time <= 0.0 {
            self.ui.string_duration(&text)
        } else {
            0.0
        };
        let Some(sn) = self.c.with(|h| {
            h.text.clone_from(&text);
            hint::start(&mut h.s, time, duration);
            Snap {
                font: h.font,
                size: h.s.font_size,
                y_pos: h.s.y_pos,
                tables: h.tables,
            }
        }) else {
            return true;
        };
        let mut icons = Vec::new();
        let stripped = hint::strip_icons(&text, &mut |cmd, pos| {
            icon_at(self.ui, &sn.tables, cmd, pos, &mut icons)
        });
        if !icons.is_empty() {
            place_icons(self.ui, &sn, &stripped, &mut icons);
        }
        self.c.with(|h| {
            h.icons = icons;
            h.text = stripped;
        });
        true
    }

    pub fn draw(&self, set: usize) {
        let Some((s, font, text, icons)) = self
            .c
            .with(|h| (h.s, h.font, h.text.clone(), h.icons.clone()))
        else {
            return;
        };
        if !hint::drawable(&s, self.ui.ptest()) {
            return;
        }
        let mut o = Out::default();
        hint::draw_label(&s, &mut o);
        if let Some(e) = o.evs.first() {
            self.ui.draw_font(
                set,
                font,
                [e.v[0], e.v[1], e.v[2]],
                s.font_size,
                [e.v[3], e.v[4], e.v[5], e.v[6]],
                ALIGN_CENTER,
                MAT_FONT_NORMAL,
                &self.ui.translate(b"Hints", b"HINT:"),
                true,
            );
        }
        let rows = self.ui.wrap_rows(
            font,
            hint::WRAP_WIDTH,
            hint::WRAP_LINE_HEIGHT,
            s.font_size,
            &text,
        );
        for (i, row) in rows.iter().enumerate() {
            let p = hint::row_pos(&s, rows.len(), i);
            self.ui.draw_font(
                set,
                font,
                [p[0], p[1], p[2]],
                s.font_size,
                [1.0, 1.0, 1.0, s.alpha],
                if p[3] != 0.0 {
                    ALIGN_CENTER
                } else {
                    ALIGN_LEFT
                },
                MAT_FONT_NORMAL,
                if rows.len() == 1 { &text } else { row },
                true,
            );
        }
        for i in &icons {
            self.ui.draw_gfx(
                set,
                i.gfx,
                i.pos,
                Some(i.size),
                [1.0, 1.0, 1.0, s.alpha],
                MAT_ALPHA,
            );
        }
    }
}
