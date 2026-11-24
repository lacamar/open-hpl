//! The `iWidget` object: base state, hierarchy, text, fonts and the C ABI of the facade
//! (`gui/Widget.h`). A widget is a `Gw` owned here; the C++ object (`iWidget` and its subclasses)
//! is a shell holding the `Gw` pointer right after its vptr, and `W` is the shell's address.
//! Widget kinds written in Rust live in `gw_kinds`; widgets still written in C++ are
//!
//!
//! Re-entrancy: widget code calls back into widgets, so no `&mut Gw` lives across a call that can
//! reach another widget; every access is a short scope in [`with`].

#![allow(clippy::indexing_slicing, clippy::too_many_arguments)] // fixed-size tables and op counts bounded by their buffers

use std::ffi::c_void;
use std::ptr;

use super::g1_gfx::Element;
use super::g1_gui::Gui;
use super::g1_set::{self, Set};
use super::g1_skin::{self, Skin, SkinFont};
use super::gw_host::host;
use super::gw_kinds::{self as gk, Kind};
use super::h19_widget::{self as hw, Base, CallbackFn, Host, Op};
use super::set::{Msg, W};
use crate::types::{Color, Vector2f, Vector3f};

pub const MSG_MOUSE_DOWN: i32 = 1;
pub const MSG_MOUSE_UP: i32 = 2;

/// `cColor()`: what a skin-less widget (the root) keeps as its font colors.
const BLACK: Color = Color {
    r: 0.0,
    g: 0.0,
    b: 0.0,
    a: 1.0,
};

const WHITE: Color = Color {
    r: 1.0,
    g: 1.0,
    b: 1.0,
    a: 1.0,
};

/// `eWidgetType_*` of the types Rust decides on.
pub const TYPE_MAIN_MENU: i32 = 14;

/// Fields C++ reads by reference; mirrored by `OhplGwPub` in `gui/Widget.h`.
#[repr(C)]
pub struct Pubf {
    pub font_color: Color,
    pub font_highlight: Color,
    pub font_size: Vector2f,
    pub color_mul: Color,
}

pub struct Gw {
    pub base: Base,
    pub p: Pubf,
    pub shell: W,
    pub set: *mut Set,
    pub skin: *mut Skin,
    pub gui: *mut Gui,
    pub ty: i32,
    pub text: Vec<u32>,
    pub name: Vec<u8>,
    pub tooltip: Vec<u32>,
    pub tooltip_enabled: bool,
    pub font: *mut SkinFont,
    pub font_type: usize,
    pub pointer_gfx: *mut Element,
    pub shortcuts: Vec<usize>,
    pub global_key_listener: bool,
    pub global_ui_listener: bool,
    pub user_data: *mut c_void,
    pub user_value: i32,
    pub children: Vec<W>,
    pub items: Vec<*mut super::gw_items::Item>,
    pub kind: Kind,
}

/// The `Gw` of a shell: the pointer stored right after the vptr.
pub(crate) fn gwp(w: W) -> *mut Gw {
    if w == 0 {
        return ptr::null_mut();
    }
    // SAFETY: `w` is a live shell (`iWidget` layout: vptr, `Gw*`).
    unsafe { *((w + 8) as *const *mut Gw) }
}

/// Runs `f` on the widget's state; `f` must not reach any other widget.
pub(crate) fn with<R>(w: W, f: impl FnOnce(&mut Gw) -> R) -> Option<R> {
    // SAFETY: live widget; the borrow ends with `f`.
    unsafe { gwp(w).as_mut() }.map(f)
}

pub(crate) fn read<R: Default>(w: W, f: impl FnOnce(&Gw) -> R) -> R {
    with(w, |g| f(g)).unwrap_or_default()
}

pub struct GwHost;

impl Host for GwHost {
    fn base(&mut self, w: W) -> *mut Base {
        let g = gwp(w);
        if g.is_null() {
            return ptr::null_mut();
        }
        // SAFETY: `g` is live; only the field address is taken.
        unsafe { &raw mut (*g).base }
    }
    fn call(&mut self, w: W, op: Op, other: W, d: &Msg, f: f32) -> bool {
        gk::call(w, op, other, d, f)
    }
    fn child_begin(&mut self, _w: W) -> usize {
        0
    }
    fn child_end(&mut self, w: W, it: usize) -> bool {
        read(w, |g| it >= g.children.len())
    }
    fn child_get(&mut self, w: W, it: usize) -> W {
        read(w, |g| g.children.get(it).copied().unwrap_or(0))
    }
    fn child_next(&mut self, _w: W, it: usize) -> usize {
        it + 1
    }
    fn child_erase(&mut self, w: W, it: usize) {
        with(w, |g| {
            if it < g.children.len() {
                g.children.remove(it);
            }
        });
    }
    fn child_push(&mut self, w: W, child: W) {
        with(w, |g| g.children.push(child));
    }
    fn needs_sorting(&mut self, w: W) {
        let s = set_of(w);
        g1_set::set_widgets_need_sorting(s);
    }
    fn virtual_size(&mut self, w: W) -> Vector2f {
        let r = g1_set::virtual_rect(set_of(w));
        Vector2f { x: r[0], y: r[1] }
    }
    fn clip_child(&mut self, _w: W, clip: usize, pos: Vector3f, size: Vector2f) -> usize {
        // SAFETY: `clip` is a live clip region of the set.
        unsafe { g1_set::ohpl_g1_clip_create_child(clip as *mut _, &pos, &size) as usize }
    }
    fn set_clip(&mut self, w: W, clip: usize) {
        // SAFETY: the widget's set is live.
        unsafe { g1_set::ohpl_g1_set_set_current_clip(set_of(w), clip as *mut _) };
    }
}

pub(crate) fn set_of(w: W) -> *mut Set {
    read(w, |g| g.set as usize) as *mut Set
}

pub(crate) fn skin_of(w: W) -> *mut Skin {
    read(w, |g| g.skin as usize) as *mut Skin
}

pub(crate) fn gui_of(w: W) -> *mut Gui {
    read(w, |g| g.gui as usize) as *mut Gui
}

pub(crate) fn skin_gfx(skin: *mut Skin, name: &str) -> *mut Element {
    let Some(i) = super::guicore::GFXS.iter().position(|n| *n == name) else {
        return ptr::null_mut();
    };
    // SAFETY: a widget's skin is live as long as the gui.
    unsafe { g1_skin::ohpl_g1_skin_gfx(skin, i as i32) }
}

pub(crate) fn skin_attr(skin: *mut Skin, name: &str) -> Vector3f {
    let Some(i) = super::guicore::ATTRIBUTES.iter().position(|n| *n == name) else {
        return Vector3f::default();
    };
    // SAFETY: as above.
    unsafe { g1_skin::ohpl_g1_skin_attribute(skin, i as i32).as_ref() }
        .copied()
        .unwrap_or_default()
}

pub(crate) fn skin_font(skin: *mut Skin, name: &str) -> *mut SkinFont {
    let Some(i) = super::guicore::FONTS.iter().position(|n| *n == name) else {
        return ptr::null_mut();
    };
    // SAFETY: as above.
    unsafe { g1_skin::ohpl_g1_skin_font(skin, i as i32) }
}

// ---------------------------------------------------------------- lifetime

/// `iWidget::iWidget`; `shell` is the C++ object.
pub(crate) fn new_gw(shell: W, ty: i32, set: *mut Set, skin: *mut Skin) -> *mut Gw {
    let mut base = std::mem::MaybeUninit::<Base>::zeroed();
    // SAFETY: all-zero is a valid `Base` (integers, floats, bools, null pointers) and `init` sets it.
    let base = unsafe {
        let b = &mut *base.as_mut_ptr();
        b.init();
        b.callbacks = hw::callbacks_new();
        base.assume_init()
    };
    let pointer_gfx = if skin.is_null() {
        ptr::null_mut()
    } else {
        skin_gfx(skin, "PointerNormal")
    };
    let gui = if set.is_null() {
        ptr::null_mut()
    } else {
        g1_set::set_gui_rust(set)
    };
    Box::into_raw(Box::new(Gw {
        base,
        shell,
        set,
        skin,
        gui,
        ty,
        text: Vec::new(),
        name: Vec::new(),
        tooltip: Vec::new(),
        tooltip_enabled: false,
        font: ptr::null_mut(),
        font_type: 0,
        p: Pubf {
            font_color: BLACK,
            font_highlight: BLACK,
            font_size: Vector2f::default(),
            color_mul: WHITE,
        },
        pointer_gfx,
        shortcuts: Vec::new(),
        global_key_listener: false,
        global_ui_listener: false,
        user_data: ptr::null_mut(),
        user_value: 0,
        children: Vec::new(),
        items: Vec::new(),
        kind: Kind::Plain,
    }))
}

/// `iWidget::~iWidget` after the subclass destructor (Rust kinds tear down first here).
pub(crate) fn destroy(w: W) {
    let g = gwp(w);
    if g.is_null() {
        return;
    }
    gk::teardown(w);
    let set = set_of(w);
    if g1_set::tooltip_widget(set) == w {
        g1_set::set_tooltip_widget(set, 0);
    }
    let shortcuts = read(w, |g| g.shortcuts.clone());
    for c in shortcuts {
        // SAFETY: the set owns its shortcuts; `c` came from it.
        unsafe { g1_set::ohpl_g1_set_remove_global_shortcut(set, c as *mut _) };
    }
    while let Some(c) = read(w, |g| g.children.first().copied()) {
        remove_child(w, c);
        // a child that does not unlink would loop forever in the original too
    }
    let par = parent(w);
    if par != 0 {
        gk::call(par, Op::RemoveChild, w, &Msg::zero(), 0.0);
    }
    let cb = with(w, |g| {
        std::mem::replace(&mut g.base.callbacks, ptr::null_mut())
    });
    if let Some(cb) = cb {
        // SAFETY: from `callbacks_new`, used no more.
        unsafe { hw::callbacks_free(cb) };
    }
    // SAFETY: from `new_gw`, freed once.
    drop(unsafe { Box::from_raw(g) });
}

// ---------------------------------------------------------------- base behaviour

pub(crate) fn parent(w: W) -> W {
    read(w, |g| g.base.parent)
}

pub(crate) fn process_message(w: W, msg: i32, d: &Msg, skip_vis: bool, skip_en: bool) -> bool {
    hw::process_message(&mut GwHost, w, msg, d, skip_vis, skip_en)
}

pub(crate) fn add_callback(w: W, msg: i32, obj: *mut c_void, f: CallbackFn) {
    if let Some(g) = with(w, |g| &raw mut g.base) {
        // SAFETY: the widget's base is live; the list is only pushed to here.
        unsafe { hw::add_callback_to(g, msg, obj, f) };
    }
}

pub(crate) fn update(w: W, dt: f32) {
    hw::update(&mut GwHost, w, dt);
}

pub(crate) fn draw(w: W, dt: f32, clip: usize) {
    hw::draw(&mut GwHost, w, dt, clip);
}

pub(crate) fn is_visible(w: W) -> bool {
    hw::is_visible(&mut GwHost, w)
}

pub(crate) fn is_enabled(w: W) -> bool {
    hw::is_enabled(&mut GwHost, w)
}

pub(crate) fn set_enabled(w: W, v: bool) {
    hw::set_enabled(&mut GwHost, w, v);
}

pub(crate) fn set_visible(w: W, v: bool) {
    hw::set_visible(&mut GwHost, w, v);
}

pub(crate) fn is_connected_to(w: W, other: W, start: bool) -> bool {
    hw::is_connected_to(&mut GwHost, w, other, start)
}

pub(crate) fn clips_graphics(w: W) -> bool {
    hw::clips_graphics(&mut GwHost, w)
}

/// The virtual `PointIsInside`.
pub(crate) fn point_is_inside(w: W, p: Vector2f, only_clipped: bool) -> bool {
    let mut d = Msg::zero();
    d.set_pos([p.x, p.y]);
    gk::call(
        w,
        Op::PointIsInside,
        0,
        &d,
        if only_clipped { 1.0 } else { 0.0 },
    )
}

pub(crate) fn point_is_inside_base(w: W, p: Vector2f, only_clipped: bool) -> bool {
    hw::point_is_inside(&mut GwHost, w, p, only_clipped)
}

/// The virtual `AttachChild`.
pub(crate) fn attach_child(w: W, child: W) {
    gk::attach_child(w, child);
}

pub(crate) fn attach_child_base(w: W, child: W) {
    if child != 0 {
        hw::attach_child(&mut GwHost, w, child);
    }
}

/// The virtual `RemoveChild`.
pub(crate) fn remove_child(w: W, child: W) {
    gk::call(w, Op::RemoveChild, child, &Msg::zero(), 0.0);
}

pub(crate) fn remove_child_base(w: W, child: W) {
    hw::remove_child(&mut GwHost, w, child);
}

pub(crate) fn set_position(w: W, p: Vector3f) {
    hw::set_position(&mut GwHost, w, p);
}

pub(crate) fn set_global_position(w: W, p: Vector3f) {
    hw::set_global_position(&mut GwHost, w, p);
}

pub(crate) fn global_position(w: W) -> Vector3f {
    hw::global_position(&mut GwHost, w)
}

pub(crate) fn set_size(w: W, s: Vector2f) {
    hw::set_size(&mut GwHost, w, s);
}

pub(crate) fn size(w: W) -> Vector2f {
    read(w, |g| g.base.size)
}

pub(crate) fn set_text(w: W, t: &[u32]) {
    let same = read(w, |g| g.text == t);
    if same {
        return;
    }
    with(w, |g| g.text = t.to_vec());
    hw::text_changed_notify(&mut GwHost, w);
}

pub(crate) fn text(w: W) -> Vec<u32> {
    read(w, |g| g.text.clone())
}

/// `SetToolTip`.
pub(crate) fn set_tooltip(w: W, t: &[u32]) {
    with(w, |g| {
        g.tooltip = t.to_vec();
        g.tooltip_enabled = !t.is_empty();
    });
    if size(w) == Vector2f::default() {
        let (font, fsize, text) = read(w, |g| (g.font_type, g.p.font_size, g.text.clone()));
        let len = font_length(font, fsize, &text);
        set_size(w, Vector2f { x: len, y: fsize.y });
    }
}

pub(crate) fn font_length(font: usize, size: Vector2f, text: &[u32]) -> f32 {
    host().map_or(0.0, |h| {
        // SAFETY: `font` is a live iFontData (or the host copes with null as the original did).
        unsafe { (h.font_length)(font, &size, text.as_ptr(), text.len()) }
    })
}

pub(crate) fn font_rows(
    font: usize,
    width: f32,
    height: f32,
    size: Vector2f,
    text: &[u32],
) -> Vec<Vec<u32>> {
    unsafe extern "C" fn sink(ctx: *mut c_void, p: *const u32, n: usize) {
        // SAFETY: `ctx` is the Vec below; `p` holds `n` units.
        if let Some(v) = unsafe { ctx.cast::<Vec<Vec<u32>>>().as_mut() } {
            v.push(unsafe { std::slice::from_raw_parts(p, n) }.to_vec());
        }
    }
    let mut rows: Vec<Vec<u32>> = Vec::new();
    if let Some(h) = host() {
        // SAFETY: as `font_length`.
        unsafe {
            (h.font_rows)(
                font,
                width,
                height,
                &size,
                text.as_ptr(),
                text.len(),
                sink,
                ptr::from_mut(&mut rows).cast(),
            )
        };
    }
    rows
}

/// `iWidget::LoadGraphics`.
pub(crate) fn load_graphics(w: W) {
    let skin = skin_of(w);
    if !skin.is_null() {
        let f = skin_font(skin, "Default");
        let hl = skin_attr(skin, "DefaultFontHighlightColor");
        with(w, |g| {
            g.font = f;
            // SAFETY: the skin's default font is live.
            if let Some(f) = unsafe { f.as_ref() } {
                g.font_type = f.font;
                g.p.font_color = f.color;
                g.p.font_size = f.size;
            }
            g.p.font_highlight = Color {
                r: hl.x,
                g: hl.y,
                b: hl.z,
                a: 1.0,
            };
        });
    } else {
        with(w, |g| g.font = ptr::null_mut());
    }
    gk::on_load_graphics(w);
}

/// `iWidget::Init`.
pub(crate) fn init(w: W) {
    gk::on_init(w);
    load_graphics(w);
}

pub(crate) fn set_default_font_size(w: W, s: Vector2f) {
    gk::set_default_font_size(w, s);
}

pub(crate) fn set_default_font_type(w: W, f: usize) {
    gk::set_default_font_type(w, f);
}

pub(crate) fn set_default_font_color(w: W, c: Color) {
    gk::set_default_font_color(w, c);
}

pub(crate) fn get_focus(w: W, d: &Msg) -> bool {
    hw::focus_message(&mut GwHost, w, 6, Op::ChildGotFocus, d)
}

pub(crate) fn got_tab_focus(w: W, d: &Msg) -> bool {
    gk::call(w, Op::Virtual, 7, d, 0.0)
}

pub(crate) fn has_focus_navigation(w: W) -> bool {
    hw::has_focus_navigation(&mut GwHost, w)
}

pub(crate) fn focus_navigation(w: W, dir: i32) -> W {
    hw::focus_navigation(&mut GwHost, w, dir)
}

pub(crate) fn has_focus(w: W) -> bool {
    // SAFETY: the widget's set is live.
    g1_set::focused_widget(set_of(w)) == w
}

pub(crate) fn add_shortcut(
    w: W,
    modifiers: i32,
    key: i32,
    msg: i32,
    bypass_vis: bool,
    bypass_en: bool,
) -> usize {
    let set = set_of(w);
    let item = super::gw_menu::is_item(w);
    let (msg, bypass_vis, bypass_en) = if item {
        (super::gw_menu::MSG_BUTTON_PRESSED, true, false)
    } else {
        (msg, bypass_vis, bypass_en)
    };
    // SAFETY: the widget's set is live.
    let c = unsafe {
        g1_set::ohpl_g1_set_add_global_shortcut(set, modifiers, key, w, msg, bypass_vis, bypass_en)
    };
    with(w, |g| g.shortcuts.push(c as usize));
    if item {
        super::gw_menu::item_shortcut_added(w, &g1_set::shortcut_text(c as *mut _));
    }
    c as usize
}

pub(crate) fn world_to_local(w: W, p: Vector3f) -> Vector3f {
    let g = global_position(w);
    Vector3f {
        x: p.x - g.x,
        y: p.y - g.y,
        z: p.z - g.z,
    }
}

pub(crate) fn center_in_set(w: W) {
    hw::center_global_position_in_set(&mut GwHost, w);
}

pub(crate) fn set_children_offset(w: W, v: Vector3f) {
    hw::set_children_offset(&mut GwHost, w, v);
}

pub(crate) fn set_scroll_amount(w: W, v: Vector3f) {
    hw::set_scroll_amount(&mut GwHost, w, v);
}

pub(crate) fn set_affected_by_scroll(w: W, v: bool) {
    hw::set_affected_by_scroll(&mut GwHost, w, v);
}

// ---------------------------------------------------------------- drawing

/// `OhplWdRun` for one op list: gfx slots, texts and fonts as the kernels index them.
pub(crate) fn run_ops(
    w: W,
    ops: &[super::widget::Op],
    gfx: &[*mut Element],
    texts: &[&[u32]],
    fonts: &[usize],
) {
    use super::widget as wd;
    let set = set_of(w);
    for op in ops {
        match op.kind {
            wd::OP_OFFSET => g1_set::set_draw_offset(set, op.pos),
            wd::OP_GFX => {
                let g = gfx.get(op.gfx as usize).copied().unwrap_or(ptr::null_mut());
                let color = if op.flags & wd::HAS_COLOR != 0 {
                    op.color
                } else {
                    WHITE
                };
                let size = if op.flags & wd::HAS_SIZE != 0 {
                    op.size
                } else {
                    Vector2f { x: -1.0, y: -1.0 }
                };
                g1_set::draw_gfx_default(set, g, op.pos, size, color);
            }
            wd::OP_FONT => {
                let Some(text) = texts.get(op.text as usize) else {
                    continue;
                };
                let font = fonts.get(op.font as usize).copied().unwrap_or(0);
                let text = match usize::try_from(op.text_len) {
                    Ok(n) => &text[..n.min(text.len())],
                    Err(_) => text,
                };
                draw_font(set, font, text, op.size, op.pos, op.color, op.align);
            }
            _ => {}
        }
    }
}

pub(crate) fn draw_font(
    set: *mut Set,
    font: usize,
    text: &[u32],
    size: Vector2f,
    pos: Vector3f,
    color: Color,
    align: i32,
) {
    if let Some(h) = host() {
        // SAFETY: live set and font; the host copies the text.
        unsafe {
            (h.draw_font)(
                set.cast(),
                font,
                text.as_ptr(),
                text.len(),
                &size,
                &pos,
                &color,
                align,
            )
        };
    }
}

/// `GfxMetrics` of an element.
pub(crate) fn metrics(g: *mut Element) -> super::widget::GfxMetrics {
    let mut m = super::widget::GfxMetrics::default();
    if !g.is_null() {
        // SAFETY: a live element.
        unsafe {
            m.active = super::g1_gfx::ohpl_g1_gfx_active_size(g)
                .as_ref()
                .copied()
                .unwrap_or_default();
            super::g1_gfx::ohpl_g1_gfx_image_size(g, &mut m.image);
        }
    }
    m
}

/// Gfx slots and their metrics, as `OhplWdGfxTable`.
pub struct GfxTable {
    pub gfx: [*mut Element; super::widget::GFX_SLOTS],
    pub m: [super::widget::GfxMetrics; super::widget::GFX_SLOTS],
}

impl GfxTable {
    pub fn new() -> Self {
        GfxTable {
            gfx: [ptr::null_mut(); super::widget::GFX_SLOTS],
            m: [super::widget::GfxMetrics::default(); super::widget::GFX_SLOTS],
        }
    }
    pub fn set(&mut self, slot: usize, g: *mut Element) {
        if let (Some(a), Some(b)) = (self.gfx.get_mut(slot), self.m.get_mut(slot)) {
            *a = g;
            if !g.is_null() {
                *b = metrics(g);
            }
        }
    }
    pub fn set_frame(
        &mut self,
        bg: *mut Element,
        borders: &[*mut Element; 4],
        corners: &[*mut Element; 4],
    ) {
        self.set(0, bg);
        for i in 0..4 {
            self.set(1 + i, borders[i]);
            self.set(5 + i, corners[i]);
        }
    }
}

impl Default for GfxTable {
    fn default() -> Self {
        Self::new()
    }
}

/// `iWidget::DrawBordersAndCorners`.
pub(crate) fn draw_borders(
    w: W,
    bg: *mut Element,
    borders: &[*mut Element; 4],
    corners: &[*mut Element; 4],
    pos: Vector3f,
    size: Vector2f,
    outside: bool,
) {
    let mut t = GfxTable::new();
    t.set_frame(bg, borders, corners);
    let mut buf = [super::widget::Op::default(); 16];
    let mut o = super::widget::Ops::new(&mut buf);
    super::widget::borders_and_corners(&mut o, !bg.is_null(), outside, pos, size, &t.m);
    let n = o.len();
    run_ops(w, &buf[..n], &t.gfx, &[], &[]);
}

// ---------------------------------------------------------------- C ABI (facade)

pub(crate) fn rdc(c: *const Color) -> Color {
    // SAFETY: C++ passes a live `cColor`.
    unsafe { c.as_ref() }.copied().unwrap_or(WHITE)
}

/// # Safety
/// The element arrays hold four entries; the vectors are readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gw_draw_borders(
    w: W,
    bg: *mut Element,
    borders: *const *mut Element,
    corners: *const *mut Element,
    pos: *const Vector3f,
    size: *const Vector2f,
    outside: bool,
) {
    // SAFETY: caller contract.
    unsafe {
        let (Some(b), Some(c), Some(p), Some(s)) = (
            borders.cast::<[*mut Element; 4]>().as_ref(),
            corners.cast::<[*mut Element; 4]>().as_ref(),
            pos.as_ref(),
            size.as_ref(),
        ) else {
            return;
        };
        draw_borders(w, bg, b, c, *p, *s, outside);
    }
}

macro_rules! ffi {
    ($name:ident ( $($a:ident : $t:ty),* ) $(-> $r:ty)? => |$w:ident| $body:expr) => {
        #[unsafe(no_mangle)]
        pub extern "C" fn $name($w: W $(, $a: $t)*) $(-> $r)? {
            $body
        }
    };
}

/// # Safety
/// `set` and `skin` are live (or null); `shell` is the C++ object under construction.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gw_new(
    shell: W,
    ty: i32,
    set: *mut c_void,
    skin: *mut c_void,
) -> *mut Gw {
    new_gw(shell, ty, set.cast(), skin.cast())
}

ffi!(ohpl_gw_destroy() => |w| destroy(w));
ffi!(ohpl_gw_pub() -> *const Pubf => |w| read(w, |g| &raw const g.p as usize) as *const Pubf);
ffi!(ohpl_gw_last_shortcut() -> usize => |w| read(w, |g| g.shortcuts.last().copied().unwrap_or(0)));
ffi!(ohpl_gw_base() -> *mut Base => |w| GwHost.base(w));
ffi!(ohpl_gw_type() -> i32 => |w| read(w, |g| g.ty));
ffi!(ohpl_gw_set() -> *mut c_void => |w| set_of(w).cast());
ffi!(ohpl_gw_skin() -> *mut c_void => |w| skin_of(w).cast());
ffi!(ohpl_gw_gui() -> *mut c_void => |w| gui_of(w).cast());
ffi!(ohpl_gw_init() => |w| init(w));
ffi!(ohpl_gw_load_graphics() => |w| load_graphics(w));
ffi!(ohpl_gw_update(dt: f32) => |w| update(w, dt));
ffi!(ohpl_gw_draw(dt: f32, clip: usize) => |w| draw(w, dt, clip));
ffi!(ohpl_gw_is_enabled() -> bool => |w| is_enabled(w));
ffi!(ohpl_gw_is_visible() -> bool => |w| is_visible(w));
ffi!(ohpl_gw_set_enabled(v: bool) => |w| set_enabled(w, v));
ffi!(ohpl_gw_set_visible(v: bool) => |w| set_visible(w, v));
ffi!(ohpl_gw_has_focus() -> bool => |w| has_focus(w));
ffi!(ohpl_gw_clips_graphics() -> bool => |w| clips_graphics(w));
ffi!(ohpl_gw_is_connected_to(o: W, start: bool) -> bool => |w| is_connected_to(w, o, start));
ffi!(ohpl_gw_attach_child_base(c: W) => |w| attach_child_base(w, c));
ffi!(ohpl_gw_attach_child(c: W) => |w| attach_child(w, c));
ffi!(ohpl_gw_remove_child_base(c: W) => |w| remove_child_base(w, c));
ffi!(ohpl_gw_remove_child(c: W) => |w| remove_child(w, c));
ffi!(ohpl_gw_parent() -> W => |w| parent(w));
ffi!(ohpl_gw_child_count() -> usize => |w| read(w, |g| g.children.len()));
ffi!(ohpl_gw_child(i: usize) -> W => |w| read(w, |g| g.children.get(i).copied().unwrap_or(0)));
ffi!(ohpl_gw_mouse_over() -> bool => |w| read(w, |g| g.base.mouse_over));
ffi!(ohpl_gw_set_clip_active(v: bool) => |w| { with(w, |g| g.base.clips = v); });
ffi!(ohpl_gw_clip_active() -> bool => |w| read(w, |g| g.base.clips));
ffi!(ohpl_gw_connected_to_children() -> bool => |w| read(w, |g| g.base.connected_to_children));
ffi!(ohpl_gw_set_connected_to_children(v: bool) => |w| { with(w, |g| g.base.connected_to_children = v); });
ffi!(ohpl_gw_set_callbacks_disabled(v: bool) => |w| { with(w, |g| g.base.callbacks_disabled = v); });
ffi!(ohpl_gw_callbacks_disabled() -> bool => |w| read(w, |g| g.base.callbacks_disabled));
ffi!(ohpl_gw_set_global_key_listener(v: bool) => |w| { with(w, |g| g.global_key_listener = v); });
ffi!(ohpl_gw_global_key_listener() -> bool => |w| read(w, |g| g.global_key_listener));
ffi!(ohpl_gw_set_global_ui_listener(v: bool) => |w| { with(w, |g| g.global_ui_listener = v); });
ffi!(ohpl_gw_global_ui_listener() -> bool => |w| read(w, |g| g.global_ui_listener));
ffi!(ohpl_gw_set_user_data(p: *mut c_void) => |w| { with(w, |g| g.user_data = p); });
ffi!(ohpl_gw_user_data() -> *mut c_void => |w| read(w, |g| g.user_data as usize) as *mut c_void);
ffi!(ohpl_gw_set_user_value(v: i32) => |w| { with(w, |g| g.user_value = v); });
ffi!(ohpl_gw_user_value() -> i32 => |w| read(w, |g| g.user_value));
ffi!(ohpl_gw_tooltip_enabled() -> bool => |w| read(w, |g| g.tooltip_enabled));
ffi!(ohpl_gw_set_tooltip_enabled(v: bool) => |w| { with(w, |g| g.tooltip_enabled = v); });
ffi!(ohpl_gw_pointer_gfx() -> *mut Element => |w| read(w, |g| g.pointer_gfx as usize) as *mut Element);
ffi!(ohpl_gw_set_pointer_gfx(e: *mut Element) => |w| { with(w, |g| g.pointer_gfx = e); });
ffi!(ohpl_gw_font_type() -> usize => |w| read(w, |g| g.font_type));
ffi!(ohpl_gw_skin_font() -> *mut SkinFont => |w| read(w, |g| g.font as usize) as *mut SkinFont);
ffi!(ohpl_gw_set_font_type_base(f: usize) => |w| { with(w, |g| g.font_type = f); });
ffi!(ohpl_gw_set_font_type(f: usize) => |w| set_default_font_type(w, f));
ffi!(ohpl_gw_set_font_size_base(x: f32, y: f32) => |w| { with(w, |g| g.p.font_size = Vector2f { x, y }); });
ffi!(ohpl_gw_set_font_size(x: f32, y: f32) => |w| set_default_font_size(w, Vector2f { x, y }));
ffi!(ohpl_gw_set_font_color_base(c: *const Color) => |w| { with(w, |g| g.p.font_color = rdc(c)); });
ffi!(ohpl_gw_set_font_color(c: *const Color) => |w| set_default_font_color(w, rdc(c)));
ffi!(ohpl_gw_set_color_mul(c: *const Color) => |w| { with(w, |g| g.p.color_mul = rdc(c)); });
ffi!(ohpl_gw_set_global_position_(x: f32, y: f32, z: f32) => |w| set_global_position(w, Vector3f { x, y, z }));
ffi!(ohpl_gw_set_position_(x: f32, y: f32, z: f32) => |w| set_position(w, Vector3f { x, y, z }));
ffi!(ohpl_gw_set_size_(x: f32, y: f32) => |w| set_size(w, Vector2f { x, y }));
ffi!(ohpl_gw_center_in_set() => |w| center_in_set(w));
ffi!(ohpl_gw_set_children_offset_(x: f32, y: f32, z: f32) => |w| set_children_offset(w, Vector3f { x, y, z }));
ffi!(ohpl_gw_set_scroll_amount_(x: f32, y: f32, z: f32) => |w| set_scroll_amount(w, Vector3f { x, y, z }));
ffi!(ohpl_gw_set_affected_by_scroll(v: bool) => |w| set_affected_by_scroll(w, v));
ffi!(ohpl_gw_global_ptr() -> *const Vector3f => |w| {
    global_position(w);
    read(w, |x| {
        if x.base.parent != 0 { &raw const x.base.global as usize } else { &raw const x.base.pos as usize }
    }) as *const Vector3f
});
ffi!(ohpl_gw_has_focus_navigation() -> bool => |w| hw::has_focus_navigation(&mut GwHost, w));
ffi!(ohpl_gw_set_focus_navigation(dir: i32, to: W) => |w| hw::set_focus_navigation(&mut GwHost, w, dir, to));
ffi!(ohpl_gw_focus_navigation(dir: i32) -> W => |w| hw::focus_navigation(&mut GwHost, w, dir));

/// # Safety
/// `d` is a `cGuiMessageData`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gw_process_message(
    w: W,
    msg: i32,
    d: *const Msg,
    skip_vis: bool,
    skip_en: bool,
) -> bool {
    // SAFETY: caller contract.
    let d = unsafe { d.as_ref() }.copied().unwrap_or(Msg::zero());
    process_message(w, msg, &d, skip_vis, skip_en)
}

/// # Safety
/// `f` is a `tGuiCallbackFunc`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gw_add_callback(w: W, msg: i32, obj: *mut c_void, f: CallbackFn) {
    add_callback(w, msg, obj, f);
}

/// # Safety
/// `d` is a `cGuiMessageData`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gw_focus_message(w: W, msg: i32, op: i32, d: *const Msg) -> bool {
    // SAFETY: caller contract.
    let d = unsafe { d.as_ref() }.copied().unwrap_or(Msg::zero());
    match op {
        8 => hw::focus_message(&mut GwHost, w, msg, Op::ChildGotFocus, &d),
        9 => hw::focus_message(&mut GwHost, w, msg, Op::ChildGotTabFocus, &d),
        10 => hw::focus_message(&mut GwHost, w, msg, Op::ChildLostFocus, &d),
        _ => false,
    }
}

ffi!(ohpl_gw_point_is_inside(x: f32, y: f32, only: bool) -> bool => |w| point_is_inside(w, Vector2f { x, y }, only));
ffi!(ohpl_gw_point_is_inside_base(x: f32, y: f32, only: bool) -> bool => |w| point_is_inside_base(w, Vector2f { x, y }, only));

/// # Safety
/// `t` is readable for `n` wide characters.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gw_set_text(w: W, t: *const u32, n: usize) {
    // SAFETY: caller contract.
    set_text(w, unsafe { std::slice::from_raw_parts(t, n) });
}

/// # Safety
/// `n` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gw_text(w: W, n: *mut usize) -> *const u32 {
    // SAFETY: caller contract; the text is stable until the next `set_text`.
    read(w, |g| {
        if let Some(n) = unsafe { n.as_mut() } {
            *n = g.text.len();
        }
        g.text.as_ptr() as usize
    }) as *const u32
}

/// # Safety
/// `t` is readable for `n` wide characters.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gw_set_tooltip(w: W, t: *const u32, n: usize) {
    // SAFETY: caller contract.
    set_tooltip(w, unsafe { std::slice::from_raw_parts(t, n) });
}

/// # Safety
/// `n` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gw_tooltip(w: W, n: *mut usize) -> *const u32 {
    // SAFETY: as `ohpl_gw_text`.
    read(w, |g| {
        if let Some(n) = unsafe { n.as_mut() } {
            *n = g.tooltip.len();
        }
        g.tooltip.as_ptr() as usize
    }) as *const u32
}

/// # Safety
/// `t` is readable for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gw_set_name(w: W, t: *const u8, n: usize) {
    // SAFETY: caller contract.
    let s = unsafe { std::slice::from_raw_parts(t, n) }.to_vec();
    with(w, |g| g.name = s);
}

/// # Safety
/// `n` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gw_name(w: W, n: *mut usize) -> *const u8 {
    // SAFETY: as `ohpl_gw_text`.
    read(w, |g| {
        if let Some(n) = unsafe { n.as_mut() } {
            *n = g.name.len();
        }
        g.name.as_ptr() as usize
    }) as *const u8
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gw_add_shortcut(
    w: W,
    m: i32,
    key: i32,
    msg: i32,
    bv: bool,
    be: bool,
) -> usize {
    add_shortcut(w, m, key, msg, bv, be)
}
