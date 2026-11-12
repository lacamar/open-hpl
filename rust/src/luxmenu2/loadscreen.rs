//! `cLuxLoadScreenHandler`: load text choice, fade and hold timing.

use crate::math::{get_flag_bit, rand_rectl, smooth_curve};

pub const STATE_GAME: i32 = 0;
pub const STATE_NONE: i32 = 1;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Load {
    pub alpha: f32,
    pub loading_alpha: f32,
    pub extra_time: f32,
    pub state: i32,
    pub rand_num: i32,
    pub prev_bits: i32,
}

impl Load {
    pub fn new() -> Load {
        Load {
            alpha: 0.0,
            loading_alpha: 1.0,
            extra_time: 0.0,
            state: STATE_NONE,
            rand_num: 0,
            prev_bits: 0,
        }
    }

    pub fn reset(&mut self) {
        self.rand_num = 0;
        self.prev_bits = 0;
        self.loading_alpha = 1.0;
        self.alpha = 0.0;
        self.extra_time = 0.0;
        self.state = STATE_NONE;
    }

    /// `SetupLoadText` once the strings are known to differ.
    pub fn setup(&mut self, same: bool, rand_num: i32) {
        if same {
            return;
        }
        self.rand_num = rand_num;
        self.prev_bits = 0;
    }

    /// `wanted` is the text duration times the duration multiplier, if there is a text entry.
    pub fn load_done(&mut self, wanted: Option<f32>, load_time: f32) {
        self.alpha = 1.0;
        self.extra_time = 0.0;
        self.loading_alpha = 1.0;
        self.state = STATE_GAME;
        if let Some(w) = wanted
            && w > load_time
        {
            self.extra_time = w - load_time;
        }
    }

    pub fn exit_pressed(&mut self) {
        if self.state == STATE_GAME {
            self.extra_time = 0.0;
        }
    }

    /// True when the screen is done and `Exit` is to run.
    pub fn update_game(&mut self, dt: f32, fade_out_time: f32) -> bool {
        if self.loading_alpha > 0.0 {
            self.loading_alpha = (-dt).mul_add(0.33, self.loading_alpha);
            if self.loading_alpha < 0.0 {
                self.loading_alpha = 0.0;
            }
        }
        if self.extra_time > 0.0 {
            self.extra_time -= dt;
            return false;
        }
        self.alpha = (-dt).mul_add(1.0 / fade_out_time, self.alpha);
        if self.alpha < 0.0 {
            self.alpha = 0.0;
            return true;
        }
        false
    }

    /// Entry suffix of the random load text, empty when there is a single text. `0N` below 10 and
    /// `N` above, where N is the one-based number, so the tenth text gets "010".
    pub fn text_suffix(&mut self) -> Vec<u8> {
        if self.rand_num <= 1 {
            return Vec::new();
        }
        let mut num: i32 = -1;
        let mut idx = rand_rectl(0, self.rand_num - 1);
        for _ in 0..self.rand_num {
            if get_flag_bit(idx) & self.prev_bits == 0 {
                num = idx;
                break;
            }
            idx = idx.wrapping_add(1);
            if idx >= self.rand_num {
                idx = 0;
            }
        }
        if num < 0 {
            self.prev_bits = 0;
            num = rand_rectl(0, self.rand_num - 1);
        }
        self.prev_bits |= get_flag_bit(num);
        let mut out = Vec::new();
        if num < 10 {
            out.push(b'0');
        }
        out.extend((num.wrapping_add(1)).to_string().bytes());
        out
    }
}

impl Default for Load {
    fn default() -> Load {
        Load::new()
    }
}

/// Y of the load text between the with-image and alone positions.
pub fn text_y(loading_alpha: f32, with_image: f32, alone: f32) -> f32 {
    let t = smooth_curve(loading_alpha);
    with_image.mul_add(t, alone * (1.0 - t))
}

pub fn centered(size: f32, center: f32) -> f32 {
    center - size / 2.0
}
