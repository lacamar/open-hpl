//! What the agent logic asks of C++: body, mesh and player queries, raycasts. Every call is a
//! pure query or an engine command that cannot re-enter the agent; script calls and messages go
//! through `Events` instead.

use super::charmover::AvoidBody;
use super::pathfinder::TrackNodeInfo;
use crate::math::{Matrixf, Vector3f};

pub const MSG_PLAYER_DETECTED: i32 = 8;
pub const MSG_PLAYER_UNDETECTED: i32 = 9;
pub const MSG_SENSES_DEACTIVATED: i32 = 17;

pub const EV_MESSAGE: u32 = 1;
pub const EV_SET_INACTIVE: u32 = 2;
pub const EV_AUTO_DISABLE_CALLBACK: u32 = 3;
pub const EV_DOOR_MESSAGE: u32 = 4;

/// One deferred script-visible effect, replayed by C++ in order after the step.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Event {
    pub kind: u32,
    pub a: i32,
    pub b: i32,
    pub v: [f32; 3],
}

#[derive(Default)]
pub struct Events(pub Vec<Event>);

impl Events {
    pub fn message(&mut self, id: i32, v: Vector3f, i: i32) {
        self.0.push(Event {
            kind: EV_MESSAGE,
            a: id,
            b: i,
            v: [v.x, v.y, v.z],
        });
    }

    pub fn plain(&mut self, kind: u32) {
        self.0.push(Event {
            kind,
            ..Event::default()
        });
    }
}

pub trait Host {
    fn has_body(&mut self) -> bool;
    fn has_map(&mut self) -> bool;
    fn has_mesh(&mut self) -> bool;
    fn ent_pos(&mut self) -> Vector3f;
    fn ent_scale_y(&mut self) -> f32;
    fn ent_global_space_anim(&mut self) -> bool;
    fn body_pos(&mut self) -> Vector3f;
    fn body_feet(&mut self) -> Vector3f;
    fn body_size(&mut self) -> Vector3f;
    fn body_forward(&mut self) -> Vector3f;
    fn body_yaw(&mut self) -> f32;
    fn body_pitch(&mut self) -> f32;
    fn body_entity_pitch_amount(&mut self) -> f32;
    fn body_move_speed(&mut self) -> f32;
    fn body_on_ground(&mut self) -> bool;
    fn body_test_collision(&mut self) -> bool;
    fn body_gravity_active(&mut self) -> bool;
    fn body_max_step_size(&mut self) -> f32;
    fn set_body_yaw(&mut self, yaw: f32);
    fn set_body_pos(&mut self, p: Vector3f);
    fn set_body_feet(&mut self, p: Vector3f);
    fn set_body_gravity(&mut self, on: bool);
    fn set_body_collision(&mut self, on: bool);
    fn has_pos_bone(&mut self) -> bool;
    fn pos_bone_world_pos(&mut self) -> Vector3f;
    fn set_mesh_matrix(&mut self, m: &Matrixf);

    fn player_body_present(&mut self) -> bool;
    fn player_dead(&mut self) -> bool;
    fn player_visibility_range_mul(&mut self) -> f32;
    fn player_visibility_max_range(&mut self) -> f32;
    fn player_pos(&mut self) -> Vector3f;
    fn player_feet(&mut self) -> Vector3f;
    fn player_size_y(&mut self) -> f32;

    fn line_of_sight(&mut self, from: Vector3f, to: Vector3f) -> bool;
    fn entity_in_player_los(&mut self) -> bool;
    fn floor_dist(&mut self, start: Vector3f, end: Vector3f) -> f32;
    fn door_count(&mut self) -> usize;
    fn body_last_pos(&mut self) -> Vector3f;
    fn body_right(&mut self) -> Vector3f;
    fn body_velocity(&mut self, dt: f32) -> Vector3f;
    fn body_add_pitch(&mut self, p: f32);
    fn body_set_max_forward(&mut self, v: f32);
    fn body_set_move_acc(&mut self, v: f32);
    fn body_set_move_deacc(&mut self, v: f32);
    fn body_move_forward(&mut self);
    /// `SomaRaycast` from the agent's entity: hit distance and normal.
    fn raycast(&mut self, start: Vector3f, end: Vector3f) -> Option<(f32, Vector3f)>;
    fn avoid_query(&mut self, center: Vector3f, dist: f32) -> usize;
    fn avoid_get(&mut self, i: usize) -> AvoidBody;
    fn anim_current(&mut self) -> i32;
    fn anim_name_nonempty(&mut self, state: i32) -> bool;
    fn play_move_anim(&mut self, state: i32);
    fn set_move_anim_speed(&mut self, speed: f32);
    fn idle_restart(&mut self);
    fn idle_extra_length(&mut self, idx: i32) -> Option<f32>;
    fn play_idle_extra(&mut self, idx: i32);
    fn pf_feet(&mut self) -> Vector3f;
    fn pf_path_len(&mut self) -> usize;
    fn pf_path_get(&mut self, i: usize) -> Vector3f;
    fn pf_build_path(&mut self, goal: Vector3f);
    fn pf_track_len(&mut self) -> usize;
    fn pf_track_info(&mut self, i: usize) -> TrackNodeInfo;
    fn pf_track_node_pos(&mut self, i: usize) -> Option<Vector3f>;
    fn door_hit(
        &mut self,
        idx: usize,
        origin: Vector3f,
        dir: Vector3f,
        max: f32,
    ) -> Option<Vector3f>;
}
