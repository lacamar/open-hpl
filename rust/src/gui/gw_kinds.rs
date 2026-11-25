//! Widget kinds written in Rust (button, check box, label, image, dummy, group, slider, frame,
//! window) on top of the `Gw` base, using the kernels in `widgets_base` / `widgets_slider` /
//! `widget`, and the complex kinds of `gw_h20` / `gw_menu`.

#![allow(clippy::indexing_slicing, clippy::too_many_arguments)] // fixed-size tables and op counts bounded by their buffers

use std::ffi::c_void;
use std::ptr;

use super::g1_gfx::Element;
use super::g1_gui;
use super::g1_set;
use super::gw::{self, GfxTable, GwHost, read, with};
use super::gw_host::{self, host};
use super::h19_widget::{self as hw, Op};
use super::set::{Msg, W};
use super::widget::{self as wd, Op as DrawOp, Ops};
use super::widgets_base as wb;
use super::widgets_slider::{self as ws, SliderState};
use crate::types::{Color, Vector2f, Vector3f};

const MSG_MOUSE_MOVE: i32 = 0;
const MSG_MOUSE_DOWN: i32 = 1;
const MSG_MOUSE_UP: i32 = 2;
const MSG_MOUSE_DBL: i32 = 3;
const MSG_MOUSE_LEAVE: i32 = 5;
const MSG_GOT_FOCUS: i32 = 6;
const MSG_GOT_TAB_FOCUS: i32 = 7;
const MSG_LOST_FOCUS: i32 = 8;
const MSG_BUTTON_PRESSED: i32 = 11;
const MSG_CHECK_CHANGE: i32 = 13;
const MSG_KEY_PRESS: i32 = 14;
const MSG_UI_ARROW_PRESS: i32 = 17;
const MSG_UI_ARROW_RELEASE: i32 = 18;
const MSG_UI_BUTTON_PRESS: i32 = 19;
const MSG_UI_BUTTON_RELEASE: i32 = 20;
const MSG_SLIDER_MOVE: i32 = 24;
const MSG_SLIDER_RELEASE: i32 = 25;
const MSG_WINDOW_CLOSE: i32 = 31;

const TYPE_MAIN_MENU: i32 = 12;
const FLAG_CLOSE_BUTTON: i32 = 1;

pub const KIND_PLAIN: i32 = 10;
pub const KIND_BUTTON: i32 = 1;
pub const KIND_CHECK: i32 = 2;
pub const KIND_LABEL: i32 = 3;
pub const KIND_IMAGE: i32 = 4;
pub const KIND_DUMMY: i32 = 5;
pub const KIND_GROUP: i32 = 6;
pub const KIND_SLIDER: i32 = 7;
pub const KIND_FRAME: i32 = 8;
pub const KIND_WINDOW: i32 = 9;

type Gfx = *mut Element;

const WHITE: Color = Color {
    r: 1.0,
    g: 1.0,
    b: 1.0,
    a: 1.0,
};

#[derive(Clone, Copy)]
pub struct Fr {
    bg: Gfx,
    borders: [Gfx; 4],
    corners: [Gfx; 4],
}

impl Fr {
    fn none() -> Fr {
        Fr {
            bg: ptr::null_mut(),
            borders: [ptr::null_mut(); 4],
            corners: [ptr::null_mut(); 4],
        }
    }
    fn load(skin: *mut super::g1_skin::Skin, prefix: &str) -> Fr {
        let g = |s: &str| gw::skin_gfx(skin, &format!("{prefix}{s}"));
        Fr {
            bg: g("Background"),
            borders: [
                g("BorderRight"),
                g("BorderLeft"),
                g("BorderUp"),
                g("BorderDown"),
            ],
            corners: [g("CornerLU"), g("CornerRU"), g("CornerRD"), g("CornerLD")],
        }
    }
}

pub struct Button {
    pressed: bool,
    toggleable: bool,
    image: Gfx,
    destroy_image: bool,
    timer: f32,
    repeat_active: bool,
    repeat_period: f32,
    repeat_times: i32,
    pressed_data: Msg,
    up: Fr,
    down: Fr,
}

pub struct Check {
    checked: bool,
    pressed: bool,
    boxes: [[Gfx; 2]; 2],
    box_size: Vector2f,
    label: W,
}

pub struct Label {
    align: i32,
    word_wrap: bool,
    rows_height: f32,
    wrap_offset: f32,
    scroll_speed_mul: f32,
    scroll_timer: f32,
    wait_to_scroll: f32,
    scrolling_down: bool,
    bg: Gfx,
    draw_bg: bool,
    bg_color: Color,
    max_chars: i32,
    autogen: bool,
}

pub struct Image {
    image: Gfx,
}

pub struct Dummy {
    enclose: bool,
}

pub struct Group {
    header: W,
    fr: Fr,
    header_offset: f32,
    header_breaks: bool,
}

pub struct Slider {
    s: SliderState,
    buttons: [W; 2],
    handle: Fr,
    arrows: [Gfx; 2],
    bg: Gfx,
}

pub struct Frame {
    draw_frame: bool,
    draw_bg: bool,
    bg_z: f32,
    bg_color: Color,
    fr: Fr,
    max_coord: Vector3f,
    hs: W,
    vs: W,
    need_update: bool,
    scroll_updated: bool,
    margin: Vector2f,
}

pub struct Window {
    label_font: *mut super::g1_skin::SkinFont,
    button: W,
    bg: Gfx,
    label_gfx: Gfx,
    fr: Fr,
    cross: Gfx,
    label_offset: Vector3f,
    flags: i32,
    is_static: bool,
    close_disables: bool,
    close_on_escape: bool,
    draw_label: bool,
    focused: bool,
    rel_mouse: Vector3f,
    moving: bool,
    prev_attention: W,
    button_size: f32,
    borders_outside: bool,
}

pub enum Kind {
    Plain,
    Button(Button),
    Check(Check),
    Label(Label),
    Image(Image),
    Dummy(Dummy),
    Group(Group),
    Slider(Slider),
    Frame(Frame),
    Window(Window),
    Text(Box<super::h20_text::TextBox>),
    List(Box<super::h20_list::ListBase>),
    Combo(Box<super::h20_combo::Combo>),
    TabFrame(Box<super::h20_tab::Frame>),
    Tab(Box<super::h20_tab::Tab>),
    TabLabel(Box<super::h20_tab::Label>),
    Ctx(Box<super::gw_menu::CtxK>),
    Main(Box<super::gw_menu::MainK>),
    Item(Box<super::gw_menu::ItemK>),
}

#[derive(Clone, Copy, PartialEq, Eq)]
enum Tag {
    Plain,
    Button,
    Check,
    Label,
    Image,
    Dummy,
    Group,
    Slider,
    Frame,
    Window,
    H20,
}

fn tag(w: W) -> Tag {
    with(w, |g| match g.kind {
        Kind::Plain => Tag::Plain,
        Kind::Button(_) => Tag::Button,
        Kind::Check(_) => Tag::Check,
        Kind::Label(_) => Tag::Label,
        Kind::Image(_) => Tag::Image,
        Kind::Dummy(_) => Tag::Dummy,
        Kind::Group(_) => Tag::Group,
        Kind::Slider(_) => Tag::Slider,
        Kind::Frame(_) => Tag::Frame,
        Kind::Window(_) => Tag::Window,
        Kind::Text(_)
        | Kind::List(_)
        | Kind::Combo(_)
        | Kind::TabFrame(_)
        | Kind::Tab(_)
        | Kind::TabLabel(_)
        | Kind::Ctx(_)
        | Kind::Main(_)
        | Kind::Item(_) => Tag::H20,
    })
    .unwrap_or(Tag::Plain)
}

macro_rules! kind {
    ($w:expr, $v:ident, |$k:ident| $e:expr) => {
        with($w, |g| match &mut g.kind {
            Kind::$v($k) => Some($e),
            _ => None,
        })
        .flatten()
    };
}

fn v2(x: f32, y: f32) -> Vector2f {
    Vector2f { x, y }
}

use crate::math::v3;

// ---------------------------------------------------------------- dispatch

pub(crate) fn call(w: W, op: Op, other: W, d: &Msg, f: f32) -> bool {
    let t = tag(w);
    if t == Tag::H20 {
        if let Some(r) = super::gw_h20::call(w, op, other, d, f) {
            return r;
        }
        if let Some(r) = super::gw_menu::call(w, op, other, d, f) {
            return r;
        }
    }
    match op {
        Op::OnMessage => false,
        Op::Virtual => virtual_msg(t, w, other as i32, d),
        Op::ChangePosition => {
            if t == Tag::Slider {
                slider_update_bar(w);
            }
            false
        }
        Op::ChangeSize => {
            match t {
                Tag::Check => check_change_size(w),
                Tag::Slider => slider_change_size(w),
                Tag::Frame => frame_change_size(w),
                Tag::Window => window_change_size(w),
                _ => {}
            }
            false
        }
        Op::ChangeText => {
            match t {
                Tag::Check => check_update_label(w),
                Tag::Label => label_change_text(w),
                Tag::Group => group_change_text(w),
                _ => {}
            }
            false
        }
        Op::ChildUpdate => {
            match t {
                Tag::Dummy => dummy_child_update(w),
                Tag::Frame => {
                    kind!(w, Frame, |k| k.need_update = true);
                }
                _ => {}
            }
            false
        }
        Op::OnAttachChild => {
            match t {
                Tag::Frame => {
                    kind!(w, Frame, |k| k.need_update = true);
                }
                Tag::Window => window_attach(w, other),
                _ => {}
            }
            false
        }
        Op::OnRemoveChild => {
            if t == Tag::Frame {
                kind!(w, Frame, |k| k.need_update = true);
            }
            false
        }
        Op::RemoveChild => {
            gw::remove_child_base(w, other);
            false
        }
        Op::ChildGotFocus | Op::ChildGotTabFocus | Op::ChildLostFocus => {
            if t == Tag::Window {
                match op {
                    Op::ChildGotFocus => window_child_got_focus(w),
                    Op::ChildLostFocus => window_child_lost_focus(w),
                    _ => hw::forward_to_parent(&mut GwHost, w, op, other, d),
                }
            } else {
                hw::forward_to_parent(&mut GwHost, w, op, other, d);
            }
            false
        }
        Op::Update => {
            match t {
                Tag::Button => button_update(w, f),
                Tag::Label => label_update(w, f),
                Tag::Frame => frame_update(w),
                _ => {}
            }
            false
        }
        Op::Draw => {
            match t {
                Tag::Button => button_draw(w),
                Tag::Check => check_draw(w),
                Tag::Label => label_draw(w, false),
                Tag::Image => image_draw(w),
                Tag::Group => group_draw(w),
                Tag::Slider => slider_draw(w),
                Tag::Frame => frame_draw(w, false),
                Tag::Window => window_draw(w),
                _ => {}
            }
            false
        }
        Op::DrawAfterClip => {
            match t {
                Tag::Label => label_draw(w, true),
                Tag::Frame => frame_draw(w, true),
                _ => {}
            }
            false
        }
        Op::PointIsInside => gw::point_is_inside_base(
            w,
            {
                let p = d.pos();
                v2(p[0], p[1])
            },
            f != 0.0,
        ),
    }
}

fn virtual_msg(t: Tag, w: W, msg: i32, d: &Msg) -> bool {
    let enabled = read(w, |g| g.base.enabled);
    match (t, msg) {
        (_, MSG_GOT_FOCUS | MSG_GOT_TAB_FOCUS)
            if !matches!(t, Tag::Window) || msg != MSG_GOT_FOCUS =>
        {
            enabled
        }
        (Tag::Button, _) => button_msg(w, msg, d),
        (Tag::Check, _) => check_msg(w, msg, d),
        (Tag::Image, MSG_MOUSE_MOVE) => true,
        (Tag::Image, MSG_MOUSE_DOWN | MSG_MOUSE_UP) => wb::image_mouse(d.val()),
        (Tag::Frame, _) => frame_msg(w, msg, d),
        (Tag::Slider, _) => slider_msg(w, msg, d),
        (Tag::Window, _) => window_msg(w, msg, d),
        _ => false,
    }
}

/// `AttachChild` virtual: no Rust kind overrides it.
pub(crate) fn attach_child(w: W, child: W) {
    if super::gw_h20::attach_child(w, child) {
        return;
    }
    gw::attach_child_base(w, child);
}

pub(crate) fn set_default_font_size(w: W, s: Vector2f) {
    match tag(w) {
        Tag::H20 => {
            with(w, |g| g.p.font_size = s);
            super::gw_h20::font_size_changed(w);
        }
        t => {
            with(w, |g| g.p.font_size = s);
            match t {
                Tag::Check => {
                    let l = kind!(w, Check, |k| k.label).unwrap_or(0);
                    gw::set_default_font_size(l, s);
                    check_update_label(w);
                }
                Tag::Label => label_change_text(w),
                Tag::Group => {
                    let hd = kind!(w, Group, |k| k.header).unwrap_or(0);
                    gw::set_default_font_size(hd, s);
                }
                _ => {}
            }
        }
    }
}

pub(crate) fn set_default_font_type(w: W, f: usize) {
    with(w, |g| g.font_type = f);
    if tag(w) == Tag::Check {
        let l = kind!(w, Check, |k| k.label).unwrap_or(0);
        gw::set_default_font_type(l, f);
        check_update_label(w);
    }
}

pub(crate) fn set_default_font_color(w: W, c: Color) {
    with(w, |g| g.p.font_color = c);
    if tag(w) == Tag::Check {
        let l = kind!(w, Check, |k| k.label).unwrap_or(0);
        gw::set_default_font_color(l, c);
    }
}

pub(crate) fn on_load_graphics(w: W) {
    if super::gw_h20::on_load_graphics(w) || super::gw_menu::on_load_graphics(w) {
        return;
    }
    let skin = gw::skin_of(w);
    match tag(w) {
        Tag::Button => {
            let up = Fr::load(skin, "ButtonUp");
            let down = Fr::load(skin, "ButtonDown");
            kind!(w, Button, |k| {
                k.up = up;
                k.down = down;
            });
        }
        Tag::Check => {
            let g = |n: &str| gw::skin_gfx(skin, n);
            let b = [
                [g("CheckBoxDisabledUnchecked"), g("CheckBoxDisabledChecked")],
                [g("CheckBoxEnabledUnchecked"), g("CheckBoxEnabledChecked")],
            ];
            kind!(w, Check, |k| k.boxes = b);
        }
        Tag::Label => {
            let bg = gw::skin_gfx(skin, "FrameBackground");
            kind!(w, Label, |k| k.bg = bg);
        }
        Tag::Group => {
            let fr = Fr::load(skin, "Group");
            let off = gw::skin_attr(skin, "GroupHeaderOffset").x;
            let over = gw::skin_attr(skin, "GroupHeaderOverUpperBorder").x == 0.0;
            kind!(w, Group, |k| {
                k.fr = fr;
                k.header_offset = off;
                k.header_breaks = over;
            });
        }
        Tag::Slider => {
            let handle = Fr::load(skin, "SliderHandle");
            let vertical = kind!(w, Slider, |k| k.s.vertical).unwrap_or(false);
            let (a0, a1, bg) = if vertical {
                (
                    gw::skin_gfx(skin, "SliderVertArrowUp"),
                    gw::skin_gfx(skin, "SliderVertArrowDown"),
                    gw::skin_gfx(skin, "SliderVertBackground"),
                )
            } else {
                (
                    gw::skin_gfx(skin, "SliderHoriArrowUp"),
                    gw::skin_gfx(skin, "SliderHoriArrowDown"),
                    gw::skin_gfx(skin, "SliderHoriBackground"),
                )
            };
            let size = gw::skin_attr(skin, "SliderButtonSize").x;
            kind!(w, Slider, |k| {
                k.handle = handle;
                k.arrows = [a0, a1];
                k.bg = bg;
                k.s.button_size = size;
            });
        }
        Tag::Frame => {
            let mut fr = Fr::load(skin, "Frame");
            fr.bg = gw::skin_gfx(skin, "FrameBackground");
            kind!(w, Frame, |k| k.fr = fr);
        }
        Tag::Window => {
            let mut fr = Fr::load(skin, "Window");
            fr.bg = gw::skin_gfx(skin, "WindowBackground");
            let lg = gw::skin_gfx(skin, "WindowLabel");
            let font = gw::skin_font(skin, "WindowLabel");
            let off = gw::skin_attr(skin, "WindowLabelTextOffset");
            let cross = gw::skin_gfx(skin, "WindowButtonCross");
            let bs = gw::skin_attr(skin, "WindowButtonSize").x;
            let out = gw::skin_attr(skin, "WindowBordersOutside").x == 1.0;
            kind!(w, Window, |k| {
                k.fr = fr;
                k.bg = fr.bg;
                k.label_gfx = lg;
                k.label_font = font;
                k.label_offset = off;
                k.cross = cross;
                k.button_size = bs;
                k.borders_outside = out;
            });
        }
        _ => {}
    }
}

pub(crate) fn on_init(w: W) {
    match tag(w) {
        Tag::H20 => {
            super::gw_h20::on_init(w);
            super::gw_menu::on_init(w);
        }
        Tag::Check => check_init(w),
        Tag::Group => group_init(w),
        Tag::Slider => slider_init(w),
        Tag::Window => window_init(w),
        _ => {}
    }
}

/// Subclass destructor work of the Rust kinds.
pub(crate) fn teardown(w: W) {
    let set = gw::set_of(w);
    let destroying = g1_set::is_destroying(set);
    super::gw_h20::teardown(w);
    super::gw_menu::teardown(w);
    let kill = |c: W| {
        if c != 0 && !destroying {
            g1_set::destroy_widget(set, c, false);
        }
    };
    match tag(w) {
        Tag::Button => {
            let (img, d) = kind!(w, Button, |k| (k.image, k.destroy_image))
                .unwrap_or((ptr::null_mut(), false));
            if !img.is_null() && d {
                g1_gui::destroy_gfx(gw::gui_of(w), img);
            }
        }
        Tag::Check => kill(kind!(w, Check, |k| k.label).unwrap_or(0)),
        Tag::Group => kill(kind!(w, Group, |k| k.header).unwrap_or(0)),
        Tag::Slider => {
            for b in kind!(w, Slider, |k| k.buttons).unwrap_or([0; 2]) {
                kill(b);
            }
        }
        Tag::Frame => {
            let (h, v) = kind!(w, Frame, |k| (k.hs, k.vs)).unwrap_or((0, 0));
            kill(h);
            kill(v);
        }
        Tag::Window => {
            // SAFETY: the set is live while its widgets are destroyed.
            unsafe { g1_set::ohpl_g1_set_remove_window(set, w) };
            kill(kind!(w, Window, |k| k.button).unwrap_or(0));
        }
        _ => {}
    }
}

// ---------------------------------------------------------------- creation

pub(crate) fn set_kind(w: W, id: i32, a: i32, b: i32) {
    if id >= super::gw_menu::KIND_ITEM {
        super::gw_menu::set_kind(w, id, b as usize);
        return;
    }
    if id >= super::gw_h20::KIND_TEXTBOX {
        super::gw_h20::set_kind(w, id, a, b as usize);
        return;
    }
    let skin = gw::skin_of(w);
    let kind = match id {
        KIND_PLAIN => Kind::Plain,
        KIND_BUTTON => Kind::Button(Button {
            pressed: false,
            toggleable: false,
            image: ptr::null_mut(),
            destroy_image: false,
            timer: 0.0,
            repeat_active: false,
            repeat_period: 1.0,
            repeat_times: 0,
            pressed_data: Msg::zero(),
            up: Fr::none(),
            down: Fr::none(),
        }),
        KIND_CHECK => Kind::Check(Check {
            checked: false,
            pressed: false,
            boxes: [[ptr::null_mut(); 2]; 2],
            box_size: v2(0.0, 0.0),
            label: 0,
        }),
        KIND_LABEL => Kind::Label(Label {
            align: 0,
            word_wrap: false,
            rows_height: 0.0,
            wrap_offset: 0.0,
            scroll_speed_mul: 1.0,
            scroll_timer: 0.0,
            wait_to_scroll: 1.0,
            scrolling_down: true,
            bg: ptr::null_mut(),
            draw_bg: false,
            bg_color: WHITE,
            max_chars: -1,
            autogen: false,
        }),
        KIND_IMAGE => Kind::Image(Image {
            image: ptr::null_mut(),
        }),
        KIND_DUMMY => Kind::Dummy(Dummy { enclose: true }),
        KIND_GROUP => Kind::Group(Group {
            header: 0,
            fr: Fr::none(),
            header_offset: 0.0,
            header_breaks: false,
        }),
        KIND_SLIDER => Kind::Slider(Slider {
            s: SliderState::new(a == 1, gw::skin_attr(skin, "SliderButtonSize").x),
            buttons: [0; 2],
            handle: Fr::none(),
            arrows: [ptr::null_mut(); 2],
            bg: ptr::null_mut(),
        }),
        KIND_FRAME => Kind::Frame(Frame {
            draw_frame: false,
            draw_bg: false,
            bg_z: -0.5,
            bg_color: WHITE,
            fr: Fr::none(),
            max_coord: v3(-999999.9, -999999.9, -999999.9),
            hs: 0,
            vs: 0,
            need_update: true,
            scroll_updated: true,
            margin: v2(0.0, 0.0),
        }),
        KIND_WINDOW => Kind::Window(Window {
            label_font: ptr::null_mut(),
            button: 0,
            bg: ptr::null_mut(),
            label_gfx: ptr::null_mut(),
            fr: Fr::none(),
            cross: ptr::null_mut(),
            label_offset: v3(0.0, 0.0, 0.0),
            flags: a,
            is_static: false,
            close_disables: true,
            close_on_escape: false,
            draw_label: true,
            focused: false,
            rel_mouse: v3(0.0, 0.0, 0.0),
            moving: false,
            prev_attention: 0,
            button_size: 0.0,
            borders_outside: false,
        }),
        _ => Kind::Plain,
    };
    with(w, |g| g.kind = kind);
    match id {
        KIND_CHECK => {
            gw::load_graphics(w);
            let set = gw::set_of(w);
            let l = create_label(set, v3(0.0, 0.0, 0.0), v2(-1.0, -1.0), &[], w, b"");
            kind!(w, Check, |k| k.label = l);
            label_set_autogen(l, true);
        }
        KIND_LABEL | KIND_GROUP | KIND_SLIDER | KIND_WINDOW => gw::load_graphics(w),
        KIND_FRAME => {
            with(w, |g| g.base.clips = true);
            let set = gw::set_of(w);
            let mk = |orient: i32, size: Vector2f| {
                let s = create_slider(set, orient, v3(0.0, 0.0, 0.0), size, 0, w, b"");
                gw::set_affected_by_scroll(s, false);
                add_callback(s, MSG_SLIDER_MOVE, w, frame_slider_move);
                gw::set_visible(s, false);
                gw::set_enabled(s, false);
                s
            };
            if a != 0 {
                let s = mk(0, v2(0.0, 18.0));
                kind!(w, Frame, |k| k.hs = s);
            }
            if b != 0 {
                let s = mk(1, v2(18.0, 0.0));
                kind!(w, Frame, |k| k.vs = s);
            }
        }
        _ => {}
    }
}

fn add_callback(w: W, msg: i32, ctx: W, f: extern "C" fn(*mut c_void, W, *const Msg) -> bool) {
    gw::add_callback(w, msg, ctx as *mut c_void, f);
}

pub(crate) fn shell(kind: i32, set: *mut super::g1_set::Set, a: i32, b: usize) -> W {
    host().map_or(0, |h| {
        // SAFETY: the host constructs a shell whose constructor calls `ohpl_gw_new`.
        unsafe {
            (h.shell_new)(
                kind,
                set.cast(),
                super::g1_set::ohpl_g1_set_skin(set).cast(),
                a,
                b,
            )
        }
    })
}

pub(crate) fn add_widget(set: *mut super::g1_set::Set, w: W, parent: W) {
    // SAFETY: live set and widgets.
    unsafe { g1_set::ohpl_g1_set_add_widget(set, w, parent) };
}

pub(crate) fn label_set_bg_color(w: W, c: Color) {
    kind!(w, Label, |k| k.bg_color = c);
}

pub(crate) fn create_menu_item(set: *mut super::g1_set::Set, text: &[u32], parent: W) -> W {
    let w = shell(gw_host::SHELL_MENUITEM, set, 0, parent);
    gw::set_position(w, v3(0.0, 0.0, 0.0));
    gw::set_size(w, v2(0.0, 0.0));
    gw::set_text(w, text);
    set_name(w, b"");
    add_widget(set, w, parent);
    w
}

pub(crate) fn create_context_menu(set: *mut super::g1_set::Set, parent: W) -> W {
    let w = shell(gw_host::SHELL_CTXMENU, set, 0, 0);
    gw::set_position(w, v3(0.0, 0.0, 0.0));
    gw::set_size(w, v2(0.0, 0.0));
    gw::set_text(w, &[]);
    set_name(w, b"");
    add_widget(set, w, parent);
    w
}

pub(crate) fn create_label(
    set: *mut super::g1_set::Set,
    pos: Vector3f,
    size: Vector2f,
    text: &[u32],
    parent: W,
    name: &[u8],
) -> W {
    let w = shell(gw_host::SHELL_LABEL, set, 0, 0usize);
    gw::set_position(w, pos);
    gw::set_size(w, size);
    gw::set_text(w, text);
    set_name(w, name);
    if size.x == -1.0 && size.y == -1.0 {
        label_set_autogen(w, true);
        label_set_autogen(w, false);
    }
    add_widget(set, w, parent);
    w
}

pub(crate) fn create_button(
    set: *mut super::g1_set::Set,
    pos: Vector3f,
    size: Vector2f,
    text: &[u32],
    parent: W,
    toggleable: bool,
    name: &[u8],
) -> W {
    let w = shell(gw_host::SHELL_BUTTON, set, 0, 0);
    gw::set_position(w, pos);
    gw::set_size(w, size);
    gw::set_text(w, text);
    set_name(w, name);
    kind!(w, Button, |k| k.toggleable = toggleable);
    add_widget(set, w, parent);
    w
}

pub(crate) fn create_slider(
    set: *mut super::g1_set::Set,
    orient: i32,
    pos: Vector3f,
    size: Vector2f,
    max: i32,
    parent: W,
    name: &[u8],
) -> W {
    let w = shell(gw_host::SHELL_SLIDER, set, orient, 0);
    gw::set_position(w, pos);
    gw::set_size(w, size);
    slider_set_max(w, max);
    set_name(w, name);
    add_widget(set, w, parent);
    w
}

pub(crate) fn create_frame(
    set: *mut super::g1_set::Set,
    pos: Vector3f,
    size: Vector2f,
    draw_frame: bool,
    parent: W,
    h: bool,
    v: bool,
    name: &[u8],
) -> W {
    let w = shell(gw_host::SHELL_FRAME, set, i32::from(h), usize::from(v));
    gw::set_position(w, pos);
    gw::set_size(w, size);
    kind!(w, Frame, |k| k.draw_frame = draw_frame);
    set_name(w, name);
    add_widget(set, w, parent);
    w
}

pub(crate) fn set_name(w: W, n: &[u8]) {
    with(w, |g| g.name = n.to_vec());
}

// ---------------------------------------------------------------- button

fn run(w: W, ops: &[DrawOp], t: &GfxTable, texts: &[&[u32]], fonts: &[usize]) {
    gw::run_ops(w, ops, &t.gfx, texts, fonts);
}

fn button_draw(w: W) {
    let Some((pressed, image, up, down)) = kind!(w, Button, |k| (k.pressed, k.image, k.up, k.down))
    else {
        return;
    };
    let skin = gw::skin_of(w);
    let (font, fsize, fcolor, cmul, text, enabled_font) = read(w, |g| {
        (
            g.font_type,
            g.p.font_size,
            g.p.font_color,
            g.p.color_mul,
            g.text.clone(),
            g.font_type != 0,
        )
    });
    let global = gw::global_position(w);
    let size = gw::size(w);
    let fr = if pressed { down } else { up };
    let d = wb::ButtonDraw {
        global,
        size,
        pressed,
        enabled: gw::is_enabled(w),
        has_image: !image.is_null(),
        has_default_font: enabled_font,
        pressed_offset: if pressed {
            gw::skin_attr(skin, "ButtonPressedContentOffset")
        } else {
            Vector3f::default()
        },
        font_size: fsize,
        font_color: fcolor,
        color_mul: cmul,
        has_background: !fr.bg.is_null(),
    };
    let mut t = GfxTable::new();
    t.set_frame(fr.bg, &fr.borders, &fr.corners);
    t.set(wd::GFX_IMAGE as usize, image);
    let mut buf = [DrawOp::default(); 16];
    let mut o = Ops::new(&mut buf);
    wb::button_draw(&mut o, &d, &t.m);
    let n = o.len();
    run(w, &buf[..n], &t, &[&text], &[font]);
}

fn button_state(k: &Button) -> wb::ButtonState {
    wb::ButtonState {
        toggleable: k.toggleable,
        pressed: k.pressed,
        repeat_active: k.repeat_active,
        repeat_times: k.repeat_times,
        timer: k.timer,
        repeat_period: k.repeat_period,
    }
}

fn button_update(w: W, dt: f32) {
    let Some(st) = kind!(w, Button, |k| button_state(k)) else {
        return;
    };
    let act = wb::button_update(st, dt);
    let data = kind!(w, Button, |k| {
        k.timer = act.state.timer;
        k.repeat_times = act.state.repeat_times;
        k.pressed_data
    });
    if act.fire
        && let Some(d) = data
    {
        gw::process_message(w, MSG_BUTTON_PRESSED, &d, false, false);
    }
}

fn button_msg(w: W, msg: i32, d: &Msg) -> bool {
    match msg {
        MSG_MOUSE_MOVE | MSG_MOUSE_DBL => true,
        MSG_MOUSE_DOWN => button_down(w, d),
        MSG_MOUSE_UP => button_up(w, d),
        MSG_MOUSE_LEAVE | MSG_LOST_FOCUS => {
            kind!(w, Button, |k| {
                if !k.toggleable {
                    k.pressed = false;
                }
            });
            false
        }
        MSG_UI_BUTTON_PRESS => {
            if gw::has_focus(w) && wb::ui_primary(d.val()) {
                return button_down(w, &Msg::with_val(1));
            }
            false
        }
        MSG_UI_BUTTON_RELEASE => {
            if gw::has_focus(w) && wb::ui_primary(d.val()) {
                let prev = read(w, |g| g.base.mouse_over);
                with(w, |g| g.base.mouse_over = true);
                button_up(w, &Msg::with_val(1));
                with(w, |g| g.base.mouse_over = prev);
                return true;
            }
            false
        }
        _ => false,
    }
}

fn button_down(w: W, d: &Msg) -> bool {
    let enabled = gw::is_enabled(w);
    let Some(st) = kind!(w, Button, |k| button_state(k)) else {
        return false;
    };
    let act = wb::button_mouse_down(st, enabled, d.val());
    if !act.ret {
        return false;
    }
    kind!(w, Button, |k| k.pressed = act.state.pressed);
    if act.fire {
        gw::process_message(w, MSG_BUTTON_PRESSED, d, false, false);
    }
    if act.store_data {
        kind!(w, Button, |k| k.pressed_data = *d);
    }
    true
}

fn button_up(w: W, d: &Msg) -> bool {
    let enabled = gw::is_enabled(w);
    let over = read(w, |g| g.base.mouse_over);
    let Some(st) = kind!(w, Button, |k| button_state(k)) else {
        return false;
    };
    let act = wb::button_mouse_up(st, enabled, d.val(), over);
    if act.fire {
        gw::process_message(w, MSG_BUTTON_PRESSED, d, false, false);
    }
    kind!(w, Button, |k| {
        k.pressed = act.state.pressed;
        k.repeat_times = act.state.repeat_times;
    });
    act.ret
}

// ---------------------------------------------------------------- check box

fn check_active(w: W) -> Vector2f {
    let g = kind!(w, Check, |k| k.boxes[0][0]).unwrap_or(ptr::null_mut());
    gw::metrics(g).active
}

fn check_init(w: W) {
    let (label, bsize) = kind!(w, Check, |k| (k.label, k.box_size)).unwrap_or((0, v2(0.0, 0.0)));
    let fh = read(w, |g| g.p.font_size.y);
    let p = wb::check_label_pos(check_active(w).x, bsize.y, fh);
    gw::set_position(label, p);
    add_callback(label, MSG_MOUSE_DOWN, w, check_label_down);
    add_callback(label, MSG_MOUSE_UP, w, check_label_up);
}

extern "C" fn check_label_down(ctx: *mut c_void, _w: W, d: *const Msg) -> bool {
    check_label_fwd(ctx as W, d, MSG_MOUSE_DOWN)
}

extern "C" fn check_label_up(ctx: *mut c_void, _w: W, d: *const Msg) -> bool {
    check_label_fwd(ctx as W, d, MSG_MOUSE_UP)
}

fn check_label_fwd(w: W, d: *const Msg, msg: i32) -> bool {
    // SAFETY: the base passes a live message.
    let d = unsafe { d.as_ref() }.copied().unwrap_or(Msg::zero());
    let enabled = read(w, |g| g.base.enabled);
    let (ret, fwd) = wb::check_label_forward(d.val(), enabled);
    if fwd {
        gw::process_message(w, msg, &d, false, false);
    }
    ret
}

fn check_update_label(w: W) {
    let (label, text) = (kind!(w, Check, |k| k.label).unwrap_or(0), gw::text(w));
    gw::set_text(label, &text);
    check_change_size(w);
}

fn check_change_size(w: W) {
    let label = kind!(w, Check, |k| k.label).unwrap_or(0);
    if label == 0 {
        return;
    }
    let lsize = gw::size(label);
    let lx = read(label, |g| g.base.pos.x);
    let fh = read(w, |g| g.p.font_size.y);
    let l = wb::check_layout(check_active(w).x, fh, lx, lsize);
    kind!(w, Check, |k| k.box_size = l.box_size);
    with(w, |g| g.base.size = l.size);
}

fn check_draw(w: W) {
    let Some((bsize, boxes)) = kind!(w, Check, |k| (k.box_size, k.boxes)) else {
        return;
    };
    let global = gw::global_position(w);
    let pos = wb::check_box_pos(global, bsize.y, check_active_h(&boxes));
    let en = usize::from(gw::is_enabled(w));
    let checked = usize::from(kind!(w, Check, |k| k.checked).unwrap_or(false));
    let set = gw::set_of(w);
    g1_set::draw_gfx_default(set, boxes[en][checked], pos, v2(-1.0, -1.0), WHITE);
}

fn check_active_h(boxes: &[[Gfx; 2]; 2]) -> f32 {
    gw::metrics(boxes[0][0]).active.y
}

fn check_msg(w: W, msg: i32, d: &Msg) -> bool {
    match msg {
        MSG_MOUSE_MOVE => true,
        MSG_MOUSE_DOWN => {
            if !wb::check_mouse_down(d.val()) {
                return false;
            }
            kind!(w, Check, |k| k.pressed = true);
            true
        }
        MSG_MOUSE_UP => {
            if kind!(w, Check, |k| k.pressed).unwrap_or(false) {
                let c = kind!(w, Check, |k| k.checked).unwrap_or(false);
                check_set_checked(w, !c, true);
            }
            kind!(w, Check, |k| k.pressed = false);
            true
        }
        MSG_MOUSE_LEAVE => {
            kind!(w, Check, |k| k.pressed = false);
            false
        }
        MSG_UI_BUTTON_PRESS => {
            if gw::has_focus(w) && wb::ui_primary(d.val()) {
                return check_msg(w, MSG_MOUSE_DOWN, &Msg::with_val(1));
            }
            false
        }
        MSG_UI_BUTTON_RELEASE => {
            if gw::has_focus(w) && wb::ui_primary(d.val()) {
                return check_msg(w, MSG_MOUSE_UP, &Msg::with_val(1));
            }
            false
        }
        _ => false,
    }
}

pub(crate) fn check_set_checked(w: W, v: bool, generate: bool) {
    let cur = kind!(w, Check, |k| k.checked).unwrap_or(false);
    if cur == v {
        return;
    }
    kind!(w, Check, |k| k.checked = v);
    if generate {
        gw::process_message(
            w,
            MSG_CHECK_CHANGE,
            &Msg::with_val(i32::from(v)),
            false,
            false,
        );
    }
}

// ---------------------------------------------------------------- label

pub(crate) fn label_set_autogen(w: W, v: bool) {
    let cur = kind!(w, Label, |k| k.autogen).unwrap_or(v);
    if cur == v {
        return;
    }
    kind!(w, Label, |k| k.autogen = v);
    label_change_text(w);
}

fn label_change_text(w: W) {
    let Some(wait) = kind!(w, Label, |k| {
        k.scroll_timer = k.wait_to_scroll;
        k.wrap_offset = 0.0;
        k.scrolling_down = true;
        (k.autogen, k.wait_to_scroll)
    }) else {
        return;
    };
    if !wait.0 {
        return;
    }
    let (font, fsize, text) = read(w, |g| (g.font_type, g.p.font_size, g.text.clone()));
    let len = gw::font_length(font, fsize, &text);
    gw::set_size(w, v2(len, fsize.y));
}

fn label_update(w: W, dt: f32) {
    let Some((on, s)) = kind!(w, Label, |k| {
        (
            k.word_wrap,
            wb::LabelScroll {
                timer: k.scroll_timer,
                offset: k.wrap_offset,
                down: k.scrolling_down,
            },
        )
    }) else {
        return;
    };
    if !on {
        return;
    }
    let (mul, rows) =
        kind!(w, Label, |k| (k.scroll_speed_mul, k.rows_height)).unwrap_or((1.0, 0.0));
    let n = wb::label_update(s, dt, mul, gw::size(w).y, rows);
    kind!(w, Label, |k| {
        k.scroll_timer = n.timer;
        k.wrap_offset = n.offset;
        k.scrolling_down = n.down;
    });
}

fn label_draw(w: W, after_clip: bool) {
    let Some((draw_bg, bg, bgc)) = kind!(w, Label, |k| (k.draw_bg, k.bg, k.bg_color)) else {
        return;
    };
    if !after_clip {
        if draw_bg {
            let (p, s) = wb::label_background(gw::global_position(w), gw::size(w));
            g1_set::draw_gfx_default(gw::set_of(w), bg, p, s, bgc);
        }
        if read(w, |g| g.base.clips) {
            return;
        }
    }
    label_draw_text(w);
}

fn label_draw_text(w: W) {
    let Some((align, wrap, maxc, offset)) = kind!(w, Label, |k| (
        k.align,
        k.word_wrap,
        k.max_chars,
        k.wrap_offset
    )) else {
        return;
    };
    let (font, fsize, fcolor, cmul, text) = read(w, |g| {
        (
            g.font_type,
            g.p.font_size,
            g.p.font_color,
            g.p.color_mul,
            g.text.clone(),
        )
    });
    let size = gw::size(w);
    let rows: Vec<Vec<u32>> = if wrap {
        gw::font_rows(font, size.x, fsize.y + 2.0, fsize, &text)
    } else {
        Vec::new()
    };
    let lens: Vec<i32> = rows.iter().map(|r| r.len() as i32).collect();
    let d = wb::LabelDraw {
        global: gw::global_position(w),
        size,
        align,
        word_wrap: wrap,
        max_chars: maxc,
        enabled: gw::is_enabled(w),
        has_default_font: font != 0,
        font_size: fsize,
        font_color: fcolor,
        color_mul: cmul,
        wrap_offset: offset,
        text_len: text.len() as i32,
    };
    let mut buf = vec![DrawOp::default(); rows.len() + 1];
    let mut o = Ops::new(&mut buf);
    let rh = wb::label_draw_text(&mut o, &d, &lens);
    let n = o.len();
    if let Some(h) = rh {
        kind!(w, Label, |k| k.rows_height = h);
    }
    let mut texts: Vec<&[u32]> = vec![&text];
    texts.extend(rows.iter().map(|r| r.as_slice()));
    gw::run_ops(w, &buf[..n], &[], &texts, &[font]);
}

// ---------------------------------------------------------------- image

fn image_draw(w: W) {
    let Some(img) = kind!(w, Image, |k| k.image) else {
        return;
    };
    if !img.is_null() {
        let cm = read(w, |g| g.p.color_mul);
        g1_set::draw_gfx_default(gw::set_of(w), img, gw::global_position(w), gw::size(w), cm);
    }
}

// ---------------------------------------------------------------- dummy / group

fn dummy_child_update(w: W) {
    if !kind!(w, Dummy, |k| k.enclose).unwrap_or(false) {
        return;
    }
    let kids = read(w, |g| g.children.clone());
    let boxes: Vec<wb::DummyChild> = kids
        .iter()
        .map(|c| wb::DummyChild {
            pos: read(*c, |g| g.base.pos),
            size: gw::size(*c),
        })
        .collect();
    gw::set_size(w, wb::dummy_enclose(&boxes));
}

pub(crate) fn dummy_set_enclose(w: W, v: bool) {
    kind!(w, Dummy, |k| k.enclose = v);
    if v {
        dummy_child_update(w);
    }
}

fn group_init(w: W) {
    let (off, set) = (
        kind!(w, Group, |k| k.header_offset).unwrap_or(0.0),
        gw::set_of(w),
    );
    let fh = read(w, |g| {
        // SAFETY: the default skin font is live.
        unsafe { g.font.as_ref() }.map_or(0.0, |f| f.size.y)
    });
    let pos = wb::group_header_pos(off, fh);
    let text = gw::text(w);
    let h = create_label(set, pos, v2(0.0, 0.0), &text, w, b"");
    kind!(w, Group, |k| k.header = h);
    with(h, |g| {
        if let Kind::Label(l) = &mut g.kind {
            l.align = 0;
        }
    });
    label_set_autogen(h, true);
    group_change_text(w);
}

fn group_change_text(w: W) {
    let h = kind!(w, Group, |k| k.header).unwrap_or(0);
    if h != 0 {
        let t = gw::text(w);
        gw::set_text(h, &t);
    }
}

fn group_draw(w: W) {
    let Some((hd, fr, breaks)) = kind!(w, Group, |k| (k.header, k.fr, k.header_breaks)) else {
        return;
    };
    let mut d = wb::GroupDraw {
        global: gw::global_position(w),
        size: gw::size(w),
        has_background: !fr.bg.is_null(),
        header_breaks_border: breaks,
        ..Default::default()
    };
    if breaks {
        d.header_pos = read(hd, |g| g.base.pos);
        d.header_size = gw::size(hd);
    }
    let mut t = GfxTable::new();
    t.set_frame(fr.bg, &fr.borders, &fr.corners);
    let mut buf = [DrawOp::default(); 16];
    let mut o = Ops::new(&mut buf);
    wb::group_draw(&mut o, &d, &t.m);
    let n = o.len();
    run(w, &buf[..n], &t, &[], &[]);
}

// ---------------------------------------------------------------- slider

pub(crate) fn slider_set_max(w: W, m: i32) {
    if kind!(w, Slider, |k| k.s.set_max_value(m)).unwrap_or(false) {
        slider_update_bar(w);
    }
}

pub(crate) fn slider_set_value(w: W, v: i32, generate: bool) {
    let Some(new) = kind!(w, Slider, |k| k.s.set_value(v)).flatten() else {
        return;
    };
    let _ = new;
    if generate {
        let (val, pressed) = kind!(w, Slider, |k| (k.s.value, k.s.pressed)).unwrap_or((0, false));
        gw::process_message(w, MSG_SLIDER_MOVE, &Msg::with_val(val), false, false);
        if !pressed {
            gw::process_message(w, MSG_SLIDER_RELEASE, &Msg::with_val(val), false, false);
        }
    }
    slider_update_bar(w);
}

pub(crate) fn slider_set_bar_size(w: W, v: i32) {
    if kind!(w, Slider, |k| k.s.set_bar_value_size(v)).unwrap_or(false) {
        slider_update_bar(w);
    }
}

fn slider_update_bar(w: W) {
    let Some(h) = kind!(w, Slider, |k| k.handle) else {
        return;
    };
    let c0 = gw::metrics(h.corners[0]).active;
    let c3 = gw::metrics(h.corners[3]).active;
    let (size, global) = (gw::size(w), gw::global_position(w));
    kind!(w, Slider, |k| k.s.update_bar(size, global, c0, c3));
}

fn slider_init(w: W) {
    let set = gw::set_of(w);
    let arrows = kind!(w, Slider, |k| k.arrows).unwrap_or([ptr::null_mut(); 2]);
    let mut bs = [0; 2];
    for (i, b) in bs.iter_mut().enumerate() {
        let btn = create_button(set, v3(0.0, 0.0, 0.0), v2(0.0, 0.0), &[], w, false, b"");
        kind!(btn, Button, |k| {
            k.image = arrows[i];
            k.destroy_image = false;
        });
        add_callback(btn, MSG_MOUSE_DOWN, w, slider_arrow_down);
        *b = btn;
    }
    kind!(w, Slider, |k| k.buttons = bs);
    slider_change_size(w);
}

fn slider_arrow(w: W, widget: W) -> bool {
    let (bs, add, val) =
        kind!(w, Slider, |k| (k.buttons, k.s.button_add, k.s.value)).unwrap_or(([0; 2], 0, 0));
    let i = bs.iter().position(|b| *b == widget).unwrap_or(2);
    if i == 0 {
        slider_set_value(w, val - add, true);
    }
    if i == 1 {
        slider_set_value(w, val + add, true);
    }
    true
}

extern "C" fn slider_arrow_down(ctx: *mut c_void, widget: W, _d: *const Msg) -> bool {
    slider_arrow(ctx as W, widget)
}

fn slider_change_size(w: W) {
    let Some((bs, vertical, btn)) =
        kind!(w, Slider, |k| (k.buttons, k.s.vertical, k.s.button_size))
    else {
        return;
    };
    if bs[0] == 0 || bs[1] == 0 {
        return;
    }
    let l = ws::slider_buttons(vertical, gw::size(w), btn);
    for i in 0..2 {
        gw::set_position(bs[i], l[i].0);
        gw::set_size(bs[i], l[i].1);
    }
    slider_update_bar(w);
}

fn slider_draw(w: W) {
    let Some((s, h, bg)) = kind!(w, Slider, |k| (k.s, k.handle, k.bg)) else {
        return;
    };
    let d = ws::slider_draw(&s, gw::size(w), gw::global_position(w));
    let set = gw::set_of(w);
    g1_set::draw_gfx_default(set, bg, d.track_pos, d.track_size, WHITE);
    gw::draw_borders(
        w, h.bg, &h.borders, &h.corners, d.bar_pos, d.bar_size, false,
    );
}

fn slider_msg(w: W, msg: i32, d: &Msg) -> bool {
    match msg {
        MSG_MOUSE_MOVE => {
            if kind!(w, Slider, |k| k.s.pressed).unwrap_or(false) {
                let p = d.pos();
                let local = gw::world_to_local(w, v3(p[0], p[1], 0.0));
                let global = gw::global_position(w);
                let r = kind!(w, Slider, |k| k.s.mouse_move(local, global)).flatten();
                if r.is_some() {
                    let v = kind!(w, Slider, |k| k.s.value).unwrap_or(0);
                    gw::process_message(w, MSG_SLIDER_MOVE, &Msg::with_val(v), false, false);
                }
            }
            true
        }
        MSG_MOUSE_DOWN => {
            let val = d.val();
            if val & wd::MOUSE_WHEEL_UP != 0 || val & wd::MOUSE_WHEEL_DOWN != 0 {
                return false;
            }
            let p = d.pos();
            let local = gw::world_to_local(w, v3(p[0], p[1], 0.0));
            let global = gw::global_position(w);
            let (ret, v) = kind!(w, Slider, |k| k.s.mouse_down(
                val,
                v2(p[0], p[1]),
                local,
                global
            ))
            .unwrap_or((false, None));
            if v.is_some() {
                let v = kind!(w, Slider, |k| k.s.value).unwrap_or(0);
                gw::process_message(w, MSG_SLIDER_MOVE, &Msg::with_val(v), false, false);
            }
            ret
        }
        MSG_MOUSE_UP => {
            if kind!(w, Slider, |k| k.s.release()).unwrap_or(false) {
                slider_update_bar(w);
            }
            let v = kind!(w, Slider, |k| k.s.value).unwrap_or(0);
            gw::process_message(w, MSG_SLIDER_RELEASE, &Msg::with_val(v), false, false);
            true
        }
        MSG_LOST_FOCUS => {
            if kind!(w, Slider, |k| k.s.release()).unwrap_or(false) {
                slider_update_bar(w);
            }
            false
        }
        MSG_UI_ARROW_PRESS => {
            let vertical = kind!(w, Slider, |k| k.s.vertical).unwrap_or(false);
            let b = ws::slider_arrow_button(vertical, d.val());
            if b < 0 {
                return false;
            }
            let btn = kind!(w, Slider, |k| k.buttons[b as usize]).unwrap_or(0);
            slider_arrow(w, btn)
        }
        MSG_UI_ARROW_RELEASE => true,
        _ => false,
    }
}

// ---------------------------------------------------------------- frame

extern "C" fn frame_slider_move(ctx: *mut c_void, widget: W, _d: *const Msg) -> bool {
    let w = ctx as W;
    let (h, v) = kind!(w, Frame, |k| (k.hs, k.vs)).unwrap_or((0, 0));
    if widget == h || widget == v {
        kind!(w, Frame, |k| k.scroll_updated = true);
    }
    true
}

fn frame_change_size(w: W) {
    let Some((h, v)) = kind!(w, Frame, |k| (k.hs, k.vs)) else {
        return;
    };
    let hv = h != 0 && gw::is_visible(h);
    let vv = v != 0 && gw::is_visible(v);
    let l = wd::frame_slider_layout(gw::size(w), hv, vv);
    if h != 0 {
        gw::set_size(h, l.h_size);
        gw::set_position(h, l.h_pos);
    }
    if v != 0 {
        gw::set_size(v, l.v_size);
        gw::set_position(v, l.v_pos);
    }
    kind!(w, Frame, |k| k.need_update = true);
}

fn slider_get(w: W) -> (i32, i32, i32) {
    kind!(w, Slider, |k| (
        k.s.value,
        k.s.button_add,
        k.s.bar_value_size
    ))
    .unwrap_or((0, 0, 0))
}

fn frame_update(w: W) {
    let mut offset = read(w, |g| g.base.scroll);
    let Some((h, v, need, margin, maxc)) = kind!(w, Frame, |k| (
        k.hs,
        k.vs,
        k.need_update,
        k.margin,
        k.max_coord
    )) else {
        return;
    };
    if need && (h != 0 || v != 0) && gw::is_visible(w) {
        kind!(w, Frame, |k| k.need_update = false);
        let kids = read(w, |g| g.children.clone());
        let boxes: Vec<wd::ChildBox> = kids
            .iter()
            .filter(|c| **c != h && **c != v)
            .map(|c| wd::ChildBox {
                pos: read(*c, |g| g.base.pos),
                size: gw::size(*c),
                visible: gw::is_visible(*c),
            })
            .collect();
        let i = wd::ScrollIn {
            size: gw::size(w),
            margin,
            offset,
            max_coord: maxc,
            h_size: if h != 0 { gw::size(h) } else { v2(0.0, 0.0) },
            v_size: if v != 0 { gw::size(v) } else { v2(0.0, 0.0) },
            has_h: h != 0,
            has_v: v != 0,
        };
        let plan = wd::frame_scroll_plan(&i, &boxes);
        kind!(w, Frame, |k| k.max_coord = plan.max_coord);
        for (s, p) in [(h, plan.h), (v, plan.v)] {
            if s == 0 {
                continue;
            }
            slider_set_max(s, p.max_value);
            slider_set_bar_size(s, p.bar_value_size);
            kind!(s, Slider, |k| k.s.button_add = p.button_add);
            let bvs = slider_get(s).2;
            kind!(s, Slider, |k| k.s.click_add = bvs);
            if p.set_value {
                slider_set_value(s, p.value, true);
            }
            gw::set_enabled(s, p.active);
            gw::set_visible(s, p.active);
        }
    }
    if kind!(w, Frame, |k| std::mem::replace(
        &mut k.scroll_updated,
        false
    ))
    .unwrap_or(false)
    {
        if h != 0 {
            offset.x = slider_get(h).0 as f32;
        }
        if v != 0 {
            offset.y = slider_get(v).0 as f32;
        }
    }
    gw::set_scroll_amount(w, offset);
}

fn frame_draw(w: W, after_clip: bool) {
    let Some((draw_frame, draw_bg, bg_z, bgc, fr)) = kind!(w, Frame, |k| (
        k.draw_frame,
        k.draw_bg,
        k.bg_z,
        k.bg_color,
        k.fr
    )) else {
        return;
    };
    let d = wd::FrameDraw {
        global: gw::global_position(w),
        size: gw::size(w),
        draw_frame,
        draw_background: draw_bg,
        background_z: bg_z,
        background_color: bgc,
    };
    let mut t = GfxTable::new();
    let mut buf = [DrawOp::default(); 16];
    let mut o = Ops::new(&mut buf);
    if after_clip {
        t.set(0, fr.bg);
        wd::frame_draw_background(&mut o, &d);
    } else {
        t.set_frame(ptr::null_mut(), &fr.borders, &fr.corners);
        wd::frame_draw(&mut o, &d, &t.m);
    }
    let n = o.len();
    run(w, &buf[..n], &t, &[], &[]);
}

fn frame_msg(w: W, msg: i32, d: &Msg) -> bool {
    match msg {
        MSG_MOUSE_MOVE | MSG_MOUSE_UP => true,
        MSG_MOUSE_DOWN => {
            let val = d.val();
            if val & wd::MOUSE_LEFT != 0 || val & wd::MOUSE_RIGHT != 0 {
                return true;
            }
            let v = kind!(w, Frame, |k| k.vs).unwrap_or(0);
            if v == 0 || !gw::is_visible(v) {
                return false;
            }
            let (value, add, _) = slider_get(v);
            if let Some(n) = wd::frame_wheel(val, value, add) {
                slider_set_value(v, n, true);
            }
            true
        }
        _ => false,
    }
}

pub(crate) fn frame_scroll_to(w: W, x: f32, y: f32) {
    let (h, v) = kind!(w, Frame, |k| (k.hs, k.vs)).unwrap_or((0, 0));
    if h != 0 {
        slider_set_value(h, wd::round_to_int(x), true);
    }
    if v != 0 {
        slider_set_value(v, wd::round_to_int(y), true);
    }
}

// ---------------------------------------------------------------- window

fn window_look(w: W) -> Option<wd::WindowLook> {
    let (fr, font, off) = kind!(w, Window, |k| (k.fr, k.label_font, k.label_offset))?;
    Some(wd::WindowLook {
        size: gw::size(w),
        corner_lu: gw::metrics(fr.corners[0]).active,
        corner_ru: gw::metrics(fr.corners[1]).active,
        // SAFETY: the skin's label font is live.
        label_font_height: unsafe { font.as_ref() }.map_or(0.0, |f| f.size.y),
        label_offset: off,
    })
}

fn window_init(w: W) {
    let (flags, cross, bs) = kind!(w, Window, |k| (k.flags, k.cross, k.button_size)).unwrap_or((
        0,
        ptr::null_mut(),
        0.0,
    ));
    if flags & FLAG_CLOSE_BUTTON != 0 {
        let set = gw::set_of(w);
        let b = create_button(set, v3(0.0, 0.0, 0.0), v2(0.0, 0.0), &[], w, false, b"");
        kind!(b, Button, |k| {
            k.image = cross;
            k.destroy_image = false;
        });
        gw::set_size(b, v2(bs, bs));
        add_callback(b, MSG_BUTTON_PRESSED, w, window_button_pressed);
        kind!(w, Window, |k| k.button = b);
        window_change_size(w);
    }
}

extern "C" fn window_button_pressed(ctx: *mut c_void, widget: W, _d: *const Msg) -> bool {
    let w = ctx as W;
    let b = kind!(w, Window, |k| k.button).unwrap_or(0);
    if b == widget {
        gw::process_message(w, MSG_WINDOW_CLOSE, &Msg::zero(), false, false);
        if kind!(w, Window, |k| k.close_disables).unwrap_or(false) {
            gw::set_enabled(w, false);
            gw::set_visible(w, false);
            let set = gw::set_of(w);
            // SAFETY: the widget's set is live.
            unsafe {
                if g1_set::attention_widget(set) == w {
                    g1_set::ohpl_g1_set_set_attention(set, 0, true, false);
                }
            }
        }
    }
    true
}

fn window_attach(w: W, child: W) {
    if read(child, |g| g.ty) == TYPE_MAIN_MENU
        && let Some(l) = window_look(w)
    {
        gw::set_position(child, wd::window_menu_pos(&l));
    }
}

fn window_change_size(w: W) {
    let Some((b, bs)) = kind!(w, Window, |k| (k.button, k.button_size)) else {
        return;
    };
    if b != 0 {
        gw::set_position(b, wd::window_button_pos(gw::size(w).x, bs));
    }
}

fn window_draw(w: W) {
    let Some((fr, font, label_gfx, draw_label, focused, outside)) = kind!(w, Window, |k| (
        k.fr,
        k.label_font,
        k.label_gfx,
        k.draw_label,
        k.focused,
        k.borders_outside
    )) else {
        return;
    };
    let Some(look) = window_look(w) else {
        return;
    };
    // SAFETY: the skin's label font is live.
    let Some(sf) = (unsafe { font.as_ref() }) else {
        return;
    };
    let cmul = read(w, |g| g.p.color_mul);
    let d = wd::WindowDraw {
        look,
        global: gw::global_position(w),
        draw_label,
        focused,
        has_background: !fr.bg.is_null(),
        borders_outside: outside,
        font_size: sf.size,
        font_color: sf.color,
        color_mul: cmul,
    };
    let mut t = GfxTable::new();
    t.set_frame(fr.bg, &fr.borders, &fr.corners);
    t.set(wd::GFX_LABEL as usize, label_gfx);
    let mut buf = [DrawOp::default(); 16];
    let mut o = Ops::new(&mut buf);
    wd::window_draw(&mut o, &d, &t.m);
    let n = o.len();
    let text = gw::text(w);
    run(w, &buf[..n], &t, &[&text], &[0, sf.font]);
}

fn window_msg(w: W, msg: i32, d: &Msg) -> bool {
    match msg {
        MSG_MOUSE_MOVE => {
            let (moving, rel) =
                kind!(w, Window, |k| (k.moving, k.rel_mouse)).unwrap_or((false, v3(0.0, 0.0, 0.0)));
            if moving {
                let p = d.pos();
                gw::set_global_position(w, wd::window_drag_pos(rel, v2(p[0], p[1])));
            }
            true
        }
        MSG_MOUSE_DOWN => {
            if kind!(w, Window, |k| k.is_static).unwrap_or(false) {
                return false;
            }
            let Some(look) = window_look(w) else {
                return false;
            };
            let p = d.pos();
            let global = gw::global_position(w);
            if wd::window_mouse_down(&look, global, v2(p[0], p[1]), d.val()) {
                let set = gw::set_of(w);
                // SAFETY: the widget's set is live.
                let prev = g1_set::attention_widget(set);
                // SAFETY: as above.
                unsafe { g1_set::ohpl_g1_set_set_attention(set, w, true, false) };
                let rel = wd::rel_to_mouse(gw::global_position(w), v2(p[0], p[1]));
                let z = gw::global_position(w).z;
                kind!(w, Window, |k| {
                    k.moving = true;
                    k.prev_attention = prev;
                    k.rel_mouse = v3(rel.x, rel.y, z);
                });
            }
            true
        }
        MSG_MOUSE_UP => {
            if d.val() & wd::MOUSE_LEFT != 0 && kind!(w, Window, |k| k.moving).unwrap_or(false) {
                let set = gw::set_of(w);
                let prev = kind!(w, Window, |k| k.prev_attention).unwrap_or(0);
                // SAFETY: as above.
                unsafe {
                    g1_set::ohpl_g1_set_set_attention(set, prev, true, false);
                    g1_set::ohpl_g1_set_set_focused(set, w, false);
                }
                kind!(w, Window, |k| k.moving = false);
            }
            true
        }
        MSG_MOUSE_LEAVE => {
            let last = gw::global_position(w);
            let (moving, rel) =
                kind!(w, Window, |k| (k.moving, k.rel_mouse)).unwrap_or((false, v3(0.0, 0.0, 0.0)));
            if moving {
                let p = d.pos();
                gw::set_global_position(w, wd::window_drag_pos(rel, v2(p[0], p[1])));
                if !gw::point_is_inside(w, v2(p[0], p[1]), false) {
                    gw::set_global_position(w, last);
                    kind!(w, Window, |k| k.moving = false);
                }
            }
            false
        }
        MSG_KEY_PRESS => {
            let (focused, esc) =
                kind!(w, Window, |k| (k.focused, k.close_on_escape)).unwrap_or((false, false));
            let (key, modifier) = (d.key(), d.modifier());
            if wd::window_key_closes(focused, esc, key, modifier) {
                return gw::process_message(w, MSG_WINDOW_CLOSE, d, false, false);
            }
            false
        }
        MSG_GOT_FOCUS => {
            window_child_got_focus(w);
            true
        }
        MSG_LOST_FOCUS => {
            if kind!(w, Window, |k| k.moving).unwrap_or(false) {
                return true;
            }
            window_child_lost_focus(w);
            true
        }
        _ => false,
    }
}

fn window_child_got_focus(w: W) {
    // SAFETY: the widget's set is live.
    unsafe { g1_set::ohpl_g1_set_set_window_on_top(gw::set_of(w), w) };
    kind!(w, Window, |k| k.focused = true);
}

fn window_child_lost_focus(w: W) {
    let set = gw::set_of(w);
    // SAFETY: the widget's set is live.
    let f = g1_set::focused_widget(set);
    let none = f == 0;
    let down = if none {
        false
    } else {
        gw::is_connected_to(w, f, false)
    };
    let up = if none || down {
        false
    } else {
        gw::is_connected_to(f, w, false)
    };
    if wd::window_focus_lost(none, down, up) {
        kind!(w, Window, |k| k.focused = false);
    }
}

// ---------------------------------------------------------------- C ABI of the kinds

macro_rules! kffi {
    ($name:ident ( $($a:ident : $t:ty),* ) $(-> $r:ty)? => |$w:ident| $body:expr) => {
        #[unsafe(no_mangle)]
        pub extern "C" fn $name($w: W $(, $a: $t)*) $(-> $r)? {
            $body
        }
    };
}

kffi!(ohpl_gw_set_kind(id: i32, a: i32, b: i32) => |w| set_kind(w, id, a, b));

// button
kffi!(ohpl_gw_button_set_image(g: *mut Element, d: bool) => |w| { kind!(w, Button, |k| { k.image = g; k.destroy_image = d; }); });
kffi!(ohpl_gw_button_image() -> *mut Element => |w| kind!(w, Button, |k| k.image as usize).unwrap_or(0) as *mut Element);
kffi!(ohpl_gw_button_set_repeat_active(v: bool) => |w| { kind!(w, Button, |k| k.repeat_active = v); });
kffi!(ohpl_gw_button_repeat_active() -> bool => |w| kind!(w, Button, |k| k.repeat_active).unwrap_or(false));
kffi!(ohpl_gw_button_set_repeat_freq(f: f32) => |w| { kind!(w, Button, |k| k.repeat_period = wb::button_repeat_period(f)); });
kffi!(ohpl_gw_button_set_toggleable(v: bool) => |w| { kind!(w, Button, |k| k.toggleable = v); });
kffi!(ohpl_gw_button_toggleable() -> bool => |w| kind!(w, Button, |k| k.toggleable).unwrap_or(false));
kffi!(ohpl_gw_button_pressed() -> bool => |w| kind!(w, Button, |k| k.pressed).unwrap_or(false));
kffi!(ohpl_gw_button_set_pressed(v: bool, generate: bool) => |w| {
    if kind!(w, Button, |k| k.toggleable).unwrap_or(false) {
        kind!(w, Button, |k| k.pressed = v);
        if generate {
            gw::process_message(w, MSG_BUTTON_PRESSED, &Msg::zero(), false, false);
        }
    }
});
// check box
kffi!(ohpl_gw_check_set_checked(v: bool, generate: bool) => |w| check_set_checked(w, v, generate));
kffi!(ohpl_gw_check_checked() -> bool => |w| kind!(w, Check, |k| k.checked).unwrap_or(false));
// label
kffi!(ohpl_gw_label_set_align(v: i32) => |w| { kind!(w, Label, |k| k.align = v); });
kffi!(ohpl_gw_label_align() -> i32 => |w| kind!(w, Label, |k| k.align).unwrap_or(0));
kffi!(ohpl_gw_label_set_word_wrap(v: bool) => |w| { kind!(w, Label, |k| k.word_wrap = v); });
kffi!(ohpl_gw_label_word_wrap() -> bool => |w| kind!(w, Label, |k| k.word_wrap).unwrap_or(false));
kffi!(ohpl_gw_label_set_max_length(v: i32) => |w| { kind!(w, Label, |k| k.max_chars = v); });
kffi!(ohpl_gw_label_max_length() -> i32 => |w| kind!(w, Label, |k| k.max_chars).unwrap_or(-1));
kffi!(ohpl_gw_label_set_autogen(v: bool) => |w| label_set_autogen(w, v));
kffi!(ohpl_gw_label_autogen() -> bool => |w| kind!(w, Label, |k| k.autogen).unwrap_or(false));
kffi!(ohpl_gw_label_set_draw_bg(v: bool) => |w| { kind!(w, Label, |k| k.draw_bg = v); });
kffi!(ohpl_gw_label_draw_bg() -> bool => |w| kind!(w, Label, |k| k.draw_bg).unwrap_or(false));
kffi!(ohpl_gw_label_set_bg_color(c: *const Color) => |w| { kind!(w, Label, |k| k.bg_color = gw::rdc(c)); });
kffi!(ohpl_gw_label_bg_color() -> *const Color => |w| kind!(w, Label, |k| &raw const k.bg_color as usize).unwrap_or(0) as *const Color);
kffi!(ohpl_gw_label_set_scroll_wait(v: f32) => |w| { kind!(w, Label, |k| k.wait_to_scroll = v); });
kffi!(ohpl_gw_label_scroll_wait() -> f32 => |w| kind!(w, Label, |k| k.wait_to_scroll).unwrap_or(0.0));
kffi!(ohpl_gw_label_set_scroll_offset(v: f32) => |w| { kind!(w, Label, |k| k.wrap_offset = v); });
kffi!(ohpl_gw_label_set_scroll_speed(v: f32) => |w| { kind!(w, Label, |k| k.scroll_speed_mul = v); });
kffi!(ohpl_gw_label_scroll_speed() -> f32 => |w| kind!(w, Label, |k| k.scroll_speed_mul).unwrap_or(1.0));
// image
kffi!(ohpl_gw_image_set(g: *mut Element) => |w| { kind!(w, Image, |k| k.image = g); });
kffi!(ohpl_gw_image() -> *mut Element => |w| kind!(w, Image, |k| k.image as usize).unwrap_or(0) as *mut Element);
// dummy
kffi!(ohpl_gw_dummy_set_enclose(v: bool) => |w| dummy_set_enclose(w, v));
// group
/// # Safety
/// `n` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gw_group_header(w: W, n: *mut usize) -> *const u32 {
    let h = kind!(w, Group, |k| k.header).unwrap_or(0);
    // SAFETY: caller contract.
    unsafe { super::gw::ohpl_gw_text(h, n) }
}
// slider
kffi!(ohpl_gw_slider_value() -> i32 => |w| kind!(w, Slider, |k| k.s.value).unwrap_or(0));
kffi!(ohpl_gw_slider_set_value(v: i32, generate: bool) => |w| slider_set_value(w, v, generate));
kffi!(ohpl_gw_slider_max() -> i32 => |w| kind!(w, Slider, |k| k.s.max_value).unwrap_or(0));
kffi!(ohpl_gw_slider_set_max(v: i32) => |w| slider_set_max(w, v));
kffi!(ohpl_gw_slider_button_add() -> i32 => |w| kind!(w, Slider, |k| k.s.button_add).unwrap_or(0));
kffi!(ohpl_gw_slider_set_button_add(v: i32) => |w| { kind!(w, Slider, |k| k.s.button_add = v); });
kffi!(ohpl_gw_slider_click_add() -> i32 => |w| kind!(w, Slider, |k| k.s.click_add).unwrap_or(0));
kffi!(ohpl_gw_slider_set_click_add(v: i32) => |w| { kind!(w, Slider, |k| k.s.click_add = v); });
kffi!(ohpl_gw_slider_bar_size() -> i32 => |w| kind!(w, Slider, |k| k.s.bar_value_size).unwrap_or(0));
kffi!(ohpl_gw_slider_set_bar_size(v: i32) => |w| slider_set_bar_size(w, v));
kffi!(ohpl_gw_slider_button_size() -> f32 => |w| kind!(w, Slider, |k| k.s.button_size).unwrap_or(0.0));
// frame
kffi!(ohpl_gw_frame_set_draw_frame(v: bool) => |w| { kind!(w, Frame, |k| k.draw_frame = v); });
kffi!(ohpl_gw_frame_draw_frame() -> bool => |w| kind!(w, Frame, |k| k.draw_frame).unwrap_or(false));
kffi!(ohpl_gw_frame_set_draw_bg(v: bool) => |w| { kind!(w, Frame, |k| k.draw_bg = v); });
kffi!(ohpl_gw_frame_draw_bg() -> bool => |w| kind!(w, Frame, |k| k.draw_bg).unwrap_or(false));
kffi!(ohpl_gw_frame_set_bg_z(v: f32) => |w| { kind!(w, Frame, |k| k.bg_z = v); });
kffi!(ohpl_gw_frame_bg_z() -> f32 => |w| kind!(w, Frame, |k| k.bg_z).unwrap_or(0.0));
kffi!(ohpl_gw_frame_set_bg_color(c: *const Color) => |w| { kind!(w, Frame, |k| k.bg_color = gw::rdc(c)); });
kffi!(ohpl_gw_frame_bg_color() -> *const Color => |w| kind!(w, Frame, |k| &raw const k.bg_color as usize).unwrap_or(0) as *const Color);
kffi!(ohpl_gw_frame_scroll_to(x: f32, y: f32) => |w| frame_scroll_to(w, x, y));
// window
kffi!(ohpl_gw_window_set_static(v: bool) => |w| { kind!(w, Window, |k| { k.is_static = v; if !v { k.moving = false; } }); });
kffi!(ohpl_gw_window_static() -> bool => |w| kind!(w, Window, |k| k.is_static).unwrap_or(false));
kffi!(ohpl_gw_window_set_draw_label(v: bool) => |w| { kind!(w, Window, |k| k.draw_label = v); });
kffi!(ohpl_gw_window_set_close_disables(v: bool) => |w| { kind!(w, Window, |k| k.close_disables = v); });
kffi!(ohpl_gw_window_close_disables() -> bool => |w| kind!(w, Window, |k| k.close_disables).unwrap_or(false));
kffi!(ohpl_gw_window_set_escape_closes(v: bool) => |w| { kind!(w, Window, |k| k.close_on_escape = v); });
kffi!(ohpl_gw_window_escape_closes() -> bool => |w| kind!(w, Window, |k| k.close_on_escape).unwrap_or(false));

// ---------------------------------------------------------------- set services

extern "C" fn root_draw_mouse(obj: *mut c_void, w: W, d: *const Msg) -> bool {
    // SAFETY: `obj` is the live set the root was made for.
    unsafe { g1_set::ohpl_g1_set_draw_mouse(obj, w, d) }
}

extern "C" fn root_draw_focus(obj: *mut c_void, w: W, d: *const Msg) -> bool {
    // SAFETY: as above.
    unsafe { g1_set::ohpl_g1_set_draw_focus(obj, w, d) }
}

/// `iWidget(eWidgetType_Root, set, NULL)` with the set's draw callbacks attached.
pub(crate) fn root_new(set: *mut super::g1_set::Set) -> W {
    let w = shell(gw_host::SHELL_PLAIN, set, 0, 0);
    set_kind(w, KIND_PLAIN, 0, 0);
    gw::add_callback(w, 9, set.cast(), root_draw_mouse);
    gw::add_callback(w, 9, set.cast(), root_draw_focus);
    w
}

/// Tooltip frame, background frame and label.
pub(crate) fn tooltip_create(set: *mut super::g1_set::Set) -> [W; 3] {
    let z = v3(0.0, 0.0, 0.0);
    let frame = create_frame(set, z, v2(0.0, 0.0), false, 0, false, false, b"");
    kind!(frame, Frame, |k| {
        k.bg_color = Color {
            r: 0.0,
            g: 0.0,
            b: 0.0,
            a: 1.0,
        };
        k.draw_bg = true;
    });
    gw::set_enabled(frame, false);
    gw::set_visible(frame, false);
    let bg = create_frame(
        set,
        v3(1.0, 1.0, 0.1),
        v2(0.0, 0.0),
        false,
        frame,
        false,
        false,
        b"",
    );
    kind!(bg, Frame, |k| {
        k.bg_color = Color {
            r: 1.0,
            g: 1.0,
            b: 0.882,
            a: 1.0,
        };
        k.draw_bg = true;
    });
    let label = create_label(set, v3(3.0, 2.0, 0.1), v2(0.0, 0.0), &[], frame, b"");
    gw::set_default_font_size(label, v2(12.0, 12.0));
    kind!(label, Label, |k| k.word_wrap = true);
    [frame, bg, label]
}

/// Text length, font height and row count of the tip text under the label's font.
pub(crate) fn tooltip_measure(label: W, tip: W, max_len: f32) -> ([f32; 2], i32) {
    let text = read(tip, |g| g.tooltip.clone());
    let (font, fsize) = read(label, |g| (g.font_type, g.p.font_size));
    let len = gw::font_length(font, fsize, &text);
    let rows = gw::font_rows(font, max_len, fsize.y + 2.0, fsize, &text).len();
    ([len, fsize.y], rows as i32)
}

pub(crate) fn tooltip_apply(label: W, frame: W, bg: W, tip: W, a: &[f32; 7]) {
    let text = read(tip, |g| g.tooltip.clone());
    gw::set_text(label, &text);
    gw::set_size(label, v2(a[3], a[4]));
    gw::set_global_position(frame, v3(a[0], a[1], a[2]));
    gw::set_size(frame, v2(a[3], a[4]));
    gw::set_size(bg, v2(a[5], a[6]));
    gw::set_enabled(frame, true);
    gw::set_visible(frame, true);
}

pub(crate) fn hide(w: W) {
    gw::set_enabled(w, false);
    gw::set_visible(w, false);
}

pub(crate) fn delete(w: W) {
    if let Some(h) = host() {
        // SAFETY: `w` is a live shell owned by its set.
        unsafe { (h.shell_delete)(w) };
    }
}

pub(crate) fn first_child(w: W) -> W {
    read(w, |g| g.children.first().copied().unwrap_or(0))
}

pub(crate) fn clear_children(w: W) {
    with(w, |g| g.children.clear());
}

pub(crate) fn name_eq(w: W, n: &[u8]) -> bool {
    read(w, |g| g.name == n)
}

/// # Safety
/// `set` is a live set; the pointers readable (`text` for `tl` wide characters, `name` for `nl` bytes).
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gw_create(
    set: *mut c_void,
    kind: i32,
    pos: *const Vector3f,
    size: *const Vector2f,
    text: *const u32,
    tl: usize,
    parent: W,
    name: *const u8,
    nl: usize,
    a: i32,
    b: i32,
    c: i32,
    img: *mut Element,
) -> W {
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
    match kind {
        KIND_LABEL => create_label(set, pos, size, text, parent, name),
        KIND_BUTTON => create_button(set, pos, size, text, parent, a != 0, name),
        KIND_SLIDER => create_slider(set, a, pos, size, c, parent, name),
        KIND_FRAME => create_frame(set, pos, size, c != 0, parent, a != 0, b != 0, name),
        KIND_WINDOW => {
            let w = shell(gw_host::SHELL_WINDOW, set, a, 0);
            gw::set_position(w, pos);
            gw::set_size(w, size);
            gw::set_text(w, text);
            set_name(w, name);
            add_widget(set, w, parent);
            // SAFETY: live set and window.
            unsafe { g1_set::ohpl_g1_set_add_window(set, w) };
            w
        }
        KIND_CHECK | KIND_GROUP => {
            let w = shell(
                if kind == KIND_CHECK {
                    gw_host::SHELL_CHECK
                } else {
                    gw_host::SHELL_GROUP
                },
                set,
                0,
                0,
            );
            gw::set_position(w, pos);
            gw::set_size(w, size);
            gw::set_text(w, text);
            set_name(w, name);
            add_widget(set, w, parent);
            w
        }
        KIND_IMAGE => {
            let w = shell(gw_host::SHELL_IMAGE, set, 0, 0);
            gw::set_position(w, pos);
            gw::set_size(w, size);
            kind!(w, Image, |k| k.image = img);
            set_name(w, name);
            add_widget(set, w, parent);
            w
        }
        KIND_DUMMY => {
            let w = shell(gw_host::SHELL_DUMMY, set, 0, 0);
            gw::set_position(w, pos);
            set_name(w, name);
            add_widget(set, w, parent);
            w
        }
        _ => 0,
    }
}
