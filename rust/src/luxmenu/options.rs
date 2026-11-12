//! Options window: option tables, value ranges and conversions, resolution list, restart and
//! apply decisions, tip fade. Widget calls and config writes stay in C++.

use crate::math::clamp;
use crate::string;

pub const GFX_RENDERERS: [&str; 2] = ["OpenGL", "Vulkan"];
pub const COLOR_OUTPUTS: [&str; 4] = ["SDR", "HDR10", "scRGB", "P3"];

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Range {
    Gamma,
    Mouse,
    Gamepad,
    Volume,
    PaperWhite,
    Peak,
}

/// `(min, step, max)` of a slider.
pub fn range(r: Range) -> (f32, f32, f32) {
    match r {
        Range::Gamma => (0.3, 0.05, 2.0),
        Range::Mouse => (0.2, 0.1, 5.0),
        Range::Gamepad => (0.5, 0.05, 3.0),
        Range::Volume => (0.0, 0.1, 1.0),
        Range::PaperWhite => (80.0, 5.0, 400.0),
        Range::Peak => (400.0, 50.0, 2000.0),
    }
}

pub fn aniso_index(x: f32) -> i32 {
    match x {
        x if x <= 1.0 => 0,
        x if x <= 2.0 => 1,
        x if x <= 4.0 => 2,
        x if x <= 8.0 => 3,
        x if x <= 16.0 => 4,
        _ => 0,
    }
}

pub fn aniso_from_index(i: i32) -> f32 {
    match i {
        1 => 2.0,
        2 => 4.0,
        3 => 8.0,
        4 => 16.0,
        _ => 1.0,
    }
}

pub fn ssao_index(samples: i32) -> i32 {
    match samples {
        s if s <= 4 => 0,
        s if s <= 8 => 1,
        s if s <= 16 => 2,
        s if s <= 32 => 3,
        _ => 0,
    }
}

/// Unknown indices give 3, not a sample count.
pub fn ssao_from_index(i: i32) -> i32 {
    match i {
        0 => 4,
        1 => 8,
        2 => 16,
        3 => 32,
        _ => 3,
    }
}

/// `cMath::RoundToInt`: `(int)floor(x + 0.5)` in double; out of range saturates.
pub fn round_to_int(x: f32) -> i32 {
    (f64::from(x) + 0.5).floor() as i32
}

/// `SetUpSlider`: `(max value, bar size)`.
pub fn slider_setup(min: f32, max: f32, step: f32) -> (i32, i32) {
    let m = round_to_int((max - min) / step);
    (m, round_to_int(0.25 * m as f32))
}

pub fn slider_to_int(value: f32, min: f32, max: f32, slider_max: i32) -> i32 {
    let v = clamp(value, min, max);
    round_to_int((v - min) * slider_max as f32 / (max - min))
}

pub fn slider_to_value(value: i32, slider_max: i32, min: f32, max: f32) -> f32 {
    let rel = value as f32 / slider_max as f32;
    (max - min).mul_add(rel, min)
}

fn widen(b: &[u8]) -> Vec<u32> {
    b.iter().map(|&c| u32::from(c)).collect()
}

pub fn slider_label(value: f32, min: f32, max: f32) -> Vec<u32> {
    let v = if value <= min {
        min
    } else if value >= max {
        max
    } else {
        value
    };
    widen(&string::float_to_string(v, 3, true))
}

pub fn int_label(x: f32) -> Vec<u32> {
    widen(&string::int_to_string(round_to_int(x), 0))
}

pub fn renderer_name(i: i32) -> &'static str {
    GFX_RENDERERS
        .get(usize::from(i == 1))
        .copied()
        .unwrap_or("OpenGL")
}

pub fn renderer_index(name: &[u8]) -> i32 {
    i32::from(string::to_lower(name) == b"vulkan")
}

pub fn color_name(i: i32) -> &'static str {
    let i = usize::try_from(i.clamp(0, 3)).unwrap_or(0);
    COLOR_OUTPUTS.get(i).copied().unwrap_or("SDR")
}

pub fn color_index(name: &[u8]) -> i32 {
    match string::to_lower(name).as_slice() {
        b"hdr10" => 1,
        b"scrgb" => 2,
        b"p3" => 3,
        _ => 0,
    }
}

pub fn gui_scale_index(scale: i32, items: i32) -> i32 {
    clamp(
        scale.wrapping_sub(1) as f32,
        0.0,
        items.wrapping_sub(1) as f32,
    ) as i32
}

/// Texture quality counts up, the combo box lists it down; the map is its own inverse.
pub fn flip_index(i: i32, items: i32) -> i32 {
    items.wrapping_sub(1).wrapping_sub(i)
}

/// Index of the language file named `lang`, 0 when none.
pub fn language_index(lang: &[u8], files: &[&[u32]]) -> i32 {
    files
        .iter()
        .position(|f| string::wide_to_utf8(f) == lang)
        .map_or(0, |i| i as i32)
}

/// Index of the sound device `id` in `ids`, the last item when absent.
pub fn sound_device_index(id: i32, ids: &[i32], items: i32) -> i32 {
    ids.iter()
        .position(|&d| d == id)
        .map_or(items.wrapping_sub(1), |i| i as i32)
}

#[repr(C)]
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub struct Mode {
    pub display: i32,
    pub width: i32,
    pub height: i32,
    pub refresh: i32,
}

impl Mode {
    fn same_screen(&self, o: &Mode) -> bool {
        self.width == o.width && self.height == o.height && self.display == o.display
    }
}

/// More than one display among the modes.
pub fn multi_display(modes: &[Mode]) -> bool {
    modes
        .first()
        .is_some_and(|f| modes.iter().skip(1).any(|m| m.display != f.display))
}

/// Collapses runs of equal display and size as the C++ shifting loop does: per run, the index of
/// the last mode and the refresh rate of the first.
pub fn dedup_modes(modes: &[Mode]) -> Vec<(usize, i32)> {
    let mut out = Vec::new();
    let mut i = 0;
    while let Some(head) = modes.get(i) {
        let run = modes
            .iter()
            .skip(i + 1)
            .take_while(|m| head.same_screen(m))
            .count();
        out.push((i + run, head.refresh));
        i += run + 1;
    }
    out
}

/// Index of the last mode on the same display and size as the current one, -1 when none.
pub fn current_mode(modes: &[Mode], display: i32, width: i32, height: i32) -> i32 {
    modes
        .iter()
        .rposition(|m| m.display == display && m.width == width && m.height == height)
        .map_or(-1, |i| i as i32)
}

pub fn resolution_label(width: i32, height: i32, custom: bool) -> Vec<u32> {
    let mut s = string::int_to_string(width, 0);
    s.push(b'x');
    s.extend(string::int_to_string(height, 0));
    if custom {
        s.extend_from_slice(b" (Custom)");
    }
    widen(&s)
}

/// `Option_OnChangeValue`: whether a restart warning must be raised.
pub fn needs_restart(setting_initial: bool, has_data: bool, data_needs_restart: bool) -> bool {
    !setting_initial && has_data && data_needs_restart
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Activate {
    /// The key config window was open: leave the widgets alone.
    KeepKeyConfig,
    /// Reload from the values kept across a language change.
    FromCurrent,
    DumpThenInitial,
    FromInitial,
}

pub fn activate(key_config_open: bool, setting_language: bool, initial_set: bool) -> Activate {
    if key_config_open {
        Activate::KeepKeyConfig
    } else if setting_language {
        Activate::FromCurrent
    } else if !initial_set {
        Activate::DumpThenInitial
    } else {
        Activate::FromInitial
    }
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub struct Toggle {
    pub show_basic: bool,
    pub show_advanced: bool,
    /// 0: "Advanced" label, 1: "Basic" label.
    pub label: usize,
    /// The toggle button is the first focus target of the tab, with refraction above it.
    pub toggle_first: bool,
}

pub fn toggle_gfx(basic_visible: bool) -> Toggle {
    Toggle {
        show_basic: !basic_visible,
        show_advanced: basic_visible,
        label: usize::from(!basic_visible),
        toggle_first: basic_visible,
    }
}

#[derive(Clone, Copy, PartialEq, Debug)]
pub struct Tip {
    pub restart: bool,
    pub text_reset: bool,
}

#[derive(Clone, Copy, PartialEq, Debug)]
pub enum TipAct {
    None,
    /// Alpha grows by `delta`; clamp to 1 when `clamp`.
    FadeIn {
        delta: f32,
        clamp: bool,
    },
    FadeOut {
        delta: f32,
    },
}

#[derive(Clone, Copy, PartialEq, Debug)]
pub struct TipStep {
    pub tip: Tip,
    pub reset_scroll: bool,
    pub act: TipAct,
}

/// `Window_OnUpdate` fade of the tip label.
pub fn tip_step(mut tip: Tip, alpha: f32, dt: f32) -> TipStep {
    let mut reset_scroll = false;
    let act = if tip.restart {
        if tip.text_reset {
            tip.text_reset = false;
            reset_scroll = true;
        }
        let delta = dt * 3.0;
        let clamp = alpha + delta >= 1.0;
        if clamp {
            tip.restart = false;
        }
        TipAct::FadeIn { delta, clamp }
    } else if alpha > 0.0 {
        TipAct::FadeOut { delta: dt * 0.8 }
    } else {
        tip.text_reset = true;
        TipAct::None
    };
    TipStep {
        tip,
        reset_scroll,
        act,
    }
}
