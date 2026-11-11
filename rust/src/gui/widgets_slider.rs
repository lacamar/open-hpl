//! `cWidgetSlider` and `cWidgetTabFrame` logic: value clamping, handle geometry and drag math,
//! tab-strip layout. Widgets, callbacks and child calls stay in C++.

use super::widget::{MOUSE_WHEEL_DOWN, MOUSE_WHEEL_UP, add3, point_in_rect, v2};
use crate::math::v3;
use crate::types::{Vector2f, Vector3f};

pub const UI_ARROW_UP: i32 = 1;
pub const UI_ARROW_RIGHT: i32 = 2;
pub const UI_ARROW_DOWN: i32 = 4;
pub const UI_ARROW_LEFT: i32 = 8;

/// Fields the C++ constructor leaves uninitialised start at zero.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct SliderState {
    pub vertical: bool,
    pub pressed: bool,
    pub button_size: f32,
    pub value: i32,
    pub max_value: i32,
    pub button_add: i32,
    pub click_add: i32,
    pub bar_value_size: i32,
    pub value_step: f32,
    pub bar_pos: Vector3f,
    pub bar_size: Vector2f,
    pub slider_size: f32,
    pub rect: [f32; 4],
    pub max_pos: f32,
    pub min_pos: f32,
    pub rel_mouse: Vector2f,
}

impl SliderState {
    pub fn new(vertical: bool, button_size: f32) -> Self {
        SliderState {
            vertical,
            pressed: false,
            button_size,
            value: 0,
            max_value: 10,
            button_add: 1,
            click_add: 10,
            bar_value_size: 1,
            value_step: 0.0,
            bar_pos: Vector3f::default(),
            bar_size: Vector2f::default(),
            slider_size: 0.0,
            rect: [0.0; 4],
            max_pos: 0.0,
            min_pos: 0.0,
            rel_mouse: Vector2f::default(),
        }
    }

    /// `SetValue` up to the callbacks: the new value when it changed.
    pub fn set_value(&mut self, v: i32) -> Option<i32> {
        let mut v = v;
        if v < 0 {
            v = 0;
        }
        if v > self.max_value {
            v = self.max_value;
        }
        if self.value == v {
            return None;
        }
        self.value = v;
        Some(v)
    }

    /// True when the value changed and the bar properties must be updated.
    pub fn set_max_value(&mut self, m: i32) -> bool {
        if self.max_value == m {
            return false;
        }
        self.max_value = m;
        if self.max_value < 0 {
            self.max_value = 0;
        }
        if self.bar_value_size > self.max_value {
            self.bar_value_size = self.max_value;
        }
        true
    }

    pub fn set_bar_value_size(&mut self, s: i32) -> bool {
        if self.bar_value_size == s {
            return false;
        }
        self.bar_value_size = s;
        let limit = self.max_value.wrapping_add(1);
        if self.bar_value_size > limit {
            self.bar_value_size = limit;
        }
        true
    }

    /// `UpdateBarProperties`; `c0`/`c3` are the active sizes of corners LU and LD.
    pub fn update_bar(&mut self, size: Vector2f, global: Vector3f, c0: Vector2f, c3: Vector2f) {
        let bvs = self.bar_value_size as f32;
        let span = self.max_value.wrapping_add(1) as f32;
        let max = self.max_value as f32;
        let val = self.value as f32;
        let btn = self.button_size;
        if self.vertical {
            self.slider_size = size.y - btn * 2.0;
            self.bar_size = v2(size.x, (bvs / span) * self.slider_size);
            let min = c0.y + c3.y + 2.0;
            if self.bar_size.y < min {
                self.bar_size.y = min;
            }
            self.value_step = if self.max_value > 0 {
                (1.0 / max) * (self.slider_size - self.bar_size.y)
            } else {
                0.0
            };
            self.bar_pos = v3(0.0, self.value_step.mul_add(val, btn), 0.2);
        } else {
            self.slider_size = size.x - btn * 2.0;
            self.bar_size = v2((bvs / span) * self.slider_size, size.y);
            let min = c0.x + c3.x + 2.0;
            if self.bar_size.x < min {
                self.bar_size.x = min;
            }
            self.value_step = if self.max_value > 0 {
                (1.0 / max) * (self.slider_size - self.bar_size.x)
            } else {
                0.0
            };
            self.bar_pos = v3(self.value_step.mul_add(val, btn), 0.0, 0.2);
        }
        self.rect = [
            global.x + self.bar_pos.x,
            global.y + self.bar_pos.y,
            self.bar_size.x,
            self.bar_size.y,
        ];
        self.max_pos = self.value_step.mul_add(max, btn);
        self.min_pos = btn;
    }

    fn clamp_bar(&mut self, global: Vector3f) -> i32 {
        let axis = |s: &SliderState, p: f32| (((p - s.button_size) / s.value_step) + 0.5) as i32;
        let (max_pos, min_pos) = (self.max_pos, self.min_pos);
        let clamp = |p: &mut f32| {
            if *p > max_pos {
                *p = max_pos;
            }
            if *p < min_pos {
                *p = min_pos;
            }
        };
        let mut v = if self.vertical {
            clamp(&mut self.bar_pos.y);
            self.rect[1] = global.y + self.bar_pos.y;
            axis(self, self.bar_pos.y)
        } else {
            clamp(&mut self.bar_pos.x);
            self.rect[0] = global.x + self.bar_pos.x;
            axis(self, self.bar_pos.x)
        };
        if v > self.max_value {
            v = self.max_value;
        }
        if v < 0 {
            v = 0;
        }
        v
    }

    fn move_to(&mut self, v: i32) -> Option<i32> {
        if v != self.value {
            self.value = v;
            Some(v)
        } else {
            None
        }
    }

    /// `OnMouseMove`: the value to report in a `SliderMove` message.
    pub fn mouse_move(&mut self, local: Vector3f, global: Vector3f) -> Option<i32> {
        if !self.pressed {
            return None;
        }
        if self.vertical {
            self.bar_pos.y = local.y + self.rel_mouse.y;
        } else {
            self.bar_pos.x = local.x + self.rel_mouse.x;
        }
        let v = self.clamp_bar(global);
        self.move_to(v)
    }

    /// `OnMouseDown`: `(return value, SliderMove value)`.
    pub fn mouse_down(
        &mut self,
        val: i32,
        pos: Vector2f,
        local: Vector3f,
        global: Vector3f,
    ) -> (bool, Option<i32>) {
        if val & MOUSE_WHEEL_UP != 0 || val & MOUSE_WHEEL_DOWN != 0 {
            return (false, None);
        }
        let [rx, ry, rw, rh] = self.rect;
        if point_in_rect(pos, rx, ry, rw, rh) {
            self.pressed = true;
            self.rel_mouse = v2(self.bar_pos.x - local.x, self.bar_pos.y - local.y);
            return (true, None);
        }
        let mut step = self.click_add as f32 * self.value_step;
        if self.vertical {
            if local.y < self.bar_pos.y + rh {
                step = -step;
            }
            self.bar_pos.y += step;
        } else {
            if local.x < self.bar_pos.x + rw {
                step = -step;
            }
            self.bar_pos.x += step;
        }
        let v = self.clamp_bar(global);
        (true, self.move_to(v))
    }

    /// `OnMouseUp` and `OnLostFocus`: true when the bar properties must be updated.
    pub fn release(&mut self) -> bool {
        let was = self.pressed;
        self.pressed = false;
        was
    }
}

/// `OnUIArrowPress`: the arrow button (0 or 1) that is pressed, or -1.
pub fn slider_arrow_button(vertical: bool, arrow: i32) -> i32 {
    let (dec, inc) = if vertical {
        (UI_ARROW_UP, UI_ARROW_DOWN)
    } else {
        (UI_ARROW_LEFT, UI_ARROW_RIGHT)
    };
    if arrow == dec {
        0
    } else if arrow == inc {
        1
    } else {
        -1
    }
}

#[repr(C)]
#[derive(Clone, Copy, Default, Debug, PartialEq)]
pub struct SliderDraw {
    pub track_pos: Vector3f,
    pub track_size: Vector2f,
    pub bar_pos: Vector3f,
    pub bar_size: Vector2f,
}

pub fn slider_draw(st: &SliderState, size: Vector2f, global: Vector3f) -> SliderDraw {
    let btn = st.button_size;
    let (track_pos, track_size) = if st.vertical {
        (
            add3(global, v3(0.0, btn, 0.1)),
            v2(size.x, size.y - btn * 2.0),
        )
    } else {
        (
            add3(global, v3(btn, 0.0, 0.1)),
            v2(size.x - btn * 2.0, size.y),
        )
    };
    SliderDraw {
        track_pos,
        track_size,
        bar_pos: add3(global, st.bar_pos),
        bar_size: st.bar_size,
    }
}

/// `OnChangeSize`: positions and sizes of the two arrow buttons.
pub fn slider_buttons(vertical: bool, size: Vector2f, btn: f32) -> [(Vector3f, Vector2f); 2] {
    if vertical {
        [
            (v3(0.0, 0.0, 0.2), v2(size.x, btn)),
            (v3(0.0, size.y - btn, 0.2), v2(size.x, btn)),
        ]
    } else {
        [
            (v3(0.0, 0.0, 0.2), v2(btn, size.y)),
            (v3(size.x - btn, 0.0, 0.2), v2(btn, size.y)),
        ]
    }
}

// ---------------------------------------------------------------- tab frame

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct TabFrameState {
    pub first_visible: i32,
    pub last_visible: i32,
    pub button_size: f32,
    pub first_tab_offset: f32,
    pub tab_separation: f32,
    pub active_offset: f32,
    pub inactive_offset: f32,
    pub icon_padding: f32,
    pub text_left_padding: f32,
    pub text_right_padding: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Default, Debug, PartialEq)]
pub struct TabItem {
    pub visible: bool,
    pub set_pos: bool,
    pub pos_x: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Default, Debug, PartialEq)]
pub struct TabArrows {
    pub left_enabled: bool,
    pub right_enabled: bool,
    pub shown: bool,
}

impl TabFrameState {
    /// `ArrowButton_Pressed` before the visibility update: true when the first tab moved.
    pub fn arrow(&mut self, right: bool, tabs: i32) -> bool {
        let before = self.first_visible;
        if !right {
            if self.first_visible > 0 {
                self.first_visible -= 1;
            }
        } else if self.last_visible < tabs.wrapping_sub(1) {
            self.first_visible += 1;
        }
        before != self.first_visible
    }

    /// `UpdateTabVisibility`: fills one item per tab from the label widths.
    pub fn visibility(&mut self, size_x: f32, widths: &[f32], items: &mut [TabItem]) -> TabArrows {
        let n = widths.len().min(items.len());
        let tabs = n as i32;
        let mut x = self.first_tab_offset;
        for (i, it) in items.iter_mut().take(n).enumerate() {
            let i = i as i32;
            if i < self.first_visible {
                *it = TabItem {
                    visible: false,
                    set_pos: false,
                    pos_x: 0.0,
                };
            }
        }
        let first = self.first_visible.max(0) as usize;
        for (i, (it, w)) in items.iter_mut().zip(widths).enumerate().take(n).skip(first) {
            *it = TabItem {
                visible: true,
                set_pos: true,
                pos_x: x,
            };
            if x + *w < size_x - (self.button_size * 2.0 + 2.0) {
                self.last_visible = i as i32;
            }
            x += *w + self.tab_separation;
        }
        TabArrows {
            left_enabled: self.first_visible > 0,
            right_enabled: self.last_visible < tabs.wrapping_sub(1),
            shown: self.last_visible.wrapping_sub(self.first_visible) < tabs.wrapping_sub(1),
        }
    }

    pub fn label_pos_y(&self, on_top: bool) -> f32 {
        -if on_top {
            self.active_offset
        } else {
            self.inactive_offset
        }
    }

    /// `cWidgetTabLabel::OnChangeText`: label width; `icon_w` is the icon image width when present.
    pub fn label_width(&self, text_len: f32, borders_w: f32, icon_w: Option<f32>) -> f32 {
        let mut w = text_len + self.text_left_padding + self.text_right_padding + borders_w;
        if let Some(i) = icon_w {
            w += self.icon_padding.mul_add(2.0, i);
        }
        w
    }

    /// `cWidgetTabLabel::OnDraw`: text position.
    pub fn label_text_pos(&self, global: Vector3f, icon_w: Option<f32>) -> Vector3f {
        let mut p = add3(global, v3(self.text_left_padding, 2.0, 0.1));
        if let Some(i) = icon_w {
            p.x += self.icon_padding.mul_add(2.0, i);
        }
        p
    }

    /// `OnChangeSize`: arrow button positions.
    pub fn arrow_pos(&self, size_x: f32, i: i32, foreground_z: f32) -> Vector3f {
        v3(
            size_x - (self.button_size + 2.0) * (2 - i) as f32,
            0.0,
            foreground_z + 0.5,
        )
    }
}

/// `OnUIButtonPress`: the tab index to select, `None` when the button is not a page flip.
pub fn tab_page_target(val: i32, top_index: i32) -> Option<i32> {
    if val & 4 != 0 {
        Some(top_index.wrapping_sub(1))
    } else if val & 8 != 0 {
        Some(top_index.wrapping_add(1))
    } else {
        None
    }
}

#[repr(C)]
#[derive(Clone, Copy, Default, Debug, PartialEq)]
pub struct TabLayout {
    pub label_pos: Vector3f,
    pub frame_pos: Vector3f,
    pub frame_size: Vector2f,
}

/// `cWidgetTab::OnChangeSize`.
pub fn tab_layout(size: Vector2f, font_h: f32, active: f32) -> TabLayout {
    let text_h = font_h + 8.0;
    TabLayout {
        label_pos: v3(0.0, active, 0.1),
        frame_pos: v3(0.0, text_h - 4.0 + active, 0.1),
        frame_size: v2(size.x - 0.0, size.y - (text_h + active)),
    }
}

#[repr(C)]
#[derive(Clone, Copy, Default, Debug, PartialEq)]
pub struct TabInit {
    pub label_h: f32,
    pub frame_size: Vector2f,
    pub frame_pos: Vector3f,
}

/// `cWidgetTab::OnInit`.
pub fn tab_init(
    size: Vector2f,
    font_h: f32,
    active: f32,
    c0: Vector2f,
    c1: Vector2f,
    c2: Vector2f,
) -> TabInit {
    let label_h = font_h + 8.0;
    let corner = v2(c0.x + c1.x, c0.y + c2.y);
    TabInit {
        label_h,
        frame_size: v2(size.x - corner.x, size.y - (label_h + active + corner.y)),
        frame_pos: v3(c0.x, c0.y + label_h + active - c0.y, 0.0),
    }
}

/// `cWidgetTab::PointIsInside` after the clipping parent check.
pub fn tab_point_inside(
    p: Vector2f,
    label_visible: bool,
    label_pos: Vector3f,
    label_size: Vector2f,
    frame_pos: Vector3f,
    frame_size: Vector2f,
) -> bool {
    if p.x < frame_pos.x
        || p.x > frame_pos.x + frame_size.x
        || p.y < frame_pos.y
        || p.y > frame_pos.y + frame_size.y
    {
        !(!label_visible
            || p.x < label_pos.x
            || p.x > label_pos.x + label_size.x
            || p.y < label_pos.y
            || p.y > label_pos.y + label_size.y)
    } else {
        true
    }
}
