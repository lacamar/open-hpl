//! S8 ops that only forwarded to the gui set, gfx elements and clip regions: done here against
//! the Rust-owned objects, bypassing the C++ host (the trace still records them).

use super::s8::op;
use crate::gui::g1_gfx::{self, Element};
use crate::gui::g1_gui;
use crate::gui::g1_set::{self, Clip, Set};
use crate::luxmenu2::host::{Arg, Res};
use crate::math::{Vector2f, Vector3f};
use crate::types::Color;

fn p(a: &[Arg], i: usize) -> usize {
    match a.get(i) {
        Some(Arg::P(v)) => *v,
        Some(Arg::I(v)) => *v as usize,
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

fn res(v: u64) -> Res {
    let mut r = Res::default();
    r.u[0] = v;
    r
}

/// The result of `op` when it is one of the forwarding ops; `None` leaves it to the host.
pub(super) fn call(op: i32, a: &[Arg]) -> Option<Res> {
    // Tests replay against fake pointers.
    if cfg!(test) {
        return None;
    }
    let set = p(a, 0) as *mut Set;
    let gui = g1_gui::current();
    Some(match op {
        op::SET_DRAW_MOUSE => {
            g1_set::set_draw_mouse(set, b(a, 1));
            Res::default()
        }
        op::SET_ACTIVE => {
            g1_set::set_active(set, b(a, 1));
            Res::default()
        }
        op::GUI_FOCUS => {
            g1_gui::focus_set(gui, set as usize);
            Res::default()
        }
        op::GUI_FOCUSED_IS => res(u64::from(g1_gui::focused_set(gui) == set as usize)),
        op::GFX_SIZE => {
            let mut v = Vector2f::default();
            // SAFETY: the script passes a live element.
            unsafe { g1_gfx::ohpl_g1_gfx_image_size(p(a, 0) as *mut Element, &mut v) };
            let mut r = Res::default();
            r.u[0] = u64::from(v.x.to_bits());
            r.u[1] = u64::from(v.y.to_bits());
            r
        }
        op::GFX_DESTROY => {
            g1_gui::destroy_gfx(gui, p(a, 0) as *mut Element);
            Res::default()
        }
        op::DRAW_GFX => {
            g1_set::draw_gfx_default(
                set,
                p(a, 1) as *mut Element,
                Vector3f {
                    x: f(a, 2),
                    y: f(a, 3),
                    z: f(a, 4),
                },
                Vector2f {
                    x: f(a, 5),
                    y: f(a, 6),
                },
                Color {
                    r: f(a, 7),
                    g: f(a, 8),
                    b: f(a, 9),
                    a: f(a, 10),
                },
            );
            Res::default()
        }
        op::CLIP_NEW => res(g1_set::ohpl_g1_clip_new() as u64),
        op::CLIP_RECT => {
            // SAFETY: the script passes a live clip region.
            unsafe {
                g1_set::ohpl_g1_clip_set_rect(
                    p(a, 0) as *mut Clip,
                    f(a, 1),
                    f(a, 2),
                    f(a, 3),
                    f(a, 4),
                );
            }
            Res::default()
        }
        op::CLIP_GET => res(g1_set::current_clip(set) as u64),
        op::CLIP_SET => {
            g1_set::set_current_clip(set, p(a, 1) as *mut Clip);
            Res::default()
        }
        op::CLIP_DELETE => {
            // SAFETY: from `CLIP_NEW`, freed once.
            unsafe { g1_set::ohpl_g1_clip_free(p(a, 0) as *mut Clip) };
            Res::default()
        }
        op::DEFAULT_FOCUS_NAV => {
            g1_set::set_default_nav(set, p(a, 1));
            Res::default()
        }
        op::FOCUSED_WIDGET => {
            g1_set::set_focused(set, p(a, 1));
            Res::default()
        }
        _ => return None,
    })
}
