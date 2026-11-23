//! C ABI of the main menu shell, mirrored in `amnesia/src/game/RustLuxMenuA9b.h`.

use super::lookup;

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a9b_new() -> u64 {
    super::new()
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a9b_free(me: u64) {
    super::free(me);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a9b_load_fonts(me: u64) {
    if let Some(m) = lookup(me) {
        m.load_fonts();
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a9b_reset(me: u64) {
    if let Some(m) = lookup(me) {
        m.reset();
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a9b_on_quit(me: u64) {
    if let Some(m) = lookup(me) {
        m.on_quit();
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a9b_enter(me: u64) {
    if let Some(m) = lookup(me) {
        m.enter();
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a9b_leave(me: u64) {
    if let Some(m) = lookup(me) {
        m.leave();
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a9b_update(me: u64, dt: f32) {
    if let Some(m) = lookup(me) {
        m.update(dt);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a9b_draw(me: u64) {
    if let Some(m) = lookup(me) {
        m.draw();
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a9b_set_window(me: u64, win: i32) {
    if let Some(m) = lookup(me) {
        m.set_window_active(win);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a9b_exit_pressed(me: u64) {
    if let Some(m) = lookup(me) {
        m.exit_pressed();
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a9b_focus(me: u64, got: i32) {
    if let Some(m) = lookup(me) {
        m.focus(got != 0);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a9b_device_changed(me: u64) {
    if let Some(m) = lookup(me) {
        m.device_changed();
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a9b_set(me: u64) -> usize {
    lookup(me).map_or(0, |m| m.set_ptr())
}

/// `OHPL_A10A_DUMP` hook: dumps the options and key config windows.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a9b_dump_a10a(me: u64) {
    if let Some(m) = lookup(me) {
        m.dump_a10a();
    }
}

/// `OHPL_A9_MENUDUMP` hook: steps the menu and dumps its widgets.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a9b_dump_a9(me: u64) {
    if let Some(m) = lookup(me) {
        m.dump_a9();
    }
}
