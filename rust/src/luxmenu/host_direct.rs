//! Menu host ops that only forwarded to the gui set and the simple widgets: done against the
//! Rust-owned objects. Combo, tab and the engine ops stay with the C++ host.

use super::host::{Args, op};
use crate::gui::g1_set::{self, Set};
use crate::gui::{direct as d, gw, gw_kinds};
use crate::types::Color;
use std::cell::RefCell;

thread_local! {
    static OUT: RefCell<Vec<u32>> = const { RefCell::new(Vec::new()) };
}

fn f(a: &Args, i: usize) -> f32 {
    a.f.get(i).copied().unwrap_or(0.0)
}

fn pos(a: &Args) -> [f32; 3] {
    [f(a, 0), f(a, 1), f(a, 2)]
}

fn size(a: &Args) -> [f32; 2] {
    [f(a, 3), f(a, 4)]
}

fn color(a: &Args) -> Color {
    Color {
        r: f(a, 0),
        g: f(a, 1),
        b: f(a, 2),
        a: f(a, 3),
    }
}

fn wide<'a>(a: &Args, i: usize) -> &'a [u32] {
    match (a.s.get(i), a.n.get(i)) {
        // SAFETY: the caller passes `n` readable units.
        (Some(&p), Some(&n)) if !p.is_null() && n != 0 => unsafe {
            std::slice::from_raw_parts(p, n)
        },
        _ => &[],
    }
}

fn narrow(a: &Args, i: usize) -> Vec<u8> {
    wide(a, i).iter().map(|&c| c as u8).collect()
}

fn out(a: &mut Args, text: Vec<u32>) {
    OUT.with(|o| {
        let mut o = o.borrow_mut();
        *o = text;
        a.s[0] = o.as_ptr();
        a.n[0] = o.len();
    });
}

fn create(code: i32, a: &Args) -> Option<usize> {
    let (set, parent) = (a.p[0], a.p[1]);
    let (p, s, t) = (pos(a), size(a), wide(a, 0));
    Some(match code {
        op::WINDOW => d::widget_window(set, false, p, s, t),
        op::BUTTON => d::widget_button(set, p, s, t, parent),
        op::LABEL => d::widget_label(set, p, s, t, parent),
        op::CHECKBOX => d::widget_checkbox(set, p, s, t, parent),
        op::SLIDER => d::widget_slider(set, 0, p, s, 0, parent),
        op::IMAGE if a.n[1] == 0 => {
            d::widget_image(set, &narrow(a, 0), p, s, d::MAT_ALPHA, false, parent)
        }
        op::GROUP => d::widget_group(set, p, s, t, parent),
        op::DUMMY => d::widget_dummy(set, p, parent),
        op::FRAME => d::widget_frame_scroll(set, p, s, parent, false, true),
        _ => return None,
    })
}

fn set_op(code: i32, a: &Args) -> Option<usize> {
    let s = a.p[0] as *mut Set;
    let target = a.p[1];
    let on = a.i[0] != 0;
    // SAFETY: the set is live; the widgets are live or null.
    unsafe {
        match code {
            op::POPUP_ACTIVE => return Some(usize::from(g1_set::ohpl_g1_set_popup_is_active(s))),
            op::SET_DEFAULT_NAV => g1_set::set_default_nav(s, target),
            op::SET_FOCUSED => g1_set::set_focused(s, target),
            op::PUSH_ATTENTION => g1_set::ohpl_g1_set_push_attention(s),
            op::POP_ATTENTION => g1_set::ohpl_g1_set_pop_attention(s, true),
            op::PUSH_FOCUSED => g1_set::ohpl_g1_set_push_focused(s),
            op::POP_FOCUSED => g1_set::ohpl_g1_set_pop_focused(s),
            op::PUSH_DEFAULT_NAV => g1_set::ohpl_g1_set_push_default_nav(s),
            op::SET_ATTENTION => g1_set::ohpl_g1_set_set_attention(s, target, true, false),
            op::SET_MOUSE_MOVE => g1_set::set_mouse_movement(s, on),
            op::SET_DRAW_MOUSE => g1_set::set_draw_mouse(s, on),
            _ => return None,
        }
    }
    Some(0)
}

fn widget_op(code: i32, a: &mut Args) -> Option<usize> {
    let w = a.p[0];
    let on = a.i[0] != 0;
    match code {
        op::SIZE => {
            let v = d::widget_get_size(w);
            a.f[0] = v[0];
            a.f[1] = v[1];
        }
        op::LOCAL_POS => {
            let v = d::widget_get_local_pos(w);
            a.f[..3].copy_from_slice(&v);
        }
        op::GLOBAL_POS => {
            let v = gw::global_position(w);
            a.f[..3].copy_from_slice(&[v.x, v.y, v.z]);
        }
        op::SET_SIZE => d::widget_size(w, [f(a, 0), f(a, 1)]),
        op::SET_POS => d::widget_pos(w, pos(a)),
        op::SET_TEXT => d::widget_text(w, wide(a, 0)),
        op::GET_TEXT => out(a, gw::text(w)),
        op::SET_VISIBLE => gw::set_visible(w, on),
        op::SET_ENABLED => gw::set_enabled(w, on),
        op::IS_VISIBLE => return Some(usize::from(gw::is_visible(w))),
        op::SET_USER_DATA => gw::ohpl_gw_set_user_data(w, a.p[1] as *mut _),
        op::GET_USER_DATA => return Some(gw::read(w, |g| g.user_data as usize)),
        op::SET_USER_VALUE => gw::ohpl_gw_set_user_value(w, a.i[0]),
        op::GET_USER_VALUE => return Some(gw::read(w, |g| g.user_value) as u32 as usize),
        op::SET_NAV => gw::ohpl_gw_set_focus_navigation(w, 1 << (a.i[0] & 3), a.p[1]),
        op::SET_GLOBAL_UI => gw::ohpl_gw_set_global_ui_listener(w, on),
        op::SET_STATIC => gw_kinds::ohpl_gw_window_set_static(w, on),
        op::FONT_COLOR => {
            let c = gw::read(w, |g| g.p.font_color);
            a.f[..4].copy_from_slice(&[c.r, c.g, c.b, c.a]);
        }
        op::SET_FONT_COLOR => d::widget_font_color(w, color(a)),
        op::FONT_SIZE => {
            let v = gw::read(w, |g| [g.p.font_size.x, g.p.font_size.y]);
            a.f[..2].copy_from_slice(&v);
        }
        op::SET_WORD_WRAP => d::label_wrap(w, on),
        op::SET_CLIP => gw::ohpl_gw_set_clip_active(w, on),
        op::SET_SCROLL_SPEED => gw_kinds::ohpl_gw_label_set_scroll_speed(w, f(a, 0)),
        op::SET_DRAW_BG => gw_kinds::ohpl_gw_label_set_draw_bg(w, on),
        op::SET_BG_COLOR => gw_kinds::ohpl_gw_label_set_bg_color(w, &color(a)),
        op::SET_ALIGN => d::label_align(w, 2),
        op::SET_AUTOSIZE => d::label_autosize(w, on),
        op::SET_SCROLL_OFFSET => gw_kinds::ohpl_gw_label_set_scroll_offset(w, f(a, 0)),
        op::PARENT => return Some(gw::read(w, |g| g.base.parent)),
        op::SET_CHECKED => gw_kinds::ohpl_gw_check_set_checked(w, on, a.i[1] != 0),
        op::IS_CHECKED => return Some(usize::from(gw_kinds::ohpl_gw_check_checked(w))),
        op::SLIDER_MAX => d::slider_max(w, a.i[0]),
        op::SLIDER_BAR => d::slider_bar(w, a.i[0]),
        op::SLIDER_SET => d::slider_set(w, a.i[0], a.i[1] != 0),
        op::SLIDER_GET => return Some(d::slider_get(w) as u32 as usize),
        op::SLIDER_GET_MAX => return Some(d::slider_get_max(w) as u32 as usize),
        op::FRAME_SCROLL => {
            let v = gw::read(w, |g| g.base.scroll);
            a.f[..3].copy_from_slice(&[v.x, v.y, v.z]);
        }
        op::FRAME_DRAW_BG => gw_kinds::ohpl_gw_frame_set_draw_bg(w, on),
        op::MOUSE_OVER => return Some(usize::from(gw::read(w, |g| g.base.mouse_over))),
        _ => return None,
    }
    Some(0)
}

/// Codes of the main menu's gui ops (`mainmenu::host::code`).
mod code {
    pub const SKIN_NEW: i32 = 200;
    pub const SET_NEW: i32 = 201;
    pub const SET_ACTIVE: i32 = 204;
    pub const GFX_RECT: i32 = 212;
    pub const GFX_IMAGE: i32 = 213;
    pub const GFX_SIZE: i32 = 214;
    pub const GFX_TEX: i32 = 215;
    pub const GFX_DESTROY: i32 = 216;
    pub const DRAW_GFX: i32 = 217;
    pub const SET_VSIZE: i32 = 218;
    pub const WIDGET_DESTROY: i32 = 219;
    pub const SET_DESTROY_ALL: i32 = 220;
    pub const WIDGET_NAME: i32 = 221;
    pub const WIDGET_HAS_FOCUS: i32 = 222;
    pub const WIDGET_ENABLED: i32 = 223;
    pub const SET_DRAW_FOCUS: i32 = 228;
    pub const SET_GET_DRAW_FOCUS: i32 = 229;
    pub const SET_GET_FOCUSED: i32 = 230;
    pub const WIDGET_SET_GLOBAL_POS: i32 = 256;
    pub const GUI_FOCUS: i32 = 257;
    pub const WIDGET_CENTER: i32 = 258;
}

const MAT_DIFFUSE: i32 = 0;

fn menu_op(c: i32, a: &mut Args) -> Option<usize> {
    let (w, on) = (a.p[0], a.i[0] != 0);
    let s = w as *mut Set;
    match c {
        code::SKIN_NEW => return Some(d::create_skin(&narrow(a, 0))),
        code::SET_NEW => return Some(d::create_set(&narrow(a, 0), a.p[0])),
        code::SET_ACTIVE => d::set_active(w, on),
        code::GFX_RECT => return Some(d::gfx_rect(color(a), d::MAT_ALPHA, true)),
        code::GFX_IMAGE => return Some(d::gfx_image(&narrow(a, 0), d::MAT_ALPHA)),
        code::GFX_SIZE => {
            let v = d::gfx_active_size(w);
            a.f[..2].copy_from_slice(&v);
        }
        code::GFX_TEX => {
            let mat = if on { d::MAT_ALPHA } else { MAT_DIFFUSE };
            return Some(d::gfx_from_texture(w, false, mat));
        }
        code::GFX_DESTROY => d::gfx_destroy(w),
        code::DRAW_GFX => {
            let col = if on {
                Color {
                    r: 1.0,
                    g: 1.0,
                    b: 1.0,
                    a: f(a, 5),
                }
            } else {
                d::WHITE
            };
            d::draw_gfx(w, a.p[1], pos(a), size(a), col, d::MAT_LAST, 0.0);
        }
        code::SET_VSIZE => {
            let r = g1_set::virtual_rect(s);
            a.f[..2].copy_from_slice(&r[..2]);
        }
        code::WIDGET_DESTROY => d::widget_destroy(w, a.p[1]),
        // SAFETY: the set is live.
        code::SET_DESTROY_ALL => unsafe { g1_set::ohpl_g1_set_destroy_all_widgets(s) },
        code::WIDGET_NAME => {
            let name = gw::read(w, |g| g.name.iter().map(|&b| u32::from(b)).collect());
            out(a, name);
        }
        code::WIDGET_HAS_FOCUS => return Some(usize::from(gw::has_focus(w))),
        code::WIDGET_ENABLED => return Some(usize::from(gw::is_enabled(w))),
        code::SET_DRAW_FOCUS => g1_set::set_draw_focus(s, on),
        code::SET_GET_DRAW_FOCUS => return Some(usize::from(g1_set::draw_focus(s))),
        code::SET_GET_FOCUSED => return Some(g1_set::focused_widget(s)),
        code::WIDGET_SET_GLOBAL_POS => {
            let p = pos(a);
            gw::set_global_position(
                w,
                crate::types::Vector3f {
                    x: p[0],
                    y: p[1],
                    z: p[2],
                },
            );
        }
        code::GUI_FOCUS => d::gui_focus(w),
        code::WIDGET_CENTER => gw::center_in_set(w),
        _ => return None,
    }
    Some(0)
}

/// The result of `code` when it is a gui-only op; `None` leaves it to the host.
pub(super) fn call(code: i32, a: &mut Args) -> Option<usize> {
    match code {
        op::WINDOW..=op::FRAME => create(code, a),
        op::SIZE..=op::MOUSE_OVER => widget_op(code, a),
        op::POPUP_ACTIVE..=op::SET_DRAW_MOUSE => set_op(code, a),
        code::SKIN_NEW..=code::WIDGET_CENTER => menu_op(code, a),
        _ => None,
    }
}
