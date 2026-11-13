//! `cWidgetTabFrame`, `cWidgetTab` and `cWidgetTabLabel`: tab list, scroll arrows, label layout
//! and drawing. State is [`Frame`], [`Tab`] and [`Label`], embedded in the C++ widgets
//! (`gui/RustGuiH20.h`); the three reach each other through state pointers.

use super::h19_widget::Base;
use super::h20_host::{self as hh, HostVt, Hx};
use super::h20_list::{MSG_SELECTION_CHANGE, attr_id, gfx_id};
use super::set::{Msg, W};
use super::widget::v2;
use super::widgets_slider::{
    TabFrameState, TabItem, tab_init, tab_layout, tab_page_target, tab_point_inside,
};
use crate::math::v3;
use crate::types::{Vector2f, Vector3f};
use std::ffi::c_void;

const MSG_BUTTON_PRESSED: i32 = 11;
const ALIGN_LEFT: i32 = 0;
const KIND_LABEL: i32 = 0;
const KIND_TAB: i32 = 1;

const G_ARROWS: usize = 0;
const G_LABEL_BORDERS: usize = 2;
const G_LABEL_CORNERS: usize = 6;
const G_LABEL_BG: usize = 10;
const G_LABEL_BG_ACTIVE: usize = 11;
const G_TAB_BORDERS: usize = 12;
const G_TAB_CORNERS: usize = 16;
const G_TAB_BG: usize = 20;

#[repr(C)]
pub struct Frame {
    pub state: TabFrameState,
    pub top_tab: W,
    pub arrows: [W; 2],
    pub tabs: *mut Vec<W>,
    pub gfx: [usize; 21],
    pub bg_z: f32,
    pub fg_z: f32,
    pub hori: bool,
    pub vert: bool,
    pub this: W,
    pub base: *mut Base,
    pub host: *const HostVt,
}

#[repr(C)]
pub struct Tab {
    pub index: i32,
    pub on_top: bool,
    pub frame_w: W,
    pub fs: *mut Frame,
    pub label: W,
    pub frame: W,
    pub ls: *mut Label,
    pub this: W,
    pub base: *mut Base,
    pub host: *const HostVt,
}

#[repr(C)]
pub struct Label {
    pub tab: *mut Tab,
    pub icon: usize,
    pub this: W,
    pub base: *mut Base,
    pub host: *const HostVt,
}

const _: () = assert!(std::mem::size_of::<Frame>() == 280);
const _: () = assert!(std::mem::size_of::<Tab>() == 72);
const _: () = assert!(std::mem::size_of::<Label>() == 40);

fn base_size(b: *mut Base) -> Vector2f {
    // SAFETY: `b` is the widget's base state or null.
    unsafe { b.as_ref() }.map_or(Vector2f { x: 0.0, y: 0.0 }, |b| b.size)
}

/// # Safety
/// `f` is live.
unsafe fn tabs(f: *mut Frame) -> Vec<W> {
    // SAFETY: caller contract.
    unsafe { (*f).tabs.as_ref() }.cloned().unwrap_or_default()
}

fn state_of<T>(hx: Hx, w: W, kind: i32) -> *mut T {
    let mut a = hh::Args::new(w);
    a.i[0] = kind;
    hx.raw(hh::STATE_PTR, &mut a) as *mut T
}

// ---------------------------------------------------------------- frame

/// # Safety
/// `f` is uninitialised storage.
pub unsafe fn frame_init(f: *mut Frame, vt: *const HostVt, this: W, base: *mut Base) {
    // SAFETY: caller contract; the struct is written whole.
    unsafe {
        f.write(Frame {
            state: TabFrameState::default(),
            top_tab: 0,
            arrows: [0; 2],
            tabs: Box::into_raw(Box::default()),
            gfx: [0; 21],
            bg_z: 0.0,
            fg_z: 0.9,
            hori: true,
            vert: true,
            this,
            base,
            host: vt,
        });
    }
}

/// # Safety
/// `f` was initialised and is not used afterwards.
pub unsafe fn frame_free(f: *mut Frame) {
    // SAFETY: caller contract; the box came from `frame_init`.
    unsafe {
        drop(Box::from_raw(std::mem::replace(
            &mut (*f).tabs,
            std::ptr::null_mut(),
        )));
    }
}

/// # Safety
/// `f` is live.
pub unsafe fn frame_destroy(hx: Hx, f: *mut Frame) {
    // SAFETY: caller contract.
    unsafe {
        for a in (*f).arrows {
            destroy(hx, (*f).this, a);
        }
        clear_tabs(hx, f);
    }
}

fn destroy(hx: Hx, owner: W, w: W) {
    let mut a = hh::Args::new(owner);
    a.p[0] = w;
    hx.raw(hh::DESTROY_WIDGET, &mut a);
}

/// # Safety
/// `f` is live.
pub unsafe fn add_tab(hx: Hx, f: *mut Frame, caption: (*const u32, usize)) -> W {
    // SAFETY: caller contract.
    unsafe {
        let w = (*f).this;
        let (pos, size) = ((*f).base.as_ref().map(|b| b.pos), base_size((*f).base));
        let pos = pos.unwrap_or_default();
        let mut a = hh::Args::new(w);
        a.f[..5].copy_from_slice(&[pos.x, pos.y, pos.z, size.x, size.y]);
        let tab = hx.raw(hh::CREATE_TAB, &mut a);
        hx.set_text(tab, caption);
        hx.set_position(tab, v3(0.0, 0.0, (*f).bg_z));
        if (*(*f).tabs).is_empty() {
            set_tab_on_top(hx, f, tab);
        }
        if let Some(t) = (*f).tabs.as_mut() {
            t.push(tab);
        }
        update_tab_visibility(hx, f);
        tab
    }
}

/// # Safety
/// `f` is live.
pub unsafe fn clear_tabs(hx: Hx, f: *mut Frame) {
    // SAFETY: caller contract.
    unsafe {
        for t in tabs(f) {
            destroy(hx, (*f).this, t);
        }
        if let Some(t) = (*f).tabs.as_mut() {
            t.clear();
        }
    }
}

/// # Safety
/// `f` is live.
pub unsafe fn tab_num(f: *mut Frame) -> i32 {
    // SAFETY: caller contract.
    unsafe { (*f).tabs.as_ref() }.map_or(0, |t| t.len() as i32)
}

/// # Safety
/// `f` is live.
pub unsafe fn tab_at(f: *mut Frame, idx: i32) -> W {
    // SAFETY: caller contract.
    unsafe {
        (*f).tabs
            .as_ref()
            .and_then(|t| usize::try_from(idx).ok().and_then(|i| t.get(i)))
            .copied()
            .unwrap_or(0)
    }
}

/// # Safety
/// `f` is live.
pub unsafe fn tab_on_top_index(f: *mut Frame) -> i32 {
    // SAFETY: caller contract.
    unsafe {
        let top = (*f).top_tab;
        tabs(f)
            .iter()
            .position(|t| *t == top)
            .map_or(-1, |i| i as i32)
    }
}

/// # Safety
/// `f` is live.
pub unsafe fn set_tab_on_top(hx: Hx, f: *mut Frame, tab: W) {
    // SAFETY: caller contract.
    unsafe {
        let top = (*f).top_tab;
        if tab == top {
            return;
        }
        if top != 0 {
            let mut v = local_pos(hx, top);
            v.z = (*f).bg_z;
            hx.set_position(top, v);
            tab_set_on_top(hx, state_of(hx, top, KIND_TAB), false);
        }
        (*f).top_tab = tab;
        let mut v = local_pos(hx, tab);
        v.z = (*f).fg_z;
        hx.set_position(tab, v);
        let ts = state_of::<Tab>(hx, tab, KIND_TAB);
        tab_set_on_top(hx, ts, true);
        if !(*(*f).tabs).is_empty() {
            let idx = ts.as_ref().map_or(0, |t| t.index);
            hx.process((*f).this, MSG_SELECTION_CHANGE, idx, None);
        }
    }
}

fn local_pos(hx: Hx, w: W) -> Vector3f {
    let mut a = hh::Args::new(w);
    hx.raw(hh::WIDGET_GET_POSITION, &mut a);
    v3(a.rf[0], a.rf[1], a.rf[2])
}

/// # Safety
/// `f` is live.
pub unsafe fn set_tab_on_top_by_index(hx: Hx, f: *mut Frame, idx: i32) {
    // SAFETY: caller contract.
    unsafe {
        let n = tab_num(f);
        if idx < 0 || idx >= n {
            return;
        }
        set_tab_on_top(hx, f, tab_at(f, idx));
    }
}

/// # Safety
/// `f` is live.
pub unsafe fn set_horizontal_scroll_enabled(f: *mut Frame, v: bool) {
    // SAFETY: caller contract.
    unsafe {
        if (*(*f).tabs).is_empty() {
            (*f).hori = v;
        }
    }
}

/// # Safety
/// `f` is live.
pub unsafe fn set_vertical_scroll_enabled(f: *mut Frame, v: bool) {
    // SAFETY: caller contract.
    unsafe {
        if (*(*f).tabs).is_empty() {
            (*f).vert = v;
        }
    }
}

/// # Safety
/// `f` is live.
pub unsafe fn on_ui_button_press(hx: Hx, f: *mut Frame, d: &Msg) -> bool {
    // SAFETY: caller contract.
    unsafe {
        let top = tab_on_top_index(f);
        let Some(target) = tab_page_target(d.val(), top) else {
            return false;
        };
        set_tab_on_top_by_index(hx, f, target);
        true
    }
}

/// # Safety
/// `f` is live.
pub unsafe fn arrow_pressed(hx: Hx, f: *mut Frame, widget: W) -> bool {
    // SAFETY: caller contract.
    unsafe {
        let n = tab_num(f);
        if widget == (*f).arrows[0] {
            (*f).state.arrow(false, n);
        }
        if widget == (*f).arrows[1] {
            (*f).state.arrow(true, n);
        }
        update_tab_visibility(hx, f);
        true
    }
}

extern "C" fn arrow_cb(obj: *mut c_void, w: W, _d: *const Msg) -> bool {
    let f = obj.cast::<Frame>();
    // SAFETY: `obj` is the `Frame` registered in `on_init`.
    unsafe {
        match (*f).host.as_ref() {
            Some(vt) => arrow_pressed(Hx::new(vt), f, w),
            None => false,
        }
    }
}

/// # Safety
/// `f` is live.
pub unsafe fn update_tab_visibility(hx: Hx, f: *mut Frame) {
    // SAFETY: caller contract.
    unsafe {
        let all = tabs(f);
        let labels: Vec<W> = all
            .iter()
            .map(|t| {
                state_of::<Tab>(hx, *t, KIND_TAB)
                    .as_ref()
                    .map_or(0, |t| t.label)
            })
            .collect();
        let widths: Vec<f32> = labels.iter().map(|l| hx.size(*l).x).collect();
        let mut items = vec![TabItem::default(); all.len()];
        let sx = base_size((*f).base).x;
        let arrows = (*f).state.visibility(sx, &widths, &mut items);
        let first = (*f).state.first_visible;
        for (i, (it, label)) in items.iter().zip(&labels).enumerate() {
            if it.visible || (i as i32) >= first {
                continue;
            }
            hx.set_flag(hh::WIDGET_SET_VISIBLE, *label, false);
            hx.set_flag(hh::WIDGET_SET_ENABLED, *label, false);
        }
        for (it, label) in items.iter().zip(&labels) {
            if !it.visible {
                continue;
            }
            hx.set_flag(hh::WIDGET_SET_VISIBLE, *label, true);
            hx.set_flag(hh::WIDGET_SET_ENABLED, *label, true);
            set_label_pos_x(hx, *label, it.pos_x);
        }
        hx.set_flag(hh::WIDGET_SET_ENABLED, (*f).arrows[0], arrows.left_enabled);
        hx.set_flag(hh::WIDGET_SET_ENABLED, (*f).arrows[1], arrows.right_enabled);
        hx.set_flag(hh::WIDGET_SET_VISIBLE, (*f).arrows[0], arrows.shown);
        hx.set_flag(hh::WIDGET_SET_VISIBLE, (*f).arrows[1], arrows.shown);
    }
}

fn set_label_pos_x(hx: Hx, label: W, x: f32) {
    let mut p = local_pos(hx, label);
    p.x = x;
    hx.set_position(label, p);
}

/// # Safety
/// `f` is live.
pub unsafe fn set_scroll_buttons_hidden(hx: Hx, f: *mut Frame, hidden: bool) {
    // SAFETY: caller contract.
    unsafe {
        for a in (*f).arrows {
            hx.set_flag(hh::WIDGET_SET_ENABLED, a, !hidden);
            hx.set_flag(hh::WIDGET_SET_VISIBLE, a, !hidden);
        }
    }
}

/// # Safety
/// `f` is live.
pub unsafe fn frame_on_init(hx: Hx, f: *mut Frame) {
    // SAFETY: caller contract.
    unsafe {
        let w = (*f).this;
        for i in 0..2 {
            let b = hx.raw(hh::CREATE_BUTTON, &mut hh::Args::new(w));
            if let Some(slot) = (*f).arrows.get_mut(i) {
                *slot = b;
            }
            let mut a = hh::Args::new(b);
            a.p[0] = (*f).gfx.get(G_ARROWS + i).copied().unwrap_or(0);
            hx.raw(hh::BUTTON_SET_IMAGE, &mut a);
            let bs = (*f).state.button_size;
            hx.set_size(b, v2(bs, bs));
            hx.add_callback(
                b,
                MSG_BUTTON_PRESSED,
                f as usize,
                arrow_cb as *const () as usize,
            );
        }
        frame_on_change_size(hx, f);
    }
}

/// # Safety
/// `f` is live.
pub unsafe fn frame_on_change_size(hx: Hx, f: *mut Frame) {
    // SAFETY: caller contract.
    unsafe {
        if (*f).arrows[0] == 0 || (*f).arrows[1] == 0 {
            return;
        }
        let size = base_size((*f).base);
        for i in 0..2 {
            let p = (*f).state.arrow_pos(size.x, i as i32, (*f).fg_z);
            hx.set_position((*f).arrows.get(i).copied().unwrap_or(0), p);
        }
        for t in tabs(f) {
            hx.set_size(t, size);
        }
        update_tab_visibility(hx, f);
    }
}

/// # Safety
/// `f` is live.
pub unsafe fn frame_on_load_graphics(hx: Hx, f: *mut Frame) {
    // SAFETY: caller contract.
    unsafe {
        let w = (*f).this;
        let at = |n: &str| hx.skin_attr(w, attr_id(n)).x;
        let st = &mut (*f).state;
        st.button_size = at("TabFrameButtonSize");
        st.first_tab_offset = at("FirstTabOffset");
        st.tab_separation = at("TabSeparation");
        st.inactive_offset = at("TabFrameLabelInactiveOffset");
        st.active_offset = at("TabFrameLabelActiveOffset");
        st.icon_padding = at("TabFrameIconPadding");
        // the C++ reads the right padding into the left field and the reverse
        st.text_left_padding = at("TabFrameTextRightPadding");
        st.text_right_padding = at("TabFrameTextLeftPadding");
        let g = |n: &str| hx.skin_gfx(w, gfx_id(n));
        (*f).gfx = [
            g("TabFrameArrowLeft"),
            g("TabFrameArrowRight"),
            g("TabLabelBorderRight"),
            g("TabLabelBorderLeft"),
            g("TabLabelBorderUp"),
            g("TabLabelBorderDown"),
            g("TabLabelCornerLU"),
            g("TabLabelCornerRU"),
            g("TabLabelCornerRD"),
            g("TabLabelCornerLD"),
            g("TabLabelBackground"),
            g("TabLabelBackgroundActive"),
            g("TabBorderRight"),
            g("TabBorderLeft"),
            g("TabBorderUp"),
            g("TabBorderDown"),
            g("TabCornerLU"),
            g("TabCornerRU"),
            g("TabCornerRD"),
            g("TabCornerLD"),
            g("TabBackground"),
        ];
    }
}

// ---------------------------------------------------------------- tab

/// # Safety
/// `t` is uninitialised storage; `fs` the parent frame's state.
pub unsafe fn tab_init_state(
    t: *mut Tab,
    vt: *const HostVt,
    this: W,
    frame_w: W,
    fs: *mut Frame,
    base: *mut Base,
) {
    // SAFETY: caller contract.
    unsafe {
        t.write(Tab {
            index: tab_num(fs),
            on_top: false,
            frame_w,
            fs,
            label: 0,
            frame: 0,
            ls: std::ptr::null_mut(),
            this,
            base,
            host: vt,
        });
    }
}

/// # Safety
/// `t` is live.
pub unsafe fn tab_destroy(hx: Hx, t: *mut Tab) {
    // SAFETY: caller contract.
    unsafe {
        destroy(hx, (*t).this, (*t).label);
        destroy(hx, (*t).this, (*t).frame);
    }
}

/// # Safety
/// `t` is live.
pub unsafe fn tab_attach_child(hx: Hx, t: *mut Tab, child: W, remove: bool) {
    // SAFETY: caller contract.
    unsafe {
        let (label, frame, this) = ((*t).label, (*t).frame, (*t).this);
        let code = if remove {
            hh::REMOVE_CHILD
        } else {
            hh::ATTACH_CHILD
        };
        let mut a = hh::Args::new(if child == label || child == frame {
            this
        } else {
            frame
        });
        a.p[0] = child;
        a.i[0] = i32::from(!(child == label || child == frame));
        hx.raw(code, &mut a);
    }
}

/// # Safety
/// `t` is live.
pub unsafe fn tab_set_icon(hx: Hx, t: *mut Tab, icon: usize) {
    // SAFETY: caller contract.
    unsafe {
        if let Some(l) = (*t).ls.as_mut() {
            l.icon = icon;
        }
        label_on_change_text(hx, (*t).ls);
    }
}

/// # Safety
/// `t` is live.
pub unsafe fn tab_set_on_top(hx: Hx, t: *mut Tab, v: bool) {
    // SAFETY: caller contract.
    unsafe {
        let Some(tab) = t.as_mut() else { return };
        tab.on_top = v;
        let mut pos = local_pos(hx, tab.label);
        pos.y = (*tab.fs).state.label_pos_y(v);
        hx.set_position(tab.label, pos);
        hx.set_flag(hh::WIDGET_SET_ENABLED, tab.frame, v);
        hx.set_flag(hh::WIDGET_SET_VISIBLE, tab.frame, v);
    }
}

/// # Safety
/// `t` is live.
pub unsafe fn tab_point_is_inside(hx: Hx, t: *mut Tab, p: [f32; 2], only_clipped: bool) -> bool {
    // SAFETY: caller contract.
    unsafe {
        let mut a = hh::Args::new((*t).this);
        a.f[..2].copy_from_slice(&p);
        if hx.raw(hh::CHECK_POINT_CLIP, &mut a) == 0 {
            return false;
        }
        let clips = (*t).base.as_ref().is_some_and(|b| b.clips);
        if only_clipped && !clips {
            return true;
        }
        let (label, frame) = ((*t).label, (*t).frame);
        let visible = hx.flag(hh::WIDGET_IS_VISIBLE, label);
        tab_point_inside(
            v2(p[0], p[1]),
            visible,
            hx.global(label),
            hx.size(label),
            hx.global(frame),
            hx.size(frame),
        )
    }
}

/// # Safety
/// `t` is live.
pub unsafe fn tab_on_change_text(hx: Hx, t: *mut Tab) {
    // SAFETY: caller contract.
    unsafe {
        if (*t).label != 0 {
            hx.set_text((*t).label, hx.text((*t).this));
        }
    }
}

fn skin_font_y(hx: Hx, w: W) -> f32 {
    let mut a = hh::Args::new(w);
    hx.raw(hh::SKIN_FONT_SIZE, &mut a);
    a.rf[1]
}

/// # Safety
/// `t` is live.
pub unsafe fn tab_on_change_size(hx: Hx, t: *mut Tab) {
    // SAFETY: caller contract.
    unsafe {
        if (*t).label == 0 || (*t).frame == 0 {
            return;
        }
        let fy = skin_font_y(hx, (*t).this);
        let l = tab_layout(base_size((*t).base), fy, (*(*t).fs).state.active_offset);
        hx.set_position((*t).label, l.label_pos);
        hx.set_position((*t).frame, l.frame_pos);
        hx.set_size((*t).frame, l.frame_size);
    }
}

/// # Safety
/// `t` is live.
pub unsafe fn tab_on_init(hx: Hx, t: *mut Tab) {
    // SAFETY: caller contract.
    unsafe {
        let this = (*t).this;
        let fs = (*t).fs;
        let c = |i: usize| {
            hx.gfx_size(
                hh::GFX_ACTIVE_SIZE,
                (*fs).gfx.get(G_TAB_CORNERS + i).copied().unwrap_or(0),
            )
        };
        let fy = skin_font_y(hx, this);
        let init = tab_init(
            base_size((*t).base),
            fy,
            (*fs).state.active_offset,
            c(0),
            c(1),
            c(2),
        );
        let mut a = hh::Args::new(this);
        a.f[1] = init.label_h;
        let label = hx.raw(hh::CREATE_TAB_LABEL, &mut a);
        (*t).label = label;
        let ls = state_of::<Label>(hx, label, KIND_LABEL);
        if let Some(l) = ls.as_mut() {
            l.tab = t;
        }
        (*t).ls = ls;
        let mut a = hh::Args::new(this);
        a.f[0] = init.frame_size.x;
        a.f[1] = init.frame_size.y;
        a.i[0] = i32::from((*fs).hori);
        a.i[1] = i32::from((*fs).vert);
        let frame = hx.raw(hh::CREATE_FRAME, &mut a);
        (*t).frame = frame;
        hx.set_flag(hh::WIDGET_SET_CLIP_ACTIVE, frame, true);
        tab_attach_child(hx, t, label, false);
        tab_attach_child(hx, t, frame, false);
        hx.set_position(label, v3(0.0, 0.0, 0.1));
        hx.set_position(frame, init.frame_pos);
        tab_set_on_top(hx, t, false);
    }
}

/// # Safety
/// `t` is live.
pub unsafe fn tab_on_draw(hx: Hx, t: *mut Tab) {
    // SAFETY: caller contract.
    unsafe {
        let fs = (*t).fs;
        let g = &(*fs).gfx;
        let b = [
            g[G_TAB_BORDERS],
            g[G_TAB_BORDERS + 1],
            g[G_TAB_BORDERS + 2],
            g[G_TAB_BORDERS + 3],
        ];
        let k = [
            g[G_TAB_CORNERS],
            g[G_TAB_CORNERS + 1],
            g[G_TAB_CORNERS + 2],
            g[G_TAB_CORNERS + 3],
        ];
        let frame = (*t).frame;
        hx.draw_borders(
            (*t).this,
            g[G_TAB_BG],
            (&b, &k),
            hx.global(frame),
            hx.size(frame),
            Some(true),
        );
    }
}

// ---------------------------------------------------------------- label

/// # Safety
/// `l` is uninitialised storage.
pub unsafe fn label_init(l: *mut Label, vt: *const HostVt, this: W, base: *mut Base) {
    // SAFETY: caller contract.
    unsafe {
        l.write(Label {
            tab: std::ptr::null_mut(),
            icon: 0,
            this,
            base,
            host: vt,
        });
    }
}

/// # Safety
/// `l` is live.
pub unsafe fn label_on_mouse_down(hx: Hx, l: *mut Label) -> bool {
    // SAFETY: caller contract.
    unsafe {
        let Some((f, tab_w)) = (*l).tab.as_ref().map(|t| (t.fs, t.this)) else {
            return true;
        };
        let n = tab_num(f) as i64;
        let mut i = 0i64;
        loop {
            let (pos, size) = ((*l).base.as_ref().map(|b| b.pos), base_size((*l).base));
            let px = pos.map_or(0.0, |p| p.x);
            if !(i <= n && px + size.x > base_size((*f).base).x) {
                break;
            }
            let first = (*f).state.first_visible;
            arrow_pressed(hx, f, (*f).arrows[1]);
            if (*f).state.first_visible == first {
                break;
            }
            i += 1;
        }
        set_tab_on_top(hx, f, tab_w);
        true
    }
}

/// # Safety
/// `l` is live.
pub unsafe fn label_on_change_text(hx: Hx, l: *mut Label) {
    // SAFETY: caller contract.
    unsafe {
        let Some(label) = l.as_ref().map(|l| (l.tab, l.icon, l.this, l.base)) else {
            return;
        };
        let (tab, icon, this, base) = label;
        let Some(fs) = tab.as_ref().map(|t| t.fs) else {
            return;
        };
        let b = |i: usize| {
            hx.gfx_size(
                hh::GFX_IMAGE_SIZE,
                (*fs).gfx.get(G_LABEL_BORDERS + i).copied().unwrap_or(0),
            )
            .x
        };
        let borders = b(0) + b(1);
        let mut a = hh::Args::new(this);
        hx.raw(hh::TEXT_LENGTH_SKIN, &mut a);
        let length = a.rf[0];
        let icon_w = (icon != 0).then(|| hx.gfx_size(hh::GFX_IMAGE_SIZE, icon).x);
        let width = (*fs).state.label_width(length, borders, icon_w);
        let h = base_size(base).y;
        hx.set_size(this, v2(width, h));
    }
}

/// # Safety
/// `l` is live.
pub unsafe fn label_on_get_ui_nav_focus(hx: Hx, l: *mut Label) -> bool {
    // SAFETY: caller contract.
    unsafe {
        if let Some(tab) = (*l).tab.as_ref() {
            set_tab_on_top(hx, tab.fs, tab.this);
        }
        true
    }
}

/// # Safety
/// `l` is live.
pub unsafe fn label_on_draw(hx: Hx, l: *mut Label) {
    // SAFETY: caller contract.
    unsafe {
        let Some((tab, icon, w, base)) = l.as_ref().map(|l| (l.tab, l.icon, l.this, l.base)) else {
            return;
        };
        let Some((fs, on_top)) = tab.as_ref().map(|t| (t.fs, t.on_top)) else {
            return;
        };
        let global = hx.global(w);
        let icon_w = (icon != 0).then(|| hx.gfx_size(hh::GFX_IMAGE_SIZE, icon).x);
        let text_pos = (*fs).state.label_text_pos(global, icon_w);
        let g = &(*fs).gfx;
        let bg = if on_top {
            g[G_LABEL_BG_ACTIVE]
        } else {
            g[G_LABEL_BG]
        };
        hx.draw_text(w, hx.text(w), text_pos, ALIGN_LEFT, false);
        let b = [
            g[G_LABEL_BORDERS],
            g[G_LABEL_BORDERS + 1],
            g[G_LABEL_BORDERS + 2],
            g[G_LABEL_BORDERS + 3],
        ];
        let k = [
            g[G_LABEL_CORNERS],
            g[G_LABEL_CORNERS + 1],
            g[G_LABEL_CORNERS + 2],
            g[G_LABEL_CORNERS + 3],
        ];
        hx.draw_borders(w, bg, (&b, &k), hx.global(w), base_size(base), None);
    }
}
