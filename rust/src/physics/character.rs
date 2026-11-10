//! `iCharacterBody::Update` and the movement, collision and step-climbing it drives.
//!
//! State lives in C++ (`OhplCharState`) because the C++ collision callbacks and the parts left in
//! C++ (camera, entity, connections, gravity attachment) read and write it during host calls, so
//! it is only ever touched through a raw pointer, never a long-lived reference.

use crate::math::{
    cos, cross3, div3, dot3, fma, len3, matrix_mul_3x3, matrix_mul_vec, matrix_rotate, normalize3,
    sin, v3,
};
use crate::types::{Matrixf, Vector3f};

pub const CB_NONE: i32 = 0;
pub const CB_PUSH: i32 = 1;
pub const CB_GRAVITY: i32 = 2;

const PI4: f32 = std::f32::consts::FRAC_PI_4;
const EPSILON: f32 = 0.0001;
const DIR_FORWARD: usize = 0;
const DIR_RIGHT: usize = 1;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct CharState {
    pub position: Vector3f,
    pub last_position: Vector3f,
    pub last_move_pos_add: Vector3f,
    pub force: Vector3f,
    pub velocity: Vector3f,
    pub last_ground_normal: Vector3f,
    pub gravity_attachment_velocity: Vector3f,
    pub custom_gravity: Vector3f,
    pub size: Vector3f,
    pub forward: Vector3f,
    pub right: Vector3f,
    pub up: Vector3f,
    pub mtx_move: Matrixf,
    pub pitch: f32,
    pub yaw: f32,
    pub mass: f32,
    pub max_gravity_speed: f32,
    pub ground_friction: f32,
    pub air_friction: f32,
    pub max_no_slide_slope_cos: f32,
    pub ground_angle_min_cos: f32,
    pub max_step_height: f32,
    pub max_step_height_in_air: f32,
    pub step_climb_speed: f32,
    pub climb_forward_mul: f32,
    pub climb_height_add: f32,
    pub check_step_climb_count: f32,
    pub check_step_climb_interval: f32,
    pub move_delay_count: f32,
    pub max_pos_move_speed: [f32; 2],
    pub max_neg_move_speed: [f32; 2],
    pub current_move_acc: [f32; 2],
    pub move_acc: [f32; 2],
    pub move_deacc: [f32; 2],
    pub move_speed: [f32; 2],
    pub move_speed_mul: [f32; 2],
    pub move_opposite_dir_acc_mul: [f32; 2],
    pub on_ground_count: i32,
    pub max_on_ground_count: i32,
    pub current_shape_idx: i32,
    pub collide_flags: u32,
    pub moving: [bool; 2],
    pub active: bool,
    pub test_collision: bool,
    pub gravity_active: bool,
    pub custom_gravity_active: bool,
    pub deaccelerate_move_speed_in_air: bool,
    pub moved_last_update: bool,
    pub disable_diag_speed_boost: bool,
    pub climbing: bool,
    pub accurate_climbing: bool,
}

/// Engine calls made while updating a character; `ctx` is the C++ `iCharacterBody`.
pub trait CharHost {
    fn world_gravity(&self) -> Vector3f;
    fn shape_radius(&self) -> f32;
    fn shape_size_y(&self, idx: i32) -> f32;
    fn check_collision(&self, push: &mut Vector3f, pos: Vector3f, callback: i32, idx: i32) -> bool;
    /// `CheckRayIntersection` with both outputs requested.
    fn cast_ray(
        &self,
        start: Vector3f,
        end: Vector3f,
        dist: &mut f32,
        normal: &mut Vector3f,
    ) -> bool;
    /// Passes the live `velocity` member by reference, as the C++ does.
    fn on_hit_ground(&self);
    fn pre_connection(&self, dt: f32);
    fn clear_gravity_attachment(&self);
    fn update_gravity_attachment(&self);
    fn update_body(&self);
    fn update_character_connection(&self, dt: f32);
    fn update_body_connection(&self, dt: f32);
    fn update_camera(&self);
    fn update_entity(&self);
    fn post_connection(&self, dt: f32);
    fn update_safe_positions(&self, dt: f32);
}

pub struct S(*mut CharState);

macro_rules! scalars {
    ($($f:ident, $set:ident : $t:ty;)*) => {
        #[allow(dead_code)] // setters only some fields need
        impl S {
            $(
                fn $f(&self) -> $t {
                    // SAFETY: `S::new` caller guarantees a live state; raw read, no reference.
                    unsafe { (*self.0).$f }
                }
                fn $set(&self, v: $t) {
                    // SAFETY: as above; raw write, no reference.
                    unsafe { (*self.0).$f = v }
                }
            )*
        }
    };
}

macro_rules! arrays {
    ($($f:ident, $set:ident : $t:ty;)*) => {
        #[allow(dead_code)] // setters only some fields need
        impl S {
            $(
                fn $f(&self, i: usize) -> $t {
                    // SAFETY: `S::new` caller guarantees a live state; raw read of a Copy array.
                    let a = unsafe { (*self.0).$f };
                    a.get(i).copied().unwrap_or_default()
                }
                fn $set(&self, i: usize, v: $t) {
                    // SAFETY: as above; read-modify-write of one array, no reference kept.
                    unsafe {
                        let mut a = (*self.0).$f;
                        if let Some(e) = a.get_mut(i) {
                            *e = v;
                        }
                        (*self.0).$f = a;
                    }
                }
            )*
        }
    };
}

scalars! {
    position, set_position: Vector3f;
    last_position, set_last_position: Vector3f;
    last_move_pos_add, set_last_move_pos_add: Vector3f;
    force, set_force: Vector3f;
    velocity, set_velocity: Vector3f;
    last_ground_normal, set_last_ground_normal: Vector3f;
    gravity_attachment_velocity, set_gravity_attachment_velocity: Vector3f;
    custom_gravity, set_custom_gravity: Vector3f;
    size, set_size: Vector3f;
    forward, set_forward: Vector3f;
    right, set_right: Vector3f;
    up, set_up: Vector3f;
    mtx_move, set_mtx_move: Matrixf;
    pitch, set_pitch: f32;
    yaw, set_yaw: f32;
    mass, set_mass: f32;
    max_gravity_speed, set_max_gravity_speed: f32;
    ground_friction, set_ground_friction: f32;
    air_friction, set_air_friction: f32;
    max_no_slide_slope_cos, set_max_no_slide_slope_cos: f32;
    ground_angle_min_cos, set_ground_angle_min_cos: f32;
    max_step_height, set_max_step_height: f32;
    max_step_height_in_air, set_max_step_height_in_air: f32;
    step_climb_speed, set_step_climb_speed: f32;
    climb_forward_mul, set_climb_forward_mul: f32;
    climb_height_add, set_climb_height_add: f32;
    check_step_climb_count, set_check_step_climb_count: f32;
    check_step_climb_interval, set_check_step_climb_interval: f32;
    move_delay_count, set_move_delay_count: f32;
    on_ground_count, set_on_ground_count: i32;
    max_on_ground_count, set_max_on_ground_count: i32;
    current_shape_idx, set_current_shape_idx: i32;
    active, set_active: bool;
    test_collision, set_test_collision: bool;
    gravity_active, set_gravity_active: bool;
    custom_gravity_active, set_custom_gravity_active: bool;
    deaccelerate_move_speed_in_air, set_deaccelerate_move_speed_in_air: bool;
    moved_last_update, set_moved_last_update: bool;
    disable_diag_speed_boost, set_disable_diag_speed_boost: bool;
    climbing, set_climbing: bool;
    accurate_climbing, set_accurate_climbing: bool;
}

arrays! {
    max_pos_move_speed, set_max_pos_move_speed: f32;
    max_neg_move_speed, set_max_neg_move_speed: f32;
    current_move_acc, set_current_move_acc: f32;
    move_acc, set_move_acc: f32;
    move_deacc, set_move_deacc: f32;
    move_speed, set_move_speed: f32;
    move_speed_mul, set_move_speed_mul: f32;
    move_opposite_dir_acc_mul, set_move_opposite_dir_acc_mul: f32;
    moving, set_moving: bool;
}

impl S {
    /// # Safety
    /// `p` points to a live `CharState` for the lifetime of the returned value; other code
    /// (C++ callbacks) may write it between calls, so no reference to it may be held.
    pub unsafe fn new(p: *mut CharState) -> Self {
        S(p)
    }

    fn is_on_ground(&self) -> bool {
        self.on_ground_count() > 0
    }
}

/// `cMath::MatrixRotateY`.
fn rotate_y(a: f32) -> Matrixf {
    let (s, c) = (sin(a), cos(a));
    Matrixf {
        m: [
            [c, 0.0, s, 0.0],
            [0.0, 1.0, 0.0, 0.0],
            [-s, 0.0, c, 0.0],
            [0.0, 0.0, 0.0, 1.0],
        ],
    }
}

fn add3(a: Vector3f, b: Vector3f) -> Vector3f {
    v3(a.x + b.x, a.y + b.y, a.z + b.z)
}

fn sub3(a: Vector3f, b: Vector3f) -> Vector3f {
    v3(a.x - b.x, a.y - b.y, a.z - b.z)
}

/// `a + b * s` per component, fused as gcc does.
fn mul_add3(a: Vector3f, b: Vector3f, s: f32) -> Vector3f {
    v3(fma(b.x, s, a.x), fma(b.y, s, a.y), fma(b.z, s, a.z))
}

/// `a - b * s` per component, fused.
fn sub_mul3(a: Vector3f, b: Vector3f, s: f32) -> Vector3f {
    v3(fma(-b.x, s, a.x), fma(-b.y, s, a.y), fma(-b.z, s, a.z))
}

fn mul3s(a: Vector3f, s: f32) -> Vector3f {
    v3(a.x * s, a.y * s, a.z * s)
}

fn normalized(v: Vector3f) -> Vector3f {
    let mut v = v;
    normalize3(&mut v);
    v
}

fn sqr_length(v: Vector3f) -> f32 {
    dot3(v, v)
}

fn abs_is_zero(v: Vector3f) -> bool {
    v.x.abs() == 0.0 && v.y.abs() == 0.0 && v.z.abs() == 0.0
}

fn max(a: f32, b: f32) -> f32 {
    if a > b { a } else { b }
}

pub fn update_move_matrix(s: &S) {
    let mut m = matrix_rotate(v3(s.pitch(), s.yaw(), 0.0), 0);
    let p = s.position();
    m.m[0][3] = p.x;
    m.m[1][3] = p.y;
    m.m[2][3] = p.z;
    s.set_mtx_move(m);
    s.set_forward(matrix_mul_3x3(&m, v3(0.0, 0.0, -1.0)));
    s.set_right(matrix_mul_3x3(&m, v3(1.0, 0.0, 0.0)));
    s.set_up(matrix_mul_3x3(&m, v3(0.0, 1.0, 0.0)));
}

pub fn update<H: CharHost>(h: &H, s: &S, dt: f32, hpl3: bool) {
    if !s.active() {
        return;
    }
    if s.move_delay_count() > 0.0 {
        s.set_move_delay_count(s.move_delay_count() - dt);
    }
    h.pre_connection(dt);
    h.clear_gravity_attachment();
    s.set_last_position(s.position());
    update_move_matrix(s);
    update_step_climbing(s, dt);
    let mut pos_add = position_from_char_speed(s, dt);
    align_pos_add(s, &mut pos_add);
    s.set_last_move_pos_add(pos_add);
    if s.test_collision() {
        check_move_collision(h, s, pos_add, dt, hpl3);
    } else {
        s.set_position(add3(s.position(), pos_add));
    }
    update_forces(h, s, dt);
    if s.test_collision() {
        check_force_collision(h, s, dt);
    }
    update_friction(s, dt);
    h.update_gravity_attachment();
    h.update_body();
    h.update_character_connection(dt);
    h.update_body_connection(dt);
    h.update_camera();
    h.update_entity();
    h.post_connection(dt);
    h.update_safe_positions(dt);
}

fn update_step_climbing(s: &S, dt: f32) {
    if s.climbing() {
        s.set_check_step_climb_count(0.0);
        s.set_on_ground_count(s.max_on_ground_count());
    } else {
        s.set_check_step_climb_count(s.check_step_climb_count() - dt);
    }
    s.set_climbing(false);
}

#[expect(
    clippy::approx_constant,
    reason = "the C++ literal 0.7071f, not 1/sqrt(2)"
)]
fn position_from_char_speed(s: &S, dt: f32) -> Vector3f {
    let diag = if s.disable_diag_speed_boost() && s.moving(DIR_FORWARD) && s.moving(DIR_RIGHT) {
        0.7071
    } else {
        1.0
    };
    s.set_moved_last_update(false);
    for i in 0..2 {
        // gcc shares deacc*dt between the idle branches only; the moving ones fuse it
        let decel = s.move_deacc(i) * dt;
        if !s.moving(i) {
            if s.is_on_ground() || s.deaccelerate_move_speed_in_air() {
                if s.move_speed(i) > 0.0 {
                    s.set_move_speed(i, s.move_speed(i) - decel);
                    if s.move_speed(i) < 0.0 {
                        s.set_move_speed(i, 0.0);
                    }
                } else {
                    s.set_move_speed(i, s.move_speed(i) + decel);
                    if s.move_speed(i) > 0.0 {
                        s.set_move_speed(i, 0.0);
                    }
                }
            }
        } else {
            s.set_moved_last_update(true);
            let max_pos = s.max_pos_move_speed(i) * s.move_speed_mul(i) * diag;
            let max_neg = s.max_neg_move_speed(i) * s.move_speed_mul(i) * diag;
            if s.is_on_ground() || s.deaccelerate_move_speed_in_air() {
                if s.move_speed(i) > max_pos {
                    s.set_move_speed(i, fma(-s.move_deacc(i), dt, s.move_speed(i)));
                    if s.move_speed(i) < max_pos {
                        s.set_move_speed(i, max_pos);
                    }
                } else if s.move_speed(i) < max_neg {
                    s.set_move_speed(i, fma(s.move_deacc(i), dt, s.move_speed(i)));
                    if s.move_speed(i) > max_neg {
                        s.set_move_speed(i, max_neg);
                    }
                }
            }
            s.set_moving(i, false);
        }
    }

    for i in 0..2 {
        let max_pos = s.max_pos_move_speed(i) * s.move_speed_mul(i) * diag;
        let max_neg = s.max_neg_move_speed(i) * s.move_speed_mul(i) * diag;
        let mut acc_mul = 1.0;
        if s.is_on_ground()
            && ((s.current_move_acc(i) > 0.0 && s.move_speed(i) < 0.0)
                || (s.current_move_acc(i) < 0.0 && s.move_speed(i) > 0.0))
        {
            acc_mul = s.move_opposite_dir_acc_mul(i);
        }
        let speed_add = s.current_move_acc(i) * s.move_acc(i) * acc_mul * dt;
        if speed_add > 0.0 && s.move_speed(i) < max_pos {
            s.set_move_speed(i, s.move_speed(i) + speed_add);
            if s.move_speed(i) > max_pos {
                s.set_move_speed(i, max_pos);
            }
        } else if speed_add < 0.0 && s.move_speed(i) > max_neg {
            s.set_move_speed(i, s.move_speed(i) + speed_add);
            if s.move_speed(i) < max_neg {
                s.set_move_speed(i, max_neg);
            }
        }
        s.set_current_move_acc(i, 0.0);
    }

    let mut pos_add = v3(0.0, 0.0, 0.0);
    let fwd = s.forward();
    let a = mul3s(fwd, s.move_speed(DIR_FORWARD));
    pos_add = mul_add3(pos_add, a, dt);
    let right = s.right();
    let b = mul3s(right, s.move_speed(DIR_RIGHT));
    pos_add = mul_add3(pos_add, b, dt);
    pos_add
}

fn align_pos_add(s: &S, pos_add: &mut Vector3f) {
    if !s.is_on_ground() || s.velocity().y > 0.0 {
        return;
    }
    let n = s.last_ground_normal();
    if sqr_length(n) < 0.01 {
        return;
    }
    let body_forward = normalized(*pos_add);
    let right = cross3(body_forward, n);
    let forward = cross3(n, right);
    *pos_add = mul3s(forward, len3(*pos_add));
}

fn check_collision<H: CharHost>(h: &H, s: &S, push: &mut Vector3f, pos: Vector3f, cb: i32) -> bool {
    h.check_collision(push, pos, cb, s.current_shape_idx())
}

fn check_fits<H: CharHost>(h: &H, s: &S, pos: Vector3f) -> bool {
    let mut push = v3(0.0, 0.0, 0.0);
    check_collision(h, s, &mut push, pos, CB_NONE);
    abs_is_zero(push)
}

/// Reflect one move-speed axis against the push-back normal while airborne.
fn deflect_speed(s: &S, dir: usize, vel: Vector3f, normal: Vector3f) {
    if s.move_speed(dir) == 0.0 {
        return;
    }
    let vel = sub_mul3(vel, normal, dot3(normal, vel));
    let speed = len3(vel);
    if s.move_speed(dir) > 0.0 {
        if s.move_speed(dir) > speed {
            s.set_move_speed(dir, speed);
        }
    } else if s.move_speed(dir) < speed {
        s.set_move_speed(dir, -speed);
    }
}

fn check_move_collision<H: CharHost>(h: &H, s: &S, pos_add: Vector3f, dt: f32, hpl3: bool) {
    let moved = pos_add.x != 0.0 || pos_add.y != 0.0 || pos_add.z != 0.0;
    let cb = if moved { CB_PUSH } else { CB_NONE };
    s.set_position(add3(s.position(), pos_add));
    let mut push = v3(0.0, 0.0, 0.0);
    let collide = check_collision(h, s, &mut push, s.position(), cb);
    if collide {
        s.set_position(add3(s.position(), push));
        if !s.is_on_ground() && sqr_length(push) != 0.0 {
            let normal = normalized(push);
            let fwd_vel = mul3s(s.forward(), s.move_speed(DIR_FORWARD));
            let right_vel = mul3s(s.right(), s.move_speed(DIR_RIGHT));
            deflect_speed(s, DIR_FORWARD, fwd_vel, normal);
            deflect_speed(s, DIR_RIGHT, right_vel, normal);
        }
    }
    check_step_climbing(h, s, pos_add, collide, push, dt, hpl3);
}

fn check_step_climbing<H: CharHost>(
    h: &H,
    s: &S,
    pos_add: Vector3f,
    collided: bool,
    push: Vector3f,
    dt: f32,
    hpl3: bool,
) {
    if s.check_step_climb_count() > 0.0 {
        return;
    }
    if sqr_length(pos_add) < EPSILON {
        return;
    }
    if hpl3 && !collided {
        return;
    }
    let radius = h.shape_radius();
    let forward_add = max(len3(pos_add), 0.05);
    let move_dir = normalized(pos_add);
    let accurate = s.accurate_climbing();
    let rays = if accurate { 3 } else { 1 };

    let mut step_add = [v3(0.0, 0.0, 0.0); 3];
    step_add[0] = mul3s(move_dir, radius + forward_add);
    if accurate {
        let comb = |dir: Vector3f| {
            let b = mul3s(move_dir, forward_add);
            v3(
                fma(dir.x, radius, b.x),
                fma(dir.y, radius, b.y),
                fma(dir.z, radius, b.z),
            )
        };
        step_add[1] = comb(matrix_mul_vec(&rotate_y(PI4), move_dir));
        step_add[2] = comb(matrix_mul_vec(&rotate_y(-PI4), move_dir));
    }

    let size = s.size();
    let mut cast = [(false, 0.0f32, v3(0.0, 0.0, 0.0)); 3];
    for (slot, add) in cast.iter_mut().zip(step_add).take(rays) {
        let start = add3(s.position(), add);
        let end = sub3(start, v3(0.0, size.y / 2.0, 0.0));
        let (mut d, mut n) = (0.0, v3(0.0, 0.0, 0.0));
        *slot = (h.cast_ray(start, end, &mut d, &mut n), d, n);
    }

    let firm = s.on_ground_count() > s.max_on_ground_count() - 4;
    let max_height = if firm || s.climbing() {
        s.max_step_height()
    } else {
        s.max_step_height_in_air()
    };
    let mut min_height = 0.025;
    if hpl3 {
        if max_height <= 0.0 {
            s.set_check_step_climb_count(s.check_step_climb_interval());
            s.set_climbing(false);
        }
        if len3(pos_add) * 0.75 > len3(add3(pos_add, push)) {
            min_height = 0.0;
        }
    }

    for &(hit, dist, normal) in cast.iter().take(rays) {
        if !hit {
            continue;
        }
        let mut height = size.y / 2.0 - dist;
        if hpl3 {
            height = max(height, 0.0);
        }
        if height <= max_height && height > min_height {
            let lift = v3(0.0, height + s.climb_height_add(), 0.0);
            let fa = mul3s(move_dir, forward_add);
            let base = add3(s.position(), lift);
            let cfm = s.climb_forward_mul();
            let step_pos = v3(
                fma(fa.x, cfm, base.x),
                fma(fa.y, cfm, base.y),
                fma(fa.z, cfm, base.z),
            );
            if check_fits(h, s, step_pos) {
                let mut p = s.position();
                p.y = fma(s.step_climb_speed(), dt, p.y);
                s.set_position(p);
                s.set_climbing(true);
                if hpl3 && normal.y > 0.2 {
                    s.set_last_ground_normal(normal);
                }
                break;
            }
        }
    }
    s.set_check_step_climb_count(s.check_step_climb_interval());
}

fn update_forces<H: CharHost>(h: &H, s: &S, dt: f32) {
    if s.gravity_active() && !s.climbing() {
        let g = if s.custom_gravity_active() {
            s.custom_gravity()
        } else {
            h.world_gravity()
        };
        s.set_velocity(mul_add3(s.velocity(), g, dt));
        let v = s.velocity();
        let length = len3(v);
        if length > s.max_gravity_speed() {
            s.set_velocity(mul3s(div3(v, length), s.max_gravity_speed()));
        }
    }
    let scale = dt * (1.0 / s.mass());
    s.set_velocity(mul_add3(s.velocity(), s.force(), scale));
    s.set_force(v3(0.0, 0.0, 0.0));
}

fn update_friction(s: &S, dt: f32) {
    let friction = if s.is_on_ground() {
        s.ground_friction()
    } else {
        s.air_friction()
    };
    let v = s.velocity();
    let mut vel_xz = v3(v.x, 0.0, v.z);
    let mut speed = len3(vel_xz);
    normalize3(&mut vel_xz);
    speed = fma(-friction, dt, speed);
    if speed < 0.0 {
        speed = 0.0;
    }
    let mut v = s.velocity();
    v.x = vel_xz.x * speed;
    v.z = vel_xz.z * speed;
    s.set_velocity(v);

    let g = s.gravity_attachment_velocity();
    let mut speed = len3(g);
    if speed > 0.0 {
        let dir = div3(g, speed);
        speed = fma(-s.air_friction(), dt, speed);
        if speed < 0.0 {
            speed = 0.0;
        }
        s.set_gravity_attachment_velocity(mul3s(dir, speed));
    }
}

fn check_force_collision<H: CharHost>(h: &H, s: &S, dt: f32) {
    let vel = s.velocity();
    // gcc fuses the first product of a sum of two products
    let g = s.gravity_attachment_velocity();
    let pos_add = v3(
        fma(vel.x, dt, g.x * dt),
        fma(vel.y, dt, g.y * dt),
        fma(vel.z, dt, g.z * dt),
    );
    let mut new_vel = v3(0.0, 0.0, 0.0);

    if pos_add.x != 0.0 || pos_add.z != 0.0 {
        let mut p = s.position();
        p.x += pos_add.x;
        p.z += pos_add.z;
        s.set_position(p);
        let mut push = v3(0.0, 0.0, 0.0);
        let collide = check_collision(h, s, &mut push, s.position(), CB_PUSH);
        if collide && !abs_is_zero(push) {
            s.set_position(add3(s.position(), push));
            let normal = normalized(push);
            let vel = s.velocity();
            let xz = v3(vel.x, 0.0, vel.z);
            let add = sub_mul3(xz, normal, dot3(normal, xz));
            new_vel = add3(new_vel, add);
            let mut g = s.gravity_attachment_velocity();
            g.x = 0.0;
            g.z = 0.0;
            s.set_gravity_attachment_velocity(g);
        } else {
            let vel = s.velocity();
            new_vel.x += vel.x;
            new_vel.z += vel.z;
        }
    }

    if pos_add.y != 0.0 {
        let old_pos = s.position();
        let mut p = s.position();
        p.y += pos_add.y;
        s.set_position(p);
        let mut push = v3(0.0, 0.0, 0.0);
        let collide = check_collision(h, s, &mut push, s.position(), CB_GRAVITY);
        if collide && !abs_is_zero(push) {
            h.on_hit_ground();
            let mut gn = normalized(push);
            if gn.y <= 0.2 {
                gn = v3(0.0, 1.0, 0.0);
            }
            s.set_last_ground_normal(gn);
            let mut g = s.gravity_attachment_velocity();
            g.y = 0.0;
            s.set_gravity_attachment_velocity(g);

            if s.velocity().y != 0.0 && normalized(push).y >= s.ground_angle_min_cos() {
                s.set_on_ground_count(s.max_on_ground_count());
            } else {
                s.set_on_ground_count(s.on_ground_count().wrapping_sub(1).max(0));
            }
            s.set_position(add3(s.position(), push));

            let mut normal = normalized(push);
            let inv_gravity = if s.custom_gravity_active() {
                normalized(mul3s(s.custom_gravity(), -1.0))
            } else {
                v3(0.0, 1.0, 0.0)
            };
            if dot3(normal, inv_gravity) >= s.max_no_slide_slope_cos() {
                normal = inv_gravity;
            }
            let y_vel = v3(0.0, s.velocity().y, 0.0);
            let add = sub_mul3(y_vel, normal, dot3(normal, y_vel));
            new_vel = add3(new_vel, add);
            if len3(add).abs() < 0.0001 {
                s.set_position(old_pos);
            }
        } else {
            s.set_on_ground_count(s.on_ground_count().wrapping_sub(1).max(0));
            new_vel.y += s.velocity().y;
            if s.on_ground_count() > 0 && !s.climbing() {
                let size = s.size();
                let p = s.position();
                let feet = sub3(p, v3(0.0, size.y / 2.0, 0.0));
                let start = add3(feet, v3(0.0, 0.001, 0.0));
                let end = v3(start.x - 0.0, fma(-size.x, 2.0001, start.y), start.z - 0.0);
                let (mut d, mut n) = (0.0, v3(0.0, 0.0, 0.0));
                if h.cast_ray(start, end, &mut d, &mut n) && n.y > 0.2 {
                    s.set_last_ground_normal(n);
                } else {
                    s.set_last_ground_normal(v3(0.0, 1.0, 0.0));
                }
            }
        }
    }
    s.set_velocity(new_vel);
}

/// `CheckCharacterFits`: `idx` is already resolved to a shape; returns `(fits, push_back)`.
pub fn check_character_fits<H: CharHost>(
    h: &H,
    pos: Vector3f,
    feet: bool,
    idx: i32,
    epsilon: f32,
) -> (bool, Vector3f) {
    let mut p = pos;
    if feet {
        p = add3(p, v3(0.0, h.shape_size_y(idx) / 2.0, 0.0));
    }
    let mut push = v3(0.0, 0.0, 0.0);
    h.check_collision(&mut push, p, CB_NONE, idx);
    let a = v3(push.x.abs(), push.y.abs(), push.z.abs());
    let fits = if epsilon == 0.0 {
        a.x == epsilon && a.y == epsilon && a.z == epsilon
    } else {
        // cVector3::operator< is lexicographic
        if a.x != epsilon {
            a.x < epsilon
        } else if a.y != epsilon {
            a.y < epsilon
        } else {
            a.z < epsilon
        }
    };
    (fits, push)
}
