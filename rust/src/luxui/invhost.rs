//! Host call table of the inventory (A11a): one `op` entry the C++ host (`LuxInventoryHost.cpp`) implements for
//! every engine object the inventory still reaches (GUI set and widgets, gfx elements, fonts, low-level graphics,
//! scripts, player, journal, hints, map, sound, config). Calls may re-enter Rust, so no reference to inventory
//! state is held across [`call`].

use super::event::Str;
use crate::luxmap::mhost::slice_of;
use std::cell::Cell;
use std::sync::atomic::{AtomicUsize, Ordering};

#[repr(C)]
#[derive(Clone, Copy)]
pub struct WStr {
    pub p: *const u32,
    pub n: usize,
}

impl WStr {
    pub const NONE: WStr = WStr {
        p: std::ptr::null(),
        n: 0,
    };

    pub fn of(s: &[u32]) -> WStr {
        WStr {
            p: s.as_ptr(),
            n: s.len(),
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct A {
    pub p: [usize; 4],
    pub i: [i32; 4],
    pub f: [f32; 12],
    pub d: f64,
    pub s: [Str; 2],
    pub w: [WStr; 2],
    pub r: usize,
    pub rs: Str,
    pub rw: WStr,
}

impl A {
    pub fn new() -> A {
        A {
            p: [0; 4],
            i: [0; 4],
            f: [0.0; 12],
            d: 0.0,
            s: [Str::NONE; 2],
            w: [WStr::NONE; 2],
            r: 0,
            rs: Str::NONE,
            rw: WStr::NONE,
        }
    }

    pub fn ret_bytes(&self) -> Vec<u8> {
        slice_of(self.rs.p, self.rs.n).to_vec()
    }

    pub fn ret_wide(&self) -> Vec<u32> {
        slice_of(self.rw.p, self.rw.n).to_vec()
    }
}

impl Default for A {
    fn default() -> A {
        A::new()
    }
}

pub type OpFn = unsafe extern "C" fn(i32, *mut A);

static HOST: AtomicUsize = AtomicUsize::new(0);

pub fn set_host(f: Option<OpFn>) {
    HOST.store(f.map_or(0, |f| f as usize), Ordering::Release);
}

thread_local! {
    static FLAVOR: Cell<i32> = const { Cell::new(0) };
}

/// Added to every op while the journal runs: the host then targets the journal's GUI set, config section and fonts.
pub const JOURNAL_BASE: i32 = 100;

pub struct JournalScope(i32);

/// Routes host ops to the journal until the guard drops; nests with the inventory's own calls.
pub fn journal_scope() -> JournalScope {
    JournalScope(FLAVOR.replace(JOURNAL_BASE))
}

impl Drop for JournalScope {
    fn drop(&mut self) {
        FLAVOR.set(self.0);
    }
}

pub fn call(op: i32, a: &mut A) {
    let op = op + FLAVOR.get();
    let h = HOST.load(Ordering::Acquire);
    if h == 0 {
        return;
    }
    if super::inv_direct::call(op, a) {
        return;
    }
    // SAFETY: `h` is the function pointer stored by `set_host`.
    let f = unsafe { std::mem::transmute::<usize, OpFn>(h) };
    // SAFETY: `a` is a live argument block.
    unsafe { f(op, a) };
    super::inv_direct::after(op, a);
}

macro_rules! consts {
    ($m:ident, { $($n:ident = $v:expr),* $(,)? }) => {
        pub mod $m {
            $(pub const $n: i32 = $v;)*
            pub const TABLE: &[(&str, i32)] = &[$((stringify!($n), $v)),*];
        }
    };
}

consts!(op, {
    CFG = 1,
    TRANSLATE = 2,
    PARSE_STRING = 3,
    STRING_DURATION = 4,
    GUI_INIT = 5,
    SET_OP = 6,
    SET_QUERY = 7,
    GFX_NEW = 8,
    GFX_OP = 9,
    DRAW_GFX = 10,
    DRAW_FONT = 11,
    FONT_LOAD = 12,
    FONT_WRAP = 13,
    FONT_ROW = 14,
    WIDGET_NEW = 15,
    WIDGET_CB = 16,
    WIDGET_GET = 17,
    WIDGET_SET = 18,
    LL = 19,
    SCRIPT = 20,
    GAME = 21,
    ITEM_EVENT = 22,
    LOG = 24,
    WIDGET_NEW2 = 25,
    WIDGET_X = 26,
    JGAME = 27,
});

consts!(cfg, {
    FLOAT = 0,
    STRING = 1,
    V3 = 2,
    V2 = 3,
    COLOR = 4,
    INT = 5,
    BOOL = 6,
});

consts!(set, {
    ACTIVE = 0,
    DRAW_MOUSE = 1,
    MOUSE_MOVEMENT = 2,
    DRAW_FOCUS = 3,
    RESET_MOUSE_OVER = 4,
    GUI_FOCUS = 5,
    FOCUSED_WIDGET = 6,
    DEFAULT_NAV = 7,
    VIEWPORT = 8,
    GAMEPAD_PRESENT = 9,
    MOUSE_POS = 10,
    MOUSE_Z = 11,
    GET_FOCUSED = 12,
    POINTER_GFX = 13,
    CLIP_CURRENT = 14,
    CLIP_SET = 15,
    CLIP_CHILD = 16,
});

consts!(gfx, {
    IMAGE = 0,
    RECT = 1,
    TEXTURE = 2,
    FILE = 3,
    DESTROY = 0,
    SIZES = 1,
    COLOR = 2,
    ACTIVE_SIZE = 3,
});

consts!(wd, {
    IMAGE = 0,
    DUMMY = 1,
    GET_SIZE = 0,
    GET_GLOBAL = 1,
    GET_USER_VALUE = 2,
    GET_COLOR_MUL = 3,
    GET_IMAGE = 4,
    SET_POSITION = 0,
    SET_SIZE = 1,
    SET_IMAGE = 2,
    SET_USER_VALUE = 3,
    SET_COLOR_MUL = 4,
    SET_GLOBAL_UI = 5,
    SET_NAV = 6,
    NAV_FOCUS = 7,
});

consts!(ll, {
    SCREEN_SIZE_INT = 0,
    TEXTURE_NEW = 1,
    TEXTURE_DESTROY = 2,
    FB_NEW = 3,
    FB_DESTROY = 4,
    BG_SCREEN = 5,
    BLEND = 6,
    DEPTH_TEST = 7,
    DEPTH_WRITE = 8,
    ORTHO = 9,
    IDENTITY = 10,
    COPY_SCREEN = 11,
    PROGRAM = 12,
    SET_FB = 13,
    SET_TEXTURE = 14,
    DRAW_QUAD = 15,
    FLUSH = 16,
    PROGRAM_NEW = 17,
});

consts!(game, {
    SANITY = 0,
    HEALTH = 1,
    LAMP_OIL = 2,
    TINDERBOXES = 3,
    COINS = 4,
    STATE_USE_ITEM = 5,
    LANTERN_ACTIVE = 6,
    TRIGGERED = 7,
    GAME_TIME = 8,
    SET_CONTAINER = 9,
    INPUT_STATE = 10,
    HINT_UPDATE = 12,
    HINT_DRAW = 13,
    FLASH_UPDATE = 14,
    FLASH_DRAW = 15,
    LEAVE_USE_ITEM = 16,
    RESUME_SOUNDS = 17,
    FADE_LOOPS = 18,
    STOP_TERROR = 19,
    VOICES = 20,
    TINDER_GET = 21,
    TINDER_SET = 22,
    ACHIEVEMENT = 23,
    SET_MESSAGE = 24,
    MAP_FOLDER = 25,
});

consts!(ev, {
    PROG_LOG = 1,
    SET_COINS = 2,
    HINT = 3,
    SET_CONTAINER_JOURNAL = 4,
    RUN_MAP_SCRIPT = 7,
    LANTERN_TOGGLE = 10,
    SET_HEALTH = 11,
    ADD_SANITY = 12,
    SET_LAMP_OIL = 13,
    SOUND = 14,
    USE_ITEM_STATE_RESET = 15,
    USE_ITEM_ENTER = 16,
    ADD_TINDERBOX = 17,
    COMPLETION = 18,
});

consts!(wn, {
    LABEL = 0,
    IMAGE = 1,
    DUMMY = 2,
    FRAME = 3,
});

consts!(wx, {
    SET_VISIBLE = 0,
    SET_ENABLED = 1,
    SETUP_LABEL = 2,
    IS_VISIBLE = 3,
    HOVER_OR_FOCUS = 4,
    GET_TEXT = 5,
    GET_FONT_SIZE = 6,
    GET_FONT = 7,
    GET_ALIGN = 8,
    DESTROY = 9,
    FONT_LENGTH = 10,
});

consts!(jg, {
    PROG_LOW = 3,
    HINT_RECENT = 4,
    MAIN_BOOL_GET = 5,
    MAIN_BOOL_SAVE = 6,
    ACH_ARCHIVIST = 7,
    GAME_NAME = 8,
    DPAD_DOWN_OK = 9,
});

/// Group name as spelled in `LuxInventoryHost.cpp` (`kA11a_<group>_<name>`) with its constants.
pub const TABLES: &[(&str, &[(&str, i32)])] = &[
    ("Op", op::TABLE),
    ("Cfg", cfg::TABLE),
    ("Set", set::TABLE),
    ("Gfx", gfx::TABLE),
    ("Wd", wd::TABLE),
    ("Ll", ll::TABLE),
    ("Game", game::TABLE),
    ("Ev", ev::TABLE),
    ("Wn", wn::TABLE),
    ("Wx", wx::TABLE),
    ("Jg", jg::TABLE),
];

fn s(a: &mut A, idx: usize, v: &[u8]) {
    if let Some(slot) = a.s.get_mut(idx) {
        *slot = Str::of(v);
    }
}

pub fn cfg_float(key: &[u8], def: f32) -> f32 {
    let mut a = A::new();
    a.i[0] = cfg::FLOAT;
    s(&mut a, 0, key);
    a.f[0] = def;
    call(op::CFG, &mut a);
    a.f[0]
}

pub fn cfg_int(key: &[u8], def: i32) -> i32 {
    let mut a = A::new();
    a.i[0] = cfg::INT;
    s(&mut a, 0, key);
    a.i[1] = def;
    call(op::CFG, &mut a);
    a.i[1]
}

pub fn cfg_bool(key: &[u8], def: bool) -> bool {
    let mut a = A::new();
    a.i[0] = cfg::BOOL;
    s(&mut a, 0, key);
    a.i[1] = i32::from(def);
    call(op::CFG, &mut a);
    a.i[1] != 0
}

pub fn cfg_string(key: &[u8], def: &[u8]) -> Vec<u8> {
    let mut a = A::new();
    a.i[0] = cfg::STRING;
    s(&mut a, 0, key);
    s(&mut a, 1, def);
    call(op::CFG, &mut a);
    a.ret_bytes()
}

pub fn cfg_v3(key: &[u8], def: [f32; 3]) -> [f32; 3] {
    let mut a = A::new();
    a.i[0] = cfg::V3;
    s(&mut a, 0, key);
    a.f[..3].copy_from_slice(&def);
    call(op::CFG, &mut a);
    [a.f[0], a.f[1], a.f[2]]
}

pub fn cfg_v2(key: &[u8], def: [f32; 2]) -> [f32; 2] {
    let mut a = A::new();
    a.i[0] = cfg::V2;
    s(&mut a, 0, key);
    a.f[..2].copy_from_slice(&def);
    call(op::CFG, &mut a);
    [a.f[0], a.f[1]]
}

pub fn cfg_color(key: &[u8], def: [f32; 4]) -> [f32; 4] {
    let mut a = A::new();
    a.i[0] = cfg::COLOR;
    s(&mut a, 0, key);
    a.f[..4].copy_from_slice(&def);
    call(op::CFG, &mut a);
    [a.f[0], a.f[1], a.f[2], a.f[3]]
}

pub fn translate(cat: &[u8], key: &[u8]) -> Vec<u32> {
    let mut a = A::new();
    s(&mut a, 0, cat);
    s(&mut a, 1, key);
    call(op::TRANSLATE, &mut a);
    a.ret_wide()
}

pub fn parse_string(text: &[u32]) -> Vec<u32> {
    let mut a = A::new();
    a.w[0] = WStr::of(text);
    call(op::PARSE_STRING, &mut a);
    a.ret_wide()
}

pub fn string_duration(text: &[u32]) -> f32 {
    let mut a = A::new();
    a.w[0] = WStr::of(text);
    call(op::STRING_DURATION, &mut a);
    a.f[0]
}

pub fn gfx_image(name: &[u8], material: i32) -> usize {
    let mut a = A::new();
    a.i[0] = gfx::IMAGE;
    a.i[1] = material;
    s(&mut a, 0, name);
    call(op::GFX_NEW, &mut a);
    a.r
}

pub fn gfx_rect(color: [f32; 4], material: i32) -> usize {
    let mut a = A::new();
    a.i[0] = gfx::RECT;
    a.i[1] = material;
    a.f[..4].copy_from_slice(&color);
    call(op::GFX_NEW, &mut a);
    a.r
}

pub fn gfx_texture(tex: usize, material: i32) -> usize {
    let mut a = A::new();
    a.i[0] = gfx::TEXTURE;
    a.i[1] = material;
    a.p[0] = tex;
    call(op::GFX_NEW, &mut a);
    a.r
}

pub fn gfx_destroy(g: usize) {
    let mut a = A::new();
    a.i[0] = gfx::DESTROY;
    a.p[0] = g;
    call(op::GFX_OP, &mut a);
}

/// Active size and image size of a gfx element.
pub fn gfx_sizes(g: usize) -> ([f32; 2], [f32; 2]) {
    let mut a = A::new();
    a.i[0] = gfx::SIZES;
    a.p[0] = g;
    call(op::GFX_OP, &mut a);
    ([a.f[0], a.f[1]], [a.f[2], a.f[3]])
}

pub fn gfx_set_color(g: usize, c: [f32; 4]) {
    let mut a = A::new();
    a.i[0] = gfx::COLOR;
    a.p[0] = g;
    a.f[..4].copy_from_slice(&c);
    call(op::GFX_OP, &mut a);
}

pub fn gfx_set_active_size(g: usize, sz: [f32; 2]) {
    let mut a = A::new();
    a.i[0] = gfx::ACTIVE_SIZE;
    a.p[0] = g;
    a.f[..2].copy_from_slice(&sz);
    call(op::GFX_OP, &mut a);
}

/// `material` -1 keeps the set default; `size` is `[-1, -1]` for the image size.
pub fn draw_gfx(g: usize, pos: [f32; 3], size: [f32; 2], color: [f32; 4], material: i32) {
    let mut a = A::new();
    a.p[0] = g;
    a.f[..3].copy_from_slice(&pos);
    a.f[3..5].copy_from_slice(&size);
    a.f[5..9].copy_from_slice(&color);
    a.i[0] = material;
    call(op::DRAW_GFX, &mut a);
}

pub fn draw_font(
    font: usize,
    text: &[u32],
    pos: [f32; 3],
    size: [f32; 2],
    color: [f32; 4],
    align: i32,
) {
    let mut a = A::new();
    a.p[0] = font;
    a.w[0] = WStr::of(text);
    a.f[..3].copy_from_slice(&pos);
    a.f[3..5].copy_from_slice(&size);
    a.f[5..9].copy_from_slice(&color);
    a.i[0] = align;
    call(op::DRAW_FONT, &mut a);
}

pub fn set_flag(kind: i32, v: i32) {
    let mut a = A::new();
    a.i[0] = kind;
    a.i[1] = v;
    call(op::SET_OP, &mut a);
}

pub fn set_widget(kind: i32, w: usize) {
    let mut a = A::new();
    a.i[0] = kind;
    a.p[0] = w;
    call(op::SET_OP, &mut a);
}

pub fn set_query(kind: i32) -> A {
    let mut a = A::new();
    a.i[0] = kind;
    call(op::SET_OP, &mut a);
    a
}

pub fn widget_new_image(file: &[u8], pos: [f32; 3], parent: usize) -> usize {
    let mut a = A::new();
    a.i[0] = wd::IMAGE;
    s(&mut a, 0, file);
    a.f[..3].copy_from_slice(&pos);
    a.p[0] = parent;
    call(op::WIDGET_NEW, &mut a);
    a.r
}

pub fn widget_new_dummy(pos: [f32; 3]) -> usize {
    let mut a = A::new();
    a.i[0] = wd::DUMMY;
    a.f[..3].copy_from_slice(&pos);
    call(op::WIDGET_NEW, &mut a);
    a.r
}

pub fn widget_callback(w: usize, msg: i32, token: usize) {
    let mut a = A::new();
    a.p[0] = w;
    a.p[1] = token;
    a.i[0] = msg;
    call(op::WIDGET_CB, &mut a);
}

pub fn widget_get(w: usize, what: i32) -> A {
    let mut a = A::new();
    a.p[0] = w;
    a.i[0] = what;
    call(op::WIDGET_GET, &mut a);
    a
}

pub fn widget_set(w: usize, what: i32, f: &[f32], i1: i32, p1: usize) {
    let mut a = A::new();
    a.p[0] = w;
    a.p[1] = p1;
    a.i[0] = what;
    a.i[1] = i1;
    if let Some(d) = a.f.get_mut(..f.len()) {
        d.copy_from_slice(f);
    }
    call(op::WIDGET_SET, &mut a);
}

pub fn game(sub: i32) -> A {
    let mut a = A::new();
    a.i[0] = sub;
    call(op::GAME, &mut a);
    a
}

pub fn game_i(sub: i32, v: i32) -> A {
    let mut a = A::new();
    a.i[0] = sub;
    a.i[1] = v;
    call(op::GAME, &mut a);
    a
}

pub fn game_f(sub: i32, v: f32) -> A {
    let mut a = A::new();
    a.i[0] = sub;
    a.f[0] = v;
    call(op::GAME, &mut a);
    a
}

pub fn game_s(sub: i32, v: &[u8]) -> A {
    let mut a = A::new();
    a.i[0] = sub;
    s(&mut a, 0, v);
    call(op::GAME, &mut a);
    a
}

pub fn game_w(sub: i32, v: &[u32]) -> A {
    let mut a = A::new();
    a.i[0] = sub;
    a.w[0] = WStr::of(v);
    call(op::GAME, &mut a);
    a
}

pub fn ll(sub: i32, a: &mut A) {
    a.i[0] = sub;
    call(op::LL, a);
}

pub fn log(kind: i32, text: &[u8]) {
    let mut a = A::new();
    a.i[0] = kind;
    s(&mut a, 0, text);
    call(op::LOG, &mut a);
}

pub fn gfx_file(name: &[u8]) -> usize {
    let mut a = A::new();
    a.i[0] = gfx::FILE;
    s(&mut a, 0, name);
    call(op::GFX_NEW, &mut a);
    a.r
}

fn wnew(
    kind: i32,
    pos: [f32; 3],
    size: [f32; 2],
    parent: usize,
    file: &[u8],
    text: &[u32],
) -> usize {
    let mut a = A::new();
    a.i[0] = kind;
    a.f[..3].copy_from_slice(&pos);
    a.f[3..5].copy_from_slice(&size);
    a.p[0] = parent;
    s(&mut a, 0, file);
    a.w[0] = WStr::of(text);
    call(op::WIDGET_NEW2, &mut a);
    a.r
}

pub fn widget_label(pos: [f32; 3], size: [f32; 2], text: &[u32], parent: usize) -> usize {
    wnew(wn::LABEL, pos, size, parent, b"", text)
}

pub fn widget_image(file: &[u8], pos: [f32; 3], size: [f32; 2], parent: usize) -> usize {
    wnew(wn::IMAGE, pos, size, parent, file, &[])
}

pub fn widget_dummy(pos: [f32; 3], parent: usize) -> usize {
    wnew(wn::DUMMY, pos, [0.0; 2], parent, b"", &[])
}

pub fn widget_frame(pos: [f32; 3], size: [f32; 2], parent: usize) -> usize {
    wnew(wn::FRAME, pos, size, parent, b"", &[])
}

pub fn wx(sub: i32, w: usize) -> A {
    let mut a = A::new();
    a.i[0] = sub;
    a.p[0] = w;
    call(op::WIDGET_X, &mut a);
    a
}

pub fn widget_visible(w: usize, on: bool) {
    let mut a = A::new();
    a.i[0] = wx::SET_VISIBLE;
    a.i[1] = i32::from(on);
    a.p[0] = w;
    call(op::WIDGET_X, &mut a);
}

pub fn widget_enabled(w: usize, on: bool) {
    let mut a = A::new();
    a.i[0] = wx::SET_ENABLED;
    a.i[1] = i32::from(on);
    a.p[0] = w;
    call(op::WIDGET_X, &mut a);
}

pub fn jgame(sub: i32) -> A {
    let mut a = A::new();
    a.i[0] = sub;
    call(op::JGAME, &mut a);
    a
}

/// Renders the scene again without GUI into `bg_tex` through the screen effect and keeps a full copy in `screen_tex`.
pub fn render_background(screen_tex: usize, bg_tex: usize, size: [f32; 2]) {
    let step = |sub: i32, f: &dyn Fn(&mut A)| {
        let mut a = A::new();
        f(&mut a);
        ll(sub, &mut a);
        a.r
    };
    let fb = step(ll::FB_NEW, &|a| a.p[0] = bg_tex);
    step(ll::BG_SCREEN, &|a| a.i[1] = 0);
    step(ll::BLEND, &|a| a.i[1] = 0);
    step(ll::DEPTH_TEST, &|a| a.i[1] = 0);
    step(ll::DEPTH_WRITE, &|a| a.i[1] = 0);
    step(ll::ORTHO, &|a| a.f[..2].copy_from_slice(&size));
    step(ll::IDENTITY, &|_| {});
    step(ll::COPY_SCREEN, &|a| a.p[0] = screen_tex);
    step(ll::PROGRAM, &|a| a.i[1] = 1);
    step(ll::SET_FB, &|a| a.p[0] = fb);
    step(ll::SET_TEXTURE, &|a| a.p[0] = screen_tex);
    step(ll::DRAW_QUAD, &|a| a.f[..2].copy_from_slice(&size));
    step(ll::PROGRAM, &|a| a.i[1] = 0);
    step(ll::SET_FB, &|a| a.p[0] = 0);
    step(ll::SET_TEXTURE, &|a| a.p[0] = 0);
    step(ll::BG_SCREEN, &|a| a.i[1] = 1);
    step(ll::COPY_SCREEN, &|a| a.p[0] = screen_tex);
    step(ll::SET_TEXTURE, &|a| a.p[0] = 0);
    step(ll::SET_FB, &|a| a.p[0] = 0);
    step(ll::DEPTH_TEST, &|a| a.i[1] = 1);
    step(ll::FLUSH, &|_| {});
    step(ll::FB_DESTROY, &|a| a.p[0] = fb);
}

pub fn texture_new(name: &[u8], clamp: bool, w: i32, h: i32) -> usize {
    let mut t = A::new();
    t.s[0] = Str::of(name);
    t.i[1] = i32::from(clamp);
    t.i[2] = w;
    t.i[3] = h;
    ll(ll::TEXTURE_NEW, &mut t);
    t.r
}

pub fn texture_destroy(tex: usize) {
    let mut a = A::new();
    a.p[0] = tex;
    ll(ll::TEXTURE_DESTROY, &mut a);
}
