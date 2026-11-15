//! C ABI of the sound layer for the shell classes in `soma/src/game/SomaSound.h`.

use super::game::{self, game};
use super::host::Engine;
use super::instance::State;
use super::{glue, natives};
use crate::string_ffi::units;
use std::ffi::c_void;

type Sink = unsafe extern "C" fn(*mut c_void, *const u8, usize);

/// # Safety
/// `name` is readable for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s7_inst_new(
    shell: usize,
    event: usize,
    name: *const u8,
    n: usize,
    entity: usize,
    has_entity: bool,
    ty: i32,
) -> usize {
    // SAFETY: caller contract.
    game::inst_new(
        shell,
        event,
        unsafe { units(name, n) },
        entity,
        has_entity,
        ty,
    )
}

/// The scalar state of instance `h`, valid until it is destroyed.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s7_inst_state(h: usize) -> *mut State {
    game()
        .bank
        .inst_mut(h)
        .map_or(std::ptr::null_mut(), |i| &raw mut i.st)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s7_inst_destroy(h: usize) {
    game::inst_destroy(h);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s7_inst_play(h: usize) {
    game().bank.play(&mut Engine, h);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s7_inst_stop(h: usize) {
    game().bank.stop(&mut Engine, h);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s7_inst_stop_now(h: usize) {
    game().bank.stop_now(&mut Engine, h);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s7_inst_fade_out(h: usize, speed: f32) {
    game().bank.fade_out(&mut Engine, h, speed);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s7_inst_fade_in_to(h: usize, mul: f32, speed: f32) {
    game().bank.fade_in_to(&mut Engine, h, mul, speed);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s7_inst_fade_volume_mul_to(h: usize, dest: f32, speed: f32) {
    game().bank.fade_volume_mul_to(h, dest, speed);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s7_inst_fade_speed_mul_to(h: usize, dest: f32, speed: f32) {
    game().bank.fade_speed_mul_to(h, dest, speed);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s7_inst_update(h: usize, dt: f32) {
    game().bank.update(&mut Engine, h, dt);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s7_inst_set_paused(h: usize, on: bool) {
    game().bank.set_paused(&mut Engine, h, on);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s7_inst_set_param(h: usize, idx: i32, v: f32) {
    game().bank.set_param(h, idx, v);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s7_inst_param_value(h: usize, idx: i32) -> f32 {
    natives::param_value(h, idx)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s7_inst_param_count(h: usize) -> i32 {
    natives::param_count(h)
}

/// # Safety
/// `name` is readable for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s7_inst_param_index(h: usize, name: *const u8, n: usize) -> i32 {
    // SAFETY: caller contract.
    game().bank.param_index(h, unsafe { units(name, n) })
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s7_inst_total_time(h: usize) -> f32 {
    game::total_time(h)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s7_inst_is_stopped(h: usize) -> bool {
    game::is_stopped(h)
}

/// # Safety
/// `pos` is three floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s7_inst_set_position(shell: usize, h: usize, pos: *const f32) {
    // SAFETY: caller contract.
    glue::set_position(shell, h, unsafe { pos.cast::<[f32; 3]>().read_unaligned() });
}

/// # Safety
/// `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s7_inst_describe(h: usize, sink: Sink, ctx: *mut c_void) {
    let d = natives::describe(h);
    // SAFETY: caller contract.
    unsafe { sink(ctx, d.as_ptr(), d.len()) };
}

/// # Safety
/// `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s7_event_name(event: usize, sink: Sink, ctx: *mut c_void) {
    let n = glue::event_name(event);
    // SAFETY: caller contract.
    unsafe { sink(ctx, n.as_ptr(), n.len()) };
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s7_event_of_data(data: usize) -> i64 {
    glue::event_of_data(data).map_or(-1, |i| i as i64)
}

/// # Safety
/// `name` is readable for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s7_resolve(name: *const u8, n: usize) -> usize {
    // SAFETY: caller contract.
    glue::resolve(unsafe { units(name, n) })
}

/// # Safety
/// `name` is readable for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s7_preload(name: *const u8, n: usize) {
    // SAFETY: caller contract.
    glue::load_project(unsafe { units(name, n) });
}

/// # Safety
/// `name` is readable for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s7_play_gui(
    name: *const u8,
    n: usize,
    volume: f32,
    ty: i32,
    looped: bool,
    stream: bool,
) -> usize {
    // SAFETY: caller contract.
    glue::play_gui(unsafe { units(name, n) }, volume, ty, looped, stream)
}

/// # Safety
/// `name` is readable for `n` bytes; `pos` is three floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s7_play_3d(
    name: *const u8,
    n: usize,
    volume: f32,
    pos: *const f32,
    ty: i32,
) -> usize {
    // SAFETY: caller contract.
    unsafe {
        glue::play_3d(
            units(name, n),
            volume,
            pos.cast::<[f32; 3]>().read_unaligned(),
            ty,
        )
    }
}

/// # Safety
/// `name` is readable for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s7_find_instance(name: *const u8, n: usize) -> usize {
    // SAFETY: caller contract.
    game::find_by_name(unsafe { units(name, n) })
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s7_is_live(shell: usize, id: i32) -> bool {
    game().live(shell, id)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s7_fade_out_all(types: u32, speed: f32) {
    for h in game::fade_out_all(types) {
        game().bank.fade_out(&mut Engine, h, speed);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s7_update(dt: f32, paused: bool) {
    game::update_events(dt);
    game().music.update();
    game().bank.dump_frame(paused);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s7_pause_update(dt: f32, paused: bool) {
    game::update_events(dt);
    game().bank.dump_frame(paused);
}

/// # Safety
/// `engine` is a live `asIScriptEngine`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s7_register_natives(engine: *mut c_void) {
    natives::register(engine);
}

/// # Safety
/// `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s7_music_serialize(sink: Sink, ctx: *mut c_void) {
    let d = game().music.serialize();
    // SAFETY: caller contract.
    unsafe { sink(ctx, d.as_ptr(), d.len()) };
}

/// # Safety
/// `data` is readable for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s7_music_deserialize(data: *const u8, n: usize) {
    // SAFETY: caller contract.
    game().music.deserialize(unsafe { units(data, n) });
}
