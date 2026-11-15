//! Engine calls of the A10b menu classes. Rust owns the class state and flow; widgets, gfx, fonts,
//! sound, config and the updater are C++ objects reached through one `call` entry per object.
//! Every call can be traced (`OHPL_A10B_DUMP`) in a line format the replay test reads back.

use std::collections::HashMap;
use std::ffi::c_void;
use std::io::Write;
use std::sync::{Mutex, OnceLock};

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Rk {
    None,
    P,
    I,
    B,
    F(usize),
    Str,
    Strs,
    U(usize),
}

macro_rules! ops {
    ($($id:ident = $n:expr, $name:literal, $rk:expr;)*) => {
        $(pub const $id: i32 = $n;)*
        pub const OPS: &[(i32, &str, Rk)] = &[$(($n, $name, $rk)),*];
    };
}

ops! {
    GUI_CREATE_SET = 1, "gui_create_set", Rk::P;
    GUI_CREATE_SKIN = 2, "gui_create_skin", Rk::P;
    SET_VIRTUAL_SIZE = 3, "set_virtual_size", Rk::None;
    SET_ACTIVE = 4, "set_active", Rk::None;
    SET_DRAW_MOUSE = 5, "set_draw_mouse", Rk::None;
    SET_SKIN = 6, "set_skin", Rk::None;
    GUI_SET_FOCUS = 7, "gui_set_focus", Rk::None;
    CALC_OFFSET = 8, "calc_offset", Rk::F(4);
    VIEWPORT_NEW = 9, "viewport_new", Rk::P;
    VIEWPORT_ACTIVE = 10, "viewport_active", Rk::None;
    VIEWPORT_VISIBLE = 11, "viewport_visible", Rk::None;
    VIEWPORT_ADD_SET = 12, "viewport_add_set", Rk::None;
    GFX_RECT = 13, "gfx_rect", Rk::P;
    GFX_TEX_FILE = 14, "gfx_tex_file", Rk::P;
    TEX_2D = 15, "tex_2d", Rk::P;
    GFX_FROM_TEX = 16, "gfx_from_tex", Rk::P;
    GFX_DESTROY = 17, "gfx_destroy", Rk::None;
    GFX_SIZE = 18, "gfx_size", Rk::F(2);
    DRAW_GFX = 19, "draw_gfx", Rk::None;
    DRAW_GFX_POS = 20, "draw_gfx_pos", Rk::None;
    DRAW_FONT = 21, "draw_font", Rk::None;
    FONT_LOAD = 22, "font_load", Rk::P;
    FONT_WRAP = 23, "font_wrap", Rk::Strs;
    FONT_LEN = 24, "font_len", Rk::F(1);
    FONT_HEIGHT = 25, "font_height", Rk::F(1);
    TRANSLATE = 26, "translate", Rk::Str;
    CFG_STRING = 27, "cfg_string", Rk::Str;
    CFG_FLOAT = 28, "cfg_float", Rk::F(1);
    CFG_VEC2 = 29, "cfg_vec2", Rk::F(2);
    CFG_COLOR = 30, "cfg_color", Rk::F(4);
    CFG_BOOL = 31, "cfg_bool", Rk::B;
    CFG_SET_BOOL = 32, "cfg_set_bool", Rk::None;
    WIDGET_LABEL = 33, "widget_label", Rk::P;
    WIDGET_BUTTON = 34, "widget_button", Rk::P;
    WIDGET_DUMMY = 35, "widget_dummy", Rk::P;
    WIDGET_IMAGE = 36, "widget_image", Rk::P;
    WIDGET_SLIDER = 37, "widget_slider", Rk::P;
    WIDGET_DESTROY = 38, "widget_destroy", Rk::None;
    WIDGET_POS = 39, "widget_pos", Rk::None;
    WIDGET_SIZE = 40, "widget_size", Rk::None;
    WIDGET_GET_SIZE = 41, "widget_get_size", Rk::F(2);
    WIDGET_GET_LOCAL_POS = 42, "widget_get_local_pos", Rk::F(3);
    WIDGET_VISIBLE = 43, "widget_visible", Rk::None;
    WIDGET_ENABLED = 44, "widget_enabled", Rk::None;
    WIDGET_IS_VISIBLE = 45, "widget_is_visible", Rk::B;
    WIDGET_TEXT = 46, "widget_text", Rk::None;
    WIDGET_FONT_COLOR = 47, "widget_font_color", Rk::None;
    WIDGET_FONT_SIZE = 48, "widget_font_size", Rk::None;
    WIDGET_ALIGN = 49, "widget_align", Rk::None;
    WIDGET_WRAP = 50, "widget_wrap", Rk::None;
    WIDGET_FONT_TYPE = 51, "widget_font_type", Rk::None;
    WIDGET_AUTOSIZE = 52, "widget_autosize", Rk::None;
    WIDGET_DEFAULT_FONT = 53, "widget_default_font", Rk::P;
    WIDGET_CALLBACK = 54, "widget_callback", Rk::None;
    SLIDER_MAX = 55, "slider_max", Rk::None;
    SLIDER_BAR = 56, "slider_bar", Rk::None;
    SLIDER_SET = 57, "slider_set", Rk::None;
    SLIDER_GET = 58, "slider_get", Rk::I;
    SLIDER_GET_MAX = 59, "slider_get_max", Rk::I;
    SET_DEFAULT_FOCUS_NAV = 60, "set_default_focus_nav", Rk::None;
    SET_FOCUSED = 61, "set_focused", Rk::None;
    MUSIC_PLAY = 62, "music_play", Rk::None;
    MUSIC_STOP = 63, "music_stop", Rk::None;
    MUSIC_PAUSE = 64, "music_pause", Rk::None;
    MUSIC_RESUME = 65, "music_resume", Rk::None;
    UPDATER_CONTAINER = 66, "updater_container", Rk::None;
    UPDATER_RESET_ALL = 67, "updater_reset_all", Rk::None;
    ENGINE_EXIT = 68, "engine_exit", Rk::None;
    INPUT_STATE = 69, "input_state", Rk::None;
    LOCK_INPUT = 70, "lock_input", Rk::None;
    RELATIVE_MOUSE = 71, "relative_mouse", Rk::None;
    GAMMA_GET = 72, "gamma_get", Rk::F(1);
    GAMMA_SET = 73, "gamma_set", Rk::None;
    BASE_FLAG = 74, "base_flag", Rk::B;
    PREMENU_PATH = 75, "premenu_path", Rk::Str;
    XML_OPEN = 76, "xml_open", Rk::P;
    XML_CLOSE = 77, "xml_close", Rk::None;
    XML_CHILD = 78, "xml_child", Rk::P;
    XML_CHILD_COUNT = 79, "xml_child_count", Rk::I;
    XML_STR = 80, "xml_str", Rk::Str;
    XML_FLOAT = 81, "xml_float", Rk::F(1);
    XML_BOOL = 82, "xml_bool", Rk::B;
    XML_COLOR = 83, "xml_color", Rk::F(4);
    XML_VEC2 = 84, "xml_vec2", Rk::F(2);
    XML_VEC3 = 85, "xml_vec3", Rk::F(3);
    BASE_CREATE_PROFILE = 86, "base_create_profile", Rk::None;
    BASE_SET_PROFILE = 87, "base_set_profile", Rk::None;
    BASE_INIT_USER_CONFIG = 88, "base_init_user_config", Rk::B;
    BASE_EXIT = 89, "base_exit", Rk::None;
    BASE_START_GAME = 90, "base_start_game", Rk::None;
    HELP_SET = 91, "help_set", Rk::P;
    DRAW_SET_TO_SCREEN = 92, "draw_set_to_screen", Rk::None;
    STRING_DURATION = 93, "string_duration", Rk::F(1);
    PLAY_GUI_SOUND = 94, "play_gui_sound", Rk::B;
    OPEN_BROWSER = 95, "open_browser", Rk::None;
    DATE = 96, "date", Rk::U(6);
    PROFILE_NAME = 97, "profile_name", Rk::Str;
    STATE_PATH = 98, "state_path", Rk::Str;
    LOG_ERROR = 99, "log_error", Rk::None;
    MAP_NAME = 100, "map_name", Rk::Str;
    PLAYER_VITALS = 101, "player_vitals", Rk::U(5);
    WIDGET_GLOBAL_UI = 102, "widget_global_ui", Rk::None;
}

pub const CFG_MENU: i64 = 0;
pub const CFG_DEMO: i64 = 1;
pub const CFG_USER: i64 = 2;

pub const FLAG_FULLSCREEN: i64 = 0;
pub const FLAG_FIRST_START: i64 = 1;
pub const FLAG_SHOW_MENU: i64 = 2;
pub const FLAG_PTEST: i64 = 3;
pub const FLAG_DEBUG_WINDOW: i64 = 4;
pub const FLAG_MAP_LOADED: i64 = 5;

pub const ALIGN_LEFT: i64 = 0;
pub const ALIGN_CENTER: i64 = 2;

/// `eLuxInputState` values the classes switch to.
pub const INPUT_NULL: i64 = 0;
pub const INPUT_GAME: i64 = 1;
pub const INPUT_PREMENU: i64 = 2;
pub const INPUT_CREDITS: i64 = 3;
pub const INPUT_DEMO_END: i64 = 4;
pub const INPUT_LOAD_SCREEN: i64 = 5;

pub const MAT_ALPHA: i64 = 1;

#[derive(Clone, Copy, Debug)]
pub enum Arg<'a> {
    P(usize),
    I(i64),
    F(f32),
    B(bool),
    S(&'a [u8]),
    W(&'a [u32]),
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct RawArg {
    pub v: u64,
    pub n: usize,
}

pub type StrSink = unsafe extern "C" fn(*mut c_void, *const u32, usize);

#[repr(C)]
pub struct RawCall {
    pub op: i32,
    pub na: u32,
    pub a: *const RawArg,
    pub u: [u64; 8],
    pub sink: StrSink,
    pub sink_ctx: *mut c_void,
}

pub type CallFn = unsafe extern "C" fn(*mut c_void, *mut RawCall);

#[repr(C)]
#[derive(Clone, Copy)]
pub struct HostVt {
    pub ctx: *mut c_void,
    pub call: CallFn,
}

#[derive(Default, Clone, Debug, PartialEq)]
pub struct Res {
    pub u: [u64; 8],
    pub s: Vec<Vec<u32>>,
}

impl Res {
    pub fn f(&self, i: usize) -> f32 {
        f32::from_bits(self.u.get(i).copied().unwrap_or(0) as u32)
    }

    pub fn p(&self) -> usize {
        self.u.first().copied().unwrap_or(0) as usize
    }

    pub fn i(&self) -> i64 {
        self.u.first().copied().unwrap_or(0) as i64
    }

    pub fn b(&self) -> bool {
        self.u.first().copied().unwrap_or(0) != 0
    }

    pub fn str0(&self) -> &[u32] {
        self.s.first().map_or(&[], Vec::as_slice)
    }
}

pub trait Host {
    fn call(&self, op: i32, args: &[Arg]) -> Res;
}

unsafe extern "C" fn collect(ctx: *mut c_void, p: *const u32, n: usize) {
    // SAFETY: `ctx` is the `Vec` handed to the call below and `p` holds `n` units.
    let v = unsafe { &mut *ctx.cast::<Vec<Vec<u32>>>() };
    let s = if n == 0 || p.is_null() {
        &[]
    } else {
        // SAFETY: the host passes `n` readable units.
        unsafe { std::slice::from_raw_parts(p, n) }
    };
    v.push(s.to_vec());
}

impl Host for HostVt {
    fn call(&self, op: i32, args: &[Arg]) -> Res {
        let raw: Vec<RawArg> = args
            .iter()
            .map(|a| match *a {
                Arg::P(p) => RawArg { v: p as u64, n: 0 },
                Arg::I(i) => RawArg { v: i as u64, n: 0 },
                Arg::F(f) => RawArg {
                    v: u64::from(f.to_bits()),
                    n: 0,
                },
                Arg::B(b) => RawArg {
                    v: u64::from(b),
                    n: 0,
                },
                Arg::S(s) => RawArg {
                    v: s.as_ptr() as u64,
                    n: s.len(),
                },
                Arg::W(s) => RawArg {
                    v: s.as_ptr() as u64,
                    n: s.len(),
                },
            })
            .collect();
        let mut strs: Vec<Vec<u32>> = Vec::new();
        let mut c = RawCall {
            op,
            na: u32::try_from(raw.len()).unwrap_or(0),
            a: raw.as_ptr(),
            u: [0; 8],
            sink: collect,
            sink_ctx: std::ptr::addr_of_mut!(strs).cast(),
        };
        // SAFETY: the host table is valid for the object's lifetime and reads `raw`, which
        // outlives the call. The call may re-enter Rust, so no state borrow is held by callers.
        unsafe { (self.call)(self.ctx, &mut c) };
        Res { u: c.u, s: strs }
    }
}

pub(super) fn rk(op: i32) -> (&'static str, Rk) {
    OPS.iter()
        .find(|o| o.0 == op)
        .map_or(("?", Rk::None), |o| (o.1, o.2))
}

/// Pointer ids by first appearance and the output sink. The same numbering is used by the C++
/// that was traced before the port, so the two dumps compare as text.
pub struct Tracer {
    inner: Mutex<TraceInner>,
}

struct TraceInner {
    ids: HashMap<usize, usize>,
    next: usize,
    sink: Box<dyn Write + Send>,
}

pub fn esc_bytes(units: impl Iterator<Item = u32>) -> String {
    let mut o = String::from("\"");
    for u in units {
        match u {
            0x20..=0x7e if u != 0x22 && u != 0x5c => o.push(char::from(u as u8)),
            0..=0xff => o.push_str(&format!("\\x{u:02x}")),
            _ => o.push_str(&format!("\\u{{{u:x}}}")),
        }
    }
    o.push('"');
    o
}

impl Tracer {
    pub fn new(sink: Box<dyn Write + Send>) -> Tracer {
        Tracer {
            inner: Mutex::new(TraceInner {
                ids: HashMap::new(),
                next: 1,
                sink,
            }),
        }
    }

    fn pid(t: &mut TraceInner, p: usize) -> String {
        if p == 0 {
            return "p0".into();
        }
        let n = *t.ids.entry(p).or_insert(t.next);
        if n == t.next {
            t.next += 1;
        }
        format!("p{n}")
    }

    fn arg(t: &mut TraceInner, a: &Arg) -> String {
        match *a {
            Arg::P(p) => Self::pid(t, p),
            Arg::I(i) => i.to_string(),
            Arg::F(f) => format!("f{:08x}", f.to_bits()),
            Arg::B(b) => u8::from(b).to_string(),
            Arg::S(s) => esc_bytes(s.iter().map(|b| u32::from(*b))),
            Arg::W(s) => esc_bytes(s.iter().copied()),
        }
    }

    pub fn event(&self, name: &str, args: &[Arg]) {
        let Ok(mut t) = self.inner.lock() else {
            return;
        };
        let mut line = format!("ev {name}");
        for a in args {
            line.push(' ');
            line.push_str(&Self::arg(&mut t, a));
        }
        let _ = writeln!(t.sink, "{line}");
    }

    pub fn call(&self, op: i32, args: &[Arg], r: &Res) {
        let Ok(mut t) = self.inner.lock() else {
            return;
        };
        let (name, kind) = rk(op);
        let mut line = String::from(name);
        for a in args {
            line.push(' ');
            line.push_str(&Self::arg(&mut t, a));
        }
        let f = |i: usize| format!("f{:08x}", r.u.get(i).copied().unwrap_or(0) as u32);
        let res = match kind {
            Rk::None => String::new(),
            Rk::P => Self::pid(&mut t, r.p()),
            Rk::I => r.i().to_string(),
            Rk::B => u8::from(r.b()).to_string(),
            Rk::F(n) => (0..n).map(f).collect::<Vec<_>>().join(","),
            Rk::U(n) => (0..n)
                .map(|i| r.u.get(i).copied().unwrap_or(0).to_string())
                .collect::<Vec<_>>()
                .join(","),
            Rk::Str => esc_bytes(r.str0().iter().copied()),
            Rk::Strs => {
                r.s.iter()
                    .map(|s| esc_bytes(s.iter().copied()))
                    .collect::<Vec<_>>()
                    .join(",")
            }
        };
        if kind != Rk::None {
            line.push_str(" -> ");
            line.push_str(&res);
        }
        if op == GFX_DESTROY
            && let Some(Arg::P(p)) = args.first()
        {
            t.ids.remove(p);
        }
        if op == WIDGET_DESTROY
            && let Some(Arg::P(p)) = args.get(1)
        {
            t.ids.remove(p);
        }
        let _ = writeln!(t.sink, "{line}");
    }
}

static GLOBAL: OnceLock<Option<Tracer>> = OnceLock::new();

pub fn global_tracer() -> Option<&'static Tracer> {
    GLOBAL
        .get_or_init(|| {
            let path = std::env::var_os("OHPL_A10B_DUMP")?;
            let f = std::fs::OpenOptions::new()
                .create(true)
                .append(true)
                .open(path)
                .ok()?;
            Some(Tracer::new(Box::new(f)))
        })
        .as_ref()
}

/// Typed calls over a [`Host`], tracing each one when a tracer is set.
pub struct Eng<'a> {
    pub h: &'a dyn Host,
    pub tr: Option<&'a Tracer>,
}

impl<'a> Eng<'a> {
    pub fn new(h: &'a dyn Host) -> Eng<'a> {
        Eng {
            h,
            tr: global_tracer(),
        }
    }

    /// A traced run uses one fixed step, so two dumps of the same session agree.
    pub fn clock(&self, dt: f32) -> f32 {
        if self.tr.is_some() { 1.0 / 60.0 } else { dt }
    }

    pub fn ev(&self, name: &str, args: &[Arg]) {
        if let Some(t) = self.tr {
            t.event(name, args);
        }
    }

    pub fn raw(&self, op: i32, args: &[Arg]) -> Res {
        let r = self.h.call(op, args);
        if let Some(t) = self.tr {
            t.call(op, args, &r);
        }
        r
    }

    fn p(&self, op: i32, args: &[Arg]) -> usize {
        self.raw(op, args).p()
    }

    fn v(&self, op: i32, args: &[Arg]) {
        self.raw(op, args);
    }

    pub fn create_set(&self, name: &[u8], skin: usize) -> usize {
        self.p(GUI_CREATE_SET, &[Arg::S(name), Arg::P(skin)])
    }

    pub fn create_skin(&self, file: &[u8]) -> usize {
        self.p(GUI_CREATE_SKIN, &[Arg::S(file)])
    }

    pub fn set_virtual_size(&self, set: usize, size: [f32; 2], offset: [f32; 2]) {
        self.v(
            SET_VIRTUAL_SIZE,
            &[
                Arg::P(set),
                Arg::F(size[0]),
                Arg::F(size[1]),
                Arg::F(-1000.0),
                Arg::F(1000.0),
                Arg::F(offset[0]),
                Arg::F(offset[1]),
                Arg::B(true),
            ],
        );
    }

    pub fn set_active(&self, set: usize, on: bool) {
        self.v(SET_ACTIVE, &[Arg::P(set), Arg::B(on)]);
    }

    pub fn set_draw_mouse(&self, set: usize, on: bool) {
        self.v(SET_DRAW_MOUSE, &[Arg::P(set), Arg::B(on)]);
    }

    pub fn set_skin(&self, set: usize, skin: usize) {
        self.v(SET_SKIN, &[Arg::P(set), Arg::P(skin)]);
    }

    pub fn gui_focus(&self, set: usize) {
        self.v(GUI_SET_FOCUS, &[Arg::P(set)]);
    }

    /// (gui set size, offset) of a virtual size on the current screen.
    pub fn calc_offset(&self, center: [f32; 2]) -> ([f32; 2], [f32; 2]) {
        let r = self.raw(CALC_OFFSET, &[Arg::F(center[0]), Arg::F(center[1])]);
        ([r.f(0), r.f(1)], [r.f(2), r.f(3)])
    }

    pub fn viewport_new(&self) -> usize {
        self.p(VIEWPORT_NEW, &[])
    }

    pub fn viewport_active(&self, vp: usize, on: bool) {
        self.v(VIEWPORT_ACTIVE, &[Arg::P(vp), Arg::B(on)]);
    }

    pub fn viewport_visible(&self, vp: usize, on: bool) {
        self.v(VIEWPORT_VISIBLE, &[Arg::P(vp), Arg::B(on)]);
    }

    pub fn viewport_add_set(&self, vp: usize, set: usize) {
        self.v(VIEWPORT_ADD_SET, &[Arg::P(vp), Arg::P(set)]);
    }

    pub fn gfx_rect(&self, c: [f32; 4], add_to_list: bool) -> usize {
        self.p(
            GFX_RECT,
            &[
                Arg::F(c[0]),
                Arg::F(c[1]),
                Arg::F(c[2]),
                Arg::F(c[3]),
                Arg::B(add_to_list),
            ],
        )
    }

    pub fn gfx_tex_file(&self, file: &[u8]) -> usize {
        self.p(GFX_TEX_FILE, &[Arg::S(file)])
    }

    pub fn tex_2d(&self, file: &[u8]) -> usize {
        self.p(TEX_2D, &[Arg::S(file)])
    }

    pub fn gfx_from_tex(&self, tex: usize) -> usize {
        self.p(GFX_FROM_TEX, &[Arg::P(tex)])
    }

    pub fn gfx_destroy(&self, gfx: usize) {
        self.v(GFX_DESTROY, &[Arg::P(gfx)]);
    }

    pub fn gfx_size(&self, gfx: usize) -> [f32; 2] {
        let r = self.raw(GFX_SIZE, &[Arg::P(gfx)]);
        [r.f(0), r.f(1)]
    }

    pub fn draw_gfx(&self, set: usize, gfx: usize, pos: [f32; 3], size: [f32; 2], c: [f32; 4]) {
        self.v(
            DRAW_GFX,
            &[
                Arg::P(set),
                Arg::P(gfx),
                Arg::F(pos[0]),
                Arg::F(pos[1]),
                Arg::F(pos[2]),
                Arg::F(size[0]),
                Arg::F(size[1]),
                Arg::F(c[0]),
                Arg::F(c[1]),
                Arg::F(c[2]),
                Arg::F(c[3]),
            ],
        );
    }

    pub fn draw_gfx_pos(&self, set: usize, gfx: usize, pos: [f32; 3]) {
        self.v(
            DRAW_GFX_POS,
            &[
                Arg::P(set),
                Arg::P(gfx),
                Arg::F(pos[0]),
                Arg::F(pos[1]),
                Arg::F(pos[2]),
            ],
        );
    }

    #[allow(clippy::too_many_arguments)]
    pub fn draw_font(
        &self,
        set: usize,
        text: &[u32],
        font: usize,
        pos: [f32; 3],
        size: [f32; 2],
        c: [f32; 4],
        align: i64,
    ) {
        self.v(
            DRAW_FONT,
            &[
                Arg::P(set),
                Arg::W(text),
                Arg::P(font),
                Arg::F(pos[0]),
                Arg::F(pos[1]),
                Arg::F(pos[2]),
                Arg::F(size[0]),
                Arg::F(size[1]),
                Arg::F(c[0]),
                Arg::F(c[1]),
                Arg::F(c[2]),
                Arg::F(c[3]),
                Arg::I(align),
            ],
        );
    }

    pub fn font_load(&self, file: &[u8]) -> usize {
        self.p(FONT_LOAD, &[Arg::S(file)])
    }

    pub fn font_wrap(
        &self,
        font: usize,
        len: f32,
        line_h: f32,
        size: [f32; 2],
        text: &[u32],
    ) -> Vec<Vec<u32>> {
        self.raw(
            FONT_WRAP,
            &[
                Arg::P(font),
                Arg::F(len),
                Arg::F(line_h),
                Arg::F(size[0]),
                Arg::F(size[1]),
                Arg::W(text),
            ],
        )
        .s
    }

    pub fn font_len(&self, font: usize, size: [f32; 2], text: &[u32]) -> f32 {
        self.raw(
            FONT_LEN,
            &[Arg::P(font), Arg::F(size[0]), Arg::F(size[1]), Arg::W(text)],
        )
        .f(0)
    }

    pub fn font_height(&self, font: usize) -> f32 {
        self.raw(FONT_HEIGHT, &[Arg::P(font)]).f(0)
    }

    pub fn translate(&self, cat: &[u8], entry: &[u8]) -> Vec<u32> {
        self.raw(TRANSLATE, &[Arg::S(cat), Arg::S(entry)])
            .str0()
            .to_vec()
    }

    pub fn cfg_string(&self, cfg: i64, sec: &[u8], key: &[u8], def: &[u8]) -> Vec<u8> {
        self.raw(
            CFG_STRING,
            &[Arg::I(cfg), Arg::S(sec), Arg::S(key), Arg::S(def)],
        )
        .str0()
        .iter()
        .map(|u| *u as u8)
        .collect()
    }

    pub fn cfg_float(&self, cfg: i64, sec: &[u8], key: &[u8], def: f32) -> f32 {
        self.raw(
            CFG_FLOAT,
            &[Arg::I(cfg), Arg::S(sec), Arg::S(key), Arg::F(def)],
        )
        .f(0)
    }

    pub fn cfg_vec2(&self, cfg: i64, sec: &[u8], key: &[u8], def: f32) -> [f32; 2] {
        let r = self.raw(
            CFG_VEC2,
            &[
                Arg::I(cfg),
                Arg::S(sec),
                Arg::S(key),
                Arg::F(def),
                Arg::F(def),
            ],
        );
        [r.f(0), r.f(1)]
    }

    pub fn cfg_color(&self, cfg: i64, sec: &[u8], key: &[u8], def: [f32; 4]) -> [f32; 4] {
        let r = self.raw(
            CFG_COLOR,
            &[
                Arg::I(cfg),
                Arg::S(sec),
                Arg::S(key),
                Arg::F(def[0]),
                Arg::F(def[1]),
                Arg::F(def[2]),
                Arg::F(def[3]),
            ],
        );
        [r.f(0), r.f(1), r.f(2), r.f(3)]
    }

    pub fn cfg_bool(&self, cfg: i64, sec: &[u8], key: &[u8], def: bool) -> bool {
        self.raw(
            CFG_BOOL,
            &[Arg::I(cfg), Arg::S(sec), Arg::S(key), Arg::B(def)],
        )
        .b()
    }

    pub fn cfg_set_bool(&self, cfg: i64, sec: &[u8], key: &[u8], v: bool) {
        self.v(
            CFG_SET_BOOL,
            &[Arg::I(cfg), Arg::S(sec), Arg::S(key), Arg::B(v)],
        );
    }

    pub fn widget_label(
        &self,
        set: usize,
        pos: [f32; 3],
        size: [f32; 2],
        text: &[u32],
        parent: usize,
    ) -> usize {
        self.p(
            WIDGET_LABEL,
            &[
                Arg::P(set),
                Arg::F(pos[0]),
                Arg::F(pos[1]),
                Arg::F(pos[2]),
                Arg::F(size[0]),
                Arg::F(size[1]),
                Arg::W(text),
                Arg::P(parent),
            ],
        )
    }

    pub fn widget_button(
        &self,
        set: usize,
        pos: [f32; 3],
        size: [f32; 2],
        text: &[u32],
        parent: usize,
    ) -> usize {
        self.p(
            WIDGET_BUTTON,
            &[
                Arg::P(set),
                Arg::F(pos[0]),
                Arg::F(pos[1]),
                Arg::F(pos[2]),
                Arg::F(size[0]),
                Arg::F(size[1]),
                Arg::W(text),
                Arg::P(parent),
            ],
        )
    }

    pub fn widget_dummy(&self, set: usize, pos: [f32; 3], parent: usize) -> usize {
        self.p(
            WIDGET_DUMMY,
            &[
                Arg::P(set),
                Arg::F(pos[0]),
                Arg::F(pos[1]),
                Arg::F(pos[2]),
                Arg::P(parent),
            ],
        )
    }

    #[allow(clippy::too_many_arguments)]
    pub fn widget_image(
        &self,
        set: usize,
        file: &[u8],
        pos: [f32; 3],
        size: [f32; 2],
        material: i64,
        animate: bool,
        parent: usize,
    ) -> usize {
        self.p(
            WIDGET_IMAGE,
            &[
                Arg::P(set),
                Arg::S(file),
                Arg::F(pos[0]),
                Arg::F(pos[1]),
                Arg::F(pos[2]),
                Arg::F(size[0]),
                Arg::F(size[1]),
                Arg::I(material),
                Arg::B(animate),
                Arg::P(parent),
            ],
        )
    }

    pub fn widget_slider(
        &self,
        set: usize,
        pos: [f32; 3],
        size: [f32; 2],
        max: i64,
        parent: usize,
    ) -> usize {
        self.p(
            WIDGET_SLIDER,
            &[
                Arg::P(set),
                Arg::I(0),
                Arg::F(pos[0]),
                Arg::F(pos[1]),
                Arg::F(pos[2]),
                Arg::F(size[0]),
                Arg::F(size[1]),
                Arg::I(max),
                Arg::P(parent),
            ],
        )
    }

    pub fn widget_destroy(&self, set: usize, w: usize) {
        self.v(WIDGET_DESTROY, &[Arg::P(set), Arg::P(w)]);
    }

    pub fn widget_pos(&self, w: usize, pos: [f32; 3]) {
        self.v(
            WIDGET_POS,
            &[Arg::P(w), Arg::F(pos[0]), Arg::F(pos[1]), Arg::F(pos[2])],
        );
    }

    pub fn widget_size(&self, w: usize, size: [f32; 2]) {
        self.v(WIDGET_SIZE, &[Arg::P(w), Arg::F(size[0]), Arg::F(size[1])]);
    }

    pub fn widget_get_size(&self, w: usize) -> [f32; 2] {
        let r = self.raw(WIDGET_GET_SIZE, &[Arg::P(w)]);
        [r.f(0), r.f(1)]
    }

    pub fn widget_get_local_pos(&self, w: usize) -> [f32; 3] {
        let r = self.raw(WIDGET_GET_LOCAL_POS, &[Arg::P(w)]);
        [r.f(0), r.f(1), r.f(2)]
    }

    pub fn widget_visible(&self, w: usize, on: bool) {
        self.v(WIDGET_VISIBLE, &[Arg::P(w), Arg::B(on)]);
    }

    pub fn widget_enabled(&self, w: usize, on: bool) {
        self.v(WIDGET_ENABLED, &[Arg::P(w), Arg::B(on)]);
    }

    pub fn widget_text(&self, w: usize, text: &[u32]) {
        self.v(WIDGET_TEXT, &[Arg::P(w), Arg::W(text)]);
    }

    pub fn widget_font_color(&self, w: usize, c: [f32; 4]) {
        self.v(
            WIDGET_FONT_COLOR,
            &[
                Arg::P(w),
                Arg::F(c[0]),
                Arg::F(c[1]),
                Arg::F(c[2]),
                Arg::F(c[3]),
            ],
        );
    }

    pub fn widget_font_size(&self, w: usize, s: [f32; 2]) {
        self.v(WIDGET_FONT_SIZE, &[Arg::P(w), Arg::F(s[0]), Arg::F(s[1])]);
    }

    pub fn widget_align(&self, w: usize, align: i64) {
        self.v(WIDGET_ALIGN, &[Arg::P(w), Arg::I(align)]);
    }

    pub fn widget_wrap(&self, w: usize, on: bool) {
        self.v(WIDGET_WRAP, &[Arg::P(w), Arg::B(on)]);
    }

    pub fn widget_font_type(&self, w: usize, font: usize) {
        self.v(WIDGET_FONT_TYPE, &[Arg::P(w), Arg::P(font)]);
    }

    pub fn widget_autosize(&self, w: usize, on: bool) {
        self.v(WIDGET_AUTOSIZE, &[Arg::P(w), Arg::B(on)]);
    }

    pub fn widget_default_font(&self, w: usize) -> usize {
        self.p(WIDGET_DEFAULT_FONT, &[Arg::P(w)])
    }

    pub fn widget_callback(&self, w: usize, msg: i64, tag: i64) {
        self.v(WIDGET_CALLBACK, &[Arg::P(w), Arg::I(msg), Arg::I(tag)]);
    }

    pub fn widget_global_ui_listener(&self, w: usize, on: bool) {
        self.v(WIDGET_GLOBAL_UI, &[Arg::P(w), Arg::B(on)]);
    }

    pub fn slider_max(&self, w: usize, v: i64) {
        self.v(SLIDER_MAX, &[Arg::P(w), Arg::I(v)]);
    }

    pub fn slider_bar(&self, w: usize, v: i64) {
        self.v(SLIDER_BAR, &[Arg::P(w), Arg::I(v)]);
    }

    pub fn slider_set(&self, w: usize, v: i64, callback: bool) {
        self.v(SLIDER_SET, &[Arg::P(w), Arg::I(v), Arg::B(callback)]);
    }

    pub fn slider_get(&self, w: usize) -> i64 {
        self.raw(SLIDER_GET, &[Arg::P(w)]).i()
    }

    pub fn slider_get_max(&self, w: usize) -> i64 {
        self.raw(SLIDER_GET_MAX, &[Arg::P(w)]).i()
    }

    pub fn set_default_focus_nav(&self, set: usize, w: usize) {
        self.v(SET_DEFAULT_FOCUS_NAV, &[Arg::P(set), Arg::P(w)]);
    }

    pub fn set_focused(&self, set: usize, w: usize) {
        self.v(SET_FOCUSED, &[Arg::P(set), Arg::P(w)]);
    }

    pub fn music_play(&self, name: &[u8], vol: f32, fade: f32, looped: bool) {
        self.v(
            MUSIC_PLAY,
            &[
                Arg::S(name),
                Arg::F(vol),
                Arg::F(fade),
                Arg::B(looped),
                Arg::B(false),
            ],
        );
    }

    pub fn music_stop(&self, fade: f32) {
        self.v(MUSIC_STOP, &[Arg::F(fade)]);
    }

    pub fn music_pause(&self) {
        self.v(MUSIC_PAUSE, &[]);
    }

    pub fn music_resume(&self) {
        self.v(MUSIC_RESUME, &[]);
    }

    pub fn updater_container(&self, name: &[u8]) {
        self.v(UPDATER_CONTAINER, &[Arg::S(name)]);
    }

    pub fn updater_reset_all(&self) {
        self.v(UPDATER_RESET_ALL, &[]);
    }

    pub fn engine_exit(&self) {
        self.v(ENGINE_EXIT, &[]);
    }

    pub fn input_state(&self, state: i64) {
        self.v(INPUT_STATE, &[Arg::I(state)]);
    }

    pub fn lock_input(&self, on: bool) {
        self.v(LOCK_INPUT, &[Arg::B(on)]);
    }

    pub fn relative_mouse(&self, on: bool) {
        self.v(RELATIVE_MOUSE, &[Arg::B(on)]);
    }

    pub fn gamma_get(&self) -> f32 {
        self.raw(GAMMA_GET, &[]).f(0)
    }

    pub fn gamma_set(&self, g: f32) {
        self.v(GAMMA_SET, &[Arg::F(g)]);
    }

    pub fn base_flag(&self, id: i64) -> bool {
        self.raw(BASE_FLAG, &[Arg::I(id)]).b()
    }

    pub fn premenu_path(&self) -> Vec<u32> {
        self.raw(PREMENU_PATH, &[]).str0().to_vec()
    }

    pub fn xml_open(&self, path: &[u32]) -> usize {
        self.p(XML_OPEN, &[Arg::W(path)])
    }

    pub fn xml_close(&self, doc: usize) {
        self.v(XML_CLOSE, &[Arg::P(doc)]);
    }

    pub fn xml_child(&self, el: usize, i: i64) -> usize {
        self.p(XML_CHILD, &[Arg::P(el), Arg::I(i)])
    }

    pub fn xml_child_count(&self, el: usize) -> i64 {
        self.raw(XML_CHILD_COUNT, &[Arg::P(el)]).i()
    }

    pub fn xml_str(&self, el: usize, name: &[u8], def: &[u8]) -> Vec<u8> {
        self.raw(XML_STR, &[Arg::P(el), Arg::S(name), Arg::S(def)])
            .str0()
            .iter()
            .map(|u| *u as u8)
            .collect()
    }

    pub fn xml_float(&self, el: usize, name: &[u8], def: f32) -> f32 {
        self.raw(XML_FLOAT, &[Arg::P(el), Arg::S(name), Arg::F(def)])
            .f(0)
    }

    pub fn xml_bool(&self, el: usize, name: &[u8], def: bool) -> bool {
        self.raw(XML_BOOL, &[Arg::P(el), Arg::S(name), Arg::B(def)])
            .b()
    }

    pub fn xml_color(&self, el: usize, name: &[u8], def: [f32; 4]) -> [f32; 4] {
        let r = self.raw(
            XML_COLOR,
            &[
                Arg::P(el),
                Arg::S(name),
                Arg::F(def[0]),
                Arg::F(def[1]),
                Arg::F(def[2]),
                Arg::F(def[3]),
            ],
        );
        [r.f(0), r.f(1), r.f(2), r.f(3)]
    }

    pub fn xml_vec2(&self, el: usize, name: &[u8], def: f32) -> [f32; 2] {
        let r = self.raw(
            XML_VEC2,
            &[Arg::P(el), Arg::S(name), Arg::F(def), Arg::F(def)],
        );
        [r.f(0), r.f(1)]
    }

    pub fn xml_vec3(&self, el: usize, name: &[u8]) -> [f32; 3] {
        let r = self.raw(XML_VEC3, &[Arg::P(el), Arg::S(name)]);
        [r.f(0), r.f(1), r.f(2)]
    }

    pub fn base_create_profile(&self) {
        self.v(BASE_CREATE_PROFILE, &[]);
    }

    pub fn base_set_profile(&self) {
        self.v(BASE_SET_PROFILE, &[]);
    }

    pub fn base_init_user_config(&self) -> bool {
        self.raw(BASE_INIT_USER_CONFIG, &[]).b()
    }

    pub fn base_exit(&self) {
        self.v(BASE_EXIT, &[]);
    }

    pub fn base_start_game(&self) {
        self.v(BASE_START_GAME, &[]);
    }

    pub fn help_set(&self) -> usize {
        self.p(HELP_SET, &[])
    }

    pub fn draw_set_to_screen(&self) {
        self.v(DRAW_SET_TO_SCREEN, &[]);
    }

    pub fn string_duration(&self, text: &[u32]) -> f32 {
        self.raw(STRING_DURATION, &[Arg::W(text)]).f(0)
    }

    pub fn play_gui_sound(&self, name: &[u8], kind: i64) -> bool {
        self.raw(PLAY_GUI_SOUND, &[Arg::S(name), Arg::I(kind)]).b()
    }

    pub fn open_browser(&self, url: &[u8]) {
        self.v(OPEN_BROWSER, &[Arg::S(url)]);
    }

    pub fn date(&self) -> Res {
        self.raw(DATE, &[])
    }

    pub fn profile_name(&self) -> Vec<u32> {
        self.raw(PROFILE_NAME, &[]).str0().to_vec()
    }

    pub fn state_path(&self) -> Vec<u32> {
        self.raw(STATE_PATH, &[]).str0().to_vec()
    }

    pub fn log_error(&self, msg: &[u8]) {
        self.v(LOG_ERROR, &[Arg::S(msg)]);
    }

    pub fn map_name(&self) -> Vec<u8> {
        self.raw(MAP_NAME, &[])
            .str0()
            .iter()
            .map(|u| *u as u8)
            .collect()
    }

    pub fn player_vitals(&self) -> Res {
        self.raw(PLAYER_VITALS, &[])
    }
}
