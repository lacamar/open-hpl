//! C ABI of the H20 widgets, declared in `gui/RustGuiH20.h`.
//!
//! `vt` is a valid host table and widget state pointers are the structs embedded in the C++
//! widgets; a null state makes a call a no-op returning the zero value.
#![allow(unused_braces, reason = "rustfmt wraps the macro bodies in braces")]

use super::h19_widget::Base;
use super::h20_combo::{self as cb, Combo};
use super::h20_host::{HostVt, Hx};
use super::h20_list::{self as l, ListBase};
use super::h20_menu::{self as mn, Ctx, Item, Main};
use super::h20_tab::{self as tb, Frame, Label, Tab};
use super::h20_text::{self as tx, TextBox};
use super::set::{Msg, W};
use crate::types::Vector3f;

/// # Safety
/// `p` is null or valid for reads.
unsafe fn rd_msg(p: *const Msg) -> Msg {
    // SAFETY: caller contract.
    unsafe { p.as_ref() }.copied().unwrap_or(Msg::zero())
}

macro_rules! ffn {
    ($ty:ty, $name:ident, |$hx:ident, $lb:ident $(, $arg:ident: $t:ty)*| -> $r:ty $body:block) => {
        /// # Safety
        /// `vt` is null or valid and `lb` is null or an initialised state struct.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name(vt: *const HostVt, $lb: *mut $ty $(, $arg: $t)*) -> $r {
            if $lb.is_null() {
                return Default::default();
            }
            // SAFETY: caller contract.
            let $hx = unsafe { Hx::new(vt) };
            // SAFETY: caller contract.
            unsafe { $body }
        }
    };
    ($ty:ty, $name:ident, |$hx:ident, $lb:ident $(, $arg:ident: $t:ty)*| $body:expr) => {
        /// # Safety
        /// `vt` is null or valid and `lb` is null or an initialised state struct.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name(vt: *const HostVt, $lb: *mut $ty $(, $arg: $t)*) {
            if $lb.is_null() {
                return;
            }
            // SAFETY: caller contract.
            let $hx = unsafe { Hx::new(vt) };
            // SAFETY: caller contract.
            unsafe { $body }
        }
    };
}

/// # Safety
/// `lb` is uninitialised storage of a `ListBase`, `base` the widget's base state.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h20_lb_init(
    vt: *const HostVt,
    lb: *mut ListBase,
    this: W,
    container: usize,
    base: *mut Base,
    kind: i32,
) {
    if !lb.is_null() {
        // SAFETY: caller contract.
        unsafe { l::init(Hx::new(vt), lb, this, container, base, kind) };
    }
}

/// # Safety
/// `lb` is null or initialised and not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h20_lb_free(lb: *mut ListBase) {
    if !lb.is_null() {
        // SAFETY: caller contract.
        unsafe { l::free(lb) };
    }
}

ffn!(ListBase, ohpl_h20_lb_destroy_slider, |hx, lb| {
    l::destroy_slider(hx, lb)
});
ffn!(ListBase, ohpl_h20_lb_destroy_columns, |hx, lb| {
    l::destroy_columns(hx, lb)
});
ffn!(
    ListBase,
    ohpl_h20_lb_set_selected_item,
    |hx, lb, x: i32, move_list: bool, cb: bool, ty: i32, clear: bool| l::set_selected_item(
        hx, lb, x, move_list, cb, ty, clear
    )
);
ffn!(ListBase, ohpl_h20_lb_selected_item, |hx, lb| -> i32 {
    let _ = hx;
    l::selected_item(lb)
});
ffn!(ListBase, ohpl_h20_lb_multi_selection, |hx,
                                             lb,
                                             idx: i32|
 -> i32 {
    let _ = hx;
    l::multi_selection(lb, idx)
});
ffn!(ListBase, ohpl_h20_lb_multi_selection_num, |hx, lb| -> i32 {
    let _ = hx;
    l::multi_selection_num(lb)
});
ffn!(ListBase, ohpl_h20_lb_clear_selection, |hx, lb| {
    l::clear_selection(hx, lb)
});
ffn!(ListBase, ohpl_h20_lb_update_properties, |hx, lb| {
    l::update_properties(hx, lb)
});
ffn!(ListBase, ohpl_h20_lb_on_change_size, |hx, lb| {
    l::on_change_size(hx, lb)
});
ffn!(ListBase, ohpl_h20_lb_on_init, |hx, lb| l::on_init(hx, lb));
ffn!(
    ListBase,
    ohpl_h20_lb_add_item_to_selection,
    |hx, lb, it: usize| { l::add_item_to_selection(hx, lb, it) }
);
ffn!(
    ListBase,
    ohpl_h20_lb_remove_item_from_selection,
    |hx, lb, it: usize| l::remove_item_from_selection(hx, lb, it)
);
ffn!(ListBase, ohpl_h20_lb_on_load_graphics, |hx, lb| {
    l::on_load_graphics(hx, lb)
});
ffn!(ListBase, ohpl_h20_lb_on_draw, |hx, lb| l::on_draw(hx, lb));
ffn!(
    ListBase,
    ohpl_h20_lb_on_draw_after_clip,
    |hx, lb, clip: usize| { l::on_draw_after_clip(hx, lb, clip) }
);
ffn!(
    ListBase,
    ohpl_h20_lb_set_cursor_pos,
    |hx, lb, pos: i32, move_list: bool, clamp: bool| l::set_cursor_pos(
        hx, lb, pos, move_list, clamp
    )
);
ffn!(ListBase, ohpl_h20_lb_on_mouse_down, |hx,
                                           lb,
                                           d: *const Msg|
 -> bool {
    l::on_mouse_down(hx, lb, &rd_msg(d))
});
ffn!(
    ListBase,
    ohpl_h20_lb_on_mouse_double_click,
    |hx, lb, d: *const Msg| -> bool { l::on_mouse_double_click(hx, lb, &rd_msg(d)) }
);
ffn!(ListBase, ohpl_h20_lb_on_key_press, |hx,
                                          lb,
                                          d: *const Msg|
 -> bool {
    l::on_key_press(hx, lb, &rd_msg(d))
});
ffn!(ListBase, ohpl_h20_lb_on_ui_arrow_press, |hx,
                                               lb,
                                               d: *const Msg|
 -> bool {
    l::on_ui_arrow_press(hx, lb, &rd_msg(d))
});
ffn!(ListBase, ohpl_h20_lb_on_ui_button_press, |hx,
                                                lb,
                                                d: *const Msg|
 -> bool {
    l::on_ui_button_press(hx, lb, &rd_msg(d))
});
ffn!(ListBase, ohpl_h20_lb_on_ui_button_release, |hx,
                                                  lb|
 -> bool {
    l::on_ui_button_release(hx, lb)
});
ffn!(
    ListBase,
    ohpl_h20_lb_on_ui_button_double_press,
    |hx, lb, d: *const Msg| -> bool { l::on_ui_button_double_press(hx, lb, &rd_msg(d)) }
);
ffn!(
    ListBase,
    ohpl_h20_lb_add_column,
    |hx, lb, text: *const u32, len: usize, index: i32, align: i32| l::add_column(
        hx,
        lb,
        (text, len),
        index,
        align
    )
);
ffn!(
    ListBase,
    ohpl_h20_lb_set_column_width,
    |hx, lb, idx: i32, width: f32| l::set_column_width(hx, lb, idx, width)
);

/// # Safety
/// `out` is writable for 3 floats; others as the `lb` functions.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h20_lb_item_start_pos(
    vt: *const HostVt,
    lb: *mut ListBase,
    out: *mut Vector3f,
) {
    if lb.is_null() || out.is_null() {
        return;
    }
    // SAFETY: caller contract.
    unsafe { out.write(l::item_start_pos(Hx::new(vt), lb)) };
}

/// # Safety
/// `c` is uninitialised storage of a `Combo`, `base` the widget's base state.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h20_cb_init(
    vt: *const HostVt,
    c: *mut Combo,
    this: W,
    container: usize,
    base: *mut Base,
) {
    if !c.is_null() {
        // SAFETY: caller contract.
        unsafe { cb::init(Hx::new(vt), vt, c, this, container, base) };
    }
}

ffn!(Combo, ohpl_h20_cb_destroy_children, |hx, c| {
    cb::destroy_children(hx, c)
});
ffn!(
    Combo,
    ohpl_h20_cb_set_selected_item,
    |hx, c, x: i32, move_list: bool, callback: bool| cb::set_selected_item(
        hx, c, x, move_list, callback
    )
);
ffn!(Combo, ohpl_h20_cb_set_can_edit, |hx, c, v: bool| {
    cb::set_can_edit(hx, c, v)
});
ffn!(Combo, ohpl_h20_cb_can_edit, |hx, c| -> bool {
    cb::can_edit(hx, c)
});
ffn!(Combo, ohpl_h20_cb_set_max_shown_items, |hx, c, v: i32| {
    cb::set_max_shown_items(hx, c, v)
});
ffn!(
    Combo,
    ohpl_h20_cb_set_default_font_size,
    |hx, c, x: f32, y: f32| cb::set_default_font_size(hx, c, crate::types::Vector2f { x, y })
);
ffn!(Combo, ohpl_h20_cb_update_properties, |hx, c| {
    cb::update_properties(hx, c)
});
ffn!(Combo, ohpl_h20_cb_on_init, |hx, c| cb::on_init(hx, c));
ffn!(Combo, ohpl_h20_cb_on_change_size, |hx, c| {
    cb::on_change_size(hx, c)
});
ffn!(Combo, ohpl_h20_cb_on_change_text, |hx, c| {
    cb::on_change_text(hx, c)
});
ffn!(Combo, ohpl_h20_cb_on_load_graphics, |hx, c| {
    cb::on_load_graphics(hx, c)
});
ffn!(Combo, ohpl_h20_cb_on_draw, |hx, c| cb::on_draw(hx, c));
ffn!(Combo, ohpl_h20_cb_on_mouse_move, |hx,
                                        c,
                                        d: *const Msg|
 -> bool {
    cb::on_mouse_move(hx, c, &rd_msg(d))
});
ffn!(Combo, ohpl_h20_cb_on_mouse_down, |hx,
                                        c,
                                        d: *const Msg|
 -> bool {
    cb::on_mouse_down(hx, c, &rd_msg(d))
});
ffn!(Combo, ohpl_h20_cb_on_mouse_up, |hx,
                                      c,
                                      d: *const Msg|
 -> bool {
    cb::on_mouse_up(hx, c, &rd_msg(d))
});
ffn!(Combo, ohpl_h20_cb_on_ui_arrow_press, |hx,
                                            c,
                                            d: *const Msg|
 -> bool {
    cb::on_ui_arrow_press(hx, c, &rd_msg(d))
});
ffn!(Combo, ohpl_h20_cb_on_ui_button_press, |hx,
                                             c,
                                             d: *const Msg|
 -> bool {
    cb::on_ui_button_press(hx, c, &rd_msg(d))
});
ffn!(Combo, ohpl_h20_cb_on_ui_button_release, |hx,
                                               c,
                                               d: *const Msg|
 -> bool {
    cb::on_ui_button_release(hx, c, &rd_msg(d))
});
ffn!(Combo, ohpl_h20_cb_on_lost_focus, |hx,
                                        c,
                                        d: *const Msg|
 -> bool {
    cb::on_lost_focus(hx, c, &rd_msg(d))
});
ffn!(Combo, ohpl_h20_cb_point_is_inside, |hx,
                                          c,
                                          x: f32,
                                          y: f32,
                                          only_clipped: bool|
 -> bool {
    cb::point_is_inside(hx, c, [x, y], only_clipped)
});

/// # Safety
/// `f` is uninitialised storage, `base` the widget's base state.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h20_tf_init(
    vt: *const HostVt,
    f: *mut Frame,
    this: W,
    base: *mut Base,
) {
    if !f.is_null() {
        // SAFETY: caller contract.
        unsafe { tb::frame_init(f, vt, this, base) };
    }
}

/// # Safety
/// `f` is null or initialised and not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h20_tf_free(f: *mut Frame) {
    if !f.is_null() {
        // SAFETY: caller contract.
        unsafe { tb::frame_free(f) };
    }
}

ffn!(Frame, ohpl_h20_tf_destroy, |hx, f| tb::frame_destroy(hx, f));
ffn!(Frame, ohpl_h20_tf_add_tab, |hx,
                                  f,
                                  text: *const u32,
                                  len: usize|
 -> usize {
    tb::add_tab(hx, f, (text, len))
});
ffn!(Frame, ohpl_h20_tf_clear_tabs, |hx, f| tb::clear_tabs(hx, f));
ffn!(Frame, ohpl_h20_tf_tab_num, |hx, f| -> i32 {
    let _ = hx;
    tb::tab_num(f)
});
ffn!(Frame, ohpl_h20_tf_tab, |hx, f, idx: i32| -> usize {
    let _ = hx;
    tb::tab_at(f, idx)
});
ffn!(Frame, ohpl_h20_tf_tab_on_top_index, |hx, f| -> i32 {
    let _ = hx;
    tb::tab_on_top_index(f)
});
ffn!(Frame, ohpl_h20_tf_set_tab_on_top, |hx, f, tab: usize| {
    tb::set_tab_on_top(hx, f, tab)
});
ffn!(
    Frame,
    ohpl_h20_tf_set_tab_on_top_by_index,
    |hx, f, idx: i32| tb::set_tab_on_top_by_index(hx, f, idx)
);
ffn!(Frame, ohpl_h20_tf_set_hori_scroll, |hx, f, v: bool| {
    let _ = hx;
    tb::set_horizontal_scroll_enabled(f, v)
});
ffn!(Frame, ohpl_h20_tf_set_vert_scroll, |hx, f, v: bool| {
    let _ = hx;
    tb::set_vertical_scroll_enabled(f, v)
});
ffn!(Frame, ohpl_h20_tf_on_ui_button_press, |hx,
                                             f,
                                             d: *const Msg|
 -> bool {
    tb::on_ui_button_press(hx, f, &rd_msg(d))
});
ffn!(
    Frame,
    ohpl_h20_tf_set_scroll_buttons_hidden,
    |hx, f, v: bool| tb::set_scroll_buttons_hidden(hx, f, v)
);
ffn!(Frame, ohpl_h20_tf_update_tab_visibility, |hx, f| {
    tb::update_tab_visibility(hx, f)
});
ffn!(Frame, ohpl_h20_tf_on_init, |hx, f| tb::frame_on_init(hx, f));
ffn!(Frame, ohpl_h20_tf_on_change_size, |hx, f| {
    tb::frame_on_change_size(hx, f)
});
ffn!(Frame, ohpl_h20_tf_on_load_graphics, |hx, f| {
    tb::frame_on_load_graphics(hx, f)
});

/// # Safety
/// `t` is uninitialised storage; `fs` the parent frame's state.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h20_tab_init(
    vt: *const HostVt,
    t: *mut Tab,
    this: W,
    frame_w: W,
    fs: *mut Frame,
    base: *mut Base,
) {
    if !t.is_null() && !fs.is_null() {
        // SAFETY: caller contract.
        unsafe { tb::tab_init_state(t, vt, this, frame_w, fs, base) };
    }
}

ffn!(Tab, ohpl_h20_tab_destroy, |hx, t| tb::tab_destroy(hx, t));
ffn!(
    Tab,
    ohpl_h20_tab_attach_child,
    |hx, t, child: usize, remove: bool| tb::tab_attach_child(hx, t, child, remove)
);
ffn!(Tab, ohpl_h20_tab_set_icon, |hx, t, icon: usize| {
    tb::tab_set_icon(hx, t, icon)
});
ffn!(Tab, ohpl_h20_tab_set_on_top, |hx, t, v: bool| {
    tb::tab_set_on_top(hx, t, v)
});
ffn!(Tab, ohpl_h20_tab_point_is_inside, |hx,
                                         t,
                                         x: f32,
                                         y: f32,
                                         only_clipped: bool|
 -> bool {
    tb::tab_point_is_inside(hx, t, [x, y], only_clipped)
});
ffn!(Tab, ohpl_h20_tab_on_change_text, |hx, t| {
    tb::tab_on_change_text(hx, t)
});
ffn!(Tab, ohpl_h20_tab_on_change_size, |hx, t| {
    tb::tab_on_change_size(hx, t)
});
ffn!(Tab, ohpl_h20_tab_on_init, |hx, t| tb::tab_on_init(hx, t));
ffn!(Tab, ohpl_h20_tab_on_draw, |hx, t| tb::tab_on_draw(hx, t));

/// # Safety
/// `l` is uninitialised storage, `base` the widget's base state.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h20_tl_init(
    vt: *const HostVt,
    l: *mut Label,
    this: W,
    base: *mut Base,
) {
    if !l.is_null() {
        // SAFETY: caller contract.
        unsafe { tb::label_init(l, vt, this, base) };
    }
}

ffn!(Label, ohpl_h20_tl_on_mouse_down, |hx, l| -> bool {
    tb::label_on_mouse_down(hx, l)
});
ffn!(Label, ohpl_h20_tl_on_change_text, |hx, l| {
    tb::label_on_change_text(hx, l)
});
ffn!(Label, ohpl_h20_tl_on_get_ui_nav_focus, |hx, l| -> bool {
    tb::label_on_get_ui_nav_focus(hx, l)
});
ffn!(Label, ohpl_h20_tl_on_draw, |hx, l| tb::label_on_draw(hx, l));

/// # Safety
/// `t` is uninitialised storage, `base` the widget's base state.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h20_tb_init(
    vt: *const HostVt,
    t: *mut TextBox,
    this: W,
    base: *mut Base,
    numeric: bool,
) {
    if !t.is_null() {
        // SAFETY: caller contract.
        unsafe { tx::init(t, vt, this, base, numeric) };
    }
}

/// # Safety
/// `t` is null or initialised and not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h20_tb_free(t: *mut TextBox) {
    if !t.is_null() {
        // SAFETY: caller contract.
        unsafe { tx::free(t) };
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h20_tb_set_default_decimals(v: i32) {
    tx::set_default_decimals(v);
}

ffn!(TextBox, ohpl_h20_tb_destroy_buttons, |hx, t| {
    tx::destroy_buttons(hx, t)
});
ffn!(
    TextBox,
    ohpl_h20_tb_set_illegal_chars,
    |hx, t, text: *const u32, len: usize| {
        let _ = hx;
        tx::set_illegal_chars(t, (text, len))
    }
);
ffn!(
    TextBox,
    ohpl_h20_tb_set_max_text_length,
    |hx, t, len: i32| tx::set_max_text_length(hx, t, len)
);
ffn!(
    TextBox,
    ohpl_h20_tb_set_selected_text,
    |hx, t, start: i32, count: i32| tx::set_selected_text(hx, t, start, count)
);
ffn!(TextBox, ohpl_h20_tb_set_numeric_value, |hx, t, v: f32| {
    tx::set_numeric_value(hx, t, v)
});
ffn!(TextBox, ohpl_h20_tb_numeric_value, |hx, t| -> f32 {
    tx::numeric_value(hx, t)
});
ffn!(
    TextBox,
    ohpl_h20_tb_background_size,
    |hx, t, out: *mut crate::types::Vector2f| {
        if !out.is_null() {
            out.write(tx::background_size(hx, t))
        }
    }
);
ffn!(TextBox, ohpl_h20_tb_set_can_edit, |hx, t, v: bool| {
    tx::set_can_edit(hx, t, v)
});
ffn!(TextBox, ohpl_h20_tb_on_change_size, |hx, t| {
    tx::on_change_size(hx, t)
});
ffn!(TextBox, ohpl_h20_tb_on_change_text, |hx, t| {
    tx::on_change_text(hx, t)
});
ffn!(TextBox, ohpl_h20_tb_on_init, |hx, t| tx::on_init(hx, t));
ffn!(TextBox, ohpl_h20_tb_on_load_graphics, |hx, t| {
    tx::on_load_graphics(hx, t)
});
ffn!(TextBox, ohpl_h20_tb_on_draw, |hx, t| tx::on_draw(hx, t));
ffn!(TextBox, ohpl_h20_tb_on_mouse_move, |hx,
                                          t,
                                          d: *const Msg|
 -> bool {
    tx::on_mouse_move(hx, t, &rd_msg(d))
});
ffn!(TextBox, ohpl_h20_tb_on_mouse_down, |hx,
                                          t,
                                          d: *const Msg|
 -> bool {
    tx::on_mouse_down(hx, t, &rd_msg(d))
});
ffn!(TextBox, ohpl_h20_tb_on_mouse_up, |hx,
                                        t,
                                        d: *const Msg|
 -> bool {
    tx::on_mouse_up(hx, t, &rd_msg(d))
});
ffn!(
    TextBox,
    ohpl_h20_tb_on_mouse_double_click,
    |hx, t, d: *const Msg| -> bool { tx::on_mouse_double_click(hx, t, &rd_msg(d)) }
);
ffn!(TextBox, ohpl_h20_tb_on_got_focus, |hx, t| -> bool {
    tx::on_got_focus(hx, t)
});
ffn!(TextBox, ohpl_h20_tb_on_got_tab_focus, |hx, t| -> bool {
    tx::on_got_tab_focus(hx, t)
});
ffn!(TextBox, ohpl_h20_tb_on_lost_focus, |hx,
                                          t,
                                          d: *const Msg|
 -> bool {
    tx::on_lost_focus(hx, t, &rd_msg(d))
});
ffn!(TextBox, ohpl_h20_tb_on_key_press, |hx,
                                         t,
                                         d: *const Msg|
 -> bool {
    tx::on_key_press(hx, t, &rd_msg(d))
});
ffn!(TextBox, ohpl_h20_tb_on_ui_button_press, |hx,
                                               t,
                                               d: *const Msg|
 -> bool {
    tx::on_ui_button_press(hx, t, &rd_msg(d))
});
ffn!(TextBox, ohpl_h20_tb_on_enter, |hx,
                                     t,
                                     d: *const Msg|
 -> bool {
    tx::on_enter(hx, t, &rd_msg(d))
});
ffn!(TextBox, ohpl_h20_tb_ui_keyboard_open, |hx, t| {
    tx::ui_keyboard_open(hx, t)
});

/// # Safety
/// `i` is uninitialised storage, `base` the widget's base state.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h20_mi_init(
    vt: *const HostVt,
    i: *mut Item,
    this: W,
    base: *mut Base,
    kind: i32,
) {
    if !i.is_null() {
        // SAFETY: caller contract.
        unsafe { mn::item_init(i, vt, this, base, kind) };
    }
}

ffn!(Item, ohpl_h20_mi_on_init, |hx, i| mn::item_on_init(hx, i));
ffn!(Item, ohpl_h20_mi_on_update, |hx, i, dt: f32| {
    mn::item_on_update(hx, i, dt)
});
ffn!(Item, ohpl_h20_mi_on_draw, |hx, i| mn::item_on_draw(hx, i));
ffn!(Item, ohpl_h20_mi_on_mouse_down, |hx, i| -> bool {
    mn::item_on_mouse_down(hx, i)
});
ffn!(Item, ohpl_h20_mi_on_mouse_up, |hx,
                                     i,
                                     d: *const Msg|
 -> bool {
    mn::item_on_mouse_up(hx, i, &rd_msg(d))
});
ffn!(Item, ohpl_h20_mi_on_mouse_move, |hx, i| -> bool {
    mn::item_on_mouse_move(hx, i)
});
ffn!(Item, ohpl_h20_mi_on_mouse_leave, |hx, i| -> bool {
    mn::item_on_mouse_leave(hx, i)
});
ffn!(Item, ohpl_h20_mi_on_key_press, |hx,
                                      i,
                                      d: *const Msg|
 -> bool {
    mn::item_on_key_press(hx, i, &rd_msg(d))
});
ffn!(Item, ohpl_h20_mi_on_load_graphics, |hx, i| {
    mn::item_on_load_graphics(hx, i)
});

/// # Safety
/// `c` is uninitialised storage, `base` the widget's base state.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h20_cm_init(
    vt: *const HostVt,
    c: *mut Ctx,
    this: W,
    base: *mut Base,
) {
    if !c.is_null() {
        // SAFETY: caller contract.
        unsafe { mn::ctx_init(c, vt, this, base) };
    }
}

ffn!(Ctx, ohpl_h20_cm_hide, |hx, c| mn::ctx_hide(hx, c));
ffn!(Ctx, ohpl_h20_cm_item_width, |hx, c| -> f32 {
    mn::ctx_item_width(hx, c)
});
ffn!(Ctx, ohpl_h20_cm_update_items_pos, |hx, c, item: usize| {
    mn::ctx_update_items_pos(hx, c, item)
});
ffn!(Ctx, ohpl_h20_cm_update_items_size, |hx, c, item: usize| {
    mn::ctx_update_items_size(hx, c, item)
});
ffn!(Ctx, ohpl_h20_cm_update_width, |hx, c, w: f32| {
    mn::ctx_update_width(hx, c, w)
});
ffn!(Ctx, ohpl_h20_cm_on_draw, |hx, c| mn::ctx_on_draw(hx, c));
ffn!(Ctx, ohpl_h20_cm_on_update, |hx, c, dt: f32| {
    mn::ctx_on_update(hx, c, dt)
});
ffn!(Ctx, ohpl_h20_cm_on_lost_focus, |hx, c| -> bool {
    mn::ctx_on_lost_focus(hx, c)
});
ffn!(Ctx, ohpl_h20_cm_on_got_focus, |hx, c| -> bool {
    let _ = hx;
    mn::ctx_on_got_focus(c)
});
ffn!(Ctx, ohpl_h20_cm_on_key_press, |hx,
                                     c,
                                     d: *const Msg|
 -> bool {
    mn::ctx_on_key_press(hx, c, &rd_msg(d))
});
ffn!(Ctx, ohpl_h20_cm_on_load_graphics, |hx, c| {
    mn::ctx_on_load_graphics(hx, c)
});

/// # Safety
/// `m` is uninitialised storage, `base` the widget's base state.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h20_mm_init(
    vt: *const HostVt,
    m: *mut Main,
    this: W,
    base: *mut Base,
) {
    if !m.is_null() {
        // SAFETY: caller contract.
        unsafe { mn::main_init(m, vt, this, base) };
    }
}

ffn!(Main, ohpl_h20_mm_update_items_pos, |hx, m, item: usize| {
    mn::main_update_items_pos(hx, m, item)
});
ffn!(Main, ohpl_h20_mm_update_items_size, |hx, m, item: usize| {
    mn::main_update_items_size(hx, m, item)
});
ffn!(Main, ohpl_h20_mm_on_child_menu_hide, |hx, m| {
    mn::main_on_child_menu_hide(hx, m)
});
ffn!(Main, ohpl_h20_mm_hide, |hx, m| mn::main_hide(hx, m));
ffn!(Main, ohpl_h20_mm_on_draw, |hx, m| mn::main_on_draw(hx, m));
ffn!(Main, ohpl_h20_mm_on_update, |hx, m| mn::main_on_update(
    hx, m
));
ffn!(Main, ohpl_h20_mm_on_init, |hx, m| mn::main_on_init(hx, m));
ffn!(Main, ohpl_h20_mm_on_load_graphics, |hx, m| {
    mn::main_on_load_graphics(hx, m)
});
