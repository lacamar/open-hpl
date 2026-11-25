//! C ABI of the GA4b ports, mirrored in `amnesia/src/game/RustLuxProp.h`: critters, hand objects,
//! interact connections, the smaller props and the entity base.
#![allow(clippy::missing_safety_doc, clippy::too_many_arguments)]

use super::critter::{
    self, BugParams, BugState, CritterParams, CritterState, Query, Reply, SpiderParams, SpiderState,
};
use super::entity::{LookIn, Los};
use super::ffi::{Sink, VarsRef};
use super::hand::{self, Fade, Hands, LightAttrs, LightParams, MeleeAct, Sway};
use super::interact;
use super::ops::{pod, rd, rw, with_ops};
use super::smallprops::{self, DoorAct, OilFill};
use super::vars::Vars;
use crate::types::{Color, Matrixf, Vector2f, Vector3f};
use std::ffi::c_void;

const _: () = {
    use std::mem::size_of;
    assert!(size_of::<CritterParams>() == 100);
    assert!(size_of::<CritterState>() == 880);
    assert!(size_of::<BugParams>() == 72);
    assert!(size_of::<BugState>() == 16);
    assert!(size_of::<SpiderParams>() == 52);
    assert!(size_of::<SpiderState>() == 28);
    assert!(size_of::<Reply>() == 20);
    assert!(size_of::<LightAttrs>() == 60);
    assert!(size_of::<LightParams>() == 60);
    assert!(size_of::<Fade>() == 8);
    assert!(size_of::<Sway>() == 8);
    assert!(size_of::<Hands>() == 12);
    assert!(size_of::<MeleeAct>() == 4);
    assert!(size_of::<DoorAct>() == 8);
    assert!(size_of::<OilFill>() == 20);
    assert!(size_of::<LookIn>() == 88);
};

#[repr(C)]
pub struct Vars2Ref {
    pub base: VarsRef,
    pub vec2: unsafe extern "C" fn(*mut c_void, *const u8, usize, *const Vector2f, *mut Vector2f),
}

impl Vars for Vars2Ref {
    fn string(&self, name: &str, def: &[u8]) -> Vec<u8> {
        self.base.string(name, def)
    }
    fn boolean(&self, name: &str, def: bool) -> bool {
        self.base.boolean(name, def)
    }
    fn float(&self, name: &str, def: f32) -> f32 {
        self.base.float(name, def)
    }
    fn int(&self, name: &str, def: i32) -> i32 {
        self.base.int(name, def)
    }
    fn vec3(&self, name: &str, def: Vector3f) -> Vector3f {
        self.base.vec3(name, def)
    }
    fn color(&self, name: &str, def: Color) -> Color {
        self.base.color(name, def)
    }
    fn put(&self, slot: i32, s: &[u8]) {
        self.base.put(slot, s);
    }
}

impl Vars2Ref {
    pub fn vec2(&self, name: &str, def: Vector2f) -> Vector2f {
        let mut out = def;
        unsafe { (self.vec2)(self.base.ctx, name.as_ptr(), name.len(), &def, &mut out) };
        out
    }
}

#[repr(C)]
pub struct QueryRef {
    pub ctx: *mut c_void,
    pub ask:
        unsafe extern "C" fn(*mut c_void, i32, *const Vector3f, *const Vector3f, i32, *mut Reply),
}

impl Query for &QueryRef {
    fn ask(&mut self, code: i32, a: Vector3f, b: Vector3f, flags: i32) -> Reply {
        let mut out = Reply::default();
        // SAFETY: the C++ side keeps the table alive for the call.
        unsafe { (self.ask)(self.ctx, code, &a, &b, flags, &mut out) };
        out
    }
}

// ---- critter ----

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4b_critter_load(v: *const Vars2Ref, out: *mut CritterParams) {
    critter::load(rd(v), rw(out));
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4b_critter_state_new(s: *mut CritterState) {
    // SAFETY: `s` is uninitialised storage for one `CritterState`.
    unsafe { s.write(CritterState::new()) }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4b_critter_setup_dirs(
    s: *mut CritterState,
    ground: *const Vector3f,
    forward: *const Vector3f,
) {
    critter::setup_dirs(rw(s), *rd(ground), *rd(forward));
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4b_critter_in_range(
    feet: *const Vector3f,
    body: *const Vector3f,
) -> bool {
    critter::in_range(*rd(feet), *rd(body))
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4b_critter_update_movement(
    s: *mut CritterState,
    q: *const QueryRef,
    sink: *const Sink,
    dt: f32,
) {
    with_ops(rd(sink), |ops| {
        critter::update_movement(pod(s), &mut rd(q), ops, dt);
    });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4b_critter_mesh_basis(s: *mut CritterState, out: *mut Matrixf) {
    *rw(out) = critter::mesh_basis(rw(s));
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4b_critter_mesh_matrix(
    rot_t: *const Matrixf,
    offset: *const Matrixf,
    skeleton: bool,
    out: *mut Matrixf,
) {
    *rw(out) = if skeleton {
        critter::mesh_matrix_skeleton(rd(rot_t), rd(offset))
    } else {
        critter::mesh_matrix_static(rd(rot_t), rd(offset))
    };
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4b_critter_anim_step(s: *mut CritterState, sink: *const Sink) {
    with_ops(rd(sink), |ops| critter::anim_step(rw(s), ops));
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4b_bug_load(v: *const Vars2Ref, p: *mut BugParams) {
    critter::load_bug(rd(v), rw(p));
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4b_bug_state_new(s: *mut BugState) {
    // SAFETY: `s` is uninitialised storage for one `BugState`.
    unsafe { s.write(BugState::new()) }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4b_bug_update_velocity(
    c: *mut CritterState,
    b: *mut BugState,
    p: *const BugParams,
    q: *const QueryRef,
    sink: *const Sink,
    dt: f32,
) {
    with_ops(rd(sink), |ops| {
        critter::bug_update_velocity(pod(c), pod(b), rd(p), &mut rd(q), ops, dt);
    });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4b_spider_load(v: *const Vars2Ref, p: *mut SpiderParams) {
    critter::load_spider(rd(v), rw(p));
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4b_spider_new(c: *mut CritterState, s: *mut SpiderState) {
    let mut st = SpiderState::default();
    critter::spider_new(rw(c), &mut st);
    // SAFETY: `s` is uninitialised storage for one `SpiderState`.
    unsafe { s.write(st) };
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4b_spider_on_damage(
    c: *mut CritterState,
    s: *mut SpiderState,
    amount: f32,
    sink: *const Sink,
) {
    with_ops(rd(sink), |ops| {
        critter::spider_on_damage(rw(c), rw(s), amount, ops);
    });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4b_spider_update_velocity(
    c: *mut CritterState,
    s: *mut SpiderState,
    p: *const SpiderParams,
    q: *const QueryRef,
    sink: *const Sink,
    dt: f32,
) {
    with_ops(rd(sink), |ops| {
        critter::spider_update_velocity(pod(c), pod(s), rd(p), &mut rd(q), ops, dt);
    });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4b_spider_ground_check(
    c: *mut CritterState,
    s: *mut SpiderState,
    q: *const QueryRef,
    dt: f32,
) {
    critter::spider_ground_check(pod(c), pod(s), &mut rd(q), dt);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4b_spider_on_shape_collision(
    c: *mut CritterState,
    s: *mut SpiderState,
    q: *const QueryRef,
    push: *const Vector3f,
    dt: f32,
) {
    critter::spider_on_shape_collision(pod(c), pod(s), &mut rd(q), *rd(push), dt);
}

// ---- hand objects ----

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4b_hand_light_load(a: *const LightAttrs, out: *mut LightParams) {
    *rw(out) = hand::light_load(rd(a));
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4b_hand_light_fade(
    p: *const LightParams,
    holster: bool,
    alpha: f32,
    dt: f32,
    out: *mut Fade,
) {
    *rw(out) = hand::light_fade(rd(p), holster, alpha, dt);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4b_hand_light_sway(
    p: *const LightParams,
    s: *mut Sway,
    pitch: f32,
    roll: f32,
    player_speed: f32,
    moving_back: bool,
    dt: f32,
    out: *mut Matrixf,
) {
    *rw(out) = hand::sway_step(rd(p), rw(s), pitch, roll, player_speed, moving_back, dt);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4b_hand_melee_do_action(
    hands_state: i32,
    pressed: bool,
    h: *mut Hands,
    out: *mut MeleeAct,
) {
    *rw(out) = hand::melee_do_action(hands_state, pressed, rw(h));
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4b_hand_melee_anim_over(h: *mut Hands, out: *mut MeleeAct) {
    *rw(out) = hand::melee_anim_over(rw(h));
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4b_hand_melee_update_charge(
    h: *mut Hands,
    charge_time: f32,
    dt: f32,
) -> bool {
    hand::melee_update_charge(rw(h), charge_time, dt)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4b_hand_melee_swing(h: *mut Hands) -> bool {
    hand::melee_swing(rw(h))
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4b_hand_melee_charge(h: *mut Hands) {
    hand::melee_charge(rw(h));
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4b_hand_melee_damage_mul(h: *const Hands) -> f32 {
    hand::melee_damage_mul(rd(h))
}

// ---- interact connections ----

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga4b_rope_wheel_angle(
    length: f32,
    min_len: f32,
    max_len: f32,
    invert: bool,
    min_limit: f32,
    max_limit: f32,
) -> f32 {
    interact::rope_wheel_angle(length, min_len, max_len, invert, min_limit, max_limit)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga4b_rope_turn_length(
    t: f32,
    invert: bool,
    min_len: f32,
    max_len: f32,
) -> f32 {
    interact::rope_turn_length(t, invert, min_len, max_len)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4b_rope_limit_length(
    is_wheel: bool,
    state: i32,
    used: i32,
    invert: bool,
    min_len: f32,
    max_len: f32,
    out: *mut f32,
) -> bool {
    match interact::rope_limit_length(is_wheel, state, used, invert, min_len, max_len) {
        Some(v) => {
            *rw(out) = v;
            true
        }
        None => false,
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4b_move_update_gate(
    interaction_only: bool,
    moving: bool,
    needs_update: *mut bool,
) -> bool {
    interact::move_update_gate(interaction_only, moving, rw(needs_update))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga4b_move_wheel_angle(
    t: f32,
    invert: bool,
    min_limit: f32,
    max_limit: f32,
) -> f32 {
    interact::move_wheel_angle(t, invert, min_limit, max_limit)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga4b_move_turn_state(t: f32, invert: bool) -> f32 {
    interact::move_turn_state(t, invert)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4b_move_limit_state(
    is_wheel: bool,
    state: i32,
    used: i32,
    invert: bool,
    out: *mut f32,
) -> bool {
    match interact::move_limit_state(is_wheel, state, used, invert) {
        Some(v) => {
            *rw(out) = v;
            true
        }
        None => false,
    }
}

// ---- entity ----

#[repr(C)]
pub struct LosRef {
    pub ctx: *mut c_void,
    pub los: unsafe extern "C" fn(*mut c_void, *const Vector3f, *const Vector3f) -> bool,
}

impl Los for &LosRef {
    fn line_of_sight(&mut self, start: Vector3f, end: Vector3f) -> bool {
        // SAFETY: the C++ side keeps the table alive for the call.
        unsafe { (self.los)(self.ctx, &start, &end) }
    }
}

// ---- small props ----

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga4b_button_can_interact(on: bool, can_on: bool, can_off: bool) -> bool {
    smallprops::button_can_interact(on, can_on, can_off)
}
