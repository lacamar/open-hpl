//! Safe wrappers over the C++ host ops of `amnesia/src/game/LuxMenuHost.cpp`. A `W` is an opaque
//! pointer to a C++ widget, GUI set, popup, or story; the host ignores null and the
//! windows rebuild every widget in `create_gui`, as the C++ windows did.

use std::ffi::c_void;
use std::ptr::null_mut;

#[repr(transparent)]
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub struct W(*mut c_void);

impl Default for W {
    fn default() -> Self {
        W::NULL
    }
}

impl W {
    pub const NULL: W = W(null_mut());

    pub fn is_null(self) -> bool {
        self.0.is_null()
    }

    pub fn from_addr(a: usize) -> W {
        W(a as *mut c_void)
    }

    pub fn addr(self) -> usize {
        self.0 as usize
    }
}

#[repr(C)]
pub(super) struct Str {
    pub p: *const u32,
    pub n: usize,
}

pub(super) type Sink = unsafe extern "C" fn(*mut c_void, *const u32, usize);

#[cfg(not(test))]
mod ffi {
    use super::{Sink, Str, W};
    use std::ffi::c_void;
    use std::ptr::null_mut;
    use std::sync::atomic::{AtomicPtr, Ordering};

    macro_rules! table {
        ($($name:ident($($a:ident: $t:ty),* $(,)?) $(-> $r:ty)?;)*) => {
            /// Mirror of `OhplA9Host`: the field order is the C++ initialiser order.
            #[repr(C)]
            pub struct Table {
                $(pub $name: unsafe extern "C" fn($($t),*) $(-> $r)?,)*
            }

            static TABLE: AtomicPtr<Table> = AtomicPtr::new(null_mut());

            #[unsafe(no_mangle)]
            pub extern "C" fn ohpl_a9_set_host(t: *const Table) {
                TABLE.store(t.cast_mut(), Ordering::Release);
            }

            $(
                #[allow(clippy::too_many_arguments)]
                pub(super) unsafe fn $name($($a: $t),*) $(-> $r)? {
                    // SAFETY: the host registers a table that lives for the whole process.
                    match unsafe { TABLE.load(Ordering::Acquire).as_ref() } {
                        // SAFETY: the table entries are the host's functions of this signature.
                        Some(t) => unsafe { (t.$name)($($a),*) },
                        None => Default::default(),
                    }
                }
            )*
        };
    }

    table! {

        ohpl_a9h_new(
            set: W,
            kind: i32,
            parent: W,
            pos: *const f32,
            size: *const f32,
            flags: i32,
            t: *const u32,
            n: usize,
        ) -> W;
        ohpl_a9h_popup(
            set: W,
            s: *const Str,
            obj: usize,
            kill_escape: i32,
            copy_focus: bool,
        ) -> W;
        ohpl_a9h_w(w: W, op: i32, arg: usize) -> usize;
        ohpl_a9h_w_set_f(w: W, op: i32, v: *const f32);
        ohpl_a9h_w_str(w: W, op: i32, t: *const u32, n: usize, arg: usize) -> usize;
        ohpl_a9h_w_get_str(w: W, op: i32, idx: i32, sink: Sink, ctx: *mut c_void);
        ohpl_a9h_w_cb(w: W, msg: i32, obj: usize);
        ohpl_a9h_text_width(w: W, t: *const u32, n: usize) -> f32;
        ohpl_a9h_image_file(w: W, t: *const u32, n: usize);
        ohpl_a9h_lux(op: i32, arg: usize) -> usize;
        ohpl_a9h_lux_str(op: i32, sink: Sink, ctx: *mut c_void);
        ohpl_a9h_lux_with(op: i32, t: *const u32, n: usize) -> usize;
        ohpl_a9h_lux_with_str(
            op: i32,
            t: *const u32,
            n: usize,
            sink: Sink,
            ctx: *mut c_void,
        );
        ohpl_a9h_plat_list(
            kind: i32,
            dir: *const u32,
            dir_n: usize,
            mask: *const u32,
            mask_n: usize,
            sink: Sink,
            ctx: *mut c_void,
        );
        ohpl_a9h_plat_date(kind: i32, path: *const u32, n: usize, out: *mut i32);
        ohpl_a9h_plat_folder(op: i32, path: *const u32, n: usize) -> usize;
        ohpl_a9h_cfg(
            op: i32,
            sec: *const u8,
            sec_n: usize,
            key: *const u8,
            key_n: usize,
            out: *mut f32,
        ) -> usize;
        ohpl_a9h_tr(
            cat: *const u8,
            cat_n: usize,
            key: *const u8,
            key_n: usize,
            sink: Sink,
            ctx: *mut c_void,
        );
        ohpl_a9h_story_new(path: *const u32, n: usize) -> W;
        ohpl_a9h_story_str(story: W, op: i32, sink: Sink, ctx: *mut c_void);
        ohpl_a9h_story_activate(story: W);
        ohpl_a9h_story_free(story: W);
    }
}
#[cfg(test)]
use super::fake as ffi;

/// SAFETY of every call below: the arguments are opaque host pointers (null is ignored by the host)
/// and slices that outlive the call.
macro_rules! host {
    ($e:expr) => {
        // SAFETY: see the module note; slices and out-pointers are live for the call.
        unsafe { $e }
    };
}

pub type WStr = Vec<u32>;

pub fn ws(s: &str) -> WStr {
    s.chars().map(u32::from).collect()
}

unsafe extern "C" fn sink_str(ctx: *mut c_void, p: *const u32, n: usize) {
    // SAFETY: `ctx` is the `WStr` the caller passed and `p` has `n` readable elements.
    let (Some(out), true) = (unsafe { ctx.cast::<WStr>().as_mut() }, !p.is_null()) else {
        return;
    };
    // SAFETY: as above.
    out.extend_from_slice(unsafe { std::slice::from_raw_parts(p, n) });
}

unsafe extern "C" fn sink_list(ctx: *mut c_void, p: *const u32, n: usize) {
    // SAFETY: `ctx` is the `Vec<WStr>` the caller passed and `p` has `n` readable elements.
    let Some(out) = (unsafe { ctx.cast::<Vec<WStr>>().as_mut() }) else {
        return;
    };
    let item = if p.is_null() {
        WStr::new()
    } else {
        // SAFETY: as above.
        unsafe { std::slice::from_raw_parts(p, n) }.to_vec()
    };
    out.push(item);
}

fn collect(f: impl FnOnce(Sink, *mut c_void)) -> WStr {
    let mut out = WStr::new();
    f(sink_str, (&raw mut out).cast());
    out
}

fn collect_list(f: impl FnOnce(Sink, *mut c_void)) -> Vec<WStr> {
    let mut out = Vec::new();
    f(sink_list, (&raw mut out).cast());
    out
}

#[derive(Clone, Copy)]
pub enum Kind {
    Window = 0,
    Button = 1,
    Label = 2,
    ListBox = 3,
    Image = 4,
    Frame = 5,
    TextBox = 6,
}

pub mod set_op {
    pub const DRAW_FOCUS: i32 = 0;
    pub const GET_DRAW_FOCUS: i32 = 1;
    pub const FOCUSED: i32 = 2;
    pub const DEFAULT_FOCUS: i32 = 3;
    pub const POPUP_ACTIVE: i32 = 4;
    pub const PUSH_ATTENTION: i32 = 5;
    pub const PUSH_DEFAULT: i32 = 6;
    pub const PUSH_FOCUSED: i32 = 7;
    pub const POP_ATTENTION: i32 = 8;
    pub const POP_DEFAULT: i32 = 9;
    pub const POP_FOCUSED: i32 = 10;
    pub const ATTENTION: i32 = 11;
    pub const GET_ATTENTION: i32 = 12;
}

pub(super) mod wop {
    pub const ENABLED: i32 = 0;
    pub const VISIBLE: i32 = 1;
    pub const IS_VISIBLE: i32 = 2;
    pub const STATIC: i32 = 3;
    pub const CENTER: i32 = 4;
    pub const GLOBAL_UI: i32 = 5;
    pub const USER_DATA: i32 = 6;
    pub const CLOSE_DISABLES: i32 = 7;
    pub const WORD_WRAP: i32 = 8;
    pub const CLIP: i32 = 9;
    pub const DRAW_BG: i32 = 10;
    pub const AUTOGEN: i32 = 11;
    pub const FORCE_ENTER: i32 = 12;
    pub const CB_LOST_FOCUS: i32 = 13;
    pub const MAX_LEN: i32 = 14;
    pub const SELECT_TEXT: i32 = 15;
    pub const LIST_CLEAR: i32 = 16;
    pub const LIST_NUM: i32 = 17;
    pub const LIST_SELECTED: i32 = 18;
    pub const LIST_SET_SELECTED: i32 = 19;
    pub const LIST_SET_LOCKED: i32 = 20;
    pub const LIST_LOCKED: i32 = 21;
    pub const ALIGN_CENTER: i32 = 22;
    pub const LIST_ITEM_USER: i32 = 23;
}

pub mod f {
    pub const POS: i32 = 0;
    pub const SIZE: i32 = 1;
    pub const FONT_COLOR: i32 = 2;
    pub const COLOR_MUL: i32 = 3;
    pub const BG_COLOR: i32 = 4;
    pub const SCROLL_SPEED: i32 = 5;
    pub const FONT_SIZE: i32 = 6;
}

mod wstr {
    #[cfg(test)]
    pub const TEXT: i32 = 0;
    pub const ILLEGAL: i32 = 1;
    pub const LIST_ADD: i32 = 2;
}

pub mod msg {
    pub const ON_UPDATE: i32 = 0;
    pub const BUTTON_PRESSED: i32 = 1;
    pub const UI_BUTTON_PRESS: i32 = 2;
    pub const SELECTION_DOUBLE_CLICK: i32 = 3;
    pub const CHECK_CHANGE: i32 = 4;
    pub const GET_UI_NAV_FOCUS: i32 = 5;
    pub const SELECTION_CHANGE: i32 = 6;
    pub const WINDOW_CLOSE: i32 = 7;
    pub const TEXT_BOX_ENTER: i32 = 8;
}

#[derive(Clone, Copy)]
pub enum Arrow {
    Up = 0,
    Right = 1,
    Down = 2,
    Left = 3,
}

/// Runs the Rust-owned path first; the tests replay against the fake host.
#[cfg(not(test))]
macro_rules! direct {
    ($e:expr) => {
        if let Some(r) = $e {
            return r;
        }
    };
}

#[cfg(test)]
macro_rules! direct {
    ($e:expr) => {};
}

pub fn create(set: W, kind: Kind, parent: W, pos: [f32; 3], size: [f32; 2], text: &[u32]) -> W {
    direct!(super::direct::create(
        set, kind, parent, pos, size, false, text
    ));
    host!(ffi::ohpl_a9h_new(
        set,
        kind as i32,
        parent,
        pos.as_ptr(),
        size.as_ptr(),
        0,
        text.as_ptr(),
        text.len()
    ))
}

pub fn create_window(set: W, close: bool, pos: [f32; 3], size: [f32; 2], text: &[u32]) -> W {
    direct!(super::direct::create(
        set,
        Kind::Window,
        W::NULL,
        pos,
        size,
        close,
        text
    ));
    host!(ffi::ohpl_a9h_new(
        set,
        Kind::Window as i32,
        W::NULL,
        pos.as_ptr(),
        size.as_ptr(),
        i32::from(close),
        text.as_ptr(),
        text.len()
    ))
}

/// Popup extras: `kill_escape` -1 leaves the default; `copy_focus` mirrors the set's draw-focus flag.
#[derive(Clone, Copy)]
pub struct Opts {
    pub kill_escape: i32,
    pub copy_focus: bool,
}

impl Opts {
    pub const PLAIN: Opts = Opts {
        kill_escape: -1,
        copy_focus: false,
    };
    pub const FOCUS: Opts = Opts {
        kill_escape: -1,
        copy_focus: true,
    };
    pub const NO_ESCAPE: Opts = Opts {
        kill_escape: 0,
        copy_focus: true,
    };
}

/// Message box with up to two buttons. `obj` is a callback token (0 for none).
pub fn popup(
    set: W,
    title: &[u32],
    text: &[u32],
    ok: &[u32],
    cancel: &[u32],
    obj: usize,
    opts: Opts,
) -> W {
    let s = [title, text, ok, cancel].map(|t| Str {
        p: t.as_ptr(),
        n: t.len(),
    });
    host!(ffi::ohpl_a9h_popup(
        set,
        s.as_ptr(),
        obj,
        opts.kill_escape,
        opts.copy_focus
    ))
}

impl W {
    pub fn set_op(self, op: i32, w: W, arg: usize) -> usize {
        #[cfg(not(test))]
        {
            super::direct::set(self, op, w, arg).unwrap_or_default()
        }
        #[cfg(test)]
        host!(ffi::ohpl_a9h_set(self, op, w, arg))
    }

    pub fn set_draw_focus(self, on: bool) {
        self.set_op(set_op::DRAW_FOCUS, W::NULL, usize::from(on));
    }

    pub fn draw_focus(self) -> bool {
        self.set_op(set_op::GET_DRAW_FOCUS, W::NULL, 0) != 0
    }

    pub fn popup_active(self) -> bool {
        self.set_op(set_op::POPUP_ACTIVE, W::NULL, 0) != 0
    }

    pub fn attention(self) -> W {
        W::from_addr(self.set_op(set_op::GET_ATTENTION, W::NULL, 0))
    }

    fn op(self, op: i32, arg: usize) -> usize {
        direct!(super::direct::op(self, op, arg));
        host!(ffi::ohpl_a9h_w(self, op, arg))
    }

    pub fn set_enabled(self, on: bool) {
        self.op(wop::ENABLED, usize::from(on));
    }

    pub fn set_visible(self, on: bool) {
        self.op(wop::VISIBLE, usize::from(on));
    }

    pub fn is_visible(self) -> bool {
        self.op(wop::IS_VISIBLE, 0) != 0
    }

    pub fn set_static(self, on: bool) {
        self.op(wop::STATIC, usize::from(on));
    }

    pub fn center_in_set(self) {
        self.op(wop::CENTER, 0);
    }

    pub fn set_global_ui_listener(self, on: bool) {
        self.op(wop::GLOBAL_UI, usize::from(on));
    }

    pub fn set_user_data(self, w: W) {
        self.op(wop::USER_DATA, w.addr());
    }

    pub fn set_close_disables_window(self, on: bool) {
        self.op(wop::CLOSE_DISABLES, usize::from(on));
    }

    pub fn set_word_wrap(self, on: bool) {
        self.op(wop::WORD_WRAP, usize::from(on));
    }

    pub fn set_clip_active(self, on: bool) {
        self.op(wop::CLIP, usize::from(on));
    }

    pub fn set_draw_background(self, on: bool) {
        self.op(wop::DRAW_BG, usize::from(on));
    }

    pub fn set_autogenerate_size(self, on: bool) {
        self.op(wop::AUTOGEN, usize::from(on));
    }

    pub fn set_force_callback_on_enter(self, on: bool) {
        self.op(wop::FORCE_ENTER, usize::from(on));
    }

    pub fn set_callback_on_lost_focus(self, on: bool) {
        self.op(wop::CB_LOST_FOCUS, usize::from(on));
    }

    pub fn set_max_text_length(self, n: i32) {
        self.op(wop::MAX_LEN, n as usize);
    }

    pub fn select_text(self) {
        self.op(wop::SELECT_TEXT, 0);
    }

    pub fn align_center(self) {
        self.op(wop::ALIGN_CENTER, 0);
    }

    pub fn list_clear(self) {
        self.op(wop::LIST_CLEAR, 0);
    }

    pub fn list_len(self) -> i32 {
        self.op(wop::LIST_NUM, 0) as i32
    }

    pub fn list_selected(self) -> i32 {
        self.op(wop::LIST_SELECTED, 0) as i32
    }

    pub fn list_select(self, i: i32) {
        self.op(wop::LIST_SET_SELECTED, i as usize);
    }

    pub fn set_locked(self, on: bool) {
        self.op(wop::LIST_SET_LOCKED, usize::from(on));
    }

    pub fn locked(self) -> bool {
        self.op(wop::LIST_LOCKED, 0) != 0
    }

    pub fn list_item_user(self, i: i32) -> W {
        W::from_addr(self.op(wop::LIST_ITEM_USER, i as usize))
    }

    pub fn list_add(self, text: &[u32], user: W) {
        host!(ffi::ohpl_a9h_w_str(
            self,
            wstr::LIST_ADD,
            text.as_ptr(),
            text.len(),
            user.addr()
        ));
    }

    pub fn set_text(self, text: &[u32]) {
        #[cfg(not(test))]
        super::direct::set_text(self, text);
        #[cfg(test)]
        host!(ffi::ohpl_a9h_w_str(
            self,
            wstr::TEXT,
            text.as_ptr(),
            text.len(),
            0
        ));
    }

    pub fn set_illegal_chars(self, text: &[u32]) {
        host!(ffi::ohpl_a9h_w_str(
            self,
            wstr::ILLEGAL,
            text.as_ptr(),
            text.len(),
            0
        ));
    }

    pub fn text(self) -> WStr {
        #[cfg(not(test))]
        {
            super::direct::text(self)
        }
        #[cfg(test)]
        collect(|s, c| host!(ffi::ohpl_a9h_w_get_str(self, wstr::TEXT, 0, s, c)))
    }

    pub fn list_item_text(self, i: i32) -> WStr {
        collect(|s, c| host!(ffi::ohpl_a9h_w_get_str(self, wstr::LIST_ADD, i, s, c)))
    }

    pub fn set_f(self, op: i32, v: &[f32]) {
        direct!(super::direct::set_f(self, op, v).then_some(()));
        host!(ffi::ohpl_a9h_w_set_f(self, op, v.as_ptr()));
    }

    pub fn set_position(self, p: [f32; 3]) {
        self.set_f(f::POS, &p);
    }

    pub fn set_size(self, s: [f32; 2]) {
        self.set_f(f::SIZE, &s);
    }

    pub fn set_font_color(self, c: [f32; 4]) {
        self.set_f(f::FONT_COLOR, &c);
    }

    pub fn size(self) -> [f32; 2] {
        let mut o = [0.0f32; 2];
        #[cfg(not(test))]
        o.copy_from_slice(&super::direct::get_f(self, f::SIZE));
        #[cfg(test)]
        host!(ffi::ohpl_a9h_w_get_f(self, f::SIZE, o.as_mut_ptr()));
        o
    }

    pub fn local_position(self) -> [f32; 3] {
        let mut o = [0.0f32; 3];
        #[cfg(not(test))]
        o.copy_from_slice(&super::direct::get_f(self, f::POS));
        #[cfg(test)]
        host!(ffi::ohpl_a9h_w_get_f(self, f::POS, o.as_mut_ptr()));
        o
    }

    pub fn add_callback(self, message: i32, obj: usize) {
        host!(ffi::ohpl_a9h_w_cb(self, message, obj));
    }

    pub fn nav(self, dir: Arrow, target: W) {
        #[cfg(not(test))]
        super::direct::nav(self, dir as i32, target);
        #[cfg(test)]
        host!(ffi::ohpl_a9h_w_nav(self, dir as i32, target));
    }

    pub fn text_width(self, text: &[u32]) -> f32 {
        host!(ffi::ohpl_a9h_text_width(self, text.as_ptr(), text.len()))
    }

    pub fn set_image_file(self, file: &[u32]) {
        host!(ffi::ohpl_a9h_image_file(self, file.as_ptr(), file.len()));
    }
}

pub mod lux_op {
    pub const HARD_GET: i32 = 0;
    pub const HARD_SET: i32 = 1;
    pub const CUSTOM_STORY_ACTIVE: i32 = 2;
    pub const CUSTOM_STORY_CLEAR: i32 = 3;
    pub const GAMEPAD: i32 = 4;
    pub const INPUT_GAME: i32 = 5;
    pub const UPDATER_RESET: i32 = 6;
    pub const PROGRESS_LOG_RESET: i32 = 7;
    pub const SAVE_EXISTS: i32 = 8;
    pub const AUTO_LOAD: i32 = 9;
    pub const USER_CONFIG_LOADED: i32 = 10;
    pub const INIT_USER_CONFIG: i32 = 11;
    pub const START_CUSTOM_STORY: i32 = 12;
    pub const ENGINE_EXIT: i32 = 13;
    pub const MENU_SET_WINDOW: i32 = 14;
    pub const MENU_EXIT: i32 = 15;
    pub const MENU_TOP_ALPHA: i32 = 16;
    pub const MENU_RECREATE: i32 = 17;
    pub const USERDIR_RESOURCES: i32 = 18;
}

pub fn lux(op: i32, arg: usize) -> usize {
    #[cfg(not(test))]
    {
        use crate::luxmenu::mainmenu as mm;
        match op {
            lux_op::MENU_SET_WINDOW => mm::set_window(arg),
            lux_op::MENU_EXIT => mm::exit_menu(arg),
            lux_op::MENU_TOP_ALPHA => mm::set_top_alpha(arg),
            lux_op::MENU_RECREATE => mm::recreate_gui(),
            _ => return host!(ffi::ohpl_a9h_lux(op, arg)),
        }
        0
    }
    #[cfg(test)]
    {
        host!(ffi::ohpl_a9h_lux(op, arg))
    }
}

pub mod lux_path {
    pub const BASE_SAVE: i32 = 0;
    pub const PROFILE_SAVE: i32 = 1;
    pub const MAIN_PROFILE_SAVE: i32 = 2;
    pub const BASE_CONFIG: i32 = 3;
    pub const DEFAULT_PROFILE: i32 = 4;
    pub const STORY_ROOT: i32 = 5;
    pub const USER_STORY_ROOT: i32 = 6;
}

pub fn lux_str(op: i32) -> WStr {
    collect(|s, c| host!(ffi::ohpl_a9h_lux_str(op, s, c)))
}

pub mod lux_with_op {
    pub const CREATE_PROFILE: i32 = 0;
    pub const SET_PROFILE: i32 = 1;
    pub const LOAD_GAME_FROM_FILE: i32 = 2;
    pub const PROFILE_IS: i32 = 3;
}

pub fn lux_with(op: i32, t: &[u32]) -> usize {
    host!(ffi::ohpl_a9h_lux_with(op, t.as_ptr(), t.len()))
}

pub fn proper_save_name(file: &[u32]) -> WStr {
    collect(|s, c| {
        host!(ffi::ohpl_a9h_lux_with_str(
            0,
            file.as_ptr(),
            file.len(),
            s,
            c
        ))
    })
}

pub fn list_files(dir: &[u32], mask: &[u32]) -> Vec<WStr> {
    collect_list(|s, c| {
        host!(ffi::ohpl_a9h_plat_list(
            0,
            dir.as_ptr(),
            dir.len(),
            mask.as_ptr(),
            mask.len(),
            s,
            c
        ))
    })
}

pub fn list_folders(dir: &[u32]) -> Vec<WStr> {
    collect_list(|s, c| {
        host!(ffi::ohpl_a9h_plat_list(
            1,
            dir.as_ptr(),
            dir.len(),
            std::ptr::null(),
            0,
            s,
            c
        ))
    })
}

#[derive(Clone, Copy)]
pub enum DateKind {
    Modified = 0,
    Created = 1,
}

/// The first six ints of `hpl::cDate`.
pub fn file_date(kind: DateKind, path: &[u32]) -> crate::luxsave::names::Date {
    let mut o = [0i32; 8];
    host!(ffi::ohpl_a9h_plat_date(
        kind as i32,
        path.as_ptr(),
        path.len(),
        o.as_mut_ptr()
    ));
    crate::luxsave::names::Date {
        seconds: o[0],
        minutes: o[1],
        hours: o[2],
        month_day: o[3],
        month: o[4],
        year: o[5],
    }
}

pub fn folder_exists(path: &[u32]) -> bool {
    host!(ffi::ohpl_a9h_plat_folder(0, path.as_ptr(), path.len())) != 0
}

pub fn create_folder(path: &[u32]) {
    host!(ffi::ohpl_a9h_plat_folder(1, path.as_ptr(), path.len()));
}

pub fn remove_folder(path: &[u32]) -> bool {
    host!(ffi::ohpl_a9h_plat_folder(2, path.as_ptr(), path.len())) != 0
}

/// `cConfigFile::GetFloat(section, key, 0)` of the menu config.
pub fn cfg_float(section: &str, key: &str) -> f32 {
    let mut o = [0.0f32; 2];
    host!(ffi::ohpl_a9h_cfg(
        0,
        section.as_ptr(),
        section.len(),
        key.as_ptr(),
        key.len(),
        o.as_mut_ptr()
    ));
    o[0]
}

pub fn cfg_vec2(section: &str, key: &str) -> [f32; 2] {
    let mut o = [0.0f32; 2];
    host!(ffi::ohpl_a9h_cfg(
        1,
        section.as_ptr(),
        section.len(),
        key.as_ptr(),
        key.len(),
        o.as_mut_ptr()
    ));
    o
}

/// `kTranslate(category, entry)`.
pub fn tr(cat: &str, key: &str) -> WStr {
    collect(|s, c| {
        host!(ffi::ohpl_a9h_tr(
            cat.as_ptr(),
            cat.len(),
            key.as_ptr(),
            key.len(),
            s,
            c
        ))
    })
}

pub fn story_new(path: &[u32]) -> W {
    host!(ffi::ohpl_a9h_story_new(path.as_ptr(), path.len()))
}

pub mod story_str_op {
    pub const NAME: i32 = 0;
    pub const AUTHOR: i32 = 1;
    pub const IMG_FILE: i32 = 2;
}

pub fn story_str(story: W, op: i32) -> WStr {
    collect(|s, c| host!(ffi::ohpl_a9h_story_str(story, op, s, c)))
}

pub fn story_activate(story: W) {
    host!(ffi::ohpl_a9h_story_activate(story));
}

pub fn story_free(story: W) {
    host!(ffi::ohpl_a9h_story_free(story));
}

pub fn lux_flag(op: i32) -> bool {
    lux(op, 0) != 0
}
