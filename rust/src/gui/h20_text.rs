//! `cWidgetTextBox`: editing state, numeric buttons, drawing and input. State is [`TextBox`],
//! embedded in the C++ widget (`gui/RustGuiH20.h`); the editing logic is [`Edit`] over a
//! [`TextHost`] that reaches the font, clipboard and number formatting through [`Hx`]. Effects on
//! the widget text are collected and replayed after the call, as the C++ glue did.

use super::h19_widget::Base;
use super::h20_host::{self as hh, HostVt, Hx};
use super::h20_list::gfx_id;
use super::set::{Msg, W};
use super::widgets_text::{self as wt, Edit, Event, Ret, TextEnv, TextHost, TextState};
use crate::types::Vector2f;
use std::ffi::c_void;
use std::sync::atomic::{AtomicI32, Ordering};

const MSG_BUTTON_PRESSED: i32 = 11;
const MSG_TEXT_BOX_ENTER: i32 = 28;
const MSG_TEXT_BOX_VALUE_UP: i32 = 29;
const MSG_TEXT_BOX_VALUE_DOWN: i32 = 30;
const ALIGN_LEFT: i32 = 0;
const G_MARKER: usize = 0;
const G_SELECTED: usize = 1;
const G_BG: usize = 2;
const G_BORDERS: usize = 3;
const G_CORNERS: usize = 7;
const COLOR_FONT: i32 = 1;

static DEFAULT_DECIMALS: AtomicI32 = AtomicI32::new(3);

pub fn set_default_decimals(v: i32) {
    DEFAULT_DECIMALS.store(v, Ordering::Relaxed);
}

#[derive(Default)]
pub struct Replay {
    active: bool,
    nested: Option<Vec<u32>>,
}

#[repr(C)]
pub struct TextBox {
    pub st: TextState,
    pub buttons: [W; 2],
    pub illegal: *mut Vec<u32>,
    pub replay: *mut Replay,
    pub prev_attention: W,
    pub gfx: [usize; 11],
    pub numeric: bool,
    pub show_buttons: bool,
    pub replaying: bool,
    pub this: W,
    pub base: *mut Base,
    pub host: *const HostVt,
}

const _: () = assert!(std::mem::size_of::<TextBox>() == 240);

type P = *mut TextBox;

/// # Safety
/// `t` is live.
unsafe fn this(t: P) -> W {
    // SAFETY: caller contract.
    unsafe { (*t).this }
}

/// # Safety
/// `t` is live.
unsafe fn size(t: P) -> Vector2f {
    // SAFETY: caller contract.
    unsafe { (*t).base.as_ref() }.map_or(Vector2f { x: 0.0, y: 0.0 }, |b| b.size)
}

fn slice<'a>(p: *const u32, n: usize) -> &'a [u32] {
    if p.is_null() || n == 0 {
        &[]
    } else {
        // SAFETY: host-provided buffer valid until the next host call; copied by the callers.
        unsafe { std::slice::from_raw_parts(p, n) }
    }
}

struct Ops {
    hx: Hx,
    w: W,
    mouse: [f32; 2],
    events: Vec<Event>,
}

impl Ops {
    fn call(&self, code: i32, f: impl FnOnce(&mut hh::Args)) -> (hh::Args, usize) {
        let mut a = hh::Args::new(self.w);
        f(&mut a);
        let v = self.hx.raw(code, &mut a);
        (a, v)
    }
}

impl TextHost for Ops {
    fn glyph(&mut self, num: i32) -> Option<f32> {
        let (a, v) = self.call(hh::GLYPH, |a| a.i[0] = num);
        (v != 0).then_some(a.rf[0])
    }

    fn length(&mut self, text: &[u32]) -> f32 {
        let (a, _) = self.call(hh::FONT_LENGTH, |a| {
            a.text = text.as_ptr();
            a.text_len = text.len();
        });
        a.rf[0]
    }

    fn local_x(&mut self) -> f32 {
        self.hx.world_to_local(self.w, self.mouse).x
    }

    fn clipboard(&mut self) -> Vec<u32> {
        let (a, v) = self.call(hh::CLIPBOARD, |_| ());
        slice(v as *const u32, usize::try_from(a.ri[0]).unwrap_or(0)).to_vec()
    }

    fn copy(&mut self, text: &[u32]) {
        self.call(hh::COPY, |a| {
            a.text = text.as_ptr();
            a.text_len = text.len();
        });
    }

    fn to_float(&mut self, text: &[u32], decimals: i32) -> f32 {
        let (a, _) = self.call(hh::TO_FLOAT, |a| {
            a.text = text.as_ptr();
            a.text_len = text.len();
            a.i[0] = decimals;
        });
        a.rf[0]
    }

    fn float_text(&mut self, v: f32, decimals: i32) -> Vec<u32> {
        let (a, p) = self.call(hh::FLOAT_TEXT, |a| {
            a.f[0] = v;
            a.i[0] = decimals;
        });
        slice(p as *const u32, usize::try_from(a.ri[0]).unwrap_or(0)).to_vec()
    }

    fn emit(&mut self, e: Event) {
        self.events.push(e);
    }
}

/// # Safety
/// `t` is live.
unsafe fn env(hx: Hx, t: P) -> TextEnv {
    // SAFETY: caller contract.
    unsafe {
        let w = this(t);
        let mut a = hh::Args::new(w);
        hx.raw(hh::FONT_RANGE, &mut a);
        TextEnv {
            font_first: a.ri[0],
            font_last: a.ri[1],
            font_size: hx.font_size(w),
            corner0: hx.gfx_size(hh::GFX_ACTIVE_SIZE, (*t).gfx[G_CORNERS]),
            default_decimals: DEFAULT_DECIMALS.load(Ordering::Relaxed),
        }
    }
}

/// Runs `f` over the editing logic with the current text; returns its result and the effects.
///
/// # Safety
/// `t` is live; `f` makes no host call into this widget.
unsafe fn with_edit<R>(
    hx: Hx,
    t: P,
    mouse: [f32; 2],
    f: impl FnOnce(&mut Edit<'_, Ops>) -> R,
) -> (R, Vec<Event>) {
    // SAFETY: caller contract.
    unsafe {
        let w = this(t);
        let environment = env(hx, t);
        let (p, n) = hx.text(w);
        let text = slice(p, n).to_vec();
        let illegal: Vec<u32> = (*t).illegal.as_ref().cloned().unwrap_or_default();
        let mut ops = Ops {
            hx,
            w,
            mouse,
            events: Vec::new(),
        };
        let r = {
            let mut e = Edit {
                st: &mut (*t).st,
                env: environment,
                text,
                illegal: &illegal,
                host: &mut ops,
            };
            f(&mut e)
        };
        (r, ops.events)
    }
}

/// Replays the `iWidget::SetText` effects; true is the `TextBoxEnter` result.
///
/// # Safety
/// `t` is live.
unsafe fn apply_events(hx: Hx, t: P, events: Vec<Event>, data: Option<&Msg>) -> bool {
    // SAFETY: caller contract.
    unsafe {
        let zero = Msg::zero();
        let data = data.unwrap_or(&zero);
        let mut result = true;
        for ev in events {
            match ev {
                Event::Enter => result = hx.process(this(t), MSG_TEXT_BOX_ENTER, 0, Some(data)),
                Event::SetText { text, nested } => {
                    (*t).replaying = true;
                    if let Some(r) = (*t).replay.as_mut() {
                        r.active = true;
                        r.nested = nested;
                    }
                    hx.set_text(this(t), (text.as_ptr(), text.len()));
                    if let Some(r) = (*t).replay.as_mut() {
                        r.active = false;
                        r.nested = None;
                    }
                    (*t).replaying = false;
                }
            }
        }
        result
    }
}

fn unicode(d: &Msg) -> u32 {
    d.0.get(24..28)
        .and_then(|b| <[u8; 4]>::try_from(b).ok())
        .map_or(0, u32::from_ne_bytes)
}

fn ret_value(r: Ret, enter: bool) -> bool {
    match r {
        Ret::Enter => enter,
        Ret::True => true,
        Ret::False => false,
    }
}

/// # Safety
/// `t` is uninitialised storage; `base` outlives it.
pub unsafe fn init(t: P, vt: *const HostVt, this: W, base: *mut Base, numeric: bool) {
    // SAFETY: caller contract; the struct is written whole.
    unsafe {
        t.write(TextBox {
            st: TextState::new(numeric),
            buttons: [0; 2],
            illegal: Box::into_raw(Box::default()),
            replay: Box::into_raw(Box::default()),
            prev_attention: 0,
            gfx: [0; 11],
            numeric,
            show_buttons: false,
            replaying: false,
            this,
            base,
            host: vt,
        });
    }
}

/// # Safety
/// `t` was initialised and is not used afterwards.
pub unsafe fn free(t: P) {
    // SAFETY: caller contract; the boxes came from `init`.
    unsafe {
        drop(Box::from_raw(std::mem::replace(
            &mut (*t).illegal,
            std::ptr::null_mut(),
        )));
        drop(Box::from_raw(std::mem::replace(
            &mut (*t).replay,
            std::ptr::null_mut(),
        )));
    }
}

/// # Safety
/// `t` is live.
pub unsafe fn destroy_buttons(hx: Hx, t: P) {
    // SAFETY: caller contract.
    unsafe {
        for b in (*t).buttons {
            if b != 0 {
                let mut a = hh::Args::new(this(t));
                a.p[0] = b;
                hx.raw(hh::DESTROY_WIDGET, &mut a);
            }
        }
    }
}

/// # Safety
/// `t` is live.
pub unsafe fn set_illegal_chars(t: P, text: (*const u32, usize)) {
    // SAFETY: caller contract.
    unsafe {
        if let Some(v) = (*t).illegal.as_mut() {
            *v = slice(text.0, text.1).to_vec();
        }
    }
}

/// # Safety
/// `t` is live.
pub unsafe fn set_max_text_length(hx: Hx, t: P, len: i32) {
    // SAFETY: caller contract.
    unsafe {
        let ((), ev) = with_edit(hx, t, [0.0; 2], |e| e.set_max_text_length(len));
        apply_events(hx, t, ev, None);
    }
}

/// # Safety
/// `t` is live.
pub unsafe fn set_selected_text(hx: Hx, t: P, start: i32, count: i32) {
    // SAFETY: caller contract.
    unsafe {
        let ((), ev) = with_edit(hx, t, [0.0; 2], |e| e.set_selected_text(start, count));
        apply_events(hx, t, ev, None);
    }
}

/// # Safety
/// `t` is live.
pub unsafe fn set_numeric_value(hx: Hx, t: P, v: f32) {
    // SAFETY: caller contract.
    unsafe {
        let ((), ev) = with_edit(hx, t, [0.0; 2], |e| e.set_numeric_value(v));
        apply_events(hx, t, ev, None);
    }
}

/// # Safety
/// `t` is live.
pub unsafe fn numeric_value(hx: Hx, t: P) -> f32 {
    // SAFETY: caller contract.
    unsafe {
        let (v, ev) = with_edit(hx, t, [0.0; 2], |e| e.numeric_value());
        apply_events(hx, t, ev, None);
        v
    }
}

/// # Safety
/// `t` is live.
pub unsafe fn background_size(hx: Hx, t: P) -> Vector2f {
    // SAFETY: caller contract.
    unsafe {
        let c = |i: usize| {
            hx.gfx_size(
                hh::GFX_ACTIVE_SIZE,
                (*t).gfx.get(G_CORNERS + i).copied().unwrap_or(0),
            )
        };
        let (c0, c1, c2) = (c(0), c(1), c(2));
        let s = size(t);
        Vector2f {
            x: s.x - c0.x - c1.x,
            y: s.y - c0.y - c2.y,
        }
    }
}

/// # Safety
/// `t` is live.
pub unsafe fn set_can_edit(hx: Hx, t: P, v: bool) {
    // SAFETY: caller contract.
    unsafe {
        (*t).st.can_edit = v;
        let mut a = hh::Args::new(this(t));
        a.i[0] = i32::from(v);
        hx.raw(hh::SET_POINTER_GFX, &mut a);
        for b in (*t).buttons {
            if b != 0 {
                hx.set_flag(hh::WIDGET_SET_ENABLED, b, v);
            }
        }
    }
}

/// # Safety
/// `t` is live.
unsafe fn button_pressed(hx: Hx, t: P, widget: W, d: &Msg) -> bool {
    // SAFETY: caller contract.
    unsafe {
        let which = if widget != 0 && widget == (*t).buttons[0] {
            0
        } else if widget != 0 && widget == (*t).buttons[1] {
            1
        } else {
            -1
        };
        let enabled = hx.flag(hh::WIDGET_IS_ENABLED, this(t));
        let (ret, ev) = with_edit(hx, t, [0.0; 2], |e| e.button_pressed(which, enabled));
        let enter = apply_events(hx, t, ev, Some(d));
        ret_value(ret, enter)
    }
}

extern "C" fn value_up(obj: *mut c_void, _w: W, _d: *const Msg) -> bool {
    value_step(obj, 0)
}

extern "C" fn value_down(obj: *mut c_void, _w: W, _d: *const Msg) -> bool {
    value_step(obj, 1)
}

fn value_step(obj: *mut c_void, which: usize) -> bool {
    let t = obj.cast::<TextBox>();
    // SAFETY: `obj` is the `TextBox` registered in `on_init`.
    unsafe {
        let Some(vt) = (*t).host.as_ref() else {
            return false;
        };
        let b = (*t).buttons.get(which).copied().unwrap_or(0);
        button_pressed(Hx::new(vt), t, b, &Msg::zero());
    }
    true
}

extern "C" fn button_cb(obj: *mut c_void, w: W, d: *const Msg) -> bool {
    let t = obj.cast::<TextBox>();
    // SAFETY: `obj` is the `TextBox` registered in `on_init`; `d` is the message reference.
    unsafe {
        let Some(vt) = (*t).host.as_ref() else {
            return false;
        };
        let zero = Msg::zero();
        button_pressed(Hx::new(vt), t, w, d.as_ref().unwrap_or(&zero))
    }
}

/// # Safety
/// `t` is live.
pub unsafe fn on_change_size(hx: Hx, t: P) {
    // SAFETY: caller contract.
    unsafe {
        let b = |i: usize| {
            hx.gfx_size(
                hh::GFX_ACTIVE_SIZE,
                (*t).gfx.get(G_BORDERS + i).copied().unwrap_or(0),
            )
        };
        let (b0, b1, b2) = (b(0), b(1), b(2));
        let button = (*t).buttons[0];
        let button_w = if (*t).numeric && button != 0 {
            hx.size(button).x
        } else {
            0.0
        };
        let fy = hx.font_size(this(t)).y;
        let sz = size(t);
        let (h, w) = wt::layout(&(*t).st, sz.x, b0, b1, b2, fy, button_w);
        if let Some(base) = (*t).base.as_mut() {
            base.size.y = h;
        }
        (*t).st.text_max_size = w;
        on_change_text(hx, t);
    }
}

/// # Safety
/// `t` is live.
pub unsafe fn on_change_text(hx: Hx, t: P) {
    // SAFETY: caller contract.
    unsafe {
        if (*t).replaying {
            let nested = (*t).replay.as_mut().and_then(|r| {
                let n = r.nested.take();
                let active = std::mem::replace(&mut r.active, false);
                active.then_some(n).flatten()
            });
            if let Some(n) = nested {
                hx.set_text(this(t), (n.as_ptr(), n.len()));
            }
            (*t).replaying = false;
            return;
        }
        let changed = (*t).base.as_ref().is_some_and(|b| b.text_changed);
        let ((), ev) = with_edit(hx, t, [0.0; 2], |e| e.on_change_text(changed));
        apply_events(hx, t, ev, None);
    }
}

/// # Safety
/// `t` is live.
pub unsafe fn on_init(hx: Hx, t: P) {
    // SAFETY: caller contract.
    unsafe {
        if !(*t).numeric || !(*t).show_buttons {
            return;
        }
        let w = this(t);
        let o = t as usize;
        hx.add_callback(w, MSG_TEXT_BOX_VALUE_UP, o, value_up as *const () as usize);
        hx.add_callback(
            w,
            MSG_TEXT_BOX_VALUE_DOWN,
            o,
            value_down as *const () as usize,
        );
        let sz = size(t);
        for i in 0..2 {
            let mut a = hh::Args::new(w);
            a.f[..5].copy_from_slice(&[sz.x - 10.0, 1.0 + i as f32 * 10.0, 0.1, 10.0, 10.0]);
            let b = hx.raw(hh::CREATE_BUTTON_AT, &mut a);
            if let Some(slot) = (*t).buttons.get_mut(i) {
                *slot = b;
            }
            let mut a = hh::Args::new(b);
            a.f[0] = 12.0;
            a.f[1] = 3.0;
            hx.raw(hh::BUTTON_SETUP, &mut a);
            hx.add_callback(b, MSG_BUTTON_PRESSED, o, button_cb as *const () as usize);
        }
        let plus: [u32; 1] = [u32::from(b'+')];
        let minus: [u32; 1] = [u32::from(b'-')];
        hx.set_text((*t).buttons[0], (plus.as_ptr(), 1));
        hx.set_text((*t).buttons[1], (minus.as_ptr(), 1));
        (*t).st.numeric_value = -1.0;
        set_numeric_value(hx, t, 0.0);
    }
}

/// # Safety
/// `t` is live.
pub unsafe fn on_load_graphics(hx: Hx, t: P) {
    // SAFETY: caller contract.
    unsafe {
        let w = this(t);
        let g = |n: &str| hx.skin_gfx(w, gfx_id(n));
        (*t).gfx = [
            g("TextBoxMarker"),
            g("TextBoxSelectedTextBack"),
            g("TextBoxBackground"),
            g("FrameBorderRight"),
            g("FrameBorderLeft"),
            g("FrameBorderUp"),
            g("FrameBorderDown"),
            g("FrameCornerLU"),
            g("FrameCornerRU"),
            g("FrameCornerRD"),
            g("FrameCornerLD"),
        ];
        on_change_size(hx, t);
    }
}

/// # Safety
/// `t` is live.
pub unsafe fn on_draw(hx: Hx, t: P) {
    // SAFETY: caller contract.
    unsafe {
        let w = this(t);
        let g = hx.global(w);
        let (d, _) = with_edit(hx, t, [0.0; 2], |e| e.draw(g));
        let (p, n) = hx.text(w);
        let text = slice(p, n).to_vec();
        let part = |start: i32, len: i32| wt::sub_w(&text, start, len);
        let shown = part(d.text_start, d.text_len);
        hx.draw_text_col(
            w,
            (shown.as_ptr(), shown.len()),
            d.text_pos,
            ALIGN_LEFT,
            COLOR_FONT,
        );
        if d.has_marker {
            hx.draw_gfx(w, (*t).gfx[G_MARKER], d.marker_pos, d.marker_size, None);
            if d.has_selection {
                hx.draw_gfx(w, (*t).gfx[G_SELECTED], d.sel_pos, d.sel_size, None);
                let hl = part(d.hl_start, d.hl_len);
                hx.draw_text(w, (hl.as_ptr(), hl.len()), d.hl_pos, ALIGN_LEFT, true);
            }
        }
        let gf = &(*t).gfx;
        let b = [
            gf[G_BORDERS],
            gf[G_BORDERS + 1],
            gf[G_BORDERS + 2],
            gf[G_BORDERS + 3],
        ];
        let k = [
            gf[G_CORNERS],
            gf[G_CORNERS + 1],
            gf[G_CORNERS + 2],
            gf[G_CORNERS + 3],
        ];
        hx.draw_borders(w, gf[G_BG], (&b, &k), hx.global(w), size(t), None);
    }
}

/// # Safety
/// `t` is live.
pub unsafe fn on_mouse_move(hx: Hx, t: P, d: &Msg) -> bool {
    // SAFETY: caller contract.
    unsafe {
        let (_, ev) = with_edit(hx, t, d.pos(), |e| e.mouse_move());
        apply_events(hx, t, ev, Some(d));
        true
    }
}

/// # Safety
/// `t` is live.
pub unsafe fn on_mouse_down(hx: Hx, t: P, d: &Msg) -> bool {
    // SAFETY: caller contract.
    unsafe {
        let ((ret, attention), ev) = with_edit(hx, t, d.pos(), |e| e.mouse_down(d.val()));
        apply_events(hx, t, ev, Some(d));
        if ret == Ret::False {
            return false;
        }
        if attention {
            let w = this(t);
            (*t).prev_attention = hx.set_call(w, hh::SC_GET_ATTENTION, 0).0;
            hx.set_call(w, hh::SC_SET_ATTENTION, w);
        }
        true
    }
}

/// # Safety
/// `t` is live.
pub unsafe fn on_mouse_up(hx: Hx, t: P, d: &Msg) -> bool {
    // SAFETY: caller contract.
    unsafe {
        let (restore, ev) = with_edit(hx, t, d.pos(), |e| e.mouse_up(d.val()));
        apply_events(hx, t, ev, Some(d));
        let Some((marker, sel_end)) = restore else {
            return true;
        };
        // SetAttention steals focus, so the variables are restored after it
        let mut a = hh::Args::new(this(t));
        a.i[0] = hh::SC_SET_ATTENTION_NO_CLEAR;
        a.p[0] = (*t).prev_attention;
        hx.raw(hh::SET_CALL, &mut a);
        let ((), ev) = with_edit(hx, t, [0.0; 2], |e| e.mouse_up_restore(marker, sel_end));
        apply_events(hx, t, ev, Some(d));
        true
    }
}

/// # Safety
/// `t` is live.
pub unsafe fn on_mouse_double_click(hx: Hx, t: P, d: &Msg) -> bool {
    // SAFETY: caller contract.
    unsafe {
        let ((), ev) = with_edit(hx, t, d.pos(), |e| e.double_click(d.val()));
        apply_events(hx, t, ev, Some(d));
        true
    }
}

/// # Safety
/// `t` is live.
pub unsafe fn on_got_focus(hx: Hx, t: P) -> bool {
    // SAFETY: caller contract.
    unsafe {
        with_edit(hx, t, [0.0; 2], |e| e.got_focus());
        true
    }
}

/// # Safety
/// `t` is live.
pub unsafe fn on_got_tab_focus(hx: Hx, t: P) -> bool {
    // SAFETY: caller contract.
    unsafe {
        with_edit(hx, t, [0.0; 2], |e| e.got_tab_focus());
        true
    }
}

/// # Safety
/// `t` is live.
pub unsafe fn on_lost_focus(hx: Hx, t: P, d: &Msg) -> bool {
    // SAFETY: caller contract.
    unsafe {
        let ((), ev) = with_edit(hx, t, [0.0; 2], |e| e.lost_focus());
        apply_events(hx, t, ev, Some(d));
        false
    }
}

/// # Safety
/// `t` is live.
pub unsafe fn on_key_press(hx: Hx, t: P, d: &Msg) -> bool {
    // SAFETY: caller contract.
    unsafe {
        let unicode = unicode(d);
        let ((ret, out), ev) = with_edit(hx, t, [0.0; 2], |e| {
            e.key_press(d.key(), d.modifier(), unicode)
        });
        if out.reset_marker_anim && (*t).gfx[G_MARKER] != 0 {
            let mut a = hh::Args::new(this(t));
            a.p[0] = (*t).gfx[G_MARKER];
            hx.raw(hh::GFX_ANIM_RESET, &mut a);
        }
        let enter = apply_events(hx, t, ev, Some(d));
        ret_value(ret, enter)
    }
}

/// # Safety
/// `t` is live.
pub unsafe fn on_ui_button_press(hx: Hx, t: P, d: &Msg) -> bool {
    // SAFETY: caller contract.
    unsafe {
        let focus = hx.has_focus(this(t));
        let (keyboard, ev) = with_edit(hx, t, [0.0; 2], |e| e.ui_button(focus, d.val()));
        apply_events(hx, t, ev, Some(d));
        if keyboard {
            hx.raw(hh::CREATE_POPUP_KEYBOARD, &mut hh::Args::new(this(t)));
            return true;
        }
        false
    }
}

/// # Safety
/// `t` is live.
pub unsafe fn on_enter(hx: Hx, t: P, d: &Msg) -> bool {
    // SAFETY: caller contract.
    unsafe {
        let (issued, ev) = with_edit(hx, t, [0.0; 2], |e| e.on_enter());
        let result = apply_events(hx, t, ev, Some(d));
        if issued { result } else { true }
    }
}

/// # Safety
/// `t` is live.
pub unsafe fn ui_keyboard_open(hx: Hx, t: P) {
    // SAFETY: caller contract.
    unsafe {
        let ((), ev) = with_edit(hx, t, [0.0; 2], |e| e.ui_keyboard_open());
        apply_events(hx, t, ev, None);
    }
}
