//! C ABI of the splash and gamma screen, mirrored in `soma/src/game/RustSomaS8.h`. Each call takes
//! the shell's host table, valid for the call; it may re-enter through load ticks and widget
//! callbacks.

use super::s8::{self, E};
use crate::luxmenu2::host::HostVt;

macro_rules! eng {
    ($vt:expr, $e:ident, $d:expr, $body:block) => {{
        // SAFETY: the shell passes its host table, live for the call.
        match unsafe { $vt.as_ref() } {
            Some(vt) => {
                let $e = E::new(vt);
                $body
            }
            None => $d,
        }
    }};
}

fn put(h_out: *mut u64) -> impl Fn(u64) {
    move |h| {
        if !h_out.is_null() {
            // SAFETY: `h_out` is the shell's handle field.
            unsafe { h_out.write(h) };
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s8_splash_new(vt: *const HostVt, h_out: *mut u64) -> u64 {
    eng!(vt, e, 0, { s8::splash_new(&e, &put(h_out)) })
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s8_splash_delete(vt: *const HostVt, h: u64) {
    eng!(vt, e, (), { s8::splash_delete(&e, h) });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s8_splash_update(vt: *const HostVt, h: u64, dt: f32) {
    eng!(vt, e, (), { s8::splash_update(&e, h, dt) });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s8_splash_draw(vt: *const HostVt, h: u64) {
    eng!(vt, e, (), { s8::splash_draw(&e, h) });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s8_splash_load_draw(vt: *const HostVt, h: u64, boot: bool) {
    eng!(vt, e, (), { s8::splash_load_draw(&e, h, boot) });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s8_splash_scripts_may_run(h: u64) -> bool {
    s8::scripts_may_run(h)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s8_splash_start_load(vt: *const HostVt, h: u64) {
    eng!(vt, e, (), { s8::start_load(&e, h) });
}

/// # Safety
/// `pos` and `size` have 2 floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s8_splash_set_load_bar(h: u64, pos: *const f32, size: *const f32) {
    // SAFETY: caller contract.
    let (p, s) = unsafe {
        (
            pos.cast::<[f32; 2]>().read(),
            size.cast::<[f32; 2]>().read(),
        )
    };
    s8::set_load_bar(h, p, s);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s8_gamma_should_show(vt: *const HostVt) -> bool {
    eng!(vt, e, false, { s8::should_show_and_mark_seen(&e) })
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s8_gamma_new(vt: *const HostVt, h_out: *mut u64) -> u64 {
    eng!(vt, e, 0, { s8::gamma_new(&e, &put(h_out)) })
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s8_gamma_delete(h: u64) {
    s8::gamma_delete(h);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s8_gamma_update(vt: *const HostVt, h: u64) {
    eng!(vt, e, (), { s8::gamma_update(&e, h) });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s8_gamma_draw(vt: *const HostVt, h: u64) {
    eng!(vt, e, (), { s8::gamma_draw(&e, h) });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s8_gamma_cb(vt: *const HostVt, h: u64, tag: i32) -> bool {
    eng!(vt, e, false, { s8::gamma_callback(&e, h, i64::from(tag)) })
}
