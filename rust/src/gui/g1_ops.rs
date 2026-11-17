//! Widget and popup operations the Rust-owned gui set performs on the C++ widgets (`iWidget`
//! hierarchy, still C++): one static table registered at start-up by `RustGuiG1.cpp`.

use std::ffi::c_void;
use std::sync::atomic::{AtomicPtr, Ordering};

use super::set::Msg;

pub type W = usize;

#[repr(C)]
pub struct Ops {
    pub is_visible: unsafe extern "C" fn(W) -> bool,
    pub is_enabled: unsafe extern "C" fn(W) -> bool,
    pub is_connected_to: unsafe extern "C" fn(W, W) -> bool,
    pub point_is_inside: unsafe extern "C" fn(W, f32, f32) -> bool,
    pub mouse_is_over: unsafe extern "C" fn(W) -> bool,
    pub set_mouse_is_over: unsafe extern "C" fn(W, bool),
    pub process_message: unsafe extern "C" fn(W, i32, *const Msg, bool, bool) -> bool,
    pub get_focus: unsafe extern "C" fn(W, *const Msg) -> bool,
    pub tooltip_enabled: unsafe extern "C" fn(W) -> bool,
    pub has_focus_nav: unsafe extern "C" fn(W) -> bool,
    pub parent: unsafe extern "C" fn(W) -> W,
    pub widget_set: unsafe extern "C" fn(W) -> usize,
    pub pointer_gfx: unsafe extern "C" fn(W) -> usize,
    pub focus_nav: unsafe extern "C" fn(W, i32) -> W,
    pub global_key_listener: unsafe extern "C" fn(W) -> bool,
    pub global_ui_listener: unsafe extern "C" fn(W) -> bool,
    pub got_tab_focus: unsafe extern "C" fn(W, *const Msg) -> bool,
    pub global_pos: unsafe extern "C" fn(W, *mut f32),
    pub set_global_pos: unsafe extern "C" fn(W, *const f32),
    pub update: unsafe extern "C" fn(W, f32),
    pub draw: unsafe extern "C" fn(W, f32, *mut c_void),
    pub delete: unsafe extern "C" fn(W),
    pub attach_child: unsafe extern "C" fn(W, W),
    pub init: unsafe extern "C" fn(W),
    pub first_child: unsafe extern "C" fn(W) -> W,
    pub clear_children: unsafe extern "C" fn(W),
    pub size: unsafe extern "C" fn(W, *mut f32),
    pub name_eq: unsafe extern "C" fn(W, *const u8, usize) -> bool,
    /// `iWidget(eWidgetType_Root, set, NULL)` with the set's draw callbacks attached.
    pub root_new: unsafe extern "C" fn(*mut c_void) -> W,
    /// Tooltip frame, background frame and label, created as the original did.
    pub tooltip_create: unsafe extern "C" fn(*mut c_void, *mut [W; 3]),
    /// Text length, font height and row count of the tip text under the label's font.
    pub tooltip_measure: unsafe extern "C" fn(W, W, f32, *mut [f32; 2], *mut i32),
    /// Label text and size, frame position and size, background size; frames enabled and visible.
    pub tooltip_apply: unsafe extern "C" fn(W, W, W, W, *const [f32; 7]),
    pub hide: unsafe extern "C" fn(W),
    /// Keyboard names for a shortcut's text: `key` is an `eKey` (modifier when `modifier`).
    pub key_name: unsafe extern "C" fn(*mut c_void, bool, i32, *mut c_void, super::g1_host::Sink),
}

static OPS: AtomicPtr<Ops> = AtomicPtr::new(std::ptr::null_mut());

/// # Safety
/// `ops` is a static table that outlives every call into the gui.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_g1_set_ops(ops: *mut Ops) {
    OPS.store(ops, Ordering::Release);
}

pub fn ops() -> Option<&'static Ops> {
    // SAFETY: `ohpl_g1_set_ops` contract.
    unsafe { OPS.load(Ordering::Acquire).as_ref() }
}
