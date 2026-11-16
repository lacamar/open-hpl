//! C ABI of `cLuxMapHandler`, mirrored in `amnesia/src/game/RustLuxMapA13.h`.

use super::a7_ffi::Ctx;
use super::mhandler::{H, Handler};
use crate::string_ffi::units;
use crate::types::Vector3f;

/// # Safety
/// `h` is a live handler.
unsafe fn hd(h: *mut Handler) -> H {
    // SAFETY: caller contract.
    unsafe { H::new(h) }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a13_handler_new(cpp: usize) -> *mut Handler {
    let h = Handler::create();
    // SAFETY: `h` was just created.
    unsafe { hd(h).init(cpp) };
    h
}

/// # Safety
/// `h` came from `ohpl_a13_handler_new`, `ohpl_a13_handler_teardown` has run, and `h` is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_handler_free(h: *mut Handler) {
    if !h.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(h) });
    }
}

/// The list context of the loaded maps, for the drop thunk registration.
///
/// # Safety
/// `h` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_handler_maps(h: *mut Handler) -> *mut Ctx {
    // SAFETY: caller contract.
    unsafe { hd(h) }.maps_ctx()
}

/// # Safety
/// `h` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_handler_teardown(h: *mut Handler) {
    // SAFETY: caller contract.
    unsafe { hd(h) }.teardown();
}

/// # Safety
/// `h` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_handler_update(h: *mut Handler, step: f32) {
    // SAFETY: caller contract.
    unsafe { hd(h) }.update(step);
}

/// # Safety
/// `h` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_handler_render_solid(h: *mut Handler, functions: usize) {
    // SAFETY: caller contract.
    unsafe { hd(h) }.render_solid(functions);
}

/// # Safety
/// `h` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_handler_reset(h: *mut Handler) {
    // SAFETY: caller contract.
    unsafe { hd(h) }.reset();
}

/// # Safety
/// `h` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_handler_on_quit(h: *mut Handler) {
    // SAFETY: caller contract.
    unsafe { hd(h) }.on_quit();
}

/// # Safety
/// `h` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_handler_set_update_active(h: *mut Handler, active: bool) {
    // SAFETY: caller contract.
    unsafe { hd(h) }.set_update_active(active);
}

/// # Safety
/// `h` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_handler_container(h: *mut Handler, enter: bool) {
    // SAFETY: caller contract.
    let h = unsafe { hd(h) };
    if enter {
        h.on_enter_container();
    } else {
        h.on_leave_container();
    }
}

/// # Safety
/// `h` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_handler_show_commentary(h: *mut Handler) -> bool {
    // SAFETY: caller contract.
    unsafe { hd(h) }.show_commentary()
}

/// # Safety
/// `h` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_handler_set_show_commentary(
    h: *mut Handler,
    show: bool,
    stop: bool,
) {
    // SAFETY: caller contract.
    let h = unsafe { hd(h) };
    if stop {
        h.set_show_commentary(show, true);
    } else {
        h.set_show_raw(show);
    }
}

/// # Safety
/// `h` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_handler_pause(h: *mut Handler, pause: bool) {
    // SAFETY: caller contract.
    let h = unsafe { hd(h) };
    if pause {
        h.pause_sounds();
    } else {
        h.resume_sounds();
    }
}

/// # Safety
/// `h` live; `file` readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_handler_load_map(
    h: *mut Handler,
    file: *const u8,
    n: usize,
    entities: bool,
) -> usize {
    // SAFETY: caller contract.
    unsafe { hd(h).load_map(units(file, n), entities) }
}

/// # Safety
/// `h` live; `cmap` a map of this handler.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_handler_destroy_map(
    h: *mut Handler,
    cmap: usize,
    loading_save: bool,
) {
    // SAFETY: caller contract.
    unsafe { hd(h) }.destroy_map(cmap, loading_save);
}

/// # Safety
/// `h` live; `pos` readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_handler_set_current(
    h: *mut Handler,
    cmap: usize,
    run_script: bool,
    first_time: bool,
    pos: *const u8,
    n: usize,
) {
    // SAFETY: caller contract.
    unsafe { hd(h).set_current_map(cmap, run_script, first_time, units(pos, n)) };
}

/// # Safety
/// `h` live; strings readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_handler_change_map(
    h: *mut Handler,
    map: *const u8,
    map_n: usize,
    pos: *const u8,
    pos_n: usize,
    start_sound: *const u8,
    start_n: usize,
    end_sound: *const u8,
    end_n: usize,
) {
    // SAFETY: caller contract.
    unsafe {
        hd(h).change_map(
            units(map, map_n),
            units(pos, pos_n),
            units(start_sound, start_n),
            units(end_sound, end_n),
        );
    }
}

/// # Safety
/// `h` live; `name` readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_handler_add_hearable(h: *mut Handler, name: *const u8, n: usize) {
    // SAFETY: caller contract.
    unsafe { hd(h).add_hearable(units(name, n)) };
}

/// # Safety
/// `h` live; `type_name` readable; `pos` readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_handler_sound_started(
    h: *mut Handler,
    type_name: *const u8,
    n: usize,
    max_dist: f32,
    volume: f32,
    pos: *const Vector3f,
) {
    // SAFETY: caller contract.
    unsafe { hd(h).sound_started(units(type_name, n), max_dist, volume, *pos) };
}
