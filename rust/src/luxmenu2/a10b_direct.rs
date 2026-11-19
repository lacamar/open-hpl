//! A10b host ops that only forwarded to the gui, its sets, gfx elements and simple widgets: done
//! against the Rust-owned objects, bypassing the C++ host (the trace still records them).

use super::host::*;
use crate::gui::direct as d;
use crate::types::Color;

fn p(a: &[Arg], i: usize) -> usize {
    match a.get(i) {
        Some(Arg::P(v)) => *v,
        Some(Arg::I(v)) => *v as usize,
        _ => 0,
    }
}

fn n(a: &[Arg], i: usize) -> i32 {
    match a.get(i) {
        Some(Arg::I(v)) => *v as i32,
        _ => 0,
    }
}

fn f(a: &[Arg], i: usize) -> f32 {
    match a.get(i) {
        Some(Arg::F(v)) => *v,
        _ => 0.0,
    }
}

fn b(a: &[Arg], i: usize) -> bool {
    matches!(a.get(i), Some(Arg::B(true)))
}

fn s<'a>(a: &[Arg<'a>], i: usize) -> &'a [u8] {
    match a.get(i) {
        Some(Arg::S(v)) => v,
        _ => &[],
    }
}

fn w<'a>(a: &[Arg<'a>], i: usize) -> &'a [u32] {
    match a.get(i) {
        Some(Arg::W(v)) => v,
        _ => &[],
    }
}

fn v2(a: &[Arg], i: usize) -> [f32; 2] {
    [f(a, i), f(a, i + 1)]
}

fn v3(a: &[Arg], i: usize) -> [f32; 3] {
    [f(a, i), f(a, i + 1), f(a, i + 2)]
}

fn c4(a: &[Arg], i: usize) -> Color {
    Color {
        r: f(a, i),
        g: f(a, i + 1),
        b: f(a, i + 2),
        a: f(a, i + 3),
    }
}

fn pr(v: usize) -> Res {
    let mut r = Res::default();
    r.u[0] = v as u64;
    r
}

fn fr(v: &[f32]) -> Res {
    let mut r = Res::default();
    for (u, x) in r.u.iter_mut().zip(v) {
        *u = u64::from(x.to_bits());
    }
    r
}

/// The result of `op` when it is a forwarding op; `None` leaves it to the host.
pub(super) fn call(op: i32, a: &[Arg]) -> Option<Res> {
    let unit = Res::default();
    Some(match op {
        GUI_CREATE_SET => pr(d::create_set(s(a, 0), p(a, 1))),
        GUI_CREATE_SKIN => pr(d::create_skin(s(a, 0))),
        SET_VIRTUAL_SIZE => {
            d::set_virtual_size(p(a, 0), v2(a, 1), f(a, 3), f(a, 4), v2(a, 5), b(a, 7));
            unit
        }
        SET_ACTIVE => {
            d::set_active(p(a, 0), b(a, 1));
            unit
        }
        SET_DRAW_MOUSE => {
            d::set_draw_mouse(p(a, 0), b(a, 1));
            unit
        }
        SET_SKIN => {
            d::set_skin(p(a, 0), p(a, 1));
            unit
        }
        GUI_SET_FOCUS => {
            d::gui_focus(p(a, 0));
            unit
        }
        GFX_RECT => pr(d::gfx_rect(c4(a, 0), d::MAT_ALPHA, b(a, 4))),
        GFX_TEX_FILE => pr(d::gfx_texture_file(s(a, 0), d::MAT_ALPHA, d::TEXTURE_RECT)),
        GFX_FROM_TEX => pr(d::gfx_from_texture(p(a, 0), true, d::MAT_ALPHA)),
        GFX_DESTROY => {
            d::gfx_destroy(p(a, 0));
            unit
        }
        GFX_SIZE => fr(&d::gfx_image_size(p(a, 0))),
        DRAW_GFX => {
            d::draw_gfx(
                p(a, 0),
                p(a, 1),
                v3(a, 2),
                v2(a, 5),
                c4(a, 7),
                d::MAT_LAST,
                0.0,
            );
            unit
        }
        DRAW_GFX_POS => {
            d::draw_gfx(
                p(a, 0),
                p(a, 1),
                v3(a, 2),
                [-1.0, -1.0],
                d::WHITE,
                d::MAT_LAST,
                0.0,
            );
            unit
        }
        WIDGET_LABEL => pr(d::widget_label(
            p(a, 0),
            v3(a, 1),
            v2(a, 4),
            w(a, 6),
            p(a, 7),
        )),
        WIDGET_BUTTON => pr(d::widget_button(
            p(a, 0),
            v3(a, 1),
            v2(a, 4),
            w(a, 6),
            p(a, 7),
        )),
        WIDGET_DUMMY => pr(d::widget_dummy(p(a, 0), v3(a, 1), p(a, 4))),
        WIDGET_IMAGE => pr(d::widget_image(
            p(a, 0),
            s(a, 1),
            v3(a, 2),
            v2(a, 5),
            n(a, 7),
            b(a, 8),
            p(a, 9),
        )),
        WIDGET_SLIDER => pr(d::widget_slider(
            p(a, 0),
            n(a, 1),
            v3(a, 2),
            v2(a, 5),
            n(a, 7),
            p(a, 8),
        )),
        WIDGET_DESTROY => {
            d::widget_destroy(p(a, 0), p(a, 1));
            unit
        }
        WIDGET_POS => {
            d::widget_pos(p(a, 0), v3(a, 1));
            unit
        }
        WIDGET_SIZE => {
            d::widget_size(p(a, 0), v2(a, 1));
            unit
        }
        WIDGET_GET_SIZE => fr(&d::widget_get_size(p(a, 0))),
        WIDGET_GET_LOCAL_POS => fr(&d::widget_get_local_pos(p(a, 0))),
        WIDGET_VISIBLE => {
            crate::gui::gw::set_visible(p(a, 0), b(a, 1));
            unit
        }
        WIDGET_ENABLED => {
            crate::gui::gw::set_enabled(p(a, 0), b(a, 1));
            unit
        }
        WIDGET_IS_VISIBLE => pr(usize::from(crate::gui::gw::is_visible(p(a, 0)))),
        WIDGET_TEXT => {
            d::widget_text(p(a, 0), w(a, 1));
            unit
        }
        WIDGET_FONT_COLOR => {
            d::widget_font_color(p(a, 0), c4(a, 1));
            unit
        }
        WIDGET_FONT_SIZE => {
            d::widget_font_size(p(a, 0), v2(a, 1));
            unit
        }
        WIDGET_ALIGN => {
            d::label_align(p(a, 0), n(a, 1));
            unit
        }
        WIDGET_WRAP => {
            d::label_wrap(p(a, 0), b(a, 1));
            unit
        }
        WIDGET_FONT_TYPE => {
            d::widget_font_type(p(a, 0), p(a, 1));
            unit
        }
        WIDGET_AUTOSIZE => {
            d::label_autosize(p(a, 0), b(a, 1));
            unit
        }
        WIDGET_DEFAULT_FONT => pr(d::widget_default_font(p(a, 0))),
        WIDGET_GLOBAL_UI => {
            crate::gui::gw::ohpl_gw_set_global_ui_listener(p(a, 0), b(a, 1));
            unit
        }
        SLIDER_MAX => {
            d::slider_max(p(a, 0), n(a, 1));
            unit
        }
        SLIDER_BAR => {
            d::slider_bar(p(a, 0), n(a, 1));
            unit
        }
        SLIDER_SET => {
            d::slider_set(p(a, 0), n(a, 1), b(a, 2));
            unit
        }
        SLIDER_GET => pr(d::slider_get(p(a, 0)) as u64 as usize),
        SLIDER_GET_MAX => pr(d::slider_get_max(p(a, 0)) as u64 as usize),
        SET_DEFAULT_FOCUS_NAV => {
            d::set_default_nav(p(a, 0), p(a, 1));
            unit
        }
        SET_FOCUSED => {
            d::set_focused(p(a, 0), p(a, 1));
            unit
        }
        _ => return None,
    })
}
