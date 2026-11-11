//! `iWidgetListBoxBase`, `cWidgetListBox` and `cWidgetComboBox` logic: cursor, selection plans,
//! scroll window and menu geometry. Items, sliders, callbacks and drawing stay in C++.

use super::widget::{MOUSE_WHEEL_DOWN, MOUSE_WHEEL_UP, UI_BUTTON_PRIMARY, add3, v2};
pub use super::widgets_slider::{UI_ARROW_DOWN, UI_ARROW_UP};
use crate::math::v3;
use crate::types::{Vector2f, Vector3f};

pub const KEY_UP: i32 = 88;
pub const KEY_DOWN: i32 = 89;
pub const KEY_SPACE: i32 = 6;
pub const MOD_CTRL: i32 = 1;
pub const MOD_SHIFT: i32 = 2;
pub const UI_BUTTON_SECONDARY: i32 = 2;

pub const SEL_SELECT: i32 = 0;
pub const SEL_TOGGLE: i32 = 1;
pub const SEL_FROM_LAST_CURSOR: i32 = 2;
pub const SEL_FROM_ANCHOR: i32 = 3;

/// Plain integer division as the aarch64 `sdiv` does it: zero divisor gives zero.
fn idiv(a: i32, b: i32) -> i32 {
    if b == 0 { 0 } else { a.wrapping_div(b) }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct ListState {
    pub first_item: i32,
    pub max_items: i32,
    pub cursor_pos: i32,
    pub last_cursor_pos: i32,
    pub anchor_pos: i32,
    pub num_clicks: i32,
    pub num_blank_rows: i32,
    pub num_items: i32,
    pub locked: bool,
    pub allow_multi: bool,
}

impl ListState {
    pub fn new() -> Self {
        ListState {
            first_item: 0,
            max_items: 1,
            cursor_pos: 0,
            last_cursor_pos: 0,
            anchor_pos: -1,
            num_clicks: 0,
            num_blank_rows: 0,
            num_items: 0,
            locked: false,
            allow_multi: false,
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct ListUpdate {
    pub big: bool,
    pub bar_value_size: i32,
    pub max_value: i32,
}

/// What C++ does to its slider after a cursor step: `SetValue(value)` when `set`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct SliderSet {
    pub set: bool,
    pub value: i32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct SelAction {
    /// 0 nothing, 1 set the item at `idx` to `flag`, 2 select a range.
    pub kind: i32,
    pub flag: bool,
    pub start: i32,
    pub end: i32,
}

/// Visit plan of `SelectRange`: `count` items from `first` by `step`, then `end` when valid.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct RangePlan {
    pub first: i32,
    pub count: i32,
    pub step: i32,
    pub end_valid: bool,
}

pub fn range_plan(start: i32, end: i32, items: i32) -> RangePlan {
    let (start, end, n) = (i64::from(start), i64::from(end), i64::from(items));
    let end_valid = (0..n).contains(&end);
    let (first, count, step) = if start < end {
        let lo = start.max(0);
        let hi = (end - 1).min(n - 1);
        (lo, (hi - lo + 1).max(0), 1)
    } else {
        let hi = start.min(n - 1);
        let lo = (end + 1).max(0);
        (hi, (hi - lo + 1).max(0), -1)
    };
    RangePlan {
        first: first as i32,
        count: count as i32,
        step,
        end_valid,
    }
}

impl ListState {
    /// `UpdateProperties` numbers.
    pub fn update(&mut self, size_y: f32, font_y: f32, items: i32) -> ListUpdate {
        self.num_items = items;
        self.max_items = ((size_y / (font_y + 2.0)) as i32).wrapping_sub(1);
        if items > self.max_items {
            ListUpdate {
                big: true,
                bar_value_size: self.max_items,
                max_value: items.wrapping_sub(self.max_items),
            }
        } else {
            ListUpdate {
                big: false,
                bar_value_size: 1,
                max_value: 0,
            }
        }
    }

    /// `SetCursorPos` up to the first slider call.
    pub fn cursor_step1(&mut self, pos: i32, move_list: bool, clamp: bool) -> SliderSet {
        let mut pos = pos;
        if clamp {
            if pos < 0 {
                pos = 0;
            }
            if pos >= self.num_items {
                pos = self.num_items.wrapping_sub(1);
            }
        } else if pos < 0 || pos >= self.num_items {
            pos = -1;
        }
        self.last_cursor_pos = self.cursor_pos;
        self.cursor_pos = pos;
        if move_list && self.cursor_pos >= self.first_item.wrapping_add(self.max_items) {
            self.first_item = self.cursor_pos.wrapping_sub(self.max_items).wrapping_add(1);
            return SliderSet {
                set: true,
                value: self.first_item,
            };
        }
        SliderSet::default()
    }

    /// `SetCursorPos` after the first slider call (whose callback may have moved `first_item`).
    pub fn cursor_step2(&mut self, move_list: bool) -> SliderSet {
        if move_list && self.cursor_pos < self.first_item && self.cursor_pos >= 0 {
            self.first_item = self.cursor_pos;
            return SliderSet {
                set: true,
                value: self.cursor_pos,
            };
        }
        SliderSet::default()
    }

    /// `SetSelectedItem` after the optional cursor move.
    pub fn select_action(
        &self,
        ty: i32,
        idx: i32,
        item_exists: bool,
        item_selected: bool,
    ) -> SelAction {
        let set = |flag| SelAction {
            kind: i32::from(item_exists),
            flag,
            ..SelAction::default()
        };
        if !self.allow_multi {
            return set(true);
        }
        match ty {
            SEL_SELECT => set(true),
            SEL_TOGGLE => set(!item_selected),
            SEL_FROM_LAST_CURSOR => SelAction {
                kind: 2,
                start: self.last_cursor_pos,
                end: idx,
                ..SelAction::default()
            },
            SEL_FROM_ANCHOR => SelAction {
                kind: 2,
                start: self.anchor_pos,
                end: idx,
                ..SelAction::default()
            },
            _ => SelAction::default(),
        }
    }
}

/// `GetMultiSelection`: the C++ loop never ends for a negative index; that case gives -1.
pub fn multi_selection(sel: &[i32], idx: i32) -> i32 {
    usize::try_from(idx)
        .ok()
        .and_then(|i| sel.get(i))
        .copied()
        .unwrap_or(-1)
}

/// `UpdateProperties`: drops selected indices at or past the item count; returns the new length.
pub fn prune_selection(sel: &mut [i32], items: i32) -> usize {
    let mut n = 0;
    for i in 0..sel.len() {
        let v = sel.get(i).copied().unwrap_or(0);
        if v < items {
            if let Some(slot) = sel.get_mut(n) {
                *slot = v;
            }
            n += 1;
        }
    }
    n
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct ListMouse {
    /// False when the click is above the first row and the handler returns false.
    pub proceed: bool,
    pub index: i32,
    pub ty: i32,
    pub clear: bool,
    pub cursor: SliderSet,
}

impl ListState {
    /// `OnMouseDown` wheel: 0 up, 1 down, -1 neither.
    pub fn wheel(val: i32) -> i32 {
        if val == MOUSE_WHEEL_UP {
            0
        } else if val == MOUSE_WHEEL_DOWN {
            1
        } else {
            -1
        }
    }

    /// `OnMouseDown` after the wheel test, up to the item lookup; the slider call of the cursor
    /// move is in `cursor`.
    pub fn mouse_down(&mut self, local_y: f32, font_y: f32, modifier: i32) -> ListMouse {
        let mut out = ListMouse::default();
        let row = ((local_y - 2.0) / (font_y + 2.0)) as i32;
        let sel = row.wrapping_sub(self.num_blank_rows);
        if sel < 0 {
            return out;
        }
        let sel = sel.wrapping_add(self.first_item);
        let ctrl = modifier & MOD_CTRL != 0;
        let shift = modifier & MOD_SHIFT != 0;
        out.clear = !ctrl;
        out.ty = if shift {
            SEL_FROM_ANCHOR
        } else if ctrl {
            SEL_TOGGLE
        } else {
            SEL_SELECT
        };
        out.cursor = self.cursor_step1(sel, true, false);
        out.proceed = true;
        out.index = sel;
        out
    }

    /// `OnMouseDown` after the cursor move: `(item index to select)`.
    pub fn mouse_down_finish(&mut self, shift: bool, sel: i32) -> i32 {
        if !shift {
            self.anchor_pos = self.cursor_pos;
        }
        if sel == self.last_cursor_pos {
            self.num_clicks = self.num_clicks.wrapping_add(1);
        } else {
            self.num_clicks = 1;
        }
        self.cursor_pos
    }

    /// `OnMouseDoubleClick`: `(return value, fire SelectionDoubleClick)`.
    pub fn double_click(&mut self, local_x: f32, size_x: f32, slider_w: f32) -> (bool, bool) {
        if local_x > size_x - slider_w {
            return (false, false);
        }
        if self.cursor_pos != -1 && self.num_clicks >= 2 {
            self.num_clicks = 0;
            return (true, true);
        }
        (true, false)
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct ListKey {
    pub handled: bool,
    pub arrow: bool,
    pub cursor: SliderSet,
    pub select: bool,
    pub ty: i32,
    pub clear: bool,
}

impl ListState {
    /// `OnKeyPress`: the cursor move for Up and Down, then the selection to make.
    pub fn key_press(&mut self, key: i32, modifier: i32) -> ListKey {
        let ctrl = modifier & MOD_CTRL != 0;
        let mut out = ListKey::default();
        if key == KEY_UP || key == KEY_DOWN {
            let d = if key == KEY_UP { -1 } else { 1 };
            out.cursor = self.cursor_step1(self.cursor_pos.wrapping_add(d), true, true);
            out.handled = true;
            out.arrow = true;
            return out;
        }
        if key == KEY_SPACE {
            out.handled = true;
            out.select = true;
            out.ty = if ctrl { SEL_TOGGLE } else { SEL_SELECT };
            out.clear = !ctrl;
        }
        out
    }

    /// `OnKeyPress` for Up/Down after the cursor sliders ran.
    pub fn key_press_finish(&mut self, modifier: i32) -> ListKey {
        let ctrl = modifier & MOD_CTRL != 0;
        let shift = modifier & MOD_SHIFT != 0;
        let mut out = ListKey {
            handled: true,
            ..ListKey::default()
        };
        if !ctrl && !shift {
            self.anchor_pos = self.cursor_pos;
            out.select = true;
            out.ty = SEL_SELECT;
            out.clear = true;
        } else if shift {
            out.select = true;
            out.ty = SEL_FROM_ANCHOR;
            out.clear = !ctrl;
        }
        out
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct ListArrow {
    /// -1 not handled by this widget (returns false), 1 continue.
    pub handled: bool,
    pub cursor: SliderSet,
    pub unlock: bool,
}

impl ListState {
    /// `OnUIArrowPress` up to the selection call.
    pub fn ui_arrow(&mut self, arrow: i32) -> ListArrow {
        let mut out = ListArrow::default();
        if !self.locked {
            return out;
        }
        if arrow == UI_ARROW_UP {
            out.cursor = self.cursor_step1(self.cursor_pos.wrapping_sub(1), true, true);
        } else if arrow == UI_ARROW_DOWN {
            let old = self.cursor_pos;
            out.cursor = self.cursor_step1(self.cursor_pos.wrapping_add(1), true, true);
            out.unlock = old == self.cursor_pos;
        } else {
            return out;
        }
        out.handled = true;
        out
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct ListButton {
    /// -1 continue with the Primary sequence, 0 return false, 1 return true.
    pub ret: i32,
    pub init_select: bool,
    pub check_locked: bool,
}

impl ListState {
    /// `OnUIButtonPress`.
    pub fn ui_button(&mut self, has_focus: bool, val: i32, selected: i32) -> ListButton {
        if !has_focus {
            return ListButton::default();
        }
        if val == UI_BUTTON_PRIMARY {
            return ListButton {
                ret: -1,
                init_select: selected == -1,
                check_locked: self.locked,
            };
        }
        if val == UI_BUTTON_SECONDARY {
            if !self.locked {
                return ListButton::default();
            }
            self.locked = false;
        }
        ListButton {
            ret: 1,
            ..ListButton::default()
        }
    }
}

/// `OnDrawAfterClip` cursor rectangles as `(pos, size)` for the four lines, `None` when
/// `cursor - first` is -1.
pub fn cursor_lines(
    st: &ListState,
    start: Vector3f,
    size_x: f32,
    slider_w: f32,
    font_y: f32,
) -> Option<[(Vector3f, Vector2f); 4]> {
    let index = st.cursor_pos.wrapping_sub(st.first_item);
    if index == -1 {
        return None;
    }
    let ul = v3(
        start.x + -1.0,
        (index as f32).mul_add(font_y + 2.0, start.y),
        start.z + 0.0,
    );
    let width = v2(size_x - slider_w, 1.0);
    let height = v2(1.0, font_y);
    Some([
        (ul, width),
        (add3(ul, v3(0.0, height.y, 0.0)), width),
        (ul, height),
        (add3(ul, v3(height.x, 0.0, 0.0)), height),
    ])
}

pub const ROW_NULL: u8 = 1;
pub const ROW_UNSELECTABLE: u8 = 2;
pub const ROW_SELECTED: u8 = 4;

pub const OP_BACKGROUND: i32 = 0;
pub const OP_SELECTION: i32 = 1;
pub const OP_TEXT: i32 = 2;
pub const OP_TEXT_HIGHLIGHT: i32 = 3;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct RowOp {
    pub kind: i32,
    pub item: i32,
    pub pos: Vector3f,
    pub size: Vector2f,
}

/// `cWidgetListBox::DrawItems`; `pos` is the first row position and `rows[k]` the flags of item
/// `first + k`. Returns the number of ops written.
#[allow(clippy::too_many_arguments, reason = "snapshot of the widget values")]
pub fn list_rows(
    first: i32,
    max_items: i32,
    items: i32,
    pos: Vector3f,
    size_x: f32,
    font_y: f32,
    rows: &[u8],
    out: &mut [RowOp],
) -> usize {
    let mut pos = pos;
    let mut n = 0;
    let mut put = |op: RowOp| {
        if let Some(s) = out.get_mut(n) {
            *s = op;
        }
        n += 1;
    };
    let mut i = first;
    while i < items {
        if i.wrapping_sub(first) > max_items {
            break;
        }
        let flags = usize::try_from(i.wrapping_sub(first))
            .ok()
            .and_then(|k| rows.get(k))
            .copied()
            .unwrap_or(ROW_NULL);
        i = i.wrapping_add(1);
        if flags & ROW_NULL != 0 {
            continue;
        }
        let back = v3(pos.x - 3.0, pos.y - 0.0, pos.z - 0.01);
        let item = i.wrapping_sub(1);
        if flags & ROW_UNSELECTABLE != 0 {
            put(RowOp {
                kind: OP_BACKGROUND,
                item,
                pos: back,
                size: v2(size_x, font_y),
            });
        }
        if flags & ROW_SELECTED != 0 {
            put(RowOp {
                kind: OP_SELECTION,
                item,
                pos: back,
                size: v2(size_x, font_y),
            });
            put(RowOp {
                kind: OP_TEXT_HIGHLIGHT,
                item,
                pos,
                size: Vector2f::default(),
            });
        } else {
            put(RowOp {
                kind: OP_TEXT,
                item,
                pos,
                size: Vector2f::default(),
            });
        }
        pos.y += font_y + 2.0;
    }
    n
}

// ---------------------------------------------------------------- combo box

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct ComboState {
    pub menu_open: bool,
    pub clicked_after_open: bool,
    pub open_by_ui: bool,
    pub menu_height: f32,
    pub menu_pos: Vector3f,
    pub mouse_over: i32,
    pub selected: i32,
    pub first_item: i32,
    pub max_items: i32,
    pub items_shown: i32,
}

impl Default for ComboState {
    fn default() -> Self {
        Self::new()
    }
}

impl ComboState {
    pub fn new() -> Self {
        ComboState {
            menu_open: false,
            clicked_after_open: false,
            open_by_ui: false,
            menu_height: 0.0,
            menu_pos: Vector3f::default(),
            mouse_over: -1,
            selected: -1,
            first_item: 0,
            max_items: 12,
            items_shown: 0,
        }
    }

    /// `SetSelectedItem` before the first slider call: false when the call is ignored.
    pub fn select_step1(&mut self, x: i32, move_list: bool, items: i32) -> Option<SliderSet> {
        if x >= items || self.selected == x {
            return None;
        }
        self.selected = x;
        if move_list && self.selected >= self.first_item.wrapping_add(self.max_items) {
            self.first_item = self.selected.wrapping_sub(self.max_items).wrapping_add(1);
            return Some(SliderSet {
                set: true,
                value: self.first_item,
            });
        }
        Some(SliderSet::default())
    }

    pub fn select_step2(&mut self, move_list: bool) -> SliderSet {
        if move_list && self.selected < self.first_item && self.selected >= 0 {
            self.first_item = self.selected;
            return SliderSet {
                set: true,
                value: self.selected,
            };
        }
        SliderSet::default()
    }

    /// `UpdateProperties`: true when the selection must be clamped to the last item.
    pub fn update(&mut self, font_y: f32, items: i32) -> bool {
        self.items_shown = if items <= self.max_items {
            items
        } else {
            self.max_items
        };
        self.menu_height = (font_y + 2.0).mul_add(self.items_shown as f32, 2.0) + 2.0;
        self.selected >= items
    }

    /// `OpenMenu` guard, evaluated before the attention is taken.
    pub fn can_open(&self, items: i32) -> bool {
        items != 0 && !self.menu_open
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct ComboOpen {
    pub slider: bool,
    pub slider_pos: Vector3f,
    pub slider_size: Vector2f,
}

impl ComboState {
    /// `OpenMenu` after `SetAttentionWidget`.
    #[allow(
        clippy::too_many_arguments,
        reason = "snapshot of the widget and skin values"
    )]
    pub fn open(
        &mut self,
        items: i32,
        size: Vector2f,
        b1: Vector2f,
        b2: Vector2f,
        global_y: f32,
        popup_z: f32,
        virtual_y: f32,
    ) -> ComboOpen {
        self.menu_open = true;
        self.mouse_over = self.selected;
        self.menu_pos = v3(0.0, size.y + b1.y, popup_z + 1.0);
        if global_y + self.menu_pos.y + self.menu_height > virtual_y {
            let diff = global_y + self.menu_pos.y + self.menu_height - virtual_y;
            self.menu_pos.y -= diff;
        }
        let mut out = ComboOpen::default();
        if items > self.max_items {
            out.slider = true;
            out.slider_pos = add3(self.menu_pos, v3(size.x - 20.0 - b1.x, 1.0, 0.2));
            out.slider_size = v2(20.0, self.menu_height - b1.y - b2.y);
        }
        out
    }

    /// `CloseMenu`: true when the menu was open.
    pub fn close(&mut self) -> bool {
        let was = self.menu_open;
        self.menu_open = false;
        was
    }

    fn in_menu(&self, local: Vector3f, size_x: f32) -> bool {
        local.x >= self.menu_pos.x
            && local.x <= self.menu_pos.x + size_x
            && local.y >= self.menu_pos.y
            && local.y <= self.menu_pos.y + self.menu_height
    }

    /// `ButtonPress`: 0 not handled, 1 forward to mouse down and up, 2 close the menu, 3 open it.
    pub fn button_press(&mut self, val: i32, local: Vector3f, size_x: f32) -> i32 {
        if val != 1 {
            return 0;
        }
        if self.menu_open {
            // The C++ compares y against x + width.
            if local.x >= self.menu_pos.x
                && local.y <= self.menu_pos.x + size_x
                && local.y >= self.menu_pos.y
                && local.y <= self.menu_pos.y + self.menu_height
            {
                1
            } else {
                2
            }
        } else {
            3
        }
    }

    /// `OnMouseMove`.
    #[allow(
        clippy::too_many_arguments,
        reason = "snapshot of the widget and skin values"
    )]
    pub fn mouse_move(
        &mut self,
        mouse_over: bool,
        local: Vector3f,
        size_x: f32,
        slider_enabled: bool,
        corner0_y: f32,
        font_y: f32,
        items: i32,
    ) -> bool {
        if !self.menu_open || !mouse_over {
            return false;
        }
        if local.y <= self.menu_pos.y {
            return false;
        }
        if slider_enabled && local.x >= size_x - 20.0 {
            return false;
        }
        let to_text = 2.0 + self.menu_pos.y + corner0_y;
        let mut sel = ((local.y - to_text) / (font_y + 2.0)) as i32;
        if sel < 0 {
            sel = 0;
        }
        sel = sel.wrapping_add(self.first_item);
        if sel >= items {
            sel = items.wrapping_sub(1);
        }
        self.mouse_over = sel;
        true
    }

    /// `OnMouseDown` up to the wheel test: `(proceed, wheel direction)`, wheel -1 up and 1 down.
    pub fn mouse_down_pre(&mut self, val: i32) -> (bool, i32) {
        if !self.menu_open {
            return (false, 0);
        }
        self.clicked_after_open = true;
        if val & MOUSE_WHEEL_UP != 0 {
            (true, -1)
        } else if val & MOUSE_WHEEL_DOWN != 0 {
            (true, 1)
        } else {
            (true, 0)
        }
    }

    /// `OnMouseDown` click: true when the menu closes.
    pub fn mouse_down_closes(&self, local_y: f32) -> bool {
        local_y < self.menu_pos.y || local_y > self.menu_pos.y + self.menu_height
    }

    /// `OnMouseUp` guard: false returns false.
    pub fn mouse_up_pre(&self, val: i32) -> bool {
        self.menu_open
            && self.clicked_after_open
            && val & MOUSE_WHEEL_UP == 0
            && val & MOUSE_WHEEL_DOWN == 0
    }

    /// `OnMouseUp`: true when the hovered item is selected and the menu closes.
    pub fn mouse_up_selects(&self, local_y: f32) -> bool {
        local_y > self.menu_pos.y
    }

    /// `OnUIArrowPress`: `(return value, slider value to set)`; `value` and `add` are the slider's.
    pub fn ui_arrow(
        &mut self,
        arrow: i32,
        items: i32,
        value: i32,
        add: i32,
    ) -> (bool, Option<i32>) {
        if !self.menu_open {
            return (false, None);
        }
        if arrow == UI_ARROW_DOWN {
            if self.mouse_over.wrapping_add(1) <= items.wrapping_sub(1) {
                self.mouse_over += 1;
            }
        } else if arrow == UI_ARROW_UP && self.mouse_over.wrapping_sub(1) >= 0 {
            self.mouse_over -= 1;
        }
        let l = idiv(value, add);
        let set = if self.mouse_over < l {
            Some(self.mouse_over.wrapping_mul(add))
        } else if self.mouse_over > l.wrapping_add(11) {
            Some(self.mouse_over.wrapping_sub(11).wrapping_mul(add))
        } else {
            None
        };
        (true, set)
    }

    /// `OnLostFocus`: true when the menu closes.
    pub fn lost_focus_closes(&self, local: Vector3f, size_x: f32) -> bool {
        self.menu_open
            && !self.open_by_ui
            && (local.x < self.menu_pos.x
                || local.x > self.menu_pos.x + size_x
                || local.y < self.menu_pos.y
                || local.y > self.menu_pos.y + self.menu_height)
    }

    /// `PointIsInside` after the clipping parent check.
    pub fn point_inside(&self, local: Vector3f, size: Vector2f) -> bool {
        let inside = !(local.x < 0.0 || local.x > size.x || local.y < 0.0 || local.y > size.y);
        inside || (self.menu_open && self.in_menu(local, size.x))
    }

    /// `SliderOnUIArrowPress`.
    pub fn slider_arrow(&mut self, arrow: i32, items: i32) -> bool {
        let last = items.wrapping_sub(1);
        if arrow == UI_ARROW_UP {
            self.mouse_over = self.mouse_over.wrapping_sub(1);
            if self.mouse_over < 0 {
                self.mouse_over = last;
                self.first_item = self
                    .mouse_over
                    .wrapping_sub(self.items_shown)
                    .wrapping_add(1);
            }
            if self.mouse_over < self.first_item {
                self.first_item = self.mouse_over;
            }
        } else if arrow == UI_ARROW_DOWN {
            self.mouse_over = self.mouse_over.wrapping_add(1);
            if self.mouse_over > last {
                self.mouse_over = 0;
                self.first_item = 0;
            }
            if self.mouse_over
                > self
                    .first_item
                    .wrapping_add(self.items_shown)
                    .wrapping_sub(1)
            {
                self.first_item = self
                    .mouse_over
                    .wrapping_sub(self.items_shown)
                    .wrapping_add(1);
            }
        } else {
            return false;
        }
        true
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct UiButton {
    pub ret: bool,
    pub clear_open_by_ui: bool,
    /// 0 nothing, 1 close, 2 open, 3 select the hovered item then close.
    pub action: i32,
}

impl ComboState {
    /// `OnUIButtonPress` with focus resolved by C++.
    pub fn ui_button(&mut self, has_focus: bool, val: i32) -> UiButton {
        let mut out = UiButton::default();
        if !has_focus {
            return out;
        }
        if val & UI_BUTTON_SECONDARY != 0 {
            if self.menu_open {
                self.open_by_ui = false;
                out.ret = true;
                out.action = 1;
            }
            return out;
        }
        if val & UI_BUTTON_PRIMARY == 0 {
            return out;
        }
        if !self.open_by_ui {
            if !self.menu_open {
                self.open_by_ui = true;
                out.action = 2;
            } else {
                out.action = 3;
            }
        } else if self.menu_open {
            out.action = 3;
            out.clear_open_by_ui = true;
        }
        out.ret = val == UI_BUTTON_PRIMARY;
        out
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct ComboLayout {
    pub button_size: Vector2f,
    pub button_pos: Vector3f,
}

/// `OnChangeSize` button placement.
pub fn combo_layout(size: Vector2f, back: Vector2f, button_w: f32) -> ComboLayout {
    ComboLayout {
        button_size: v2(button_w, back.y),
        button_pos: v3(
            size.x - (size.x - back.x) / 2.0 - button_w,
            (size.y - back.y) / 2.0,
            0.3,
        ),
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct ComboDraw {
    pub bg_pos: Vector3f,
    pub bg_size: Vector2f,
    pub border_pos: Vector3f,
    pub border_size: Vector2f,
}

pub fn combo_draw(st: &ComboState, global: Vector3f, size_x: f32) -> ComboDraw {
    ComboDraw {
        bg_pos: add3(global, st.menu_pos),
        bg_size: v2(size_x, st.menu_height),
        border_pos: add3(add3(global, st.menu_pos), v3(0.0, 0.0, 0.2)),
        border_size: v2(size_x, st.menu_height),
    }
}

/// `DrawText` callback rows: ops use `OP_SELECTION` / `OP_TEXT_HIGHLIGHT` / `OP_TEXT`.
pub fn combo_rows(
    st: &ComboState,
    global: Vector3f,
    size_x: f32,
    font_y: f32,
    items: i32,
    out: &mut [RowOp],
) -> usize {
    let mut pos = add3(add3(global, st.menu_pos), v3(3.0, 2.0, 0.2));
    let mut n = 0;
    let mut put = |op: RowOp| {
        if let Some(s) = out.get_mut(n) {
            *s = op;
        }
        n += 1;
    };
    let mut i = st.first_item;
    while i < items {
        if i.wrapping_sub(st.first_item) >= st.max_items {
            break;
        }
        if i == st.mouse_over {
            put(RowOp {
                kind: OP_SELECTION,
                item: i,
                pos: v3(pos.x - 3.0, pos.y - 0.0, pos.z - 0.1),
                size: v2(size_x, font_y),
            });
            put(RowOp {
                kind: OP_TEXT_HIGHLIGHT,
                item: i,
                pos,
                size: Vector2f::default(),
            });
        } else {
            put(RowOp {
                kind: OP_TEXT,
                item: i,
                pos,
                size: Vector2f::default(),
            });
        }
        pos.y += font_y + 2.0;
        i = i.wrapping_add(1);
    }
    n
}
