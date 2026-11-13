//! C ABI of the headless control server (`HPL2/core/include/engine/RustEngineH22.h`).

use super::h22_hc::{self, Ev, HcHost, HcServer, LogLevel, MouseButton, Shot};
use crate::string_ffi::{Sink, units};
use crate::sys::sdl2;
use std::ffi::{CString, c_void};

#[repr(C)]
#[derive(Clone, Copy)]
pub struct HcHostC {
    pub ctx: *mut c_void,
    pub now_ms: unsafe extern "C" fn(*mut c_void) -> u64,
    pub rendered_frames: unsafe extern "C" fn(*mut c_void) -> u32,
    pub log: unsafe extern "C" fn(*mut c_void, i32, *const u8, usize),
    pub exit: unsafe extern "C" fn(*mut c_void),
    pub set_wait_focus: unsafe extern "C" fn(*mut c_void, bool),
    pub dev_hud_active: unsafe extern "C" fn(*mut c_void) -> bool,
    pub set_dev_hud: unsafe extern "C" fn(*mut c_void, bool),
    pub screen_stats: unsafe extern "C" fn(*mut c_void, *mut f32, *mut f32, *mut f32) -> bool,
    pub force_window_size: unsafe extern "C" fn(*mut c_void, i32, i32) -> bool,
    pub shader_fail_count: unsafe extern "C" fn(*mut c_void) -> i32,
    pub shader_report_json: unsafe extern "C" fn(*mut c_void, bool, Sink<u8>, *mut c_void),
    pub frame_stats:
        unsafe extern "C" fn(*mut c_void, Sink<u8>, *mut c_void, Sink<u8>, *mut c_void) -> bool,
    pub xdg_cache_home: unsafe extern "C" fn(*mut c_void, Sink<u8>, *mut c_void),
    pub ensure_folder: unsafe extern "C" fn(*mut c_void, *const u8, usize),
    pub screenshot: unsafe extern "C" fn(*mut c_void, *const u8, usize) -> i32,
    pub inject_text: unsafe extern "C" fn(*mut c_void, *const u8, usize),
    pub run_message: unsafe extern "C" fn(*mut c_void, i32),
}

unsafe extern "C" fn collect(ctx: *mut c_void, p: *const u8, n: usize) {
    // SAFETY: `ctx` is the `Vec<u8>` the caller passed; `p` is valid for `n` bytes.
    unsafe { (*ctx.cast::<Vec<u8>>()).extend_from_slice(units(p, n)) }
}

impl HcHostC {
    fn string(&self, f: impl FnOnce(Sink<u8>, *mut c_void)) -> Vec<u8> {
        let mut out = Vec::new();
        f(collect, std::ptr::from_mut(&mut out).cast());
        out
    }
}

impl HcHost for HcHostC {
    fn now_ms(&self) -> u64 {
        // SAFETY: host contract.
        unsafe { (self.now_ms)(self.ctx) }
    }

    fn rendered_frames(&self) -> u32 {
        // SAFETY: host contract.
        unsafe { (self.rendered_frames)(self.ctx) }
    }

    fn log(&self, level: LogLevel, msg: &[u8]) {
        let l = match level {
            LogLevel::Log => 0,
            LogLevel::Warning => 1,
            LogLevel::Error => 2,
        };
        // SAFETY: host contract.
        unsafe { (self.log)(self.ctx, l, msg.as_ptr(), msg.len()) }
    }

    fn exit(&self) {
        // SAFETY: host contract.
        unsafe { (self.exit)(self.ctx) }
    }

    fn set_wait_if_out_of_focus(&self, on: bool) {
        // SAFETY: host contract.
        unsafe { (self.set_wait_focus)(self.ctx, on) }
    }

    fn dev_hud_active(&self) -> bool {
        // SAFETY: host contract.
        unsafe { (self.dev_hud_active)(self.ctx) }
    }

    fn set_dev_hud_active(&self, on: bool) {
        // SAFETY: host contract.
        unsafe { (self.set_dev_hud)(self.ctx, on) }
    }

    fn screen_float_stats(&self) -> Option<(f32, f32, f32)> {
        let (mut a, mut b, mut c) = (0.0, 0.0, 0.0);
        // SAFETY: host contract; the out pointers are live locals.
        let ok = unsafe { (self.screen_stats)(self.ctx, &raw mut a, &raw mut b, &raw mut c) };
        ok.then_some((a, b, c))
    }

    fn force_window_size(&self, w: i32, h: i32) -> bool {
        // SAFETY: host contract.
        unsafe { (self.force_window_size)(self.ctx, w, h) }
    }

    fn shader_fail_count(&self) -> i32 {
        // SAFETY: host contract.
        unsafe { (self.shader_fail_count)(self.ctx) }
    }

    fn shader_report_json(&self, failed_only: bool) -> Vec<u8> {
        // SAFETY: host contract; `collect` matches the sink type.
        self.string(|s, c| unsafe { (self.shader_report_json)(self.ctx, failed_only, s, c) })
    }

    fn frame_stats(&self) -> Option<(Vec<u8>, Vec<u8>)> {
        let (mut frame, mut gl) = (Vec::new(), Vec::new());
        // SAFETY: host contract; both sinks write into live locals.
        let ok = unsafe {
            (self.frame_stats)(
                self.ctx,
                collect,
                std::ptr::from_mut(&mut frame).cast(),
                collect,
                std::ptr::from_mut(&mut gl).cast(),
            )
        };
        ok.then_some((frame, gl))
    }

    fn xdg_cache_home(&self) -> Vec<u8> {
        // SAFETY: host contract.
        self.string(|s, c| unsafe { (self.xdg_cache_home)(self.ctx, s, c) })
    }

    fn ensure_folder(&self, dir: &[u8]) {
        // SAFETY: host contract.
        unsafe { (self.ensure_folder)(self.ctx, dir.as_ptr(), dir.len()) }
    }

    fn screenshot(&self, path: &[u8]) -> Shot {
        // SAFETY: host contract.
        match unsafe { (self.screenshot)(self.ctx, path.as_ptr(), path.len()) } {
            0 => Shot::Saved,
            1 => Shot::CopyFailed,
            _ => Shot::SaveFailed,
        }
    }

    fn key_from_name(&self, name: &[u8]) -> i32 {
        let Ok(c) = CString::new(name.split(|&b| b == 0).next().unwrap_or_default()) else {
            return 0;
        };
        // SAFETY: `c` is NUL-terminated.
        unsafe { sdl2::SDL_GetKeyFromName(c.as_ptr()) }
    }

    fn scancode_from_key(&self, key: i32) -> i32 {
        // SAFETY: plain value call.
        unsafe { sdl2::SDL_GetScancodeFromKey(key) }
    }

    fn push_event(&self, ev: &Ev) {
        // SAFETY: an all-zero SDL_Event is valid; only the active member is written.
        let mut e: sdl2::SDL_Event = unsafe { std::mem::zeroed() };
        match ev {
            Ev::Key {
                down,
                sym,
                scancode,
            } => {
                e.key.type_ = if *down {
                    sdl2::SDL_KEYDOWN
                } else {
                    sdl2::SDL_KEYUP
                };
                e.key.state = if *down {
                    sdl2::SDL_PRESSED
                } else {
                    sdl2::SDL_RELEASED
                };
                e.key.keysym.sym = *sym;
                e.key.keysym.scancode = *scancode;
            }
            Ev::Motion { x, y, xrel, yrel } => {
                e.motion.type_ = sdl2::SDL_MOUSEMOTION;
                e.motion.x = *x;
                e.motion.y = *y;
                e.motion.xrel = *xrel;
                e.motion.yrel = *yrel;
            }
            Ev::Button { button, down, x, y } => {
                e.button.type_ = if *down {
                    sdl2::SDL_MOUSEBUTTONDOWN
                } else {
                    sdl2::SDL_MOUSEBUTTONUP
                };
                e.button.button = match button {
                    MouseButton::Left => sdl2::SDL_BUTTON_LEFT,
                    MouseButton::Middle => sdl2::SDL_BUTTON_MIDDLE,
                    MouseButton::Right => sdl2::SDL_BUTTON_RIGHT,
                };
                e.button.state = if *down {
                    sdl2::SDL_PRESSED
                } else {
                    sdl2::SDL_RELEASED
                };
                e.button.clicks = 1;
                e.button.x = *x;
                e.button.y = *y;
            }
            Ev::Text(t) => {
                // SAFETY: host contract; the text is at most 31 bytes.
                unsafe { (self.inject_text)(self.ctx, t.as_ptr(), t.len()) };
                return;
            }
        }
        // SAFETY: `e` is a fully initialised event.
        unsafe { sdl2::SDL_PushEvent(&raw mut e) };
    }

    fn run_message(&self, msg: i32) {
        // SAFETY: host contract.
        unsafe { (self.run_message)(self.ctx, msg) }
    }
}

/// # Safety
/// `host` is valid; `path` valid for `n` bytes. The host struct is copied; `ctx` must outlive the server.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h22_hc_new(
    host: *const HcHostC,
    path: *const u8,
    n: usize,
) -> *mut HcServer {
    // SAFETY: caller contract.
    let host = unsafe { *host };
    // SAFETY: caller contract.
    let server = HcServer::new(&host, unsafe { units(path, n) });
    Box::into_raw(Box::new(server))
}

/// # Safety
/// `s` came from `ohpl_h22_hc_new` and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h22_hc_free(s: *mut HcServer) {
    if !s.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(s) });
    }
}

/// # Safety
/// `s` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h22_hc_listening(s: *const HcServer) -> bool {
    // SAFETY: caller contract.
    unsafe { (*s).is_listening() }
}

/// # Safety
/// `s` is valid; `line` valid for `n` bytes. Callable from the log hook while `update` runs.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h22_hc_push_log(s: *const HcServer, line: *const u8, n: usize) {
    // SAFETY: caller contract; shared borrow only.
    unsafe { (*s).push_log_line(units(line, n)) }
}

/// # Safety
/// `s` and `host` are valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h22_hc_update(
    s: *const HcServer,
    host: *const HcHostC,
    loading: bool,
) {
    // SAFETY: caller contract; shared borrows, so host re-entry is fine.
    unsafe { (*s).update(&*host, loading) }
}

/// # Safety
/// `s` and `host` are valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h22_hc_logic_step(s: *const HcServer, host: *const HcHostC) {
    // SAFETY: caller contract.
    unsafe { (*s).logic_step(&*host) }
}

pub type LogFn = unsafe extern "C" fn(*mut c_void, i32, *const u8, usize);

/// # Safety
/// `log` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h22_hc_acquire_lock(log: LogFn, ctx: *mut c_void) {
    let env = |k: &str| {
        use std::os::unix::ffi::OsStringExt;
        std::env::var_os(k).map(OsStringExt::into_vec)
    };
    let log = |level: LogLevel, msg: &str| {
        let l = match level {
            LogLevel::Log => 0,
            LogLevel::Warning => 1,
            LogLevel::Error => 2,
        };
        // SAFETY: caller contract.
        unsafe { log(ctx, l, msg.as_ptr(), msg.len()) }
    };
    h22_hc::acquire_single_instance_lock(&env, &log);
}
