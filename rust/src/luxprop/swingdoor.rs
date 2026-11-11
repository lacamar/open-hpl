//! `cLuxProp_SwingDoor`: open/close/locked/break state, timed opening and damage levels.

use super::base::{set_translation, translation};
use super::object::{MoveData, load_rotate};
use super::ops::{Op, Ops, Probe, op};
use super::vars::{Vars, put_var};
use crate::math::{to_rad, v3};
use crate::types::{Matrixf, Quaternion};

unsafe extern "C" {
    fn ohpl_math_matrix_mul(a: *const Matrixf, b: *const Matrixf, out: *mut Matrixf);
    safe fn ohpl_math_matrix_quaternion(q: &Quaternion, out: &mut Matrixf);
    safe fn ohpl_quaternion_from_angle_axis(
        q: &mut Quaternion,
        angle: f32,
        axis: &crate::types::Vector3f,
    );
}

type MatrixMul = unsafe extern "C" fn(*const Matrixf, *const Matrixf, *mut Matrixf);

/// Called through an opaque pointer, out of line as in C++: inlined, LLVM commutes operands and
/// folds a negation into `cos`/`sin`, which changes the sign of a NaN result.
fn matrix_mul(a: &Matrixf, b: &Matrixf) -> Matrixf {
    let mut out = Matrixf::default();
    let f: MatrixMul = std::hint::black_box(ohpl_math_matrix_mul);
    // SAFETY: pointers to live locals.
    unsafe { f(a, b, &mut out) };
    out
}

pub const SLOT_CLOSE_ON: i32 = 30;
pub const SLOT_CLOSE_OFF: i32 = 31;
pub const SLOT_LOCK_ON: i32 = 32;
pub const SLOT_LOCK_OFF: i32 = 33;
pub const SLOT_INTERACT_LOCKED: i32 = 34;
pub const SLOT_DAMAGE_MESH1: i32 = 35;
pub const SLOT_DAMAGE_MESH2: i32 = 36;
pub const SLOT_BROKEN_ENTITY: i32 = 37;
pub const SLOT_DAMAGE_SOUND: i32 = 38;
pub const SLOT_DAMAGE_PS: i32 = 39;
pub const SLOT_BREAK_SOUND: i32 = 40;
pub const SLOT_BREAK_PS: i32 = 41;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct DoorParams {
    pub data: MoveData,
    pub can_interact_static: bool,
    pub breakable: bool,
    pub break_impulse: f32,
    pub health_damage: [f32; 2],
}

pub fn load(v: &dyn Vars, out: &mut DoorParams) {
    load_rotate(v, &mut out.data);
    out.can_interact_static = v.boolean("CanInteractWithStaticBody", false);
    put_var(v, SLOT_CLOSE_ON, "CloseOnSound");
    put_var(v, SLOT_CLOSE_OFF, "CloseOffSound");
    put_var(v, SLOT_LOCK_ON, "LockOnSound");
    put_var(v, SLOT_LOCK_OFF, "LockOffSound");
    put_var(v, SLOT_INTERACT_LOCKED, "InteractLockedSound");
    out.breakable = v.boolean("Breakable", false);
    put_var(v, SLOT_DAMAGE_MESH1, "DamageMesh1");
    put_var(v, SLOT_DAMAGE_MESH2, "DamageMesh2");
    put_var(v, SLOT_BROKEN_ENTITY, "BrokenEntity");
    put_var(v, SLOT_DAMAGE_SOUND, "DamageSound");
    put_var(v, SLOT_DAMAGE_PS, "DamagePS");
    put_var(v, SLOT_BREAK_SOUND, "BreakSound");
    put_var(v, SLOT_BREAK_PS, "BreakPS");
    out.break_impulse = v.float("BreakImpulse", 0.0);
    out.health_damage[0] = v.float("HealthDamage1", 0.0);
    out.health_damage[1] = v.float("HealthDamage2", 0.0);
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct DoorState {
    pub locked: bool,
    pub closed: bool,
    pub broken_entity_id: i32,
    pub disable_auto_close: bool,
    pub current_mesh_entity: i32,
    pub broken: bool,
    pub interact_sound_count: f32,
    pub goal_open_amount: f32,
    pub opening_towards_max: bool,
    pub current_open_amount: f32,
    pub open_speed: f32,
    pub is_updating_open_amount: bool,
    pub disable_breakable: bool,
}

impl DoorState {
    pub fn new() -> Self {
        Self {
            broken_entity_id: -1,
            opening_towards_max: true,
            ..Self::default()
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct DoorJoint {
    pub max_angle: f32,
    pub min_angle: f32,
}

pub struct Env<'a> {
    pub joints: &'a [DoorJoint],
    /// `mvJoints.size()`, hinge or not.
    pub n_joints: i32,
}

pub fn can_interact(p: &DoorParams, mass: f32, in_sticky_area: bool) -> bool {
    !(mass == 0.0 && !p.can_interact_static && !in_sticky_area)
}

pub fn set_closed(s: &mut DoorState, e: &Env, closed: bool, effects: bool, ops: &mut Ops) {
    if s.closed == closed {
        return;
    }
    s.closed = closed;
    for (i, j) in e.joints.iter().enumerate() {
        if closed {
            ops.push(Op::new(op::HINGE_MAX).i(i as i32).f(to_rad(2.0)));
            ops.push(Op::new(op::HINGE_STICKY).i(i as i32).j(0));
        } else {
            ops.push(Op::new(op::HINGE_MAX).i(i as i32).f(j.max_angle));
            ops.push(Op::new(op::HINGE_STICKY).i(i as i32).j(1));
        }
    }
    ops.push(Op::new(op::BODIES_WAKE));
    if effects {
        let snd = if closed {
            op::SND_CLOSE_ON
        } else {
            op::SND_CLOSE_OFF
        };
        ops.push(Op::new(op::PLAY_SOUND).i(snd));
    }
}

/// -1 closed, 0 between, 1 open.
pub fn door_state(e: &Env, probe: &mut dyn Probe) -> i32 {
    let mut state = 0;
    for (i, j) in e.joints.iter().enumerate() {
        let abs_angle = probe.hinge(i as i32).angle.abs();
        let closed_angle = to_rad(5.0);
        let open_angle = j.max_angle.abs() * 0.70f32;
        if abs_angle < closed_angle {
            if state == 1 {
                return 0;
            }
            state = -1;
        } else if abs_angle > open_angle {
            if state == -1 {
                return 0;
            }
            state = 1;
        } else {
            return 0;
        }
    }
    state
}

pub fn set_locked(s: &mut DoorState, e: &Env, locked: bool, effects: bool, ops: &mut Ops) {
    if s.locked == locked {
        return;
    }
    s.locked = locked;
    if locked {
        set_closed(s, e, true, false, ops);
    }
    if effects {
        let snd = if locked {
            op::SND_LOCK_ON
        } else {
            op::SND_LOCK_OFF
        };
        ops.push(Op::new(op::PLAY_SOUND).i(snd));
    }
}

pub fn set_damage_level(s: &mut DoorState, level: i32, ops: &mut Ops) {
    if s.current_mesh_entity == level {
        return;
    }
    s.current_mesh_entity = level;
    ops.push(Op::new(op::DOOR_DAMAGE_LEVEL).i(level));
}

pub fn on_connection_state(s: &mut DoorState, e: &Env, state: i32, ops: &mut Ops) {
    set_locked(s, e, state > 0, true, ops);
}

/// `OnInteract` after the still-opening check and the body pick; returns whether to show the hint.
pub fn on_interact(s: &mut DoorState, e: &Env, show_hints: bool, ops: &mut Ops) -> bool {
    if s.locked && s.interact_sound_count <= 0.0 {
        ops.push(Op::new(op::PLAY_SOUND).i(op::SND_INTERACT_LOCKED));
        s.interact_sound_count = 0.5;
    }
    if !s.locked {
        set_closed(s, e, false, true, ops);
    }
    s.disable_auto_close = false;
    !s.locked && show_hints
}

fn rotate_about_pivot(h: &super::ops::Hinge, wanted: f32) -> Matrixf {
    let mut q = Quaternion::default();
    let from_angle_axis: extern "C" fn(&mut Quaternion, f32, &crate::types::Vector3f) =
        std::hint::black_box(ohpl_quaternion_from_angle_axis);
    from_angle_axis(&mut q, -std::hint::black_box(wanted), &h.pin_dir);
    let mut rot = Matrixf::default();
    let to_matrix: extern "C" fn(&Quaternion, &mut Matrixf) =
        std::hint::black_box(ohpl_math_matrix_quaternion);
    to_matrix(&q, &mut rot);
    let body_pos = translation(&h.child);
    let off = v3(
        h.pivot.x - body_pos.x,
        h.pivot.y - body_pos.y,
        h.pivot.z - body_pos.z,
    );
    let mut m = h.child;
    set_translation(&mut m, v3(-off.x, -off.y, -off.z));
    m = matrix_mul(&rot, &m);
    let t = translation(&m);
    set_translation(
        &mut m,
        v3(
            t.x + off.x + body_pos.x,
            t.y + off.y + body_pos.y,
            t.z + off.z + body_pos.z,
        ),
    );
    m
}

pub fn setup_physics(
    s: &mut DoorState,
    e: &Env,
    open: f32,
    towards_max: bool,
    apply_always: bool,
    probe: &mut dyn Probe,
    ops: &mut Ops,
) {
    if apply_always || open > 0.0 {
        for (i, j) in e.joints.iter().enumerate() {
            let h = probe.hinge(i as i32);
            let wanted = (if towards_max {
                j.max_angle
            } else {
                j.min_angle
            }) * open;
            ops.push(
                Op::new(op::DOOR_BODY_MATRIX)
                    .i(i as i32)
                    .m(rotate_about_pivot(&h, wanted)),
            );
        }
    }
    if e.n_joints == 1 {
        for i in 0..e.joints.len() {
            if probe.hinge(i as i32).angle.abs() < to_rad(10.0) {
                set_closed(s, e, true, false, ops);
            }
        }
    }
}

pub fn set_open_amount(
    s: &mut DoorState,
    e: &Env,
    open: f32,
    duration: f32,
    towards_max: bool,
    probe: &mut dyn Probe,
    ops: &mut Ops,
) {
    if e.joints.is_empty() {
        return;
    }
    ops.push(Op::new(op::BODY_ZERO_VEL).i(0));
    let h = probe.hinge(0);
    if (towards_max && h.max_angle == 0.0) || (!towards_max && h.min_angle == 0.0) {
        return;
    }
    s.goal_open_amount = open;
    s.opening_towards_max = towards_max;
    s.current_open_amount = if towards_max {
        h.angle / h.max_angle
    } else {
        h.angle / h.min_angle
    };
    if s.goal_open_amount == s.current_open_amount {
        return;
    }
    if duration <= 0.0 {
        setup_physics(s, e, open, towards_max, true, probe, ops);
        return;
    }
    s.open_speed = (s.goal_open_amount - s.current_open_amount) / duration;
    s.is_updating_open_amount = true;
}

fn update_opening(s: &mut DoorState, e: &Env, dt: f32, probe: &mut dyn Probe, ops: &mut Ops) {
    if !s.is_updating_open_amount {
        return;
    }
    let new_open = s.current_open_amount + dt * s.open_speed;
    if (s.open_speed > 0.0 && new_open >= s.goal_open_amount)
        || (s.open_speed < 0.0 && new_open <= s.goal_open_amount)
    {
        setup_physics(
            s,
            e,
            s.goal_open_amount - s.current_open_amount,
            s.opening_towards_max,
            true,
            probe,
            ops,
        );
        s.is_updating_open_amount = false;
        return;
    }
    setup_physics(
        s,
        e,
        dt * s.open_speed,
        s.opening_towards_max,
        true,
        probe,
        ops,
    );
    let h = probe.hinge(0);
    s.current_open_amount = if s.opening_towards_max {
        h.angle / h.max_angle
    } else {
        h.angle / h.min_angle
    };
}

pub fn update(
    s: &mut DoorState,
    e: &Env,
    dt: f32,
    interacted: bool,
    probe: &mut dyn Probe,
    ops: &mut Ops,
) {
    update_opening(s, e, dt, probe, ops);
    if s.interact_sound_count > 0.0 {
        s.interact_sound_count -= dt;
    }
    if !s.closed && !interacted && !s.disable_auto_close && e.n_joints == 1 {
        for i in 0..e.joints.len() {
            if probe.hinge(i as i32).angle.abs() < to_rad(10.0) {
                set_closed(s, e, true, true, ops);
            }
        }
    }
}

/// `OnHealthChange`. `has_main` is whether any body has mass.
pub fn on_health_change(
    s: &mut DoorState,
    p: &DoorParams,
    health: f32,
    has_main: bool,
    ops: &mut Ops,
) {
    if !p.breakable || s.disable_breakable || s.broken {
        return;
    }
    let mut effects = 0;
    if health <= 0.0 && !s.broken {
        s.broken = true;
        set_damage_level(s, 0, ops);
        ops.push(Op::new(op::DOOR_DISABLE_BODIES));
        ops.event(Op::new(op::DOOR_BROKEN_ENTITY), s);
        ops.event(Op::new(op::RUN_BREAK_CALLBACK), s);
        effects = 2;
    } else if health < p.health_damage[1] && s.current_mesh_entity <= 1 {
        set_damage_level(s, 2, ops);
        effects = 1;
    } else if health < p.health_damage[0] && s.current_mesh_entity == 0 {
        set_damage_level(s, 1, ops);
        effects = 1;
    }
    if has_main && effects != 0 {
        ops.push(Op::new(op::DOOR_EFFECTS).i(effects));
    }
}

pub fn reset(s: &mut DoorState, e: &Env, probe: &mut dyn Probe, ops: &mut Ops) {
    if s.broken {
        s.broken = false;
        if s.broken_entity_id >= 0 {
            let id = s.broken_entity_id;
            ops.event(Op::new(op::DOOR_DESTROY_BROKEN).i(id), s);
        }
    }
    setup_physics(s, e, 0.0, true, false, probe, ops);
    set_damage_level(s, 0, ops);
}

pub fn load_instance(
    v: &dyn Vars,
    s: &mut DoorState,
    e: &Env,
    probe: &mut dyn Probe,
    ops: &mut Ops,
) {
    s.disable_breakable = v.boolean("DisableBreakable", false);
    let locked = v.boolean("Locked", false);
    set_locked(s, e, locked, false, ops);
    let open = v.float("OpenAmount", 0.0);
    setup_physics(s, e, open, true, false, probe, ops);
}
