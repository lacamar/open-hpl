//! `cLuxEnemyMover`: stuck counter, turning, speed multiplier and move-state animation choice.

use super::host::{AnimReq, Host};
use super::{angle_dist_rad, angle_from_points_2d, cmin, dist3};
use crate::math::{Vector3f, dot3, fma, len3, normalize3, sub3};

pub const MOVE_BACKWARD: i32 = 0;
pub const MOVE_STOPPED: i32 = 1;
pub const MOVE_WALKING: i32 = 2;
pub const MOVE_RUNNING: i32 = 3;
pub const MOVE_NONE: i32 = 4;

pub const ANIM_BACKWARD: i32 = 0;
pub const ANIM_IDLE: i32 = 1;
pub const ANIM_WALK: i32 = 2;
pub const ANIM_RUN: i32 = 3;

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct MoverState {
    pub turning: bool,
    pub override_move: bool,
    pub turn_goal_angle: f32,
    pub turn_speed: f32,
    pub turn_break_acc: f32,
    pub stuck_counter: f32,
    pub stuck_limit: f32,
    pub max_stuck_counter: f32,
    pub move_state: i32,
}

impl Default for MoverState {
    fn default() -> Self {
        Self {
            turning: false,
            override_move: false,
            turn_goal_angle: 0.0,
            turn_speed: 0.0,
            turn_break_acc: 0.0,
            stuck_counter: 0.0,
            stuck_limit: 0.3,
            max_stuck_counter: 2.0,
            move_state: MOVE_NONE,
        }
    }
}

/// The enemy fields the mover reads, copied per call.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct MoverParams {
    pub turn_min_break_angle: f32,
    pub turn_break_mul: f32,
    pub turn_speed_mul: f32,
    pub turn_max_speed: f32,
    pub move_speed_anim_mul: f32,
    pub pose: i32,
    pub stopped_to_walk: [f32; 2],
    pub walk_to_stopped: [f32; 2],
    pub walk_to_run: [f32; 2],
    pub run_to_walk: [f32; 2],
}

fn at(a: &[f32; 2], i: i32) -> f32 {
    a.get(i.max(0) as usize).copied().unwrap_or(0.0)
}

impl MoverState {
    pub fn on_update(&mut self, p: &MoverParams, dt: f32, h: &mut dyn Host) {
        self.update_stuck_counter(dt, h);
        self.update_turning(p, dt, h);
        self.update_move_animation(p, h);
    }

    pub fn move_to_pos(&mut self, feet: Vector3f, h: &mut dyn Host) {
        self.turn_to_pos(feet, h);
        h.body_move_fwd();
    }

    pub fn turn_to_pos(&mut self, feet: Vector3f, h: &mut dyn Host) {
        let start = h.body_pos();
        let goal = -angle_from_points_2d(start.x, start.z, feet.x, feet.z);
        self.turn_to_angle(goal);
    }

    pub fn turn_to_angle(&mut self, a: f32) {
        self.turning = true;
        self.turn_goal_angle = a;
    }

    pub fn use_move_state_animations(&mut self) {
        if !self.override_move {
            return;
        }
        self.override_move = false;
        self.move_state = MOVE_NONE;
    }

    pub fn stuck_at_max(&self) -> bool {
        self.stuck_counter >= self.max_stuck_counter
    }

    pub fn calculate_speed_mul(&self, p: &MoverParams, h: &mut dyn Host) -> f32 {
        let mut mul = 1.0f32;
        if self.turning {
            let dist = angle_dist_rad(h.body_yaw(), self.turn_goal_angle).abs();
            if dist >= p.turn_min_break_angle && h.body_move_speed_fwd() > 0.15 {
                mul = fma(-p.turn_break_mul, dist, mul);
                if mul < 0.0 {
                    mul = 0.0;
                }
            }
        }
        mul
    }

    pub fn move_speed(h: &mut dyn Host) -> f32 {
        let dt = h.step_size();
        let mut v = h.body_velocity(dt);
        v.y = 0.0;
        len3(v)
    }

    pub fn wanted_speed_amount(h: &mut dyn Host) -> f32 {
        let wanted = h.body_move_speed_fwd();
        let real = Self::move_speed(h);
        if wanted.abs() < 0.001 {
            return 1.0;
        }
        real / wanted
    }

    fn update_stuck_counter(&mut self, dt: f32, h: &mut dyn Host) {
        let wanted = h.body_move_speed_fwd();
        let (pos, last) = (h.body_pos(), h.body_last_pos());
        let real = dist3(pos, last) / dt;
        let wanted_dir = h.body_forward();
        let mut real_dir = sub3(pos, last);
        normalize3(&mut real_dir);
        let cos = dot3(wanted_dir, real_dir);
        if real / wanted < self.stuck_limit || (cos.abs() < 0.3 && wanted > 0.001) {
            self.stuck_counter += dt;
            if self.stuck_counter > self.max_stuck_counter {
                self.stuck_counter = self.max_stuck_counter;
            }
        } else {
            self.stuck_counter = fma(-dt, 0.8, self.stuck_counter);
            if self.stuck_counter < 0.0 {
                self.stuck_counter = 0.0;
            }
        }
    }

    fn update_turning(&mut self, p: &MoverParams, dt: f32, h: &mut dyn Host) {
        if !self.turning {
            return;
        }
        let dist = angle_dist_rad(h.body_yaw(), self.turn_goal_angle);
        if dist.abs() < 0.001 {
            self.turning = false;
            self.turn_speed = 0.0;
            self.turn_break_acc = 0.0;
            return;
        }
        self.turn_speed = cmin(p.turn_speed_mul * dist.abs(), p.turn_max_speed);
        if dist < 0.0 {
            h.body_add_yaw(-self.turn_speed * dt);
        } else {
            h.body_add_yaw(self.turn_speed * dt);
        }
    }

    fn play_move_anim(&mut self, h: &mut dyn Host, a: AnimReq) {
        if h.play_anim(&a) {
            self.override_move = a.override_move;
        }
    }

    pub fn update_move_animation(&mut self, p: &MoverParams, h: &mut dyn Host) {
        if self.override_move {
            return;
        }
        let mut speed = Self::move_speed(h);
        if h.body_move_speed_fwd() < 0.0 {
            speed = -speed;
        }
        let pose = p.pose;
        let prev = self.move_state;
        match self.move_state {
            MOVE_BACKWARD => {
                if speed >= 0.0 {
                    self.move_state = MOVE_STOPPED;
                }
            }
            MOVE_STOPPED => {
                if speed < -0.05 {
                    self.move_state = MOVE_BACKWARD;
                } else if speed >= at(&p.stopped_to_walk, pose)
                    || (self.turn_speed.abs() > 0.07 && h.body_move_delay() <= 0.0)
                {
                    self.move_state = MOVE_WALKING;
                }
            }
            MOVE_WALKING => {
                if speed >= at(&p.walk_to_run, pose) {
                    self.move_state = MOVE_RUNNING;
                } else if speed <= at(&p.walk_to_stopped, pose) && self.turn_speed.abs() < 0.03 {
                    self.move_state = MOVE_STOPPED;
                }
            }
            MOVE_RUNNING => {
                if speed <= at(&p.run_to_walk, pose) {
                    self.move_state = MOVE_WALKING;
                }
            }
            _ => self.move_state = MOVE_STOPPED,
        }
        if prev != self.move_state {
            let mul = p.move_speed_anim_mul;
            let req = |kind, fade, dep, speed_mul, sync| AnimReq {
                kind,
                n: 0,
                looped: true,
                fade,
                depends_on_speed: dep,
                speed_mul,
                sync,
                override_move: false,
                use_move_when_over: true,
            };
            let a = match self.move_state {
                MOVE_BACKWARD => Some(req(ANIM_BACKWARD, 0.4, true, mul, false)),
                MOVE_STOPPED => Some(req(ANIM_IDLE, 0.7, false, 1.0, false)),
                MOVE_WALKING => Some(req(ANIM_WALK, 0.5, true, mul, prev == MOVE_RUNNING)),
                MOVE_RUNNING => Some(req(ANIM_RUN, 0.5, true, mul, prev == MOVE_WALKING)),
                _ => None,
            };
            if let Some(a) = a {
                self.play_move_anim(h, a);
            }
        }
        if h.anim_exists() && self.move_state != MOVE_STOPPED {
            if speed.abs() > 0.05 {
                h.anim_set_speed(speed.abs() * p.move_speed_anim_mul);
            } else {
                h.anim_set_speed(self.turn_speed.abs() * p.move_speed_anim_mul * 2.0);
            }
        }
    }
}
