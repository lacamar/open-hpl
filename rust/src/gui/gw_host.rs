//! Engine services the Rust-owned widgets reach: C++ shells for widgets created from Rust, the
//! virtuals of widgets still written in C++, and fonts. One static table registered at start-up by
//! `RustGuiG3.cpp`.

use std::ffi::c_void;
use std::sync::atomic::{AtomicPtr, Ordering};

use super::set::W;
use crate::types::{Color, Vector2f, Vector3f};

pub const SHELL_PLAIN: i32 = 0;
pub const SHELL_FRAME: i32 = 1;
pub const SHELL_LABEL: i32 = 2;
pub const SHELL_BUTTON: i32 = 3;
pub const SHELL_SLIDER: i32 = 4;
pub const SHELL_WINDOW: i32 = 5;
pub const SHELL_CHECK: i32 = 6;
pub const SHELL_GROUP: i32 = 7;
pub const SHELL_IMAGE: i32 = 8;
pub const SHELL_DUMMY: i32 = 9;
pub const SHELL_MENUITEM: i32 = 10;
pub const SHELL_CTXMENU: i32 = 11;
pub const SHELL_MAINMENU: i32 = 12;

pub type RowSink = unsafe extern "C" fn(*mut c_void, *const u32, usize);

#[repr(C)]
pub struct GwHost {
    /// `new` of the C++ shell for a widget kind; its constructor calls `ohpl_gw_new`.
    pub shell_new: unsafe extern "C" fn(i32, *mut c_void, *mut c_void, i32, usize) -> W,
    pub shell_delete: unsafe extern "C" fn(W),
    pub draw_font: unsafe extern "C" fn(
        *mut c_void,
        usize,
        *const u32,
        usize,
        *const Vector2f,
        *const Vector3f,
        *const Color,
        i32,
    ),
    pub font_length: unsafe extern "C" fn(usize, *const Vector2f, *const u32, usize) -> f32,
    /// Word-wrap rows, each delivered to the sink as wide characters.
    pub font_rows: unsafe extern "C" fn(
        usize,
        f32,
        f32,
        *const Vector2f,
        *const u32,
        usize,
        RowSink,
        *mut c_void,
    ),
    /// Keyboard names for a shortcut's text: `key` is an `eKey` (modifier when `modifier`).
    pub key_name: unsafe extern "C" fn(*mut c_void, bool, i32, *mut c_void, super::g1_host::Sink),
}

static HOST: AtomicPtr<GwHost> = AtomicPtr::new(std::ptr::null_mut());

/// # Safety
/// `h` is a static table that outlives every call into the widgets.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gw_set_host(h: *mut GwHost) {
    HOST.store(h, Ordering::Release);
}

pub fn host() -> Option<&'static GwHost> {
    // SAFETY: `ohpl_gw_set_host` contract.
    unsafe { HOST.load(Ordering::Acquire).as_ref() }
}
