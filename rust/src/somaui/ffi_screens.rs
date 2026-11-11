//! C ABI of the gamma screen and splash, mirrored in `soma/src/game/RustSomaUi.h`.

use super::screens::{self, GammaLayout, SplashIn, SplashPlan};

const _: () = {
    use std::mem::size_of;
    assert!(size_of::<GammaLayout>() == 56);
    assert!(size_of::<SplashPlan>() == 64);
};

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs7_gamma_wrap_width(screen_w: f32) -> f32 {
    screens::gamma_wrap_width(screen_w)
}

/// # Safety
/// `screen` has 2 floats; `checker` is null or has 2 floats; `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_gamma_layout(
    screen: *const f32,
    checker: *const f32,
    wrap_w: f32,
    rows: usize,
    out: *mut GammaLayout,
) {
    // SAFETY: caller contract.
    unsafe {
        let c = (!checker.is_null()).then(|| checker.cast::<[f32; 2]>().read());
        *out = screens::gamma_layout(screen.cast::<[f32; 2]>().read(), c, wrap_w, rows);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs7_gamma_to_slider(gamma: f32) -> i32 {
    screens::gamma_to_slider(gamma)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs7_gamma_from_slider(value: i32, max_value: i32) -> f32 {
    screens::slider_to_gamma(value, max_value)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs7_mouse_edge(down: bool, was_down: bool) -> i32 {
    screens::mouse_edge(down, was_down)
}

/// # Safety
/// `elapsed` and `finished` are writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_splash_update(elapsed: *mut f32, dt: f32) -> bool {
    // SAFETY: caller contract.
    unsafe {
        let (e, done) = screens::splash_update(*elapsed, dt);
        *elapsed = e;
        done
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs7_splash_scripts_may_run(finished: bool, elapsed: f32) -> bool {
    screens::scripts_may_run(finished, elapsed)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs7_splash_boot_bar_fraction(t: f32) -> f32 {
    screens::boot_bar_fraction(t)
}

/// # Safety
/// `screen` has 2 floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_splash_virtual_width(screen: *const f32) -> f32 {
    // SAFETY: caller contract.
    screens::virtual_width(unsafe { screen.cast::<[f32; 2]>().read() })
}

/// # Safety
/// Vectors have the stated float counts.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_splash_virtual_to_screen(
    pos: *const f32,
    z: f32,
    vw: f32,
    screen: *const f32,
    out: *mut f32,
) {
    // SAFETY: caller contract.
    unsafe {
        let r = screens::virtual_to_screen(
            pos.cast::<[f32; 2]>().read(),
            z,
            vw,
            screen.cast::<[f32; 2]>().read(),
        );
        out.cast::<[f32; 3]>().write(r);
    }
}

/// # Safety
/// Vectors have the stated float counts.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_splash_virtual_size_to_screen(
    size: *const f32,
    vw: f32,
    screen: *const f32,
    out: *mut f32,
) {
    // SAFETY: caller contract.
    unsafe {
        let r = screens::virtual_size_to_screen(
            size.cast::<[f32; 2]>().read(),
            vw,
            screen.cast::<[f32; 2]>().read(),
        );
        out.cast::<[f32; 2]>().write(r);
    }
}

/// The ping-pong frame of the brain icon, -1 when none is drawable.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs7_splash_brain_frame(time: f32) -> i32 {
    screens::brain_frame(time).map_or(-1, |f| f as i32)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs7_splash_load_draw_due(now: u64, last: u64) -> bool {
    screens::load_draw_due(now, last)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs7_splash_boot_elapsed(now: u64, boot_start: u64) -> f32 {
    screens::boot_elapsed(now, boot_start)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs7_splash_load_fraction(now: u64, start: u64) -> f32 {
    screens::load_fraction(now, start)
}

/// # Safety
/// `screen` has 2 floats; `premenu` is null or has 2 floats; `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_splash_plan(
    screen: *const f32,
    vw: f32,
    t: f32,
    premenu: *const f32,
    out: *mut SplashPlan,
) {
    // SAFETY: caller contract.
    unsafe {
        let i = SplashIn {
            screen: screen.cast::<[f32; 2]>().read(),
            vw,
            t,
            premenu_image: (!premenu.is_null()).then(|| premenu.cast::<[f32; 2]>().read()),
        };
        *out = screens::splash_plan(&i);
    }
}

/// # Safety
/// `pos` has 3 floats, `size` has 2.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_splash_brain_rect(
    vw: f32,
    screen: *const f32,
    pos: *mut f32,
    size: *mut f32,
) {
    // SAFETY: caller contract.
    unsafe {
        let (p, s) = screens::brain_rect(vw, screen.cast::<[f32; 2]>().read());
        pos.cast::<[f32; 3]>().write(p);
        size.cast::<[f32; 2]>().write(s);
    }
}
