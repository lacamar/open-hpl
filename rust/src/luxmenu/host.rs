//! Host ops of the C++ GUI and engine objects the menu windows drive; one dispatch function whose
//! codes and argument layout are mirrored in `amnesia/src/game/RustMenuHost.cpp`.

use std::sync::atomic::{AtomicUsize, Ordering};

pub const NSTR: usize = 4;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Args {
    pub p: [usize; 6],
    pub f: [f32; 6],
    pub i: [i32; 4],
    pub s: [*const u32; NSTR],
    pub n: [usize; NSTR],
}

impl Args {
    pub const fn new() -> Args {
        Args {
            p: [0; 6],
            f: [0.0; 6],
            i: [0; 4],
            s: [std::ptr::null(); NSTR],
            n: [0; NSTR],
        }
    }
}

impl Default for Args {
    fn default() -> Self {
        Args::new()
    }
}

pub type HostFn = unsafe extern "C" fn(code: i32, a: *mut Args) -> usize;

static HOST: AtomicUsize = AtomicUsize::new(0);

pub fn set_host(f: HostFn) {
    HOST.store(f as usize, Ordering::Release);
}

pub fn call(code: i32, a: &mut Args) -> usize {
    #[cfg(not(test))]
    if code == op::SET_WINDOW_ACTIVE {
        super::mainmenu::set_window(a.i[0] as usize);
        return 0;
    }
    #[cfg(test)]
    {
        super::fake::call(code, a)
    }
    #[cfg(not(test))]
    {
        let raw = HOST.load(Ordering::Acquire);
        if raw == 0 {
            return 0;
        }
        // SAFETY: only `set_host` stores here, and it stores a `HostFn`.
        let f: HostFn = unsafe { std::mem::transmute::<usize, HostFn>(raw) };
        // SAFETY: `a` is a live argument block and the host reads and writes only its fields.
        unsafe { f(code, a) }
    }
}

pub mod op {
    pub const WINDOW: i32 = 1;
    pub const BUTTON: i32 = 2;
    pub const LABEL: i32 = 3;
    pub const CHECKBOX: i32 = 4;
    pub const COMBO: i32 = 5;
    pub const SLIDER: i32 = 6;
    pub const TABFRAME: i32 = 7;
    pub const IMAGE: i32 = 8;
    pub const GROUP: i32 = 9;
    pub const DUMMY: i32 = 10;
    pub const FRAME: i32 = 11;

    pub const SIZE: i32 = 20;
    pub const LOCAL_POS: i32 = 21;
    pub const SET_SIZE: i32 = 22;
    pub const SET_POS: i32 = 23;
    pub const SET_TEXT: i32 = 24;
    pub const GET_TEXT: i32 = 25;
    pub const SET_VISIBLE: i32 = 26;
    pub const SET_ENABLED: i32 = 27;
    pub const IS_VISIBLE: i32 = 28;
    pub const SET_USER_DATA: i32 = 29;
    pub const GET_USER_DATA: i32 = 30;
    pub const SET_USER_VALUE: i32 = 31;
    pub const GET_USER_VALUE: i32 = 32;
    pub const SET_NAV: i32 = 33;
    pub const SET_GLOBAL_UI: i32 = 34;
    pub const SET_STATIC: i32 = 35;
    pub const FONT_COLOR: i32 = 36;
    pub const SET_FONT_COLOR: i32 = 37;
    pub const FONT_SIZE: i32 = 38;
    pub const SET_FONT_SIZE: i32 = 39;
    pub const TEXT_LENGTH: i32 = 40;
    pub const SET_WORD_WRAP: i32 = 41;
    pub const SET_CLIP: i32 = 42;
    pub const SET_SCROLL_SPEED: i32 = 43;
    pub const SET_DRAW_BG: i32 = 44;
    pub const SET_BG_COLOR: i32 = 45;
    pub const SET_ALIGN: i32 = 46;
    pub const SET_AUTOSIZE: i32 = 47;
    pub const SET_SCROLL_OFFSET: i32 = 48;
    pub const PARENT: i32 = 49;
    pub const GLOBAL_POS: i32 = 50;
    pub const ADD_CALLBACK: i32 = 51;
    pub const SET_CHECKED: i32 = 52;
    pub const IS_CHECKED: i32 = 53;
    pub const COMBO_ADD: i32 = 54;
    pub const COMBO_CLEAR: i32 = 55;
    pub const COMBO_SELECT: i32 = 56;
    pub const COMBO_SELECTED: i32 = 57;
    pub const COMBO_NUM: i32 = 58;
    pub const SLIDER_MAX: i32 = 59;
    pub const SLIDER_BAR: i32 = 60;
    pub const SLIDER_SET: i32 = 61;
    pub const SLIDER_GET: i32 = 62;
    pub const SLIDER_GET_MAX: i32 = 63;
    pub const TAB_ADD: i32 = 64;
    pub const TAB_NUM: i32 = 65;
    pub const TAB_GET: i32 = 66;
    pub const TAB_TOP: i32 = 67;
    pub const TAB_LABEL: i32 = 68;
    pub const TAB_FRAME: i32 = 69;
    pub const FRAME_OF: i32 = 70;
    pub const FRAME_SCROLL_TO: i32 = 71;
    pub const FRAME_SCROLL: i32 = 72;
    pub const FRAME_DRAW_BG: i32 = 73;
    pub const MOUSE_OVER: i32 = 74;

    pub const POPUP_ACTIVE: i32 = 80;
    pub const SET_DEFAULT_NAV: i32 = 81;
    pub const SET_FOCUSED: i32 = 82;
    pub const PUSH_ATTENTION: i32 = 83;
    pub const POP_ATTENTION: i32 = 84;
    pub const PUSH_FOCUSED: i32 = 85;
    pub const POP_FOCUSED: i32 = 86;
    pub const PUSH_DEFAULT_NAV: i32 = 87;
    pub const SET_ATTENTION: i32 = 88;
    pub const SET_MOUSE_MOVE: i32 = 89;
    pub const SET_DRAW_MOUSE: i32 = 90;
    pub const MSGBOX: i32 = 91;

    pub const SET_WINDOW_ACTIVE: i32 = 100;
    pub const TRANSLATE: i32 = 101;
    pub const OPT_READ: i32 = 102;
    pub const OPT_APPLY: i32 = 103;
    pub const SAVE_CONFIG: i32 = 104;
    pub const NEEDS_RESTART: i32 = 105;
    pub const RESTART_WARNING: i32 = 106;
    pub const SET_GAMMA: i32 = 107;
    pub const SET_MOUSE_SENS: i32 = 108;
    pub const SET_PAD_SENS: i32 = 109;
    pub const SET_VOLUME: i32 = 110;
    pub const PLAY_SANITY: i32 = 111;
    pub const GAMEPAD_PRESENT: i32 = 112;
    pub const SHOW_COMMENTARY_CFG: i32 = 114;
    pub const LANG_FILES: i32 = 115;
    pub const LIST_STR: i32 = 116;
    pub const VIDEO_MODES: i32 = 117;
    pub const VIDEO_MODE: i32 = 118;
    pub const DISPLAY_NAME: i32 = 119;
    pub const SND_DEVICES: i32 = 120;
    pub const SND_DEVICE: i32 = 121;

    pub const ACTIONS: i32 = 140;
    pub const ACTION: i32 = 141;
    pub const DEFAULT_INPUTS: i32 = 142;
    pub const DEFAULT_INPUT: i32 = 143;
    pub const INITIAL_INPUTS: i32 = 144;
    pub const ACTION_CLEAR: i32 = 145;
    pub const ACTION_ADD: i32 = 146;
    pub const ACTION_NAME: i32 = 147;
    pub const GUI_MOUSE: i32 = 148;
    pub const AXIS_RANGE: i32 = 149;
}

/// Callback message codes; `host` maps them to `eGuiMessage_*`.
pub mod msg {
    pub const ON_UPDATE: i32 = 0;
    pub const BUTTON_PRESSED: i32 = 1;
    pub const UI_BUTTON: i32 = 2;
    pub const SELECTION: i32 = 3;
    pub const CHECK: i32 = 4;
    pub const SLIDER_MOVE: i32 = 5;
    pub const MOUSE_MOVE: i32 = 6;
    pub const GET_NAV_FOCUS: i32 = 7;
    pub const LOSE_NAV_FOCUS: i32 = 8;
    pub const MOUSE_ENTER: i32 = 9;
    pub const MOUSE_LEAVE: i32 = 10;
    pub const MOUSE_DOWN: i32 = 11;
    pub const TEXT_CHANGE: i32 = 12;
    pub const KEY_PRESS: i32 = 13;
    pub const UI_ARROW: i32 = 14;
    pub const GAMEPAD: i32 = 15;
    pub const ON_DRAW: i32 = 16;
}

pub const UP: i32 = 0;
pub const RIGHT: i32 = 1;
pub const DOWN: i32 = 2;
pub const LEFT: i32 = 3;

pub const UI_PRIMARY: i32 = 1;
pub const UI_SECONDARY: i32 = 2;
pub const UI_PREV_PAGE: i32 = 4;
pub const UI_NEXT_PAGE: i32 = 8;
pub const KEY_ESCAPE: i32 = 5;
pub const PAD_B: i32 = 1;
pub const MOUSE_LEFT: i32 = 1;

/// Wide string for `Args::s`.
pub type S = Vec<u32>;

pub fn w(s: &str) -> S {
    s.chars().map(u32::from).collect()
}

pub fn narrow(s: &[u32]) -> String {
    s.iter().filter_map(|&c| char::from_u32(c)).collect()
}

fn read_str(a: &Args, slot: usize) -> S {
    let (Some(&p), Some(&n)) = (a.s.get(slot), a.n.get(slot)) else {
        return S::new();
    };
    if p.is_null() || n == 0 {
        return S::new();
    }
    // SAFETY: the host leaves `s[slot]` pointing at `n` live code units until its next op.
    unsafe { std::slice::from_raw_parts(p, n) }.to_vec()
}

/// Widget pointer; 0 is null.
#[derive(Clone, Copy, PartialEq, Eq, Default, Debug)]
pub struct Wd(pub usize);

pub fn tr(cat: &str, name: &str) -> S {
    let (c, n) = (w(cat), w(name));
    let mut a = Args::new();
    a.s[0] = c.as_ptr();
    a.n[0] = c.len();
    a.s[1] = n.as_ptr();
    a.n[1] = n.len();
    call(op::TRANSLATE, &mut a);
    read_str(&a, 0)
}

pub fn simple(code: i32, i0: i32) -> usize {
    let mut a = Args::new();
    a.i[0] = i0;
    call(code, &mut a)
}

pub fn simple_f(code: i32, f0: f32) {
    let mut a = Args::new();
    a.f[0] = f0;
    call(code, &mut a);
}

pub fn string_op(code: i32, i0: i32) -> S {
    let mut a = Args::new();
    a.i[0] = i0;
    call(code, &mut a);
    read_str(&a, 0)
}

impl Wd {
    pub fn is_null(self) -> bool {
        self.0 == 0
    }

    fn base(self) -> Args {
        let mut a = Args::new();
        a.p[0] = self.0;
        a
    }

    fn run(self, code: i32, mut a: Args) -> (usize, Args) {
        let r = call(code, &mut a);
        (r, a)
    }

    pub fn size(self) -> (f32, f32) {
        let (_, a) = self.run(op::SIZE, self.base());
        (a.f[0], a.f[1])
    }

    pub fn local_pos(self) -> [f32; 3] {
        let (_, a) = self.run(op::LOCAL_POS, self.base());
        [a.f[0], a.f[1], a.f[2]]
    }

    pub fn global_pos(self) -> [f32; 3] {
        let (_, a) = self.run(op::GLOBAL_POS, self.base());
        [a.f[0], a.f[1], a.f[2]]
    }

    pub fn set_size(self, x: f32, y: f32) {
        let mut a = self.base();
        a.f[0] = x;
        a.f[1] = y;
        call(op::SET_SIZE, &mut a);
    }

    pub fn set_pos(self, p: [f32; 3]) {
        let mut a = self.base();
        a.f[..3].copy_from_slice(&p);
        call(op::SET_POS, &mut a);
    }

    pub fn set_text(self, t: &[u32]) {
        let mut a = self.base();
        a.s[0] = t.as_ptr();
        a.n[0] = t.len();
        call(op::SET_TEXT, &mut a);
    }

    pub fn text(self) -> S {
        let (_, a) = self.run(op::GET_TEXT, self.base());
        read_str(&a, 0)
    }

    fn flag(self, code: i32, v: bool) {
        let mut a = self.base();
        a.i[0] = i32::from(v);
        call(code, &mut a);
    }

    pub fn set_visible(self, v: bool) {
        self.flag(op::SET_VISIBLE, v);
    }

    pub fn set_enabled(self, v: bool) {
        self.flag(op::SET_ENABLED, v);
    }

    pub fn is_visible(self) -> bool {
        self.run(op::IS_VISIBLE, self.base()).0 != 0
    }

    pub fn set_user_data(self, v: usize) {
        let mut a = self.base();
        a.p[1] = v;
        call(op::SET_USER_DATA, &mut a);
    }

    pub fn user_data(self) -> usize {
        self.run(op::GET_USER_DATA, self.base()).0
    }

    pub fn set_user_value(self, v: i32) {
        let mut a = self.base();
        a.i[0] = v;
        call(op::SET_USER_VALUE, &mut a);
    }

    pub fn user_value(self) -> i32 {
        self.run(op::GET_USER_VALUE, self.base()).0 as i32
    }

    pub fn set_nav(self, dir: i32, to: Wd) {
        let mut a = self.base();
        a.i[0] = dir;
        a.p[1] = to.0;
        call(op::SET_NAV, &mut a);
    }

    pub fn set_global_ui(self, v: bool) {
        self.flag(op::SET_GLOBAL_UI, v);
    }

    pub fn set_static(self, v: bool) {
        self.flag(op::SET_STATIC, v);
    }

    pub fn font_color(self) -> [f32; 4] {
        let (_, a) = self.run(op::FONT_COLOR, self.base());
        [a.f[0], a.f[1], a.f[2], a.f[3]]
    }

    pub fn set_font_color(self, c: [f32; 4]) {
        let mut a = self.base();
        a.f[..4].copy_from_slice(&c);
        call(op::SET_FONT_COLOR, &mut a);
    }

    pub fn font_size(self) -> (f32, f32) {
        let (_, a) = self.run(op::FONT_SIZE, self.base());
        (a.f[0], a.f[1])
    }

    pub fn set_font_size(self, x: f32, y: f32) {
        let mut a = self.base();
        a.f[0] = x;
        a.f[1] = y;
        call(op::SET_FONT_SIZE, &mut a);
    }

    pub fn text_length(self) -> f32 {
        let (_, a) = self.run(op::TEXT_LENGTH, self.base());
        a.f[0]
    }

    pub fn set_word_wrap(self, v: bool) {
        self.flag(op::SET_WORD_WRAP, v);
    }

    pub fn set_clip(self, v: bool) {
        self.flag(op::SET_CLIP, v);
    }

    pub fn set_scroll_speed(self, v: f32) {
        let mut a = self.base();
        a.f[0] = v;
        call(op::SET_SCROLL_SPEED, &mut a);
    }

    pub fn set_draw_bg(self, v: bool) {
        self.flag(op::SET_DRAW_BG, v);
    }

    pub fn set_bg_color(self, c: [f32; 4]) {
        let mut a = self.base();
        a.f[..4].copy_from_slice(&c);
        call(op::SET_BG_COLOR, &mut a);
    }

    pub fn set_center_align(self) {
        self.flag(op::SET_ALIGN, true);
    }

    pub fn set_autosize(self, v: bool) {
        self.flag(op::SET_AUTOSIZE, v);
    }

    pub fn set_scroll_offset(self, v: f32) {
        let mut a = self.base();
        a.f[0] = v;
        call(op::SET_SCROLL_OFFSET, &mut a);
    }

    pub fn parent(self) -> Wd {
        Wd(self.run(op::PARENT, self.base()).0)
    }

    pub fn on(self, message: i32, token: usize) {
        let mut a = self.base();
        a.i[0] = message;
        a.p[1] = token;
        call(op::ADD_CALLBACK, &mut a);
    }

    pub fn set_checked(self, v: bool, callback: bool) {
        let mut a = self.base();
        a.i[0] = i32::from(v);
        a.i[1] = i32::from(callback);
        call(op::SET_CHECKED, &mut a);
    }

    pub fn checked(self) -> bool {
        self.run(op::IS_CHECKED, self.base()).0 != 0
    }

    pub fn combo_add(self, t: &[u32]) {
        let mut a = self.base();
        a.s[0] = t.as_ptr();
        a.n[0] = t.len();
        call(op::COMBO_ADD, &mut a);
    }

    pub fn combo_clear(self) {
        let mut a = self.base();
        call(op::COMBO_CLEAR, &mut a);
    }

    pub fn combo_select(self, item: i32, move_list: bool, callback: bool) {
        let mut a = self.base();
        a.i[0] = item;
        a.i[1] = i32::from(move_list);
        a.i[2] = i32::from(callback);
        call(op::COMBO_SELECT, &mut a);
    }

    pub fn combo_selected(self) -> i32 {
        self.run(op::COMBO_SELECTED, self.base()).0 as i32
    }

    pub fn combo_num(self) -> i32 {
        self.run(op::COMBO_NUM, self.base()).0 as i32
    }

    pub fn slider_set_max(self, v: i32) {
        let mut a = self.base();
        a.i[0] = v;
        call(op::SLIDER_MAX, &mut a);
    }

    pub fn slider_set_bar(self, v: i32) {
        let mut a = self.base();
        a.i[0] = v;
        call(op::SLIDER_BAR, &mut a);
    }

    pub fn slider_set(self, v: i32, callback: bool) {
        let mut a = self.base();
        a.i[0] = v;
        a.i[1] = i32::from(callback);
        call(op::SLIDER_SET, &mut a);
    }

    pub fn slider_value(self) -> i32 {
        self.run(op::SLIDER_GET, self.base()).0 as i32
    }

    pub fn slider_max(self) -> i32 {
        self.run(op::SLIDER_GET_MAX, self.base()).0 as i32
    }

    pub fn tab_add(self, t: &[u32]) -> Wd {
        let mut a = self.base();
        a.s[0] = t.as_ptr();
        a.n[0] = t.len();
        Wd(call(op::TAB_ADD, &mut a))
    }

    pub fn tab_num(self) -> i32 {
        self.run(op::TAB_NUM, self.base()).0 as i32
    }

    pub fn tab_get(self, i: i32) -> Wd {
        let mut a = self.base();
        a.i[0] = i;
        Wd(call(op::TAB_GET, &mut a))
    }

    pub fn tab_top(self) -> Wd {
        Wd(self.run(op::TAB_TOP, self.base()).0)
    }

    pub fn tab_label(self) -> Wd {
        Wd(self.run(op::TAB_LABEL, self.base()).0)
    }

    pub fn tab_frame(self) -> Wd {
        Wd(self.run(op::TAB_FRAME, self.base()).0)
    }

    /// The widget itself when it is a `cWidgetFrame`, else null.
    pub fn frame_of(self) -> Wd {
        Wd(self.run(op::FRAME_OF, self.base()).0)
    }

    pub fn frame_scroll_to(self, x: f32, y: f32) {
        let mut a = self.base();
        a.f[0] = x;
        a.f[1] = y;
        call(op::FRAME_SCROLL_TO, &mut a);
    }

    pub fn frame_scroll(self) -> [f32; 3] {
        let (_, a) = self.run(op::FRAME_SCROLL, self.base());
        [a.f[0], a.f[1], a.f[2]]
    }

    pub fn mouse_over(self) -> bool {
        self.run(op::MOUSE_OVER, self.base()).0 != 0
    }

    pub fn frame_draw_bg(self, v: bool) {
        self.flag(op::FRAME_DRAW_BG, v);
    }
}

/// The GUI set widgets are created in.
#[derive(Clone, Copy)]
pub struct Set(pub usize);

pub struct Spec<'a> {
    pub pos: [f32; 3],
    pub size: (f32, f32),
    pub text: &'a [u32],
    pub parent: Wd,
}

impl Set {
    fn create(self, code: i32, sp: &Spec, i0: i32, i1: i32) -> Wd {
        let mut a = Args::new();
        a.p[0] = self.0;
        a.p[1] = sp.parent.0;
        a.f[..3].copy_from_slice(&sp.pos);
        a.f[3] = sp.size.0;
        a.f[4] = sp.size.1;
        a.i[0] = i0;
        a.i[1] = i1;
        a.s[0] = sp.text.as_ptr();
        a.n[0] = sp.text.len();
        Wd(call(code, &mut a))
    }

    pub fn window(self, pos: [f32; 3], size: (f32, f32), title: &[u32]) -> Wd {
        self.create(
            op::WINDOW,
            &Spec {
                pos,
                size,
                text: title,
                parent: Wd(0),
            },
            0,
            0,
        )
    }

    pub fn button(self, pos: [f32; 3], size: (f32, f32), text: &[u32], parent: Wd) -> Wd {
        self.create(
            op::BUTTON,
            &Spec {
                pos,
                size,
                text,
                parent,
            },
            0,
            0,
        )
    }

    pub fn label(self, pos: [f32; 3], size: (f32, f32), text: &[u32], parent: Wd) -> Wd {
        self.create(
            op::LABEL,
            &Spec {
                pos,
                size,
                text,
                parent,
            },
            0,
            0,
        )
    }

    pub fn checkbox(self, pos: [f32; 3], size: (f32, f32), text: &[u32], parent: Wd) -> Wd {
        self.create(
            op::CHECKBOX,
            &Spec {
                pos,
                size,
                text,
                parent,
            },
            0,
            0,
        )
    }

    pub fn combo(self, pos: [f32; 3], size: (f32, f32), parent: Wd) -> Wd {
        self.create(
            op::COMBO,
            &Spec {
                pos,
                size,
                text: &[],
                parent,
            },
            0,
            0,
        )
    }

    pub fn slider(self, pos: [f32; 3], size: (f32, f32), parent: Wd) -> Wd {
        self.create(
            op::SLIDER,
            &Spec {
                pos,
                size,
                text: &[],
                parent,
            },
            0,
            0,
        )
    }

    pub fn tabframe(self, pos: [f32; 3], size: (f32, f32), parent: Wd) -> Wd {
        self.create(
            op::TABFRAME,
            &Spec {
                pos,
                size,
                text: &[],
                parent,
            },
            0,
            1,
        )
    }

    pub fn group(self, pos: [f32; 3], size: (f32, f32), text: &[u32], parent: Wd) -> Wd {
        self.create(
            op::GROUP,
            &Spec {
                pos,
                size,
                text,
                parent,
            },
            0,
            0,
        )
    }

    pub fn dummy(self, pos: [f32; 3], parent: Wd) -> Wd {
        self.create(
            op::DUMMY,
            &Spec {
                pos,
                size: (0.0, 0.0),
                text: &[],
                parent,
            },
            0,
            0,
        )
    }

    pub fn frame(self, pos: [f32; 3], size: (f32, f32), parent: Wd) -> Wd {
        self.create(
            op::FRAME,
            &Spec {
                pos,
                size,
                text: &[],
                parent,
            },
            0,
            0,
        )
    }

    /// `name` empty selects the overload without a widget name.
    pub fn image(self, file: &str, pos: [f32; 3], size: (f32, f32), parent: Wd, name: &str) -> Wd {
        let f = w(file);
        let n = w(name);
        let mut a = Args::new();
        a.p[0] = self.0;
        a.p[1] = parent.0;
        a.f[..3].copy_from_slice(&pos);
        a.f[3] = size.0;
        a.f[4] = size.1;
        a.s[0] = f.as_ptr();
        a.n[0] = f.len();
        a.s[1] = n.as_ptr();
        a.n[1] = n.len();
        Wd(call(op::IMAGE, &mut a))
    }

    fn with(self, code: i32, to: Wd) {
        let mut a = Args::new();
        a.p[0] = self.0;
        a.p[1] = to.0;
        call(code, &mut a);
    }

    pub fn popup_active(self) -> bool {
        let mut a = Args::new();
        a.p[0] = self.0;
        call(op::POPUP_ACTIVE, &mut a) != 0
    }

    pub fn set_default_nav(self, to: Wd) {
        self.with(op::SET_DEFAULT_NAV, to);
    }

    pub fn set_focused(self, to: Wd) {
        self.with(op::SET_FOCUSED, to);
    }

    pub fn set_attention(self, to: Wd) {
        self.with(op::SET_ATTENTION, to);
    }

    pub fn push_attention(self) {
        self.with(op::PUSH_ATTENTION, Wd(0));
    }

    pub fn pop_attention(self) {
        self.with(op::POP_ATTENTION, Wd(0));
    }

    pub fn push_focused(self) {
        self.with(op::PUSH_FOCUSED, Wd(0));
    }

    pub fn pop_focused(self) {
        self.with(op::POP_FOCUSED, Wd(0));
    }

    pub fn push_default_nav(self) {
        self.with(op::PUSH_DEFAULT_NAV, Wd(0));
    }

    pub fn set_mouse_move(self, v: bool) {
        let mut a = Args::new();
        a.p[0] = self.0;
        a.i[0] = i32::from(v);
        call(op::SET_MOUSE_MOVE, &mut a);
    }

    pub fn set_draw_mouse(self, v: bool) {
        let mut a = Args::new();
        a.p[0] = self.0;
        a.i[0] = i32::from(v);
        call(op::SET_DRAW_MOUSE, &mut a);
    }

    /// Message box (title, text, ok, cancel) whose result goes to `token`.
    pub fn msgbox(self, strs: [&[u32]; 4], token: usize) -> usize {
        let mut a = Args::new();
        a.p[0] = self.0;
        a.p[1] = token;
        for (k, s) in strs.iter().enumerate() {
            if let (Some(p), Some(n)) = (a.s.get_mut(k), a.n.get_mut(k)) {
                *p = s.as_ptr();
                *n = s.len();
            }
        }
        call(op::MSGBOX, &mut a)
    }

    pub fn restart_warning(self, token: usize) -> bool {
        let mut a = Args::new();
        a.p[0] = self.0;
        a.p[1] = token;
        call(op::RESTART_WARNING, &mut a) != 0
    }
}

pub fn host_list_str(list: i32, i: i32) -> S {
    let mut a = Args::new();
    a.i[0] = list;
    a.i[1] = i;
    call(op::LIST_STR, &mut a);
    read_str(&a, 0)
}
