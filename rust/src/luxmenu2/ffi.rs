//! C ABI of the main menu (GA7b), mirrored in `amnesia/src/game/RustLuxMenu2.h`. State structs live
//! in the C++ objects; every call takes a short-lived reference and never calls back into C++.

use super::menu::{self, Base, Menu, Times};
use std::ffi::c_void;

type Ctx = *mut c_void;
type Sink32 = unsafe extern "C" fn(Ctx, *const u32, usize);

fn slice<'a, T>(p: *const T, n: usize) -> &'a [T] {
    if n == 0 || p.is_null() {
        return &[];
    }
    // SAFETY: the caller passes `n` readable elements.
    unsafe { std::slice::from_raw_parts(p, n) }
}

fn slice_mut<'a, T>(p: *mut T, n: usize) -> &'a mut [T] {
    if n == 0 || p.is_null() {
        return &mut [];
    }
    // SAFETY: the caller passes `n` writable elements.
    unsafe { std::slice::from_raw_parts_mut(p, n) }
}

macro_rules! st {
    ($p:expr, $d:expr) => {
        // SAFETY: the caller passes a live state or null.
        match unsafe { $p.as_mut() } {
            Some(s) => s,
            None => return $d,
        }
    };
    ($p:expr) => {
        // SAFETY: the caller passes a live state or null.
        match unsafe { $p.as_mut() } {
            Some(s) => s,
            None => return,
        }
    };
}

fn put<T: Copy>(p: *mut T, v: T) {
    if !p.is_null() {
        // SAFETY: non-null and writable per the contract.
        unsafe { p.write(v) };
    }
}

// ---- main menu ------------------------------------------------------------------------------

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_menu_new(s: *mut Menu) {
    put(s, Menu::new());
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_menu_enter(s: *mut Menu) {
    st!(s).enter();
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_menu_update_base(
    s: *mut Menu,
    dt: f32,
    t: *const Times,
    has_camera: bool,
    out: *mut Base,
) {
    let m = st!(s);
    // SAFETY: `t` is readable.
    let Some(t) = (unsafe { t.as_ref() }) else {
        return;
    };
    put(out, menu::update_base(m, dt, t, has_camera));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_menu_update_top(
    s: *mut Menu,
    dt: f32,
    fade_in: f32,
    fade_out: f32,
    save_focus: i32,
) -> bool {
    menu::update_top(st!(s, false), dt, fade_in, fade_out, save_focus)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_menu_win_begin(s: *mut Menu, win: i32) -> i32 {
    menu::win_begin(st!(s, -2), win)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_menu_win_commit(s: *mut Menu, win: i32) -> bool {
    menu::win_commit(st!(s, false), win)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_menu_draw_focus(win: i32) -> bool {
    menu::draw_focus_window(win)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_menu_exit_pressed(s: *mut Menu, map_loaded: bool) -> i32 {
    menu::exit_pressed(st!(s, 0), map_loaded)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_menu_exit_menu(s: *mut Menu, msg: i32) -> i32 {
    menu::exit_menu(st!(s, 0), msg)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_menu_on_exit(msg: i32, demo_on_all_exit: bool) -> i32 {
    menu::on_menu_exit(msg, demo_on_all_exit)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_menu_top_press(val: i32) -> i32 {
    menu::top_press(val)
}

/// Writes up to `cap` label kinds and returns how many there are.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_menu_plan(flags: u32, out: *mut i32, cap: usize) -> usize {
    let v = menu::plan(flags);
    for (d, s) in slice_mut(out, cap).iter_mut().zip(&v) {
        *d = *s;
    }
    v.len().min(cap)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_menu_font_fit(
    lengths: *const f32,
    chars: *const i32,
    n: usize,
    screen_x: f32,
    font_x: f32,
    font_y: f32,
    out: *mut f32,
) {
    let (mul, add) = menu::font_fit(
        slice(lengths, n),
        slice(chars, n),
        screen_x,
        [font_x, font_y],
    );
    if let [a, b] = slice_mut(out, 2) {
        *a = mul;
        *b = add;
    }
}

/// Calls `sink` with the text and returns true when it has a `#`.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_menu_subst_hash(
    text: *const u32,
    n: usize,
    num: i64,
    sink: Sink32,
    ctx: Ctx,
) -> bool {
    match menu::subst_hash(slice(text, n), num) {
        Some(v) => {
            // SAFETY: `sink` accepts `ctx`.
            unsafe { sink(ctx, v.as_ptr(), v.len()) };
            true
        }
        None => false,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_menu_pay_save(tinderboxes: i32) -> i32 {
    menu::pay_save(tinderboxes).unwrap_or(-1)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_popup_ok(val: i32) -> bool {
    menu::popup_ok(val)
}
