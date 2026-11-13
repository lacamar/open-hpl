//! Host call table of the A3 state drivers: one `call` entry with a scalar/pointer block, one op per engine call
//! the C++ still owns (player, camera, character body, physics bodies and joints, props, map, input, sound).

use crate::types::{Matrixf, Vector2f, Vector3f};
use std::ffi::c_void;

pub type Ptr = *mut c_void;

pub mod op {
    pub const PREV_STATE: u32 = 1;
    pub const CHANGE_STATE: u32 = 2;
    pub const CHANGE_MOVE_STATE: u32 = 3;
    pub const SET_INTERACT_MUL: u32 = 4;
    pub const DEFAULT_MASS: u32 = 5;
    pub const HAND_DRAWN: u32 = 6;
    pub const SET_FOCUS: u32 = 7;
    pub const SET_FOCUS_TEXT: u32 = 8;
    pub const ICON_STYLE_DEFAULT: u32 = 9;
    pub const MOVE_ALLOWS_INTERACTION: u32 = 10;
    pub const HANDS_DO_ACTION: u32 = 11;
    pub const CUR_MOVE_STATE: u32 = 12;
    pub const MOVE_NORMAL_GET: u32 = 13;
    pub const MOVE_NORMAL_CMD: u32 = 14;
    pub const LANTERN_ACTIVE: u32 = 15;
    pub const LANTERN_SET: u32 = 16;
    pub const CAM_V3: u32 = 20;
    pub const CAM_ANGLES: u32 = 21;
    pub const CAM_PITCH_LIMITS: u32 = 22;
    pub const CAM_ADD_PITCH: u32 = 23;
    pub const CAM_ADD_YAW: u32 = 24;
    pub const CAM_SET_PITCH_LIMITS: u32 = 25;
    pub const CAM_SET_YAW_LIMITS: u32 = 26;
    pub const CB_V3: u32 = 30;
    pub const CB_SET_POS: u32 = 31;
    pub const CB_SET_YAW: u32 = 32;
    pub const CB_SET_MASS: u32 = 33;
    pub const CB_FLAG: u32 = 34;
    pub const CB_CMD: u32 = 35;
    pub const CB_FITS: u32 = 36;
    pub const CB_RAY: u32 = 37;
    pub const CB_MOVE: u32 = 38;
    pub const CB_MAX_SPEED: u32 = 39;
    pub const CB_SHAPE_COLLIDE: u32 = 40;
    pub const CB_EXISTS: u32 = 41;
    pub const BODY_MASS: u32 = 50;
    pub const BODY_V3: u32 = 51;
    pub const BODY_MATRIX: u32 = 52;
    pub const BODY_VEL_AT: u32 = 53;
    pub const BODY_FLAG: u32 = 54;
    pub const BODY_SET_V3: u32 = 55;
    pub const BODY_SET_MASS: u32 = 56;
    pub const BODY_SET_FLAG: u32 = 57;
    pub const BODY_ADD: u32 = 58;
    pub const BODY_JOINT_NUM: u32 = 59;
    pub const BODY_JOINT: u32 = 60;
    pub const BODY_USER_DATA: u32 = 61;
    pub const JOINT_V3: u32 = 62;
    pub const JOINT_BODY: u32 = 63;
    pub const ENT_IS_PROP: u32 = 70;
    pub const ENT_MAX_FOCUS: u32 = 71;
    pub const ENT_CAN_INTERACT: u32 = 72;
    pub const ENT_DISABLED: u32 = 73;
    pub const ENT_CROSSHAIR: u32 = 74;
    pub const ENT_ON_INTERACT: u32 = 75;
    pub const ENT_INTERACT_CB: u32 = 76;
    pub const ENT_FOCUS_DRAW: u32 = 77;
    pub const PROP_IS_ITEM: u32 = 80;
    pub const PROP_SET_INTERACTED: u32 = 81;
    pub const PROP_SET_DISABLE_COLL: u32 = 82;
    pub const PROP_BODY_NUM: u32 = 83;
    pub const PROP_BODY: u32 = 84;
    pub const PROP_CONNECTED: u32 = 85;
    pub const PROP_EFFECT_MESH: u32 = 86;
    pub const PROP_DATA: u32 = 87;
    pub const DATA_F: u32 = 88;
    pub const MESH_SUB_NUM: u32 = 90;
    pub const MESH_SUB_PARENT: u32 = 91;
    pub const MESH_SUB_OUTLINE: u32 = 92;
    pub const FX_CLEAR_OUTLINE: u32 = 93;
    pub const CLOSEST_ENTITY: u32 = 100;
    pub const MAP_EXISTS: u32 = 101;
    pub const USE_CB_GET: u32 = 102;
    pub const USE_CB_RUN: u32 = 103;
    pub const USE_CB_REMOVE: u32 = 104;
    pub const MESSAGE: u32 = 105;
    pub const GUI_DRAW_GFX: u32 = 106;
    pub const IN_SMOOTH_MOUSE: u32 = 110;
    pub const IN_RESET_SMOOTH: u32 = 111;
    pub const IN_SMOOTH_POS: u32 = 112;
    pub const IN_INVERT_PITCH: u32 = 113;
    pub const IN_ROTATE_TRIGGERED: u32 = 114;
    pub const LADDER_V3: u32 = 120;
    pub const LADDER_SOUND: u32 = 121;
    pub const CFG_FLOAT: u32 = 130;
    pub const DEBUG_ROW: u32 = 150;
    pub const VARS_ITEM_TAKE: u32 = 140;
    pub const VARS_INTERACT: u32 = 141;
    pub const VARS_LADDER: u32 = 142;
    pub const PROP_AS_ENTITY: u32 = 143;
}

pub mod cb {
    pub const STOP_MOVEMENT: i32 = 0;
    pub const RESET_CLIMBING: i32 = 1;
    pub const FLAG_GRAVITY: i32 = 0;
    pub const FLAG_TEST_COLLISION: i32 = 1;
    pub const FLAG_DIAG_BOOST: i32 = 2;
    pub const V3_FEET: i32 = 0;
    pub const V3_FORWARD: i32 = 1;
    pub const V3_RIGHT: i32 = 2;
    pub const V3_POS: i32 = 3;
    pub const V3_SIZE: i32 = 4;
}

pub mod bd {
    pub const V3_LIN_VEL: i32 = 0;
    pub const V3_ANG_VEL: i32 = 1;
    pub const V3_POS: i32 = 2;
    pub const V3_MASS_CENTRE: i32 = 3;
    pub const M_LOCAL: i32 = 0;
    pub const M_INERTIA: i32 = 1;
    pub const FLAG_GRAVITY: i32 = 0;
    pub const FLAG_COLLIDE_CHAR: i32 = 1;
    pub const ADD_FORCE: i32 = 0;
    pub const ADD_TORQUE: i32 = 1;
    pub const ADD_IMPULSE: i32 = 2;
    pub const ADD_FORCE_AT: i32 = 3;
    pub const ADD_IMPULSE_AT: i32 = 4;
}

pub mod data {
    pub const GRAB: i32 = 0;
    pub const PUSH: i32 = 1;
    pub const SLIDE: i32 = 2;
    pub const MOVE_BASE: i32 = 3;
    pub const G_USE_DEPTH: i32 = 0;
    pub const G_USE_OFFSET: i32 = 1;
    pub const G_USE_ROTATION: i32 = 2;
    pub const G_DEPTH: i32 = 3;
    pub const G_DEPTH_INC: i32 = 4;
    pub const G_MIN_DEPTH: i32 = 5;
    pub const G_MAX_DEPTH: i32 = 6;
    pub const G_THROW_IMPULSE: i32 = 7;
    pub const G_MASS_MUL: i32 = 8;
    pub const G_FORCE_MUL: i32 = 9;
    pub const G_TORQUE_MUL: i32 = 10;
    pub const G_POS_OFFSET: i32 = 11;
    pub const G_ROT_OFFSET: i32 = 12;
    pub const P_AT_POINT: i32 = 0;
    pub const P_FORCE_MUL: i32 = 1;
    pub const P_IMPULSE: i32 = 2;
    pub const S_MAX_SPEED: i32 = 0;
    pub const S_SLOW_DOWN: i32 = 1;
    pub const S_SPEED_FACTOR: i32 = 2;
    pub const S_THROW_IMPULSE: i32 = 3;
    pub const M_MAX_SPEED: i32 = 0;
    pub const M_SLOW_DOWN: i32 = 1;
    pub const M_SPEED_FACTOR: i32 = 2;
    pub const M_THROW_IMPULSE: i32 = 3;
}

#[repr(C)]
pub struct Io {
    pub p: [Ptr; 3],
    pub i: [i32; 3],
    pub f: [f32; 24],
}

impl Io {
    pub fn new() -> Io {
        Io {
            p: [std::ptr::null_mut(); 3],
            i: [0; 3],
            f: [0.0; 24],
        }
    }

    pub fn v3(&self) -> Vector3f {
        Vector3f {
            x: self.f[0],
            y: self.f[1],
            z: self.f[2],
        }
    }
}

impl Default for Io {
    fn default() -> Io {
        Io::new()
    }
}

#[repr(C)]
pub struct Host {
    pub ctx: Ptr,
    pub call: extern "C" fn(Ptr, u32, *mut Io) -> i32,
}

fn put3(io: &mut Io, at: usize, v: Vector3f) {
    if let Some(s) = io.f.get_mut(at..at + 3) {
        s.copy_from_slice(&[v.x, v.y, v.z]);
    }
}

impl Host {
    pub fn raw(&self, op: u32, io: &mut Io) -> i32 {
        (self.call)(self.ctx, op, io)
    }

    fn ii(&self, op: u32, i0: i32, i1: i32) -> i32 {
        let mut io = Io::new();
        io.i = [i0, i1, 0];
        self.raw(op, &mut io)
    }

    fn pi(&self, op: u32, p0: Ptr, i0: i32) -> i32 {
        let mut io = Io::new();
        io.p[0] = p0;
        io.i[0] = i0;
        self.raw(op, &mut io)
    }

    fn pf(&self, op: u32, p0: Ptr, i0: i32) -> f32 {
        let mut io = Io::new();
        io.p[0] = p0;
        io.i[0] = i0;
        self.raw(op, &mut io);
        io.f[0]
    }

    fn pv(&self, op: u32, p0: Ptr, i0: i32) -> Vector3f {
        let mut io = Io::new();
        io.p[0] = p0;
        io.i[0] = i0;
        self.raw(op, &mut io);
        io.v3()
    }

    fn pset3(&self, op: u32, p0: Ptr, i0: i32, v: Vector3f) {
        let mut io = Io::new();
        io.p[0] = p0;
        io.i[0] = i0;
        put3(&mut io, 0, v);
        self.raw(op, &mut io);
    }

    pub fn prev_state(&self) -> i32 {
        self.ii(op::PREV_STATE, 0, 0)
    }

    pub fn change_state(&self, s: i32) {
        self.ii(op::CHANGE_STATE, s, 0);
    }

    pub fn change_move_state(&self, s: i32) {
        self.ii(op::CHANGE_MOVE_STATE, s, 0);
    }

    pub fn set_interact_mul(&self, f: f32) {
        let mut io = Io::new();
        io.f[0] = f;
        self.raw(op::SET_INTERACT_MUL, &mut io);
    }

    pub fn default_mass(&self) -> f32 {
        let mut io = Io::new();
        self.raw(op::DEFAULT_MASS, &mut io);
        io.f[0]
    }

    pub fn hand_drawn(&self, b: bool) {
        self.ii(op::HAND_DRAWN, i32::from(b), 0);
    }

    pub fn set_focus(&self, ent: Ptr, body: Ptr, dist: f32) {
        let mut io = Io::new();
        io.p = [ent, body, std::ptr::null_mut()];
        io.f[0] = dist;
        self.raw(op::SET_FOCUS, &mut io);
    }

    pub fn set_focus_text(&self, ent: Ptr) {
        self.pi(op::SET_FOCUS_TEXT, ent, 0);
    }

    pub fn icon_style_default(&self) -> bool {
        self.ii(op::ICON_STYLE_DEFAULT, 0, 0) != 0
    }

    pub fn move_allows_interaction(&self) -> bool {
        self.ii(op::MOVE_ALLOWS_INTERACTION, 0, 0) != 0
    }

    pub fn hands_do_action(&self, action: i32, pressed: bool) {
        self.ii(op::HANDS_DO_ACTION, action, i32::from(pressed));
    }

    pub fn cur_move_state(&self) -> i32 {
        self.ii(op::CUR_MOVE_STATE, 0, 0)
    }

    pub fn move_normal_running(&self) -> bool {
        self.ii(op::MOVE_NORMAL_GET, 0, 0) != 0
    }

    pub fn move_normal_crouching(&self) -> bool {
        self.ii(op::MOVE_NORMAL_GET, 1, 0) != 0
    }

    pub fn move_normal_uncrouch(&self) {
        self.ii(op::MOVE_NORMAL_CMD, 0, 0);
    }

    pub fn move_normal_reset_jumping(&self) {
        self.ii(op::MOVE_NORMAL_CMD, 1, 0);
    }

    pub fn move_normal_jump(&self) {
        self.ii(op::MOVE_NORMAL_CMD, 2, 0);
    }

    pub fn lantern_active(&self) -> bool {
        self.ii(op::LANTERN_ACTIVE, 0, 0) != 0
    }

    pub fn lantern_set(&self, active: bool) {
        self.ii(op::LANTERN_SET, i32::from(active), 0);
    }

    pub fn cam_v3(&self, which: i32) -> Vector3f {
        self.pv(op::CAM_V3, std::ptr::null_mut(), which)
    }

    pub fn cam_pos(&self) -> Vector3f {
        self.cam_v3(0)
    }

    pub fn cam_forward(&self) -> Vector3f {
        self.cam_v3(1)
    }

    pub fn cam_up(&self) -> Vector3f {
        self.cam_v3(2)
    }

    pub fn cam_right(&self) -> Vector3f {
        self.cam_v3(3)
    }

    /// `(pitch, yaw, roll)`.
    pub fn cam_angles(&self) -> Vector3f {
        self.pv(op::CAM_ANGLES, std::ptr::null_mut(), 0)
    }

    pub fn cam_pitch_limits(&self) -> (f32, f32) {
        let mut io = Io::new();
        self.raw(op::CAM_PITCH_LIMITS, &mut io);
        (io.f[0], io.f[1])
    }

    pub fn cam_add_pitch(&self, f: f32) {
        let mut io = Io::new();
        io.f[0] = f;
        self.raw(op::CAM_ADD_PITCH, &mut io);
    }

    pub fn cam_add_yaw(&self, f: f32) {
        let mut io = Io::new();
        io.f[0] = f;
        self.raw(op::CAM_ADD_YAW, &mut io);
    }

    pub fn cam_set_pitch_limits(&self, a: f32, b: f32) {
        let mut io = Io::new();
        io.f[0] = a;
        io.f[1] = b;
        self.raw(op::CAM_SET_PITCH_LIMITS, &mut io);
    }

    pub fn cam_set_yaw_limits(&self, a: f32, b: f32) {
        let mut io = Io::new();
        io.f[0] = a;
        io.f[1] = b;
        self.raw(op::CAM_SET_YAW_LIMITS, &mut io);
    }

    pub fn cb_v3(&self, which: i32) -> Vector3f {
        self.pv(op::CB_V3, std::ptr::null_mut(), which)
    }

    pub fn cb_set_pos(&self, v: Vector3f) {
        self.pset3(op::CB_SET_POS, std::ptr::null_mut(), 0, v);
    }

    pub fn cb_set_yaw(&self, f: f32) {
        let mut io = Io::new();
        io.f[0] = f;
        self.raw(op::CB_SET_YAW, &mut io);
    }

    pub fn cb_set_mass(&self, f: f32) {
        let mut io = Io::new();
        io.f[0] = f;
        self.raw(op::CB_SET_MASS, &mut io);
    }

    pub fn cb_flag(&self, which: i32, on: bool) {
        self.ii(op::CB_FLAG, which, i32::from(on));
    }

    pub fn cb_cmd(&self, which: i32) {
        self.ii(op::CB_CMD, which, 0);
    }

    pub fn cb_fits(&self, pos: Vector3f) -> bool {
        let mut io = Io::new();
        put3(&mut io, 0, pos);
        self.raw(op::CB_FITS, &mut io) != 0
    }

    pub fn cb_ray(&self, a: Vector3f, b: Vector3f) -> bool {
        let mut io = Io::new();
        put3(&mut io, 0, a);
        put3(&mut io, 3, b);
        self.raw(op::CB_RAY, &mut io) != 0
    }

    pub fn cb_move(&self, dir: i32, mul: f32) {
        let mut io = Io::new();
        io.i[0] = dir;
        io.f[0] = mul;
        self.raw(op::CB_MOVE, &mut io);
    }

    pub fn cb_max_speed(&self, dir: i32, speed: f32) {
        let mut io = Io::new();
        io.i[0] = dir;
        io.f[0] = speed;
        io.f[1] = -speed;
        self.raw(op::CB_MAX_SPEED, &mut io);
    }

    pub fn cb_shape_collide(&self, body: Ptr, char_pos: Vector3f, body_mtx: &Matrixf) -> bool {
        let mut io = Io::new();
        io.p[0] = body;
        put3(&mut io, 0, char_pos);
        for (d, s) in io.f.iter_mut().skip(3).zip(body_mtx.m.iter().flatten()) {
            *d = *s;
        }
        self.raw(op::CB_SHAPE_COLLIDE, &mut io) != 0
    }

    pub fn cb_exists(&self) -> bool {
        self.ii(op::CB_EXISTS, 0, 0) != 0
    }

    pub fn body_mass(&self, b: Ptr) -> f32 {
        self.pf(op::BODY_MASS, b, 0)
    }

    pub fn body_v3(&self, b: Ptr, which: i32) -> Vector3f {
        self.pv(op::BODY_V3, b, which)
    }

    pub fn body_matrix(&self, b: Ptr, which: i32) -> Matrixf {
        let mut io = Io::new();
        io.p[0] = b;
        io.i[0] = which;
        self.raw(op::BODY_MATRIX, &mut io);
        let mut m = Matrixf::default();
        for (d, s) in m.m.iter_mut().flatten().zip(io.f.iter()) {
            *d = *s;
        }
        m
    }

    pub fn body_vel_at(&self, b: Ptr, pos: Vector3f) -> Vector3f {
        let mut io = Io::new();
        io.p[0] = b;
        put3(&mut io, 0, pos);
        self.raw(op::BODY_VEL_AT, &mut io);
        io.v3()
    }

    pub fn body_flag(&self, b: Ptr, which: i32) -> bool {
        self.pi(op::BODY_FLAG, b, which) != 0
    }

    pub fn body_set_v3(&self, b: Ptr, which: i32, v: Vector3f) {
        self.pset3(op::BODY_SET_V3, b, which, v);
    }

    pub fn body_set_mass(&self, b: Ptr, f: f32) {
        let mut io = Io::new();
        io.p[0] = b;
        io.f[0] = f;
        self.raw(op::BODY_SET_MASS, &mut io);
    }

    pub fn body_set_flag(&self, b: Ptr, which: i32, on: bool) {
        let mut io = Io::new();
        io.p[0] = b;
        io.i = [which, i32::from(on), 0];
        self.raw(op::BODY_SET_FLAG, &mut io);
    }

    pub fn body_add(&self, b: Ptr, kind: i32, v: Vector3f, at: Vector3f) {
        let mut io = Io::new();
        io.p[0] = b;
        io.i[0] = kind;
        put3(&mut io, 0, v);
        put3(&mut io, 3, at);
        self.raw(op::BODY_ADD, &mut io);
    }

    pub fn body_joint_num(&self, b: Ptr) -> i32 {
        self.pi(op::BODY_JOINT_NUM, b, 0)
    }

    pub fn body_joint(&self, b: Ptr, i: i32) -> Ptr {
        let mut io = Io::new();
        io.p[0] = b;
        io.i[0] = i;
        self.raw(op::BODY_JOINT, &mut io);
        io.p[0]
    }

    pub fn body_user_data(&self, b: Ptr) -> Ptr {
        let mut io = Io::new();
        io.p[0] = b;
        self.raw(op::BODY_USER_DATA, &mut io);
        io.p[0]
    }

    pub fn joint_pin(&self, j: Ptr) -> Vector3f {
        self.pv(op::JOINT_V3, j, 0)
    }

    pub fn joint_pivot(&self, j: Ptr) -> Vector3f {
        self.pv(op::JOINT_V3, j, 1)
    }

    pub fn joint_child(&self, j: Ptr) -> Ptr {
        let mut io = Io::new();
        io.p[0] = j;
        io.i[0] = 0;
        self.raw(op::JOINT_BODY, &mut io);
        io.p[0]
    }

    pub fn joint_parent(&self, j: Ptr) -> Ptr {
        let mut io = Io::new();
        io.p[0] = j;
        io.i[0] = 1;
        self.raw(op::JOINT_BODY, &mut io);
        io.p[0]
    }

    pub fn ent_is_prop(&self, e: Ptr) -> bool {
        self.pi(op::ENT_IS_PROP, e, 0) != 0
    }

    pub fn ent_max_focus(&self, e: Ptr) -> f32 {
        self.pf(op::ENT_MAX_FOCUS, e, 0)
    }

    pub fn ent_can_interact(&self, e: Ptr, body: Ptr) -> bool {
        let mut io = Io::new();
        io.p = [e, body, std::ptr::null_mut()];
        self.raw(op::ENT_CAN_INTERACT, &mut io) != 0
    }

    pub fn ent_disabled(&self, e: Ptr) -> bool {
        self.pi(op::ENT_DISABLED, e, 0) != 0
    }

    pub fn ent_crosshair(&self, e: Ptr, body: Ptr, pos: Vector3f) -> i32 {
        let mut io = Io::new();
        io.p = [e, body, std::ptr::null_mut()];
        put3(&mut io, 0, pos);
        self.raw(op::ENT_CROSSHAIR, &mut io)
    }

    pub fn ent_on_interact(&self, e: Ptr, body: Ptr, pos: Vector3f) {
        let mut io = Io::new();
        io.p = [e, body, std::ptr::null_mut()];
        put3(&mut io, 0, pos);
        self.raw(op::ENT_ON_INTERACT, &mut io);
    }

    pub fn ent_interact_cb(&self, e: Ptr) {
        self.pi(op::ENT_INTERACT_CB, e, 0);
    }

    pub fn ent_focus_draw(&self, e: Ptr, gui_set: Ptr, frame_time: f32) {
        let mut io = Io::new();
        io.p = [e, gui_set, std::ptr::null_mut()];
        io.f[0] = frame_time;
        self.raw(op::ENT_FOCUS_DRAW, &mut io);
    }

    pub fn prop_is_item(&self, p: Ptr) -> bool {
        self.pi(op::PROP_IS_ITEM, p, 0) != 0
    }

    pub fn prop_set_interacted(&self, p: Ptr, on: bool) {
        self.pi(op::PROP_SET_INTERACTED, p, i32::from(on));
    }

    pub fn prop_set_disable_coll(&self, p: Ptr, on: bool) {
        self.pi(op::PROP_SET_DISABLE_COLL, p, i32::from(on));
    }

    pub fn prop_body_num(&self, p: Ptr) -> i32 {
        self.pi(op::PROP_BODY_NUM, p, 0)
    }

    pub fn prop_body(&self, p: Ptr, i: i32) -> Ptr {
        let mut io = Io::new();
        io.p[0] = p;
        io.i[0] = i;
        self.raw(op::PROP_BODY, &mut io);
        io.p[0]
    }

    pub fn prop_connected(&self, p: Ptr) -> bool {
        self.pi(op::PROP_CONNECTED, p, 0) != 0
    }

    pub fn prop_effect_mesh(&self, p: Ptr) -> Ptr {
        let mut io = Io::new();
        io.p[0] = p;
        self.raw(op::PROP_EFFECT_MESH, &mut io);
        io.p[0]
    }

    pub fn prop_data(&self, p: Ptr, which: i32) -> Ptr {
        let mut io = Io::new();
        io.p[0] = p;
        io.i[0] = which;
        self.raw(op::PROP_DATA, &mut io);
        io.p[0]
    }

    pub fn data_f(&self, d: Ptr, which: i32, field: i32) -> f32 {
        let mut io = Io::new();
        io.p[0] = d;
        io.i = [which, field, 0];
        self.raw(op::DATA_F, &mut io);
        io.f[0]
    }

    pub fn data_b(&self, d: Ptr, which: i32, field: i32) -> bool {
        self.data_f(d, which, field) != 0.0
    }

    pub fn data_v3(&self, d: Ptr, which: i32, field: i32) -> Vector3f {
        let mut io = Io::new();
        io.p[0] = d;
        io.i = [which, field, 0];
        self.raw(op::DATA_F, &mut io);
        io.v3()
    }

    pub fn mesh_sub_num(&self, m: Ptr) -> i32 {
        self.pi(op::MESH_SUB_NUM, m, 0)
    }

    pub fn mesh_sub_parent(&self, m: Ptr, i: i32) -> Ptr {
        let mut io = Io::new();
        io.p[0] = m;
        io.i[0] = i;
        self.raw(op::MESH_SUB_PARENT, &mut io);
        io.p[0]
    }

    pub fn mesh_sub_outline(&self, m: Ptr, i: i32) {
        self.pi(op::MESH_SUB_OUTLINE, m, i);
    }

    pub fn fx_clear_outline(&self) {
        self.ii(op::FX_CLEAR_OUTLINE, 0, 0);
    }

    /// `(distance, body, entity)` of the closest focusable entity along the camera ray.
    pub fn closest_entity(&self, pos: Vector3f, dir: Vector3f, max: f32) -> (f32, Ptr, Ptr) {
        let mut io = Io::new();
        put3(&mut io, 0, pos);
        put3(&mut io, 3, dir);
        io.f[6] = max;
        self.raw(op::CLOSEST_ENTITY, &mut io);
        (io.f[0], io.p[0], io.p[1])
    }

    pub fn map_exists(&self) -> bool {
        self.ii(op::MAP_EXISTS, 0, 0) != 0
    }

    pub fn use_cb_get(&self, item: Ptr, ent: Ptr) -> Ptr {
        let mut io = Io::new();
        io.p = [item, ent, std::ptr::null_mut()];
        self.raw(op::USE_CB_GET, &mut io);
        io.p[2]
    }

    /// Reads the callback's auto-destroy flag and name, then runs its script; true when it auto-destroys.
    pub fn use_cb_run(&self, cb: Ptr) -> bool {
        self.pi(op::USE_CB_RUN, cb, 0) != 0
    }

    pub fn use_cb_remove(&self, cb: Ptr) {
        self.pi(op::USE_CB_REMOVE, cb, 0);
    }

    pub fn message(&self, which: i32) {
        self.ii(op::MESSAGE, which, 0);
    }

    pub fn gui_draw_gfx(&self, gfx: Ptr, pos: Vector3f, size: Vector2f, color: f32) {
        let mut io = Io::new();
        io.p[0] = gfx;
        put3(&mut io, 0, pos);
        io.f[3] = size.x;
        io.f[4] = size.y;
        io.f[5] = color;
        self.raw(op::GUI_DRAW_GFX, &mut io);
    }

    pub fn in_smooth_mouse(&self) -> bool {
        self.ii(op::IN_SMOOTH_MOUSE, 0, 0) != 0
    }

    pub fn in_reset_smooth(&self) {
        self.ii(op::IN_RESET_SMOOTH, 0, 0);
    }

    pub fn in_smooth_pos(&self, v: Vector2f) -> Vector2f {
        let mut io = Io::new();
        io.f[0] = v.x;
        io.f[1] = v.y;
        self.raw(op::IN_SMOOTH_POS, &mut io);
        Vector2f {
            x: io.f[0],
            y: io.f[1],
        }
    }

    pub fn in_invert_pitch(&self) -> bool {
        self.ii(op::IN_INVERT_PITCH, 0, 0) != 0
    }

    pub fn in_rotate_triggered(&self) -> bool {
        self.ii(op::IN_ROTATE_TRIGGERED, 0, 0) != 0
    }

    pub fn ladder_v3(&self, l: Ptr, which: i32) -> Vector3f {
        self.pv(op::LADDER_V3, l, which)
    }

    pub fn ladder_y(&self, l: Ptr, which: i32) -> f32 {
        self.pf(op::LADDER_V3, l, which)
    }

    pub fn ladder_sound(&self, l: Ptr, which: i32) {
        self.pi(op::LADDER_SOUND, l, which);
    }

    pub fn cfg(
        &self,
        section: &'static std::ffi::CStr,
        key: &'static std::ffi::CStr,
        def: f32,
    ) -> f32 {
        let mut io = Io::new();
        io.p[0] = section.as_ptr().cast_mut().cast();
        io.p[1] = key.as_ptr().cast_mut().cast();
        io.f[0] = def;
        self.raw(op::CFG_FLOAT, &mut io);
        io.f[0]
    }

    /// One debug HUD row (`hud` is the gui set and font): the C++ formats and draws it; the prop row returns the new y.
    pub fn debug_row(
        &self,
        hud: (Ptr, Ptr),
        row: i32,
        y: f32,
        obj: Ptr,
        v: Vector3f,
        s: f32,
    ) -> f32 {
        let mut io = Io::new();
        io.p = [hud.0, hud.1, obj];
        io.i[0] = row;
        io.f[0] = y;
        put3(&mut io, 1, v);
        io.f[4] = s;
        self.raw(op::DEBUG_ROW, &mut io);
        io.f[0]
    }

    pub fn take_inventory_item(&self) -> Ptr {
        let mut io = Io::new();
        self.raw(op::VARS_ITEM_TAKE, &mut io);
        io.p[0]
    }

    /// `(prop, body, focus pos)` of `cLuxPlayerStateVars::SetupInteraction`.
    pub fn vars_interact(&self) -> (Ptr, Ptr, Vector3f) {
        let mut io = Io::new();
        self.raw(op::VARS_INTERACT, &mut io);
        (io.p[0], io.p[1], io.v3())
    }

    /// `(ladder, start position)` of `cLuxPlayerStateVars::SetupLadder`.
    pub fn vars_ladder(&self) -> (Ptr, Vector3f) {
        let mut io = Io::new();
        self.raw(op::VARS_LADDER, &mut io);
        (io.p[0], io.v3())
    }

    pub fn prop_as_entity(&self, p: Ptr) -> Ptr {
        let mut io = Io::new();
        io.p[0] = p;
        self.raw(op::PROP_AS_ENTITY, &mut io);
        io.p[0]
    }
}
