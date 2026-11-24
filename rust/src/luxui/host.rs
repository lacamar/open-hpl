//! Engine services the Rust hint and message handlers reach: configs, GUI gfx and fonts, the game
//! HUD set, sound, the map handler and the yes/no callback. C++ fills one [`HostTable`]
//! (`amnesia/src/game/RustLuxUiA11b.h`); tests use a recording fake. No host call may be made
//! while a handler borrow is held, since a callback can re-enter the handler.

use super::dump::{self, answer, answers, esc, esc32, num};
use crate::gui::direct;
use crate::gui::gw;
use crate::types::Color;
use crate::types::Vector2f;
use std::collections::BTreeMap;
use std::ffi::c_void;
use std::sync::Mutex;
use std::sync::atomic::{AtomicPtr, Ordering};

pub type Ctx = *mut c_void;
type Sink = unsafe extern "C" fn(Ctx, *const u8, usize);
type Sink32 = unsafe extern "C" fn(Ctx, *const u32, usize);

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Bytes {
    pub p: *const u8,
    pub n: usize,
}

fn bytes(s: &[u8]) -> Bytes {
    Bytes {
        p: s.as_ptr(),
        n: s.len(),
    }
}

pub const ALIGN_LEFT: i32 = 0;
pub const ALIGN_CENTER: i32 = 2;
pub const MAT_ALPHA: i32 = 1;
pub const MAT_FONT_NORMAL: i32 = 2;
pub const MAT_ADDITIVE: i32 = 3;
pub const MAT_DEFAULT: i32 = 6;
pub const GFX_ACTIVE_SIZE: i32 = 0;
pub const GFX_IMAGE_SIZE: i32 = 1;

#[repr(C)]
pub struct HostTable {
    pub menu_str: unsafe extern "C" fn(Bytes, Bytes, Bytes, Sink, Ctx),
    pub menu_f32: unsafe extern "C" fn(Bytes, Bytes, f32) -> f32,
    pub menu_vec2: unsafe extern "C" fn(Bytes, Bytes, f32, *mut f32),
    pub user_bool: unsafe extern "C" fn(Bytes, Bytes, i32) -> i32,
    pub set_user_bool: unsafe extern "C" fn(Bytes, Bytes, i32),
    pub font_range: unsafe extern "C" fn(usize, *mut u32),
    pub font_glyph: unsafe extern "C" fn(usize, u32, *mut f32) -> i32,
    pub parse_string: unsafe extern "C" fn(*const u32, usize, Sink32, Ctx),
    pub string_duration: unsafe extern "C" fn(*const u32, usize) -> f32,
    pub gui_sound: unsafe extern "C" fn(Bytes),
    pub set_update_active: unsafe extern "C" fn(i32),
    pub input_name: unsafe extern "C" fn(Bytes, Sink32, Ctx),
    pub hud: unsafe extern "C" fn(*mut f32),
    pub ptest: unsafe extern "C" fn() -> i32,
    pub draw_font: unsafe extern "C" fn(
        usize,
        usize,
        *const f32,
        *const f32,
        *const f32,
        i32,
        i32,
        *const u32,
        usize,
        i32,
    ),
    pub translate: unsafe extern "C" fn(Bytes, Bytes, Sink32, Ctx),
    pub chinese: unsafe extern "C" fn() -> i32,
    pub callback: unsafe extern "C" fn(usize, i32),
    pub map_loaded: unsafe extern "C" fn() -> i32,
    pub journal_active: unsafe extern "C" fn() -> i32,
    pub enter_journal: unsafe extern "C" fn(),
}

static HOST: AtomicPtr<HostTable> = AtomicPtr::new(std::ptr::null_mut());

/// # Safety
/// `t` outlives the program and every function in it is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a11b_set_host(t: *const HostTable) {
    HOST.store(t.cast_mut(), Ordering::Release);
}

#[derive(Clone, Copy, Default)]
pub struct Hud {
    pub center: [f32; 2],
    pub start: [f32; 3],
}

/// `set` is the GUI set to draw into; 0 is the game HUD set.
pub trait Ui {
    fn menu_str(&self, sec: &[u8], key: &[u8], def: &[u8]) -> Vec<u8>;
    fn menu_f32(&self, sec: &[u8], key: &[u8], def: f32) -> f32;
    fn menu_vec2(&self, sec: &[u8], key: &[u8], def: f32) -> [f32; 2];
    fn user_bool(&self, sec: &[u8], key: &[u8], def: bool) -> bool;
    fn set_user_bool(&self, sec: &[u8], key: &[u8], v: bool);
    fn gfx_rect(&self, color: [f32; 4]) -> usize;
    fn gfx_image(&self, file: &[u8]) -> usize;
    fn gfx_texture(&self, file: &[u8]) -> usize;
    fn gfx_size(&self, gfx: usize, which: i32) -> [f32; 2];
    fn wrap_rows(
        &self,
        font: usize,
        width: f32,
        line: f32,
        size: [f32; 2],
        text: &[u32],
    ) -> Vec<Vec<u32>>;
    fn font_len(&self, font: usize, size: [f32; 2], text: &[u32]) -> f32;
    fn font_range(&self, font: usize) -> (u32, u32);
    /// Glyph size and advance; `None` for a null glyph.
    fn font_glyph(&self, font: usize, idx: u32) -> Option<([f32; 2], f32)>;
    fn parse_string(&self, text: &[u32]) -> Vec<u32>;
    fn string_duration(&self, text: &[u32]) -> f32;
    fn gui_sound(&self, name: &[u8]);
    fn set_update_active(&self, on: bool);
    fn input_name(&self, action: &[u8]) -> Vec<u32>;
    fn hud(&self) -> Hud;
    fn ptest(&self) -> bool;
    fn draw_gfx(
        &self,
        set: usize,
        gfx: usize,
        pos: [f32; 3],
        size: Option<[f32; 2]>,
        col: [f32; 4],
        mat: i32,
    );
    #[allow(clippy::too_many_arguments)]
    fn draw_font(
        &self,
        set: usize,
        font: usize,
        pos: [f32; 3],
        size: [f32; 2],
        col: [f32; 4],
        align: i32,
        mat: i32,
        text: &[u32],
        space_prefix: bool,
    );
    fn translate(&self, cat: &[u8], key: &[u8]) -> Vec<u32>;
    fn chinese(&self) -> bool;
    fn callback(&self, cb: usize, yes: bool);
}

pub struct HostUi(&'static HostTable);

pub(super) fn table() -> Option<&'static HostTable> {
    // SAFETY: as `host`.
    unsafe { HOST.load(Ordering::Acquire).as_ref() }
}

pub fn host() -> Option<HostUi> {
    // SAFETY: `ohpl_a11b_set_host` stored a table that lives for the whole run, or null.
    unsafe { HOST.load(Ordering::Acquire).as_ref() }.map(HostUi)
}

unsafe extern "C" fn push_bytes(ctx: Ctx, p: *const u8, n: usize) {
    // SAFETY: `ctx` is the `Vec` handed to the host call; the host passes `n` readable bytes.
    let out = unsafe { &mut *ctx.cast::<Vec<u8>>() };
    out.clear();
    if !p.is_null() {
        out.extend_from_slice(unsafe { std::slice::from_raw_parts(p, n) });
    }
}

unsafe extern "C" fn push_units(ctx: Ctx, p: *const u32, n: usize) {
    // SAFETY: as `push_bytes`.
    let out = unsafe { &mut *ctx.cast::<Vec<u32>>() };
    out.clear();
    if !p.is_null() {
        out.extend_from_slice(unsafe { std::slice::from_raw_parts(p, n) });
    }
}

fn bytes_out(f: impl FnOnce(Sink, Ctx)) -> Vec<u8> {
    let mut v: Vec<u8> = Vec::new();
    f(push_bytes, std::ptr::from_mut(&mut v).cast());
    v
}

fn units_out(f: impl FnOnce(Sink32, Ctx)) -> Vec<u32> {
    let mut v: Vec<u32> = Vec::new();
    f(push_units, std::ptr::from_mut(&mut v).cast());
    v
}

const TEXTURE_2D: i32 = 1;

/// Gfx element to file name, for the recorded answers (`OHPL_A11B_ANSWERS`).
static NAMES: Mutex<BTreeMap<usize, Vec<u8>>> = Mutex::new(BTreeMap::new());

impl HostUi {
    fn name(&self, gfx: usize, name: &[u8]) {
        if answers()
            && let Ok(mut n) = NAMES.lock()
        {
            n.insert(gfx, name.to_vec());
        }
    }
}

// SAFETY (all blocks below): every pointer passed is to a live local or slice for the duration
// of the call, and the table's functions accept them.
impl Ui for HostUi {
    fn menu_str(&self, sec: &[u8], key: &[u8], def: &[u8]) -> Vec<u8> {
        let v = bytes_out(|s, c| unsafe {
            (self.0.menu_str)(bytes(sec), bytes(key), bytes(def), s, c)
        });
        if answers() {
            answer(
                "menu_str",
                &format!("{}|{}|{}", esc(sec), esc(key), esc(def)),
                &esc(&v),
            );
        }
        v
    }

    fn menu_f32(&self, sec: &[u8], key: &[u8], def: f32) -> f32 {
        let v = unsafe { (self.0.menu_f32)(bytes(sec), bytes(key), def) };
        if answers() {
            answer(
                "menu_f32",
                &format!("{}|{}|{}", esc(sec), esc(key), num(def)),
                &num(v),
            );
        }
        v
    }

    fn menu_vec2(&self, sec: &[u8], key: &[u8], def: f32) -> [f32; 2] {
        let mut o = [0.0; 2];
        unsafe { (self.0.menu_vec2)(bytes(sec), bytes(key), def, o.as_mut_ptr()) };
        if answers() {
            answer(
                "menu_vec2",
                &format!("{}|{}|{}", esc(sec), esc(key), num(def)),
                &format!("{}|{}", num(o[0]), num(o[1])),
            );
        }
        o
    }

    fn user_bool(&self, sec: &[u8], key: &[u8], def: bool) -> bool {
        let v = unsafe { (self.0.user_bool)(bytes(sec), bytes(key), i32::from(def)) != 0 };
        if answers() {
            answer(
                "user_bool",
                &format!("{}|{}|{}", esc(sec), esc(key), i32::from(def)),
                &i32::from(v).to_string(),
            );
        }
        v
    }

    fn set_user_bool(&self, sec: &[u8], key: &[u8], v: bool) {
        unsafe { (self.0.set_user_bool)(bytes(sec), bytes(key), i32::from(v)) }
    }

    fn gfx_rect(&self, color: [f32; 4]) -> usize {
        let c = Color {
            r: color[0],
            g: color[1],
            b: color[2],
            a: color[3],
        };
        let g = direct::gfx_rect(c, MAT_ALPHA, true);
        self.name(g, b"rect");
        g
    }

    fn gfx_image(&self, file: &[u8]) -> usize {
        let g = direct::gfx_image(file, MAT_ALPHA);
        self.name(g, file);
        g
    }

    fn gfx_texture(&self, file: &[u8]) -> usize {
        let g = direct::gfx_texture_file(file, MAT_ALPHA, TEXTURE_2D);
        self.name(g, file);
        g
    }

    fn gfx_size(&self, gfx: usize, which: i32) -> [f32; 2] {
        let v = if which == GFX_ACTIVE_SIZE {
            direct::gfx_active_size(gfx)
        } else {
            direct::gfx_image_size(gfx)
        };
        if answers() {
            let name = NAMES
                .lock()
                .ok()
                .and_then(|n| n.get(&gfx).cloned())
                .unwrap_or_default();
            answer(
                "gfx_size",
                &format!("{}|{}", esc(&name), i32::from(which != 0)),
                &format!("{}|{}", num(v[0]), num(v[1])),
            );
        }
        v
    }

    fn wrap_rows(
        &self,
        font: usize,
        width: f32,
        line: f32,
        size: [f32; 2],
        text: &[u32],
    ) -> Vec<Vec<u32>> {
        let rows = gw::font_rows(
            font,
            width,
            line,
            Vector2f {
                x: size[0],
                y: size[1],
            },
            text,
        );
        if answers() {
            let joined: String = rows
                .iter()
                .enumerate()
                .map(|(i, r)| format!("{}{}", if i > 0 { "\\u0001" } else { "" }, esc32(r)))
                .collect();
            answer(
                "wrap_rows",
                &format!(
                    "{}|{}|{}|{}|{}",
                    num(width),
                    num(line),
                    num(size[0]),
                    num(size[1]),
                    esc32(text)
                ),
                &format!("{}|{joined}", rows.len()),
            );
        }
        rows
    }

    fn font_len(&self, font: usize, size: [f32; 2], text: &[u32]) -> f32 {
        let v = gw::font_length(
            font,
            Vector2f {
                x: size[0],
                y: size[1],
            },
            text,
        );
        if answers() {
            answer(
                "font_len",
                &format!("{}|{}|{}", num(size[0]), num(size[1]), esc32(text)),
                &num(v),
            );
        }
        v
    }

    fn font_range(&self, font: usize) -> (u32, u32) {
        let mut o = [0u32; 2];
        unsafe { (self.0.font_range)(font, o.as_mut_ptr()) };
        if answers() {
            answer("font_range", "", &format!("{}|{}", o[0], o[1]));
        }
        (o[0], o[1])
    }

    fn font_glyph(&self, font: usize, idx: u32) -> Option<([f32; 2], f32)> {
        let mut o = [0.0f32; 3];
        let ok = unsafe { (self.0.font_glyph)(font, idx, o.as_mut_ptr()) };
        if answers() {
            let out = if ok == 0 {
                "none".to_owned()
            } else {
                format!("{}|{}|{}", num(o[0]), num(o[1]), num(o[2]))
            };
            answer("font_glyph", &idx.to_string(), &out);
        }
        (ok != 0).then_some(([o[0], o[1]], o[2]))
    }

    fn parse_string(&self, text: &[u32]) -> Vec<u32> {
        let v = units_out(|s, c| unsafe { (self.0.parse_string)(text.as_ptr(), text.len(), s, c) });
        if answers() {
            answer("parse_string", &esc32(text), &esc32(&v));
        }
        v
    }

    fn string_duration(&self, text: &[u32]) -> f32 {
        let v = unsafe { (self.0.string_duration)(text.as_ptr(), text.len()) };
        if answers() {
            answer("string_duration", &esc32(text), &num(v));
        }
        v
    }

    fn gui_sound(&self, name: &[u8]) {
        dump::log(&format!("snd {}", String::from_utf8_lossy(name)));
        unsafe { (self.0.gui_sound)(bytes(name)) }
    }

    fn set_update_active(&self, on: bool) {
        dump::log(&format!("upd {}", i32::from(on)));
        unsafe { (self.0.set_update_active)(i32::from(on)) }
    }

    fn input_name(&self, action: &[u8]) -> Vec<u32> {
        let v = units_out(|s, c| unsafe { (self.0.input_name)(bytes(action), s, c) });
        if answers() {
            answer("input_name", &esc(action), &esc32(&v));
        }
        v
    }

    fn hud(&self) -> Hud {
        let mut o = [0.0f32; 5];
        unsafe { (self.0.hud)(o.as_mut_ptr()) };
        if answers() {
            let out: Vec<String> = o.iter().map(|&x| num(x)).collect();
            answer("hud", "", &out.join("|"));
        }
        Hud {
            center: [o[0], o[1]],
            start: [o[2], o[3], o[4]],
        }
    }

    fn ptest(&self) -> bool {
        let v = unsafe { (self.0.ptest)() != 0 };
        if answers() {
            answer("ptest", "", if v { "1" } else { "0" });
        }
        v
    }

    fn draw_gfx(
        &self,
        set: usize,
        gfx: usize,
        pos: [f32; 3],
        size: Option<[f32; 2]>,
        col: [f32; 4],
        mat: i32,
    ) {
        let size = size.unwrap_or([-1.0, -1.0]);
        dump::log_gfx(pos, size, col, mat);
        let set = if set == 0 { direct::hud_set() } else { set };
        let c = Color {
            r: col[0],
            g: col[1],
            b: col[2],
            a: col[3],
        };
        direct::draw_gfx(set, gfx, pos, size, c, mat, 0.0);
    }

    fn draw_font(
        &self,
        set: usize,
        font: usize,
        pos: [f32; 3],
        size: [f32; 2],
        col: [f32; 4],
        align: i32,
        mat: i32,
        text: &[u32],
        space_prefix: bool,
    ) {
        if dump::enabled() {
            let shown: Vec<u32> = if space_prefix {
                std::iter::once(32).chain(text.iter().copied()).collect()
            } else {
                text.to_vec()
            };
            dump::log_font(pos, size, col, align, mat, &shown);
        }
        unsafe {
            (self.0.draw_font)(
                set,
                font,
                pos.as_ptr(),
                size.as_ptr(),
                col.as_ptr(),
                align,
                mat,
                text.as_ptr(),
                text.len(),
                i32::from(space_prefix),
            )
        }
    }

    fn translate(&self, cat: &[u8], key: &[u8]) -> Vec<u32> {
        let v = units_out(|s, c| unsafe { (self.0.translate)(bytes(cat), bytes(key), s, c) });
        if answers() {
            answer(
                "translate",
                &format!("{}|{}", esc(cat), esc(key)),
                &esc32(&v),
            );
        }
        v
    }

    fn chinese(&self) -> bool {
        let v = unsafe { (self.0.chinese)() != 0 };
        if answers() {
            answer("chinese", "", if v { "1" } else { "0" });
        }
        v
    }

    fn callback(&self, cb: usize, yes: bool) {
        dump::log(&format!(
            "callback {} {}",
            i32::from(cb != 0),
            i32::from(yes)
        ));
        if cb == dump::CALLBACK {
            dump::log(&format!("cb {}", i32::from(yes)));
        } else if cb != 0 {
            unsafe { (self.0.callback)(cb, i32::from(yes)) }
        }
    }
}

/// Short borrow of one arena slot; never held across a [`Ui`] call.
pub struct Slot<'a, T> {
    pub arena: &'a std::cell::RefCell<crate::obj::Arena<T>>,
    pub h: crate::obj::Handle<T>,
}

impl<T> Slot<'_, T> {
    pub fn with<R>(&self, f: impl FnOnce(&mut T) -> R) -> Option<R> {
        let mut a = self.arena.try_borrow_mut().ok()?;
        a.get_mut(self.h).map(f)
    }
}
