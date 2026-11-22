//! C ABI of `cLuxMap`, mirrored in `amnesia/src/game/RustLuxMapA13.h`.

use super::a7_ffi::Ctx;
use super::mhost::S;
use super::mmap::{Broadcast, M, Props};
use crate::string_ffi::{Sink, emit, units};
use crate::types::Vector3f;
use std::ffi::c_void;

/// # Safety
/// `c` is a live context; the handle is used for this call only.
unsafe fn m(c: *mut Ctx) -> M {
    // SAFETY: caller contract.
    unsafe { M::new(c) }
}

macro_rules! text {
    ($p:expr, $n:expr) => {
        units($p, $n)
    };
}

#[repr(C)]
pub struct MapState {
    pub quests: i32,
    pub total: i32,
    pub current: i32,
    pub cp_count: i32,
    pub cp_prio: i32,
    pub cp_resume: bool,
    pub cp_volume: f32,
    pub cp_name: S,
    pub cp_start: S,
    pub cp_callback: S,
    pub cp_music: S,
}

/// # Safety
/// `c` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_map_init(c: *mut Ctx, cmap: usize) {
    // SAFETY: caller contract.
    unsafe { m(c).init(cmap) };
}

/// # Safety
/// `c` live; `file` readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_map_load(c: *mut Ctx, file: *const u8, n: usize, entities: bool) {
    // SAFETY: caller contract.
    unsafe { m(c).load_from_file(text!(file, n), entities) };
}

/// # Safety
/// `c` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_map_after_world_load(c: *mut Ctx) {
    // SAFETY: caller contract.
    unsafe { m(c).after_world_load() };
}

/// # Safety
/// `c` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_map_update_doomed(c: *mut Ctx, use_callbacks: bool) {
    // SAFETY: caller contract.
    unsafe { m(c).update_doomed(use_callbacks) };
}

/// # Safety
/// `c` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_map_on_enter(c: *mut Ctx, run_script: bool, first_time: bool) {
    // SAFETY: caller contract.
    unsafe { m(c).on_enter(run_script, first_time) };
}

/// # Safety
/// `c` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_map_on_leave(c: *mut Ctx, run_script: bool) {
    // SAFETY: caller contract.
    unsafe { m(c).on_leave(run_script) };
}

/// # Safety
/// `c` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_map_update(c: *mut Ctx, step: f32) {
    // SAFETY: caller contract.
    unsafe { m(c).update(step) };
}

/// # Safety
/// `c` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_map_render_solid(c: *mut Ctx, functions: usize) {
    // SAFETY: caller contract.
    unsafe { m(c).render_solid(functions) };
}

/// # Safety
/// `c` live; `cmd` readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_map_run_script(c: *mut Ctx, cmd: *const u8, n: usize) {
    // SAFETY: caller contract.
    unsafe { m(c).run_script(text!(cmd, n)) };
}

/// # Safety
/// `c` live; `sink` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_map_recompile(
    c: *mut Ctx,
    sink: Sink<u8>,
    ctx: *mut c_void,
) -> bool {
    // SAFETY: caller contract.
    let (ok, out) = unsafe { m(c).recompile_script() };
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, &out) };
    ok
}

/// # Safety
/// `c` live; `name` readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_map_place_player(c: *mut Ctx, name: *const u8, n: usize) {
    // SAFETY: caller contract.
    unsafe { m(c).place_player_at_start_pos(text!(name, n)) };
}

/// # Safety
/// `c` live; strings readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_map_set_checkpoint(
    c: *mut Ctx,
    name: *const u8,
    name_n: usize,
    start: *const u8,
    start_n: usize,
    callback: *const u8,
    callback_n: usize,
) {
    // SAFETY: caller contract.
    unsafe {
        m(c).set_check_point(
            text!(name, name_n),
            text!(start, start_n),
            text!(callback, callback_n),
        )
    };
}

/// # Safety
/// `c` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_map_load_checkpoint(c: *mut Ctx) {
    // SAFETY: caller contract.
    unsafe { m(c).load_checkpoint() };
}

/// # Safety
/// `c` live; strings readable; `mtx` 16 floats; `scale` 3 floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_map_create_entity(
    c: *mut Ctx,
    name: *const u8,
    name_n: usize,
    file: *const u8,
    file_n: usize,
    mtx: *const [f32; 16],
    scale: *const [f32; 3],
) {
    // SAFETY: caller contract.
    unsafe { m(c).create_entity(text!(name, name_n), text!(file, file_n), &*mtx, &*scale) };
}

/// # Safety
/// `c` live; `e` a live entity.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_map_add_entity(c: *mut Ctx, e: usize) {
    // SAFETY: caller contract.
    unsafe { m(c).add_entity(e) };
}

/// # Safety
/// `c` live; `e` a live entity.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_map_destroy_entity(c: *mut Ctx, e: usize) {
    // SAFETY: caller contract.
    unsafe { m(c).destroy_entity(e) };
}

/// # Safety
/// `c` live; vectors readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_map_broadcast(
    c: *mut Ctx,
    kind: i32,
    has_pos: bool,
    pos: *const Vector3f,
    radius: f32,
    time: f32,
    local: bool,
    vx: *const Vector3f,
    fx: f32,
    lx: i32,
) {
    // SAFETY: caller contract.
    unsafe {
        m(c).broadcast_enemy_message(&Broadcast {
            kind,
            has_pos,
            pos: *pos,
            radius,
            time,
            local,
            vx: *vx,
            fx,
            lx,
        })
    };
}

/// # Safety
/// `c` live; `pos` readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_map_broadcast_sound(
    c: *mut Ctx,
    pos: *const Vector3f,
    volume: f32,
    min: f32,
    max: f32,
) {
    // SAFETY: caller contract.
    unsafe { m(c).broadcast_enemy_sound(*pos, volume, min, max) };
}

/// # Safety
/// `c` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_map_node_is_goal(c: *mut Ctx, node: usize) -> bool {
    // SAFETY: caller contract.
    unsafe { m(c).ai_node_used_as_goal(node) }
}

/// Door state of the swing door with `id`: `broken` and the state (both 0 when it is no door).
///
/// # Safety
/// `c` live; `state` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_map_door(c: *mut Ctx, id: i32, state: *mut i32) -> bool {
    // SAFETY: caller contract.
    let (broken, s) = unsafe { m(c).door(id) };
    // SAFETY: caller contract.
    unsafe { *state = s };
    broken
}

/// # Safety
/// `c` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_map_body_in_sticky(c: *mut Ctx, body: usize) -> bool {
    // SAFETY: caller contract.
    unsafe { m(c).body_in_detachable_sticky(body) }
}

/// # Safety
/// `c` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_map_detach_sticky(c: *mut Ctx, body: usize) -> bool {
    // SAFETY: caller contract.
    unsafe { m(c).detach_body_from_sticky(body) }
}

/// # Safety
/// `c` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_map_body_from_pair(
    c: *mut Ctx,
    parent: i32,
    child: i32,
) -> usize {
    // SAFETY: caller contract.
    unsafe { m(c).body_from_id_pair(parent, child) }
}

/// # Safety
/// `c` live; colliders live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_map_check_collision(c: *mut Ctx, c1: usize, c2: usize) -> bool {
    // SAFETY: caller contract.
    unsafe { m(c).check_collision(c1, c2) }
}

/// # Safety
/// `c` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_map_add_completion(c: *mut Ctx, amount: i32, delay: f32) {
    // SAFETY: caller contract.
    unsafe { m(c).add_completion_amount(amount, delay) };
}

/// # Safety
/// `c` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_map_add_dissolve(c: *mut Ctx, mesh: usize, time: f32) {
    // SAFETY: caller contract.
    unsafe { m(c).add_dissolve_entity(mesh, time) };
}

/// # Safety
/// `c` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_map_add_lamp(
    c: *mut Ctx,
    lamp: usize,
    light: usize,
    amount: f32,
    on_color: bool,
    spec: bool,
) -> usize {
    // SAFETY: caller contract.
    unsafe { m(c).add_lamp_light_connection(lamp, light, amount, on_color, spec) }
}

/// Frees the world and the script, then every object the map owns.
///
/// # Safety
/// `c` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_map_teardown(c: *mut Ctx) {
    // SAFETY: caller contract.
    unsafe { m(c).teardown() };
}

/// # Safety
/// `c` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_map_world(c: *mut Ctx) -> usize {
    // SAFETY: caller contract.
    unsafe { m(c).world() }
}

/// # Safety
/// `c` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_map_phys(c: *mut Ctx) -> usize {
    // SAFETY: caller contract.
    unsafe { m(c).phys() }
}

/// # Safety
/// `c` live; `out` writable. The strings stay valid until the next change of the state.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_map_get(c: *mut Ctx, out: *mut MapState) {
    // SAFETY: caller contract.
    let p: &Props = unsafe { m(c).props() };
    // SAFETY: caller contract.
    unsafe {
        *out = MapState {
            quests: p.quests,
            total: p.total,
            current: p.current,
            cp_count: p.cp_count,
            cp_prio: p.cp_prio,
            cp_resume: p.cp_resume,
            cp_volume: p.cp_volume,
            cp_name: S::of(&p.cp_name),
            cp_start: S::of(&p.cp_start),
            cp_callback: S::of(&p.cp_callback),
            cp_music: S::of(&p.cp_music),
        }
    };
}

/// # Safety
/// `c` live; `s` readable with readable strings.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_map_set(c: *mut Ctx, s: *const MapState) {
    // SAFETY: caller contract.
    let (p, s) = unsafe { (m(c).props(), &*s) };
    p.quests = s.quests;
    p.total = s.total;
    p.current = s.current;
    p.cp_count = s.cp_count;
    p.cp_prio = s.cp_prio;
    p.cp_resume = s.cp_resume;
    p.cp_volume = s.cp_volume;
    // SAFETY: caller contract.
    unsafe {
        p.cp_name = units(s.cp_name.p, s.cp_name.n).to_vec();
        p.cp_start = units(s.cp_start.p, s.cp_start.n).to_vec();
        p.cp_callback = units(s.cp_callback.p, s.cp_callback.n).to_vec();
        p.cp_music = units(s.cp_music.p, s.cp_music.n).to_vec();
    }
}

/// # Safety
/// `c` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a13_map_set_quests(c: *mut Ctx, n: i32) {
    // SAFETY: caller contract.
    unsafe { m(c).props().quests = n };
}
