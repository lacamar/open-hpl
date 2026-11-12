//! `cLuxPreMenu`: intro section sequencing, fades and the gamma slider maths.

use crate::string::to_lower;

pub const S_INITIAL: i32 = 0;
pub const S_FADE_IN: i32 = 1;
pub const S_FADE_OUT: i32 = 2;
pub const S_FAST_FADE_OUT: i32 = 3;
pub const S_SHOW: i32 = 4;
pub const S_FINAL: i32 = 5;

pub const ACT_NONE: i32 = 0;
pub const ACT_SET_TIMER: i32 = 1;
pub const ACT_FINAL: i32 = 2;
pub const ACT_SECTION: i32 = 3;
pub const ACT_CHECK_SHOW: i32 = 4;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Pre {
    pub alpha: f32,
    pub timer: f32,
    pub section: i32,
    pub state: i32,
    pub exit: bool,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Step {
    pub act: i32,
    pub section: i32,
}

impl Pre {
    /// C++ left the fade and timer members uninitialised until the state machine set them.
    pub fn new() -> Pre {
        Pre {
            alpha: 0.0,
            timer: 0.0,
            section: -1,
            state: S_INITIAL,
            exit: false,
        }
    }

    pub fn enter(&mut self) {
        self.section = -1;
        self.state = S_INITIAL;
    }

    pub fn update_actions(&mut self, dt: f32) {
        match self.state {
            S_INITIAL => self.alpha = 0.0,
            S_FINAL => self.exit = true,
            S_FADE_IN => {
                if self.alpha > 0.0 {
                    self.alpha -= dt;
                } else {
                    self.alpha = 0.0;
                }
            }
            S_FADE_OUT => {
                if self.alpha < 1.0 {
                    self.alpha += dt;
                } else {
                    self.alpha = 1.0;
                }
            }
            S_FAST_FADE_OUT => {
                if self.alpha < 1.0 {
                    self.alpha += dt * 2.0;
                } else {
                    self.alpha = 1.0;
                }
            }
            S_SHOW => self.timer -= dt,
            _ => {}
        }
    }

    /// `first_only[i]`: section i has `ShowFirstStartOnly`. The caller destroys the old background
    /// and labels on `ACT_FINAL`/`ACT_SECTION`, then builds the new section.
    pub fn update_state(&mut self, first_start: bool, first_only: &[bool]) -> Step {
        let mut st = Step {
            act: ACT_NONE,
            section: -1,
        };
        match self.state {
            S_INITIAL => self.state = S_FADE_OUT,
            S_FADE_IN => {
                if self.alpha == 0.0 {
                    self.state = S_SHOW;
                    st.act = ACT_SET_TIMER;
                }
            }
            S_FADE_OUT | S_FAST_FADE_OUT => {
                if self.alpha == 1.0 {
                    self.section += 1;
                    let n = first_only.len();
                    if !first_start {
                        while usize::try_from(self.section).is_ok_and(|i| i < n)
                            && usize::try_from(self.section)
                                .ok()
                                .and_then(|i| first_only.get(i))
                                .copied()
                                .unwrap_or(false)
                        {
                            self.section += 1;
                        }
                    }
                    // C++ read past the section list when a button press revived a finished sequence.
                    if usize::try_from(self.section).is_ok_and(|i| i >= n) {
                        self.state = S_FINAL;
                        st.act = ACT_FINAL;
                    } else {
                        self.state = S_FADE_IN;
                        st.act = ACT_SECTION;
                        st.section = self.section;
                    }
                }
            }
            S_SHOW => st.act = ACT_CHECK_SHOW,
            _ => {}
        }
        st
    }

    pub fn check_show(&mut self, has_text: bool, has_gamma: bool) {
        if !has_text && !has_gamma && self.timer <= 0.0 {
            self.state = S_FADE_OUT;
        }
    }

    pub fn button_pressed(&mut self, has_section: bool, allow_skipping: bool) {
        if has_section && !allow_skipping {
            return;
        }
        self.state = S_FAST_FADE_OUT;
    }
}

impl Default for Pre {
    fn default() -> Pre {
        Pre::new()
    }
}

pub fn music_fade_speed(fade_time: f32) -> f32 {
    if fade_time == 0.0 {
        100.0
    } else {
        1.0 / fade_time
    }
}

/// `cMath::RoundToInt`.
fn round_to_int(x: f32) -> i32 {
    (f64::from(x) + 0.5).floor() as i32
}

pub const GAMMA_MIN: f32 = 0.3;
pub const GAMMA_MAX: f32 = 2.0;
pub const GAMMA_STEP: f32 = 0.05;

/// (slider max value, bar size).
pub fn gamma_slider() -> (i32, i32) {
    let max = round_to_int((GAMMA_MAX - GAMMA_MIN) / GAMMA_STEP);
    (max, round_to_int(0.25 * max as f32))
}

pub fn gamma_to_slider(gamma: f32, max_value: i32) -> i32 {
    let g = crate::math::clamp(gamma, GAMMA_MIN, GAMMA_MAX);
    let range = GAMMA_MAX - GAMMA_MIN;
    round_to_int((g - GAMMA_MIN) * max_value as f32 / range)
}

pub fn slider_to_gamma(value: i32, max_value: i32) -> f32 {
    let rel = value as f32 / max_value as f32;
    (GAMMA_MAX - GAMMA_MIN).mul_add(rel, GAMMA_MIN)
}

/// Slider step of a UI arrow press: -1 left, 1 right, 0 otherwise.
pub fn gamma_arrow(arrow: i32) -> i32 {
    match arrow {
        8 => -1,
        2 => 1,
        _ => 0,
    }
}

/// (x, width) of a background of the given image size on the 800x600 canvas.
pub fn background_rect(w: f32, h: f32) -> (f32, f32) {
    let ratio_mul = w / h / (4.0 / 3.0);
    (400.0 * (1.0 - ratio_mul), 800.0 * ratio_mul)
}

/// `ToFontAlign`: 0 left, 1 right, 2 center; None for an unknown name (C++ logged an error).
pub fn parse_align(name: &[u8]) -> Option<i32> {
    match to_lower(name).as_slice() {
        b"center" => Some(2),
        b"right" => Some(1),
        b"left" => Some(0),
        _ => None,
    }
}
