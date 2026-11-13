//! `iWidget` base class: position chain, hierarchy, enabled/visible chains, message dispatch with
//! user callbacks, draw traversal and focus propagation. The base state lives in [`Base`], shared
//! with the C++ widget (`gui/RustGuiH19.h`); the widget list, virtuals and the owning set are
//! reached through [`Host`]. Subclass virtuals and children iteration may re-enter these
//! functions, so state is only touched through short raw-pointer scopes.

use super::set::{MSG_SIZE, Msg, W};
use super::widget::{self, ParentPos};
use crate::math::sub3;
use crate::types::{Vector2f, Vector3f};
use std::ffi::c_void;

pub const MSG_GOT_FOCUS: i32 = 6;
pub const MSG_GOT_TAB_FOCUS: i32 = 7;
pub const MSG_LOST_FOCUS: i32 = 8;
pub const MSG_ON_DRAW: i32 = 9;
pub const MSG_ON_UPDATE: i32 = 10;
pub const MSG_TEXT_CHANGE: i32 = 12;
const MSG_COUNT: usize = 32;

const OFF_VAL: usize = 48;
const OFF_DATA: usize = 56;
const OFF_MSG: usize = 64;

/// Virtual and hook calls the base makes on a C++ widget.
#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Op {
    /// `OnMessage(msg, data)`
    OnMessage = 0,
    /// The per-message virtual (`OnMouseMove` ...), message number in `other`.
    Virtual = 1,
    ChangePosition = 2,
    ChangeSize = 3,
    ChangeText = 4,
    ChildUpdate = 5,
    OnAttachChild = 6,
    /// Virtual `RemoveChild(child)` itself.
    RemoveChild = 7,
    ChildGotFocus = 8,
    ChildGotTabFocus = 9,
    ChildLostFocus = 10,
    Update = 11,
    /// `OnDraw(dt, clip)`, clip in `other`.
    Draw = 12,
    DrawAfterClip = 13,
    /// Virtual `PointIsInside(data.pos, f != 0)`.
    PointIsInside = 14,
    OnRemoveChild = 15,
}

pub type CallbackFn = extern "C" fn(*mut c_void, W, *const Msg) -> bool;

#[derive(Clone, Copy)]
struct Callback {
    obj: *mut c_void,
    func: CallbackFn,
}

/// Per-message callback lists in registration order.
pub struct Callbacks(Vec<Vec<Callback>>);

pub fn callbacks_new() -> *mut Callbacks {
    Box::into_raw(Box::new(Callbacks(vec![Vec::new(); MSG_COUNT])))
}

/// # Safety
/// `c` came from [`callbacks_new`] and is not used afterwards.
pub unsafe fn callbacks_free(c: *mut Callbacks) {
    if !c.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(c) });
    }
}

/// Plain fields shared with the C++ widget, which reads and writes them directly.
#[repr(C)]
pub struct Base {
    pub pos: Vector3f,
    pub global: Vector3f,
    pub children_offset: Vector3f,
    pub scroll: Vector3f,
    pub size: Vector2f,
    pub parent: W,
    pub focus_nav: [W; 4],
    pub callbacks: *mut Callbacks,
    pub position_count: i32,
    pub affected_by_scroll: bool,
    pub enabled: bool,
    pub visible: bool,
    pub mouse_over: bool,
    pub clips: bool,
    pub connected_to_children: bool,
    pub callbacks_disabled: bool,
    pub position_updated: bool,
    pub text_changed: bool,
}

const _: () = assert!(std::mem::size_of::<Base>() == 120);

impl Base {
    pub fn init(&mut self) {
        let zero3 = Vector3f {
            x: 0.0,
            y: 0.0,
            z: 0.0,
        };
        self.pos = zero3;
        self.global = zero3;
        self.children_offset = zero3;
        self.scroll = zero3;
        self.size = Vector2f { x: 0.0, y: 0.0 };
        self.parent = 0;
        self.focus_nav = [0; 4];
        self.callbacks = std::ptr::null_mut();
        self.position_count = 0;
        self.affected_by_scroll = true;
        self.enabled = true;
        self.visible = true;
        self.mouse_over = false;
        self.clips = false;
        self.connected_to_children = true;
        self.callbacks_disabled = false;
        self.position_updated = true;
        self.text_changed = false;
    }
}

pub trait Host {
    fn base(&mut self, w: W) -> *mut Base;
    fn call(&mut self, w: W, op: Op, other: W, d: &Msg, f: f32) -> bool;
    fn child_begin(&mut self, w: W) -> usize;
    fn child_end(&mut self, w: W, it: usize) -> bool;
    fn child_get(&mut self, w: W, it: usize) -> W;
    fn child_next(&mut self, w: W, it: usize) -> usize;
    fn child_erase(&mut self, w: W, it: usize);
    fn child_push(&mut self, w: W, child: W);
    fn needs_sorting(&mut self, w: W);
    fn virtual_size(&mut self, w: W) -> Vector2f;
    fn clip_child(&mut self, w: W, clip: usize, pos: Vector3f, size: Vector2f) -> usize;
    fn set_clip(&mut self, w: W, clip: usize);
}

/// Message payload fields the base writes (`mMessage`, `mfVal`, `mpData`).
fn put(d: &mut Msg, at: usize, b: &[u8]) {
    if let Some(s) = d.0.get_mut(at..at + b.len()) {
        s.copy_from_slice(b);
    }
}

fn msg_with(msg: i32) -> Msg {
    let mut d = Msg::zero();
    put(&mut d, OFF_MSG, &msg.to_ne_bytes());
    d
}

macro_rules! field {
    ($get:ident, $set:ident, $f:ident, $t:ty) => {
        #[allow(dead_code)]
        fn $get(b: *mut Base) -> $t {
            // SAFETY: `base()` returns a live widget's state; reads copy out.
            unsafe { (*b).$f }
        }
        #[allow(dead_code)]
        fn $set(b: *mut Base, v: $t) {
            // SAFETY: as above; the write ends before any host call.
            unsafe { (*b).$f = v }
        }
    };
}

field!(pos, set_pos, pos, Vector3f);
field!(global, set_global, global, Vector3f);
field!(
    children_offset,
    set_children_offset_f,
    children_offset,
    Vector3f
);
field!(scroll, set_scroll_f, scroll, Vector3f);
field!(size, set_size_f, size, Vector2f);
field!(parent, set_parent, parent, W);
field!(position_count, set_position_count, position_count, i32);
field!(affected, set_affected, affected_by_scroll, bool);
field!(enabled, set_enabled_f, enabled, bool);
field!(visible, set_visible_f, visible, bool);
field!(clips, _set_clips, clips, bool);
field!(
    conn_children,
    _set_conn_children,
    connected_to_children,
    bool
);
field!(cb_disabled, _set_cb_disabled, callbacks_disabled, bool);
field!(pos_updated, set_pos_updated, position_updated, bool);
field!(text_changed, set_text_changed, text_changed, bool);

fn same3(a: Vector3f, b: Vector3f) -> bool {
    a.x == b.x && a.y == b.y && a.z == b.z
}

fn base_of(h: &mut dyn Host, w: W) -> *mut Base {
    if w == 0 {
        std::ptr::null_mut()
    } else {
        h.base(w)
    }
}

pub fn is_enabled(h: &mut dyn Host, w: W) -> bool {
    let mut cur = w;
    let mut ok = true;
    while cur != 0 {
        let b = h.base(cur);
        ok &= enabled(b);
        cur = parent(b);
    }
    ok
}

pub fn is_visible(h: &mut dyn Host, w: W) -> bool {
    let mut cur = w;
    let mut ok = true;
    while cur != 0 {
        let b = h.base(cur);
        ok &= visible(b);
        cur = parent(b);
    }
    ok
}

pub fn set_enabled(h: &mut dyn Host, w: W, v: bool) {
    let b = h.base(w);
    if enabled(b) != v {
        set_enabled_f(b, v);
    }
}

pub fn set_visible(h: &mut dyn Host, w: W, v: bool) {
    let b = h.base(w);
    if visible(b) == v {
        return;
    }
    set_visible_f(b, v);
    notify_parent(h, w);
}

fn notify_parent(h: &mut dyn Host, w: W) {
    let p = parent(h.base(w));
    if p != 0 {
        h.call(p, Op::ChildUpdate, w, &Msg::zero(), 0.0);
    }
}

fn virtual_for(msg: i32) -> bool {
    matches!(msg, 0..=8 | 14 | 17..=23)
}

pub fn process_message(
    h: &mut dyn Host,
    w: W,
    msg: i32,
    data: &Msg,
    skip_vis: bool,
    skip_enabled: bool,
) -> bool {
    let b = h.base(w);
    if cb_disabled(b) || (!skip_vis && !is_visible(h, w)) || (!skip_enabled && !is_enabled(h, w)) {
        return false;
    }
    let mut t = *data;
    put(&mut t, OFF_MSG, &msg.to_ne_bytes());
    let mut ret = h.call(w, Op::OnMessage, msg as W, &t, 0.0);
    if !ret && virtual_for(msg) {
        ret = h.call(w, Op::Virtual, msg as W, &t, 0.0);
    }
    if process_callbacks(h, w, msg, &t) {
        ret = true;
    }
    ret
}

fn callback_at(b: *mut Base, msg: usize, i: usize) -> Option<Callback> {
    // SAFETY: `callbacks` is null or the live list owned by this widget; the borrow ends here.
    let c = unsafe { (*b).callbacks.as_ref() }?;
    c.0.get(msg)?.get(i).copied()
}

fn process_callbacks(h: &mut dyn Host, w: W, msg: i32, d: &Msg) -> bool {
    let b = h.base(w);
    let Ok(m) = usize::try_from(msg) else {
        return false;
    };
    let mut ret = false;
    let mut i = 0;
    while let Some(cb) = callback_at(b, m, i) {
        if (cb.func)(cb.obj, w, d) {
            ret = true;
        }
        i += 1;
    }
    ret
}

/// # Safety
/// `h.base(w)` is a live widget state, as for [`add_callback_to`].
pub unsafe fn add_callback(h: &mut dyn Host, w: W, msg: i32, obj: *mut c_void, func: CallbackFn) {
    let b = h.base(w);
    // SAFETY: caller contract.
    unsafe { add_callback_to(b, msg, obj, func) };
}

/// # Safety
/// `b` points to a live widget state whose `callbacks` is null or from [`callbacks_new`].
pub unsafe fn add_callback_to(b: *mut Base, msg: i32, obj: *mut c_void, func: CallbackFn) {
    let Ok(m) = usize::try_from(msg) else {
        return;
    };
    // SAFETY: caller contract; no host call while the borrow lives.
    if let Some(c) = unsafe { (*b).callbacks.as_mut() }
        && let Some(l) = c.0.get_mut(m)
    {
        l.push(Callback { obj, func });
    }
}

pub fn update(h: &mut dyn Host, w: W, dt: f32) {
    let mut d = Msg::zero();
    put(&mut d, OFF_VAL, &dt.to_ne_bytes());
    process_message(h, w, MSG_ON_UPDATE, &d, false, false);
    h.call(w, Op::Update, 0, &Msg::zero(), dt);
}

pub fn draw(h: &mut dyn Host, w: W, dt: f32, clip: usize) {
    let b = h.base(w);
    if !visible(b) {
        return;
    }
    h.call(w, Op::Draw, clip, &Msg::zero(), dt);
    let mut child_region = clip;
    if clips(b) {
        let g = global_position(h, w);
        let s = size(b);
        child_region = h.clip_child(w, clip, g, s);
        h.set_clip(w, child_region);
    }
    h.call(w, Op::DrawAfterClip, clip, &Msg::zero(), dt);
    let mut d = Msg::zero();
    put(&mut d, OFF_VAL, &dt.to_ne_bytes());
    put(&mut d, OFF_DATA, &clip.to_ne_bytes());
    process_message(h, w, MSG_ON_DRAW, &d, false, false);
    let mut it = h.child_begin(w);
    while !h.child_end(w, it) {
        let c = h.child_get(w, it);
        draw(h, c, dt, child_region);
        it = h.child_next(w, it);
    }
    if clips(b) {
        h.set_clip(w, clip);
    }
}

pub fn clips_graphics(h: &mut dyn Host, w: W) -> bool {
    let mut cur = w;
    while cur != 0 {
        let b = h.base(cur);
        if clips(b) {
            return true;
        }
        cur = parent(b);
    }
    false
}

pub fn is_connected_to(h: &mut dyn Host, w: W, other: W, is_start: bool) -> bool {
    let mut cur = w;
    let mut start = is_start;
    loop {
        let b = h.base(cur);
        if !start && !conn_children(b) {
            return false;
        }
        if other == 0 {
            return false;
        }
        if other == cur {
            return true;
        }
        cur = parent(b);
        if cur == 0 {
            return false;
        }
        start = false;
    }
}

fn check_point_inside_clipping_parent(h: &mut dyn Host, w: W, p: Vector2f) -> bool {
    let par = parent(h.base(w));
    if par != 0 && clips_graphics(h, par) {
        let mut d = Msg::zero();
        d.set_pos([p.x, p.y]);
        if !h.call(par, Op::PointIsInside, 1, &d, 1.0) {
            return false;
        }
    }
    true
}

/// Base `PointIsInside`.
pub fn point_is_inside(h: &mut dyn Host, w: W, p: Vector2f, only_clipped: bool) -> bool {
    let parent_ok = check_point_inside_clipping_parent(h, w, p);
    let stage = widget::point_stage(parent_ok, only_clipped, clips(h.base(w)));
    if stage != widget::PointStage::CheckBox {
        return stage == widget::PointStage::Inside;
    }
    let g = global_position(h, w);
    widget::point_in_box(p, g, size(h.base(w)))
}

pub fn set_position_updated(h: &mut dyn Host, w: W) {
    let b = h.base(w);
    set_pos_updated(b, true);
    set_position_count(b, position_count(b).wrapping_add(1));
    h.call(w, Op::ChangePosition, 0, &Msg::zero(), 0.0);
    let mut it = h.child_begin(w);
    while !h.child_end(w, it) {
        let c = h.child_get(w, it);
        set_position_updated(h, c);
        it = h.child_next(w, it);
    }
}

fn parent_pos(h: &mut dyn Host, par: W) -> ParentPos {
    let g = global_position(h, par);
    let b = h.base(par);
    ParentPos {
        global: g,
        children_offset: children_offset(b),
        scroll: scroll(b),
    }
}

/// `GetGlobalPosition`: refreshes the cached global position; the caller returns `global` for a
/// widget with a parent and `pos` for a root.
pub fn global_position(h: &mut dyn Host, w: W) -> Vector3f {
    let b = h.base(w);
    let par = parent(b);
    if par == 0 {
        return pos(b);
    }
    if pos_updated(b) {
        set_pos_updated(b, false);
        let pp = parent_pos(h, par);
        let g = widget::global_position(&pp, affected(b), pos(b));
        set_global(b, g);
    }
    global(b)
}

pub fn set_position(h: &mut dyn Host, w: W, p: Vector3f) {
    let b = h.base(w);
    if pos(b).z != p.z {
        h.needs_sorting(w);
    }
    set_pos(b, p);
    notify_parent(h, w);
    set_position_updated(h, w);
}

pub fn set_global_position(h: &mut dyn Host, w: W, p: Vector3f) {
    let par = parent(h.base(w));
    let local = if par != 0 {
        let pp = parent_pos(h, par);
        widget::global_to_local(p, Some(&pp))
    } else {
        widget::global_to_local(p, None)
    };
    set_position(h, w, local);
}

pub fn set_children_offset(h: &mut dyn Host, w: W, v: Vector3f) {
    let b = h.base(w);
    if same3(children_offset(b), v) {
        return;
    }
    set_children_offset_f(b, v);
    set_position_updated(h, w);
}

pub fn set_affected_by_scroll(h: &mut dyn Host, w: W, v: bool) {
    let b = h.base(w);
    if affected(b) == v {
        return;
    }
    set_affected(b, v);
    set_position_updated(h, w);
}

pub fn set_scroll_amount(h: &mut dyn Host, w: W, v: Vector3f) {
    let b = h.base(w);
    if same3(scroll(b), v) {
        return;
    }
    set_scroll_f(b, v);
    set_position_updated(h, w);
}

pub fn center_global_position_in_set(h: &mut dyn Host, w: W) {
    let set = h.virtual_size(w);
    let z = global_position(h, w).z;
    let s = size(h.base(w));
    set_global_position(h, w, widget::center_in_set(set, s, z));
}

pub fn set_size(h: &mut dyn Host, w: W, s: Vector2f) {
    let b = h.base(w);
    set_size_f(b, s);
    h.call(w, Op::ChangeSize, 0, &Msg::zero(), 0.0);
    notify_parent(h, w);
}

/// After `msText` was replaced with a different value.
pub fn text_changed_notify(h: &mut dyn Host, w: W) {
    let b = h.base(w);
    set_text_changed(b, true);
    h.call(w, Op::ChangeText, 0, &Msg::zero(), 0.0);
    process_message(h, w, MSG_TEXT_CHANGE, &Msg::zero(), false, false);
    notify_parent(h, w);
    set_text_changed(b, false);
}

pub fn attach_child(h: &mut dyn Host, w: W, child: W) {
    let cb = h.base(child);
    let old = parent(cb);
    if old != 0 {
        h.call(old, Op::RemoveChild, child, &Msg::zero(), 0.0);
        let g = global_position(h, w);
        let local = sub3(pos(cb), g);
        set_position(h, child, local);
    }
    set_parent(cb, w);
    set_position_updated(h, child);
    h.child_push(w, child);
    h.call(w, Op::ChildUpdate, child, &Msg::zero(), 0.0);
    h.call(w, Op::OnAttachChild, child, &Msg::zero(), 0.0);
}

pub fn remove_child(h: &mut dyn Host, w: W, child: W) {
    let mut it = h.child_begin(w);
    while !h.child_end(w, it) {
        if h.child_get(w, it) == child {
            h.child_erase(w, it);
            let cb = h.base(child);
            set_parent(cb, 0);
            set_position_updated(h, child);
            let g = global_position(h, w);
            let p = pos(cb);
            set_position(h, child, widget::add3(p, g));
            h.call(w, Op::OnRemoveChild, child, &Msg::zero(), 0.0);
            break;
        }
        it = h.child_next(w, it);
    }
}

pub fn set_focus_navigation(h: &mut dyn Host, w: W, dir: i32, to: W) {
    let slot = widget::ui_arrow_slot(dir);
    let Ok(s) = usize::try_from(slot) else {
        return;
    };
    let b = h.base(w);
    // SAFETY: `b` is live and the write is a single word.
    if let Some(n) = unsafe { (*b).focus_nav.get_mut(s) } {
        *n = to;
    }
}

pub fn focus_navigation(h: &mut dyn Host, w: W, dir: i32) -> W {
    let slot = widget::ui_arrow_slot(dir);
    let Ok(s) = usize::try_from(slot) else {
        return 0;
    };
    let b = h.base(w);
    // SAFETY: as above.
    unsafe { (*b).focus_nav.get(s).copied().unwrap_or(0) }
}

pub fn has_focus_navigation(h: &mut dyn Host, w: W) -> bool {
    let b = h.base(w);
    // SAFETY: as above.
    unsafe { (*b).focus_nav.iter().any(|n| *n != 0) }
}

/// `GetFocus` / `GetTabFocus` / `LoseFocus`: `msg` is the focus message, `op` the matching
/// child-notification hook.
pub fn focus_message(h: &mut dyn Host, w: W, msg: i32, op: Op, d: &Msg) -> bool {
    let ret = process_message(h, w, msg, d, false, false);
    if ret {
        h.call(w, op, w, d, 0.0);
    }
    ret
}

/// Base `OnChildGotFocus` and its tab/lost variants: forward to the parent.
pub fn forward_to_parent(h: &mut dyn Host, w: W, op: Op, child: W, d: &Msg) {
    let p = parent(h.base(w));
    if p != 0 {
        h.call(p, op, child, d, 0.0);
    }
}

pub fn parent_of(h: &mut dyn Host, w: W) -> W {
    let b = base_of(h, w);
    if b.is_null() { 0 } else { parent(b) }
}

pub fn blank_message(msg: i32) -> Msg {
    msg_with(msg)
}

const _: () = assert!(MSG_SIZE == 72);
