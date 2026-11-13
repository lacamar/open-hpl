//! `iPhysicsBody` and `iPhysicsJoint` object shells (`OHPL_IMPL=rust`): scalar state in repr(C)
//! structs embedded in the C++ objects (their members are references into them), object lists,
//! construction defaults and the destroy sequence here. Other objects are reached through one host op.
//! No registry lock is held across an op: ops re-enter C++ and then this module.

use super::host::{H, Str, Vt};
use super::joint::{self, JointState as MoveState, MoveParams};
use super::wreg::{Wa, WorldOp};
use crate::math::{IDENTITY, matrix_inverse, matrix_mul_3x3, matrix_mul_vec};
use crate::obj::{Arena, Handle};
use crate::types::{Matrixf, Vector3f};
use std::collections::BTreeMap;
use std::sync::{LazyLock, Mutex, MutexGuard};

pub const BO_SETUP: i32 = 301;
pub const BO_SHAPE_DESTROY: i32 = 302;
pub const BO_CHAR_DISCONNECT: i32 = 303;
pub const BO_VERLET_REMOVE: i32 = 304;
pub const BO_JOINT_REMOVE_BODY: i32 = 305;
pub const BO_JOINT_ORPHAN: i32 = 306;
pub const BO_WORLD_DESTROY_JOINT: i32 = 307;
pub const BO_SOUND_DESTROY: i32 = 308;
pub const BO_DELETE_LOW_LEVEL: i32 = 309;
pub const BO_CB_AABB: i32 = 310;
pub const BO_CB_COLLIDE: i32 = 311;

pub const JO_BODY_ADD_JOINT: i32 = 401;
pub const JO_BODY_LOCAL_MATRIX: i32 = 402;
pub const JO_BODY_REMOVE_JOINT: i32 = 403;
pub const JO_CTRL_NAME: i32 = 404;
pub const JO_CTRL_SET_BODY: i32 = 405;
pub const JO_CTRL_SET_JOINT: i32 = 406;
pub const JO_CTRL_SET_ACTIVE: i32 = 407;
pub const JO_CTRL_SET_PAUSED: i32 = 408;
pub const JO_WORLD_DESTROY_CTRL: i32 = 409;
pub const JO_SOUND_DESTROY: i32 = 410;
pub const JO_RESET_POSITION: i32 = 411;
pub const JO_MOVE_SOUND: i32 = 412;
pub const JO_BREAK_SOUND: i32 = 413;
pub const JO_DELETE_CALLBACK: i32 = 414;

/// Scalar and pointer state of an `iPhysicsBody`; layout mirrored by `OhplBodyState` in `PhysicsBody.h`.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct BodyState {
    pub shell: u64,
    pub world: H,
    pub shape: H,
    pub material: H,
    pub char_body: H,
    pub scrape_body: H,
    pub scrape_sound: H,
    pub roll_sound: H,
    pub user_data: H,
    pub prev_scrape: Matrixf,
    pub scrape_sound_id: i32,
    pub roll_sound_id: i32,
    pub impact_count: i32,
    pub collide_flags: u32,
    pub buoyancy_density_mul: f32,
    pub mass_mul_applied: f32,
    pub push_strength: i32,
    pub freeze_count: i32,
    pub gravity_axes: i32,
    pub in_update_list: bool,
    pub pushed_by_char_gravity: bool,
    pub blocks_sound: bool,
    pub blocks_light: bool,
    pub is_character: bool,
    pub collide_character: bool,
    pub is_rag_doll: bool,
    pub collide_rag_doll: bool,
    pub is_volatile: bool,
    pub use_surface_effects: bool,
    pub gravity_can_attach: bool,
    pub gravity_attach_rotation: bool,
    pub gravity_attach_velocity: bool,
    pub collide: bool,
    pub destroying: bool,
}

const AXES_XYZ: i32 = 2;

impl BodyState {
    fn new(shell: u64, world: H, shape: H) -> Self {
        Self {
            shell,
            world,
            shape,
            material: 0,
            char_body: 0,
            scrape_body: 0,
            scrape_sound: 0,
            roll_sound: 0,
            user_data: 0,
            prev_scrape: IDENTITY,
            scrape_sound_id: -1,
            roll_sound_id: -1,
            impact_count: 0,
            collide_flags: u32::MAX,
            buoyancy_density_mul: 1.0,
            mass_mul_applied: 1.0,
            push_strength: 0,
            freeze_count: 0,
            gravity_axes: AXES_XYZ,
            in_update_list: false,
            pushed_by_char_gravity: true,
            blocks_sound: false,
            blocks_light: true,
            is_character: false,
            collide_character: true,
            is_rag_doll: false,
            collide_rag_doll: true,
            is_volatile: false,
            use_surface_effects: true,
            gravity_can_attach: true,
            gravity_attach_rotation: true,
            gravity_attach_velocity: true,
            collide: true,
            destroying: false,
        }
    }
}

/// Scalar and pointer state of an `iPhysicsJoint`; layout mirrored by `OhplJointState` in `PhysicsJoint.h`.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct JointStateC {
    pub shell: u64,
    pub parent: H,
    pub child: H,
    pub world: H,
    pub sound: H,
    pub callback: H,
    pub user_data: H,
    pub parent_setup: Matrixf,
    pub child_setup: Matrixf,
    pub prev_child: Matrixf,
    pub prev_parent: Matrixf,
    pub pin_dir: Vector3f,
    pub pivot: Vector3f,
    pub start_pivot: Vector3f,
    pub local_pivot: Vector3f,
    pub local_pin_dir: Vector3f,
    pub start_pin_dir: Vector3f,
    pub min_move_speed: f32,
    pub min_move_freq: f32,
    pub min_move_freq_speed: f32,
    pub min_move_volume: f32,
    pub max_move_freq: f32,
    pub max_move_freq_speed: f32,
    pub max_move_volume: f32,
    pub middle_move_speed: f32,
    pub middle_move_volume: f32,
    pub break_force: f32,
    pub limit_auto_sleep_dist: f32,
    pub unique_id: i32,
    pub sound_id: i32,
    pub limit_auto_sleep_num_steps: i32,
    pub speed_count: i32,
    pub limit_step_count: i32,
    pub move_speed_type: i32,
    pub sticky_min: bool,
    pub sticky_max: bool,
    pub allow_position_reset: bool,
    pub position_reset_pending: bool,
    pub breakable: bool,
    pub broken: bool,
    pub limit_auto_sleep: bool,
    pub has_collided: bool,
    pub auto_delete_callback: bool,
}

#[derive(Default)]
struct BodyShell {
    op: Option<WorldOp>,
    state: usize,
    joints: Vec<usize>,
    chars: Vec<usize>,
    callbacks: Vec<usize>,
    verlets: Vec<usize>,
}

#[derive(Default)]
struct JointShell {
    op: Option<WorldOp>,
    state: usize,
    vt: usize,
    controllers: BTreeMap<Vec<u8>, usize>,
}

#[derive(Default)]
struct Shells {
    bodies: Arena<BodyShell>,
    joints: Arena<JointShell>,
}

static SHELLS: LazyLock<Mutex<Shells>> = LazyLock::new(|| Mutex::new(Shells::default()));

fn shells() -> Option<MutexGuard<'static, Shells>> {
    SHELLS.lock().ok()
}

fn body<R>(bits: u64, f: impl FnOnce(&mut BodyShell) -> R) -> Option<R> {
    let h = Handle::from_bits(bits)?;
    shells()?.bodies.get_mut(h).map(f)
}

fn jnt<R>(bits: u64, f: impl FnOnce(&mut JointShell) -> R) -> Option<R> {
    let h = Handle::from_bits(bits)?;
    shells()?.joints.get_mut(h).map(f)
}

fn call(op: Option<WorldOp>, code: i32, p: [usize; 6], f0: f32, i0: i32) -> Wa {
    let mut a = Wa {
        p,
        f: [f0, 0.0],
        i: [i0, 0],
        s: std::ptr::null(),
        n: 0,
        rf: 0.0,
        rv: [0.0; 3],
    };
    if let Some(op) = op {
        // SAFETY: the op was registered with the shell and outlives it; `a` is a live block.
        a.p[5] = unsafe { op(code, &mut a) };
    }
    a
}

/// # Safety
/// `state` is the live state embedded in the owning object.
unsafe fn bstate<'a>(sh: &BodyShell) -> Option<&'a mut BodyState> {
    // SAFETY: caller contract.
    unsafe { (sh.state as *mut BodyState).as_mut() }
}

// ---------------------------------------------------------------- body

/// Fills the defaults, registers the shell, then lets the host run the shape/material setup.
///
/// # Safety
/// `state` is writable and embedded in the body `me`.
pub unsafe fn body_new(op: WorldOp, me: usize, state: *mut BodyState, world: H, shape: H) -> u64 {
    let bits = shells()
        .and_then(|mut s| {
            s.bodies.insert(BodyShell {
                op: Some(op),
                state: state as usize,
                ..BodyShell::default()
            })
        })
        .map_or(0, Handle::to_bits);
    // SAFETY: caller contract.
    if let Some(s) = unsafe { state.as_mut() } {
        *s = BodyState::new(bits, world, shape);
    }
    call(Some(op), BO_SETUP, [me, 0, 0, 0, 0, 0], 0.0, 0);
    bits
}

pub fn body_free(bits: u64) {
    if let (Some(h), Some(mut s)) = (Handle::from_bits(bits), shells()) {
        s.bodies.remove(h);
    }
}

pub fn body_add_joint(bits: u64, j: usize) {
    body(bits, |b| b.joints.push(j));
}

pub fn body_joint_at(bits: u64, i: i32) -> usize {
    body(bits, |b| {
        usize::try_from(i)
            .ok()
            .and_then(|i| b.joints.get(i).copied())
    })
    .flatten()
    .unwrap_or(0)
}

pub fn body_joint_num(bits: u64) -> i32 {
    body(bits, |b| i32::try_from(b.joints.len()).unwrap_or(0)).unwrap_or(0)
}

pub fn body_remove_joint(bits: u64, j: usize) {
    body(bits, |b| {
        if let Some(p) = b.joints.iter().position(|&x| x == j) {
            b.joints.remove(p);
        }
    });
}

pub fn body_add_callback(bits: u64, cb: usize) {
    body(bits, |b| b.callbacks.push(cb));
}

pub fn body_remove_callback(bits: u64, cb: usize) {
    body(bits, |b| {
        if let Some(p) = b.callbacks.iter().position(|&x| x == cb) {
            b.callbacks.remove(p);
        }
    });
}

/// The next callback after `cur` in list order (the first for `None`), like a `std::list` iterator.
fn next_cb(bits: u64, cur: Option<usize>) -> Option<(usize, Option<WorldOp>)> {
    body(bits, |b| {
        let i = cur.map_or(0, |c| {
            b.callbacks
                .iter()
                .position(|&x| x == c)
                .map_or(0, |p| p + 1)
        });
        b.callbacks.get(i).map(|&c| (c, b.op))
    })
    .flatten()
}

/// `OnAABBCollision`: every callback runs, any false vetoes.
pub fn body_on_aabb(bits: u64, me: usize, other: usize) -> bool {
    let mut ok = true;
    let mut cur = None;
    while let Some((cb, op)) = next_cb(bits, cur) {
        ok &= call(op, BO_CB_AABB, [cb, me, other, 0, 0, 0], 0.0, 0).p[5] != 0;
        cur = Some(cb);
    }
    ok
}

/// `OnCollide` callbacks.
pub fn body_on_collide(bits: u64, me: usize, other: usize, contact: usize) {
    let mut cur = None;
    while let Some((cb, op)) = next_cb(bits, cur) {
        call(op, BO_CB_COLLIDE, [cb, me, other, contact, 0, 0], 0.0, 0);
        cur = Some(cb);
    }
}

pub fn body_has_callbacks(bits: u64) -> bool {
    body(bits, |b| !b.callbacks.is_empty()).unwrap_or(false)
}

pub fn body_add_char(bits: u64, c: usize) {
    body(bits, |b| {
        // SAFETY: the state lives as long as the shell.
        if !unsafe { bstate(b) }.is_some_and(|s| s.destroying) {
            b.chars.push(c);
        }
    });
}

pub fn body_remove_char(bits: u64, c: usize) {
    body(bits, |b| {
        // SAFETY: the state lives as long as the shell.
        if !unsafe { bstate(b) }.is_some_and(|s| s.destroying)
            && let Some(p) = b.chars.iter().position(|&x| x == c)
        {
            b.chars.remove(p);
        }
    });
}

pub fn body_add_verlet(bits: u64, c: usize) {
    body(bits, |b| b.verlets.push(c));
}

pub fn body_remove_verlet(bits: u64, c: usize) {
    body(bits, |b| b.verlets.retain(|&x| x != c));
}

/// `iPhysicsBody::Destroy`.
pub fn body_destroy(bits: u64, me: usize) {
    let Some((op, state)) = body(bits, |b| (b.op, b.state)) else {
        return;
    };
    // SAFETY: the state lives as long as the shell.
    let (world, shape) = match unsafe { (state as *mut BodyState).as_mut() } {
        Some(s) => {
            s.destroying = true;
            (s.world, s.shape)
        }
        None => (0, 0),
    };
    call(op, BO_SHAPE_DESTROY, [me, world, shape, 0, 0, 0], 0.0, 0);

    let chars = body(bits, |b| b.chars.clone()).unwrap_or_default();
    for c in chars {
        call(op, BO_CHAR_DISCONNECT, [c, 0, 0, 0, 0, 0], 0.0, 0);
    }

    let verlets = body(bits, |b| std::mem::take(&mut b.verlets)).unwrap_or_default();
    for v in verlets {
        call(op, BO_VERLET_REMOVE, [v, me, 0, 0, 0, 0], 0.0, 0);
    }

    let mut i = 0;
    while let Some(j) = body(bits, |b| b.joints.get(i).copied()).flatten() {
        call(op, BO_JOINT_REMOVE_BODY, [j, me, 0, 0, 0, 0], 0.0, 0);
        if call(op, BO_JOINT_ORPHAN, [j, 0, 0, 0, 0, 0], 0.0, 0).p[5] != 0 {
            call(op, BO_WORLD_DESTROY_JOINT, [world, j, 0, 0, 0, 0], 0.0, 0);
        }
        i += 1;
    }

    call(op, BO_SOUND_DESTROY, [me, 0, 0, 0, 0, 0], 0.0, 0);
    call(op, BO_SOUND_DESTROY, [me, 1, 0, 0, 0, 0], 0.0, 0);
    call(op, BO_DELETE_LOW_LEVEL, [me, 0, 0, 0, 0, 0], 0.0, 0);
}

// ---------------------------------------------------------------- joint

fn matrix_of(op: Option<WorldOp>, body: H) -> Matrixf {
    let mut m = IDENTITY;
    call(
        op,
        JO_BODY_LOCAL_MATRIX,
        [body, &mut m as *mut Matrixf as usize, 0, 0, 0, 0],
        0.0,
        0,
    );
    m
}

/// `iPhysicsJoint` constructor: bodies learn about the joint, setup matrices and local pivot are derived.
///
/// # Safety
/// `state` is writable and embedded in the joint `me`.
#[allow(clippy::too_many_arguments)]
pub unsafe fn joint_new(
    op: WorldOp,
    me: usize,
    state: *mut JointStateC,
    vt: usize,
    parent: H,
    child: H,
    world: H,
    pivot: Vector3f,
    pin: Vector3f,
) -> u64 {
    let bits = shells()
        .and_then(|mut s| {
            s.joints.insert(JointShell {
                op: Some(op),
                state: state as usize,
                vt,
                ..JointShell::default()
            })
        })
        .map_or(0, Handle::to_bits);
    let o = Some(op);
    let mut parent_setup = IDENTITY;
    if parent != 0 {
        call(o, JO_BODY_ADD_JOINT, [parent, me, 0, 0, 0, 0], 0.0, 0);
        parent_setup = matrix_of(o, parent);
    }
    call(o, JO_BODY_ADD_JOINT, [child, me, 0, 0, 0, 0], 0.0, 0);
    let child_setup = matrix_of(o, child);
    let (local_pivot, local_pin) = if parent != 0 {
        let inv = matrix_inverse(&matrix_of(o, parent));
        (matrix_mul_vec(&inv, pivot), matrix_mul_3x3(&inv, pin))
    } else {
        (pivot, pin)
    };
    let s = JointStateC {
        shell: bits,
        parent,
        child,
        world,
        sound: 0,
        callback: 0,
        user_data: 0,
        parent_setup,
        child_setup,
        prev_child: IDENTITY,
        prev_parent: IDENTITY,
        pin_dir: pin,
        pivot,
        start_pivot: pivot,
        local_pivot,
        local_pin_dir: local_pin,
        start_pin_dir: pin,
        min_move_speed: 0.0,
        min_move_freq: 0.0,
        min_move_freq_speed: 0.0,
        min_move_volume: 0.0,
        max_move_freq: 0.0,
        max_move_freq_speed: 0.0,
        max_move_volume: 0.0,
        middle_move_speed: 0.0,
        middle_move_volume: 0.0,
        break_force: 0.0,
        limit_auto_sleep_dist: 0.02,
        unique_id: -1,
        sound_id: -1,
        limit_auto_sleep_num_steps: 10,
        speed_count: 0,
        limit_step_count: 0,
        move_speed_type: 0,
        sticky_min: false,
        sticky_max: false,
        allow_position_reset: false,
        position_reset_pending: false,
        breakable: false,
        broken: false,
        limit_auto_sleep: false,
        has_collided: false,
        auto_delete_callback: false,
    };
    // SAFETY: caller contract.
    if let Some(d) = unsafe { state.as_mut() } {
        *d = s;
    }
    bits
}

/// Destructor body after the callback is released: controllers, body links, sound.
pub fn joint_drop(bits: u64, me: usize) {
    let Some((op, state, ctrls)) = jnt(bits, |j| {
        (
            j.op,
            j.state,
            j.controllers.values().copied().collect::<Vec<_>>(),
        )
    }) else {
        return;
    };
    // SAFETY: the state lives as long as the shell.
    let Some(s) = (unsafe { (state as *mut JointStateC).as_ref() }).copied() else {
        return;
    };
    if s.auto_delete_callback && s.callback != 0 {
        call(op, JO_DELETE_CALLBACK, [s.callback, 0, 0, 0, 0, 0], 0.0, 0);
    }
    for c in ctrls {
        call(op, JO_WORLD_DESTROY_CTRL, [s.world, c, 0, 0, 0, 0], 0.0, 0);
    }
    if s.child != 0 {
        call(op, JO_BODY_REMOVE_JOINT, [s.child, me, 0, 0, 0, 0], 0.0, 0);
    }
    if s.parent != 0 {
        call(op, JO_BODY_REMOVE_JOINT, [s.parent, me, 0, 0, 0, 0], 0.0, 0);
    }
    if s.sound != 0 {
        call(op, JO_SOUND_DESTROY, [s.world, s.sound, 0, 0, 0, 0], 0.0, 0);
    }
    if let (Some(h), Some(mut sh)) = (Handle::from_bits(bits), shells()) {
        sh.joints.remove(h);
    }
}

pub fn joint_add_controller(bits: u64, me: usize, ctrl: usize) {
    let Some((op, state)) = jnt(bits, |j| (j.op, j.state)) else {
        return;
    };
    let name = call(op, JO_CTRL_NAME, [ctrl, 0, 0, 0, 0, 0], 0.0, 0);
    // SAFETY: the host returns the controller's name, alive as long as the controller.
    let key = unsafe {
        Str {
            p: name.s,
            n: name.n,
        }
        .bytes()
    }
    .to_vec();
    jnt(bits, |j| {
        j.controllers.entry(key).or_insert(ctrl);
    });
    // SAFETY: the state lives as long as the shell.
    let child = unsafe { (state as *const JointStateC).as_ref() }.map_or(0, |s| s.child);
    call(op, JO_CTRL_SET_BODY, [ctrl, child, 0, 0, 0, 0], 0.0, 0);
    call(op, JO_CTRL_SET_JOINT, [ctrl, me, 0, 0, 0, 0], 0.0, 0);
}

pub fn joint_get_controller(bits: u64, name: &[u8]) -> usize {
    jnt(bits, |j| j.controllers.get(name).copied())
        .flatten()
        .unwrap_or(0)
}

pub fn joint_change_controller(bits: u64, name: &[u8]) -> bool {
    let Some((op, all)) = jnt(bits, |j| {
        (j.op, j.controllers.values().copied().collect::<Vec<_>>())
    }) else {
        return false;
    };
    let new = joint_get_controller(bits, name);
    if new == 0 {
        return false;
    }
    for c in all {
        call(
            op,
            JO_CTRL_SET_ACTIVE,
            [c, 0, 0, 0, 0, 0],
            0.0,
            i32::from(c == new),
        );
    }
    true
}

pub fn joint_set_all_paused(bits: u64, paused: bool) {
    let Some((op, all)) = jnt(bits, |j| {
        (j.op, j.controllers.values().copied().collect::<Vec<_>>())
    }) else {
        return;
    };
    for c in all {
        call(
            op,
            JO_CTRL_SET_PAUSED,
            [c, 0, 0, 0, 0, 0],
            0.0,
            i32::from(paused),
        );
    }
}

pub fn joint_controller_len(bits: u64) -> usize {
    jnt(bits, |j| j.controllers.len()).unwrap_or(0)
}

pub fn joint_controller_at(bits: u64, i: usize) -> Option<(*const u8, usize, usize)> {
    jnt(bits, |j| {
        j.controllers
            .iter()
            .nth(i)
            .map(|(k, v)| (k.as_ptr(), k.len(), *v))
    })
    .flatten()
}

fn move_sound(op: Option<WorldOp>, me: usize) -> Str {
    let a = call(op, JO_MOVE_SOUND, [me, 0, 0, 0, 0, 0], 0.0, 0);
    Str { p: a.s, n: a.n }
}

/// `iPhysicsJoint::OnPhysicsUpdate`: the pending reset, then the move-sound step on the embedded state.
///
/// # Safety
/// The registered host table and state are live.
pub unsafe fn joint_update(bits: u64, me: usize) -> bool {
    let Some((op, state, vt)) = jnt(bits, |j| (j.op, j.state, j.vt)) else {
        return false;
    };
    // SAFETY: the state lives as long as the shell; the vt is the static host table.
    let (Some(s), Some(vt)) = (unsafe { (state as *mut JointStateC).as_mut() }, unsafe {
        (vt as *const Vt).as_ref()
    }) else {
        return false;
    };
    if s.position_reset_pending {
        call(op, JO_RESET_POSITION, [me, 0, 0, 0, 0, 0], 0.0, 0);
    }
    s.position_reset_pending = false;
    let params = MoveParams {
        min_speed: s.min_move_speed,
        min_freq: s.min_move_freq,
        min_freq_speed: s.min_move_freq_speed,
        min_volume: s.min_move_volume,
        max_freq: s.max_move_freq,
        max_freq_speed: s.max_move_freq_speed,
        max_volume: s.max_move_volume,
        middle_speed: s.middle_move_speed,
        middle_volume: s.middle_move_volume,
        speed_type: s.move_speed_type,
        move_sound: move_sound(op, me),
    };
    let mut ms = MoveState {
        parent: s.parent,
        child: s.child,
        world: s.world,
        pivot: s.pivot,
        pin_dir: s.pin_dir,
        local_pivot: s.local_pivot,
        local_pin_dir: s.local_pin_dir,
        prev_child: s.prev_child,
        prev_parent: s.prev_parent,
        sound: s.sound,
        sound_id: s.sound_id,
        speed_count: s.speed_count,
    };
    let book = 0;
    let updated = joint::on_physics_update_bk(&vt, &params, &mut ms, book);
    // SAFETY: the host calls above may have re-entered C++ but never reallocate the state.
    let s = unsafe { (state as *mut JointStateC).as_mut() };
    if let Some(s) = s {
        s.pivot = ms.pivot;
        s.pin_dir = ms.pin_dir;
        s.prev_child = ms.prev_child;
        s.prev_parent = ms.prev_parent;
        s.sound = ms.sound;
        s.sound_id = ms.sound_id;
        s.speed_count = ms.speed_count;
    }
    updated
}

/// `iPhysicsJoint::CheckBreakage`.
///
/// # Safety
/// The registered host table and state are live.
pub unsafe fn joint_check_breakage(bits: u64, me: usize) -> bool {
    let Some((op, state, vt)) = jnt(bits, |j| (j.op, j.state, j.vt)) else {
        return false;
    };
    // SAFETY: as in `joint_update`.
    let (Some(s), Some(vt)) = (unsafe { (state as *const JointStateC).as_ref() }, unsafe {
        (vt as *const Vt).as_ref()
    }) else {
        return false;
    };
    let a = call(op, JO_BREAK_SOUND, [me, 0, 0, 0, 0, 0], 0.0, 0);
    joint::check_breakage(
        &vt,
        me,
        s.breakable,
        s.broken,
        s.break_force,
        Str { p: a.s, n: a.n },
    )
}
