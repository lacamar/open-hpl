//! `cLuxEffect_PlayVoice`: decisions of the voice list, interruption and subtitle queue. The sound
//! entries, the list itself and the word wrapping stay C++; every step returns ordered ops.

use super::hw::{msub, mul};
use super::ops::Ops;

pub mod k {
    pub const FADE_VOICE: u32 = 10;
    pub const VOICE_NULL: u32 = 11;
    pub const FADE_EFFECT: u32 = 12;
    pub const EFFECT_NULL: u32 = 13;
    pub const POP_FRONT: u32 = 14;
    pub const PUSH_FRONT: u32 = 15;
    pub const PUSH_BACK: u32 = 16;
    pub const VOL_VOICE: u32 = 17;
    pub const VOL_EFFECT: u32 = 18;
    pub const CLEAR_TEXT: u32 = 19;
    pub const WRAP_NOW: u32 = 20;
    pub const QUEUE: u32 = 21;
}

pub const ACTION_RETURN: i32 = 0;
pub const ACTION_FINALIZE: i32 = 1;
pub const ACTION_PLAY: i32 = 2;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct AddIn {
    pub active: u32,
    pub list_empty: u32,
    pub front_priority: i32,
    pub priority: i32,
    pub remove_interrupted: u32,
    pub voice_valid: u32,
    pub effect_valid: u32,
}

/// `AddMultiSubbedVoice` after the voice data was built. Ops: interrupt the current voice, then
/// pop and push the list.
pub fn add(i: &AddIn, o: &mut Ops) {
    if i.active != 0 && i.list_empty == 0 {
        if i.priority >= 0 && i.front_priority <= i.priority {
            if i.voice_valid != 0 {
                o.k(k::FADE_VOICE);
            }
            o.k(k::VOICE_NULL);
            if i.effect_valid != 0 {
                o.k(k::FADE_EFFECT);
            }
            o.k(k::EFFECT_NULL);
            if i.remove_interrupted != 0 {
                o.k(k::POP_FRONT);
            }
            o.k(k::PUSH_FRONT);
        } else {
            o.k(k::PUSH_BACK);
        }
    } else {
        o.k(k::PUSH_BACK);
    }
}

/// Subtracts the step from every queued delay; returns how many leading entries are due.
pub fn text_tick(delays: &mut [f32], dt: f32) -> usize {
    for d in delays.iter_mut() {
        *d -= dt;
    }
    delays.iter().take_while(|d| **d <= 0.0).count()
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct UpdateIn {
    pub volume_mul: f32,
    pub voice_nonnull: u32,
    pub voice_valid: u32,
    pub effect_nonnull: u32,
    pub effect_valid: u32,
    pub list_empty: u32,
}

/// First half of `Update` after the subtitle queue: volume ops and the early exits.
pub fn update_a(i: &UpdateIn, o: &mut Ops) -> i32 {
    if i.volume_mul < 1.0 {
        if i.voice_nonnull != 0 && i.voice_valid != 0 {
            o.kf(k::VOL_VOICE, i.volume_mul);
        }
        if i.effect_nonnull != 0 && i.effect_valid != 0 {
            o.kf(k::VOL_EFFECT, i.volume_mul);
        }
    }
    if i.voice_valid != 0 {
        return ACTION_RETURN;
    }
    if i.voice_nonnull == 0 && i.effect_valid != 0 {
        return ACTION_RETURN;
    }
    if i.list_empty != 0 {
        ACTION_FINALIZE
    } else {
        ACTION_PLAY
    }
}

/// Start offset into the stream, in double precision as the C++ computes it.
pub fn start_time(current: f64, interrupted_at: f64, game_time: f64) -> f64 {
    let mut t = current;
    if interrupted_at != -1.0 {
        t += game_time - interrupted_at;
    }
    t
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct TextsIn {
    pub nonempty_mask: u32,
    pub delays: [f32; 7],
    pub list_size: u32,
    pub voice_null: u32,
    pub effect_null: u32,
}

/// Second half of `Update` after the streams started. Ops: clear the text, then wrap the first text
/// now or queue each text with its delay. Returns true when the list ends up empty with nothing
/// playing (the over callback then reads the already cleared name).
pub fn update_b(i: &TextsIn, o: &mut Ops) -> bool {
    o.k(k::CLEAR_TEXT);
    for (n, d) in i.delays.iter().enumerate() {
        if i.nonempty_mask >> n & 1 == 0 {
            continue;
        }
        if n == 0 && *d <= 0.0 {
            o.ki(k::WRAP_NOW, 0);
        } else {
            o.push(k::QUEUE, n as i32, *d, 0.0, 0.0, 0.0);
        }
    }
    i.list_size == 1 && i.voice_null != 0 && i.effect_null != 0
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Row {
    pub row: u32,
    pub align: u32,
    pub x: f32,
    pub y: f32,
    pub z: f32,
}

pub const ALIGN_LEFT: u32 = 0;
pub const ALIGN_CENTER: u32 = 1;

/// Subtitle block of the voice effect: rows stacked upwards from the bottom of the 800x600 hud.
pub fn voice_rows(rows: usize, row_width: f32, font_y: f32) -> Vec<Row> {
    let step = font_y + 2.0f32;
    let mut y = msub(rows as f32, step, 580.0f32);
    let x = 400.0f32 - row_width / 2.0f32;
    (0..rows)
        .map(|r| {
            let row = Row {
                row: r as u32,
                align: ALIGN_LEFT,
                x,
                y,
                z: 4.0,
            };
            y += step;
            row
        })
        .collect()
}

/// Subtitle block of the sound-stream insanity event (fixed 18 high font).
pub fn stream_rows(rows: usize) -> Vec<Row> {
    let mut y = msub(rows as f32, 20.0f32, 580.0f32);
    (0..rows)
        .map(|r| {
            let row = Row {
                row: r as u32,
                align: ALIGN_LEFT,
                x: 250.0,
                y,
                z: 4.0,
            };
            y += 20.0f32;
            row
        })
        .collect()
}

/// Centered text of the emotion flash: one row centered, several left aligned.
pub fn emotion_rows(rows: usize, font_y: f32) -> Vec<Row> {
    let step = font_y + 2.0f32;
    let start_y = msub(mul(step, 0.5f32), rows as f32, 300.0f32);
    if rows == 1 {
        return vec![Row {
            row: 0,
            align: ALIGN_CENTER,
            x: 400.0,
            y: start_y,
            z: 4.0,
        }];
    }
    let mut y = start_y;
    (0..rows)
        .map(|r| {
            let row = Row {
                row: r as u32,
                align: ALIGN_LEFT,
                x: 150.0,
                y,
                z: 4.0,
            };
            y += step;
            row
        })
        .collect()
}
