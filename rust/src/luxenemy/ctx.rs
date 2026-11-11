//! Pointers into the C++ enemy, built once per enemy by C++ (`OhplGa3Ctx`). The enemy state
//! stays in C++ members so subclasses and save code keep reading them; Rust reads and writes
//! them in place through short-lived accesses (a host call may alias them).

use super::host::{Cmd, Host, Io};
use super::machine::{Queue, StatePtrs, send_message};
use super::mover::MoverState;
use super::pathfinder::PfWin;
use crate::math::{Vector3f, v3};

pub type P<T> = *mut T;

#[repr(C)]
pub struct Ctx {
    pub queue: P<Queue>,
    pub cur: P<i32>,
    pub next: P<i32>,
    pub prev: P<i32>,
    pub disabled: P<bool>,
    pub mover: P<MoverState>,
    pub pf_moving: P<bool>,
    pub pf_goal: P<f32>,
    pub pf_win: P<PfWin>,
    pub look_count: P<f32>,
    pub los_count: P<i32>,
    pub can_see: P<bool>,
    pub detected: P<bool>,
    pub in_range: P<bool>,
    pub check_door_count: P<f32>,
    pub stuck_at_door: P<bool>,
    pub stuck_door_id: P<i32>,
    pub last_pos_count: P<f32>,
    pub last_known: P<f32>,
    pub fov_mul: P<f32>,
    pub sight_range: P<f32>,
    pub health: P<f32>,
    pub blind: P<bool>,
    pub darkness_sight_range: P<f32>,
    pub in_darkness_light_level: P<f32>,
    pub crouch_visible_range_mul: P<f32>,
    pub skip_visibility_handicaps: P<bool>,
    pub fov: P<f32>,
    pub fov_x_mul: P<f32>,
    pub activation_distance: P<f32>,
    pub max_regen_health: P<f32>,
    pub regen_health_speed: P<f32>,
    pub damage_count: P<f32>,
    pub temp_pos: P<f32>,
    pub temp_val: P<f32>,
    pub temp_val_l: P<f32>,
    pub attack_hit_counter: P<i32>,
    pub forward_speed: P<f32>,
    pub backward_speed: P<f32>,
    pub forward_acc: P<f32>,
    pub forward_deacc: P<f32>,
    pub def_forward_speed: P<[[f32; 2]; 2]>,
    pub def_backward_speed: P<[[f32; 2]; 2]>,
    pub def_forward_acc: P<[[f32; 2]; 2]>,
    pub def_forward_deacc: P<[[f32; 2]; 2]>,
    pub pose: P<i32>,
    pub start_position: P<f32>,
    pub hear_volume: P<f32>,
    pub player_search_time: P<f32>,
    pub normal_attack_distance: P<f32>,
    pub cur_patrol: P<i32>,
    pub is_seen_by_player: P<bool>,
    pub auto_remove_at_path_end: P<bool>,
    pub wait_time: P<f32>,
    pub alert_run_towards_count: P<f32>,
    pub groggy_damage_count: P<f32>,
    pub alert_to_hunt_distance: P<f32>,
    pub alert_to_instant_hunt_distance: P<f32>,
    pub hunt_pause_min_time: P<f32>,
    pub hunt_pause_max_time: P<f32>,
    pub increase_alert_speed_distance: P<f32>,
    pub increased_alert_speed_mul: P<f32>,
    pub alert_run_towards_to_hunt_limit: P<f32>,
    pub alert_run_towards_check_distance: P<f32>,
    pub idle_extra_time_min: P<f32>,
    pub idle_extra_time_max: P<f32>,
    pub idle_extra_num: P<i32>,
}

pub fn g<T: Copy>(p: P<T>) -> T {
    // SAFETY: C++ built the pointer from a live member of the enemy that outlives the call.
    unsafe { *p }
}

pub fn s<T>(p: P<T>, v: T) {
    // SAFETY: as in `g`.
    unsafe { *p = v }
}

pub fn gv(p: P<f32>) -> Vector3f {
    // SAFETY: `p` points at a `cVector3f` (three floats).
    unsafe { v3(*p, *p.add(1), *p.add(2)) }
}

pub fn sv(p: P<f32>, v: Vector3f) {
    // SAFETY: as in `gv`.
    unsafe {
        *p = v.x;
        *p.add(1) = v.y;
        *p.add(2) = v.z;
    }
}

pub struct Env<'a> {
    pub c: &'a Ctx,
    pub h: &'a mut dyn Host,
    pub cmds: Vec<Cmd>,
}

impl<'a> Env<'a> {
    pub fn new(c: &'a Ctx, h: &'a mut dyn Host) -> Self {
        Self {
            c,
            h,
            cmds: Vec::new(),
        }
    }

    pub fn st(&self) -> StatePtrs {
        StatePtrs {
            cur: self.c.cur,
            next: self.c.next,
            prev: self.c.prev,
        }
    }

    pub fn change_state(&self, s: i32) {
        self.st().change_state(s);
    }

    pub fn send(&mut self, ty: i32, time: f32, local: bool, v: Vector3f, f: f32, i: i32) {
        if g(self.c.disabled) {
            return;
        }
        let trig = self.h.triggers_disabled();
        let cur = g(self.c.cur);
        let blocked = trig
            && ty > super::machine::MSG_END_OF_PATH
            && ty != super::machine::MSG_PLAYER_IN_RANGE
            && ty != super::machine::MSG_PLAYER_OUT_OF_RANGE;
        if !blocked && ty == super::machine::MSG_PLAYER_IN_RANGE {
            self.h.debug_msg(100, 0.0);
        }
        send_message(self.c.queue, cur, false, trig, ty, time, local, v, f, i);
    }

    pub fn send0(&mut self, ty: i32, time: f32, local: bool) {
        self.send(ty, time, local, v3(0.0, 0.0, 0.0), 0.0, 0);
    }

    pub fn cmd(&mut self, op: u32, io: Io) {
        self.cmds.push(Cmd { op, io });
    }

    pub fn flush(&mut self) {
        self.h.flush(&mut self.cmds);
    }
}
