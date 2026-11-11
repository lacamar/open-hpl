use super::ctx::{Ctx, Env, g};
use super::grunt::{self, GruntHandler};
use super::host::*;
use super::machine::*;
use super::mover::{MOVE_RUNNING, MOVE_WALKING, MoverParams, MoverState};
use super::pathfinder::{NodeQuery, PfWin, node_at_pos, update_moving};
use crate::math::{Vector3f, c_srand, v3};

#[derive(Default)]
struct Fix {
    queue: Queue,
    cur: i32,
    next: i32,
    prev: i32,
    disabled: bool,
    mover: MoverState,
    pf_moving: bool,
    pf_goal: [f32; 3],
    pf_win: PfWin,
    look_count: f32,
    los_count: i32,
    can_see: bool,
    detected: bool,
    in_range: bool,
    check_door_count: f32,
    stuck_at_door: bool,
    stuck_door_id: i32,
    last_pos_count: f32,
    last_known: [f32; 3],
    fov_mul: f32,
    sight_range: f32,
    health: f32,
    blind: bool,
    darkness_sight_range: f32,
    in_darkness_light_level: f32,
    crouch_visible_range_mul: f32,
    skip_visibility_handicaps: bool,
    fov: f32,
    fov_x_mul: f32,
    activation_distance: f32,
    max_regen_health: f32,
    regen_health_speed: f32,
    damage_count: f32,
    temp_pos: [f32; 3],
    temp_val: f32,
    temp_val_l: f32,
    attack_hit_counter: i32,
    forward_speed: f32,
    backward_speed: f32,
    forward_acc: f32,
    forward_deacc: f32,
    def_forward_speed: [[f32; 2]; 2],
    def_backward_speed: [[f32; 2]; 2],
    def_forward_acc: [[f32; 2]; 2],
    def_forward_deacc: [[f32; 2]; 2],
    pose: i32,
    start_position: [f32; 3],
    hear_volume: f32,
    player_search_time: f32,
    normal_attack_distance: f32,
    cur_patrol: i32,
    is_seen_by_player: bool,
    auto_remove_at_path_end: bool,
    wait_time: f32,
    alert_run_towards_count: f32,
    groggy_damage_count: f32,
    alert_to_hunt_distance: f32,
    alert_to_instant_hunt_distance: f32,
    hunt_pause_min_time: f32,
    hunt_pause_max_time: f32,
    increase_alert_speed_distance: f32,
    increased_alert_speed_mul: f32,
    alert_run_towards_to_hunt_limit: f32,
    alert_run_towards_check_distance: f32,
    idle_extra_time_min: f32,
    idle_extra_time_max: f32,
    idle_extra_num: i32,
}

macro_rules! build_ctx {
    ($f:expr; $($n:ident),*) => { Ctx { $($n: (&raw mut $f.$n).cast()),* } };
}

impl Fix {
    fn new() -> Box<Self> {
        let mut f = Box::<Self>::default();
        f.next = 0;
        f.cur = STATE_LAST;
        f.prev = STATE_LAST;
        f.fov = 6.0;
        f.fov_x_mul = 1.0;
        f.fov_mul = 1.0;
        f.sight_range = 20.0;
        f.health = 100.0;
        f.activation_distance = 10.0;
        f.idle_extra_num = 3;
        f.idle_extra_time_min = 2.0;
        f.idle_extra_time_max = 6.0;
        f.normal_attack_distance = 1.5;
        f.groggy_damage_count = 5.0;
        f.stuck_door_id = -1;
        f.mover = MoverState::default();
        f
    }

    fn ctx(&mut self) -> Ctx {
        build_ctx!(self; queue, cur, next, prev, disabled, mover, pf_moving, pf_goal, pf_win, look_count,
            los_count, can_see, detected, in_range, check_door_count, stuck_at_door, stuck_door_id,
            last_pos_count, last_known, fov_mul, sight_range, health, blind, darkness_sight_range,
            in_darkness_light_level, crouch_visible_range_mul, skip_visibility_handicaps, fov, fov_x_mul,
            activation_distance, max_regen_health, regen_health_speed, damage_count, temp_pos, temp_val,
            temp_val_l, attack_hit_counter, forward_speed, backward_speed, forward_acc, forward_deacc,
            def_forward_speed, def_backward_speed, def_forward_acc, def_forward_deacc, pose, start_position,
            hear_volume, player_search_time, normal_attack_distance, cur_patrol, is_seen_by_player,
            auto_remove_at_path_end, wait_time, alert_run_towards_count, groggy_damage_count,
            alert_to_hunt_distance, alert_to_instant_hunt_distance, hunt_pause_min_time, hunt_pause_max_time,
            increase_alert_speed_distance, increased_alert_speed_mul, alert_run_towards_to_hunt_limit,
            alert_run_towards_check_distance, idle_extra_time_min, idle_extra_time_max, idle_extra_num)
    }
}

struct Fake {
    pos: Vector3f,
    last_pos: Vector3f,
    feet: Vector3f,
    forward: Vector3f,
    size: Vector3f,
    yaw: f32,
    move_speed: f32,
    vel: Vector3f,
    player_pos: Vector3f,
    dead: bool,
    path: Vec<Vector3f>,
    nodes: Vec<Vector3f>,
    bv_hit: bool,
    patrols: i32,
    log: Vec<String>,
}

impl Fake {
    fn new() -> Self {
        Self {
            pos: v3(0.0, 1.0, 0.0),
            last_pos: v3(0.0, 1.0, 0.0),
            feet: v3(0.0, 0.0, 0.0),
            forward: v3(0.0, 0.0, -1.0),
            size: v3(0.8, 1.8, 0.8),
            yaw: 0.0,
            move_speed: 0.0,
            vel: v3(0.0, 0.0, 0.0),
            player_pos: v3(10.0, 1.0, 0.0),
            dead: false,
            path: Vec::new(),
            nodes: Vec::new(),
            bv_hit: false,
            patrols: 0,
            log: Vec::new(),
        }
    }
}

fn put(io: &mut Io, v: Vector3f) {
    io.v[0] = a_of(v);
}

impl Host for Fake {
    fn call(&mut self, op: u32, io: &mut Io) -> i32 {
        match op {
            OP_STEP_SIZE => io.f[0] = 1.0 / 60.0,
            OP_BODY_POS => put(io, self.pos),
            OP_BODY_LAST_POS => put(io, self.last_pos),
            OP_BODY_FEET => put(io, self.feet),
            OP_BODY_SIZE => put(io, self.size),
            OP_BODY_FORWARD => put(io, self.forward),
            OP_BODY_YAW => io.f[0] = self.yaw,
            OP_BODY_MOVE_SPEED_FWD => io.f[0] = self.move_speed,
            OP_BODY_VELOCITY | OP_PLAYER_VELOCITY => put(io, self.vel),
            OP_PLAYER_POS => put(io, self.player_pos),
            OP_PLAYER_FEET => put(io, self.player_pos),
            OP_PLAYER_DEAD => return i32::from(self.dead),
            OP_PLAYER_TERROR => io.f[0] = 0.0,
            OP_PF_PATH_LEN => return self.path.len() as i32,
            OP_PF_PATH_BACK => put(io, self.path.last().copied().unwrap_or_default()),
            OP_PF_PATH_POP => {
                self.path.pop();
            }
            OP_PF_PATH_CLEAR => self.path.clear(),
            OP_BV_CONTAINS => return i32::from(self.bv_hit),
            OP_NC_PRESENT => return 1,
            OP_NC_COUNT => return self.nodes.len() as i32,
            OP_NC_ITER_BEGIN => return self.nodes.len() as i32,
            OP_NC_ITER_NODE => {
                put(
                    io,
                    self.nodes
                        .get(io.i[0] as usize)
                        .copied()
                        .unwrap_or_default(),
                );
                return 1;
            }
            OP_NC_FREE_PATH => return 1,
            OP_PATROL_COUNT => return self.patrols,
            OP_PLAY_ANIM => {
                self.log.push(format!("anim {} {}", io.i[0], io.i[1]));
                return 1;
            }
            OP_BODY_MOVE_FWD => self.log.push("move".into()),
            OP_BODY_ADD_YAW => {
                self.yaw += io.f[0];
                self.log.push(format!("yaw {}", io.f[0]));
            }
            OP_ANIM_EXISTS => return 0,
            OP_PF_MOVE_TO => self.log.push(format!("moveto {:?}", io.v[0])),
            OP_PF_STOP => self.log.push("stop".into()),
            OP_SET_ACTIVE_FALSE => self.log.push("setactive".into()),
            other => self.log.push(format!("op{other}")),
        }
        0
    }
}

struct Recorder<'a> {
    st: StatePtrs,
    q: *mut Queue,
    log: &'a mut Vec<String>,
    on_msg: Option<fn(&StatePtrs, *mut Queue, &Msg)>,
}

impl Handler for Recorder<'_> {
    fn event(&mut self, state: i32, ev: i32, msg: Option<&Msg>) -> bool {
        if state < 0 {
            return false;
        }
        self.log
            .push(format!("{state}:{ev}:{}", msg.map_or(-1, |m| m.ty)));
        if let (Some(m), Some(f)) = (msg, self.on_msg) {
            f(&self.st, self.q, m);
        }
        true
    }
}

fn st_of(f: &mut Fix) -> StatePtrs {
    StatePtrs {
        cur: &raw mut f.cur,
        next: &raw mut f.next,
        prev: &raw mut f.prev,
    }
}

#[test]
fn scheduler_orders_enter_update_messages() {
    let mut f = Fix::new();
    let st = st_of(&mut f);
    let q: *mut Queue = &raw mut f.queue;
    for (ty, t, local) in [(1, 0.0, false), (2, 0.05, false), (3, 0.0, true)] {
        send_message(
            q,
            st.cur(),
            false,
            false,
            ty,
            t,
            local,
            v3(0.0, 0.0, 0.0),
            0.0,
            0,
        );
    }
    let mut log = Vec::new();
    let mut r = Recorder {
        st,
        q,
        log: &mut log,
        on_msg: None,
    };
    update_state_machine(&st, q, 0.02, &mut r);
    // A local message queued while no state exists is global.
    assert_eq!(log, ["0:0:-1", "0:2:-1", "0:3:1", "0:3:3"]);
    assert_eq!(f.queue.msgs.len(), 1);
    assert_eq!(f.queue.msgs[0].ty, 2);
}

#[test]
fn handler_pushes_are_seen_in_the_same_pass_and_state_changes_apply_after() {
    fn on_msg(st: &StatePtrs, q: *mut Queue, m: &Msg) {
        if m.ty == 1 {
            send_message(
                q,
                st.cur(),
                false,
                false,
                4,
                0.0,
                false,
                v3(0.0, 0.0, 0.0),
                0.0,
                0,
            );
            st.change_state(7);
        }
    }
    let mut f = Fix::new();
    let st = st_of(&mut f);
    let q: *mut Queue = &raw mut f.queue;
    send_message(
        q,
        STATE_LAST,
        false,
        false,
        1,
        0.0,
        false,
        v3(0.0, 0.0, 0.0),
        0.0,
        0,
    );
    let mut log = Vec::new();
    let mut r = Recorder {
        st,
        q,
        log: &mut log,
        on_msg: Some(on_msg),
    };
    update_state_machine(&st, q, 0.02, &mut r);
    assert_eq!(
        log,
        ["0:0:-1", "0:2:-1", "0:3:1", "0:3:4", "0:1:-1", "7:0:-1"]
    );
    assert_eq!((f.prev, f.cur), (0, 7));
}

#[test]
fn send_filters() {
    let mut f = Fix::new();
    let q: *mut Queue = &raw mut f.queue;
    let z = v3(0.0, 0.0, 0.0);
    assert!(!send_message(q, 0, true, false, 1, 0.0, false, z, 0.0, 0));
    assert!(!send_message(
        q,
        0,
        false,
        true,
        MSG_SOUND_HEARD,
        0.0,
        false,
        z,
        0.0,
        0
    ));
    assert!(send_message(
        q,
        0,
        false,
        true,
        MSG_PLAYER_IN_RANGE,
        0.0,
        false,
        z,
        0.0,
        0
    ));
    assert!(send_message(
        q,
        0,
        false,
        true,
        MSG_END_OF_PATH,
        0.0,
        true,
        z,
        0.0,
        0
    ));
    assert_eq!(f.queue.msgs.last().map(|m| m.state), Some(0));
}

fn params() -> MoverParams {
    MoverParams {
        turn_min_break_angle: 0.5,
        turn_break_mul: 0.4,
        turn_speed_mul: 3.0,
        turn_max_speed: 2.0,
        move_speed_anim_mul: 1.0,
        pose: 0,
        stopped_to_walk: [0.2, 0.2],
        walk_to_stopped: [0.1, 0.1],
        walk_to_run: [3.0, 3.0],
        run_to_walk: [2.5, 2.5],
    }
}

#[test]
fn mover_stuck_and_move_state() {
    let mut m = MoverState::default();
    let mut h = Fake::new();
    h.move_speed = 1.0;
    let p = params();
    for _ in 0..30 {
        m.on_update(&p, 1.0 / 60.0, &mut h);
    }
    assert!(m.stuck_counter > 0.4);
    assert_eq!(m.move_state, MOVE_WALKING - 1);
    h.vel = v3(0.0, 0.0, -3.5);
    h.last_pos = v3(0.0, 1.0, 0.07);
    h.pos = v3(0.0, 1.0, 0.0);
    m.on_update(&p, 1.0 / 60.0, &mut h);
    m.on_update(&p, 1.0 / 60.0, &mut h);
    assert_eq!(m.move_state, MOVE_RUNNING);
    assert!(h.log.iter().any(|l| l == "anim 2 0"));
    assert!(h.log.iter().any(|l| l == "anim 3 0"));
}

#[test]
fn mover_turn_ends_close_to_goal() {
    let mut m = MoverState::default();
    let mut h = Fake::new();
    m.turn_to_angle(0.0005);
    m.on_update(&params(), 1.0 / 60.0, &mut h);
    assert!(!m.turning);
    m.turn_to_angle(1.0);
    m.on_update(&params(), 1.0 / 60.0, &mut h);
    assert!(h.yaw > 0.0 && m.turn_speed > 0.0);
}

#[test]
fn update_moving_reaches_node_and_ends_path() {
    let mut f = Fix::new();
    f.pf_moving = true;
    f.pf_goal = [3.0, 0.0, 0.0];
    let c = f.ctx();
    let mut h = Fake::new();
    h.path = vec![v3(1.0, 0.0, 0.0), v3(2.0, 0.0, 0.0)];
    h.bv_hit = true;
    {
        let mut e = Env::new(&c, &mut h);
        update_moving(&mut e);
    }
    assert_eq!(h.path.len(), 1);
    assert!(g(c.pf_moving));
    h.path.clear();
    {
        let mut e = Env::new(&c, &mut h);
        update_moving(&mut e);
    }
    assert!(!f.pf_moving);
    assert_eq!(f.queue.msgs.last().map(|m| m.ty), Some(MSG_END_OF_PATH));
    assert_eq!(f.pf_win.len, 0);
}

#[test]
fn update_moving_stuck_counter_clears_path() {
    let mut f = Fix::new();
    f.pf_moving = true;
    f.mover.stuck_counter = 2.0;
    let c = f.ctx();
    let mut h = Fake::new();
    h.path = vec![v3(1.0, 0.0, 0.0)];
    {
        let mut e = Env::new(&c, &mut h);
        update_moving(&mut e);
    }
    assert!(h.path.is_empty());
    let m = f.queue.msgs.last().copied().unwrap_or_default();
    assert_eq!((m.ty, m.custom_i), (MSG_END_OF_PATH, 1));
    assert_eq!(f.mover.stuck_counter, 0.0);
}

#[test]
fn node_pick_closest_and_random() {
    c_srand(5);
    let mut h = Fake::new();
    h.nodes = vec![
        v3(5.0, 0.0, 0.0),
        v3(2.0, 0.0, 0.0),
        v3(30.0, 0.0, 0.0),
        v3(3.0, 0.0, 0.0),
    ];
    let q = NodeQuery::new(0.0, 10.0, true, false, false, 0);
    let (i, p) = node_at_pos(&mut h, v3(0.0, 0.0, 0.0), &q).unwrap_or((-1, v3(0.0, 0.0, 0.0)));
    assert_eq!((i, p.x), (1, 2.0));
    let q = NodeQuery::new(2.5, 10.0, false, false, false, 0);
    let (i, _) = node_at_pos(&mut h, v3(0.0, 0.0, 0.0), &q).unwrap_or((-1, v3(0.0, 0.0, 0.0)));
    assert!(i == 0 || i == 3);
    let q = NodeQuery::new(100.0, 200.0, true, false, false, 0);
    assert!(node_at_pos(&mut h, v3(0.0, 0.0, 0.0), &q).is_none());
}

fn run_grunt(f: &mut Fix, h: &mut Fake, state: i32, ev: i32, msg: Option<&Msg>) -> bool {
    let c = f.ctx();
    let mut e = Env::new(&c, h);
    GruntHandler { e: &mut e }.event(state, ev, msg)
}

#[test]
fn grunt_idle_in_range_goes_to_wait_or_patrol() {
    let mut f = Fix::new();
    let mut h = Fake::new();
    f.cur = 0;
    let m = Msg {
        ty: MSG_PLAYER_IN_RANGE,
        ..Msg::default()
    };
    assert!(run_grunt(&mut f, &mut h, 0, EV_MESSAGE, Some(&m)));
    assert_eq!(f.next, 2);
    h.patrols = 2;
    assert!(run_grunt(&mut f, &mut h, 0, EV_MESSAGE, Some(&m)));
    assert_eq!(f.next, 3);
}

#[test]
fn grunt_global_take_hit_and_unknown_state() {
    let mut f = Fix::new();
    let mut h = Fake::new();
    let m = Msg {
        ty: MSG_TAKE_HIT,
        custom_f: 6.0,
        ..Msg::default()
    };
    f.cur = 2;
    assert!(run_grunt(&mut f, &mut h, -1, EV_MESSAGE, Some(&m)));
    assert_eq!((f.next, f.damage_count), (8, 0.0));
    assert!(!run_grunt(&mut f, &mut h, 7, EV_ENTER, None));
    assert!(
        h.log
            .iter()
            .any(|l| l == &format!("op{OP_ENEMY_STATE_FATAL}"))
    );
}

#[test]
fn grunt_helpers() {
    assert_eq!(grunt::damage_mul(3), 1.0);
    assert_eq!(grunt::damage_mul(8), 0.5);
    assert_eq!(grunt::damage_mul(9), 0.2);
    let mut cur = 2;
    grunt::inc_patrol(&raw mut cur, 3, true);
    assert_eq!(cur, 0);
    cur = 2;
    grunt::inc_patrol(&raw mut cur, 3, false);
    assert_eq!(cur, 2);
}

#[test]
fn senses_in_range_edges_and_regen() {
    let mut f = Fix::new();
    let mut h = Fake::new();
    f.cur = 0;
    let c = f.ctx();
    h.player_pos = v3(5.0, 0.0, 0.0);
    {
        let mut e = Env::new(&c, &mut h);
        e.update_player_in_range();
    }
    assert!(f.in_range);
    assert_eq!(f.queue.msgs.len(), 1);
    f.max_regen_health = 120.0;
    f.regen_health_speed = 60.0;
    let c = f.ctx();
    let mut e = Env::new(&c, &mut h);
    e.update_regen_health(0.5);
    assert_eq!(f.health, 120.0);
}
