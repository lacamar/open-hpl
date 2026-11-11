//! `cLuxMessageHandler`: game message, pause message and quest-added icon timing, and the draw
//! layout. Fonts, wrapped rows, draws, sound and the yes/no callback stay C++.

use super::event::{Ev, Out};
use super::hint::Oscillation;

pub const EV_QUEST_SOUND: u32 = 1;
pub const EV_SET_UPDATE_ACTIVE: u32 = 2;
pub const EV_CALLBACK: u32 = 3;
pub const EV_MSG_ROW: u32 = 4;
pub const EV_PAUSE_BG: u32 = 5;
pub const EV_PAUSE_ROW: u32 = 6;
pub const EV_YES: u32 = 7;
pub const EV_YES_KEY: u32 = 8;
pub const EV_NO: u32 = 9;
pub const EV_NO_KEY: u32 = 10;
pub const EV_QUEST_ICON: u32 = 11;
pub const EV_QUEST_GLOW: u32 = 12;

pub const MESSAGE_WRAP_WIDTH: f32 = 700.0;
pub const PAUSE_WRAP_WIDTH: f32 = 500.0;

pub const ACTION_INTERACT: i32 = 0;
pub const ACTION_ATTACK: i32 = 1;

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct MessageState {
    pub show_subtitles: u8,
    pub show_effect_subtitles: u8,
    pub pause_active: u8,
    pub quest_active: u8,
    pub yes_no: u8,
    pub pause_alpha: f32,
    pub message_alpha: f32,
    pub message_time: f32,
    pub quest_alpha: f32,
    pub quest_time: f32,
    pub font_size: [f32; 2],
    pub quest_oscill: Oscillation,
}

pub fn reset(s: &mut MessageState) {
    s.pause_active = 0;
    s.pause_alpha = 0.0;
    s.quest_active = 0;
    s.quest_alpha = 0.0;
    s.quest_time = 0.0;
    s.quest_oscill.set_up(0.0, 1.0, 0.0, 1.0, 1.0);
    s.yes_no = 0;
    s.message_alpha = 0.0;
    s.message_time = 0.0;
}

pub fn start_quest_added(s: &mut MessageState) {
    if s.quest_active != 0 {
        return;
    }
    s.quest_active = 1;
    s.quest_alpha = 0.0;
    s.quest_time = 0.0;
}

pub fn set_message(s: &mut MessageState, time: f32, parsed_duration: f32) {
    s.message_alpha = 0.0;
    s.message_time = if time <= 0.0 { parsed_duration } else { time };
}

pub fn is_message_active(s: &MessageState) -> bool {
    s.message_time > 0.0
}

pub fn start_pause_message(s: &mut MessageState, yes_no: bool, o: &mut Out) {
    s.yes_no = u8::from(yes_no);
    set_pause_active(s, true, o);
}

pub fn set_pause_active(s: &mut MessageState, on: bool, o: &mut Out) {
    if (s.pause_active != 0) == on {
        return;
    }
    s.pause_active = u8::from(on);
    o.push(Ev::ints(EV_SET_UPDATE_ACTIVE, i32::from(!on), 0));
}

pub fn do_action(
    s: &mut MessageState,
    action: i32,
    pressed: bool,
    has_callback: bool,
    o: &mut Out,
) {
    if !pressed {
        return;
    }
    set_pause_active(s, false, o);
    if s.yes_no != 0 && has_callback {
        if action == ACTION_INTERACT {
            o.push(Ev::ints(EV_CALLBACK, 1, 0));
        } else if action == ACTION_ATTACK {
            o.push(Ev::ints(EV_CALLBACK, 0, 0));
        }
    }
}

pub fn update(s: &mut MessageState, dt: f32, o: &mut Out) {
    if s.message_time > 0.0 {
        s.message_time -= dt;
        s.message_alpha += dt * 1.0;
        if s.message_alpha > 1.0 {
            s.message_alpha = 1.0;
        }
    } else if s.message_alpha > 0.0 {
        s.message_alpha = (-dt).mul_add(1.5, s.message_alpha);
        if s.message_alpha < 0.0 {
            s.message_alpha = 0.0;
        }
    }

    if s.quest_active != 0 {
        if f64::from(s.quest_alpha) < 0.1 {
            s.quest_alpha += dt;
            if f64::from(s.quest_alpha) >= 0.1 {
                o.push(Ev::new(EV_QUEST_SOUND));
            }
        } else if s.quest_alpha < 1.0 {
            s.quest_alpha += dt;
            if s.quest_alpha > 1.0 {
                s.quest_alpha = 1.0;
            }
        } else {
            s.quest_time += dt;
            if s.quest_time > 3.0 {
                s.quest_active = 0;
            }
        }
    } else if s.quest_alpha > 0.0 {
        s.quest_alpha -= dt;
        if s.quest_alpha < 0.0 {
            s.quest_alpha = 0.0;
        }
    }

    if s.quest_alpha > 0.0 {
        s.quest_oscill.update(dt);
    }

    if s.pause_active != 0 {
        s.pause_alpha += dt * 2.0;
        if s.pause_alpha > 1.0 {
            s.pause_alpha = 1.0;
        }
    } else {
        s.pause_alpha -= dt * 2.0;
        if s.pause_alpha < 0.0 {
            s.pause_alpha = 0.0;
        }
    }
}

/// Row positions of the game message; `rows` is the wrapped row count.
pub fn draw_message(s: &MessageState, rows: usize, o: &mut Out) {
    if s.message_alpha <= 0.0 {
        return;
    }
    let mut alpha = s.message_alpha;
    if s.pause_alpha > 0.0 {
        alpha = s.message_alpha * (1.0 - s.pause_alpha);
    }
    let mut y = 345.0f32;
    for i in 0..rows {
        o.push(Ev {
            a: i as i32,
            ..Ev::floats(EV_MSG_ROW, &[400.0, y, 4.0, 0.0, 0.0, 1.0, 1.0, 1.0, alpha])
        });
        y += s.font_size[1] + 2.0;
    }
}

pub fn draw_pause(s: &MessageState, rows: usize, o: &mut Out) {
    if s.pause_alpha <= 0.0 {
        return;
    }
    o.push(Ev::floats(
        EV_PAUSE_BG,
        &[
            0.0,
            0.0,
            3.0,
            800.0,
            600.0,
            1.0,
            1.0,
            1.0,
            s.pause_alpha * 0.5,
        ],
    ));
    let a = s.pause_alpha;
    let mut y = 200.0f32;
    for i in 0..rows {
        o.push(Ev {
            a: i as i32,
            ..Ev::floats(EV_PAUSE_ROW, &[150.0, y, 4.0, 0.0, 0.0, 1.0, 1.0, 1.0, a])
        });
        y += s.font_size[1] + 2.0;
    }
    if s.yes_no != 0 {
        y += 10.0;
        let (w, h) = (s.font_size[0], s.font_size[1]);
        o.push(Ev::floats(EV_YES, &[170.0, y, 4.0, w, h, 1.0, 1.0, 1.0, a]));
        o.push(Ev::floats(
            EV_YES_KEY,
            &[250.0, y, 4.0, w - 1.0, h - 1.0, 1.0, 1.0, 1.0, a],
        ));
        y += h + 4.0;
        o.push(Ev::floats(EV_NO, &[170.0, y, 4.0, w, h, 1.0, 1.0, 1.0, a]));
        o.push(Ev::floats(
            EV_NO_KEY,
            &[250.0, y, 4.0, w - 1.0, h - 1.0, 1.0, 1.0, 1.0, a],
        ));
    }
}

/// `center`/`start` are the HUD virtual size and start position, `icon` the icon's active size.
pub fn draw_quest(
    s: &MessageState,
    icon: Option<[f32; 2]>,
    center: [f32; 2],
    start: [f32; 3],
    o: &mut Out,
) {
    let Some(icon) = icon else { return };
    if s.quest_alpha <= 0.0 {
        return;
    }
    let x = center[0] - icon[0] - 20.0 - start[0];
    let y = center[1] - icon[1] - 20.0 - start[1];
    let z = 10.0 - start[2];
    o.push(Ev::floats(
        EV_QUEST_ICON,
        &[x, y, z, 0.0, 0.0, 1.0, 1.0, 1.0, s.quest_alpha],
    ));
    let g = s.quest_alpha * s.quest_oscill.val;
    for _ in 0..2 {
        o.push(Ev::floats(
            EV_QUEST_GLOW,
            &[x, y, z + 1.0, 0.0, 0.0, g, g, g, 1.0],
        ));
    }
}
