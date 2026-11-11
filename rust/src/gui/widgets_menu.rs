//! `cWidgetMenuItem`, `iWidgetMenu`, `cWidgetMainMenu` and `cWidgetContextMenu` logic: item layout,
//! submenu placement, open/hide decisions and the item draw list. Widgets, the `cGuiSet` calls and
//! every virtual call stay in C++, which applies the returned decisions in the original order.

use super::widget::{add3, v2};
use crate::math::v3;
use crate::types::{Vector2f, Vector3f};

pub const KIND_OTHER: i32 = 0;
pub const KIND_CONTEXT: i32 = 1;
pub const KIND_MAIN: i32 = 2;

pub const PARENT_ROOT: i32 = 0;
pub const PARENT_WINDOW: i32 = 1;
pub const PARENT_OTHER: i32 = 2;

pub const OP_GFX: u32 = 0;
pub const OP_TEXT: u32 = 1;

pub const GFX_LINE: u32 = 0;
pub const GFX_ARROW: u32 = 1;
pub const GFX_CHECKED_DISABLED: u32 = 2;
pub const GFX_CHECKED_ENABLED: u32 = 3;
pub const GFX_SELECTION: u32 = 4;

pub const TEXT_SHORTCUT: u32 = 0;
pub const TEXT_LABEL: u32 = 1;

pub const COLOR_DISABLED: u32 = 0;
pub const COLOR_HIGHLIGHT: u32 = 1;
pub const COLOR_DEFAULT: u32 = 2;

pub const ALIGN_LEFT: i32 = 0;
pub const ALIGN_RIGHT: i32 = 1;

pub const KEY_ESCAPE: i32 = super::widget::KEY_ESCAPE;

pub const MAX_ITEM_OPS: usize = 4;

/// One draw call of a menu item: `OP_GFX` draws gfx slot `slot` at `pos` (with `size` when
/// `has_size`); `OP_TEXT` draws text `slot` with colour `color` and font alignment `align`.
#[repr(C)]
#[derive(Clone, Copy, Default, Debug, PartialEq)]
pub struct MenuOp {
    pub kind: u32,
    pub slot: u32,
    pub has_size: u32,
    pub color: u32,
    pub align: i32,
    pub pos: Vector3f,
    pub size: Vector2f,
}

const _: () = assert!(std::mem::size_of::<MenuOp>() == 5 * 4 + 12 + 8);

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct ItemDrawIn {
    pub kind: i32,
    pub separator: bool,
    pub highlighted: bool,
    pub enabled: bool,
    pub widget_enabled: bool,
    pub is_parent: bool,
    pub checked: bool,
    pub global: Vector3f,
    pub size: Vector2f,
    pub separator_padding: f32,
    pub separator_height: f32,
    pub arrow_offset: Vector3f,
    pub check_offset: Vector3f,
    pub text_offset: Vector3f,
}

pub fn item_draw(i: &ItemDrawIn, out: &mut [MenuOp]) -> usize {
    let mut n = 0;
    let mut push = |op: MenuOp| {
        if let Some(slot) = out.get_mut(n) {
            *slot = op;
        }
        n += 1;
    };
    let gfx = |slot, pos, size: Option<Vector2f>| MenuOp {
        kind: OP_GFX,
        slot,
        has_size: u32::from(size.is_some()),
        pos,
        size: size.unwrap_or_default(),
        ..MenuOp::default()
    };
    if i.separator {
        push(gfx(
            GFX_LINE,
            add3(i.global, v3(i.separator_padding, i.size.y * 0.5 - 1.0, 0.2)),
            Some(v2(i.size.x - 2.0 * i.separator_padding, i.separator_height)),
        ));
        return n.min(out.len());
    }
    let color = if i.enabled {
        if i.highlighted {
            COLOR_HIGHLIGHT
        } else {
            COLOR_DEFAULT
        }
    } else {
        COLOR_DISABLED
    };
    let text = |slot, pos, align| MenuOp {
        kind: OP_TEXT,
        slot,
        color,
        align,
        pos,
        ..MenuOp::default()
    };
    if i.kind == KIND_CONTEXT {
        if i.is_parent {
            push(gfx(GFX_ARROW, add3(i.global, i.arrow_offset), None));
        } else {
            push(text(
                TEXT_SHORTCUT,
                add3(
                    i.global,
                    v3(i.arrow_offset.x, i.text_offset.y, i.text_offset.z),
                ),
                ALIGN_RIGHT,
            ));
        }
        if i.checked {
            let slot = if i.widget_enabled {
                GFX_CHECKED_ENABLED
            } else {
                GFX_CHECKED_DISABLED
            };
            push(gfx(slot, add3(i.global, i.check_offset), None));
        }
    }
    if i.highlighted {
        push(gfx(
            GFX_SELECTION,
            add3(i.global, v3(0.0, 0.0, 0.1)),
            Some(i.size),
        ));
    }
    push(text(TEXT_LABEL, add3(i.global, i.text_offset), ALIGN_LEFT));
    n.min(out.len())
}

/// `ShowSubMenu`: global position of the child menu.
pub fn item_submenu_pos(
    kind: i32,
    parent_global: Vector3f,
    item_pos: Vector3f,
    item_size_y: f32,
    parent_w: f32,
    child_w: f32,
    virtual_x: f32,
) -> Vector3f {
    let mut p = add3(parent_global, v3(0.0, item_pos.y, item_pos.z));
    match kind {
        KIND_CONTEXT => {
            if p.x + parent_w + child_w > virtual_x {
                p.x -= child_w;
            } else {
                p.x += parent_w;
            }
        }
        KIND_MAIN => {
            p.x += item_pos.x;
            p.y += item_size_y;
        }
        _ => {}
    }
    p
}

/// `cWidgetMenuItem::OnInit` size.
pub fn item_init_size(
    separator: bool,
    separator_height: f32,
    v_padding: f32,
    font_y: f32,
    menu_item_width: f32,
) -> Vector2f {
    let mut y = 2.0 * v_padding;
    if separator {
        y += separator_height;
    } else {
        y += font_y;
    }
    v2(menu_item_width, y)
}

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct ItemUpdate {
    pub timer: f32,
    pub show_submenu: bool,
}

/// `OnUpdate` after the visibility/attention early-out.
#[allow(clippy::too_many_arguments, reason = "one argument per C++ state read")]
pub fn item_update(
    timer: f32,
    dt: f32,
    open_time: f32,
    mouse_over: bool,
    kind: i32,
    highlighted_is_self: bool,
    is_parent: bool,
    menu_open: bool,
) -> ItemUpdate {
    let mut timer = timer;
    if mouse_over && kind == KIND_CONTEXT {
        timer += dt;
    }
    ItemUpdate {
        timer,
        show_submenu: highlighted_is_self && is_parent && timer >= open_time && !menu_open,
    }
}

/// `OnMouseUp` decision.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct ItemUp {
    pub ret: bool,
    pub hide_topmost: bool,
    pub press: bool,
}

pub fn item_mouse_up(separator: bool, highlighted_is_self: bool, is_parent: bool) -> ItemUp {
    if separator || !highlighted_is_self {
        return ItemUp {
            ret: false,
            hide_topmost: false,
            press: false,
        };
    }
    ItemUp {
        ret: true,
        hide_topmost: !is_parent,
        press: true,
    }
}

/// `OnMouseMove`: whether this item becomes the highlighted one.
pub fn item_mouse_move_highlights(
    visible: bool,
    draw_mouse: bool,
    attention_blocks: bool,
    mouse_over: bool,
) -> bool {
    !(!visible || !draw_mouse || attention_blocks) && mouse_over
}

/// `OnMouseLeave`.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct ItemLeave {
    pub ret: bool,
    pub clear_highlight: bool,
    pub reset_timer: bool,
}

pub fn item_mouse_leave(attention_blocks: bool, menu_open: bool) -> ItemLeave {
    if attention_blocks {
        return ItemLeave {
            ret: false,
            clear_highlight: false,
            reset_timer: false,
        };
    }
    ItemLeave {
        ret: true,
        clear_highlight: !menu_open,
        reset_timer: true,
    }
}

/// `AddShortcut` text: `cur`, a comma when `cur` is not empty, then `add`.
pub fn join_shortcut(cur: &[u32], add: &[u32]) -> Vec<u32> {
    let mut v = cur.to_vec();
    if !v.is_empty() {
        v.push(u32::from(b','));
    }
    v.extend_from_slice(add);
    v
}

// ---------------------------------------------------------------- context menu

/// `GetMenuItemWidth`.
pub fn ctx_item_width(size_x: f32, left_border_w: f32, right_border_w: f32) -> f32 {
    size_x - (left_border_w + right_border_w)
}

/// `UpdateMenuItemsPos`: `num_items` includes the new item.
pub fn ctx_item_pos(
    size_y: f32,
    up_border_h: f32,
    left_border_w: f32,
    num_items: usize,
) -> Vector3f {
    let mut y = size_y;
    if num_items > 1 {
        y -= up_border_h;
    } else {
        y += up_border_h;
    }
    v3(left_border_w, y, 0.1)
}

/// Gap above a new item: none next to a separator.
pub fn ctx_separation(prev_separator: bool, new_separator: bool, item_separation: f32) -> f32 {
    if prev_separator || new_separator {
        0.0
    } else {
        item_separation
    }
}

/// Size of an empty menu: the borders only.
pub fn ctx_empty_size(size_x: f32, up_img_h: f32, down_img_h: f32) -> Vector2f {
    v2(size_x, up_img_h + down_img_h)
}

/// Menu size after an item of height `item_h` joined it.
pub fn ctx_grown_size(size: Vector2f, item_h: f32, separation: f32) -> Vector2f {
    v2(size.x, size.y + item_h + separation)
}

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct CtxWidth {
    pub resize_items: bool,
    pub item_width: f32,
    pub menu_size: Vector2f,
    pub arrow_pos: Vector3f,
    pub text_pos: Vector3f,
    pub check_pos: Vector3f,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct CtxMetrics {
    pub h_padding: f32,
    pub check_icon: f32,
    pub text_left_padding: f32,
    pub text_right_padding: f32,
    pub arrow_icon: f32,
    pub v_padding: f32,
    pub left_border_w: f32,
    pub right_border_w: f32,
}

/// `UpdateMenuWidth`: C++ resizes the items when `resize_items`, then the menu, then the offsets.
pub fn ctx_update_width(
    m: &CtxMetrics,
    item_text_w: f32,
    current_item_width: f32,
    size_y: f32,
) -> CtxWidth {
    let non_text = m.h_padding
        + m.check_icon
        + m.text_left_padding
        + m.text_right_padding
        + m.arrow_icon
        + m.h_padding;
    let mut item_width = current_item_width;
    let mut resize_items = false;
    if item_text_w > item_width - non_text {
        item_width = item_text_w + non_text;
        resize_items = true;
    }
    let menu_w = item_width + m.left_border_w + m.right_border_w;
    CtxWidth {
        resize_items,
        item_width,
        menu_size: v2(menu_w, size_y),
        arrow_pos: v3(
            item_width - (m.h_padding + m.arrow_icon),
            m.arrow_icon * 0.5,
            0.2,
        ),
        text_pos: v3(
            m.h_padding + m.check_icon + m.text_left_padding,
            m.v_padding,
            0.2,
        ),
        check_pos: v3(m.h_padding, m.v_padding, 0.2),
    }
}

/// `Hide` steps, in order: hide the open child, clear the highlight, then either tell the parent
/// menu or restore attention and drop the context-menu z; finally hide self and reset the timer.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct CtxHide {
    pub hide_child: bool,
    pub notify_parent: bool,
}

pub fn ctx_hide_plan(submenu_open: bool, has_parent_menu: bool) -> CtxHide {
    CtxHide {
        hide_child: submenu_open,
        notify_parent: has_parent_menu,
    }
}

/// `cWidgetContextMenu::OnUpdate` timer step: `timer` is the value to store before any hide. The
/// must-hide flag is read by C++ after that hide, which can set it through a focus change.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct CtxUpdate {
    pub timer: f32,
    pub hide_timer: bool,
}

pub fn ctx_update(
    timer: f32,
    dt: f32,
    hide_time: f32,
    visible: bool,
    parent_item_not_highlighted: bool,
) -> CtxUpdate {
    let mut timer = timer;
    let mut hide_timer = false;
    if parent_item_not_highlighted && visible {
        timer += dt;
        hide_timer = timer >= hide_time;
    }
    CtxUpdate { timer, hide_timer }
}

/// `OnLostFocus`: hide the topmost menu unless focus moved inside the menu tree.
pub fn ctx_lost_focus_hides(
    focus_null: bool,
    connected_to_focus: bool,
    focus_connected_to_menu: bool,
) -> bool {
    focus_null || (!connected_to_focus && !focus_connected_to_menu)
}

/// `OnKeyPress`: `true` hides the menu, `false` forwards the key to the items.
pub fn ctx_key_hides(key: i32) -> bool {
    key == KEY_ESCAPE
}

// ---------------------------------------------------------------- main menu

/// `UpdateMenuItemsPos`: `prev` is the previous item's `(local x, width)` when one exists.
pub fn main_item_pos(v_padding: f32, prev: Option<(f32, f32)>, item_separation: f32) -> Vector3f {
    let mut x = 3.0;
    if let Some((px, pw)) = prev {
        x += px;
        x += pw;
        x += item_separation;
    }
    v3(x, v_padding, 1.0)
}

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct MainItemSize {
    pub size: Vector2f,
    pub text_pos: Vector3f,
}

pub fn main_item_size(
    text_len: f32,
    text_padding: f32,
    font_y: f32,
    v_padding: f32,
) -> MainItemSize {
    MainItemSize {
        size: v2(text_len + 2.0 * text_padding, font_y + 2.0 * v_padding),
        text_pos: v3(text_padding, v_padding, 0.2),
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct MainInit {
    pub size: Vector2f,
    pub children_offset: Vector3f,
    pub pos_delta: Vector3f,
}

/// `cWidgetMainMenu::OnInit`; the position is the old local position plus `pos_delta`.
pub fn main_init(parent_kind: i32, virtual_x: f32, parent_w: f32, font_y: f32) -> MainInit {
    let height = font_y + 4.0 + 4.0;
    let (width, x_offset) = match parent_kind {
        PARENT_ROOT => (virtual_x, 0.0),
        PARENT_WINDOW => (parent_w - 2.0 - 2.0, 2.0),
        _ => (parent_w, 0.0),
    };
    let zero_h = 0.0f32;
    MainInit {
        size: v2(width, height),
        children_offset: v3(0.0, zero_h, 0.0),
        pos_delta: v3(x_offset, -zero_h, 1.0),
    }
}
