//! S5d: `cSomaLuxPlayer` (`SomaLuxPlayer.cpp`): the player handler, its player and move state
//! machines, camera fades and scripted camera/automove, health and damage ids, the `cLuxPlayer`
//! and `cLuxPlayerState`/`cLuxMoveState` script natives. The camera, character body, scriptables,
//! map and entities stay C++ behind one host table (`OhplS5dOp`); the script-visible state fields
//! live in `#[repr(C)]` blocks the C++ shells embed.

use super::player::{
    self, Automove, CameraRotate, DamageId, Fade, MessageKind, MoveSpeeds, MoveVec, StateChange,
};
use super::s5_input_ffi::{Host, Io};
use crate::math::{Vector3f, to_deg, to_rad};
use crate::somascript::s2_as::{Engine, Frame, Soma, ohpl_as2_str_ctor, str_bytes};
use std::cell::{Cell, RefCell};
use std::collections::BTreeMap;
use std::ffi::c_void;
use std::sync::OnceLock;

pub mod op {
    pub const DUMP_ENABLED: u32 = 1;
    pub const DUMP_LOG: u32 = 2;
    pub const CFG_FLOAT: u32 = 3;
    pub const NEW_STATE: u32 = 4;
    pub const LOAD_STATE: u32 = 5;
    pub const DELETE: u32 = 6;
    pub const CALL: u32 = 7;
    pub const BASE: u32 = 8;
    pub const MAP_ACTIVE: u32 = 9;
    pub const TRANSFER: u32 = 10;
    pub const CAM_GET: u32 = 11;
    pub const CAM_SET: u32 = 12;
    pub const BODY_GET: u32 = 13;
    pub const BODY_SET: u32 = 14;
    pub const LIQUID_SURFACE: u32 = 15;
    pub const LIQUID_COUNT: u32 = 16;
    pub const LIQUID_ID: u32 = 17;
    pub const LIQUID_SET: u32 = 18;
    pub const LIQUID_GET: u32 = 19;
    pub const ADD_COLLIDE: u32 = 20;
    pub const REMOVE_COLLIDE: u32 = 21;
    pub const HAS_COLLIDE: u32 = 22;
    pub const CHECK_COLLIDE: u32 = 23;
    pub const DEAD_BROADCAST: u32 = 24;
    pub const ERROR_LOG: u32 = 25;
}

pub mod base {
    pub const UPDATE: i32 = 0;
    pub const POST_UPDATE: i32 = 1;
    pub const VARIABLE_UPDATE: i32 = 2;
    pub const GUI: i32 = 3;
    pub const MESSAGE: i32 = 4;
    pub const MAP_MESSAGE: i32 = 5;
    pub const ACTION: i32 = 6;
    pub const ANALOG: i32 = 7;
}

/// `kOhplS5dArg_*`.
mod arg_kind {
    pub const DWORD: i32 = 0;
    pub const FLOAT: i32 = 1;
    pub const BYTE: i32 = 2;
    pub const ADDRESS: i32 = 3;
    pub const OBJECT: i32 = 4;
    pub const STRING: i32 = 5;
}

mod cam {
    pub const POS: i32 = 0;
    pub const PITCH: i32 = 1;
    pub const YAW: i32 = 2;
    pub const FOV: i32 = 3;

    pub const SET_ROLL: i32 = 0;
    pub const SET_FOV: i32 = 1;
    pub const SET_YAW: i32 = 2;
    pub const SET_PITCH: i32 = 3;
    pub const ADD_PITCH: i32 = 4;
    pub const SET_NEAR: i32 = 5;
    pub const SET_FAR: i32 = 6;
    pub const SET_PITCH_LIMITS: i32 = 7;
    pub const RESET_EULER: i32 = 8;
}

mod body {
    pub const VELOCITY: i32 = 0;
    pub const FEET: i32 = 1;
    pub const FORCE_VELOCITY: i32 = 2;
    pub const FORWARD: i32 = 3;
    pub const RIGHT: i32 = 4;
    pub const YAW: i32 = 5;
    pub const CAMERA: i32 = 6;

    pub const SET_CAMERA_POS_ADD: i32 = 0;
    pub const ADD_YAW: i32 = 1;
    pub const MOVE_FORWARD: i32 = 2;
    pub const MOVE_RIGHT: i32 = 3;
    pub const SET_YAW: i32 = 4;
    pub const SET_FEET: i32 = 5;
    pub const ZERO_FORCE_VELOCITY: i32 = 6;
    pub const UPDATE: i32 = 7;
    pub const INIT: i32 = 8;
    pub const SPEEDS: i32 = 9;
}

#[repr(C)]
pub struct StateBlock {
    pub id: i32,
    pub player: *mut c_void,
}

#[repr(C)]
pub struct MoveBlock {
    pub id: i32,
    pub player: *mut c_void,
    pub speeds: MoveSpeeds,
}

#[repr(C)]
struct CArg {
    kind: i32,
    i: i32,
    f: f32,
    p: *mut c_void,
}

const _: () = {
    use std::mem::{offset_of, size_of};
    assert!(size_of::<StateBlock>() == 16 && offset_of!(StateBlock, player) == 8);
    assert!(size_of::<MoveBlock>() == 64 && offset_of!(MoveBlock, speeds) == 16);
    assert!(size_of::<MoveSpeeds>() == 48 && size_of::<Fade>() == 20);
    assert!(size_of::<CArg>() == 24);
};

/// `cSomaID`: the script `tID`.
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct SomaId {
    a: u8,
    b: i32,
    c: i32,
}

enum Arg<'a> {
    Dword(i32),
    Float(f32),
    Byte(bool),
    Addr(*mut c_void),
    Obj(*const c_void),
    Str(&'a [u8]),
}

impl Arg<'_> {
    fn raw(&self) -> CArg {
        let (kind, i, f, p) = match *self {
            Self::Dword(v) => (arg_kind::DWORD, v, 0.0, std::ptr::null_mut()),
            Self::Float(v) => (arg_kind::FLOAT, 0, v, std::ptr::null_mut()),
            Self::Byte(v) => (arg_kind::BYTE, i32::from(v), 0.0, std::ptr::null_mut()),
            Self::Addr(p) => (arg_kind::ADDRESS, 0, 0.0, p),
            Self::Obj(p) => (arg_kind::OBJECT, 0, 0.0, p.cast_mut()),
            Self::Str(s) => (
                arg_kind::STRING,
                i32::try_from(s.len()).unwrap_or(0),
                0.0,
                s.as_ptr().cast_mut().cast(),
            ),
        };
        CArg { kind, i, f, p }
    }
}

// host

pub trait Ops {
    fn go(&self, op: u32, io: Io) -> (i32, Io);
}

struct CHost(&'static Host);

impl Ops for CHost {
    fn go(&self, op: u32, mut io: Io) -> (i32, Io) {
        // SAFETY: the table was installed by the host and `io` outlives the call.
        let r = unsafe { (self.0.call)(self.0.ctx, op, &mut io) };
        (r, io)
    }
}

static HOST: OnceLock<&'static Host> = OnceLock::new();

#[cfg(test)]
thread_local! {
    pub(super) static TEST_OPS: RefCell<Option<std::rc::Rc<dyn Ops>>> = const { RefCell::new(None) };
}

fn host(op: u32, io: Io) -> (i32, Io) {
    #[cfg(test)]
    if let Some(h) = TEST_OPS.with(|t| t.borrow().clone()) {
        return h.go(op, io);
    }
    match HOST.get() {
        Some(h) => CHost(h).go(op, io),
        None => (0, io),
    }
}

fn io_s(s: &[u8]) -> Io {
    let mut io = Io::new();
    io.s = s.as_ptr();
    io.n = s.len();
    io
}

fn pv(p: usize) -> *mut c_void {
    p as *mut c_void
}

fn dump_on() -> bool {
    host(op::DUMP_ENABLED, Io::new()).0 != 0
}

fn cfg_float(key: &[u8], def: f32) -> f32 {
    let s = [&b"Player"[..], key].join(&0u8);
    let mut io = io_s(&s);
    io.f[0] = def;
    host(op::CFG_FLOAT, io).1.f[0]
}

/// `Call` (mode 0, returns whether it ran) or `CallBool` (mode 1). A null target is the map script.
fn script(target: usize, decl: &[u8], args: &[Arg], mode: i32, default: bool) -> bool {
    let raw: Vec<CArg> = args.iter().map(Arg::raw).collect();
    let mut io = io_s(decl);
    io.p = [pv(target), raw.as_ptr().cast_mut().cast()];
    io.i = [
        i32::try_from(raw.len()).unwrap_or(0),
        mode,
        i32::from(default),
        0,
    ];
    host(op::CALL, io).0 != 0
}

fn call(target: usize, decl: &[u8], args: &[Arg]) -> bool {
    script(target, decl, args, 0, false)
}

fn call_bool(target: usize, decl: &[u8], args: &[Arg], default: bool) -> bool {
    script(target, decl, args, 1, default)
}

fn base_op(target: usize, sub: i32, f: impl FnOnce(&mut Io)) {
    let mut io = Io::new();
    io.p[0] = pv(target);
    io.i[0] = sub;
    f(&mut io);
    host(op::BASE, io);
}

fn base_dt(target: usize, sub: i32, dt: f32) {
    base_op(target, sub, |io| io.f[0] = dt);
}

fn base_text(target: usize, sub: i32, decl: &[u8], map: usize) {
    let mut io = io_s(decl);
    io.p = [pv(target), pv(map)];
    io.i[0] = sub;
    host(op::BASE, io);
}

fn cam_get(c: usize, field: i32) -> Vector3f {
    let mut io = Io::new();
    io.p[0] = pv(c);
    io.i[0] = field;
    let o = host(op::CAM_GET, io).1;
    Vector3f {
        x: o.f[0],
        y: o.f[1],
        z: o.f[2],
    }
}

fn cam_set(c: usize, field: i32, a: f32, b: f32) {
    let mut io = Io::new();
    io.p[0] = pv(c);
    io.i[0] = field;
    io.f[0] = a;
    io.f[1] = b;
    host(op::CAM_SET, io);
}

fn body_get(b: usize, field: i32, dt: f32) -> (Vector3f, usize) {
    let mut io = Io::new();
    io.p[0] = pv(b);
    io.i[0] = field;
    io.f[0] = dt;
    let o = host(op::BODY_GET, io).1;
    (
        Vector3f {
            x: o.f[0],
            y: o.f[1],
            z: o.f[2],
        },
        o.p[1] as usize,
    )
}

fn body_set(b: usize, field: i32, v: [f32; 3]) {
    let mut io = Io::new();
    io.p[0] = pv(b);
    io.i[0] = field;
    io.f = [v[0], v[1], v[2], 0.0];
    host(op::BODY_SET, io);
}

// state

#[derive(Clone, Copy)]
struct Rec {
    shell: usize,
    block: usize,
    auto_speeds: bool,
}

struct Pl {
    shell: usize,
    camera: usize,
    body: usize,
    active: bool,
    default_fov: f32,
    states: BTreeMap<i32, Rec>,
    moves: BTreeMap<i32, Rec>,
    state: Option<i32>,
    move_state: Option<i32>,
    base_cam_add: Vector3f,
    cam_add: BTreeMap<i32, MoveVec>,
    cam_roll: BTreeMap<i32, Fade>,
    fov_keyed: BTreeMap<i32, Fade>,
    fov_mul: Fade,
    aspect_mul: Fade,
    fov: Fade,
    damage_ids: Vec<i32>,
    health: f32,
    can_die: bool,
    max_health: f32,
    rot_active: bool,
    rot: CameraRotate,
    auto_active: bool,
    auto_target: Vector3f,
    auto_mul: f32,
    vis_mul: BTreeMap<i32, f32>,
    vis_max: BTreeMap<i32, f32>,
    time_since_phys: f32,
    avg_speed: f32,
    smoothing: f32,
    avg_dir: Vector3f,
    tracking_avg: [f32; 2],
    cam_add_sum: Vector3f,
}

fn one_fade() -> Fade {
    Fade {
        value: 1.0,
        goal: 1.0,
        ..Fade::default()
    }
}

impl Pl {
    fn new(shell: usize) -> Self {
        Self {
            shell,
            camera: 0,
            body: 0,
            active: true,
            default_fov: 70.0,
            states: BTreeMap::new(),
            moves: BTreeMap::new(),
            state: None,
            move_state: None,
            base_cam_add: Vector3f::default(),
            cam_add: BTreeMap::new(),
            cam_roll: BTreeMap::new(),
            fov_keyed: BTreeMap::new(),
            fov_mul: one_fade(),
            aspect_mul: one_fade(),
            fov: Fade::default(),
            damage_ids: Vec::new(),
            health: 1.0,
            can_die: true,
            max_health: 1.0,
            rot_active: false,
            rot: CameraRotate::default(),
            auto_active: false,
            auto_target: Vector3f::default(),
            auto_mul: 0.0,
            vis_mul: BTreeMap::new(),
            vis_max: BTreeMap::new(),
            time_since_phys: 0.0,
            avg_speed: 0.0,
            smoothing: 0.0,
            avg_dir: Vector3f::default(),
            tracking_avg: [0.0; 2],
            cam_add_sum: Vector3f::default(),
        }
    }

    fn table(&self, mv: bool) -> &BTreeMap<i32, Rec> {
        if mv { &self.moves } else { &self.states }
    }

    fn current(&self, mv: bool) -> Option<i32> {
        if mv { self.move_state } else { self.state }
    }

    fn current_rec(&self, mv: bool) -> Option<Rec> {
        self.current(mv)
            .and_then(|id| self.table(mv).get(&id).copied())
    }

    fn fov_mul_total(&self) -> f32 {
        let v: Vec<f32> = self.fov_keyed.values().map(|f| f.value).collect();
        player::fov_mul(self.fov_mul.value, &v)
    }

    fn fov_keyed(&mut self, id: i32) -> &mut Fade {
        self.fov_keyed.entry(id).or_insert_with(one_fade)
    }
}

thread_local! {
    static PL: RefCell<Option<Pl>> = const { RefCell::new(None) };
    static TICK: Cell<u32> = const { Cell::new(0) };
}

fn with<R>(f: impl FnOnce(&mut Pl) -> R) -> Option<R> {
    PL.with(|p| p.borrow_mut().as_mut().map(f))
}

fn shell() -> usize {
    with(|p| p.shell).unwrap_or(0)
}

fn camera() -> usize {
    with(|p| p.camera).unwrap_or(0)
}

fn body() -> usize {
    with(|p| p.body).unwrap_or(0)
}

fn rec(mv: bool) -> Option<Rec> {
    with(|p| p.current_rec(mv)).flatten()
}

fn state_id(mv: bool) -> i32 {
    with(|p| p.current(mv)).flatten().unwrap_or(-1)
}

fn enter_state(shell: usize, prev: i32) {
    call(
        shell,
        b"void OnEnterState(int alPrevStateId)",
        &[Arg::Dword(prev)],
    );
}

fn leave_state(shell: usize, next: i32) {
    call(
        shell,
        b"void OnLeaveState(int alNextStateId)",
        &[Arg::Dword(next)],
    );
}

// lifecycle

pub fn new(shell_ptr: usize) {
    PL.with(|p| *p.borrow_mut() = Some(Pl::new(shell_ptr)));
}

pub fn free() {
    let Some(pl) = PL.with(|p| p.borrow_mut().take()) else {
        return;
    };
    for r in pl.states.values().chain(pl.moves.values()) {
        let mut io = Io::new();
        io.p[0] = pv(r.shell);
        host(op::DELETE, io);
    }
}

pub fn set_camera(cam_ptr: usize) {
    if with(|p| p.camera = cam_ptr).is_none() {
        return;
    }
    let default = cfg_float(b"FOV", 70.0);
    let min = cfg_float(b"CameraPitchLimit_Min", -70.0);
    let max = cfg_float(b"CameraPitchLimit_Max", 70.0);
    let near = cfg_float(b"NearClipPlane", 0.03);
    let far = cfg_float(b"FarClipPlane", 1000.0);
    cam_set(cam_ptr, cam::SET_NEAR, near, 0.0);
    cam_set(cam_ptr, cam::SET_FAR, far, 0.0);
    with(|p| {
        p.default_fov = default;
        p.fov.value = default;
        p.fov.goal = default;
    });
    cam_set(cam_ptr, cam::SET_PITCH_LIMITS, to_rad(min), to_rad(max));
}

pub fn set_body(b: usize) {
    let c = with(|p| {
        p.body = b;
        p.camera
    })
    .unwrap_or(0);
    if b != 0 {
        let mut io = Io::new();
        io.p = [pv(b), pv(c)];
        io.i[0] = body::INIT;
        host(op::BODY_SET, io);
    }
}

pub fn add_state(name: &[u8], id: i32, file: &[u8], class: &[u8], mv: bool) {
    if with(|p| p.table(mv).contains_key(&id)).unwrap_or(true) {
        return;
    }
    let mut io = io_s(name);
    io.i = [i32::from(mv), id, 0, 0];
    let o = host(op::NEW_STATE, io).1;
    let (sh, block) = (o.p[0] as usize, o.p[1] as usize);
    if mv {
        init_move_block(block);
    }
    let base: &[u8] = if mv {
        b"cLuxMoveState"
    } else {
        b"cLuxPlayerState"
    };
    let text = [file, class, base].join(&0u8);
    let mut io = io_s(&text);
    io.p[0] = pv(sh);
    io.i[0] = i32::from(mv);
    if host(op::LOAD_STATE, io).0 == 0 {
        return;
    }
    let first = with(|p| {
        let r = Rec {
            shell: sh,
            block,
            auto_speeds: true,
        };
        let (table, cur) = if mv {
            (&mut p.moves, &mut p.move_state)
        } else {
            (&mut p.states, &mut p.state)
        };
        table.insert(id, r);
        let first = cur.is_none();
        if first {
            *cur = Some(id);
        }
        first
    })
    .unwrap_or(false);
    if first {
        enter_state(sh, -1);
    }
}

fn init_move_block(block: usize) {
    let speeds = MoveSpeeds {
        forward_opposite_mul: 1.0,
        sideway_opposite_mul: 1.0,
        max_forward_mul: 1.0,
        max_backward_mul: 1.0,
        max_sideway_mul: 1.0,
        ..MoveSpeeds::default()
    };
    // SAFETY: the host returned the block of the move-state shell it just created.
    unsafe { (*(block as *mut MoveBlock)).speeds = speeds };
}

pub fn change_state(id: i32, mv: bool) {
    let Some((target, cur, prev)) = with(|p| {
        (
            p.table(mv).get(&id).copied(),
            p.current(mv),
            p.current_rec(mv),
        )
    }) else {
        return;
    };
    match player::state_change(target.is_some(), target.is_some() && cur == Some(id)) {
        StateChange::Missing => {
            let what = if mv { "move state" } else { "state" };
            let m = format!("Could not change to {what} id {id}. It does not exist!\n");
            host(op::ERROR_LOG, io_s(m.as_bytes()));
            return;
        }
        StateChange::Same => return,
        StateChange::Switch => {}
    }
    let Some(next) = target else {
        return;
    };
    if let Some(r) = prev {
        leave_state(r.shell, id);
    }
    with(|p| {
        if mv {
            p.move_state = Some(id);
        } else {
            p.state = Some(id);
        }
    });
    enter_state(next.shell, cur.unwrap_or(-1));
}

pub fn place_at_start(feet: Vector3f, yaw: f32, crouching: bool) {
    let Some(sh) =
        with(|p| (player::place_plan(p.body != 0, false).is_some()).then_some(p.shell)).flatten()
    else {
        return;
    };
    let pos = feet;
    let handled = call(
        sh,
        b"void SetupStartPos(const cVector3f&in avPos, float afAngle, bool abCrouching)",
        &[
            Arg::Obj(std::ptr::from_ref(&pos).cast()),
            Arg::Float(yaw),
            Arg::Byte(crouching),
        ],
    );
    let b = body();
    if b == 0 {
        return;
    }
    if player::place_plan(true, handled).is_some_and(|p| p.set_feet_and_yaw) {
        body_set(b, body::SET_FEET, [feet.x, feet.y, feet.z]);
        body_set(b, body::SET_YAW, [yaw, 0.0, 0.0]);
    }
    body_set(b, body::ZERO_FORCE_VELOCITY, [0.0; 3]);
    body_set(b, body::UPDATE, [0.001, 0.0, 0.0]);
    let c = camera();
    if c != 0 {
        cam_set(c, cam::SET_YAW, yaw, 0.0);
        cam_set(c, cam::SET_PITCH, 0.0, 0.0);
    }
}

pub fn hit_ground(b: usize, vel: Vector3f) {
    let decl: &[u8] =
        b"void CharBody_HitGround(iCharacterBody@ apCharBody, const cVector3f &in avVel)";
    let args = [
        Arg::Addr(pv(b)),
        Arg::Addr(std::ptr::from_ref(&vel).cast_mut().cast()),
    ];
    let sh = shell();
    call(sh, decl, &args);
    if let Some(r) = rec(false) {
        call(r.shell, decl, &args);
    }
    if let Some(r) = rec(true) {
        call(r.shell, decl, &args);
    }
}

// per-frame

fn map_active() -> bool {
    host(op::MAP_ACTIVE, Io::new()).0 != 0
}

fn apply_speeds(r: Rec) {
    let b = body();
    if !r.auto_speeds || b == 0 {
        return;
    }
    // SAFETY: the block belongs to a live move-state shell.
    let s = unsafe { (*(r.block as *const MoveBlock)).speeds };
    let v = player::move_speed_settings(&s);
    let mut io = Io::new();
    io.p = [pv(b), v.as_ptr().cast_mut().cast()];
    io.i[0] = body::SPEEDS;
    host(op::BODY_SET, io);
}

pub fn update(dt: f32) {
    if !map_active() {
        return;
    }
    let Some(sh) = with(|p| p.shell) else {
        return;
    };
    base_dt(sh, base::UPDATE, dt);
    if let Some(r) = rec(false) {
        base_dt(r.shell, base::UPDATE, dt);
    }
    if let Some(r) = rec(true) {
        base_dt(r.shell, base::UPDATE, dt);
        apply_speeds(r);
    }
    with(|p| p.time_since_phys += dt);
    let b = body();
    if b != 0 {
        let vel = body_get(b, body::VELOCITY, dt).0;
        with(|p| player::update_average_move(&mut p.avg_speed, &mut p.avg_dir, vel));
    }
    update_camera(dt);
    let b = body();
    if b != 0 && dump_on() {
        dump_line(dt, b);
    }
}

fn dump_line(dt: f32, b: usize) {
    let (feet, vel) = (
        body_get(b, body::FEET, 0.0).0,
        body_get(b, body::FORCE_VELOCITY, 0.0).0,
    );
    let yaw = body_get(b, body::YAW, 0.0).0.x;
    let c = camera();
    let (pitch, fov) = if c != 0 {
        (cam_get(c, cam::PITCH).x, cam_get(c, cam::FOV).x)
    } else {
        (0.0, 0.0)
    };
    let (avg, health) = with(|p| (p.avg_speed, p.health)).unwrap_or_default();
    let tick = TICK.with(|t| {
        let v = t.get();
        t.set(v.wrapping_add(1));
        v
    });
    let bits = f32::to_bits;
    let line = format!(
        "pl {tick} {:08x} {:08x} {:08x} {:08x} {:08x} {:08x} {:08x} {:08x} {:08x} {:08x} {:08x} {} {} {:08x}",
        bits(dt),
        bits(feet.x),
        bits(feet.y),
        bits(feet.z),
        bits(vel.x),
        bits(vel.y),
        bits(vel.z),
        bits(yaw),
        bits(pitch),
        bits(fov),
        bits(avg),
        state_id(false),
        state_id(true),
        bits(health),
    );
    host(op::DUMP_LOG, io_s(line.as_bytes()));
}

pub fn post_update(dt: f32) {
    if !map_active() {
        return;
    }
    base_dt(shell(), base::POST_UPDATE, dt);
    if let Some(r) = rec(false) {
        base_dt(r.shell, base::POST_UPDATE, dt);
    }
    if let Some(r) = rec(true) {
        base_dt(r.shell, base::POST_UPDATE, dt);
    }
}

pub fn variable_update(dt: f32) {
    if map_active() {
        base_dt(shell(), base::VARIABLE_UPDATE, dt);
    }
}

pub fn gui(dt: f32) {
    base_dt(shell(), base::GUI, dt);
    if let Some(r) = rec(false) {
        base_dt(r.shell, base::GUI, dt);
    }
    if let Some(r) = rec(true) {
        base_dt(r.shell, base::GUI, dt);
    }
}

fn all_shells() -> Vec<usize> {
    with(|p| {
        p.states
            .values()
            .chain(p.moves.values())
            .map(|r| r.shell)
            .collect()
    })
    .unwrap_or_default()
}

pub fn message(decl: &[u8]) {
    let kind = player::message_kind(decl);
    with(|p| {
        let has = p.state.is_some();
        if player::drops_state_on_reset(kind, has, p.state.unwrap_or(0)) {
            p.state = None;
        }
    });
    base_text(shell(), base::MESSAGE, decl, 0);
    for s in all_shells() {
        base_text(s, base::MESSAGE, decl, 0);
    }
}

pub fn map_message(decl: &[u8], map: usize) {
    let kind = player::message_kind(decl);
    if kind == MessageKind::MapEnter {
        reset_for_map();
    }
    base_text(shell(), base::MAP_MESSAGE, decl, map);
    for s in all_shells() {
        base_text(s, base::MAP_MESSAGE, decl, map);
    }
    if kind == MessageKind::DestroyWorldEntities {
        with(|p| p.body = 0);
    }
}

fn reset_for_map() {
    for mv in [false, true] {
        let r = with(|p| {
            let r = p.table(mv).get(&0).copied();
            if mv {
                p.move_state = r.map(|_| 0);
            } else {
                p.state = r.map(|_| 0);
            }
            r
        })
        .flatten();
        if let Some(r) = r {
            enter_state(r.shell, -1);
        }
    }
    let is_transfer = host(op::TRANSFER, Io::new()).0 != 0;
    if player::clears_camera_adds(is_transfer) {
        with(|p| p.cam_add.clear());
    }
    let c = camera();
    if c != 0 {
        set_camera(c);
        cam_set(c, cam::RESET_EULER, 0.0, 0.0);
    }
}

pub fn action(action: i32, pressed: bool) {
    if !with(|p| p.active).unwrap_or(false) {
        return;
    }
    let decl: &[u8] = b"bool OnAction(int alAction, bool abPressed)";
    let args = [Arg::Dword(action), Arg::Byte(pressed)];
    for mv in [false, true] {
        if let Some(r) = rec(mv)
            && !call_bool(r.shell, decl, &args, true)
        {
            return;
        }
    }
    base_op(shell(), base::ACTION, |io| {
        io.i[1..3].copy_from_slice(&[action, i32::from(pressed)])
    });
}

pub fn analog(id: i32, amount: Vector3f) {
    if !with(|p| p.active).unwrap_or(false) {
        return;
    }
    let decl: &[u8] = b"bool OnAnalogInput(int alAnalogId, const cVector3f &in avAmount)";
    let args = [
        Arg::Dword(id),
        Arg::Addr(std::ptr::from_ref(&amount).cast_mut().cast()),
    ];
    for mv in [false, true] {
        if let Some(r) = rec(mv)
            && !call_bool(r.shell, decl, &args, true)
        {
            return;
        }
    }
    base_op(shell(), base::ANALOG, |io| {
        io.i[1] = id;
        io.f[..3].copy_from_slice(&[amount.x, amount.y, amount.z]);
    });
}

struct CamStep {
    add: Vector3f,
    roll: f32,
    fov: f32,
}

fn step_fades(p: &mut Pl, dt: f32) -> CamStep {
    let mut add = p.base_cam_add;
    for m in p.cam_add.values_mut() {
        player::step_move_vec(m, dt);
        add.x += m.value.x;
        add.y += m.value.y;
        add.z += m.value.z;
    }
    let mut roll = 0.0;
    for f in p.cam_roll.values_mut() {
        roll += player::fade_towards(f, dt);
    }
    player::fade_towards(&mut p.fov, dt);
    player::fade_towards(&mut p.fov_mul, dt);
    for f in p.fov_keyed.values_mut() {
        player::fade_towards(f, dt);
    }
    let fov = to_rad(p.fov.value) * p.fov_mul_total();
    player::fade_towards(&mut p.aspect_mul, dt);
    CamStep { add, roll, fov }
}

fn update_camera(dt: f32) {
    let c = camera();
    if c == 0 {
        return;
    }
    let Some(step) = with(|p| step_fades(p, dt)) else {
        return;
    };
    let b = body();
    if b != 0 {
        body_set(
            b,
            body::SET_CAMERA_POS_ADD,
            [step.add.x, step.add.y, step.add.z],
        );
    }
    cam_set(c, cam::SET_ROLL, step.roll, 0.0);
    cam_set(c, cam::SET_FOV, step.fov, 0.0);
    if b == 0 {
        return;
    }
    if let Some(rot) = with(|p| p.rot_active.then_some(p.rot)).flatten() {
        let pos = cam_get(c, cam::POS);
        let yaw = body_get(b, body::YAW, 0.0).0.x;
        let pitch = cam_get(c, cam::PITCH).x;
        let s = player::step_camera_rotate(&rot, pos, yaw, pitch, dt);
        with(|p| p.rot.speed = s.speed);
        if let Some((add_yaw, add_pitch)) = s.add {
            body_set(b, body::ADD_YAW, [add_yaw, 0.0, 0.0]);
            cam_set(c, cam::ADD_PITCH, add_pitch, 0.0);
        }
    }
    if let Some((target, mul)) =
        with(|p| p.auto_active.then_some((p.auto_target, p.auto_mul))).flatten()
    {
        let feet = body_get(b, body::FEET, 0.0).0;
        let fwd = body_get(b, body::FORWARD, 0.0).0;
        let right = body_get(b, body::RIGHT, 0.0).0;
        match player::step_automove(target, feet, fwd, right, mul) {
            Automove::Move { forward, right } => {
                body_set(b, body::MOVE_FORWARD, [forward, 0.0, 0.0]);
                body_set(b, body::MOVE_RIGHT, [right, 0.0, 0.0]);
            }
            Automove::Arrived => {
                with(|p| p.auto_active = false);
            }
        }
    }
    if body_get(b, body::CAMERA, 0.0).1 == c {
        let yaw = body_get(b, body::YAW, 0.0).0.x;
        cam_set(c, cam::SET_YAW, yaw, 0.0);
        let cam_yaw = cam_get(c, cam::YAW).x;
        if cam_yaw != yaw {
            body_set(b, body::SET_YAW, [cam_yaw, 0.0, 0.0]);
        }
    }
}

// health

pub fn set_health(x: f32) {
    let died = with(|p| {
        let (h, died) = player::set_health(p.health, p.max_health, p.can_die, x);
        p.health = h;
        died
    })
    .unwrap_or(false);
    if !died {
        return;
    }
    let decl: &[u8] = b"void OnPlayerDead(int, const tString&in)";
    let args = [Arg::Dword(-1), Arg::Str(b"")];
    call(shell(), decl, &args);
    script(0, decl, &args, 0, false);
    host(op::DEAD_BROADCAST, Io::new());
}

pub fn is_dead() -> bool {
    with(|p| player::is_dead(p.health, p.can_die)).unwrap_or(false)
}

pub fn give_damage(amount: f32, ty: i32, min_health: f32, source: SomaId, damage_id: i32) -> bool {
    if is_dead() {
        return false;
    }
    if damage_id != -1 {
        let reject = with(|p| match player::damage_id(&p.damage_ids, damage_id) {
            DamageId::Reject => true,
            DamageId::EvictPush => {
                p.damage_ids.remove(0);
                p.damage_ids.push(damage_id);
                false
            }
            DamageId::Push => {
                p.damage_ids.push(damage_id);
                false
            }
            DamageId::Untracked => false,
        })
        .unwrap_or(false);
        if reject {
            return false;
        }
    }
    call(
        shell(),
        b"void OnDamageTaken(float, int, tID)",
        &[
            Arg::Float(amount),
            Arg::Dword(ty),
            Arg::Obj(std::ptr::from_ref(&source).cast()),
        ],
    );
    let h = with(|p| player::health_after_damage(p.health, amount, min_health)).unwrap_or(0.0);
    set_health(h);
    true
}

// natives

fn text<'a>(f: &Frame, i: usize) -> &'a [u8] {
    // SAFETY: the declaration passes a `const tString &in` in slot `i`.
    unsafe { str_bytes(f.ptr_arg(i)) }
}

fn vec_arg(f: &Frame, i: usize) -> Vector3f {
    *f.rf::<Vector3f>(i)
}

fn empty_string() -> *mut c_void {
    thread_local! {
        static EMPTY: Cell<usize> = const { Cell::new(0) };
    }
    EMPTY.with(|e| {
        if e.get() == 0 {
            let mem: &'static mut [u64; 8] = Box::leak(Box::new([0u64; 8]));
            let p = std::ptr::from_mut(mem).cast::<c_void>();
            // SAFETY: 64 bytes hold a `std::string`; the leak keeps it alive.
            unsafe { ohpl_as2_str_ctor(p, std::ptr::null(), 0) };
            e.set(p as usize);
        }
        pv(e.get())
    })
}

fn handle(f: &Frame, i: usize) -> usize {
    let p = f.handle_arg(i) as usize;
    f.release_handles();
    p
}

fn player_entity_call(opc: u32, entity: usize, name: &[u8]) -> i32 {
    let mut io = io_s(name);
    io.p[1] = pv(entity);
    host(opc, io).0
}

/// Offsets of the shell members the natives hand to scripts.
#[derive(Clone, Copy)]
pub struct Offsets {
    pub state_block: i32,
    pub move_block: i32,
    pub state_name: i32,
    pub move_name: i32,
}

pub fn register(e: Engine, off: Offsets) {
    let s = Soma(e);
    let t = "cLuxPlayer";
    register_basic(&s, t, off);
    register_camera(&s, t);
    register_body(&s, t);
    register_health(&s, t);
    register_misc(&s, t);
    register_states(e, &s, off);
}

fn register_basic(s: &Soma, t: &str, off: Offsets) {
    s.method(t, "void SetActive(bool abX)", |f| {
        let x = f.arg::<bool>(0);
        with(|p| p.active = x);
    });
    s.method(t, "bool IsActive()", |f| {
        f.ret(with(|p| p.active).unwrap_or(false));
    });
    s.method(t, "cCamera@ GetCamera()", |f| {
        f.ret_ref(pv(camera()));
    });
    s.method(t, "iCharacterBody@ GetCharacterBody()", |f| {
        f.ret_ref(pv(body()));
    });
    s.method(t, "void SetCharacterBody(iCharacterBody@ apBody)", |f| {
        let b = f.handle_arg(0) as usize;
        set_body(b);
        f.release_handles();
    });
    s.method(t, "bool IsInLiquid()", |f| {
        f.ret(liquid_surface() > -100_000.0);
    });
    s.method(t, "float GetLiquidHeight()", |f| {
        let b = body();
        f.ret(if b != 0 {
            liquid_surface() - body_get(b, body::FEET, 0.0).0.y
        } else {
            0.0
        });
    });
    s.method(t, "void AddUsedLiquidArea(cLuxLiquidArea@ apArea)", |f| {
        liquid_set(handle(f, 0), true);
    });
    s.method(
        t,
        "void RemoveUsedLiquidArea(cLuxLiquidArea@ apArea)",
        |f| {
            liquid_set(handle(f, 0), false);
        },
    );
    s.method(t, "bool GetIsLiquidAreaUsed(cLuxLiquidArea@ apArea)", |f| {
        let a = handle(f, 0);
        let mut io = Io::new();
        io.p[1] = pv(a);
        f.ret(a != 0 && host(op::LIQUID_GET, io).0 != 0);
    });
    s.method(t, "int GetUsedLiquidAreaNum()", |f| {
        f.ret(host(op::LIQUID_COUNT, Io::new()).0);
    });
    s.method(t, "tID GetUsedLiquidAreaID(int alId)", |f| {
        let i = f.arg::<i32>(0);
        let mut io = Io::new();
        io.i[0] = i;
        let (found, o) = host(op::LIQUID_ID, io);
        let id = if found != 0 {
            SomaId {
                a: u8::try_from(o.i[0]).unwrap_or(0),
                b: o.i[1],
                c: o.i[2],
            }
        } else {
            SomaId::default()
        };
        f.ret(id);
    });
    s.method(t, "float GetSmoothingRotAmount()", |f| {
        f.ret(with(|p| p.smoothing).unwrap_or(0.0));
    });
    s.method(t, "void SetSmoothingRotAmount(float afX)", |f| {
        let x = f.arg::<f32>(0);
        with(|p| p.smoothing = x);
    });
    s.method(t, "float GetAverageMoveSpeed()", |f| {
        f.ret(with(|p| p.avg_speed).unwrap_or(0.0));
    });
    s.method(t, "const cVector3f& GetAverageMoveDirection()", |f| {
        if let Some(p) = with(|p| std::ptr::addr_of_mut!(p.avg_dir)) {
            f.ret_ref(p);
        }
    });
    s.method(t, "void ChangeState(int alId)", |f| {
        change_state(f.arg::<i32>(0), false);
    });
    s.method(t, "int GetCurrentStateId()", |f| {
        f.ret(state_id(false));
    });
    let name_off = off.state_name;
    s.method(t, "const tString& GetCurrentStateName()", move |f| {
        f.ret_ref(rec(false).map_or_else(empty_string, |r| state_name(r.shell, name_off)));
    });
    s.method(t, "void ChangeMoveState(int alId)", |f| {
        change_state(f.arg::<i32>(0), true);
    });
    s.method(t, "int GetCurrentMoveStateId()", |f| {
        f.ret(state_id(true));
    });
    let move_off = off.move_name;
    s.method(t, "const tString& GetCurrentMoveStateName()", move |f| {
        f.ret_ref(rec(true).map_or_else(empty_string, |r| state_name(r.shell, move_off)));
    });
    s.method(t, "cLuxMoveState@ GetCurrentMoveState()", |f| {
        f.ret_ref(pv(rec(true).map_or(0, |r| r.shell)));
    });
    s.method(
        t,
        "void AddState(const tString&in asName, int alId, const tString&in asScriptFile, const tString&in asScriptClass)",
        |f| add_state(text(f, 0), f.arg::<i32>(1), text(f, 2), text(f, 3), false),
    );
    s.method(
        t,
        "void AddMoveState(const tString&in asName, int alId, const tString&in asScriptFile, const tString&in asScriptClass)",
        |f| add_state(text(f, 0), f.arg::<i32>(1), text(f, 2), text(f, 3), true),
    );
    s.method(
        t,
        "void GiveDamage(float afAmount, int alStrength, int aType, float afMinHealth, const tString&in asSource)",
        |f| {
            let (a, min) = (f.arg::<f32>(0), f.arg::<f32>(3));
            let h = with(|p| player::health_after_script_damage(p.health, a, min)).unwrap_or(0.0);
            set_health(h);
        },
    );
    s.method(
        t,
        "bool GiveDamage(float afAmount, int alStrength, int aType, float afMinHealth, tID a_idSource, int alDamageID)",
        |f| {
            let (a, ty, min) = (f.arg::<f32>(0), f.arg::<i32>(2), f.arg::<f32>(3));
            // SAFETY: slot 4 is a `tID` passed by value.
            let src = unsafe { f.obj_arg(4).cast::<SomaId>().read() };
            f.ret(give_damage(a, ty, min, src, f.arg::<i32>(5)));
        },
    );
}

fn liquid_surface() -> f32 {
    host(op::LIQUID_SURFACE, Io::new()).1.f[0]
}

fn liquid_set(a: usize, on: bool) {
    if a != 0 {
        let mut io = Io::new();
        io.p[1] = pv(a);
        io.i[0] = i32::from(on);
        host(op::LIQUID_SET, io);
    }
}

fn register_camera(s: &Soma, t: &str) {
    s.method(
        t,
        "void SetBaseCameraPosAdd(const cVector3f&in avVec)",
        |f| {
            let v = vec_arg(f, 0);
            with(|p| p.base_cam_add = v);
        },
    );
    s.method(t, "const cVector3f& GetBaseCameraPosAdd()", |f| {
        if let Some(p) = with(|p| std::ptr::addr_of_mut!(p.base_cam_add)) {
            f.ret_ref(p);
        }
    });
    s.method(
        t,
        "void SetCameraPosAdd(int alType, const cVector3f&in avVector)",
        |f| {
            let (k, v) = (f.arg::<i32>(0), vec_arg(f, 1));
            with(|p| {
                let m = p.cam_add.entry(k).or_default();
                m.value = v;
                m.goal = v;
                m.moving = false;
            });
        },
    );
    s.method(t, "const cVector3f& GetCameraPosAdd(int alType)", |f| {
        let k = f.arg::<i32>(0);
        if let Some(p) = with(|p| std::ptr::addr_of_mut!(p.cam_add.entry(k).or_default().value)) {
            f.ret_ref(p);
        }
    });
    s.method(t, "const cVector3f& GetCameraPosAddGoal(int alType)", |f| {
        let k = f.arg::<i32>(0);
        if let Some(p) = with(|p| std::ptr::addr_of_mut!(p.cam_add.entry(k).or_default().goal)) {
            f.ret_ref(p);
        }
    });
    s.method(t, "const cVector3f& GetCameraPosAddSum()", |f| {
        let p = with(|p| {
            let mut sum = Vector3f::default();
            for m in p.cam_add.values() {
                sum.x += m.value.x;
                sum.y += m.value.y;
                sum.z += m.value.z;
            }
            p.cam_add_sum = sum;
            std::ptr::addr_of_mut!(p.cam_add_sum)
        });
        if let Some(p) = p {
            f.ret_ref(p);
        }
    });
    s.method(
        t,
        "void MoveCameraPosAdd(int alType, const cVector3f&in avGoal,float afAcc, float afSpeed, float afSlowdownDist)",
        |f| {
            let (k, g) = (f.arg::<i32>(0), vec_arg(f, 1));
            let (acc, speed, slow) = (f.arg::<f32>(2), f.arg::<f32>(3), f.arg::<f32>(4));
            with(|p| {
                let m = p.cam_add.entry(k).or_default();
                m.goal = g;
                m.acc = acc;
                m.max_speed = speed;
                m.slowdown_dist = slow;
                m.moving = true;
            });
        },
    );
    s.method(
        t,
        "void EnableCameraLock(float afLocalYawMin, float afLocalYawMax, float afLocalPitchMin, float afLocalPitchMax)",
        |f| {
            let (min, max) = (f.arg::<f32>(2), f.arg::<f32>(3));
            let c = camera();
            if c != 0 {
                cam_set(c, cam::SET_PITCH_LIMITS, to_rad(min), to_rad(max));
            }
        },
    );
    s.method(t, "void DisableCameraLock()", |_| {
        let c = camera();
        if c != 0 {
            cam_set(c, cam::SET_PITCH_LIMITS, to_rad(-70.0), to_rad(70.0));
        }
    });
    s.method(
        t,
        "void RotateCameraTowards(float afAcc, float afSpeedMul, float afMaxSpeed, const cVector3f&in avLookAtPos, bool abLocalCoord)",
        |f| {
            let (acc, mul, max) = (f.arg::<f32>(0), f.arg::<f32>(1), f.arg::<f32>(2));
            let (pos, local) = (vec_arg(f, 3), f.arg::<bool>(4));
            with(|p| {
                p.rot_active = true;
                p.rot.acc = acc;
                p.rot.speed_mul = mul;
                p.rot.max_speed = max;
                p.rot.target = pos;
                p.rot.local = local;
            });
        },
    );
    s.method(
        t,
        "void SetRotateCameraTarget(const cVector3f&in avLookAtPos, bool abLocalCoord)",
        |f| {
            let (pos, local) = (vec_arg(f, 0), f.arg::<bool>(1));
            with(|p| {
                p.rot.target = pos;
                p.rot.local = local;
            });
        },
    );
    s.method(t, "float GetRotateCameraTargetDistance()", |f| {
        let c = camera();
        let target = with(|p| p.rot.target).unwrap_or_default();
        f.ret(if c != 0 {
            crate::math::len3(crate::math::sub3(target, cam_get(c, cam::POS)))
        } else {
            0.0
        });
    });
    s.method(t, "bool IsCameraRotateActive()", |f| {
        f.ret(with(|p| p.rot_active).unwrap_or(false));
    });
    s.method(t, "void StopCameraRotate(float afDeacc)", |_| {
        with(|p| {
            p.rot_active = false;
            p.rot.speed = 0.0;
        });
    });
    register_fades(s, t);
}

fn register_fades(s: &Soma, t: &str) {
    s.method(
        t,
        "void FadeCameraFOVMulTo(float afX, float afSpeed)",
        |f| {
            let (x, sp) = (f.arg::<f32>(0), f.arg::<f32>(1));
            with(|p| {
                p.fov_mul.goal = x;
                p.fov_mul.speed = sp;
                p.fov_mul.speed_mul = 0.0;
            });
        },
    );
    s.method(
        t,
        "void FadeCameraFOVMulTo(int alId, float afX, float afSpeed)",
        |f| {
            let (id, x, sp) = (f.arg::<i32>(0), f.arg::<f32>(1), f.arg::<f32>(2));
            with(|p| {
                let m = p.fov_keyed(id);
                m.goal = x;
                m.speed = sp;
            });
        },
    );
    s.method(t, "void SetCameraFOVMul(int alId, float afFOVMul)", |f| {
        let (id, x) = (f.arg::<i32>(0), f.arg::<f32>(1));
        with(|p| {
            let m = p.fov_keyed(id);
            m.value = x;
            m.goal = x;
        });
    });
    s.method(t, "float GetCameraFOVMul(int alId)", |f| {
        let id = f.arg::<i32>(0);
        f.ret(with(|p| p.fov_keyed(id).value).unwrap_or(1.0));
    });
    s.method(t, "float GetCameraFOVMul()", |f| {
        f.ret(with(|p| p.fov_mul_total()).unwrap_or(1.0));
    });
    s.method(
        t,
        "void FadeCameraAspectMulTo(float afX, float afSpeed)",
        |f| {
            let (x, sp) = (f.arg::<f32>(0), f.arg::<f32>(1));
            with(|p| {
                p.aspect_mul.goal = x;
                p.aspect_mul.speed = sp;
                p.aspect_mul.speed_mul = 0.0;
            });
        },
    );
    s.method(
        t,
        "void FadeCameraRollTo(int alId, float afX, float afSpeedMul, float afMaxSpeed)",
        |f| {
            let (id, x) = (f.arg::<i32>(0), f.arg::<f32>(1));
            let (mul, max) = (f.arg::<f32>(2), f.arg::<f32>(3));
            with(|p| {
                let m = p.cam_roll.entry(id).or_default();
                m.goal = x;
                m.speed_mul = mul;
                m.max_speed = max;
            });
        },
    );
    s.method(t, "void SetCameraRoll(int alId, float afX)", |f| {
        let (id, x) = (f.arg::<i32>(0), f.arg::<f32>(1));
        with(|p| {
            let m = p.cam_roll.entry(id).or_default();
            m.value = x;
            m.goal = x;
        });
    });
    s.method(t, "float GetDefaultFOV()", |f| {
        f.ret(to_rad(with(|p| p.default_fov).unwrap_or(70.0)));
    });
    s.method(
        t,
        "void FadeCameraFOVTo(float afTargetFOV, float afSpeed)",
        |f| {
            let (x, sp) = (f.arg::<f32>(0), f.arg::<f32>(1));
            with(|p| {
                p.fov.goal = if x < 0.0 { p.default_fov } else { to_deg(x) };
                p.fov.speed = to_deg(sp);
                p.fov.speed_mul = 0.0;
            });
        },
    );
}

fn register_body(s: &Soma, t: &str) {
    s.method(
        t,
        "void AutomoveCharBodyTo(float afAcc, float afSpeedMul, float afMaxSpeed, const cVector3f&in avPosition)",
        |f| {
            let (mul, pos) = (f.arg::<f32>(1), vec_arg(f, 3));
            with(|p| {
                p.auto_active = true;
                p.auto_mul = mul;
                p.auto_target = pos;
            });
        },
    );
    s.method(
        t,
        "void SetAutomoveCharBodyTarget(const cVector3f&in avPosition)",
        |f| {
            let pos = vec_arg(f, 0);
            with(|p| p.auto_target = pos);
        },
    );
    s.method(t, "float GetAutoMoveTargetDistance()", |f| {
        let b = body();
        let target = with(|p| p.auto_target).unwrap_or_default();
        f.ret(if b != 0 {
            crate::math::len3(crate::math::sub3(target, body_get(b, body::FEET, 0.0).0))
        } else {
            0.0
        });
    });
    s.method(t, "bool IsAutomoveCharBodyActive()", |f| {
        f.ret(with(|p| p.auto_active).unwrap_or(false));
    });
    s.method(t, "void StopAutomoveCharBody()", |_| {
        with(|p| p.auto_active = false);
    });
}

fn register_health(s: &Soma, t: &str) {
    s.method(t, "void SetHealth(float afX)", |f| {
        set_health(f.arg::<f32>(0))
    });
    s.method(t, "float GetHealth()", |f| {
        f.ret(with(|p| p.health).unwrap_or(0.0));
    });
    s.method_new(t, "bool SetCanDie(bool abX)", |f| {
        let b = f.arg::<bool>(0);
        with(|p| p.can_die = b);
        f.ret(b);
    });
    s.method_new(t, "bool GetCanDie()", |f| {
        f.ret(with(|p| p.can_die).unwrap_or(true));
    });
    s.method(t, "void SetMaxHealth(float afX)", |f| {
        let x = f.arg::<f32>(0);
        with(|p| p.max_health = x);
    });
    s.method(t, "float GetMaxHealth()", |f| {
        f.ret(with(|p| p.max_health).unwrap_or(0.0));
    });
    s.method(t, "void AddHealth(float afX, float afMinHealth)", |f| {
        let (x, min) = (f.arg::<f32>(0), f.arg::<f32>(1));
        let h = with(|p| player::add_health(p.health, x, min)).unwrap_or(0.0);
        set_health(h);
    });
    s.method(t, "bool IsDead()", |f| f.ret(is_dead()));
}

fn register_misc(s: &Soma, t: &str) {
    s.method(t, "bool HasCollideCallbacks()", |f| {
        f.ret(host(op::HAS_COLLIDE, Io::new()).0 != 0);
    });
    s.method(
        t,
        "void AddCollideCallback(iLuxEntity @apEntity, const tString&in asCallbackFunc)",
        |f| {
            let c = handle(f, 0);
            player_entity_call(op::ADD_COLLIDE, c, text(f, 1));
        },
    );
    s.method(
        t,
        "void RemoveCollideCallback(const tString&in asEntityName)",
        |f| {
            player_entity_call(op::REMOVE_COLLIDE, 0, text(f, 0));
        },
    );
    s.method(t, "bool CheckEntityCollision(iLuxEntity@ apEntity)", |f| {
        let c = handle(f, 0);
        f.ret(c != 0 && player_entity_call(op::CHECK_COLLIDE, c, b"") != 0);
    });
    s.method(t, "float GetVisibilityRangeMul()", |f| {
        f.ret(visibility_range_mul())
    });
    s.method(t, "void SetVisibilityRangeMul(int alId, float afX)", |f| {
        let (id, x) = (f.arg::<i32>(0), f.arg::<f32>(1));
        with(|p| p.vis_mul.insert(id, x));
    });
    s.method(t, "void SetVisibilityMaxRange(int alId, float afX)", |f| {
        let (id, x) = (f.arg::<i32>(0), f.arg::<f32>(1));
        with(|p| p.vis_max.insert(id, x));
    });
    s.method(t, "float GetVisibilityMaxRange()", |f| {
        f.ret(visibility_max_range())
    });
    s.method(t, "float GetTimeSincePhysicsObjectInteraction()", |f| {
        f.ret(with(|p| p.time_since_phys).unwrap_or(0.0));
    });
    s.method(t, "void ResetTimeSincePhysicsObjectInteraction()", |_| {
        with(|p| p.time_since_phys = 0.0);
    });
    s.method(t, "void SetMaxCameraTrackingAmount(int alSize)", |_| {});
    s.method(t, "const cVector2f& GetCameraTrackingAvgMovement()", |f| {
        if let Some(p) = with(|p| std::ptr::addr_of_mut!(p.tracking_avg)) {
            f.ret_ref(p);
        }
    });
    s.method(t, "void ResetBasicProperties()", |_| {});
}

pub fn visibility_range_mul() -> f32 {
    with(|p| {
        let v: Vec<f32> = p.vis_mul.values().copied().collect();
        player::visibility_range_mul(&v)
    })
    .unwrap_or(1.0)
}

pub fn visibility_max_range() -> f32 {
    with(|p| {
        let v: Vec<f32> = p.vis_max.values().copied().collect();
        player::visibility_max_range(&v)
    })
    .unwrap_or(-1.0)
}

fn state_name(this: usize, off: i32) -> *mut c_void {
    pv(this.wrapping_add_signed(off as isize))
}

fn register_states(e: Engine, s: &Soma, off: Offsets) {
    let ps = "cLuxPlayerState";
    e.reg_property(ps.as_bytes(), b"int mlId", off.state_block, 0, false);
    e.reg_property(
        ps.as_bytes(),
        b"cLuxPlayer @mpPlayer",
        off.state_block + 8,
        0,
        false,
    );
    let name_off = off.state_name;
    let block_off = off.state_block;
    s.method(ps, "const tString& GetName()", move |f| {
        f.ret_ref(state_name(f.this as usize, name_off));
    });
    s.method(ps, "int GetId()", move |f| {
        // SAFETY: `this` is a player-state shell and the block sits at `block_off`.
        f.ret(unsafe {
            f.this
                .cast::<u8>()
                .offset(block_off as isize)
                .cast::<i32>()
                .read()
        });
    });

    let mt = "cLuxMoveState";
    e.reg_property(mt.as_bytes(), b"int mlId", off.move_block, 0, false);
    e.reg_property(
        mt.as_bytes(),
        b"cLuxPlayer @mpPlayer",
        off.move_block + 8,
        0,
        false,
    );
    const PROPS: [&str; 12] = [
        "float mfMaxForwardSpeed",
        "float mfMaxBackwardSpeed",
        "float mfMaxSidwaySpeed",
        "float mfForwardAcc",
        "float mfSidewayAcc",
        "float mfForwardDeacc",
        "float mfSidewayDeacc",
        "float mfForwardOppositeDirAccMul",
        "float mfSidewayOppositeDirAccMul",
        "float mfMaxForwardSpeedMul",
        "float mfMaxBackwardSpeedMul",
        "float mfMaxSidwaySpeedMul",
    ];
    for (i, d) in PROPS.iter().enumerate() {
        e.reg_property(
            mt.as_bytes(),
            d.as_bytes(),
            off.move_block + 16 + 4 * i as i32,
            0,
            false,
        );
    }
    let (move_name, move_block) = (off.move_name, off.move_block);
    s.method(mt, "const tString& GetName()", move |f| {
        f.ret_ref(state_name(f.this as usize, move_name));
    });
    s.method(mt, "int GetId()", move |f| {
        // SAFETY: `this` is a move-state shell and the block sits at `move_block`.
        f.ret(unsafe {
            f.this
                .cast::<u8>()
                .offset(move_block as isize)
                .cast::<i32>()
                .read()
        });
    });
    s.method(mt, "void SetAutoUpdateSpeedSettings(bool abX)", |f| {
        let (this, on) = (f.this as usize, f.arg::<bool>(0));
        with(|p| {
            for r in p.moves.values_mut().filter(|r| r.shell == this) {
                r.auto_speeds = on;
            }
        });
    });
    super::s5_scriptable::register(e, ps);
    super::s5_scriptable::register(e, mt);
}

// C ABI

fn bytes<'a>(p: *const u8, n: usize) -> &'a [u8] {
    if p.is_null() {
        &[]
    } else {
        // SAFETY: the C++ caller passes `n` readable bytes.
        unsafe { std::slice::from_raw_parts(p, n) }
    }
}

fn vec3(p: *const f32) -> Vector3f {
    // SAFETY: the C++ caller passes three floats.
    unsafe {
        Vector3f {
            x: *p,
            y: *p.add(1),
            z: *p.add(2),
        }
    }
}

/// # Safety
/// `host` outlives the process.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s5d_install_host(h: *const Host) {
    // SAFETY: caller contract.
    if let Some(h) = unsafe { h.as_ref() } {
        // SAFETY: the table lives for the process.
        let _ = HOST.set(unsafe { &*std::ptr::from_ref(h) });
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5d_new(sh: *mut c_void) {
    new(sh as usize);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5d_free() {
    free();
}

/// # Safety
/// `engine` is a live `asIScriptEngine`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s5d_register(
    engine: *mut c_void,
    state_block: i32,
    move_block: i32,
    state_name: i32,
    move_name: i32,
) {
    register(
        Engine(engine),
        Offsets {
            state_block,
            move_block,
            state_name,
            move_name,
        },
    );
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5d_set_camera(c: *mut c_void) {
    set_camera(c as usize);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5d_camera() -> *mut c_void {
    pv(camera())
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5d_set_body(b: *mut c_void) {
    set_body(b as usize);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5d_body() -> *mut c_void {
    pv(body())
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5d_active() -> bool {
    with(|p| p.active).unwrap_or(false)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5d_set_active(on: bool) {
    with(|p| p.active = on);
}

/// # Safety
/// Each pointer addresses its length in bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s5d_add_state(
    name: *const u8,
    n: usize,
    id: i32,
    file: *const u8,
    fn_: usize,
    class: *const u8,
    cn: usize,
    mv: bool,
) {
    add_state(bytes(name, n), id, bytes(file, fn_), bytes(class, cn), mv);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5d_change_state(id: i32, mv: bool) {
    change_state(id, mv);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5d_state(mv: bool) -> *mut c_void {
    pv(rec(mv).map_or(0, |r| r.shell))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5d_state_id(mv: bool) -> i32 {
    state_id(mv)
}

/// # Safety
/// `feet` points to three floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s5d_place_at_start(feet: *const f32, yaw: f32, crouching: bool) {
    place_at_start(vec3(feet), yaw, crouching);
}

/// # Safety
/// `vel` points to three floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s5d_hit_ground(b: *mut c_void, vel: *const f32) {
    hit_ground(b as usize, vec3(vel));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5d_update(dt: f32) {
    update(dt);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5d_post_update(dt: f32) {
    post_update(dt);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5d_variable_update(dt: f32) {
    variable_update(dt);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5d_gui(dt: f32) {
    gui(dt);
}

/// # Safety
/// `decl` points to `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s5d_message(decl: *const u8, n: usize) {
    message(bytes(decl, n));
}

/// # Safety
/// `decl` points to `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s5d_map_message(decl: *const u8, n: usize, map: *mut c_void) {
    map_message(bytes(decl, n), map as usize);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5d_action(a: i32, pressed: bool) {
    action(a, pressed);
}

/// # Safety
/// `amount` points to three floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s5d_analog(id: i32, amount: *const f32) {
    analog(id, vec3(amount));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5d_health() -> f32 {
    with(|p| p.health).unwrap_or(1.0)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5d_set_health_raw(x: f32) {
    with(|p| p.health = x);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5d_set_health(x: f32) {
    set_health(x);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5d_is_dead() -> bool {
    is_dead()
}

/// # Safety
/// `source` points to a `cSomaID`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s5d_give_damage(
    amount: f32,
    ty: i32,
    min_health: f32,
    source: *const c_void,
    damage_id: i32,
) -> bool {
    // SAFETY: caller contract.
    let src = unsafe { source.cast::<SomaId>().read() };
    give_damage(amount, ty, min_health, src, damage_id)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5d_visibility_range_mul() -> f32 {
    visibility_range_mul()
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5d_visibility_max_range() -> f32 {
    visibility_max_range()
}

/// # Safety
/// `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s5d_fov_mul(out: *mut Fade, aspect: bool) {
    let v = with(|p| if aspect { p.aspect_mul } else { p.fov_mul }).unwrap_or_default();
    // SAFETY: caller contract.
    unsafe { out.write(v) };
}

/// # Safety
/// `input` is readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s5d_set_fov_mul(input: *const Fade, aspect: bool) {
    // SAFETY: caller contract.
    let v = unsafe { input.read() };
    with(|p| {
        if aspect {
            p.aspect_mul = v;
        } else {
            p.fov_mul = v;
        }
    });
}

#[cfg(test)]
#[path = "s5d_tests.rs"]
mod tests;
