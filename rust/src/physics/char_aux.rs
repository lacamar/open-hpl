//! The parts of `iCharacterBody` that `character.rs` left in C++: connections to a body, gravity
//! attachment, camera and entity smoothing, safe positions.
//!
//! `Aux` owns the PID, smoothing and safe-position state; the engine objects stay in C++ and are
//! reached through [`BodyHost`], so nothing here holds a borrow across a host call.

use super::character::{CharState, S, rotate_y, update_move_matrix};
use super::host::H;
use super::pid::{Pid, Pid3};
use crate::math::{
    IDENTITY, acos, cross3, div3, dot3, len3, matrix_inverse, matrix_mul, matrix_mul_vec,
    matrix_to_euler_angles, normalize3, sub3, v3, vector3_angle,
};
use crate::types::{Matrixf, Vector3f};

use std::sync::atomic::{AtomicU64, Ordering};

pub const COUNTERS: usize = 16;
pub const C_BODY_CONN: usize = 0;
pub const C_CHAR_CONN: usize = 1;
pub const C_GRAVITY: usize = 2;
pub const C_CAMERA: usize = 3;
pub const C_ENTITY: usize = 4;
pub const C_SAFE: usize = 5;
const C_ATTACHED: usize = 6;
const C_ATT_ROT: usize = 7;
const C_ATT_YAW: usize = 8;
const C_ATT_POS: usize = 9;
const C_ATT_VEL: usize = 10;
const C_ALIGN_CENTER: usize = 11;
const C_ALIGN_BODY_ROT: usize = 12;
const C_ALIGN_FREE_ROT: usize = 13;
const C_ALIGN_CHAR_ROT: usize = 14;
const C_SMOOTH: usize = 15;

static CALLS: [AtomicU64; COUNTERS] = [const { AtomicU64::new(0) }; COUNTERS];

pub fn bump(i: usize) {
    if let Some(c) = CALLS.get(i) {
        c.fetch_add(1, Ordering::Relaxed);
    }
}

pub fn counter(i: usize) -> u64 {
    CALLS.get(i).map_or(0, |c| c.load(Ordering::Relaxed))
}

const ZERO: Vector3f = v3(0.0, 0.0, 0.0);
const SAFE_NONE: Vector3f = v3(-1_000_000.0, -1_000_000.0, -1_000_000.0);
const PID_STEPS: usize = 10;

/// Body queries and force output, one call per `iPhysicsBody` method.
pub trait BodyHost {
    fn mass(&self, b: H) -> f32;
    fn world_matrix(&self, b: H) -> Matrixf;
    fn local_matrix(&self, b: H) -> Matrixf;
    fn world_position(&self, b: H) -> Vector3f;
    fn inertia(&self, b: H) -> Matrixf;
    fn torque_from_force(&self, b: H, force: Vector3f, pos: Vector3f) -> Vector3f;
    fn velocity_at_position(&self, b: H, pos: Vector3f) -> Vector3f;
    fn add_force(&self, b: H, f: Vector3f);
    fn add_torque(&self, b: H, t: Vector3f);
    fn gravity_attachment_rotation(&self, b: H) -> bool;
    fn gravity_attachment_velocity(&self, b: H) -> bool;
    fn gravity_attachment_axes(&self, b: H) -> i32;
}

/// Connection settings written by `ConnectToBody`, mirrored in `RustPhysicsH14.h`.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Conn {
    pub body: H,
    pub max_force: f32,
    pub max_torque: f32,
    pub distance_to_body: f32,
    pub pos_local_to_body: Vector3f,
    pub pos_local_to_char: Vector3f,
    pub normal_local_to_body: Vector3f,
    pub local_up_body: Vector3f,
    pub body_start_rotation: Vector3f,
    pub char_local_forward: Vector3f,
    pub world_gravity: Vector3f,
    pub depend_on_body_rotation: bool,
    pub align_center: bool,
    pub align_rotation: bool,
    pub affect_char: bool,
    pub depend_on_char_rotation: bool,
    pub align_char_rotation: bool,
}

/// Gravity attachment members of the character that the collide callback also writes.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Grav {
    pub attached: H,
    pub last_attached: H,
    pub prev_matrix: Matrixf,
    pub body_velocity: Vector3f,
    pub rotate_yaw: bool,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct CamIn {
    pub position: Vector3f,
    pub right: Vector3f,
    pub forward: Vector3f,
    pub pos_add: Vector3f,
    pub main_height: f32,
    pub size_y: f32,
    pub yaw: f32,
    pub smooth_num: i32,
    pub update_active: bool,
    pub use_smoothing: bool,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct CamOut {
    pub position: Vector3f,
    pub yaw: f32,
    pub set: bool,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct EntIn {
    pub position: Vector3f,
    pub mtx_move: Matrixf,
    pub offset: Matrixf,
    pub post_offset: Matrixf,
    pub yaw: f32,
    pub smooth_pos_num: i32,
    pub smooth_y_num: i32,
    pub smooth_y: bool,
}

pub struct Aux {
    body_point: Pid3,
    body_center: Pid3,
    body_one_axis: Pid,
    body_angular: Pid3,
    char_point: Pid3,
    cam_pos: Vec<Vector3f>,
    ent_pos: Vec<Vector3f>,
    ent_y: Vec<f32>,
    safe_history: [Vector3f; 4],
    safe_timer: f32,
    safe_interval: f32,
    safe_buffer: f32,
    safe_positions: Vec<Vector3f>,
}

fn rotation(m: &Matrixf) -> Matrixf {
    let mut r = IDENTITY;
    for (dst, src) in r.m.iter_mut().zip(&m.m).take(3) {
        dst[..3].copy_from_slice(&src[..3]);
    }
    r
}

fn translation(m: &Matrixf) -> Vector3f {
    v3(m.m[0][3], m.m[1][3], m.m[2][3])
}

fn add3(a: Vector3f, b: Vector3f) -> Vector3f {
    v3(a.x + b.x, a.y + b.y, a.z + b.z)
}

fn mul3(a: Vector3f, s: f32) -> Vector3f {
    v3(a.x * s, a.y * s, a.z * s)
}

/// `cMath::Vector3ClampToLength` (math/mod.rs does not export it).
fn clamp_len(v: Vector3f, max: f32) -> Vector3f {
    if max <= 0.0 {
        return v;
    }
    let l = len3(v);
    if l > max { mul3(div3(v, l), max) } else { v }
}

fn normalized(mut v: Vector3f) -> Vector3f {
    normalize3(&mut v);
    v
}

/// Mean over `list` as the C++ loops sum it: left to right from zero.
fn mean(list: &[Vector3f]) -> Vector3f {
    let total = list.iter().fold(ZERO, |t, p| add3(t, *p));
    div3(total, list.len() as f32)
}

/// `iCharacterBody::GetBodyConnectionPos`.
pub fn body_connection_pos<B: BodyHost>(h: &B, c: &Conn) -> Vector3f {
    if c.body == 0 {
        return ZERO;
    }
    if c.depend_on_body_rotation {
        matrix_mul_vec(&h.world_matrix(c.body), c.pos_local_to_body)
    } else {
        add3(h.world_position(c.body), c.pos_local_to_body)
    }
}

/// `iCharacterBody::GetCharConnectionPos`.
pub fn char_connection_pos(s: &CharState, c: &Conn) -> Vector3f {
    if c.body == 0 {
        return ZERO;
    }
    if c.depend_on_char_rotation || c.align_char_rotation {
        add3(
            s.position,
            crate::math::matrix_mul_3x3(&s.mtx_move, c.pos_local_to_char),
        )
    } else {
        add3(s.position, c.pos_local_to_char)
    }
}

impl Default for Aux {
    fn default() -> Self {
        Aux::new()
    }
}

impl Aux {
    pub fn new() -> Self {
        let mut one_axis = Pid::new();
        one_axis.set_error_num(PID_STEPS as i32);
        Aux {
            body_point: Pid3::new(PID_STEPS),
            body_center: Pid3::new(PID_STEPS),
            body_one_axis: one_axis,
            body_angular: Pid3::new(PID_STEPS),
            char_point: Pid3::new(PID_STEPS),
            cam_pos: Vec::new(),
            ent_pos: Vec::new(),
            ent_y: Vec::new(),
            safe_history: [ZERO; 4],
            safe_timer: 0.0,
            safe_interval: 0.0,
            safe_buffer: 0.0,
            safe_positions: Vec::new(),
        }
    }

    pub fn reset_pids(&mut self) {
        self.body_point.reset();
        self.body_center.reset();
        self.body_one_axis.reset();
        self.body_angular.reset();
        self.char_point.reset();
    }

    pub fn clear_smoothing(&mut self) {
        self.cam_pos.clear();
        self.ent_pos.clear();
        self.ent_y.clear();
    }

    pub fn clear_camera(&mut self) {
        self.cam_pos.clear();
    }

    pub fn setup_safe_positions(&mut self, interval: f32, buffer: f32) {
        if buffer <= 0.0 {
            return;
        }
        self.safe_timer = 0.0;
        self.safe_interval = interval;
        self.safe_buffer = buffer;
        self.safe_history = [SAFE_NONE; 4];
        self.safe_positions.clear();
    }

    pub fn update_safe_positions(&mut self, dt: f32, position: Vector3f) {
        if self.safe_interval <= 0.0 {
            return;
        }
        self.safe_history.rotate_left(1);
        self.safe_history[3] = position;
        self.safe_timer -= dt;
        if self.safe_timer > 0.0 {
            return;
        }
        self.safe_timer = self.safe_interval;
        if self.safe_history[0] == SAFE_NONE {
            return;
        }
        self.safe_positions.push(self.safe_history[0]);
        if self.safe_positions.len() as f32 > self.safe_buffer / self.safe_interval {
            self.safe_positions.pop();
        }
    }

    /// Position `TeleportToSafePosition` moves to, if any.
    pub fn safe_position(&self, elapsed: f32) -> Option<Vector3f> {
        if self.safe_positions.is_empty() {
            return None;
        }
        let idx = (elapsed / self.safe_interval + 0.5).floor() as i32;
        let idx = idx.min(self.safe_positions.len() as i32 - 1);
        self.safe_positions.get(usize::try_from(idx).ok()?).copied()
    }

    /// `iCharacterBody::UpdateBodyConnection`; the caller checked for a connected body.
    pub fn update_body_connection<B: BodyHost>(
        &mut self,
        h: &B,
        s: &CharState,
        c: &Conn,
        on_ground: bool,
        dt: f32,
    ) {
        let body = c.body;
        let body_mass = h.mass(body);
        if body_mass == 0.0 {
            return;
        }
        let mut total_torque = ZERO;
        let mut total_force = ZERO;

        let mut max_total_char_force = c.max_force;
        if !on_ground {
            let g = if s.custom_gravity_active {
                len3(s.custom_gravity)
            } else {
                len3(c.world_gravity)
            };
            max_total_char_force = g * s.mass;
        }
        let max_rel_force = max_total_char_force / body_mass;
        let max_rel_torque = c.max_torque / body_mass;

        self.body_point.p = 200.0;
        self.body_point.i = 0.0;
        self.body_point.d = 10.0;

        let body_conn = body_connection_pos(h, c);
        let char_conn = char_connection_pos(s, c);

        let error = sub3(char_conn, body_conn);
        let force = clamp_len(self.body_point.output(error, dt), max_rel_force);
        total_force = add3(total_force, force);

        if c.depend_on_body_rotation {
            let torque = h.torque_from_force(body, force, body_conn);
            total_torque = add3(total_torque, torque);
        }

        let mut normal = c.normal_local_to_body;
        if c.depend_on_char_rotation {
            normal = matrix_mul_vec(&rotation(&s.mtx_move), normal);
        }

        if c.align_center {
            bump(C_ALIGN_CENTER);
            self.body_center.p = 200.0;
            self.body_center.i = 0.0;
            self.body_center.d = 10.0;
            let centre = h.world_position(body);
            let d = c.distance_to_body;
            let wanted = v3(
                normal.x.mul_add(d, char_conn.x),
                normal.y.mul_add(d, char_conn.y),
                normal.z.mul_add(d, char_conn.z),
            );
            let force = self.body_center.output(sub3(wanted, centre), dt);
            total_force = add3(total_force, force);
        }

        if c.align_rotation {
            if c.depend_on_body_rotation {
                bump(C_ALIGN_BODY_ROT);
                self.body_one_axis.p = 20.0;
                self.body_one_axis.i = 0.0;
                self.body_one_axis.d = 2.5;

                let body_up = matrix_mul_vec(&rotation(&h.world_matrix(body)), c.local_up_body);
                let body_right = cross3(normal, s.up);
                let mut error = vector3_angle(body_up, s.up);
                if dot3(body_right, body_up) > 0.0 {
                    error = -error;
                }
                let size = self.body_one_axis.output(error, dt);
                let torque = clamp_len(mul3(normal, size), max_rel_torque);
                total_torque = add3(total_torque, torque);
            } else {
                bump(C_ALIGN_FREE_ROT);
                self.body_angular.p = 20.0;
                self.body_angular.i = 0.0;
                self.body_angular.d = 2.5;

                let current = matrix_to_euler_angles(&rotation(&h.local_matrix(body)));
                let error = sub3(c.body_start_rotation, current);
                let torque = clamp_len(self.body_angular.output(error, dt), max_rel_torque);
                total_torque = add3(total_torque, torque);
            }
        }

        total_force = clamp_len(total_force, 100.0);
        total_force = clamp_len(total_force, max_total_char_force);
        h.add_force(body, mul3(total_force, body_mass));

        total_torque = clamp_len(total_torque, 10.0);
        h.add_torque(body, matrix_mul_vec(&h.inertia(body), total_torque));
    }

    /// `iCharacterBody::UpdateCharacterConnection`; the force to add to the character, if any.
    pub fn update_character_connection<B: BodyHost>(
        &mut self,
        h: &B,
        s: *mut CharState,
        c: &Conn,
        dt: f32,
    ) -> Vector3f {
        if !c.affect_char {
            return ZERO;
        }
        if c.align_char_rotation {
            bump(C_ALIGN_CHAR_ROT);
            let wanted = matrix_mul_vec(&rotation(&h.world_matrix(c.body)), c.char_local_forward);
            let xz = normalized(v3(wanted.x, 0.0, wanted.z));
            let mut yaw = vector3_angle(v3(0.0, 0.0, -1.0), xz);
            if xz.x > 0.0 {
                yaw = -yaw;
            }
            // SAFETY: `s` is the live character state; short raw accesses, no reference kept.
            unsafe {
                (*s).yaw = yaw;
                update_move_matrix(&S::new(s));
            }
        }

        self.char_point.p = 100.0;
        self.char_point.i = 0.0;
        self.char_point.d = 0.0;

        // SAFETY: as above.
        let state = unsafe { *s };
        let body_conn = body_connection_pos(h, c);
        let char_conn = char_connection_pos(&state, c);
        let force = self.char_point.output(sub3(body_conn, char_conn), dt);
        mul3(force, state.mass)
    }

    /// `iCharacterBody::UpdateGravityAttachment`.
    pub fn update_gravity_attachment<B: BodyHost>(h: &B, s: *mut CharState, g: &mut Grav) {
        // SAFETY (all raw accesses): `s` is the live character state; no reference is kept.
        if g.attached == 0 {
            if g.last_attached != 0 {
                unsafe { (*s).gravity_attachment_velocity = g.body_velocity };
            }
            return;
        }
        unsafe { (*s).gravity_attachment_velocity = ZERO };
        bump(C_ATTACHED);

        let world = h.world_matrix(g.attached);
        if g.last_attached == g.attached {
            if h.gravity_attachment_rotation(g.attached) {
                bump(C_ATT_ROT);
                let inv_prev = matrix_inverse(&g.prev_matrix);
                let delta = matrix_mul(&world, &inv_prev);
                unsafe { (*s).position = matrix_mul_vec(&delta, (*s).position) };

                if g.rotate_yaw {
                    let rot_delta = matrix_mul(&rotation(&world), &rotation(&inv_prev));
                    let mut forward = matrix_mul_vec(&rot_delta, v3(0.0, 0.0, -1.0));
                    forward.y = 0.0;
                    let length = len3(forward);
                    if length > 0.0001 {
                        forward = div3(forward, length);
                        let mut angle = acos(-forward.z);
                        if lex_positive(forward) {
                            angle = -angle;
                        }
                        bump(C_ATT_YAW);
                        unsafe {
                            (*s).yaw += angle;
                            update_move_matrix(&S::new(s));
                        }
                    }
                }
            } else {
                bump(C_ATT_POS);
                let moved = sub3(translation(&world), translation(&g.prev_matrix));
                unsafe { (*s).position = add3((*s).position, moved) };
            }
        }

        if h.gravity_attachment_velocity(g.attached) {
            bump(C_ATT_VEL);
            let feet = unsafe {
                v3(
                    (*s).position.x,
                    (*s).position.y - (*s).size.y / 2.0,
                    (*s).position.z,
                )
            };
            let vel = h.velocity_at_position(g.attached, feet);
            g.body_velocity = match h.gravity_attachment_axes(g.attached) {
                0 => v3(vel.x, 0.0, vel.z),
                1 => v3(0.0, vel.y, 0.0),
                _ => vel,
            };
        } else {
            g.body_velocity = ZERO;
        }
        g.prev_matrix = world;
    }

    /// `iCharacterBody::UpdateCamera` after the camera null check.
    pub fn update_camera(&mut self, i: &CamIn) -> CamOut {
        let mut out = CamOut {
            position: ZERO,
            yaw: i.yaw,
            set: false,
        };
        if !i.update_active {
            self.cam_pos.clear();
            return out;
        }

        let mut add = ZERO;
        add.y += i.pos_add.y;
        add = v3(
            i.right.x.mul_add(i.pos_add.x, add.x),
            i.right.y.mul_add(i.pos_add.x, add.y),
            i.right.z.mul_add(i.pos_add.x, add.z),
        );
        add = v3(
            i.forward.x.mul_add(i.pos_add.z, add.x),
            i.forward.y.mul_add(i.pos_add.z, add.y),
            i.forward.z.mul_add(i.pos_add.z, add.z),
        );

        let head = add3(i.position, v3(0.0, i.main_height - i.size_y / 2.0, 0.0));
        let pos = if i.smooth_num <= 0 || !i.use_smoothing {
            head
        } else {
            bump(C_SMOOTH);
            self.cam_pos.push(head);
            if self.cam_pos.len() as i64 > i64::from(i.smooth_num) {
                self.cam_pos.remove(0);
            }
            mean(&self.cam_pos)
        };
        out.position = add3(pos, add);
        out.set = true;
        out
    }

    /// `iCharacterBody::UpdateEntity` after the entity null check; the matrix to set.
    pub fn update_entity(&mut self, i: &EntIn) -> Matrixf {
        let mut smooth_y = i.position.y;
        if i.smooth_y {
            self.ent_y.push(i.position.y);
            if self.ent_y.len() as i64 > i64::from(i.smooth_y_num) {
                self.ent_y.remove(0);
            }
            smooth_y = self.ent_y.iter().fold(0.0, |t, y| t + y) / self.ent_y.len() as f32;
        }

        if i.smooth_pos_num <= 0 {
            let mut m = rotate_y(i.yaw);
            m.m[0][3] = i.position.x;
            m.m[1][3] = smooth_y;
            m.m[2][3] = i.position.z;
            return matrix_mul(&m, &i.offset);
        }

        self.ent_pos.push(i.position);
        if self.ent_pos.len() as i64 > i64::from(i.smooth_pos_num) {
            self.ent_pos.remove(0);
        }
        let mut pos = mean(&self.ent_pos);
        if i.smooth_y {
            pos.y = smooth_y;
        }

        let mut m = i.mtx_move;
        m.m[0][3] = 0.0;
        m.m[1][3] = 0.0;
        m.m[2][3] = 0.0;
        let mut m = matrix_mul(&i.post_offset, &m);
        m.m[0][3] += pos.x;
        m.m[1][3] += pos.y;
        m.m[2][3] += pos.z;
        matrix_mul(&m, &i.offset)
    }
}

/// `cVector3f > 0` with the scalar widened to a vector: lexicographic, first non-zero component.
fn lex_positive(v: Vector3f) -> bool {
    if v.x != 0.0 {
        return v.x > 0.0;
    }
    if v.y != 0.0 {
        return v.y > 0.0;
    }
    v.z > 0.0
}

const _: () = {
    use std::mem::{offset_of, size_of};
    assert!(size_of::<Conn>() == 112 && offset_of!(Conn, pos_local_to_body) == 20);
    assert!(offset_of!(Conn, depend_on_body_rotation) == 104);
    assert!(size_of::<Grav>() == 96 && offset_of!(Grav, prev_matrix) == 16);
    assert!(offset_of!(Grav, rotate_yaw) == 92);
    assert!(size_of::<CamIn>() == 68 && offset_of!(CamIn, main_height) == 48);
    assert!(offset_of!(CamIn, update_active) == 64);
    assert!(size_of::<CamOut>() == 20 && offset_of!(CamOut, set) == 16);
    assert!(size_of::<EntIn>() == 220 && offset_of!(EntIn, yaw) == 204);
    assert!(offset_of!(EntIn, smooth_y) == 216);
};
