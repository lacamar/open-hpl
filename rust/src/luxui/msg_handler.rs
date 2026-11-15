//! `cLuxMessageHandler`: the object behind the C++ shell. Owns the state, the wrapped rows and the
//! pending yes/no callback; engine calls go through [`Ui`] after the state change is done.

use super::event::{Ev, Out};
use super::host::{ALIGN_CENTER, ALIGN_LEFT, MAT_ADDITIVE, MAT_DEFAULT, MAT_FONT_NORMAL, Slot, Ui};
use super::message::{self as msg, MessageState};

fn w(s: &str) -> Vec<u32> {
    s.chars().map(u32::from).collect()
}

pub struct MessageHandler {
    pub s: MessageState,
    pub font: usize,
    black: usize,
    quest_icon: usize,
    quest_sound: Vec<u8>,
    rows: Vec<Vec<u32>>,
    lines: Vec<Vec<u32>>,
    pub callback: usize,
}

impl MessageHandler {
    pub fn new(ui: &dyn Ui) -> Self {
        let black = ui.gfx_rect([0.0, 0.0, 0.0, 1.0]);
        let quest_sound = ui.menu_str(b"Messages", b"QuestAddedSound", b"");
        let icon = ui.menu_str(b"Messages", b"QuestAddedIcon", b"");
        let quest_icon = if icon.is_empty() {
            0
        } else {
            ui.gfx_image(&icon)
        };
        let font_size = ui.menu_vec2(b"Messages", b"GameMessageFontSize", 0.0);
        Self {
            s: MessageState {
                font_size,
                ..MessageState::default()
            },
            font: 0,
            black,
            quest_icon,
            quest_sound,
            rows: Vec::new(),
            lines: Vec::new(),
            callback: 0,
        }
    }

    pub fn reset(&mut self) {
        msg::reset(&mut self.s);
        self.lines.clear();
        self.callback = 0;
    }

    pub fn load_user_config(&mut self, ui: &dyn Ui) {
        self.s.show_subtitles = u8::from(ui.user_bool(b"Game", b"ShowSubtitles", true));
        self.s.show_effect_subtitles =
            u8::from(ui.user_bool(b"Game", b"ShowEffectSubtitles", false));
    }

    pub fn save_user_config(&self, ui: &dyn Ui) {
        ui.set_user_bool(b"Game", b"ShowSubtitles", self.s.show_subtitles != 0);
        ui.set_user_bool(
            b"Game",
            b"ShowEffectSubtitles",
            self.s.show_effect_subtitles != 0,
        );
    }

    pub fn start_quest_added(&mut self) {
        msg::start_quest_added(&mut self.s);
    }

    pub fn is_message_active(&self) -> bool {
        msg::is_message_active(&self.s)
    }
}

fn wrap_width(pause: bool) -> f32 {
    if pause {
        msg::PAUSE_WRAP_WIDTH
    } else {
        msg::MESSAGE_WRAP_WIDTH
    }
}

/// The handler operations that touch the engine. Each reads or changes the state through the
/// slot, drops the borrow, then calls into the engine.
pub struct Msg<'a> {
    pub ui: &'a dyn Ui,
    pub c: Slot<'a, MessageHandler>,
}

impl Msg<'_> {
    fn read<R>(&self, f: impl FnOnce(&mut MessageHandler) -> R) -> Option<R> {
        self.c.with(f)
    }

    fn run(&self, evs: &[Ev]) {
        for e in evs {
            match e.kind {
                msg::EV_QUEST_SOUND => {
                    if let Some(s) = self.read(|h| h.quest_sound.clone()) {
                        self.ui.gui_sound(&s);
                    }
                }
                msg::EV_SET_UPDATE_ACTIVE => self.ui.set_update_active(e.a != 0),
                msg::EV_CALLBACK => {
                    let cb = self.read(|h| h.callback).unwrap_or(0);
                    self.ui.callback(cb, e.a != 0);
                }
                _ => {}
            }
        }
    }

    pub fn start_pause(&self, text: &[u32], yes_no: bool, cb: usize) {
        let Some((font, size)) = self.read(|h| (h.font, h.s.font_size)) else {
            return;
        };
        let parsed = self.ui.parse_string(text);
        let rows = self
            .ui
            .wrap_rows(font, wrap_width(true), size[1] + 2.0, size, &parsed);
        let mut o = Out::default();
        self.read(|h| {
            h.lines.clone_from(&rows);
            h.callback = cb;
            msg::start_pause_message(&mut h.s, yes_no, &mut o);
        });
        self.run(&o.evs);
    }

    pub fn set_message(&self, text: &[u32], time: f32) {
        let parsed = self.ui.parse_string(text);
        let duration = if time <= 0.0 {
            self.ui.string_duration(&parsed)
        } else {
            0.0
        };
        let Some((font, size)) = self.read(|h| {
            msg::set_message(&mut h.s, time, duration);
            h.rows.clear();
            (h.font, h.s.font_size)
        }) else {
            return;
        };
        let rows = self
            .ui
            .wrap_rows(font, wrap_width(false), size[1] + 2.0, size, &parsed);
        self.read(|h| h.rows.clone_from(&rows));
    }

    pub fn update(&self, dt: f32) {
        let mut o = Out::default();
        self.read(|h| msg::update(&mut h.s, dt, &mut o));
        self.run(&o.evs);
    }

    pub fn do_action(&self, action: i32, pressed: bool) {
        let mut o = Out::default();
        self.read(|h| msg::do_action(&mut h.s, action, pressed, h.callback != 0, &mut o));
        self.run(&o.evs);
    }

    pub fn set_pause_active(&self, on: bool) {
        let mut o = Out::default();
        self.read(|h| msg::set_pause_active(&mut h.s, on, &mut o));
        self.run(&o.evs);
    }

    pub fn draw(&self) {
        self.draw_message();
        self.draw_pause();
        self.draw_quest();
    }

    fn draw_quest(&self) {
        let hud = self.ui.hud();
        let Some((icon, snapshot)) = self.read(|h| (h.quest_icon, h.s)) else {
            return;
        };
        let size = (icon != 0).then(|| self.ui.gfx_size(icon, super::host::GFX_ACTIVE_SIZE));
        let mut o = Out::default();
        msg::draw_quest(&snapshot, size, hud.center, hud.start, &mut o);
        for e in &o.evs {
            let pos = [e.v[0], e.v[1], e.v[2]];
            if e.kind == msg::EV_QUEST_ICON {
                self.ui
                    .draw_gfx(0, icon, pos, None, [1.0, 1.0, 1.0, e.v[8]], MAT_DEFAULT);
            } else {
                self.ui.draw_gfx(
                    0,
                    icon,
                    pos,
                    None,
                    [e.v[5], e.v[5], e.v[5], e.v[8]],
                    MAT_ADDITIVE,
                );
            }
        }
    }

    fn draw_message(&self) {
        let Some((s, font, rows)) = self.read(|h| (h.s, h.font, h.rows.clone())) else {
            return;
        };
        let mut o = Out::default();
        msg::draw_message(&s, rows.len(), &mut o);
        for e in &o.evs {
            let Some(row) = usize::try_from(e.a).ok().and_then(|i| rows.get(i)) else {
                continue;
            };
            self.ui.draw_font(
                0,
                font,
                [e.v[0], e.v[1], e.v[2]],
                s.font_size,
                [1.0, 1.0, 1.0, e.v[8]],
                ALIGN_CENTER,
                MAT_FONT_NORMAL,
                row,
                false,
            );
        }
    }

    fn draw_pause(&self) {
        let Some((s, font, lines, black)) = self.read(|h| (h.s, h.font, h.lines.clone(), h.black))
        else {
            return;
        };
        let mut o = Out::default();
        msg::draw_pause(&s, lines.len(), &mut o);
        for e in &o.evs {
            let pos = [e.v[0], e.v[1], e.v[2]];
            let col = [1.0, 1.0, 1.0, e.v[8]];
            let text = |size: [f32; 2], t: &[u32]| {
                self.ui.draw_font(
                    0,
                    font,
                    pos,
                    size,
                    col,
                    ALIGN_LEFT,
                    MAT_FONT_NORMAL,
                    t,
                    false,
                );
            };
            match e.kind {
                msg::EV_PAUSE_BG => {
                    self.ui
                        .draw_gfx(0, black, pos, Some([e.v[3], e.v[4]]), col, MAT_DEFAULT)
                }
                msg::EV_PAUSE_ROW => {
                    if let Some(row) = usize::try_from(e.a).ok().and_then(|i| lines.get(i)) {
                        text(s.font_size, row);
                    }
                }
                msg::EV_YES => text(s.font_size, &w("Yes")),
                msg::EV_NO => text(s.font_size, &w("No")),
                msg::EV_YES_KEY | msg::EV_NO_KEY => {
                    let action: &[u8] = if e.kind == msg::EV_YES_KEY {
                        b"Interact"
                    } else {
                        b"Attack"
                    };
                    let mut t = w("(");
                    t.extend(self.ui.input_name(action));
                    t.push(u32::from(b')'));
                    text([e.v[3], e.v[4]], &t);
                }
                _ => {}
            }
        }
    }
}
