//! The host table of the H20 widgets, answered natively: every op is a call on the `Gw` widgets,
//! the set, the skin or the font services of `gw_host`.

use std::ffi::c_void;

use super::g1_set;
use super::g1_skin;
use super::gw::{self, skin_of};
use super::gw_h20::{self, KIND_TAB, KIND_TABLABEL, KIND_TEXTBOX, P};
use super::gw_host::host;
use super::gw_items as gi;
use super::gw_kinds as gk;
use super::gw_menu;
use super::h20_ffi as f;
use super::h20_host::{self as hh, Args, HostVt};
use super::set::{Msg, W};
use crate::types::{Color, Vector2f, Vector3f};

fn v3(a: &Args) -> Vector3f {
    Vector3f {
        x: a.f[0],
        y: a.f[1],
        z: a.f[2],
    }
}

fn v2(a: &Args) -> Vector2f {
    Vector2f {
        x: a.f[0],
        y: a.f[1],
    }
}

fn text<'a>(a: &Args) -> &'a [u32] {
    if a.text.is_null() {
        &[]
    } else {
        // SAFETY: the caller passes a readable range.
        unsafe { std::slice::from_raw_parts(a.text, a.text_len) }
    }
}

fn out3(a: &mut Args, v: Vector3f) {
    a.rf = [v.x, v.y, v.z];
}

fn out2(a: &mut Args, v: Vector2f) {
    a.rf[0] = v.x;
    a.rf[1] = v.y;
}

fn pubf<T: Default>(w: W, f: impl FnOnce(&gw::Pubf) -> T) -> T {
    gw::read(w, |g| f(&g.p))
}

fn skin_font_size(w: W) -> Vector2f {
    // SAFETY: the skin font is live while the skin is.
    gw::read(w, |g| {
        unsafe { g.font.as_ref() }.map_or(Vector2f::default(), |f| f.size)
    })
}

fn disabled_color(w: W) -> Color {
    let f = gw::skin_font(skin_of(w), "Disabled");
    // SAFETY: as above.
    unsafe { f.as_ref() }.map_or(
        Color {
            r: 1.0,
            g: 1.0,
            b: 1.0,
            a: 1.0,
        },
        |f| f.color,
    )
}

fn mul(c: Color, m: Color) -> Color {
    Color {
        r: c.r * m.r,
        g: c.g * m.g,
        b: c.b * m.b,
        a: c.a * m.a,
    }
}

/// `DrawDefaultText` with an explicit colour (`None`: the font colour).
fn draw_text(w: W, text: &[u32], pos: Vector3f, align: i32, col: Option<Color>) {
    let (font, size, fc, cm) = gw::read(w, |g| {
        (g.font_type, g.p.font_size, g.p.font_color, g.p.color_mul)
    });
    if font == 0 {
        return;
    }
    gw::draw_font(
        gw::set_of(w),
        font,
        text,
        size,
        pos,
        mul(col.unwrap_or(fc), cm),
        align,
    );
}

fn set_call(a: &mut Args) -> usize {
    let s = gw::set_of(a.w);
    let wp = a.p[0];
    // SAFETY: the widget's set is live; the `core_op` helpers take it.
    unsafe {
        match a.i[0] {
            hh::SC_PUSH_ATTENTION => g1_set::ohpl_g1_set_push_attention(s),
            hh::SC_PUSH_FOCUSED => g1_set::ohpl_g1_set_push_focused(s),
            hh::SC_PUSH_DEFAULT_NAV => g1_set::ohpl_g1_set_push_default_nav(s),
            hh::SC_SET_ATTENTION => g1_set::ohpl_g1_set_set_attention(s, wp, true, false),
            hh::SC_SET_FOCUSED => g1_set::ohpl_g1_set_set_focused(s, wp, false),
            hh::SC_POP_ATTENTION => g1_set::ohpl_g1_set_pop_attention(s, true),
            hh::SC_POP_FOCUSED => g1_set::ohpl_g1_set_pop_focused(s),
            hh::SC_POP_DEFAULT_NAV => g1_set::ohpl_g1_set_pop_default_nav(s),
            hh::SC_POPUP_Z => a.rf[0] = g1_set::ohpl_g1_set_popup_z(s),
            hh::SC_VIRTUAL_SIZE => {
                let r = g1_set::virtual_rect(s);
                a.rf[0] = r[0];
                a.rf[1] = r[1];
            }
            hh::SC_DEC_CONTEXT_Z => g1_set::ohpl_g1_set_add_context_menu_z(s, -5.0),
            hh::SC_GET_ATTENTION => return g1_set::attention_widget(s),
            hh::SC_GET_FOCUSED => return g1_set::focused_widget(s),
            hh::SC_BASE_CLIP => return g1_set::ohpl_g1_set_base_clip(s) as usize,
            hh::SC_CURRENT_CLIP => return g1_set::ohpl_g1_set_current_clip(s) as usize,
            hh::SC_SET_ATTENTION_NO_CLEAR => g1_set::ohpl_g1_set_set_attention(s, wp, false, false),
            hh::SC_SHOW_CONTEXT_MENU => {
                gw_menu::show_context_menu(s, wp, v3(a));
            }
            hh::SC_GET_DRAW_MOUSE => return usize::from(g1_set::draw_mouse(s)),
            _ => {}
        }
    }
    0
}

#[allow(clippy::too_many_arguments)]
fn create(
    a: &mut Args,
    kind: i32,
    pos: Vector3f,
    size: Vector2f,
    txt: &[u32],
    parent: W,
    b: i32,
    num: f32,
    tb: i32,
) -> usize {
    let s = gw::set_of(a.w);
    // SAFETY: the set is live, the slices readable.
    unsafe {
        gw_h20::ohpl_gw_create_c(
            s.cast(),
            kind,
            &pos,
            &size,
            txt.as_ptr(),
            txt.len(),
            parent,
            std::ptr::null(),
            0,
            tb,
            b,
            num,
        )
    }
}

extern "C" fn op(_ctx: *mut c_void, code: i32, a: *mut Args) -> usize {
    // SAFETY: the logic passes a live argument block.
    let Some(a) = (unsafe { a.as_mut() }) else {
        return 0;
    };
    let w = a.w;
    let set = gw::set_of(w);
    let vt = gw_h20::vt();
    match code {
        hh::SLIDER_SET_VALUE => {
            gk::ohpl_gw_slider_set_value(w, a.i[0], if a.i[1] != 0 { a.i[2] != 0 } else { true });
            0
        }
        hh::SLIDER_GET_VALUE => {
            a.ri[0] = gk::ohpl_gw_slider_value(w);
            0
        }
        hh::SLIDER_SET_BAR_VALUE_SIZE => {
            gk::ohpl_gw_slider_set_bar_size(w, a.i[0]);
            0
        }
        hh::SLIDER_SET_MAX_VALUE => {
            gk::ohpl_gw_slider_set_max(w, a.i[0]);
            0
        }
        hh::SLIDER_SET_BUTTON_VALUE_ADD => {
            gk::ohpl_gw_slider_set_button_add(w, a.i[0]);
            0
        }
        hh::SLIDER_SET_BAR_CLICK_VALUE_ADD => {
            gk::ohpl_gw_slider_set_click_add(w, a.i[0]);
            0
        }
        hh::SLIDER_GET_BUTTON_VALUE_ADD => {
            a.ri[0] = gk::ohpl_gw_slider_button_add(w);
            0
        }
        hh::WIDGET_SET_SIZE => {
            gw::set_size(w, v2(a));
            0
        }
        hh::WIDGET_SET_POSITION => {
            gw::set_position(w, v3(a));
            0
        }
        hh::WIDGET_GET_SIZE => {
            out2(a, gw::size(w));
            0
        }
        hh::ITEM_NUM => {
            a.ri[0] = gi::num(a.p[0]) as i32;
            0
        }
        hh::ITEM_GET => gi::get(a.p[0], a.i[0]) as usize,
        hh::ITEM_IS_SELECTED => usize::from(gi::ohpl_gwi_selected(a.p[0] as *mut _)),
        hh::ITEM_SET_SELECTED => {
            gi::set_selected(a.p[0] as *mut _, a.i[0] != 0);
            0
        }
        hh::ITEM_IS_SELECTABLE => usize::from(gi::ohpl_gwi_selectable(a.p[0] as *mut _)),
        hh::ITEM_INDEX => {
            a.ri[0] = gi::ohpl_gwi_index(a.p[0] as *mut _);
            0
        }
        hh::ITEM_TEXT => {
            let mut n = 0;
            let p = gi::ohpl_gwi_text(a.p[0] as *mut _, &mut n);
            a.ri[0] = n as i32;
            p as usize
        }
        hh::PROCESS_MESSAGE => {
            let r = if a.p[0] != 0 {
                // SAFETY: the logic passes a live message.
                let d = unsafe { *(a.p[0] as *const Msg) };
                gw::process_message(w, a.i[0], &d, a.i[2] != 0, a.i[3] != 0)
            } else {
                gw::process_message(w, a.i[0], &Msg::with_val(a.i[1]), false, false)
            };
            usize::from(r)
        }
        hh::WORLD_TO_LOCAL => {
            let l = gw::world_to_local(
                w,
                Vector3f {
                    x: a.f[0],
                    y: a.f[1],
                    z: 0.0,
                },
            );
            out3(a, l);
            0
        }
        hh::GET_GLOBAL => {
            out3(a, gw::global_position(w));
            0
        }
        hh::GET_FONT_SIZE => {
            out2(a, pubf(w, |p| p.font_size));
            0
        }
        hh::HAS_FOCUS => usize::from(gw::has_focus(w)),
        hh::CREATE_SLIDER => {
            gk::create_slider(set, 1, Vector3f::default(), Vector2f::default(), 0, w, b"") as usize
        }
        hh::ADD_CALLBACK => {
            // SAFETY: the logic passes a callback of the widget callback type.
            let cb: super::h19_widget::CallbackFn = unsafe { std::mem::transmute(a.p[1]) };
            gw::add_callback(w, a.i[0], a.p[0] as *mut c_void, cb);
            0
        }
        hh::DRAW_GFX => {
            let g = a.p[0] as *mut super::g1_gfx::Element;
            let (pos, size) = (
                v3(a),
                Vector2f {
                    x: a.f[3],
                    y: a.f[4],
                },
            );
            if a.i[1] != 0 {
                g1_set::draw_gfx_default(
                    set,
                    g,
                    pos,
                    Vector2f { x: -1.0, y: -1.0 },
                    Color {
                        r: 1.0,
                        g: 1.0,
                        b: 1.0,
                        a: 1.0,
                    },
                );
            } else if a.i[0] != 0 {
                g1_set::draw_gfx_default(
                    set,
                    g,
                    pos,
                    size,
                    Color {
                        r: a.f[5],
                        g: a.f[6],
                        b: a.f[7],
                        a: a.f[8],
                    },
                );
            } else {
                g1_set::draw_gfx_default(
                    set,
                    g,
                    pos,
                    size,
                    Color {
                        r: 1.0,
                        g: 1.0,
                        b: 1.0,
                        a: 1.0,
                    },
                );
            }
            0
        }
        hh::DRAW_BORDERS => {
            // SAFETY: the logic passes arrays of four elements.
            let (b, c) = unsafe {
                (
                    *(a.p[1] as *const [*mut _; 4]),
                    *(a.p[2] as *const [*mut _; 4]),
                )
            };
            gw::draw_borders(
                w,
                a.p[0] as *mut _,
                &b,
                &c,
                v3(a),
                Vector2f {
                    x: a.f[3],
                    y: a.f[4],
                },
                a.i[1] != 0,
            );
            0
        }
        hh::SKIN_GFX => {
            // SAFETY: the widget's skin is live.
            unsafe { g1_skin::ohpl_g1_skin_gfx(skin_of(w), a.i[0]) as usize }
        }
        hh::SKIN_ATTRIBUTE => {
            // SAFETY: as above.
            let v = unsafe { g1_skin::ohpl_g1_skin_attribute(skin_of(w), a.i[0]).as_ref() }
                .copied()
                .unwrap_or_default();
            out3(a, v);
            0
        }
        hh::GFX_ACTIVE_SIZE => {
            out2(a, gw::metrics(a.p[0] as *mut _).active);
            0
        }
        hh::GFX_IMAGE_SIZE => {
            out2(a, gw::metrics(a.p[0] as *mut _).image);
            0
        }
        hh::CLIP_CREATE_CHILD => {
            // SAFETY: the logic passes a live clip region.
            unsafe {
                g1_set::ohpl_g1_clip_create_child(
                    a.p[0] as *mut _,
                    &v3(a),
                    &Vector2f {
                        x: a.f[3],
                        y: a.f[4],
                    },
                ) as usize
            }
        }
        hh::SET_CLIP => {
            // SAFETY: live set and clip.
            unsafe { g1_set::ohpl_g1_set_set_current_clip(set, a.p[0] as *mut _) };
            0
        }
        hh::DRAW_TEXT => {
            let t = text(a);
            let pos = v3(a);
            let (enabled, hl) = (gw::is_enabled(w), pubf(w, |p| p.font_highlight));
            match a.i[2] {
                1 => draw_text(
                    w,
                    t,
                    pos,
                    a.i[0],
                    Some(if enabled {
                        pubf(w, |p| p.font_color)
                    } else {
                        disabled_color(w)
                    }),
                ),
                2 => {
                    let c = match a.i[3] {
                        0 => disabled_color(w),
                        1 => hl,
                        _ => pubf(w, |p| p.font_color),
                    };
                    draw_text(w, t, pos, a.i[0], Some(c));
                }
                _ => draw_text(w, t, pos, a.i[0], if a.i[1] != 0 { Some(hl) } else { None }),
            }
            0
        }
        hh::GFX_RECT => super::g1_gui::ohpl_g1_gfx_rect() as usize,
        hh::CREATE_LABEL => {
            let l = gk::create_label(set, v3(a), Vector2f { x: -1.0, y: -1.0 }, text(a), w, b"");
            gw::set_default_font_size(l, pubf(w, |p| p.font_size));
            gk::label_set_bg_color(
                l,
                Color {
                    r: 0.82,
                    g: 0.81,
                    b: 0.79,
                    a: 1.0,
                },
            );
            l
        }
        hh::TEXT_LENGTH => {
            let (font, size) = gw::read(w, |g| (g.font_type, g.p.font_size));
            a.rf[0] = if font == 0 {
                0.0
            } else {
                gw::font_length(font, size, &gw::text(a.p[0]))
            };
            0
        }
        hh::ITEM_PROP_GET => gi::ohpl_gwi_prop(a.p[0] as *mut _, a.i[0]) as usize,
        hh::PROP_TYPE => gi::ohpl_gwi_prop_type(a.p[0] as *mut _) as usize,
        hh::PROP_TEXT => {
            let mut n = 0;
            let p = gi::ohpl_gwi_prop_text(a.p[0] as *mut _, &mut n);
            a.ri[0] = n as i32;
            p as usize
        }
        hh::PROP_GFX => gi::ohpl_gwi_prop_gfx(a.p[0] as *mut _),
        hh::DESTROY_WIDGET => {
            g1_set::destroy_widget(set, a.p[0], false);
            0
        }
        hh::WIDGET_SET_VISIBLE => {
            gw::set_visible(w, a.i[0] != 0);
            0
        }
        hh::WIDGET_SET_ENABLED => {
            gw::set_enabled(w, a.i[0] != 0);
            0
        }
        hh::WIDGET_IS_ENABLED => usize::from(gw::is_enabled(w)),
        hh::WIDGET_IS_VISIBLE => usize::from(gw::is_visible(w)),
        hh::WIDGET_GET_TEXT => {
            let (p, n) = gw::read(w, |g| (g.text.as_ptr() as usize, g.text.len()));
            a.ri[0] = n as i32;
            p
        }
        hh::WIDGET_SET_TEXT => {
            gw::set_text(w, text(a));
            0
        }
        hh::WIDGET_SET_FONT_SIZE => {
            gw::set_default_font_size(w, v2(a));
            0
        }
        hh::WIDGET_GET_POSITION => {
            out3(a, gw::read(w, |g| g.base.pos));
            0
        }
        hh::TEXTBOX_SET_CAN_EDIT => {
            if let Some(P::Text(t)) = gw_h20::ptr(w) {
                // SAFETY: the state is live.
                unsafe { f::ohpl_h20_tb_set_can_edit(vt, t, a.i[0] != 0) };
            }
            0
        }
        hh::TEXTBOX_GET_CAN_EDIT => match gw_h20::ptr(w) {
            // SAFETY: the state is live.
            Some(P::Text(t)) => usize::from(unsafe { (*t).st.can_edit }),
            _ => 0,
        },
        hh::TEXTBOX_BG_SIZE => {
            if let Some(P::Text(t)) = gw_h20::ptr(w) {
                let mut s = Vector2f::default();
                // SAFETY: the state is live.
                unsafe { f::ohpl_h20_tb_background_size(vt, t, &mut s) };
                out2(a, s);
            }
            0
        }
        hh::CREATE_TEXTBOX => create(
            a,
            KIND_TEXTBOX,
            Vector3f::default(),
            v2(a),
            &[],
            w,
            1,
            1.0,
            0,
        ),
        hh::CREATE_BUTTON => gk::create_button(
            set,
            Vector3f::default(),
            Vector2f::default(),
            &[],
            w,
            false,
            b"",
        ) as usize,
        hh::BUTTON_SET_IMAGE => {
            gk::ohpl_gw_button_set_image(w, a.p[0] as *mut _, false);
            0
        }
        hh::CHECK_POINT_CLIP => {
            let par = gw::parent(w);
            usize::from(
                par == 0 || !gw::clips_graphics(par) || gw::point_is_inside(par, v2(a), true),
            )
        }
        hh::SET_CALL => set_call(a),
        hh::STATE_PTR => gw_h20::ohpl_gw_state(w) as usize,
        hh::CREATE_TAB => create(
            a,
            KIND_TAB,
            v3(a),
            Vector2f {
                x: a.f[3],
                y: a.f[4],
            },
            &[],
            w,
            0,
            0.0,
            0,
        ),
        hh::CREATE_TAB_LABEL => {
            let t = gw::text(w);
            create(
                a,
                KIND_TABLABEL,
                Vector3f::default(),
                v2(a),
                &t,
                0,
                0,
                0.0,
                0,
            )
        }
        hh::CREATE_FRAME => gk::create_frame(
            set,
            Vector3f::default(),
            v2(a),
            false,
            0,
            a.i[0] != 0,
            a.i[1] != 0,
            b"",
        ) as usize,
        hh::WIDGET_SET_CLIP_ACTIVE => {
            gw::with(w, |g| g.base.clips = a.i[0] != 0);
            0
        }
        hh::ATTACH_CHILD => {
            if a.i[0] != 0 {
                gw::attach_child(w, a.p[0])
            } else {
                gw::attach_child_base(w, a.p[0])
            }
            0
        }
        hh::REMOVE_CHILD => {
            if a.i[0] != 0 {
                gw::remove_child(w, a.p[0])
            } else {
                gw::remove_child_base(w, a.p[0])
            }
            0
        }
        hh::SKIN_FONT_SIZE => {
            out2(a, skin_font_size(w));
            0
        }
        hh::TEXT_LENGTH_SKIN => {
            let (font, t) = gw::read(w, |g| (g.font_type, g.text.clone()));
            a.rf[0] = if font == 0 {
                0.0
            } else {
                gw::font_length(font, skin_font_size(w), &t)
            };
            0
        }
        hh::CREATE_BUTTON_AT => gk::create_button(
            set,
            v3(a),
            Vector2f {
                x: a.f[3],
                y: a.f[4],
            },
            &[],
            w,
            false,
            b"",
        ) as usize,
        hh::BUTTON_SETUP => {
            gw::set_default_font_size(
                w,
                Vector2f {
                    x: a.f[0],
                    y: a.f[0],
                },
            );
            gk::ohpl_gw_button_set_repeat_active(w, true);
            gk::ohpl_gw_button_set_repeat_freq(w, a.f[1]);
            0
        }
        hh::FONT_RANGE => {
            let font = gw::read(w, |g| g.font_type);
            let (mut lo, mut hi) = (0, 0);
            if font != 0
                && let Some(h) = host()
            {
                // SAFETY: a live font.
                unsafe { (h.font_range)(font, &mut lo, &mut hi) };
            }
            a.ri = [lo, hi];
            0
        }
        hh::GLYPH => {
            let font = gw::read(w, |g| g.font_type);
            let mut adv = 0.0;
            // SAFETY: a live font.
            let ok = font != 0
                && host().is_some_and(|h| unsafe { (h.glyph_advance)(font, a.i[0], &mut adv) });
            if ok {
                a.rf[0] = adv;
            }
            usize::from(ok)
        }
        hh::FONT_LENGTH => {
            let (font, size) = gw::read(w, |g| (g.font_type, g.p.font_size));
            a.rf[0] = if font == 0 {
                0.0
            } else {
                gw::font_length(font, size, text(a))
            };
            0
        }
        hh::CLIPBOARD => {
            let mut out = Vec::new();
            if let Some(h) = host() {
                // SAFETY: the sink appends to `out` during the call.
                unsafe { (h.clipboard_load)((&raw mut out).cast(), push_wide) };
            }
            CLIP.with(|c| *c.borrow_mut() = out);
            CLIP.with(|c| {
                let c = c.borrow();
                a.ri[0] = c.len() as i32;
                c.as_ptr() as usize
            })
        }
        hh::COPY => {
            if let Some(h) = host() {
                let t = text(a);
                // SAFETY: the host copies the text.
                unsafe { (h.clipboard_save)(t.as_ptr(), t.len()) };
            }
            0
        }
        hh::TO_FLOAT => {
            let t = text(a);
            a.rf[0] = host().map_or(0.0, |h| {
                // SAFETY: the host reads the text.
                unsafe { (h.to_float)(t.as_ptr(), t.len(), a.i[0]) }
            });
            0
        }
        hh::FLOAT_TEXT => {
            let mut out = Vec::new();
            if let Some(h) = host() {
                // SAFETY: the sink appends to `out` during the call.
                unsafe { (h.float_text)(a.f[0], a.i[0], (&raw mut out).cast(), push_wide) };
            }
            NUM.with(|c| *c.borrow_mut() = out);
            NUM.with(|c| {
                let c = c.borrow();
                a.ri[0] = c.len() as i32;
                c.as_ptr() as usize
            })
        }
        hh::SET_POINTER_GFX => {
            let g = gw::skin_gfx(
                skin_of(w),
                if a.i[0] != 0 {
                    "PointerText"
                } else {
                    "PointerNormal"
                },
            );
            gw::with(w, |x| x.pointer_gfx = g);
            0
        }
        hh::GFX_ANIM_RESET => {
            // SAFETY: a live element.
            unsafe { super::g1_gfx::ohpl_g1_gfx_set_animation_time(a.p[0] as *mut _, 0.0) };
            0
        }
        hh::CREATE_POPUP_KEYBOARD => {
            // SAFETY: the set and its skin are live.
            unsafe {
                super::g2_popup::ohpl_g2_popup_kb(
                    set as usize,
                    g1_set::ohpl_g1_set_skin(set) as usize,
                    w,
                )
            };
            0
        }
        hh::MENU_CALL => gw_menu::menu_call(a),
        hh::WIDGET_TYPE => {
            if w == 0 {
                usize::MAX
            } else {
                gw::read(w, |g| g.ty) as usize
            }
        }
        hh::WIDGET_SET_CHILDREN_OFFSET => {
            gw::set_children_offset(w, v3(a));
            0
        }
        hh::IS_CONNECTED => usize::from(gw::is_connected_to(w, a.p[0], a.i[0] == 0)),
        hh::ITEM_TEXT_LENGTH | hh::ITEM_IS_SEPARATOR | hh::WIDGET_SHORTCUT_TEXT => {
            gw_menu::item_op(code, a)
        }
        _ => 0,
    }
}

thread_local! {
    static CLIP: std::cell::RefCell<Vec<u32>> = const { std::cell::RefCell::new(Vec::new()) };
    static NUM: std::cell::RefCell<Vec<u32>> = const { std::cell::RefCell::new(Vec::new()) };
}

unsafe extern "C" fn push_wide(ctx: *mut c_void, p: *const u32, n: usize) {
    // SAFETY: `ctx` is the `Vec` of the call, `p` readable for `n` characters.
    unsafe { (*ctx.cast::<Vec<u32>>()).extend_from_slice(std::slice::from_raw_parts(p, n)) };
}

pub(crate) static VT: HostVt = HostVt {
    ctx: std::ptr::null_mut(),
    op,
};

// SAFETY: the table holds a null context and a plain function.
unsafe impl Sync for HostVt {}
