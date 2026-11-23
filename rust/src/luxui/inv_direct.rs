//! Inventory/journal host ops that only forwarded to the gui set, gfx elements and simple
//! widgets: done against the Rust-owned objects. The set of each flavor is the one `GUI_INIT`
//! returned.

use super::event::Str;
use super::invhost::{A, JOURNAL_BASE, WStr, op};
use crate::gui::g1_gfx::{self, Element};
use crate::gui::g1_set::{self, Clip, Set};
use crate::gui::{direct as d, g1_gui, gw, gw_kinds};
use crate::types::{Color, Vector2f, Vector3f};
use std::cell::{Cell, RefCell};

thread_local! {
    static SETS: Cell<(usize, usize)> = const { Cell::new((0, 0)) };
    static TEXT: RefCell<Vec<u32>> = const { RefCell::new(Vec::new()) };
    static ROWS: RefCell<Vec<Vec<u32>>> = const { RefCell::new(Vec::new()) };
}

const MAT_ADDITIVE: i32 = 3;
const MAT_MODULATIVE: i32 = 4;
const MSG_GET_UI_NAV_FOCUS: i32 = 22;

fn journal(code: i32) -> bool {
    code >= JOURNAL_BASE
}

/// The op without the journal offset.
fn base(code: i32) -> i32 {
    if journal(code) {
        code - JOURNAL_BASE
    } else {
        code
    }
}

fn set_of(code: i32) -> usize {
    let (inv, jrn) = SETS.with(Cell::get);
    if journal(code) { jrn } else { inv }
}

/// Remembers the set a `GUI_INIT` created.
pub(super) fn after(code: i32, a: &A) {
    if base(code) == op::GUI_INIT {
        SETS.with(|s| {
            let (inv, jrn) = s.get();
            s.set(if journal(code) {
                (inv, a.r)
            } else {
                (a.r, jrn)
            });
        });
    }
}

fn mat(code: i32) -> i32 {
    match code {
        0 => d::MAT_ALPHA,
        1 => MAT_ADDITIVE,
        2 => 0,
        3 => MAT_MODULATIVE,
        _ => d::MAT_LAST,
    }
}

fn f(a: &A, i: usize) -> f32 {
    a.f.get(i).copied().unwrap_or(0.0)
}

fn v3(a: &A, i: usize) -> [f32; 3] {
    [f(a, i), f(a, i + 1), f(a, i + 2)]
}

fn v2(a: &A, i: usize) -> [f32; 2] {
    [f(a, i), f(a, i + 1)]
}

fn col(a: &A, i: usize) -> Color {
    Color {
        r: f(a, i),
        g: f(a, i + 1),
        b: f(a, i + 2),
        a: f(a, i + 3),
    }
}

fn bytes<'a>(a: &A, i: usize) -> &'a [u8] {
    match a.s.get(i) {
        // SAFETY: the caller passes `n` readable bytes.
        Some(s) if !s.p.is_null() && s.n != 0 => unsafe { std::slice::from_raw_parts(s.p, s.n) },
        _ => &[],
    }
}

fn wide<'a>(a: &A, i: usize) -> &'a [u32] {
    match a.w.get(i) {
        // SAFETY: the caller passes `n` readable units.
        Some(s) if !s.p.is_null() && s.n != 0 => unsafe { std::slice::from_raw_parts(s.p, s.n) },
        _ => &[],
    }
}

fn set_op(set: usize, a: &mut A) -> bool {
    let s = set as *mut Set;
    let on = a.i[1] != 0;
    match a.i[0] {
        0 => g1_set::set_active(s, on),
        1 => g1_set::set_draw_mouse(s, on),
        2 => g1_set::set_mouse_movement(s, on),
        3 => g1_set::set_draw_focus(s, on),
        // SAFETY: the inventory's set is live.
        4 => unsafe { g1_set::ohpl_g1_set_reset_mouse_over(s) },
        5 => d::gui_focus(if on { set } else { 0 }),
        6 => g1_set::set_focused(s, a.p[0]),
        7 => g1_set::set_default_nav(s, a.p[0]),
        10 => {
            let v = g1_set::mouse_pos(s);
            a.f[0] = v[0];
            a.f[1] = v[1];
        }
        11 => a.f[0] = g1_set::mouse_z(s),
        12 => a.r = g1_set::focused_widget(s),
        // SAFETY: the inventory's set is live.
        13 => a.r = unsafe { g1_set::ohpl_g1_set_pointer(s) } as usize,
        14 => a.r = g1_set::current_clip(s) as usize,
        15 => g1_set::set_current_clip(s, a.p[0] as *mut Clip),
        16 => {
            let (pos, size) = (
                Vector3f {
                    x: a.f[0],
                    y: a.f[1],
                    z: a.f[2],
                },
                Vector2f {
                    x: a.f[3],
                    y: a.f[4],
                },
            );
            // SAFETY: the clip region is live; the vectors outlive the call.
            a.r = unsafe { g1_set::ohpl_g1_clip_create_child(a.p[0] as *mut Clip, &pos, &size) }
                as usize;
        }
        _ => return false,
    }
    true
}

fn gfx_new(a: &mut A) -> bool {
    let m = mat(a.i[1]);
    a.r = match a.i[0] {
        0 => d::gfx_image(bytes(a, 0), m),
        1 => d::gfx_rect(col(a, 0), m, true),
        2 => g1_gui::create_gfx_texture(
            g1_gui::current(),
            a.p[0],
            false,
            m,
            d::WHITE,
            true,
            [0.0, 0.0],
            [1.0, 1.0],
        ) as usize,
        3 => d::gfx_texture_file(bytes(a, 0), d::MAT_ALPHA, d::TEXTURE_RECT),
        _ => return false,
    };
    true
}

fn gfx_op(a: &mut A) -> bool {
    let g = a.p[0] as *mut Element;
    match a.i[0] {
        0 => {
            if a.p[0] != 0 {
                d::gfx_destroy(a.p[0]);
            }
        }
        1 => {
            let (v, u) = (d::gfx_active_size(a.p[0]), d::gfx_image_size(a.p[0]));
            a.f[..4].copy_from_slice(&[v[0], v[1], u[0], u[1]]);
        }
        2 => {
            let c = col(a, 0);
            // SAFETY: the element is live.
            unsafe { g1_gfx::ohpl_g1_gfx_set_color(g, &c) };
        }
        3 => {
            let v = Vector2f {
                x: a.f[0],
                y: a.f[1],
            };
            // SAFETY: the element is live.
            unsafe { g1_gfx::ohpl_g1_gfx_set_active_size(g, &v) };
        }
        _ => return false,
    }
    true
}

fn widget_new(set: usize, a: &mut A) -> bool {
    a.r = if a.i[0] == 0 {
        d::widget_image(
            set,
            bytes(a, 0),
            v3(a, 0),
            [-1.0, -1.0],
            d::MAT_ALPHA,
            false,
            a.p[0],
        )
    } else {
        d::widget_dummy(set, v3(a, 0), 0)
    };
    true
}

fn widget_new2(set: usize, a: &mut A) -> bool {
    let parent = a.p[0];
    a.r = match a.i[0] {
        0 => d::widget_label(set, v3(a, 0), v2(a, 3), wide(a, 0), parent),
        1 => d::widget_image(
            set,
            bytes(a, 0),
            v3(a, 0),
            v2(a, 3),
            d::MAT_ALPHA,
            false,
            parent,
        ),
        2 => d::widget_dummy(set, v3(a, 0), parent),
        3 => d::widget_frame(set, v3(a, 0), v2(a, 3), parent),
        _ => return false,
    };
    true
}

fn widget_get(a: &mut A) -> bool {
    let w = a.p[0];
    match a.i[0] {
        0 => {
            let v = d::widget_get_size(w);
            a.f[..2].copy_from_slice(&v);
        }
        1 => {
            let v = gw::global_position(w);
            a.f[..3].copy_from_slice(&[v.x, v.y, v.z]);
        }
        2 => a.r = gw::read(w, |g| g.user_value) as isize as usize,
        3 => {
            let c = gw::read(w, |g| {
                [
                    g.p.color_mul.r,
                    g.p.color_mul.g,
                    g.p.color_mul.b,
                    g.p.color_mul.a,
                ]
            });
            a.f[..4].copy_from_slice(&c);
        }
        4 => a.r = gw_kinds::ohpl_gw_image(w) as usize,
        _ => return false,
    }
    true
}

fn widget_set(a: &mut A) -> bool {
    let w = a.p[0];
    match a.i[0] {
        0 => d::widget_pos(w, v3(a, 0)),
        1 => d::widget_size(w, v2(a, 0)),
        2 => gw_kinds::ohpl_gw_image_set(w, a.p[1] as *mut Element),
        3 => gw::with(w, |g| g.user_value = a.i[1]).unwrap_or_default(),
        4 => {
            let c = col(a, 0);
            gw::ohpl_gw_set_color_mul(w, &c);
        }
        5 => gw::ohpl_gw_set_global_ui_listener(w, a.i[1] != 0),
        6 => {
            let dir = match a.i[1] {
                0 => 1,
                1 => 2,
                2 => 4,
                _ => 8,
            };
            gw::ohpl_gw_set_focus_navigation(w, dir, a.p[1]);
        }
        7 => {
            gw::process_message(
                w,
                MSG_GET_UI_NAV_FOCUS,
                &crate::gui::set::Msg::zero(),
                false,
                false,
            );
        }
        _ => return false,
    }
    true
}

fn widget_x(set: usize, a: &mut A) -> bool {
    let w = a.p[0];
    match a.i[0] {
        0 => gw::set_visible(w, a.i[1] != 0),
        1 => gw::set_enabled(w, a.i[1] != 0),
        2 => {
            d::widget_font_color(w, col(a, 0));
            d::widget_font_size(w, v2(a, 4));
            d::label_align(w, a.i[1]);
            d::widget_font_type(w, a.p[1]);
        }
        3 => a.r = usize::from(gw::is_visible(w)),
        4 => a.r = usize::from(gw::read(w, |g| g.base.mouse_over) || gw::has_focus(w)),
        5 => TEXT.with(|t| {
            let mut t = t.borrow_mut();
            *t = gw::text(w);
            a.rw.p = t.as_ptr();
            a.rw.n = t.len();
        }),
        6 => {
            let v = gw::read(w, |g| [g.p.font_size.x, g.p.font_size.y]);
            a.f[..2].copy_from_slice(&v);
        }
        7 => a.r = d::widget_default_font(w),
        8 => a.r = gw_kinds::ohpl_gw_label_align(w) as isize as usize,
        9 => d::widget_destroy(set, w),
        10 => {
            a.f[0] = gw::font_length(
                w,
                Vector2f {
                    x: f(a, 0),
                    y: f(a, 1),
                },
                wide(a, 0),
            );
        }
        _ => return false,
    }
    true
}

/// True when `op` (with the journal offset) was done here; false leaves it to the host.
pub(super) fn call(code: i32, a: &mut A) -> bool {
    // Tests replay against fake host pointers.
    let set = set_of(code);
    if cfg!(test) || set == 0 {
        return false;
    }
    a.rs = Str::NONE;
    a.rw = WStr::NONE;
    match base(code) {
        op::SET_OP => set_op(set, a),
        op::GFX_NEW => gfx_new(a),
        op::GFX_OP => gfx_op(a),
        op::DRAW_GFX => {
            d::draw_gfx(set, a.p[0], v3(a, 0), v2(a, 3), col(a, 5), mat(a.i[0]), 0.0);
            true
        }
        op::WIDGET_NEW => widget_new(set, a),
        op::WIDGET_NEW2 => widget_new2(set, a),
        op::WIDGET_GET => widget_get(a),
        op::WIDGET_SET => widget_set(a),
        op::WIDGET_X => widget_x(set, a),
        op::DRAW_FONT => {
            let size = Vector2f {
                x: f(a, 3),
                y: f(a, 4),
            };
            let pos = Vector3f {
                x: f(a, 0),
                y: f(a, 1),
                z: f(a, 2),
            };
            gw::draw_font(
                set as *mut Set,
                a.p[0],
                wide(a, 0),
                size,
                pos,
                col(a, 5),
                a.i[0],
            );
            true
        }
        op::FONT_WRAP => {
            let size = Vector2f {
                x: f(a, 2),
                y: f(a, 2),
            };
            let rows = gw::font_rows(a.p[0], f(a, 0), f(a, 1), size, wide(a, 0));
            a.i[0] = rows.len() as i32;
            ROWS.with(|r| *r.borrow_mut() = rows);
            true
        }
        op::FONT_ROW => {
            ROWS.with(|r| {
                let r = r.borrow();
                if let Some(row) = usize::try_from(a.i[0]).ok().and_then(|i| r.get(i)) {
                    a.rw.p = row.as_ptr();
                    a.rw.n = row.len();
                }
            });
            true
        }
        _ => false,
    }
}
