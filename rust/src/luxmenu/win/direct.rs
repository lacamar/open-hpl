//! Window-host ops that only forwarded to the gui set and the widgets: done against the
//! Rust-owned objects.

use super::host::{Kind, W, f, set_op, wop};
use super::ohpl_a9_cb;
use crate::gui::g1_set::{self, Set};
use crate::gui::gw_h20::{self, P};
use crate::gui::h20_list::ListBase;
use crate::gui::h20_text::TextBox;
use crate::gui::set::Msg;
use crate::gui::{direct as d, gw, gw_items as gi, gw_kinds, h20_ffi};
use crate::types::Color;
use std::ffi::c_void;

const ALIGN_CENTER: i32 = 2;
const FRAME_VSCROLL: i32 = 1;

fn color(v: &[f32]) -> Color {
    let at = |i: usize| v.get(i).copied().unwrap_or(0.0);
    Color {
        r: at(0),
        g: at(1),
        b: at(2),
        a: at(3),
    }
}

/// `ohpl_a9h_new`.
pub(super) fn create(
    set: W,
    kind: Kind,
    parent: W,
    pos: [f32; 3],
    size: [f32; 2],
    close: bool,
    text: &[u32],
) -> W {
    let (s, p) = (set.addr(), parent.addr());
    let w = match kind {
        Kind::Window => d::widget_window(s, close, pos, size, text),
        Kind::Button => d::widget_button(s, pos, size, text, p),
        Kind::Label => d::widget_label(s, pos, size, text, p),
        Kind::Image => d::widget_image(s, &[], pos, size, d::MAT_ALPHA, false, p),
        Kind::Frame => d::widget_frame_scroll(s, pos, size, p, false, FRAME_VSCROLL != 0),
        Kind::ListBox => d::widget_list(s, pos, size, p),
        Kind::TextBox => d::widget_textbox(s, pos, size, text, p),
    };
    W::from_addr(w)
}

/// `ohpl_a9h_set`.
pub(super) fn set(set: W, op: i32, w: W, arg: usize) -> Option<usize> {
    let s = set.addr() as *mut Set;
    let target = w.addr();
    // SAFETY: the set is live; the widgets are live or null.
    unsafe {
        Some(match op {
            set_op::DRAW_FOCUS => {
                g1_set::set_draw_focus(s, arg != 0);
                0
            }
            set_op::GET_DRAW_FOCUS => usize::from(g1_set::draw_focus(s)),
            set_op::FOCUSED => {
                g1_set::set_focused(s, target);
                0
            }
            set_op::DEFAULT_FOCUS => {
                g1_set::set_default_nav(s, target);
                0
            }
            set_op::POPUP_ACTIVE => usize::from(g1_set::ohpl_g1_set_popup_is_active(s)),
            set_op::PUSH_ATTENTION => {
                g1_set::ohpl_g1_set_push_attention(s);
                0
            }
            set_op::PUSH_DEFAULT => {
                g1_set::ohpl_g1_set_push_default_nav(s);
                0
            }
            set_op::PUSH_FOCUSED => {
                g1_set::ohpl_g1_set_push_focused(s);
                0
            }
            set_op::POP_ATTENTION => {
                g1_set::ohpl_g1_set_pop_attention(s, true);
                0
            }
            set_op::POP_DEFAULT => {
                g1_set::ohpl_g1_set_pop_default_nav(s);
                0
            }
            set_op::POP_FOCUSED => {
                g1_set::ohpl_g1_set_pop_focused(s);
                0
            }
            set_op::ATTENTION => {
                g1_set::ohpl_g1_set_set_attention(s, target, true, false);
                0
            }
            set_op::GET_ATTENTION => g1_set::attention_widget(s),
            _ => return None,
        })
    }
}

/// `ohpl_a9h_w`.
pub(super) fn op(w: W, op: i32, arg: usize) -> Option<usize> {
    let w = w.addr();
    op_inner(w, op, arg)
}

fn op_inner(w: usize, op: i32, arg: usize) -> Option<usize> {
    let on = arg != 0;
    // SAFETY: the widget is live; the state pointers are null for another kind.
    unsafe {
        match op {
            wop::ENABLED => gw::set_enabled(w, on),
            wop::VISIBLE => gw::set_visible(w, on),
            wop::IS_VISIBLE => return Some(usize::from(gw::is_visible(w))),
            wop::STATIC => gw_kinds::ohpl_gw_window_set_static(w, on),
            wop::CENTER => gw::center_in_set(w),
            wop::GLOBAL_UI => gw::ohpl_gw_set_global_ui_listener(w, on),
            wop::USER_DATA => gw::ohpl_gw_set_user_data(w, arg as *mut _),
            wop::CLOSE_DISABLES => gw_kinds::ohpl_gw_window_set_close_disables(w, on),
            wop::WORD_WRAP => d::label_wrap(w, on),
            wop::CLIP => gw::ohpl_gw_set_clip_active(w, on),
            wop::DRAW_BG => gw_kinds::ohpl_gw_label_set_draw_bg(w, on),
            wop::AUTOGEN => d::label_autosize(w, on),
            wop::ALIGN_CENTER => d::label_align(w, ALIGN_CENTER),
            wop::FORCE_ENTER => {
                if let Some(t) = text_box(w) {
                    t.st.force_enter = on;
                }
            }
            wop::CB_LOST_FOCUS => {
                if let Some(t) = text_box(w) {
                    t.st.enter_on_lost_focus = on;
                }
            }
            wop::MAX_LEN => h20_ffi::ohpl_h20_tb_set_max_text_length(text_ptr(w), arg as i32),
            wop::SELECT_TEXT => h20_ffi::ohpl_h20_tb_set_selected_text(text_ptr(w), 0, -1),
            wop::LIST_CLEAR => gi::clear(w),
            wop::LIST_NUM => return Some(gi::num(w)),
            wop::LIST_SELECTED => {
                return Some(h20_ffi::ohpl_h20_lb_selected_item(list_ptr(w)) as isize as usize);
            }
            wop::LIST_SET_SELECTED => h20_ffi::ohpl_h20_lb_set_selected_item(
                list_ptr(w),
                arg as i32,
                false,
                true,
                LIST_SELECT_TOGGLE,
                true,
            ),
            wop::LIST_SET_LOCKED => {
                if let Some(l) = list_ptr(w).as_mut() {
                    l.list.locked = on;
                }
            }
            wop::LIST_LOCKED => {
                return Some(usize::from(
                    list_ptr(w).as_ref().is_some_and(|l| l.list.locked),
                ));
            }
            wop::LIST_ITEM_USER => {
                return Some(gi::ohpl_gwi_user_data(gi::get(w, arg as i32)) as usize);
            }
            _ => return None,
        }
    }
    Some(0)
}

/// `ohpl_a9h_w_set_f`; true when done.
pub(super) fn set_f(w: W, op: i32, v: &[f32]) -> bool {
    let w = w.addr();
    let at = |i: usize| v.get(i).copied().unwrap_or(0.0);
    match op {
        f::POS => d::widget_pos(w, [at(0), at(1), at(2)]),
        f::SIZE => d::widget_size(w, [at(0), at(1)]),
        f::FONT_COLOR => d::widget_font_color(w, color(v)),
        f::COLOR_MUL => gw::ohpl_gw_set_color_mul(w, &color(v)),
        f::BG_COLOR => gw_kinds::ohpl_gw_label_set_bg_color(w, &color(v)),
        f::SCROLL_SPEED => gw_kinds::ohpl_gw_label_set_scroll_speed(w, at(0)),
        f::FONT_SIZE => gw::set_default_font_size(w, crate::types::Vector2f { x: at(0), y: at(1) }),
        _ => return false,
    }
    true
}

/// `ohpl_a9h_w_get_f`.
pub(super) fn get_f(w: W, op: i32) -> Vec<f32> {
    if op == f::POS {
        d::widget_get_local_pos(w.addr()).to_vec()
    } else {
        d::widget_get_size(w.addr()).to_vec()
    }
}

pub(super) fn set_text(w: W, text: &[u32]) {
    d::widget_text(w.addr(), text);
}

pub(super) fn text(w: W) -> Vec<u32> {
    gw::text(w.addr())
}

pub(super) fn nav(w: W, dir: i32, target: W) {
    let flag = match dir {
        0 => 1,
        1 => 2,
        2 => 4,
        _ => 8,
    };
    gw::ohpl_gw_set_focus_navigation(w.addr(), flag, target.addr());
}

const LIST_SELECT_TOGGLE: i32 = 1;

fn text_ptr(w: usize) -> *mut TextBox {
    match gw_h20::ptr(w) {
        Some(P::Text(t)) => t,
        _ => std::ptr::null_mut(),
    }
}

fn text_box<'a>(w: usize) -> Option<&'a mut TextBox> {
    // SAFETY: the state is boxed and lives as long as the widget; the borrow ends in the caller.
    unsafe { text_ptr(w).as_mut() }
}

fn list_ptr(w: usize) -> *mut ListBase {
    match gw_h20::ptr(w) {
        Some(P::List(t)) => t,
        _ => std::ptr::null_mut(),
    }
}

/// `eGuiMessage` of the window messages, in `host::msg` order.
const MESSAGES: [i32; 9] = [10, 11, 19, 27, 13, 22, 26, 31, 28];

extern "C" fn tramp(obj: *mut c_void, w: usize, d: *const Msg) -> bool {
    // SAFETY: `d` is the message the widget passes.
    ohpl_a9_cb(
        obj as usize,
        W::from_addr(w),
        unsafe { d.as_ref() }.map_or(0, Msg::val),
    )
}

/// `ohpl_a9h_w_cb`.
pub(super) fn add_callback(w: W, msg: i32, obj: usize) {
    if let Some(&m) = usize::try_from(msg).ok().and_then(|i| MESSAGES.get(i)) {
        gw::add_callback(w.addr(), m, obj as *mut c_void, tramp);
    }
}

/// `ohpl_a9h_text_width`.
pub(super) fn text_width(w: W, text: &[u32]) -> f32 {
    let w = w.addr();
    gw::font_length(
        d::widget_default_font(w),
        gw::read(w, |g| g.p.font_size),
        text,
    )
}

/// `ohpl_a9h_image_file`.
pub(super) fn set_image_file(w: W, file: &[u32]) {
    let w = w.addr();
    let old = gw_kinds::ohpl_gw_image(w) as usize;
    if old != 0 {
        d::gfx_destroy(old);
    }
    let name: Vec<u8> = file.iter().map(|&c| c as u8).collect();
    let g = d::gfx_texture_file(&name, d::MAT_ALPHA, d::TEXTURE_RECT);
    gw_kinds::ohpl_gw_image_set(w, g as *mut _);
}

/// `ohpl_a9h_w_str` with `ILLEGAL`.
pub(super) fn set_illegal_chars(w: W, text: &[u32]) {
    // SAFETY: the widget is a live text box; `text` is readable.
    unsafe {
        h20_ffi::ohpl_h20_tb_set_illegal_chars(text_ptr(w.addr()), text.as_ptr(), text.len());
    }
}

/// `ohpl_a9h_w_str` with `LIST_ADD`: the text, and `user` as the item's user data when not 0.
pub(super) fn list_add(w: W, text: &[u32], user: usize) {
    let item = gi::add(w.addr(), text);
    if user != 0 {
        gi::ohpl_gwi_set_user_data(item, user as *mut c_void);
    }
}

/// `ohpl_a9h_w_get_str` with `LIST_ADD`.
pub(super) fn list_item_text(w: W, idx: i32) -> Vec<u32> {
    let mut n = 0usize;
    let p = gi::ohpl_gwi_text(gi::get(w.addr(), idx), &mut n);
    if p.is_null() {
        Vec::new()
    } else {
        // SAFETY: the item text is readable for `n` units.
        unsafe { std::slice::from_raw_parts(p, n) }.to_vec()
    }
}

/// `ohpl_a9h_popup`.
pub(super) fn popup(set: W, strs: [&[u32]; 4], obj: usize, kill_escape: i32) -> W {
    let s = set.addr();
    // SAFETY: `s` is a live set; the strings are readable for the call.
    unsafe {
        let skin = g1_set::ohpl_g1_set_skin(s as *mut Set) as usize;
        let cb = if obj != 0 {
            tramp as extern "C" fn(*mut c_void, usize, *const Msg) -> bool as usize
        } else {
            0
        };
        let p = crate::gui::g2_popup::ohpl_g2_popup_msgbox(
            s,
            skin,
            strs[0].as_ptr(),
            strs[0].len(),
            strs[1].as_ptr(),
            strs[1].len(),
            strs[2].as_ptr(),
            strs[2].len(),
            strs[3].as_ptr(),
            strs[3].len(),
            obj,
            cb,
        );
        if kill_escape >= 0 {
            crate::gui::g2_popup::ohpl_g2_popup_set_kill_on_escape(p, kill_escape != 0);
        }
        W::from_addr(p as usize)
    }
}
