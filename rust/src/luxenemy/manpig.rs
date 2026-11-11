//! `cLuxEnemy_ManPig`: the state machine, patrol/flee/stalk/track steering decisions and the
//! lantern-exposure timer. Body, animation, sound and frustum calls stay C++ behind the
//! host table (ops from `OP_PIG_BASE` up, animation kinds from `ANIM_POSE_BASE` up).

use super::ctx::{Ctx, Env, P, g, gv, s, sv};
use super::ffi::OhplGa3Host;
use super::grunt::{self, inc_patrol};
use super::host::{CMD_RUN_CALLBACK, Host, Io, a_of, v_of};
use super::machine::*;
use super::mover::MoverState;
use super::pathfinder::{NodeQuery, next_goal_pos, node_at_pos};
use super::{angle_dist_rad, normalized};
use crate::math::{PI, Vector3f, fma, rand_rectf, rand_rectl, to_rad, v3};

pub const S_IDLE: i32 = 0;
pub const S_GO_HOME: i32 = 1;
pub const S_WAIT: i32 = 2;
pub const S_PATROL: i32 = 3;
pub const S_INVESTIGATE: i32 = 4;
pub const S_ALERT: i32 = 5;
pub const S_SEARCH: i32 = 6;
pub const S_EAT: i32 = 7;
pub const S_HURT: i32 = 8;
pub const S_HUNT: i32 = 9;
pub const S_HUNT_PAUSE: i32 = 10;
pub const S_HUNT_WANDER: i32 = 11;
pub const S_ATTACK_SHORT: i32 = 12;
pub const S_ATTACK_LONG: i32 = 13;
pub const S_BREAK_DOOR: i32 = 15;
pub const S_DEAD: i32 = 16;
pub const S_FLEE: i32 = 18;
pub const S_STALK: i32 = 19;
pub const S_TRACK: i32 = 20;

pub const MSG_TIMEOUT_4: i32 = 3;
pub const MSG_FOOD_IN_RANGE: i32 = 8;
pub const MSG_CHANGE_POSE: i32 = 17;

pub const SOUND_SILENT: i32 = 0;
pub const SOUND_IDLE: i32 = 1;
pub const SOUND_ALERT: i32 = 2;
pub const SOUND_HUNT: i32 = 3;
pub const MUSIC_SEARCH: i32 = 0;
pub const MUSIC_ATTACK: i32 = 1;
pub const WALK: usize = 0;
pub const RUN: usize = 1;

pub const SND_NOTICE: i32 = 0;
pub const SND_GIVE_UP: i32 = 1;
pub const SND_ENABLED: i32 = 2;

const IDLE_NONE: i32 = 0;
const IDLE_STALK: i32 = 1;
const IDLE_TRACK: i32 = 2;

pub const OP_SEEN_BY_PLAYER: u32 = 200;
pub const OP_IN_FOV_AT_FEET: u32 = 201;
pub const OP_VISIBLE_AT_FEET: u32 = 202;
pub const OP_TELEPORT_FEET: u32 = 203;
pub const OP_BODY_SET_MOVE_DELAY: u32 = 204;
pub const OP_PLAY_HUNT_SOUND: u32 = 205;
pub const OP_INSIDE_PLAYER_VIEW: u32 = 206;
pub const OP_PATROL_NODE_INFO: u32 = 207;
pub const OP_ATTACK_EX: u32 = 208;
pub const OP_FORCE_TESLA: u32 = 209;
pub const OP_PLAYER_YAW: u32 = 210;

pub const ANIM_IDLE_EXTRA_POSE: i32 = 100;
pub const ANIM_NOTICE_POSE: i32 = 101;
pub const ANIM_THREAT_LOOP: i32 = 102;
pub const ANIM_ATTACK_POSE: i32 = 103;
pub const ANIM_BITE: i32 = 104;
pub const ANIM_CHARGE: i32 = 105;
pub const ANIM_FLINCH_POSE: i32 = 106;
pub const ANIM_DEAD_PLAIN: i32 = 107;
pub const ANIM_PATROL_NODE: i32 = 108;

/// Pointers into the ManPig members that the handlers use beyond the shared `Ctx`.
#[repr(C)]
pub struct PigCtx {
    pub last_short_miss: P<bool>,
    pub force_charge: P<bool>,
    pub flee_invisible_count: P<f32>,
    pub threaten_on_alert: P<bool>,
    pub flee_from_player: P<bool>,
    pub auto_disable_after_flee: P<bool>,
    pub idle_behavior: P<i32>,
    pub patrol_move_speed: P<i32>,
    pub allow_zero_wait: P<bool>,
    pub damage_mul: P<f32>,
    pub run_speed_mul: P<f32>,
    pub play_activate_sound: P<bool>,
    pub hunt_pause_time_mul: P<f32>,
    pub is_tesla: P<bool>,
    pub tesla_notice: P<f32>,
    pub in_lantern_count: P<f32>,
    pub check_flash_light: P<f32>,
    pub lantern_sensitivity: P<f32>,
    pub disable_triggers: P<bool>,
}

pub(super) struct PHost<'a>(pub &'a OhplGa3Host);

impl Host for PHost<'_> {
    fn call(&mut self, op: u32, io: &mut Io) -> i32 {
        // SAFETY: the caller of the entry point guarantees the table accepts its `ctx`.
        unsafe { (self.0.call)(self.0.ctx, op, io) }
    }
}

pub(super) fn call(h: &mut dyn Host, op: u32, io: &mut Io) -> i32 {
    h.call(op, io)
}

pub(super) fn call_v(h: &mut dyn Host, op: u32, v: Vector3f) -> i32 {
    let mut io = Io::default();
    io.v[0] = a_of(v);
    h.call(op, &mut io)
}

pub(super) fn set_move_speed(e: &mut Env, t: usize) {
    let c = e.c;
    let pose = g(c.pose).clamp(0, 1) as usize;
    let at = |a: *mut [[f32; 2]; 2]| {
        g(a).get(pose)
            .and_then(|r| r.get(t))
            .copied()
            .unwrap_or(0.0)
    };
    s(c.forward_speed, at(c.def_forward_speed));
    s(c.backward_speed, at(c.def_backward_speed));
    s(c.forward_acc, at(c.def_forward_acc));
    s(c.forward_deacc, at(c.def_forward_deacc));
}

pub(super) fn mul_fwd(e: &mut Env, m: f32) {
    s(e.c.forward_speed, g(e.c.forward_speed) * m);
}

/// `FadeOutCurrentAnim(0.2)`, which ends in `UseMoveStateAnimations` when it faded.
pub(super) fn fade_out(e: &mut Env) {
    if e.h.fade_out_anim(0.2) {
        mover(e.c).use_move_state_animations();
    }
}

pub(super) fn mover<'x>(c: &Ctx) -> &'x mut MoverState {
    // SAFETY: C++ aliases the mover only inside host calls, which are not in flight here.
    unsafe { &mut *c.mover }
}

pub(super) fn break_door_state(e: &mut Env) {
    let id = g(e.c.stuck_door_id);
    let p = e.h.entity_attach_pos(id);
    sv(e.c.temp_pos, p);
    e.change_state(S_BREAK_DOOR);
}

pub(super) fn sound_heard(e: &mut Env, m: &Msg) {
    if m.custom_f > g(e.c.hear_volume) {
        e.change_state(S_INVESTIGATE);
        sv(e.c.temp_pos, v_of(m.custom_v));
        s(e.c.temp_val, m.custom_f);
    }
}

unsafe extern "C" {
    fn ohpl_math_vector3_angle(a: *const Vector3f, b: *const Vector3f) -> f32;
}

fn angle_between(a: Vector3f, b: Vector3f) -> f32 {
    // SAFETY: pure function on pointers to live locals.
    unsafe { ohpl_math_vector3_angle(&a, &b) }
}

/// `iLuxEnemy::GetDirection2D`.
fn dir_2d(e: &mut Env, p: Vector3f) -> Vector3f {
    let b = e.h.body_pos();
    let mut d = v3(p.x - b.x, p.y - b.y, p.z - b.z);
    d.y = 0.0;
    normalized(d)
}

pub struct Pig<'a, 'b, 'c> {
    pub e: &'a mut Env<'b>,
    pub p: &'c PigCtx,
}

impl Pig<'_, '_, '_> {
    fn anim(&mut self, kind: i32, n: i32, looped: bool, fade: f32, use_move: bool) {
        let a = super::AnimReq {
            kind,
            n,
            looped,
            fade,
            depends_on_speed: false,
            speed_mul: 1.0,
            sync: false,
            override_move: true,
            use_move_when_over: use_move,
        };
        if self.e.h.play_anim(&a) {
            mover(self.e.c).override_move = true;
        }
    }

    fn move_to(&mut self, p: Vector3f) -> bool {
        self.e.h.pf_move_to(p)
    }

    fn fov(&mut self, m: f32) {
        s(self.e.c.fov_mul, m);
    }

    fn speed(&mut self, t: usize) {
        set_move_speed(self.e, t);
    }

    fn run_speed(&mut self) {
        self.speed(RUN);
        mul_fwd(self.e, g(self.p.run_speed_mul));
    }

    fn detected(&mut self) -> bool {
        grunt::player_is_detected(self.e)
    }

    fn force_tesla(&mut self) {
        if g(self.p.is_tesla) {
            call(self.e.h, OP_FORCE_TESLA, &mut Io::default());
        }
    }

    fn attack(&mut self, which: i32) -> bool {
        let mut io = Io::default();
        io.i[0] = which;
        call(self.e.h, OP_ATTACK_EX, &mut io) != 0
    }

    fn query_at(&mut self, op: u32, p: Vector3f) -> bool {
        call_v(self.e.h, op, p) != 0
    }

    fn hunt_sound(&mut self) {
        call(self.e.h, OP_PLAY_HUNT_SOUND, &mut Io::default());
    }

    fn wait_for(&mut self, t: f32) {
        s(self.e.c.wait_time, t);
        self.e.change_state(S_WAIT);
    }

    fn node(&mut self, pos: Vector3f, q: &NodeQuery) -> Option<Vector3f> {
        node_at_pos(self.e.h, pos, q).map(|(_, p)| p)
    }

    fn node_ray1(&mut self, pos: Vector3f) -> Option<Vector3f> {
        let mut q = NodeQuery::new(0.0, 10.0, true, true, true, 0);
        q.ray_num = 1;
        self.node(pos, &q)
    }

    /// `CheckEnemyAutoRemoval`; the script callback is replayed by the caller's flush.
    fn auto_removal(&mut self, d: f32) -> bool {
        let c = self.e.c;
        if !g(c.is_seen_by_player) && self.e.dist_to_player() > d && g(c.auto_remove_at_path_end) {
            self.e.h.set_active_false();
            self.e.cmd(CMD_RUN_CALLBACK, Io::default());
            return true;
        }
        false
    }

    fn patrol_update_goal(&mut self) {
        let c = self.e.c;
        if self.e.h.patrol_count() == 0 {
            match g(self.p.idle_behavior) {
                IDLE_NONE => self.e.change_state(S_WAIT),
                IDLE_STALK => self.e.change_state(S_STALK),
                IDLE_TRACK => self.e.change_state(S_TRACK),
                _ => {}
            }
            return;
        }
        // A cursor past the end made the C++ dereference null; nothing is moved here.
        if let Some(p) = self.e.h.patrol_node_pos(g(c.cur_patrol)) {
            self.move_to(p);
        }
    }

    fn flee_try_find_safe_node(&mut self) -> bool {
        let length = 20.0f32;
        let pp = self.e.h.player_pos();
        let dir_player = dir_2d(self.e, pp);
        for _ in 0..10 {
            let bp = self.e.h.body_pos();
            let at = v3(
                fma(dir_player.x * length, 0.2, bp.x),
                fma(dir_player.y * length, 0.2, bp.y),
                fma(dir_player.z * length, 0.2, bp.z),
            );
            let q = NodeQuery::new(0.0, length, false, false, true, 0);
            // A null node made the C++ dereference null (`&&` where `||` was meant); it is skipped.
            let Some(n) = self.node(at, &q) else {
                continue;
            };
            self.move_to(n);
            let next = next_goal_pos(self.e.h, gv(self.e.c.pf_goal));
            let dir_node = dir_2d(self.e, next);
            if angle_between(dir_player, dir_node) < to_rad(45.0) {
                self.e.h.pf_stop();
                continue;
            }
            return true;
        }
        false
    }

    fn visible_at(&mut self, p: Vector3f) -> bool {
        self.query_at(OP_VISIBLE_AT_FEET, p)
    }

    fn stalk_find_node(&mut self) -> bool {
        for _ in 0..10 {
            let pf = self.e.h.player_feet();
            let q = NodeQuery::new(7.0, 30.0, false, false, true, 0);
            let Some(n) = self.node(pf, &q) else {
                return false;
            };
            let pp = self.e.h.player_pos();
            let (dx, dz) = (pp.x - n.x, pp.z - n.z);
            let dist = fma(dx, dx, dz * dz).sqrt();
            if dist < 15.5 && self.visible_at(n) {
                continue;
            }
            self.move_to(n);
            let next = next_goal_pos(self.e.h, gv(self.e.c.pf_goal));
            if self.visible_at(next) {
                self.e.h.pf_stop();
                continue;
            }
            return true;
        }
        false
    }

    fn track_find_node(&mut self) -> bool {
        let dist = self.e.dist_to_player_2d();
        let max_d = if dist * 0.65 > 3.5 { dist * 0.65 } else { 3.5 };
        let min_d = if dist * 0.25 > 1.5 { dist * 0.25 } else { 1.5 };
        let pf = self.e.h.player_feet();
        let q = NodeQuery::new(min_d, max_d, false, false, true, 0);
        // The first of the ten tries always returns.
        match self.node(pf, &q) {
            None => false,
            Some(n) => {
                self.move_to(n);
                true
            }
        }
    }

    fn track_teleport_behind_player(&mut self) -> bool {
        let dist = self.e.dist_to_player_2d();
        for i in 0..10 {
            let pf = self.e.h.player_feet();
            let q = NodeQuery::new(dist * 0.5, dist, false, true, false, 0);
            let Some(n) = self.node(pf, &q) else {
                return false;
            };
            if i == 9 {
                self.move_to(n);
                return true;
            }
            if self.visible_at(n) {
                continue;
            }
            call_v(self.e.h, OP_TELEPORT_FEET, n);
            return true;
        }
        false
    }

    fn default_state(&mut self, mt: Option<i32>) -> bool {
        let c = self.e.c;
        match mt {
            Some(MSG_RESET) => {
                self.e.h.music(false, MUSIC_ATTACK);
                self.e.h.player_remove_terror_enemy();
                s(self.p.last_short_miss, false);
                s(self.p.force_charge, false);
                self.e.change_state(S_WAIT);
                true
            }
            Some(MSG_PLAYER_DETECTED) => {
                self.e.change_state(S_ALERT);
                true
            }
            Some(MSG_TAKE_HIT | MSG_CHANGE_POSE) => true,
            Some(MSG_PLAYER_OUT_OF_RANGE) => {
                self.e.h.set_active_false();
                true
            }
            Some(MSG_PLAYER_DEAD) => {
                self.e.change_state(S_IDLE);
                true
            }
            Some(MSG_HELP_ME) => {
                let f = self.e.h.player_feet();
                sv(c.last_known, f);
                self.e.change_state(S_HUNT);
                true
            }
            _ => false,
        }
    }

    pub fn state_event(&mut self, state: i32, ev: i32, m: Option<&Msg>) -> bool {
        let mt = match (ev, m) {
            (EV_MESSAGE, Some(m)) => Some(m.ty),
            _ => None,
        };
        if state < 0 {
            return self.default_state(mt);
        }
        let overload = matches!(mt, Some(MSG_PLAYER_DETECTED | MSG_HELP_ME));
        match state {
            S_IDLE => self.idle_state(ev, mt),
            S_GO_HOME => self.go_home_state(ev, mt, m),
            S_WAIT => self.wait_state(ev, mt, m),
            S_PATROL => self.patrol_state(ev, mt, m),
            S_INVESTIGATE => self.investigate_state(ev, mt, m),
            S_ALERT => self.alert_state(ev, mt, m),
            S_SEARCH => self.search_state(ev, mt, m),
            S_FLEE => self.flee_state(ev, mt, overload),
            S_STALK => self.stalk_state(ev, mt),
            S_TRACK => self.track_state(ev, mt, m),
            S_HUNT => self.hunt_state(ev, mt, m, overload),
            S_HUNT_WANDER => self.hunt_wander_state(ev, mt, overload),
            S_HUNT_PAUSE => self.hunt_pause_state(ev, mt, overload),
            S_BREAK_DOOR => self.break_door_handler(ev, mt, overload),
            S_ATTACK_SHORT => self.attack_short_state(ev, mt, overload),
            S_ATTACK_LONG => self.attack_long_state(ev, mt, overload),
            S_HURT => self.hurt_state(ev, mt, overload),
            S_DEAD => match (ev, mt) {
                (EV_ENTER, _) => {
                    self.e.h.pf_stop();
                    self.anim(ANIM_DEAD_PLAIN, 0, false, 0.3, false);
                    self.e.h.player_remove_terror_enemy();
                    self.e.h.music(false, MUSIC_ATTACK);
                    self.e.h.body_set_active(false);
                    true
                }
                (_, Some(MSG_ANIMATION_OVER)) => true,
                _ => overload,
            },
            _ => {
                self.e.h.fatal_state(state);
                false
            }
        }
    }

    fn idle_state(&mut self, ev: i32, mt: Option<i32>) -> bool {
        match (ev, mt) {
            (EV_ENTER, _) => {
                s(self.e.c.in_range, false);
                self.speed(WALK);
                true
            }
            (_, Some(MSG_PLAYER_IN_RANGE)) => {
                self.e.h.debug_msg(1, 0.0);
                if self.e.h.activate_sound_allowed() && g(self.p.play_activate_sound) {
                    self.e.h.set_activate_sound_made();
                    self.e.h.play_sound(SND_ENABLED);
                }
                self.e.change_state(S_PATROL);
                true
            }
            (_, Some(MSG_PLAYER_DETECTED)) => true,
            _ => false,
        }
    }

    fn go_home_state(&mut self, ev: i32, mt: Option<i32>, m: Option<&Msg>) -> bool {
        match (ev, mt) {
            (EV_ENTER, _) => {
                let p = gv(self.e.c.start_position);
                self.move_to(p);
                self.speed(WALK);
                true
            }
            (_, Some(MSG_END_OF_PATH)) => {
                self.e.change_state(S_IDLE);
                true
            }
            (_, Some(MSG_SOUND_HEARD)) => {
                if let Some(m) = m {
                    sound_heard(self.e, m);
                }
                true
            }
            _ => false,
        }
    }

    fn wait_state(&mut self, ev: i32, mt: Option<i32>, m: Option<&Msg>) -> bool {
        let c = self.e.c;
        match (ev, mt) {
            (EV_ENTER, _) => {
                if g(c.wait_time) <= 0.0 {
                    if g(self.p.allow_zero_wait) {
                        self.e.send0(MSG_TIMEOUT, 0.01, true);
                    } else {
                        let t = rand_rectf(1.0, 3.0);
                        self.e.send0(MSG_TIMEOUT, t, true);
                    }
                } else {
                    let t = g(c.wait_time);
                    self.e.send0(MSG_TIMEOUT, t, true);
                }
                s(c.wait_time, 0.0);
                let t = rand_rectf(2.0, 5.0);
                self.e.send0(MSG_TIMEOUT_2, t, true);
                self.speed(WALK);
                self.e.h.pf_stop();
                true
            }
            (EV_LEAVE, _) => {
                mover(self.e.c).override_move = false;
                true
            }
            (_, Some(MSG_TIMEOUT)) => {
                let prev = self.e.st().prev();
                if matches!(prev, S_SEARCH | S_STALK | S_TRACK) {
                    fade_out(self.e);
                    self.e.change_state(prev);
                } else if self.e.h.patrol_count() > 0 || g(self.p.idle_behavior) != IDLE_NONE {
                    fade_out(self.e);
                    self.e.change_state(S_PATROL);
                } else {
                    let t = rand_rectf(3.0, 5.0);
                    self.e.send0(MSG_TIMEOUT, t, true);
                }
                true
            }
            (_, Some(MSG_TIMEOUT_2)) => {
                let n = rand_rectl(1, 2);
                self.anim(ANIM_IDLE_EXTRA_POSE, n, false, 0.3, true);
                true
            }
            (_, Some(MSG_ANIMATION_OVER)) => {
                let t = rand_rectf(4.0, 13.0);
                self.e.send0(MSG_TIMEOUT_2, t, true);
                true
            }
            (_, Some(MSG_SOUND_HEARD)) => {
                if let Some(m) = m {
                    sound_heard(self.e, m);
                }
                true
            }
            _ => false,
        }
    }

    fn patrol_state(&mut self, ev: i32, mt: Option<i32>, m: Option<&Msg>) -> bool {
        let c = self.e.c;
        match (ev, mt) {
            (EV_ENTER, _) => {
                self.e.h.music(false, MUSIC_SEARCH);
                self.e.h.music(false, MUSIC_ATTACK);
                self.e.h.player_remove_terror_enemy();
                self.e.h.change_sound_state(SOUND_IDLE);
                let ps = g(self.p.patrol_move_speed);
                self.speed(ps.clamp(0, 1) as usize);
                if ps == 1 {
                    mul_fwd(self.e, g(self.p.run_speed_mul));
                }
                self.patrol_update_goal();
                true
            }
            (EV_UPDATE, _) => {
                if g(c.stuck_at_door) {
                    break_door_state(self.e);
                }
                true
            }
            (_, Some(MSG_END_OF_PATH)) => {
                let cur = g(c.cur_patrol);
                let count = self.e.h.patrol_count();
                if cur >= count - 1 && self.auto_removal(10.0) {
                    return true;
                }
                let mut io = Io::default();
                io.i[0] = cur;
                let info = call(self.e.h, OP_PATROL_NODE_INFO, &mut io);
                let wait = io.f[0];
                match info {
                    1 => self.wait_for(wait),
                    2 | 3 => {
                        self.anim(ANIM_PATROL_NODE, cur, info == 3, 0.2, true);
                        if info == 3 {
                            self.e.send0(MSG_TIMEOUT, wait, true);
                            s(c.wait_time, 0.0);
                        } else {
                            s(c.wait_time, wait);
                        }
                    }
                    _ => self.wait_for(0.0),
                }
                inc_patrol(c.cur_patrol, self.e.h.patrol_count(), true);
                true
            }
            (_, Some(MSG_ANIMATION_OVER)) => {
                self.e.change_state(S_WAIT);
                true
            }
            (_, Some(MSG_TIMEOUT)) => {
                mover(self.e.c).use_move_state_animations();
                self.e.change_state(S_WAIT);
                true
            }
            (_, Some(MSG_SOUND_HEARD)) => {
                if let Some(m) = m {
                    sound_heard(self.e, m);
                }
                true
            }
            _ => false,
        }
    }

    fn investigate_state(&mut self, ev: i32, mt: Option<i32>, m: Option<&Msg>) -> bool {
        let c = self.e.c;
        match (ev, mt) {
            (EV_ENTER, _) => {
                self.e.h.change_sound_state(SOUND_ALERT);
                self.speed(WALK);
                self.e.h.pf_stop();
                self.anim(ANIM_NOTICE_POSE, 0, false, 0.3, true);
                self.e.h.play_sound(SND_NOTICE);
                self.fov(4.0);
                let tv = g(c.temp_val);
                self.e.h.debug_msg(2, tv);
                self.force_tesla();
                true
            }
            (EV_LEAVE, _) => {
                self.fov(1.0);
                fade_out(self.e);
                self.speed(WALK);
                true
            }
            (_, Some(MSG_ANIMATION_OVER)) => {
                self.speed(WALK);
                let tp = gv(c.temp_pos);
                if let Some(p) = self.node_ray1(tp) {
                    self.move_to(p);
                } else {
                    self.e.h.debug_msg(3, 0.0);
                    self.move_to(tp);
                }
                true
            }
            (_, Some(MSG_END_OF_PATH)) => {
                self.e.h.play_sound(SND_GIVE_UP);
                if self.e.st().prev() == S_SEARCH {
                    self.e.change_state(S_SEARCH);
                } else {
                    self.e.change_state(S_PATROL);
                }
                true
            }
            (EV_UPDATE, _) => {
                if g(c.stuck_at_door) {
                    break_door_state(self.e);
                }
                true
            }
            (_, Some(MSG_SOUND_HEARD)) => {
                if let Some(m) = m
                    && g(c.pf_moving)
                    && m.custom_f > g(c.temp_val)
                {
                    let tp = gv(c.temp_pos);
                    if let Some(p) = self.node_ray1(tp) {
                        self.move_to(p);
                    }
                    s(c.temp_val, m.custom_f);
                }
                true
            }
            _ => false,
        }
    }

    fn alert_state(&mut self, ev: i32, mt: Option<i32>, m: Option<&Msg>) -> bool {
        let c = self.e.c;
        let threaten = g(self.p.threaten_on_alert);
        let flee = g(self.p.flee_from_player);
        let tesla = g(self.p.is_tesla);
        match (ev, mt) {
            (EV_ENTER, _) => {
                self.force_tesla();
                if flee && !threaten {
                    self.e.change_state(S_FLEE);
                } else {
                    self.e.h.change_sound_state(SOUND_ALERT);
                    self.e.h.player_add_terror_enemy();
                    if threaten {
                        let len = if flee { 3.0 } else { 6.0 };
                        let first = if flee { 1.0 } else { 3.5 };
                        self.e.h.pf_stop();
                        self.anim(ANIM_THREAT_LOOP, 0, true, 0.3, true);
                        self.e.send0(MSG_TIMEOUT_2, len, true);
                        self.e.send0(MSG_TIMEOUT_3, first, true);
                        s(c.skip_visibility_handicaps, true);
                    } else {
                        let lk = gv(c.last_known);
                        self.move_to(lk);
                        self.speed(WALK);
                        self.e.send0(MSG_TIMEOUT, 0.3, true);
                    }
                    self.fov(4.0);
                    s(c.alert_run_towards_count, 0.0);
                }
                true
            }
            (EV_LEAVE, _) => {
                s(c.skip_visibility_handicaps, false);
                if threaten {
                    self.e.h.call(
                        OP_BODY_SET_MOVE_DELAY,
                        &mut Io {
                            f: [0.6, 0.0, 0.0, 0.0],
                            ..Io::default()
                        },
                    );
                    mover(self.e.c).override_move = false;
                }
                self.fov(1.0);
                if g(c.next) != S_BREAK_DOOR {
                    self.speed(WALK);
                    self.e.h.player_remove_terror_enemy();
                }
                true
            }
            (EV_UPDATE, _) => {
                let dist = self.e.dist_to_player();
                if threaten && self.detected() {
                    let yaw = self.e.h.body_yaw();
                    let py = call_f(self.e.h, OP_PLAYER_YAW);
                    let ang = angle_dist_rad(yaw + PI, py);
                    if ang.abs() > to_rad(30.0) {
                        let pf = self.e.h.player_feet();
                        let h = &mut *self.e.h;
                        mover(self.e.c).turn_to_pos(pf, h);
                    }
                }
                let mut cnt = g(c.alert_run_towards_count);
                if dist < g(c.alert_run_towards_check_distance) {
                    cnt += self.e.player_movement_toward_enemy_amount();
                } else {
                    cnt -= 1.0;
                }
                s(c.alert_run_towards_count, cnt);
                if cnt < 0.0 {
                    s(c.alert_run_towards_count, 0.0);
                }
                if dist > g(c.increase_alert_speed_distance) {
                    self.speed(WALK);
                    mul_fwd(self.e, g(c.increased_alert_speed_mul));
                } else {
                    self.speed(WALK);
                }
                if self.e.h.player_terror() >= 1.0 && g(c.stuck_at_door) {
                    break_door_state(self.e);
                } else if !self.detected() {
                    let t = f64::from(self.e.h.player_terror());
                    if tesla {
                        if t < 0.1 {
                            self.e.change_state(S_WAIT);
                        }
                    } else if t < 0.45 {
                        self.e.change_state(S_WAIT);
                    } else if t < 1.0 {
                        self.e.change_state(S_SEARCH);
                    }
                } else if threaten {
                    let cnt = g(c.alert_run_towards_count);
                    let instant = g(c.alert_to_instant_hunt_distance);
                    if flee {
                        if cnt > g(c.alert_run_towards_to_hunt_limit) || dist < instant * 2.0 {
                            if rand_rectl(0, 3) > 0 {
                                self.e.change_state(S_FLEE);
                            } else {
                                self.e.change_state(S_HUNT);
                            }
                        }
                    } else if (self.e.h.player_terror() >= 1.0
                        && (cnt > g(c.alert_run_towards_to_hunt_limit) || dist < instant * 2.0))
                        || dist < instant
                    {
                        self.e.h.player_set_terror(1.0);
                        self.e.change_state(S_HUNT);
                    }
                } else {
                    let mut t = self.e.h.player_terror();
                    if tesla {
                        t *= 3.0;
                    }
                    let cnt = g(c.alert_run_towards_count);
                    if (t >= 1.0
                        && (dist > g(c.alert_to_hunt_distance)
                            || cnt > g(c.alert_run_towards_to_hunt_limit)))
                        || dist < g(c.alert_to_instant_hunt_distance)
                    {
                        self.e.h.player_set_terror(1.0);
                        self.e.change_state(S_HUNT);
                    }
                }
                true
            }
            (_, Some(MSG_END_OF_PATH)) => {
                if !threaten {
                    let dist = self.e.dist_to_player();
                    let flag = m.map_or(0, |m| m.custom_i);
                    if !self.detected() || (flag == 1 && dist > 5.0) {
                        self.e.change_state(if tesla { S_WAIT } else { S_SEARCH });
                    } else if flag == 1 {
                        self.detected();
                    }
                }
                true
            }
            (_, Some(MSG_TIMEOUT)) => {
                if self.detected() {
                    let lk = gv(c.last_known);
                    self.move_to(lk);
                }
                self.e.send0(MSG_TIMEOUT, 0.3, true);
                true
            }
            (_, Some(MSG_TIMEOUT_2)) => {
                if flee {
                    self.e.change_state(S_FLEE);
                } else {
                    self.e.h.player_set_terror(1.0);
                    self.e.change_state(S_HUNT);
                }
                true
            }
            (_, Some(MSG_TIMEOUT_3)) => {
                if !self.detected() {
                    self.e.change_state(S_PATROL);
                } else {
                    self.e.send0(MSG_TIMEOUT_3, 0.75, true);
                }
                true
            }
            (_, Some(MSG_TAKE_HIT)) => {
                self.e.change_state(S_HURT);
                self.e.h.music(false, MUSIC_SEARCH);
                true
            }
            (_, Some(MSG_PLAYER_DETECTED)) => true,
            _ => false,
        }
    }

    fn search_state(&mut self, ev: i32, mt: Option<i32>, m: Option<&Msg>) -> bool {
        let c = self.e.c;
        match (ev, mt) {
            (EV_ENTER, _) => {
                self.force_tesla();
                self.e.h.change_sound_state(SOUND_ALERT);
                let t = g(c.player_search_time);
                self.e.send0(MSG_TIMEOUT, t, true);
                let t2 = rand_rectf(0.0, 1.0);
                self.e.send0(MSG_TIMEOUT_2, t2, true);
                self.e.h.music(false, MUSIC_ATTACK);
                self.e.h.music(true, MUSIC_SEARCH);
                self.speed(WALK);
                mul_fwd(self.e, 1.0);
                true
            }
            (EV_LEAVE, _) => {
                self.speed(WALK);
                true
            }
            (EV_UPDATE, _) => {
                if g(c.stuck_at_door) {
                    break_door_state(self.e);
                }
                true
            }
            (_, Some(MSG_END_OF_PATH)) => {
                self.e.h.pf_stop();
                let t = rand_rectf(1.0, 3.0);
                self.e.send0(MSG_TIMEOUT_2, t, true);
                true
            }
            (_, Some(MSG_TIMEOUT_2)) => {
                let pf = self.e.h.player_feet();
                let q = NodeQuery::new(4.0, 12.0, false, false, true, 0);
                if let Some(p) = self.node(pf, &q) {
                    self.move_to(p);
                } else {
                    self.e.change_state(S_PATROL);
                }
                true
            }
            (_, Some(MSG_TIMEOUT)) => {
                self.e.change_state(S_PATROL);
                self.e.h.music(false, MUSIC_SEARCH);
                true
            }
            (_, Some(MSG_SOUND_HEARD)) => {
                if let Some(m) = m {
                    sound_heard(self.e, m);
                }
                true
            }
            _ => false,
        }
    }

    fn flee_state(&mut self, ev: i32, mt: Option<i32>, overload: bool) -> bool {
        let c = self.e.c;
        match (ev, mt) {
            (EV_ENTER, _) => {
                inc_patrol(c.cur_patrol, self.e.h.patrol_count(), true);
                if self.flee_try_find_safe_node() {
                    s(self.p.flee_invisible_count, 0.0);
                    self.fov(5.0);
                    self.run_speed();
                    self.e.h.music(false, MUSIC_SEARCH);
                    self.e.h.music(false, MUSIC_ATTACK);
                    self.e.h.player_remove_terror_enemy();
                    self.e.send0(MSG_TIMEOUT, 2.0, true);
                    self.e.send0(MSG_TIMEOUT_2, 0.2, true);
                } else {
                    self.e.change_state(S_HUNT);
                }
                true
            }
            (EV_LEAVE, _) => {
                self.fov(1.0);
                true
            }
            (_, Some(MSG_TIMEOUT)) => {
                if self.e.dist_to_player_2d() < 2.5 {
                    self.e.change_state(S_HUNT);
                }
                self.e.send0(MSG_TIMEOUT, 1.0, true);
                true
            }
            (_, Some(MSG_TIMEOUT_2)) => {
                let cnt = if !self.detected() {
                    g(self.p.flee_invisible_count) + 0.2
                } else {
                    0.0
                };
                s(self.p.flee_invisible_count, cnt);
                if f64::from(cnt) >= 1.6 {
                    if !self.auto_removal(15.0) {
                        self.e.change_state(S_WAIT);
                    }
                } else {
                    self.e.send0(MSG_TIMEOUT_2, 0.2, true);
                }
                true
            }
            (_, Some(MSG_END_OF_PATH)) => {
                if !g(self.p.auto_disable_after_flee) || !self.auto_removal(15.0) {
                    if self.detected() {
                        if !self.flee_try_find_safe_node() {
                            self.e.change_state(S_ALERT);
                        }
                    } else {
                        self.e.change_state(S_WAIT);
                    }
                }
                true
            }
            _ => overload,
        }
    }

    fn stalk_state(&mut self, ev: i32, mt: Option<i32>) -> bool {
        let c = self.e.c;
        match (ev, mt) {
            (EV_ENTER, _) => {
                self.speed(WALK);
                self.e.send0(MSG_TIMEOUT, 2.5, true);
                self.fov(0.5);
                if !self.stalk_find_node() {
                    let t = rand_rectf(2.0, 4.0);
                    self.wait_for(t);
                }
                true
            }
            (EV_LEAVE, _) => {
                self.fov(1.0);
                true
            }
            (_, Some(MSG_TIMEOUT)) => {
                let seen = call(self.e.h, OP_SEEN_BY_PLAYER, &mut Io::default()) != 0;
                let dist = self.e.dist_to_player();
                if dist < 7.5 || seen {
                    self.run_speed();
                } else {
                    self.speed(WALK);
                }
                let renew = dist < 4.5
                    || seen
                    || {
                        let next = next_goal_pos(self.e.h, gv(c.pf_goal));
                        self.visible_at(next)
                    }
                    || {
                        let fin = gv(c.pf_goal);
                        self.visible_at(fin)
                    };
                if renew && !self.stalk_find_node() {
                    let t = rand_rectf(1.0, 3.0);
                    self.wait_for(t);
                }
                if dist < 6.0 && seen {
                    self.e.change_state(S_ALERT);
                }
                self.e.send0(MSG_TIMEOUT, 2.5, true);
                true
            }
            (_, Some(MSG_END_OF_PATH)) => {
                let t = rand_rectf(2.0, 4.0);
                self.wait_for(t);
                true
            }
            (_, Some(MSG_PLAYER_DETECTED)) => {
                self.e.send0(MSG_TIMEOUT, 0.2, true);
                true
            }
            _ => false,
        }
    }

    fn track_state(&mut self, ev: i32, mt: Option<i32>, m: Option<&Msg>) -> bool {
        match (ev, mt) {
            (EV_ENTER, _) => {
                self.speed(WALK);
                self.e.send0(MSG_TIMEOUT, 2.5, true);
                if !self.track_find_node() {
                    let t = rand_rectf(1.0, 2.0);
                    self.wait_for(t);
                }
                true
            }
            (EV_LEAVE, _) => {
                self.fov(1.0);
                true
            }
            (_, Some(MSG_TIMEOUT)) => {
                if !g(self.p.is_tesla)
                    && {
                        let bf = self.e.h.body_feet();
                        !self.query_at(OP_IN_FOV_AT_FEET, bf)
                    }
                    && self.e.dist_to_player_2d() > 6.0
                    && self.track_teleport_behind_player()
                {
                    self.e.h.pf_stop();
                    if !self.track_find_node() {
                        let t = rand_rectf(1.0, 2.0);
                        self.wait_for(t);
                    }
                }
                self.e.send0(MSG_TIMEOUT, 2.5, true);
                true
            }
            (_, Some(MSG_END_OF_PATH)) => {
                if !self.track_find_node() {
                    if self.e.dist_to_player_2d() < 4.0 {
                        let t = rand_rectf(2.0, 6.0);
                        self.wait_for(t);
                    } else if self.e.dist_to_player_2d() < 6.0 {
                        let t = rand_rectf(1.0, 3.0);
                        self.wait_for(t);
                    } else if !self.track_find_node() {
                        let t = rand_rectf(1.0, 2.0);
                        self.wait_for(t);
                    }
                }
                true
            }
            (_, Some(MSG_SOUND_HEARD)) => {
                if let Some(m) = m {
                    sound_heard(self.e, m);
                }
                true
            }
            _ => false,
        }
    }

    fn hunt_state(&mut self, ev: i32, mt: Option<i32>, m: Option<&Msg>, overload: bool) -> bool {
        let c = self.e.c;
        let nad = g(c.normal_attack_distance);
        match (ev, mt) {
            (EV_ENTER, _) => {
                self.force_tesla();
                self.e.h.music(false, MUSIC_SEARCH);
                let max_hits = if g(self.p.is_tesla) {
                    1
                } else {
                    rand_rectl(1, 3)
                };
                if g(c.attack_hit_counter) >= max_hits || g(self.p.last_short_miss) {
                    s(c.attack_hit_counter, 0);
                    self.e.change_state(S_HUNT_PAUSE);
                    s(self.p.last_short_miss, false);
                } else {
                    self.e.h.change_sound_state(SOUND_HUNT);
                    self.run_speed();
                    self.e.send0(MSG_TIMEOUT, 0.1, true);
                    self.fov(4.0);
                    self.e.h.music(true, MUSIC_ATTACK);
                    self.e.h.player_add_terror_enemy();
                    let lk = gv(c.last_known);
                    self.move_to(lk);
                    self.e.send0(MSG_TIMEOUT_2, 0.5, true);
                    let t = rand_rectf(0.25, 1.0);
                    self.e.send0(MSG_TIMEOUT_3, t, true);
                }
                true
            }
            (EV_LEAVE, _) => {
                self.speed(WALK);
                self.fov(1.0);
                true
            }
            (EV_UPDATE, _) => {
                let dist = self.e.dist_to_player();
                if g(c.can_see) && dist < nad {
                    if g(self.p.force_charge) {
                        self.e.change_state(S_ATTACK_LONG);
                        s(self.p.force_charge, false);
                    } else {
                        self.e.change_state(S_ATTACK_SHORT);
                    }
                } else if g(c.stuck_at_door) {
                    break_door_state(self.e);
                }
                true
            }
            (_, Some(MSG_TIMEOUT_2)) => {
                let d = self.e.dist_to_player();
                if g(c.can_see) && d > nad && d < nad * 4.0 && mover(self.e.c).stuck_counter < 0.5 {
                    self.e.change_state(S_ATTACK_LONG);
                }
                self.e.send0(MSG_TIMEOUT_2, 1.5, true);
                true
            }
            (_, Some(MSG_TIMEOUT_3)) => {
                self.hunt_sound();
                let t = rand_rectf(0.5, 2.0);
                self.e.send0(MSG_TIMEOUT_3, t, true);
                true
            }
            (_, Some(MSG_END_OF_PATH)) => {
                let dist = self.e.dist_to_player();
                let flag = m.map_or(0, |m| m.custom_i);
                if flag == 1 && dist >= nad {
                    if !self.detected() {
                        self.e.send0(MSG_TIMEOUT_4, 1.5, true);
                    } else {
                        self.e.change_state(S_HUNT_WANDER);
                    }
                } else {
                    let lk = gv(c.last_known);
                    self.move_to(lk);
                    if !self.detected() {
                        self.e.send0(MSG_TIMEOUT_4, 1.5, true);
                    }
                }
                true
            }
            (_, Some(MSG_TIMEOUT)) => {
                let (bp, bf) = (self.e.h.body_pos(), self.e.h.body_feet());
                self.e
                    .h
                    .broadcast_help(bp, bf, g(c.activation_distance) * 0.5);
                let lk = gv(c.last_known);
                self.move_to(lk);
                self.e.send0(MSG_TIMEOUT, 0.2, true);
                true
            }
            (_, Some(MSG_TIMEOUT_4)) => {
                if !self.detected() {
                    self.e.h.player_remove_terror_enemy();
                    self.e.h.music(false, MUSIC_ATTACK);
                    if g(self.p.threaten_on_alert) || g(self.p.is_tesla) {
                        self.e.change_state(S_PATROL);
                    } else {
                        self.e.change_state(S_SEARCH);
                    }
                } else {
                    self.e.send0(MSG_TIMEOUT, 0.2, true);
                }
                true
            }
            _ => overload,
        }
    }

    fn hunt_wander_state(&mut self, ev: i32, mt: Option<i32>, overload: bool) -> bool {
        match (ev, mt) {
            (EV_ENTER, _) => {
                self.force_tesla();
                mover(self.e.c).stuck_counter = 0.0;
                self.speed(WALK);
                mul_fwd(self.e, 1.2);
                self.fov(4.0);
                let bf = self.e.h.body_feet();
                let q = NodeQuery::new(2.0, 5.0, false, true, true, 0);
                if let Some(p) = self.node(bf, &q) {
                    self.move_to(p);
                } else {
                    self.e.change_state(S_HUNT);
                }
                let t = rand_rectf(0.5, 2.0);
                self.e.send0(MSG_TIMEOUT_3, t, true);
                true
            }
            (EV_LEAVE, _) => {
                self.fov(1.0);
                true
            }
            (_, Some(MSG_TIMEOUT_3)) => {
                self.hunt_sound();
                let t = rand_rectf(0.5, 3.0);
                self.e.send0(MSG_TIMEOUT_3, t, true);
                true
            }
            (_, Some(MSG_END_OF_PATH)) => {
                self.e.change_state(S_HUNT);
                true
            }
            _ => overload,
        }
    }

    fn hunt_pause_state(&mut self, ev: i32, mt: Option<i32>, overload: bool) -> bool {
        let c = self.e.c;
        match (ev, mt) {
            (EV_ENTER, _) => {
                self.force_tesla();
                let t = rand_rectf(g(c.hunt_pause_min_time), g(c.hunt_pause_max_time))
                    * g(self.p.hunt_pause_time_mul);
                self.e.send0(MSG_TIMEOUT_2, t, true);
                self.e.h.pf_stop();
                self.speed(WALK);
                mul_fwd(self.e, 0.6);
                self.fov(4.0);
                self.e.send0(MSG_TIMEOUT, 0.2, true);
                let t = rand_rectf(0.5, 3.0);
                self.e.send0(MSG_TIMEOUT_3, t, true);
                true
            }
            (EV_LEAVE, _) => {
                self.fov(1.0);
                true
            }
            (_, Some(MSG_TIMEOUT)) => {
                let pf = self.e.h.player_feet();
                if self.e.dist_to_player_2d() > 2.0 {
                    self.move_to(pf);
                } else {
                    let h = &mut *self.e.h;
                    mover(self.e.c).turn_to_pos(pf, h);
                }
                if g(c.can_see) {
                    self.e.send0(MSG_TIMEOUT, 0.2, true);
                }
                true
            }
            (_, Some(MSG_TIMEOUT_2)) => {
                self.e.change_state(S_ALERT);
                true
            }
            (_, Some(MSG_TIMEOUT_3)) => {
                self.hunt_sound();
                let t = rand_rectf(0.5, 3.0);
                self.e.send0(MSG_TIMEOUT_3, t, true);
                true
            }
            _ => overload,
        }
    }

    fn break_door_handler(&mut self, ev: i32, mt: Option<i32>, overload: bool) -> bool {
        let c = self.e.c;
        match (ev, mt) {
            (EV_ENTER, _) => {
                self.e.h.pf_stop();
                let n = rand_rectl(1, 3);
                self.anim(ANIM_ATTACK_POSE, n, false, 0.3, true);
                self.fov(4.0);
                true
            }
            (EV_LEAVE, _) => {
                s(c.attack_hit_counter, 0);
                self.fov(1.0);
                true
            }
            (EV_UPDATE, _) => {
                let p = gv(c.temp_pos);
                let h = &mut *self.e.h;
                mover(self.e.c).turn_to_pos(p, h);
                true
            }
            (_, Some(MSG_ANIMATION_OVER)) => {
                if self.detected() {
                    self.e.change_state(S_ALERT);
                } else if self.e.h.door_broken(g(c.stuck_door_id)) {
                    let prev = self.e.st().prev();
                    self.e
                        .change_state(if prev == S_HURT { S_HUNT } else { prev });
                } else {
                    let n = rand_rectl(1, 3);
                    self.anim(ANIM_ATTACK_POSE, n, false, 0.3, true);
                }
                true
            }
            (_, Some(MSG_ANIMATION_SPECIAL_EVENT)) => {
                self.attack(2);
                true
            }
            _ => overload,
        }
    }

    fn attack_short_state(&mut self, ev: i32, mt: Option<i32>, overload: bool) -> bool {
        let c = self.e.c;
        match (ev, mt) {
            (EV_ENTER, _) => {
                self.force_tesla();
                self.e.h.pf_stop();
                if rand_rectl(0, 2) == 0 {
                    self.anim(ANIM_BITE, 0, false, 0.3, true);
                } else {
                    let n = rand_rectl(1, 3);
                    self.anim(ANIM_ATTACK_POSE, n, false, 0.3, true);
                }
                self.fov(4.0);
                s(self.p.tesla_notice, 0.35);
                true
            }
            (EV_LEAVE, _) => {
                self.fov(1.0);
                true
            }
            (_, Some(MSG_ANIMATION_OVER)) => {
                let prev = self.e.st().prev();
                self.e
                    .change_state(if prev == S_HURT { S_HUNT } else { prev });
                true
            }
            (_, Some(MSG_ANIMATION_SPECIAL_EVENT)) => {
                if !self.attack(0) {
                    s(self.p.last_short_miss, true);
                    s(self.p.force_charge, rand_rectl(0, 1) == 0);
                }
                let _ = c;
                true
            }
            _ => overload,
        }
    }

    fn attack_long_state(&mut self, ev: i32, mt: Option<i32>, overload: bool) -> bool {
        let c = self.e.c;
        match (ev, mt) {
            (EV_ENTER, _) => {
                self.force_tesla();
                self.e.h.pf_stop();
                self.speed(RUN);
                mul_fwd(self.e, 2.0);
                s(c.temp_val_l, 0.0);
                self.fov(4.0);
                self.e.send0(MSG_TIMEOUT, 1.0, true);
                s(self.p.tesla_notice, 0.35);
                true
            }
            (EV_LEAVE, _) => {
                self.fov(1.0);
                self.speed(RUN);
                true
            }
            (EV_UPDATE, _) => {
                if g(c.temp_val_l) == 0.0 && self.e.dist_to_player_2d() < 1.3 {
                    s(c.temp_val_l, 1.0);
                    self.anim(ANIM_CHARGE, 0, false, 0.3, true);
                }
                let pf = self.e.h.player_feet();
                let h = &mut *self.e.h;
                mover(self.e.c).move_to_pos(pf, h);
                true
            }
            (_, Some(MSG_TIMEOUT)) => {
                if g(c.temp_val_l) == 0.0 {
                    s(c.temp_val_l, 1.0);
                    self.anim(ANIM_CHARGE, 0, false, 0.3, true);
                }
                true
            }
            (_, Some(MSG_ANIMATION_SPECIAL_EVENT)) => {
                if g(c.temp_val_l) == 1.0 {
                    self.attack(1);
                }
                true
            }
            (_, Some(MSG_ANIMATION_OVER)) => {
                let prev = self.e.st().prev();
                self.e.change_state(prev);
                true
            }
            _ => overload,
        }
    }

    fn hurt_state(&mut self, ev: i32, mt: Option<i32>, overload: bool) -> bool {
        let c = self.e.c;
        match (ev, mt) {
            (EV_ENTER, _) => {
                self.e.h.pf_stop();
                self.anim(ANIM_FLINCH_POSE, 0, false, 0.5, true);
                self.e.send0(MSG_TIMEOUT, 0.2, true);
                self.fov(4.0);
                true
            }
            (EV_LEAVE, _) => {
                self.fov(1.0);
                true
            }
            (_, Some(MSG_ANIMATION_OVER)) => {
                if self.detected() {
                    self.e.change_state(S_HUNT);
                } else {
                    self.e.change_state(S_ALERT);
                }
                true
            }
            (_, Some(MSG_TIMEOUT)) => {
                if rand_rectl(0, 1) == 0
                    && self.e.dist_to_player() < g(c.normal_attack_distance) * 1.3
                {
                    self.e.change_state(S_ATTACK_SHORT);
                }
                true
            }
            _ => overload,
        }
    }
}

fn call_f(h: &mut dyn Host, op: u32) -> f32 {
    let mut io = Io::default();
    h.call(op, &mut io);
    io.f[0]
}

/// `UpdateCheckInLantern`.
pub fn update_check_in_lantern(e: &mut Env, p: &PigCtx, dt: f32) {
    if !e.h.player_lantern() || g(p.disable_triggers) {
        s(p.in_lantern_count, 0.0);
        return;
    }
    let t = g(p.check_flash_light) - dt;
    s(p.check_flash_light, t);
    if t > 0.0 {
        return;
    }
    s(p.check_flash_light, 0.2);
    if call(e.h, OP_INSIDE_PLAYER_VIEW, &mut Io::default()) != 0 {
        let cnt = g(p.in_lantern_count);
        if cnt < 1.0 {
            let mut n = fma(0.1, g(p.lantern_sensitivity), cnt);
            if n > 1.0 {
                n = 1.0;
            }
            s(p.in_lantern_count, n);
            let f = e.h.player_feet();
            sv(e.c.last_known, f);
        }
    } else {
        let mut n = g(p.in_lantern_count) - 0.2;
        if n < 0.0 {
            n = 0.0;
        }
        s(p.in_lantern_count, n);
    }
}

pub(super) struct PigHandler<'a, 'b, 'c> {
    pub e: &'a mut Env<'b>,
    pub p: &'c PigCtx,
}

impl Handler for PigHandler<'_, '_, '_> {
    fn event(&mut self, state: i32, ev: i32, msg: Option<&Msg>) -> bool {
        let r = Pig {
            e: &mut *self.e,
            p: self.p,
        }
        .state_event(state, ev, msg);
        self.e.flush();
        r
    }
}

/// # Safety
/// `c`, `p`, `h` are live for the call; `m` is null or readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga3b_pig_event(
    c: *const Ctx,
    p: *const PigCtx,
    h: *const OhplGa3Host,
    state: i32,
    ev: i32,
    m: *const Msg,
) -> bool {
    // SAFETY: caller contract.
    let (mut host, p, m) = unsafe { (PHost(&*h), &*p, m.as_ref()) };
    // SAFETY: caller contract.
    let e = &mut Env::new(unsafe { &*c }, &mut host);
    PigHandler { e, p }.event(state, ev, m)
}

/// # Safety
/// `c`, `p`, `h` are live for the call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga3b_pig_update_lantern(
    c: *const Ctx,
    p: *const PigCtx,
    h: *const OhplGa3Host,
    dt: f32,
) {
    // SAFETY: caller contract.
    let (mut host, p) = unsafe { (PHost(&*h), &*p) };
    // SAFETY: caller contract.
    let e = &mut Env::new(unsafe { &*c }, &mut host);
    update_check_in_lantern(e, p, dt);
}
