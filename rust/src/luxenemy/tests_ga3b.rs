use super::ctx::{Ctx, Env};
use super::host::*;
use super::machine::*;
use super::manpig::*;
use super::waterlurker::{self, Wl, WlCtx};

#[derive(Default)]
struct Fake {
    log: Vec<u32>,
    lantern: bool,
    inside: bool,
    dead: bool,
    activate: bool,
    body_feet: [f32; 3],
    player_feet: [f32; 3],
}

impl Host for Fake {
    fn call(&mut self, op: u32, io: &mut Io) -> i32 {
        self.log.push(op);
        match op {
            OP_PLAYER_LANTERN => return i32::from(self.lantern),
            OP_INSIDE_PLAYER_VIEW => return i32::from(self.inside),
            OP_PLAYER_DEAD => return i32::from(self.dead),
            OP_ACTIVATE_SOUND_ALLOWED => return i32::from(self.activate),
            OP_BODY_FEET => io.v[0] = self.body_feet,
            OP_PLAYER_FEET => io.v[0] = self.player_feet,
            _ => {}
        }
        0
    }
}

struct Store {
    cur: i32,
    next: i32,
    prev: i32,
    last_known: [f32; 3],
    can_see: bool,
    nad: f32,
    force_charge: bool,
    play_activate: bool,
    lantern_count: f32,
    check_flash: f32,
    sensitivity: f32,
    disable_triggers: bool,
    height: f32,
}

impl Store {
    fn new() -> Self {
        Self {
            cur: 0,
            next: 0,
            prev: 0,
            last_known: [0.0; 3],
            can_see: false,
            nad: 1.9,
            force_charge: false,
            play_activate: true,
            lantern_count: 0.0,
            check_flash: 0.0,
            sensitivity: 1.0,
            disable_triggers: false,
            height: 0.6,
        }
    }

    fn ctx(&mut self) -> Ctx {
        // SAFETY: all-null pointers are a valid `Ctx`; tests set only what the code under test reads.
        let mut c: Ctx = unsafe { std::mem::zeroed() };
        c.cur = &mut self.cur;
        c.next = &mut self.next;
        c.prev = &mut self.prev;
        c.last_known = self.last_known.as_mut_ptr();
        c.can_see = &mut self.can_see;
        c.normal_attack_distance = &mut self.nad;
        c
    }

    fn pig(&mut self) -> PigCtx {
        // SAFETY: as in `ctx`.
        let mut p: PigCtx = unsafe { std::mem::zeroed() };
        p.force_charge = &mut self.force_charge;
        p.play_activate_sound = &mut self.play_activate;
        p.in_lantern_count = &mut self.lantern_count;
        p.check_flash_light = &mut self.check_flash;
        p.lantern_sensitivity = &mut self.sensitivity;
        p.disable_triggers = &mut self.disable_triggers;
        p
    }

    fn wl(&mut self) -> WlCtx {
        // SAFETY: as in `ctx`.
        let mut w: WlCtx = unsafe { std::mem::zeroed() };
        w.player_detection_height = &mut self.height;
        w
    }
}

fn msg(ty: i32) -> Msg {
    Msg {
        ty,
        ..Msg::default()
    }
}

#[test]
fn pig_default_player_detected_goes_alert() {
    let mut s = Store::new();
    let (c, p) = (s.ctx(), s.pig());
    let mut h = Fake::default();
    let e = &mut Env::new(&c, &mut h);
    let m = msg(MSG_PLAYER_DETECTED);
    assert!(Pig { e, p: &p }.state_event(-1, EV_MESSAGE, Some(&m)));
    assert_eq!(s.next, S_ALERT);
}

#[test]
fn pig_idle_in_range_plays_enabled_sound_once_and_patrols() {
    let mut s = Store::new();
    let (c, p) = (s.ctx(), s.pig());
    let mut h = Fake {
        activate: true,
        ..Fake::default()
    };
    let e = &mut Env::new(&c, &mut h);
    let m = msg(MSG_PLAYER_IN_RANGE);
    assert!(Pig { e, p: &p }.state_event(S_IDLE, EV_MESSAGE, Some(&m)));
    assert_eq!(s.next, S_PATROL);
    assert!(h.log.contains(&OP_SET_ACTIVATE_SOUND_MADE) && h.log.contains(&OP_PLAY_SOUND));

    s.play_activate = false;
    let (c, p) = (s.ctx(), s.pig());
    let mut h = Fake {
        activate: true,
        ..Fake::default()
    };
    let e = &mut Env::new(&c, &mut h);
    Pig { e, p: &p }.state_event(S_IDLE, EV_MESSAGE, Some(&m));
    assert!(!h.log.contains(&OP_PLAY_SOUND));
}

#[test]
fn pig_unknown_state_is_fatal() {
    let mut s = Store::new();
    let (c, p) = (s.ctx(), s.pig());
    let mut h = Fake::default();
    let e = &mut Env::new(&c, &mut h);
    assert!(!Pig { e, p: &p }.state_event(S_EAT, EV_ENTER, None));
    assert!(h.log.contains(&OP_ENEMY_STATE_FATAL));
}

#[test]
fn pig_overloaded_messages_are_swallowed_per_state() {
    let mut s = Store::new();
    let (c, p) = (s.ctx(), s.pig());
    let mut h = Fake::default();
    let e = &mut Env::new(&c, &mut h);
    let m = msg(MSG_HELP_ME);
    let mut pig = Pig { e, p: &p };
    assert!(pig.state_event(S_HUNT, EV_MESSAGE, Some(&m)));
    assert!(!pig.state_event(S_TRACK, EV_MESSAGE, Some(&m)));
    assert!(!pig.state_event(S_WAIT, EV_MESSAGE, Some(&msg(MSG_PLAYER_DETECTED))));
}

#[test]
fn pig_hunt_update_prefers_forced_charge_when_close() {
    let mut s = Store::new();
    s.can_see = true;
    s.force_charge = true;
    let (c, p) = (s.ctx(), s.pig());
    let mut h = Fake {
        body_feet: [0.0, 0.0, 0.0],
        player_feet: [1.0, 0.0, 0.0],
        ..Fake::default()
    };
    let e = &mut Env::new(&c, &mut h);
    assert!(Pig { e, p: &p }.state_event(S_HUNT, EV_UPDATE, None));
    assert_eq!(s.next, S_ATTACK_LONG);
    assert!(!s.force_charge);
}

#[test]
fn lantern_exposure_builds_decays_and_resets() {
    let mut s = Store::new();
    s.sensitivity = 2.0;
    let (c, p) = (s.ctx(), s.pig());
    let mut h = Fake {
        lantern: true,
        inside: true,
        player_feet: [1.0, 2.0, 3.0],
        ..Fake::default()
    };
    let e = &mut Env::new(&c, &mut h);
    update_check_in_lantern(e, &p, 0.016);
    assert!((s.lantern_count - 0.2).abs() < 1e-6);
    assert_eq!(s.last_known, [1.0, 2.0, 3.0]);
    assert!((s.check_flash - 0.2).abs() < 1e-6);

    let e = &mut Env::new(&c, &mut h);
    update_check_in_lantern(e, &p, 0.016);
    assert!((s.lantern_count - 0.2).abs() < 1e-6, "interval not elapsed");

    s.check_flash = 0.0;
    h.inside = false;
    let e = &mut Env::new(&c, &mut h);
    update_check_in_lantern(e, &p, 0.016);
    assert_eq!(s.lantern_count, 0.0);

    h.lantern = false;
    s.lantern_count = 0.7;
    let e = &mut Env::new(&c, &mut h);
    update_check_in_lantern(e, &p, 0.016);
    assert_eq!(s.lantern_count, 0.0);
}

#[test]
fn lantern_disabled_triggers_reset() {
    let mut s = Store::new();
    s.disable_triggers = true;
    s.lantern_count = 0.5;
    let (c, p) = (s.ctx(), s.pig());
    let mut h = Fake {
        lantern: true,
        inside: true,
        ..Fake::default()
    };
    let e = &mut Env::new(&c, &mut h);
    update_check_in_lantern(e, &p, 0.016);
    assert_eq!(s.lantern_count, 0.0);
}

#[test]
fn lurker_detection_is_height_band_and_dead_player_never() {
    let mut s = Store::new();
    let (c, w) = (s.ctx(), s.wl());
    let mut h = Fake {
        player_feet: [0.0, 0.5, 0.0],
        ..Fake::default()
    };
    assert!(waterlurker::player_is_detected(
        &mut Env::new(&c, &mut h),
        &w
    ));
    h.player_feet = [0.0, 0.7, 0.0];
    assert!(!waterlurker::player_is_detected(
        &mut Env::new(&c, &mut h),
        &w
    ));
    h.player_feet = [0.0, 0.6, 0.0];
    assert!(waterlurker::player_is_detected(
        &mut Env::new(&c, &mut h),
        &w
    ));
    h.dead = true;
    h.player_feet = [0.0, 0.0, 0.0];
    assert!(!waterlurker::player_is_detected(
        &mut Env::new(&c, &mut h),
        &w
    ));
}

#[test]
fn lurker_default_and_dead_state_messages() {
    let mut s = Store::new();
    s.cur = S_HUNT;
    s.next = S_HUNT;
    let (c, w) = (s.ctx(), s.wl());
    let mut h = Fake::default();
    let e = &mut Env::new(&c, &mut h);
    let mut wl = Wl { e, w: &w };
    assert!(wl.state_event(-1, EV_MESSAGE, Some(&msg(MSG_PLAYER_DEAD))));
    assert_eq!(s.next, S_IDLE);
    assert!(wl.state_event(S_DEAD, EV_MESSAGE, Some(&msg(MSG_HELP_ME))));
    assert!(!wl.state_event(S_DEAD, EV_MESSAGE, Some(&msg(MSG_FOOD_IN_RANGE))));
    assert!(wl.state_event(S_SEARCH, EV_MESSAGE, Some(&msg(MSG_FOOD_IN_RANGE))));
    assert!(!wl.state_event(S_PATROL, EV_ENTER, None));
    assert!(h.log.contains(&OP_ENEMY_STATE_FATAL));
}
