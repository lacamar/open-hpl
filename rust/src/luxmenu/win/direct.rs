//! Window-host ops that only forwarded to the gui set and the simple widgets: done against the
//! Rust-owned objects. List and text boxes stay with the C++ host.

use super::host::{Kind, W, f, set_op, wop};
use crate::gui::g1_set::{self, Set};
use crate::gui::{direct as d, gw, gw_kinds};
use crate::types::Color;

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
) -> Option<W> {
    let (s, p) = (set.addr(), parent.addr());
    let w = match kind {
        Kind::Window => d::widget_window(s, close, pos, size, text),
        Kind::Button => d::widget_button(s, pos, size, text, p),
        Kind::Label => d::widget_label(s, pos, size, text, p),
        Kind::Image => d::widget_image(s, &[], pos, size, d::MAT_ALPHA, false, p),
        Kind::Frame => d::widget_frame_scroll(s, pos, size, p, false, FRAME_VSCROLL != 0),
        Kind::ListBox | Kind::TextBox => return None,
    };
    Some(W::from_addr(w))
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
    let on = arg != 0;
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
        _ => return None,
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
