//! C ABI of the H20 widgets, declared in `gui/RustGuiH20.h`.
//!
//! Widget state pointers are the structs embedded in the C++
//! widgets; a null state makes a call a no-op returning the zero value.
#![allow(unused_braces, reason = "rustfmt wraps the macro bodies in braces")]

use super::h20_combo::{self as cb, Combo};
use super::h20_list::{self as l, ListBase};
use super::h20_menu::{self as mn, Ctx, Item, Main};
use super::h20_tab::{self as tb, Frame, Label, Tab};
use super::h20_text::{self as tx, TextBox};
use super::set::Msg;
use crate::types::Vector3f;

/// # Safety
/// `p` is null or valid for reads.
unsafe fn rd_msg(p: *const Msg) -> Msg {
    // SAFETY: caller contract.
    unsafe { p.as_ref() }.copied().unwrap_or(Msg::zero())
}

macro_rules! ffn {
    ($ty:ty, $name:ident, |$lb:ident $(, $arg:ident: $t:ty)*| -> $r:ty $body:block) => {
        /// # Safety
        /// `lb` is null or an initialised state struct.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name($lb: *mut $ty $(, $arg: $t)*) -> $r {
            if $lb.is_null() {
                return Default::default();
            }
            // SAFETY: caller contract.
            unsafe { $body }
        }
    };
    ($ty:ty, $name:ident, |$lb:ident $(, $arg:ident: $t:ty)*| $body:expr) => {
        /// # Safety
        /// `lb` is null or an initialised state struct.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name($lb: *mut $ty $(, $arg: $t)*) {
            if $lb.is_null() {
                return;
            }
            // SAFETY: caller contract.
            unsafe { $body }
        }
    };
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

ffn!(ListBase, ohpl_h20_lb_destroy_slider, |lb| {
    l::destroy_slider(lb)
});
ffn!(ListBase, ohpl_h20_lb_destroy_columns, |lb| {
    l::destroy_columns(lb)
});
ffn!(
    ListBase,
    ohpl_h20_lb_set_selected_item,
    |lb, x: i32, move_list: bool, cb: bool, ty: i32, clear: bool| l::set_selected_item(
        lb, x, move_list, cb, ty, clear
    )
);
ffn!(ListBase, ohpl_h20_lb_selected_item, |lb| -> i32 {
    l::selected_item(lb)
});
ffn!(ListBase, ohpl_h20_lb_multi_selection, |lb,
                                             idx: i32|
 -> i32 {
    l::multi_selection(lb, idx)
});
ffn!(ListBase, ohpl_h20_lb_multi_selection_num, |lb| -> i32 {
    l::multi_selection_num(lb)
});
ffn!(ListBase, ohpl_h20_lb_clear_selection, |lb| {
    l::clear_selection(lb)
});
ffn!(ListBase, ohpl_h20_lb_update_properties, |lb| {
    l::update_properties(lb)
});
ffn!(ListBase, ohpl_h20_lb_on_change_size, |lb| {
    l::on_change_size(lb)
});
ffn!(ListBase, ohpl_h20_lb_on_init, |lb| l::on_init(lb));
ffn!(
    ListBase,
    ohpl_h20_lb_add_item_to_selection,
    |lb, it: usize| { l::add_item_to_selection(lb, it) }
);
ffn!(
    ListBase,
    ohpl_h20_lb_remove_item_from_selection,
    |lb, it: usize| l::remove_item_from_selection(lb, it)
);
ffn!(ListBase, ohpl_h20_lb_on_load_graphics, |lb| {
    l::on_load_graphics(lb)
});
ffn!(ListBase, ohpl_h20_lb_on_draw, |lb| l::on_draw(lb));
ffn!(
    ListBase,
    ohpl_h20_lb_on_draw_after_clip,
    |lb, clip: usize| { l::on_draw_after_clip(lb, clip) }
);
ffn!(
    ListBase,
    ohpl_h20_lb_set_cursor_pos,
    |lb, pos: i32, move_list: bool, clamp: bool| l::set_cursor_pos(lb, pos, move_list, clamp)
);
ffn!(ListBase, ohpl_h20_lb_on_mouse_down, |lb,
                                           d: *const Msg|
 -> bool {
    l::on_mouse_down(lb, &rd_msg(d))
});
ffn!(
    ListBase,
    ohpl_h20_lb_on_mouse_double_click,
    |lb, d: *const Msg| -> bool { l::on_mouse_double_click(lb, &rd_msg(d)) }
);
ffn!(ListBase, ohpl_h20_lb_on_key_press, |lb,
                                          d: *const Msg|
 -> bool {
    l::on_key_press(lb, &rd_msg(d))
});
ffn!(ListBase, ohpl_h20_lb_on_ui_arrow_press, |lb,
                                               d: *const Msg|
 -> bool {
    l::on_ui_arrow_press(lb, &rd_msg(d))
});
ffn!(ListBase, ohpl_h20_lb_on_ui_button_press, |lb,
                                                d: *const Msg|
 -> bool {
    l::on_ui_button_press(lb, &rd_msg(d))
});
ffn!(ListBase, ohpl_h20_lb_on_ui_button_release, |lb| -> bool {
    l::on_ui_button_release(lb)
});
ffn!(
    ListBase,
    ohpl_h20_lb_on_ui_button_double_press,
    |lb, d: *const Msg| -> bool { l::on_ui_button_double_press(lb, &rd_msg(d)) }
);
ffn!(
    ListBase,
    ohpl_h20_lb_add_column,
    |lb, text: *const u32, len: usize, index: i32, align: i32| l::add_column(
        lb,
        (text, len),
        index,
        align
    )
);
ffn!(
    ListBase,
    ohpl_h20_lb_set_column_width,
    |lb, idx: i32, width: f32| l::set_column_width(lb, idx, width)
);

/// # Safety
/// `out` is writable for 3 floats; others as the `lb` functions.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h20_lb_item_start_pos(lb: *mut ListBase, out: *mut Vector3f) {
    if lb.is_null() || out.is_null() {
        return;
    }
    // SAFETY: caller contract.
    unsafe { out.write(l::item_start_pos(lb)) };
}

ffn!(Combo, ohpl_h20_cb_destroy_children, |c| {
    cb::destroy_children(c)
});
ffn!(
    Combo,
    ohpl_h20_cb_set_selected_item,
    |c, x: i32, move_list: bool, callback: bool| cb::set_selected_item(c, x, move_list, callback)
);
ffn!(Combo, ohpl_h20_cb_set_can_edit, |c, v: bool| {
    cb::set_can_edit(c, v)
});
ffn!(Combo, ohpl_h20_cb_can_edit, |c| -> bool { cb::can_edit(c) });
ffn!(Combo, ohpl_h20_cb_set_max_shown_items, |c, v: i32| {
    cb::set_max_shown_items(c, v)
});
ffn!(
    Combo,
    ohpl_h20_cb_set_default_font_size,
    |c, x: f32, y: f32| cb::set_default_font_size(c, crate::types::Vector2f { x, y })
);
ffn!(Combo, ohpl_h20_cb_update_properties, |c| {
    cb::update_properties(c)
});
ffn!(Combo, ohpl_h20_cb_on_init, |c| cb::on_init(c));
ffn!(Combo, ohpl_h20_cb_on_change_size, |c| {
    cb::on_change_size(c)
});
ffn!(Combo, ohpl_h20_cb_on_change_text, |c| {
    cb::on_change_text(c)
});
ffn!(Combo, ohpl_h20_cb_on_load_graphics, |c| {
    cb::on_load_graphics(c)
});
ffn!(Combo, ohpl_h20_cb_on_draw, |c| cb::on_draw(c));
ffn!(Combo, ohpl_h20_cb_on_mouse_move, |c,
                                        d: *const Msg|
 -> bool {
    cb::on_mouse_move(c, &rd_msg(d))
});
ffn!(Combo, ohpl_h20_cb_on_mouse_down, |c,
                                        d: *const Msg|
 -> bool {
    cb::on_mouse_down(c, &rd_msg(d))
});
ffn!(Combo, ohpl_h20_cb_on_mouse_up, |c, d: *const Msg| -> bool {
    cb::on_mouse_up(c, &rd_msg(d))
});
ffn!(Combo, ohpl_h20_cb_on_ui_arrow_press, |c,
                                            d: *const Msg|
 -> bool {
    cb::on_ui_arrow_press(c, &rd_msg(d))
});
ffn!(Combo, ohpl_h20_cb_on_ui_button_press, |c,
                                             d: *const Msg|
 -> bool {
    cb::on_ui_button_press(c, &rd_msg(d))
});
ffn!(Combo, ohpl_h20_cb_on_ui_button_release, |c,
                                               d: *const Msg|
 -> bool {
    cb::on_ui_button_release(c, &rd_msg(d))
});
ffn!(Combo, ohpl_h20_cb_on_lost_focus, |c,
                                        d: *const Msg|
 -> bool {
    cb::on_lost_focus(c, &rd_msg(d))
});
ffn!(Combo, ohpl_h20_cb_point_is_inside, |c,
                                          x: f32,
                                          y: f32,
                                          only_clipped: bool|
 -> bool {
    cb::point_is_inside(c, [x, y], only_clipped)
});

/// # Safety
/// `f` is null or initialised and not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h20_tf_free(f: *mut Frame) {
    if !f.is_null() {
        // SAFETY: caller contract.
        unsafe { tb::frame_free(f) };
    }
}

ffn!(Frame, ohpl_h20_tf_destroy, |f| tb::frame_destroy(f));
ffn!(Frame, ohpl_h20_tf_add_tab, |f,
                                  text: *const u32,
                                  len: usize|
 -> usize {
    tb::add_tab(f, (text, len))
});
ffn!(Frame, ohpl_h20_tf_clear_tabs, |f| tb::clear_tabs(f));
ffn!(Frame, ohpl_h20_tf_tab_num, |f| -> i32 { tb::tab_num(f) });
ffn!(Frame, ohpl_h20_tf_tab, |f, idx: i32| -> usize {
    tb::tab_at(f, idx)
});
ffn!(Frame, ohpl_h20_tf_tab_on_top_index, |f| -> i32 {
    tb::tab_on_top_index(f)
});
ffn!(Frame, ohpl_h20_tf_set_tab_on_top, |f, tab: usize| {
    tb::set_tab_on_top(f, tab)
});
ffn!(Frame, ohpl_h20_tf_set_tab_on_top_by_index, |f, idx: i32| {
    tb::set_tab_on_top_by_index(f, idx)
});
ffn!(Frame, ohpl_h20_tf_set_hori_scroll, |f, v: bool| {
    tb::set_horizontal_scroll_enabled(f, v)
});
ffn!(Frame, ohpl_h20_tf_set_vert_scroll, |f, v: bool| {
    tb::set_vertical_scroll_enabled(f, v)
});
ffn!(Frame, ohpl_h20_tf_on_ui_button_press, |f,
                                             d: *const Msg|
 -> bool {
    tb::on_ui_button_press(f, &rd_msg(d))
});
ffn!(
    Frame,
    ohpl_h20_tf_set_scroll_buttons_hidden,
    |f, v: bool| tb::set_scroll_buttons_hidden(f, v)
);
ffn!(Frame, ohpl_h20_tf_update_tab_visibility, |f| {
    tb::update_tab_visibility(f)
});
ffn!(Frame, ohpl_h20_tf_on_init, |f| tb::frame_on_init(f));
ffn!(Frame, ohpl_h20_tf_on_change_size, |f| {
    tb::frame_on_change_size(f)
});
ffn!(Frame, ohpl_h20_tf_on_load_graphics, |f| {
    tb::frame_on_load_graphics(f)
});

ffn!(Tab, ohpl_h20_tab_destroy, |t| tb::tab_destroy(t));
ffn!(
    Tab,
    ohpl_h20_tab_attach_child,
    |t, child: usize, remove: bool| tb::tab_attach_child(t, child, remove)
);
ffn!(Tab, ohpl_h20_tab_set_icon, |t, icon: usize| {
    tb::tab_set_icon(t, icon)
});
ffn!(Tab, ohpl_h20_tab_set_on_top, |t, v: bool| {
    tb::tab_set_on_top(t, v)
});
ffn!(Tab, ohpl_h20_tab_point_is_inside, |t,
                                         x: f32,
                                         y: f32,
                                         only_clipped: bool|
 -> bool {
    tb::tab_point_is_inside(t, [x, y], only_clipped)
});
ffn!(Tab, ohpl_h20_tab_on_change_text, |t| {
    tb::tab_on_change_text(t)
});
ffn!(Tab, ohpl_h20_tab_on_change_size, |t| {
    tb::tab_on_change_size(t)
});
ffn!(Tab, ohpl_h20_tab_on_init, |t| tb::tab_on_init(t));
ffn!(Tab, ohpl_h20_tab_on_draw, |t| tb::tab_on_draw(t));

ffn!(Label, ohpl_h20_tl_on_mouse_down, |l| -> bool {
    tb::label_on_mouse_down(l)
});
ffn!(Label, ohpl_h20_tl_on_change_text, |l| {
    tb::label_on_change_text(l)
});
ffn!(Label, ohpl_h20_tl_on_get_ui_nav_focus, |l| -> bool {
    tb::label_on_get_ui_nav_focus(l)
});
ffn!(Label, ohpl_h20_tl_on_draw, |l| tb::label_on_draw(l));

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
pub extern "C" fn ohpl_h20_tb_default_decimals() -> i32 {
    tx::default_decimals()
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h20_tb_set_default_decimals(v: i32) {
    tx::set_default_decimals(v);
}

ffn!(TextBox, ohpl_h20_tb_destroy_buttons, |t| {
    tx::destroy_buttons(t)
});
ffn!(
    TextBox,
    ohpl_h20_tb_set_illegal_chars,
    |t, text: *const u32, len: usize| { tx::set_illegal_chars(t, (text, len)) }
);
ffn!(TextBox, ohpl_h20_tb_set_max_text_length, |t, len: i32| {
    tx::set_max_text_length(t, len)
});
ffn!(
    TextBox,
    ohpl_h20_tb_set_selected_text,
    |t, start: i32, count: i32| tx::set_selected_text(t, start, count)
);
ffn!(TextBox, ohpl_h20_tb_set_numeric_value, |t, v: f32| {
    tx::set_numeric_value(t, v)
});
ffn!(TextBox, ohpl_h20_tb_numeric_value, |t| -> f32 {
    tx::numeric_value(t)
});
ffn!(
    TextBox,
    ohpl_h20_tb_background_size,
    |t, out: *mut crate::types::Vector2f| {
        if !out.is_null() {
            out.write(tx::background_size(t))
        }
    }
);
ffn!(TextBox, ohpl_h20_tb_set_can_edit, |t, v: bool| {
    tx::set_can_edit(t, v)
});
ffn!(TextBox, ohpl_h20_tb_on_change_size, |t| {
    tx::on_change_size(t)
});
ffn!(TextBox, ohpl_h20_tb_on_change_text, |t| {
    tx::on_change_text(t)
});
ffn!(TextBox, ohpl_h20_tb_on_init, |t| tx::on_init(t));
ffn!(TextBox, ohpl_h20_tb_on_load_graphics, |t| {
    tx::on_load_graphics(t)
});
ffn!(TextBox, ohpl_h20_tb_on_draw, |t| tx::on_draw(t));
ffn!(TextBox, ohpl_h20_tb_on_mouse_move, |t,
                                          d: *const Msg|
 -> bool {
    tx::on_mouse_move(t, &rd_msg(d))
});
ffn!(TextBox, ohpl_h20_tb_on_mouse_down, |t,
                                          d: *const Msg|
 -> bool {
    tx::on_mouse_down(t, &rd_msg(d))
});
ffn!(TextBox, ohpl_h20_tb_on_mouse_up, |t,
                                        d: *const Msg|
 -> bool {
    tx::on_mouse_up(t, &rd_msg(d))
});
ffn!(
    TextBox,
    ohpl_h20_tb_on_mouse_double_click,
    |t, d: *const Msg| -> bool { tx::on_mouse_double_click(t, &rd_msg(d)) }
);
ffn!(TextBox, ohpl_h20_tb_on_got_focus, |t| -> bool {
    tx::on_got_focus(t)
});
ffn!(TextBox, ohpl_h20_tb_on_got_tab_focus, |t| -> bool {
    tx::on_got_tab_focus(t)
});
ffn!(TextBox, ohpl_h20_tb_on_lost_focus, |t,
                                          d: *const Msg|
 -> bool {
    tx::on_lost_focus(t, &rd_msg(d))
});
ffn!(TextBox, ohpl_h20_tb_on_key_press, |t,
                                         d: *const Msg|
 -> bool {
    tx::on_key_press(t, &rd_msg(d))
});
ffn!(TextBox, ohpl_h20_tb_on_ui_button_press, |t,
                                               d: *const Msg|
 -> bool {
    tx::on_ui_button_press(t, &rd_msg(d))
});
ffn!(TextBox, ohpl_h20_tb_on_enter, |t, d: *const Msg| -> bool {
    tx::on_enter(t, &rd_msg(d))
});
ffn!(TextBox, ohpl_h20_tb_ui_keyboard_open, |t| {
    tx::ui_keyboard_open(t)
});

ffn!(Item, ohpl_h20_mi_on_init, |i| mn::item_on_init(i));
ffn!(Item, ohpl_h20_mi_on_update, |i, dt: f32| {
    mn::item_on_update(i, dt)
});
ffn!(Item, ohpl_h20_mi_on_draw, |i| mn::item_on_draw(i));
ffn!(Item, ohpl_h20_mi_on_mouse_down, |i| -> bool {
    mn::item_on_mouse_down(i)
});
ffn!(Item, ohpl_h20_mi_on_mouse_up, |i, d: *const Msg| -> bool {
    mn::item_on_mouse_up(i, &rd_msg(d))
});
ffn!(Item, ohpl_h20_mi_on_mouse_move, |i| -> bool {
    mn::item_on_mouse_move(i)
});
ffn!(Item, ohpl_h20_mi_on_mouse_leave, |i| -> bool {
    mn::item_on_mouse_leave(i)
});
ffn!(Item, ohpl_h20_mi_on_key_press, |i, d: *const Msg| -> bool {
    mn::item_on_key_press(i, &rd_msg(d))
});
ffn!(Item, ohpl_h20_mi_on_load_graphics, |i| {
    mn::item_on_load_graphics(i)
});

ffn!(Ctx, ohpl_h20_cm_hide, |c| mn::ctx_hide(c));
ffn!(Ctx, ohpl_h20_cm_item_width, |c| -> f32 {
    mn::ctx_item_width(c)
});
ffn!(Ctx, ohpl_h20_cm_update_items_pos, |c, item: usize| {
    mn::ctx_update_items_pos(c, item)
});
ffn!(Ctx, ohpl_h20_cm_update_items_size, |c, item: usize| {
    mn::ctx_update_items_size(c, item)
});
ffn!(Ctx, ohpl_h20_cm_update_width, |c, w: f32| {
    mn::ctx_update_width(c, w)
});
ffn!(Ctx, ohpl_h20_cm_on_draw, |c| mn::ctx_on_draw(c));
ffn!(Ctx, ohpl_h20_cm_on_update, |c, dt: f32| {
    mn::ctx_on_update(c, dt)
});
ffn!(Ctx, ohpl_h20_cm_on_lost_focus, |c| -> bool {
    mn::ctx_on_lost_focus(c)
});
ffn!(Ctx, ohpl_h20_cm_on_got_focus, |c| -> bool {
    mn::ctx_on_got_focus(c)
});
ffn!(Ctx, ohpl_h20_cm_on_key_press, |c, d: *const Msg| -> bool {
    mn::ctx_on_key_press(c, &rd_msg(d))
});
ffn!(Ctx, ohpl_h20_cm_on_load_graphics, |c| {
    mn::ctx_on_load_graphics(c)
});

ffn!(Main, ohpl_h20_mm_update_items_pos, |m, item: usize| {
    mn::main_update_items_pos(m, item)
});
ffn!(Main, ohpl_h20_mm_update_items_size, |m, item: usize| {
    mn::main_update_items_size(m, item)
});
ffn!(Main, ohpl_h20_mm_on_child_menu_hide, |m| {
    mn::main_on_child_menu_hide(m)
});
ffn!(Main, ohpl_h20_mm_hide, |m| mn::main_hide(m));
ffn!(Main, ohpl_h20_mm_on_draw, |m| mn::main_on_draw(m));
ffn!(Main, ohpl_h20_mm_on_update, |m| mn::main_on_update(m));
ffn!(Main, ohpl_h20_mm_on_init, |m| mn::main_on_init(m));
ffn!(Main, ohpl_h20_mm_on_load_graphics, |m| {
    mn::main_on_load_graphics(m)
});
