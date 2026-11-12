//! `iLuxProp_CritterBase`, `cLuxCritter_Bug`, `cLuxCritter_Spider`: critter parameters, steering,
//! collision response, mesh basis and the spider/bug state machines. Engine queries come through
//! `Query`; sounds, animations, the attack and the virtual shape-collision hook leave as ops.

use super::ops::{Op, Ops, Pod};
use super::vars::{put_var, zero3};
use super::vars2::Vars2;
use crate::math::{
    TWO_PI, dot3, fma, len3, matrix_mul, matrix_mul_vec, matrix_quaternion, matrix_rotate,
    matrix_scale, normalize3, quat_from_angle_axis, rand_rectf, to_rad, v3,
};
use crate::math::{cross3, matrix_unit_vectors};
use crate::types::{Matrixf, Quaternion, Vector2f, Vector3f};

pub const SLOT_DEATH_ENTITY: i32 = 80;
pub const SLOT_DEATH_PS: i32 = 81;
pub const SLOT_DEATH_SOUND: i32 = 82;
pub const SLOT_ATTACK_HIT_SOUND: i32 = 83;
pub const SLOT_BUG_NORMAL_SOUND: i32 = 84;
pub const SLOT_BUG_SCARED_SOUND: i32 = 85;
pub const SLOT_SPIDER_HIT_SOUND: i32 = 86;
pub const SLOT_SPIDER_IDLE_SOUND: i32 = 87;
pub const SLOT_SPIDER_ATTACK_SOUND: i32 = 88;

pub mod cop {
    pub const SOUND: i32 = 100;
    pub const ATTACK: i32 = 101;
    pub const ANIM_FADE: i32 = 102;
    pub const ANIM_SPEED: i32 = 103;
    pub const SET_BODY_POS: i32 = 104;
    pub const SHAPE_COLLISION: i32 = 105;

    pub const SND_SPIDER_HIT: i32 = 0;
    pub const SND_SPIDER_IDLE: i32 = 1;
    pub const SND_SPIDER_ATTACK: i32 = 2;
    pub const SND_BUG_NORMAL: i32 = 3;
    pub const SND_BUG_SCARED: i32 = 4;

    pub const ANIM_ATTACK: i32 = 0;
    pub const ANIM_WALK: i32 = 1;
    pub const ANIM_IDLE: i32 = 2;

    pub const Q_PLAYER_FEET: i32 = 1;
    pub const Q_PLAYER_POS: i32 = 2;
    pub const Q_PLAYER_DEAD: i32 = 3;
    pub const Q_BODY_WORLD: i32 = 4;
    pub const Q_BODY_LOCAL: i32 = 5;
    pub const Q_BODY_SIZE: i32 = 6;
    pub const Q_SHAPE_RADIUS: i32 = 7;
    pub const Q_LANTERN: i32 = 8;
    pub const Q_RAY_RESET: i32 = 9;
    pub const Q_RAY_CAST: i32 = 10;
    pub const Q_RAY_HIT: i32 = 11;
    pub const Q_RAY_NORMAL: i32 = 12;
    pub const Q_RAY_MASS: i32 = 13;
    pub const Q_SHAPE_COLLIDE: i32 = 14;
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Reply {
    pub ok: i32,
    pub v: Vector3f,
    pub f: f32,
}

/// Engine queries a critter makes mid-update. They never call back into a prop.
pub trait Query {
    fn ask(&mut self, code: i32, a: Vector3f, b: Vector3f, flags: i32) -> Reply;
}

fn q3(q: &mut dyn Query, code: i32) -> Vector3f {
    q.ask(code, zero3(), zero3(), 0).v
}

fn qb(q: &mut dyn Query, code: i32) -> bool {
    q.ask(code, zero3(), zero3(), 0).ok != 0
}

fn qf(q: &mut dyn Query, code: i32) -> f32 {
    q.ask(code, zero3(), zero3(), 0).f
}

fn add(a: Vector3f, b: Vector3f) -> Vector3f {
    v3(a.x + b.x, a.y + b.y, a.z + b.z)
}

fn sub(a: Vector3f, b: Vector3f) -> Vector3f {
    v3(a.x - b.x, a.y - b.y, a.z - b.z)
}

fn scale(a: Vector3f, s: f32) -> Vector3f {
    v3(a.x * s, a.y * s, a.z * s)
}

fn add_s(a: Vector3f, s: f32) -> Vector3f {
    v3(a.x + s, a.y + s, a.z + s)
}

/// `a + b * s`, fused as gcc does for `a += b * s`.
fn add_scaled(a: Vector3f, b: Vector3f, s: f32) -> Vector3f {
    v3(fma(b.x, s, a.x), fma(b.y, s, a.y), fma(b.z, s, a.z))
}

/// `a - b * s`, fused as gcc does for `a - b * s`.
fn sub_scaled(a: Vector3f, b: Vector3f, s: f32) -> Vector3f {
    v3(fma(-b.x, s, a.x), fma(-b.y, s, a.y), fma(-b.z, s, a.z))
}

fn vector3_max_length(v: Vector3f, max: f32) -> Vector3f {
    let l = len3(v);
    if l > max {
        scale(v3(v.x / l, v.y / l, v.z / l), max)
    } else {
        v
    }
}

/// `n * s - v`, the wanted-velocity difference gcc fuses.
fn scaled_minus(n: Vector3f, s: f32, v: Vector3f) -> Vector3f {
    v3(fma(n.x, s, -v.x), fma(n.y, s, -v.y), fma(n.z, s, -v.z))
}

fn normalized(v: Vector3f) -> Vector3f {
    let mut v = v;
    normalize3(&mut v);
    v
}

fn dist_sqr(a: Vector3f, b: Vector3f) -> f32 {
    let dx = b.x - a.x;
    let dy = b.y - a.y;
    let dz = b.z - a.z;
    fma(dz, dz, fma(dx, dx, dy * dy))
}

fn dist(a: Vector3f, b: Vector3f) -> f32 {
    dist_sqr(a, b).sqrt()
}

fn is_zero(v: Vector3f) -> bool {
    v.x == 0.0 && v.y == 0.0 && v.z == 0.0
}

fn rotate_y(a: f32) -> Matrixf {
    crate::math::matrix_rotate(v3(0.0, a, 0.0), 0)
}

const PI: f32 = crate::math::PI;
const MAX_FWD: usize = 24;
const MAX_UP: usize = 44;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct CritterParams {
    pub attack_size_radius: f32,
    pub attack_offset: Vector3f,
    pub attack_damage: Vector2f,
    pub attack_force: Vector2f,
    pub attack_strength: i32,
    pub mesh_offset: Matrixf,
}

/// `iLuxPropLoader_Critter::LoadVariables`.
pub fn load(v: &dyn Vars2, out: &mut CritterParams) {
    put_var(v, SLOT_DEATH_ENTITY, "DeathEntity");
    put_var(v, SLOT_DEATH_PS, "DeathPS");
    put_var(v, SLOT_DEATH_SOUND, "DeathSound");
    out.attack_size_radius = v.float("AttackSizeRadius", 0.0);
    out.attack_offset = v.vec3("AttackOffset", zero3());
    out.attack_damage = v.vec2("AttackDamageMinMax", Vector2f::default());
    out.attack_force = v.vec2("AttackForce", Vector2f::default());
    out.attack_strength = v.int("AttackStrength", 0);
    put_var(v, SLOT_ATTACK_HIT_SOUND, "AttackHitSound");
    let pos = v.vec3("MeshOffsetPos", zero3());
    let r = v.vec3("MeshOffsetRot", zero3());
    let rot = v3(to_rad(r.x), to_rad(r.y), to_rad(r.z));
    let sc = v.vec3("MeshOffsetScale", v3(1.0, 1.0, 1.0));
    let mut m = matrix_mul(&matrix_rotate(rot, 0), &matrix_scale(sc));
    m.m[0][3] = pos.x;
    m.m[1][3] = pos.y;
    m.m[2][3] = pos.z;
    out.mesh_offset = m;
}

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct CritterState {
    pub vel: Vector3f,
    pub gravity_vel: Vector3f,
    pub ground_normal: Vector3f,
    pub swarm_point: Vector3f,
    pub anim_state: i32,
    pub use_ray: bool,
    pub colliding: bool,
    pub update_animation: bool,
    pub causes_sanity: bool,
    pub n_fwd: i32,
    pub n_up: i32,
    pub fwd: [Vector3f; MAX_FWD],
    pub up: [Vector3f; MAX_UP],
}

impl Default for CritterState {
    fn default() -> Self {
        Self::new()
    }
}

impl CritterState {
    pub fn new() -> Self {
        Self {
            vel: zero3(),
            gravity_vel: zero3(),
            ground_normal: v3(0.0, 1.0, 0.0),
            swarm_point: zero3(),
            anim_state: 0,
            use_ray: true,
            colliding: false,
            update_animation: true,
            causes_sanity: false,
            n_fwd: 0,
            n_up: 0,
            fwd: [zero3(); MAX_FWD],
            up: [zero3(); MAX_UP],
        }
    }

    fn fwd_len(&self) -> usize {
        usize::try_from(self.n_fwd).map_or(0, |n| n.min(MAX_FWD))
    }

    fn up_len(&self) -> usize {
        usize::try_from(self.n_up).map_or(0, |n| n.min(MAX_UP))
    }

    fn fwd_back(&self) -> Option<Vector3f> {
        self.fwd.get(self.fwd_len().checked_sub(1)?).copied()
    }

    fn push_fwd(&mut self, v: Vector3f) {
        push_ring(&mut self.fwd, &mut self.n_fwd, v, 20);
    }

    fn push_up(&mut self, v: Vector3f) {
        push_ring(&mut self.up, &mut self.n_up, v, 40);
    }
}

/// `list.push_back(v); if(size > cap) list.pop_front();`
fn push_ring(buf: &mut [Vector3f], n: &mut i32, v: Vector3f, cap: usize) {
    let len = usize::try_from(*n).map_or(0, |n| n.min(buf.len().saturating_sub(1)));
    if let Some(slot) = buf.get_mut(len) {
        *slot = v;
    }
    let mut len = len + 1;
    if len > cap {
        buf.copy_within(1..len, 0);
        len -= 1;
    }
    *n = i32::try_from(len).unwrap_or(0);
}

/// `OnSetupAfterLoad`: the ground normal and the first forward direction, which `CreateOrthoVectors`
/// rewrites before it is pushed.
pub fn setup_dirs(s: &mut CritterState, ground: Vector3f, forward: Vector3f) {
    s.ground_normal = ground;
    let (_, fwd) = create_ortho(ground, forward);
    s.push_fwd(fwd);
}

/// `CreateOrthoVectors`: `(right, forward)` for the up vector and the forward hint.
pub fn create_ortho(up: Vector3f, forward: Vector3f) -> (Vector3f, Vector3f) {
    let mut fwd = forward;
    if fwd == up || fwd == scale(up, -1.0) {
        fwd.x = up.z;
        fwd.y = -up.x;
        fwd.z = up.x;
    }
    let mut right = cross3(up, fwd);
    normalize3(&mut right);
    let mut f = cross3(right, up);
    normalize3(&mut f);
    (right, f)
}

/// Is the player close enough (15 m) for the critter to update?
pub fn in_range(player_feet: Vector3f, body: Vector3f) -> bool {
    dist_sqr(player_feet, body) <= 15.0 * 15.0
}

fn dir_of_vel_or_random(s: &CritterState) -> Vector3f {
    if is_zero(s.vel) {
        match s.fwd_back() {
            None => {
                let a = rand_rectf(0.0, TWO_PI);
                matrix_mul_vec(&rotate_y(a), v3(1.0, 0.0, 0.0))
            }
            Some(f) => f,
        }
    } else {
        normalized(s.vel)
    }
}

/// `GetWanderAdd`.
pub fn wander_add(s: &CritterState, dt: f32) -> Vector3f {
    scale(wander_sum(s), dt)
}

/// `GetWanderAdd` before the time step: `vDir + vForce`.
fn wander_sum(s: &CritterState) -> Vector3f {
    let d = dir_of_vel_or_random(s);
    let n = s.ground_normal;
    let k = dot3(n, d);
    let d = sub_scaled(d, n, k);
    let a = rand_rectf(0.0, TWO_PI);
    let mut q = Quaternion::default();
    quat_from_angle_axis(&mut q, a, n);
    let force = matrix_mul_vec(&matrix_quaternion(&q), v3(2.0, 0.0, 0.0));
    add(d, force)
}

/// `GetTowardPlayerAdd`.
pub fn toward_player_add(
    s: &CritterState,
    q: &mut dyn Query,
    depend_on_distance: bool,
    dt: f32,
) -> Vector3f {
    let feet = q3(q, cop::Q_PLAYER_FEET);
    let local = q3(q, cop::Q_BODY_LOCAL);
    let mut to = sub(feet, local);
    let d = len3(to);
    normalize3(&mut to);
    let n = s.ground_normal;
    let k = dot3(n, to);
    to = sub_scaled(to, n, k);
    let mut add = scale(to, dt);
    if depend_on_distance {
        add = scale(add, 1.0f32 / d);
    }
    add
}

/// `UpdateMovement`.
pub fn update_movement(s: Pod<CritterState>, q: &mut dyn Query, ops: &mut Ops, dt: f32) {
    if is_zero(s.r().vel) {
        return;
    }
    let mv = Move {
        vel: s.r().vel,
        grav: s.r().gravity_vel,
        dt,
    };
    if s.r().use_ray {
        check_ray(s, q, ops, mv);
    } else {
        check_shape(s, q, ops, mv);
    }
}

/// The velocities of one update.
#[derive(Clone, Copy)]
struct Move {
    vel: Vector3f,
    grav: Vector3f,
    dt: f32,
}

impl Move {
    fn vel_add(&self) -> Vector3f {
        scale(self.vel, self.dt)
    }

    fn grav_add(&self) -> Vector3f {
        scale(self.grav, self.dt)
    }
}

fn ray(q: &mut dyn Query, from: Vector3f, to: Vector3f, flags: i32) {
    q.ask(cop::Q_RAY_CAST, from, to, flags);
}

fn check_ray(s: Pod<CritterState>, q: &mut dyn Query, ops: &mut Ops, mv: Move) {
    let vel_add = mv.vel_add();
    let grav_add = mv.grav_add();
    let mut pos = q3(q, cop::Q_BODY_WORLD);
    if !is_zero(vel_add) {
        q.ask(cop::Q_RAY_RESET, zero3(), zero3(), 0);
        let dir = normalized(vel_add);
        let size = q3(q, cop::Q_BODY_SIZE);
        let t = scale(dir, size.x);
        let to = add(add(pos, vel_add), scale(t, 0.5));
        ray(q, pos, to, 0);
        if qb(q, cop::Q_RAY_HIT) {
            let mut n = q3(q, cop::Q_RAY_NORMAL);
            n.y = 0.0;
            normalize3(&mut n);
            let vel = s.r().vel;
            let k = dot3(n, vel);
            let mut nv = sub_scaled(vel, n, k);
            nv.y = 0.0;
            s.m().vel = nv;
        } else {
            pos = add(pos, vel_add);
        }
    }
    if !is_zero(grav_add) {
        q.ask(cop::Q_RAY_RESET, zero3(), zero3(), 0);
        let size = q3(q, cop::Q_BODY_SIZE);
        ray(q, pos, add(add_s(pos, size.y * -0.5), grav_add), 0);
        if qb(q, cop::Q_RAY_HIT) {
            s.m().gravity_vel = zero3();
        } else {
            pos = add(pos, grav_add);
        }
    }
    ops.push(Op::new(cop::SET_BODY_POS).v(pos));
}

fn check_shape(s: Pod<CritterState>, q: &mut dyn Query, ops: &mut Ops, mv: Move) {
    let dt = mv.dt;
    let mut pos = q3(q, cop::Q_BODY_WORLD);
    s.m().colliding = false;
    if !is_zero(mv.vel_add()) {
        pos = add(pos, mv.vel_add());
        let r = q.ask(cop::Q_SHAPE_COLLIDE, pos, zero3(), 0);
        if r.ok != 0 {
            s.m().colliding = true;
            let push = r.v;
            if !is_zero(push) {
                pos = add(pos, push);
                let dir = normalized(push);
                ops.event(Op::new(cop::SHAPE_COLLISION).v(push).f(dt));
                let vel = s.r().vel;
                let k = dot3(dir, vel);
                s.m().vel = sub_scaled(vel, dir, k);
            }
        }
    }
    if !is_zero(mv.grav_add()) {
        pos = add(pos, mv.grav_add());
        let r = q.ask(cop::Q_SHAPE_COLLIDE, pos, zero3(), 0);
        if r.ok != 0 {
            s.m().colliding = true;
            let push = r.v;
            if !is_zero(push) {
                pos = add(pos, push);
                let dir = normalized(push);
                ops.event(Op::new(cop::SHAPE_COLLISION).v(push).f(dt));
                let g = s.r().gravity_vel;
                let k = dot3(dir, g);
                let g = sub_scaled(g, dir, k);
                let n = s.r().ground_normal;
                s.m().gravity_vel = scale(n, dot3(g, n));
            }
        }
    }
    ops.push(Op::new(cop::SET_BODY_POS).v(pos));
}

/// The part of `UpdateMesh` before the matrices: direction history and the rotation basis.
/// Returns `rotation^T` (right, up, forward as columns).
pub fn mesh_basis(s: &mut CritterState) -> Matrixf {
    if !is_zero(s.vel) {
        let d = normalized(s.vel);
        s.push_fwd(d);
    }
    let mut forward = v3(0.0, 0.0, 1.0);
    let n = s.fwd_len();
    if n > 0 {
        let mut t = zero3();
        for v in s.fwd.iter().take(n) {
            t = add(t, *v);
        }
        forward = v3(t.x / n as f32, t.y / n as f32, t.z / n as f32);
    }
    let g = s.ground_normal;
    s.push_up(g);
    let n = s.up_len();
    let mut t = zero3();
    for v in s.up.iter().take(n) {
        t = add(t, *v);
    }
    let up = v3(t.x / n as f32, t.y / n as f32, t.z / n as f32);
    let (right, forward) = create_ortho(up, forward);
    matrix_unit_vectors(right, up, forward, zero3())
}

/// `UpdateMesh` for a skeletal mesh: the whole mesh matrix.
pub fn mesh_matrix_skeleton(rot_t: &Matrixf, offset: &Matrixf) -> Matrixf {
    matrix_mul(&matrix_mul(rot_t, &rotate_y(PI)), offset)
}

/// `UpdateMesh` for a static mesh: the matrix before the sub mesh base matrix.
pub fn mesh_matrix_static(rot_t: &Matrixf, offset: &Matrixf) -> Matrixf {
    matrix_mul(rot_t, offset)
}

/// The walk/idle animation switch of `UpdateMesh`.
pub fn anim_step(s: &mut CritterState, ops: &mut Ops) {
    let speed = len3(s.vel);
    if s.anim_state == 0 {
        if f64::from(speed) > 0.1 {
            ops.push(Op::new(cop::ANIM_FADE).i(cop::ANIM_WALK));
            s.anim_state = 1;
        }
    } else if s.anim_state == 1 {
        ops.push(Op::new(cop::ANIM_SPEED).f(speed));
        if speed < 0.05 {
            ops.push(Op::new(cop::ANIM_FADE).i(cop::ANIM_IDLE));
            s.anim_state = 0;
        }
    }
}

// ---- bug ----

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct BugParams {
    pub max_speed: f32,
    pub max_speed_normal: f32,
    pub max_speed_afraid: f32,
    pub gravity: f32,
    pub flee_distance: f32,
    pub flee_mul: f32,
    pub back_to_spawn_mul: f32,
    pub wander_radius: f32,
    pub wander_dist: f32,
    pub random_pauses: bool,
    pub pause_time: Vector2f,
    pub move_time: Vector2f,
    pub normal_sound_rand: Vector2f,
    pub scared_sound_rand: Vector2f,
}

pub fn load_bug(v: &dyn Vars2, out: &mut BugParams) {
    out.max_speed_normal = v.float("MaxSpeedNormal", 0.0);
    out.max_speed_afraid = v.float("MaxSpeedAfraid", 0.0);
    out.max_speed = v.float("MaxSpeed", 0.0);
    out.gravity = v.float("Gravity", 0.0);
    out.flee_distance = v.float("PlayerFleeDistance", 0.0);
    out.flee_mul = v.float("FleeMul", 0.0);
    out.back_to_spawn_mul = v.float("BackToSpawnPointMul", 0.0);
    out.wander_radius = v.float("WanderCircleRadius", 0.0);
    out.wander_dist = v.float("WanderCircleDist", 0.0);
    out.random_pauses = v.boolean("HasRandomPauses", false);
    out.pause_time = v.vec2("RandomPauseTimeMinMax", Vector2f { x: 1.0, y: 3.0 });
    out.move_time = v.vec2("RandomMoveTimeMinMax", Vector2f { x: 2.0, y: 5.0 });
    put_var(v, SLOT_BUG_NORMAL_SOUND, "NormalSound");
    out.normal_sound_rand = v.vec2("NormalSoundRandMinMax", Vector2f::default());
    put_var(v, SLOT_BUG_SCARED_SOUND, "ScaredSound");
    out.scared_sound_rand = v.vec2("ScaredSoundRandMinMax", Vector2f::default());
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct BugState {
    pub play_sound_count: f32,
    pub pause_count: f32,
    pub paused: bool,
    pub max_speed: f32,
}

impl BugState {
    pub fn new() -> Self {
        let play_sound_count = rand_rectf(1.0, 5.0);
        let pause_count = rand_rectf(1.0, 3.0);
        Self {
            play_sound_count,
            pause_count,
            paused: false,
            max_speed: 0.0,
        }
    }
}

/// `cLuxCritter_Bug::UpdateVelocity`.
pub fn bug_update_velocity(
    c: Pod<CritterState>,
    b: Pod<BugState>,
    p: &BugParams,
    q: &mut dyn Query,
    ops: &mut Ops,
    dt: f32,
) {
    let mut scared = false;
    let mut player = q3(q, cop::Q_PLAYER_FEET);
    let body = q3(q, cop::Q_BODY_WORLD);
    let player_dist_sqr = dist_sqr(player, body);
    if player_dist_sqr < p.flee_distance * p.flee_distance {
        scared = true;
        b.m().paused = false;
    }
    if p.random_pauses && !scared {
        b.m().pause_count -= dt;
        if b.r().pause_count < 0.0 {
            if b.r().paused {
                b.m().paused = false;
                b.m().pause_count = rand_rectf(p.move_time.x, p.move_time.y);
            } else {
                b.m().paused = true;
                b.m().pause_count = rand_rectf(p.pause_time.x, p.pause_time.y);
            }
        }
    }
    if !b.r().paused {
        let dir = dir_of_vel_or_random(c.r());
        let a = rand_rectf(0.0, TWO_PI);
        let force = matrix_mul_vec(&rotate_y(a), v3(p.wander_radius, 0.0, 0.0));
        let cur = c.r().vel;
        let wd = p.wander_dist;
        let add_v = v3(
            fma(dir.x, wd, force.x),
            fma(dir.y, wd, force.y),
            fma(dir.z, wd, force.z),
        );
        c.m().vel = add_scaled(cur, add_v, dt);
        if scared {
            let mut pd = player_dist_sqr.sqrt();
            if pd == 0.0 {
                pd = 0.0001;
            }
            player.y = body.y;
            let vel = c.r().vel;
            let d = scaled_minus(normalized(sub(body, player)), b.r().max_speed, vel);
            let force = scale(scale(d, p.flee_mul), 1.0f32 / pd);
            c.m().vel = add_scaled(vel, force, dt);
            b.m().max_speed = p.max_speed_afraid;
        } else {
            b.m().max_speed = p.max_speed_normal;
        }
        {
            let swarm = c.r().swarm_point;
            let sd = dist(swarm, body);
            let swarm_pos = v3(swarm.x, body.y, swarm.z);
            let vel = c.r().vel;
            let mut d = scaled_minus(normalized(sub(swarm_pos, body)), b.r().max_speed, vel);
            d.y = 0.0 - vel.y;
            let force = scale(scale(d, 0.2), sd);
            c.m().vel = add_scaled(vel, force, dt);
        }
    } else {
        let vel = c.r().vel;
        c.m().vel = sub_scaled(vel, scale(vel, dt), 3.0);
    }
    b.m().play_sound_count -= dt;
    if b.r().play_sound_count < 0.0 {
        let r = if scared {
            p.scared_sound_rand
        } else {
            p.normal_sound_rand
        };
        b.m().play_sound_count = rand_rectf(r.x, r.y);
        ops.push(Op::new(cop::SOUND).i(if scared {
            cop::SND_BUG_SCARED
        } else {
            cop::SND_BUG_NORMAL
        }));
    }
    let speed = len3(c.r().vel);
    if speed > b.r().max_speed {
        let vel = c.r().vel;
        c.m().vel = scale(
            v3(vel.x / speed, vel.y / speed, vel.z / speed),
            b.r().max_speed,
        );
    }
    let g = c.r().gravity_vel;
    c.m().gravity_vel.y = fma(-p.gravity, dt, g.y);
    let g = c.r().gravity_vel;
    c.m().gravity_vel = vector3_max_length(g, 10.0);
}

// ---- spider ----

pub const ST_IDLE: i32 = 0;
pub const ST_MOVE: i32 = 1;
pub const ST_FLEE: i32 = 2;
pub const ST_HUNT: i32 = 3;
pub const ST_HIT: i32 = 4;
pub const ST_ATTACK_1: i32 = 5;
pub const ST_ATTACK_2: i32 = 6;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct SpiderParams {
    pub max_idle_speed: f32,
    pub max_hunt_speed: f32,
    pub idle_sound_rand: Vector2f,
    pub attack_player_dist: Vector2f,
    pub hunt_player_dist: f32,
    pub hunt_time: Vector2f,
    pub move_time: Vector2f,
    pub idle_time: Vector2f,
}

pub fn load_spider(v: &dyn Vars2, out: &mut SpiderParams) {
    out.max_idle_speed = v.float("MaxIdleSpeed", 0.0);
    out.max_hunt_speed = v.float("MaxHuntSpeed", 0.0);
    put_var(v, SLOT_SPIDER_HIT_SOUND, "HitSound");
    put_var(v, SLOT_SPIDER_IDLE_SOUND, "IdleSound");
    out.idle_sound_rand = v.vec2("IdleSoundRandMinMax", Vector2f::default());
    put_var(v, SLOT_SPIDER_ATTACK_SOUND, "AttackSound");
    out.attack_player_dist = v.vec2("AttackPlayerDistMinMax", Vector2f::default());
    out.hunt_player_dist = v.float("HuntPlayerDist", 0.0);
    out.hunt_time = v.vec2("HuntTimeMinMax", Vector2f::default());
    out.move_time = v.vec2("MoveTimeMinMax", Vector2f::default());
    out.idle_time = v.vec2("IdleTimeMinMax", Vector2f::default());
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct SpiderState {
    pub state: i32,
    pub falling_count: f32,
    pub new_ground_normal_count: f32,
    pub on_ground: bool,
    pub max_speed: f32,
    pub count: f32,
    pub play_sound_count: f32,
}

/// The constructor: base flags and the first sound timer.
pub fn spider_new(c: &mut CritterState, s: &mut SpiderState) {
    c.use_ray = false;
    *s = SpiderState {
        state: ST_IDLE,
        falling_count: 0.0,
        new_ground_normal_count: 0.0,
        on_ground: false,
        max_speed: 0.5,
        count: 0.0,
        play_sound_count: rand_rectf(1.0, 5.0),
    };
    c.causes_sanity = true;
}

/// `OnDamageCritter`.
pub fn spider_on_damage(c: &mut CritterState, s: &mut SpiderState, amount: f32, ops: &mut Ops) {
    if amount <= 2.0 {
        return;
    }
    s.count = 1.0;
    s.state = ST_HIT;
    s.max_speed = 0.1;
    c.ground_normal = v3(0.0, 1.0, 0.0);
    ops.push(Op::new(cop::SOUND).i(cop::SND_SPIDER_HIT));
}

fn spider_begin_attack(
    c: Pod<CritterState>,
    s: Pod<SpiderState>,
    q: &mut dyn Query,
    ops: &mut Ops,
) {
    s.m().state = ST_ATTACK_1;
    ops.push(Op::new(cop::SOUND).i(cop::SND_SPIDER_ATTACK));
    let pp = q3(q, cop::Q_PLAYER_POS);
    let local = q3(q, cop::Q_BODY_LOCAL);
    let dir = normalized(sub(add(pp, v3(0.0, 0.5, 0.0)), local));
    let g = c.r().gravity_vel;
    c.m().gravity_vel = add_scaled(g, dir, 25.0);
    s.m().max_speed = 3.0;
    c.m().update_animation = false;
    ops.push(Op::new(cop::ANIM_FADE).i(cop::ANIM_ATTACK));
    c.m().anim_state = 0;
    s.m().count = 0.8;
    c.m().ground_normal = v3(0.0, 1.0, 0.0);
}

fn dist_to_player(q: &mut dyn Query) -> f32 {
    let p = q3(q, cop::Q_PLAYER_POS);
    let l = q3(q, cop::Q_BODY_LOCAL);
    dist(p, l)
}

fn dist_to_player_2d(q: &mut dyn Query) -> f32 {
    let p = q3(q, cop::Q_PLAYER_FEET);
    let l = q3(q, cop::Q_BODY_LOCAL);
    let dx = p.x - l.x;
    let dz = p.z - l.z;
    fma(dz, dz, dx * dx).sqrt()
}

/// `cLuxCritter_Spider::UpdateVelocity`.
pub fn spider_update_velocity(
    c: Pod<CritterState>,
    s: Pod<SpiderState>,
    p: &SpiderParams,
    q: &mut dyn Query,
    ops: &mut Ops,
    dt: f32,
) {
    let mut add_vel = zero3();
    if s.r().count > 0.0 {
        s.m().count -= dt;
    }
    if s.r().state != ST_FLEE {
        let lantern = q.ask(cop::Q_LANTERN, zero3(), zero3(), 0);
        if lantern.ok != 0 && dist_to_player(q) < lantern.f {
            s.m().state = ST_FLEE;
            s.m().count = rand_rectf(p.move_time.x, p.move_time.y);
        }
    }
    let st = s.r().state;
    if st == ST_IDLE {
        s.m().max_speed = 0.0;
        if s.r().count <= 0.0 {
            s.m().state = ST_MOVE;
            s.m().count = rand_rectf(p.move_time.x, p.move_time.y);
        }
    } else if st == ST_FLEE {
        s.m().max_speed = p.max_idle_speed;
        let t = toward_player_add(c.r(), q, false, dt);
        add_vel = sub(add_vel, t);
        add_vel = add(add_vel, wander_add(c.r(), dt));
        if s.r().count <= 0.0 {
            s.m().state = ST_IDLE;
            s.m().count = rand_rectf(p.idle_time.x, p.idle_time.y);
        }
    } else if st == ST_MOVE {
        s.m().max_speed = p.max_idle_speed;
        add_vel = add(add_vel, wander_add(c.r(), dt));
        {
            let body = q3(q, cop::Q_BODY_WORLD);
            let swarm = c.r().swarm_point;
            let sd = dist(swarm, body);
            let swarm_pos = v3(swarm.x, body.x, swarm.z);
            let vel = c.r().vel;
            let nn = normalized(sub(swarm_pos, body));
            let msp = s.r().max_speed;
            // gcc vectorises x and z differently: only z fuses the wanted-velocity product
            let d = v3(nn.x * msp - vel.x, 0.0 - vel.y, fma(nn.z, msp, -vel.z));
            let force = scale(scale(d, 0.01), sd);
            add_vel = add_scaled(add_vel, force, dt);
        }
        if s.r().count <= 0.0 {
            let d2 = dist_to_player_2d(q);
            if !qb(q, cop::Q_PLAYER_DEAD)
                && d2 < p.attack_player_dist.y
                && d2 > p.attack_player_dist.x
            {
                spider_begin_attack(c, s, q, ops);
            } else {
                s.m().state = ST_IDLE;
                s.m().count = rand_rectf(p.idle_time.x, p.idle_time.y);
            }
        }
    } else if st == ST_HUNT {
        s.m().max_speed = p.max_hunt_speed;
        add_vel = add_scaled(add_vel, toward_player_add(c.r(), q, false, dt), 3.0);
        add_vel = add(add_vel, wander_add(c.r(), dt));
        let d2 = dist_to_player_2d(q);
        if d2 < p.attack_player_dist.y && d2 > p.attack_player_dist.x {
            spider_begin_attack(c, s, q, ops);
        }
        if s.r().count <= 0.0 {
            s.m().state = ST_IDLE;
            s.m().count = rand_rectf(1.0, 3.0);
        }
    } else if st == ST_HIT {
        if s.r().count <= 0.0 {
            s.m().state = ST_IDLE;
        }
    } else if st == ST_ATTACK_1 {
        s.m().max_speed = p.max_hunt_speed;
        add_vel = add(add_vel, toward_player_add(c.r(), q, false, dt));
        if s.r().count <= 0.0 || dist_to_player_2d(q) < 1.5 {
            s.m().state = ST_ATTACK_2;
            c.m().update_animation = true;
            let pp = q3(q, cop::Q_PLAYER_POS);
            let local = q3(q, cop::Q_BODY_LOCAL);
            ops.event(Op::new(cop::ATTACK).v(normalized(sub(pp, local))));
            s.m().count = 0.5;
        }
    } else if st == ST_ATTACK_2 {
        s.m().max_speed = p.max_hunt_speed;
        add_vel = add(add_vel, toward_player_add(c.r(), q, false, dt));
        if s.r().count <= 0.0 {
            s.m().state = ST_IDLE;
        }
    }
    let vel = c.r().vel;
    c.m().vel = add(vel, add_vel);
    let st = s.r().state;
    if st == ST_MOVE || st == ST_HUNT || st == ST_FLEE {
        s.m().play_sound_count -= dt;
        if s.r().play_sound_count < 0.0 {
            s.m().play_sound_count = rand_rectf(p.idle_sound_rand.x, p.idle_sound_rand.y);
            ops.push(Op::new(cop::SOUND).i(cop::SND_SPIDER_IDLE));
        }
    }
    let vel = c.r().vel;
    c.m().vel = vector3_max_length(vel, s.r().max_speed);
}

/// `UpdateGroundCheck`.
pub fn spider_ground_check(c: Pod<CritterState>, s: Pod<SpiderState>, q: &mut dyn Query, dt: f32) {
    s.m().on_ground = false;
    {
        let local = q3(q, cop::Q_BODY_LOCAL);
        let radius = qf(q, cop::Q_SHAPE_RADIUS);
        let n = c.r().ground_normal;
        let start = sub_scaled(local, n, radius - 0.05);
        let end = sub_scaled(start, n, 0.25);
        q.ask(cop::Q_RAY_RESET, zero3(), zero3(), 0);
        ray(q, start, end, 0);
        if qb(q, cop::Q_RAY_HIT) {
            c.m().ground_normal = q3(q, cop::Q_RAY_NORMAL);
            s.m().on_ground = true;
        }
    }
    if !s.r().on_ground || !c.r().colliding {
        let n = c.r().ground_normal;
        let g = c.r().gravity_vel;
        let t = scale(n, 9.8);
        c.m().gravity_vel = sub_scaled(g, t, dt);
        s.m().falling_count += dt;
        if s.r().falling_count > 0.3 {
            c.m().ground_normal = v3(0.0, 1.0, 0.0);
            let g = c.r().gravity_vel;
            let n = c.r().ground_normal;
            c.m().gravity_vel = scale(scale(n, len3(g)), -1.0);
        }
        let g = c.r().gravity_vel;
        c.m().gravity_vel = vector3_max_length(g, 10.0);
    } else {
        s.m().falling_count = 0.0;
    }
}

/// `OnShapeCollision`: may climb onto the surface that was hit.
pub fn spider_on_shape_collision(
    c: Pod<CritterState>,
    s: Pod<SpiderState>,
    q: &mut dyn Query,
    push: Vector3f,
    dt: f32,
) {
    if s.r().state == ST_HIT {
        return;
    }
    if s.r().new_ground_normal_count > 0.0 {
        s.m().new_ground_normal_count -= dt;
        return;
    }
    let radius = qf(q, cop::Q_SHAPE_RADIUS);
    let local = q3(q, cop::Q_BODY_LOCAL);
    let ground = c.r().ground_normal;
    let mut push_dir = scale(normalized(push), -1.0);
    normalize3(&mut push_dir);
    let k = dot3(ground, push_dir);
    push_dir = sub_scaled(push_dir, ground, k);
    let mut start = add_scaled(local, push_dir, radius - 0.05);
    let mut end = add_scaled(start, push_dir, 0.1);
    q.ask(cop::Q_RAY_RESET, zero3(), zero3(), 0);
    ray(q, start, end, 0);
    let intersect;
    if qb(q, cop::Q_RAY_HIT) {
        if qf(q, cop::Q_RAY_MASS) != 0.0 {
            return;
        }
        intersect = q3(q, cop::Q_RAY_NORMAL);
        if is_zero(intersect) {
            return;
        }
        if f64::from(dot3(intersect, ground)) > 8.0 {
            return;
        }
    } else {
        return;
    }
    let test_height = 1.5f32;
    start = sub_scaled(local, push_dir, radius);
    end = add_scaled(start, ground, test_height);
    q.ask(cop::Q_RAY_RESET, zero3(), zero3(), 0);
    ray(q, start, end, 1);
    if qb(q, cop::Q_RAY_HIT) {
        return;
    }
    let test_points = 5i32;
    let mut climb = 0;
    for i in 1..=5 {
        let h = (test_height / test_points as f32) * i as f32;
        start = add_scaled(local, ground, h);
        end = add_scaled(start, push_dir, radius + 0.25);
        ray(q, start, end, 1);
        if qb(q, cop::Q_RAY_HIT) {
            climb += 1;
        }
    }
    if climb < 3 {
        return;
    }
    let old_ground = ground;
    c.m().ground_normal = intersect;
    let vel = c.r().vel;
    let fwd = normalized(vel);
    let mut right = cross3(old_ground, fwd);
    normalize3(&mut right);
    let mut fwd = cross3(right, intersect);
    normalize3(&mut fwd);
    c.m().vel = scale(fwd, len3(vel));
}
