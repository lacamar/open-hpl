//! What the enemy logic asks of C++. One dispatcher with an op code and a small register file;
//! the typed wrappers below keep the call sites readable. Everything is a query or an engine
//! command that cannot re-enter the enemy; `Cmd`s (flushed in order after a handler returns)
//! cover the ones that can.

use crate::math::{Vector3f, v3};

pub const OP_STEP_SIZE: u32 = 1;
pub const OP_BODY_POS: u32 = 2;
pub const OP_BODY_LAST_POS: u32 = 3;
pub const OP_BODY_FEET: u32 = 4;
pub const OP_BODY_SIZE: u32 = 5;
pub const OP_BODY_FORWARD: u32 = 6;
pub const OP_BODY_YAW: u32 = 7;
pub const OP_BODY_MOVE_SPEED_FWD: u32 = 8;
pub const OP_BODY_MOVE_DELAY: u32 = 9;
pub const OP_BODY_VELOCITY: u32 = 10;
pub const OP_BODY_MOVE_FWD: u32 = 11;
pub const OP_BODY_ADD_YAW: u32 = 12;
pub const OP_BODY_SET_SPEEDS: u32 = 13;
pub const OP_BODY_BV_RADIUS: u32 = 14;
pub const OP_PLAY_ANIM: u32 = 15;
pub const OP_ANIM_EXISTS: u32 = 16;
pub const OP_ANIM_SET_SPEED: u32 = 17;
pub const OP_FADE_OUT_ANIM: u32 = 18;
pub const OP_PLAYER_DEAD: u32 = 19;
pub const OP_PLAYER_FEET: u32 = 20;
pub const OP_PLAYER_POS: u32 = 21;
pub const OP_PLAYER_SIZE: u32 = 22;
pub const OP_PLAYER_BV_RADIUS: u32 = 23;
pub const OP_PLAYER_LANTERN: u32 = 24;
pub const OP_PLAYER_CROUCH: u32 = 25;
pub const OP_PLAYER_LIGHT_LEVEL: u32 = 26;
pub const OP_PLAYER_AVG_SPEED: u32 = 27;
pub const OP_PLAYER_VELOCITY: u32 = 28;
pub const OP_PLAYER_TERROR: u32 = 29;
pub const OP_PLAYER_SET_TERROR: u32 = 30;
pub const OP_PLAYER_ADD_TERROR_ENEMY: u32 = 31;
pub const OP_PLAYER_REMOVE_TERROR_ENEMY: u32 = 32;
pub const OP_TRIGGERS_DISABLED: u32 = 33;
pub const OP_MUSIC: u32 = 34;
pub const OP_LOS_RAY: u32 = 35;
pub const OP_ENTITY_ATTACH_POS: u32 = 36;
pub const OP_DOOR_BROKEN: u32 = 37;
pub const OP_DEBUG_MSG: u32 = 38;
pub const OP_PLAY_SOUND: u32 = 39;
pub const OP_CHANGE_SOUND_STATE: u32 = 40;
pub const OP_ACTIVATE_SOUND_ALLOWED: u32 = 41;
pub const OP_SET_ACTIVATE_SOUND_MADE: u32 = 42;
pub const OP_PF_MOVE_TO: u32 = 43;
pub const OP_PF_STOP: u32 = 44;
pub const OP_PF_PATH_LEN: u32 = 45;
pub const OP_PF_PATH_BACK: u32 = 46;
pub const OP_PF_PATH_POP: u32 = 47;
pub const OP_PF_PATH_CLEAR: u32 = 48;
pub const OP_NC_PRESENT: u32 = 49;
pub const OP_NC_COUNT: u32 = 50;
pub const OP_NC_ITER_BEGIN: u32 = 51;
pub const OP_NC_ITER_NODE: u32 = 52;
pub const OP_NC_FREE_PATH: u32 = 53;
pub const OP_NC_COMMIT_LAST: u32 = 54;
pub const OP_BV_CONTAINS: u32 = 55;
pub const OP_DOOR_COUNT: u32 = 56;
pub const OP_DOOR_AT: u32 = 57;
pub const OP_ENEMY_STATE_FATAL: u32 = 60;
pub const OP_BODY_SET_ACTIVE: u32 = 62;
pub const OP_PATROL_NODE_POS: u32 = 64;
pub const OP_PATROL_WAIT: u32 = 65;
pub const OP_PATROL_COUNT: u32 = 67;
pub const OP_PLAYER_CAM_POS: u32 = 63;
pub const OP_SET_ACTIVE_FALSE: u32 = 100;
pub const CMD_RUN_CALLBACK: u32 = 101;
pub const OP_BROADCAST_HELP: u32 = 102;
pub const OP_ATTACK: u32 = 103;

/// Register file shared by every op.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Io {
    pub i: [i32; 8],
    pub f: [f32; 4],
    pub v: [[f32; 3]; 3],
}

pub trait Host {
    /// Executes `op`; the return value is the op's boolean or integer result.
    fn call(&mut self, op: u32, io: &mut Io) -> i32;
}

/// One animation request as `iLuxEnemy::PlayAnim` takes it; `kind`/`n` name the animation.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct AnimReq {
    pub kind: i32,
    pub n: i32,
    pub looped: bool,
    pub fade: f32,
    pub depends_on_speed: bool,
    pub speed_mul: f32,
    pub sync: bool,
    pub override_move: bool,
    pub use_move_when_over: bool,
}

/// A command C++ replays after the handler returns.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Cmd {
    pub op: u32,
    pub io: Io,
}

pub(crate) fn v_of(a: [f32; 3]) -> Vector3f {
    v3(a[0], a[1], a[2])
}

pub(crate) fn a_of(v: Vector3f) -> [f32; 3] {
    [v.x, v.y, v.z]
}

macro_rules! q3 {
    ($($name:ident = $op:expr),* $(,)?) => {$(
        pub fn $name(&mut self) -> Vector3f {
            let mut io = Io::default();
            self.call($op, &mut io);
            v_of(io.v[0])
        }
    )*};
}
macro_rules! qf {
    ($($name:ident = $op:expr),* $(,)?) => {$(
        pub fn $name(&mut self) -> f32 {
            let mut io = Io::default();
            self.call($op, &mut io);
            io.f[0]
        }
    )*};
}
macro_rules! qb {
    ($($name:ident = $op:expr),* $(,)?) => {$(
        pub fn $name(&mut self) -> bool {
            self.call($op, &mut Io::default()) != 0
        }
    )*};
}
macro_rules! cmd0 {
    ($($name:ident = $op:expr),* $(,)?) => {$(
        pub fn $name(&mut self) {
            self.call($op, &mut Io::default());
        }
    )*};
}

impl dyn Host + '_ {
    q3! {
        body_pos = OP_BODY_POS, body_last_pos = OP_BODY_LAST_POS, body_feet = OP_BODY_FEET,
        body_size = OP_BODY_SIZE, body_forward = OP_BODY_FORWARD, player_feet = OP_PLAYER_FEET,
        player_pos = OP_PLAYER_POS, player_size = OP_PLAYER_SIZE, player_cam_pos = OP_PLAYER_CAM_POS,
        pf_path_back = OP_PF_PATH_BACK,
    }
    qf! {
        step_size = OP_STEP_SIZE, body_yaw = OP_BODY_YAW, body_move_speed_fwd = OP_BODY_MOVE_SPEED_FWD,
        body_move_delay = OP_BODY_MOVE_DELAY, body_bv_radius = OP_BODY_BV_RADIUS,
        player_bv_radius = OP_PLAYER_BV_RADIUS, player_light_level = OP_PLAYER_LIGHT_LEVEL,
        player_avg_speed = OP_PLAYER_AVG_SPEED, player_terror = OP_PLAYER_TERROR,
    }
    qb! {
        anim_exists = OP_ANIM_EXISTS, player_dead = OP_PLAYER_DEAD, player_lantern = OP_PLAYER_LANTERN,
        player_crouch = OP_PLAYER_CROUCH, triggers_disabled = OP_TRIGGERS_DISABLED,
        activate_sound_allowed = OP_ACTIVATE_SOUND_ALLOWED, nc_present = OP_NC_PRESENT,
    }
    cmd0! {
        body_move_fwd = OP_BODY_MOVE_FWD, set_activate_sound_made = OP_SET_ACTIVATE_SOUND_MADE,
        pf_stop = OP_PF_STOP, pf_path_pop = OP_PF_PATH_POP, pf_path_clear = OP_PF_PATH_CLEAR,
        player_add_terror_enemy = OP_PLAYER_ADD_TERROR_ENEMY,
        player_remove_terror_enemy = OP_PLAYER_REMOVE_TERROR_ENEMY,
    }

    pub fn body_velocity(&mut self, dt: f32) -> Vector3f {
        let mut io = Io::default();
        io.f[0] = dt;
        self.call(OP_BODY_VELOCITY, &mut io);
        v_of(io.v[0])
    }

    pub fn player_velocity(&mut self, dt: f32) -> Vector3f {
        let mut io = Io::default();
        io.f[0] = dt;
        self.call(OP_PLAYER_VELOCITY, &mut io);
        v_of(io.v[0])
    }

    pub fn body_add_yaw(&mut self, a: f32) {
        let mut io = Io::default();
        io.f[0] = a;
        self.call(OP_BODY_ADD_YAW, &mut io);
    }

    pub fn body_set_speeds(&mut self, max_pos: f32, max_neg: f32, acc: f32, deacc: f32) {
        let mut io = Io {
            f: [max_pos, max_neg, acc, deacc],
            ..Io::default()
        };
        self.call(OP_BODY_SET_SPEEDS, &mut io);
    }

    /// True when `PlayAnim` reached the point where it sets the mover's override flag.
    pub fn play_anim(&mut self, a: &AnimReq) -> bool {
        let mut io = Io {
            i: [
                a.kind,
                a.n,
                i32::from(a.looped),
                i32::from(a.depends_on_speed),
                i32::from(a.sync),
                i32::from(a.override_move),
                i32::from(a.use_move_when_over),
                0,
            ],
            f: [a.fade, a.speed_mul, 0.0, 0.0],
            ..Io::default()
        };
        self.call(OP_PLAY_ANIM, &mut io) != 0
    }

    pub fn anim_set_speed(&mut self, s: f32) {
        let mut io = Io::default();
        io.f[0] = s;
        self.call(OP_ANIM_SET_SPEED, &mut io);
    }

    /// True when the mover should go back to its move-state animations.
    pub fn fade_out_anim(&mut self, f: f32) -> bool {
        let mut io = Io::default();
        io.f[0] = f;
        self.call(OP_FADE_OUT_ANIM, &mut io) != 0
    }

    pub fn body_set_active(&mut self, on: bool) {
        let mut io = Io::default();
        io.i[0] = i32::from(on);
        self.call(OP_BODY_SET_ACTIVE, &mut io);
    }

    pub fn patrol_count(&mut self) -> i32 {
        self.call(OP_PATROL_COUNT, &mut Io::default())
    }

    pub fn patrol_node_pos(&mut self, idx: i32) -> Option<Vector3f> {
        let mut io = Io::default();
        io.i[0] = idx;
        (self.call(OP_PATROL_NODE_POS, &mut io) != 0).then(|| v_of(io.v[0]))
    }

    pub fn patrol_wait_time(&mut self, idx: i32) -> Option<f32> {
        let mut io = Io::default();
        io.i[0] = idx;
        (self.call(OP_PATROL_WAIT, &mut io) != 0).then_some(io.f[0])
    }

    pub fn set_active_false(&mut self) {
        self.call(OP_SET_ACTIVE_FALSE, &mut Io::default());
    }

    pub fn broadcast_help(&mut self, pos: Vector3f, feet: Vector3f, radius: f32) {
        let mut io = Io {
            v: [a_of(pos), a_of(feet), [0.0; 3]],
            f: [radius, 0.0, 0.0, 0.0],
            ..Io::default()
        };
        self.call(OP_BROADCAST_HELP, &mut io);
    }

    pub fn attack(&mut self, which: i32) {
        let mut io = Io::default();
        io.i[0] = which;
        self.call(OP_ATTACK, &mut io);
    }

    pub fn player_set_terror(&mut self, t: f32) {
        let mut io = Io::default();
        io.f[0] = t;
        self.call(OP_PLAYER_SET_TERROR, &mut io);
    }

    pub fn music(&mut self, add: bool, kind: i32) {
        let mut io = Io::default();
        io.i[0] = i32::from(add);
        io.i[1] = kind;
        self.call(OP_MUSIC, &mut io);
    }

    pub fn los_ray(&mut self, a: Vector3f, b: Vector3f) -> bool {
        let mut io = Io::default();
        io.v[0] = a_of(a);
        io.v[1] = a_of(b);
        self.call(OP_LOS_RAY, &mut io) != 0
    }

    pub fn entity_attach_pos(&mut self, id: i32) -> Vector3f {
        let mut io = Io::default();
        io.i[0] = id;
        self.call(OP_ENTITY_ATTACH_POS, &mut io);
        v_of(io.v[0])
    }

    pub fn door_broken(&mut self, id: i32) -> bool {
        let mut io = Io::default();
        io.i[0] = id;
        self.call(OP_DOOR_BROKEN, &mut io) != 0
    }

    pub fn debug_msg(&mut self, kind: i32, f: f32) {
        let mut io = Io::default();
        io.i[0] = kind;
        io.f[0] = f;
        self.call(OP_DEBUG_MSG, &mut io);
    }

    pub fn play_sound(&mut self, kind: i32) {
        let mut io = Io::default();
        io.i[0] = kind;
        self.call(OP_PLAY_SOUND, &mut io);
    }

    pub fn change_sound_state(&mut self, s: i32) {
        let mut io = Io::default();
        io.i[0] = s;
        self.call(OP_CHANGE_SOUND_STATE, &mut io);
    }

    pub fn pf_move_to(&mut self, p: Vector3f) -> bool {
        let mut io = Io::default();
        io.v[0] = a_of(p);
        self.call(OP_PF_MOVE_TO, &mut io) != 0
    }

    pub fn pf_path_len(&mut self) -> i32 {
        self.call(OP_PF_PATH_LEN, &mut Io::default())
    }

    pub fn nc_count(&mut self) -> i32 {
        self.call(OP_NC_COUNT, &mut Io::default())
    }

    /// `skip`: 0 no skip node, 1 the last search node, 2 the node the caller named.
    pub fn nc_iter_begin(&mut self, pos: Vector3f, max_dist: f32, skip: i32) -> i32 {
        let mut io = Io::default();
        io.v[0] = a_of(pos);
        io.f[0] = max_dist;
        io.i[0] = skip;
        self.call(OP_NC_ITER_BEGIN, &mut io)
    }

    /// `None` when the node is skipped (the skip node, or already used as a goal).
    pub fn nc_iter_node(&mut self, idx: i32, skip_used: bool) -> Option<Vector3f> {
        let mut io = Io::default();
        io.i[0] = idx;
        io.i[1] = i32::from(skip_used);
        (self.call(OP_NC_ITER_NODE, &mut io) != 0).then(|| v_of(io.v[0]))
    }

    pub fn nc_free_path(&mut self, a: Vector3f, b: Vector3f, rays: i32, flags: i32) -> bool {
        let mut io = Io::default();
        io.v[0] = a_of(a);
        io.v[1] = a_of(b);
        io.i[0] = rays;
        io.i[1] = flags;
        self.call(OP_NC_FREE_PATH, &mut io) != 0
    }

    pub fn nc_commit_last(&mut self, idx: i32) {
        let mut io = Io::default();
        io.i[0] = idx;
        self.call(OP_NC_COMMIT_LAST, &mut io);
    }

    pub fn bv_contains(&mut self, point: Vector3f, size: Vector3f, pos: Vector3f) -> bool {
        let mut io = Io {
            v: [a_of(point), a_of(size), a_of(pos)],
            ..Io::default()
        };
        self.call(OP_BV_CONTAINS, &mut io) != 0
    }

    pub fn door_count(&mut self) -> i32 {
        self.call(OP_DOOR_COUNT, &mut Io::default())
    }

    /// `(id, body local position, main body world position)` of overlapping door body `i`.
    pub fn door_at(&mut self, i: i32) -> (i32, Vector3f, Vector3f) {
        let mut io = Io::default();
        io.i[0] = i;
        let id = self.call(OP_DOOR_AT, &mut io);
        (id, v_of(io.v[0]), v_of(io.v[1]))
    }

    pub fn fatal_state(&mut self, s: i32) {
        let mut io = Io::default();
        io.i[0] = s;
        self.call(OP_ENEMY_STATE_FATAL, &mut io);
    }

    pub fn flush(&mut self, cmds: &mut Vec<Cmd>) {
        for c in cmds.drain(..) {
            let mut io = c.io;
            self.call(c.op, &mut io);
        }
    }
}
