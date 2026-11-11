//! `cAgentCharMover`: steering, turning, speed limits, stuck detection and move animations.

use super::cmax;
use super::cmin;
use super::host::{Events, Host};
use super::{add3, angle3, normalized};
use crate::math::{
    PI, PI2, TWO_PI, Vector3f, atan2, clamp, cross3, dot3, fma, len3, mul3, normalize3, rand_rectf,
    rand_rectl, sub3, to_rad, v3,
};

pub const MSG_STUCK: i32 = 1;
pub const MSG_TURNING_DONE: i32 = 5;
pub const EV_TURNED_CALLBACK: u32 = 5;
pub const STUCK_FRAMES: usize = 30;
const MAX_STUCK: f32 = 2.0;

/// `cAgentSpeedState`.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct SpeedState {
    pub forward: f32,
    pub backward: f32,
    pub sideways: f32,
    pub turn_break_mul: f32,
    pub turn_speed_mul: f32,
    pub turn_max_speed: f32,
    pub forward_acc: f32,
    pub forward_deacc: f32,
    pub sideway_acc: f32,
    pub sideway_deacc: f32,
}

impl Default for SpeedState {
    fn default() -> Self {
        Self {
            forward: 1.0,
            backward: 1.0,
            sideways: 1.0,
            turn_break_mul: -1.0,
            turn_speed_mul: -1.0,
            turn_max_speed: -1.0,
            forward_acc: -1.0,
            forward_deacc: -1.0,
            sideway_acc: -1.0,
            sideway_deacc: -1.0,
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct WallSample {
    pub dir: [f32; 3],
    pub push: [f32; 3],
    pub weight: f32,
    pub hit: bool,
}

/// One body returned by the object-avoidance physics query.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AvoidBody {
    pub center: [f32; 3],
    pub radius: f32,
    pub mass: f32,
    pub active: bool,
    pub collide: bool,
    pub character: bool,
    pub player: bool,
    pub own: bool,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct CharMoverState {
    pub max_forward: f32,
    pub max_backward: f32,
    pub turn_min_break_angle: f32,
    pub turn_break_mul: f32,
    pub turn_speed_mul: f32,
    pub turn_max_speed: f32,
    pub stopped_to_walk: f32,
    pub walk_to_run: f32,
    pub walk_to_stopped: f32,
    pub run_to_walk: f32,
    pub move_speed_anim_mul: f32,
    pub use_move_state_anims: bool,
    pub moving: bool,
    pub move_requested: bool,
    pub slow_down_at_goal: bool,
    pub is_3d: bool,
    pub turning: bool,
    pub banking: bool,
    pub anim_playing: bool,
    pub idle_extra_active: bool,
    pub wall_avoid: bool,
    pub object_avoid: bool,
    pub speed_state: i32,
    pub goal: [f32; 3],
    pub turn_goal: f32,
    pub bank_angle_mul: f32,
    pub bank_max_angle: f32,
    pub bank_max_speed: f32,
    pub turn_speed: f32,
    pub turn_sign: f32,
    pub roll: f32,
    pub roll_goal: f32,
    pub anim_state: i32,
    pub stuck: f32,
    pub stuck_disp: [[f32; 3]; STUCK_FRAMES],
    pub stuck_idx: i32,
    pub stuck_count: i32,
    pub idle_extra_min: f32,
    pub idle_extra_max: f32,
    pub idle_extra_wait: f32,
    pub idle_extra_left: f32,
    pub idle_extra_count: i32,
    pub wall_radius: f32,
    pub wall_steer: f32,
    pub wall_timer: f32,
    pub object_avoid_dist: f32,
    pub object_avoid_min_mass: f32,
    pub object_avoid_steer: f32,
}

impl Default for CharMoverState {
    fn default() -> Self {
        Self {
            max_forward: 1.0,
            max_backward: 1.0,
            turn_min_break_angle: to_rad(40.0),
            turn_break_mul: 0.0,
            turn_speed_mul: 3.0,
            turn_max_speed: 4.0,
            stopped_to_walk: 0.05,
            walk_to_run: 3.0,
            walk_to_stopped: 0.025,
            run_to_walk: 0.8,
            move_speed_anim_mul: 1.0,
            use_move_state_anims: true,
            moving: false,
            move_requested: false,
            slow_down_at_goal: false,
            is_3d: false,
            turning: false,
            banking: false,
            anim_playing: false,
            idle_extra_active: true,
            wall_avoid: false,
            object_avoid: false,
            speed_state: -1,
            goal: [0.0; 3],
            turn_goal: 0.0,
            bank_angle_mul: 1.0,
            bank_max_angle: 0.0,
            bank_max_speed: 1.0,
            turn_speed: 0.0,
            turn_sign: 1.0,
            roll: 0.0,
            roll_goal: 0.0,
            anim_state: -1,
            stuck: 0.0,
            stuck_disp: [[0.0; 3]; STUCK_FRAMES],
            stuck_idx: 0,
            stuck_count: 0,
            idle_extra_min: 0.0,
            idle_extra_max: 0.0,
            idle_extra_wait: 0.0,
            idle_extra_left: 0.0,
            idle_extra_count: 0,
            wall_radius: 0.0,
            wall_steer: 0.0,
            wall_timer: 0.0,
            object_avoid_dist: 2.0,
            object_avoid_min_mass: 4.0,
            object_avoid_steer: 1.0,
        }
    }
}

/// Angle wrapped into [-pi, pi]; non-finite or huge inputs return as they are (C++ looped forever).
pub fn wrap(mut a: f32) -> f32 {
    while a > PI {
        let n = a - TWO_PI;
        if n == a {
            break;
        }
        a = n;
    }
    while a < -PI {
        let n = a + TWO_PI;
        if n == a {
            break;
        }
        a = n;
    }
    a
}

/// `Normalize()` of the random wall-sample direction: gcc fuses only the first two terms.
fn normalize_dir(v: &mut Vector3f) {
    let l = (fma(v.x, v.x, v.y * v.y) + v.z * v.z).sqrt();
    if f64::from(l) > 1e-08 {
        let inv = 1.0f32 / l;
        v.x *= inv;
        v.y *= inv;
        v.z *= inv;
    }
}

pub fn yaw_to(d: Vector3f) -> f32 {
    atan2(-d.x, -d.z)
}

fn a3(v: [f32; 3]) -> Vector3f {
    v3(v[0], v[1], v[2])
}

fn to3(v: Vector3f) -> [f32; 3] {
    [v.x, v.y, v.z]
}

fn sqr_len(v: Vector3f) -> f32 {
    dot3(v, v)
}

fn mat_mul_rot_y(a: f32, v: Vector3f) -> Vector3f {
    crate::math::matrix_mul_vec(&super::agent::mat_rotate_y(a), v)
}

impl CharMoverState {
    pub fn move_to_pos(&mut self, pos: Vector3f, slow_down: bool) {
        self.goal = to3(pos);
        self.move_requested = true;
        self.slow_down_at_goal = slow_down;
        self.turning = false;
    }

    pub fn stop(&mut self) {
        self.moving = false;
        self.move_requested = false;
    }

    pub fn turn_to(&mut self, yaw: f32) {
        self.moving = false;
        self.move_requested = false;
        self.turning = true;
        self.turn_goal = yaw;
    }

    pub fn set_use_move_state_anims(&mut self, on: bool) {
        if on == self.use_move_state_anims {
            return;
        }
        self.use_move_state_anims = on;
        if on {
            self.anim_playing = false;
            self.anim_state = -1;
        }
    }

    fn update_wall_avoidance(
        &mut self,
        dt: f32,
        walls: &mut [WallSample],
        h: &mut dyn Host,
    ) -> Vector3f {
        if !self.wall_avoid || walls.is_empty() || !self.moving {
            return v3(0.0, 0.0, 0.0);
        }
        let pos = h.body_pos();
        self.wall_timer += dt;
        if self.wall_timer >= 0.1 {
            self.wall_timer = 0.0;
            let (mut fwd, mut right) = (h.body_forward(), h.body_right());
            if !self.is_3d {
                fwd.y = 0.0;
                right.y = 0.0;
            }
            normalize3(&mut fwd);
            normalize3(&mut right);
            let up = cross3(right, fwd);
            for s in walls.iter_mut() {
                if !s.hit {
                    let mut dir = v3(
                        rand_rectf(-1.0, 1.0),
                        rand_rectf(-1.0, 1.0),
                        rand_rectf(0.2, 1.0),
                    );
                    if !self.is_3d {
                        if dir.x.abs() < dir.y.abs() {
                            std::mem::swap(&mut dir.x, &mut dir.y);
                        }
                        dir.y *= 0.15;
                    }
                    normalize_dir(&mut dir);
                    let c = |r: f32, u: f32, f: f32| fma(f, dir.z, fma(u, dir.y, r * dir.x));
                    s.dir = [
                        c(right.x, up.x, fwd.x),
                        c(right.y, up.y, fwd.y),
                        c(right.z, up.z, fwd.z),
                    ];
                    s.weight = 0.1;
                }
                let d = a3(s.dir);
                let r = self.wall_radius;
                let end = v3(fma(d.x, r, pos.x), fma(d.y, r, pos.y), fma(d.z, r, pos.z));
                match h.raycast(pos, end) {
                    Some((dist, _)) => {
                        s.hit = true;
                        let k = -(1.0 - dist / self.wall_radius);
                        s.push = to3(mul3(a3(s.dir), k));
                    }
                    None => s.hit = false,
                }
            }
        }
        let mut sum = v3(0.0, 0.0, 0.0);
        let mut any = false;
        for s in walls.iter_mut() {
            if s.hit {
                s.weight = cmin(fma(5.0, dt, s.weight), 1.0);
                let p = a3(s.push);
                sum = v3(
                    fma(p.x, s.weight, sum.x),
                    fma(p.y, s.weight, sum.y),
                    fma(p.z, s.weight, sum.z),
                );
                any = true;
            }
        }
        if !any {
            return v3(0.0, 0.0, 0.0);
        }
        if sqr_len(sum) > 1.0 {
            normalize3(&mut sum);
        }
        mul3(sum, self.wall_steer)
    }

    fn update_object_avoidance(&mut self, dt: f32, h: &mut dyn Host) -> Vector3f {
        if !self.object_avoid || !self.moving {
            return v3(0.0, 0.0, 0.0);
        }
        let vel = h.body_velocity(dt);
        let speed = len3(vel);
        if speed < 1e-4 {
            return v3(0.0, 0.0, 0.0);
        }
        let pos = h.body_pos();
        let goal = a3(self.goal);
        let mut to_goal = sub3(goal, pos);
        if !self.is_3d {
            to_goal.y = 0.0;
        }
        normalize3(&mut to_goal);
        let side = normalized(mat_mul_rot_y(
            PI2,
            v3(vel.x / speed, 0.0 / speed, vel.z / speed),
        ));
        let dist = self.object_avoid_dist;
        let n = h.avoid_query(pos, dist);
        let mut sum = v3(0.0, 0.0, 0.0);
        for i in 0..n {
            let b = h.avoid_get(i);
            if !b.active
                || !b.collide
                || (b.mass == 0.0 && !b.character)
                || (b.mass != 0.0 && b.mass < self.object_avoid_min_mass)
                || b.player
                || b.own
            {
                continue;
            }
            let center = a3(b.center);
            let radius = b.radius * 0.71;
            if sqr_len(sub3(center, pos)) > (radius + dist) * (radius + dist)
                || sqr_len(sub3(goal, center)) < radius * radius
            {
                continue;
            }
            let mut away = sub3(pos, center);
            if !self.is_3d {
                away.y = 0.0;
            }
            let mut d = len3(away) - radius;
            if d > dist {
                continue;
            }
            d = cmax(d, 1e-4);
            normalize3(&mut away);
            let angle = angle3(mul3(away, -1.0), to_goal);
            if angle < to_rad(20.0) {
                d *= 0.5;
            }
            let weight = 1.0 - d / dist;
            if angle < to_rad(30.0) {
                let s = if dot3(side, away) < 0.0 {
                    mul3(side, -1.0)
                } else {
                    side
                };
                away = add3(away, s);
            }
            let k = weight * weight * self.object_avoid_steer;
            sum = v3(
                fma(away.x, k, sum.x),
                fma(away.y, k, sum.y),
                fma(away.z, k, sum.z),
            );
        }
        sum
    }

    fn play_move_anim(&mut self, state: i32, speed: f32, h: &mut dyn Host) {
        if self.anim_playing && h.anim_current() < 0 {
            self.set_use_move_state_anims(true);
        }
        if !self.use_move_state_anims {
            self.anim_state = -1;
            return;
        }
        if state != self.anim_state && h.anim_name_nonempty(state) {
            self.anim_state = state;
            h.play_move_anim(state);
        }
        if state > 0 {
            let div = cmax(if state == 1 { 1.0 } else { self.walk_to_run }, 0.1);
            h.set_move_anim_speed(cmax(speed * self.move_speed_anim_mul / div, 0.2));
        }
    }

    fn update_stuck(&mut self, dt: f32, speed: f32, h: &mut dyn Host, ev: &mut Events) {
        let disp = sub3(h.body_pos(), h.body_last_pos());
        if let Some(slot) = self.stuck_disp.get_mut(self.stuck_idx as usize) {
            *slot = to3(disp);
        }
        self.stuck_idx = (self.stuck_idx + 1).rem_euclid(STUCK_FRAMES as i32);
        self.stuck_count = (self.stuck_count + 1).min(STUCK_FRAMES as i32);
        if self.anim_playing || speed < 1e-5 {
            self.stuck = 0.0;
            return;
        }
        let mut sum = v3(0.0, 0.0, 0.0);
        for d in self
            .stuck_disp
            .iter()
            .take(self.stuck_count.max(0) as usize)
        {
            sum = add3(sum, a3(*d));
        }
        let dist = len3(disp);
        let dir = if dist > 1e-8 {
            v3(disp.x / dist, disp.y / dist, disp.z / dist)
        } else {
            disp
        };
        let dot = dot3(h.body_forward(), dir);
        if len3(sum) < 0.1 * speed || dist / dt / speed < 0.3 || (dot.abs() < 0.3 && speed > 0.001)
        {
            self.stuck = cmin(self.stuck + dt, MAX_STUCK);
        } else {
            self.stuck = cmax(fma(-0.8, dt, self.stuck), 0.0);
        }
        if self.stuck >= MAX_STUCK {
            self.stuck = 0.0;
            ev.message(MSG_STUCK, v3(0.0, 0.0, 0.0), 0);
        }
    }

    fn update_idle_extra(&mut self, dt: f32, anim: i32, h: &mut dyn Host) {
        if self.idle_extra_count <= 0 || anim != 0 || !self.use_move_state_anims {
            return;
        }
        if self.idle_extra_left > 0.0 {
            self.idle_extra_left -= dt;
            if self.idle_extra_left <= 0.0 {
                h.idle_restart();
            }
        }
        if !self.idle_extra_active {
            return;
        }
        self.idle_extra_wait -= dt;
        if self.idle_extra_wait > 0.0 {
            return;
        }
        let idx = rand_rectl(1, self.idle_extra_count);
        if let Some(len) = h.idle_extra_length(idx) {
            self.idle_extra_left = len - 0.3;
            self.idle_extra_wait =
                self.idle_extra_left + rand_rectf(self.idle_extra_min, self.idle_extra_max);
            h.play_idle_extra(idx);
        }
    }

    /// `cAgentCharMover::Update`; `speed_state` is the current entry of the speed-state map.
    pub fn update(
        &mut self,
        dt: f32,
        speed_state: Option<&SpeedState>,
        walls: &mut [WallSample],
        h: &mut dyn Host,
        ev: &mut Events,
    ) {
        if !h.has_body() {
            return;
        }
        self.moving = std::mem::take(&mut self.move_requested);
        let turn_speed_mul = match speed_state {
            Some(s) if s.turn_speed_mul >= 0.0 => s.turn_speed_mul,
            _ => self.turn_speed_mul,
        };
        let turn_max = match speed_state {
            Some(s) if s.turn_max_speed >= 0.0 => s.turn_max_speed,
            _ => self.turn_max_speed,
        };
        let break_mul = match speed_state {
            Some(s) if s.turn_break_mul >= 0.0 => s.turn_break_mul,
            _ => self.turn_break_mul,
        };

        let yaw = h.body_yaw();
        let mut goal_yaw = yaw;
        let mut rotate = false;
        let mut dist = 0.0f32;
        let wall = self.update_wall_avoidance(dt, walls, h);
        let object = self.update_object_avoidance(dt, h);
        let avoid = add3(wall, object);
        if self.moving {
            let mut delta = sub3(a3(self.goal), h.body_pos());
            let mut avoided = false;
            if avoid != v3(0.0, 0.0, 0.0) {
                let size_x = h.body_size().x;
                let dl = if self.is_3d {
                    fma(delta.y, delta.y, fma(delta.x, delta.x, delta.z * delta.z)).sqrt()
                } else {
                    (delta.x * delta.x + delta.y * delta.y + delta.z * delta.z).sqrt()
                };
                let k = cmin(dl / fma(2.0, size_x, 2.0), 1.0);
                let mut offset = mul3(avoid, k);
                if !self.is_3d {
                    offset.y = 0.0;
                }
                let mut avoid_delta = add3(delta, offset);
                avoid_delta.y = 0.0;
                if len3(avoid_delta) > 0.05 {
                    let ny = if self.is_3d {
                        fma(avoid.y, k, delta.y)
                    } else {
                        delta.y + offset.y
                    };
                    delta = v3(avoid_delta.x, ny, avoid_delta.z);
                    avoided = true;
                }
            }
            let dy = if self.is_3d { delta.y } else { 0.0 };
            delta.y = 0.0;
            // gcc folds the zeroed y differently once the avoided delta was built
            let dist_xz = if avoided {
                fma(delta.z, delta.z, delta.x * delta.x).sqrt()
            } else {
                (delta.x * delta.x + delta.z * delta.z).sqrt()
            };
            dist = if avoided {
                (dist_xz * dist_xz + dy * dy).sqrt()
            } else {
                fma(dy, dy, dist_xz * dist_xz).sqrt()
            };
            if dist_xz > 0.05 {
                goal_yaw = yaw_to(delta);
                rotate = true;
            }
            if self.is_3d && dist > 0.05 {
                let diff = atan2(dy, dist_xz) - h.body_pitch();
                let lim = self.turn_max_speed * dt;
                h.body_add_pitch(clamp(diff, -lim, lim));
            }
        } else if self.turning {
            goal_yaw = self.turn_goal;
            rotate = true;
        }
        let diff = wrap(goal_yaw - yaw);
        if rotate {
            let mut step = cmin(diff.abs() * turn_speed_mul, turn_max) * dt;
            step = cmin(step, diff.abs());
            h.set_body_yaw(yaw + if diff < 0.0 { -step } else { step });
            self.turn_speed = step / dt;
            self.turn_sign = if diff < 0.0 { -1.0 } else { 1.0 };
            if self.turning && diff.abs() < to_rad(2.0) {
                self.turning = false;
                let cur = h.body_yaw();
                h.set_body_yaw(cur + wrap(goal_yaw - cur));
                ev.message(MSG_TURNING_DONE, v3(0.0, 0.0, 0.0), 0);
                ev.plain(EV_TURNED_CALLBACK);
            }
        } else {
            self.turn_speed = cmax(self.turn_speed - 4.0 * dt, 0.0);
        }
        if self.banking {
            self.roll_goal = clamp(
                self.turn_sign * self.turn_speed * self.bank_angle_mul,
                -self.bank_max_angle,
                self.bank_max_angle,
            );
        }
        self.roll = fma(
            clamp(
                (self.roll_goal - self.roll) * self.bank_max_speed,
                -self.bank_max_speed,
                self.bank_max_speed,
            ),
            dt,
            self.roll,
        );

        let mut wanted = 0.0f32;
        if self.moving {
            wanted = speed_state.map_or(self.max_forward, |s| s.forward);
            if diff.abs() > self.turn_min_break_angle {
                wanted *= break_mul;
            }
            if self.slow_down_at_goal && dist < 1.0 {
                wanted *= cmax(dist, 0.2);
            }
        }
        h.body_set_max_forward(cmax(wanted, 0.001));
        if let Some(s) = speed_state {
            if s.forward_acc > 0.0 {
                h.body_set_move_acc(s.forward_acc);
            }
            if s.forward_deacc > 0.0 {
                h.body_set_move_deacc(s.forward_deacc);
            }
        }
        if wanted > 0.0 {
            h.body_move_forward();
        }

        let speed = h.body_move_speed();
        self.update_stuck(dt, speed, h, ev);

        let mut anim = if self.anim_state < 0 {
            0
        } else {
            self.anim_state
        };
        if anim == 0 && speed > self.stopped_to_walk {
            anim = 1;
        } else if anim == 1 && speed > self.walk_to_run {
            anim = 2;
        } else if anim == 2 && speed < self.run_to_walk {
            anim = 1;
        } else if anim == 1 && speed < self.walk_to_stopped {
            anim = 0;
        }
        self.play_move_anim(anim, speed, h);
        self.update_idle_extra(dt, anim, h);
    }
}
