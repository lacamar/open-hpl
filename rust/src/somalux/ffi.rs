//! C ABI for the Lux game layer, mirrored in `soma/src/game/RustSomaLux.h`.

use super::game::{self, Edge, Route};
use super::pause::{self, Op, Pause};
use super::player::{
    self, Automove, CameraRotate, DamageId, Fade, MessageKind, MoveSpeeds, MoveVec, StateChange,
};
use super::scriptable::{self, Fader, Timer, TimerAdd};
use super::transfer::{self, AfterLoad, Body, Preload, Rel, Restore, Take, Taken};
use crate::string_ffi::units;
use crate::types::{Matrixf, Vector3f};

const _: () = {
    use std::mem::{offset_of, size_of};
    assert!(size_of::<Timer>() == 24 && offset_of!(Timer, repeat) == 16);
    assert!(size_of::<Fader>() == 32 && offset_of!(Fader, reverse_at_end) == 24);
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

/// Steps `n` timers laid out `stride` bytes apart; writes the due indices to `due` and the
/// keep flags to `keep` (both `n` long) and returns the due count.
///
/// # Safety
/// `base` points to `n` timers `stride` bytes apart, exclusively borrowed; `due` and `keep` are
/// writable for `n` entries.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs3_timers_update(
    base: *mut u8,
    n: usize,
    stride: usize,
    dt: f32,
    due: *mut u32,
    keep: *mut u8,
) -> usize {
    let mut count = 0;
    for i in 0..n {
        // SAFETY: caller contract.
        let t = unsafe { rw(base.add(i * stride).cast::<Timer>()) };
        let (is_due, kept) = scriptable::step_timer(t, dt);
        if is_due {
            // SAFETY: at most `n` entries are written.
            unsafe { wr(due.add(count), i as u32) };
            count += 1;
        }
        // SAFETY: `keep` has `n` entries.
        unsafe { wr(keep.add(i), u8::from(kept)) };
    }
    count
}

/// # Safety
/// `f` points to `n` exclusively borrowed faders.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs3_faders_update(f: *mut Fader, n: usize, dt: f32) {
    if n > 0 {
        // SAFETY: caller contract.
        scriptable::update_faders(unsafe { std::slice::from_raw_parts_mut(f, n) }, dt);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs3_fade_speed(value: f32, goal: f32, time: f32) -> f32 {
    scriptable::fade_speed(value, goal, time)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs3_timer_value(
    found: bool,
    length: f32,
    left: f32,
    min: f32,
    max: f32,
    kind: i32,
    abs: bool,
) -> f32 {
    scriptable::timer_value(found.then_some((length, left)), min, max, kind, abs)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs3_timer_has_passed(
    found: bool,
    length: f32,
    left: f32,
    wanted: f32,
) -> bool {
    scriptable::timer_time_has_passed(found.then_some((length, left)), wanted)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs3_fader_value(
    found: bool,
    value: f32,
    min: f32,
    max: f32,
    kind: i32,
    abs: bool,
) -> f32 {
    scriptable::fader_value(found.then_some(value), min, max, kind, abs)
}

/// 0 skip, 1 push a new timer, 2 overwrite the existing one.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs3_timer_add(exists: bool, create_if_exist: bool) -> i32 {
    match scriptable::timer_add(exists, create_if_exist) {
        TimerAdd::Skip => 0,
        TimerAdd::Push => 1,
        TimerAdd::Overwrite => 2,
    }
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

// player

/// # Safety
/// `x` is exclusively borrowed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs3_fade_towards(x: *mut Fade, dt: f32) -> f32 {
    // SAFETY: caller contract.
    player::fade_towards(unsafe { rw(x) }, dt)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs3_angle_delta(from: f32, to: f32) -> f32 {
    player::angle_delta(from, to)
}

/// # Safety
/// `m` is exclusively borrowed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs3_step_move_vec(m: *mut MoveVec, dt: f32) {
    // SAFETY: caller contract.
    player::step_move_vec(unsafe { rw(m) }, dt)
}

/// # Safety
/// `v` points to `n` floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs3_fov_mul(base: f32, v: *const f32, n: usize) -> f32 {
    // SAFETY: caller contract.
    player::fov_mul(base, unsafe { units(v, n) })
}

/// # Safety
/// `v` points to `n` floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs3_visibility_range_mul(v: *const f32, n: usize) -> f32 {
    // SAFETY: caller contract.
    player::visibility_range_mul(unsafe { units(v, n) })
}

/// # Safety
/// `v` points to `n` floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs3_visibility_max_range(v: *const f32, n: usize) -> f32 {
    // SAFETY: caller contract.
    player::visibility_max_range(unsafe { units(v, n) })
}

#[repr(C)]
pub struct RotateIn {
    pub speed: f32,
    pub acc: f32,
    pub speed_mul: f32,
    pub max_speed: f32,
    pub local: bool,
    pub target: Vector3f,
}

#[repr(C)]
pub struct RotateOut {
    pub speed: f32,
    pub apply: bool,
    pub add_yaw: f32,
    pub add_pitch: f32,
}

/// # Safety
/// Pointers are valid; `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs3_step_camera_rotate(
    r: *const RotateIn,
    cam_pos: *const Vector3f,
    body_yaw: f32,
    cam_pitch: f32,
    dt: f32,
    out: *mut RotateOut,
) {
    // SAFETY: caller contract.
    unsafe {
        let r = &*r;
        let rot = CameraRotate {
            speed: r.speed,
            acc: r.acc,
            speed_mul: r.speed_mul,
            max_speed: r.max_speed,
            local: r.local,
            target: r.target,
        };
        let s = player::step_camera_rotate(&rot, rd(cam_pos), body_yaw, cam_pitch, dt);
        let (add_yaw, add_pitch) = s.add.unwrap_or((0.0, 0.0));
        wr(
            out,
            RotateOut {
                speed: s.speed,
                apply: s.add.is_some(),
                add_yaw,
                add_pitch,
            },
        )
    }
}

/// Returns false when the target is reached; otherwise `fwd` and `right` are the move amounts.
///
/// # Safety
/// Pointers are valid; `fwd` and `right` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs3_step_automove(
    target: *const Vector3f,
    feet: *const Vector3f,
    forward: *const Vector3f,
    right_dir: *const Vector3f,
    speed_mul: f32,
    fwd: *mut f32,
    right: *mut f32,
) -> bool {
    // SAFETY: caller contract.
    unsafe {
        match player::step_automove(rd(target), rd(feet), rd(forward), rd(right_dir), speed_mul) {
            Automove::Arrived => false,
            Automove::Move { forward, right: r } => {
                wr(fwd, forward);
                wr(right, r);
                true
            }
        }
    }
}

/// # Safety
/// Pointers are valid; the averages are exclusively borrowed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs3_update_average_move(
    speed: *mut f32,
    dir: *mut Vector3f,
    vel: *const Vector3f,
) {
    // SAFETY: caller contract.
    unsafe { player::update_average_move(rw(speed), rw(dir), rd(vel)) }
}

/// # Safety
/// `s` is readable; `out` is writable for 10 floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs3_move_speed_settings(s: *const MoveSpeeds, out: *mut f32) {
    // SAFETY: caller contract.
    let v = player::move_speed_settings(&unsafe { rd(s) });
    for (i, x) in v.iter().enumerate() {
        // SAFETY: 10 entries.
        unsafe { wr(out.add(i), *x) };
    }
}

/// 0 missing, 1 already current, 2 switch.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs3_state_change(exists: bool, is_current: bool) -> i32 {
    match player::state_change(exists, is_current) {
        StateChange::Missing => 0,
        StateChange::Same => 1,
        StateChange::Switch => 2,
    }
}

/// 0 other, 1 `Reset()`, 2 `OnMapEnter`, 3 `DestroyWorldEntities`.
///
/// # Safety
/// `p` points to `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs3_message_kind(p: *const u8, n: usize) -> i32 {
    // SAFETY: caller contract.
    match player::message_kind(unsafe { bytes(p, n) }) {
        MessageKind::Other => 0,
        MessageKind::Reset => 1,
        MessageKind::MapEnter => 2,
        MessageKind::DestroyWorldEntities => 3,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs3_drops_state_on_reset(kind: i32, has_state: bool, state_id: i32) -> bool {
    player::drops_state_on_reset(
        if kind == 1 {
            MessageKind::Reset
        } else {
            MessageKind::Other
        },
        has_state,
        state_id,
    )
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs3_clears_camera_adds(is_transfer: bool) -> bool {
    player::clears_camera_adds(is_transfer)
}

/// 0 nothing (no body), 1 script placed the player, 2 set feet and yaw ourselves.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs3_place_plan(has_body: bool, script_handled: bool) -> i32 {
    match player::place_plan(has_body, script_handled) {
        None => 0,
        Some(p) if p.set_feet_and_yaw => 2,
        Some(_) => 1,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs3_is_dead(health: f32, can_die: bool) -> bool {
    player::is_dead(health, can_die)
}

/// Writes the new health and returns whether the player just died.
///
/// # Safety
/// `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs3_set_health(
    health: f32,
    max: f32,
    can_die: bool,
    x: f32,
    out: *mut f32,
) -> bool {
    let (h, died) = player::set_health(health, max, can_die, x);
    // SAFETY: caller contract.
    unsafe { wr(out, h) };
    died
}

/// 0 untracked, 1 reject, 2 remember, 3 forget the oldest then remember.
///
/// # Safety
/// `ids` points to `n` ints.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs3_damage_id(ids: *const i32, n: usize, id: i32) -> i32 {
    // SAFETY: caller contract.
    match player::damage_id(unsafe { units(ids, n) }, id) {
        DamageId::Untracked => 0,
        DamageId::Reject => 1,
        DamageId::Push => 2,
        DamageId::EvictPush => 3,
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs3_health_after_damage(health: f32, amount: f32, min_health: f32) -> f32 {
    player::health_after_damage(health, amount, min_health)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs3_health_after_script_damage(
    health: f32,
    amount: f32,
    min_health: f32,
) -> f32 {
    player::health_after_script_damage(health, amount, min_health)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs3_add_health(health: f32, x: f32, min_health: f32) -> f32 {
    player::add_health(health, x, min_health)
}

/// # Safety
/// `x` and `y` are writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs3_mouse_vector(
    rel_x: i32,
    rel_y: i32,
    sensitivity: f32,
    height: f32,
    x: *mut f32,
    y: *mut f32,
) {
    let (a, b) = player::mouse_vector(rel_x, rel_y, sensitivity, height);
    // SAFETY: caller contract.
    unsafe {
        wr(x, a);
        wr(y, b);
    }
}

/// # Safety
/// `xy` points to `n` (x, y) pairs; `x` and `y` are writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs3_smooth_mouse(
    xy: *const [f32; 2],
    n: usize,
    prev_mul: f32,
    x: *mut f32,
    y: *mut f32,
) {
    // SAFETY: caller contract.
    let s: Vec<(f32, f32)> = unsafe { units(xy, n) }
        .iter()
        .map(|p| (p[0], p[1]))
        .collect();
    let (a, b) = player::smooth_mouse(&s, prev_mul);
    // SAFETY: caller contract.
    unsafe {
        wr(x, a);
        wr(y, b);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs3_pops_oldest_sample(len: usize, max: i32) -> bool {
    player::pops_oldest_sample(len, max)
}
