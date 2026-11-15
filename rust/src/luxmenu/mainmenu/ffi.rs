//! C ABI of the main menu shell, mirrored in `amnesia/src/game/RustLuxMenuA9b.h`.

use super::{DumpInfo, lookup};

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct State {
    pub set: usize,
    pub first_label: usize,
    pub camera: usize,
    pub world: usize,
    pub logo: usize,
    pub top_alpha: f32,
    pub fade_alpha: f32,
    pub logo_size: [f32; 2],
    pub window: i32,
    pub top_visible: i32,
}

impl From<DumpInfo> for State {
    fn from(d: DumpInfo) -> State {
        State {
            set: d.set,
            first_label: d.first_label,
            camera: d.camera,
            world: d.world,
            logo: d.logo,
            top_alpha: d.top_alpha,
            fade_alpha: d.fade_alpha,
            logo_size: d.logo_size,
            window: d.window,
            top_visible: i32::from(d.top_visible),
        }
    }
}

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
pub extern "C" fn ohpl_a9b_create_gui(me: u64) {
    if let Some(m) = lookup(me) {
        m.create_gui();
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

/// # Safety
/// `out` is null or points at a writable `State`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a9b_state(me: u64, out: *mut State) {
    let (Some(m), false) = (lookup(me), out.is_null()) else {
        return;
    };
    // SAFETY: non-null and writable per the contract.
    unsafe { out.write(m.dump_info().into()) };
}

/// Builds (`begin`) or hides a window's widgets for the `OHPL_A10A_DUMP` hook; returns the widget.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a9b_dump_window(me: u64, win: i32, begin: i32) -> usize {
    lookup(me).map_or(0, |m| m.window_dump(win, begin != 0))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a9b_dump_background(me: u64) {
    if let Some(m) = lookup(me) {
        m.dump_background();
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a9b_dump_end(me: u64) {
    if let Some(m) = lookup(me) {
        m.dump_end();
    }
}
