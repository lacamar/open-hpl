//! `cWidgetMenuItem`, `cWidgetContextMenu` and `cWidgetMainMenu`: item state and drawing, the
//! context menu size and visibility rules, and the main menu bar. State is [`Item`], [`Ctx`] and
//! [`Main`], embedded in the C++ widgets (`gui/RustGuiH20.h`); the `iWidgetMenu` base class and
//! the item list live in `gw_menu`.

use super::gw_menu as gm;
use super::h19_widget::Base;
use super::h20_host as hh;
use super::h20_list::{attr_id, gfx_id};
use super::set::{Msg, W};
use super::widget::{add3, v2};
use super::widgets_menu as wm;
use crate::types::Vector2f;

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
}

#[repr(C)]
pub struct Main {
    pub text_padding: f32,
    pub v_padding: f32,
    pub separation: f32,
    pub gfx_bg: usize,
    pub this: W,
    pub base: *mut Base,
}

const _: () = assert!(std::mem::size_of::<Item>() == 96);
const _: () = assert!(std::mem::size_of::<Ctx>() == 128);
const _: () = assert!(std::mem::size_of::<Main>() == 40);

fn base_size(b: *mut Base) -> Vector2f {
    // SAFETY: `b` is the widget's base state or null.
    unsafe { b.as_ref() }.map_or(Vector2f { x: 0.0, y: 0.0 }, |b| b.size)
}

fn attention_blocks(w: W) -> bool {
    let att = hh::attention(w);
    att != 0 && !hh::is_connected(w, att, false)
}

// ---------------------------------------------------------------- item

/// # Safety
/// `i` is uninitialised storage.
pub unsafe fn item_init(i: *mut Item, this: W, base: *mut Base, kind: i32) {
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
        });
    }
}

/// # Safety
/// `i` is live.
pub unsafe fn item_on_init(i: *mut Item) {
    // SAFETY: caller contract.
    unsafe {
        let w = (*i).this;
        let parent = gm::parent_menu_of_item(w);
        let width = gm::item_width(parent);
        let size = wm::item_init_size(
            gm::is_separator(w),
            (*i).separator_height,
            (*i).v_padding,
            hh::skin_font_size(w).y,
            width,
        );
        hh::set_size(w, size);
    }
}

/// # Safety
/// `i` is live.
unsafe fn show_sub_menu(i: *mut Item) {
    // SAFETY: caller contract.
    unsafe {
        let child = (*i).child;
        if child == 0 {
            return;
        }
        let w = (*i).this;
        let parent = gm::parent_menu_of_item(w);
        let pg = hh::global(parent);
        let pos = (*i).base.as_ref().map(|b| b.pos).unwrap_or_default();
        let size = base_size((*i).base);
        let vx = hh::virtual_size(w)[0];
        let p = wm::item_submenu_pos(
            (*i).kind,
            pg,
            pos,
            size.y,
            hh::size(parent).x,
            hh::size(child).x,
            vx,
        );
        hh::show_context_menu(w, child, p);
    }
}

fn menu_open(i: &Item) -> bool {
    i.child != 0 && hh::is_visible(i.child)
}

/// # Safety
/// `i` is live.
pub unsafe fn item_on_update(i: *mut Item, dt: f32) {
    // SAFETY: caller contract.
    unsafe {
        let w = (*i).this;
        if !hh::is_visible(w) || attention_blocks(w) {
            return;
        }
        let parent = gm::parent_menu_of_item(w);
        let highlighted_self = gm::highlighted(parent) == w;
        let over = (*i).base.as_ref().is_some_and(|b| b.mouse_over);
        let is_parent = (*i).child != 0;
        let open = menu_open(&*i);
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
            show_sub_menu(i);
        }
    }
}

/// # Safety
/// `i` is live.
pub unsafe fn item_on_draw(i: *mut Item) {
    // SAFETY: caller contract.
    unsafe {
        let w = (*i).this;
        let parent = gm::parent_menu_of_item(w);
        let input = wm::ItemDrawIn {
            kind: (*i).kind,
            separator: gm::is_separator(w),
            highlighted: gm::highlighted(parent) == w,
            enabled: hh::is_enabled(w),
            widget_enabled: (*i).base.as_ref().is_some_and(|b| b.enabled),
            is_parent: (*i).child != 0,
            checked: (*i).checked,
            global: hh::global(w),
            size: base_size((*i).base),
            separator_padding: (*i).separator_padding,
            separator_height: (*i).separator_height,
            arrow_offset: gm::offset(parent, gm::Offset::Arrow),
            check_offset: gm::offset(parent, gm::Offset::Check),
            text_offset: gm::offset(parent, gm::Offset::Text),
        };
        let mut ops = [wm::MenuOp::default(); wm::MAX_ITEM_OPS];
        let n = wm::item_draw(&input, &mut ops).min(wm::MAX_ITEM_OPS);
        let gfx = (*i).gfx;
        for op in ops.iter().take(n) {
            if op.kind == wm::OP_GFX {
                let g = gfx.get(op.slot as usize).copied().unwrap_or(0);
                if op.has_size != 0 {
                    hh::draw_gfx(w, g, op.pos, op.size, None);
                } else {
                    hh::draw_gfx_native(w, g, op.pos);
                }
            } else {
                let text = if op.slot == wm::TEXT_SHORTCUT {
                    gm::shortcut(w)
                } else {
                    hh::text(w)
                };
                hh::draw_text_state(w, text, op.pos, op.align, op.color as i32);
            }
        }
    }
}

/// # Safety
/// `i` is live.
pub unsafe fn item_on_mouse_down(i: *mut Item) -> bool {
    // SAFETY: caller contract.
    unsafe {
        if gm::is_separator((*i).this) {
            return false;
        }
        (*i).timer = (*i).open_time;
        true
    }
}

/// # Safety
/// `i` is live.
pub unsafe fn item_on_mouse_up(i: *mut Item, d: &Msg) -> bool {
    // SAFETY: caller contract.
    unsafe {
        let w = (*i).this;
        let parent = gm::parent_menu_of_item(w);
        let highlighted_self = gm::highlighted(parent) == w;
        let up = wm::item_mouse_up(gm::is_separator(w), highlighted_self, (*i).child != 0);
        if !up.ret {
            return false;
        }
        if up.hide_topmost {
            let top = gm::top_most(parent);
            gm::hide(top);
        }
        hh::press(w, MSG_BUTTON_PRESSED, d);
        true
    }
}

/// # Safety
/// `i` is live.
pub unsafe fn item_on_mouse_move(i: *mut Item) -> bool {
    // SAFETY: caller contract.
    unsafe {
        let w = (*i).this;
        let blocks = attention_blocks(w);
        let draw_mouse = hh::draw_mouse(w);
        let over = (*i).base.as_ref().is_some_and(|b| b.mouse_over);
        if wm::item_mouse_move_highlights(hh::is_visible(w), draw_mouse, blocks, over) {
            let parent = gm::parent_menu_of_item(w);
            gm::set_highlight(parent, w);
        }
        true
    }
}

/// # Safety
/// `i` is live.
pub unsafe fn item_on_mouse_leave(i: *mut Item) -> bool {
    // SAFETY: caller contract.
    unsafe {
        let w = (*i).this;
        let blocks = attention_blocks(w);
        let open = menu_open(&*i);
        let leave = wm::item_mouse_leave(blocks, open);
        if !leave.ret {
            return false;
        }
        if leave.clear_highlight {
            let parent = gm::parent_menu_of_item(w);
            gm::set_highlight(parent, 0);
        }
        if leave.reset_timer {
            (*i).timer = 0.0;
        }
        true
    }
}

/// # Safety
/// `i` is live.
pub unsafe fn item_on_key_press(i: *mut Item, d: &Msg) -> bool {
    // SAFETY: caller contract.
    unsafe {
        let child = (*i).child;
        if child != 0 {
            return hh::process(child, MSG_KEY_PRESS, 0, Some(d));
        }
        false
    }
}

/// # Safety
/// `i` is live.
pub unsafe fn item_on_load_graphics(i: *mut Item) {
    // SAFETY: caller contract.
    unsafe {
        let w = (*i).this;
        let g = |n: &str| hh::skin_gfx(w, gfx_id(n));
        // slot order of `widgets_menu::GFX_*`: line, arrow, disabled check, enabled check, selection
        (*i).gfx = [
            g("MenuSeparatorLine"),
            g("MenuItemArrow"),
            g("MenuDisabledChecked"),
            g("MenuEnabledChecked"),
            g("TextBoxSelectedTextBack"),
        ];
        let at = |n: &str| hh::skin_attr(w, attr_id(n)).x;
        (*i).separator_padding = at("MenuSeparatorPadding");
        (*i).separator_height = at("MenuSeparatorHeight");
        (*i).v_padding = at("ContextMenuItemVPadding");
        hh::set_font_size(w, v2(12.5, 12.5));
    }
}

// ---------------------------------------------------------------- context menu

/// # Safety
/// `c` is uninitialised storage.
pub unsafe fn ctx_init(c: *mut Ctx, this: W, base: *mut Base) {
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
        });
    }
}

/// # Safety
/// `c` is live.
unsafe fn img(c: *mut Ctx, i: usize) -> Vector2f {
    // SAFETY: caller contract.
    hh::gfx_image_size(unsafe { (*c).gfx.get(1 + i).copied().unwrap_or(0) })
}

/// # Safety
/// `c` is live.
pub unsafe fn ctx_hide(c: *mut Ctx) {
    // SAFETY: caller contract.
    unsafe {
        let w = (*c).this;
        let has_parent = gm::parent_menu(w) != 0;
        let plan = wm::ctx_hide_plan(gm::submenu_open(w), has_parent);
        if plan.hide_child {
            let item = gm::highlighted(w);
            let child = (hh::state_ptr(item) as *mut Item)
                .as_ref()
                .map_or(0, |i| i.child);
            gm::hide(child);
        }
        gm::set_highlight(w, 0);
        if plan.notify_parent {
            let parent = gm::parent_menu(w);
            gm::child_menu_hide(parent);
        } else {
            let prev = gm::prev_attention(w);
            hh::set_attention(w, prev);
            hh::dec_context_z(w);
        }
        hh::set_visible(w, false);
        (*c).timer = 0.0;
    }
}

/// # Safety
/// `c` is live.
pub unsafe fn ctx_item_width(c: *mut Ctx) -> f32 {
    // SAFETY: caller contract.
    unsafe { wm::ctx_item_width(base_size((*c).base).x, img(c, 0).x, img(c, 1).x) }
}

/// # Safety
/// `c` is live.
pub unsafe fn ctx_update_items_pos(c: *mut Ctx, item: W) {
    // SAFETY: caller contract.
    unsafe {
        let n = gm::item_count((*c).this);
        let pos = wm::ctx_item_pos(base_size((*c).base).y, img(c, 2).y, img(c, 0).x, n);
        hh::set_position(item, pos);
    }
}

/// # Safety
/// `c` is live.
pub unsafe fn ctx_update_items_size(c: *mut Ctx, item: W) {
    // SAFETY: caller contract.
    unsafe {
        let w = (*c).this;
        ctx_update_width(c, gm::text_length(item));
        let mut sep = 0.0;
        let n = gm::item_count(w);
        if n > 1 {
            let prev = gm::item_at(w, n - 2);
            sep = wm::ctx_separation(
                gm::is_separator(prev),
                gm::is_separator(item),
                (*c).separation,
            );
        } else {
            let e = wm::ctx_empty_size(base_size((*c).base).x, img(c, 2).y, img(c, 3).y);
            hh::set_size(w, e);
        }
        let grown = wm::ctx_grown_size(base_size((*c).base), hh::size(item).y, sep);
        hh::set_size(w, grown);
    }
}

/// # Safety
/// `c` is live.
pub unsafe fn ctx_update_width(c: *mut Ctx, item_text: f32) {
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
            left_border_w: img(c, 0).x,
            right_border_w: img(c, 1).x,
        };
        let cur = ctx_item_width(c);
        let width = wm::ctx_update_width(&met, item_text, cur, base_size((*c).base).y);
        if width.resize_items {
            let n = gm::item_count(w);
            for k in 0..n {
                let it = gm::item_at(w, k);
                let y = hh::size(it).y;
                hh::set_size(it, v2(width.item_width, y));
            }
        }
        hh::set_size(w, width.menu_size);
        gm::set_offsets(
            w,
            Some(width.arrow_pos),
            Some(width.text_pos),
            Some(width.check_pos),
        );
    }
}

/// # Safety
/// `c` is live.
pub unsafe fn ctx_on_draw(c: *mut Ctx) {
    // SAFETY: caller contract.
    unsafe {
        let w = (*c).this;
        let g = (*c).gfx;
        let b = [g[1], g[2], g[3], g[4]];
        let k = [g[5], g[6], g[7], g[8]];
        hh::draw_borders(w, g[0], (&b, &k), hh::global(w), base_size((*c).base), None);
    }
}

/// # Safety
/// `c` is live.
pub unsafe fn ctx_on_update(c: *mut Ctx, dt: f32) {
    // SAFETY: caller contract.
    unsafe {
        let w = (*c).this;
        let parent_item = gm::parent_item(w);
        let not_highlighted = parent_item != 0 && {
            let parent_menu = gm::parent_menu(w);
            gm::highlighted(parent_menu) != parent_item
        };
        let visible = hh::is_visible(w);
        let u = wm::ctx_update((*c).timer, dt, (*c).hide_time, visible, not_highlighted);
        (*c).timer = u.timer;
        if u.hide_timer {
            ctx_hide(c);
        }
        if gm::must_hide(w) {
            ctx_hide(c);
            gm::set_must_hide(w, false);
        }
    }
}

/// # Safety
/// `c` is live.
pub unsafe fn ctx_on_lost_focus(c: *mut Ctx) -> bool {
    // SAFETY: caller contract.
    unsafe {
        let w = (*c).this;
        let focus = hh::focused(w);
        let connected_to_focus = focus != 0 && hh::is_connected(w, focus, true);
        let focus_connected = focus != 0 && hh::is_connected(focus, w, true);
        if wm::ctx_lost_focus_hides(focus == 0, connected_to_focus, focus_connected) {
            let top = gm::top_most(w);
            gm::set_must_hide(top, true);
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
pub unsafe fn ctx_on_key_press(c: *mut Ctx, d: &Msg) -> bool {
    // SAFETY: caller contract.
    unsafe {
        if wm::ctx_key_hides(d.key()) {
            ctx_hide(c);
            return true;
        }
        let w = (*c).this;
        let n = gm::item_count(w);
        for k in 0..n {
            let it = gm::item_at(w, k);
            if hh::process(it, MSG_KEY_PRESS, 0, Some(d)) {
                break;
            }
        }
        true
    }
}

/// # Safety
/// `c` is live.
pub unsafe fn ctx_on_load_graphics(c: *mut Ctx) {
    // SAFETY: caller contract.
    unsafe {
        let w = (*c).this;
        let g = |n: &str| hh::skin_gfx(w, gfx_id(n));
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
        let at = |n: &str| hh::skin_attr(w, attr_id(n)).x;
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
pub unsafe fn main_init(m: *mut Main, this: W, base: *mut Base) {
    // SAFETY: caller contract; the struct is written whole.
    unsafe {
        m.write(Main {
            text_padding: 0.0,
            v_padding: 0.0,
            separation: 0.0,
            gfx_bg: 0,
            this,
            base,
        });
    }
}

/// # Safety
/// `m` is live.
pub unsafe fn main_update_items_pos(m: *mut Main, item: W) {
    // SAFETY: caller contract.
    unsafe {
        let w = (*m).this;
        let n = gm::item_count(w);
        let prev = if n > 1 {
            let p = gm::item_at(w, n - 2);
            Some((hh::position(p).x, hh::size(p).x))
        } else {
            None
        };
        let pos = wm::main_item_pos((*m).v_padding, prev, (*m).separation);
        hh::set_position(item, pos);
    }
}

/// # Safety
/// `m` is live.
pub unsafe fn main_update_items_size(m: *mut Main, item: W) {
    // SAFETY: caller contract.
    unsafe {
        let w = (*m).this;
        let len = gm::text_length(item);
        let size = wm::main_item_size(
            len,
            (*m).text_padding,
            hh::skin_font_size(w).y,
            (*m).v_padding,
        );
        hh::set_size(item, size.size);
        gm::set_offsets(w, None, Some(size.text_pos), None);
    }
}

/// # Safety
/// `m` is live.
pub unsafe fn main_on_child_menu_hide(m: *mut Main) {
    // SAFETY: caller contract.
    unsafe {
        let w = (*m).this;
        let prev = gm::prev_attention(w);
        hh::set_attention(w, prev);
    }
}

/// # Safety
/// `m` is live.
pub unsafe fn main_hide(m: *mut Main) {
    // SAFETY: caller contract.
    unsafe {
        let w = (*m).this;
        if gm::submenu_open(w) {
            let item = gm::highlighted(w);
            let child = (hh::state_ptr(item) as *mut Item)
                .as_ref()
                .map_or(0, |i| i.child);
            gm::hide(child);
            gm::set_highlight(w, 0);
        }
        gm::set_must_hide(w, false);
    }
}

/// # Safety
/// `m` is live.
pub unsafe fn main_on_draw(m: *mut Main) {
    // SAFETY: caller contract.
    unsafe {
        let w = (*m).this;
        let s = base_size((*m).base);
        hh::draw_gfx(
            w,
            (*m).gfx_bg,
            hh::global(w),
            v2(s.x - 0.0, s.y - 1.0),
            None,
        );
    }
}

/// # Safety
/// `m` is live.
pub unsafe fn main_on_update(m: *mut Main) {
    // SAFETY: caller contract.
    unsafe {
        let w = (*m).this;
        if gm::must_hide(w) {
            main_hide(m);
            gm::set_must_hide(w, false);
        }
    }
}

/// # Safety
/// `m` is live.
pub unsafe fn main_on_init(m: *mut Main) {
    // SAFETY: caller contract.
    unsafe {
        let w = (*m).this;
        let parent = (*m).base.as_ref().map_or(0, |b| b.parent);
        let ty = hh::widget_type(parent);
        let kind = if ty == hh::TYPE_ROOT {
            wm::PARENT_ROOT
        } else if ty == hh::TYPE_WINDOW {
            wm::PARENT_WINDOW
        } else {
            wm::PARENT_OTHER
        };
        let vx = hh::virtual_size(w)[0];
        let init = wm::main_init(kind, vx, hh::size(parent).x, hh::skin_font_size(w).y);
        hh::set_size(w, init.size);
        hh::set_children_offset(parent, init.children_offset);
        let p = add3(hh::position(w), init.pos_delta);
        hh::set_position(w, p);
    }
}

/// # Safety
/// `m` is live.
pub unsafe fn main_on_load_graphics(m: *mut Main) {
    // SAFETY: caller contract.
    unsafe {
        let w = (*m).this;
        (*m).gfx_bg = hh::skin_gfx(w, gfx_id("ButtonUpBackground"));
        let at = |n: &str| hh::skin_attr(w, attr_id(n)).x;
        (*m).text_padding = at("MainMenuItemTextPadding");
        (*m).v_padding = at("MainMenuItemVPadding");
        (*m).separation = at("MainMenuItemSeparation");
    }
}
