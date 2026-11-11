//! `SomaImGui.cpp` widget decisions: buttons, sliders, check boxes, multi-selects and the
//! geometry of text, gauges and windows. The script-owned widget structs and the draw recording
//! stay C++.

use super::imgui::{Actions, State, became_pressed};
use crate::math::clamp;

fn fmax(a: f32, b: f32) -> f32 {
    if a < b { b } else { a }
}

/// Which colour a button's frame takes.
pub const COLOR_BASE: i32 = 0;
pub const COLOR_TRIG_FOCUS: i32 = 1;
pub const COLOR_TRIG: i32 = 2;
pub const COLOR_FOCUS: i32 = 3;

/// The script-owned button struct's flags the decisions read.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct ButtonFlags {
    pub use_trig_gfx: bool,
    pub use_focus_gfx: bool,
    pub use_trig_color: bool,
    pub use_trig_focus_color: bool,
    pub use_focus_color: bool,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct ButtonOut {
    pub clicked: bool,
    pub down: bool,
    pub result: bool,
    pub trig: bool,
    pub focus: bool,
    pub color: i32,
}

/// `DoButton`: modes 1 toggle, 2 repeat, 3/4 multi-toggle item on/off.
pub fn button(
    st: &mut State,
    act: &mut Actions<'_>,
    over: bool,
    mouse_over: bool,
    mode: i32,
    dt: f32,
    fl: ButtonFlags,
) -> ButtonOut {
    let clicked = over && became_pressed(act, true, true);
    let down_at = |a: &Actions<'_>, k: usize| a.down.get(k).copied().unwrap_or(false);
    let down = over && (down_at(act, 2) || (mouse_over && down_at(act, 1)));
    let mut result = clicked;
    if mode == 1 {
        if !st.set_int {
            st.int = 0;
            st.set_int = true;
        }
        if clicked {
            st.int = i32::from(st.int == 0);
        }
        result = st.int != 0;
    } else if mode == 2 {
        if down {
            st.float -= dt;
            result = clicked || st.float <= 0.0;
            if st.float <= 0.0 {
                st.float = 0.1;
            }
        } else {
            st.float = 0.4;
        }
    }
    let trig = if mode == 1 {
        result
    } else {
        mode == 3 || (mode != 4 && down && over)
    };
    let focus = over && !(mode == 2 && trig && (fl.use_trig_gfx || fl.use_trig_color));
    let color = if trig && focus && fl.use_trig_focus_color && !fl.use_trig_gfx {
        COLOR_TRIG_FOCUS
    } else if trig && fl.use_trig_color && !fl.use_trig_gfx {
        COLOR_TRIG
    } else if focus && fl.use_focus_color && !fl.use_focus_gfx {
        COLOR_FOCUS
    } else {
        COLOR_BASE
    };
    ButtonOut {
        clicked,
        down,
        result,
        trig,
        focus,
        color,
    }
}

#[repr(C)]
pub struct SliderIn {
    pub in_focus: bool,
    pub over: bool,
    pub min: f32,
    pub max: f32,
    pub step: f32,
    pub vertical: bool,
    pub pos: [f32; 3],
    pub size: [f32; 2],
    pub mouse: [f32; 2],
}

/// `DoSlider` after the widget base: keys nudge the value, the mouse drags it. Returns whether
/// the mouse focus is to be locked.
pub fn slider(st: &mut State, act: &mut Actions<'_>, i: &SliderIn) -> bool {
    if i.in_focus {
        let step = if i.step > 0.0 {
            i.step
        } else {
            (i.max - i.min) * 0.05
        };
        let (inc, dec) = if i.vertical { (5, 4) } else { (6, 7) };
        let t_inc = act.triggered(inc, false);
        let t_dec = act.triggered(dec, false);
        let mut d = (if t_inc { step } else { 0.0 }) - (if t_dec { step } else { 0.0 });
        let t8 = act.triggered(8, false);
        let t9 = act.triggered(9, false);
        d += (if t8 { step } else { 0.0 }) - (if t9 { step } else { 0.0 });
        if i.vertical {
            d = -d;
        }
        if d != 0.0 {
            let lo = if i.max < i.min { i.max } else { i.min };
            let hi = fmax(i.min, i.max);
            st.float = clamp(st.float + d, lo, hi);
        }
        if i.over && act.triggered(1, false) {
            st.int = 1;
        }
    }
    if !act.down.get(1).copied().unwrap_or(false) {
        st.int = 0;
    }
    if st.int != 0 && i.size[0] > 0.0 && i.size[1] > 0.0 {
        let t = if i.vertical {
            1.0 - (i.mouse[1] - i.pos[1]) / i.size[1]
        } else {
            (i.mouse[0] - i.pos[0]) / i.size[0]
        };
        let mut v = clamp(t, 0.0, 1.0).mul_add(i.max - i.min, i.min);
        if i.step > 0.0 {
            v = ((v - i.min) / i.step).round().mul_add(i.step, i.min);
        }
        st.float = v;
        return true;
    }
    false
}

/// Slider handle position along the track.
pub fn slider_button_pos(
    value: f32,
    range: [f32; 2],
    pos: [f32; 3],
    size: [f32; 2],
    button: [f32; 2],
    vertical: bool,
) -> [f32; 3] {
    let t = if range[1] > range[0] {
        (value - range[0]) / (range[1] - range[0])
    } else {
        0.0
    };
    if vertical {
        [
            pos[0],
            (1.0 - t).mul_add(size[1] - button[1], pos[1]),
            pos[2] + 0.1,
        ]
    } else {
        [t.mul_add(size[0] - button[0], pos[0]), pos[1], pos[2] + 0.1]
    }
}

/// `DoCheckBox` toggle: returns whether it was toggled.
pub fn checkbox(st: &mut State, act: &mut Actions<'_>, over: bool) -> bool {
    let toggled = over && became_pressed(act, true, true);
    if toggled {
        st.int = i32::from(st.int == 0);
    }
    toggled
}

#[repr(C)]
pub struct MultiIn {
    pub over: bool,
    pub mouse_over: bool,
    pub num: i32,
    pub mouse_x: f32,
    pub pos_x: f32,
    pub size_x: f32,
}

/// `DoMultiSelect` after the widget base: arrows and clicks step through the items.
pub fn multi_select(st: &mut State, act: &mut Actions<'_>, i: &MultiIn) {
    let num = i.num;
    if i.over && num > 0 {
        let a = i32::from(act.triggered(2, false));
        let b = i32::from(act.triggered(6, false));
        let c = i32::from(act.triggered(7, false));
        let mut d = a + b - c;
        if act.triggered(1, false) && i.mouse_over {
            d += if i.mouse_x < i.size_x.mul_add(0.5, i.pos_x) {
                -1
            } else {
                1
            };
        }
        st.int = st
            .int
            .wrapping_add(d)
            .wrapping_rem(num)
            .wrapping_add(num)
            .wrapping_rem(num);
    }
    if num > 0 {
        // C++ called the float `cMath::Clamp` on the ints
        st.int = clamp(st.int as f32, 0.0, (num - 1) as f32) as i32;
    }
}

/// `DrawText`: where the text of a widget goes for an alignment (1 right, 2 centre).
pub fn text_origin(pos: [f32; 3], size: [f32; 2], font: [f32; 2], align: i32) -> [f32; 3] {
    let x = match align {
        2 => size[0].mul_add(0.5, pos[0]),
        1 => pos[0] + size[0],
        _ => pos[0],
    };
    let y = pos[1] + fmax(0.0, (size[1] - font[1]) * 0.5);
    [x, y, pos[2] + 0.2]
}

pub fn color_mul(a: [f32; 4], b: [f32; 4]) -> [f32; 4] {
    [a[0] * b[0], a[1] * b[1], a[2] * b[2], a[3] * b[3]]
}

/// Gauge fill rectangle: (position, size).
pub fn gauge_fill(
    pos: [f32; 3],
    size: [f32; 2],
    pad: [f32; 2],
    fill: f32,
    vertical: bool,
) -> ([f32; 3], [f32; 2]) {
    let inner = [pad[0].mul_add(-2.0, size[0]), pad[1].mul_add(-2.0, size[1])];
    let f = clamp(fill, 0.0, 1.0);
    let fs = if vertical {
        [inner[0], inner[1] * f]
    } else {
        [inner[0] * f, inner[1]]
    };
    (
        [
            pos[0] + pad[0],
            pos[1] + (pad[1] + if vertical { inner[1] - fs[1] } else { 0.0 }),
            pos[2] + 0.1,
        ],
        fs,
    )
}
