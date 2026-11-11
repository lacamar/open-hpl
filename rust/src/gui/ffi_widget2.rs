//! C ABI for the slider, tab frame, list box, combo box and text box logic, declared in
//! `gui/RustWidget2.h`.
//!
//! Pointers are valid, aligned and non-null for the call unless noted; a null input makes the call
//! a no-op that returns the zero value.

use std::ffi::c_void;

use super::widgets_list::{
    self as wl, ComboDraw, ComboLayout, ComboOpen, ComboState, ListArrow, ListButton, ListKey,
    ListMouse, ListState, ListUpdate, RangePlan, RowOp, SelAction, SliderSet, UiButton,
};
use super::widgets_slider::{
    self as ws, SliderDraw, SliderState, TabArrows, TabFrameState, TabInit, TabItem, TabLayout,
};
use super::widgets_text::{self as wt, Edit, Event, Ret, TextDraw, TextEnv, TextHost, TextState};
use crate::types::{Vector2f, Vector3f};

/// # Safety
/// `p` is null or valid for reads.
unsafe fn rd<'a, T>(p: *const T) -> Option<&'a T> {
    // SAFETY: caller contract.
    unsafe { p.as_ref() }
}

/// # Safety
/// `p` is null or valid for reads and writes.
unsafe fn rw<'a, T>(p: *mut T) -> Option<&'a mut T> {
    // SAFETY: caller contract.
    unsafe { p.as_mut() }
}

/// # Safety
/// `p` is null or valid for writes.
unsafe fn wr<T>(p: *mut T, v: T) {
    // SAFETY: caller contract.
    if let Some(r) = unsafe { p.as_mut() } {
        *r = v;
    }
}

/// # Safety
/// `p` is null or points to `n` readable elements.
unsafe fn items<'a, T>(p: *const T, n: usize) -> &'a [T] {
    if p.is_null() {
        &[]
    } else {
        // SAFETY: caller contract.
        unsafe { std::slice::from_raw_parts(p, n) }
    }
}

/// # Safety
/// `p` is null or points to `n` writable elements.
unsafe fn items_mut<'a, T>(p: *mut T, n: usize) -> &'a mut [T] {
    if p.is_null() {
        &mut []
    } else {
        // SAFETY: caller contract.
        unsafe { std::slice::from_raw_parts_mut(p, n) }
    }
}

// ---------------------------------------------------------------- slider

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_w2_slider_init(vertical: bool, button_size: f32) -> SliderState {
    SliderState::new(vertical, button_size)
}

/// # Safety
/// Pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_slider_set_value(
    st: *mut SliderState,
    v: i32,
    out: *mut i32,
) -> bool {
    // SAFETY: caller contract.
    unsafe {
        match rw(st).and_then(|s| s.set_value(v)) {
            Some(n) => {
                wr(out, n);
                true
            }
            None => false,
        }
    }
}

/// # Safety
/// `st` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_slider_set_max(st: *mut SliderState, m: i32) -> bool {
    // SAFETY: caller contract.
    unsafe { rw(st) }.is_some_and(|s| s.set_max_value(m))
}

/// # Safety
/// `st` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_slider_set_bar_size(st: *mut SliderState, v: i32) -> bool {
    // SAFETY: caller contract.
    unsafe { rw(st) }.is_some_and(|s| s.set_bar_value_size(v))
}

/// # Safety
/// Pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_slider_update_bar(
    st: *mut SliderState,
    size: *const Vector2f,
    global: *const Vector3f,
    c0: *const Vector2f,
    c3: *const Vector2f,
) {
    // SAFETY: caller contract.
    unsafe {
        if let (Some(s), Some(a), Some(g), Some(c), Some(d)) =
            (rw(st), rd(size), rd(global), rd(c0), rd(c3))
        {
            s.update_bar(*a, *g, *c, *d);
        }
    }
}

/// # Safety
/// Pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_slider_mouse_move(
    st: *mut SliderState,
    local: *const Vector3f,
    global: *const Vector3f,
    out: *mut i32,
) -> bool {
    // SAFETY: caller contract.
    unsafe {
        match (rw(st), rd(local), rd(global)) {
            (Some(s), Some(l), Some(g)) => match s.mouse_move(*l, *g) {
                Some(v) => {
                    wr(out, v);
                    true
                }
                None => false,
            },
            _ => false,
        }
    }
}

/// Returns the handler's return value; `fire` reports a changed value in `out`.
///
/// # Safety
/// Pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_slider_mouse_down(
    st: *mut SliderState,
    val: i32,
    pos: *const Vector2f,
    local: *const Vector3f,
    global: *const Vector3f,
    out: *mut i32,
    fire: *mut bool,
) -> bool {
    // SAFETY: caller contract.
    unsafe {
        match (rw(st), rd(pos), rd(local), rd(global)) {
            (Some(s), Some(p), Some(l), Some(g)) => {
                let (ret, v) = s.mouse_down(val, *p, *l, *g);
                wr(fire, v.is_some());
                if let Some(v) = v {
                    wr(out, v);
                }
                ret
            }
            _ => false,
        }
    }
}

/// # Safety
/// `st` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_slider_release(st: *mut SliderState) -> bool {
    // SAFETY: caller contract.
    unsafe { rw(st) }.is_some_and(SliderState::release)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_w2_slider_arrow(vertical: bool, arrow: i32) -> i32 {
    ws::slider_arrow_button(vertical, arrow)
}

/// # Safety
/// Pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_slider_draw(
    st: *const SliderState,
    size: *const Vector2f,
    global: *const Vector3f,
    out: *mut SliderDraw,
) {
    // SAFETY: caller contract.
    unsafe {
        if let (Some(s), Some(z), Some(g)) = (rd(st), rd(size), rd(global)) {
            wr(out, ws::slider_draw(s, *z, *g));
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct PosSize {
    pub pos: Vector3f,
    pub size: Vector2f,
}

/// # Safety
/// Pointers valid; `out` holds two entries.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_slider_buttons(
    vertical: bool,
    size: *const Vector2f,
    btn: f32,
    out: *mut PosSize,
) {
    // SAFETY: caller contract.
    unsafe {
        if let Some(z) = rd(size) {
            for (slot, (pos, size)) in items_mut(out, 2)
                .iter_mut()
                .zip(ws::slider_buttons(vertical, *z, btn))
            {
                *slot = PosSize { pos, size };
            }
        }
    }
}

// ---------------------------------------------------------------- tab frame

/// # Safety
/// `st` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_tab_arrow(st: *mut TabFrameState, right: bool, tabs: i32) -> bool {
    // SAFETY: caller contract.
    unsafe { rw(st) }.is_some_and(|s| s.arrow(right, tabs))
}

/// # Safety
/// `widths` and `out_items` hold `n` entries.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_tab_visibility(
    st: *mut TabFrameState,
    size_x: f32,
    widths: *const f32,
    out_items: *mut TabItem,
    n: usize,
    out: *mut TabArrows,
) {
    // SAFETY: caller contract.
    unsafe {
        if let Some(s) = rw(st) {
            wr(
                out,
                s.visibility(size_x, items(widths, n), items_mut(out_items, n)),
            );
        }
    }
}

/// # Safety
/// `st` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_tab_label_pos_y(st: *const TabFrameState, on_top: bool) -> f32 {
    // SAFETY: caller contract.
    unsafe { rd(st) }.map_or(0.0, |s| s.label_pos_y(on_top))
}

/// # Safety
/// `st` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_tab_label_width(
    st: *const TabFrameState,
    text_len: f32,
    borders_w: f32,
    has_icon: bool,
    icon_w: f32,
) -> f32 {
    // SAFETY: caller contract.
    unsafe { rd(st) }.map_or(0.0, |s| {
        s.label_width(text_len, borders_w, has_icon.then_some(icon_w))
    })
}

/// # Safety
/// Pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_tab_label_text_pos(
    st: *const TabFrameState,
    global: *const Vector3f,
    has_icon: bool,
    icon_w: f32,
    out: *mut Vector3f,
) {
    // SAFETY: caller contract.
    unsafe {
        if let (Some(s), Some(g)) = (rd(st), rd(global)) {
            wr(out, s.label_text_pos(*g, has_icon.then_some(icon_w)));
        }
    }
}

/// # Safety
/// Pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_tab_arrow_pos(
    st: *const TabFrameState,
    size_x: f32,
    i: i32,
    fg_z: f32,
    out: *mut Vector3f,
) {
    // SAFETY: caller contract.
    unsafe {
        if let Some(s) = rd(st) {
            wr(out, s.arrow_pos(size_x, i, fg_z));
        }
    }
}

/// # Safety
/// `out` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_tab_page_target(val: i32, top: i32, out: *mut i32) -> bool {
    // SAFETY: caller contract.
    unsafe {
        match ws::tab_page_target(val, top) {
            Some(t) => {
                wr(out, t);
                true
            }
            None => false,
        }
    }
}

/// # Safety
/// Pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_tab_layout(
    size: *const Vector2f,
    font_h: f32,
    active: f32,
    out: *mut TabLayout,
) {
    // SAFETY: caller contract.
    unsafe {
        if let Some(z) = rd(size) {
            wr(out, ws::tab_layout(*z, font_h, active));
        }
    }
}

/// # Safety
/// Pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_tab_init(
    size: *const Vector2f,
    font_h: f32,
    active: f32,
    c: *const Vector2f,
    out: *mut TabInit,
) {
    // SAFETY: caller contract; `c` holds the corners LU, RU and RD.
    unsafe {
        if let (Some(z), [c0, c1, c2]) = (rd(size), items(c, 3)) {
            wr(out, ws::tab_init(*z, font_h, active, *c0, *c1, *c2));
        }
    }
}

/// # Safety
/// Pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_tab_point_inside(
    p: *const Vector2f,
    label_visible: bool,
    label_pos: *const Vector3f,
    label_size: *const Vector2f,
    frame_pos: *const Vector3f,
    frame_size: *const Vector2f,
) -> bool {
    // SAFETY: caller contract.
    unsafe {
        match (
            rd(p),
            rd(label_pos),
            rd(label_size),
            rd(frame_pos),
            rd(frame_size),
        ) {
            (Some(p), Some(lp), Some(ls), Some(fp), Some(fs)) => {
                ws::tab_point_inside(*p, label_visible, *lp, *ls, *fp, *fs)
            }
            _ => false,
        }
    }
}

// ---------------------------------------------------------------- list box

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_w2_list_init() -> ListState {
    ListState::new()
}

/// # Safety
/// `st` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_list_update(
    st: *mut ListState,
    size_y: f32,
    font_y: f32,
    n: i32,
    out: *mut ListUpdate,
) {
    // SAFETY: caller contract.
    unsafe {
        if let Some(s) = rw(st) {
            wr(out, s.update(size_y, font_y, n));
        }
    }
}

/// # Safety
/// `sel` holds `n` entries.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_list_prune(sel: *mut i32, n: usize, items_n: i32) -> usize {
    // SAFETY: caller contract.
    unsafe { wl::prune_selection(items_mut(sel, n), items_n) }
}

/// # Safety
/// `sel` holds `n` entries.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_list_multi_selection(sel: *const i32, n: usize, idx: i32) -> i32 {
    // SAFETY: caller contract.
    wl::multi_selection(unsafe { items(sel, n) }, idx)
}

/// # Safety
/// `st` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_list_cursor1(
    st: *mut ListState,
    pos: i32,
    move_list: bool,
    clamp: bool,
) -> SliderSet {
    // SAFETY: caller contract.
    unsafe { rw(st) }.map_or_else(SliderSet::default, |s| {
        s.cursor_step1(pos, move_list, clamp)
    })
}

/// # Safety
/// `st` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_list_cursor2(st: *mut ListState, move_list: bool) -> SliderSet {
    // SAFETY: caller contract.
    unsafe { rw(st) }.map_or_else(SliderSet::default, |s| s.cursor_step2(move_list))
}

/// # Safety
/// `st` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_list_select_action(
    st: *const ListState,
    ty: i32,
    idx: i32,
    exists: bool,
    selected: bool,
) -> SelAction {
    // SAFETY: caller contract.
    unsafe { rd(st) }.map_or_else(SelAction::default, |s| {
        s.select_action(ty, idx, exists, selected)
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_w2_list_range(start: i32, end: i32, n: i32) -> RangePlan {
    wl::range_plan(start, end, n)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_w2_list_wheel(val: i32) -> i32 {
    ListState::wheel(val)
}

/// # Safety
/// `st` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_list_mouse_down(
    st: *mut ListState,
    local_y: f32,
    font_y: f32,
    modifier: i32,
) -> ListMouse {
    // SAFETY: caller contract.
    unsafe { rw(st) }.map_or_else(ListMouse::default, |s| {
        s.mouse_down(local_y, font_y, modifier)
    })
}

/// # Safety
/// `st` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_list_mouse_down_finish(
    st: *mut ListState,
    shift: bool,
    sel: i32,
) -> i32 {
    // SAFETY: caller contract.
    unsafe { rw(st) }.map_or(0, |s| s.mouse_down_finish(shift, sel))
}

/// # Safety
/// Pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_list_double_click(
    st: *mut ListState,
    local_x: f32,
    size_x: f32,
    slider_w: f32,
    fire: *mut bool,
) -> bool {
    // SAFETY: caller contract.
    unsafe {
        match rw(st) {
            Some(s) => {
                let (ret, f) = s.double_click(local_x, size_x, slider_w);
                wr(fire, f);
                ret
            }
            None => false,
        }
    }
}

/// # Safety
/// `st` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_list_key(st: *mut ListState, key: i32, modifier: i32) -> ListKey {
    // SAFETY: caller contract.
    unsafe { rw(st) }.map_or_else(ListKey::default, |s| s.key_press(key, modifier))
}

/// # Safety
/// `st` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_list_key_finish(st: *mut ListState, modifier: i32) -> ListKey {
    // SAFETY: caller contract.
    unsafe { rw(st) }.map_or_else(ListKey::default, |s| s.key_press_finish(modifier))
}

/// # Safety
/// `st` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_list_ui_arrow(st: *mut ListState, arrow: i32) -> ListArrow {
    // SAFETY: caller contract.
    unsafe { rw(st) }.map_or_else(ListArrow::default, |s| s.ui_arrow(arrow))
}

/// # Safety
/// `st` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_list_ui_button(
    st: *mut ListState,
    has_focus: bool,
    val: i32,
    selected: i32,
) -> ListButton {
    // SAFETY: caller contract.
    unsafe { rw(st) }.map_or_else(ListButton::default, |s| {
        s.ui_button(has_focus, val, selected)
    })
}

/// # Safety
/// Pointers valid; `out` holds four entries. Returns false when the cursor is outside the window.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_list_cursor_lines(
    st: *const ListState,
    start: *const Vector3f,
    size_x: f32,
    slider_w: f32,
    font_y: f32,
    out: *mut PosSize,
) -> bool {
    // SAFETY: caller contract.
    unsafe {
        let (Some(s), Some(p)) = (rd(st), rd(start)) else {
            return false;
        };
        let Some(lines) = wl::cursor_lines(s, *p, size_x, slider_w, font_y) else {
            return false;
        };
        for (slot, (pos, size)) in items_mut(out, 4).iter_mut().zip(lines) {
            *slot = PosSize { pos, size };
        }
        true
    }
}

/// # Safety
/// `rows` holds the flags of the items from `first`; `out` holds `cap` ops.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_list_rows(
    first: i32,
    max_items: i32,
    n: i32,
    pos: *const Vector3f,
    size_x: f32,
    font_y: f32,
    rows: *const u8,
    n_rows: usize,
    out: *mut RowOp,
    cap: usize,
) -> usize {
    // SAFETY: caller contract.
    unsafe {
        match rd(pos) {
            Some(p) => wl::list_rows(
                first,
                max_items,
                n,
                *p,
                size_x,
                font_y,
                items(rows, n_rows),
                items_mut(out, cap),
            )
            .min(cap),
            None => 0,
        }
    }
}

// ---------------------------------------------------------------- combo box

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_w2_combo_init() -> ComboState {
    ComboState::new()
}

/// Returns false when the selection call is ignored.
///
/// # Safety
/// Pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_combo_select1(
    st: *mut ComboState,
    x: i32,
    move_list: bool,
    n: i32,
    out: *mut SliderSet,
) -> bool {
    // SAFETY: caller contract.
    unsafe {
        match rw(st).and_then(|s| s.select_step1(x, move_list, n)) {
            Some(v) => {
                wr(out, v);
                true
            }
            None => false,
        }
    }
}

/// # Safety
/// `st` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_combo_select2(st: *mut ComboState, move_list: bool) -> SliderSet {
    // SAFETY: caller contract.
    unsafe { rw(st) }.map_or_else(SliderSet::default, |s| s.select_step2(move_list))
}

/// # Safety
/// `st` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_combo_update(st: *mut ComboState, font_y: f32, n: i32) -> bool {
    // SAFETY: caller contract.
    unsafe { rw(st) }.is_some_and(|s| s.update(font_y, n))
}

/// # Safety
/// `st` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_combo_can_open(st: *const ComboState, n: i32) -> bool {
    // SAFETY: caller contract.
    unsafe { rd(st) }.is_some_and(|s| s.can_open(n))
}

/// # Safety
/// Pointers valid; `skin` holds the active sizes of borders LEFT (index 0) and UP (index 1).
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_combo_open(
    st: *mut ComboState,
    n: i32,
    size: *const Vector2f,
    skin: *const Vector2f,
    global_y: f32,
    popup_z: f32,
    virtual_y: f32,
    out: *mut ComboOpen,
) {
    // SAFETY: caller contract.
    unsafe {
        if let (Some(s), Some(z), [b1, b2]) = (rw(st), rd(size), items(skin, 2)) {
            wr(out, s.open(n, *z, *b1, *b2, global_y, popup_z, virtual_y));
        }
    }
}

/// # Safety
/// `st` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_combo_close(st: *mut ComboState) -> bool {
    // SAFETY: caller contract.
    unsafe { rw(st) }.is_some_and(ComboState::close)
}

/// # Safety
/// Pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_combo_button_press(
    st: *mut ComboState,
    val: i32,
    local: *const Vector3f,
    size_x: f32,
) -> i32 {
    // SAFETY: caller contract.
    unsafe {
        match (rw(st), rd(local)) {
            (Some(s), Some(l)) => s.button_press(val, *l, size_x),
            _ => 0,
        }
    }
}

/// # Safety
/// Pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_combo_mouse_move(
    st: *mut ComboState,
    mouse_over: bool,
    local: *const Vector3f,
    size_x: f32,
    slider_enabled: bool,
    corner0_y: f32,
    font_y: f32,
    n: i32,
) -> bool {
    // SAFETY: caller contract.
    unsafe {
        match (rw(st), rd(local)) {
            (Some(s), Some(l)) => {
                s.mouse_move(mouse_over, *l, size_x, slider_enabled, corner0_y, font_y, n)
            }
            _ => false,
        }
    }
}

/// Returns whether to proceed; `wheel` is -1 up, 1 down.
///
/// # Safety
/// Pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_combo_mouse_down_pre(
    st: *mut ComboState,
    val: i32,
    wheel: *mut i32,
) -> bool {
    // SAFETY: caller contract.
    unsafe {
        match rw(st) {
            Some(s) => {
                let (proceed, w) = s.mouse_down_pre(val);
                wr(wheel, w);
                proceed
            }
            None => false,
        }
    }
}

/// # Safety
/// `st` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_combo_mouse_down_closes(
    st: *const ComboState,
    local_y: f32,
) -> bool {
    // SAFETY: caller contract.
    unsafe { rd(st) }.is_some_and(|s| s.mouse_down_closes(local_y))
}

/// # Safety
/// `st` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_combo_mouse_up_pre(st: *const ComboState, val: i32) -> bool {
    // SAFETY: caller contract.
    unsafe { rd(st) }.is_some_and(|s| s.mouse_up_pre(val))
}

/// # Safety
/// `st` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_combo_mouse_up_selects(
    st: *const ComboState,
    local_y: f32,
) -> bool {
    // SAFETY: caller contract.
    unsafe { rd(st) }.is_some_and(|s| s.mouse_up_selects(local_y))
}

/// Returns the handler's return value; `set` is the slider value to set when `has_set`.
///
/// # Safety
/// Pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_combo_ui_arrow(
    st: *mut ComboState,
    arrow: i32,
    n: i32,
    value: i32,
    add: i32,
    set: *mut i32,
    has_set: *mut bool,
) -> bool {
    // SAFETY: caller contract.
    unsafe {
        match rw(st) {
            Some(s) => {
                let (ret, v) = s.ui_arrow(arrow, n, value, add);
                wr(has_set, v.is_some());
                if let Some(v) = v {
                    wr(set, v);
                }
                ret
            }
            None => false,
        }
    }
}

/// # Safety
/// Pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_combo_lost_focus(
    st: *const ComboState,
    local: *const Vector3f,
    size_x: f32,
) -> bool {
    // SAFETY: caller contract.
    unsafe {
        match (rd(st), rd(local)) {
            (Some(s), Some(l)) => s.lost_focus_closes(*l, size_x),
            _ => false,
        }
    }
}

/// # Safety
/// Pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_combo_point_inside(
    st: *const ComboState,
    local: *const Vector3f,
    size: *const Vector2f,
) -> bool {
    // SAFETY: caller contract.
    unsafe {
        match (rd(st), rd(local), rd(size)) {
            (Some(s), Some(l), Some(z)) => s.point_inside(*l, *z),
            _ => false,
        }
    }
}

/// # Safety
/// `st` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_combo_slider_arrow(
    st: *mut ComboState,
    arrow: i32,
    n: i32,
) -> bool {
    // SAFETY: caller contract.
    unsafe { rw(st) }.is_some_and(|s| s.slider_arrow(arrow, n))
}

/// # Safety
/// `st` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_combo_ui_button(
    st: *mut ComboState,
    has_focus: bool,
    val: i32,
) -> UiButton {
    // SAFETY: caller contract.
    unsafe { rw(st) }.map_or_else(UiButton::default, |s| s.ui_button(has_focus, val))
}

/// # Safety
/// Pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_combo_layout(
    size: *const Vector2f,
    back: *const Vector2f,
    button_w: f32,
    out: *mut ComboLayout,
) {
    // SAFETY: caller contract.
    unsafe {
        if let (Some(a), Some(b)) = (rd(size), rd(back)) {
            wr(out, wl::combo_layout(*a, *b, button_w));
        }
    }
}

/// # Safety
/// Pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_combo_draw(
    st: *const ComboState,
    global: *const Vector3f,
    size_x: f32,
    out: *mut ComboDraw,
) {
    // SAFETY: caller contract.
    unsafe {
        if let (Some(s), Some(g)) = (rd(st), rd(global)) {
            wr(out, wl::combo_draw(s, *g, size_x));
        }
    }
}

/// # Safety
/// Pointers valid; `out` holds `cap` ops.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_combo_rows(
    st: *const ComboState,
    global: *const Vector3f,
    size_x: f32,
    font_y: f32,
    n: i32,
    out: *mut RowOp,
    cap: usize,
) -> usize {
    // SAFETY: caller contract.
    unsafe {
        match (rd(st), rd(global)) {
            (Some(s), Some(g)) => {
                wl::combo_rows(s, *g, size_x, font_y, n, items_mut(out, cap)).min(cap)
            }
            _ => 0,
        }
    }
}

// ---------------------------------------------------------------- text box

/// Callbacks into the C++ widget. None of them may re-enter a Rust text call.
#[repr(C)]
pub struct TextHostC {
    pub ctx: *mut c_void,
    pub glyph: Option<unsafe extern "C" fn(*mut c_void, i32, *mut f32) -> bool>,
    pub local_x: Option<unsafe extern "C" fn(*mut c_void) -> f32>,
    pub length: Option<unsafe extern "C" fn(*mut c_void, *const u32, usize) -> f32>,
    pub clipboard: Option<unsafe extern "C" fn(*mut c_void, *mut usize) -> *const u32>,
    pub copy: Option<unsafe extern "C" fn(*mut c_void, *const u32, usize)>,
    pub to_float: Option<unsafe extern "C" fn(*mut c_void, *const u32, usize, i32) -> f32>,
    pub float_text: Option<unsafe extern "C" fn(*mut c_void, f32, i32, *mut usize) -> *const u32>,
    pub emit:
        Option<unsafe extern "C" fn(*mut c_void, i32, *const u32, usize, *const u32, usize, bool)>,
}

impl TextHost for &TextHostC {
    fn glyph(&mut self, num: i32) -> Option<f32> {
        let f = self.glyph?;
        let mut adv = 0.0;
        // SAFETY: the C++ callback contract of `TextHostC`.
        unsafe { f(self.ctx, num, &mut adv) }.then_some(adv)
    }

    fn local_x(&mut self) -> f32 {
        // SAFETY: as above.
        self.local_x.map_or(0.0, |f| unsafe { f(self.ctx) })
    }

    fn length(&mut self, text: &[u32]) -> f32 {
        // SAFETY: as above; `text` is valid for the call.
        self.length
            .map_or(0.0, |f| unsafe { f(self.ctx, text.as_ptr(), text.len()) })
    }

    fn clipboard(&mut self) -> Vec<u32> {
        let mut n = 0usize;
        // SAFETY: as above; the returned buffer is valid until the next callback.
        self.clipboard
            .map(|f| unsafe { items(f(self.ctx, &mut n), n) }.to_vec())
            .unwrap_or_default()
    }

    fn copy(&mut self, text: &[u32]) {
        if let Some(f) = self.copy {
            // SAFETY: as above.
            unsafe { f(self.ctx, text.as_ptr(), text.len()) }
        }
    }

    fn to_float(&mut self, text: &[u32], decimals: i32) -> f32 {
        // SAFETY: as above.
        self.to_float.map_or(0.0, |f| unsafe {
            f(self.ctx, text.as_ptr(), text.len(), decimals)
        })
    }

    fn float_text(&mut self, v: f32, decimals: i32) -> Vec<u32> {
        let mut n = 0usize;
        // SAFETY: as above; the returned buffer is valid until the next callback.
        self.float_text
            .map(|f| unsafe { items(f(self.ctx, v, decimals, &mut n), n) }.to_vec())
            .unwrap_or_default()
    }

    fn emit(&mut self, e: Event) {
        let Some(f) = self.emit else { return };
        // SAFETY: as above.
        unsafe {
            match &e {
                Event::SetText { text, nested } => {
                    let b: &[u32] = nested.as_deref().unwrap_or(&[]);
                    f(
                        self.ctx,
                        0,
                        text.as_ptr(),
                        text.len(),
                        b.as_ptr(),
                        b.len(),
                        nested.is_some(),
                    );
                }
                Event::Enter => f(self.ctx, 1, std::ptr::null(), 0, std::ptr::null(), 0, false),
            }
        }
    }
}

/// Everything one text call needs; the text is copied, the state is updated in place.
#[repr(C)]
pub struct TextCall {
    pub st: *mut TextState,
    pub env: TextEnv,
    pub host: *const TextHostC,
    pub text: *const u32,
    pub text_len: usize,
    pub illegal: *const u32,
    pub illegal_len: usize,
}

/// # Safety
/// `c` and everything it points to valid for the call.
unsafe fn with_edit<R>(
    c: *const TextCall,
    default: R,
    f: impl FnOnce(&mut Edit<'_, &TextHostC>) -> R,
) -> R {
    // SAFETY: caller contract.
    unsafe {
        let Some(c) = rd(c) else { return default };
        let (Some(st), Some(host)) = (rw(c.st), rd(c.host)) else {
            return default;
        };
        let mut h = host;
        let mut e = Edit {
            st,
            env: c.env,
            text: items(c.text, c.text_len).to_vec(),
            illegal: items(c.illegal, c.illegal_len),
            host: &mut h,
        };
        f(&mut e)
    }
}

fn ret_code(r: Ret) -> i32 {
    match r {
        Ret::False => 0,
        Ret::True => 1,
        Ret::Enter => 2,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_w2_tb_init(numeric: bool) -> TextState {
    TextState::new(numeric)
}

/// # Safety
/// Pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_tb_layout(
    st: *const TextState,
    size_x: f32,
    b: *const Vector2f,
    font_h: f32,
    button_w: f32,
    out_h: *mut f32,
    out_w: *mut f32,
) {
    // SAFETY: caller contract; `b` holds the active sizes of borders 0, 1 and 2.
    unsafe {
        if let (Some(s), [b0, b1, b2]) = (rd(st), items(b, 3)) {
            let (h, w) = wt::layout(s, size_x, *b0, *b1, *b2, font_h, button_w);
            wr(out_h, h);
            wr(out_w, w);
        }
    }
}

/// # Safety
/// See [`TextCall`].
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_tb_change_text(c: *const TextCall, in_set_text: bool) {
    // SAFETY: caller contract.
    unsafe { with_edit(c, (), |e| e.on_change_text(in_set_text)) }
}

/// # Safety
/// See [`TextCall`].
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_tb_set_max_len(c: *const TextCall, len: i32) {
    // SAFETY: caller contract.
    unsafe { with_edit(c, (), |e| e.set_max_text_length(len)) }
}

/// # Safety
/// See [`TextCall`].
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_tb_set_selected(c: *const TextCall, start: i32, count: i32) {
    // SAFETY: caller contract.
    unsafe { with_edit(c, (), |e| e.set_selected_text(start, count)) }
}

/// # Safety
/// See [`TextCall`].
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_tb_set_numeric(c: *const TextCall, v: f32) {
    // SAFETY: caller contract.
    unsafe { with_edit(c, (), |e| e.set_numeric_value(v)) }
}

/// # Safety
/// See [`TextCall`].
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_tb_get_numeric(c: *const TextCall) -> f32 {
    // SAFETY: caller contract.
    unsafe { with_edit(c, 0.0, |e| e.numeric_value()) }
}

/// # Safety
/// See [`TextCall`].
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_tb_button_pressed(
    c: *const TextCall,
    which: i32,
    enabled: bool,
) -> i32 {
    // SAFETY: caller contract.
    unsafe { with_edit(c, 0, |e| ret_code(e.button_pressed(which, enabled))) }
}

/// Returns true when the `TextBoxEnter` message was emitted.
///
/// # Safety
/// See [`TextCall`].
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_tb_enter(c: *const TextCall) -> bool {
    // SAFETY: caller contract.
    unsafe { with_edit(c, false, |e| e.on_enter()) }
}

/// # Safety
/// See [`TextCall`].
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_tb_mouse_move(c: *const TextCall) -> i32 {
    // SAFETY: caller contract.
    unsafe { with_edit(c, 0, |e| ret_code(e.mouse_move())) }
}

/// # Safety
/// See [`TextCall`]; `attention` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_tb_mouse_down(
    c: *const TextCall,
    val: i32,
    attention: *mut bool,
) -> i32 {
    // SAFETY: caller contract.
    unsafe {
        with_edit(c, 0, |e| {
            let (r, a) = e.mouse_down(val);
            wr(attention, a);
            ret_code(r)
        })
    }
}

/// Returns true when C++ must restore the attention.
///
/// # Safety
/// See [`TextCall`]; outputs valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_tb_mouse_up(
    c: *const TextCall,
    val: i32,
    marker: *mut i32,
    sel: *mut i32,
) -> bool {
    // SAFETY: caller contract.
    unsafe {
        with_edit(c, false, |e| match e.mouse_up(val) {
            Some((m, s)) => {
                wr(marker, m);
                wr(sel, s);
                true
            }
            None => false,
        })
    }
}

/// # Safety
/// See [`TextCall`].
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_tb_mouse_up_restore(c: *const TextCall, marker: i32, sel: i32) {
    // SAFETY: caller contract.
    unsafe { with_edit(c, (), |e| e.mouse_up_restore(marker, sel)) }
}

/// # Safety
/// See [`TextCall`].
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_tb_double_click(c: *const TextCall, val: i32) {
    // SAFETY: caller contract.
    unsafe { with_edit(c, (), |e| e.double_click(val)) }
}

/// # Safety
/// See [`TextCall`].
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_tb_got_focus(c: *const TextCall) {
    // SAFETY: caller contract.
    unsafe { with_edit(c, (), |e| e.got_focus()) }
}

/// # Safety
/// See [`TextCall`].
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_tb_got_tab_focus(c: *const TextCall) {
    // SAFETY: caller contract.
    unsafe { with_edit(c, (), |e| e.got_tab_focus()) }
}

/// # Safety
/// See [`TextCall`].
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_tb_lost_focus(c: *const TextCall) {
    // SAFETY: caller contract.
    unsafe { with_edit(c, (), |e| e.lost_focus()) }
}

/// # Safety
/// See [`TextCall`]; `anim` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_tb_key(
    c: *const TextCall,
    key: i32,
    modifier: i32,
    unicode: u32,
    anim: *mut bool,
) -> i32 {
    // SAFETY: caller contract.
    unsafe {
        with_edit(c, 0, |e| {
            let (r, o) = e.key_press(key, modifier, unicode);
            wr(anim, o.reset_marker_anim);
            ret_code(r)
        })
    }
}

/// Returns true when C++ must create the pop-up keyboard.
///
/// # Safety
/// See [`TextCall`].
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_tb_ui_button(
    c: *const TextCall,
    has_focus: bool,
    val: i32,
) -> bool {
    // SAFETY: caller contract.
    unsafe { with_edit(c, false, |e| e.ui_button(has_focus, val)) }
}

/// # Safety
/// See [`TextCall`].
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_tb_ui_keyboard_open(c: *const TextCall) {
    // SAFETY: caller contract.
    unsafe { with_edit(c, (), |e| e.ui_keyboard_open()) }
}

/// # Safety
/// See [`TextCall`]; `global` and `out` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_w2_tb_draw(
    c: *const TextCall,
    global: *const Vector3f,
    out: *mut TextDraw,
) {
    // SAFETY: caller contract.
    unsafe {
        if let Some(g) = rd(global) {
            with_edit(c, (), |e| wr(out, e.draw(*g)));
        }
    }
}

const _: () = {
    assert!(std::mem::size_of::<SliderState>() == 88);
    assert!(std::mem::size_of::<SliderDraw>() == 40);
    assert!(std::mem::size_of::<PosSize>() == 20);
    assert!(std::mem::size_of::<TabFrameState>() == 40);
    assert!(std::mem::size_of::<TabItem>() == 8);
    assert!(std::mem::size_of::<TabArrows>() == 3);
    assert!(std::mem::size_of::<TabLayout>() == 32);
    assert!(std::mem::size_of::<TabInit>() == 24);
    assert!(std::mem::size_of::<ListState>() == 36);
    assert!(std::mem::size_of::<ListUpdate>() == 12);
    assert!(std::mem::size_of::<SliderSet>() == 8);
    assert!(std::mem::size_of::<SelAction>() == 16);
    assert!(std::mem::size_of::<RangePlan>() == 16);
    assert!(std::mem::size_of::<ListMouse>() == 24);
    assert!(std::mem::size_of::<ListKey>() == 24);
    assert!(std::mem::size_of::<ListArrow>() == 16);
    assert!(std::mem::size_of::<ListButton>() == 8);
    assert!(std::mem::size_of::<RowOp>() == 28);
    assert!(std::mem::size_of::<ComboState>() == 40);
    assert!(std::mem::size_of::<ComboOpen>() == 24);
    assert!(std::mem::size_of::<UiButton>() == 8);
    assert!(std::mem::size_of::<ComboLayout>() == 20);
    assert!(std::mem::size_of::<ComboDraw>() == 40);
    assert!(std::mem::size_of::<TextState>() == 80);
    assert!(std::mem::size_of::<TextEnv>() == 28);
    assert!(std::mem::size_of::<TextDraw>() == 88);
    assert!(std::mem::size_of::<TextHostC>() == 72);
    assert!(std::mem::size_of::<TextCall>() == 80);
};
