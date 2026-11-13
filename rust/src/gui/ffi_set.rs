//! C ABI for the `cGuiSet` state machine, declared in `gui/RustGuiH18.h`.
//!
//! `s` is a valid `OhplH18SetState`; `vt` a valid host table whose callbacks may re-enter these
//! functions. Widget handles are `iWidget*` as `usize`, 0 for null.

use super::set::{self, Core, Host, Lists, Msg, SetState, W};
use std::ffi::c_void;

type Ctx = *mut c_void;

#[repr(C)]
pub struct HostVt {
    ctx: Ctx,
    is_visible: extern "C" fn(Ctx, W) -> bool,
    is_enabled: extern "C" fn(Ctx, W) -> bool,
    is_connected_to: extern "C" fn(Ctx, W, W) -> bool,
    point_is_inside: extern "C" fn(Ctx, W, f32, f32) -> bool,
    mouse_is_over: extern "C" fn(Ctx, W) -> bool,
    set_mouse_is_over: extern "C" fn(Ctx, W, bool),
    process_message: extern "C" fn(Ctx, W, i32, *const Msg, bool, bool) -> bool,
    get_focus: extern "C" fn(Ctx, W, *const Msg) -> bool,
    tooltip_enabled: extern "C" fn(Ctx, W) -> bool,
    has_focus_nav: extern "C" fn(Ctx, W) -> bool,
    parent: extern "C" fn(Ctx, W) -> W,
    widget_set: extern "C" fn(Ctx, W) -> usize,
    pointer_gfx: extern "C" fn(Ctx, W) -> usize,
    focus_nav: extern "C" fn(Ctx, W, i32) -> W,
    global_key_listener: extern "C" fn(Ctx, W) -> bool,
    global_ui_listener: extern "C" fn(Ctx, W) -> bool,
    got_tab_focus: extern "C" fn(Ctx, W, *const Msg) -> bool,
    is_valid: extern "C" fn(Ctx, W) -> bool,
    list_begin: extern "C" fn(Ctx) -> usize,
    list_end: extern "C" fn(Ctx, usize) -> bool,
    list_get: extern "C" fn(Ctx, usize) -> W,
    list_next: extern "C" fn(Ctx, usize) -> usize,
    sort_widgets: extern "C" fn(Ctx),
    current_pointer: extern "C" fn(Ctx) -> usize,
    set_current_pointer: extern "C" fn(Ctx, usize),
    skin_normal_pointer: extern "C" fn(Ctx) -> usize,
    exec_shortcut: extern "C" fn(Ctx, *const Msg) -> bool,
    global_pos: extern "C" fn(Ctx, W, *mut f32),
    set_global_pos: extern "C" fn(Ctx, W, *const f32),
}

impl Host for &HostVt {
    fn is_visible(&mut self, w: W) -> bool {
        (self.is_visible)(self.ctx, w)
    }
    fn is_enabled(&mut self, w: W) -> bool {
        (self.is_enabled)(self.ctx, w)
    }
    fn is_connected_to(&mut self, w: W, other: W) -> bool {
        (self.is_connected_to)(self.ctx, w, other)
    }
    fn point_is_inside(&mut self, w: W, p: [f32; 2]) -> bool {
        (self.point_is_inside)(self.ctx, w, p[0], p[1])
    }
    fn mouse_is_over(&mut self, w: W) -> bool {
        (self.mouse_is_over)(self.ctx, w)
    }
    fn set_mouse_is_over(&mut self, w: W, v: bool) {
        (self.set_mouse_is_over)(self.ctx, w, v)
    }
    fn process_message(&mut self, w: W, msg: i32, d: &Msg, skip_vis: bool, skip_en: bool) -> bool {
        (self.process_message)(self.ctx, w, msg, d, skip_vis, skip_en)
    }
    fn get_focus(&mut self, w: W, d: &Msg) -> bool {
        (self.get_focus)(self.ctx, w, d)
    }
    fn tooltip_enabled(&mut self, w: W) -> bool {
        (self.tooltip_enabled)(self.ctx, w)
    }
    fn has_focus_nav(&mut self, w: W) -> bool {
        (self.has_focus_nav)(self.ctx, w)
    }
    fn parent(&mut self, w: W) -> W {
        (self.parent)(self.ctx, w)
    }
    fn widget_set(&mut self, w: W) -> usize {
        (self.widget_set)(self.ctx, w)
    }
    fn pointer_gfx(&mut self, w: W) -> usize {
        (self.pointer_gfx)(self.ctx, w)
    }
    fn focus_nav(&mut self, w: W, dir: i32) -> W {
        (self.focus_nav)(self.ctx, w, dir)
    }
    fn global_key_listener(&mut self, w: W) -> bool {
        (self.global_key_listener)(self.ctx, w)
    }
    fn global_ui_listener(&mut self, w: W) -> bool {
        (self.global_ui_listener)(self.ctx, w)
    }
    fn got_tab_focus(&mut self, w: W, d: &Msg) -> bool {
        (self.got_tab_focus)(self.ctx, w, d)
    }
    fn is_valid(&mut self, w: W) -> bool {
        (self.is_valid)(self.ctx, w)
    }
    fn list_begin(&mut self) -> usize {
        (self.list_begin)(self.ctx)
    }
    fn list_end(&mut self, it: usize) -> bool {
        (self.list_end)(self.ctx, it)
    }
    fn list_get(&mut self, it: usize) -> W {
        (self.list_get)(self.ctx, it)
    }
    fn list_next(&mut self, it: usize) -> usize {
        (self.list_next)(self.ctx, it)
    }
    fn sort_widgets(&mut self) {
        (self.sort_widgets)(self.ctx)
    }
    fn current_pointer(&mut self) -> usize {
        (self.current_pointer)(self.ctx)
    }
    fn set_current_pointer(&mut self, g: usize) {
        (self.set_current_pointer)(self.ctx, g)
    }
    fn skin_normal_pointer(&mut self) -> usize {
        (self.skin_normal_pointer)(self.ctx)
    }
    fn exec_shortcut(&mut self, d: &Msg) -> bool {
        (self.exec_shortcut)(self.ctx, d)
    }
    fn global_pos(&mut self, w: W) -> [f32; 3] {
        let mut p = [0.0; 3];
        (self.global_pos)(self.ctx, w, p.as_mut_ptr());
        p
    }
    fn set_global_pos(&mut self, w: W, p: [f32; 3]) {
        (self.set_global_pos)(self.ctx, w, p.as_ptr())
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h18_lists_new() -> *mut Lists {
    set::lists_new()
}

/// # Safety
/// `l` is null or from `ohpl_h18_lists_new`, and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h18_lists_free(l: *mut Lists) {
    // SAFETY: caller contract.
    unsafe { set::lists_free(l) }
}

/// Runs `f` against the state and host; null pointers make the call return `R::default()`.
///
/// # Safety
/// `s` and `vt` are null or valid.
unsafe fn with<R: Default>(
    s: *mut SetState,
    vt: *const HostVt,
    f: impl FnOnce(Core, &mut &HostVt) -> R,
) -> R {
    // SAFETY: caller contract.
    match (s.is_null(), unsafe { vt.as_ref() }) {
        (false, Some(mut h)) => f(unsafe { Core::new(s) }, &mut h),
        _ => R::default(),
    }
}

/// # Safety
/// `s` is null or valid.
unsafe fn state(s: *mut SetState) -> Option<Core> {
    // SAFETY: caller contract.
    (!s.is_null()).then(|| unsafe { Core::new(s) })
}

macro_rules! exported {
    ($name:ident ( $($a:ident : $t:ty),* ) $(-> $r:ty)? => |$c:ident, $h:ident| $body:expr) => {
        /// # Safety
        /// `s` and `vt` are valid per the module contract.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name(s: *mut SetState, vt: *const HostVt $(, $a: $t)*) $(-> $r)? {
            // SAFETY: caller contract.
            unsafe { with(s, vt, |$c, $h| $body) }
        }
    };
}

exported!(ohpl_h18_set_focused(w: W, check: bool) => |c, h| c.set_focused_widget(h, w, check));
exported!(ohpl_h18_pop_focused() => |c, h| c.pop_focused(h));
exported!(ohpl_h18_set_attention(w: W, clear: bool, check: bool) => |c, h| c.set_attention_widget(h, w, clear, check));
exported!(ohpl_h18_pop_attention(clear: bool) => |c, h| c.pop_attention(h, clear));
exported!(ohpl_h18_set_default_nav(w: W, check: bool) => |c, h| c.set_default_focus_nav(h, w, check));
exported!(ohpl_h18_pop_default_nav() => |c, h| c.pop_default_focus_nav(h));
exported!(ohpl_h18_set_window_on_top(w: W) => |c, h| c.set_window_on_top(h, w));
exported!(ohpl_h18_send_message(msg: i32, d: *const Msg) -> bool => |c, h| {
    // `d` is a valid `cGuiMessageData`.
    d.as_ref().is_some_and(|d| c.send_message(h, msg, d))
});

macro_rules! plain {
    ($name:ident ( $($a:ident : $t:ty),* ) => |$c:ident| $body:expr) => {
        /// # Safety
        /// `s` is valid per the module contract.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name(s: *mut SetState $(, $a: $t)*) {
            // SAFETY: caller contract.
            if let Some($c) = unsafe { state(s) } {
                $body
            }
        }
    };
}

plain!(ohpl_h18_push_focused() => |c| c.push_focused());
plain!(ohpl_h18_push_attention() => |c| c.push_attention());
plain!(ohpl_h18_push_default_nav() => |c| c.push_default_focus_nav());
plain!(ohpl_h18_add_to_tab_order(w: W) => |c| c.add_to_tab_order(w));
plain!(ohpl_h18_clear_tab_order() => |c| c.clear_tab_order());
plain!(ohpl_h18_add_window(w: W) => |c| c.add_window(w));
plain!(ohpl_h18_remove_window(w: W) => |c| c.remove_window(w));
plain!(ohpl_h18_destroy_prelude(w: W) => |c| c.destroy_widget_prelude(w));
