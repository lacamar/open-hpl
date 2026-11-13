//! `cWidgetComboBox`: text box, button and slider children, the drop-down menu, selection and
//! input. State is [`Combo`], embedded in the C++ widget (`gui/RustGuiH20.h`).

use super::h19_widget::Base;
use super::h20_host::{self as hh, HostVt, Hx};
use super::h20_list::{MSG_SELECTION_CHANGE, MSG_SLIDER_MOVE, attr_id, gfx_id};
use super::set::{Msg, W};
use super::widget::{UI_BUTTON_PRIMARY, v2};
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
    pub host: *const HostVt,
}

const _: () = assert!(std::mem::size_of::<Combo>() == 184);

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
unsafe fn num(hx: Hx, c: P) -> i32 {
    // SAFETY: caller contract.
    hx.item_num(unsafe { (*c).container })
}

/// # Safety
/// `c` is live.
unsafe fn item_text(hx: Hx, c: P, idx: i32) -> (*const u32, usize) {
    // SAFETY: caller contract.
    let it = hx.item(unsafe { (*c).container }, idx);
    hx.item_text(it)
}

/// # Safety
/// `c` is uninitialised storage; `base` outlives it.
pub unsafe fn init(hx: Hx, vt: *const HostVt, c: P, this: W, container: usize, base: *mut Base) {
    let button_w = hx.skin_attr(this, attr_id("ComboBoxButtonWidth")).x;
    let slider_w = hx.skin_attr(this, attr_id("ComboBoxSliderWidth")).x;
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
            host: vt,
        });
    }
}

/// # Safety
/// `c` is live.
pub unsafe fn destroy_children(hx: Hx, c: P) {
    // SAFETY: caller contract.
    unsafe {
        for child in [(*c).text, (*c).button, (*c).slider] {
            let mut a = hh::Args::new(this(c));
            a.p[0] = child;
            hx.raw(hh::DESTROY_WIDGET, &mut a);
        }
    }
}

/// # Safety
/// `c` is live.
pub unsafe fn set_selected_item(hx: Hx, c: P, x: i32, move_list: bool, callback: bool) {
    // SAFETY: caller contract.
    unsafe {
        let items = num(hx, c);
        let Some(s1) = cs(c, |s| s.select_step1(x, move_list, items)) else {
            return;
        };
        apply(hx, c, s1);
        let s2 = cs(c, |s| s.select_step2(move_list));
        apply(hx, c, s2);
        let sel = cs(c, |s| s.selected);
        if sel >= 0 && sel < num(hx, c) {
            hx.set_text(this(c), item_text(hx, c, sel));
        } else {
            hx.set_text(this(c), (std::ptr::null(), 0));
        }
        if callback {
            hx.process(this(c), MSG_SELECTION_CHANGE, sel, None);
        }
    }
}

/// # Safety
/// `c` is live.
unsafe fn apply(hx: Hx, c: P, s: SliderSet) {
    if s.set {
        // SAFETY: caller contract.
        hx.slider_set_value(unsafe { (*c).slider }, s.value, None);
    }
}

/// # Safety
/// `c` is live.
pub unsafe fn set_can_edit(hx: Hx, c: P, v: bool) {
    // SAFETY: caller contract.
    hx.set_flag(hh::TEXTBOX_SET_CAN_EDIT, unsafe { (*c).text }, v);
}

/// # Safety
/// `c` is live.
pub unsafe fn can_edit(hx: Hx, c: P) -> bool {
    // SAFETY: caller contract.
    hx.flag(hh::TEXTBOX_GET_CAN_EDIT, unsafe { (*c).text })
}

/// # Safety
/// `c` is live.
pub unsafe fn set_max_shown_items(hx: Hx, c: P, v: i32) {
    // SAFETY: caller contract.
    unsafe {
        cs(c, |s| s.max_items = v);
        update_properties(hx, c);
    }
}

/// # Safety
/// `c` is live.
pub unsafe fn set_default_font_size(hx: Hx, c: P, s: Vector2f) {
    let mut a = hh::Args::new(unsafe { (*c).text });
    a.f[0] = s.x;
    a.f[1] = s.y;
    hx.raw(hh::WIDGET_SET_FONT_SIZE, &mut a);
}

/// # Safety
/// `c` is live.
pub unsafe fn update_properties(hx: Hx, c: P) {
    // SAFETY: caller contract.
    unsafe {
        let fy = hx.font_size(this(c)).y;
        let items = num(hx, c);
        if cs(c, |s| s.update(fy, items)) {
            set_selected_item(hx, c, num(hx, c) - 1, false, true);
        }
        on_change_size(hx, c);
    }
}

/// # Safety
/// `c` is live.
unsafe fn open_menu(hx: Hx, c: P) {
    // SAFETY: caller contract.
    unsafe {
        let w = this(c);
        let items = num(hx, c);
        if !cs(c, |s| s.can_open(items)) {
            return;
        }
        hx.set_call(w, hh::SC_PUSH_ATTENTION, 0);
        hx.set_call(w, hh::SC_PUSH_FOCUSED, 0);
        hx.set_call(w, hh::SC_PUSH_DEFAULT_NAV, 0);
        hx.set_call(w, hh::SC_SET_ATTENTION, w);
        let b1 = hx.gfx_size(hh::GFX_ACTIVE_SIZE, (*c).gfx[GFX_BORDERS + 1]);
        let b2 = hx.gfx_size(hh::GFX_ACTIVE_SIZE, (*c).gfx[GFX_BORDERS + 2]);
        let global_y = hx.global(w).y;
        let popup_z = hx.set_call(w, hh::SC_POPUP_Z, 0).1[0];
        let virtual_y = hx.set_call(w, hh::SC_VIRTUAL_SIZE, 0).1[1];
        let sz = size(c);
        let open = cs(c, |s| {
            s.open(items, sz, b1, b2, global_y, popup_z, virtual_y)
        });
        let slider = (*c).slider;
        if open.slider {
            hx.set_flag(hh::WIDGET_SET_ENABLED, slider, true);
            hx.set_flag(hh::WIDGET_SET_VISIBLE, slider, true);
            hx.set_position(slider, open.slider_pos);
            hx.set_size(slider, open.slider_size);
        }
        hx.set_call(w, hh::SC_SET_FOCUSED, w);
    }
}

/// # Safety
/// `c` is live.
unsafe fn close_menu(hx: Hx, c: P) {
    // SAFETY: caller contract.
    unsafe {
        if !cs(c, ComboState::close) {
            return;
        }
        let w = this(c);
        hx.set_call(w, hh::SC_POP_ATTENTION, 0);
        hx.set_call(w, hh::SC_POP_FOCUSED, 0);
        hx.set_call(w, hh::SC_POP_DEFAULT_NAV, 0);
        let slider = (*c).slider;
        hx.set_flag(hh::WIDGET_SET_ENABLED, slider, false);
        hx.set_flag(hh::WIDGET_SET_VISIBLE, slider, false);
        if let Some(b) = (*c).base.as_mut() {
            b.clips = false;
        }
    }
}

extern "C" fn button_press(obj: *mut c_void, _w: W, d: *const Msg) -> bool {
    let c = obj.cast::<Combo>();
    // SAFETY: `obj` is the `Combo` registered in `on_init`; `d` is the message reference.
    unsafe {
        let (Some(d), Some(vt)) = (d.as_ref(), (*c).host.as_ref()) else {
            return false;
        };
        let hx = Hx::new(vt);
        let mut local = v3(0.0, 0.0, 0.0);
        if d.val() == MOUSE_LEFT && cs(c, |s| s.menu_open) {
            local = hx.world_to_local(this(c), d.pos());
        }
        let sz = size(c);
        match cs(c, |s| s.button_press(d.val(), local, sz.x)) {
            1 => on_mouse_down(hx, c, d) && on_mouse_up(hx, c, d),
            2 => {
                close_menu(hx, c);
                true
            }
            3 => {
                open_menu(hx, c);
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
        let Some(vt) = (*c).host.as_ref() else {
            return false;
        };
        let hx = Hx::new(vt);
        let over = (*c).base.as_ref().is_some_and(|b| b.mouse_over);
        if cs(c, |s| s.menu_open) && !over {
            close_menu(hx, c);
        }
    }
    false
}

extern "C" fn slider_ui_arrow_press(obj: *mut c_void, _w: W, d: *const Msg) -> bool {
    let c = obj.cast::<Combo>();
    // SAFETY: as `button_press`.
    unsafe {
        let (Some(d), Some(vt)) = (d.as_ref(), (*c).host.as_ref()) else {
            return false;
        };
        let items = num(Hx::new(vt), c);
        cs(c, |s| s.slider_arrow(d.val(), items))
    }
}

extern "C" fn slider_ui_button_press(obj: *mut c_void, _w: W, d: *const Msg) -> bool {
    let c = obj.cast::<Combo>();
    // SAFETY: as `button_press`.
    unsafe {
        let (Some(d), Some(vt)) = (d.as_ref(), (*c).host.as_ref()) else {
            return false;
        };
        let hx = Hx::new(vt);
        if d.val() == UI_BUTTON_PRIMARY {
            let over = cs(c, |s| s.mouse_over);
            set_selected_item(hx, c, over, false, true);
            close_menu(hx, c);
            cs(c, |s| s.open_by_ui = false);
            hx.set_call(this(c), hh::SC_SET_FOCUSED, this(c));
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
        let Some(vt) = (*c).host.as_ref() else {
            return false;
        };
        let hx = Hx::new(vt);
        if !cs(c, |s| s.menu_open) {
            return false;
        }
        let w = this(c);
        let prev = hx.set_call(w, hh::SC_CURRENT_CLIP, 0).0;
        hx.set_clip(w, hx.set_call(w, hh::SC_BASE_CLIP, 0).0);
        let g = hx.global(w);
        let sz = size(c);
        let fy = hx.font_size(w).y;
        let items = num(hx, c);
        let st = cs(c, |s| *s);
        let mut ops = vec![wl::RowOp::default(); usize::try_from(items).unwrap_or(0) * 2 + 2];
        let n = wl::combo_rows(&st, g, sz.x, fy, items, &mut ops);
        for op in ops.iter().take(n) {
            match op.kind {
                wl::OP_SELECTION => hx.draw_gfx(w, (*c).gfx[GFX_SEL], op.pos, op.size, None),
                wl::OP_TEXT_HIGHLIGHT => {
                    hx.draw_text(w, item_text(hx, c, op.item), op.pos, ALIGN_LEFT, true);
                }
                wl::OP_TEXT => {
                    hx.draw_text(w, item_text(hx, c, op.item), op.pos, ALIGN_LEFT, false);
                }
                _ => {}
            }
        }
        hx.set_clip(w, prev);
    }
    true
}

/// # Safety
/// `c` is live.
pub unsafe fn on_init(hx: Hx, c: P) {
    // SAFETY: caller contract.
    unsafe {
        let w = this(c);
        let o = c as usize;
        let sz = size(c);
        let mut a = hh::Args::new(w);
        a.f[0] = sz.x;
        a.f[1] = sz.y;
        let text = hx.raw(hh::CREATE_TEXTBOX, &mut a);
        (*c).text = text;
        hx.set_text(text, hx.text(w));
        hx.set_flag(hh::TEXTBOX_SET_CAN_EDIT, text, false);
        let button = hx.raw(hh::CREATE_BUTTON, &mut hh::Args::new(w));
        (*c).button = button;
        let mut a = hh::Args::new(button);
        a.p[0] = hx.skin_gfx(w, gfx_id("ComboBoxButtonIcon"));
        hx.raw(hh::BUTTON_SET_IMAGE, &mut a);
        hx.add_callback(
            button,
            MSG_MOUSE_DOWN,
            o,
            button_press as *const () as usize,
        );
        let mut a = hh::Args::new(w);
        a.i[0] = 1;
        let slider = hx.raw(hh::CREATE_SLIDER, &mut a);
        (*c).slider = slider;
        hx.add_callback(
            slider,
            MSG_SLIDER_MOVE,
            o,
            slider_move as *const () as usize,
        );
        hx.add_callback(
            slider,
            MSG_LOST_FOCUS,
            o,
            slider_lost_focus as *const () as usize,
        );
        hx.add_callback(
            slider,
            MSG_UI_ARROW_PRESS,
            o,
            slider_ui_arrow_press as *const () as usize,
        );
        hx.add_callback(
            slider,
            MSG_UI_BUTTON_PRESS,
            o,
            slider_ui_button_press as *const () as usize,
        );
        hx.add_callback(
            slider,
            MSG_UI_BUTTON_RELEASE,
            o,
            slider_ui_button_release as *const () as usize,
        );
        hx.set_flag(hh::WIDGET_SET_ENABLED, slider, false);
        hx.set_flag(hh::WIDGET_SET_VISIBLE, slider, false);
        hx.add_callback(w, MSG_ON_DRAW, o, draw_text as *const () as usize);
        on_change_size(hx, c);
    }
}

/// # Safety
/// `c` is live.
pub unsafe fn on_change_size(hx: Hx, c: P) {
    // SAFETY: caller contract.
    unsafe {
        let (text, button, slider) = ((*c).text, (*c).button, (*c).slider);
        if text == 0 || button == 0 || slider == 0 {
            return;
        }
        hx.set_size(text, size(c));
        let ts = hx.size(text);
        if let Some(b) = (*c).base.as_mut() {
            b.size = ts;
        }
        let mut a = hh::Args::new(text);
        hx.raw(hh::TEXTBOX_BG_SIZE, &mut a);
        let back = v2(a.rf[0], a.rf[1]);
        let layout = wl::combo_layout(ts, back, (*c).button_w);
        hx.set_size(button, layout.button_size);
        hx.set_position(button, layout.button_pos);
        let max = cs(c, |s| s.max_items);
        hx.slider_call(hh::SLIDER_SET_BAR_VALUE_SIZE, slider, max);
        let items = num(hx, c);
        hx.slider_call(hh::SLIDER_SET_MAX_VALUE, slider, items.wrapping_sub(max));
    }
}

/// # Safety
/// `c` is live.
pub unsafe fn on_change_text(hx: Hx, c: P) {
    // SAFETY: caller contract.
    unsafe {
        let text = (*c).text;
        if text != 0 {
            hx.set_text(text, hx.text(this(c)));
        }
    }
}

/// # Safety
/// `c` is live.
pub unsafe fn on_load_graphics(hx: Hx, c: P) {
    // SAFETY: caller contract.
    let w = unsafe { this(c) };
    let g = |n: &str| hx.skin_gfx(w, gfx_id(n));
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
pub unsafe fn on_draw(hx: Hx, c: P) {
    // SAFETY: caller contract.
    unsafe {
        if !cs(c, |s| s.menu_open) {
            return;
        }
        let w = this(c);
        let prev = hx.set_call(w, hh::SC_CURRENT_CLIP, 0).0;
        hx.set_clip(w, hx.set_call(w, hh::SC_BASE_CLIP, 0).0);
        let g = hx.global(w);
        let sz = size(c);
        let st = cs(c, |s| *s);
        let d = wl::combo_draw(&st, g, sz.x);
        hx.draw_gfx(w, (*c).gfx[GFX_BG], d.bg_pos, d.bg_size, None);
        let b = [(*c).gfx[2], (*c).gfx[3], (*c).gfx[4], (*c).gfx[5]];
        let k = [(*c).gfx[6], (*c).gfx[7], (*c).gfx[8], (*c).gfx[9]];
        hx.draw_borders(w, 0, (&b, &k), d.border_pos, d.border_size, None);
        hx.set_clip(w, prev);
    }
}

/// # Safety
/// `c` is live.
pub unsafe fn on_mouse_move(hx: Hx, c: P, d: &Msg) -> bool {
    // SAFETY: caller contract.
    unsafe {
        if !cs(c, |s| s.menu_open) {
            return false;
        }
        if !(*c).base.as_ref().is_some_and(|b| b.mouse_over) {
            return false;
        }
        let w = this(c);
        let local = hx.world_to_local(w, d.pos());
        let sz = size(c);
        let enabled = hx.flag(hh::WIDGET_IS_ENABLED, (*c).slider);
        let c0 = hx.gfx_size(hh::GFX_ACTIVE_SIZE, (*c).gfx[GFX_CORNERS]).y;
        let fy = hx.font_size(w).y;
        let items = num(hx, c);
        cs(c, |s| {
            s.mouse_move(true, local, sz.x, enabled, c0, fy, items)
        })
    }
}

/// # Safety
/// `c` is live.
pub unsafe fn on_mouse_down(hx: Hx, c: P, d: &Msg) -> bool {
    // SAFETY: caller contract.
    unsafe {
        let (ok, wheel) = cs(c, |s| s.mouse_down_pre(d.val()));
        if !ok {
            return false;
        }
        let slider = (*c).slider;
        let sv = hx.slider_value(slider);
        let add = hx.slider_call(hh::SLIDER_GET_BUTTON_VALUE_ADD, slider, 0);
        if wheel < 0 {
            hx.slider_set_value(slider, sv.wrapping_sub(add), None);
        } else if wheel > 0 {
            hx.slider_set_value(slider, sv.wrapping_add(add), None);
        } else {
            let local = hx.world_to_local(this(c), d.pos());
            if cs(c, |s| s.mouse_down_closes(local.y)) {
                close_menu(hx, c);
            }
        }
        true
    }
}

/// # Safety
/// `c` is live.
pub unsafe fn on_mouse_up(hx: Hx, c: P, d: &Msg) -> bool {
    // SAFETY: caller contract.
    unsafe {
        if !cs(c, |s| s.mouse_up_pre(d.val())) {
            return false;
        }
        let local = hx.world_to_local(this(c), d.pos());
        if cs(c, |s| s.mouse_up_selects(local.y)) {
            let over = cs(c, |s| s.mouse_over);
            set_selected_item(hx, c, over, false, true);
            close_menu(hx, c);
        }
        true
    }
}

/// # Safety
/// `c` is live.
pub unsafe fn on_ui_arrow_press(hx: Hx, c: P, d: &Msg) -> bool {
    // SAFETY: caller contract.
    unsafe {
        if !cs(c, |s| s.menu_open) {
            return false;
        }
        let slider = (*c).slider;
        let items = num(hx, c);
        let value = hx.slider_value(slider);
        let add = hx.slider_call(hh::SLIDER_GET_BUTTON_VALUE_ADD, slider, 0);
        let (ret, set) = cs(c, |s| s.ui_arrow(d.val(), items, value, add));
        if let Some(v) = set {
            hx.slider_set_value(slider, v, None);
        }
        ret
    }
}

/// # Safety
/// `c` is live.
pub unsafe fn on_ui_button_press(hx: Hx, c: P, d: &Msg) -> bool {
    // SAFETY: caller contract.
    unsafe {
        let focus = hx.has_focus(this(c));
        let b = cs(c, |s| s.ui_button(focus, d.val()));
        match b.action {
            1 => close_menu(hx, c),
            2 => open_menu(hx, c),
            3 => {
                let over = cs(c, |s| s.mouse_over);
                set_selected_item(hx, c, over, true, true);
                close_menu(hx, c);
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
pub unsafe fn on_ui_button_release(hx: Hx, c: P, d: &Msg) -> bool {
    // SAFETY: caller contract.
    if hx.has_focus(unsafe { this(c) }) {
        return d.val() == UI_BUTTON_PRIMARY;
    }
    false
}

/// # Safety
/// `c` is live.
pub unsafe fn on_lost_focus(hx: Hx, c: P, d: &Msg) -> bool {
    // SAFETY: caller contract.
    unsafe {
        let (open, by_ui) = cs(c, |s| (s.menu_open, s.open_by_ui));
        if open && !by_ui {
            let local = hx.world_to_local(this(c), d.pos());
            let sz = size(c);
            if cs(c, |s| s.lost_focus_closes(local, sz.x)) {
                close_menu(hx, c);
            }
        }
    }
    false
}

/// # Safety
/// `c` is live.
pub unsafe fn point_is_inside(hx: Hx, c: P, p: [f32; 2], only_clipped: bool) -> bool {
    // SAFETY: caller contract.
    unsafe {
        let w = this(c);
        let mut a = hh::Args::new(w);
        a.f[0] = p[0];
        a.f[1] = p[1];
        let in_clip = hx.raw(hh::CHECK_POINT_CLIP, &mut a) != 0;
        if !cs(c, |s| s.menu_open) && !in_clip {
            return false;
        }
        let clips = (*c).base.as_ref().is_some_and(|b| b.clips);
        if only_clipped && !clips {
            return true;
        }
        let local = hx.world_to_local(w, p);
        let sz = size(c);
        cs(c, |s| s.point_inside(local, sz))
    }
}
