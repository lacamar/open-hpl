//! Menu kinds: `iWidgetMenu` state (item list, highlight, offsets, parent item) shared by the
//! context and main menus, and the menu items.

use std::ptr::addr_of_mut;

use super::g1_set;
use super::gw::{self, with};
use super::gw_kinds::{self as gk, Kind};
use super::h19_widget::Op;
use super::h20_ffi as f;
use super::h20_menu::{Ctx, Item, Main};
use super::set::{Msg, W};
use crate::types::{Vector2f, Vector3f};

pub const KIND_ITEM: i32 = 18;
pub const KIND_CTX: i32 = 19;
pub const KIND_MAIN: i32 = 20;

const TYPE_CTX: i32 = 11;
const TYPE_MAIN: i32 = 12;

pub(crate) const MSG_BUTTON_PRESSED: i32 = 11;
const MSG_MOUSE_MOVE: i32 = 0;
const MSG_MOUSE_DOWN: i32 = 1;
const MSG_MOUSE_UP: i32 = 2;
const MSG_MOUSE_ENTER: i32 = 4;
const MSG_MOUSE_LEAVE: i32 = 5;
const MSG_GOT_FOCUS: i32 = 6;
const MSG_LOST_FOCUS: i32 = 8;
const MSG_KEY_PRESS: i32 = 14;

pub struct MenuBase {
    items: Vec<W>,
    highlighted: W,
    text_pos: Vector3f,
    arrow_pos: Vector3f,
    check_pos: Vector3f,
    parent_item: W,
    prev_attention: W,
    must_hide: bool,
}

impl MenuBase {
    fn new() -> MenuBase {
        let z = Vector3f::default();
        MenuBase {
            items: Vec::new(),
            highlighted: 0,
            text_pos: z,
            arrow_pos: z,
            check_pos: z,
            parent_item: 0,
            prev_attention: 0,
            must_hide: false,
        }
    }
}

#[repr(C)]
pub struct CtxK {
    c: Ctx,
    m: MenuBase,
}

#[repr(C)]
pub struct MainK {
    c: Main,
    m: MenuBase,
}

#[repr(C)]
pub struct ItemK {
    i: Item,
    shortcut: Vec<u32>,
}

#[derive(Clone, Copy)]
pub(crate) enum P {
    Ctx(*mut CtxK),
    Main(*mut MainK),
    Item(*mut ItemK),
}

pub(crate) fn ptr(w: W) -> Option<P> {
    with(w, |g| match &mut g.kind {
        Kind::Ctx(k) => Some(P::Ctx(&raw mut **k)),
        Kind::Main(k) => Some(P::Main(&raw mut **k)),
        Kind::Item(k) => Some(P::Item(&raw mut **k)),
        _ => None,
    })
    .flatten()
}

fn menu(w: W) -> Option<*mut MenuBase> {
    match ptr(w)? {
        // SAFETY: the boxed kind is live while the widget is.
        P::Ctx(k) => Some(unsafe { addr_of_mut!((*k).m) }),
        // SAFETY: as above.
        P::Main(k) => Some(unsafe { addr_of_mut!((*k).m) }),
        P::Item(_) => None,
    }
}

fn mb<R: Default>(w: W, f: impl FnOnce(&mut MenuBase) -> R) -> R {
    // SAFETY: `menu` yields a live state; `f` reaches no other widget.
    menu(w).map_or_else(R::default, |m| f(unsafe { &mut *m }))
}

fn item(w: W) -> Option<*mut Item> {
    match ptr(w)? {
        // SAFETY: as in `menu`.
        P::Item(k) => Some(unsafe { addr_of_mut!((*k).i) }),
        _ => None,
    }
}

pub(crate) fn ty_of(w: W) -> i32 {
    gw::read(w, |g| g.ty)
}

fn boxed<T>(init: impl FnOnce(*mut T)) -> Box<T> {
    let mut b = Box::<T>::new_uninit();
    init(b.as_mut_ptr());
    // SAFETY: `init` wrote the whole struct.
    unsafe { b.assume_init() }
}

pub(crate) fn set_kind(w: W, id: i32, parent: W) {
    let Some(base) = with(w, |g| &raw mut g.base) else {
        return;
    };
    match id {
        KIND_CTX => {
            // SAFETY: `base` is live; the struct is written whole.
            let k = boxed(|p: *mut CtxK| unsafe {
                addr_of_mut!((*p).m).write(MenuBase::new());
                super::h20_menu::ctx_init(addr_of_mut!((*p).c), w, base);
            });
            with(w, |g| g.kind = Kind::Ctx(k));
        }
        KIND_MAIN => {
            // SAFETY: as above.
            let k = boxed(|p: *mut MainK| unsafe {
                addr_of_mut!((*p).m).write(MenuBase::new());
                super::h20_menu::main_init(addr_of_mut!((*p).c), w, base);
            });
            with(w, |g| g.kind = Kind::Main(k));
            gw::load_graphics(w);
        }
        KIND_ITEM => {
            let kind = match ty_of(parent) {
                TYPE_CTX => 1,
                TYPE_MAIN => 2,
                _ => 0,
            };
            // SAFETY: as above.
            let k = boxed(|p: *mut ItemK| unsafe {
                addr_of_mut!((*p).shortcut).write(Vec::new());
                super::h20_menu::item_init(addr_of_mut!((*p).i), w, base, kind);
            });
            with(w, |g| g.kind = Kind::Item(k));
            gw::load_graphics(w);
        }
        _ => {}
    }
}

pub(crate) fn teardown(w: W) {
    let destroying = g1_set::is_destroying(gw::set_of(w));
    match ptr(w) {
        Some(P::Ctx(_) | P::Main(_)) if !destroying => clear_items(w),
        Some(P::Item(_)) if !destroying => clear_child(w),
        _ => {}
    }
}

fn clear_items(w: W) {
    let set = gw::set_of(w);
    let items = mb(w, |m| std::mem::take(&mut m.items));
    for it in items {
        g1_set::destroy_widget(set, it, false);
    }
}

fn clear_child(w: W) {
    let Some(i) = item(w) else { return };
    // SAFETY: live item state.
    let c = unsafe { std::mem::replace(&mut (*i).child, 0) };
    if c != 0 {
        g1_set::destroy_widget(gw::set_of(w), c, false);
    }
}

pub(crate) fn hide(w: W) {
    // SAFETY: as in `call`.
    unsafe {
        match ptr(w) {
            Some(P::Ctx(k)) => f::ohpl_h20_cm_hide(addr_of_mut!((*k).c)),
            Some(P::Main(k)) => f::ohpl_h20_mm_hide(addr_of_mut!((*k).c)),
            _ => {}
        }
    }
}

pub(crate) fn item_width(w: W) -> f32 {
    match ptr(w) {
        // SAFETY: as in `call`.
        Some(P::Ctx(k)) => unsafe { f::ohpl_h20_cm_item_width(addr_of_mut!((*k).c)) },
        _ => 0.0,
    }
}

pub(crate) fn parent_menu_of_item(w: W) -> W {
    gw::parent(w)
}

pub(crate) fn parent_menu(w: W) -> W {
    let pi = mb(w, |m| m.parent_item);
    if pi == 0 { 0 } else { parent_menu_of_item(pi) }
}

pub(crate) fn top_most(w: W) -> W {
    let mut cur = w;
    while mb(cur, |m| m.parent_item) != 0 {
        cur = parent_menu(cur);
    }
    cur
}

fn item_menu_open(w: W) -> bool {
    item(w).is_some_and(|i| {
        // SAFETY: live item state.
        let c = unsafe { (*i).child };
        c != 0 && gw::is_visible(c)
    })
}

pub(crate) fn submenu_open(w: W) -> bool {
    let h = mb(w, |m| m.highlighted);
    h != 0 && item_menu_open(h)
}

pub(crate) fn child_menu_hide(w: W) {
    match ptr(w) {
        Some(P::Ctx(_)) => g1_set::set_focused(gw::set_of(w), w),
        // SAFETY: as in `call`.
        Some(P::Main(k)) => unsafe { f::ohpl_h20_mm_on_child_menu_hide(addr_of_mut!((*k).c)) },
        _ => {}
    }
}

fn update_items(w: W, it: W) {
    // SAFETY: as in `call`.
    unsafe {
        match ptr(w) {
            Some(P::Ctx(k)) => {
                f::ohpl_h20_cm_update_items_pos(addr_of_mut!((*k).c), it);
                f::ohpl_h20_cm_update_items_size(addr_of_mut!((*k).c), it);
            }
            Some(P::Main(k)) => {
                f::ohpl_h20_mm_update_items_pos(addr_of_mut!((*k).c), it);
                f::ohpl_h20_mm_update_items_size(addr_of_mut!((*k).c), it);
            }
            _ => {}
        }
    }
}

/// `iWidgetMenu::AddMenuItem`.
pub(crate) fn add_menu_item(w: W, text: &[u32]) -> W {
    let set = gw::set_of(w);
    let it = gk::create_menu_item(set, text, w);
    mb(w, |m| m.items.push(it));
    update_items(w, it);
    it
}

/// `cWidgetMenuItem::AddMenuItem`: the child menu is a context menu under the item's menu.
pub(crate) fn item_add_menu_item(w: W, text: &[u32]) -> W {
    let Some(i) = item(w) else { return 0 };
    // SAFETY: live item state.
    if unsafe { (*i).child } == 0 {
        let set = gw::set_of(w);
        let c = gk::create_context_menu(set, gw::parent(w));
        gw::set_visible(c, false);
        mb(c, |m| m.parent_item = w);
        // SAFETY: live item state.
        unsafe { (*i).child = c };
    }
    // SAFETY: as above.
    add_menu_item(unsafe { (*i).child }, text)
}

/// `cGuiSet::ShowContextMenu`.
pub(crate) fn show_context_menu(set: *mut g1_set::Set, menu: W, pos: Vector3f) {
    if menu == 0 {
        return;
    }
    let mut p = pos;
    let virt = g1_set::virtual_rect(set);
    let size = gw::size(menu);
    if pos.x + size.x > virt[0] {
        p.x = virt[0] - size.x;
    }
    if pos.y + size.y > virt[1] {
        p.y = virt[1] - size.y;
    }
    gw::set_visible(menu, true);
    gw::set_enabled(menu, true);
    mb(menu, |m| m.highlighted = 0);
    let pi = mb(menu, |m| m.parent_item);
    if pi == 0 || ty_of(parent_menu(menu)) == TYPE_MAIN {
        // SAFETY: the set is live.
        unsafe {
            p.z = g1_set::ohpl_g1_set_context_menu_z(set);
            g1_set::ohpl_g1_set_add_context_menu_z(set, 5.0);
        }
    } else {
        p.z = gw::global_position(parent_menu(menu)).z;
    }
    // SAFETY: the set is live.
    unsafe { g1_set::ohpl_g1_set_set_attention(set, top_most(menu), true, false) };
    gw::set_global_position(menu, p);
    g1_set::set_focused(set, menu);
}

/// Virtual `op` of the menu kinds.
pub(crate) fn call(w: W, op: Op, other: W, d: &Msg, dt: f32) -> Option<bool> {
    let p = ptr(w)?;
    // SAFETY: the state is live for the call; the logic re-enters through the host table only.
    unsafe {
        match p {
            P::Ctx(k) => {
                let c = addr_of_mut!((*k).c);
                match op {
                    Op::Draw => f::ohpl_h20_cm_on_draw(c),
                    Op::Update => f::ohpl_h20_cm_on_update(c, dt),
                    Op::Virtual => {
                        return match other as i32 {
                            MSG_MOUSE_DOWN | MSG_MOUSE_UP | MSG_MOUSE_ENTER => Some(true),
                            MSG_MOUSE_LEAVE => {
                                if !submenu_open(w) {
                                    mb(w, |m| m.highlighted = 0);
                                }
                                Some(true)
                            }
                            MSG_LOST_FOCUS => Some(f::ohpl_h20_cm_on_lost_focus(c)),
                            MSG_GOT_FOCUS => Some(f::ohpl_h20_cm_on_got_focus(c)),
                            MSG_KEY_PRESS => Some(f::ohpl_h20_cm_on_key_press(c, d)),
                            _ => None,
                        };
                    }
                    _ => return None,
                }
            }
            P::Main(k) => {
                let m = addr_of_mut!((*k).c);
                match op {
                    Op::Draw => f::ohpl_h20_mm_on_draw(m),
                    Op::Update => f::ohpl_h20_mm_on_update(m),
                    _ => return None,
                }
            }
            P::Item(k) => {
                let i = addr_of_mut!((*k).i);
                match op {
                    Op::Draw => f::ohpl_h20_mi_on_draw(i),
                    Op::Update => f::ohpl_h20_mi_on_update(i, dt),
                    Op::Virtual => {
                        return match other as i32 {
                            MSG_MOUSE_DOWN => Some(f::ohpl_h20_mi_on_mouse_down(i)),
                            MSG_MOUSE_UP => Some(f::ohpl_h20_mi_on_mouse_up(i, d)),
                            MSG_MOUSE_ENTER | MSG_GOT_FOCUS | MSG_LOST_FOCUS => Some(true),
                            MSG_MOUSE_MOVE => Some(f::ohpl_h20_mi_on_mouse_move(i)),
                            MSG_MOUSE_LEAVE => Some(f::ohpl_h20_mi_on_mouse_leave(i)),
                            MSG_KEY_PRESS => Some(f::ohpl_h20_mi_on_key_press(i, d)),
                            _ => None,
                        };
                    }
                    _ => return None,
                }
            }
        }
    }
    Some(false)
}

pub(crate) fn on_init(w: W) -> bool {
    // SAFETY: as in `call`.
    unsafe {
        match ptr(w) {
            Some(P::Ctx(_)) => gw::set_visible(w, false),
            Some(P::Main(k)) => f::ohpl_h20_mm_on_init(addr_of_mut!((*k).c)),
            Some(P::Item(k)) => f::ohpl_h20_mi_on_init(addr_of_mut!((*k).i)),
            None => return false,
        }
    }
    true
}

pub(crate) fn on_load_graphics(w: W) -> bool {
    // SAFETY: as in `call`.
    unsafe {
        match ptr(w) {
            Some(P::Ctx(k)) => f::ohpl_h20_cm_on_load_graphics(addr_of_mut!((*k).c)),
            Some(P::Main(k)) => f::ohpl_h20_mm_on_load_graphics(addr_of_mut!((*k).c)),
            Some(P::Item(k)) => f::ohpl_h20_mi_on_load_graphics(addr_of_mut!((*k).i)),
            None => return false,
        }
    }
    true
}

pub(crate) fn state(w: W) -> usize {
    // SAFETY: only field addresses are taken.
    unsafe {
        match ptr(w) {
            Some(P::Ctx(k)) => addr_of_mut!((*k).c) as usize,
            Some(P::Main(k)) => addr_of_mut!((*k).c) as usize,
            Some(P::Item(k)) => addr_of_mut!((*k).i) as usize,
            None => 0,
        }
    }
}

pub(crate) fn highlighted(w: W) -> W {
    mb(w, |m| m.highlighted)
}

pub(crate) fn offset(w: W, which: Offset) -> Vector3f {
    mb(w, |m| match which {
        Offset::Arrow => m.arrow_pos,
        Offset::Check => m.check_pos,
        Offset::Text => m.text_pos,
    })
}

pub(crate) fn set_must_hide(w: W, v: bool) {
    mb(w, |m| m.must_hide = v);
}

pub(crate) fn must_hide(w: W) -> bool {
    mb(w, |m| m.must_hide)
}

pub(crate) fn parent_item(w: W) -> W {
    mb(w, |m| m.parent_item)
}

pub(crate) fn prev_attention(w: W) -> W {
    mb(w, |m| m.prev_attention)
}

pub(crate) fn item_count(w: W) -> usize {
    mb(w, |m| m.items.len())
}

pub(crate) fn item_at(w: W, i: usize) -> W {
    mb(w, |m| m.items.get(i).copied().unwrap_or(0))
}

pub(crate) fn set_offsets(
    w: W,
    arrow: Option<Vector3f>,
    text: Option<Vector3f>,
    check: Option<Vector3f>,
) {
    mb(w, |m| {
        if let Some(v) = arrow {
            m.arrow_pos = v;
        }
        if let Some(v) = text {
            m.text_pos = v;
        }
        if let Some(v) = check {
            m.check_pos = v;
        }
    });
}

pub(crate) fn is_separator(w: W) -> bool {
    gw::read(w, |g| g.text.is_empty())
}

#[derive(Clone, Copy)]
pub(crate) enum Offset {
    Arrow,
    Check,
    Text,
}

pub(crate) fn shortcut(w: W) -> (*const u32, usize) {
    match ptr(w) {
        // SAFETY: live item state; the text outlives the caller's use within the op.
        Some(P::Item(k)) => unsafe { ((*k).shortcut.as_ptr(), (*k).shortcut.len()) },
        _ => (std::ptr::null(), 0),
    }
}

/// `cWidgetMenuItem::GetTextLength`.
pub(crate) fn text_length(w: W) -> f32 {
    let (font, size, text) = gw::read(w, |g| {
        // SAFETY: the skin font is live while the skin is.
        let s = unsafe { g.font.as_ref() }.map_or(Vector2f::default(), |f| f.size);
        (g.font_type, s, g.text.clone())
    });
    let sc = shortcut_vec(w);
    gw::font_length(font, size, &text) + gw::font_length(font, size, &sc)
}

fn shortcut_vec(w: W) -> Vec<u32> {
    let (p, n) = shortcut(w);
    if p.is_null() {
        Vec::new()
    } else {
        // SAFETY: `shortcut` returns a live slice.
        unsafe { std::slice::from_raw_parts(p, n) }.to_vec()
    }
}

/// The part of `cWidgetMenuItem::AddShortcut` after the base shortcut: the display text.
pub(crate) fn item_shortcut_added(w: W, added: &[u32]) {
    let cur = shortcut_vec(w);
    let joined = super::widgets_menu::join_shortcut(&cur, added);
    if let Some(P::Item(k)) = ptr(w) {
        // SAFETY: live item state.
        unsafe { (*k).shortcut = joined };
    }
    let parent = gw::parent(w);
    let len = text_length(w);
    // SAFETY: as in `call`.
    unsafe {
        if let Some(P::Ctx(k)) = ptr(parent) {
            f::ohpl_h20_cm_update_width(addr_of_mut!((*k).c), len);
        }
    }
}

pub(crate) fn is_item(w: W) -> bool {
    matches!(ptr(w), Some(P::Item(_)))
}

pub(crate) fn set_highlight(w: W, it: W) {
    mb(w, |m| m.highlighted = it);
}

pub(crate) fn save_prev_attention(w: W) {
    let a = g1_set::attention_widget(gw::set_of(w));
    mb(w, |m| m.prev_attention = a);
}

pub(crate) fn clear_menu_items(w: W) {
    match ptr(w) {
        Some(P::Ctx(_) | P::Main(_)) => clear_items(w),
        Some(P::Item(_)) => clear_child(w),
        None => {}
    }
}

/// # Safety
/// `p` is readable for `n` wide characters.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gw_menu_add_item(w: W, p: *const u32, n: usize) -> W {
    let text = if p.is_null() {
        &[][..]
    } else {
        // SAFETY: caller contract.
        unsafe { std::slice::from_raw_parts(p, n) }
    };
    match ptr(w) {
        Some(P::Item(_)) => item_add_menu_item(w, text),
        Some(_) => add_menu_item(w, text),
        None => 0,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gw_menu_add_separator(w: W) {
    match ptr(w) {
        Some(P::Item(_)) => {
            item_add_menu_item(w, &[]);
        }
        Some(P::Ctx(_)) => {
            add_menu_item(w, &[]);
        }
        _ => {}
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gw_menu_clear(w: W) {
    clear_menu_items(w);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gw_menu_set_highlight(w: W, it: W) {
    set_highlight(w, it);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gw_ctx_save_prev_attention(w: W) {
    save_prev_attention(w);
}

/// # Safety
/// `set` is a live set.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gw_show_context_menu(
    set: *mut g1_set::Set,
    menu: W,
    x: f32,
    y: f32,
    z: f32,
) {
    show_context_menu(set, menu, Vector3f { x, y, z });
}
