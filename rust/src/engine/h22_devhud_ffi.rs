//! C ABI of the DevHud draw sequence (`RustEngineH22.h`).

use super::h22_devhud::{self as hud, HudHost, HudState};
use crate::string_ffi::{Sink, units};
use std::ffi::c_void;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct HudHostC {
    pub ctx: *mut c_void,
    pub app_time_ms: unsafe extern "C" fn(*mut c_void) -> u64,
    pub init: unsafe extern "C" fn(*mut c_void) -> bool,
    pub capture_bug_shot: unsafe extern "C" fn(*mut c_void),
    pub screen_size: unsafe extern "C" fn(*mut c_void, *mut i32, *mut i32),
    pub set_virtual_size: unsafe extern "C" fn(*mut c_void, f32, f32),
    pub fps: unsafe extern "C" fn(*mut c_void) -> f32,
    pub avg_frame_ms: unsafe extern "C" fn(*mut c_void) -> f32,
    pub system_line_count: unsafe extern "C" fn(*mut c_void) -> usize,
    pub system_line: unsafe extern "C" fn(*mut c_void, usize, Sink<u8>, *mut c_void),
    pub font_length: unsafe extern "C" fn(*mut c_void, f32, *const u32, usize) -> f32,
    pub draw_gfx: unsafe extern "C" fn(*mut c_void, f32, f32, f32, f32, f32),
    pub draw_font: unsafe extern "C" fn(
        *mut c_void,
        *const u32,
        usize,
        f32,
        f32,
        f32,
        f32,
        f32,
        f32,
        f32,
        f32,
    ),
    pub set_frame_buffer_null: unsafe extern "C" fn(*mut c_void),
    pub render_set: unsafe extern "C" fn(*mut c_void),
}

unsafe extern "C" fn collect(ctx: *mut c_void, p: *const u8, n: usize) {
    // SAFETY: `ctx` is the `Vec<u8>` the caller passed; `p` is valid for `n` bytes.
    unsafe { (*ctx.cast::<Vec<u8>>()).extend_from_slice(units(p, n)) }
}

impl HudHost for HudHostC {
    fn app_time_ms(&self) -> u64 {
        // SAFETY: host contract.
        unsafe { (self.app_time_ms)(self.ctx) }
    }
    fn init(&self) -> bool {
        // SAFETY: host contract.
        unsafe { (self.init)(self.ctx) }
    }
    fn capture_bug_shot(&self) {
        // SAFETY: host contract.
        unsafe { (self.capture_bug_shot)(self.ctx) }
    }
    fn screen_size(&self) -> (i32, i32) {
        let (mut x, mut y) = (0, 0);
        // SAFETY: host contract; out pointers are live locals.
        unsafe { (self.screen_size)(self.ctx, &raw mut x, &raw mut y) };
        (x, y)
    }
    fn set_virtual_size(&self, w: f32, h: f32) {
        // SAFETY: host contract.
        unsafe { (self.set_virtual_size)(self.ctx, w, h) }
    }
    fn fps(&self) -> f32 {
        // SAFETY: host contract.
        unsafe { (self.fps)(self.ctx) }
    }
    fn avg_frame_ms(&self) -> f32 {
        // SAFETY: host contract.
        unsafe { (self.avg_frame_ms)(self.ctx) }
    }
    fn system_lines(&self) -> Vec<Vec<u8>> {
        // SAFETY: host contract.
        let n = unsafe { (self.system_line_count)(self.ctx) };
        (0..n)
            .map(|i| {
                let mut v = Vec::new();
                // SAFETY: host contract; `collect` matches the sink type.
                unsafe {
                    (self.system_line)(self.ctx, i, collect, std::ptr::from_mut(&mut v).cast());
                }
                v
            })
            .collect()
    }
    fn font_length(&self, size: f32, text: &[u32]) -> f32 {
        // SAFETY: host contract.
        unsafe { (self.font_length)(self.ctx, size, text.as_ptr(), text.len()) }
    }
    fn draw_gfx(&self, pos: [f32; 3], size: [f32; 2]) {
        // SAFETY: host contract.
        unsafe { (self.draw_gfx)(self.ctx, pos[0], pos[1], pos[2], size[0], size[1]) }
    }
    fn draw_font(&self, text: &[u32], pos: [f32; 3], size: f32, c: [f32; 4]) {
        // SAFETY: host contract.
        unsafe {
            (self.draw_font)(
                self.ctx,
                text.as_ptr(),
                text.len(),
                pos[0],
                pos[1],
                pos[2],
                size,
                c[0],
                c[1],
                c[2],
                c[3],
            );
        }
    }
    fn set_frame_buffer_null(&self) {
        // SAFETY: host contract.
        unsafe { (self.set_frame_buffer_null)(self.ctx) }
    }
    fn render_set(&self) {
        // SAFETY: host contract.
        unsafe { (self.render_set)(self.ctx) }
    }
}

/// Returns true when the pending bug capture was taken.
///
/// # Safety
/// `host` valid; `text` valid for `text_n` units, `last` for `last_n` bytes.
#[unsafe(no_mangle)]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ohpl_h22_hud_render(
    host: *const HudHostC,
    hud_active: bool,
    bug_open: bool,
    bug_capture: bool,
    saved_time: u64,
    text: *const u32,
    text_n: usize,
    last: *const u8,
    last_n: usize,
) -> bool {
    // SAFETY: caller contract.
    let (host, text, last) = unsafe { (*host, units(text, text_n), units(last, last_n)) };
    hud::render(
        &host,
        &HudState {
            hud: hud_active,
            bug_open,
            bug_capture,
            saved_time,
            bug_text: text,
            last_saved: last,
        },
    )
}
