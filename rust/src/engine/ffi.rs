//! C ABI for the engine layer (`HPL2/core/include/engine/RustEngine.h`). Byte arguments are
//! (pointer, length) pairs; results go through sink callbacks, so Rust never hands out memory.

use super::frame::{self, Fps, FrameTiming, Loop};
use super::multi_settings::{self as ms, Entry, SlotAction};
use super::save::{self, Cursor, Registry};
use super::setup_vars::SetupVars;
use super::updater::{self, AddResult, Updater, Visit};
use crate::string_ffi::{Sink, emit, units};
use std::ffi::c_void;

// ---- updater ----

pub type VisitFn = unsafe extern "C" fn(*mut c_void, usize, i32, i32, f32, *const u8, usize);

fn visitor(cb: VisitFn, ctx: *mut c_void) -> impl FnMut(usize, Visit<'_>) {
    move |w, v| {
        let (op, msg, x, name): (i32, i32, f32, &[u8]) = match v {
            Visit::Message(m, x) => (0, m, x, &[]),
            Visit::TimedUpdate(x) => (1, updater::MSG_UPDATE, x, &[]),
            Visit::Leave(n) => (2, 0, 0.0, n),
            Visit::Enter(n) => (3, 0, 0.0, n),
        };
        // SAFETY: host callback contract; `name` is valid for the call.
        unsafe { cb(ctx, w, op, msg, x, name.as_ptr(), name.len()) }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_en_upd_new() -> *mut Updater {
    Box::into_raw(Box::default())
}

/// # Safety
/// `u` came from `ohpl_en_upd_new` and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_upd_free(u: *mut Updater) {
    if !u.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(u) });
    }
}

/// # Safety
/// `u` is valid; `name` valid for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_upd_add_container(u: *mut Updater, name: *const u8, n: usize) {
    // SAFETY: caller contract.
    unsafe { (*u).add_container(units(name, n)) }
}

/// 0 unknown container, 1 added, 2 null updatable.
///
/// # Safety
/// `u` is valid; `name` valid for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_upd_add_update(
    u: *mut Updater,
    name: *const u8,
    n: usize,
    upd: usize,
) -> i32 {
    // SAFETY: caller contract.
    match unsafe { (*u).add_update(units(name, n), upd) } {
        AddResult::NoContainer => 0,
        AddResult::Added => 1,
        AddResult::Null => 2,
    }
}

/// # Safety
/// `u` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_upd_add_global(u: *mut Updater, upd: usize) {
    // SAFETY: caller contract.
    unsafe { (*u).add_global(upd) }
}

/// # Safety
/// `u` is valid; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_upd_current_name(
    u: *const Updater,
    sink: Sink<u8>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, (*u).current_name()) }
}

/// # Safety
/// `u` is valid; `cb` may re-enter the updater through `u`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_upd_run_message(
    u: *mut Updater,
    msg: i32,
    x: f32,
    cb: VisitFn,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    unsafe { updater::run_message(u, msg, x, &mut visitor(cb, ctx)) }
}

/// # Safety
/// As `ohpl_en_upd_run_message`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_upd_broadcast(
    u: *mut Updater,
    msg: i32,
    x: f32,
    cb: VisitFn,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    unsafe { updater::broadcast(u, msg, x, &mut visitor(cb, ctx)) }
}

/// # Safety
/// As `ohpl_en_upd_run_message`; `name` valid for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_upd_set_container(
    u: *mut Updater,
    name: *const u8,
    n: usize,
    cb: VisitFn,
    ctx: *mut c_void,
) -> bool {
    // SAFETY: caller contract.
    unsafe { updater::set_container(u, units(name, n), &mut visitor(cb, ctx)) }
}

// ---- fps counter and loop state ----

/// # Safety
/// `f` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_fps_init(f: *mut Fps, now: u64) {
    // SAFETY: caller contract.
    unsafe { f.write(Fps::new(now)) }
}

/// # Safety
/// `f` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_fps_add_frame(f: *mut Fps, now: u64) {
    // SAFETY: caller contract.
    unsafe { (*f).add_frame(now) }
}

/// # Safety
/// `f` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_fps_avg_frame_ms(f: *const Fps) -> f32 {
    // SAFETY: caller contract.
    unsafe { (*f).avg_frame_ms() }
}

/// # Safety
/// `l` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_loop_init(l: *mut Loop) {
    // SAFETY: caller contract.
    unsafe { l.write(Loop::default()) }
}

macro_rules! loop_fn {
    ($name:ident, $m:ident $(, $a:ident: $t:ty)*) => {
        /// # Safety
        /// `l` is valid.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name(l: *mut Loop $(, $a: $t)*) {
            // SAFETY: caller contract.
            unsafe { (*l).$m($($a),*) }
        }
    };
    ($name:ident, $m:ident $(, $a:ident: $t:ty)* => $r:ty) => {
        /// # Safety
        /// `l` is valid.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name(l: *mut Loop $(, $a: $t)*) -> $r {
            // SAFETY: caller contract.
            unsafe { (*l).$m($($a),*) }
        }
    };
}

loop_fn!(ohpl_en_loop_begin_run, begin_run);
loop_fn!(ohpl_en_loop_step_begin, step_begin);
loop_fn!(ohpl_en_loop_step_end, step_end);
loop_fn!(ohpl_en_loop_pause_tick, pause_tick);
loop_fn!(ohpl_en_loop_advance_game_time, advance_game_time, step: f32);
loop_fn!(ohpl_en_loop_update_log_tick, update_log_tick => bool);
loop_fn!(ohpl_en_loop_add_logic_ms, add_logic_ms, ms: f64);
loop_fn!(ohpl_en_loop_add_swap_ms, add_swap_ms, ms: f64);
loop_fn!(ohpl_en_loop_skip_render_once, skip_render_once, render_once: bool => bool);
loop_fn!(ohpl_en_loop_take_swap, take_swap => bool);
loop_fn!(ohpl_en_loop_swap_done, swap_done, render_once: bool => bool);
loop_fn!(ohpl_en_loop_want_render, want_render, limit_fps: bool => bool);
loop_fn!(ohpl_en_loop_mean_framerate, mean_framerate, elapsed_ms: u64 => f64);

/// # Safety
/// `l` and `out` are valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_loop_frame_rendered(l: *mut Loop, out: *mut FrameTiming) {
    // SAFETY: caller contract.
    unsafe { out.write((*l).frame_rendered()) }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_en_frame_time(timer_sec: f64) -> f32 {
    frame::frame_time(timer_sec)
}

fn out_msgs(v: &[i32], out: *mut i32) -> usize {
    for (i, m) in v.iter().enumerate() {
        // SAFETY: the callers' `out` holds 8 slots and `v` has at most 6 entries.
        unsafe { out.add(i).write(*m) };
    }
    v.len()
}

/// `had`/`now` bits: 1 input focus, 2 mouse focus, 4 visible. Writes at most 6 messages.
///
/// # Safety
/// `out` has room for 8 `i32`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_focus_messages(had: u32, now: u32, out: *mut i32) -> usize {
    let bits = |v: u32| [v & 1 != 0, v & 2 != 0, v & 4 != 0];
    out_msgs(&frame::focus_messages(bits(had), bits(now)), out)
}

/// `env_first` is the first byte of `OPENHPL_DEV_HUD`, or -1 when unset or empty.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_en_dev_hud_active(requested: bool, env_first: i32) -> bool {
    frame::dev_hud_active(requested, u8::try_from(env_first).ok())
}

/// # Safety
/// `s` valid for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_engine_type_from_string(s: *const u8, n: usize) -> i32 {
    // SAFETY: caller contract.
    frame::engine_type_from_string(unsafe { units(s, n) })
}

/// # Safety
/// `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_engine_type_name(
    i: i32,
    sink: Sink<u8>,
    ctx: *mut c_void,
) -> bool {
    let Some(t) = usize::try_from(i)
        .ok()
        .and_then(|i| frame::VARIABLE_TYPES.get(i))
    else {
        return false;
    };
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, t.as_bytes()) };
    true
}

// ---- setup variables ----

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_en_vars_new() -> *mut SetupVars {
    Box::into_raw(Box::default())
}

/// # Safety
/// `v` came from `ohpl_en_vars_new` and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_vars_free(v: *mut SetupVars) {
    if !v.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(v) });
    }
}

/// # Safety
/// `v` valid; byte arguments valid for their lengths.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_vars_add_string(
    v: *mut SetupVars,
    name: *const u8,
    n: usize,
    val: *const u8,
    val_n: usize,
) {
    // SAFETY: caller contract.
    unsafe { (*v).add_string(units(name, n), units(val, val_n)) }
}

/// # Safety
/// `v` valid; `name` valid for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_vars_add_int(
    v: *mut SetupVars,
    name: *const u8,
    n: usize,
    x: i32,
) {
    // SAFETY: caller contract.
    unsafe { (*v).add_int(units(name, n), x) }
}

/// # Safety
/// `v` valid; `name` valid for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_vars_add_float(
    v: *mut SetupVars,
    name: *const u8,
    n: usize,
    x: f32,
) {
    // SAFETY: caller contract.
    unsafe { (*v).add_float(units(name, n), x) }
}

/// # Safety
/// `v` valid; `name` valid for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_vars_add_bool(
    v: *mut SetupVars,
    name: *const u8,
    n: usize,
    x: bool,
) {
    // SAFETY: caller contract.
    unsafe { (*v).add_bool(units(name, n), x) }
}

/// # Safety
/// `v` valid; `name` valid for `n` bytes; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_vars_get_string(
    v: *const SetupVars,
    name: *const u8,
    n: usize,
    sink: Sink<u8>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, (*v).get_string(units(name, n))) }
}

/// # Safety
/// `v` valid; `name` valid for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_vars_get_float(
    v: *const SetupVars,
    name: *const u8,
    n: usize,
    default: f32,
) -> f32 {
    // SAFETY: caller contract.
    unsafe { (*v).get_float(units(name, n), default) }
}

/// # Safety
/// `v` valid; `name` valid for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_vars_get_int(
    v: *const SetupVars,
    name: *const u8,
    n: usize,
    default: i32,
) -> i32 {
    // SAFETY: caller contract.
    unsafe { (*v).get_int(units(name, n), default) }
}

/// # Safety
/// `v` valid; `name` valid for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_vars_get_bool(
    v: *const SetupVars,
    name: *const u8,
    n: usize,
    default: bool,
) -> bool {
    // SAFETY: caller contract.
    unsafe { (*v).get_bool(units(name, n), default) }
}

// ---- multiple settings ----

/// # Safety
/// `e` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_ms_fade_to(e: *mut Entry, dest: f32, speed: f32, destroy: bool) {
    // SAFETY: caller contract.
    unsafe { (*e).fade_to(dest, speed, destroy) }
}

/// # Safety
/// `e` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_ms_set_val_and_dest(e: *mut Entry, val: f32) {
    // SAFETY: caller contract.
    unsafe { (*e).set_val_and_dest(val) }
}

/// True when the entry reached its destination and asked to be destroyed.
///
/// # Safety
/// `e` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_ms_update(e: *mut Entry, dt: f32) -> bool {
    // SAFETY: caller contract.
    unsafe { (*e).update(dt) }
}

/// # Safety
/// `slots` holds `n` pointers, each null or a valid entry.
unsafe fn slot_entries(slots: *const *const Entry, n: usize) -> Vec<Option<Entry>> {
    // SAFETY: caller contract.
    unsafe { units(slots, n) }
        .iter()
        // SAFETY: caller contract.
        .map(|p| unsafe { p.as_ref() }.copied())
        .collect()
}

/// # Safety
/// `slots` holds `n` pointers, each null or a valid entry.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_ms_calc_results(
    slots: *const *const Entry,
    n: usize,
    types: u32,
    calc: i32,
    default: f32,
) -> f32 {
    // SAFETY: caller contract.
    ms::calc_results(&unsafe { slot_entries(slots, n) }, types, calc, default)
}

/// 0 null, 1 existing, 2 create in place, 3 resize then create.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_en_ms_slot_action(idx: i32, len: usize, occupied: bool, force: bool) -> i32 {
    match ms::get_slot_action(idx, len, occupied, force) {
        SlotAction::Null => 0,
        SlotAction::Existing => 1,
        SlotAction::Create => 2,
        SlotAction::ResizeAndCreate => 3,
    }
}

/// # Safety
/// `slots` holds `n` pointers, each null or a valid entry.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_ms_create_slot(slots: *const *const Entry, n: usize) -> usize {
    // SAFETY: caller contract.
    let occupied: Vec<bool> = unsafe { units(slots, n) }
        .iter()
        .map(|p| !p.is_null())
        .collect();
    ms::create_slot(&occupied)
}

/// # Safety
/// `out` holds 5 `usize`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_ms_entry_layout(out: *mut usize) -> usize {
    for (i, o) in ms::ENTRY_OFFSETS.iter().enumerate() {
        // SAFETY: caller contract.
        unsafe { out.add(i).write(*o) };
    }
    size_of::<Entry>()
}

// ---- save registry ----

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_en_reg_new() -> *mut Registry {
    Box::into_raw(Box::default())
}

/// # Safety
/// `r` came from `ohpl_en_reg_new` and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_reg_free(r: *mut Registry) {
    if !r.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(r) });
    }
}

/// # Safety
/// `r` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_reg_add(r: *mut Registry, key: i32, obj: usize) {
    // SAFETY: caller contract.
    unsafe { (*r).add(key, obj) }
}

/// 0 when absent.
///
/// # Safety
/// `r` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_reg_get(r: *const Registry, key: i32) -> usize {
    // SAFETY: caller contract.
    unsafe { (*r).get(key) }.unwrap_or(0)
}

/// # Safety
/// `r` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_reg_size(r: *const Registry) -> usize {
    // SAFETY: caller contract.
    unsafe { (*r).len() }
}

/// # Safety
/// `r` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_reg_clear(r: *mut Registry) {
    // SAFETY: caller contract.
    unsafe { (*r).clear() }
}

/// # Safety
/// `r` and `c` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_reg_has_next(r: *const Registry, c: *const Cursor) -> bool {
    // SAFETY: caller contract.
    unsafe { (*r).has_next(&*c) }
}

/// 0 at the end.
///
/// # Safety
/// `r` and `c` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_reg_peek(r: *const Registry, c: *const Cursor) -> usize {
    // SAFETY: caller contract.
    unsafe { (*r).peek(&*c) }.unwrap_or(0)
}

/// 0 at the end.
///
/// # Safety
/// `r` and `c` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_reg_next(r: *const Registry, c: *mut Cursor) -> usize {
    // SAFETY: caller contract.
    unsafe { (*r).next(&mut *c) }.unwrap_or(0)
}

/// # Safety
/// `counter` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_save_next_id(counter: *mut i32) -> i32 {
    // SAFETY: caller contract.
    save::next_id(unsafe { &mut *counter })
}

/// Visits every registered object; `visit` returns the object's id. Returns the new id counter.
///
/// # Safety
/// `r` valid; `visit` may re-enter the registry through `r`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en_reg_set_up_all(
    r: *mut Registry,
    visit: unsafe extern "C" fn(*mut c_void, usize) -> i32,
    ctx: *mut c_void,
) -> i32 {
    // SAFETY: host callback contract.
    unsafe { save::set_up_all(r, &mut |o| visit(ctx, o)) }
}
