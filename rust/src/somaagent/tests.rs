use super::agent::AgentState;
use super::charmover::{AvoidBody, CharMoverState, MSG_STUCK, SpeedState, WallSample, wrap};
use super::host::{EV_MESSAGE, Event, Events, Host};
use super::misc::{ProxState, SoundListener, bark_due, proximity_update};
use super::pathfinder::{EV_RESULT_CALLBACK, MSG_END_OF_PATH, PathfinderState, TrackNodeInfo};
use super::statemachine::{
    ACT_ENTER, ACT_LEAVE, ACT_MESSAGE, ACT_SUB_STATE_OVER, ACT_TIMER_UP, ACT_UPDATE, SmExtra, SmPod,
};
use crate::math::{Matrixf, Vector3f, v3};

#[derive(Default)]
struct Fake {
    body: bool,
    map: bool,
    ent_pos: Vector3f,
    pos: Vector3f,
    feet: Vector3f,
    size: Vector3f,
    forward: Vector3f,
    yaw: f32,
    speed: f32,
    player: bool,
    dead: bool,
    player_pos: Vector3f,
    player_feet: Vector3f,
    los: bool,
    path: Vec<Vector3f>,
    track: Vec<TrackNodeInfo>,
    node_pos: Option<Vector3f>,
    calls: Vec<String>,
    last_pos: Vector3f,
}

impl Fake {
    fn new() -> Self {
        Self {
            body: true,
            map: true,
            size: v3(0.9, 1.9, 0.9),
            forward: v3(0.0, 0.0, -1.0),
            player: true,
            los: true,
            ..Self::default()
        }
    }
}

impl Host for Fake {
    fn has_body(&mut self) -> bool {
        self.body
    }
    fn has_map(&mut self) -> bool {
        self.map
    }
    fn has_mesh(&mut self) -> bool {
        true
    }
    fn ent_pos(&mut self) -> Vector3f {
        self.ent_pos
    }
    fn ent_scale_y(&mut self) -> f32 {
        1.0
    }
    fn ent_global_space_anim(&mut self) -> bool {
        false
    }
    fn body_pos(&mut self) -> Vector3f {
        self.pos
    }
    fn body_feet(&mut self) -> Vector3f {
        self.feet
    }
    fn body_size(&mut self) -> Vector3f {
        self.size
    }
    fn body_forward(&mut self) -> Vector3f {
        self.forward
    }
    fn body_yaw(&mut self) -> f32 {
        self.yaw
    }
    fn body_pitch(&mut self) -> f32 {
        0.0
    }
    fn body_entity_pitch_amount(&mut self) -> f32 {
        1.0
    }
    fn body_move_speed(&mut self) -> f32 {
        self.speed
    }
    fn body_on_ground(&mut self) -> bool {
        true
    }
    fn body_test_collision(&mut self) -> bool {
        true
    }
    fn body_gravity_active(&mut self) -> bool {
        true
    }
    fn body_max_step_size(&mut self) -> f32 {
        0.3
    }
    fn set_body_yaw(&mut self, yaw: f32) {
        self.yaw = yaw;
        self.calls.push(format!("yaw {yaw}"));
    }
    fn set_body_pos(&mut self, _p: Vector3f) {}
    fn set_body_feet(&mut self, p: Vector3f) {
        self.calls.push(format!("feet {}", p.y));
    }
    fn set_body_gravity(&mut self, _on: bool) {}
    fn set_body_collision(&mut self, _on: bool) {}
    fn has_pos_bone(&mut self) -> bool {
        false
    }
    fn pos_bone_world_pos(&mut self) -> Vector3f {
        Vector3f::default()
    }
    fn set_mesh_matrix(&mut self, _m: &Matrixf) {
        self.calls.push("mesh".into());
    }
    fn player_body_present(&mut self) -> bool {
        self.player
    }
    fn player_dead(&mut self) -> bool {
        self.dead
    }
    fn player_visibility_range_mul(&mut self) -> f32 {
        1.0
    }
    fn player_visibility_max_range(&mut self) -> f32 {
        -1.0
    }
    fn player_pos(&mut self) -> Vector3f {
        self.player_pos
    }
    fn player_feet(&mut self) -> Vector3f {
        self.player_feet
    }
    fn player_size_y(&mut self) -> f32 {
        1.8
    }
    fn line_of_sight(&mut self, _from: Vector3f, _to: Vector3f) -> bool {
        self.los
    }
    fn entity_in_player_los(&mut self) -> bool {
        false
    }
    fn floor_dist(&mut self, _start: Vector3f, _end: Vector3f) -> f32 {
        0.1
    }
    fn body_last_pos(&mut self) -> Vector3f {
        self.last_pos
    }
    fn body_right(&mut self) -> Vector3f {
        v3(1.0, 0.0, 0.0)
    }
    fn body_velocity(&mut self, _dt: f32) -> Vector3f {
        Vector3f::default()
    }
    fn body_add_pitch(&mut self, _p: f32) {}
    fn body_set_max_forward(&mut self, v: f32) {
        self.calls.push(format!("maxfwd {v}"));
    }
    fn body_set_move_acc(&mut self, _v: f32) {}
    fn body_set_move_deacc(&mut self, _v: f32) {}
    fn body_move_forward(&mut self) {
        self.calls.push("move".into());
    }
    fn raycast(&mut self, _s: Vector3f, _e: Vector3f) -> Option<(f32, Vector3f)> {
        None
    }
    fn avoid_query(&mut self, _c: Vector3f, _d: f32) -> usize {
        0
    }
    fn avoid_get(&mut self, _i: usize) -> AvoidBody {
        AvoidBody::default()
    }
    fn anim_current(&mut self) -> i32 {
        -1
    }
    fn anim_name_nonempty(&mut self, _state: i32) -> bool {
        true
    }
    fn play_move_anim(&mut self, state: i32) {
        self.calls.push(format!("anim {state}"));
    }
    fn set_move_anim_speed(&mut self, _speed: f32) {}
    fn idle_restart(&mut self) {}
    fn idle_extra_length(&mut self, _idx: i32) -> Option<f32> {
        None
    }
    fn play_idle_extra(&mut self, _idx: i32) {}
    fn pf_feet(&mut self) -> Vector3f {
        self.feet
    }
    fn pf_path_len(&mut self) -> usize {
        self.path.len()
    }
    fn pf_path_get(&mut self, i: usize) -> Vector3f {
        self.path.get(i).copied().unwrap_or_default()
    }
    fn pf_build_path(&mut self, goal: Vector3f) {
        self.path = vec![goal];
    }
    fn pf_track_len(&mut self) -> usize {
        self.track.len()
    }
    fn pf_track_info(&mut self, i: usize) -> TrackNodeInfo {
        self.track.get(i).copied().unwrap_or_default()
    }
    fn pf_track_node_pos(&mut self, _i: usize) -> Option<Vector3f> {
        self.node_pos
    }
    fn door_count(&mut self) -> usize {
        0
    }
    fn door_hit(&mut self, _i: usize, _o: Vector3f, _d: Vector3f, _m: f32) -> Option<Vector3f> {
        None
    }
}

fn messages(ev: &Events) -> Vec<(i32, i32)> {
    ev.0.iter()
        .filter(|e| e.kind == EV_MESSAGE)
        .map(|e| (e.a, e.b))
        .collect()
}

#[test]
fn wrap_folds_and_terminates() {
    assert!((wrap(7.0) - (7.0 - std::f32::consts::TAU)).abs() < 1e-6);
    assert!((wrap(-7.0) + 7.0 - std::f32::consts::TAU).abs() < 1e-6);
    assert!(wrap(f32::INFINITY).is_infinite());
    assert!(wrap(1e30) > 1e29);
    assert!(wrap(f32::NAN).is_nan());
}

#[test]
fn sound_listener_filters() {
    let l = SoundListener {
        min_prio: 2,
        ..SoundListener::default()
    };
    let (a, p) = (v3(0.0, 0.0, 0.0), v3(5.0, 0.0, 0.0));
    assert!(l.hears(true, true, a, p, 10.0, 2));
    assert!(!l.hears(true, true, a, p, 10.0, 1));
    assert!(!l.hears(false, true, a, p, 10.0, 2));
    assert!(!l.hears(true, false, a, p, 10.0, 2));
    assert!(!l.hears(true, true, a, p, 4.0, 2));
    assert!(
        !l.hears(true, true, a, p, 5.0, 2),
        "strictly inside the radius"
    );
    let far = SoundListener {
        hear_radius: 1.0,
        ..SoundListener::default()
    };
    // the hear radius only widens the sound's own radius, it never narrows it
    assert!(far.hears(true, true, a, p, 10.0, 0));
    let ignore = SoundListener {
        ignore_radius: 6.0,
        ..SoundListener::default()
    };
    assert!(!ignore.hears(true, true, a, p, 10.0, 0));
}

#[test]
fn bark_timer_rolls_over() {
    let mut c = 0.5;
    assert!(!bark_due(&mut c, 0.1, true, true, 1.0, 2.0));
    assert!(bark_due(&mut c, 0.5, true, true, 1.0, 2.0));
    assert!((1.0..=2.0).contains(&c));
    assert!(!bark_due(&mut c, 10.0, false, true, 1.0, 2.0));
    assert!(!bark_due(&mut c, 10.0, true, false, 1.0, 2.0));
}

#[test]
fn proximity_enters_and_leaves() {
    let mut h = Fake::new();
    h.player_pos = v3(2.0, 0.0, 0.0);
    let agent = AgentState::default();
    let mut states = [ProxState {
        exists: true,
        active: true,
        enter: 3.0,
        leave: 5.0,
        ..ProxState::default()
    }];
    let (mut t, mut ev) = (0.0, Events::default());
    proximity_update(&mut t, &mut states, 0.1, &agent, true, &mut h, &mut ev);
    assert!(ev.0.is_empty(), "under the 0.15 s tick");
    proximity_update(&mut t, &mut states, 0.1, &agent, true, &mut h, &mut ev);
    assert_eq!(messages(&ev), vec![(0x15, 0)]);
    h.player_pos = v3(4.0, 0.0, 0.0);
    proximity_update(&mut t, &mut states, 0.2, &agent, true, &mut h, &mut ev);
    assert_eq!(messages(&ev).len(), 1, "inside until past leave");
    h.player_pos = v3(6.0, 0.0, 0.0);
    proximity_update(&mut t, &mut states, 0.2, &agent, true, &mut h, &mut ev);
    assert_eq!(messages(&ev), vec![(0x15, 0), (0x16, 0)]);
}

#[test]
fn proximity_los_is_lazy() {
    let mut h = Fake::new();
    h.player_pos = v3(1.0, 0.0, 0.0);
    h.los = false;
    let agent = AgentState::default();
    let mut states = [ProxState {
        exists: true,
        active: true,
        los: true,
        enter: 3.0,
        leave: 5.0,
        ..ProxState::default()
    }];
    let (mut t, mut ev) = (1.0, Events::default());
    proximity_update(&mut t, &mut states, 0.1, &agent, true, &mut h, &mut ev);
    assert!(ev.0.is_empty());
    h.los = true;
    proximity_update(&mut t, &mut states, 1.0, &agent, true, &mut h, &mut ev);
    assert_eq!(messages(&ev), vec![(0x15, 0)]);
}

#[test]
fn senses_detect_after_four_sightings() {
    let mut h = Fake::new();
    h.player_pos = v3(0.0, 0.0, -5.0);
    h.player_feet = v3(0.0, -0.9, -5.0);
    let mut a = AgentState::default();
    let mut ev = Events::default();
    for _ in 0..3 {
        a.update_senses(0.2, &mut h, &mut ev);
        assert!(!a.seen && !a.detected);
    }
    a.update_senses(0.2, &mut h, &mut ev);
    assert!(a.seen && a.detected);
    assert_eq!(a.seen_count, 4);
    assert_eq!(messages(&ev), vec![(8, 0)]);
    h.los = false;
    a.update_senses(0.2, &mut h, &mut ev);
    assert!(!a.seen && !a.detected);
    assert_eq!(messages(&ev), vec![(8, 0), (9, 0)]);
}

#[test]
fn senses_inactive_and_dead_player() {
    let mut h = Fake::new();
    h.player_pos = v3(0.0, 0.0, -5.0);
    let mut a = AgentState {
        detected: true,
        ..AgentState::default()
    };
    let mut ev = Events::default();
    h.dead = true;
    a.update_senses(0.2, &mut h, &mut ev);
    assert!(!a.detected);
    assert_eq!(messages(&ev), vec![(9, 0)]);
    let mut ev = Events::default();
    a.set_senses_active(false, &mut ev);
    assert_eq!(messages(&ev), vec![(17, 0)]);
    a.set_senses_active(false, &mut ev);
    assert_eq!(ev.0.len(), 1, "no change, no message");
}

#[test]
fn auto_disable_fires_after_three_seconds() {
    let mut h = Fake::new();
    let mut a = AgentState {
        auto_disable: true,
        auto_disable_min_dist: 1.0,
        ..AgentState::default()
    };
    h.player_feet = v3(50.0, 0.0, 0.0);
    let mut ev = Events::default();
    for _ in 0..10 {
        a.update_auto_disable(0.3, &mut h, &mut ev);
    }
    assert!(ev.0.is_empty());
    a.update_auto_disable(0.3, &mut h, &mut ev);
    let kinds: Vec<u32> = ev.0.iter().map(|e| e.kind).collect();
    assert_eq!(kinds, vec![2, 3]);
    assert!(!a.auto_disable);
}

#[test]
fn step_up_lifts_feet() {
    let mut h = Fake::new();
    h.feet = v3(0.0, 1.0, 0.0);
    let a = AgentState::default();
    a.step_up(&mut h);
    assert_eq!(h.calls.len(), 1);
    assert!(h.calls.first().is_some_and(|c| c.starts_with("feet")));
}

fn sm(extra: &mut SmExtra) -> SmPod {
    SmPod {
        cur: -1,
        prev: -1,
        next: 0,
        sub_cur: -1,
        sub_prev: -1,
        sub_next: -1,
        in_sub: false,
        rs: extra,
    }
}

fn drain(
    p: &mut SmPod,
    mut on: impl FnMut(&mut SmPod, u32, bool, i32, u64),
) -> Vec<(u32, bool, i32)> {
    let mut out = Vec::new();
    while let Some(a) = p.next(&|_, id| (0..4).contains(&id)) {
        out.push((a.kind, a.sub, a.state));
        on(p, a.kind, a.sub, a.state, a.arg_u);
    }
    out
}

#[test]
fn state_machine_enters_and_updates() {
    let mut x = SmExtra::new();
    let mut p = sm(&mut x);
    p.begin_update(0.1);
    let seq = drain(&mut p, |_, _, _, _, _| {});
    assert_eq!(seq, vec![(ACT_ENTER, false, 0), (ACT_UPDATE, false, 0)]);
    assert_eq!((p.cur, p.prev), (0, -1));
    p.next = 2;
    p.begin_update(0.1);
    let seq = drain(&mut p, |_, _, _, _, _| {});
    assert_eq!(
        seq,
        vec![
            (ACT_LEAVE, false, 0),
            (ACT_ENTER, false, 2),
            (ACT_UPDATE, false, 2)
        ]
    );
    assert_eq!((p.cur, p.prev), (2, 0));
}

#[test]
fn state_machine_sees_script_effects_mid_update() {
    let mut x = SmExtra::new();
    let mut p = sm(&mut x);
    p.begin_update(0.1);
    // the main Update asks for a sub state: the sub phase of the same update must see it
    let seq = drain(&mut p, |p, kind, sub, _, _| {
        if kind == ACT_UPDATE && !sub {
            p.sub_next = 1;
        }
    });
    assert_eq!(
        seq,
        vec![
            (ACT_ENTER, false, 0),
            (ACT_UPDATE, false, 0),
            (ACT_ENTER, true, 1),
            (ACT_UPDATE, true, 1)
        ]
    );
    p.sub_next = -1;
    p.begin_update(0.1);
    let seq = drain(&mut p, |_, _, _, _, _| {});
    assert_eq!(
        seq,
        vec![
            (ACT_UPDATE, false, 0),
            (ACT_LEAVE, true, 1),
            (ACT_SUB_STATE_OVER, false, 0)
        ]
    );
    assert_eq!(p.sub_cur, -1);
}

#[test]
fn state_machine_timers_fire_in_order_and_survive_new_ones() {
    let mut x = SmExtra::new();
    let mut p = sm(&mut x);
    p.begin_update(0.1);
    drain(&mut p, |_, _, _, _, _| {});
    {
        let x = unsafe { &mut *p.rs };
        x.add_timer(false, 7, 0.15);
        x.add_timer(false, 9, 0.05);
        x.add_timer(false, 11, 5.0);
        assert!(x.timer_exists(false, 11));
    }
    p.begin_update(0.1);
    let mut fired = Vec::new();
    let seq = drain(&mut p, |p, kind, _, _, id| {
        if kind == ACT_TIMER_UP {
            fired.push(id);
            if id == 9 {
                // a timer added from a TimerUp is not ticked in this update
                unsafe { &mut *p.rs }.add_timer(false, 20, 0.01);
            }
        }
    });
    assert_eq!(fired, vec![9]);
    assert_eq!(seq.iter().filter(|a| a.0 == ACT_TIMER_UP).count(), 1);
    let x = unsafe { &mut *p.rs };
    assert_eq!(x.timer_len(false), 3);
    x.stop_timer(false, 11);
    assert!(!x.timer_exists(false, 11));
    x.clear_timers(false);
    assert_eq!(x.timer_len(false), 0);
}

#[test]
fn state_machine_message_runs_inside_update() {
    let mut x = SmExtra::new();
    let mut p = sm(&mut x);
    p.begin_update(0.1);
    let mut nested = Vec::new();
    let seq = drain(&mut p, |p, kind, _, _, _| {
        if kind == ACT_ENTER {
            p.begin_message(5);
            while let Some(a) = p.next(&|_, _| true) {
                nested.push((a.kind, a.arg_i));
            }
        }
    });
    assert_eq!(nested, vec![(ACT_MESSAGE, 5)]);
    assert_eq!(seq, vec![(ACT_ENTER, false, 0), (ACT_UPDATE, false, 0)]);
}

#[test]
fn state_machine_rejects_missing_state() {
    let mut x = SmExtra::new();
    let mut p = sm(&mut x);
    p.next = 9;
    p.begin_update(0.1);
    let seq = drain(&mut p, |_, _, _, _, _| {});
    assert!(seq.is_empty());
    assert_eq!(p.cur, -1);
    p.reset(true);
    assert_eq!((p.next, p.cur), (0, -1));
}

#[test]
fn mover_turns_and_moves_toward_goal() {
    let mut h = Fake::new();
    let mut cm = CharMoverState::default();
    cm.move_to_pos(v3(5.0, 0.0, 0.0), false);
    let mut ev = Events::default();
    cm.update(0.1, None, &mut [], &mut h, &mut ev);
    assert!(cm.moving && !cm.move_requested);
    assert!(h.yaw != 0.0, "turned toward +x");
    assert!(h.calls.iter().any(|c| c.starts_with("maxfwd")));
    // facing away: the break multiplier (0) stops forward motion
    assert!(!h.calls.iter().any(|c| c == "move"));
    assert!(ev.0.is_empty());
}

#[test]
fn mover_stuck_counter_sends_message() {
    let mut h = Fake::new();
    h.speed = 1.0;
    let mut cm = CharMoverState::default();
    let mut ev = Events::default();
    for _ in 0..80 {
        cm.move_to_pos(v3(0.0, 0.0, -50.0), false);
        cm.update(0.05, None, &mut [], &mut h, &mut ev);
    }
    assert!(messages(&ev).contains(&(MSG_STUCK, 0)));
}

#[test]
fn mover_turn_to_reports_done() {
    let mut h = Fake::new();
    let mut cm = CharMoverState::default();
    cm.turn_to(0.5);
    let mut ev = Events::default();
    for _ in 0..40 {
        cm.update(0.05, None, &mut [], &mut h, &mut ev);
    }
    assert!(!cm.turning);
    assert!(messages(&ev).contains(&(5, 0)));
    assert!(ev.0.iter().any(|e| e.kind == 5));
}

#[test]
fn mover_speed_state_overrides() {
    let mut h = Fake::new();
    let mut cm = CharMoverState::default();
    cm.move_to_pos(v3(0.0, 0.0, -10.0), false);
    let s = SpeedState {
        forward: 2.5,
        forward_acc: 4.0,
        ..SpeedState::default()
    };
    let mut ev = Events::default();
    cm.update(0.1, Some(&s), &mut [], &mut h, &mut ev);
    assert!(h.calls.contains(&"maxfwd 2.5".to_string()));
    assert!(h.calls.contains(&"move".to_string()));
}

#[test]
fn mover_wall_samples_cast_rays() {
    let mut h = Fake::new();
    let mut cm = CharMoverState {
        wall_avoid: true,
        wall_radius: 2.0,
        wall_steer: 1.0,
        ..CharMoverState::default()
    };
    let mut walls = [WallSample::default(); 3];
    cm.move_to_pos(v3(0.0, 0.0, -10.0), false);
    let mut ev = Events::default();
    for _ in 0..3 {
        cm.update(0.1, None, &mut walls, &mut h, &mut ev);
    }
    assert!(walls.iter().all(|w| w.weight > 0.0));
}

#[test]
fn pathfinder_arrives_and_reports() {
    let mut h = Fake::new();
    let mut cm = CharMoverState::default();
    let mut pf = PathfinderState::default();
    pf.move_to(v3(0.0, 0.0, -1.0), true, false, Some(&mut cm), &mut h);
    assert!(pf.moving && cm.move_requested);
    let mut ev = Events::default();
    pf.update(0.1, Some(&mut cm), &mut h, &mut ev);
    assert!(pf.goal_pending && pf.moving);
    pf.send_goal(Some(&mut cm), &mut h);
    assert!(!pf.goal_pending);
    h.pos = v3(0.0, 0.0, -1.0);
    pf.update(0.1, Some(&mut cm), &mut h, &mut ev);
    assert!(!pf.moving);
    assert_eq!(messages(&ev), vec![(MSG_END_OF_PATH, 1)]);
    assert!(ev.0.iter().any(|e| e.kind == EV_RESULT_CALLBACK));
}

#[test]
fn pathfinder_gives_up_when_stuck() {
    let mut h = Fake::new();
    let mut cm = CharMoverState::default();
    let mut pf = PathfinderState::default();
    pf.move_to(v3(0.0, 0.0, -50.0), false, false, Some(&mut cm), &mut h);
    let mut ev = Events::default();
    pf.on_message(MSG_STUCK, Some(&mut cm), &mut h, &mut ev);
    assert!(!pf.moving);
    assert_eq!(messages(&ev), vec![(MSG_END_OF_PATH, 0)]);
}

#[test]
fn pathfinder_no_progress_history_arrives_near_node() {
    let mut h = Fake::new();
    let mut pf = PathfinderState::default();
    pf.move_to(v3(0.0, 0.0, -3.0), false, false, None, &mut h);
    let mut ev = Events::default();
    for _ in 0..160 {
        pf.update(0.016, None, &mut h, &mut ev);
        h.pos.z -= 0.0001;
        if !pf.moving {
            break;
        }
    }
    assert!(pf.moving, "no history shortcut while still approaching");
}

#[test]
fn pathfinder_history_shortcut_needs_151_samples_and_growth() {
    let mut h = Fake::new();
    let mut pf = PathfinderState::default();
    pf.move_to(v3(0.0, 0.0, -1.0), false, false, None, &mut h);
    let mut ev = Events::default();
    for i in 0..150 {
        h.pos.z = i as f32 * 0.001;
        pf.update(0.016, None, &mut h, &mut ev);
        assert!(pf.moving, "sample {i}");
    }
    h.pos.z = 0.151;
    pf.update(0.016, None, &mut h, &mut ev);
    assert!(
        !pf.moving,
        "151st sample with growing distance ends the path"
    );
    assert_eq!(messages(&ev), vec![(MSG_END_OF_PATH, 1)]);
}

#[test]
fn pathfinder_track_walks_nodes() {
    let mut h = Fake::new();
    h.track = vec![
        TrackNodeInfo {
            min_wait: 0.5,
            max_wait: 0.5,
            anim_nonempty: true,
            loop_anim: false,
        };
        2
    ];
    h.node_pos = Some(v3(1.0, 0.0, 0.0));
    let mut pf = PathfinderState {
        track_active: true,
        track_idx: 0,
        ..PathfinderState::default()
    };
    let mut cm = CharMoverState::default();
    pf.start_track_node(Some(&mut cm), &mut h);
    assert!(pf.moving && !pf.at_track_node);
    h.pos = v3(1.0, 0.0, 0.0);
    let mut ev = Events::default();
    pf.update(0.1, Some(&mut cm), &mut h, &mut ev);
    assert!(pf.at_track_node);
    let kinds: Vec<u32> = ev.0.iter().map(|e| e.kind).collect();
    assert!(kinds.contains(&9) && messages(&ev).contains(&(11, 0)));
    for _ in 0..10 {
        pf.update(0.1, Some(&mut cm), &mut h, &mut ev);
    }
    assert_eq!(pf.track_idx, 1);
    pf.track_idx = 1;
    pf.go_to_next_track_node(Some(&mut cm), &mut h, &mut ev);
    assert!(!pf.track_active, "ran off the end without looping");
    assert_eq!(pf.track_idx, 1);
    assert!(messages(&ev).contains(&(12, 0)));
}

#[test]
fn events_are_plain_data() {
    let e = Event::default();
    assert_eq!(std::mem::size_of_val(&e), 24);
}
