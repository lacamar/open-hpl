//! `iWidgetListBoxBase`, `cWidgetListBox` and `cWidgetMultiPropertyListBox`: slider, selection list,
//! cursor, input handling and drawing. State is [`ListBase`], embedded in the C++ widget
//! (`gui/RustGuiH20.h`); items, the slider and draw calls are reached through the `h20_host` calls.

use super::h19_widget::Base;
use super::h20_host as hh;
use super::set::{Msg, W};
use super::widget::{add3, v2};
use super::widgets_list::{self as wl, ListState, SliderSet};
use crate::math::{sub3, v3};
use crate::types::{Vector2f, Vector3f};
use std::ffi::c_void;

pub const MSG_SELECTION_CHANGE: i32 = 26;
pub const MSG_SELECTION_DOUBLE_CLICK: i32 = 27;
pub const MSG_CHECK_CHANGE: i32 = 13;
pub const MSG_SLIDER_MOVE: i32 = 24;
const MOD_SHIFT: i32 = 2;
const ALIGN_LEFT: i32 = 0;
const ALIGN_RIGHT: i32 = 1;
const GFX_BG: usize = 0;
const GFX_SEL: usize = 1;
const GFX_BORDERS: usize = 2;
const GFX_CORNERS: usize = 6;
pub const KIND_LIST: i32 = 0;
pub const KIND_MULTI: i32 = 1;
const ICON_SIZE: f32 = 15.0;
const BG_COLOR: [f32; 4] = [0.82, 0.81, 0.79, 1.0];

pub fn gfx_id(name: &str) -> i32 {
    super::guicore::GFXS
        .iter()
        .position(|g| *g == name)
        .map_or(0, |p| p as i32)
}

pub fn attr_id(name: &str) -> i32 {
    super::guicore::ATTRIBUTES
        .iter()
        .position(|g| *g == name)
        .map_or(0, |p| p as i32)
}

#[repr(C)]
pub struct Column {
    pub label: W,
    pub index: i32,
    pub align: i32,
}

/// Fields shared with the C++ list box.
#[repr(C)]
pub struct ListBase {
    pub list: ListState,
    pub selected: *mut Vec<i32>,
    pub columns: *mut Vec<Column>,
    pub slider: W,
    pub gfx: [usize; 10],
    pub bg_z: f32,
    pub slider_w: f32,
    pub clearing: bool,
    pub kind: i32,
    pub this: W,
    pub container: usize,
    pub base: *mut Base,
}

const _: () = assert!(std::mem::size_of::<ListBase>() == 184);

type P = *mut ListBase;

/// # Safety
/// `lb` points to a live `ListBase`; the access is short and no host call is made inside.
unsafe fn lst<R>(lb: P, f: impl FnOnce(&mut ListState) -> R) -> R {
    // SAFETY: caller contract.
    f(unsafe { &mut (*lb).list })
}

/// # Safety
/// As [`lst`].
unsafe fn sel<R>(lb: P, f: impl FnOnce(&mut Vec<i32>) -> R) -> Option<R> {
    // SAFETY: caller contract.
    unsafe { (*lb).selected.as_mut() }.map(f)
}

/// # Safety
/// `lb` is live.
unsafe fn size(lb: P) -> Vector2f {
    // SAFETY: caller contract.
    unsafe { (*lb).base.as_ref() }.map_or(Vector2f { x: 0.0, y: 0.0 }, |b| b.size)
}

/// # Safety
/// `lb` is live.
unsafe fn this(lb: P) -> W {
    // SAFETY: caller contract.
    unsafe { (*lb).this }
}

/// # Safety
/// `lb` is live.
unsafe fn num(lb: P) -> i32 {
    // SAFETY: caller contract.
    hh::item_num(unsafe { (*lb).container })
}

/// # Safety
/// `lb` is live.
unsafe fn item(lb: P, idx: i32) -> usize {
    // SAFETY: caller contract.
    hh::item(unsafe { (*lb).container }, idx)
}

/// # Safety
/// `lb` is live and uninitialised memory of the right size; `base` outlives it.
pub unsafe fn init(lb: P, this: W, container: usize, base: *mut Base, kind: i32) {
    let slider_w = hh::skin_attr(this, attr_id("ListBoxSliderWidth")).x;
    // SAFETY: caller contract; the struct is written whole.
    unsafe {
        lb.write(ListBase {
            list: ListState::new(),
            selected: Box::into_raw(Box::default()),
            columns: Box::into_raw(Box::default()),
            slider: 0,
            gfx: [0; 10],
            bg_z: -0.5,
            slider_w,
            clearing: false,
            kind,
            this,
            container,
            base,
        });
        if kind == KIND_MULTI {
            (*lb).list.num_blank_rows = 1;
        }
    }
}

/// # Safety
/// `lb` was initialised and is not used afterwards.
pub unsafe fn free(lb: P) {
    // SAFETY: caller contract; the boxes came from `init`.
    unsafe {
        drop(Box::from_raw(std::mem::replace(
            &mut (*lb).selected,
            std::ptr::null_mut(),
        )));
        drop(Box::from_raw(std::mem::replace(
            &mut (*lb).columns,
            std::ptr::null_mut(),
        )));
    }
}

/// # Safety
/// `lb` is live.
pub unsafe fn destroy_slider(lb: P) {
    // SAFETY: caller contract.
    let (w, s) = unsafe { ((*lb).this, (*lb).slider) };
    hh::destroy_widget(w, s);
}

/// # Safety
/// `lb` is live; `bg`.. as `SetSelectedItem`.
pub unsafe fn set_selected_item(
    lb: P,
    x: i32,
    move_list: bool,
    callback: bool,
    ty: i32,
    clear_previous: bool,
) {
    // SAFETY: caller contract for every access below.
    unsafe {
        if clear_previous || !lst(lb, |l| l.allow_multi) {
            clear_selection(lb);
        }
        if move_list {
            set_cursor_pos(lb, x, true, true);
        }
        let it = item(lb, x);
        let selected = it != 0 && hh::item_selected(it);
        let action = lst(lb, |l| l.select_action(ty, x, it != 0, selected));
        if action.kind == 1 {
            hh::item_set_selected(it, action.flag);
        } else if action.kind == 2 {
            select_range(lb, action.start, action.end);
        }
        if callback {
            hh::process(this(lb), MSG_SELECTION_CHANGE, x, None);
        }
    }
}

/// # Safety
/// `lb` is live.
pub unsafe fn selected_item(lb: P) -> i32 {
    // SAFETY: caller contract.
    unsafe { sel(lb, |s| s.last().copied()) }
        .flatten()
        .unwrap_or(-1)
}

/// # Safety
/// `lb` is live.
pub unsafe fn multi_selection(lb: P, idx: i32) -> i32 {
    // SAFETY: caller contract.
    unsafe { sel(lb, |s| wl::multi_selection(s, idx)) }.unwrap_or(-1)
}

/// # Safety
/// `lb` is live.
pub unsafe fn multi_selection_num(lb: P) -> i32 {
    // SAFETY: caller contract.
    unsafe { sel(lb, |s| s.len() as i32) }.unwrap_or(0)
}

/// # Safety
/// `lb` is live.
pub unsafe fn clear_selection(lb: P) {
    // SAFETY: caller contract.
    unsafe {
        (*lb).clearing = true;
        let all = sel(lb, |s| s.clone()).unwrap_or_default();
        for i in all {
            let it = item(lb, i);
            if it != 0 {
                hh::item_set_selected(it, false);
            }
        }
        sel(lb, |s| s.clear());
        (*lb).clearing = false;
    }
}

/// # Safety
/// `lb` is live.
pub unsafe fn item_start_pos(lb: P) -> Vector3f {
    // SAFETY: caller contract.
    unsafe {
        let g = hh::global(this(lb));
        if (*lb).kind == KIND_MULTI {
            let fy = hh::font_size(this(lb)).y;
            let rows = (*lb).list.num_blank_rows as f32;
            add3(g, v3(0.0, rows * (fy + 2.0), (*lb).bg_z + 0.02))
        } else {
            add3(g, v3(0.0, 2.0, 0.01))
        }
    }
}

/// # Safety
/// `lb` is live.
unsafe fn select_range(lb: P, start: i32, end: i32) {
    // SAFETY: caller contract.
    unsafe {
        let r = wl::range_plan(start, end, num(lb));
        for i in 0..r.count {
            let it = item(lb, r.first + i * r.step);
            if it != 0 {
                hh::item_set_selected(it, true);
            }
        }
        if r.end_valid {
            let it = item(lb, end);
            if it != 0 {
                hh::item_set_selected(it, true);
            }
        }
    }
}

/// # Safety
/// `lb` is live.
pub unsafe fn update_properties(lb: P) {
    // SAFETY: caller contract.
    unsafe {
        let w = this(lb);
        let sz = size(lb);
        let fy = hh::font_size(w).y;
        let u = lst(lb, |l| l.update(sz.y, fy, num(lb)));
        let slider = (*lb).slider;
        if slider == 0 {
            return;
        }
        if u.big {
            hh::slider_set_bar_size(slider, u.bar_value_size);
            hh::slider_set_max(slider, u.max_value);
        } else {
            hh::slider_set_max(slider, u.max_value);
            hh::slider_set_bar_size(slider, u.bar_value_size);
        }
        hh::slider_set_button_add(slider, 1);
        let click = lst(lb, |l| l.max_items).wrapping_sub(1);
        hh::slider_set_click_add(slider, click);
        let items = num(lb);
        sel(lb, |s| {
            let n = wl::prune_selection(s, items);
            s.truncate(n);
        });
        hh::slider_set_value(slider, selected_item(lb), Some(true));
        if lst(lb, |l| l.num_items) == 0 {
            set_cursor_pos(lb, 0, true, true);
        }
    }
}

/// # Safety
/// `lb` is live.
pub unsafe fn on_change_size(lb: P) {
    // SAFETY: caller contract.
    unsafe {
        let slider = (*lb).slider;
        if slider != 0 {
            let sz = size(lb);
            hh::set_size(
                slider,
                Vector2f {
                    x: (*lb).slider_w,
                    y: sz.y,
                },
            );
            hh::set_position(slider, v3(sz.x - (*lb).slider_w, 0.0, 0.2));
        }
        update_properties(lb);
    }
}

extern "C" fn move_slider(obj: *mut c_void, _w: W, d: *const Msg) -> bool {
    let lb = obj.cast::<ListBase>();
    // SAFETY: `obj` is the `ListBase` registered in `on_init`; `d` is the message reference.
    unsafe {
        if let Some(d) = d.as_ref() {
            (*lb).list.first_item = d.val();
        }
    }
    true
}

/// # Safety
/// `lb` is live.
pub unsafe fn on_init(lb: P) {
    // SAFETY: caller contract.
    unsafe {
        let w = this(lb);
        let s = hh::create_slider(w);
        (*lb).slider = s;
        hh::add_callback(
            s,
            MSG_SLIDER_MOVE,
            lb as usize,
            move_slider as *const () as usize,
        );
        on_change_size(lb);
    }
}

/// # Safety
/// `lb` is live.
pub unsafe fn add_item_to_selection(lb: P, it: usize) {
    let idx = hh::item_index(it);
    // SAFETY: caller contract.
    unsafe { sel(lb, |s| s.push(idx)) };
}

/// # Safety
/// `lb` is live.
pub unsafe fn remove_item_from_selection(lb: P, it: usize) {
    // SAFETY: caller contract.
    unsafe {
        if !(*lb).clearing {
            let idx = hh::item_index(it);
            sel(lb, |s| s.retain(|v| *v != idx));
        }
    }
}

/// # Safety
/// `lb` is live.
pub unsafe fn on_load_graphics(lb: P) {
    // SAFETY: caller contract.
    let w = unsafe { this(lb) };
    let g = |n: &str| hh::skin_gfx(w, gfx_id(n));
    let table = [
        g("ListBoxBackground"),
        g("TextBoxSelectedTextBack"),
        g("FrameBorderRight"),
        g("FrameBorderLeft"),
        g("FrameBorderUp"),
        g("FrameBorderDown"),
        g("FrameCornerLU"),
        g("FrameCornerRU"),
        g("FrameCornerRD"),
        g("FrameCornerLD"),
    ];
    // SAFETY: caller contract.
    unsafe { (*lb).gfx = table };
}

fn borders(lb: &ListBase) -> ([usize; 4], [usize; 4]) {
    let b = [
        lb.gfx[GFX_BORDERS],
        lb.gfx[GFX_BORDERS + 1],
        lb.gfx[GFX_BORDERS + 2],
        lb.gfx[GFX_BORDERS + 3],
    ];
    let c = [
        lb.gfx[GFX_CORNERS],
        lb.gfx[GFX_CORNERS + 1],
        lb.gfx[GFX_CORNERS + 2],
        lb.gfx[GFX_CORNERS + 3],
    ];
    (b, c)
}

/// # Safety
/// `lb` is live.
pub unsafe fn on_draw(lb: P) {
    // SAFETY: caller contract.
    unsafe {
        let w = this(lb);
        let sz = size(lb);
        let g = hh::global(w);
        let bg_z = (*lb).bg_z;
        hh::draw_gfx(w, (*lb).gfx[GFX_BG], add3(g, v3(0.0, 0.0, bg_z)), sz, None);
        let (b, c) = borders(&*lb);
        let c0 = hh::gfx_active_size(c[0]);
        let c2 = hh::gfx_active_size(c[2]);
        let g = hh::global(w);
        let pos = sub3(g, v3(c0.x, c0.y, 0.0));
        let full = v2(sz.x + c0.x + c2.x, sz.y + c0.y + c2.y);
        hh::draw_borders(w, 0, (&b, &c), pos, full, None);
    }
}

/// # Safety
/// `lb` is live; `clip` is the `cGuiClipRegion*`.
pub unsafe fn on_draw_after_clip(lb: P, clip: usize) {
    // SAFETY: caller contract.
    unsafe {
        let w = this(lb);
        let sz = size(lb);
        let sw = (*lb).slider_w;
        let region = hh::clip_child(clip, hh::global(w), v2(sz.x - sw, sz.y - 0.0));
        hh::set_clip(w, region);
        draw_items(lb, region);
        if hh::has_focus(w) {
            let l = lst(lb, |l| *l);
            if l.cursor_pos.wrapping_sub(l.first_item) != -1 {
                let start = item_start_pos(lb);
                let fy = hh::font_size(w).y;
                if let Some(lines) = wl::cursor_lines(&l, start, sz.x, sw, fy) {
                    let rect = hh::gfx_rect();
                    for (p, s) in lines {
                        hh::draw_gfx(w, rect, p, s, Some([0.3, 0.3, 0.3, 0.4]));
                    }
                }
            }
        }
        hh::set_clip(w, clip);
    }
}

/// # Safety
/// `lb` is live.
unsafe fn draw_items(lb: P, clip: usize) {
    // SAFETY: caller contract.
    unsafe {
        if (*lb).kind == KIND_MULTI {
            draw_multi(lb, clip);
        } else {
            draw_plain(lb);
        }
    }
}

/// # Safety
/// `lb` is live.
unsafe fn draw_plain(lb: P) {
    // SAFETY: caller contract.
    unsafe {
        let w = this(lb);
        let sz = size(lb);
        let l = lst(lb, |l| *l);
        let bg_z = (*lb).bg_z;
        let start = add3(item_start_pos(lb), v3(3.0, 0.0, bg_z + 0.1));
        let fy = hh::font_size(w).y;
        let n = num(lb);
        let mut rows = Vec::new();
        let mut i = l.first_item;
        while i < n {
            if i.wrapping_sub(l.first_item) > l.max_items {
                break;
            }
            let it = item(lb, i);
            let mut flags = 0u8;
            if it == 0 {
                flags = wl::ROW_NULL;
            } else {
                if !hh::item_selectable(it) {
                    flags |= wl::ROW_UNSELECTABLE;
                }
                if hh::item_selected(it) {
                    flags |= wl::ROW_SELECTED;
                }
            }
            rows.push(flags);
            i += 1;
        }
        let mut ops = vec![wl::RowOp::default(); rows.len() * 3 + 1];
        let count = wl::list_rows(
            l.first_item,
            l.max_items,
            n,
            start,
            sz.x,
            fy,
            &rows,
            &mut ops,
        );
        for op in ops.iter().take(count) {
            match op.kind {
                wl::OP_BACKGROUND => {
                    hh::draw_gfx(w, (*lb).gfx[GFX_BG], op.pos, op.size, Some(BG_COLOR));
                }
                wl::OP_SELECTION => hh::draw_gfx(w, (*lb).gfx[GFX_SEL], op.pos, op.size, None),
                wl::OP_TEXT_HIGHLIGHT => {
                    let t = hh::item_text(item(lb, op.item));
                    hh::draw_text(w, t, op.pos, ALIGN_LEFT, true);
                }
                wl::OP_TEXT => {
                    let t = hh::item_text(item(lb, op.item));
                    hh::draw_text(w, t, op.pos, ALIGN_LEFT, false);
                }
                _ => {}
            }
        }
    }
}

/// # Safety
/// `lb` is live.
unsafe fn draw_multi(lb: P, clip: usize) {
    // SAFETY: caller contract.
    unsafe {
        let w = this(lb);
        let sz = size(lb);
        let bg_z = (*lb).bg_z;
        let fy = hh::font_size(w).y;
        let mut pos = hh::global(w);
        pos.z += bg_z + 0.01;
        hh::draw_gfx(
            w,
            (*lb).gfx[GFX_BG],
            pos,
            v2(sz.x, fy + 2.0),
            Some(BG_COLOR),
        );
        let cols: Vec<(W, i32, i32)> = (*lb)
            .columns
            .as_ref()
            .map(|c| c.iter().map(|c| (c.label, c.index, c.align)).collect())
            .unwrap_or_default();
        let mut regions = Vec::new();
        for (label, _, _) in &cols {
            let p = hh::global(*label);
            let s = hh::size(*label);
            regions.push(hh::clip_child(clip, p, v2(s.x, sz.y)));
        }
        let l = lst(lb, |l| *l);
        let n = num(lb);
        pos = item_start_pos(lb);
        // size_t loop in the C++: negative first_item or max_items wrap
        let first = i64::from(l.first_item) as u64;
        let max = i64::from(l.max_items) as u64;
        let mut i = first;
        while i < n as u64 {
            if i.wrapping_sub(first) > max {
                break;
            }
            let it = item(lb, i as i32);
            let selected = hh::item_selected(it);
            pos.x = hh::global(w).x;
            if selected {
                hh::draw_gfx(
                    w,
                    (*lb).gfx[GFX_SEL],
                    sub3(pos, v3(0.0, 0.0, 0.01)),
                    v2(sz.x - (*lb).slider_w, fy + 2.0),
                    None,
                );
            }
            for (j, (label, index, align)) in cols.iter().enumerate() {
                pos.x = hh::global(*label).x;
                if *align == ALIGN_RIGHT {
                    pos.x += hh::size(*label).x - 10.0;
                }
                let prop = hh::item_prop(it, *index);
                if prop == 0 {
                    continue;
                }
                hh::set_clip(w, regions.get(j).copied().unwrap_or(0));
                if hh::prop_is_text(prop) {
                    hh::draw_text(w, hh::prop_text(prop), pos, *align, selected);
                } else {
                    let gfx = hh::prop_gfx(prop);
                    if gfx != 0 {
                        hh::draw_gfx(w, gfx, pos, v2(ICON_SIZE, ICON_SIZE), None);
                    }
                }
                hh::set_clip(w, clip);
            }
            pos.y += fy + 2.0;
            i += 1;
        }
    }
}

/// # Safety
/// `lb` is live.
pub unsafe fn apply_cursor(lb: P, first: SliderSet, move_list: bool) {
    // SAFETY: caller contract.
    unsafe {
        let slider = (*lb).slider;
        if first.set {
            hh::slider_set_value(slider, first.value, None);
        }
        let second = lst(lb, |l| l.cursor_step2(move_list));
        if second.set {
            hh::slider_set_value(slider, second.value, None);
        }
    }
}

/// # Safety
/// `lb` is live.
pub unsafe fn set_cursor_pos(lb: P, pos: i32, move_list: bool, clamp: bool) {
    // SAFETY: caller contract.
    unsafe {
        let first = lst(lb, |l| l.cursor_step1(pos, move_list, clamp));
        apply_cursor(lb, first, move_list);
    }
}

/// # Safety
/// `lb` is live; `d` a message.
pub unsafe fn on_mouse_down(lb: P, d: &Msg) -> bool {
    // SAFETY: caller contract.
    unsafe {
        let w = this(lb);
        let slider = (*lb).slider;
        let sv = hh::slider_value(slider);
        let wheel = ListState::wheel(d.val());
        if wheel == 0 {
            hh::slider_set_value(slider, sv.wrapping_sub(5), None);
            return true;
        }
        if wheel == 1 {
            hh::slider_set_value(slider, sv.wrapping_add(5), None);
            return true;
        }
        let local = hh::world_to_local(w, d.pos());
        let modifier = d.modifier();
        let fy = hh::font_size(w).y;
        let mouse = lst(lb, |l| l.mouse_down(local.y, fy, modifier));
        if !mouse.proceed {
            return false;
        }
        apply_cursor(lb, mouse.cursor, true);
        let shift = modifier & MOD_SHIFT != 0;
        let cursor = lst(lb, |l| l.mouse_down_finish(shift, mouse.index));
        let it = item(lb, cursor);
        let skip = multi_selection_num(lb) == 1 && it != 0 && hh::item_selected(it);
        if !skip {
            set_selected_item(lb, cursor, false, true, mouse.ty, mouse.clear);
        }
        true
    }
}

/// # Safety
/// `lb` is live.
pub unsafe fn on_mouse_double_click(lb: P, d: &Msg) -> bool {
    // SAFETY: caller contract.
    unsafe {
        let w = this(lb);
        let local = hh::world_to_local(w, d.pos());
        let sz = size(lb);
        let sw = hh::size((*lb).slider).x;
        let (ret, fire) = lst(lb, |l| l.double_click(local.x, sz.x, sw));
        if fire {
            hh::process(w, MSG_SELECTION_DOUBLE_CLICK, 0, Some(d));
        }
        ret
    }
}

/// # Safety
/// `lb` is live.
pub unsafe fn on_key_press(lb: P, d: &Msg) -> bool {
    // SAFETY: caller contract.
    unsafe {
        let m = d.modifier();
        let key = lst(lb, |l| l.key_press(d.key(), m));
        if key.arrow {
            apply_cursor(lb, key.cursor, true);
            let finish = lst(lb, |l| l.key_press_finish(m));
            if finish.select {
                let cp = lst(lb, |l| l.cursor_pos);
                set_selected_item(lb, cp, false, true, finish.ty, finish.clear);
            }
            return true;
        }
        if key.select {
            let cp = lst(lb, |l| l.cursor_pos);
            set_selected_item(lb, cp, false, true, key.ty, key.clear);
            return true;
        }
        false
    }
}

/// # Safety
/// `lb` is live.
pub unsafe fn on_ui_arrow_press(lb: P, d: &Msg) -> bool {
    // SAFETY: caller contract.
    unsafe {
        let arrow = lst(lb, |l| l.ui_arrow(d.val()));
        if !arrow.handled {
            return false;
        }
        apply_cursor(lb, arrow.cursor, true);
        let cp = lst(lb, |l| l.cursor_pos);
        set_selected_item(lb, cp, true, true, wl::SEL_SELECT, true);
        if arrow.unlock {
            hh::process(this(lb), MSG_CHECK_CHANGE, 0, Some(d));
            lst(lb, |l| l.locked = false);
        }
        true
    }
}

/// # Safety
/// `lb` is live.
pub unsafe fn on_ui_button_press(lb: P, d: &Msg) -> bool {
    // SAFETY: caller contract.
    unsafe {
        let focus = hh::has_focus(this(lb));
        let selected = selected_item(lb);
        let b = lst(lb, |l| l.ui_button(focus, d.val(), selected));
        if b.ret == 0 {
            return false;
        }
        if b.ret == 1 {
            return true;
        }
        if b.init_select {
            set_cursor_pos(lb, 0, true, true);
            let cp = lst(lb, |l| l.cursor_pos);
            set_selected_item(lb, cp, true, true, wl::SEL_SELECT, true);
        }
        if b.check_locked && hh::process(this(lb), MSG_CHECK_CHANGE, 0, Some(d)) {
            lst(lb, |l| l.locked = false);
            return true;
        }
        lst(lb, |l| l.locked = true);
        true
    }
}

/// # Safety
/// `lb` is live.
pub unsafe fn on_ui_button_double_press(lb: P, d: &Msg) -> bool {
    // SAFETY: caller contract.
    unsafe {
        if hh::has_focus(this(lb)) {
            if d.val() == super::widget::UI_BUTTON_PRIMARY {
                lst(lb, |l| l.locked = false);
                return hh::process(this(lb), MSG_SELECTION_DOUBLE_CLICK, 0, Some(d));
            }
            return true;
        }
        false
    }
}

/// # Safety
/// `lb` is live.
pub unsafe fn on_ui_button_release(lb: P) -> bool {
    // SAFETY: caller contract.
    hh::has_focus(unsafe { this(lb) })
}

/// # Safety
/// `lb` is live; multi-property list only.
pub unsafe fn add_column(lb: P, name: (*const u32, usize), index: i32, align: i32) {
    // SAFETY: caller contract.
    unsafe {
        let w = this(lb);
        let label = hh::create_label(w, name, (*lb).bg_z + 0.5);
        if let Some(c) = (*lb).columns.as_mut() {
            c.push(Column {
                label,
                index,
                align,
            });
        }
        let count = (*lb).columns.as_ref().map_or(0, Vec::len) as i32;
        set_column_width(lb, count - 1, 0.0);
        update_columns(lb);
    }
}

/// # Safety
/// `lb` is live; multi-property list only.
pub unsafe fn set_column_width(lb: P, idx: i32, width: f32) {
    // SAFETY: caller contract.
    unsafe {
        let label = (*lb)
            .columns
            .as_ref()
            .and_then(|c| usize::try_from(idx).ok().and_then(|i| c.get(i)))
            .map(|c| c.label);
        let Some(label) = label else { return };
        let mut s = hh::size(label);
        let text_w = hh::text_length(this(lb), label);
        s.x = if width < text_w { text_w } else { width };
        hh::set_size(label, s);
        update_columns(lb);
    }
}

/// # Safety
/// `lb` is live.
pub unsafe fn update_columns(lb: P) {
    // SAFETY: caller contract.
    unsafe {
        let labels: Vec<W> = (*lb)
            .columns
            .as_ref()
            .map(|c| c.iter().map(|c| c.label).collect())
            .unwrap_or_default();
        let mut pos = v3(0.0, 0.0, 0.02);
        for l in labels {
            hh::set_position(l, pos);
            pos.x += hh::size(l).x;
        }
    }
}

/// # Safety
/// `lb` is live.
pub unsafe fn destroy_columns(lb: P) {
    // SAFETY: caller contract.
    unsafe {
        let labels: Vec<W> = (*lb)
            .columns
            .as_ref()
            .map(|c| c.iter().map(|c| c.label).collect())
            .unwrap_or_default();
        for l in labels {
            hh::destroy_widget(this(lb), l);
        }
        if let Some(c) = (*lb).columns.as_mut() {
            c.clear();
        }
    }
}
