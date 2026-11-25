//! `cWidgetComboBox`: text box, button and slider children, the drop-down menu, selection and
//! input. State is [`Combo`], embedded in the C++ widget (`gui/RustGuiH20.h`).

use super::h19_widget::Base;
use super::h20_host as hh;
use super::h20_list::{MSG_SELECTION_CHANGE, MSG_SLIDER_MOVE, attr_id, gfx_id};
use super::set::{Msg, W};
use super::widget::UI_BUTTON_PRIMARY;
use super::widgets_list::{self as wl, ComboState, SliderSet};
use crate::math::v3;
use crate::types::Vector2f;
use std::ffi::c_void;

const MSG_MOUSE_DOWN: i32 = 1;
const MSG_LOST_FOCUS: i32 = 8;
const MSG_ON_DRAW: i32 = 9;
const MSG_UI_ARROW_PRESS: i32 = 17;
const MSG_UI_BUTTON_PRESS: i32 = 19;
const MSG_UI_BUTTON_RELEASE: i32 = 20;
const GFX_BG: usize = 0;
const GFX_SEL: usize = 1;
const GFX_BORDERS: usize = 2;
const GFX_CORNERS: usize = 6;
const ALIGN_LEFT: i32 = 0;
const MOUSE_LEFT: i32 = 1;

#[repr(C)]
pub struct Combo {
    pub combo: ComboState,
    pub text: W,
    pub button: W,
    pub slider: W,
    pub gfx: [usize; 10],
    pub button_w: f32,
    pub slider_w: f32,
    pub this: W,
    pub container: usize,
    pub base: *mut Base,
}

const _: () = assert!(std::mem::size_of::<Combo>() == 176);

type P = *mut Combo;

/// # Safety
/// `c` is live; no host call inside `f`.
unsafe fn cs<R>(c: P, f: impl FnOnce(&mut ComboState) -> R) -> R {
    // SAFETY: caller contract.
    f(unsafe { &mut (*c).combo })
}

/// # Safety
/// `c` is live.
unsafe fn size(c: P) -> Vector2f {
    // SAFETY: caller contract.
    unsafe { (*c).base.as_ref() }.map_or(Vector2f { x: 0.0, y: 0.0 }, |b| b.size)
}

/// # Safety
/// `c` is live.
unsafe fn this(c: P) -> W {
    // SAFETY: caller contract.
    unsafe { (*c).this }
}

/// # Safety
/// `c` is live.
unsafe fn num(c: P) -> i32 {
    // SAFETY: caller contract.
    hh::item_num(unsafe { (*c).container })
}

/// # Safety
/// `c` is live.
unsafe fn item_text(c: P, idx: i32) -> (*const u32, usize) {
    // SAFETY: caller contract.
    let it = hh::item(unsafe { (*c).container }, idx);
    hh::item_text(it)
}

/// # Safety
/// `c` is uninitialised storage; `base` outlives it.
pub unsafe fn init(c: P, this: W, container: usize, base: *mut Base) {
    let button_w = hh::skin_attr(this, attr_id("ComboBoxButtonWidth")).x;
    let slider_w = hh::skin_attr(this, attr_id("ComboBoxSliderWidth")).x;
    // SAFETY: caller contract; the struct is written whole.
    unsafe {
        c.write(Combo {
            combo: ComboState::new(),
            text: 0,
            button: 0,
            slider: 0,
            gfx: [0; 10],
            button_w,
            slider_w,
            this,
            container,
            base,
        });
    }
}

/// # Safety
/// `c` is live.
pub unsafe fn destroy_children(c: P) {
    // SAFETY: caller contract.
    unsafe {
        for child in [(*c).text, (*c).button, (*c).slider] {
            hh::destroy_widget(this(c), child);
        }
    }
}

/// # Safety
/// `c` is live.
pub unsafe fn set_selected_item(c: P, x: i32, move_list: bool, callback: bool) {
    // SAFETY: caller contract.
    unsafe {
        let items = num(c);
        let Some(s1) = cs(c, |s| s.select_step1(x, move_list, items)) else {
            return;
        };
        apply(c, s1);
        let s2 = cs(c, |s| s.select_step2(move_list));
        apply(c, s2);
        let sel = cs(c, |s| s.selected);
        if sel >= 0 && sel < num(c) {
            hh::set_text(this(c), item_text(c, sel));
        } else {
            hh::set_text(this(c), (std::ptr::null(), 0));
        }
        if callback {
            hh::process(this(c), MSG_SELECTION_CHANGE, sel, None);
        }
    }
}

/// # Safety
/// `c` is live.
unsafe fn apply(c: P, s: SliderSet) {
    if s.set {
        // SAFETY: caller contract.
        hh::slider_set_value(unsafe { (*c).slider }, s.value, None);
    }
}

/// # Safety
/// `c` is live.
pub unsafe fn set_can_edit(c: P, v: bool) {
    // SAFETY: caller contract.
    hh::set_can_edit(unsafe { (*c).text }, v);
}

/// # Safety
/// `c` is live.
pub unsafe fn can_edit(c: P) -> bool {
    // SAFETY: caller contract.
    hh::can_edit(unsafe { (*c).text })
}

/// # Safety
/// `c` is live.
pub unsafe fn set_max_shown_items(c: P, v: i32) {
    // SAFETY: caller contract.
    unsafe {
        cs(c, |s| s.max_items = v);
        update_properties(c);
    }
}

/// # Safety
/// `c` is live.
pub unsafe fn set_default_font_size(c: P, s: Vector2f) {
    hh::set_font_size(unsafe { (*c).text }, s);
}

/// # Safety
/// `c` is live.
pub unsafe fn update_properties(c: P) {
    // SAFETY: caller contract.
    unsafe {
        let fy = hh::font_size(this(c)).y;
        let items = num(c);
        if cs(c, |s| s.update(fy, items)) {
            set_selected_item(c, num(c) - 1, false, true);
        }
        on_change_size(c);
    }
}

/// # Safety
/// `c` is live.
unsafe fn open_menu(c: P) {
    // SAFETY: caller contract.
    unsafe {
        let w = this(c);
        let items = num(c);
        if !cs(c, |s| s.can_open(items)) {
            return;
        }
        hh::push_attention(w);
        hh::push_focused(w);
        hh::push_default_nav(w);
        hh::set_attention(w, w);
        let b1 = hh::gfx_active_size((*c).gfx[GFX_BORDERS + 1]);
        let b2 = hh::gfx_active_size((*c).gfx[GFX_BORDERS + 2]);
        let global_y = hh::global(w).y;
        let popup_z = hh::popup_z(w);
        let virtual_y = hh::virtual_size(w)[1];
        let sz = size(c);
        let open = cs(c, |s| {
            s.open(items, sz, b1, b2, global_y, popup_z, virtual_y)
        });
        let slider = (*c).slider;
        if open.slider {
            hh::set_enabled(slider, true);
            hh::set_visible(slider, true);
            hh::set_position(slider, open.slider_pos);
            hh::set_size(slider, open.slider_size);
        }
        hh::set_focused(w, w);
    }
}

/// # Safety
/// `c` is live.
unsafe fn close_menu(c: P) {
    // SAFETY: caller contract.
    unsafe {
        if !cs(c, ComboState::close) {
            return;
        }
        let w = this(c);
        hh::pop_attention(w);
        hh::pop_focused(w);
        hh::pop_default_nav(w);
        let slider = (*c).slider;
        hh::set_enabled(slider, false);
        hh::set_visible(slider, false);
        if let Some(b) = (*c).base.as_mut() {
            b.clips = false;
        }
    }
}

extern "C" fn button_press(obj: *mut c_void, _w: W, d: *const Msg) -> bool {
    let c = obj.cast::<Combo>();
    // SAFETY: `obj` is the `Combo` registered in `on_init`; `d` is the message reference.
    unsafe {
        let Some(d) = d.as_ref() else {
            return false;
        };
        let mut local = v3(0.0, 0.0, 0.0);
        if d.val() == MOUSE_LEFT && cs(c, |s| s.menu_open) {
            local = hh::world_to_local(this(c), d.pos());
        }
        let sz = size(c);
        match cs(c, |s| s.button_press(d.val(), local, sz.x)) {
            1 => on_mouse_down(c, d) && on_mouse_up(c, d),
            2 => {
                close_menu(c);
                true
            }
            3 => {
                open_menu(c);
                cs(c, |s| s.clicked_after_open = false);
                true
            }
            _ => false,
        }
    }
}

extern "C" fn slider_move(obj: *mut c_void, _w: W, d: *const Msg) -> bool {
    let c = obj.cast::<Combo>();
    // SAFETY: as `button_press`.
    unsafe {
        if let Some(d) = d.as_ref() {
            cs(c, |s| s.first_item = d.val());
        }
    }
    true
}

extern "C" fn slider_lost_focus(obj: *mut c_void, _w: W, _d: *const Msg) -> bool {
    let c = obj.cast::<Combo>();
    // SAFETY: as `button_press`.
    unsafe {
        let over = (*c).base.as_ref().is_some_and(|b| b.mouse_over);
        if cs(c, |s| s.menu_open) && !over {
            close_menu(c);
        }
    }
    false
}

extern "C" fn slider_ui_arrow_press(obj: *mut c_void, _w: W, d: *const Msg) -> bool {
    let c = obj.cast::<Combo>();
    // SAFETY: as `button_press`.
    unsafe {
        let Some(d) = d.as_ref() else {
            return false;
        };
        let items = num(c);
        cs(c, |s| s.slider_arrow(d.val(), items))
    }
}

extern "C" fn slider_ui_button_press(obj: *mut c_void, _w: W, d: *const Msg) -> bool {
    let c = obj.cast::<Combo>();
    // SAFETY: as `button_press`.
    unsafe {
        let Some(d) = d.as_ref() else {
            return false;
        };
        if d.val() == UI_BUTTON_PRIMARY {
            let over = cs(c, |s| s.mouse_over);
            set_selected_item(c, over, false, true);
            close_menu(c);
            cs(c, |s| s.open_by_ui = false);
            hh::set_focused(this(c), this(c));
        }
    }
    true
}

extern "C" fn slider_ui_button_release(_obj: *mut c_void, _w: W, _d: *const Msg) -> bool {
    true
}

extern "C" fn draw_text(obj: *mut c_void, _w: W, _d: *const Msg) -> bool {
    let c = obj.cast::<Combo>();
    // SAFETY: as `button_press`.
    unsafe {
        if !cs(c, |s| s.menu_open) {
            return false;
        }
        let w = this(c);
        let prev = hh::current_clip(w);
        hh::set_clip(w, hh::base_clip(w));
        let g = hh::global(w);
        let sz = size(c);
        let fy = hh::font_size(w).y;
        let items = num(c);
        let st = cs(c, |s| *s);
        let mut ops = vec![wl::RowOp::default(); usize::try_from(items).unwrap_or(0) * 2 + 2];
        let n = wl::combo_rows(&st, g, sz.x, fy, items, &mut ops);
        for op in ops.iter().take(n) {
            match op.kind {
                wl::OP_SELECTION => hh::draw_gfx(w, (*c).gfx[GFX_SEL], op.pos, op.size, None),
                wl::OP_TEXT_HIGHLIGHT => {
                    hh::draw_text(w, item_text(c, op.item), op.pos, ALIGN_LEFT, true);
                }
                wl::OP_TEXT => {
                    hh::draw_text(w, item_text(c, op.item), op.pos, ALIGN_LEFT, false);
                }
                _ => {}
            }
        }
        hh::set_clip(w, prev);
    }
    true
}

/// # Safety
/// `c` is live.
pub unsafe fn on_init(c: P) {
    // SAFETY: caller contract.
    unsafe {
        let w = this(c);
        let o = c as usize;
        let sz = size(c);
        let text = hh::create_textbox(w, sz);
        (*c).text = text;
        hh::set_text(text, hh::text(w));
        hh::set_can_edit(text, false);
        let button = hh::create_button(w);
        (*c).button = button;
        hh::button_set_image(button, hh::skin_gfx(w, gfx_id("ComboBoxButtonIcon")));
        hh::add_callback(
            button,
            MSG_MOUSE_DOWN,
            o,
            button_press as *const () as usize,
        );
        let slider = hh::create_slider(w);
        (*c).slider = slider;
        hh::add_callback(
            slider,
            MSG_SLIDER_MOVE,
            o,
            slider_move as *const () as usize,
        );
        hh::add_callback(
            slider,
            MSG_LOST_FOCUS,
            o,
            slider_lost_focus as *const () as usize,
        );
        hh::add_callback(
            slider,
            MSG_UI_ARROW_PRESS,
            o,
            slider_ui_arrow_press as *const () as usize,
        );
        hh::add_callback(
            slider,
            MSG_UI_BUTTON_PRESS,
            o,
            slider_ui_button_press as *const () as usize,
        );
        hh::add_callback(
            slider,
            MSG_UI_BUTTON_RELEASE,
            o,
            slider_ui_button_release as *const () as usize,
        );
        hh::set_enabled(slider, false);
        hh::set_visible(slider, false);
        hh::add_callback(w, MSG_ON_DRAW, o, draw_text as *const () as usize);
        on_change_size(c);
    }
}

/// # Safety
/// `c` is live.
pub unsafe fn on_change_size(c: P) {
    // SAFETY: caller contract.
    unsafe {
        let (text, button, slider) = ((*c).text, (*c).button, (*c).slider);
        if text == 0 || button == 0 || slider == 0 {
            return;
        }
        hh::set_size(text, size(c));
        let ts = hh::size(text);
        if let Some(b) = (*c).base.as_mut() {
            b.size = ts;
        }
        let back = hh::textbox_background_size(text);
        let layout = wl::combo_layout(ts, back, (*c).button_w);
        hh::set_size(button, layout.button_size);
        hh::set_position(button, layout.button_pos);
        let max = cs(c, |s| s.max_items);
        hh::slider_set_bar_size(slider, max);
        let items = num(c);
        hh::slider_set_max(slider, items.wrapping_sub(max));
    }
}

/// # Safety
/// `c` is live.
pub unsafe fn on_change_text(c: P) {
    // SAFETY: caller contract.
    unsafe {
        let text = (*c).text;
        if text != 0 {
            hh::set_text(text, hh::text(this(c)));
        }
    }
}

/// # Safety
/// `c` is live.
pub unsafe fn on_load_graphics(c: P) {
    // SAFETY: caller contract.
    let w = unsafe { this(c) };
    let g = |n: &str| hh::skin_gfx(w, gfx_id(n));
    let table = [
        g("ComboBoxBackground"),
        g("TextBoxSelectedTextBack"),
        g("ComboBoxBorderRight"),
        g("ComboBoxBorderLeft"),
        g("ComboBoxBorderUp"),
        g("ComboBoxBorderDown"),
        g("ComboBoxCornerLU"),
        g("ComboBoxCornerRU"),
        g("ComboBoxCornerRD"),
        g("ComboBoxCornerLD"),
    ];
    // SAFETY: caller contract.
    unsafe { (*c).gfx = table };
}

/// # Safety
/// `c` is live.
pub unsafe fn on_draw(c: P) {
    // SAFETY: caller contract.
    unsafe {
        if !cs(c, |s| s.menu_open) {
            return;
        }
        let w = this(c);
        let prev = hh::current_clip(w);
        hh::set_clip(w, hh::base_clip(w));
        let g = hh::global(w);
        let sz = size(c);
        let st = cs(c, |s| *s);
        let d = wl::combo_draw(&st, g, sz.x);
        hh::draw_gfx(w, (*c).gfx[GFX_BG], d.bg_pos, d.bg_size, None);
        let b = [(*c).gfx[2], (*c).gfx[3], (*c).gfx[4], (*c).gfx[5]];
        let k = [(*c).gfx[6], (*c).gfx[7], (*c).gfx[8], (*c).gfx[9]];
        hh::draw_borders(w, 0, (&b, &k), d.border_pos, d.border_size, None);
        hh::set_clip(w, prev);
    }
}

/// # Safety
/// `c` is live.
pub unsafe fn on_mouse_move(c: P, d: &Msg) -> bool {
    // SAFETY: caller contract.
    unsafe {
        if !cs(c, |s| s.menu_open) {
            return false;
        }
        if !(*c).base.as_ref().is_some_and(|b| b.mouse_over) {
            return false;
        }
        let w = this(c);
        let local = hh::world_to_local(w, d.pos());
        let sz = size(c);
        let enabled = hh::is_enabled((*c).slider);
        let c0 = hh::gfx_active_size((*c).gfx[GFX_CORNERS]).y;
        let fy = hh::font_size(w).y;
        let items = num(c);
        cs(c, |s| {
            s.mouse_move(true, local, sz.x, enabled, c0, fy, items)
        })
    }
}

/// # Safety
/// `c` is live.
pub unsafe fn on_mouse_down(c: P, d: &Msg) -> bool {
    // SAFETY: caller contract.
    unsafe {
        let (ok, wheel) = cs(c, |s| s.mouse_down_pre(d.val()));
        if !ok {
            return false;
        }
        let slider = (*c).slider;
        let sv = hh::slider_value(slider);
        let add = hh::slider_button_add(slider);
        if wheel < 0 {
            hh::slider_set_value(slider, sv.wrapping_sub(add), None);
        } else if wheel > 0 {
            hh::slider_set_value(slider, sv.wrapping_add(add), None);
        } else {
            let local = hh::world_to_local(this(c), d.pos());
            if cs(c, |s| s.mouse_down_closes(local.y)) {
                close_menu(c);
            }
        }
        true
    }
}

/// # Safety
/// `c` is live.
pub unsafe fn on_mouse_up(c: P, d: &Msg) -> bool {
    // SAFETY: caller contract.
    unsafe {
        if !cs(c, |s| s.mouse_up_pre(d.val())) {
            return false;
        }
        let local = hh::world_to_local(this(c), d.pos());
        if cs(c, |s| s.mouse_up_selects(local.y)) {
            let over = cs(c, |s| s.mouse_over);
            set_selected_item(c, over, false, true);
            close_menu(c);
        }
        true
    }
}

/// # Safety
/// `c` is live.
pub unsafe fn on_ui_arrow_press(c: P, d: &Msg) -> bool {
    // SAFETY: caller contract.
    unsafe {
        if !cs(c, |s| s.menu_open) {
            return false;
        }
        let slider = (*c).slider;
        let items = num(c);
        let value = hh::slider_value(slider);
        let add = hh::slider_button_add(slider);
        let (ret, set) = cs(c, |s| s.ui_arrow(d.val(), items, value, add));
        if let Some(v) = set {
            hh::slider_set_value(slider, v, None);
        }
        ret
    }
}

/// # Safety
/// `c` is live.
pub unsafe fn on_ui_button_press(c: P, d: &Msg) -> bool {
    // SAFETY: caller contract.
    unsafe {
        let focus = hh::has_focus(this(c));
        let b = cs(c, |s| s.ui_button(focus, d.val()));
        match b.action {
            1 => close_menu(c),
            2 => open_menu(c),
            3 => {
                let over = cs(c, |s| s.mouse_over);
                set_selected_item(c, over, true, true);
                close_menu(c);
                if b.clear_open_by_ui {
                    cs(c, |s| s.open_by_ui = false);
                }
            }
            _ => {}
        }
        b.ret
    }
}

/// # Safety
/// `c` is live.
pub unsafe fn on_ui_button_release(c: P, d: &Msg) -> bool {
    // SAFETY: caller contract.
    if hh::has_focus(unsafe { this(c) }) {
        return d.val() == UI_BUTTON_PRIMARY;
    }
    false
}

/// # Safety
/// `c` is live.
pub unsafe fn on_lost_focus(c: P, d: &Msg) -> bool {
    // SAFETY: caller contract.
    unsafe {
        let (open, by_ui) = cs(c, |s| (s.menu_open, s.open_by_ui));
        if open && !by_ui {
            let local = hh::world_to_local(this(c), d.pos());
            let sz = size(c);
            if cs(c, |s| s.lost_focus_closes(local, sz.x)) {
                close_menu(c);
            }
        }
    }
    false
}

/// # Safety
/// `c` is live.
pub unsafe fn point_is_inside(c: P, p: [f32; 2], only_clipped: bool) -> bool {
    // SAFETY: caller contract.
    unsafe {
        let w = this(c);
        let in_clip = hh::check_point_clip(w, p);
        if !cs(c, |s| s.menu_open) && !in_clip {
            return false;
        }
        let clips = (*c).base.as_ref().is_some_and(|b| b.clips);
        if only_clipped && !clips {
            return true;
        }
        let local = hh::world_to_local(w, p);
        let sz = size(c);
        cs(c, |s| s.point_inside(local, sz))
    }
}
