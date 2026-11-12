//! C ABI of GA7b, mirrored in `amnesia/src/game/RustLuxMenu2.h`. State structs live in the C++
//! objects; every call takes a short-lived reference and never calls back into C++.

use super::credits::{self, Credits, DemoEnd, DrawRow, Row};
use super::loadscreen::{self, Load};
use super::menu::{self, Base, Menu, Times};
use super::pages;
use super::premenu::{self, Pre, Step};
use crate::luxsave::names::Date;
use std::ffi::c_void;

type Ctx = *mut c_void;
type Sink = unsafe extern "C" fn(Ctx, *const u8, usize);
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

// ---- start game, load game, custom story ------------------------------------------------------

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_pg_exit_target(custom_story_active: bool, ok: bool) -> i32 {
    pages::exit_target(custom_story_active, ok).unwrap_or(-1)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_pg_ui_switch(val: i32) -> i32 {
    pages::ui_switch(val)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_pg_mode_gold(hard: bool) -> i32 {
    let (n, h) = pages::mode_gold(hard);
    i32::from(n) | (i32::from(h) << 1)
}

/// `dates` points at `n` `hpl::cDate` (8 ints each); `out` receives the listing order.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_pg_sort_saves(dates: *const [i32; 8], n: usize, out: *mut u32) {
    let ds: Vec<Date> = slice(dates, n)
        .iter()
        .map(|d| Date {
            seconds: d[0],
            minutes: d[1],
            hours: d[2],
            month_day: d[3],
            month: d[4],
            year: d[5],
        })
        .collect();
    for (d, s) in slice_mut(out, n).iter_mut().zip(pages::sort_saves(&ds)) {
        *d = s as u32;
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_pg_load_index(selected: i32, count: usize) -> i32 {
    pages::load_index(selected, count).map_or(-1, |i| i as i32)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_pg_press_ok(selected: i32) -> i32 {
    pages::press_ok(selected)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_pg_button_width(lengths: *const f32, n: usize) -> f32 {
    pages::button_width(slice(lengths, n))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_pg_story_buttons(
    save_exists: bool,
    start_y: f32,
    heights: *const f32,
    n: usize,
    active: *mut bool,
    y: *mut f32,
) {
    pages::story_buttons(
        save_exists,
        start_y,
        slice(heights, n),
        slice_mut(active, n),
        slice_mut(y, n),
    );
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_pg_story_fallbacks(
    name_empty: bool,
    author_empty: bool,
    desc_empty: bool,
) -> u32 {
    pages::story_fallbacks(name_empty, author_empty, desc_empty)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_pg_start_folders(custom_exists: bool, profile_exists: bool) -> u32 {
    pages::start_folders(custom_exists, profile_exists)
}

// ---- premenu ----------------------------------------------------------------------------------

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_pm_new(s: *mut Pre) {
    put(s, Pre::new());
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_pm_enter(s: *mut Pre) {
    st!(s).enter();
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_pm_update_actions(s: *mut Pre, dt: f32) {
    st!(s).update_actions(dt);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_pm_update_state(
    s: *mut Pre,
    first_start: bool,
    first_only: *const bool,
    n: usize,
    out: *mut Step,
) {
    let p = st!(s);
    put(out, p.update_state(first_start, slice(first_only, n)));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_pm_check_show(s: *mut Pre, has_text: bool, has_gamma: bool) {
    st!(s).check_show(has_text, has_gamma);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_pm_button_pressed(
    s: *mut Pre,
    has_section: bool,
    allow_skipping: bool,
) {
    st!(s).button_pressed(has_section, allow_skipping);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_pm_music_fade_speed(fade_time: f32) -> f32 {
    premenu::music_fade_speed(fade_time)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_pm_gamma_slider(out: *mut i32) {
    let (m, b) = premenu::gamma_slider();
    if let [a, c] = slice_mut(out, 2) {
        *a = m;
        *c = b;
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_pm_gamma_to_slider(gamma: f32, max_value: i32) -> i32 {
    premenu::gamma_to_slider(gamma, max_value)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_pm_slider_to_gamma(value: i32, max_value: i32) -> f32 {
    premenu::slider_to_gamma(value, max_value)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_pm_gamma_arrow(arrow: i32) -> i32 {
    premenu::gamma_arrow(arrow)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_pm_background_rect(w: f32, h: f32, out: *mut f32) {
    let (x, width) = premenu::background_rect(w, h);
    if let [a, b] = slice_mut(out, 2) {
        *a = x;
        *b = width;
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_pm_parse_align(name: *const u8, n: usize) -> i32 {
    premenu::parse_align(slice(name, n)).unwrap_or(-1)
}

// ---- credits ----------------------------------------------------------------------------------

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_cr_new(s: *mut Credits) {
    put(s, Credits::new());
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_cr_reset(s: *mut Credits) {
    st!(s).reset();
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_cr_update(
    s: *mut Credits,
    dt: f32,
    rows: *const Row,
    n: usize,
    scroll_speed: f32,
    fade_speed: f32,
) -> u32 {
    st!(s, 0).update(dt, slice(rows, n), scroll_speed, fade_speed)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_cr_exit_pressed(s: *mut Credits) -> bool {
    st!(s, false).exit_pressed()
}

/// Writes up to `cap` draw rows and returns how many there are.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_cr_draw_rows(
    y0: f32,
    rows: *const Row,
    n: usize,
    add_normal: f32,
    add_header: f32,
    out: *mut DrawRow,
    cap: usize,
) -> usize {
    let v = credits::draw_rows(y0, slice(rows, n), [add_normal, add_header]);
    for (d, s) in slice_mut(out, cap).iter_mut().zip(&v) {
        *d = *s;
    }
    v.len().min(cap)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_cr_end_code(end_num: i32, ptest: bool) -> i32 {
    credits::end_code(end_num, ptest)
}

// ---- demo end ---------------------------------------------------------------------------------

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_de_new(s: *mut DemoEnd) {
    put(s, DemoEnd::new());
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_de_reset(s: *mut DemoEnd) {
    st!(s).reset();
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_de_update(s: *mut DemoEnd, dt: f32) -> u32 {
    st!(s, 0).update(dt)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_de_exit(s: *mut DemoEnd, website: bool) {
    st!(s).exit(website);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_de_button_color(
    fade: *mut f32,
    hover: bool,
    dt: f32,
    base: *const f32,
    out: *mut f32,
) {
    let f = st!(fade);
    let &[r, g, b, a] = slice(base, 4) else {
        return;
    };
    let c = credits::button_color(f, hover, dt, [r, g, b, a]);
    for (d, s) in slice_mut(out, 4).iter_mut().zip(c) {
        *d = s;
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_de_message_rows(count: usize, text_y: f32, font_h: f32, out: *mut f32) {
    for (d, s) in slice_mut(out, count)
        .iter_mut()
        .zip(credits::message_rows(count, text_y, font_h))
    {
        *d = s;
    }
}

// ---- load screen ------------------------------------------------------------------------------

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_ls_new(s: *mut Load) {
    put(s, Load::new());
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_ls_reset(s: *mut Load) {
    st!(s).reset();
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_ls_setup(s: *mut Load, same: bool, rand_num: i32) {
    st!(s).setup(same, rand_num);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_ls_load_done(
    s: *mut Load,
    has_wanted: bool,
    wanted: f32,
    load_time: f32,
) {
    st!(s).load_done(has_wanted.then_some(wanted), load_time);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_ls_exit_pressed(s: *mut Load) {
    st!(s).exit_pressed();
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_ls_update_game(s: *mut Load, dt: f32, fade_out_time: f32) -> bool {
    st!(s, false).update_game(dt, fade_out_time)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_ls_text_suffix(s: *mut Load, sink: Sink, ctx: Ctx) {
    let v = st!(s).text_suffix();
    // SAFETY: `sink` accepts `ctx`.
    unsafe { sink(ctx, v.as_ptr(), v.len()) };
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_ls_text_y(loading_alpha: f32, with_image: f32, alone: f32) -> f32 {
    loadscreen::text_y(loading_alpha, with_image, alone)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga7b_ls_centered(size: f32, center: f32) -> f32 {
    loadscreen::centered(size, center)
}
