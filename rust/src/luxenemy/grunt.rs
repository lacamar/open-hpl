//! `cLuxEnemy_Grunt`: the state machine, `PlayerIsDetected`, damage multiplier and patrol steps.

use super::ctx::{Env, g, gv, s, sv};
use super::host::AnimReq;
use super::host::{CMD_RUN_CALLBACK, Io};
use super::machine::*;
use super::mover::MoverState;
use super::pathfinder::{NodeQuery, node_at_pos};
use crate::math::{Vector3f, rand_rectf, rand_rectl};

pub const S_IDLE: i32 = 0;
pub const S_GO_HOME: i32 = 1;
pub const S_WAIT: i32 = 2;
pub const S_PATROL: i32 = 3;
pub const S_INVESTIGATE: i32 = 4;
pub const S_ALERT: i32 = 5;
pub const S_SEARCH: i32 = 6;
pub const S_HURT: i32 = 8;
pub const S_HUNT: i32 = 9;
pub const S_HUNT_PAUSE: i32 = 10;
pub const S_HUNT_WANDER: i32 = 11;
pub const S_ATTACK_SHORT: i32 = 12;
pub const S_ATTACK_LONG: i32 = 13;
pub const S_BREAK_DOOR: i32 = 15;
pub const S_DEAD: i32 = 16;

pub const ANIM_IDLE_EXTRA: i32 = 10;
pub const ANIM_NOTICE: i32 = 11;
pub const ANIM_BREAK_DOOR: i32 = 12;
pub const ANIM_SWING_CLAWS: i32 = 13;
pub const ANIM_SWING_LAUNCH: i32 = 14;
pub const ANIM_FLINCH: i32 = 15;
pub const ANIM_DEAD: i32 = 16;

pub const SND_NOTICE: i32 = 0;
pub const SND_GIVE_UP: i32 = 1;
pub const SND_ENABLED: i32 = 2;

const SOUND_IDLE: i32 = 1;
const SOUND_ALERT: i32 = 2;
const SOUND_HUNT: i32 = 3;
const MUSIC_SEARCH: i32 = 0;
const MUSIC_ATTACK: i32 = 1;
const WALK: usize = 0;
const RUN: usize = 1;

pub struct Gr<'a, 'b> {
    pub e: &'a mut Env<'b>,
}

impl Gr<'_, '_> {
    fn set_move_speed(&mut self, t: usize) {
        let c = self.e.c;
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

    fn mul_fwd(&mut self, m: f32) {
        s(self.e.c.forward_speed, g(self.e.c.forward_speed) * m);
    }

    fn anim(&mut self, kind: i32, n: i32, looped: bool, fade: f32, ovr: bool, use_move: bool) {
        let a = AnimReq {
            kind,
            n,
            looped,
            fade,
            depends_on_speed: false,
            speed_mul: 1.0,
            sync: false,
            override_move: ovr,
            use_move_when_over: use_move,
        };
        if self.e.h.play_anim(&a) {
            // SAFETY: C++ aliases the mover only inside `PlayAnim`, which has returned.
            unsafe { (*self.e.c.mover).override_move = ovr };
        }
    }

    fn mover(&mut self) -> &mut MoverState {
        // SAFETY: as in `anim`.
        unsafe { &mut *self.e.c.mover }
    }

    fn move_to(&mut self, p: Vector3f) -> bool {
        self.e.h.pf_move_to(p)
    }

    fn fov(&mut self, m: f32) {
        s(self.e.c.fov_mul, m);
    }

    fn temp_from_msg(&mut self, m: &Msg) {
        sv(self.e.c.temp_pos, super::host::v_of(m.custom_v));
        s(self.e.c.temp_val, m.custom_f);
    }

    fn sound_heard(&mut self, m: &Msg) {
        if m.custom_f > g(self.e.c.hear_volume) {
            self.e.change_state(S_INVESTIGATE);
            self.temp_from_msg(m);
        }
    }

    fn break_door_state(&mut self) {
        let id = g(self.e.c.stuck_door_id);
        let p = self.e.h.entity_attach_pos(id);
        sv(self.e.c.temp_pos, p);
        self.e.change_state(S_BREAK_DOOR);
    }

    fn player_is_detected(&mut self) -> bool {
        player_is_detected(self.e)
    }

    fn attack(&mut self, which: i32) {
        self.e.h.attack(which);
    }

    fn state_event(&mut self, state: i32, ev: i32, m: Option<&Msg>) -> bool {
        let mt = match (ev, m) {
            (EV_MESSAGE, Some(m)) => Some(m.ty),
            _ => None,
        };
        let c = self.e.c;
        if state < 0 {
            return self.default_state(mt, m);
        }
        let g_overload = matches!(mt, Some(MSG_PLAYER_DETECTED | MSG_HELP_ME));
        match state {
            S_IDLE => match (ev, mt) {
                (EV_ENTER, _) => {
                    s(c.in_range, false);
                    self.set_move_speed(WALK);
                    true
                }
                (_, Some(MSG_PLAYER_IN_RANGE)) => {
                    self.e.h.debug_msg(1, 0.0);
                    if self.e.h.activate_sound_allowed() {
                        self.e.h.set_activate_sound_made();
                        self.e.h.play_sound(SND_ENABLED);
                    }
                    if self.e.h.patrol_count() == 0 {
                        self.e.change_state(S_WAIT);
                    } else {
                        self.e.change_state(S_PATROL);
                    }
                    true
                }
                (_, Some(MSG_PLAYER_DETECTED)) => true,
                _ => false,
            },
            S_GO_HOME => match (ev, mt) {
                (EV_ENTER, _) => {
                    let p = gv(c.start_position);
                    self.move_to(p);
                    self.set_move_speed(WALK);
                    true
                }
                (_, Some(MSG_END_OF_PATH)) => {
                    self.e.change_state(S_IDLE);
                    true
                }
                (_, Some(MSG_SOUND_HEARD)) => {
                    if let Some(m) = m {
                        self.sound_heard(m);
                    }
                    true
                }
                _ => false,
            },
            S_WAIT => self.wait_state(ev, mt, m),
            S_PATROL => match (ev, mt) {
                (EV_ENTER, _) => {
                    self.e.h.change_sound_state(SOUND_IDLE);
                    self.set_move_speed(WALK);
                    self.patrol_update_goal();
                    true
                }
                (EV_UPDATE, _) => {
                    if g(c.stuck_at_door) {
                        self.break_door_state();
                    }
                    true
                }
                (_, Some(MSG_END_OF_PATH)) => {
                    self.patrol_end_of_path();
                    true
                }
                (_, Some(MSG_SOUND_HEARD)) => {
                    if let Some(m) = m {
                        self.sound_heard(m);
                    }
                    true
                }
                _ => false,
            },
            S_INVESTIGATE => self.investigate_state(ev, mt, m),
            S_ALERT => self.alert_state(ev, mt, m),
            S_SEARCH => self.search_state(ev, mt, m),
            S_HUNT => self.hunt_state(ev, mt, m, g_overload),
            S_HUNT_WANDER => match (ev, mt) {
                (EV_ENTER, _) => {
                    self.mover().stuck_counter = 0.0;
                    self.set_move_speed(WALK);
                    self.mul_fwd(1.2);
                    self.fov(4.0);
                    let bf = self.e.h.body_feet();
                    let q = NodeQuery::new(2.0, 5.0, false, true, true, 0);
                    if let Some((_, p)) = node_at_pos(self.e.h, bf, &q) {
                        self.move_to(p);
                    } else {
                        self.e.change_state(S_HUNT);
                    }
                    true
                }
                (EV_LEAVE, _) => {
                    self.fov(1.0);
                    true
                }
                (_, Some(MSG_END_OF_PATH)) => {
                    self.e.change_state(S_HUNT);
                    true
                }
                _ => g_overload,
            },
            S_HUNT_PAUSE => match (ev, mt) {
                (EV_ENTER, _) => {
                    let t = rand_rectf(g(c.hunt_pause_min_time), g(c.hunt_pause_max_time));
                    self.e.send0(MSG_TIMEOUT_2, t, true);
                    self.e.h.pf_stop();
                    self.set_move_speed(WALK);
                    self.mul_fwd(0.6);
                    self.fov(4.0);
                    self.e.send0(MSG_TIMEOUT, 0.2, true);
                    true
                }
                (EV_LEAVE, _) => {
                    self.fov(1.0);
                    true
                }
                (_, Some(MSG_TIMEOUT)) => {
                    if self.e.dist_to_player_2d() > 2.0 {
                        let pf = self.e.h.player_feet();
                        self.move_to(pf);
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
                _ => g_overload,
            },
            S_BREAK_DOOR => match (ev, mt) {
                (EV_ENTER, _) => {
                    self.e.h.pf_stop();
                    self.anim(ANIM_BREAK_DOOR, 0, false, 0.3, true, true);
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
                    // SAFETY: as in `mover`.
                    let h = &mut *self.e.h;
                    unsafe { (*c.mover).turn_to_pos(p, h) };
                    true
                }
                (_, Some(MSG_ANIMATION_OVER)) => {
                    if self.player_is_detected() {
                        self.e.change_state(S_ALERT);
                    } else if self.e.h.door_broken(g(c.stuck_door_id)) {
                        if self.e.st().prev() == S_HURT {
                            self.e.change_state(S_HUNT);
                        } else {
                            let p = self.e.st().prev();
                            self.e.change_state(p);
                        }
                    } else {
                        self.anim(ANIM_BREAK_DOOR, 0, false, 0.3, true, true);
                    }
                    true
                }
                (_, Some(MSG_ANIMATION_SPECIAL_EVENT)) => {
                    self.attack(1);
                    true
                }
                _ => g_overload,
            },
            S_ATTACK_SHORT => match (ev, mt) {
                (EV_ENTER, _) => {
                    self.e.h.pf_stop();
                    let n = rand_rectl(1, 2);
                    self.anim(ANIM_SWING_CLAWS, n, false, 0.3, true, true);
                    self.fov(4.0);
                    true
                }
                (EV_LEAVE, _) => {
                    self.fov(1.0);
                    true
                }
                (_, Some(MSG_ANIMATION_OVER)) => {
                    if self.e.st().prev() == S_HURT {
                        self.e.change_state(S_HUNT);
                    } else {
                        let p = self.e.st().prev();
                        self.e.change_state(p);
                    }
                    true
                }
                (_, Some(MSG_ANIMATION_SPECIAL_EVENT)) => {
                    self.attack(0);
                    true
                }
                _ => g_overload,
            },
            S_ATTACK_LONG => match (ev, mt) {
                (EV_ENTER, _) => {
                    self.e.h.pf_stop();
                    self.set_move_speed(RUN);
                    self.mul_fwd(1.5);
                    self.anim(ANIM_SWING_LAUNCH, 0, false, 0.3, true, true);
                    s(c.temp_val_l, 0.0);
                    self.fov(4.0);
                    true
                }
                (EV_LEAVE, _) => {
                    self.fov(1.0);
                    self.set_move_speed(RUN);
                    true
                }
                (EV_UPDATE, _) => {
                    if g(c.temp_val_l) == 0.0 {
                        let pf = self.e.h.player_feet();
                        let h = &mut *self.e.h;
                        // SAFETY: as in `mover`.
                        unsafe { (*c.mover).move_to_pos(pf, h) };
                    }
                    true
                }
                (_, Some(MSG_ANIMATION_SPECIAL_EVENT)) => {
                    s(c.temp_val_l, 1.0);
                    self.attack(0);
                    true
                }
                (_, Some(MSG_ANIMATION_OVER)) => {
                    let p = self.e.st().prev();
                    self.e.change_state(p);
                    true
                }
                _ => g_overload,
            },
            S_HURT => match (ev, mt) {
                (EV_ENTER, _) => {
                    self.e.h.pf_stop();
                    self.anim(ANIM_FLINCH, 0, false, 0.5, true, true);
                    self.e.send0(MSG_TIMEOUT, 0.2, true);
                    self.fov(4.0);
                    true
                }
                (EV_LEAVE, _) => {
                    self.fov(1.0);
                    true
                }
                (_, Some(MSG_ANIMATION_OVER)) => {
                    if self.player_is_detected() {
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
                _ => g_overload,
            },
            S_DEAD => match (ev, mt) {
                (EV_ENTER, _) => {
                    self.e.h.pf_stop();
                    self.anim(ANIM_DEAD, 0, false, 0.3, true, false);
                    self.e.h.player_remove_terror_enemy();
                    self.e.h.music(false, MUSIC_ATTACK);
                    self.e.h.body_set_active(false);
                    true
                }
                (_, Some(MSG_ANIMATION_OVER)) => true,
                _ => g_overload,
            },
            _ => {
                self.e.h.fatal_state(state);
                false
            }
        }
    }

    fn default_state(&mut self, mt: Option<i32>, m: Option<&Msg>) -> bool {
        let c = self.e.c;
        match mt {
            Some(MSG_RESET) => {
                self.e.h.music(false, MUSIC_ATTACK);
                self.e.h.player_remove_terror_enemy();
                self.e.change_state(S_WAIT);
                true
            }
            Some(MSG_PLAYER_DETECTED) => {
                self.e.change_state(S_ALERT);
                true
            }
            Some(MSG_TAKE_HIT) => {
                let add = m.map_or(0.0, |m| m.custom_f);
                s(c.damage_count, g(c.damage_count) + add);
                if g(c.damage_count) > g(c.groggy_damage_count) {
                    self.e.change_state(S_HURT);
                    s(c.damage_count, 0.0);
                }
                true
            }
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

    fn wait_state(&mut self, ev: i32, mt: Option<i32>, m: Option<&Msg>) -> bool {
        let c = self.e.c;
        match (ev, mt) {
            (EV_ENTER, _) => {
                if g(c.wait_time) <= 0.0 {
                    let t = rand_rectf(1.0, 3.0);
                    self.e.send0(MSG_TIMEOUT, t, true);
                } else {
                    let t = g(c.wait_time);
                    self.e.send0(MSG_TIMEOUT, t, true);
                }
                s(c.wait_time, 0.0);
                let t = rand_rectf(g(c.idle_extra_time_min), g(c.idle_extra_time_max));
                self.e.send0(MSG_TIMEOUT_2, t, true);
                self.set_move_speed(WALK);
                true
            }
            (_, Some(MSG_TIMEOUT)) => {
                if self.e.st().prev() == S_SEARCH {
                    self.fade_out();
                    self.e.change_state(S_SEARCH);
                } else if self.e.h.patrol_count() > 0 {
                    self.fade_out();
                    self.e.change_state(S_PATROL);
                } else {
                    let t = rand_rectf(3.0, 5.0);
                    self.e.send0(MSG_TIMEOUT, t, true);
                }
                true
            }
            (_, Some(MSG_TIMEOUT_2)) => {
                let n = rand_rectl(1, g(c.idle_extra_num));
                self.anim(ANIM_IDLE_EXTRA, n, false, 0.3, true, true);
                true
            }
            (_, Some(MSG_ANIMATION_OVER)) => {
                let t = rand_rectf(2.0, 6.0);
                self.e.send0(MSG_TIMEOUT_2, t, true);
                true
            }
            (_, Some(MSG_SOUND_HEARD)) => {
                if let Some(m) = m {
                    self.sound_heard(m);
                }
                true
            }
            _ => false,
        }
    }

    /// `FadeOutCurrentAnim(0.2)`, which ends in `UseMoveStateAnimations` when it faded.
    fn fade_out(&mut self) {
        if self.e.h.fade_out_anim(0.2) {
            self.mover().use_move_state_animations();
        }
    }

    fn investigate_state(&mut self, ev: i32, mt: Option<i32>, m: Option<&Msg>) -> bool {
        let c = self.e.c;
        match (ev, mt) {
            (EV_ENTER, _) => {
                self.e.h.change_sound_state(SOUND_ALERT);
                self.set_move_speed(WALK);
                self.e.h.pf_stop();
                let n = rand_rectl(1, 2);
                self.anim(ANIM_NOTICE, n, false, 0.3, true, true);
                self.e.h.play_sound(SND_NOTICE);
                self.fov(4.0);
                let tv = g(c.temp_val);
                self.e.h.debug_msg(2, tv);
                true
            }
            (_, Some(MSG_ANIMATION_OVER)) => {
                self.set_move_speed(WALK);
                let tp = gv(c.temp_pos);
                let mut q = NodeQuery::new(0.0, 10.0, true, true, true, 0);
                q.ray_num = 1;
                if let Some((_, p)) = node_at_pos(self.e.h, tp, &q) {
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
                    self.break_door_state();
                }
                true
            }
            (EV_LEAVE, _) => {
                self.fov(1.0);
                self.fade_out();
                self.set_move_speed(WALK);
                true
            }
            (_, Some(MSG_SOUND_HEARD)) => {
                if let Some(m) = m
                    && g(c.pf_moving)
                    && m.custom_f > g(c.temp_val)
                {
                    let tp = gv(c.temp_pos);
                    let mut q = NodeQuery::new(0.0, 10.0, true, true, true, 0);
                    q.ray_num = 1;
                    if let Some((_, p)) = node_at_pos(self.e.h, tp, &q) {
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
        match (ev, mt) {
            (EV_ENTER, _) => {
                self.e.h.change_sound_state(SOUND_ALERT);
                self.e.send0(MSG_TIMEOUT, 0.3, true);
                let lk = gv(c.last_known);
                self.move_to(lk);
                self.e.h.player_add_terror_enemy();
                self.set_move_speed(WALK);
                self.mul_fwd(1.2);
                self.fov(4.0);
                s(c.alert_run_towards_count, 0.0);
                true
            }
            (EV_LEAVE, _) => {
                self.fov(1.0);
                if g(c.next) != S_BREAK_DOOR {
                    self.set_move_speed(WALK);
                    self.e.h.player_remove_terror_enemy();
                }
                true
            }
            (EV_UPDATE, _) => {
                let dist = self.e.dist_to_player();
                let mut cnt = g(c.alert_run_towards_count);
                if dist < g(c.alert_run_towards_check_distance) {
                    cnt += self.e.player_movement_toward_enemy_amount();
                } else {
                    cnt -= 1.0;
                }
                if cnt < 0.0 {
                    cnt = 0.0;
                }
                s(c.alert_run_towards_count, cnt);
                if dist > g(c.increase_alert_speed_distance) {
                    self.set_move_speed(WALK);
                    self.mul_fwd(1.2 * g(c.increased_alert_speed_mul));
                } else {
                    self.set_move_speed(WALK);
                    self.mul_fwd(1.2);
                }
                if self.e.h.player_terror() >= 1.0 && g(c.stuck_at_door) {
                    self.break_door_state();
                } else if !self.player_is_detected() {
                    if self.e.h.player_terror() < 1.0 {
                        self.e.change_state(S_SEARCH);
                    }
                } else if (self.e.h.player_terror() >= 1.0
                    && (dist > g(c.alert_to_hunt_distance)
                        || g(c.alert_run_towards_count) > g(c.alert_run_towards_to_hunt_limit)))
                    || dist < g(c.alert_to_instant_hunt_distance)
                {
                    self.e.h.player_set_terror(1.0);
                    self.e.change_state(S_HUNT);
                }
                true
            }
            (_, Some(MSG_END_OF_PATH)) => {
                let dist = self.e.dist_to_player();
                let flag = m.map_or(0, |m| m.custom_i);
                if !self.player_is_detected() || (flag == 1 && dist > 5.0) {
                    self.e.change_state(S_SEARCH);
                } else if flag == 1 {
                    self.player_is_detected();
                }
                true
            }
            (_, Some(MSG_TIMEOUT)) => {
                if self.player_is_detected() {
                    let lk = gv(c.last_known);
                    self.move_to(lk);
                }
                self.e.send0(MSG_TIMEOUT, 0.3, true);
                true
            }
            (_, Some(MSG_TAKE_HIT)) => {
                self.e.change_state(S_HURT);
                self.e.h.music(false, MUSIC_SEARCH);
                true
            }
            _ => false,
        }
    }

    fn search_state(&mut self, ev: i32, mt: Option<i32>, m: Option<&Msg>) -> bool {
        let c = self.e.c;
        match (ev, mt) {
            (EV_ENTER, _) => {
                self.e.h.change_sound_state(SOUND_ALERT);
                let t = g(c.player_search_time);
                self.e.send0(MSG_TIMEOUT, t, true);
                let t2 = rand_rectf(0.0, 1.0);
                self.e.send0(MSG_TIMEOUT_2, t2, true);
                self.e.h.music(false, MUSIC_ATTACK);
                self.e.h.music(true, MUSIC_SEARCH);
                self.set_move_speed(WALK);
                self.mul_fwd(1.0);
                true
            }
            (EV_LEAVE, _) => {
                self.set_move_speed(WALK);
                true
            }
            (EV_UPDATE, _) => {
                if g(c.stuck_at_door) {
                    self.break_door_state();
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
                if let Some((_, p)) = node_at_pos(self.e.h, pf, &q) {
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
                    self.sound_heard(m);
                }
                true
            }
            _ => false,
        }
    }

    fn hunt_state(&mut self, ev: i32, mt: Option<i32>, m: Option<&Msg>, g_overload: bool) -> bool {
        let c = self.e.c;
        match (ev, mt) {
            (EV_ENTER, _) => {
                if g(c.attack_hit_counter) >= 1 {
                    s(c.attack_hit_counter, 0);
                    self.e.h.music(false, MUSIC_SEARCH);
                    self.e.change_state(S_HUNT_PAUSE);
                } else {
                    self.e.h.change_sound_state(SOUND_HUNT);
                    self.set_move_speed(RUN);
                    self.e.send0(MSG_TIMEOUT, 0.1, true);
                    self.fov(4.0);
                    self.e.h.music(false, MUSIC_SEARCH);
                    self.e.h.music(true, MUSIC_ATTACK);
                    self.e.h.player_add_terror_enemy();
                    let lk = gv(c.last_known);
                    self.move_to(lk);
                    self.e.send0(MSG_TIMEOUT_2, 0.6, true);
                }
                true
            }
            (EV_LEAVE, _) => {
                self.set_move_speed(WALK);
                self.fov(1.0);
                true
            }
            (EV_UPDATE, _) => {
                let dist = self.e.dist_to_player();
                if g(c.can_see) && dist < g(c.normal_attack_distance) {
                    self.e.change_state(S_ATTACK_SHORT);
                } else if g(c.stuck_at_door) {
                    self.break_door_state();
                }
                true
            }
            (_, Some(MSG_TIMEOUT_2)) => {
                let d = self.e.dist_to_player();
                let nad = g(c.normal_attack_distance);
                if g(c.can_see) && d > nad && d < nad * 2.0 && self.mover().stuck_counter < 0.5 {
                    self.e.change_state(S_ATTACK_LONG);
                }
                self.e.send0(MSG_TIMEOUT_2, 0.6, true);
                true
            }
            (_, Some(MSG_END_OF_PATH)) => {
                let dist = self.e.dist_to_player();
                let stuck_flag = m.map_or(0, |m| m.custom_i);
                if stuck_flag == 1 && dist >= g(c.normal_attack_distance) {
                    if !self.player_is_detected() {
                        self.e.send0(MSG_TIMEOUT_3, 1.5, true);
                    } else {
                        self.e.change_state(S_HUNT_WANDER);
                    }
                } else {
                    let lk = gv(c.last_known);
                    self.move_to(lk);
                    if !self.player_is_detected() {
                        self.e.send0(MSG_TIMEOUT_3, 1.5, true);
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
            (_, Some(MSG_TIMEOUT_3)) => {
                if !self.player_is_detected() {
                    self.e.h.player_remove_terror_enemy();
                    self.e.h.music(false, MUSIC_ATTACK);
                    self.e.change_state(S_SEARCH);
                } else {
                    self.e.send0(MSG_TIMEOUT, 0.2, true);
                }
                true
            }
            _ => g_overload,
        }
    }

    fn patrol_update_goal(&mut self) {
        let c = self.e.c;
        if self.e.h.patrol_count() == 0 {
            self.e.change_state(S_WAIT);
            return;
        }
        // A cursor past the end made the C++ dereference null; nothing is moved here.
        if let Some(p) = self.e.h.patrol_node_pos(g(c.cur_patrol)) {
            self.move_to(p);
        }
    }

    fn patrol_end_of_path(&mut self) {
        let c = self.e.c;
        if g(c.cur_patrol) >= self.e.h.patrol_count() - 1
            && !g(c.is_seen_by_player)
            && self.e.dist_to_player() > 10.0
            && g(c.auto_remove_at_path_end)
        {
            self.e.h.set_active_false();
            self.e.cmd(CMD_RUN_CALLBACK, Io::default());
            return;
        }
        let w = self.e.h.patrol_wait_time(g(c.cur_patrol)).unwrap_or(0.0);
        s(c.wait_time, w);
        self.e.change_state(S_WAIT);
        inc_patrol(c.cur_patrol, self.e.h.patrol_count(), true);
    }
}

pub fn inc_patrol(cur: *mut i32, size: i32, loop_at_end: bool) {
    let mut v = g(cur) + 1;
    if v >= size {
        v = if loop_at_end { 0 } else { size - 1 };
    }
    s(cur, v);
}

/// `cLuxEnemy_Grunt::PlayerIsDetected`.
pub fn player_is_detected(e: &mut Env) -> bool {
    if g(e.c.can_see) {
        true
    } else {
        let d = e.dist_to_player();
        d < e.h.body_size().x && e.player_in_fov()
    }
}

pub fn damage_mul(cur: i32) -> f32 {
    match cur {
        S_IDLE | S_WAIT | S_PATROL | S_INVESTIGATE => 1.0,
        S_HURT => 0.5,
        _ => 0.2,
    }
}

pub struct GruntHandler<'a, 'b> {
    pub e: &'a mut Env<'b>,
}

impl Handler for GruntHandler<'_, '_> {
    fn event(&mut self, state: i32, ev: i32, msg: Option<&Msg>) -> bool {
        let r = Gr { e: self.e }.state_event(state, ev, msg);
        self.e.flush();
        r
    }
}
