//! `cAgent`: perception, door checks, auto-disable, ground alignment and mesh sync.

use super::host::{
    EV_AUTO_DISABLE_CALLBACK, EV_DOOR_MESSAGE, EV_SET_INACTIVE, Events, Host, MSG_PLAYER_DETECTED,
    MSG_PLAYER_UNDETECTED, MSG_SENSES_DEACTIVATED,
};
use super::{add3, angle3, dist3, normalized, std_min};
use crate::math::{
    Matrixf, PI, Vector3f, cos, fma, matrix_mul, matrix_mul_vec, rotate_z, sin, sub3, v3,
};

pub(crate) fn mat_rotate_x(a: f32) -> Matrixf {
    let (s, c) = (sin(a), cos(a));
    Matrixf {
        m: [
            [1.0, 0.0, 0.0, 0.0],
            [0.0, c, -s, 0.0],
            [0.0, s, c, 0.0],
            [0.0, 0.0, 0.0, 1.0],
        ],
    }
}

pub(crate) fn mat_rotate_y(a: f32) -> Matrixf {
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

/// The scalar part of `cAgent` that scripts poke at; C++ `cAgent` derives from it.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AgentState {
    pub senses_active: bool,
    pub update_detection: bool,
    pub seen: bool,
    pub detected: bool,
    pub sight_range_affected_by_modifiers: bool,
    pub static_collider: bool,
    pub check_for_doors: bool,
    pub align_ground: bool,
    pub pos_bone_is_feet: bool,
    pub global_space: bool,
    pub gravity_before_global: bool,
    pub collision_before_global: bool,
    pub auto_disable: bool,
    pub fov: f32,
    pub fov_mul: f32,
    pub sight_range: f32,
    pub sight_range_mul: f32,
    pub eye_height: f32,
    pub detect_min_time: f32,
    pub detect_count: f32,
    pub detect_timer: f32,
    pub current_sight_dist: f32,
    pub seen_count: i32,
    pub last_known_player_pos: [f32; 3],
    pub align_ray_start: f32,
    pub align_ray_max: f32,
    pub align_timer: f32,
    pub align_y: f32,
    pub align_dist: [f32; 3],
    pub align_count: i32,
    pub align_idx: i32,
    pub ground_align_pos: [f32; 3],
    pub pos_bone_y_offset: f32,
    pub max_door_dist: f32,
    pub check_doors_count: f32,
    pub door_check_timer: f32,
    pub auto_disable_min_dist: f32,
    pub auto_disable_timer: f32,
    pub auto_disable_count: f32,
}

impl Default for AgentState {
    fn default() -> Self {
        Self {
            senses_active: true,
            update_detection: true,
            seen: false,
            detected: false,
            sight_range_affected_by_modifiers: true,
            static_collider: false,
            check_for_doors: true,
            align_ground: false,
            pos_bone_is_feet: true,
            global_space: false,
            gravity_before_global: true,
            collision_before_global: true,
            auto_disable: false,
            fov: crate::math::to_rad(90.0),
            fov_mul: 1.0,
            sight_range: 30.0,
            sight_range_mul: 1.0,
            eye_height: 0.9,
            detect_min_time: 0.0,
            detect_count: 0.0,
            detect_timer: 0.0,
            current_sight_dist: 0.0,
            seen_count: 0,
            last_known_player_pos: [0.0; 3],
            align_ray_start: 0.5,
            align_ray_max: 0.5,
            align_timer: 0.0,
            align_y: 0.0,
            align_dist: [0.0; 3],
            align_count: 0,
            align_idx: 0,
            ground_align_pos: [0.0; 3],
            pos_bone_y_offset: 0.0,
            max_door_dist: 1.0,
            check_doors_count: 0.0,
            door_check_timer: 0.0,
            auto_disable_min_dist: 0.0,
            auto_disable_timer: 0.0,
            auto_disable_count: 0.0,
        }
    }
}

impl AgentState {
    pub fn eye(&self, h: &mut dyn Host) -> Vector3f {
        if h.has_body() {
            let size = h.body_size();
            add3(h.body_feet(), v3(0.0, size.y * self.eye_height, 0.0))
        } else {
            h.ent_pos()
        }
    }

    fn forward(h: &mut dyn Host) -> Vector3f {
        if h.has_body() {
            h.body_forward()
        } else {
            v3(0.0, 0.0, -1.0)
        }
    }

    pub fn player_dist(&self, h: &mut dyn Host) -> f32 {
        let from = if h.has_body() {
            h.body_feet()
        } else {
            h.ent_pos()
        };
        dist3(from, h.player_feet())
    }

    fn player_in_sight(&self, h: &mut dyn Host, fov: f32) -> bool {
        let eye = self.eye(h);
        let size_y = h.player_size_y();
        let pos = h.player_pos();
        let head = v3(pos.x + 0.0, fma(size_y, 0.4, pos.y), pos.z + 0.0);
        let dir = sub3(head, eye);
        angle3(normalized(dir), Self::forward(h)) < fov * 0.5 && h.line_of_sight(eye, head)
    }

    pub fn reset_detection(&mut self) {
        self.detected = false;
        self.seen = false;
        self.detect_count = 0.0;
    }

    pub fn set_senses_active(&mut self, on: bool, ev: &mut Events) {
        if self.senses_active == on {
            return;
        }
        self.senses_active = on;
        if !on {
            ev.message(MSG_SENSES_DEACTIVATED, v3(0.0, 0.0, 0.0), 0);
        }
        self.reset_detection();
    }

    fn set_undetected(&mut self, ev: &mut Events) {
        if self.detected {
            ev.message(MSG_PLAYER_UNDETECTED, v3(0.0, 0.0, 0.0), 0);
        }
        self.detected = false;
    }

    fn detected_callback(&mut self, on: bool, h: &mut dyn Host, ev: &mut Events) {
        if !on || self.detect_min_time > self.detect_count {
            if on {
                self.detect_count += 0.15;
            } else if self.detect_count > 0.0 {
                self.detect_count -= 0.15;
            }
            self.set_undetected(ev);
            return;
        }
        let f = h.player_feet();
        self.last_known_player_pos = [f.x, f.y, f.z];
        if !self.detected {
            self.detected = true;
            ev.message(MSG_PLAYER_DETECTED, v3(0.0, 0.0, 0.0), 0);
        }
    }

    fn seen_callback(&mut self, on: bool, h: &mut dyn Host, ev: &mut Events) {
        self.seen = if on {
            self.seen_count = self.seen_count.wrapping_add(1);
            self.seen_count >= 4
        } else {
            false
        };
        if !on {
            self.seen_count = 0;
        }
        if self.seen {
            return self.detected_callback(true, h, ev);
        }
        if h.has_body() && h.body_size().x > self.player_dist(h) {
            let insight = self.player_in_sight(h, self.fov * self.fov_mul);
            return self.detected_callback(insight, h, ev);
        }
        if self.detect_count > 0.0 {
            self.detect_count -= 0.15;
        }
        self.set_undetected(ev);
    }

    pub fn update_senses(&mut self, dt: f32, h: &mut dyn Host, ev: &mut Events) {
        if !self.senses_active || !h.player_body_present() {
            return;
        }
        if h.player_dead() {
            self.set_undetected(ev);
            self.seen = false;
            return;
        }
        self.detect_timer += dt;
        if self.detect_timer < 0.15 {
            return;
        }
        self.detect_timer = 0.0;
        if !self.update_detection {
            self.seen = false;
            self.set_undetected(ev);
            return;
        }
        let mut range = self.sight_range * self.sight_range_mul;
        let mut max = -1.0f32;
        if self.sight_range_affected_by_modifiers {
            range *= h.player_visibility_range_mul();
            max = h.player_visibility_max_range();
        }
        self.current_sight_dist = if max >= 0.0 {
            std_min(range, max)
        } else {
            range
        };
        let seen = self.player_dist(h) <= self.current_sight_dist
            && self.player_in_sight(h, self.fov * self.fov_mul);
        self.seen_callback(seen, h, ev);
    }

    pub fn update_auto_disable(&mut self, dt: f32, h: &mut dyn Host, ev: &mut Events) {
        if !self.auto_disable {
            return;
        }
        self.auto_disable_timer -= dt;
        if self.auto_disable_timer > 0.0 {
            return;
        }
        self.auto_disable_timer = 0.3;
        if self.player_dist(h) < self.auto_disable_min_dist {
            return;
        }
        if h.entity_in_player_los() || h.player_dead() {
            self.auto_disable_count = 0.0;
            return;
        }
        self.auto_disable_count += 0.3;
        if self.auto_disable_count <= 3.0 {
            return;
        }
        self.auto_disable = false;
        ev.plain(EV_SET_INACTIVE);
        ev.plain(EV_AUTO_DISABLE_CALLBACK);
    }

    pub fn check_for_doors(&mut self, dt: f32, h: &mut dyn Host, ev: &mut Events) {
        self.check_doors_count -= dt;
        self.door_check_timer -= dt;
        if !self.check_for_doors
            || !h.has_body()
            || self.check_doors_count > 0.0
            || self.door_check_timer > 0.0
            || !h.has_map()
        {
            return;
        }
        self.door_check_timer = 0.1;
        if h.body_move_speed() < 0.05 {
            return;
        }
        let max = h.body_size().x * 0.5 + self.max_door_dist;
        let (pos, fwd) = (h.body_pos(), Self::forward(h));
        for i in 0..h.door_count() {
            if let Some(p) = h.door_hit(i, pos, fwd, max) {
                ev.0.push(super::host::Event {
                    kind: EV_DOOR_MESSAGE,
                    a: i as i32,
                    b: 0,
                    v: [p.x, p.y, p.z],
                });
                return;
            }
        }
    }

    pub fn pre_update(&mut self, dt: f32, h: &mut dyn Host, ev: &mut Events) {
        self.update_senses(dt, h, ev);
        self.check_for_doors(dt, h, ev);
        self.update_auto_disable(dt, h, ev);
    }

    pub fn step_up(&self, h: &mut dyn Host) {
        if !h.has_body()
            || self.global_space
            || self.static_collider
            || !h.body_test_collision()
            || !h.has_map()
        {
            return;
        }
        let feet = h.body_feet();
        let step = h.body_max_step_size();
        let dist = h.floor_dist(add3(feet, v3(0.0, step, 0.0)), feet);
        if dist < step - 0.01 {
            h.set_body_feet(add3(feet, v3(0.0, step - dist, 0.0)));
        }
    }

    pub fn align_with_ground(&mut self, dt: f32, h: &mut dyn Host) {
        if !self.align_ground || !h.has_body() || !h.has_map() || !h.body_on_ground() {
            return;
        }
        self.align_timer -= dt;
        if self.align_timer > 0.0 {
            return;
        }
        self.align_timer = 0.02;
        let yaw = h.body_yaw();
        let size_x = h.body_size().x;
        let rot = matrix_mul_vec(&mat_rotate_y(yaw), v3(0.0, 0.0, -1.0));
        let ahead = v3(
            rot.x * size_x * self.align_ray_start,
            rot.y * size_x * self.align_ray_start,
            rot.z * size_x * self.align_ray_start,
        );
        let start = add3(add3(h.body_feet(), ahead), v3(0.0, 0.05, 0.0));
        let dist = h.floor_dist(start, sub3(start, v3(0.0, self.align_ray_max, 0.0)));
        if dist > self.align_ray_max {
            return;
        }
        if let Some(slot) = self.align_dist.get_mut(self.align_idx as usize) {
            *slot = dist - 0.05;
        }
        self.align_idx = (self.align_idx + 1).rem_euclid(3);
        self.align_count = self.align_count.saturating_add(1).min(3);
        self.align_y = 0.0;
        for i in 0..self.align_count as usize {
            self.align_y +=
                self.align_dist.get(i).copied().unwrap_or(0.0) / self.align_count as f32;
        }
        let feet = h.body_feet();
        self.ground_align_pos = {
            let p = sub3(add3(feet, ahead), v3(0.0, self.align_y, 0.0));
            [p.x, p.y, p.z]
        };
    }

    /// `mesh_offset` is `cAgent::mtxMeshOffset`, `roll` the char mover's `mfRoll` (0 without one).
    pub fn sync_mesh(&mut self, mesh_offset: &Matrixf, roll: f32, h: &mut dyn Host) {
        if !h.has_body() || !h.has_mesh() {
            return;
        }
        let ent_global = h.ent_global_space_anim();
        if self.global_space != ent_global {
            self.global_space = ent_global;
            if self.global_space {
                self.gravity_before_global = h.body_gravity_active();
                self.collision_before_global = h.body_test_collision();
            }
            h.set_body_gravity(!self.global_space && self.gravity_before_global);
            h.set_body_collision(!self.global_space && self.collision_before_global);
            if self.global_space {
                h.set_body_yaw(0.0);
            }
        }
        if self.global_space {
            if !h.has_pos_bone() {
                return;
            }
            let scale = h.ent_scale_y();
            let bone = h.pos_bone_world_pos();
            let pos = v3(
                bone.x + 0.0,
                fma(self.pos_bone_y_offset, scale, bone.y),
                bone.z + 0.0,
            );
            if self.pos_bone_is_feet {
                h.set_body_feet(pos);
            } else {
                h.set_body_pos(pos);
            }
            return;
        }
        let yaw = h.body_yaw();
        let pitch = h.body_pitch();
        let amount = h.body_entity_pitch_amount();
        let mut mtx = matrix_mul(&mat_rotate_y(yaw + PI), &mat_rotate_x(-pitch * amount));
        if roll != 0.0 {
            mtx = matrix_mul(&mtx, &rotate_z(-roll));
        }
        let feet = h.body_feet();
        mtx.m[0][3] = feet.x;
        mtx.m[1][3] = feet.y;
        mtx.m[2][3] = feet.z;
        let mut offset = *mesh_offset;
        offset.m[1][3] -= self.align_y;
        h.set_mesh_matrix(&matrix_mul(&mtx, &offset));
    }
}
