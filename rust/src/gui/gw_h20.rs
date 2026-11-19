//! The complex widgets (text box, list boxes, combo box, tab frame, menus) as `Gw` kinds: their
//! state is the H20 struct boxed in the kind, the virtuals go to the H20 logic, and the C++ shells
//! keep only forwarders.

use std::ffi::c_void;

use super::gw::{self, with};
use super::gw_kinds::Kind;
use super::h19_widget::Op;
use super::h20_combo::Combo;
use super::h20_ffi as f;
use super::h20_host::{HostVt, Hx};
use super::h20_list::{KIND_MULTI, ListBase};
use super::h20_tab::{Frame, Label, Tab};
use super::h20_text::TextBox;
use super::set::{Msg, W};

pub const KIND_TEXTBOX: i32 = 11;
pub const KIND_LIST: i32 = 12;
pub const KIND_MPLIST: i32 = 13;
pub const KIND_COMBO: i32 = 14;
pub const KIND_TABFRAME: i32 = 15;
pub const KIND_TAB: i32 = 16;
pub const KIND_TABLABEL: i32 = 17;

const MSG_MOUSE_MOVE: i32 = 0;
const MSG_MOUSE_DOWN: i32 = 1;
const MSG_MOUSE_UP: i32 = 2;
const MSG_MOUSE_DBL: i32 = 3;
const MSG_MOUSE_ENTER: i32 = 4;
const MSG_MOUSE_LEAVE: i32 = 5;
const MSG_GOT_FOCUS: i32 = 6;
const MSG_GOT_TAB_FOCUS: i32 = 7;
const MSG_LOST_FOCUS: i32 = 8;
const MSG_KEY_PRESS: i32 = 14;
const MSG_UI_ARROW_PRESS: i32 = 17;
const MSG_UI_ARROW_RELEASE: i32 = 18;
const MSG_UI_BUTTON_PRESS: i32 = 19;
const MSG_UI_BUTTON_RELEASE: i32 = 20;
const MSG_UI_BUTTON_DBL: i32 = 21;
const MSG_GET_UI_NAV_FOCUS: i32 = 22;

pub(crate) fn vt() -> *const HostVt {
    &raw const super::gw_ops::VT
}

/// The state pointer of a complex widget (boxed, stable while the widget lives).
#[derive(Clone, Copy)]
pub(crate) enum P {
    Text(*mut TextBox),
    List(*mut ListBase),
    Combo(*mut Combo),
    TabFrame(*mut Frame),
    Tab(*mut Tab),
    TabLabel(*mut Label),
}

pub(crate) fn ptr(w: W) -> Option<P> {
    with(w, |g| match &mut g.kind {
        Kind::Text(t) => Some(P::Text(&raw mut **t)),
        Kind::List(t) => Some(P::List(&raw mut **t)),
        Kind::Combo(t) => Some(P::Combo(&raw mut **t)),
        Kind::TabFrame(t) => Some(P::TabFrame(&raw mut **t)),
        Kind::Tab(t) => Some(P::Tab(&raw mut **t)),
        Kind::TabLabel(t) => Some(P::TabLabel(&raw mut **t)),
        _ => None,
    })
    .flatten()
}

fn boxed<T>(init: impl FnOnce(*mut T)) -> Box<T> {
    let mut b = Box::<T>::new_uninit();
    init(b.as_mut_ptr());
    // SAFETY: `init` wrote the whole struct.
    unsafe { b.assume_init() }
}

/// Constructor work of kind `id` (the C++ constructor bodies).
pub(crate) fn set_kind(w: W, id: i32, a: i32, ctr: usize) {
    if id >= super::gw_menu::KIND_ITEM {
        super::gw_menu::set_kind(w, id, ctr);
        return;
    }
    let Some(base) = with(w, |g| &raw mut g.base) else {
        return;
    };
    if id == KIND_TEXTBOX {
        // SAFETY: `base` is the widget's live base state.
        let t = boxed(|p| unsafe { super::h20_text::init(p, vt(), w, base, a != 0) });
        with(w, |g| g.kind = Kind::Text(t));
        gw::load_graphics(w);
        let gfx = gw::skin_gfx(gw::skin_of(w), "PointerText");
        with(w, |g| g.pointer_gfx = gfx);
    } else if id == KIND_LIST || id == KIND_MPLIST {
        let kind = i32::from(id == KIND_MPLIST);
        // SAFETY: as above; `ctr` is the shell's item container.
        let t = boxed(|p| unsafe { super::h20_list::init(Hx::new(vt()), p, w, w, base, kind) });
        with(w, |g| {
            g.kind = Kind::List(t);
            g.base.clips = true;
        });
    } else if id == KIND_COMBO {
        // SAFETY: as above.
        let t = boxed(|p| unsafe { super::h20_combo::init(Hx::new(vt()), vt(), p, w, w, base) });
        with(w, |g| g.kind = Kind::Combo(t));
    } else if id == KIND_TABFRAME {
        // SAFETY: as above.
        let t = boxed(|p| unsafe { super::h20_tab::frame_init(p, vt(), w, base) });
        with(w, |g| g.kind = Kind::TabFrame(t));
        gw::load_graphics(w);
        with(w, |g| g.base.clips = true);
    } else if id == KIND_TAB {
        // `ctr` is the parent tab frame.
        let Some(P::TabFrame(fs)) = ptr(ctr) else {
            return;
        };
        // SAFETY: as above; the frame state outlives its tabs.
        let t = boxed(|p| unsafe { super::h20_tab::tab_init_state(p, vt(), w, ctr, fs, base) });
        with(w, |g| g.kind = Kind::Tab(t));
        gw::load_graphics(w);
    } else if id == KIND_TABLABEL {
        // SAFETY: as above.
        let t = boxed(|p| unsafe { super::h20_tab::label_init(p, vt(), w, base) });
        with(w, |g| g.kind = Kind::TabLabel(t));
        gw::load_graphics(w);
    }
}

pub(crate) fn teardown(w: W) {
    let destroying = super::g1_set::is_destroying(gw::set_of(w));
    let Some(p) = ptr(w) else { return };
    // SAFETY: the state is live until the `Gw` drops.
    unsafe {
        match p {
            P::Text(t) => {
                if !destroying {
                    f::ohpl_h20_tb_destroy_buttons(vt(), t);
                }
                f::ohpl_h20_tb_free(t);
            }
            P::List(l) => {
                if !destroying {
                    if (*l).kind == KIND_MULTI {
                        f::ohpl_h20_lb_destroy_columns(vt(), l);
                    }
                    f::ohpl_h20_lb_destroy_slider(vt(), l);
                }
                f::ohpl_h20_lb_free(l);
                super::gw_items::free_all(w);
            }
            P::Combo(c) => {
                if !destroying {
                    f::ohpl_h20_cb_destroy_children(vt(), c);
                }
                super::gw_items::free_all(w);
            }
            P::TabFrame(t) => {
                if !destroying {
                    f::ohpl_h20_tf_destroy(vt(), t);
                }
                f::ohpl_h20_tf_free(t);
            }
            P::Tab(t) => {
                if !destroying {
                    f::ohpl_h20_tab_destroy(vt(), t);
                }
            }
            P::TabLabel(_) => {}
        }
    }
}

/// Virtual `op` of a complex widget; `None` when the kind is not one or leaves it to the base.
pub(crate) fn call(w: W, op: Op, other: W, d: &Msg, fl: f32) -> Option<bool> {
    let p = ptr(w)?;
    let v = vt();
    // SAFETY: the state is live for the call; the logic re-enters through the host table only.
    unsafe {
        match p {
            P::Text(t) => match op {
                Op::ChangeSize => f::ohpl_h20_tb_on_change_size(v, t),
                Op::ChangeText => f::ohpl_h20_tb_on_change_text(v, t),
                Op::Draw => f::ohpl_h20_tb_on_draw(v, t),
                Op::Virtual => {
                    return match other as i32 {
                        MSG_MOUSE_MOVE => Some(f::ohpl_h20_tb_on_mouse_move(v, t, d)),
                        MSG_MOUSE_DOWN => Some(f::ohpl_h20_tb_on_mouse_down(v, t, d)),
                        MSG_MOUSE_UP => Some(f::ohpl_h20_tb_on_mouse_up(v, t, d)),
                        MSG_MOUSE_DBL => Some(f::ohpl_h20_tb_on_mouse_double_click(v, t, d)),
                        MSG_MOUSE_ENTER => Some(true),
                        MSG_GOT_FOCUS => Some(f::ohpl_h20_tb_on_got_focus(v, t)),
                        MSG_GOT_TAB_FOCUS => Some(f::ohpl_h20_tb_on_got_tab_focus(v, t)),
                        MSG_LOST_FOCUS => Some(f::ohpl_h20_tb_on_lost_focus(v, t, d)),
                        MSG_KEY_PRESS => Some(f::ohpl_h20_tb_on_key_press(v, t, d)),
                        MSG_UI_BUTTON_PRESS => Some(f::ohpl_h20_tb_on_ui_button_press(v, t, d)),
                        _ => None,
                    };
                }
                _ => return None,
            },
            P::List(l) => match op {
                Op::ChangeSize => f::ohpl_h20_lb_on_change_size(v, l),
                Op::Draw => f::ohpl_h20_lb_on_draw(v, l),
                Op::DrawAfterClip => f::ohpl_h20_lb_on_draw_after_clip(v, l, other),
                Op::Virtual => {
                    return match other as i32 {
                        MSG_MOUSE_MOVE | MSG_MOUSE_UP => Some(true),
                        MSG_MOUSE_DOWN => Some(f::ohpl_h20_lb_on_mouse_down(v, l, d)),
                        MSG_MOUSE_DBL => Some(f::ohpl_h20_lb_on_mouse_double_click(v, l, d)),
                        MSG_MOUSE_ENTER | MSG_MOUSE_LEAVE | MSG_UI_ARROW_RELEASE => Some(false),
                        MSG_KEY_PRESS => Some(f::ohpl_h20_lb_on_key_press(v, l, d)),
                        MSG_UI_ARROW_PRESS => Some(f::ohpl_h20_lb_on_ui_arrow_press(v, l, d)),
                        MSG_UI_BUTTON_PRESS => Some(f::ohpl_h20_lb_on_ui_button_press(v, l, d)),
                        MSG_UI_BUTTON_RELEASE => Some(f::ohpl_h20_lb_on_ui_button_release(v, l)),
                        MSG_UI_BUTTON_DBL => {
                            Some(f::ohpl_h20_lb_on_ui_button_double_press(v, l, d))
                        }
                        _ => None,
                    };
                }
                _ => return None,
            },
            P::Combo(c) => match op {
                Op::ChangeSize => f::ohpl_h20_cb_on_change_size(v, c),
                Op::ChangeText => f::ohpl_h20_cb_on_change_text(v, c),
                Op::Draw => f::ohpl_h20_cb_on_draw(v, c),
                Op::PointIsInside => {
                    let p = d.pos();
                    return Some(f::ohpl_h20_cb_point_is_inside(v, c, p[0], p[1], fl != 0.0));
                }
                Op::Virtual => {
                    return match other as i32 {
                        MSG_MOUSE_MOVE => Some(f::ohpl_h20_cb_on_mouse_move(v, c, d)),
                        MSG_MOUSE_DOWN => Some(f::ohpl_h20_cb_on_mouse_down(v, c, d)),
                        MSG_MOUSE_UP => Some(f::ohpl_h20_cb_on_mouse_up(v, c, d)),
                        MSG_MOUSE_ENTER | MSG_MOUSE_LEAVE => Some(false),
                        MSG_UI_ARROW_PRESS => Some(f::ohpl_h20_cb_on_ui_arrow_press(v, c, d)),
                        MSG_UI_BUTTON_PRESS => Some(f::ohpl_h20_cb_on_ui_button_press(v, c, d)),
                        MSG_UI_BUTTON_RELEASE => Some(f::ohpl_h20_cb_on_ui_button_release(v, c, d)),
                        MSG_LOST_FOCUS => Some(f::ohpl_h20_cb_on_lost_focus(v, c, d)),
                        _ => None,
                    };
                }
                _ => return None,
            },
            P::TabFrame(t) => match op {
                Op::ChangeSize => f::ohpl_h20_tf_on_change_size(v, t),
                Op::Virtual if other as i32 == MSG_UI_BUTTON_PRESS => {
                    return Some(f::ohpl_h20_tf_on_ui_button_press(v, t, d));
                }
                _ => return None,
            },
            P::Tab(t) => match op {
                Op::ChangeText => f::ohpl_h20_tab_on_change_text(v, t),
                Op::ChangeSize => f::ohpl_h20_tab_on_change_size(v, t),
                Op::Draw => f::ohpl_h20_tab_on_draw(v, t),
                Op::RemoveChild => f::ohpl_h20_tab_attach_child(v, t, other, true),
                Op::PointIsInside => {
                    let p = d.pos();
                    return Some(f::ohpl_h20_tab_point_is_inside(v, t, p[0], p[1], fl != 0.0));
                }
                _ => return None,
            },
            P::TabLabel(l) => match op {
                Op::ChangeText => f::ohpl_h20_tl_on_change_text(v, l),
                Op::Draw => f::ohpl_h20_tl_on_draw(v, l),
                Op::Virtual => {
                    return match other as i32 {
                        MSG_MOUSE_DOWN => Some(f::ohpl_h20_tl_on_mouse_down(v, l)),
                        MSG_GET_UI_NAV_FOCUS => Some(f::ohpl_h20_tl_on_get_ui_nav_focus(v, l)),
                        _ => None,
                    };
                }
                _ => return None,
            },
        }
    }
    Some(false)
}

pub(crate) fn on_init(w: W) {
    let v = vt();
    // SAFETY: as in `call`.
    unsafe {
        match ptr(w) {
            Some(P::Text(t)) => f::ohpl_h20_tb_on_init(v, t),
            Some(P::List(l)) => f::ohpl_h20_lb_on_init(v, l),
            Some(P::Combo(c)) => f::ohpl_h20_cb_on_init(v, c),
            Some(P::TabFrame(t)) => f::ohpl_h20_tf_on_init(v, t),
            Some(P::Tab(t)) => f::ohpl_h20_tab_on_init(v, t),
            Some(P::TabLabel(_)) | None => {}
        }
    }
}

pub(crate) fn on_load_graphics(w: W) -> bool {
    let v = vt();
    // SAFETY: as in `call`.
    unsafe {
        match ptr(w) {
            Some(P::Text(t)) => f::ohpl_h20_tb_on_load_graphics(v, t),
            Some(P::List(l)) => f::ohpl_h20_lb_on_load_graphics(v, l),
            Some(P::Combo(c)) => f::ohpl_h20_cb_on_load_graphics(v, c),
            Some(P::TabFrame(t)) => f::ohpl_h20_tf_on_load_graphics(v, t),
            Some(P::Tab(_) | P::TabLabel(_)) => {}
            None => return false,
        }
    }
    true
}

/// `SetDefaultFontSize` overrides: the kinds that react once the base stored the size.
pub(crate) fn font_size_changed(w: W) {
    match ptr(w) {
        Some(P::Text(_)) => {
            call(w, Op::ChangeSize, 0, &Msg::zero(), 0.0);
        }
        Some(P::Combo(c)) => {
            let s = gw::read(w, |g| g.p.font_size);
            // SAFETY: as in `call`.
            unsafe { f::ohpl_h20_cb_set_default_font_size(vt(), c, s.x, s.y) };
        }
        _ => {}
    }
}

/// The state pointer for the C++ shell accessors.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gw_state(w: W) -> *mut c_void {
    match ptr(w) {
        Some(P::Text(t)) => t.cast(),
        Some(P::List(t)) => t.cast(),
        Some(P::Combo(t)) => t.cast(),
        Some(P::TabFrame(t)) => t.cast(),
        Some(P::Tab(t)) => t.cast(),
        Some(P::TabLabel(t)) => t.cast(),
        None => super::gw_menu::state(w) as *mut c_void,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gw_tb_kb_open(w: W) {
    if let Some(P::Text(t)) = ptr(w) {
        // SAFETY: as in `call`.
        unsafe { f::ohpl_h20_tb_ui_keyboard_open(vt(), t) };
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gw_tb_kb_close(w: W) {
    call(
        w,
        Op::Virtual,
        W::from(MSG_LOST_FOCUS as u8),
        &Msg::zero(),
        0.0,
    );
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gw_set_kind_ctr(w: W, id: i32, a: i32, ctr: usize) {
    set_kind(w, id, a, ctr);
}

/// The virtual `AttachChild`: true when a complex kind handled it.
pub(crate) fn attach_child(w: W, child: W) -> bool {
    if let Some(P::Tab(t)) = ptr(w) {
        // SAFETY: as in `call`.
        unsafe { f::ohpl_h20_tab_attach_child(vt(), t, child, false) };
        return true;
    }
    false
}

/// The container's virtual `UpdateProperties`.
pub(crate) fn update_properties(w: W) {
    // SAFETY: as in `call`.
    unsafe {
        match ptr(w) {
            Some(P::List(l)) => f::ohpl_h20_lb_update_properties(vt(), l),
            Some(P::Combo(c)) => f::ohpl_h20_cb_update_properties(vt(), c),
            _ => {}
        }
    }
}

// ---------------------------------------------------------------- creation

/// The `cGuiSet::CreateWidget*` of the complex kinds: shell, position, size, text, name, then the set.
///
/// # Safety
/// `set` is a live set; `text` and `name` are readable for their lengths.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gw_create_c(
    set: *mut std::ffi::c_void,
    kind: i32,
    pos: *const crate::types::Vector3f,
    size: *const crate::types::Vector2f,
    text: *const u32,
    tl: usize,
    parent: W,
    name: *const u8,
    nl: usize,
    a: i32,
    b: i32,
    num_add: f32,
) -> W {
    use super::gw_host as gh;
    use super::gw_kinds::{add_widget, set_name, shell};
    let set: *mut super::g1_set::Set = set.cast();
    // SAFETY: caller contract.
    let (pos, size, text, name) = unsafe {
        (
            pos.as_ref().copied().unwrap_or_default(),
            size.as_ref().copied().unwrap_or_default(),
            if text.is_null() {
                &[][..]
            } else {
                std::slice::from_raw_parts(text, tl)
            },
            if name.is_null() {
                &[][..]
            } else {
                std::slice::from_raw_parts(name, nl)
            },
        )
    };
    let sh = match kind {
        KIND_TEXTBOX => gh::SHELL_TEXTBOX,
        KIND_LIST => gh::SHELL_LIST,
        KIND_MPLIST => gh::SHELL_MPLIST,
        KIND_COMBO => gh::SHELL_COMBO,
        KIND_TABFRAME => gh::SHELL_TABFRAME,
        KIND_TAB => gh::SHELL_TAB,
        KIND_TABLABEL => gh::SHELL_TABLABEL,
        super::gw_menu::KIND_ITEM => gh::SHELL_MENUITEM,
        super::gw_menu::KIND_CTX => gh::SHELL_CTXMENU,
        super::gw_menu::KIND_MAIN => gh::SHELL_MAINMENU,
        _ => return 0,
    };
    let arg = if kind == KIND_TEXTBOX { a } else { 0 };
    let w = shell(
        sh,
        set,
        arg,
        if kind == KIND_TAB || kind == super::gw_menu::KIND_ITEM {
            parent
        } else {
            0
        },
    );
    match kind {
        KIND_TEXTBOX => {
            gw::set_position(w, pos);
            gw::set_size(w, size);
            if let Some(P::Text(t)) = ptr(w) {
                // SAFETY: the state is live.
                unsafe {
                    (*t).show_buttons = b != 0;
                    (*t).st.numeric_add = num_add;
                }
            }
            gw::set_text(w, text);
        }
        super::gw_menu::KIND_MAIN => {}
        _ => {
            gw::set_position(w, pos);
            gw::set_size(w, size);
            if matches!(
                kind,
                KIND_COMBO
                    | KIND_TABFRAME
                    | KIND_TAB
                    | KIND_TABLABEL
                    | super::gw_menu::KIND_ITEM
                    | super::gw_menu::KIND_CTX
            ) {
                gw::set_text(w, text);
            }
            if kind == KIND_TABFRAME
                && let Some(P::TabFrame(f)) = ptr(w)
            {
                // SAFETY: as in `call`.
                unsafe {
                    f::ohpl_h20_tf_set_hori_scroll(vt(), f, a != 0);
                    f::ohpl_h20_tf_set_vert_scroll(vt(), f, b != 0);
                }
            }
        }
    }
    set_name(w, name);
    add_widget(set, w, parent);
    w
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gw_h20_vt() -> *const HostVt {
    vt()
}
