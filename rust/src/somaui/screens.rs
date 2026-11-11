//! `SomaGammaScreen.cpp` layout and `SomaSplash.cpp` timing and draw plans.

use super::hw;
use crate::math::clamp;

fn fmin(a: f32, b: f32) -> f32 {
    if a < b { a } else { b }
}

fn fmax(a: f32, b: f32) -> f32 {
    if a > b { a } else { b }
}

pub const GAMMA_MIN: f32 = 0.3;
pub const GAMMA_MAX: f32 = 2.0;
pub const INSTRUCTIONS_ROW_HEIGHT: f32 = 30.0;

/// Width the instruction text wraps at.
pub fn gamma_wrap_width(screen_w: f32) -> f32 {
    let w = fmin(700.0, screen_w - 160.0);
    if w < 200.0 { screen_w } else { w }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct GammaLayout {
    pub checker_pos: [f32; 2],
    pub instructions_pos: [f32; 2],
    pub slider_pos: [f32; 3],
    pub slider_size: [f32; 2],
    pub button_pos: [f32; 3],
    pub button_size: [f32; 2],
}

/// `checker` is the checkerboard image size when it loaded; `rows` the wrapped instruction rows.
pub fn gamma_layout(
    screen: [f32; 2],
    checker: Option<[f32; 2]>,
    wrap_w: f32,
    rows: usize,
) -> GammaLayout {
    let (mut cpos, mut csize) = ([0.0f32; 2], [0.0f32; 2]);
    if let Some(c) = checker {
        csize = c;
        cpos = [(screen[0] - c[0]) * 0.5, (screen[1] - c[1]) * 0.5 - 40.0];
    }
    let block = INSTRUCTIONS_ROW_HEIGHT * rows as f32;
    let instructions_pos = [
        (screen[0] - wrap_w) * 0.5,
        fmax(20.0, cpos[1] - 20.0 - block),
    ];
    let slider_size = [300.0, 25.0];
    let slider_pos = [
        (screen[0] - slider_size[0]) * 0.5,
        cpos[1] + csize[1] + 30.0,
        0.1,
    ];
    let button_size = [120.0, 30.0];
    let button_pos = [
        (screen[0] - button_size[0]) * 0.5,
        slider_pos[1] + slider_size[1] + 30.0,
        0.1,
    ];
    GammaLayout {
        checker_pos: cpos,
        instructions_pos,
        slider_pos,
        slider_size,
        button_pos,
        button_size,
    }
}

/// Slider value (0..=100) for the current gamma.
pub fn gamma_to_slider(gamma: f32) -> i32 {
    let g = clamp(gamma, GAMMA_MIN, GAMMA_MAX);
    let v = (g - GAMMA_MIN) * 100.0 / (GAMMA_MAX - GAMMA_MIN);
    ((f64::from(v) + 0.5).floor()) as i32
}

pub fn slider_to_gamma(value: i32, max_value: i32) -> f32 {
    let rel = value as f32 / max_value as f32;
    (GAMMA_MAX - GAMMA_MIN).mul_add(rel, GAMMA_MIN)
}

/// Mouse button edge: 0 none, 1 pressed, 2 released.
pub fn mouse_edge(down: bool, was_down: bool) -> i32 {
    if down && !was_down {
        1
    } else if !down && was_down {
        2
    } else {
        0
    }
}

pub const BOOT_FADE_IN: f32 = 2.2;
pub const BOOT_FADE_OUT_START: f32 = 3.6;
pub const BOOT_FADE_OUT: f32 = 4.0;
pub const BRAIN_START: f32 = 2.05;
pub const BRAIN_FADE_IN: f32 = 1.0;
pub const SCRIPT_START: f32 = 3.7;
pub const BOOT_BLACK_HOLD: f32 = 0.1;
pub const BRAIN_FRAMES: i32 = 26;
const BRAIN_FRAME_RATE: f32 = 15.0;

pub fn boot_duration() -> f32 {
    BOOT_FADE_OUT_START + BOOT_FADE_OUT + BOOT_BLACK_HOLD
}

/// Elapsed time after an update and whether the splash is done.
pub fn splash_update(elapsed: f32, dt: f32) -> (f32, bool) {
    let e = hw::add(elapsed, dt);
    (e, e >= boot_duration())
}

pub fn scripts_may_run(finished: bool, elapsed: f32) -> bool {
    finished || elapsed >= SCRIPT_START
}

/// Boot loading bar progress over time as measured on the official game.
pub fn boot_bar_fraction(t: f32) -> f32 {
    const CURVE: [[f32; 2]; 12] = [
        [0.0, 0.60],
        [0.2, 0.64],
        [0.46, 0.68],
        [0.72, 0.69],
        [0.98, 0.71],
        [1.25, 0.73],
        [1.51, 0.77],
        [1.77, 0.80],
        [2.03, 0.82],
        [2.3, 0.86],
        [2.56, 0.95],
        [2.82, 1.0],
    ];
    for w in CURVE.windows(2) {
        if let [a, b] = w
            && t < b[0]
        {
            return a[1] + (b[1] - a[1]) * (t - a[0]) / (b[0] - a[0]);
        }
    }
    1.0
}

/// Official HUD virtual space: 1024x768 centre, widened by 4/3 of the extra width.
pub fn virtual_width(screen: [f32; 2]) -> f32 {
    (4.0 * 768.0 * screen[0] / screen[1] - 1024.0) / 3.0
}

pub fn virtual_to_screen(pos: [f32; 2], z: f32, vw: f32, screen: [f32; 2]) -> [f32; 3] {
    [
        (pos[0] + (vw - 1024.0) * 0.5) * screen[0] / vw,
        pos[1] * screen[1] / 768.0,
        z,
    ]
}

pub fn virtual_size_to_screen(size: [f32; 2], vw: f32, screen: [f32; 2]) -> [f32; 2] {
    [size[0] * screen[0] / vw, size[1] * screen[1] / 768.0]
}

/// Ping-pong frame of the brain icon; `None` for a negative time (out of range in C++).
pub fn brain_frame(time: f32) -> Option<usize> {
    let period = 2 * (BRAIN_FRAMES - 1);
    let step = ((time * BRAIN_FRAME_RATE) as i32).wrapping_rem(period);
    let frame = if step < BRAIN_FRAMES {
        step
    } else {
        period - step
    };
    usize::try_from(frame).ok()
}

/// Whether a loading-screen frame is due (30 fps cap).
pub fn load_draw_due(now: u64, last: u64) -> bool {
    now.wrapping_sub(last) >= 33
}

pub fn boot_elapsed(now: u64, boot_start: u64) -> f32 {
    fmin(
        now.wrapping_sub(boot_start) as f32 / 1000.0,
        BOOT_FADE_OUT_START,
    )
}

/// Time-based progress of a script loading screen (`ms` since it started).
pub fn load_fraction(now: u64, start: u64) -> f32 {
    // C++: `1.0f - expf((lNow - mlLoadStart) / -4000.0f)` on unsigned longs
    let d = now.wrapping_sub(start) as f32;
    1.0 - exp(d / -4000.0)
}

unsafe extern "C" {
    #[link_name = "expf"]
    fn c_expf(x: f32) -> f32;
}

fn exp(x: f32) -> f32 {
    // SAFETY: pure libm function on plain floats.
    unsafe { c_expf(x) }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct SplashPlan {
    pub alpha: f32,
    /// Premenu image: pos x, y, size w, h; `has_premenu` image present.
    pub premenu_pos: [f32; 3],
    pub premenu_size: [f32; 2],
    pub bar_pos: [f32; 3],
    pub bar_size: [f32; 2],
    /// Clip rect of the bar fill: x, y, w, h.
    pub bar_clip: [f32; 4],
    pub brain_alpha: f32,
}

pub struct SplashIn {
    pub screen: [f32; 2],
    pub vw: f32,
    pub t: f32,
    pub premenu_image: Option<[f32; 2]>,
}

pub fn splash_plan(i: &SplashIn) -> SplashPlan {
    let t = i.t;
    let alpha = if t < BOOT_FADE_OUT_START {
        clamp(t / BOOT_FADE_IN, 0.0, 1.0)
    } else {
        // gcc turns the exact /4.0 into a fused 1 - x * 0.25, which also fixes the NaN sign
        clamp(hw::msub(t - BOOT_FADE_OUT_START, 0.25, 1.0), 0.0, 1.0)
    };
    let mut p = SplashPlan {
        alpha,
        ..SplashPlan::default()
    };
    if let Some(img) = i.premenu_image {
        let k = fmin(i.screen[0] / img[0], i.screen[1] / img[1]);
        let size = [img[0] * k, img[1] * k];
        p.premenu_size = size;
        p.premenu_pos = [
            (i.screen[0] - size[0]) * 0.5,
            (i.screen[1] - size[1]) * 0.5,
            1.0,
        ];
    }
    p.bar_pos = virtual_to_screen([0.0, 512.0], 2.0, i.vw, i.screen);
    p.bar_size = virtual_size_to_screen([1024.0, 128.0], i.vw, i.screen);
    p.bar_clip = [
        p.bar_pos[0],
        p.bar_pos[1],
        p.bar_size[0] * boot_bar_fraction(t),
        p.bar_size[1],
    ];
    p.brain_alpha = clamp((t - BRAIN_START) / BRAIN_FADE_IN, 0.0, 1.0);
    p
}

/// Brain icon position and size, in screen units.
pub fn brain_rect(vw: f32, screen: [f32; 2]) -> ([f32; 3], [f32; 2]) {
    (
        virtual_to_screen(
            [1024.0 + (vw - 1024.0) * 0.5 - 150.0, 648.0],
            3.0,
            vw,
            screen,
        ),
        virtual_size_to_screen([70.0, 70.0], vw, screen),
    )
}
