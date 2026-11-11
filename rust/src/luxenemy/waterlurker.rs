//! `cLuxEnemy_WaterLurker`: the state machine and `PlayerIsDetected`. Splash effects and the
//! food bite stay C++ behind the host table (ops from `OP_WL_SPLASH`).

use super::ctx::{Ctx, Env, P, g, gv, s, sv};
use super::ffi::OhplGa3Host;
use super::host::{Io, v_of};
use super::machine::*;
use super::manpig::{
    MSG_FOOD_IN_RANGE, MUSIC_ATTACK, PHost, RUN, S_ATTACK_SHORT, S_BREAK_DOOR, S_DEAD, S_EAT,
    S_GO_HOME, S_HUNT, S_HUNT_PAUSE, S_IDLE, S_INVESTIGATE, S_SEARCH, SOUND_HUNT, SOUND_IDLE,
    SOUND_SILENT, WALK, break_door_state, call, mover, set_move_speed, sound_heard,
};
use super::mover::MoverState;
use super::pathfinder::{NodeQuery, node_at_pos};
use super::{dist3, z3};
use crate::math::{Vector3f, rand_rectf};

pub const OP_WL_SPLASH: u32 = 220;
pub const OP_WL_FOOD_BITE: u32 = 221;
pub const OP_WL_DEBUG: u32 = 222;

const SPLASH_WALK: i32 = 0;
const SPLASH_RUN: i32 = 1;
const SPLASH_ATTACK: i32 = 3;

const DBG_FOUND_FOOD: i32 = 0;
const DBG_FOOD_NO_NODE: i32 = 1;
const DBG_TO_EAT: i32 = 2;

/// Pointers into the WaterLurker members that the handlers use beyond the shared `Ctx`.
#[repr(C)]
pub struct WlCtx {
    pub player_detection_height: P<f32>,
    pub after_attack_pause: P<f32>,
    pub causes_sanity_decrease: P<bool>,
}

pub struct Wl<'a, 'b, 'c> {
    pub e: &'a mut Env<'b>,
    pub w: &'c WlCtx,
}

impl Wl<'_, '_, '_> {
    fn speed(&mut self, t: usize) {
        set_move_speed(self.e, t);
    }

    fn move_to(&mut self, p: Vector3f) -> bool {
        self.e.h.pf_move_to(p)
    }

    fn splash(&mut self, kind: i32) {
        let mut io = Io::default();
        io.i[0] = kind;
        call(self.e.h, OP_WL_SPLASH, &mut io);
    }

    fn debug(&mut self, kind: i32) {
        let mut io = Io::default();
        io.i[0] = kind;
        call(self.e.h, OP_WL_DEBUG, &mut io);
    }

    fn detected(&mut self) -> bool {
        player_is_detected(self.e, self.w)
    }

    fn sanity(&mut self, on: bool) {
        s(self.w.causes_sanity_decrease, on);
    }

    fn node_ray1(&mut self, pos: Vector3f) -> Option<Vector3f> {
        let mut q = NodeQuery::new(0.0, 10.0, true, true, true, 0);
        q.ray_num = 1;
        node_at_pos(self.e.h, pos, &q).map(|(_, p)| p)
    }

    fn splash_repeat(&mut self, kind: i32, t: f32) {
        self.splash(kind);
        self.e.send0(MSG_TIMEOUT_2, t, true);
    }

    fn default_state(&mut self, mt: Option<i32>, m: Option<&Msg>) -> bool {
        let c = self.e.c;
        match mt {
            Some(MSG_PLAYER_DETECTED) => {
                self.e.change_state(S_HUNT);
                true
            }
            Some(MSG_TAKE_HIT) => true,
            Some(MSG_PLAYER_DEAD) => {
                self.e.change_state(S_IDLE);
                true
            }
            Some(MSG_FOOD_IN_RANGE) => {
                let at = m.map_or(z3(), |m| v_of(m.custom_v));
                let q = NodeQuery::new(0.0, 1.5, true, true, true, 0);
                if let Some((_, p)) = node_at_pos(self.e.h, at, &q) {
                    self.debug(DBG_FOUND_FOOD);
                    sv(c.temp_pos, p);
                    self.e.change_state(S_SEARCH);
                    self.e.h.music(false, MUSIC_ATTACK);
                } else {
                    self.debug(DBG_FOOD_NO_NODE);
                }
                true
            }
            _ => false,
        }
    }

    pub fn state_event(&mut self, state: i32, ev: i32, m: Option<&Msg>) -> bool {
        let c = self.e.c;
        let mt = match (ev, m) {
            (EV_MESSAGE, Some(m)) => Some(m.ty),
            _ => None,
        };
        if state < 0 {
            return self.default_state(mt, m);
        }
        let food = mt == Some(MSG_FOOD_IN_RANGE);
        let overload = food || mt == Some(MSG_PLAYER_DETECTED);
        let nad = g(c.normal_attack_distance);
        match state {
            S_IDLE => match (ev, mt) {
                (EV_ENTER, _) => {
                    self.e.h.change_sound_state(SOUND_IDLE);
                    true
                }
                (_, Some(MSG_SOUND_HEARD)) => {
                    if let Some(m) = m {
                        sound_heard(self.e, m);
                    }
                    true
                }
                _ => false,
            },
            S_GO_HOME => match (ev, mt) {
                (EV_ENTER, _) => {
                    self.e.h.change_sound_state(SOUND_IDLE);
                    let pf = self.e.h.player_feet();
                    let q = NodeQuery::new(0.0, 6.0, false, false, true, 0);
                    if let Some((_, p)) = node_at_pos(self.e.h, pf, &q) {
                        self.move_to(p);
                    } else {
                        let sp = gv(c.start_position);
                        self.move_to(sp);
                    }
                    self.e.send0(MSG_TIMEOUT_2, 0.1, true);
                    true
                }
                (EV_UPDATE, _) => {
                    if mover(self.e.c).stuck_counter > 0.9 {
                        self.e.change_state(S_IDLE);
                        self.e.h.music(false, MUSIC_ATTACK);
                    }
                    true
                }
                (_, Some(MSG_SOUND_HEARD)) => {
                    if let Some(m) = m {
                        sound_heard(self.e, m);
                    }
                    true
                }
                (_, Some(MSG_TIMEOUT_2)) => {
                    self.splash_repeat(SPLASH_WALK, 1.0);
                    true
                }
                (_, Some(MSG_END_OF_PATH)) => {
                    self.e.change_state(S_IDLE);
                    self.e.h.music(false, MUSIC_ATTACK);
                    true
                }
                _ => false,
            },
            S_INVESTIGATE => match (ev, mt) {
                (EV_ENTER, _) => {
                    let tv = g(c.temp_val);
                    self.e.h.debug_msg(2, tv);
                    let tp = gv(c.temp_pos);
                    if let Some(p) = self.node_ray1(tp) {
                        self.move_to(p);
                    } else {
                        self.e.h.debug_msg(3, 0.0);
                        self.move_to(tp);
                    }
                    self.e.send0(MSG_TIMEOUT_2, 0.1, true);
                    true
                }
                (_, Some(MSG_END_OF_PATH)) => {
                    self.e.change_state(S_IDLE);
                    true
                }
                (EV_UPDATE, _) => {
                    if mover(self.e.c).stuck_counter > 0.9 {
                        self.e.change_state(S_IDLE);
                    }
                    true
                }
                (_, Some(MSG_TIMEOUT_2)) => {
                    self.splash_repeat(SPLASH_WALK, 1.0);
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
            },
            S_SEARCH => match (ev, mt) {
                (EV_ENTER, _) => {
                    self.e.h.change_sound_state(SOUND_IDLE);
                    let tp = gv(c.temp_pos);
                    self.move_to(tp);
                    self.e.send0(MSG_TIMEOUT_2, 0.1, true);
                    true
                }
                (EV_UPDATE, _) => true,
                (_, Some(MSG_TIMEOUT_2)) => {
                    self.splash_repeat(SPLASH_WALK, 1.0);
                    true
                }
                (_, Some(MSG_END_OF_PATH)) => {
                    self.debug(DBG_TO_EAT);
                    self.e.change_state(S_EAT);
                    true
                }
                _ => food,
            },
            S_EAT => match (ev, mt) {
                (EV_ENTER, _) => {
                    self.e.h.change_sound_state(SOUND_IDLE);
                    self.e.send0(MSG_TIMEOUT, 0.1, true);
                    let bf = self.e.h.body_feet();
                    sv(c.temp_pos, bf);
                    self.e.h.pf_stop();
                    true
                }
                (_, Some(MSG_TIMEOUT)) => {
                    let mut io = Io::default();
                    io.f[0] = g(self.w.player_detection_height);
                    if call(self.e.h, OP_WL_FOOD_BITE, &mut io) != 0 {
                        self.e.send0(MSG_TIMEOUT, 1.2, true);
                        sv(c.temp_pos, v_of(io.v[0]));
                    } else {
                        self.e.change_state(S_GO_HOME);
                    }
                    true
                }
                (EV_UPDATE, _) => {
                    let (mut a, mut b) = (self.e.h.body_feet(), gv(c.temp_pos));
                    a.y = 0.0;
                    b.y = 0.0;
                    if dist3(a, b) > self.e.h.body_size().x * 0.75 {
                        let tp = gv(c.temp_pos);
                        let h = &mut *self.e.h;
                        mover(self.e.c).move_to_pos(tp, h);
                    }
                    true
                }
                (_, Some(MSG_PLAYER_DETECTED)) => {
                    if self.e.dist_to_player_2d() < 2.0 {
                        self.e.change_state(S_HUNT);
                    } else {
                        s(c.detected, false);
                    }
                    true
                }
                _ => food,
            },
            S_HUNT => match (ev, mt) {
                (EV_ENTER, _) => {
                    if !self.detected() {
                        self.e.change_state(S_GO_HOME);
                        self.sanity(false);
                    } else if self.e.st().prev() == S_ATTACK_SHORT {
                        self.e.change_state(S_HUNT_PAUSE);
                    } else {
                        self.e.h.change_sound_state(SOUND_HUNT);
                        self.speed(RUN);
                        self.e.send0(MSG_TIMEOUT, 0.1, true);
                        self.e.send0(MSG_TIMEOUT_2, 0.1, true);
                        self.e.h.music(true, MUSIC_ATTACK);
                        self.sanity(true);
                    }
                    true
                }
                (EV_LEAVE, _) => {
                    self.speed(WALK);
                    true
                }
                (_, Some(MSG_TIMEOUT_2)) => {
                    if MoverState::wanted_speed_amount(self.e.h) > 0.2 {
                        self.splash(SPLASH_RUN);
                    }
                    self.e.send0(MSG_TIMEOUT_2, 0.6, true);
                    true
                }
                (_, Some(MSG_TIMEOUT)) => {
                    let pf = self.e.h.player_feet();
                    self.move_to(pf);
                    self.e.send0(MSG_TIMEOUT, 0.4, true);
                    if !self.detected() {
                        self.e.change_state(S_GO_HOME);
                        self.sanity(false);
                    }
                    true
                }
                (EV_UPDATE, _) => {
                    if self.e.dist_to_player() < nad {
                        self.e.change_state(S_ATTACK_SHORT);
                    } else if g(c.stuck_at_door) {
                        break_door_state(self.e);
                    }
                    true
                }
                _ => food,
            },
            S_HUNT_PAUSE => match (ev, mt) {
                (EV_ENTER, _) => {
                    self.e.h.pf_stop();
                    let t = g(self.w.after_attack_pause);
                    self.e.send0(MSG_TIMEOUT, t, true);
                    true
                }
                (_, Some(MSG_TIMEOUT)) => {
                    self.e.change_state(S_HUNT);
                    true
                }
                _ => overload,
            },
            S_BREAK_DOOR => match (ev, mt) {
                (EV_ENTER, _) => {
                    self.e.h.pf_stop();
                    self.e.send0(MSG_TIMEOUT, 0.01, true);
                    true
                }
                (_, Some(MSG_TIMEOUT)) => {
                    self.e.h.attack(1);
                    let t = rand_rectf(0.5, 1.5);
                    self.e.send0(MSG_TIMEOUT_2, t, true);
                    true
                }
                (_, Some(MSG_TIMEOUT_2)) => {
                    self.e.change_state(S_HUNT);
                    true
                }
                (EV_UPDATE, _) => {
                    let tp = gv(c.temp_pos);
                    let h = &mut *self.e.h;
                    mover(self.e.c).turn_to_pos(tp, h);
                    true
                }
                _ => overload,
            },
            S_ATTACK_SHORT => match (ev, mt) {
                (EV_ENTER, _) => {
                    self.e.send0(MSG_TIMEOUT, 0.3, true);
                    true
                }
                (_, Some(MSG_TIMEOUT_2)) => {
                    let prev = self.e.st().prev();
                    self.e.change_state(prev);
                    true
                }
                (_, Some(MSG_TIMEOUT)) => {
                    self.splash(SPLASH_ATTACK);
                    self.e.h.attack(0);
                    self.e.send0(MSG_TIMEOUT_2, 0.5, true);
                    self.e.h.pf_stop();
                    true
                }
                _ => overload,
            },
            S_DEAD => match (ev, mt) {
                (EV_ENTER, _) => {
                    self.e.h.change_sound_state(SOUND_SILENT);
                    self.e.h.pf_stop();
                    self.e.h.player_remove_terror_enemy();
                    self.e.h.music(false, MUSIC_ATTACK);
                    self.e.h.body_set_active(false);
                    true
                }
                (_, Some(MSG_ANIMATION_OVER | MSG_PLAYER_DETECTED | MSG_HELP_ME)) => true,
                _ => false,
            },
            _ => {
                self.e.h.fatal_state(state);
                false
            }
        }
    }
}

/// `cLuxEnemy_WaterLurker::PlayerIsDetected`: `AbsHeightDistToPlayer() <= PlayerDetectionHeight`.
pub fn player_is_detected(e: &mut Env, w: &WlCtx) -> bool {
    let d = if e.h.player_dead() {
        100000.0
    } else {
        (e.h.body_feet().y - e.h.player_feet().y).abs()
    };
    d <= g(w.player_detection_height)
}

struct WlHandler<'a, 'b, 'c> {
    e: &'a mut Env<'b>,
    w: &'c WlCtx,
}

impl Handler for WlHandler<'_, '_, '_> {
    fn event(&mut self, state: i32, ev: i32, msg: Option<&Msg>) -> bool {
        let r = Wl {
            e: &mut *self.e,
            w: self.w,
        }
        .state_event(state, ev, msg);
        self.e.flush();
        r
    }
}

/// # Safety
/// `c`, `w`, `h` are live for the call; `m` is null or readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga3b_wl_event(
    c: *const Ctx,
    w: *const WlCtx,
    h: *const OhplGa3Host,
    state: i32,
    ev: i32,
    m: *const Msg,
) -> bool {
    // SAFETY: caller contract.
    let (mut host, w, m) = unsafe { (PHost(&*h), &*w, m.as_ref()) };
    // SAFETY: caller contract.
    let e = &mut Env::new(unsafe { &*c }, &mut host);
    WlHandler { e, w }.event(state, ev, m)
}

/// # Safety
/// `c`, `w`, `h` are live for the call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga3b_wl_player_is_detected(
    c: *const Ctx,
    w: *const WlCtx,
    h: *const OhplGa3Host,
) -> bool {
    // SAFETY: caller contract.
    let (mut host, w) = unsafe { (PHost(&*h), &*w) };
    // SAFETY: caller contract.
    let e = &mut Env::new(unsafe { &*c }, &mut host);
    player_is_detected(e, w)
}
