//! C ABI of `char_aux.rs`, mirrored in `HPL2/core/include/physics/RustPhysicsH14.h`.

use super::char_aux::{
    Aux, BodyHost, CamIn, CamOut, Conn, EntIn, Grav, body_connection_pos, char_connection_pos,
};
use super::char_aux::{
    C_BODY_CONN, C_CAMERA, C_CHAR_CONN, C_ENTITY, C_GRAVITY, C_SAFE, COUNTERS, bump, counter,
};
use super::character::CharState;
use super::host::H;
use super::rope::{RopePlan, rope_particle_pos, rope_plan};
use crate::types::{Matrixf, Vector3f};

/// Branch counters (see `char_aux::C_*`), for corpus coverage checks.
///
/// # Safety
/// `out` points to `COUNTERS` (16) `u64`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h14_counts(out: *mut u64) {
    for i in 0..COUNTERS {
        // SAFETY: caller contract.
        unsafe { out.add(i).write(counter(i)) };
    }
}

#[repr(C)]
pub struct BodyVt {
    pub mass: unsafe extern "C" fn(H) -> f32,
    pub world_matrix: unsafe extern "C" fn(H, *mut Matrixf),
    pub local_matrix: unsafe extern "C" fn(H, *mut Matrixf),
    pub world_position: unsafe extern "C" fn(H) -> Vector3f,
    pub inertia: unsafe extern "C" fn(H, *mut Matrixf),
    pub torque_from_force: unsafe extern "C" fn(H, Vector3f, Vector3f) -> Vector3f,
    pub velocity_at_position: unsafe extern "C" fn(H, Vector3f) -> Vector3f,
    pub add_force: unsafe extern "C" fn(H, Vector3f),
    pub add_torque: unsafe extern "C" fn(H, Vector3f),
    pub gravity_attachment_rotation: unsafe extern "C" fn(H) -> bool,
    pub gravity_attachment_velocity: unsafe extern "C" fn(H) -> bool,
    pub gravity_attachment_axes: unsafe extern "C" fn(H) -> i32,
}

fn matrix(f: unsafe extern "C" fn(H, *mut Matrixf), b: H) -> Matrixf {
    let mut m = Matrixf::default();
    // SAFETY: `m` is a valid out-pointer; `f` is a live C++ forwarder.
    unsafe { f(b, &mut m) };
    m
}

// SAFETY (every forwarder): the C++ side fills `BodyVt` with live function pointers.
impl BodyHost for &BodyVt {
    fn mass(&self, b: H) -> f32 {
        unsafe { (self.mass)(b) }
    }
    fn world_matrix(&self, b: H) -> Matrixf {
        matrix(self.world_matrix, b)
    }
    fn local_matrix(&self, b: H) -> Matrixf {
        matrix(self.local_matrix, b)
    }
    fn world_position(&self, b: H) -> Vector3f {
        unsafe { (self.world_position)(b) }
    }
    fn inertia(&self, b: H) -> Matrixf {
        matrix(self.inertia, b)
    }
    fn torque_from_force(&self, b: H, force: Vector3f, pos: Vector3f) -> Vector3f {
        unsafe { (self.torque_from_force)(b, force, pos) }
    }
    fn velocity_at_position(&self, b: H, pos: Vector3f) -> Vector3f {
        unsafe { (self.velocity_at_position)(b, pos) }
    }
    fn add_force(&self, b: H, f: Vector3f) {
        unsafe { (self.add_force)(b, f) }
    }
    fn add_torque(&self, b: H, t: Vector3f) {
        unsafe { (self.add_torque)(b, t) }
    }
    fn gravity_attachment_rotation(&self, b: H) -> bool {
        unsafe { (self.gravity_attachment_rotation)(b) }
    }
    fn gravity_attachment_velocity(&self, b: H) -> bool {
        unsafe { (self.gravity_attachment_velocity)(b) }
    }
    fn gravity_attachment_axes(&self, b: H) -> i32 {
        unsafe { (self.gravity_attachment_axes)(b) }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h14_aux_new() -> *mut Aux {
    Box::into_raw(Box::new(Aux::new()))
}

/// # Safety
/// `a` comes from `ohpl_h14_aux_new` and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h14_aux_free(a: *mut Aux) {
    if !a.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(a) });
    }
}

/// # Safety
/// `a` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h14_reset_pids(a: *mut Aux) {
    // SAFETY: caller contract.
    if let Some(a) = unsafe { a.as_mut() } {
        a.reset_pids();
    }
}

/// # Safety
/// `a` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h14_clear_smoothing(a: *mut Aux) {
    // SAFETY: caller contract.
    if let Some(a) = unsafe { a.as_mut() } {
        a.clear_smoothing();
    }
}

/// # Safety
/// `a` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h14_clear_camera(a: *mut Aux) {
    // SAFETY: caller contract.
    if let Some(a) = unsafe { a.as_mut() } {
        a.clear_camera();
    }
}

/// # Safety
/// `a` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h14_setup_safe_positions(a: *mut Aux, interval: f32, buffer: f32) {
    // SAFETY: caller contract.
    if let Some(a) = unsafe { a.as_mut() } {
        a.setup_safe_positions(interval, buffer);
    }
}

/// # Safety
/// `a` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h14_update_safe_positions(a: *mut Aux, dt: f32, pos: Vector3f) {
    // SAFETY: caller contract.
    if let Some(a) = unsafe { a.as_mut() } {
        bump(C_SAFE);
        a.update_safe_positions(dt, pos);
    }
}

/// # Safety
/// `a` live, `out` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h14_safe_position(
    a: *const Aux,
    elapsed: f32,
    out: *mut Vector3f,
) -> bool {
    // SAFETY: caller contract.
    match (unsafe { a.as_ref() }, unsafe { out.as_mut() }) {
        (Some(a), Some(out)) => a.safe_position(elapsed).is_some_and(|p| {
            *out = p;
            true
        }),
        _ => false,
    }
}

/// # Safety
/// `vt`, `conn`, `out` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h14_body_connection_pos(
    vt: *const BodyVt,
    conn: *const Conn,
    out: *mut Vector3f,
) {
    // SAFETY: caller contract.
    if let (Some(vt), Some(conn), Some(out)) = unsafe { (vt.as_ref(), conn.as_ref(), out.as_mut()) }
    {
        *out = body_connection_pos(&vt, conn);
    }
}

/// # Safety
/// `st`, `conn`, `out` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h14_char_connection_pos(
    st: *const CharState,
    conn: *const Conn,
    out: *mut Vector3f,
) {
    // SAFETY: caller contract.
    if let (Some(st), Some(conn), Some(out)) = unsafe { (st.as_ref(), conn.as_ref(), out.as_mut()) }
    {
        *out = char_connection_pos(st, conn);
    }
}

/// # Safety
/// `a`, `vt`, `st`, `conn` valid; `st` not borrowed by Rust elsewhere.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h14_update_body_connection(
    a: *mut Aux,
    vt: *const BodyVt,
    st: *const CharState,
    conn: *const Conn,
    on_ground: bool,
    dt: f32,
) {
    // SAFETY: caller contract.
    if let (Some(a), Some(vt), Some(st), Some(conn)) =
        unsafe { (a.as_mut(), vt.as_ref(), st.as_ref(), conn.as_ref()) }
    {
        bump(C_BODY_CONN);
        a.update_body_connection(&vt, st, conn, on_ground, dt);
    }
}

/// Returns true and fills `force` when the character must add it to its own force.
///
/// # Safety
/// `a`, `vt`, `conn`, `force` valid; `st` live and not borrowed by Rust elsewhere.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h14_update_character_connection(
    a: *mut Aux,
    vt: *const BodyVt,
    st: *mut CharState,
    conn: *const Conn,
    dt: f32,
    force: *mut Vector3f,
) -> bool {
    // SAFETY: caller contract.
    let (Some(a), Some(vt), Some(conn), Some(force)) = (
        unsafe { a.as_mut() },
        unsafe { vt.as_ref() },
        unsafe { conn.as_ref() },
        unsafe { force.as_mut() },
    ) else {
        return false;
    };
    if !conn.affect_char {
        return false;
    }
    bump(C_CHAR_CONN);
    *force = a.update_character_connection(&vt, st, conn, dt);
    true
}

/// # Safety
/// `vt`, `grav` valid; `st` live and not borrowed by Rust elsewhere.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h14_update_gravity_attachment(
    vt: *const BodyVt,
    st: *mut CharState,
    grav: *mut Grav,
) {
    // SAFETY: caller contract.
    if let (Some(vt), Some(grav)) = unsafe { (vt.as_ref(), grav.as_mut()) } {
        bump(C_GRAVITY);
        Aux::update_gravity_attachment(&vt, st, grav);
    }
}

/// # Safety
/// `a`, `i`, `o` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h14_update_camera(a: *mut Aux, i: *const CamIn, o: *mut CamOut) {
    // SAFETY: caller contract.
    if let (Some(a), Some(i), Some(o)) = unsafe { (a.as_mut(), i.as_ref(), o.as_mut()) } {
        bump(C_CAMERA);
        *o = a.update_camera(i);
    }
}

/// # Safety
/// `a`, `i`, `o` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h14_update_entity(a: *mut Aux, i: *const EntIn, o: *mut Matrixf) {
    // SAFETY: caller contract.
    if let (Some(a), Some(i), Some(o)) = unsafe { (a.as_mut(), i.as_ref(), o.as_mut()) } {
        bump(C_ENTITY);
        *o = a.update_entity(i);
    }
}

/// # Safety
/// `out` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h14_rope_plan(total: f32, segment: f32, out: *mut RopePlan) {
    // SAFETY: caller contract.
    if let Some(out) = unsafe { out.as_mut() } {
        *out = rope_plan(total, segment);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h14_rope_particle_pos(
    start: Vector3f,
    dir: Vector3f,
    length: f32,
) -> Vector3f {
    rope_particle_pos(start, dir, length)
}

/// # Safety
/// `ext` and `st` are writable and embedded in the character; `st.size` is set.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h14_char_init(
    ext: *mut super::char_conn::CharExt,
    st: *mut super::character::CharState,
    world: usize,
    max_no_slide_slope_cos: f32,
) {
    // SAFETY: caller contract.
    unsafe { super::char_conn::char_init(ext, st, world, max_no_slide_slope_cos) };
}

/// # Safety
/// `ext` and `st` are the live embedded states of character `me`; `op` is its host op.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h14_connect_to_body(
    op: super::wreg::WorldOp,
    me: usize,
    ext: *mut super::char_conn::CharExt,
    st: *mut super::character::CharState,
    body: usize,
    pos: crate::types::Vector3f,
    flags: u32,
) {
    // SAFETY: caller contract.
    unsafe { super::char_conn::connect_to_body(op, me, ext, st, body, pos, flags) };
}

/// # Safety
/// `ext` is the live embedded state of character `me`; `op` is its host op.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h14_disconnect_body(
    op: super::wreg::WorldOp,
    me: usize,
    ext: *mut super::char_conn::CharExt,
) {
    // SAFETY: caller contract.
    unsafe { super::char_conn::disconnect_body(op, me, ext) };
}

/// # Safety
/// `ext` and `st` are the live embedded states of character `ch`; `cd` is the live collide data.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h14_collide_gravity(
    op: super::wreg::WorldOp,
    ch: usize,
    ext: *mut super::char_conn::CharExt,
    st: *const super::character::CharState,
    body: usize,
    cd: usize,
) {
    // SAFETY: caller contract.
    unsafe { super::char_collide::collide_gravity(op, ch, ext, st, body, cd) };
}

/// # Safety
/// `ext` and `st` are the live embedded states of a character; `cd` is the live collide data.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h14_collide_push(
    op: super::wreg::WorldOp,
    ch: usize,
    ext: *const super::char_conn::CharExt,
    st: *const super::character::CharState,
    body: usize,
    cd: usize,
) {
    // SAFETY: caller contract.
    unsafe { super::char_collide::collide_push(op, ch, ext, st, body, cd) };
}

/// # Safety
/// `st` is writable and embedded in the rope.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h14_rope_init(
    st: *mut super::rope::RopeState,
    gravity: crate::types::Vector3f,
    start: crate::types::Vector3f,
    end: crate::types::Vector3f,
) {
    // SAFETY: caller contract.
    if let Some(s) = unsafe { st.as_mut() } {
        *s = super::rope::RopeState::initial(gravity, start, end);
    }
}

/// # Safety
/// `st` is the live embedded state of the rope.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h14_rope_check_collision(
    st: *const super::rope::RopeState,
    body: usize,
) -> bool {
    // SAFETY: caller contract.
    unsafe { super::rope_shell::check_particle_body_collision(st, body) }
}

/// # Safety
/// `st` is the live embedded state of the rope; `op` is its host op.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h14_rope_check_sleeping(
    op: super::wreg::WorldOp,
    st: *mut super::rope::RopeState,
) -> bool {
    // SAFETY: caller contract.
    unsafe { super::rope_shell::check_specific_sleeping(op, st) }
}

/// # Safety
/// As [`ohpl_h14_rope_check_sleeping`].
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h14_rope_check_awake(
    op: super::wreg::WorldOp,
    st: *mut super::rope::RopeState,
) -> bool {
    // SAFETY: caller contract.
    unsafe { super::rope_shell::check_specific_awake(op, st) }
}

/// # Safety
/// As [`ohpl_h14_rope_check_sleeping`].
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h14_rope_set_specific_sleeping(
    op: super::wreg::WorldOp,
    st: *mut super::rope::RopeState,
    sleeping: bool,
) {
    // SAFETY: caller contract.
    unsafe { super::rope_shell::set_specific_sleeping(op, st, sleeping) };
}

/// # Safety
/// `st` is the live embedded state of rope `me`; `op` is its host op.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h14_rope_set_motor(
    op: super::wreg::WorldOp,
    me: usize,
    st: *mut super::rope::RopeState,
    on: bool,
) {
    // SAFETY: caller contract.
    unsafe { super::rope_shell::set_motor_active(op, me, st, on) };
}

/// # Safety
/// As [`ohpl_h14_rope_set_motor`].
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h14_rope_set_auto_move(
    op: super::wreg::WorldOp,
    me: usize,
    st: *mut super::rope::RopeState,
    on: bool,
) {
    // SAFETY: caller contract.
    unsafe { super::rope_shell::set_auto_move_active(op, me, st, on) };
}

/// # Safety
/// As [`ohpl_h14_rope_set_motor`].
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h14_rope_remove_attached(
    op: super::wreg::WorldOp,
    me: usize,
    st: *mut super::rope::RopeState,
    body: usize,
    remove_container: bool,
) {
    // SAFETY: caller contract.
    unsafe { super::rope_shell::remove_attached_body(op, me, st, body, remove_container) };
}

/// # Safety
/// As [`ohpl_h14_rope_set_motor`].
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h14_rope_set_attached(
    op: super::wreg::WorldOp,
    me: usize,
    st: *mut super::rope::RopeState,
    idx: usize,
    particle: usize,
    body: usize,
) {
    // SAFETY: caller contract.
    unsafe { super::rope_shell::set_attached_body(op, me, st, idx, particle, body) };
}
