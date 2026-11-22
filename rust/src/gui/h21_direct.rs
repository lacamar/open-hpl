//! The popups' host: every command is a call on the Rust-owned set and widgets. `ctx` is the
//! popup's [`G2Popup`] allocation.

use super::g1_set::{self, Set};
use super::g1_skin::Skin;
use super::g2_popup::{
    G2Popup, ohpl_g2_popup_binding, ohpl_g2_popup_dispatch, ohpl_g2_popup_msgbox,
    ohpl_g2_popup_self_destruct, ohpl_g2_popup_str,
};
use super::gw::{self, read};
use super::gw_h20::{self, P};
use super::gw_items as gi;
use super::gw_kinds as gk;
use super::h20_ffi as f;
use super::h21_fb::{self as fb, FileBrowser, ohpl_g2fb_assign};
use super::h21_popup::{Cmd, Op, Ret};
use super::set::{Msg, W};
use crate::types::{Color, Vector2f, Vector3f};
use std::ffi::c_void;

const SKIN_FILE_PICKER_UP: i32 = 107;
const MSG_KEY_PRESS: i32 = 14;
const ALIGN_RIGHT: i32 = 1;

pub(super) extern "C" fn dispatch(obj: *mut c_void, w: W, d: *const Msg) -> bool {
    // SAFETY: `obj` is a popup binding and `d` a message, as the widget callbacks pass them.
    unsafe { ohpl_g2_popup_dispatch(obj, w, d) }
}

fn v3(c: &Cmd) -> Vector3f {
    Vector3f {
        x: c.f[0],
        y: c.f[1],
        z: c.f[2],
    }
}

fn v2(c: &Cmd) -> Vector2f {
    Vector2f {
        x: c.f[0],
        y: c.f[1],
    }
}

fn size(c: &Cmd) -> Vector2f {
    Vector2f {
        x: c.f[3],
        y: c.f[4],
    }
}

fn text<'a>(c: &Cmd, k: usize) -> &'a [u32] {
    match (c.s.get(k), c.n.get(k)) {
        // SAFETY: the popup passes `n` readable units that outlive the command.
        (Some(&p), Some(&n)) if !p.is_null() => unsafe { std::slice::from_raw_parts(p, n) },
        _ => &[],
    }
}

fn create(set: *mut Set, kind: i32, c: &Cmd, a: i32, b: i32, extra: i32, txt: &[u32]) -> W {
    let (pos, sz) = (v3(c), size(c));
    // SAFETY: `set` is a live set; every pointer is readable for the call.
    unsafe {
        gk::ohpl_gw_create(
            set.cast(),
            kind,
            &pos,
            &sz,
            txt.as_ptr(),
            txt.len(),
            c.w[0],
            std::ptr::null(),
            0,
            a,
            b,
            extra,
            std::ptr::null_mut(),
        )
    }
}

fn create_c(set: *mut Set, kind: i32, c: &Cmd, a: i32, b: i32, add: f32, txt: &[u32]) -> W {
    let (pos, sz) = (v3(c), size(c));
    // SAFETY: as in `create`.
    unsafe {
        gw_h20::ohpl_gw_create_c(
            set.cast(),
            kind,
            &pos,
            &sz,
            txt.as_ptr(),
            txt.len(),
            c.w[0],
            std::ptr::null(),
            0,
            a,
            b,
            add,
        )
    }
}

fn keep(p: *mut G2Popup, r: &mut Ret, idx: usize, s: &[u32]) {
    // SAFETY: `p` is the live popup of this command and `r` the answer being built.
    unsafe { ohpl_g2_popup_str(p, idx, s.as_ptr(), s.len(), r) };
}

fn msg_key(c: &Cmd) -> Msg {
    let mut m = Msg::zero();
    let mut k = [0u8; 12];
    for (dst, v) in k.chunks_exact_mut(4).zip(c.i) {
        dst.copy_from_slice(&v.to_ne_bytes());
    }
    m.set_key_press(&k);
    m
}

fn color(c: &Cmd) -> Color {
    Color {
        r: c.f[0],
        g: c.f[1],
        b: c.f[2],
        a: c.f[3],
    }
}

fn item_text(it: *mut gi::Item) -> Vec<u32> {
    let mut n = 0usize;
    let p = gi::ohpl_gwi_text(it, &mut n);
    if p.is_null() {
        Vec::new()
    } else {
        // SAFETY: the item text is readable for `n` units until its next change.
        unsafe { std::slice::from_raw_parts(p, n) }.to_vec()
    }
}

fn prop_text(pr: *mut gi::Prop) -> Vec<u32> {
    let mut n = 0usize;
    let p = gi::ohpl_gwi_prop_text(pr, &mut n);
    if p.is_null() {
        Vec::new()
    } else {
        // SAFETY: as above.
        unsafe { std::slice::from_raw_parts(p, n) }.to_vec()
    }
}

fn skin_gfx(skin: *mut Skin, idx: i32) -> usize {
    // SAFETY: `skin` is the popup's live skin.
    unsafe { super::g1_skin::ohpl_g1_skin_gfx(skin, idx) as usize }
}

fn skin_default_font(skin: *mut Skin) -> (usize, Vector2f) {
    // SAFETY: `skin` is live and the font entry is pinned for its life.
    unsafe { super::g1_skin::ohpl_g1_skin_font(skin, 0).as_ref() }
        .map_or((0, Vector2f::default()), |f| (f.font, f.size))
}

fn fb_ref<'a>(p: usize) -> Option<&'a FileBrowser> {
    // SAFETY: the popup owns the browser for its life.
    unsafe { (p as *const FileBrowser).as_ref() }
}

fn fb_mut<'a>(p: usize) -> Option<&'a mut FileBrowser> {
    // SAFETY: as above; the borrow ends within one command, before any popup callback.
    unsafe { (p as *mut FileBrowser).as_mut() }
}

fn cat<'a>(p: usize, i: i32) -> Option<&'a fb::Category> {
    fb_ref(p).and_then(|b| usize::try_from(i).ok().and_then(|i| b.cats.get(i)))
}

fn entry(v: Option<&Vec<u32>>) -> Vec<u32> {
    v.cloned().unwrap_or_default()
}

pub(super) extern "C" fn exec(ctx: *mut c_void, cmd: *const Cmd, ret: *mut Ret) {
    // SAFETY: the popup passes live pointers to its own commands and answers.
    let (p, c, r) = unsafe { (ctx.cast::<G2Popup>(), &*cmd, &mut *ret) };
    *r = Ret::default();
    // SAFETY: `ctx` is the live popup allocation.
    let (set, skin, fbp) = unsafe { ((*p).set as *mut Set, (*p).skin as *mut Skin, (*p).fb) };
    let (w, w2) = (c.w[0], c.w[1]);
    let on = c.i[0] != 0;
    let Some(op) = Op::from_i32(c.op) else {
        return;
    };
    let vt = gw_h20::vt();
    let ptr = gw_h20::ptr(w);
    let text_box = || match ptr {
        Some(P::Text(t)) => t,
        _ => std::ptr::null_mut(),
    };
    let combo = || match ptr {
        Some(P::Combo(t)) => t,
        _ => std::ptr::null_mut(),
    };
    let list = || match ptr {
        Some(P::List(t)) => t,
        _ => std::ptr::null_mut(),
    };
    // SAFETY: every `unsafe` call below passes a live set, widget or state pointer taken from
    // the command, as the C++ host did with its casts.
    unsafe {
        match op {
            Op::VirtualSize => {
                let s = g1_set::set_info(set).0;
                r.f[..2].copy_from_slice(&s);
            }
            Op::PopupZ => r.f[0] = g1_set::ohpl_g1_set_popup_z(set),
            Op::IncPopupZ => g1_set::ohpl_g1_set_inc_popup_z(set, 0),
            Op::PopupCount => g1_set::ohpl_g1_set_add_popup_count(set, c.i[0]),
            Op::GetAttention => r.w = g1_set::attention_widget(set),
            Op::GetFocused => r.w = g1_set::focused_widget(set),
            Op::GetDefaultFocus => r.w = g1_set::default_nav_widget(set),
            Op::SetAttention => g1_set::ohpl_g1_set_set_attention(set, w, true, false),
            Op::SetFocused => g1_set::ohpl_g1_set_set_focused(set, w, false),
            Op::SetDefaultFocus => g1_set::ohpl_g1_set_set_default_nav(set, w, false),
            Op::PushAttention => g1_set::ohpl_g1_set_push_attention(set),
            Op::PushFocused => g1_set::ohpl_g1_set_push_focused(set),
            Op::PushDefaultFocus => g1_set::ohpl_g1_set_push_default_nav(set),
            Op::PopAttention => g1_set::ohpl_g1_set_pop_attention(set, true),
            Op::PopFocused => g1_set::ohpl_g1_set_pop_focused(set),
            Op::PopDefaultFocus => g1_set::ohpl_g1_set_pop_default_nav(set),
            Op::DestroyWidget => g1_set::ohpl_g1_set_destroy_widget(set, w, on),
            Op::CreateWindow => {
                let mut cc = *c;
                cc.w[0] = 0;
                r.w = create(set, gk::KIND_WINDOW, &cc, c.i[0], 0, 0, &[]);
            }
            Op::CreateButton => r.w = create(set, gk::KIND_BUTTON, c, 0, 0, 0, text(c, 0)),
            Op::CreateLabel => r.w = create(set, gk::KIND_LABEL, c, 0, 0, 0, text(c, 0)),
            Op::CreateSlider => r.w = create(set, gk::KIND_SLIDER, c, c.i[0], 0, c.i[1], &[]),
            Op::CreateTextbox => {
                r.w = create_c(set, gw_h20::KIND_TEXTBOX, c, c.i[0], 1, c.f[5], text(c, 0));
            }
            Op::CreateFrame => r.w = create(set, gk::KIND_FRAME, c, 0, 0, i32::from(on), &[]),
            Op::CreateCombo => {
                r.w = create_c(set, gw_h20::KIND_COMBO, c, 0, 0, 0.0, text(c, 0));
            }
            Op::CreateList => r.w = create_c(set, gw_h20::KIND_MPLIST, c, 0, 0, 0.0, &[]),
            Op::MessageBox => {
                let (obj, cb) = if c.i[0] >= 0 {
                    (
                        ohpl_g2_popup_binding(p, c.i[0]),
                        dispatch as extern "C" fn(*mut c_void, W, *const Msg) -> bool as usize,
                    )
                } else {
                    (std::ptr::null_mut(), 0)
                };
                let sk = g1_set::ohpl_g1_set_skin(set) as usize;
                ohpl_g2_popup_msgbox(
                    set as usize,
                    sk,
                    c.s[0],
                    c.n[0],
                    c.s[1],
                    c.n[1],
                    c.s[2],
                    c.n[2],
                    c.s[3],
                    c.n[3],
                    obj as usize,
                    cb,
                );
            }
            Op::DestroyPopup => ohpl_g2_popup_self_destruct(p),
            Op::AddResourceDir => {
                let res = g1_set::ohpl_g1_set_resources(set);
                let t = text(c, 0);
                fb::add_resource_dir(res, t.as_ptr(), t.len());
            }
            Op::RunCallback | Op::CallDestroyCb => {
                if c.p[1] != 0 {
                    // SAFETY: `p[1]` is a `tGuiCallbackFunc` given by the popup's owner.
                    let f: extern "C" fn(*mut c_void, W, *const Msg) -> bool =
                        std::mem::transmute(c.p[1]);
                    let (target, m) = if op == Op::CallDestroyCb {
                        (0, Msg::zero())
                    } else if c.i[0] == 0 {
                        (w, Msg::zero())
                    } else {
                        (w, Msg::with_val(c.i[1]))
                    };
                    let res = f(c.p[0] as *mut c_void, target, &m);
                    if op == Op::RunCallback {
                        r.i = i32::from(res);
                    }
                }
            }
            Op::AddCallback => {
                let obj = ohpl_g2_popup_binding(p, c.i[1]);
                gw::add_callback(w, c.i[0], obj, dispatch);
            }
            Op::SetText => gw::set_text(w, text(c, 0)),
            Op::GetText => keep(p, r, 0, &gw::text(w)),
            Op::SetPosition => gw::set_position(w, v3(c)),
            Op::SetSize => gw::set_size(w, v2(c)),
            Op::SetGlobalPosition => gw::set_global_position(w, v3(c)),
            Op::GetGlobalPosition => {
                let g = gw::global_position(w);
                r.f[..3].copy_from_slice(&[g.x, g.y, g.z]);
            }
            Op::GetSize => {
                let s = gw::size(w);
                r.f[..2].copy_from_slice(&[s.x, s.y]);
            }
            Op::SetEnabled => gw::set_enabled(w, on),
            Op::SetVisible => gw::set_visible(w, on),
            Op::SetFocusNav => gw::ohpl_gw_set_focus_navigation(w, c.i[0], w2),
            Op::SetGlobalUiListener => gw::ohpl_gw_set_global_ui_listener(w, on),
            Op::WinCloseDisables => gk::ohpl_gw_window_set_close_disables(w, on),
            Op::WinEscCloses => gk::ohpl_gw_window_set_escape_closes(w, on),
            Op::WinGetEsc => r.i = i32::from(gk::ohpl_gw_window_escape_closes(w)),
            Op::WinStatic => gk::ohpl_gw_window_set_static(w, on),
            Op::WinDrawLabel => gk::ohpl_gw_window_set_draw_label(w, on),
            Op::WinFontSize => {
                let s = read(w, |g| g.p.font_size);
                r.f[..2].copy_from_slice(&[s.x, s.y]);
            }
            Op::InsideBounds => super::direct::position_inside_bounds(set, w),
            Op::SliderGet => r.i = gk::ohpl_gw_slider_value(w),
            Op::SliderSet => gk::ohpl_gw_slider_set_value(w, c.i[0], c.i[1] != 0),
            Op::TbGetNum => r.f[0] = f::ohpl_h20_tb_numeric_value(vt, text_box()),
            Op::TbSetNum => f::ohpl_h20_tb_set_numeric_value(vt, text_box(), c.f[0]),
            Op::TbLower => {
                if let Some(t) = text_box().as_mut() {
                    t.st.has_lower = on;
                    t.st.lower = c.f[0];
                }
            }
            Op::TbUpper => {
                if let Some(t) = text_box().as_mut() {
                    t.st.has_upper = on;
                    t.st.upper = c.f[0];
                }
            }
            Op::TbDecimals => {
                if let Some(t) = text_box().as_mut() {
                    t.st.decimals = c.i[0];
                }
            }
            Op::TbForceEnter => {
                if let Some(t) = text_box().as_mut() {
                    t.st.force_enter = on;
                }
            }
            Op::TbLostFocus => {
                if let Some(t) = text_box().as_mut() {
                    t.st.enter_on_lost_focus = on;
                }
            }
            Op::TbSelectAll => f::ohpl_h20_tb_set_selected_text(vt, text_box(), 0, -1),
            Op::TbUserData => r.w = gw::ohpl_gw_user_data(w) as usize,
            Op::TbKeySkip => {
                r.i = i32::from(gw::process_message(
                    w,
                    MSG_KEY_PRESS,
                    &msg_key(c),
                    true,
                    true,
                ));
            }
            Op::TbKey => {
                r.i = i32::from(gw::process_message(
                    w,
                    MSG_KEY_PRESS,
                    &msg_key(c),
                    false,
                    false,
                ));
            }
            Op::FrameColor => gk::ohpl_gw_frame_set_bg_color(w, &color(c)),
            Op::FrameDrawBg => gk::ohpl_gw_frame_set_draw_bg(w, on),
            Op::FrameBgZ => gk::ohpl_gw_frame_set_bg_z(w, c.f[0]),
            Op::CbCanEdit => f::ohpl_h20_cb_set_can_edit(vt, combo(), on),
            Op::CbItemNum => r.i = gi::num(w) as i32,
            Op::CbSelected => r.i = combo().as_ref().map_or(0, |x| x.combo.selected),
            Op::CbItemText => keep(p, r, 0, &item_text(gi::get(w, c.i[0]))),
            Op::CbAddItem => {
                gi::add(w, text(c, 0));
            }
            Op::CbClear | Op::MlClear => gi::clear(w),
            Op::CbSetSelected => f::ohpl_h20_cb_set_selected_item(vt, combo(), c.i[0], false, true),
            Op::MlAddColumn => {
                let align = if c.i[1] == 1 { ALIGN_RIGHT } else { 0 };
                let n = text(c, 0);
                f::ohpl_h20_lb_add_column(vt, list(), n.as_ptr(), n.len(), c.i[0], align);
            }
            Op::MlColWidth => f::ohpl_h20_lb_set_column_width(vt, list(), c.i[0], c.f[0]),
            Op::MlSelected => r.i = f::ohpl_h20_lb_selected_item(vt, list()),
            Op::MlProp1 => {
                let it = gi::get(w, c.i[0]);
                if it.is_null() {
                    r.i = 0;
                } else {
                    let pr = gi::ohpl_gwi_prop(it, 1);
                    if pr.is_null() {
                        r.i = 1;
                    } else {
                        r.i = 2;
                        keep(p, r, 0, &prop_text(pr));
                    }
                }
            }
            Op::TextLen => {
                let (font, sz) = skin_default_font(skin);
                r.f[0] = gw::font_length(font, sz, text(c, 0));
            }
            Op::FontSize => {
                let (_, sz) = skin_default_font(skin);
                r.f[..2].copy_from_slice(&[sz.x, sz.y]);
            }
            Op::ColorRead => {
                let q = c.p[0] as *const f32;
                for (i, v) in r.f.iter_mut().enumerate() {
                    *v = *q.add(i);
                }
            }
            Op::ColorWrite => {
                let q = c.p[0] as *mut f32;
                for (i, v) in c.f.iter().take(4).enumerate() {
                    *q.add(i) = *v;
                }
            }
            Op::TbKbOpen => gw_h20::ohpl_gw_tb_kb_open(w),
            Op::TbKbClose => gw_h20::ohpl_gw_tb_kb_close(w),
            Op::FpLoadGfx => {
                if let Some(b) = fb_mut(fbp) {
                    b.up_gfx = skin_gfx(skin, SKIN_FILE_PICKER_UP);
                    for (i, slot) in b.icons.iter_mut().enumerate() {
                        *slot = skin_gfx(skin, SKIN_FILE_PICKER_UP + 1 + i as i32);
                    }
                }
            }
            Op::BtnUpImage => {
                if let Some(b) = fb_ref(fbp).filter(|b| b.up_gfx != 0) {
                    gk::ohpl_gw_button_set_image(w, b.up_gfx as *mut _, false);
                }
            }
            Op::MlAddRow => {
                let icons = fb_ref(fbp).map(|b| b.icons);
                let it = gi::add(w, &[]);
                if c.i[0] == 0 {
                    gi::ohpl_gwi_add_prop_gfx(it, 0);
                    gi::ohpl_gwi_add_prop_text(it, c.s[0], c.n[0]);
                    gi::ohpl_gwi_add_prop_text(it, std::ptr::null(), 0);
                    gi::ohpl_gwi_add_prop_text(it, std::ptr::null(), 0);
                } else {
                    let g = icons
                        .and_then(|i| usize::try_from(c.i[1]).ok().and_then(|k| i.get(k).copied()))
                        .unwrap_or(0);
                    gi::ohpl_gwi_add_prop_gfx(it, g);
                    for (s, n) in c.s.iter().zip(c.n).take(3) {
                        gi::ohpl_gwi_add_prop_text(it, *s, n);
                    }
                }
            }
            Op::FbNavUp => {
                fb::navigate_up(fbp as *mut FileBrowser);
            }
            Op::FbNavTo => r.i = i32::from(fb::navigate_to(fbp as *mut FileBrowser, text(c, 0))),
            Op::FbFullPath => {
                if let Some(b) = fb_ref(fbp) {
                    keep(p, r, 0, &b.full_path());
                }
            }
            Op::FbPathCount => r.i = fb_ref(fbp).map_or(0, |b| b.path.len() as i32),
            Op::FbPathPop => {
                if let Some(b) = fb_mut(fbp) {
                    for _ in 0..c.i[0] {
                        if b.path.pop().is_none() {
                            break;
                        }
                    }
                }
            }
            Op::FbPathClear => {
                if let Some(b) = fb_mut(fbp) {
                    b.path.clear();
                }
            }
            Op::FbPathEntry => {
                let e = fb_ref(fbp)
                    .and_then(|b| usize::try_from(c.i[0]).ok().and_then(|i| b.path.get(i)));
                keep(p, r, 0, &entry(e));
            }
            Op::FbDirs => {
                r.i = i32::from(!fb::dir(1).is_empty()) | (i32::from(!fb::dir(0).is_empty()) << 1);
            }
            Op::FbCatCount => r.i = fb_ref(fbp).map_or(0, |b| b.cats.len() as i32),
            Op::FbCatName => keep(p, r, 0, &entry(cat(fbp, c.i[0]).map(|k| &k.name))),
            Op::FbCatFilterCount => r.i = cat(fbp, c.i[0]).map_or(0, |k| k.filters.len() as i32),
            Op::FbCatFilter => {
                let e = cat(fbp, c.i[0])
                    .and_then(|k| usize::try_from(c.i[1]).ok().and_then(|i| k.filters.get(i)));
                keep(p, r, 0, &entry(e));
            }
            Op::FbInit => fb::init(fbp as *mut FileBrowser),
            Op::FbRootCount => r.i = fb_ref(fbp).map_or(0, |b| b.roots.len() as i32),
            Op::FbRoot => {
                let e = fb_ref(fbp)
                    .and_then(|b| usize::try_from(c.i[0]).ok().and_then(|i| b.roots.get(i)));
                keep(p, r, 0, &entry(e));
            }
            Op::FbList => {
                if let Some(b) = fb_mut(fbp) {
                    r.i = b.list_current(c.i[0]) as i32;
                }
            }
            Op::FbRow => {
                if let Some(b) = fb_ref(fbp) {
                    let (ty, name, size, date) = b.row(usize::try_from(c.i[0]).unwrap_or(0));
                    r.i = ty;
                    keep(p, r, 0, &name);
                    keep(p, r, 1, &size);
                    keep(p, r, 2, &date);
                }
            }
            Op::FileExists => r.i = i32::from(fb::exists(false, text(c, 0))),
            Op::FolderExists => r.i = i32::from(fb::exists(true, text(c, 0))),
            Op::SaveDest => {
                if let Some(b) = fb_ref(fbp).filter(|b| b.save_dest != 0) {
                    ohpl_g2fb_assign(b.save_dest as *mut c_void, 0, c.s[0], c.n[0]);
                }
            }
            Op::LoadDest => {
                if let Some(b) = fb_ref(fbp).filter(|b| b.load_dest != 0) {
                    ohpl_g2fb_assign(b.load_dest as *mut c_void, 1, c.s[0], c.n[0]);
                }
            }
        }
    }
}
