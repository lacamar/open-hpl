//! `cWidgetTextBox` editing logic: cursor, selection, word skip, clipboard decisions, character
//! filtering, numeric input and the visible-window math. The widget keeps the text object, the
//! font and every `cGuiSet` call; Rust works on a copy of the text for one event and reports the
//! `iWidget::SetText` side effects as [`Event`]s that C++ replays in order.

use super::widget::{MOUSE_LEFT, add3, v2};
use crate::math::v3;
use crate::string::{count_chars, sub};
use crate::types::{Vector2f, Vector3f};

pub const KEY_RETURN: i32 = 3;
pub const KEY_KP_ENTER: i32 = 86;
pub const KEY_A: i32 = 44;
pub const KEY_C: i32 = 46;
pub const KEY_X: i32 = 67;
pub const KEY_V: i32 = 65;
pub const KEY_LEFT: i32 = 91;
pub const KEY_RIGHT: i32 = 90;
pub const KEY_DELETE: i32 = 70;
pub const KEY_BACKSPACE: i32 = 0;
pub const KEY_HOME: i32 = 93;
pub const KEY_END: i32 = 94;
pub const MOD_CTRL: i32 = 1;
pub const MOD_SHIFT: i32 = 2;
pub const UI_BUTTON_PRIMARY: i32 = 1;

const DOT: u32 = b'.' as u32;
const MINUS: u32 = b'-' as u32;
const SPACE: u32 = b' ' as u32;

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct TextState {
    pub marker: i32,
    pub sel_end: i32,
    pub first_visible: i32,
    pub visible_size: i32,
    pub text_max_size: f32,
    pub max_chars: i32,
    pub max_size_neg: f32,
    pub can_edit: bool,
    pub numeric: bool,
    pub numeric_add: f32,
    pub numeric_value: f32,
    pub numeric_updated: bool,
    pub has_lower: bool,
    pub lower: f32,
    pub has_upper: bool,
    pub upper: f32,
    pub decimals: i32,
    pub limit_enabled: bool,
    pub limit_max: u32,
    pub limit_min: u32,
    pub changed_since_enter: bool,
    pub force_enter: bool,
    pub enter_on_lost_focus: bool,
    pub pressed: bool,
    pub got_focus_recently: bool,
}

impl TextState {
    pub fn new(numeric: bool) -> Self {
        TextState {
            marker: -1,
            sel_end: -1,
            first_visible: 0,
            visible_size: 0,
            text_max_size: 0.0,
            max_chars: -1,
            max_size_neg: 0.0,
            can_edit: true,
            numeric,
            numeric_add: 0.5,
            numeric_value: 0.0,
            numeric_updated: true,
            has_lower: false,
            lower: 0.0,
            has_upper: false,
            upper: 0.0,
            decimals: -1,
            limit_enabled: false,
            limit_max: 0,
            limit_min: 0,
            changed_since_enter: false,
            force_enter: false,
            enter_on_lost_focus: true,
            pressed: false,
            got_focus_recently: false,
        }
    }
}

/// Per-call snapshot of the font and skin values the logic reads.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct TextEnv {
    pub font_first: i32,
    pub font_last: i32,
    pub font_size: Vector2f,
    pub corner0: Vector2f,
    pub default_decimals: i32,
}

/// Side effects of one event, replayed by C++ in order.
#[derive(Clone, Debug, PartialEq)]
pub enum Event {
    /// `iWidget::SetText` minus the `OnChangeText` call; `nested` is the truncating `SetText`
    /// issued from inside `OnChangeText`.
    SetText {
        text: Vec<u32>,
        nested: Option<Vec<u32>>,
    },
    /// `ProcessMessage(eGuiMessage_TextBoxEnter)`.
    Enter,
}

pub trait TextHost {
    /// Advance of the font glyph `num`, `None` for a missing glyph.
    fn glyph(&mut self, num: i32) -> Option<f32>;
    /// `GetLength(font size, text.c_str())`.
    fn length(&mut self, text: &[u32]) -> f32;
    /// `WorldToLocalPosition(mouse).x`; only asked for when the handler needs it.
    fn local_x(&mut self) -> f32;
    fn clipboard(&mut self) -> Vec<u32>;
    fn copy(&mut self, text: &[u32]);
    /// `RoundFloatToDecimals(ToFloat(text), decimals)`.
    fn to_float(&mut self, text: &[u32], decimals: i32) -> f32;
    /// `cString::ToStringW(value, decimals, true)`.
    fn float_text(&mut self, v: f32, decimals: i32) -> Vec<u32>;
    fn emit(&mut self, e: Event);
}

/// What a handler returns to the message dispatcher.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Ret {
    False,
    True,
    /// The result of the `TextBoxEnter` message.
    Enter,
}

#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct KeyOut {
    pub reset_marker_anim: bool,
}

fn at(text: &[u32], i: i32) -> u32 {
    usize::try_from(i)
        .ok()
        .and_then(|i| text.get(i))
        .copied()
        .unwrap_or(0)
}

/// `cString::SubW`; the start is never negative where the widget calls it.
pub fn sub_w(text: &[u32], start: i32, count: i32) -> Vec<u32> {
    sub(text, start, count, true).unwrap_or_default().to_vec()
}

fn cat(parts: &[&[u32]]) -> Vec<u32> {
    parts.iter().flat_map(|p| p.iter().copied()).collect()
}

fn sel_range(st: &TextState) -> (i32, i32) {
    (st.marker.min(st.sel_end), st.marker.max(st.sel_end))
}

pub struct Edit<'a, H: TextHost> {
    pub st: &'a mut TextState,
    pub env: TextEnv,
    pub text: Vec<u32>,
    pub illegal: &'a [u32],
    pub host: &'a mut H,
}

impl<H: TextHost> Edit<'_, H> {
    fn len(&self) -> i32 {
        self.text.len() as i32
    }

    fn decimals(&self) -> i32 {
        if self.st.decimals < 0 {
            self.env.default_decimals
        } else {
            self.st.decimals
        }
    }

    fn advance(&mut self, ch: u32) -> Option<f32> {
        self.host
            .glyph(ch.wrapping_sub(self.env.font_first as u32) as i32)
    }

    fn skip_index(&self, i: i32) -> bool {
        i < self.env.font_first || i > self.env.font_last
    }

    /// `GetLastCharInSize`; the font range test compares the index, not the character.
    pub fn last_char_in_size(&mut self, start: i32, max: f32, add: f32) -> i32 {
        let mut pos = self.len();
        let mut length = 0.0f32;
        let mut i = start.max(0);
        while i < self.len() {
            if !self.skip_index(i) {
                let ch = at(&self.text, i);
                if let Some(adv) = self.advance(ch) {
                    length = adv.mul_add(self.env.font_size.x, length);
                    if length + add >= max {
                        pos = i;
                        break;
                    }
                }
            }
            i += 1;
        }
        pos
    }

    /// `GetFirstCharInSize`; reads past the end of the text see a NUL character.
    pub fn first_char_in_size(&mut self, start: i32, max: f32, add: f32) -> i32 {
        let mut pos = 0;
        let mut length = 0.0f32;
        let mut i = start;
        while i >= 0 {
            if !self.skip_index(i) {
                let ch = at(&self.text, i);
                if let Some(adv) = self.advance(ch) {
                    length = adv.mul_add(self.env.font_size.x, length);
                    if length + add >= max {
                        pos = i;
                        break;
                    }
                }
            }
            i -= 1;
        }
        pos
    }

    pub fn world_to_char_pos(&mut self) -> i32 {
        let local_x = self.host.local_x();
        let text_pos = local_x - self.env.corner0.x + 3.0;
        if text_pos > 0.0 {
            self.last_char_in_size(self.st.first_visible, text_pos, 3.0)
        } else {
            self.st.first_visible
        }
    }

    pub fn char_to_local_pos(&mut self, c: i32) -> f32 {
        let mut pos = -2.0;
        if c > 0 && c.wrapping_sub(self.st.first_visible) > 0 {
            let part = sub_w(
                &self.text,
                self.st.first_visible,
                c.wrapping_sub(self.st.first_visible),
            );
            pos = self.host.length(&part);
        }
        pos
    }

    fn is_illegal(&self, c: u32) -> bool {
        if self.illegal.contains(&c) {
            return true;
        }
        self.st.limit_enabled && (c > self.st.limit_max || c < self.st.limit_min)
    }

    fn some_illegal(&self) -> bool {
        !self.illegal.is_empty() || self.st.limit_enabled
    }

    fn text_updated(&mut self) {
        self.st.changed_since_enter = true;
        if self.st.numeric {
            self.st.numeric_updated = true;
        }
    }

    fn recompute_visible(&mut self) {
        self.st.visible_size = if self.text.is_empty() {
            0
        } else {
            let first = self.st.first_visible;
            self.last_char_in_size(first, self.st.text_max_size, 0.0)
                .wrapping_sub(first)
        };
    }

    fn truncation(&self) -> Option<Vec<u32>> {
        (self.st.max_chars >= 0 && self.len() > self.st.max_chars)
            .then(|| sub_w(&self.text, 0, self.st.max_chars))
    }

    fn clamp_to_max(&mut self) {
        let m = self.st.max_chars;
        if self.st.sel_end >= m {
            self.st.sel_end = m - 1;
        }
        if self.st.marker >= m {
            self.st.marker = m - 1;
        }
    }

    /// `OnChangeText`; `in_set_text` is `mbTextChanged` of the caller.
    pub fn on_change_text(&mut self, in_set_text: bool) {
        self.recompute_visible();
        if let Some(t) = self.truncation() {
            self.set_text(t);
            self.clamp_to_max();
        } else if in_set_text && self.st.numeric {
            self.st.numeric_updated = true;
        }
    }

    /// `iWidget::SetText` with the virtual `OnChangeText` folded in.
    pub fn set_text(&mut self, new: Vec<u32>) {
        if new == self.text {
            return;
        }
        self.text = new;
        let text = self.text.clone();
        self.recompute_visible();
        let mut nested = None;
        if let Some(t) = self.truncation() {
            self.text = t.clone();
            self.recompute_visible();
            nested = Some(t);
            self.clamp_to_max();
        }
        if self.st.numeric {
            self.st.numeric_updated = true;
        }
        self.host.emit(Event::SetText { text, nested });
    }

    pub fn set_marker_pos(&mut self, pos: i32) {
        let st_len = self.len();
        self.st.marker = pos;
        if self.st.marker < 0 {
            self.st.marker = 0;
        }
        if self.st.marker > st_len && st_len > 0 {
            self.st.marker = st_len;
        }
        let m = self.st.marker;
        if m > self.st.first_visible.wrapping_add(self.st.visible_size) {
            self.st.first_visible = self.first_char_in_size(m, self.st.text_max_size, 0.0) + 1;
            if st_len <= 1 {
                self.st.first_visible = 0;
            }
            self.on_change_text(false);
        } else if m < self.st.first_visible {
            self.st.first_visible = m;
            self.on_change_text(false);
        }
    }

    pub fn set_max_text_length(&mut self, len: i32) {
        if self.st.max_chars == len {
            return;
        }
        self.st.max_chars = len;
        if len >= 0 && self.len() > len {
            let t = sub_w(&self.text, 0, len);
            self.set_text(t);
            self.clamp_to_max();
            self.on_change_text(false);
        }
    }

    pub fn set_selected_text(&mut self, start: i32, count: i32) {
        let len = self.len();
        let mut start = start;
        if start < 0 {
            start = 0;
        }
        if start > len {
            start = len;
        }
        self.st.sel_end = start;
        let count = if count == -1 {
            len.wrapping_sub(self.st.sel_end)
        } else {
            count
        };
        self.set_marker_pos(self.st.sel_end.wrapping_add(count));
    }

    pub fn numeric_value(&mut self) -> f32 {
        if self.st.numeric_updated {
            self.st.numeric_updated = false;
            let d = self.decimals();
            self.st.numeric_value = self.host.to_float(&self.text, d);
        }
        self.st.numeric_value
    }

    pub fn set_numeric_value(&mut self, v: f32) {
        if !self.st.numeric {
            return;
        }
        let mut v = v;
        if self.st.has_lower && v < self.st.lower {
            v = self.st.lower;
        }
        if self.st.has_upper && v > self.st.upper {
            v = self.st.upper;
        }
        if self.st.numeric_value == v {
            return;
        }
        self.st.numeric_value = v;
        let d = self.decimals();
        let t = self.host.float_text(v, d);
        self.set_text(t);
        self.text_updated();
        self.st.numeric_updated = false;
    }

    /// `OnEnter`: true when the `TextBoxEnter` message was issued (its result is the return value).
    pub fn on_enter(&mut self) -> bool {
        if self.st.numeric {
            let v = self.numeric_value();
            let d = self.decimals();
            let t = self.host.float_text(v, d);
            self.set_text(t);
            self.st.numeric_updated = false;
        }
        if self.st.changed_since_enter || self.st.force_enter {
            self.st.changed_since_enter = false;
            self.host.emit(Event::Enter);
            return true;
        }
        false
    }

    fn enter_ret(&mut self) -> Ret {
        if self.on_enter() {
            Ret::Enter
        } else {
            Ret::True
        }
    }

    /// `ButtonPressed`; `which` is 0 for the up button, 1 for the down button, -1 otherwise.
    pub fn button_pressed(&mut self, which: i32, enabled: bool) -> Ret {
        if !enabled {
            return Ret::False;
        }
        let add = match which {
            0 => self.st.numeric_add,
            1 => -self.st.numeric_add,
            _ => 0.0,
        };
        if add == 0.0 {
            return Ret::True;
        }
        let v = self.numeric_value() + add;
        self.set_numeric_value(v);
        self.enter_ret()
    }

    pub fn mouse_move(&mut self) -> Ret {
        if self.st.pressed {
            let pos = self.world_to_char_pos();
            if pos != self.st.marker {
                if self.st.sel_end == -1 {
                    self.st.sel_end = self.st.marker;
                }
                self.set_marker_pos(pos);
            }
        }
        Ret::True
    }

    /// `OnMouseDown`; the second value asks C++ to take the attention.
    pub fn mouse_down(&mut self, val: i32) -> (Ret, bool) {
        if val & MOUSE_LEFT == 0 || !self.st.can_edit {
            return (Ret::False, false);
        }
        let p = self.world_to_char_pos();
        self.set_marker_pos(p);
        self.st.sel_end = -1;
        self.st.pressed = true;
        (Ret::True, true)
    }

    /// `OnMouseUp` up to the attention call: `(saved marker, saved selection end)` when C++ must
    /// restore the attention and call [`Edit::mouse_up_restore`].
    pub fn mouse_up(&mut self, val: i32) -> Option<(i32, i32)> {
        if val & MOUSE_LEFT == 0 || !self.st.can_edit {
            return None;
        }
        if self.st.got_focus_recently {
            let pos = self.world_to_char_pos();
            if self.st.sel_end == -1 || self.st.sel_end == pos {
                let l = self.len();
                self.set_marker_pos(l);
                self.st.sel_end = 0;
            }
            self.st.got_focus_recently = false;
        }
        self.st.pressed = false;
        let saved = (self.st.marker, self.st.sel_end);
        self.st.got_focus_recently = false;
        Some(saved)
    }

    pub fn mouse_up_restore(&mut self, marker: i32, sel_end: i32) {
        self.st.got_focus_recently = false;
        self.set_marker_pos(marker);
        self.st.sel_end = sel_end;
    }

    pub fn double_click(&mut self, val: i32) {
        if val & MOUSE_LEFT == 0 || !self.st.can_edit {
            return;
        }
        let p = self.world_to_char_pos();
        self.set_marker_pos(p);
        let len = self.len();
        if self.st.marker != len && at(&self.text, self.st.marker) == SPACE {
            return;
        }
        self.st.sel_end = self.st.marker;
        while self.st.sel_end > 0 {
            if at(&self.text, self.st.sel_end - 1) == SPACE {
                break;
            }
            self.st.sel_end -= 1;
        }
        let mut i = self.st.marker;
        while i < len {
            if at(&self.text, i) == SPACE || i == len - 1 {
                if i == len - 1 {
                    self.set_marker_pos(i + 1);
                } else {
                    self.set_marker_pos(i);
                }
                break;
            }
            i += 1;
        }
        self.st.pressed = false;
    }

    pub fn got_focus(&mut self) {
        self.st.got_focus_recently = true;
    }

    pub fn got_tab_focus(&mut self) {
        self.st.marker = self.len();
        self.st.sel_end = 0;
        self.st.got_focus_recently = false;
    }

    pub fn lost_focus(&mut self) {
        if self.st.enter_on_lost_focus {
            self.on_enter();
        }
        self.st.pressed = false;
        self.st.marker = -1;
        self.st.sel_end = -1;
    }

    /// `OnUIButtonPress` before the pop-up keyboard is created: true when C++ must create it.
    pub fn ui_button(&mut self, has_focus: bool, val: i32) -> bool {
        if !self.st.can_edit {
            return false;
        }
        if has_focus && val == UI_BUTTON_PRIMARY {
            self.st.sel_end = 0;
            let l = self.len();
            self.set_marker_pos(l);
            return true;
        }
        false
    }

    pub fn ui_keyboard_open(&mut self) {
        self.st.sel_end = 0;
        let l = self.len();
        self.set_marker_pos(l);
    }

    fn replace(&mut self, start: i32, end: i32, middle: &[u32]) {
        let t = cat(&[
            &sub_w(&self.text, 0, start),
            middle,
            &sub_w(&self.text, end, -1),
        ]);
        self.set_text(t);
    }

    fn insert_at(&mut self, pos: i32, c: &[u32]) {
        self.replace(pos, pos, c);
    }

    fn paste(&mut self) {
        let mut extra = self.host.clipboard();
        if extra.is_empty() {
            return;
        }
        if self.some_illegal() {
            extra.retain(|&c| !self.is_illegal(c));
        }
        let extra_len = extra.len() as i32;
        let (start, end) = sel_range(self.st);
        let select_size = end - start;
        let max = self.st.max_chars;
        if self.st.sel_end < 0 {
            if max == -1 || self.len().wrapping_add(extra_len) <= max {
                let m = self.st.marker;
                self.insert_at(m, &extra);
                self.set_marker_pos(self.st.marker.wrapping_add(extra_len));
            }
        } else if max < 0
            || extra_len <= select_size
            || extra_len.wrapping_add(self.len()).wrapping_sub(select_size) <= max
        {
            self.replace(start, end, &extra);
            self.st.sel_end = -1;
            self.set_marker_pos(start.wrapping_add(extra_len));
        }
        self.text_updated();
    }

    fn ctrl_key(&mut self, key: i32) -> Ret {
        let (start, end) = sel_range(self.st);
        match key {
            KEY_A => {
                self.st.sel_end = 0;
                self.st.marker = self.len();
            }
            KEY_C => {
                if self.st.sel_end >= 0 {
                    let t = sub_w(&self.text, start, end - start);
                    self.host.copy(&t);
                }
            }
            KEY_X => {
                if self.st.sel_end >= 0 {
                    let t = sub_w(&self.text, start, end - start);
                    self.host.copy(&t);
                    self.replace(start, end, &[]);
                    self.st.sel_end = -1;
                    self.text_updated();
                }
            }
            KEY_V => self.paste(),
            _ => return Ret::False,
        }
        Ret::True
    }

    fn delete_key(&mut self, key: i32) {
        if self.st.sel_end >= 0 {
            let (start, end) = sel_range(self.st);
            self.replace(start, end, &[]);
            self.st.sel_end = -1;
            self.set_marker_pos(start);
            self.text_updated();
        } else if key == KEY_DELETE {
            let m = self.st.marker;
            let t = cat(&[
                &sub_w(&self.text, 0, m),
                &sub_w(&self.text, m.wrapping_add(1), -1),
            ]);
            self.set_text(t);
            self.text_updated();
        } else if self.st.marker != 0 {
            let m = self.st.marker;
            let t = cat(&[&sub_w(&self.text, 0, m - 1), &sub_w(&self.text, m, -1)]);
            self.set_text(t);
            self.set_marker_pos(self.st.marker - 1);
            self.text_updated();
        }
    }

    fn numeric_char(&mut self, u: u32) {
        let m = self.st.marker;
        if self.st.sel_end < 0 {
            if u == MINUS {
                if m == 0 && at(&self.text, 0) != MINUS {
                    let t = cat(&[&[u], &sub_w(&self.text, 0, -1)]);
                    self.set_text(t);
                    self.set_marker_pos(self.st.marker + 1);
                    self.text_updated();
                }
            } else if u == DOT {
                if self.st.decimals != 0 && count_chars(&self.text, &[DOT]) == 0 {
                    self.insert_at(m, &[u]);
                    self.set_marker_pos(self.st.marker + 1);
                    self.text_updated();
                }
            } else {
                self.insert_at(m, &[u]);
                self.set_marker_pos(self.st.marker + 1);
                self.text_updated();
            }
        } else {
            let (start, end) = sel_range(self.st);
            if u == MINUS {
                if start == 0 {
                    let t = cat(&[&[u], &sub_w(&self.text, end, -1)]);
                    self.set_text(t);
                    self.text_updated();
                }
            } else if u == DOT {
                let dots = count_chars(&self.text, &[DOT]);
                let dot_pos = self
                    .text
                    .iter()
                    .rposition(|&c| c == DOT)
                    .map_or(-1, |p| p as i32);
                if dots == 0 || (dots == 1 && dot_pos >= start && dot_pos <= end) {
                    self.replace(start, end, &[u]);
                    self.text_updated();
                }
            } else {
                self.replace(start, end, &[u]);
                self.text_updated();
            }
            self.st.sel_end = -1;
            self.set_marker_pos(start + 1);
        }
    }

    fn plain_char(&mut self, u: u32) {
        if self.st.sel_end < 0 {
            if self.st.max_chars == -1 || self.len() < self.st.max_chars {
                let m = self.st.marker;
                self.insert_at(m, &[u]);
                self.set_marker_pos(self.st.marker + 1);
                self.text_updated();
            }
        } else {
            let (start, end) = sel_range(self.st);
            self.replace(start, end, &[u]);
            self.st.sel_end = -1;
            self.set_marker_pos(start + 1);
            self.text_updated();
        }
    }

    fn char_key(&mut self, u: u32) {
        if self.is_illegal(u) {
            return;
        }
        let in_font = |s: &mut Self| {
            u >= s.env.font_first as u32 && u <= s.env.font_last as u32 && s.advance(u).is_some()
        };
        if self.st.numeric {
            let digit = (b'0' as u32..=b'9' as u32).contains(&u);
            if digit || ((u == DOT || u == MINUS) && self.advance(u).is_some()) {
                self.numeric_char(u);
            }
        } else if in_font(self) {
            self.plain_char(u);
        }
    }

    /// `OnKeyPress`.
    pub fn key_press(&mut self, key: i32, modifier: i32, unicode: u32) -> (Ret, KeyOut) {
        let mut out = KeyOut::default();
        if !self.st.can_edit || self.st.marker < 0 {
            return (Ret::False, out);
        }
        out.reset_marker_anim = true;
        let shift = modifier & MOD_SHIFT != 0;
        let ret = if key == KEY_RETURN || key == KEY_KP_ENTER {
            self.enter_ret()
        } else if modifier & MOD_CTRL != 0 {
            self.ctrl_key(key)
        } else if key == KEY_LEFT || key == KEY_RIGHT {
            if shift && self.st.sel_end == -1 {
                self.st.sel_end = self.st.marker;
            }
            let d = if key == KEY_LEFT { -1 } else { 1 };
            self.set_marker_pos(self.st.marker.wrapping_add(d));
            if !shift {
                self.st.sel_end = -1;
            }
            Ret::True
        } else if key == KEY_DELETE || key == KEY_BACKSPACE {
            self.delete_key(key);
            Ret::True
        } else if key == KEY_HOME || key == KEY_END {
            if shift {
                if self.st.sel_end == -1 {
                    self.st.sel_end = self.st.marker;
                }
            } else {
                self.st.sel_end = -1;
            }
            let to = if key == KEY_HOME { 0 } else { self.len() };
            self.set_marker_pos(to);
            Ret::True
        } else {
            self.char_key(unicode);
            Ret::True
        };
        (ret, out)
    }
}

/// `OnChangeSize`: `(size.y, text max size)`.
pub fn layout(
    st: &TextState,
    size_x: f32,
    b0: Vector2f,
    b1: Vector2f,
    b2: Vector2f,
    font_h: f32,
    button_w: f32,
) -> (f32, f32) {
    let h = b0.y + b2.y + font_h + 4.0;
    let w = size_x - b0.x - b1.x - 6.0 - st.max_size_neg - button_w;
    (h, w)
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct TextDraw {
    pub text_start: i32,
    pub text_len: i32,
    pub text_pos: Vector3f,
    pub has_marker: bool,
    pub marker_pos: Vector3f,
    pub marker_size: Vector2f,
    pub has_selection: bool,
    pub sel_pos: Vector3f,
    pub sel_size: Vector2f,
    pub hl_start: i32,
    pub hl_len: i32,
    pub hl_pos: Vector3f,
}

impl<H: TextHost> Edit<'_, H> {
    /// `OnDraw` up to the borders.
    pub fn draw(&mut self, global: Vector3f) -> TextDraw {
        let add = add3(
            v3(3.0, 2.0, 0.2),
            v3(self.env.corner0.x, self.env.corner0.y, 0.0),
        );
        let base = add3(global, add);
        let mut d = TextDraw {
            text_start: self.st.first_visible,
            text_len: self.st.visible_size,
            text_pos: base,
            ..TextDraw::default()
        };
        if self.st.marker >= 0 {
            let marker_pos = self.char_to_local_pos(self.st.marker);
            d.has_marker = true;
            d.marker_pos = add3(base, v3(marker_pos, 0.0, 0.1));
            d.marker_size = v2(2.0, self.env.font_size.y);
            if self.st.sel_end >= 0 {
                let (start, end) = sel_range(self.st);
                let first = self.st.first_visible;
                let hl_start = first.max(start);
                let hl_end = first.wrapping_add(self.st.visible_size).min(end);
                let sel_end = self.char_to_local_pos(self.st.sel_end);
                let mut pos = if sel_end < marker_pos {
                    sel_end
                } else {
                    marker_pos
                };
                let mut end_pos = if sel_end > marker_pos {
                    sel_end
                } else {
                    marker_pos
                };
                if pos < 0.0 {
                    pos = 0.0;
                }
                if end_pos > self.st.text_max_size {
                    end_pos = self.st.text_max_size;
                }
                d.has_selection = true;
                d.sel_pos = add3(base, v3(pos, 0.0, 0.01));
                d.sel_size = v2(end_pos - pos, self.env.font_size.y);
                d.hl_start = hl_start;
                d.hl_len = hl_end.wrapping_sub(hl_start);
                d.hl_pos = add3(base, v3(pos, 0.0, 0.02));
            }
        }
        d
    }
}
