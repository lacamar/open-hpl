//! C ABI for `devhud.rs` (`HPL2/core/include/engine/RustEngine.h`).

use super::devhud::{self, BugBox, BugMetrics, HudLayout, KeyAction, Report, Scene, SystemInfo};
use crate::string_ffi::{Sink, emit, units};
use std::ffi::c_void;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Bytes {
    pub ptr: *const u8,
    pub len: usize,
}

impl Bytes {
    /// # Safety
    /// `ptr` valid for `len` bytes.
    unsafe fn get<'a>(self) -> &'a [u8] {
        // SAFETY: caller contract.
        unsafe { units(self.ptr, self.len) }
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct BytesPair {
    pub key: Bytes,
    pub val: Bytes,
}

#[repr(C)]
pub struct OhplEnReport {
    pub id: Bytes,
    pub time: Bytes,
    pub description: Bytes,
    pub version: Bytes,
    pub exe: Bytes,
    pub cwd: Bytes,
    pub args: *const Bytes,
    pub args_n: usize,
    pub env: *const BytesPair,
    pub env_n: usize,
    pub system: *const Bytes,
    pub system_n: usize,
    pub res_x: i32,
    pub res_y: i32,
    pub fps: f32,
    pub frame_ms: f32,
    pub game_time: f32,
    pub has_scene: bool,
    pub map: Bytes,
    pub pos: [f32; 3],
    pub forward: [f32; 3],
    pub world_json: Bytes,
    pub render_json: Bytes,
    pub files: Bytes,
}

#[repr(C)]
pub struct OhplEnSystemInfo {
    pub version: Bytes,
    pub device: Bytes,
    pub machine: Bytes,
    pub cores: i32,
    pub mem_gb: i32,
    pub os: Bytes,
    pub kernel: Bytes,
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_en_hud_font_size(screen_y: i32) -> f32 {
    devhud::hud_font_size(screen_y)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_en_hud_layout(font: f32, max_width: f32, lines: usize) -> HudLayout {
    devhud::hud_layout(font, max_width, lines)
}

/// # Safety
/// `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_hud_stats_line(
    w: i32,
    h: i32,
    fps: f32,
    frame_ms: f32,
    sink: Sink<u8>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, &devhud::stats_line(w, h, fps, frame_ms)) }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_en_bug_metrics(sx: i32, sy: i32) -> BugMetrics {
    devhud::bug_metrics(sx, sy)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_en_bug_toast_box_w(m: BugMetrics, path_width: f32) -> f32 {
    devhud::toast_box_w(&m, path_width)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_en_bug_box(
    m: BugMetrics,
    box_w: f32,
    lines: usize,
    sx: i32,
    sy: i32,
) -> BugBox {
    devhud::bug_box(&m, box_w, lines, sx, sy)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_en_toast_active(saved_ms: u64, now_ms: u64) -> bool {
    devhud::toast_active(saved_ms, now_ms)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_en_should_render(hud: bool, bug_open: bool, toast: bool) -> bool {
    devhud::should_render(hud, bug_open, toast)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_en_cursor_char(now_ms: u64) -> u32 {
    devhud::cursor_char(now_ms)
}

/// Wraps `text` and reports each line through `line`; `measure` returns the font width of a
/// NUL-terminated wide string it is given as a (pointer, length) pair.
///
/// # Safety
/// `text` valid for `n` units; callbacks accept `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_wrap_bug_text(
    text: *const u32,
    n: usize,
    text_w: f32,
    measure: unsafe extern "C" fn(*mut c_void, *const u32, usize) -> f32,
    line: Sink<u32>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    let text = unsafe { units(text, n) };
    // SAFETY: host callback contract.
    let lines = devhud::wrap_bug_text(text, text_w, &mut |l| unsafe {
        measure(ctx, l.as_ptr(), l.len())
    });
    for l in lines {
        // SAFETY: host callback contract.
        unsafe { emit(line, ctx, &l) }
    }
}

/// Edits the text in place through the callbacks: `sink` receives the new text.
///
/// # Safety
/// `text` valid for `n` units; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_ctrl_backspace(
    text: *const u32,
    n: usize,
    sink: Sink<u32>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    let mut v = unsafe { units(text, n) }.to_vec();
    devhud::ctrl_backspace(&mut v);
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, &v) }
}

/// 0 nothing, 1 close, 2 newline, 3 save, 4 ctrl+backspace, 5 backspace, 6 paste if clipboard.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_en_key_action(key: i32, ctrl: bool, shift: bool, text_empty: bool) -> i32 {
    match devhud::key_action(key, ctrl, shift, text_empty) {
        KeyAction::Nothing => 0,
        KeyAction::Close => 1,
        KeyAction::NewLine => 2,
        KeyAction::Save => 3,
        KeyAction::CtrlBackspace => 4,
        KeyAction::Backspace => 5,
        KeyAction::PasteIfClipboard => 6,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_en_swallowed_event(ty: u32) -> bool {
    devhud::swallowed_event(ty)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_en_opens_report(ty: u32, key: i32, repeat: bool) -> bool {
    devhud::opens_report(ty, key, repeat)
}

/// # Safety
/// `file` valid for `n` bytes; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_first_line(
    file: *const u8,
    n: usize,
    sink: Sink<u8>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, devhud::first_line(units(file, n))) }
}

/// # Safety
/// `file` valid for `n` bytes; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_os_name(
    file: *const u8,
    n: usize,
    sink: Sink<u8>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, &devhud::os_name(units(file, n))) }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_en_mem_gb(pages: i64, page_size: i64) -> i64 {
    devhud::mem_gb(pages, page_size)
}

/// Emits the three lines one by one.
///
/// # Safety
/// `i` and its byte fields valid; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_system_lines(
    i: *const OhplEnSystemInfo,
    sink: Sink<u8>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    unsafe {
        let i = &*i;
        let info = SystemInfo {
            version: i.version.get(),
            device: i.device.get(),
            machine: i.machine.get(),
            cores: i.cores,
            mem_gb: i.mem_gb,
            os: i.os.get(),
            kernel: i.kernel.get(),
        };
        for l in devhud::system_lines(&info) {
            emit(sink, ctx, &l);
        }
    }
}

/// # Safety
/// `base` valid for `n` bytes; callbacks accept `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_unique_name(
    base: *const u8,
    n: usize,
    exists: unsafe extern "C" fn(*mut c_void, *const u8, usize) -> bool,
    sink: Sink<u8>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    let name = devhud::unique_name(unsafe { units(base, n) }, &mut |s| unsafe {
        exists(ctx, s.as_ptr(), s.len())
    });
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, &name) }
}

/// # Safety
/// `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_files_json(
    screenshot: bool,
    log: bool,
    sink: Sink<u8>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, &devhud::files_json(screenshot, log)) }
}

/// Returns false when `var` is not an `OPENHPL_`/`OHPL_` variable; else emits name then value.
///
/// # Safety
/// `var` valid for `n` bytes; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_env_pair(
    var: *const u8,
    n: usize,
    sink: Sink<u8>,
    ctx: *mut c_void,
) -> bool {
    // SAFETY: caller contract.
    let Some((k, v)) = devhud::env_pair(unsafe { units(var, n) }) else {
        return false;
    };
    // SAFETY: caller contract.
    unsafe {
        emit(sink, ctx, k);
        emit(sink, ctx, v);
    }
    true
}

/// Emits each `/proc/self/cmdline` argument.
///
/// # Safety
/// `raw` valid for `n` bytes; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_cmdline_args(
    raw: *const u8,
    n: usize,
    sink: Sink<u8>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    unsafe {
        for a in devhud::cmdline_args(units(raw, n)) {
            emit(sink, ctx, a);
        }
    }
}

/// # Safety
/// All pointers in `r` valid for their lengths; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_report_json(
    r: *const OhplEnReport,
    sink: Sink<u8>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    unsafe {
        let r = &*r;
        let args: Vec<&[u8]> = units(r.args, r.args_n).iter().map(|b| b.get()).collect();
        let env: Vec<(&[u8], &[u8])> = units(r.env, r.env_n)
            .iter()
            .map(|p| (p.key.get(), p.val.get()))
            .collect();
        let system: Vec<&[u8]> = units(r.system, r.system_n)
            .iter()
            .map(|b| b.get())
            .collect();
        let rep = Report {
            id: r.id.get(),
            time: r.time.get(),
            description: r.description.get(),
            version: r.version.get(),
            exe: r.exe.get(),
            cwd: r.cwd.get(),
            args: &args,
            env: &env,
            system: &system,
            resolution: (r.res_x, r.res_y),
            fps: r.fps,
            frame_ms: r.frame_ms,
            game_time: r.game_time,
            scene: r.has_scene.then(|| Scene {
                map: r.map.get(),
                pos: r.pos,
                forward: r.forward,
                world_json: r.world_json.get(),
                render_json: r.render_json.get(),
            }),
            files: r.files.get(),
        };
        emit(sink, ctx, &devhud::report_json(&rep));
    }
}

/// # Safety
/// Byte arguments valid for their lengths; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_index_line(
    id: Bytes,
    map: Bytes,
    description: Bytes,
    dir: Bytes,
    sink: Sink<u8>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    unsafe {
        emit(
            sink,
            ctx,
            &devhud::index_line(id.get(), map.get(), description.get(), dir.get()),
        )
    }
}
