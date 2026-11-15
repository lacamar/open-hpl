//! Engine services the Rust hint and message handlers reach: configs, GUI gfx and fonts, the game
//! HUD set, sound, the map handler and the yes/no callback. C++ fills one [`HostTable`]
//! (`amnesia/src/game/RustLuxUiA11b.h`); tests use a recording fake. No host call may be made
//! while a handler borrow is held, since a callback can re-enter the handler.

use std::ffi::c_void;
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
    pub gfx_rect: unsafe extern "C" fn(*const f32) -> usize,
    pub gfx_image: unsafe extern "C" fn(Bytes) -> usize,
    pub gfx_texture: unsafe extern "C" fn(Bytes) -> usize,
    pub gfx_size: unsafe extern "C" fn(usize, i32, *mut f32),
    pub wrap_rows:
        unsafe extern "C" fn(usize, f32, f32, *const f32, *const u32, usize, Sink32, Ctx),
    pub font_len: unsafe extern "C" fn(usize, *const f32, *const u32, usize) -> f32,
    pub font_range: unsafe extern "C" fn(usize, *mut u32),
    pub font_glyph: unsafe extern "C" fn(usize, u32, *mut f32) -> i32,
    pub parse_string: unsafe extern "C" fn(*const u32, usize, Sink32, Ctx),
    pub string_duration: unsafe extern "C" fn(*const u32, usize) -> f32,
    pub gui_sound: unsafe extern "C" fn(Bytes),
    pub set_update_active: unsafe extern "C" fn(i32),
    pub input_name: unsafe extern "C" fn(Bytes, Sink32, Ctx),
    pub hud: unsafe extern "C" fn(*mut f32),
    pub ptest: unsafe extern "C" fn() -> i32,
    pub draw_gfx: unsafe extern "C" fn(usize, usize, *const f32, *const f32, *const f32, i32),
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

unsafe extern "C" fn push_row(ctx: Ctx, p: *const u32, n: usize) {
    // SAFETY: `ctx` is the row list handed to `wrap_rows`.
    let out = unsafe { &mut *ctx.cast::<Vec<Vec<u32>>>() };
    let row = if p.is_null() {
        Vec::new()
    } else {
        unsafe { std::slice::from_raw_parts(p, n) }.to_vec()
    };
    out.push(row);
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

// SAFETY (all blocks below): every pointer passed is to a live local or slice for the duration
// of the call, and the table's functions accept them.
impl Ui for HostUi {
    fn menu_str(&self, sec: &[u8], key: &[u8], def: &[u8]) -> Vec<u8> {
        bytes_out(|s, c| unsafe { (self.0.menu_str)(bytes(sec), bytes(key), bytes(def), s, c) })
    }

    fn menu_f32(&self, sec: &[u8], key: &[u8], def: f32) -> f32 {
        unsafe { (self.0.menu_f32)(bytes(sec), bytes(key), def) }
    }

    fn menu_vec2(&self, sec: &[u8], key: &[u8], def: f32) -> [f32; 2] {
        let mut o = [0.0; 2];
        unsafe { (self.0.menu_vec2)(bytes(sec), bytes(key), def, o.as_mut_ptr()) };
        o
    }

    fn user_bool(&self, sec: &[u8], key: &[u8], def: bool) -> bool {
        unsafe { (self.0.user_bool)(bytes(sec), bytes(key), i32::from(def)) != 0 }
    }

    fn set_user_bool(&self, sec: &[u8], key: &[u8], v: bool) {
        unsafe { (self.0.set_user_bool)(bytes(sec), bytes(key), i32::from(v)) }
    }

    fn gfx_rect(&self, color: [f32; 4]) -> usize {
        unsafe { (self.0.gfx_rect)(color.as_ptr()) }
    }

    fn gfx_image(&self, file: &[u8]) -> usize {
        unsafe { (self.0.gfx_image)(bytes(file)) }
    }

    fn gfx_texture(&self, file: &[u8]) -> usize {
        unsafe { (self.0.gfx_texture)(bytes(file)) }
    }

    fn gfx_size(&self, gfx: usize, which: i32) -> [f32; 2] {
        let mut o = [0.0; 2];
        unsafe { (self.0.gfx_size)(gfx, which, o.as_mut_ptr()) };
        o
    }

    fn wrap_rows(
        &self,
        font: usize,
        width: f32,
        line: f32,
        size: [f32; 2],
        text: &[u32],
    ) -> Vec<Vec<u32>> {
        let mut rows: Vec<Vec<u32>> = Vec::new();
        unsafe {
            (self.0.wrap_rows)(
                font,
                width,
                line,
                size.as_ptr(),
                text.as_ptr(),
                text.len(),
                push_row,
                std::ptr::from_mut(&mut rows).cast(),
            )
        };
        rows
    }

    fn font_len(&self, font: usize, size: [f32; 2], text: &[u32]) -> f32 {
        unsafe { (self.0.font_len)(font, size.as_ptr(), text.as_ptr(), text.len()) }
    }

    fn font_range(&self, font: usize) -> (u32, u32) {
        let mut o = [0u32; 2];
        unsafe { (self.0.font_range)(font, o.as_mut_ptr()) };
        (o[0], o[1])
    }

    fn font_glyph(&self, font: usize, idx: u32) -> Option<([f32; 2], f32)> {
        let mut o = [0.0f32; 3];
        let ok = unsafe { (self.0.font_glyph)(font, idx, o.as_mut_ptr()) };
        (ok != 0).then_some(([o[0], o[1]], o[2]))
    }

    fn parse_string(&self, text: &[u32]) -> Vec<u32> {
        units_out(|s, c| unsafe { (self.0.parse_string)(text.as_ptr(), text.len(), s, c) })
    }

    fn string_duration(&self, text: &[u32]) -> f32 {
        unsafe { (self.0.string_duration)(text.as_ptr(), text.len()) }
    }

    fn gui_sound(&self, name: &[u8]) {
        unsafe { (self.0.gui_sound)(bytes(name)) }
    }

    fn set_update_active(&self, on: bool) {
        unsafe { (self.0.set_update_active)(i32::from(on)) }
    }

    fn input_name(&self, action: &[u8]) -> Vec<u32> {
        units_out(|s, c| unsafe { (self.0.input_name)(bytes(action), s, c) })
    }

    fn hud(&self) -> Hud {
        let mut o = [0.0f32; 5];
        unsafe { (self.0.hud)(o.as_mut_ptr()) };
        Hud {
            center: [o[0], o[1]],
            start: [o[2], o[3], o[4]],
        }
    }

    fn ptest(&self) -> bool {
        unsafe { (self.0.ptest)() != 0 }
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
        let sp = size.as_ref().map_or(std::ptr::null(), |s| s.as_ptr());
        unsafe { (self.0.draw_gfx)(set, gfx, pos.as_ptr(), sp, col.as_ptr(), mat) }
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
        units_out(|s, c| unsafe { (self.0.translate)(bytes(cat), bytes(key), s, c) })
    }

    fn chinese(&self) -> bool {
        unsafe { (self.0.chinese)() != 0 }
    }

    fn callback(&self, cb: usize, yes: bool) {
        unsafe { (self.0.callback)(cb, i32::from(yes)) }
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
