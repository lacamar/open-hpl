//! `cLuxHintHandler`: hint queue state and timing, and the draw layout of the hint text. The
//! given/blocked name sets stay C++ (the save layer reads them); fonts and draws stay C++.

use super::event::{Ev, Out};

pub const EV_HINT_LABEL: u32 = 1;
pub const EV_HINT_ROW: u32 = 2;

pub const WRAP_WIDTH: f32 = 680.0;
pub const WRAP_LINE_HEIGHT: f32 = 15.0;

/// Mirrors `hpl::cLinearOscillation` (`add` is its private last member).
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct Oscillation {
    pub max: f32,
    pub min: f32,
    pub val: f32,
    pub up_speed: f32,
    pub down_speed: f32,
    pub add: f32,
}

impl Oscillation {
    pub fn set_up(&mut self, min: f32, max: f32, val: f32, up: f32, down: f32) {
        self.min = min;
        self.max = max;
        self.val = val;
        self.up_speed = up.abs();
        self.down_speed = -down.abs();
        self.add = self.up_speed;
    }

    pub fn update(&mut self, dt: f32) {
        self.val = self.add.mul_add(dt, self.val);
        if self.add > 0.0 {
            if self.val >= self.max {
                self.val = self.max;
                self.add = self.down_speed;
            }
        } else if self.val <= self.min {
            self.val = self.min;
            self.add = self.up_speed;
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct HintState {
    pub active: u8,
    pub alpha: f32,
    pub show_count: f32,
    pub y_pos: f32,
    pub font_size: [f32; 2],
    pub duration_mul: f32,
    pub oscill: Oscillation,
}

pub fn init(s: &mut HintState, y_pos: f32, font: [f32; 2], duration_mul: f32) {
    s.y_pos = y_pos;
    s.font_size = font;
    s.duration_mul = duration_mul;
    s.oscill.set_up(0.0, 1.0, 0.0, 1.0, 1.0);
}

pub fn reset(s: &mut HintState) {
    s.alpha = 0.0;
    s.show_count = 0.0;
}

/// Returns whether the current text is cleared.
pub fn set_active(s: &mut HintState, on: bool) -> bool {
    s.active = u8::from(on);
    if !on {
        s.alpha = 0.0;
        s.show_count = 0.0;
    }
    !on
}

pub fn update(s: &mut HintState, dt: f32) {
    if s.active == 0 {
        return;
    }
    if s.alpha > 0.0 {
        s.oscill.update(dt);
    }
    if s.show_count <= 0.0 && s.alpha > 0.0 {
        s.alpha = (f64::from(s.alpha) - 0.5 * f64::from(dt)) as f32;
        if s.alpha < 0.0 {
            s.alpha = 0.0;
        }
        return;
    }
    if s.show_count > 0.0 {
        s.alpha = 1.2f32.mul_add(dt, s.alpha);
        if s.alpha > 1.0 {
            s.alpha = 1.0;
        }
        s.show_count -= dt;
    }
}

/// First half of `Add`: whether a hint may start. C++ then records the name and parses the text.
pub fn can_add(s: &HintState, blocked: bool, given: bool) -> bool {
    s.show_count <= 0.0 && !blocked && !given
}

pub fn start(s: &mut HintState, time_shown: f32, parsed_duration: f32) {
    s.show_count = if time_shown <= 0.0 {
        parsed_duration * s.duration_mul
    } else {
        time_shown
    };
    s.alpha = 0.0;
}

pub fn drawable(s: &HintState, ptest: bool) -> bool {
    !ptest && s.active != 0 && s.alpha > 0.0
}

/// Label draw and, for one wrapped row, its position. `rows` is the wrapped row count.
pub fn draw_label(s: &HintState, o: &mut Out) {
    let f = (f64::from(s.oscill.val) * 0.5) as f32;
    o.push(Ev::floats(
        EV_HINT_LABEL,
        &[400.0, s.y_pos, 20.0, 1.0, f, f, s.alpha],
    ));
}

pub fn row_pos(s: &HintState, rows: usize, i: usize) -> [f32; 4] {
    let start_y = s.y_pos + s.font_size[1] + 3.0;
    if rows == 1 {
        [400.0, start_y, 20.0, 1.0]
    } else {
        [
            400.0 - WRAP_WIDTH * 0.5,
            (i as f32).mul_add(s.font_size[1] + 2.0, start_y),
            20.0,
            0.0,
        ]
    }
}

/// Removes `$command$` spans from `text`; `replace(command, position)` supplies what takes their
/// place. An unterminated span is dropped.
pub fn strip_icons(text: &[u32], replace: &mut dyn FnMut(&[u32], i32) -> Vec<u32>) -> Vec<u32> {
    let mut out = Vec::new();
    let mut cmd: Vec<u32> = Vec::new();
    let mut in_cmd = false;
    let mut pos = 0i32;
    for &c in text {
        if in_cmd {
            if c != u32::from(b'$') {
                cmd.push(c);
            } else {
                let r = replace(&cmd, pos);
                pos = pos.wrapping_add(r.len() as i32);
                out.extend_from_slice(&r);
                in_cmd = false;
            }
        } else if c == u32::from(b'$') {
            in_cmd = true;
            cmd.clear();
        } else {
            pos = pos.wrapping_add(1);
            out.push(c);
        }
    }
    out
}
