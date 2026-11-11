//! C ABI of the prop core, mirrored in `amnesia/src/game/RustLuxProp.h`.
//!
//! Every pointer is valid for the call and state structs are exclusively borrowed; ops leave
//! through the sink, queries through the host table.
#![allow(clippy::missing_safety_doc, clippy::too_many_arguments)]

use super::base::{
    self, BaseInstance, BaseParams, BaseState, EffectsIn, LightData, MoveIn, OutsideBody,
};
use super::lamp::{self, LampLink, LampParams, LampState};
use super::lever::{self, LeverParams, LeverState};
use super::moveobject::{self, MoveObjState, MoveParams};
use super::multislider::{self, MultiParams, MultiState};
use super::object::{
    self, ImpactBody, JointData, ObjectInstance, ObjectParams, ObjectState, UpdateIn,
};
use super::ops::{Hinge, Op, Ops, Pod, Probe, Slider};
use super::swingdoor::{self, DoorJoint, DoorParams, DoorState, Env};
use super::vars::Vars;
use super::wheel::{self, WheelParams, WheelState};
use crate::string_ffi::units;
use crate::types::{Color, Matrixf, Vector3f};
use std::ffi::c_void;

const _: () = {
    use std::mem::size_of;
    assert!(size_of::<BaseParams>() == 56);
    assert!(size_of::<BaseInstance>() == 5);
    assert!(size_of::<LightData>() == 24);
    assert!(size_of::<super::base::Angular>() == 124);
    assert!(size_of::<BaseState>() == 244);
    assert!(size_of::<EffectsIn>() == 28);
    assert!(size_of::<MoveIn>() == 72);
    assert!(size_of::<OutsideBody>() == 28);
    assert!(size_of::<super::object::GrabData>() == 68);
    assert!(size_of::<super::object::MoveData>() == 16);
    assert!(size_of::<ObjectParams>() == 136);
    assert!(size_of::<ObjectState>() == 24);
    assert!(size_of::<JointData>() == 16);
    assert!(size_of::<DoorParams>() == 32);
    assert!(size_of::<DoorState>() == 44);
    assert!(size_of::<WheelState>() == 48);
    assert!(size_of::<LeverParams>() == 52);
    assert!(size_of::<LampParams>() == 72);
    assert!(size_of::<MultiParams>() == 40);
    assert!(size_of::<MoveParams>() == 56);
    assert!(size_of::<MoveObjState>() == 144);
    assert!(size_of::<Hinge>() == 112);
    assert!(size_of::<Slider>() == 32);
};

fn rd<'a, T>(p: *const T) -> &'a T {
    // SAFETY: caller contract.
    unsafe { &*p }
}

fn vr<'a>(p: *const VarsRef) -> &'a VarsRef {
    // SAFETY: caller contract.
    unsafe { &*p }
}

fn hr<'a>(p: *const Host) -> &'a Host {
    // SAFETY: caller contract.
    unsafe { &*p }
}

fn pod<T>(p: *mut T) -> Pod<T> {
    // SAFETY: caller contract: `p` is live state.
    unsafe { Pod::from_ptr(p) }
}

fn rw<'a, T>(p: *mut T) -> &'a mut T {
    // SAFETY: caller contract.
    unsafe { &mut *p }
}

fn sl<'a, T>(p: *const T, n: i32) -> &'a [T] {
    match usize::try_from(n) {
        // SAFETY: caller contract: `p` holds `n` elements.
        Ok(n) if n > 0 && !p.is_null() => unsafe { std::slice::from_raw_parts(p, n) },
        _ => &[],
    }
}

#[repr(C)]
pub struct VarsOps {
    pub get_string: unsafe extern "C" fn(
        *mut c_void,
        *const u8,
        usize,
        *const u8,
        usize,
        unsafe extern "C" fn(*mut c_void, *const u8, usize),
        *mut c_void,
    ),
    pub get_bool: unsafe extern "C" fn(*mut c_void, *const u8, usize, bool) -> bool,
    pub get_float: unsafe extern "C" fn(*mut c_void, *const u8, usize, f32) -> f32,
    pub get_int: unsafe extern "C" fn(*mut c_void, *const u8, usize, i32) -> i32,
    pub get_vec3:
        unsafe extern "C" fn(*mut c_void, *const u8, usize, *const Vector3f, *mut Vector3f),
    pub get_color: unsafe extern "C" fn(*mut c_void, *const u8, usize, *const Color, *mut Color),
    pub put: unsafe extern "C" fn(*mut c_void, i32, *const u8, usize),
}

#[repr(C)]
pub struct VarsRef {
    pub ops: *const VarsOps,
    pub ctx: *mut c_void,
}

unsafe extern "C" fn collect(ctx: *mut c_void, p: *const u8, n: usize) {
    // SAFETY: `ctx` is the `Vec` handed to `get_string`; `p` holds `n` bytes.
    unsafe { (*ctx.cast::<Vec<u8>>()).extend_from_slice(units(p, n)) }
}

impl Vars for VarsRef {
    fn string(&self, name: &str, def: &[u8]) -> Vec<u8> {
        let mut out = Vec::new();
        // SAFETY: the C++ side keeps `ops` and `ctx` alive for the call.
        unsafe {
            ((*self.ops).get_string)(
                self.ctx,
                name.as_ptr(),
                name.len(),
                def.as_ptr(),
                def.len(),
                collect,
                std::ptr::from_mut(&mut out).cast(),
            );
        }
        out
    }

    fn boolean(&self, name: &str, def: bool) -> bool {
        // SAFETY: as in `string`.
        unsafe { ((*self.ops).get_bool)(self.ctx, name.as_ptr(), name.len(), def) }
    }

    fn float(&self, name: &str, def: f32) -> f32 {
        // SAFETY: as in `string`.
        unsafe { ((*self.ops).get_float)(self.ctx, name.as_ptr(), name.len(), def) }
    }

    fn int(&self, name: &str, def: i32) -> i32 {
        // SAFETY: as in `string`.
        unsafe { ((*self.ops).get_int)(self.ctx, name.as_ptr(), name.len(), def) }
    }

    fn vec3(&self, name: &str, def: Vector3f) -> Vector3f {
        let mut out = def;
        // SAFETY: as in `string`.
        unsafe { ((*self.ops).get_vec3)(self.ctx, name.as_ptr(), name.len(), &def, &mut out) };
        out
    }

    fn color(&self, name: &str, def: Color) -> Color {
        let mut out = def;
        // SAFETY: as in `string`.
        unsafe { ((*self.ops).get_color)(self.ctx, name.as_ptr(), name.len(), &def, &mut out) };
        out
    }

    fn put(&self, slot: i32, s: &[u8]) {
        // SAFETY: as in `string`.
        unsafe { ((*self.ops).put)(self.ctx, slot, s.as_ptr(), s.len()) }
    }
}

#[repr(C)]
pub struct Sink {
    pub ctx: *mut c_void,
    pub push: unsafe extern "C" fn(*mut c_void, *const Op),
}

#[repr(C)]
pub struct Host {
    pub ctx: *mut c_void,
    pub move_push: unsafe extern "C" fn(*mut c_void, *const Vector3f, *mut Vector3f),
    pub outside_collides: unsafe extern "C" fn(*mut c_void, i32) -> bool,
    pub insanity_in_fov: unsafe extern "C" fn(*mut c_void) -> bool,
    pub hinge: unsafe extern "C" fn(*mut c_void, i32, *mut Hinge),
    pub slider: unsafe extern "C" fn(*mut c_void, *mut Slider),
}

impl Probe for &Host {
    fn move_push(&mut self, add: Vector3f) -> Vector3f {
        let mut out = Vector3f::default();
        // SAFETY: the C++ side keeps the table alive for the call.
        unsafe { (self.move_push)(self.ctx, &add, &mut out) };
        out
    }

    fn outside_collides(&mut self, i: i32) -> bool {
        // SAFETY: as above.
        unsafe { (self.outside_collides)(self.ctx, i) }
    }

    fn insanity_in_fov(&mut self) -> bool {
        // SAFETY: as above.
        unsafe { (self.insanity_in_fov)(self.ctx) }
    }

    fn hinge(&mut self, i: i32) -> Hinge {
        let mut out = Hinge::default();
        // SAFETY: as above.
        unsafe { (self.hinge)(self.ctx, i, &mut out) };
        out
    }

    fn slider(&mut self) -> Slider {
        let mut out = Slider::default();
        // SAFETY: as above.
        unsafe { (self.slider)(self.ctx, &mut out) };
        out
    }
}

/// Runs `f` with an `Ops` that forwards to the C sink.
fn with_ops<R>(sink: &Sink, f: impl FnOnce(&mut Ops) -> R) -> R {
    let (ctx, push) = (sink.ctx, sink.push);
    let mut fwd = move |op: &Op| {
        // SAFETY: the sink stays valid for the call.
        unsafe { push(ctx, op) }
    };
    f(&mut Ops(&mut fwd))
}

macro_rules! host {
    ($h:expr) => {
        &mut hr($h)
    };
}

// ---- base ----

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_base_init(s: *mut BaseState) {
    // SAFETY: `s` is uninitialised storage for one `BaseState`.
    unsafe { s.write(BaseState::new()) }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_base_load(v: *const VarsRef, out: *mut BaseParams) {
    base::load_base(vr(v), rw(out));
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_base_load_instance(
    v: *const VarsRef,
    force_full_game_save: bool,
    out: *mut BaseInstance,
) {
    base::load_base_instance(vr(v), force_full_game_save, rw(out));
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_give_damage(
    health: f32,
    toughness: i32,
    amount: f32,
    strength: i32,
    out_amount: *mut f32,
) -> f32 {
    let (h, a) = base::give_damage(health, toughness, amount, strength);
    *rw(out_amount) = a;
    h
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_effects_set_active(
    s: *mut BaseState,
    a: *const EffectsIn,
    lights: *const LightData,
    n_lights: i32,
    sink: *const Sink,
) {
    with_ops(rd(sink), |ops| {
        base::effects_set_active(rw(s), rd(a), sl(lights, n_lights), ops);
    });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_effects_update(
    s: *mut BaseState,
    dt: f32,
    on_time: f32,
    off_time: f32,
    lights: *const LightData,
    n_lights: i32,
    has_mesh: bool,
    sink: *const Sink,
) {
    with_ops(rd(sink), |ops| {
        base::effects_update(
            rw(s),
            dt,
            on_time,
            off_time,
            sl(lights, n_lights),
            has_mesh,
            ops,
        );
    });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_fade_in_mesh(
    s: *mut BaseState,
    time: f32,
    has_mesh: bool,
    sink: *const Sink,
) {
    with_ops(rd(sink), |ops| {
        base::fade_in_mesh(rw(s), time, has_mesh, ops)
    });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_mesh_fading(
    s: *mut BaseState,
    dt: f32,
    has_mesh: bool,
    sink: *const Sink,
) {
    with_ops(rd(sink), |ops| base::mesh_fading(rw(s), dt, has_mesh, ops));
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_anim_update(
    index: *mut i32,
    over: bool,
    has_callback: bool,
    sink: *const Sink,
) {
    with_ops(rd(sink), |ops| {
        base::anim_update(pod(index), over, has_callback, ops)
    });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga4_connection_turn(angle: f32, min: f32, max: f32) -> f32 {
    base::connection_turn(angle, min, max)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_reset_move(s: *mut BaseState) {
    rw(s).reset_move();
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_move_linear_to(
    s: *mut BaseState,
    goal: *const Vector3f,
    acc: f32,
    max_speed: f32,
    slowdown: f32,
    reset: bool,
    sink: *const Sink,
) {
    with_ops(rd(sink), |ops| {
        rw(s).move_linear_to(*rd(goal), acc, max_speed, slowdown, reset, ops);
    });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_move_angular_to(
    s: *mut BaseState,
    goal: *const Matrixf,
    acc: f32,
    max_speed: f32,
    slowdown: f32,
    reset: bool,
    use_offset: bool,
    world_offset: *const Vector3f,
    local_offset: *const Vector3f,
    sink: *const Sink,
) {
    with_ops(rd(sink), |ops| {
        rw(s).move_angular_to(
            rd(goal),
            acc,
            max_speed,
            slowdown,
            reset,
            use_offset,
            *rd(world_offset),
            *rd(local_offset),
            ops,
        );
    });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_rotate_at_speed(
    s: *mut BaseState,
    acc: f32,
    goal_speed: f32,
    axis: *const Vector3f,
    reset: bool,
    use_offset: bool,
    world_offset: *const Vector3f,
    local_offset: *const Vector3f,
    sink: *const Sink,
) {
    with_ops(rd(sink), |ops| {
        rw(s).rotate_at_speed(
            acc,
            goal_speed,
            *rd(axis),
            reset,
            use_offset,
            *rd(world_offset),
            *rd(local_offset),
            ops,
        );
    });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_stop_move(s: *mut BaseState, sink: *const Sink) {
    with_ops(rd(sink), |ops| rw(s).stop_move(ops));
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_update_moving(
    s: *mut BaseState,
    a: *const MoveIn,
    host: *const Host,
    sink: *const Sink,
) {
    with_ops(rd(sink), |ops| {
        base::update_moving(rw(s), rd(a), host!(host), ops)
    });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_outside_player(
    bodies: *const OutsideBody,
    n: i32,
    player_pos: *const Vector3f,
    host: *const Host,
    sink: *const Sink,
) -> bool {
    with_ops(rd(sink), |ops| {
        base::outside_player(sl(bodies, n), *rd(player_pos), host!(host), ops)
    })
}

// ---- object ----

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_object_load(
    v: *const VarsRef,
    sub_type: *const u8,
    sub_type_len: usize,
    focus: *mut f32,
    defaults: *const f32,
    out: *mut ObjectParams,
) {
    let d = sl(defaults, 3);
    let d = [
        d.first().copied().unwrap_or(0.0),
        d.get(1).copied().unwrap_or(0.0),
        d.get(2).copied().unwrap_or(0.0),
    ];
    object::load(
        vr(v), // SAFETY: caller contract.
        unsafe { units(sub_type, sub_type_len) },
        rw(focus),
        d,
        rw(out),
    );
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_object_load_instance(
    v: *const VarsRef,
    s: *mut ObjectState,
    out: *mut ObjectInstance,
    sink: *const Sink,
) {
    with_ops(rd(sink), |ops| {
        object::load_instance(vr(v), rw(s), rw(out), ops)
    });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_object_can_interact(
    p: *const ObjectParams,
    joint_num: i32,
    mass: f32,
    in_sticky_area: bool,
    has_interact_callback: bool,
) -> bool {
    object::can_interact(
        rd(p),
        joint_num,
        mass,
        in_sticky_area,
        has_interact_callback,
    )
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_object_focus_crosshair(
    p: *const ObjectParams,
    can_interact: bool,
    custom_is_default: bool,
) -> i32 {
    object::focus_crosshair(rd(p), can_interact, custom_is_default)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_object_show_outlines(p: *const ObjectParams) -> bool {
    object::show_outlines_on_connected(rd(p))
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_object_impact_energy(
    bodies: *const ImpactBody,
    point: *const Vector3f,
    normal: *const Vector3f,
) -> f32 {
    let b = sl(bodies, 2);
    match (b.first(), b.get(1)) {
        (Some(a), Some(c)) => object::impact_energy(&[*a, *c], *rd(point), *rd(normal)),
        _ => 0.0,
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_object_hit_hurts(p: *const ObjectParams, speed: f32) -> bool {
    object::hit_hurts(rd(p), speed)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_object_on_health_change(
    p: *const ObjectParams,
    health: f32,
    sink: *const Sink,
) {
    with_ops(rd(sink), |ops| object::on_health_change(rd(p), health, ops));
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_object_update(
    s: *mut ObjectState,
    p: *const ObjectParams,
    i: *const ObjectInstance,
    a: *const UpdateIn,
    host: *const Host,
    sink: *const Sink,
) {
    with_ops(rd(sink), |ops| {
        object::update(pod(s), rd(p), rd(i), rd(a), host!(host), ops);
    });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_object_joint_data(
    kind: i32,
    min: f32,
    max: f32,
    out: *mut JointData,
) {
    *rw(out) = object::joint_data(kind, min, max);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_object_set_stuck_state(
    s: *mut ObjectState,
    state: i32,
    joints: *const JointData,
    n: i32,
    sink: *const Sink,
) {
    with_ops(rd(sink), |ops| {
        object::set_stuck_state(rw(s), state, sl(joints, n), ops);
    });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_object_set_insanity_visible(
    s: *mut ObjectState,
    visible: bool,
    sink: *const Sink,
) {
    with_ops(rd(sink), |ops| {
        object::set_insanity_visible(rw(s), visible, ops)
    });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_break_impulse(
    center: *const Vector3f,
    origin: *const Vector3f,
    impulse: f32,
    add: *const Vector3f,
    out: *mut Vector3f,
) {
    let add = if add.is_null() { None } else { Some(*rd(add)) };
    *rw(out) = object::break_impulse(*rd(center), *rd(origin), impulse, add);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_bodies_center(pos: *const Vector3f, n: i32, out: *mut Vector3f) {
    *rw(out) = object::bodies_center(sl(pos, n));
}

// ---- swing door ----

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_door_load(v: *const VarsRef, out: *mut DoorParams) {
    swingdoor::load(vr(v), rw(out));
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_door_state_new(s: *mut DoorState) {
    // SAFETY: `s` is uninitialised storage for one `DoorState`.
    unsafe { s.write(DoorState::new()) }
}

macro_rules! env {
    ($j:expr, $n:expr, $nj:expr) => {
        &Env {
            joints: sl($j, $n),
            n_joints: $nj,
        }
    };
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_door_load_instance(
    v: *const VarsRef,
    s: *mut DoorState,
    joints: *const DoorJoint,
    n: i32,
    n_joints: i32,
    host: *const Host,
    sink: *const Sink,
) {
    with_ops(rd(sink), |ops| {
        swingdoor::load_instance(vr(v), rw(s), env!(joints, n, n_joints), host!(host), ops);
    });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_door_can_interact(
    p: *const DoorParams,
    mass: f32,
    in_sticky_area: bool,
) -> bool {
    swingdoor::can_interact(rd(p), mass, in_sticky_area)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_door_set_closed(
    s: *mut DoorState,
    joints: *const DoorJoint,
    n: i32,
    n_joints: i32,
    closed: bool,
    effects: bool,
    sink: *const Sink,
) {
    with_ops(rd(sink), |ops| {
        swingdoor::set_closed(rw(s), env!(joints, n, n_joints), closed, effects, ops);
    });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_door_state(
    joints: *const DoorJoint,
    n: i32,
    host: *const Host,
) -> i32 {
    swingdoor::door_state(env!(joints, n, n), host!(host))
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_door_set_locked(
    s: *mut DoorState,
    joints: *const DoorJoint,
    n: i32,
    n_joints: i32,
    locked: bool,
    effects: bool,
    sink: *const Sink,
) {
    with_ops(rd(sink), |ops| {
        swingdoor::set_locked(rw(s), env!(joints, n, n_joints), locked, effects, ops);
    });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_door_set_damage_level(
    s: *mut DoorState,
    level: i32,
    sink: *const Sink,
) {
    with_ops(rd(sink), |ops| {
        swingdoor::set_damage_level(rw(s), level, ops)
    });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_door_on_connection_state(
    s: *mut DoorState,
    joints: *const DoorJoint,
    n: i32,
    n_joints: i32,
    state: i32,
    sink: *const Sink,
) {
    with_ops(rd(sink), |ops| {
        swingdoor::on_connection_state(rw(s), env!(joints, n, n_joints), state, ops);
    });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_door_on_interact(
    s: *mut DoorState,
    joints: *const DoorJoint,
    n: i32,
    n_joints: i32,
    show_hints: bool,
    sink: *const Sink,
) -> bool {
    with_ops(rd(sink), |ops| {
        swingdoor::on_interact(rw(s), env!(joints, n, n_joints), show_hints, ops)
    })
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_door_setup_physics(
    s: *mut DoorState,
    joints: *const DoorJoint,
    n: i32,
    n_joints: i32,
    open: f32,
    towards_max: bool,
    apply_always: bool,
    host: *const Host,
    sink: *const Sink,
) {
    with_ops(rd(sink), |ops| {
        swingdoor::setup_physics(
            rw(s),
            env!(joints, n, n_joints),
            open,
            towards_max,
            apply_always,
            host!(host),
            ops,
        );
    });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_door_set_open_amount(
    s: *mut DoorState,
    joints: *const DoorJoint,
    n: i32,
    n_joints: i32,
    open: f32,
    duration: f32,
    towards_max: bool,
    host: *const Host,
    sink: *const Sink,
) {
    with_ops(rd(sink), |ops| {
        swingdoor::set_open_amount(
            rw(s),
            env!(joints, n, n_joints),
            open,
            duration,
            towards_max,
            host!(host),
            ops,
        );
    });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_door_update(
    s: *mut DoorState,
    joints: *const DoorJoint,
    n: i32,
    n_joints: i32,
    dt: f32,
    interacted: bool,
    host: *const Host,
    sink: *const Sink,
) {
    with_ops(rd(sink), |ops| {
        swingdoor::update(
            rw(s),
            env!(joints, n, n_joints),
            dt,
            interacted,
            host!(host),
            ops,
        );
    });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_door_on_health_change(
    s: *mut DoorState,
    p: *const DoorParams,
    health: f32,
    has_main: bool,
    sink: *const Sink,
) {
    with_ops(rd(sink), |ops| {
        swingdoor::on_health_change(pod(s), rd(p), health, has_main, ops);
    });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_door_reset(
    s: *mut DoorState,
    joints: *const DoorJoint,
    n: i32,
    n_joints: i32,
    host: *const Host,
    sink: *const Sink,
) {
    with_ops(rd(sink), |ops| {
        swingdoor::reset(pod(s), env!(joints, n, n_joints), host!(host), ops);
    });
}

// ---- wheel ----

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_wheel_load(v: *const VarsRef, out: *mut WheelParams) {
    wheel::load(vr(v), rw(out));
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_wheel_load_instance(
    v: *const VarsRef,
    s: *mut WheelState,
    p: *mut WheelParams,
    host: *const Host,
    sink: *const Sink,
) {
    with_ops(rd(sink), |ops| {
        wheel::load_instance(vr(v), pod(s), pod(p), host!(host), ops);
    });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_wheel_can_interact(p: *const WheelParams, mass: f32) -> bool {
    wheel::can_interact(rd(p), mass)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_wheel_setup(p: *const WheelParams, sink: *const Sink) {
    with_ops(rd(sink), |ops| wheel::setup(rd(p), ops));
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_wheel_set_stuck_state(
    s: *mut WheelState,
    p: *const WheelParams,
    state: i32,
    effects: bool,
    host: *const Host,
    sink: *const Sink,
) {
    with_ops(rd(sink), |ops| {
        wheel::set_stuck_state(pod(s), rd(p), state, effects, host!(host), ops);
    });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_wheel_set_angle(
    s: *mut WheelState,
    p: *const WheelParams,
    x: f32,
    auto_move: bool,
    sink: *const Sink,
) {
    with_ops(rd(sink), |ops| {
        wheel::set_angle(rw(s), rd(p), x, auto_move, ops)
    });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_wheel_on_connection_state(
    s: *mut WheelState,
    p: *const WheelParams,
    state: i32,
    host: *const Host,
    sink: *const Sink,
) {
    with_ops(rd(sink), |ops| {
        wheel::on_connection_state(pod(s), rd(p), state, host!(host), ops);
    });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_wheel_on_interact(
    s: *mut WheelState,
    p: *const WheelParams,
    show_hints: bool,
    host: *const Host,
    sink: *const Sink,
) -> bool {
    with_ops(rd(sink), |ops| {
        let disables = rd(s).interaction_disables_stuck;
        wheel::on_interact(pod(s), rd(p), show_hints, disables, host!(host), ops)
    })
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_wheel_reset_auto_move(s: *mut WheelState, sink: *const Sink) {
    with_ops(rd(sink), |ops| wheel::reset_auto_move(rw(s), ops));
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_wheel_update(
    s: *mut WheelState,
    p: *const WheelParams,
    dt: f32,
    interacted: bool,
    host: *const Host,
    sink: *const Sink,
) {
    with_ops(rd(sink), |ops| {
        wheel::update(pod(s), rd(p), dt, interacted, host!(host), ops);
    });
}

// ---- lever ----

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_lever_load(v: *const VarsRef, out: *mut LeverParams) {
    lever::load(vr(v), rw(out));
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_lever_load_instance(
    v: *const VarsRef,
    s: *mut LeverState,
    p: *mut LeverParams,
    sink: *const Sink,
) {
    with_ops(rd(sink), |ops| {
        lever::load_instance(vr(v), pod(s), pod(p), ops)
    });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_lever_can_interact(p: *const LeverParams, mass: f32) -> bool {
    lever::can_interact(rd(p), mass)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_lever_setup(
    s: *mut LeverState,
    p: *const LeverParams,
    min: f32,
    max: f32,
    sink: *const Sink,
) {
    with_ops(rd(sink), |ops| lever::setup(rw(s), rd(p), min, max, ops));
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_lever_calculate_middle_angle(
    s: *mut LeverState,
    p: *const LeverParams,
) {
    lever::calculate_middle_angle(rw(s), rd(p));
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_lever_set_stuck_state(
    s: *mut LeverState,
    p: *const LeverParams,
    state: i32,
    effects: bool,
    sink: *const Sink,
) {
    with_ops(rd(sink), |ops| {
        lever::set_stuck_state(pod(s), rd(p), state, effects, ops)
    });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_lever_on_connection_state(
    s: *mut LeverState,
    p: *const LeverParams,
    state: i32,
    sink: *const Sink,
) {
    with_ops(rd(sink), |ops| {
        lever::on_connection_state(pod(s), rd(p), state, ops)
    });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_lever_on_interact(
    s: *mut LeverState,
    p: *const LeverParams,
    show_hints: bool,
    sink: *const Sink,
) -> bool {
    with_ops(rd(sink), |ops| {
        lever::on_interact(pod(s), rd(p), show_hints, ops)
    })
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_lever_update(
    s: *mut LeverState,
    p: *const LeverParams,
    dt: f32,
    interacted: bool,
    host: *const Host,
    sink: *const Sink,
) {
    with_ops(rd(sink), |ops| {
        lever::update(pod(s), rd(p), dt, interacted, host!(host), ops);
    });
}

// ---- lamp ----

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_lamp_load(v: *const VarsRef, out: *mut LampParams) {
    lamp::load(vr(v), rw(out));
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_lamp_state_new(s: *mut LampState) {
    // SAFETY: `s` is uninitialised storage for one `LampState`.
    unsafe { s.write(LampState::new()) }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_lamp_load_instance(
    v: *const VarsRef,
    s: *mut LampState,
    base: *mut BaseState,
    env: *const EffectsIn,
    lights: *const LightData,
    n_lights: i32,
    sink: *const Sink,
) {
    with_ops(rd(sink), |ops| {
        lamp::load_instance(vr(v), rw(s), rw(base), rd(env), sl(lights, n_lights), ops);
    });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_lamp_set_lit(
    s: *mut LampState,
    base: *mut BaseState,
    env: *const EffectsIn,
    lights: *const LightData,
    n_lights: i32,
    lit: bool,
    use_effects: bool,
    sink: *const Sink,
) {
    with_ops(rd(sink), |ops| {
        lamp::set_lit(
            rw(s),
            rw(base),
            rd(env),
            sl(lights, n_lights),
            lit,
            use_effects,
            ops,
        );
    });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_lamp_can_interact(
    s: *const LampState,
    p: *const LampParams,
    has_interact_callback: bool,
) -> bool {
    lamp::can_interact(rd(s), rd(p), has_interact_callback)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_lamp_can_ignite(
    s: *const LampState,
    p: *const LampParams,
) -> bool {
    lamp::can_ignite_by_player(rd(s), rd(p))
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_lamp_focus_crosshair(
    s: *const LampState,
    p: *const LampParams,
    custom_is_default: bool,
    has_callback: bool,
) -> i32 {
    lamp::focus_crosshair(rd(s), rd(p), custom_is_default, has_callback)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga4_lamp_sanity_add(
    sanity: f32,
    min_increase: f32,
    max_increase: f32,
) -> f32 {
    lamp::sanity_add(sanity, min_increase, max_increase)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_lamp_on_connection_state(
    s: *mut LampState,
    base: *mut BaseState,
    env: *const EffectsIn,
    lights: *const LightData,
    n_lights: i32,
    state: i32,
    sink: *const Sink,
) {
    with_ops(rd(sink), |ops| {
        lamp::on_connection_state(rw(s), rw(base), rd(env), sl(lights, n_lights), state, ops);
    });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_lamp_fade_target(
    light: *const Color,
    light_radius: f32,
    rgba: *const f32,
    radius: f32,
    out_color: *mut Color,
) -> f32 {
    let a = sl(rgba, 4);
    let a = [
        a.first().copied().unwrap_or(0.0),
        a.get(1).copied().unwrap_or(0.0),
        a.get(2).copied().unwrap_or(0.0),
        a.get(3).copied().unwrap_or(0.0),
    ];
    let (c, r) = lamp::fade_target(*rd(light), light_radius, a, radius);
    *rw(out_color) = c;
    r
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_lamp_light_connection_color(
    default: *const Color,
    links: *const LampLink,
    n: i32,
    out: *mut Color,
) {
    *rw(out) = lamp::light_connection_color(*rd(default), sl(links, n));
}

// ---- multi slider ----

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_multi_load(v: *const VarsRef, out: *mut MultiParams) {
    multislider::load(vr(v), rw(out));
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_multi_state_new(s: *mut MultiState) {
    // SAFETY: `s` is uninitialised storage for one `MultiState`.
    unsafe { s.write(MultiState::new()) }
}

/// Fills `out[0..n)` (`n` states) and returns how many were written.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_multi_compute_states(
    min: f32,
    max: f32,
    n: i32,
    out: *mut f32,
) -> i32 {
    let states = multislider::compute_states(min, max, n);
    for (i, v) in states.iter().enumerate() {
        // SAFETY: caller contract: `out` holds `n` floats.
        unsafe { out.add(i).write(*v) };
    }
    states.len() as i32
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_multi_can_interact(p: *const MultiParams, mass: f32) -> bool {
    multislider::can_interact(rd(p), mass)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_multi_set_stuck_state(
    s: *mut MultiState,
    states: *const f32,
    n: i32,
    state: i32,
    effects: bool,
    sink: *const Sink,
) {
    with_ops(rd(sink), |ops| {
        multislider::set_stuck_state(pod(s), sl(states, n), state, effects, ops);
    });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_multi_on_interact(
    s: *mut MultiState,
    states: *const f32,
    n: i32,
    sink: *const Sink,
) {
    with_ops(rd(sink), |ops| {
        multislider::on_interact(pod(s), sl(states, n), ops)
    });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_multi_update(
    s: *mut MultiState,
    p: *const MultiParams,
    states: *const f32,
    n: i32,
    dt: f32,
    interacted: bool,
    host: *const Host,
    sink: *const Sink,
) {
    with_ops(rd(sink), |ops| {
        multislider::update(
            pod(s),
            rd(p),
            sl(states, n),
            dt,
            interacted,
            host!(host),
            ops,
        );
    });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_multi_load_instance(
    v: *const VarsRef,
    s: *mut MultiState,
    states: *const f32,
    n: i32,
    sink: *const Sink,
) {
    with_ops(rd(sink), |ops| {
        multislider::load_instance(vr(v), pod(s), sl(states, n), ops);
    });
}

// ---- move object ----

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_move_load(v: *const VarsRef, out: *mut MoveParams) {
    moveobject::load(vr(v), rw(out));
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_move_load_instance(v: *const VarsRef) {
    moveobject::load_instance(vr(v));
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_move_setup(
    s: *mut MoveObjState,
    p: *const MoveParams,
    body: *const Matrixf,
    local_min: *const Vector3f,
    local_max: *const Vector3f,
) {
    moveobject::setup(rw(s), rd(p), rd(body), *rd(local_min), *rd(local_max));
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_move_set_angular_offset(
    s: *mut MoveObjState,
    p: *const MoveParams,
    pos: *const Vector3f,
) {
    moveobject::set_angular_offset(rw(s), rd(p), *rd(pos));
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_move_get_state(
    s: *const MoveObjState,
    p: *const MoveParams,
    body: *const Matrixf,
) -> f32 {
    moveobject::get_move_state(rd(s), rd(p), rd(body))
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_move_to_state(
    s: *const MoveObjState,
    p: *const MoveParams,
    base: *mut BaseState,
    state: f32,
    acc: f32,
    max_speed: f32,
    slowdown: f32,
    reset: bool,
    sink: *const Sink,
) {
    with_ops(rd(sink), |ops| {
        moveobject::move_to_state(
            rd(s),
            rd(p),
            rw(base),
            state,
            acc,
            max_speed,
            slowdown,
            reset,
            ops,
        );
    });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_move_to_state_auto(
    s: *const MoveObjState,
    p: *const MoveParams,
    base: *mut BaseState,
    body: *const Matrixf,
    state: f32,
    sink: *const Sink,
) {
    with_ops(rd(sink), |ops| {
        moveobject::move_to_state_auto(rd(s), rd(p), rw(base), rd(body), state, ops);
    });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_move_update_auto_move(
    s: *mut MoveObjState,
    p: *const MoveParams,
    base: *mut BaseState,
    body: *const Matrixf,
    sink: *const Sink,
) {
    with_ops(rd(sink), |ops| {
        moveobject::update_auto_move(rw(s), rd(p), rw(base), rd(body), ops);
    });
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga4_move_on_connection_state(
    s: *const MoveObjState,
    p: *const MoveParams,
    base: *mut BaseState,
    body: *const Matrixf,
    state: i32,
    sink: *const Sink,
) {
    with_ops(rd(sink), |ops| {
        moveobject::on_connection_state(rd(s), rd(p), rw(base), rd(body), state, ops);
    });
}
