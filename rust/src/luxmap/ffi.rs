//! C ABI of `luxmap`, mirrored in `amnesia/src/game/RustLuxMap.h`.
//! Texts are (pointer, length) in; results leave through sinks.

use super::area::{
    self, BuoyIn, BuoyOut, ExamineIn, ExamineOut, LadderSetup, PlayerStart, StickyAttachIn,
    StickyAttachOut,
};
use super::handler;
use super::helper::{self, DamageFilter, SubMesh};
use super::map;
use crate::math::Planef;
use crate::string_ffi::{Sink, emit, units};
use crate::types::{Matrixf, Vector3f};
use std::ffi::c_void;

const _: () = {
    use std::mem::{offset_of, size_of};
    assert!(size_of::<BuoyIn>() == 4 && size_of::<BuoyOut>() == 3);
    assert!(size_of::<ExamineIn>() == 5 && size_of::<ExamineOut>() == 4);
    assert!(size_of::<StickyAttachIn>() == 180 && offset_of!(StickyAttachIn, move_body) == 176);
    assert!(size_of::<StickyAttachOut>() == 88 && offset_of!(StickyAttachOut, mode) == 80);
    assert!(size_of::<LadderSetup>() == 20 && size_of::<PlayerStart>() == 16);
    assert!(size_of::<SubMesh>() == 4);
    assert!(size_of::<DamageFilter>() == 8);
};

/// # Safety
/// `p` is valid for writes.
unsafe fn wr<T>(p: *mut T, v: T) {
    // SAFETY: caller contract.
    unsafe { p.write(v) }
}

/// # Safety
/// `p` is valid for reads.
unsafe fn rd<T: Copy>(p: *const T) -> T {
    // SAFETY: caller contract.
    unsafe { *p }
}

macro_rules! pure {
    ($(#[$m:meta])* $name:ident ($($a:ident: $at:ty),*) -> $r:ty => $body:expr) => {
        $(#[$m])*
        #[unsafe(no_mangle)]
        pub extern "C" fn $name($($a: $at),*) -> $r {
            $body
        }
    };
}

macro_rules! text {
    ($name:ident ($($a:ident: $at:ty),*; $($s:ident, $n:ident),+) -> $r:ty => $body:expr) => {
        /// # Safety
        /// Each text is valid for its length.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name($($a: $at,)* $($s: *const u8, $n: usize),+) -> $r {
            // SAFETY: caller contract.
            $(let $s = unsafe { units($s, $n) };)+
            $body
        }
    };
}

macro_rules! sink {
    ($name:ident ($($a:ident: $at:ty),*; $($s:ident, $n:ident),*) => $body:expr) => {
        /// # Safety
        /// Each text is valid for its length; `sink` accepts `ctx`.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name($($a: $at,)* $($s: *const u8, $n: usize,)* sink: Sink<u8>, ctx: *mut c_void) {
            // SAFETY: caller contract.
            $(let $s = unsafe { units($s, $n) };)*
            let r: Vec<u8> = $body;
            // SAFETY: caller contract.
            unsafe { emit(sink, ctx, &r) }
        }
    };
}

// area

pure!(ohpl_ga5_step_too_small(step: f32, engine: f32) -> bool => area::step_too_small(step, engine));
pure!(ohpl_ga5_body_skipped(collide: bool, active: bool, mass: f32, is_char: bool) -> bool => area::body_skipped(collide, active, mass, is_char));
pure!(ohpl_ga5_liquid_surface_y(pos_y: f32, size_y: f32) -> f32 => area::liquid_surface_y(pos_y, size_y));
pure!(ohpl_ga5_wave_surface_y(time: f32, freq: f32, px: f32, pz: f32, amp: f32, surface_y: f32) -> f32 => area::wave_surface_y(time, freq, px, pz, amp, surface_y));
pure!(ohpl_ga5_liquid_char(is_player: bool, has_enemy: bool, inside: bool, in_water: bool) -> i32 => area::liquid_char(is_player, has_enemy, inside, in_water));
pure!(ohpl_ga5_sticky_inside(inside: bool, is_last: bool) -> i32 => area::sticky_inside(inside, is_last));
pure!(ohpl_ga5_sticky_after_callback(has_callback: bool, allow: bool) -> bool => area::sticky_after_callback(has_callback, allow));
pure!(ohpl_ga5_script_crosshair(has_callback: bool, custom: i32, default: i32, grab: i32) -> i32 => area::script_crosshair(has_callback, custom, default, grab));
pure!(ohpl_ga5_script_can_interact(item: bool, has_callback: bool) -> bool => area::script_can_interact(item, has_callback));
pure!(ohpl_ga5_rope_after_world(created: bool, nothing_linked: bool) -> i32 => area::rope_after_world(created, nothing_linked));
pure!(ohpl_ga5_rope_max_segments(max_len: f32, segment: f32) -> i32 => area::rope_max_segments(max_len, segment));

/// # Safety
/// `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga5_liquid_surface_plane(pos_y: f32, size_y: f32, out: *mut Planef) {
    // SAFETY: caller contract.
    unsafe { wr(out, area::liquid_surface_plane(pos_y, size_y)) }
}

/// # Safety
/// `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga5_wave_plane(surface_y: f32, out: *mut Planef) {
    // SAFETY: caller contract.
    unsafe { wr(out, area::wave_plane(surface_y)) }
}

/// # Safety
/// `i` is readable, `out` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga5_liquid_body(
    i: *const BuoyIn,
    dist_sqr: f32,
    max_dist_sqr: f32,
    out: *mut BuoyOut,
) {
    // SAFETY: caller contract.
    unsafe { wr(out, area::liquid_body(rd(i), dist_sqr, max_dist_sqr)) }
}

/// Returns skip; `allow` receives the new `mbAllowAttachment`.
///
/// # Safety
/// Texts valid for their length, `allow` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga5_sticky_name(
    attach: *const u8,
    attach_n: usize,
    body: *const u8,
    body_n: usize,
    allow: *mut bool,
) -> bool {
    // SAFETY: caller contract.
    let (skip, a) = area::sticky_name(unsafe { units(attach, attach_n) }, unsafe {
        units(body, body_n)
    });
    // SAFETY: caller contract.
    unsafe { wr(allow, a) };
    skip
}

/// # Safety
/// `i` is readable, `out` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga5_sticky_attach(
    i: *const StickyAttachIn,
    out: *mut StickyAttachOut,
) {
    // SAFETY: caller contract.
    unsafe { wr(out, area::sticky_attach(&rd(i))) }
}

/// # Safety
/// `count` is exclusively borrowed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga5_tick(count: *mut f32, step: f32) -> bool {
    // SAFETY: caller contract.
    area::tick(unsafe { &mut *count }, step)
}

/// # Safety
/// `count` is exclusively borrowed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga5_tick_random(
    count: *mut f32,
    step: f32,
    min: f32,
    max: f32,
) -> bool {
    // SAFETY: caller contract.
    area::tick_random(unsafe { &mut *count }, step, min, max)
}

/// # Safety
/// `count` is exclusively borrowed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga5_tick_fixed(count: *mut f32, step: f32, next: f32) -> bool {
    // SAFETY: caller contract.
    area::tick_fixed(unsafe { &mut *count }, step, next)
}

/// # Safety
/// `count` is exclusively borrowed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga5_examine_tick(count: *mut f32, step: f32) {
    // SAFETY: caller contract.
    area::examine_tick(unsafe { &mut *count }, step)
}

/// # Safety
/// `i` is readable, `out` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga5_examine_interact(
    i: *const ExamineIn,
    sanity: f32,
    insane_limit: f32,
    sound_count: f32,
    out: *mut ExamineOut,
) {
    // SAFETY: caller contract.
    unsafe {
        wr(
            out,
            area::examine_interact(rd(i), sanity, insane_limit, sound_count),
        )
    }
}

/// # Safety
/// Vectors readable, `out` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga5_follow_offset(
    offset: *const Vector3f,
    parent: *const Vector3f,
    body: *const Vector3f,
    out: *mut Vector3f,
) {
    // SAFETY: caller contract.
    unsafe { wr(out, area::follow_offset(rd(offset), rd(parent), rd(body))) }
}

/// # Safety
/// Vectors readable, `out` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga5_attach_offset(
    body: *const Vector3f,
    parent: *const Vector3f,
    out: *mut Vector3f,
) {
    // SAFETY: caller contract.
    unsafe { wr(out, area::attach_offset(rd(body), rd(parent))) }
}

/// # Safety
/// `world` readable, `out` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga5_ladder_setup(
    world: *const Matrixf,
    pos_y: f32,
    size_y: f32,
    out: *mut LadderSetup,
) {
    // SAFETY: caller contract.
    unsafe { wr(out, area::ladder_setup(&rd(world), pos_y, size_y)) }
}

/// # Safety
/// Vectors readable, `out` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga5_ladder_start_pos(
    area_pos: *const Vector3f,
    forward: *const Vector3f,
    char_size: *const Vector3f,
    char_y: f32,
    max_y: f32,
    min_y: f32,
    out: *mut Vector3f,
) {
    // SAFETY: caller contract.
    unsafe {
        wr(
            out,
            area::ladder_start_pos(
                rd(area_pos),
                rd(forward),
                rd(char_size),
                char_y,
                max_y,
                min_y,
            ),
        )
    }
}

pub type FitsFn = unsafe extern "C" fn(*mut c_void, *const Vector3f) -> bool;

/// Returns whether a position was found; `start` holds it.
///
/// # Safety
/// `start` is exclusively borrowed; `fits` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga5_ladder_find_start(
    start: *mut Vector3f,
    max_y: f32,
    fits: FitsFn,
    ctx: *mut c_void,
) -> bool {
    // SAFETY: caller contract.
    let s = unsafe { rd(start) };
    // SAFETY: `fits` accepts `ctx` and a readable position.
    let r = area::ladder_find_start(s, max_y, &mut |p| unsafe { fits(ctx, &p) });
    match r {
        Some(p) => {
            // SAFETY: caller contract.
            unsafe { wr(start, p) };
            true
        }
        None => false,
    }
}

/// # Safety
/// `m` readable, `out` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga5_player_start(m: *const Matrixf, out: *mut PlayerStart) {
    // SAFETY: caller contract.
    unsafe { wr(out, area::player_start(&rd(m))) }
}

/// # Safety
/// `t` readable, `out` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga5_node_up(t: *const Vector3f, out: *mut Vector3f) {
    // SAFETY: caller contract.
    unsafe { wr(out, area::node_up(rd(t))) }
}

sink!(ohpl_ga5_call1(; func, fn_n, name, name_n) => area::call1(func, name));
sink!(ohpl_ga5_call2(; func, fn_n, a, a_n, b, b_n) => area::call2(func, a, b));

// map

pure!(ohpl_ga5_timer_count(time: f32) -> f32 => map::timer_count(time));
pure!(ohpl_ga5_timer_step(count: f32, step: f32) -> f32 => count - step);
pure!(ohpl_ga5_timer_due(count: f32, destroy_me: bool) -> bool => map::timer_due(count, destroy_me));
pure!(ohpl_ga5_timer_remove_marks(updating: bool) -> bool => map::timer_remove_marks(updating));
pure!(ohpl_ga5_completion_fraction(current: i32, total: i32) -> f32 => map::completion_fraction(current, total));
pure!(ohpl_ga5_quest_completion(total: i32, quests: i32, value: i32) -> i32 => map::quest_completion(total, quests, value));
pure!(ohpl_ga5_dissolve_speed(time: f32) -> f32 => map::dissolve_speed(time));
pure!(ohpl_ga5_checkpoint_music_volume(volume: f32, volume_mul: f32) -> f32 => map::checkpoint_music_volume(volume, volume_mul));
pure!(ohpl_ga5_hard_tinderbox_every(tinderboxes: i32) -> f32 => map::hard_tinderbox_every(tinderboxes));
pure!(ohpl_ga5_hard_oil_every() -> f32 => map::HARD_OIL_EVERY);
pure!(ohpl_ga5_script_choice(plain: bool, compressed_enabled: bool, compressed: bool) -> i32 => map::script_choice(plain, compressed_enabled, compressed));
pure!(ohpl_ga5_enter_plan(run_script: bool, has_script: bool, first_time: bool) -> i32 => map::enter_plan(run_script, has_script, first_time));
pure!(ohpl_ga5_run_script_allowed(has_script: bool, is_current: bool) -> bool => map::run_script_allowed(has_script, is_current));
pure!(ohpl_ga5_entity_lists(is_enemy: bool, is_area: bool, is_sticky: bool) -> i32 => map::entity_lists(is_enemy, is_area, is_sticky));
pure!(ohpl_ga5_world_load_flags(entities: bool, phys: bool, stat: bool, ent: bool, f0: i32, f1: i32, f2: i32, f3: i32) -> i32 => map::world_load_flags(entities, phys, stat, ent, [f0, f1, f2, f3]));

/// `-1` when unchanged, else the new icon state.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga5_commentary_sync(icons_active: bool, show: bool) -> i32 {
    map::commentary_sync(icons_active, show).map_or(-1, i32::from)
}

/// # Safety
/// `current` is exclusively borrowed, `prev`/`new` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga5_completion_add(
    current: *mut i32,
    total: i32,
    amount: i32,
    prev: *mut f32,
    new: *mut f32,
) {
    // SAFETY: caller contract.
    let (p, n) = map::completion_add(unsafe { &mut *current }, total, amount);
    // SAFETY: caller contract.
    unsafe {
        wr(prev, p);
        wr(new, n);
    }
}

/// # Safety
/// `values` points to 5 ints.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga5_completion_value(kind: i32, values: *const i32) -> i32 {
    // SAFETY: caller contract.
    map::completion_value(kind, &unsafe { rd(values.cast::<[i32; 5]>()) })
}

/// Returns remove; `alpha` is updated.
///
/// # Safety
/// `alpha` is exclusively borrowed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga5_dissolve_step(alpha: *mut f32, speed: f32, step: f32) -> bool {
    // SAFETY: caller contract.
    let (a, remove) = map::dissolve_step(unsafe { rd(alpha) }, speed, step);
    // SAFETY: caller contract.
    unsafe { wr(alpha, a) };
    remove
}

/// # Safety
/// `counter` is exclusively borrowed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga5_hard_remove(counter: *mut f32, every: f32) -> bool {
    // SAFETY: caller contract.
    map::hard_remove(unsafe { &mut *counter }, every)
}

/// # Safety
/// `ids` is readable for `n` entries.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga5_free_entity_id(ids: *const i32, n: usize) -> i32 {
    // SAFETY: caller contract.
    map::free_entity_id(unsafe { units(ids, n) })
}

/// Returns whether an enemy hears it; `volume` receives the heard volume.
///
/// # Safety
/// `volume` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga5_sound_volume(
    dist: f32,
    min: f32,
    max: f32,
    volume: f32,
    out: *mut f32,
) -> bool {
    match map::sound_volume(dist, min, max, volume) {
        Some(v) => {
            // SAFETY: caller contract.
            unsafe { wr(out, v) };
            true
        }
        None => false,
    }
}

text!(ohpl_ga5_callback_remove_ok(; name, name_n, cb, cb_n) -> bool => map::callback_remove_ok(name, cb));
text!(ohpl_ga5_use_item_matches(; item, item_n, ent, ent_n, want_item, want_item_n, want_ent, want_ent_n) -> bool => map::use_item_matches(item, ent, want_item, want_ent));
text!(ohpl_ga5_checkpoint_music_pick(looped: bool, special: bool; file, file_n) -> bool => map::checkpoint_music_pick(file, looped, special));
text!(ohpl_ga5_sound_prefix_matches(; type_name, type_n, name, name_n) -> bool => map::sound_prefix_matches(type_name, name));
sink!(ohpl_ga5_checkpoint_call(count: i32; cb, cb_n, name, name_n) => map::checkpoint_call(cb, name, count));
sink!(ohpl_ga5_compile_errors(; msgs, msgs_n) => map::compile_errors(msgs));

// handler

text!(ohpl_ga5_is_new_map(; current, current_n, file, file_n) -> bool => handler::is_new_map(current, file));
text!(ohpl_ga5_pair_events(hard: bool; last, last_n, new, new_n) -> i32 => handler::pair_events(last, new, hard));
sink!(ohpl_ga5_map_name(; file, file_n) => handler::map_name(file));
sink!(ohpl_ga5_change_file(; name, name_n) => handler::change_file(name));
pure!(ohpl_ga5_load_seconds(start_ms: u64, now_ms: u64) -> f32 => handler::load_seconds(start_ms, now_ms));
pure!(ohpl_ga5_first_time(saved_exists: bool) -> bool => handler::first_time(saved_exists));
pure!(ohpl_ga5_commentary_stops(show: bool) -> bool => handler::commentary_stops(show));

pure!(ohpl_ga5_change_proceeds(active: bool, fading: bool) -> bool => handler::change_proceeds(active, fading));

/// Writes the call sequence (at most 12 entries) and returns its length.
///
/// # Safety
/// `out` is writable for 12 bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga5_set_current_plan(
    has_current: bool,
    has_new: bool,
    same: bool,
    out: *mut u8,
) -> usize {
    let plan = handler::set_current_plan(has_current, has_new, same);
    for (i, &op) in plan.iter().take(12).enumerate() {
        // SAFETY: at most 12 entries written.
        unsafe { wr(out.add(i), op) };
    }
    plan.len().min(12)
}

/// # Safety
/// `paused` is exclusively borrowed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga5_pause_sounds(paused: *mut bool) -> bool {
    // SAFETY: caller contract.
    handler::pause_sounds(unsafe { &mut *paused })
}

/// # Safety
/// `paused` is exclusively borrowed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga5_resume_sounds(paused: *mut bool) -> bool {
    // SAFETY: caller contract.
    handler::resume_sounds(unsafe { &mut *paused })
}

// helper

/// # Safety
/// `subs` is readable for `n` entries.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga5_los_before(
    is_char: bool,
    collide: bool,
    check_shadow: bool,
    blocks_light: bool,
    has_mesh_entity: bool,
    subs: *const SubMesh,
    n: usize,
) -> bool {
    // SAFETY: caller contract.
    let subs = unsafe { units(subs, n) };
    helper::los_before(
        is_char,
        collide,
        check_shadow,
        blocks_light,
        has_mesh_entity,
        subs,
    )
}

pure!(ohpl_ga5_closest_entity_before(has_entity: bool, entity_active: bool, collide: bool, is_char: bool) -> i32 => helper::closest_entity_before(has_entity, entity_active, collide, is_char));
pure!(ohpl_ga5_closer(dist: f32, closest: f32) -> bool => helper::closer(dist, closest));
pure!(ohpl_ga5_char_collider_before(collide_character: bool, is_char: bool) -> bool => helper::char_collider_before(collide_character, is_char));
pure!(ohpl_ga5_attack_ray_before(is_char: bool, collide: bool, is_skip: bool) -> bool => helper::attack_ray_before(is_char, collide, is_skip));
pure!(ohpl_ga5_takes_push(mass: f32, is_char: bool) -> bool => helper::takes_push(mass, is_char));
pure!(ohpl_ga5_damage_target(has_entity: bool, is_player_body: bool) -> i32 => helper::damage_target(has_entity, is_player_body));
pure!(ohpl_ga5_max_rgb(r: f32, g: f32, b: f32) -> f32 => helper::max_rgb(r, g, b));
pure!(ohpl_ga5_light_kind(is_box: bool, is_spot: bool, casts_shadows: bool) -> i32 => helper::light_kind(is_box, is_spot, casts_shadows));
pure!(ohpl_ga5_light_falloff_add(level: f32, amount: f32, dist: f32, radius: f32, radius_add: f32) -> f32 => helper::light_falloff_add(level, amount, dist, radius, radius_add));

/// # Safety
/// `f` readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga5_damage_skips(f: *const DamageFilter) -> bool {
    // SAFETY: caller contract.
    helper::damage_skips(unsafe { rd(f) })
}

/// # Safety
/// `pts` readable for `n` points, `out` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga5_hit_position(pts: *const Vector3f, n: usize, out: *mut Vector3f) {
    // SAFETY: caller contract.
    unsafe { wr(out, helper::hit_position(units(pts, n))) }
}

/// # Safety
/// Vectors readable, `out` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga5_hit_direction(
    hit: *const Vector3f,
    origin: *const Vector3f,
    out: *mut Vector3f,
) {
    // SAFETY: caller contract.
    unsafe { wr(out, helper::hit_direction(rd(hit), rd(origin))) }
}

/// # Safety
/// `dir` readable, `out` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga5_char_push(dir: *const Vector3f, force: f32, out: *mut Vector3f) {
    // SAFETY: caller contract.
    unsafe { wr(out, helper::char_push(rd(dir), force)) }
}

/// # Safety
/// `dir` readable, `out` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga5_prop_impulse(
    dir: *const Vector3f,
    force: f32,
    mass: f32,
    max_impulse: f32,
    out: *mut Vector3f,
) {
    // SAFETY: caller contract.
    unsafe { wr(out, helper::prop_impulse(rd(dir), force, mass, max_impulse)) }
}
