//! `cWidgetMenuItem`, `cWidgetContextMenu` and `cWidgetMainMenu`: item state and drawing, the
//! context menu size and visibility rules, and the main menu bar. State is [`Item`], [`Ctx`] and
//! [`Main`], embedded in the C++ widgets (`gui/RustGuiH20.h`); the `iWidgetMenu` base class and
//! the item list stay in C++ and are reached through the `MENU_CALL` host op.

use super::h19_widget::Base;
use super::h20_host::{self as hh, HostVt, Hx};
use super::h20_list::{attr_id, gfx_id};
use super::set::{Msg, W};
use super::widget::{add3, v2};
use super::widgets_menu as wm;
use crate::math::v3;
use crate::types::{Vector2f, Vector3f};

const MSG_KEY_PRESS: i32 = 14;
const MSG_BUTTON_PRESSED: i32 = 11;
pub const KIND_ITEM: i32 = 3;
pub const KIND_CTX: i32 = 4;

#[repr(C)]
pub struct Item {
    pub child: W,
    pub gfx: [usize; 5],
    pub timer: f32,
    pub open_time: f32,
    pub separator_padding: f32,
    pub separator_height: f32,
    pub v_padding: f32,
    pub checked: bool,
    pub default: bool,
    pub kind: i32,
    pub this: W,
    pub base: *mut Base,
    pub host: *const HostVt,
}

#[repr(C)]
pub struct Ctx {
    pub timer: f32,
    pub hide_time: f32,
    pub h_padding: f32,
    pub v_padding: f32,
    pub separation: f32,
    pub arrow_icon: f32,
    pub check_icon: f32,
    pub text_left: f32,
    pub text_right: f32,
    pub gfx: [usize; 9],
    pub this: W,
    pub base: *mut Base,
    pub host: *const HostVt,
}

#[repr(C)]
pub struct Main {
    pub text_padding: f32,
    pub v_padding: f32,
    pub separation: f32,
    pub gfx_bg: usize,
    pub this: W,
    pub base: *mut Base,
    pub host: *const HostVt,
}

const _: () = assert!(std::mem::size_of::<Item>() == 104);
const _: () = assert!(std::mem::size_of::<Ctx>() == 136);
const _: () = assert!(std::mem::size_of::<Main>() == 48);

fn menu_call(hx: Hx, w: W, code: i32, f: impl FnOnce(&mut hh::Args)) -> (hh::Args, usize) {
    let mut a = hh::Args::new(w);
    a.i[0] = code;
    f(&mut a);
    let v = hx.raw(hh::MENU_CALL, &mut a);
    (a, v)
}

fn mc(hx: Hx, w: W, code: i32) -> usize {
    menu_call(hx, w, code, |_| ()).1
}

fn mc_vec(hx: Hx, w: W, code: i32) -> Vector3f {
    let (a, _) = menu_call(hx, w, code, |_| ());
    v3(a.rf[0], a.rf[1], a.rf[2])
}

fn state_of<T>(hx: Hx, w: W, kind: i32) -> *mut T {
    let mut a = hh::Args::new(w);
    a.i[0] = kind;
    hx.raw(hh::STATE_PTR, &mut a) as *mut T
}

fn base_size(b: *mut Base) -> Vector2f {
    // SAFETY: `b` is the widget's base state or null.
    unsafe { b.as_ref() }.map_or(Vector2f { x: 0.0, y: 0.0 }, |b| b.size)
}

fn connected(hx: Hx, w: W, other: W, explicit_false: bool) -> bool {
    let mut a = hh::Args::new(w);
    a.p[0] = other;
    a.i[0] = i32::from(explicit_false);
    hx.raw(hh::IS_CONNECTED, &mut a) != 0
}

fn attention_blocks(hx: Hx, w: W) -> bool {
    let att = hx.set_call(w, hh::SC_GET_ATTENTION, 0).0;
    att != 0 && !connected(hx, w, att, false)
}

fn skin_font_y(hx: Hx, w: W) -> f32 {
    let mut a = hh::Args::new(w);
    hx.raw(hh::SKIN_FONT_SIZE, &mut a);
    a.rf[1]
}

fn is_separator(hx: Hx, w: W) -> bool {
    hx.raw(hh::ITEM_IS_SEPARATOR, &mut hh::Args::new(w)) != 0
}

fn text_length(hx: Hx, w: W) -> f32 {
    let mut a = hh::Args::new(w);
    hx.raw(hh::ITEM_TEXT_LENGTH, &mut a);
    a.rf[0]
}

// ---------------------------------------------------------------- item

/// # Safety
/// `i` is uninitialised storage.
pub unsafe fn item_init(i: *mut Item, vt: *const HostVt, this: W, base: *mut Base, kind: i32) {
    // SAFETY: caller contract; the struct is written whole.
    unsafe {
        i.write(Item {
            child: 0,
            gfx: [0; 5],
            timer: 0.0,
            open_time: 0.5,
            separator_padding: 0.0,
            separator_height: 0.0,
            v_padding: 0.0,
            checked: false,
            default: false,
            kind,
            this,
            base,
            host: vt,
        });
    }
}

/// # Safety
/// `i` is live.
pub unsafe fn item_on_init(hx: Hx, i: *mut Item) {
    // SAFETY: caller contract.
    unsafe {
        let w = (*i).this;
        let parent = mc(hx, w, hh::MC_ITEM_PARENT_MENU);
        let width = menu_call(hx, parent, hh::MC_ITEM_WIDTH, |_| ()).0.rf[0];
        let size = wm::item_init_size(
            is_separator(hx, w),
            (*i).separator_height,
            (*i).v_padding,
            skin_font_y(hx, w),
            width,
        );
        hx.set_size(w, size);
    }
}

/// # Safety
/// `i` is live.
unsafe fn show_sub_menu(hx: Hx, i: *mut Item) {
    // SAFETY: caller contract.
    unsafe {
        let child = (*i).child;
        if child == 0 {
            return;
        }
        let w = (*i).this;
        let parent = mc(hx, w, hh::MC_ITEM_PARENT_MENU);
        let pg = hx.global(parent);
        let pos = (*i).base.as_ref().map(|b| b.pos).unwrap_or_default();
        let size = base_size((*i).base);
        let vx = hx.set_call(w, hh::SC_VIRTUAL_SIZE, 0).1[0];
        let p = wm::item_submenu_pos(
            (*i).kind,
            pg,
            pos,
            size.y,
            hx.size(parent).x,
            hx.size(child).x,
            vx,
        );
        let mut a = hh::Args::new(w);
        a.i[0] = hh::SC_SHOW_CONTEXT_MENU;
        a.p[0] = child;
        a.f[..3].copy_from_slice(&[p.x, p.y, p.z]);
        hx.raw(hh::SET_CALL, &mut a);
    }
}

fn menu_open(hx: Hx, i: &Item) -> bool {
    i.child != 0 && hx.flag(hh::WIDGET_IS_VISIBLE, i.child)
}

/// # Safety
/// `i` is live.
pub unsafe fn item_on_update(hx: Hx, i: *mut Item, dt: f32) {
    // SAFETY: caller contract.
    unsafe {
        let w = (*i).this;
        if !hx.flag(hh::WIDGET_IS_VISIBLE, w) || attention_blocks(hx, w) {
            return;
        }
        let parent = mc(hx, w, hh::MC_ITEM_PARENT_MENU);
        let highlighted_self = menu_call(hx, parent, hh::MC_HIGHLIGHTED, |_| ()).1 == w;
        let over = (*i).base.as_ref().is_some_and(|b| b.mouse_over);
        let is_parent = (*i).child != 0;
        let open = menu_open(hx, &*i);
        let u = wm::item_update(
            (*i).timer,
            dt,
            (*i).open_time,
            over,
            (*i).kind,
            highlighted_self,
            is_parent,
            open,
        );
        (*i).timer = u.timer;
        if u.show_submenu {
            show_sub_menu(hx, i);
        }
    }
}

/// # Safety
/// `i` is live.
pub unsafe fn item_on_draw(hx: Hx, i: *mut Item) {
    // SAFETY: caller contract.
    unsafe {
        let w = (*i).this;
        let parent = mc(hx, w, hh::MC_ITEM_PARENT_MENU);
        let input = wm::ItemDrawIn {
            kind: (*i).kind,
            separator: is_separator(hx, w),
            highlighted: menu_call(hx, parent, hh::MC_HIGHLIGHTED, |_| ()).1 == w,
            enabled: hx.flag(hh::WIDGET_IS_ENABLED, w),
            widget_enabled: (*i).base.as_ref().is_some_and(|b| b.enabled),
            is_parent: (*i).child != 0,
            checked: (*i).checked,
            global: hx.global(w),
            size: base_size((*i).base),
            separator_padding: (*i).separator_padding,
            separator_height: (*i).separator_height,
            arrow_offset: mc_vec(hx, parent, hh::MC_ARROW_OFFSET),
            check_offset: mc_vec(hx, parent, hh::MC_CHECK_OFFSET),
            text_offset: mc_vec(hx, parent, hh::MC_TEXT_OFFSET),
        };
        let mut ops = [wm::MenuOp::default(); wm::MAX_ITEM_OPS];
        let n = wm::item_draw(&input, &mut ops).min(wm::MAX_ITEM_OPS);
        let gfx = (*i).gfx;
        for op in ops.iter().take(n) {
            if op.kind == wm::OP_GFX {
                let g = gfx.get(op.slot as usize).copied().unwrap_or(0);
                if op.has_size != 0 {
                    hx.draw_gfx(w, g, op.pos, op.size, None);
                } else {
                    let mut a = hh::Args::new(w);
                    a.p[0] = g;
                    a.f[..3].copy_from_slice(&[op.pos.x, op.pos.y, op.pos.z]);
                    a.i[1] = 1;
                    hx.raw(hh::DRAW_GFX, &mut a);
                }
            } else {
                let text = if op.slot == wm::TEXT_SHORTCUT {
                    let mut a = hh::Args::new(w);
                    let p = hx.raw(hh::WIDGET_SHORTCUT_TEXT, &mut a) as *const u32;
                    (p, usize::try_from(a.ri[0]).unwrap_or(0))
                } else {
                    hx.text(w)
                };
                let mut a = hh::Args::new(w);
                a.text = text.0;
                a.text_len = text.1;
                a.f[..3].copy_from_slice(&[op.pos.x, op.pos.y, op.pos.z]);
                a.i[0] = op.align;
                a.i[2] = 2;
                a.i[3] = op.color as i32;
                hx.raw(hh::DRAW_TEXT, &mut a);
            }
        }
    }
}

/// # Safety
/// `i` is live.
pub unsafe fn item_on_mouse_down(hx: Hx, i: *mut Item) -> bool {
    // SAFETY: caller contract.
    unsafe {
        if is_separator(hx, (*i).this) {
            return false;
        }
        (*i).timer = (*i).open_time;
        true
    }
}

/// # Safety
/// `i` is live.
pub unsafe fn item_on_mouse_up(hx: Hx, i: *mut Item, d: &Msg) -> bool {
    // SAFETY: caller contract.
    unsafe {
        let w = (*i).this;
        let parent = mc(hx, w, hh::MC_ITEM_PARENT_MENU);
        let highlighted_self = menu_call(hx, parent, hh::MC_HIGHLIGHTED, |_| ()).1 == w;
        let up = wm::item_mouse_up(is_separator(hx, w), highlighted_self, (*i).child != 0);
        if !up.ret {
            return false;
        }
        if up.hide_topmost {
            let top = mc(hx, parent, hh::MC_TOP_MOST);
            menu_call(hx, top, hh::MC_HIDE, |_| ());
        }
        let mut a = hh::Args::new(w);
        a.i[0] = MSG_BUTTON_PRESSED;
        a.i[2] = 1;
        a.i[3] = 1;
        a.p[0] = std::ptr::from_ref(d) as usize;
        hx.raw(hh::PROCESS_MESSAGE, &mut a);
        true
    }
}

/// # Safety
/// `i` is live.
pub unsafe fn item_on_mouse_move(hx: Hx, i: *mut Item) -> bool {
    // SAFETY: caller contract.
    unsafe {
        let w = (*i).this;
        let blocks = attention_blocks(hx, w);
        let draw_mouse = hx.set_call(w, hh::SC_GET_DRAW_MOUSE, 0).0 != 0;
        let over = (*i).base.as_ref().is_some_and(|b| b.mouse_over);
        if wm::item_mouse_move_highlights(
            hx.flag(hh::WIDGET_IS_VISIBLE, w),
            draw_mouse,
            blocks,
            over,
        ) {
            let parent = mc(hx, w, hh::MC_ITEM_PARENT_MENU);
            menu_call(hx, parent, hh::MC_SET_HIGHLIGHTED, |a| a.p[0] = w);
        }
        true
    }
}

/// # Safety
/// `i` is live.
pub unsafe fn item_on_mouse_leave(hx: Hx, i: *mut Item) -> bool {
    // SAFETY: caller contract.
    unsafe {
        let w = (*i).this;
        let blocks = attention_blocks(hx, w);
        let open = menu_open(hx, &*i);
        let leave = wm::item_mouse_leave(blocks, open);
        if !leave.ret {
            return false;
        }
        if leave.clear_highlight {
            let parent = mc(hx, w, hh::MC_ITEM_PARENT_MENU);
            menu_call(hx, parent, hh::MC_SET_HIGHLIGHTED, |a| a.p[0] = 0);
        }
        if leave.reset_timer {
            (*i).timer = 0.0;
        }
        true
    }
}

/// # Safety
/// `i` is live.
pub unsafe fn item_on_key_press(hx: Hx, i: *mut Item, d: &Msg) -> bool {
    // SAFETY: caller contract.
    unsafe {
        let child = (*i).child;
        if child != 0 {
            return hx.process(child, MSG_KEY_PRESS, 0, Some(d));
        }
        false
    }
}

/// # Safety
/// `i` is live.
pub unsafe fn item_on_load_graphics(hx: Hx, i: *mut Item) {
    // SAFETY: caller contract.
    unsafe {
        let w = (*i).this;
        let g = |n: &str| hx.skin_gfx(w, gfx_id(n));
        // slot order of `widgets_menu::GFX_*`: line, arrow, disabled check, enabled check, selection
        (*i).gfx = [
            g("MenuSeparatorLine"),
            g("MenuItemArrow"),
            g("MenuDisabledChecked"),
            g("MenuEnabledChecked"),
            g("TextBoxSelectedTextBack"),
        ];
        let at = |n: &str| hx.skin_attr(w, attr_id(n)).x;
        (*i).separator_padding = at("MenuSeparatorPadding");
        (*i).separator_height = at("MenuSeparatorHeight");
        (*i).v_padding = at("ContextMenuItemVPadding");
        let mut a = hh::Args::new(w);
        a.f[0] = 12.5;
        a.f[1] = 12.5;
        hx.raw(hh::WIDGET_SET_FONT_SIZE, &mut a);
    }
}

// ---------------------------------------------------------------- context menu

/// # Safety
/// `c` is uninitialised storage.
pub unsafe fn ctx_init(c: *mut Ctx, vt: *const HostVt, this: W, base: *mut Base) {
    // SAFETY: caller contract; the struct is written whole.
    unsafe {
        c.write(Ctx {
            timer: 0.0,
            hide_time: 0.0,
            h_padding: 0.0,
            v_padding: 0.0,
            separation: 0.0,
            arrow_icon: 0.0,
            check_icon: 0.0,
            text_left: 0.0,
            text_right: 0.0,
            gfx: [0; 9],
            this,
            base,
            host: vt,
        });
    }
}

/// # Safety
/// `c` is live.
unsafe fn img(hx: Hx, c: *mut Ctx, i: usize) -> Vector2f {
    // SAFETY: caller contract.
    hx.gfx_size(hh::GFX_IMAGE_SIZE, unsafe {
        (*c).gfx.get(1 + i).copied().unwrap_or(0)
    })
}

/// # Safety
/// `c` is live.
pub unsafe fn ctx_hide(hx: Hx, c: *mut Ctx) {
    // SAFETY: caller contract.
    unsafe {
        let w = (*c).this;
        let has_parent = mc(hx, w, hh::MC_PARENT_MENU) != 0;
        let plan = wm::ctx_hide_plan(mc(hx, w, hh::MC_IS_SUBMENU_OPEN) != 0, has_parent);
        if plan.hide_child {
            let item = menu_call(hx, w, hh::MC_HIGHLIGHTED, |_| ()).1;
            let child = state_of::<Item>(hx, item, KIND_ITEM)
                .as_ref()
                .map_or(0, |i| i.child);
            menu_call(hx, child, hh::MC_HIDE, |_| ());
        }
        menu_call(hx, w, hh::MC_SET_HIGHLIGHTED, |a| a.p[0] = 0);
        if plan.notify_parent {
            let parent = mc(hx, w, hh::MC_PARENT_MENU);
            menu_call(hx, parent, hh::MC_ON_CHILD_HIDE, |_| ());
        } else {
            let prev = mc(hx, w, hh::MC_PREV_ATTENTION);
            hx.set_call(w, hh::SC_SET_ATTENTION, prev);
            hx.set_call(w, hh::SC_DEC_CONTEXT_Z, 0);
        }
        hx.set_flag(hh::WIDGET_SET_VISIBLE, w, false);
        (*c).timer = 0.0;
    }
}

/// # Safety
/// `c` is live.
pub unsafe fn ctx_item_width(hx: Hx, c: *mut Ctx) -> f32 {
    // SAFETY: caller contract.
    unsafe { wm::ctx_item_width(base_size((*c).base).x, img(hx, c, 0).x, img(hx, c, 1).x) }
}

/// # Safety
/// `c` is live.
pub unsafe fn ctx_update_items_pos(hx: Hx, c: *mut Ctx, item: W) {
    // SAFETY: caller contract.
    unsafe {
        let n = mc(hx, (*c).this, hh::MC_ITEM_COUNT);
        let pos = wm::ctx_item_pos(base_size((*c).base).y, img(hx, c, 2).y, img(hx, c, 0).x, n);
        hx.set_position(item, pos);
    }
}

/// # Safety
/// `c` is live.
pub unsafe fn ctx_update_items_size(hx: Hx, c: *mut Ctx, item: W) {
    // SAFETY: caller contract.
    unsafe {
        let w = (*c).this;
        ctx_update_width(hx, c, text_length(hx, item));
        let mut sep = 0.0;
        let n = mc(hx, w, hh::MC_ITEM_COUNT);
        if n > 1 {
            let prev = menu_call(hx, w, hh::MC_ITEM_AT, |a| a.i[1] = n as i32 - 2).1;
            sep = wm::ctx_separation(
                is_separator(hx, prev),
                is_separator(hx, item),
                (*c).separation,
            );
        } else {
            let e = wm::ctx_empty_size(base_size((*c).base).x, img(hx, c, 2).y, img(hx, c, 3).y);
            hx.set_size(w, e);
        }
        let grown = wm::ctx_grown_size(base_size((*c).base), hx.size(item).y, sep);
        hx.set_size(w, grown);
    }
}

/// # Safety
/// `c` is live.
pub unsafe fn ctx_update_width(hx: Hx, c: *mut Ctx, item_text: f32) {
    // SAFETY: caller contract.
    unsafe {
        let w = (*c).this;
        let met = wm::CtxMetrics {
            h_padding: (*c).h_padding,
            check_icon: (*c).check_icon,
            text_left_padding: (*c).text_left,
            text_right_padding: (*c).text_right,
            arrow_icon: (*c).arrow_icon,
            v_padding: (*c).v_padding,
            left_border_w: img(hx, c, 0).x,
            right_border_w: img(hx, c, 1).x,
        };
        let cur = ctx_item_width(hx, c);
        let width = wm::ctx_update_width(&met, item_text, cur, base_size((*c).base).y);
        if width.resize_items {
            let n = mc(hx, w, hh::MC_ITEM_COUNT);
            for k in 0..n {
                let it = menu_call(hx, w, hh::MC_ITEM_AT, |a| a.i[1] = k as i32).1;
                let y = hx.size(it).y;
                hx.set_size(it, v2(width.item_width, y));
            }
        }
        hx.set_size(w, width.menu_size);
        menu_call(hx, w, hh::MC_SET_OFFSETS, |a| {
            a.i[1] = 7;
            a.f[..3].copy_from_slice(&[width.arrow_pos.x, width.arrow_pos.y, width.arrow_pos.z]);
            a.f[3..6].copy_from_slice(&[width.text_pos.x, width.text_pos.y, width.text_pos.z]);
            a.f[6..9].copy_from_slice(&[width.check_pos.x, width.check_pos.y, width.check_pos.z]);
        });
    }
}

/// # Safety
/// `c` is live.
pub unsafe fn ctx_on_draw(hx: Hx, c: *mut Ctx) {
    // SAFETY: caller contract.
    unsafe {
        let w = (*c).this;
        let g = (*c).gfx;
        let b = [g[1], g[2], g[3], g[4]];
        let k = [g[5], g[6], g[7], g[8]];
        hx.draw_borders(w, g[0], (&b, &k), hx.global(w), base_size((*c).base), None);
    }
}

/// # Safety
/// `c` is live.
pub unsafe fn ctx_on_update(hx: Hx, c: *mut Ctx, dt: f32) {
    // SAFETY: caller contract.
    unsafe {
        let w = (*c).this;
        let parent_item = mc(hx, w, hh::MC_PARENT_ITEM);
        let not_highlighted = parent_item != 0 && {
            let parent_menu = mc(hx, w, hh::MC_PARENT_MENU);
            menu_call(hx, parent_menu, hh::MC_HIGHLIGHTED, |_| ()).1 != parent_item
        };
        let visible = hx.flag(hh::WIDGET_IS_VISIBLE, w);
        let u = wm::ctx_update((*c).timer, dt, (*c).hide_time, visible, not_highlighted);
        (*c).timer = u.timer;
        if u.hide_timer {
            ctx_hide(hx, c);
        }
        if mc(hx, w, hh::MC_GET_MUST_HIDE) != 0 {
            ctx_hide(hx, c);
            menu_call(hx, w, hh::MC_SET_MUST_HIDE, |a| a.i[1] = 0);
        }
    }
}

/// # Safety
/// `c` is live.
pub unsafe fn ctx_on_lost_focus(hx: Hx, c: *mut Ctx) -> bool {
    // SAFETY: caller contract.
    unsafe {
        let w = (*c).this;
        let focus = hx.set_call(w, hh::SC_GET_FOCUSED, 0).0;
        let connected_to_focus = focus != 0 && connected(hx, w, focus, true);
        let focus_connected = focus != 0 && connected(hx, focus, w, true);
        if wm::ctx_lost_focus_hides(focus == 0, connected_to_focus, focus_connected) {
            let top = mc(hx, w, hh::MC_TOP_MOST);
            menu_call(hx, top, hh::MC_SET_MUST_HIDE, |a| a.i[1] = 1);
        }
        true
    }
}

/// # Safety
/// `c` is live.
pub unsafe fn ctx_on_got_focus(c: *mut Ctx) -> bool {
    // SAFETY: caller contract.
    unsafe { (*c).timer = 0.0 };
    true
}

/// # Safety
/// `c` is live.
pub unsafe fn ctx_on_key_press(hx: Hx, c: *mut Ctx, d: &Msg) -> bool {
    // SAFETY: caller contract.
    unsafe {
        if wm::ctx_key_hides(d.key()) {
            ctx_hide(hx, c);
            return true;
        }
        let w = (*c).this;
        let n = mc(hx, w, hh::MC_ITEM_COUNT);
        for k in 0..n {
            let it = menu_call(hx, w, hh::MC_ITEM_AT, |a| a.i[1] = k as i32).1;
            if hx.process(it, MSG_KEY_PRESS, 0, Some(d)) {
                break;
            }
        }
        true
    }
}

/// # Safety
/// `c` is live.
pub unsafe fn ctx_on_load_graphics(hx: Hx, c: *mut Ctx) {
    // SAFETY: caller contract.
    unsafe {
        let w = (*c).this;
        let g = |n: &str| hx.skin_gfx(w, gfx_id(n));
        (*c).gfx = [
            g("ButtonUpBackground"),
            g("ComboBoxBorderRight"),
            g("ComboBoxBorderLeft"),
            g("ComboBoxBorderUp"),
            g("ComboBoxBorderDown"),
            g("ComboBoxCornerLU"),
            g("ComboBoxCornerRU"),
            g("ComboBoxCornerRD"),
            g("ComboBoxCornerLD"),
        ];
        let at = |n: &str| hx.skin_attr(w, attr_id(n)).x;
        (*c).h_padding = at("ContextMenuItemHPadding");
        (*c).v_padding = at("ContextMenuItemVPadding");
        (*c).separation = at("ContextMenuItemSeparation");
        (*c).arrow_icon = at("ContextMenuItemArrowSize");
        (*c).check_icon = at("ContextMenuItemCheckSize");
        (*c).text_left = at("ContextMenuItemTextLeftPadding");
        (*c).text_right = at("ContextMenuItemTextRightPadding");
    }
}

// ---------------------------------------------------------------- main menu

/// # Safety
/// `m` is uninitialised storage.
pub unsafe fn main_init(m: *mut Main, vt: *const HostVt, this: W, base: *mut Base) {
    // SAFETY: caller contract; the struct is written whole.
    unsafe {
        m.write(Main {
            text_padding: 0.0,
            v_padding: 0.0,
            separation: 0.0,
            gfx_bg: 0,
            this,
            base,
            host: vt,
        });
    }
}

/// # Safety
/// `m` is live.
pub unsafe fn main_update_items_pos(hx: Hx, m: *mut Main, item: W) {
    // SAFETY: caller contract.
    unsafe {
        let w = (*m).this;
        let n = mc(hx, w, hh::MC_ITEM_COUNT);
        let prev = if n > 1 {
            let p = menu_call(hx, w, hh::MC_ITEM_AT, |a| a.i[1] = n as i32 - 2).1;
            let mut a = hh::Args::new(p);
            hx.raw(hh::WIDGET_GET_POSITION, &mut a);
            Some((a.rf[0], hx.size(p).x))
        } else {
            None
        };
        let pos = wm::main_item_pos((*m).v_padding, prev, (*m).separation);
        hx.set_position(item, pos);
    }
}

/// # Safety
/// `m` is live.
pub unsafe fn main_update_items_size(hx: Hx, m: *mut Main, item: W) {
    // SAFETY: caller contract.
    unsafe {
        let w = (*m).this;
        let len = text_length(hx, item);
        let size = wm::main_item_size(len, (*m).text_padding, skin_font_y(hx, w), (*m).v_padding);
        hx.set_size(item, size.size);
        menu_call(hx, w, hh::MC_SET_OFFSETS, |a| {
            a.i[1] = 2;
            a.f[3..6].copy_from_slice(&[size.text_pos.x, size.text_pos.y, size.text_pos.z]);
        });
    }
}

/// # Safety
/// `m` is live.
pub unsafe fn main_on_child_menu_hide(hx: Hx, m: *mut Main) {
    // SAFETY: caller contract.
    unsafe {
        let w = (*m).this;
        let prev = mc(hx, w, hh::MC_PREV_ATTENTION);
        hx.set_call(w, hh::SC_SET_ATTENTION, prev);
    }
}

/// # Safety
/// `m` is live.
pub unsafe fn main_hide(hx: Hx, m: *mut Main) {
    // SAFETY: caller contract.
    unsafe {
        let w = (*m).this;
        if mc(hx, w, hh::MC_IS_SUBMENU_OPEN) != 0 {
            let item = menu_call(hx, w, hh::MC_HIGHLIGHTED, |_| ()).1;
            let child = state_of::<Item>(hx, item, KIND_ITEM)
                .as_ref()
                .map_or(0, |i| i.child);
            menu_call(hx, child, hh::MC_HIDE, |_| ());
            menu_call(hx, w, hh::MC_SET_HIGHLIGHTED, |a| a.p[0] = 0);
        }
        menu_call(hx, w, hh::MC_SET_MUST_HIDE, |a| a.i[1] = 0);
    }
}

/// # Safety
/// `m` is live.
pub unsafe fn main_on_draw(hx: Hx, m: *mut Main) {
    // SAFETY: caller contract.
    unsafe {
        let w = (*m).this;
        let s = base_size((*m).base);
        hx.draw_gfx(w, (*m).gfx_bg, hx.global(w), v2(s.x - 0.0, s.y - 1.0), None);
    }
}

/// # Safety
/// `m` is live.
pub unsafe fn main_on_update(hx: Hx, m: *mut Main) {
    // SAFETY: caller contract.
    unsafe {
        let w = (*m).this;
        if mc(hx, w, hh::MC_GET_MUST_HIDE) != 0 {
            main_hide(hx, m);
            menu_call(hx, w, hh::MC_SET_MUST_HIDE, |a| a.i[1] = 0);
        }
    }
}

/// # Safety
/// `m` is live.
pub unsafe fn main_on_init(hx: Hx, m: *mut Main) {
    // SAFETY: caller contract.
    unsafe {
        let w = (*m).this;
        let parent = (*m).base.as_ref().map_or(0, |b| b.parent);
        let mut a = hh::Args::new(parent);
        let ty = hx.raw(hh::WIDGET_TYPE, &mut a) as i32;
        let kind = if ty == hh::TYPE_ROOT {
            wm::PARENT_ROOT
        } else if ty == hh::TYPE_WINDOW {
            wm::PARENT_WINDOW
        } else {
            wm::PARENT_OTHER
        };
        let vx = hx.set_call(w, hh::SC_VIRTUAL_SIZE, 0).1[0];
        let init = wm::main_init(kind, vx, hx.size(parent).x, skin_font_y(hx, w));
        hx.set_size(w, init.size);
        let mut a = hh::Args::new(parent);
        let o = init.children_offset;
        a.f[..3].copy_from_slice(&[o.x, o.y, o.z]);
        hx.raw(hh::WIDGET_SET_CHILDREN_OFFSET, &mut a);
        let mut a = hh::Args::new(w);
        hx.raw(hh::WIDGET_GET_POSITION, &mut a);
        let p = add3(v3(a.rf[0], a.rf[1], a.rf[2]), init.pos_delta);
        hx.set_position(w, p);
    }
}

/// # Safety
/// `m` is live.
pub unsafe fn main_on_load_graphics(hx: Hx, m: *mut Main) {
    // SAFETY: caller contract.
    unsafe {
        let w = (*m).this;
        (*m).gfx_bg = hx.skin_gfx(w, gfx_id("ButtonUpBackground"));
        let at = |n: &str| hx.skin_attr(w, attr_id(n)).x;
        (*m).text_padding = at("MainMenuItemTextPadding");
        (*m).v_padding = at("MainMenuItemVPadding");
        (*m).separation = at("MainMenuItemSeparation");
    }
}
