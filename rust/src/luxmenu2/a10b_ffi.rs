//! C ABI of the A10b classes, mirrored in `amnesia/src/game/RustLuxMenuA10b.h`. Each call takes the
//! shell's host table; the table is valid for the call and may re-enter through widget callbacks.

use super::a10b_cr as cr;
use super::a10b_log as log;
use super::a10b_ls as ls;
use super::a10b_pm as pm;
use super::host::{Eng, HostVt};
use std::ffi::c_void;

fn bytes<'a>(p: *const u8, n: usize) -> &'a [u8] {
    if n == 0 || p.is_null() {
        return &[];
    }
    // SAFETY: the caller passes `n` readable bytes.
    unsafe { std::slice::from_raw_parts(p, n) }
}

type Sink = unsafe extern "C" fn(*mut c_void, *const u8, usize);

fn give(sink: Sink, ctx: *mut c_void, v: &[u8]) {
    // SAFETY: `sink` accepts `ctx` and a byte range.
    unsafe { sink(ctx, v.as_ptr(), v.len()) };
}

macro_rules! eng {
    ($vt:expr, $e:ident, $body:block) => {{
        // SAFETY: the shell passes its host table, live for the call.
        if let Some(vt) = unsafe { $vt.as_ref() } {
            let $e = Eng::new(vt);
            $body
        }
    }};
    ($vt:expr, $e:ident, $d:expr, $body:block) => {{
        // SAFETY: the shell passes its host table, live for the call.
        match unsafe { $vt.as_ref() } {
            Some(vt) => {
                let $e = Eng::new(vt);
                $body
            }
            None => $d,
        }
    }};
}

// ---- premenu --------------------------------------------------------------------------------

/// `h_out` receives the handle before the first host call, because widget callbacks fire while
/// the constructor runs.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_pm_new(vt: *const HostVt, h_out: *mut u64) -> u64 {
    eng!(vt, e, 0, {
        pm::new(&e, &|h| {
            if !h_out.is_null() {
                // SAFETY: `h_out` is the shell's handle field.
                unsafe { h_out.write(h) };
            }
        })
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_pm_delete(h: u64) {
    pm::delete(h);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_pm_enter(vt: *const HostVt, h: u64) {
    eng!(vt, e, { pm::on_enter(&e, h) });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_pm_leave(vt: *const HostVt, h: u64) {
    eng!(vt, e, { pm::on_leave(&e, h) });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_pm_update(vt: *const HostVt, h: u64, dt: f32) {
    eng!(vt, e, { pm::update(&e, h, e.clock(dt)) });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_pm_draw(vt: *const HostVt, h: u64) {
    eng!(vt, e, { pm::on_draw(&e, h) });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_pm_button_pressed(vt: *const HostVt, h: u64) {
    eng!(vt, e, { pm::button_pressed(&e, h) });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_pm_focus(vt: *const HostVt, got: bool) {
    eng!(vt, e, {
        if got {
            pm::focus_got(&e)
        } else {
            pm::focus_lost(&e)
        }
    });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_pm_set(h: u64) -> usize {
    pm::set_of(h)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_pm_continue_visible(vt: *const HostVt, h: u64) -> bool {
    eng!(vt, e, false, { pm::continue_visible(&e, h) })
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_pm_cb(vt: *const HostVt, h: u64, tag: i32, lval: i32) -> bool {
    eng!(vt, e, false, { pm::callback(&e, h, i64::from(tag), lval) })
}

// ---- load screen ----------------------------------------------------------------------------

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_ls_new(vt: *const HostVt) -> u64 {
    eng!(vt, e, 0, { ls::new(&e) })
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_ls_delete(h: u64) {
    ls::delete(h);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_ls_load_fonts(vt: *const HostVt, h: u64) {
    eng!(vt, e, { ls::load_fonts(&e, h) });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_ls_reset(vt: *const HostVt, h: u64) {
    eng!(vt, e, { ls::reset(&e, h) });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_ls_update(vt: *const HostVt, h: u64, dt: f32) {
    eng!(vt, e, { ls::update(&e, h, e.clock(dt)) });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_ls_enter(vt: *const HostVt, h: u64) {
    eng!(vt, e, { ls::on_enter(&e, h) });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_ls_leave(vt: *const HostVt, h: u64) {
    eng!(vt, e, { ls::on_leave(&e, h) });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_ls_draw(vt: *const HostVt, h: u64) {
    eng!(vt, e, { ls::on_draw(&e, h) });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_ls_load_done(
    vt: *const HostVt,
    h: u64,
    snd: *const u8,
    snd_len: usize,
    load_time: f32,
) {
    eng!(vt, e, {
        ls::game_screen_load_done(&e, h, bytes(snd, snd_len), e.clock(load_time))
    });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_ls_setup(
    vt: *const HostVt,
    h: u64,
    cat: *const u8,
    cat_len: usize,
    entry: *const u8,
    entry_len: usize,
    rand_num: i32,
    image: *const u8,
    image_len: usize,
) {
    eng!(vt, e, {
        ls::setup_load_text(
            &e,
            h,
            bytes(cat, cat_len),
            bytes(entry, entry_len),
            rand_num,
            bytes(image, image_len),
        )
    });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_ls_draw_game_screen(vt: *const HostVt, h: u64) {
    eng!(vt, e, { ls::draw_game_screen_public(&e, h) });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_ls_draw_menu_screen(vt: *const HostVt, h: u64) {
    eng!(vt, e, { ls::draw_menu_screen(&e, h) });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_ls_exit_pressed(vt: *const HostVt, h: u64) {
    eng!(vt, e, { ls::exit_pressed(&e, h) });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_ls_text(h: u64, which: i32, sink: Sink, ctx: *mut c_void) {
    give(sink, ctx, &ls::text_field(h, which));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_ls_rand_num(h: u64) -> i32 {
    ls::rand_num(h)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_ls_prev_bits(h: u64) -> i32 {
    ls::prev_bits(h)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_ls_set_prev_bits(h: u64, v: i32) {
    ls::set_prev_bits(h, v);
}

// ---- credits --------------------------------------------------------------------------------

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_cr_new(vt: *const HostVt) -> u64 {
    eng!(vt, e, 0, { cr::cr_new(&e) })
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_cr_delete(h: u64) {
    cr::cr_delete(h);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_cr_load_fonts(vt: *const HostVt, h: u64) {
    eng!(vt, e, { cr::cr_load_fonts(&e, h) });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_cr_reset(vt: *const HostVt, h: u64) {
    eng!(vt, e, { cr::cr_reset(&e, h) });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_cr_enter(vt: *const HostVt, h: u64) {
    eng!(vt, e, { cr::cr_on_enter(&e, h) });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_cr_leave(vt: *const HostVt, h: u64) {
    eng!(vt, e, { cr::cr_on_leave(&e, h) });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_cr_update(vt: *const HostVt, h: u64, dt: f32) {
    eng!(vt, e, { cr::cr_update(&e, h, e.clock(dt)) });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_cr_add_key(vt: *const HostVt, h: u64, key: i32) {
    eng!(vt, e, { cr::cr_add_key(&e, h, key) });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_cr_setup(
    vt: *const HostVt,
    h: u64,
    music: *const u8,
    music_len: usize,
    looped: bool,
    cat: *const u8,
    cat_len: usize,
    entry: *const u8,
    entry_len: usize,
    end_num: i32,
) {
    eng!(vt, e, {
        cr::cr_setup(
            &e,
            h,
            bytes(music, music_len),
            looped,
            bytes(cat, cat_len),
            bytes(entry, entry_len),
            end_num,
        )
    });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_cr_exit_pressed(vt: *const HostVt, h: u64) {
    eng!(vt, e, { cr::cr_exit_pressed(&e, h) });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_cr_draw(vt: *const HostVt, h: u64) {
    eng!(vt, e, { cr::cr_on_draw(&e, h) });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_cr_focus(vt: *const HostVt, got: bool) {
    eng!(vt, e, {
        if got {
            cr::cr_focus_got(&e)
        } else {
            cr::cr_focus_lost(&e)
        }
    });
}

// ---- demo end -------------------------------------------------------------------------------

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_de_new(vt: *const HostVt) -> u64 {
    eng!(vt, e, 0, { cr::de_new(&e) })
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_de_delete(h: u64) {
    cr::de_delete(h);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_de_show_on_all_exit(h: u64) -> bool {
    cr::de_show_on_all_exit(h)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_de_load_fonts(vt: *const HostVt, h: u64) {
    eng!(vt, e, { cr::de_load_fonts(&e, h) });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_de_reset(vt: *const HostVt, h: u64) {
    eng!(vt, e, { cr::de_reset(&e, h) });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_de_enter(vt: *const HostVt, h: u64) {
    eng!(vt, e, { cr::de_on_enter(&e, h) });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_de_leave(vt: *const HostVt, h: u64) {
    eng!(vt, e, { cr::de_on_leave(&e, h) });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_de_update(vt: *const HostVt, h: u64, dt: f32) {
    eng!(vt, e, { cr::de_update(&e, h, e.clock(dt)) });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_de_exit(vt: *const HostVt, h: u64, website: bool) {
    eng!(vt, e, { cr::de_exit(&e, h, website) });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_de_draw(vt: *const HostVt, h: u64) {
    eng!(vt, e, { cr::de_on_draw(&e, h) });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_de_focus(vt: *const HostVt, got: bool) {
    eng!(vt, e, {
        if got {
            cr::de_focus_got(&e)
        } else {
            cr::de_focus_lost(&e)
        }
    });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_de_cb(
    vt: *const HostVt,
    h: u64,
    tag: i32,
    widget: usize,
    fval: f32,
) -> bool {
    eng!(vt, e, false, {
        cr::de_callback(&e, h, i64::from(tag), widget, e.clock(fval))
    })
}

// ---- progress log ---------------------------------------------------------------------------

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_log_new() -> u64 {
    log::new()
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_log_delete(h: u64) {
    log::delete(h);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_log_load_user_config(vt: *const HostVt, h: u64) {
    eng!(vt, e, { log::load_user_config(&e, h) });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_log_save_user_config(vt: *const HostVt, h: u64) {
    eng!(vt, e, { log::save_user_config(&e, h) });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_log_create(vt: *const HostVt, h: u64) {
    eng!(vt, e, { log::create_and_reset(&e, h) });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_log_update(h: u64) {
    log::update(h);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a10b_log_add(
    vt: *const HostVt,
    h: u64,
    level: i32,
    msg: *const u8,
    msg_len: usize,
) {
    eng!(vt, e, { log::add_log(&e, h, level, bytes(msg, msg_len)) });
}
