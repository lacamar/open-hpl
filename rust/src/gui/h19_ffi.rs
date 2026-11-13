//! C ABI of the `iWidget` base, declared in `gui/RustGuiH19.h`.
//!
//! `vt` is a valid host table and `w` an `iWidget*` as `usize`; `Base` pointers are the widget's
//! shared state. Null tables or widgets make a call a no-op returning the zero value.

use super::h19_widget::{self as hw, Base, CallbackFn, Callbacks, Host, Op};
use super::set::{Msg, W};
use crate::types::{Vector2f, Vector3f};
use std::ffi::c_void;

type Ctx = *mut c_void;

#[repr(C)]
pub struct HostVt {
    ctx: Ctx,
    base: extern "C" fn(Ctx, W) -> *mut Base,
    call: extern "C" fn(Ctx, W, i32, W, *const Msg, f32) -> bool,
    child_begin: extern "C" fn(Ctx, W) -> usize,
    child_end: extern "C" fn(Ctx, W, usize) -> bool,
    child_get: extern "C" fn(Ctx, W, usize) -> W,
    child_next: extern "C" fn(Ctx, W, usize) -> usize,
    child_erase: extern "C" fn(Ctx, W, usize),
    child_push: extern "C" fn(Ctx, W, W),
    needs_sorting: extern "C" fn(Ctx, W),
    virtual_size: extern "C" fn(Ctx, W, *mut f32),
    clip_child: extern "C" fn(Ctx, W, usize, *const f32, *const f32) -> usize,
    set_clip: extern "C" fn(Ctx, W, usize),
}

impl Host for &HostVt {
    fn base(&mut self, w: W) -> *mut Base {
        (self.base)(self.ctx, w)
    }
    fn call(&mut self, w: W, op: Op, other: W, d: &Msg, f: f32) -> bool {
        (self.call)(self.ctx, w, op as i32, other, d, f)
    }
    fn child_begin(&mut self, w: W) -> usize {
        (self.child_begin)(self.ctx, w)
    }
    fn child_end(&mut self, w: W, it: usize) -> bool {
        (self.child_end)(self.ctx, w, it)
    }
    fn child_get(&mut self, w: W, it: usize) -> W {
        (self.child_get)(self.ctx, w, it)
    }
    fn child_next(&mut self, w: W, it: usize) -> usize {
        (self.child_next)(self.ctx, w, it)
    }
    fn child_erase(&mut self, w: W, it: usize) {
        (self.child_erase)(self.ctx, w, it)
    }
    fn child_push(&mut self, w: W, child: W) {
        (self.child_push)(self.ctx, w, child)
    }
    fn needs_sorting(&mut self, w: W) {
        (self.needs_sorting)(self.ctx, w)
    }
    fn virtual_size(&mut self, w: W) -> Vector2f {
        let mut s = [0.0f32; 2];
        (self.virtual_size)(self.ctx, w, s.as_mut_ptr());
        Vector2f { x: s[0], y: s[1] }
    }
    fn clip_child(&mut self, w: W, clip: usize, pos: Vector3f, size: Vector2f) -> usize {
        let p = [pos.x, pos.y, pos.z];
        let s = [size.x, size.y];
        (self.clip_child)(self.ctx, w, clip, p.as_ptr(), s.as_ptr())
    }
    fn set_clip(&mut self, w: W, clip: usize) {
        (self.set_clip)(self.ctx, w, clip)
    }
}

/// # Safety
/// `b` is null or points to writable storage for a `Base`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h19_base_init(b: *mut Base, with_callbacks: bool) {
    // SAFETY: caller contract.
    if let Some(r) = unsafe { b.as_mut() } {
        r.init();
        if with_callbacks {
            r.callbacks = hw::callbacks_new();
        }
    }
}

/// # Safety
/// `b` is null or an initialised `Base` that is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h19_base_free(b: *mut Base) {
    // SAFETY: caller contract.
    if let Some(r) = unsafe { b.as_mut() } {
        let c: *mut Callbacks = std::mem::replace(&mut r.callbacks, std::ptr::null_mut());
        // SAFETY: `c` is null or from `callbacks_new`.
        unsafe { hw::callbacks_free(c) };
    }
}

/// # Safety
/// `vt` is null or valid.
unsafe fn with<R: Default>(vt: *const HostVt, w: W, f: impl FnOnce(&mut &HostVt) -> R) -> R {
    // SAFETY: caller contract.
    match (w, unsafe { vt.as_ref() }) {
        (0, _) | (_, None) => R::default(),
        (_, Some(mut h)) => f(&mut h),
    }
}

/// # Safety
/// `p` is null or valid for reads.
unsafe fn rd<T: Copy + Default>(p: *const T) -> T {
    // SAFETY: caller contract.
    unsafe { p.as_ref() }.copied().unwrap_or_default()
}

/// # Safety
/// `p` is null or valid for reads.
unsafe fn rd_msg(p: *const Msg) -> Msg {
    // SAFETY: caller contract.
    unsafe { p.as_ref() }.copied().unwrap_or(Msg::zero())
}

macro_rules! exported {
    ($name:ident ( $($a:ident : $t:ty),* ) $(-> $r:ty)? => |$h:ident, $w:ident| $body:expr) => {
        /// # Safety
        /// `vt` is valid per the module contract.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name(vt: *const HostVt, $w: W $(, $a: $t)*) $(-> $r)? {
            // SAFETY: caller contract.
            unsafe { with(vt, $w, |$h| $body) }
        }
    };
}

exported!(ohpl_h19_process_message(msg: i32, d: *const Msg, skip_vis: bool, skip_en: bool) -> bool => |h, w| {
    // SAFETY: `d` is a valid `cGuiMessageData`.
    let data = rd_msg(d);
    hw::process_message(h, w, msg, &data, skip_vis, skip_en)
});
exported!(ohpl_h19_add_callback(msg: i32, obj: *mut c_void, f: CallbackFn) => |h, w| hw::add_callback(h, w, msg, obj, f));
exported!(ohpl_h19_update(dt: f32) => |h, w| hw::update(h, w, dt));
exported!(ohpl_h19_draw(dt: f32, clip: usize) => |h, w| hw::draw(h, w, dt, clip));
exported!(ohpl_h19_clips_graphics() -> bool => |h, w| hw::clips_graphics(h, w));
exported!(ohpl_h19_is_connected_to(other: W, start: bool) -> bool => |h, w| hw::is_connected_to(h, w, other, start));
exported!(ohpl_h19_is_enabled() -> bool => |h, w| hw::is_enabled(h, w));
exported!(ohpl_h19_is_visible() -> bool => |h, w| hw::is_visible(h, w));
exported!(ohpl_h19_set_enabled(v: bool) => |h, w| hw::set_enabled(h, w, v));
exported!(ohpl_h19_set_visible(v: bool) => |h, w| hw::set_visible(h, w, v));
exported!(ohpl_h19_point_is_inside(x: f32, y: f32, only_clipped: bool) -> bool => |h, w| hw::point_is_inside(h, w, Vector2f { x, y }, only_clipped));
exported!(ohpl_h19_set_position_updated() => |h, w| hw::set_position_updated(h, w));
exported!(ohpl_h19_refresh_global() => |h, w| {
    hw::global_position(h, w);
});
exported!(ohpl_h19_set_position(p: *const Vector3f) => |h, w| {
    // SAFETY: `p` is valid.
    hw::set_position(h, w, rd(p))
});
exported!(ohpl_h19_set_global_position(p: *const Vector3f) => |h, w| {
    // SAFETY: `p` is valid.
    hw::set_global_position(h, w, rd(p))
});
exported!(ohpl_h19_set_children_offset(p: *const Vector3f) => |h, w| {
    // SAFETY: `p` is valid.
    hw::set_children_offset(h, w, rd(p))
});
exported!(ohpl_h19_set_scroll_amount(p: *const Vector3f) => |h, w| {
    // SAFETY: `p` is valid.
    hw::set_scroll_amount(h, w, rd(p))
});
exported!(ohpl_h19_set_affected_by_scroll(v: bool) => |h, w| hw::set_affected_by_scroll(h, w, v));
exported!(ohpl_h19_center_in_set() => |h, w| hw::center_global_position_in_set(h, w));
exported!(ohpl_h19_set_size(p: *const Vector2f) => |h, w| {
    // SAFETY: `p` is valid.
    hw::set_size(h, w, rd(p))
});
exported!(ohpl_h19_text_changed() => |h, w| hw::text_changed_notify(h, w));
exported!(ohpl_h19_attach_child(child: W) => |h, w| if child != 0 { hw::attach_child(h, w, child) });
exported!(ohpl_h19_remove_child(child: W) => |h, w| hw::remove_child(h, w, child));
exported!(ohpl_h19_set_focus_navigation(dir: i32, to: W) => |h, w| hw::set_focus_navigation(h, w, dir, to));
exported!(ohpl_h19_focus_navigation(dir: i32) -> W => |h, w| hw::focus_navigation(h, w, dir));
exported!(ohpl_h19_has_focus_navigation() -> bool => |h, w| hw::has_focus_navigation(h, w));
exported!(ohpl_h19_focus_message(msg: i32, op: i32, d: *const Msg) -> bool => |h, w| {
    // SAFETY: `d` is valid.
    let data = rd_msg(d);
    match op_from(op) {
        Some(o) => hw::focus_message(h, w, msg, o, &data),
        None => false,
    }
});
exported!(ohpl_h19_forward_to_parent(op: i32, child: W, d: *const Msg) => |h, w| {
    // SAFETY: `d` is valid.
    let data = rd_msg(d);
    if let Some(o) = op_from(op) {
        hw::forward_to_parent(h, w, o, child, &data);
    }
});

fn op_from(op: i32) -> Option<Op> {
    match op {
        8 => Some(Op::ChildGotFocus),
        9 => Some(Op::ChildGotTabFocus),
        10 => Some(Op::ChildLostFocus),
        _ => None,
    }
}
