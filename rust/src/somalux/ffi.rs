//! C ABI for the Lux game layer, mirrored in `soma/src/game/RustSomaLux.h`.

use super::game::{self, Edge, Route};
use super::pause::{self, Op, Pause};
use super::player::{Fade, MoveSpeeds, MoveVec};
use super::scriptable;
use super::transfer::{self, AfterLoad, Body, Preload, Rel, Restore, Take, Taken};
use crate::string_ffi::units;
use crate::types::{Matrixf, Vector3f};

const _: () = {
    use std::mem::{offset_of, size_of};
    assert!(size_of::<Fade>() == 20);
    assert!(size_of::<MoveVec>() == 44 && offset_of!(MoveVec, moving) == 40);
    assert!(size_of::<MoveSpeeds>() == 48);
    assert!(size_of::<Pause>() == 8 && offset_of!(Pause, saving_disabled_time) == 4);
    assert!(size_of::<Preload>() == 16 && offset_of!(Preload, debt_ms) == 8);
};

/// # Safety
/// `p` is valid for reads.
unsafe fn rd<T: Copy>(p: *const T) -> T {
    // SAFETY: caller contract.
    unsafe { *p }
}

/// # Safety
/// `p` is valid for writes.
unsafe fn wr<T>(p: *mut T, v: T) {
    // SAFETY: caller contract.
    unsafe { p.write(v) }
}

/// # Safety
/// `p` is valid and exclusively borrowed.
unsafe fn rw<'a, T>(p: *mut T) -> &'a mut T {
    // SAFETY: caller contract.
    unsafe { &mut *p }
}

/// # Safety
/// `p` points to `n` readable bytes.
unsafe fn bytes<'a>(p: *const u8, n: usize) -> &'a [u8] {
    // SAFETY: caller contract.
    unsafe { units(p, n) }
}

// scriptable

/// # Safety
/// `p` points to `n` readable bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs3_hash64(p: *const u8, n: usize) -> u64 {
    // SAFETY: caller contract.
    scriptable::hash64(unsafe { bytes(p, n) })
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs3_easing(kind: i32, t: f32) -> f32 {
    scriptable::easing(kind, t)
}

// map change and preload

/// # Safety
/// Both pointers cover their lengths.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs3_same_map(
    a: *const u8,
    an: usize,
    b: *const u8,
    bn: usize,
) -> bool {
    // SAFETY: caller contract.
    unsafe { transfer::same_map(bytes(a, an), bytes(b, bn)) }
}

#[repr(C)]
pub struct Begin {
    pub use_transfer: bool,
    pub preloaded: bool,
    pub lookup_area: bool,
}

/// # Safety
/// Pointers cover their lengths; `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs3_map_begin(
    start_empty: bool,
    pending_transfer_empty: bool,
    has_map: bool,
    preload: *const u8,
    preload_n: usize,
    map: *const u8,
    map_n: usize,
    out: *mut Begin,
) {
    // SAFETY: caller contract.
    let b = unsafe {
        transfer::begin(
            start_empty,
            pending_transfer_empty,
            has_map,
            bytes(preload, preload_n),
            bytes(map, map_n),
        )
    };
    // SAFETY: caller contract.
    unsafe {
        wr(
            out,
            Begin {
                use_transfer: b.use_transfer,
                preloaded: b.preloaded,
                lookup_area: b.lookup_area,
            },
        )
    }
}

/// # Safety
/// `m` is readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs3_start_yaw(m: *const Matrixf) -> f32 {
    // SAFETY: caller contract.
    transfer::start_yaw(&unsafe { rd(m) })
}

/// # Safety
/// Pointers are valid; `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs3_capture(
    area: *const Matrixf,
    feet: *const Vector3f,
    yaw: f32,
    active_size: i32,
    has_camera: bool,
    pitch: f32,
    out: *mut Rel,
) {
    // SAFETY: caller contract.
    unsafe {
        let body = Body {
            feet: rd(feet),
            yaw,
            active_size,
        };
        wr(
            out,
            transfer::capture(&rd(area), &body, has_camera.then_some(pitch)),
        )
    }
}

/// # Safety
/// Pointers are readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs3_carries(
    inv: *const Matrixf,
    pos: *const Vector3f,
    size: *const Vector3f,
) -> bool {
    // SAFETY: caller contract.
    unsafe { transfer::carries(&rd(inv), rd(pos), rd(size)) }
}

#[repr(C)]
pub struct Arrival {
    pub feet: Vector3f,
    pub yaw: f32,
    pub crouching: bool,
}

/// # Safety
/// Pointers are valid; `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs3_arrive(
    new_area: *const Matrixf,
    rel: *const Rel,
    out: *mut Arrival,
) {
    // SAFETY: caller contract.
    let a = unsafe { transfer::arrive(&rd(new_area), &rd(rel)) };
    // SAFETY: caller contract.
    unsafe {
        wr(
            out,
            Arrival {
                feet: a.feet,
                yaw: a.yaw,
                crouching: a.crouching,
            },
        )
    }
}

/// 0 load failed, 1 start-area placement only, 2 restore the transferred state.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs3_after_load(loaded: bool, captured: bool, has_current_map: bool) -> i32 {
    match transfer::after_load(loaded, captured, has_current_map) {
        AfterLoad::LoadFailed => 0,
        AfterLoad::Done => 1,
        AfterLoad::Transfer => 2,
    }
}

/// 0 skip, 1 set the matrices, 2 make the prop dynamic first.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs3_restore_prop(found: bool, bodies_match: bool, is_static: bool) -> i32 {
    match transfer::restore_prop(found, bodies_match, is_static) {
        Restore::Skip => 0,
        Restore::Set => 1,
        Restore::MakeDynamicThenSet => 2,
    }
}

/// # Safety
/// `p` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs3_preload_init(p: *mut Preload) {
    // SAFETY: caller contract.
    unsafe { wr(p, Preload::default()) }
}

#[repr(C)]
pub struct Tick {
    pub drop_old: bool,
    pub start: bool,
    pub step: bool,
    pub budget_ms: u64,
}

/// # Safety
/// Pointers cover their lengths; `st` is exclusively borrowed and `out` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs3_preload_tick(
    st: *mut Preload,
    want: *const u8,
    want_n: usize,
    loaded: *const u8,
    loaded_n: usize,
    running: bool,
    frame: u32,
    out: *mut Tick,
) {
    // SAFETY: caller contract.
    let t = unsafe {
        transfer::preload_tick(
            rw(st),
            bytes(want, want_n),
            bytes(loaded, loaded_n),
            running,
            frame,
        )
    };
    // SAFETY: caller contract.
    unsafe {
        wr(
            out,
            Tick {
                drop_old: t.drop_old,
                start: t.start,
                step: t.step,
                budget_ms: t.budget_ms,
            },
        )
    }
}

/// # Safety
/// `st` is exclusively borrowed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs3_preload_stepped(
    st: *mut Preload,
    elapsed_ms: u64,
    budget_ms: u64,
) {
    // SAFETY: caller contract.
    transfer::preload_stepped(unsafe { rw(st) }, elapsed_ms, budget_ms)
}

/// 0 nothing preloads, 1 finish and discard, 2 finish and keep.
///
/// # Safety
/// Pointers cover their lengths.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs3_take_plan(
    loaded: *const u8,
    loaded_n: usize,
    map: *const u8,
    map_n: usize,
) -> i32 {
    // SAFETY: caller contract.
    match unsafe { transfer::take_plan(bytes(loaded, loaded_n), bytes(map, map_n)) } {
        Take::None => 0,
        Take::Finish { same: false } => 1,
        Take::Finish { same: true } => 2,
    }
}

/// 0 hand the world out, 1 discard the entities, 2 discard the entities and destroy the world.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs3_taken(have_world: bool, same: bool) -> i32 {
    match transfer::taken(have_world, same) {
        Taken::Use => 0,
        Taken::Discard {
            destroy_world: false,
        } => 1,
        Taken::Discard {
            destroy_world: true,
        } => 2,
    }
}

/// # Safety
/// Pointers cover their lengths.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs3_preload_ready(
    loaded: *const u8,
    loaded_n: usize,
    running: bool,
    map: *const u8,
    map_n: usize,
) -> bool {
    // SAFETY: caller contract.
    unsafe { transfer::preload_ready(bytes(loaded, loaded_n), running, bytes(map, map_n)) }
}

// pause

/// # Safety
/// `p` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs3_pause_init(p: *mut Pause) {
    // SAFETY: caller contract.
    unsafe { wr(p, Pause::default()) }
}

#[repr(C)]
pub struct OpOut {
    pub code: i32,
    pub arg: u32,
}

pub const OP_CAPACITY: usize = 12;

/// Writes at most `OP_CAPACITY` ops to `ops` in replay order and returns their number.
///
/// # Safety
/// `st` is exclusively borrowed; `ops` is writable for `OP_CAPACITY` entries.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs3_set_paused(
    st: *mut Pause,
    script_paused: bool,
    pause: bool,
    music: bool,
    sounds: bool,
    ambience: bool,
    types: u32,
    ops: *mut OpOut,
) -> usize {
    // SAFETY: caller contract.
    let list = pause::set_paused(
        unsafe { rw(st) },
        script_paused,
        pause,
        music,
        sounds,
        ambience,
        types,
    );
    for (i, op) in list.iter().take(OP_CAPACITY).enumerate() {
        let (code, arg) = match *op {
            Op::SetScriptPaused(b) => (0, u32::from(b)),
            Op::ClearUpdateWhenPaused => (1, 0),
            Op::WorldActive(b) => (2, u32::from(b)),
            Op::MusicResume => (3, 0),
            Op::MusicPause => (4, 0),
            Op::SoundsResumeAll => (5, 0),
            Op::SoundsPause(t) => (6, t),
            Op::SoundscapePaused(b) => (7, u32::from(b)),
            Op::ReapplyVoicePauses => (8, 0),
        };
        // SAFETY: at most `OP_CAPACITY` entries are written.
        unsafe { wr(ops.add(i), OpOut { code, arg }) };
    }
    list.len().min(OP_CAPACITY)
}

/// # Safety
/// `st` is exclusively borrowed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs3_disable_saving(st: *mut Pause, duration: f32) {
    // SAFETY: caller contract.
    pause::disable_saving(unsafe { rw(st) }, duration)
}

/// # Safety
/// `st` is exclusively borrowed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs3_tick_saving(st: *mut Pause, dt: f32) {
    // SAFETY: caller contract.
    pause::tick_saving(unsafe { rw(st) }, dt)
}

/// Bits: 1 game input, 2 game update, 4 game paused, 8 map update, 16 update-when-paused meshes.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs3_frame_plan(
    has_game: bool,
    has_map: bool,
    map_active: bool,
    uses_real_player: bool,
    paused: bool,
) -> u32 {
    let f = pause::frame_plan(has_game, has_map, map_active, uses_real_player, paused);
    u32::from(f.game_input)
        | u32::from(f.game_update) << 1
        | u32::from(f.game_paused) << 2
        | u32::from(f.map_update) << 3
        | u32::from(f.update_when_paused) << 4
}

/// Bits: 1 game post-update, 2 map post-update.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs3_post_plan(
    updated: bool,
    scripts_held: bool,
    paused: bool,
    has_game: bool,
    has_map: bool,
    map_active: bool,
) -> u32 {
    let (g, m) = pause::post_plan(updated, scripts_held, paused, has_game, has_map, map_active);
    u32::from(g) | u32::from(m) << 1
}

// game

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs3_route_action(paused: bool) -> bool {
    game::route_action(paused) == Route::ModulesOnly
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs3_analog_allowed(paused: bool) -> bool {
    game::analog_allowed(paused)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs3_visits_effect(effect_active: bool, active_only: bool) -> bool {
    game::visits_effect(effect_active, active_only)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs3_string_duration(
    min: f32,
    start: f32,
    per_char: f32,
    chars: usize,
) -> f32 {
    game::string_duration(min, start, per_char, chars)
}

/// -1 keep the pointer mode, otherwise the new relative flag.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs3_pointer_mode(
    current: i32,
    focus_present: bool,
    focus_is_3d: bool,
) -> i32 {
    game::pointer_mode(current, focus_present, focus_is_3d).map_or(-1, i32::from)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs3_pointer_moved(before: bool, rel_x: i32, rel_y: i32) -> bool {
    game::pointer_moved(before, rel_x, rel_y)
}

/// 0 none, 1 became down, 2 became up.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs3_action_edge(
    id: i32,
    max_actions: i32,
    down: bool,
    prev_down: bool,
) -> i32 {
    match game::action_edge(id, max_actions, down, prev_down) {
        Edge::None => 0,
        Edge::Down => 1,
        Edge::Up => 2,
    }
}

/// Axis (0..2) of the analog sample, or -1 when the action does not fire; `mul` receives its value.
///
/// # Safety
/// `mul` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs3_analog_fire(
    analog_id: i32,
    axis: i32,
    action_mul: f32,
    triggered: bool,
    mul: *mut f32,
) -> i32 {
    match game::analog_fire(analog_id, axis, action_mul, triggered) {
        Some((a, m)) => {
            // SAFETY: caller contract.
            unsafe { wr(mul, m) };
            a as i32
        }
        None => -1,
    }
}
