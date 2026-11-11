//! `iLuxEnemy` senses and helpers: sight, detection edges, range, door checks, FOV, line of
//! sight, search-node picking and distance helpers.

use super::ctx::{Env, g, gv, s, sv};
use super::machine::{
    MSG_PLAYER_DETECTED, MSG_PLAYER_IN_RANGE, MSG_PLAYER_OUT_OF_RANGE, MSG_PLAYER_SEEN,
    MSG_PLAYER_UNDETECTED, MSG_PLAYER_UNSEEN, MSG_STUCK_AT_DOOR,
};
use super::mover::MoverParams;
use super::pathfinder::{NodeQuery, next_goal_pos, node_at_pos};
use super::{K2PI, angle_dist_rad, angle_from_points_3d, dist3, normalized, rot_y, sqr_len, z3};
use crate::math::{Vector3f, cross3, dot3, fma, matrix_mul_vec, rand_rectf, sub3, v3};

const POS_ADDS: [(f32, f32); 9] = [
    (0.0, 0.0),
    (0.75, 0.0),
    (-0.75, 0.0),
    (0.0, 1.0),
    (0.0, -1.0),
    (0.5, 0.5),
    (-0.5, 0.5),
    (0.25, 0.0),
    (-0.25, 0.0),
];

fn add(a: Vector3f, b: Vector3f) -> Vector3f {
    v3(a.x + b.x, a.y + b.y, a.z + b.z)
}

impl Env<'_> {
    pub fn dist_to_player(&mut self) -> f32 {
        if self.h.player_dead() {
            return 100000.0;
        }
        let (a, b) = (self.h.body_feet(), self.h.player_feet());
        dist3(a, b)
    }

    pub fn dist_to_player_2d(&mut self) -> f32 {
        if self.h.player_dead() {
            return 100000.0;
        }
        let (mut a, mut b) = (self.h.body_feet(), self.h.player_feet());
        a.y = 0.0;
        b.y = 0.0;
        dist3(a, b)
    }

    pub fn player_movement_toward_enemy_amount(&mut self) -> f32 {
        let dt = self.h.step_size();
        let mut dir = self.h.player_velocity(dt);
        dir.y = 0.0;
        if sqr_len(dir) < 0.1 * 0.1 {
            return -1.0;
        }
        dir = normalized(dir);
        let mut to_enemy = sub3(self.h.body_pos(), self.h.player_pos());
        to_enemy.y = 0.0;
        to_enemy = normalized(to_enemy);
        dot3(dir, to_enemy)
    }

    pub fn in_fov(&mut self, pos: Vector3f) -> bool {
        let fov = g(self.c.fov) * g(self.c.fov_mul);
        if fov < K2PI {
            let (bp, bs) = (self.h.body_pos(), self.h.body_size());
            let start = v3(bp.x + 0.0, bp.y + (bs.y / 2.0 - 0.2), bp.z + 0.0);
            let dir = normalized(sub3(pos, start));
            let fwd = self.h.body_forward();
            let ta = angle_from_points_3d(z3(), dir);
            let ea = angle_from_points_3d(z3(), fwd);
            let ax = angle_dist_rad(ta.x, ea.x).abs();
            let ay = angle_dist_rad(ta.y, ea.y).abs();
            if ay > fov * 0.5 {
                return false;
            }
            if ax > fov * g(self.c.fov_x_mul) * 0.5 {
                return false;
            }
        }
        true
    }

    pub fn player_in_fov(&mut self) -> bool {
        let p = self.h.player_pos();
        self.in_fov(p)
    }

    pub fn line_of_sight(
        &mut self,
        pos: Vector3f,
        size: Vector3f,
        check_fov: bool,
        source: Option<Vector3f>,
    ) -> bool {
        let start_center = match source {
            Some(s) => s,
            None => {
                let (bp, bs) = (self.h.body_pos(), self.h.body_size());
                v3(bp.x + 0.0, bp.y + (bs.y / 2.0 - 0.1), bp.z + 0.0)
            }
        };
        let end_center = pos;
        let forward = normalized(sub3(end_center, start_center));
        let up = v3(0.0, 1.0, 0.0);
        let right = cross3(forward, up);
        if check_fov && !self.in_fov(pos) {
            return false;
        }
        let half_w = size.x * 0.4;
        let half_h = size.y * 0.4;
        let mut count = 0;
        for (ax, ay) in POS_ADDS {
            let (sx, sy) = (ax * half_w, ay * half_h);
            let a = v3(
                fma(right.x, sx, up.x * sy),
                fma(right.y, sx, up.y * sy),
                fma(right.z, sx, up.z * sy),
            );
            let (st, en) = (add(start_center, a), add(end_center, a));
            if self.h.los_ray(st, en) {
                count += 1;
            }
            if count == 2 {
                return true;
            }
        }
        false
    }

    pub fn update_can_see_player(&mut self, dt: f32) {
        if self.h.player_dead() {
            return;
        }
        if self.h.triggers_disabled() {
            s(self.c.can_see, false);
        }
        if g(self.c.blind) {
            let f = self.h.player_feet();
            sv(self.c.last_known, f);
            s(self.c.can_see, false);
            return;
        }
        s(self.c.look_count, g(self.c.look_count) - dt);
        if g(self.c.look_count) > 0.0 {
            return;
        }
        s(self.c.look_count, 0.3);
        let (pp, bp) = (self.h.player_pos(), self.h.body_pos());
        let dist = dist3(pp, bp);
        let lantern = self.h.player_lantern();
        let crouching = self.h.player_crouch();
        let mut max_range = g(self.c.sight_range);
        // The lantern boost multiplies the member itself, every check, and not the local.
        if lantern {
            s(self.c.sight_range, g(self.c.sight_range) * 1.5);
        }
        let skip = g(self.c.skip_visibility_handicaps);
        if !lantern
            && self.h.player_light_level() < g(self.c.in_darkness_light_level)
            && self.h.player_avg_speed() < 0.05
            && !skip
        {
            max_range = g(self.c.darkness_sight_range);
        }
        if crouching && !lantern && !skip {
            max_range *= g(self.c.crouch_visible_range_mul);
        }
        if max_range < dist {
            if !g(self.c.can_see) {
                self.send0(MSG_PLAYER_UNSEEN, 0.0, false);
            }
            s(self.c.can_see, false);
            s(self.c.los_count, 0);
            return;
        }
        let min_dist = self.h.body_bv_radius() + self.h.player_bv_radius() + 0.05;
        let use_fov = dist >= min_dist;
        let (psize, ppos) = (self.h.player_size(), self.h.player_pos());
        if self.line_of_sight(ppos, psize, use_fov, None) {
            if g(self.c.los_count) >= 2 {
                if !g(self.c.can_see) {
                    self.send0(MSG_PLAYER_SEEN, 0.0, false);
                }
                s(self.c.can_see, true);
            } else {
                s(self.c.los_count, g(self.c.los_count) + 1);
            }
        } else if g(self.c.los_count) > 0 {
            s(self.c.los_count, 0);
            if !g(self.c.can_see) {
                self.send0(MSG_PLAYER_UNSEEN, 0.0, false);
            }
            s(self.c.can_see, false);
        }
    }

    /// `detected_now` is the enemy type's `PlayerIsDetected()`.
    pub fn update_player_detected(&mut self, detected_now: impl FnOnce(&mut Self) -> bool) {
        if self.h.player_dead() {
            return;
        }
        let d = detected_now(self);
        let was = g(self.c.detected);
        if d && !was {
            self.send0(MSG_PLAYER_DETECTED, 0.0, false);
        } else if !d && was {
            self.send0(MSG_PLAYER_UNDETECTED, 0.0, false);
        }
        s(self.c.detected, d);
    }

    pub fn update_player_in_range(&mut self) {
        if self.h.player_dead() {
            return;
        }
        let d = self.dist_to_player();
        let act = g(self.c.activation_distance);
        if d <= act && !g(self.c.in_range) {
            s(self.c.in_range, true);
            self.send0(MSG_PLAYER_IN_RANGE, 0.0, false);
        } else if d > fma(act, 1.25, 2.0) && g(self.c.in_range) {
            s(self.c.in_range, false);
            self.send0(MSG_PLAYER_OUT_OF_RANGE, 0.0, false);
        }
    }

    pub fn update_check_stuck_at_door(&mut self, dt: f32) {
        // SAFETY: the mover state is only aliased by C++ inside `play_anim`.
        let stuck = unsafe { (*self.c.mover).stuck_counter };
        if stuck < 0.1 {
            s(self.c.stuck_at_door, false);
            return;
        }
        s(self.c.check_door_count, g(self.c.check_door_count) - dt);
        if g(self.c.check_door_count) > 0.0 {
            return;
        }
        s(self.c.check_door_count, 0.1);
        let mut at_door = false;
        s(self.c.stuck_door_id, -1);
        let dir = if g(self.c.pf_moving) {
            let goal = next_goal_pos(self.h, gv(self.c.pf_goal));
            normalized(sub3(goal, self.h.body_pos()))
        } else {
            self.h.body_forward()
        };
        let mut found = z3();
        let n = self.h.door_count();
        for i in 0..n {
            let (id, local, world) = self.h.door_at(i);
            let to_door = normalized(sub3(local, self.h.body_pos()));
            if dot3(dir, to_door) > 0.2 {
                s(self.c.stuck_door_id, id);
                found = world;
                at_door = true;
                break;
            }
        }
        if at_door && !g(self.c.stuck_at_door) {
            s(self.c.stuck_at_door, true);
            // SAFETY: as above.
            unsafe { (*self.c.mover).stuck_counter = 0.0 };
            self.send(MSG_STUCK_AT_DOOR, 0.0, false, found, 0.0, 0);
        } else if !at_door && g(self.c.stuck_at_door) {
            s(self.c.stuck_at_door, false);
        }
    }

    pub fn update_check_last_player_pos(&mut self, dt: f32) {
        let mut c = g(self.c.last_pos_count);
        if g(self.c.can_see) {
            c += dt;
            if c > 1.0 {
                c = 1.0;
            }
        } else {
            c -= dt;
            if c < 0.0 {
                c = 0.0;
            }
        }
        s(self.c.last_pos_count, c);
        if c > 0.0 {
            let f = self.h.player_feet();
            sv(self.c.last_known, f);
        }
    }

    pub fn update_regen_health(&mut self, dt: f32) {
        let hp = g(self.c.health);
        if hp <= 0.0 {
            return;
        }
        let (max, speed) = (g(self.c.max_regen_health), g(self.c.regen_health_speed));
        if hp < max && speed > 0.0 {
            let mut n = fma(dt, speed, hp);
            if n > max {
                n = max;
            }
            s(self.c.health, n);
        }
    }

    pub fn update_char_body(&mut self, mp: &MoverParams) {
        // SAFETY: as above.
        let mul = unsafe { (*self.c.mover).calculate_speed_mul(mp, self.h) };
        let (f, b) = (g(self.c.forward_speed), g(self.c.backward_speed));
        self.h.body_set_speeds(
            f * mul,
            b * mul,
            g(self.c.forward_acc),
            g(self.c.forward_deacc),
        );
    }

    /// `GetSearchForPlayerNode`; the node is the host's last iteration at the returned index.
    pub fn search_for_player_node(
        &mut self,
        iters: i32,
        max_angle_mul: f32,
        min_dist_mul: f32,
        max_dist_mul: f32,
        p: &[f32; 4],
    ) -> Option<i32> {
        let max_angle = p[0] * max_angle_mul;
        for _ in 0..iters {
            let angle = rand_rectf(-max_angle, max_angle);
            let (pf, bf) = (self.h.player_feet(), self.h.body_feet());
            let mut to_player = sub3(pf, bf);
            let to_dist = crate::math::len3(to_player);
            to_player = normalized(to_player);
            let dir = matrix_mul_vec(&rot_y(angle), to_player);
            let length = rand_rectf(p[1] * min_dist_mul, p[2] * max_dist_mul) * to_dist;
            let wanted = v3(
                fma(dir.x, length, bf.x),
                fma(dir.y, length, bf.y),
                fma(dir.z, length, bf.z),
            );
            let q = NodeQuery::new(0.0, length, true, false, true, 1);
            if let Some((i, _)) = node_at_pos(self.h, wanted, &q) {
                self.h.nc_commit_last(i);
                return Some(i);
            }
        }
        let bf = self.h.body_feet();
        let q = NodeQuery::new(0.0, 13.0, false, false, true, 1);
        node_at_pos(self.h, bf, &q).map(|(i, _)| i)
    }

    /// `GetPatrolAroundPlayerNode`; `p` is max angle, min dist, max dist.
    pub fn patrol_around_player_node(
        &mut self,
        iters: i32,
        max_angle_mul: f32,
        min_dist_mul: f32,
        max_dist_mul: f32,
        p: &[f32; 3],
        start: Vector3f,
    ) -> Option<i32> {
        let max_angle = p[0] * max_angle_mul;
        let pf = self.h.player_feet();
        for _ in 0..iters {
            let angle = rand_rectf(-max_angle, max_angle);
            let to_start = normalized(sub3(start, pf));
            let dir = matrix_mul_vec(&rot_y(angle), to_start);
            let length = rand_rectf(p[1] * min_dist_mul, p[2] * max_dist_mul);
            let wanted = v3(
                fma(dir.x, length, pf.x),
                fma(dir.y, length, pf.y),
                fma(dir.z, length, pf.z),
            );
            let q = NodeQuery::new(0.0, length, true, false, true, 1);
            if let Some((i, _)) = node_at_pos(self.h, wanted, &q) {
                self.h.nc_commit_last(i);
                return Some(i);
            }
        }
        let bf = self.h.body_feet();
        let q = NodeQuery::new(0.0, 13.0, false, false, true, 1);
        node_at_pos(self.h, bf, &q).map(|(i, _)| i)
    }
}
