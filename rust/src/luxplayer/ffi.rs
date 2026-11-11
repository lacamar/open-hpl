//! C ABI for the player core, mirrored in `amnesia/src/game/RustLuxPlayer.h`.

use super::camera::{self, CamState, HeadPosAdd};
use super::hands::{self, HandsPos};
use super::helpers::{
    self, CamDir, Collapse, Darkness, DarknessIn, Death, Flashback, Hurt, Lantern, LanternSet,
    LookAt,
};
use super::ops::{Op, Ops};
use super::sanity::{self, Sanity};
use super::vitals::{self, Vitals};
use crate::types::{Vector2f, Vector3f};

const _: () = {
    use std::mem::{offset_of, size_of};
    assert!(size_of::<Op>() == 24 && offset_of!(Op, a) == 8);
    assert!(size_of::<Ops>() == 388 && offset_of!(Ops, ops) == 4);
    assert!(size_of::<Vitals>() == 16);
    assert!(size_of::<HeadPosAdd>() == 36 && offset_of!(HeadPosAdd, add) == 24);
    assert!(size_of::<Collapse>() == 60);
    assert!(size_of::<CamDir>() == 32);
    assert!(size_of::<Hurt>() == 56);
    assert!(size_of::<LookAt>() == 20);
    assert!(size_of::<Flashback>() == 28);
    assert!(size_of::<Lantern>() == 32);
    assert!(size_of::<Darkness>() == 36);
    assert!(size_of::<Death>() == 56);
    assert!(size_of::<Sanity>() == 124);
    assert!(size_of::<CamState>() == 128);
};

/// # Safety
/// `p` is valid and exclusively borrowed.
unsafe fn rw<'a, T>(p: *mut T) -> &'a mut T {
    // SAFETY: caller contract.
    unsafe { &mut *p }
}

/// # Safety
/// `p` is valid for reads.
unsafe fn rd<'a, T>(p: *const T) -> &'a T {
    // SAFETY: caller contract.
    unsafe { &*p }
}

/// # Safety
/// `p` points to `n` readable elements, or `n` is 0.
unsafe fn slice<'a, T>(p: *const T, n: usize) -> &'a [T] {
    if n == 0 {
        return &[];
    }
    // SAFETY: caller contract.
    unsafe { std::slice::from_raw_parts(p, n) }
}

pub const VITALS_GIVE_DAMAGE: i32 = 0;
pub const VITALS_SET_HEALTH: i32 = 1;
pub const VITALS_SET_SANITY: i32 = 2;
pub const VITALS_LOWER_SANITY: i32 = 3;
pub const VITALS_ADD_HEALTH: i32 = 4;
pub const VITALS_ADD_SANITY: i32 = 5;
pub const VITALS_ADD_LAMP_OIL: i32 = 6;

/// `flags`: bit 0 spin head / use effect / show effect, bit 1 lethal.
///
/// # Safety
/// `v` is readable, `out` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga2_vitals(
    which: i32,
    v: *const Vitals,
    amount: f32,
    ty: i32,
    flags: u32,
    out: *mut Ops,
) {
    // SAFETY: caller contract.
    let (v, o) = unsafe { (rd(v), rw(out)) };
    *o = Ops::default();
    let on = flags & 1 != 0;
    match which {
        VITALS_GIVE_DAMAGE => vitals::give_damage(v, amount, ty, on, flags & 2 != 0, o),
        VITALS_SET_HEALTH => vitals::set_health(v, amount, o),
        VITALS_SET_SANITY => vitals::set_sanity(v, amount, o),
        VITALS_LOWER_SANITY => vitals::lower_sanity(v, amount, on, o),
        VITALS_ADD_HEALTH => vitals::add_health(v, amount, o),
        VITALS_ADD_SANITY => vitals::add_sanity(v, amount, on, o),
        VITALS_ADD_LAMP_OIL => vitals::add_lamp_oil(v, amount, o),
        _ => {}
    }
}

/// # Safety
/// `s` is exclusively borrowed, `out` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga2_update_camera(s: *mut CamState, dt: f32, out: *mut Ops) {
    // SAFETY: caller contract.
    let (s, o) = unsafe { (rw(s), rw(out)) };
    *o = Ops::default();
    camera::update_camera(s, dt, o);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga2_clamp_pitch_goal(x: f32, min: f32, max: f32) -> f32 {
    camera::clamp_pitch_goal(x, min, max)
}

/// # Safety
/// `adds` points to `n` exclusively borrowed entries; the other pointers are valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga2_update_head_pos_add(
    adds: *mut HeadPosAdd,
    n: usize,
    camera_pos_add: *const Vector3f,
    dt: f32,
    total: *mut Vector3f,
    sum: *mut Vector3f,
) {
    let a: &mut [HeadPosAdd] = if n == 0 {
        &mut []
    } else {
        // SAFETY: caller contract.
        unsafe { std::slice::from_raw_parts_mut(adds, n) }
    };
    // SAFETY: caller contract.
    let (t, s) = camera::update_head_pos_add(a, *unsafe { rd(camera_pos_add) }, dt);
    // SAFETY: caller contract.
    unsafe {
        *rw(total) = t;
        *rw(sum) = s;
    }
}

/// # Safety
/// `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga2_spin_head(rx: f32, ry: f32, speed: f32, out: *mut Vector2f) {
    // SAFETY: caller contract.
    unsafe { *rw(out) = camera::spin_head(rx, ry, speed) };
}

/// Returns false when the spin is zero; else updates `spin` and writes `(pitch, yaw)` to `add`.
///
/// # Safety
/// Both pointers are valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga2_update_head_spin(
    spin: *mut Vector2f,
    deacc: f32,
    dt: f32,
    add: *mut Vector2f,
) -> bool {
    // SAFETY: caller contract.
    let (spin, add) = unsafe { (rw(spin), rw(add)) };
    match camera::update_head_spin(*spin, deacc, dt) {
        None => false,
        Some((s, p, y)) => {
            *spin = s;
            *add = Vector2f { x: p, y };
            true
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga2_terror_step(
    terror: f32,
    enemies: bool,
    inc: f32,
    dec: f32,
    dt: f32,
) -> f32 {
    camera::terror_step(terror, enemies, inc, dec, dt)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga2_focus_alpha(alpha: f32, hide: bool, dt: f32) -> f32 {
    camera::update_focus_alpha(alpha, hide, dt)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga2_prev_speeds_pop(size: usize, max: i32) -> bool {
    camera::prev_speeds_pop(size, max)
}

/// # Safety
/// `p` points to `n` floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga2_avg_speed(p: *const f32, n: usize) -> f32 {
    // SAFETY: caller contract.
    camera::avg_speed(unsafe { slice(p, n) })
}

/// # Safety
/// `s` exclusively borrowed, `out` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga2_collapse_update(s: *mut Collapse, dt: f32, out: *mut Ops) {
    // SAFETY: caller contract.
    let (s, o) = unsafe { (rw(s), rw(out)) };
    *o = Ops::default();
    helpers::collapse_update(s, dt, o);
}

/// # Safety
/// `s` exclusively borrowed, `list` has `n` entries, `out` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga2_camdir_update(
    s: *mut CamDir,
    list: *const Vector2f,
    n: usize,
    sanity: f32,
    dt: f32,
    out: *mut Ops,
) {
    // SAFETY: caller contract.
    let (s, o, l) = unsafe { (rw(s), rw(out), slice(list, n)) };
    *o = Ops::default();
    helpers::camdir_update(s, l, sanity, dt, o);
}

/// # Safety
/// `s` is exclusively borrowed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga2_camdir_add(
    s: *mut CamDir,
    list_empty: bool,
    yaw: bool,
    x: f32,
) -> f32 {
    // SAFETY: caller contract.
    helpers::camdir_add_and_get(unsafe { rw(s) }, list_empty, yaw, x)
}

/// Returns true and the amount when the health regain applies.
///
/// # Safety
/// `amount` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga2_hurt_regain(
    health: f32,
    limit: f32,
    speed: f32,
    dt: f32,
    amount: *mut f32,
) -> bool {
    match helpers::hurt_regain(health, limit, speed, dt) {
        Some(a) => {
            // SAFETY: caller contract.
            unsafe { *rw(amount) = a };
            true
        }
        None => false,
    }
}

/// # Safety
/// `s` exclusively borrowed, `out` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga2_hurt_update(
    s: *mut Hurt,
    health: f32,
    dead: bool,
    speed: f32,
    dt: f32,
    out: *mut Ops,
) {
    // SAFETY: caller contract.
    let (s, o) = unsafe { (rw(s), rw(out)) };
    *o = Ops::default();
    helpers::hurt_update(s, health, dead, speed, dt, o);
}

/// # Safety
/// `s` exclusively borrowed, `goal` readable, `out` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga2_lookat_update(
    s: *mut LookAt,
    goal: *const Vector3f,
    pitch: f32,
    yaw: f32,
    dt: f32,
    out: *mut Ops,
) {
    // SAFETY: caller contract.
    let (s, o, g) = unsafe { (rw(s), rw(out), *rd(goal)) };
    *o = Ops::default();
    helpers::lookat_update(s, g, pitch, yaw, dt, o);
}

/// # Safety
/// `s` exclusively borrowed, `out` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga2_flashback_update(
    s: *mut Flashback,
    queued: bool,
    voice_done: bool,
    has_callback: bool,
    dt: f32,
    out: *mut Ops,
) {
    // SAFETY: caller contract.
    let (s, o) = unsafe { (rw(s), rw(out)) };
    *o = Ops::default();
    helpers::flashback_update(s, queued, voice_done, has_callback, dt, o);
}

/// # Safety
/// `s` exclusively borrowed, `out` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga2_lantern_update(
    s: *mut Lantern,
    oil: f32,
    emotion_flash: bool,
    dt: f32,
    out: *mut Ops,
) -> bool {
    // SAFETY: caller contract.
    let (s, o) = unsafe { (rw(s), rw(out)) };
    *o = Ops::default();
    helpers::lantern_update(s, oil, emotion_flash, dt, o)
}

/// # Safety
/// `p` readable, `out` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga2_lantern_set_active(p: *const LanternSet, out: *mut Ops) {
    // SAFETY: caller contract.
    let (p, o) = unsafe { (rd(p), rw(out)) };
    *o = Ops::default();
    helpers::lantern_set_active(p, o);
}

/// # Safety
/// `out` has 5 entries.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga2_light_positions(
    pos: *const Vector3f,
    size: *const Vector3f,
    forward: *const Vector3f,
    out: *mut Vector3f,
) {
    // SAFETY: caller contract.
    let r = unsafe { helpers::light_test_positions(*rd(pos), *rd(size), *rd(forward)) };
    for (i, v) in r.iter().enumerate() {
        // SAFETY: `out` has 5 entries.
        unsafe { *rw(out.add(i)) = *v };
    }
}

/// # Safety
/// `count` exclusively borrowed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga2_light_timer(count: *mut f32, dt: f32) -> bool {
    // SAFETY: caller contract.
    helpers::light_timer(unsafe { rw(count) }, dt)
}

/// # Safety
/// `ext` and `normal` have 5 entries; the outputs are writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga2_light_combine(
    lantern: bool,
    ext: *const f32,
    normal: *const f32,
    out_ext: *mut f32,
    out_normal: *mut f32,
) {
    // SAFETY: caller contract.
    unsafe {
        let mut e = [0.0f32; 5];
        let mut n = [0.0f32; 5];
        for (d, s) in e.iter_mut().zip(slice(ext, 5)) {
            *d = *s;
        }
        for (d, s) in n.iter_mut().zip(slice(normal, 5)) {
            *d = *s;
        }
        let (a, b) = helpers::light_combine(lantern, &e, &n);
        *rw(out_ext) = a;
        *rw(out_normal) = b;
    }
}

/// # Safety
/// `s` exclusively borrowed, `i` readable, `out` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga2_darkness_update(
    s: *mut Darkness,
    i: *const DarknessIn,
    dt: f32,
    out: *mut Ops,
) {
    // SAFETY: caller contract.
    let (s, i, o) = unsafe { (rw(s), rd(i), rw(out)) };
    *o = Ops::default();
    helpers::darkness_update(s, i, dt, o);
}

/// # Safety
/// `s` exclusively borrowed, `out` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga2_death_phase1(s: *mut Death, perma: bool, dt: f32, out: *mut Ops) {
    // SAFETY: caller contract.
    let (s, o) = unsafe { (rw(s), rw(out)) };
    *o = Ops::default();
    helpers::death_phase1(s, perma, dt, o);
}

/// # Safety
/// `s` exclusively borrowed, `out` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga2_death_phase2(
    s: *mut Death,
    hard_mode_active: bool,
    dt: f32,
    out: *mut Ops,
) {
    // SAFETY: caller contract.
    let (s, o) = unsafe { (rw(s), rw(out)) };
    *o = Ops::default();
    helpers::death_phase2(s, hard_mode_active, dt, o);
}

/// # Safety
/// `out` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga2_death_phase3(state: i32, voice_gone: bool, out: *mut Ops) {
    // SAFETY: caller contract.
    let o = unsafe { rw(out) };
    *o = Ops::default();
    helpers::death_phase3(state, voice_gone, o);
}

/// # Safety
/// `s` exclusively borrowed, `out` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga2_sanity_begin(
    s: *mut Sanity,
    sanity: f32,
    dt: f32,
    out: *mut Ops,
) {
    // SAFETY: caller contract.
    let (s, o) = unsafe { (rw(s), rw(out)) };
    *o = Ops::default();
    sanity::sanity_begin(s, sanity, dt, o);
}

/// # Safety
/// `s` exclusively borrowed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga2_sanity_scan_due(s: *mut Sanity, dt: f32) -> bool {
    // SAFETY: caller contract.
    sanity::sanity_scan_due(unsafe { rw(s) }, dt)
}

/// # Safety
/// `s` exclusively borrowed, `out` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga2_sanity_scan_result(
    s: *mut Sanity,
    seen_enemy: bool,
    enemy_decrease: f32,
    out: *mut Ops,
) {
    // SAFETY: caller contract.
    let (s, o) = unsafe { (rw(s), rw(out)) };
    *o = Ops::default();
    sanity::sanity_scan_result(s, seen_enemy, enemy_decrease, o);
}

/// # Safety
/// `s` exclusively borrowed, `out` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga2_sanity_enemy_seen(s: *mut Sanity, dt: f32, out: *mut Ops) {
    // SAFETY: caller contract.
    let (s, o) = unsafe { (rw(s), rw(out)) };
    *o = Ops::default();
    sanity::sanity_enemy_seen_effect(s, dt, o);
}

/// # Safety
/// `s` exclusively borrowed, `out` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga2_sanity_hit(s: *mut Sanity, dead: bool, dt: f32, out: *mut Ops) {
    // SAFETY: caller contract.
    let (s, o) = unsafe { (rw(s), rw(out)) };
    *o = Ops::default();
    sanity::sanity_update_hit(s, dead, dt, o);
}

/// # Safety
/// `s` exclusively borrowed, `out` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga2_sanity_effects(
    s: *mut Sanity,
    sanity: f32,
    dead: bool,
    dt: f32,
    out: *mut Ops,
) {
    // SAFETY: caller contract.
    let (s, o) = unsafe { (rw(s), rw(out)) };
    *o = Ops::default();
    sanity::sanity_insane_effects(s, sanity, dead, dt, o);
}

/// Writes the new state to `new_state` and returns whether hand objects update afterwards.
///
/// # Safety
/// `new_state` and `out` are writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga2_hands_update(
    state: i32,
    has_object: bool,
    anim_over: bool,
    object_anim_over: bool,
    new_state: *mut i32,
    out: *mut Ops,
) -> bool {
    // SAFETY: caller contract.
    let o = unsafe { rw(out) };
    *o = Ops::default();
    let (s, run) = hands::hands_update(state, has_object, anim_over, object_anim_over, o);
    // SAFETY: caller contract.
    unsafe { *rw(new_state) = s };
    run
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga2_hands_action_gate(state: i32, has_object: bool) -> bool {
    hands::hands_action_gate(state, has_object)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga2_hands_switch_plan(has_current: bool, has_new: bool) -> i32 {
    hands::hands_switch_plan(has_current, has_new)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga2_rotations_pop(size: usize, max: i32) -> bool {
    hands::rotations_pop(size, max)
}

/// # Safety
/// `p` points to `n` vectors, `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga2_hands_rotation(p: *const Vector3f, n: usize, out: *mut Vector3f) {
    // SAFETY: caller contract.
    unsafe { *rw(out) = hands::hands_final_rotation(slice(p, n)) };
}

/// # Safety
/// `p` readable, `out` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga2_hands_translation(p: *const HandsPos, out: *mut Vector3f) {
    // SAFETY: caller contract.
    unsafe { *rw(out) = hands::hands_translation(rd(p)) };
}
