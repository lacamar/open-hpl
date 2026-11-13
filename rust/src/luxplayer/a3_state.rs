//! Player state family owned by Rust: one `Base` header shared by every state, one POD per concrete state that
//! starts with it, and the `Kind` enum that picks the behaviour. The C++ state classes embed these PODs.

use super::a3_host::Ptr;
use super::states_vec::Pid3;
use crate::types::{Matrixf, Vector2f, Vector3f};

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Kind {
    Normal,
    HandObject,
    UseItem,
    Grab,
    Push,
    SwingDoor,
    Lever,
    Wheel,
    Slide,
    Ladder,
}

impl Kind {
    pub fn from_i32(i: i32) -> Option<Kind> {
        Some(match i {
            0 => Kind::Normal,
            1 => Kind::HandObject,
            2 => Kind::UseItem,
            3 => Kind::Grab,
            4 => Kind::Push,
            5 => Kind::SwingDoor,
            6 => Kind::Lever,
            7 => Kind::Wheel,
            8 => Kind::Slide,
            9 => Kind::Ladder,
            _ => return None,
        })
    }

    pub fn is_default_base(self) -> bool {
        matches!(self, Kind::Normal | Kind::HandObject | Kind::UseItem)
    }

    pub fn is_rotate(self) -> bool {
        matches!(self, Kind::SwingDoor | Kind::Lever | Kind::Wheel)
    }

    pub fn is_interact(self) -> bool {
        matches!(self, Kind::Grab | Kind::Push | Kind::Slide) || self.is_rotate()
    }
}

/// `eLuxPlayerState` values the states switch to.
pub mod ps {
    pub const NORMAL: i32 = 0;
    pub const HAND_OBJECT: i32 = 1;
    pub const GRAB: i32 = 3;
    pub const PUSH: i32 = 4;
}

/// `eLuxPlayerAction` values.
pub mod act {
    pub const INTERACT: i32 = 0;
    pub const ATTACK: i32 = 1;
    pub const HOLSTER: i32 = 3;
    pub const LANTERN: i32 = 4;
}

/// `eLuxMoveState_Normal`.
pub const MOVE_NORMAL: i32 = 0;

#[repr(C)]
pub struct Base {
    pub body_in_focus: Ptr,
    pub entity_in_focus: Ptr,
    pub cur_body: Ptr,
    pub cur_prop: Ptr,
    pub focus_pos: Vector3f,
    pub cur_focus_pos: Vector3f,
    pub focus_distance: f32,
    pub kind: i32,
    pub has_outline: bool,
    pub pad: [u8; 7],
    pub ext: Ptr,
}

/// `hpl::cLinearOscillation`; `add` is its private last member.
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct Oscillation {
    pub max: f32,
    pub min: f32,
    pub val: f32,
    pub up_speed: f32,
    pub down_speed: f32,
    pub add: f32,
}

impl Oscillation {
    pub fn set_up(&mut self, min: f32, max: f32, val: f32, up: f32, down: f32) {
        self.min = min;
        self.max = max;
        self.val = val;
        self.up_speed = up.abs();
        self.down_speed = -down.abs();
        self.add = self.up_speed;
    }

    pub fn update(&mut self, dt: f32) {
        self.val = self.add.mul_add(dt, self.val);
        if self.add > 0.0 {
            if self.val >= self.max {
                self.val = self.max;
                self.add = self.down_speed;
            }
        } else if self.val <= self.min {
            self.val = self.min;
            self.add = self.up_speed;
        }
    }
}

#[repr(C)]
pub struct Plain {
    pub base: Base,
}

#[repr(C)]
pub struct UseItem {
    pub base: Base,
    pub item: Ptr,
    pub flash: Oscillation,
    pub min_use_distance: f32,
    pub pad: u32,
}

#[repr(C)]
pub struct Grab {
    pub base: Base,
    pub data: Ptr,
    pub max_force: f32,
    pub max_torque: f32,
    pub max_angular_speed: f32,
    pub min_slow_mass: f32,
    pub max_slow_mass: f32,
    pub min_slow_mul: f32,
    pub max_leave_angular: f32,
    pub max_leave_linear: f32,
    pub local_body_offset: Vector3f,
    pub body_rotation: Matrixf,
    pub force_pid: Pid3,
    pub torque_pid: Pid3,
    pub depth: f32,
    pub max_distance: f32,
    pub mass_sum: f32,
    pub custom_materials: bool,
    pub pad: [u8; 7],
}

#[repr(C)]
pub struct Push {
    pub base: Base,
    pub data: Ptr,
    pub total_move_mul: [f32; 2],
    pub max_speed_in_dir: [f32; 2],
    pub velocity_pid: Pid3,
    pub stop_pid: Pid3,
    pub local_attach: Vector3f,
    pub push_dir: Vector3f,
    pub max_attach_dist: f32,
    pub walk_max_speed: f32,
    pub walk_force: f32,
    pub run_max_speed: f32,
    pub run_force: f32,
    pub crouch_max_speed: f32,
    pub crouch_force: f32,
    pub pitch_min_range: f32,
    pub pitch_max_range: f32,
    pub yaw_range: f32,
    pub prev_min_pitch: f32,
    pub prev_max_pitch: f32,
    pub max_force: f32,
    pub moving: [bool; 2],
    pub pad: [u8; 2],
}

#[repr(C)]
pub struct Slide {
    pub base: Base,
    pub data: Ptr,
    pub joint: Ptr,
    pub force_pid: Pid3,
    pub max_distance: f32,
    pub local_interact_pos: Vector3f,
    pub mouse_add: Vector2f,
    pub last_mouse_add: Vector2f,
    pub last_force: Vector3f,
    pub slide_speed: f32,
    pub move_to_mouse_factor: f32,
    pub max_force: f32,
    pub has_gravity: bool,
    pub pad: [u8; 7],
}

#[repr(C)]
pub struct Rotate {
    pub base: Base,
    pub data: Ptr,
    pub joint: Ptr,
    pub move_to_mouse_factor: f32,
    pub max_torque: f32,
    pub max_distance: f32,
    pub joint_right: Vector3f,
    pub joint_forward: Vector3f,
    pub force_axis: [Vector3f; 2],
    pub mouse_add: Vector2f,
    pub last_mouse_add: Vector2f,
    pub rot_speed: f32,
    pub rotate_pid: Pid3,
    pub cam_dir: [Vector3f; 3],
    pub axis_cos: [f32; 2],
    pub has_gravity: bool,
    pub pad: [u8; 3],
}

#[repr(C)]
pub struct Ladder {
    pub base: Base,
    pub ladder: Ptr,
    pub state: i32,
    pub time_count: f32,
    pub start_position: Vector3f,
    pub char_position: Vector3f,
    pub goal_pos: Vector3f,
    pub goal_rot: Vector3f,
    pub pos_add: Vector3f,
    pub rot_add: Vector3f,
    pub move_mul: f32,
    pub up_speed: f32,
    pub down_speed: f32,
    pub step_length: f32,
    pub step_count: f32,
    pub pitch_max_limit: f32,
    pub pitch_min_limit: f32,
    pub leave_at_top: f32,
    pub played_sound: bool,
    pub lantern_drawn: bool,
    pub pad: [u8; 6],
}
