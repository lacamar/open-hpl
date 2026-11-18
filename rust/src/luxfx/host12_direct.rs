//! A12b host codes that only forwarded to the gui: done against the Rust-owned gfx elements and
//! the game HUD set. The dump lines are the ones the C++ host wrote.

use super::host12::{Args, Host, code, log};
use crate::gui::direct as d;
use crate::somascript::cfmt::fmt_g9;
use crate::types::Color;

const MAT_MODULATIVE: i32 = 4;

fn g9(v: f32) -> String {
    String::from_utf8_lossy(&fmt_g9(v)).into_owned()
}

fn text(a: &Args, slot: usize) -> Vec<u8> {
    match (a.s.get(slot), a.n.get(slot)) {
        (Some(&p), Some(&n)) if !p.is_null() && n != 0 => {
            // SAFETY: the caller passes `n` readable bytes in the slot.
            unsafe { std::slice::from_raw_parts(p, n) }.to_vec()
        }
        _ => Vec::new(),
    }
}

/// The result of `c` when it only touched the gui; `None` leaves it to the host.
pub(super) fn call(h: &dyn Host, c: i32, a: &Args) -> Option<usize> {
    Some(match c {
        code::G_IMAGE => {
            let f = text(a, 0);
            log(h, &format!("i gfx_image {}", String::from_utf8_lossy(&f)));
            d::gfx_image(&f, d::MAT_ALPHA)
        }
        code::G_RECT_WHITE => {
            log(h, "i gfx_rect white");
            d::gfx_rect(d::WHITE, MAT_MODULATIVE, true)
        }
        code::G_TEXTURE => {
            let f = text(a, 0);
            log(h, &format!("i gfx_texture {}", String::from_utf8_lossy(&f)));
            d::gfx_texture_file(&f, d::MAT_ALPHA, d::TEXTURE_RECT)
        }
        code::G_DESTROY => {
            log(h, "i gfx_destroy");
            d::gfx_destroy(a.p[0]);
            0
        }
        code::D_BUG => {
            let active = d::gfx_active_size(a.p[0]);
            let (pos, size) = ([a.f[0], a.f[1]], [active[0] * a.f[2], active[1] * a.f[2]]);
            if h.logging() {
                log(
                    h,
                    &format!(
                        "i bug {} pos {} {} size {} {} col {} angle {}",
                        a.i[0],
                        g9(pos[0]),
                        g9(pos[1]),
                        g9(size[0]),
                        g9(size[1]),
                        g9(a.f[3]),
                        g9(a.f[4])
                    ),
                );
            }
            let col = Color {
                r: 1.0,
                g: 1.0,
                b: 1.0,
                a: a.f[3],
            };
            d::draw_gfx(
                d::hud_set(),
                a.p[0],
                [pos[0], pos[1], 0.0],
                size,
                col,
                d::MAT_LAST,
                a.f[4],
            );
            0
        }
        _ => return None,
    })
}
