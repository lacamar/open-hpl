//! `cGuiSet`, `cGuiClipRegion` and `cGuiGlobalShortcut`, owned here. C++ holds only the set
//! pointer (opaque class; plain fields live in [`Pub`] and are read by inline accessors).
//! Widgets are the `gw` objects; popups, fonts and the graphics device stay C++ objects.
//!
//! Re-entrancy: widget code calls back into the set, so no `&mut Set` lives across a call to
//! widgets; every access is a short raw-pointer scope.

use std::ffi::c_void;
use std::ptr;
use std::sync::atomic::{AtomicU32, Ordering};

use super::draw::{self, Frame, Host as DrawHost, Text};
use super::g1_gfx::{self, Element};
use super::g1_gui::{self, Gui};
use super::g1_host::{self, Sink};
use super::g1_skin::Skin;
use super::gw;
use super::gw_kinds;
use super::layout::{self, TooltipInput};
use super::list::RenderList;
use super::set::W;
use super::set::{self, Core, Host, Msg, SetState};
use crate::math::{Rect2f, Vector2f, Vector3f};
use crate::types::{Color, Vector2l};

const SKIN_POINTER_NORMAL: i32 = 0;

static GLOBAL_SCALE: AtomicU32 = AtomicU32::new(0x3f80_0000);

pub struct Clip {
    rect: Rect2f,
    #[allow(clippy::vec_box)] // C++ keeps the child pointers
    children: Vec<Box<Clip>>,
}

pub struct Shortcut {
    enabled: bool,
    bypass_visibility: bool,
    bypass_enabled: bool,
    key: [i32; 3],
    widget: W,
    message: i32,
}

/// Fields C++ reads and writes directly; mirrored by `OhplG1SetPub` in `RustGuiG1.h`.
#[repr(C)]
pub struct Pub {
    pub state: SetState,
    pub virtual_size: Vector2f,
    pub virtual_offset: Vector2f,
    pub draw_offset: Vector3f,
    pub size_3d: Vector3f,
    pub mtx_3d: [f32; 16],
    pub mouse_z: f32,
    pub draw_prio: i32,
    pub programs: [usize; 2],
    pub active: bool,
    pub draw_mouse: bool,
    pub is_3d: bool,
    pub renders_before_post: bool,
    pub cull_backface: bool,
    pub flip_y: bool,
    pub draw_focus: bool,
    pub destroying: bool,
    pub sort_widgets: bool,
}

type FocusCb = unsafe extern "C" fn(*mut c_void, W, *const Msg) -> bool;

#[repr(C)]
pub struct Set {
    /// First: the C++ side reads it through the set pointer.
    p: Pub,
    name: Vec<u8>,
    gui: *mut Gui,
    gui_shell: *mut c_void,
    skin: *mut Skin,
    resources: *mut c_void,
    graphics: *mut c_void,
    sound: *mut c_void,
    scene: *mut c_void,
    root: W,
    /// The widget list, oldest first; the original's `std::list` is this reversed (new widgets
    /// go to its front). 0 marks an erased slot so iteration cursors stay valid.
    widgets: Vec<W>,
    list: RenderList,
    popup_count: i32,
    last_popup_z: f32,
    context_menu_z: f32,
    virtual_min_z: f32,
    virtual_max_z: f32,
    pointer: *mut Element,
    popups: Vec<usize>,
    base_clip: Box<Clip>,
    current_clip: *mut Clip,
    #[allow(clippy::vec_box)] // C++ keeps the shortcut pointers
    shortcuts: Vec<Box<Shortcut>>,
    tooltip_time_to_pop_up: f32,
    tip_frame: W,
    tip_background: W,
    tip_label: W,
    old_destroying: bool,
    topmost_destroying: W,
    focus_obj: *mut c_void,
    focus_cb: Option<FocusCb>,
}

fn global_scale() -> f32 {
    f32::from_bits(GLOBAL_SCALE.load(Ordering::Relaxed))
}

/// # Safety
/// `s` is null or a live set; the borrow must end before any call into a widget.
unsafe fn st<'a>(s: *mut Set) -> Option<&'a mut Set> {
    // SAFETY: caller contract.
    unsafe { s.as_mut() }
}

fn core(s: *mut Set) -> Option<Core> {
    // SAFETY: the state lives in the set for as long as the handle is used.
    (!s.is_null()).then(|| unsafe { Core::new(&raw mut (*s).p.state) })
}

fn with<R: Default>(s: *mut Set, f: impl FnOnce(&mut Set) -> R) -> R {
    with_or(s, R::default(), f)
}

fn with_or<R>(s: *mut Set, default: R, f: impl FnOnce(&mut Set) -> R) -> R {
    // SAFETY: `s` is live and `f` makes no call into Ops.
    unsafe { st(s) }.map_or(default, f)
}

pub struct SetHost(*mut Set);

impl Host for SetHost {
    fn is_visible(&mut self, w: W) -> bool {
        gw::is_visible(w)
    }
    fn is_enabled(&mut self, w: W) -> bool {
        gw::is_enabled(w)
    }
    fn is_connected_to(&mut self, w: W, other: W) -> bool {
        gw::is_connected_to(w, other, true)
    }
    fn point_is_inside(&mut self, w: W, p: [f32; 2]) -> bool {
        gw::point_is_inside(w, Vector2f { x: p[0], y: p[1] }, false)
    }
    fn mouse_is_over(&mut self, w: W) -> bool {
        gw::read(w, |g| g.base.mouse_over)
    }
    fn set_mouse_is_over(&mut self, w: W, v: bool) {
        gw::with(w, |g| g.base.mouse_over = v);
    }
    fn process_message(&mut self, w: W, msg: i32, d: &Msg, skip_vis: bool, skip_en: bool) -> bool {
        gw::process_message(w, msg, d, skip_vis, skip_en)
    }
    fn get_focus(&mut self, w: W, d: &Msg) -> bool {
        gw::get_focus(w, d)
    }
    fn tooltip_enabled(&mut self, w: W) -> bool {
        gw::read(w, |g| g.tooltip_enabled)
    }
    fn has_focus_nav(&mut self, w: W) -> bool {
        gw::has_focus_navigation(w)
    }
    fn parent(&mut self, w: W) -> W {
        gw::parent(w)
    }
    fn widget_set(&mut self, w: W) -> usize {
        gw::set_of(w) as usize
    }
    fn pointer_gfx(&mut self, w: W) -> usize {
        gw::read(w, |g| g.pointer_gfx as usize)
    }
    fn focus_nav(&mut self, w: W, dir: i32) -> W {
        gw::focus_navigation(w, dir)
    }
    fn global_key_listener(&mut self, w: W) -> bool {
        gw::read(w, |g| g.global_key_listener)
    }
    fn global_ui_listener(&mut self, w: W) -> bool {
        gw::read(w, |g| g.global_ui_listener)
    }
    fn got_tab_focus(&mut self, w: W, d: &Msg) -> bool {
        gw::got_tab_focus(w, d)
    }
    fn is_valid(&mut self, w: W) -> bool {
        w == 0 || with(self.0, |s| s.widgets.contains(&w))
    }
    fn list_begin(&mut self) -> usize {
        with(self.0, |s| prev_live(&s.widgets, s.widgets.len()))
    }
    fn list_end(&mut self, it: usize) -> bool {
        it == 0
    }
    fn list_get(&mut self, it: usize) -> W {
        with(self.0, |s| {
            it.checked_sub(1)
                .and_then(|i| s.widgets.get(i).copied())
                .unwrap_or(0)
        })
    }
    fn list_next(&mut self, it: usize) -> usize {
        with(self.0, |s| prev_live(&s.widgets, it.saturating_sub(1)))
    }
    fn sort_widgets(&mut self) {
        let set = self.0;
        let need = with(set, |s| std::mem::take(&mut s.p.sort_widgets));
        if !need {
            return;
        }
        let list: Vec<W> = with(set, |s| {
            s.widgets
                .iter()
                .rev()
                .copied()
                .filter(|w| *w != 0)
                .collect()
        });
        let mut keyed: Vec<(f32, W)> = list
            .into_iter()
            .map(|w| (self.global_pos(w)[2], w))
            .collect();
        keyed.sort_by(|a, b| {
            if a.0 > b.0 {
                std::cmp::Ordering::Less
            } else if b.0 > a.0 {
                std::cmp::Ordering::Greater
            } else {
                std::cmp::Ordering::Equal
            }
        });
        with(set, |s| {
            s.widgets = keyed.into_iter().rev().map(|(_, w)| w).collect();
        });
    }
    fn current_pointer(&mut self) -> usize {
        with(self.0, |s| s.pointer as usize)
    }
    fn set_current_pointer(&mut self, g: usize) {
        with(self.0, |s| s.pointer = g as *mut Element);
    }
    fn skin_normal_pointer(&mut self) -> usize {
        with(self.0, |s| skin_gfx(s.skin, SKIN_POINTER_NORMAL) as usize)
    }
    fn exec_shortcut(&mut self, d: &Msg) -> bool {
        exec_shortcut(self.0, d)
    }
    fn global_pos(&mut self, w: W) -> [f32; 3] {
        let v = gw::global_position(w);
        [v.x, v.y, v.z]
    }
    fn set_global_pos(&mut self, w: W, p: [f32; 3]) {
        gw::set_global_position(
            w,
            Vector3f {
                x: p[0],
                y: p[1],
                z: p[2],
            },
        );
    }
}

/// Highest live slot below `end`, as a cursor (index + 1); 0 when none.
fn prev_live(v: &[W], end: usize) -> usize {
    v.get(..end)
        .and_then(|s| s.iter().rposition(|w| *w != 0))
        .map_or(0, |i| i + 1)
}

fn skin_gfx(skin: *mut Skin, ty: i32) -> *mut Element {
    // SAFETY: a set's skin is live as long as the gui.
    unsafe { super::g1_skin::ohpl_g1_skin_gfx(skin, ty) }
}

fn exec_shortcut(set: *mut Set, d: &Msg) -> bool {
    let n = with(set, |s| s.shortcuts.len());
    for i in 0..n {
        let Some((enabled, key, w, msg, bv, be)) = with(set, |s| {
            s.shortcuts.get(i).map(|c| {
                (
                    c.enabled,
                    c.key,
                    c.widget,
                    c.message,
                    c.bypass_visibility,
                    c.bypass_enabled,
                )
            })
        }) else {
            break;
        };
        if enabled && key[2] == d.modifier() && key[0] == d.key() && exec(w, msg, bv, be) {
            return true;
        }
    }
    false
}

fn exec(w: W, msg: i32, bypass_visibility: bool, bypass_enabled: bool) -> bool {
    w != 0 && gw::process_message(w, msg, &Msg::zero(), bypass_visibility, bypass_enabled)
}

fn new_clip() -> Box<Clip> {
    Box::new(Clip {
        rect: Rect2f {
            x: 0.0,
            y: 0.0,
            w: -1.0,
            h: -1.0,
        },
        children: Vec::new(),
    })
}

fn screen_size(graphics: *mut c_void) -> Vector2f {
    let mut s = [0.0f32; 2];
    if let Some(h) = g1_host::host() {
        // SAFETY: host contract.
        unsafe { (h.screen_size)(graphics, &mut s) };
    }
    Vector2f { x: s[0], y: s[1] }
}

fn set_virtual_size(
    s: &mut Set,
    size: Vector2f,
    min_z: f32,
    max_z: f32,
    offset: Vector2f,
    ignore: bool,
) {
    s.virtual_min_z = min_z;
    s.virtual_max_z = max_z;
    s.p.virtual_size = layout::virtual_size(size, global_scale(), ignore);
    s.p.virtual_offset = offset;
}

fn set_skin(s: &mut Set, skin: *mut Skin) {
    s.skin = skin;
    s.pointer = if skin.is_null() {
        ptr::null_mut()
    } else {
        skin_gfx(skin, SKIN_POINTER_NORMAL)
    };
}

/// `cGuiSet::cGuiSet`.
///
/// # Safety
/// `gui` is live; the engine pointers outlive the set.
#[allow(clippy::too_many_arguments)]
pub unsafe fn create(
    gui: *mut Gui,
    shell: *mut c_void,
    name: &[u8],
    skin: *mut Skin,
    resources: *mut c_void,
    graphics: *mut c_void,
    sound: *mut c_void,
    scene: *mut c_void,
) -> *mut Set {
    let screen = screen_size(graphics);
    let mut set = Box::new(Set {
        p: Pub {
            state: SetState {
                focused: 0,
                attention: 0,
                default_nav: 0,
                tab_widget: 0,
                tooltip_widget: 0,
                tooltip_timer: 0.0,
                mouse_pos: [0.0; 2],
                window_z: -1.0,
                mouse_down: [false; 9],
                mouse_movement: true,
                lists: set::lists_new(),
            },
            virtual_size: Vector2f::default(),
            virtual_offset: Vector2f::default(),
            draw_offset: Vector3f::default(),
            size_3d: Vector3f {
                x: 1.0,
                y: 1.0,
                z: 1.0,
            },
            mtx_3d: IDENTITY,
            mouse_z: 1000.0,
            draw_prio: 0,
            programs: [0; 2],
            active: true,
            draw_mouse: true,
            is_3d: false,
            renders_before_post: false,
            cull_backface: false,
            flip_y: false,
            draw_focus: false,
            destroying: false,
            sort_widgets: false,
        },
        name: name.to_vec(),
        gui,
        gui_shell: shell,
        skin: ptr::null_mut(),
        resources,
        graphics,
        sound,
        scene,
        root: 0,
        widgets: Vec::new(),
        list: RenderList::default(),
        popup_count: 0,
        last_popup_z: 20.0,
        context_menu_z: 500.0,
        virtual_min_z: -1000.0,
        virtual_max_z: 1000.0,
        pointer: ptr::null_mut(),
        popups: Vec::new(),
        base_clip: new_clip(),
        current_clip: ptr::null_mut(),
        shortcuts: Vec::new(),
        tooltip_time_to_pop_up: 0.5,
        tip_frame: 0,
        tip_background: 0,
        tip_label: 0,
        old_destroying: false,
        topmost_destroying: 0,
        focus_obj: ptr::null_mut(),
        focus_cb: None,
    });
    set.current_clip = &raw mut *set.base_clip;
    set_virtual_size(
        &mut set,
        screen,
        -1000.0,
        1000.0,
        Vector2f::default(),
        false,
    );
    let p = Box::into_raw(set);
    let root = gw_kinds::root_new(p);
    with(p, |s| s.root = root);
    with(p, |s| set_skin(s, skin));
    if !skin.is_null() {
        let t = gw_kinds::tooltip_create(p);
        with(p, |s| {
            s.tip_frame = t[0];
            s.tip_background = t[1];
            s.tip_label = t[2];
        });
    }
    p
}

const IDENTITY: [f32; 16] = [
    1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0,
];

/// `cGuiSet::~cGuiSet`.
///
/// # Safety
/// `s` is from [`create`], freed once.
pub unsafe fn free(s: *mut Set) {
    if s.is_null() {
        return;
    }
    with(s, |s| s.p.destroying = true);
    let popups = with(s, |s| std::mem::take(&mut s.popups));
    delete_popups(popups);
    let widgets: Vec<W> = with(s, |s| {
        s.widgets
            .iter()
            .rev()
            .copied()
            .filter(|w| *w != 0)
            .collect()
    });
    for w in widgets {
        gw_kinds::delete(w);
    }
    with(s, |s| s.widgets.clear());
    let root = with(s, |s| s.root);
    if root != 0 {
        gw_kinds::delete(root);
    }
    with(s, |s| s.p.destroying = false);
    clear_shortcuts(s);
    // SAFETY: caller contract; the state's lists were allocated by `create`.
    unsafe {
        let set = Box::from_raw(s);
        set::lists_free(set.p.state.lists);
    }
}

fn delete_popups(popups: Vec<usize>) {
    for p in popups {
        // SAFETY: the set owned the popup.
        unsafe { super::g2_popup::ohpl_g2_popup_free(p as *mut _) };
    }
}

fn clear_shortcuts(s: *mut Set) {
    with(s, |s| s.shortcuts.clear());
}

pub(crate) fn is_active(s: *mut Set) -> bool {
    with(s, |s| s.p.active)
}

/// Virtual size then virtual size offset.
pub(crate) fn virtual_rect(s: *mut Set) -> [f32; 4] {
    with(s, |s| {
        [
            s.p.virtual_size.x,
            s.p.virtual_size.y,
            s.p.virtual_offset.x,
            s.p.virtual_offset.y,
        ]
    })
}

/// `cGuiSet::Update`.
pub(crate) fn update(s: *mut Set, dt: f32) {
    let popups = with(s, |s| std::mem::take(&mut s.popups));
    delete_popups(popups);
    let mut host = SetHost(s);
    let mut it = host.list_begin();
    while !host.list_end(it) {
        let w = host.list_get(it);
        gw::update(w, dt);
        it = host.list_next(it);
    }
    update_tooltip(s, dt);
}

/// `cGuiSet::DrawAll`.
pub(crate) fn draw_all(s: *mut Set, dt: f32) {
    let Some((active, base, root)) = with(s, |s| {
        Some((s.p.active, ptr::from_mut(&mut *s.base_clip), s.root))
    }) else {
        return;
    };
    if !active {
        return;
    }
    with(s, |s| s.current_clip = base);
    gw::draw(root, dt, base as usize);
    with(s, |s| s.current_clip = base);
}

fn tooltip_hide_if_visible(frame: W) {
    if gw::is_visible(frame) {
        gw_kinds::hide(frame);
    }
}

fn update_tooltip(s: *mut Set, dt: f32) {
    let Some((skin, frame, tip, timer, time_to_pop)) = with(s, |s| {
        Some((
            s.skin,
            s.tip_frame,
            s.p.state.tooltip_widget,
            s.p.state.tooltip_timer,
            s.tooltip_time_to_pop_up,
        ))
    }) else {
        return;
    };
    if skin.is_null() || frame == 0 {
        return;
    }
    if tip == 0 || !gw::is_visible(tip) {
        tooltip_hide_if_visible(frame);
        return;
    }
    if timer >= time_to_pop {
        if gw::is_visible(frame) {
            return;
        }
        let Some((label, bg, vsize, mouse, mouse_z, pointer)) = with(s, |s| {
            Some((
                s.tip_label,
                s.tip_background,
                s.p.virtual_size,
                s.p.state.mouse_pos,
                s.p.mouse_z,
                s.pointer,
            ))
        }) else {
            return;
        };
        let (info, rows) = gw_kinds::tooltip_measure(label, tip, vsize.x * 0.4);
        let mut pointer_size = Vector2f::default();
        // SAFETY: a set's pointer gfx is live.
        unsafe { g1_gfx::ohpl_g1_gfx_image_size(pointer, &mut pointer_size) };
        let t = layout::tooltip(&TooltipInput {
            mouse: Vector2f {
                x: mouse[0],
                y: mouse[1],
            },
            mouse_z,
            pointer_size,
            virtual_size: vsize,
            text_length: info[0],
            font_height: info[1],
            rows,
        });
        let args = [
            t.pos.x,
            t.pos.y,
            t.pos.z,
            t.size.x,
            t.size.y,
            t.background_size.x,
            t.background_size.y,
        ];
        gw_kinds::tooltip_apply(label, frame, bg, tip, &args);
    } else {
        with(s, |s| s.p.state.tooltip_timer += dt);
        tooltip_hide_if_visible(frame);
    }
}

/// `cGuiSet::SendMessage`.
pub(crate) fn send_message(s: *mut Set, msg: i32, data: &Msg) -> bool {
    let Some(c) = core(s) else { return false };
    c.send_message(&mut SetHost(s), msg, data)
}

fn frame(s: &Set) -> Frame {
    let clip = s.current_clip;
    // SAFETY: the current clip is the base clip or a child kept by it.
    let rect = unsafe { clip.as_ref() }.map_or_else(Rect2f::default, |c| c.rect);
    Frame {
        clip: clip.cast_const().cast(),
        clip_rect: rect,
        draw_offset: s.p.draw_offset,
    }
}

unsafe extern "C" fn host_gfx_info(_: *mut c_void, g: *mut c_void, out: *mut draw::GfxInfo) {
    // SAFETY: `g` is a live element, `out` writable.
    unsafe { g1_gfx::ohpl_g1_gfx_info(g.cast(), out) };
}

unsafe extern "C" fn host_gfx_flush(_: *mut c_void, g: *mut c_void) {
    // SAFETY: `g` is a live element.
    unsafe { g1_gfx::ohpl_g1_gfx_flush(g.cast()) };
}

unsafe extern "C" fn host_material(ctx: *mut c_void, m: i32) -> *mut c_void {
    let gui = with(ctx.cast(), |s: &mut Set| s.gui);
    // SAFETY: the set's gui is live.
    unsafe { g1_gui::ohpl_g1_gui_material(gui, m) }
        .cast_mut()
        .cast()
}

fn draw_host(s: *mut Set) -> DrawHost {
    DrawHost {
        ctx: s.cast(),
        gfx_info: host_gfx_info,
        gfx_flush: host_gfx_flush,
        material: host_material,
    }
}

/// `cGuiSet::DrawGfx` (after the trace hook).
///
/// # Safety
/// `s` live; `g` a live element; pointers readable.
#[allow(clippy::too_many_arguments)]
pub unsafe fn draw_gfx(
    s: *mut Set,
    g: *mut Element,
    pos: Vector3f,
    size: Vector2f,
    color: Color,
    material: i32,
    angle: f32,
    pivot: Option<Vector3f>,
) {
    let host = draw_host(s);
    // SAFETY: caller contract; the render list is only touched here and the host calls
    // (element reads) never reach the set's list.
    let Some(set) = (unsafe { st(s) }) else {
        return;
    };
    let fr = frame(set);
    let d = draw::Gfx {
        pos,
        size,
        color,
        material,
        angle,
        pivot: pivot.map(|p| Vector2f { x: p.x, y: p.y }),
        gfx: g.cast(),
        host: &host,
    };
    draw::draw_gfx(&mut set.list, &fr, &d);
}

/// `cGuiSet::DrawTextFromCharArry`.
///
/// # Safety
/// `s` live; `table` describes a live font; `text` has `len` units.
#[allow(clippy::too_many_arguments)]
pub unsafe fn draw_text(
    s: *mut Set,
    table: &draw::GlyphTable,
    text: &[u32],
    size: Vector2f,
    pos: Vector3f,
    color: Color,
    material: i32,
    align: i32,
) {
    let host = draw_host(s);
    // SAFETY: caller contract.
    let Some(set) = (unsafe { st(s) }) else {
        return;
    };
    let fr = frame(set);
    let t = Text {
        text,
        size,
        pos,
        color,
        material,
        align,
        host: &host,
    };
    draw::draw_text(&mut set.list, &fr, table, &t);
}

/// `cGuiSet::DestroyWidget`.
pub(crate) fn destroy_widget(s: *mut Set, w: W, destroy_children: bool) {
    if let Some(c) = core(s) {
        c.destroy_widget_prelude(w);
    }
    let has_children = gw_kinds::first_child(w) != 0;
    if destroy_children && has_children {
        with(s, |s| {
            if s.topmost_destroying == 0 {
                s.topmost_destroying = w;
                s.old_destroying = s.p.destroying;
                s.p.destroying = true;
            }
        });
        loop {
            let child = gw_kinds::first_child(w);
            if child == 0 {
                break;
            }
            destroy_widget(s, child, true);
        }
    }
    with(s, |s| {
        if let Some(i) = s.widgets.iter().rposition(|x| *x == w)
            && let Some(slot) = s.widgets.get_mut(i)
        {
            *slot = 0;
        }
    });
    gw_kinds::delete(w);
    with(s, |s| {
        if s.topmost_destroying == w {
            s.topmost_destroying = 0;
            s.p.destroying = s.old_destroying;
        }
    });
}

/// `cGuiSet::DestroyAllWidgets`.
pub(crate) fn destroy_all_widgets(s: *mut Set) {
    with(s, |s| s.p.destroying = true);
    let popups = with(s, |s| std::mem::take(&mut s.popups));
    delete_popups(popups);
    let widgets: Vec<W> = with(s, |s| {
        s.widgets
            .iter()
            .rev()
            .copied()
            .filter(|w| *w != 0)
            .collect()
    });
    for w in widgets {
        gw_kinds::delete(w);
    }
    let root = with(s, |s| s.root);
    gw_kinds::clear_children(root);
    with(s, |s| {
        s.widgets.clear();
        s.p.state.focused = 0;
        s.p.state.attention = 0;
        s.p.destroying = false;
        s.p.state.tooltip_widget = 0;
    });
    {
        let t = gw_kinds::tooltip_create(s);
        with(s, |s| {
            s.tip_frame = t[0];
            s.tip_background = t[1];
            s.tip_label = t[2];
        });
    }
}

// ---- C ABI ----

macro_rules! getter {
    ($name:ident, $t:ty, $d:expr, |$s:ident| $e:expr) => {
        /// # Safety
        /// `s` is a live set.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name(s: *mut Set) -> $t {
            with_or(s, $d, |$s| $e)
        }
    };
}

macro_rules! setter {
    ($name:ident ( $a:ident : $t:ty ), |$s:ident| $e:expr) => {
        /// # Safety
        /// `s` is a live set.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name(s: *mut Set, $a: $t) {
            with(s, |$s| $e)
        }
    };
}

/// # Safety
/// `s` is a live set.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_set_update(s: *mut Set, dt: f32) {
    update(s, dt);
}

/// # Safety
/// `s` is a live set.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_set_draw_all(s: *mut Set, dt: f32) {
    draw_all(s, dt);
}

/// # Safety
/// `s` is a live set; `d` a `cGuiMessageData`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_set_send_message(s: *mut Set, msg: i32, d: *const Msg) -> bool {
    // SAFETY: caller contract.
    unsafe { d.as_ref() }.is_some_and(|d| send_message(s, msg, d))
}

/// # Safety
/// `s` is a live set.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_set_clear_render_objects(s: *mut Set) {
    with(s, |s| s.list.clear());
}

/// # Safety
/// `s` is a live set; the pointers readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_set_draw_gfx(
    s: *mut Set,
    g: *mut Element,
    pos: *const Vector3f,
    size: *const Vector2f,
    color: *const Color,
    material: i32,
    angle: f32,
    pivot: *const Vector3f,
) {
    // SAFETY: caller contract.
    unsafe {
        if let (Some(&pos), Some(&size), Some(&color)) =
            (pos.as_ref(), size.as_ref(), color.as_ref())
        {
            draw_gfx(
                s,
                g,
                pos,
                size,
                color,
                material,
                angle,
                pivot.as_ref().copied(),
            );
        }
    }
}

/// # Safety
/// `s` is a live set; `table` describes a live font; `text` has `len` units; the pointers readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_set_draw_text(
    s: *mut Set,
    table: *const draw::GlyphTable,
    text: *const u32,
    len: usize,
    size: *const Vector2f,
    pos: *const Vector3f,
    color: *const Color,
    material: i32,
    align: i32,
) {
    // SAFETY: caller contract.
    unsafe {
        if let (Some(table), Some(&size), Some(&pos), Some(&color)) =
            (table.as_ref(), size.as_ref(), pos.as_ref(), color.as_ref())
        {
            draw_text(
                s,
                table,
                std::slice::from_raw_parts(text, len),
                size,
                pos,
                color,
                material,
                align,
            );
        }
    }
}

/// `cGuiSet::AddWidget`.
///
/// # Safety
/// `s` is a live set; the widgets are live C++ widgets.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_set_add_widget(s: *mut Set, w: W, parent: W) {
    let root = with(s, |s| {
        s.widgets.push(w);
        s.root
    });
    gw::attach_child(if parent != 0 { parent } else { root }, w);
    gw::init(w);
}

/// # Safety
/// `s` is a live set; `name` readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_set_widget_from_name(
    s: *mut Set,
    name: *const u8,
    len: usize,
) -> W {
    // SAFETY: caller contract.
    let name = unsafe { std::slice::from_raw_parts(name, len) };
    let widgets: Vec<W> = with(s, |s| {
        s.widgets
            .iter()
            .rev()
            .copied()
            .filter(|w| *w != 0)
            .collect()
    });
    widgets
        .into_iter()
        .find(|&w| gw_kinds::name_eq(w, name))
        .unwrap_or(0)
}

/// # Safety
/// `s` is a live set.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_set_destroy_widget(s: *mut Set, w: W, children: bool) {
    destroy_widget(s, w, children);
}

/// # Safety
/// `s` is a live set.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_set_destroy_all_widgets(s: *mut Set) {
    destroy_all_widgets(s);
}

/// # Safety
/// `s` is a live set.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_set_is_valid_widget(s: *mut Set, w: W) -> bool {
    SetHost(s).is_valid(w)
}

/// # Safety
/// `s` is a live set.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_set_reset_mouse_over(s: *mut Set) {
    let widgets: Vec<W> = with(s, |s| {
        s.widgets
            .iter()
            .rev()
            .copied()
            .filter(|w| *w != 0)
            .collect()
    });
    let mut h = SetHost(s);
    for w in widgets {
        h.set_mouse_is_over(w, false);
    }
}

/// # Safety
/// `s` is a live set.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_set_destroy_popup(s: *mut Set, popup: usize) {
    with(s, |s| s.popups.insert(0, popup));
}

getter!(ohpl_g1_set_popup_is_active, bool, false, |s| s.popup_count
    > 0);
setter!(ohpl_g1_set_add_popup_count(n: i32), |s| s.popup_count += n);
getter!(ohpl_g1_set_popup_z, f32, 0.0, |s| s.last_popup_z);
setter!(ohpl_g1_set_inc_popup_z(_unused: i32), |s| s.last_popup_z = layout::next_popup_z(s.last_popup_z));
getter!(ohpl_g1_set_context_menu_z, f32, 0.0, |s| s.context_menu_z);
setter!(ohpl_g1_set_add_context_menu_z(dz: f32), |s| s.context_menu_z += dz);
getter!(ohpl_g1_set_gui, *mut c_void, ptr::null_mut(), |s| s
    .gui_shell);
getter!(ohpl_g1_set_resources, *mut c_void, ptr::null_mut(), |s| s
    .resources);
getter!(ohpl_g1_set_graphics, *mut c_void, ptr::null_mut(), |s| s
    .graphics);
getter!(ohpl_g1_set_sound, *mut c_void, ptr::null_mut(), |s| s.sound);
getter!(ohpl_g1_set_scene, *mut c_void, ptr::null_mut(), |s| s.scene);
getter!(ohpl_g1_set_skin, *mut Skin, ptr::null_mut(), |s| s.skin);
getter!(ohpl_g1_set_root, W, 0, |s| s.root);
getter!(ohpl_g1_set_list, *mut c_void, ptr::null_mut(), |s| {
    (&raw mut s.list).cast()
});
getter!(ohpl_g1_set_pointer, *mut Element, ptr::null_mut(), |s| s
    .pointer);
setter!(ohpl_g1_set_set_pointer(g: *mut Element), |s| s.pointer = g);
setter!(ohpl_g1_set_set_skin(skin: *mut Skin), |s| set_skin(s, skin));
getter!(ohpl_g1_set_virtual_min_z, f32, 0.0, |s| s.virtual_min_z);
getter!(ohpl_g1_set_virtual_max_z, f32, 0.0, |s| s.virtual_max_z);
getter!(ohpl_g1_set_base_clip, *mut Clip, ptr::null_mut(), |s| {
    &raw mut *s.base_clip
});
getter!(ohpl_g1_set_current_clip, *mut Clip, ptr::null_mut(), |s| s
    .current_clip);
setter!(ohpl_g1_set_set_current_clip(c: *mut Clip), |s| s.current_clip = c);
setter!(ohpl_g1_set_set_focus_draw_callback_obj(o: *mut c_void), |s| s.focus_obj = o);
getter!(ohpl_g1_set_tip_frame, W, 0, |s| s.tip_frame);

/// # Safety
/// `s` is a live set; `cb` null or a `tGuiCallbackFunc`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_set_set_focus_draw_callback(
    s: *mut Set,
    obj: *mut c_void,
    cb: Option<FocusCb>,
) {
    with(s, |s| {
        s.focus_obj = obj;
        s.focus_cb = cb;
    });
}

/// # Safety
/// `s` is a live set; `name` is written through `len`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_set_name(s: *mut Set, len: *mut usize) -> *const u8 {
    // SAFETY: caller contract.
    match (unsafe { st(s) }, unsafe { len.as_mut() }) {
        (Some(s), Some(len)) => {
            *len = s.name.len();
            s.name.as_ptr()
        }
        _ => ptr::null(),
    }
}

/// # Safety
/// `s` is a live set; the pointers readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_set_set_virtual_size(
    s: *mut Set,
    size: *const Vector2f,
    min_z: f32,
    max_z: f32,
    offset: *const Vector2f,
    ignore_scale: bool,
) {
    // SAFETY: caller contract.
    if let (Some(&size), Some(&offset)) = unsafe { (size.as_ref(), offset.as_ref()) } {
        with(s, |s| {
            set_virtual_size(s, size, min_z, max_z, offset, ignore_scale)
        });
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_g1_set_set_global_scale(scale: f32) {
    GLOBAL_SCALE.store(
        (if scale > 0.0 { scale } else { 1.0 }).to_bits(),
        Ordering::Relaxed,
    );
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_g1_set_global_scale() -> f32 {
    global_scale()
}

/// # Safety
/// `s` is a live set; `m` readable (16 floats).
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_set_set_3d_transform(s: *mut Set, m: *const [f32; 16]) {
    // SAFETY: caller contract.
    if let Some(&m) = unsafe { m.as_ref() } {
        with(s, |s| s.p.mtx_3d = m);
    }
}

// Focus, attention, tab order, windows (the `Core` state machine).

macro_rules! core_op {
    ($name:ident ( $($a:ident : $t:ty),* ) => |$c:ident, $h:ident| $body:expr) => {
        /// # Safety
        /// `s` is a live set.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name(s: *mut Set $(, $a: $t)*) {
            if let Some($c) = core(s) {
                let mut host = SetHost(s);
                let $h = &mut host;
                $body
            }
        }
    };
}

core_op!(ohpl_g1_set_set_focused(w: W, check: bool) => |c, h| c.set_focused_widget(h, w, check));
core_op!(ohpl_g1_set_push_focused() => |c, _h| c.push_focused());
core_op!(ohpl_g1_set_pop_focused() => |c, h| c.pop_focused(h));
core_op!(ohpl_g1_set_set_attention(w: W, clear: bool, check: bool) => |c, h| c.set_attention_widget(h, w, clear, check));
core_op!(ohpl_g1_set_push_attention() => |c, _h| c.push_attention());
core_op!(ohpl_g1_set_pop_attention(clear: bool) => |c, h| c.pop_attention(h, clear));
core_op!(ohpl_g1_set_set_default_nav(w: W, check: bool) => |c, h| c.set_default_focus_nav(h, w, check));
core_op!(ohpl_g1_set_push_default_nav() => |c, _h| c.push_default_focus_nav());
core_op!(ohpl_g1_set_pop_default_nav() => |c, h| c.pop_default_focus_nav(h));
core_op!(ohpl_g1_set_add_to_tab_order(w: W) => |c, _h| c.add_to_tab_order(w));
core_op!(ohpl_g1_set_clear_tab_order() => |c, _h| c.clear_tab_order());
core_op!(ohpl_g1_set_add_window(w: W) => |c, _h| c.add_window(w));
core_op!(ohpl_g1_set_remove_window(w: W) => |c, _h| c.remove_window(w));
core_op!(ohpl_g1_set_set_window_on_top(w: W) => |c, h| c.set_window_on_top(h, w));
core_op!(ohpl_g1_set_set_last_window_z(z: f32) => |c, _h| c.set_window_z(z));

/// `cGuiSet::DrawMouse`, registered on the root widget.
///
/// # Safety
/// `obj` is a live set.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_set_draw_mouse(obj: *mut c_void, _w: W, _d: *const Msg) -> bool {
    let s: *mut Set = obj.cast();
    let Some((focus, draw_mouse, pointer, pos, z)) = with(s, |s| {
        Some((
            has_focus(s),
            s.p.draw_mouse,
            s.pointer,
            s.p.state.mouse_pos,
            s.p.mouse_z,
        ))
    }) else {
        return true;
    };
    if focus && draw_mouse && !pointer.is_null() {
        let mut size = Vector2f::default();
        // SAFETY: the pointer gfx is live.
        unsafe { g1_gfx::ohpl_g1_gfx_image_size(pointer, &mut size) };
        let white = Color {
            r: 1.0,
            g: 1.0,
            b: 1.0,
            a: 1.0,
        };
        let pos = Vector3f {
            x: pos[0],
            y: pos[1],
            z,
        };
        trace_and_draw(s, pointer, pos, size, white);
    }
    true
}

/// `cGuiSet::DrawFocus`, registered on the root widget.
///
/// # Safety
/// `obj` is a live set.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_set_draw_focus(obj: *mut c_void, _w: W, d: *const Msg) -> bool {
    let s: *mut Set = obj.cast();
    let Some((has, focused, default_nav, draw_focus, cb, cb_obj, mouse_z)) = with(s, |s| {
        Some((
            has_focus(s),
            s.p.state.focused,
            s.p.state.default_nav,
            s.p.draw_focus,
            s.focus_cb,
            s.focus_obj,
            s.p.mouse_z,
        ))
    }) else {
        return true;
    };
    if !(has
        && focused != 0
        && draw_focus
        && (focused == default_nav || gw::has_focus_navigation(focused)))
    {
        return true;
    }
    if let (false, Some(cb)) = (cb_obj.is_null(), cb) {
        unsafe { cb(cb_obj, focused, d) };
    } else {
        let g = gw::global_position(focused);
        let sz = gw::size(focused);
        let p = [g.x, g.y, g.z];
        let size = [sz.x, sz.y];
        let pos = Vector3f {
            x: p[0],
            y: p[1],
            z: mouse_z - 1.0,
        };
        let c = Color {
            r: 0.8,
            g: 0.8,
            b: 0.8,
            a: 0.2,
        };
        trace_and_draw(
            s,
            g1_gui::ohpl_g1_gfx_rect(),
            pos,
            Vector2f {
                x: size[0],
                y: size[1],
            },
            c,
        );
    }
    true
}

fn has_focus(s: &Set) -> bool {
    // SAFETY: the gui outlives its sets.
    unsafe { g1_gui::ohpl_g1_gui_focused_set(s.gui) == ptr::from_ref(s) as usize }
}

fn trace_and_draw(s: *mut Set, g: *mut Element, pos: Vector3f, size: Vector2f, color: Color) {
    // SAFETY: the trace hook (C++) and the draw both take the live set and element.
    unsafe {
        ohpl_g1_set_trace(s.cast(), g.cast(), &pos, &size, &color, 6);
        draw_gfx(s, g, pos, size, color, 6, 0.0, None);
    }
}

type TraceFn = unsafe extern "C" fn(
    *mut c_void,
    *mut c_void,
    *const Vector3f,
    *const Vector2f,
    *const Color,
    i32,
);

static TRACE: std::sync::atomic::AtomicPtr<c_void> =
    std::sync::atomic::AtomicPtr::new(ptr::null_mut());

/// Registers the C++ `gfpOhplGuiTrace` caller (called once at start-up).
///
/// # Safety
/// `f` is a static function of the `TraceFn` shape.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_set_trace_fn(f: TraceFn) {
    TRACE.store(f as *mut c_void, Ordering::Release);
}

unsafe fn ohpl_g1_set_trace(
    s: *mut c_void,
    g: *mut c_void,
    pos: *const Vector3f,
    size: *const Vector2f,
    color: *const Color,
    material: i32,
) {
    let f = TRACE.load(Ordering::Acquire);
    if !f.is_null() {
        // SAFETY: stored by `ohpl_g1_set_trace_fn`.
        let f: TraceFn = unsafe { std::mem::transmute::<*mut c_void, TraceFn>(f) };
        // SAFETY: caller contract.
        unsafe { f(s, g, pos, size, color, material) };
    }
}

// Shortcuts.

/// # Safety
/// `s` is a live set.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_set_add_global_shortcut(
    s: *mut Set,
    modifiers: i32,
    key: i32,
    w: W,
    msg: i32,
    bypass_visibility: bool,
    bypass_enabled: bool,
) -> *mut Shortcut {
    with_or(s, ptr::null_mut(), |s| {
        let mut c = Box::new(Shortcut {
            enabled: true,
            bypass_visibility,
            bypass_enabled,
            key: [key, 0, modifiers],
            widget: w,
            message: msg,
        });
        let p = &raw mut *c;
        s.shortcuts.push(c);
        p
    })
}

/// # Safety
/// `s` is a live set.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_set_remove_global_shortcut(s: *mut Set, c: *mut Shortcut) {
    with(s, |s| {
        if let Some(i) = s.shortcuts.iter().position(|x| ptr::eq(&raw const **x, c)) {
            s.shortcuts.remove(i);
        }
    });
}

/// # Safety
/// `s` is a live set.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_set_clear_global_shortcuts(s: *mut Set) {
    clear_shortcuts(s);
}

/// # Safety
/// `c` is a live shortcut.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_shortcut_key(c: *mut Shortcut) -> *const [i32; 3] {
    // SAFETY: caller contract.
    unsafe { c.as_ref() }.map_or(ptr::null(), |c| &raw const c.key)
}

/// # Safety
/// `c` is a live shortcut.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_shortcut_enabled(c: *mut Shortcut) -> bool {
    // SAFETY: caller contract.
    unsafe { c.as_ref() }.is_some_and(|c| c.enabled)
}

/// # Safety
/// `c` is a live shortcut.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_shortcut_set_enabled(c: *mut Shortcut, v: bool) {
    // SAFETY: caller contract.
    if let Some(c) = unsafe { c.as_mut() } {
        c.enabled = v;
    }
}

/// `cGuiGlobalShortcut::ProcessKeyPress`; `key` is a `cKeyPress`.
///
/// # Safety
/// `c` is a live shortcut; `key` readable (12 bytes).
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_shortcut_process_key_press(
    c: *mut Shortcut,
    key: *const [i32; 3],
) -> bool {
    // SAFETY: caller contract.
    let (Some(c), Some(k)) = (unsafe { c.as_ref() }, unsafe { key.as_ref() }) else {
        return false;
    };
    c.enabled
        && c.key[2] == k[2]
        && c.key[0] == k[0]
        && exec(c.widget, c.message, c.bypass_visibility, c.bypass_enabled)
}

/// # Safety
/// `c` is a live shortcut.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_shortcut_exec(c: *mut Shortcut) -> bool {
    // SAFETY: caller contract.
    unsafe { c.as_ref() }
        .is_some_and(|c| exec(c.widget, c.message, c.bypass_visibility, c.bypass_enabled))
}

const KEY_NONE: i32 = 0;
const MODIFIER_COUNT: i32 = 4;

/// `ToString` of a shortcut as wide characters.
pub(crate) fn shortcut_text(c: *mut Shortcut) -> Vec<u32> {
    extern "C" fn collect(ctx: *mut c_void, p: *const u8, n: usize) {
        // SAFETY: `ctx` is the `String` below, `p` readable for `n` bytes.
        let (out, b) = unsafe { (&mut *ctx.cast::<String>(), std::slice::from_raw_parts(p, n)) };
        *out = String::from_utf8_lossy(b).into_owned();
    }
    let mut s = String::new();
    // SAFETY: `c` is a live shortcut of the set; the sink fills `s`.
    unsafe { ohpl_g1_shortcut_to_string(c, collect, (&raw mut s).cast()) };
    s.chars().map(u32::from).collect()
}

/// `cGuiGlobalShortcut::ToString`.
///
/// # Safety
/// `c` is a live shortcut; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_shortcut_to_string(
    c: *mut Shortcut,
    sink: Sink,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    let Some(c) = (unsafe { c.as_ref() }) else {
        return;
    };
    let Some(h) = super::gw_host::host() else {
        return;
    };
    if c.widget == 0 {
        return;
    }
    let set = gw::set_of(c.widget);
    // SAFETY: the widget's set is live.
    let input = with_or(set, ptr::null_mut(), |s| unsafe {
        g1_gui::ohpl_g1_gui_input(s.gui)
    });
    unsafe extern "C" fn push(ctx: *mut c_void, p: *const u8, n: usize) {
        // SAFETY: `ctx` is a Vec below; `p` holds `n` bytes.
        if let Some(v) = unsafe { ctx.cast::<Vec<u8>>().as_mut() } {
            v.extend_from_slice(unsafe { std::slice::from_raw_parts(p, n) });
        }
    }
    let name = |modifier: bool, k: i32| -> Vec<u8> {
        let mut out: Vec<u8> = Vec::new();
        // SAFETY: host contract.
        unsafe { (h.key_name)(input, modifier, k, ptr::from_mut(&mut out).cast(), push) };
        out
    };
    let mut text: Vec<u8> = Vec::new();
    for i in 0..MODIFIER_COUNT {
        let m = 1i32 << i;
        if c.key[2] & m != 0 {
            if !text.is_empty() {
                text.push(b'+');
            }
            text.extend(name(true, m));
        }
    }
    if c.key[0] != KEY_NONE {
        if !text.is_empty() {
            text.push(b'+');
        }
        text.extend(name(false, c.key[0]));
    }
    // SAFETY: caller contract.
    unsafe { sink(ctx, text.as_ptr(), text.len()) };
}

/// `cGuiClipRegion::CreateChild`.
///
/// # Safety
/// `c` is a live clip region owned by a set.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_clip_create_child(
    c: *mut Clip,
    pos: *const Vector3f,
    size: *const Vector2f,
) -> *mut Clip {
    // SAFETY: caller contract.
    let (Some(c), Some(&pos), Some(&size)) =
        (unsafe { c.as_mut() }, unsafe { pos.as_ref() }, unsafe {
            size.as_ref()
        })
    else {
        return ptr::null_mut();
    };
    let mut child = new_clip();
    child.rect = layout::child_clip_rect(&c.rect, pos, size);
    let p = &raw mut *child;
    c.children.push(child);
    p
}

/// # Safety
/// `c` is a live clip region.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_clip_rect(c: *const Clip, out: *mut Rect2f) {
    // SAFETY: caller contract.
    if let (Some(c), Some(out)) = unsafe { (c.as_ref(), out.as_mut()) } {
        *out = c.rect;
    }
}

/// `mBaseClipRegion.Clear()` after a render.
///
/// # Safety
/// `s` is a live set.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_set_clear_base_clip(s: *mut Set) {
    with(s, |s| s.base_clip.children.clear());
}

/// Scissor `(pos, size)` of a clip region for the 2D render path.
///
/// # Safety
/// `s` is a live set; `out_*` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_set_scissor(
    s: *mut Set,
    c: *const Clip,
    target: *const Vector2l,
    out_pos: *mut Vector2l,
    out_size: *mut Vector2l,
) {
    // SAFETY: caller contract.
    let (Some(c), Some(&target)) = (unsafe { c.as_ref() }, unsafe { target.as_ref() }) else {
        return;
    };
    let (vs, vo, flip) = with_or(s, (Vector2f::default(), Vector2f::default(), false), |s| {
        (s.p.virtual_size, s.p.virtual_offset, s.p.flip_y)
    });
    let (pos, size) = layout::scissor(&c.rect, target, vs, vo, flip);
    // SAFETY: caller contract.
    unsafe {
        *out_pos = pos;
        *out_size = size;
    }
}

/// A free-standing clip region (the SOMA splash draws with its own).
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_g1_clip_new() -> *mut Clip {
    Box::into_raw(new_clip())
}

/// # Safety
/// `c` is a live clip region.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_clip_set_rect(c: *mut Clip, x: f32, y: f32, w: f32, h: f32) {
    // SAFETY: caller contract.
    if let Some(c) = unsafe { c.as_mut() } {
        c.rect = Rect2f { x, y, w, h };
    }
}

/// # Safety
/// `c` is from `ohpl_g1_clip_new`, freed once.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_clip_free(c: *mut Clip) {
    if !c.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(c) });
    }
}

pub(crate) fn set_draw_mouse(s: *mut Set, v: bool) {
    with(s, |s| s.p.draw_mouse = v);
}

pub(crate) fn set_active(s: *mut Set, v: bool) {
    with(s, |s| s.p.active = v);
}

pub(crate) fn current_clip(s: *mut Set) -> *mut Clip {
    with_or(s, ptr::null_mut(), |s| s.current_clip)
}

pub(crate) fn set_current_clip(s: *mut Set, c: *mut Clip) {
    with(s, |s| s.current_clip = c);
}

pub(crate) fn set_default_nav(s: *mut Set, w: W) {
    if let Some(c) = core(s) {
        c.set_default_focus_nav(&mut SetHost(s), w, false);
    }
}

pub(crate) fn set_focused(s: *mut Set, w: W) {
    if let Some(c) = core(s) {
        c.set_focused_widget(&mut SetHost(s), w, false);
    }
}

/// `cGuiSet::DrawGfx` as the C++ inline did: trace hook, then draw.
#[allow(clippy::too_many_arguments)]
pub(crate) fn draw_gfx_traced(
    s: *mut Set,
    g: *mut Element,
    pos: Vector3f,
    size: Vector2f,
    color: Color,
    material: i32,
    angle: f32,
) {
    // SAFETY: the trace hook and the draw take the live set and element.
    unsafe {
        ohpl_g1_set_trace(s.cast(), g.cast(), &pos, &size, &color, material);
        draw_gfx(s, g, pos, size, color, material, angle, None);
    }
}

pub(crate) fn draw_gfx_default(
    s: *mut Set,
    g: *mut Element,
    pos: Vector3f,
    size: Vector2f,
    color: Color,
) {
    draw_gfx_traced(s, g, pos, size, color, 6, 0.0);
}

/// Virtual size, offset and whether the set is 3D.
pub(crate) fn set_info(s: *mut Set) -> ([f32; 2], [f32; 2], bool) {
    let r = virtual_rect(s);
    ([r[0], r[1]], [r[2], r[3]], with(s, |s| s.p.is_3d))
}

pub(crate) fn set_widgets_need_sorting(s: *mut Set) {
    with(s, |s| s.p.sort_widgets = true);
}

pub(crate) fn set_gui_rust(s: *mut Set) -> *mut Gui {
    with_or(s, ptr::null_mut(), |s| s.gui)
}

pub(crate) fn is_destroying(s: *mut Set) -> bool {
    with(s, |s| s.p.destroying)
}

pub(crate) fn tooltip_widget(s: *mut Set) -> W {
    with(s, |s| s.p.state.tooltip_widget)
}

pub(crate) fn set_tooltip_widget(s: *mut Set, w: W) {
    with(s, |s| s.p.state.tooltip_widget = w);
}

pub(crate) fn focused_widget(s: *mut Set) -> W {
    with(s, |s| s.p.state.focused)
}

pub(crate) fn attention_widget(s: *mut Set) -> W {
    with(s, |s| s.p.state.attention)
}

pub(crate) fn set_draw_offset(s: *mut Set, p: Vector3f) {
    with(s, |s| s.p.draw_offset = p);
}
