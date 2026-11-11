//! `SomaCritter.cpp`: critter movement step and the `cLuxCritter` Move_* natives' maths.

use super::hw;
use crate::math::{atan2, clamp, dot3, len3, rand_rectf, sin, sub3, v3, vector3_reflect};
use crate::types::Vector3f;

const PI: f32 = std::f32::consts::PI;
const TWO_PI: f32 = std::f32::consts::TAU;

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct CritterState {
    pub flying: bool,
    pub test_collision: bool,
    pub align_to_ground: bool,
    pub can_run_on_walls: bool,
    pub use_ray_collision: bool,
    pub init: bool,
    pub vel: [f32; 3],
    pub wander_angle: f32,
    pub wander_pitch: f32,
    pub yaw: f32,
    pub pitch: f32,
    pub playing_anim_state: i32,
    pub ground_timer: f32,
    pub ground_dist: f32,
    pub wall_timer: f32,
}

impl Default for CritterState {
    fn default() -> Self {
        Self {
            flying: false,
            test_collision: true,
            align_to_ground: false,
            can_run_on_walls: false,
            use_ray_collision: false,
            init: false,
            vel: [0.0; 3],
            wander_angle: 0.0,
            wander_pitch: 0.0,
            yaw: 0.0,
            pitch: 0.0,
            playing_anim_state: -1,
            ground_timer: 0.0,
            ground_dist: -1.0,
            wall_timer: 0.0,
        }
    }
}

/// The script object's critter properties, copied in and out by C++.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct CritterProps {
    pub wanted_vel: [f32; 3],
    pub gravity_vel: [f32; 3],
    pub ground_normal: [f32; 3],
    pub wall_avoid_normal: [f32; 3],
    pub wall_avoid_position: [f32; 3],
    pub max_velocity: f32,
    pub max_turn_speed: f32,
    pub turn_speed_mul: f32,
    pub max_gravity_velocity: f32,
    pub forward_rotation_y_offset: f32,
    pub anim_state: i32,
    pub wall_avoid_detected: bool,
}

pub trait CritterHost {
    /// Nearest body hit along the ray as distance and normal (-1 and zero for none); `None`
    /// when there is no map to cast in.
    fn cast(&mut self, start: Vector3f, end: Vector3f) -> Option<(f32, Vector3f)>;
}

#[derive(Clone, Copy, Debug)]
pub struct Member {
    pub is_self: bool,
    pub pos: Vector3f,
    pub vel: Vector3f,
}

pub fn vec(a: [f32; 3]) -> Vector3f {
    v3(a[0], a[1], a[2])
}

pub fn arr(a: Vector3f) -> [f32; 3] {
    [a.x, a.y, a.z]
}

const ZERO: Vector3f = v3(0.0, 0.0, 0.0);

fn scale(a: Vector3f, k: f32) -> Vector3f {
    v3(a.x * k, a.y * k, a.z * k)
}

pub fn init_state(s: &mut CritterState, fwd_x: f32, fwd_z: f32) {
    s.init = true;
    s.yaw = atan2(fwd_x, fwd_z);
}

pub fn safe_normalize(a: Vector3f) -> Vector3f {
    let len = len3(a);
    if len > 1e-6 {
        v3(a.x / len, a.y / len, a.z / len)
    } else {
        ZERO
    }
}

fn wrap(mut a: f32) -> f32 {
    while a > PI {
        a -= TWO_PI;
    }
    while a < -PI {
        a += TWO_PI;
    }
    a
}

fn turn_towards(from: f32, to: f32, max_step: f32) -> f32 {
    let diff = wrap(to - from);
    from + clamp(diff, -max_step, max_step)
}

fn fmax(a: f32, b: f32) -> f32 {
    if a < b { b } else { a }
}

fn fmin(a: f32, b: f32) -> f32 {
    if b < a { b } else { a }
}

/// `SomaRaycast` minus the host's map lookup; a ray too short to cast gives `None`.
fn raycast(host: &mut dyn CritterHost, a: Vector3f, b: Vector3f) -> Option<(f32, Vector3f)> {
    let d = sub3(b, a);
    if dot3(d, d) < 1e-8 {
        return None;
    }
    host.cast(a, b)
}

/// Result of a step: new position, yaw, pitch and the animation to play (0 none, 1 move, 2 idle).
pub struct Step {
    pub pos: Vector3f,
    pub yaw: f32,
    pub pitch: f32,
    pub play_anim: i32,
}

pub fn update(
    s: &mut CritterState,
    p: &mut CritterProps,
    host: &mut dyn CritterHost,
    pos: Vector3f,
    dt: f32,
) -> Step {
    let mut wanted = vec(p.wanted_vel);
    if len3(wanted) > p.max_velocity {
        wanted = scale(safe_normalize(wanted), p.max_velocity);
    }
    let mut vel = wanted;
    if !s.flying {
        p.gravity_vel[1] = fmax(
            (-9.8f32).mul_add(dt, p.gravity_vel[1]),
            -p.max_gravity_velocity,
        );
        vel.x += p.gravity_vel[0];
        vel.y += p.gravity_vel[1];
        vel.z += p.gravity_vel[2];
    }
    let mut new = v3(
        vel.x.mul_add(dt, pos.x),
        vel.y.mul_add(dt, pos.y),
        vel.z.mul_add(dt, pos.z),
    );
    if !s.flying
        && let Some((dist, n)) = raycast(
            host,
            v3(new.x + 0.0, new.y + 0.3, new.z + 0.0),
            v3(new.x - 0.0, new.y - 0.05, new.z - 0.0),
        )
        && dist >= 0.0
    {
        new.y = new.y + 0.3 - dist;
        p.gravity_vel = [0.0; 3];
        p.ground_normal = arr(n);
    }
    if s.test_collision && raycast(host, pos, new).is_some_and(|(d, _)| d >= 0.0) {
        new = pos;
        wanted = ZERO;
    }
    p.wanted_vel = arr(wanted);
    let d = sub3(new, pos);
    s.vel = [d.x / dt, d.y / dt, d.z / dt];

    let dir = if s.flying { vel } else { v3(vel.x, 0.0, vel.z) };
    // gcc folds these length forms per branch: flying keeps y*y fused with x*x
    let dir_len = if s.flying {
        (dir.y.mul_add(dir.y, dir.x * dir.x) + dir.z * dir.z).sqrt()
    } else {
        (dir.x * dir.x + dir.z * dir.z).sqrt()
    };
    if dir_len > 0.01 {
        let turn = p.max_turn_speed * dt;
        let mul = p.turn_speed_mul * dt;
        let yaw = atan2(dir.x, dir.z) + p.forward_rotation_y_offset;
        s.yaw = turn_towards(
            s.yaw,
            yaw,
            fmin(turn, wrap(yaw - s.yaw).abs().mul_add(mul, 1e-4)),
        );
        if s.flying {
            let pitch = -atan2(dir.y, (dir.x * dir.x + dir.z * dir.z).sqrt());
            s.pitch = turn_towards(s.pitch, pitch, turn);
        }
    }

    let mut play_anim = 0;
    if p.anim_state != s.playing_anim_state {
        s.playing_anim_state = p.anim_state;
        play_anim = if p.anim_state == 1 { 1 } else { 2 };
    }
    Step {
        pos: new,
        yaw: s.yaw,
        pitch: s.pitch,
        play_anim,
    }
}

pub fn change_max_speed(f: f32, goal: f32, acc: f32, dt: f32) -> f32 {
    let step = acc * dt;
    if f < goal {
        fmin(f + step, goal)
    } else {
        fmax(f - step, goal)
    }
}

/// `Move_GetTowardPosAdd` and `Move_GetTowardPlayerAdd`.
pub fn toward_add(pos: Vector3f, target: Vector3f, normalize: bool, dt: f32) -> Vector3f {
    let d = sub3(target, pos);
    if normalize {
        safe_normalize(d)
    } else {
        scale(d, dt)
    }
}

pub fn stop_add(wanted: Vector3f, amount: f32, dt: f32) -> Vector3f {
    scale(wanted, -amount * dt)
}

pub fn toward_center_add(pos: Vector3f, center: Option<Vector3f>, dt: f32) -> Vector3f {
    center.map_or(ZERO, |c| scale(sub3(c, pos), dt))
}

pub struct Flock {
    pub center_mul: f32,
    pub center_y_mul: f32,
    pub separation: f32,
    pub alignment: f32,
    pub cohesion: f32,
    pub max_checks: i32,
    pub dt: f32,
}

pub fn flocking_add(
    pos: Vector3f,
    self_vel: Vector3f,
    center: Option<Vector3f>,
    members: &[Member],
    f: &Flock,
) -> Vector3f {
    let mut add = ZERO;
    if let Some(c) = center {
        let mut to = sub3(c, pos);
        to.y *= f.center_y_mul;
        add.x = to.x.mul_add(f.center_mul, add.x);
        add.y = to.y.mul_add(f.center_mul, add.y);
        add.z = to.z.mul_add(f.center_mul, add.z);
    }
    if members.is_empty() {
        return scale(add, f.dt);
    }
    let (mut mean, mut sep, mut vel) = (ZERO, ZERO, ZERO);
    let mut checked = 0i32;
    for m in members {
        mean.x += m.pos.x;
        mean.y += m.pos.y;
        mean.z += m.pos.z;
        if m.is_self || checked >= f.max_checks {
            continue;
        }
        checked = checked.wrapping_add(1);
        let delta = sub3(pos, m.pos);
        let sqr = dot3(delta, delta);
        if sqr > 1e-6 && sqr < 4.0 {
            sep.x += delta.x / sqr;
            sep.y += delta.y / sqr;
            sep.z += delta.z / sqr;
        }
        vel.x += m.vel.x;
        vel.y += m.vel.y;
        vel.z += m.vel.z;
    }
    let n = members.len() as f32;
    mean = v3(mean.x / n, mean.y / n, mean.z / n);
    let d = sub3(mean, pos);
    add.x += d.x.mul_add(f.cohesion, sep.x * f.separation);
    add.y += d.y.mul_add(f.cohesion, sep.y * f.separation);
    add.z += d.z.mul_add(f.cohesion, sep.z * f.separation);
    if checked > 0 {
        let c = checked as f32;
        add.x = (vel.x / c - self_vel.x).mul_add(f.alignment, add.x);
        add.y = (vel.y / c - self_vel.y).mul_add(f.alignment, add.y);
        add.z = (vel.z / c - self_vel.z).mul_add(f.alignment, add.z);
    }
    scale(add, f.dt)
}

pub fn wander_add(
    s: &mut CritterState,
    wanted: Vector3f,
    length: f32,
    radius: f32,
    dt: f32,
    three_d: bool,
) -> Vector3f {
    s.wander_angle = (rand_rectf(-1.0, 1.0) * 2.0 * dt).mul_add(10.0, s.wander_angle);
    if three_d {
        s.wander_pitch = clamp(
            (rand_rectf(-1.0, 1.0) * dt).mul_add(10.0, s.wander_pitch),
            -1.0,
            1.0,
        );
    }
    let mut fwd = safe_normalize(wanted);
    if dot3(fwd, fwd) == 0.0 {
        let (sy, cy) = hw::sincos(s.yaw);
        fwd = v3(sy, 0.0, cy);
    }
    let (sa, ca) = hw::sincos(s.wander_angle);
    let circle = v3(sa, if three_d { sin(s.wander_pitch) } else { 0.0 }, ca);
    let mut add = v3(
        fwd.x.mul_add(length, circle.x * radius),
        fwd.y.mul_add(length, circle.y * radius),
        fwd.z.mul_add(length, circle.z * radius),
    );
    if !three_d {
        add.y = 0.0;
    }
    scale(add, dt)
}

/// Rays recast at random intervals, longer when the critter is far from the player.
fn ray_timer_elapsed(
    timer: &mut f32,
    dt: f32,
    far_exceeded: bool,
    near: (f32, f32),
    far: (f32, f32),
) -> bool {
    if *timer >= 0.0 {
        *timer -= dt;
        return false;
    }
    *timer = if far_exceeded {
        rand_rectf(far.0, far.1)
    } else {
        rand_rectf(near.0, near.1)
    };
    true
}

/// Past half of `MaxEnabledDistance` from the player.
pub fn far_exceeded(player: Vector3f, pos: Vector3f, max_enabled_distance: f32) -> bool {
    let far = max_enabled_distance * 0.5;
    let d = sub3(player, pos);
    dot3(d, d) > far * far
}

pub fn towards_ground_add(
    s: &mut CritterState,
    host: &mut dyn CritterHost,
    pos: Vector3f,
    max_height: f32,
    mesh_height: Option<f32>,
    dt: f32,
    far_exceeded: bool,
) -> Vector3f {
    if ray_timer_elapsed(
        &mut s.ground_timer,
        dt,
        far_exceeded,
        (0.2, 0.3),
        (0.4, 0.5),
    ) {
        let length = mesh_height.map_or(max_height, |h| h.mul_add(0.5, max_height));
        s.ground_dist = raycast(host, pos, v3(pos.x - 0.0, pos.y - length, pos.z - 0.0))
            .and_then(|(d, _)| (d >= 0.0).then_some(d))
            .map_or(-1.0, |d| d);
    }
    if s.ground_dist < 0.0 {
        ZERO
    } else {
        v3(0.0, -s.ground_dist, 0.0)
    }
}

pub fn wall_avoid_add(
    s: &mut CritterState,
    p: &mut CritterProps,
    host: &mut dyn CritterHost,
    pos: Vector3f,
    distance_forward: f32,
    dt: f32,
    far_exceeded: bool,
) -> Vector3f {
    let dir = safe_normalize(vec(p.wanted_vel));
    if ray_timer_elapsed(
        &mut s.wall_timer,
        dt,
        far_exceeded,
        (0.05, 0.075),
        (0.1, 0.15),
    ) {
        let end = v3(
            dir.x.mul_add(distance_forward, pos.x),
            dir.y.mul_add(distance_forward, pos.y),
            dir.z.mul_add(distance_forward, pos.z),
        );
        // C++ read the distance uninitialised when no ray was cast; -1 as for a miss
        let mut dist = -1.0;
        p.wall_avoid_detected = false;
        if let Some((d, n)) = raycast(host, pos, end) {
            dist = d;
            p.wall_avoid_normal = arr(n);
            p.wall_avoid_detected = d >= 0.0;
        }
        p.wall_avoid_position = arr(v3(
            dir.x.mul_add(dist, pos.x),
            dir.y.mul_add(dist, pos.y),
            dir.z.mul_add(dist, pos.z),
        ));
    }
    if !p.wall_avoid_detected {
        return ZERO;
    }
    let r = vector3_reflect(dir, vec(p.wall_avoid_normal));
    let d = sub3(vec(p.wall_avoid_position), pos);
    let k = fmax(len3(d), 0.05);
    v3(r.x / k, r.y / k, r.z / k)
}
