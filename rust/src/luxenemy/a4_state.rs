//! A4 POD embedded in the C++ enemy (`OhplA4Base`) and the host call table for engine objects.

use super::ctx::Ctx;
use super::host::{Host, Io as Ga3Io};
use crate::math::{Vector3f, v3};
use std::ffi::c_void;

pub const OP_GA3: u32 = 1;
pub const OP_STEP_SIZE: u32 = 2;
pub const OP_UPDATE_STEP: u32 = 3;
pub const OP_ANIM_NAME_EQ: u32 = 10;
pub const OP_ANIM_IS_ACTIVE: u32 = 11;
pub const OP_ANIM_IS_OVER: u32 = 12;
pub const OP_ANIM_FIND: u32 = 13;
pub const OP_WARN_NO_ANIM: u32 = 14;
pub const OP_ANIM_SET_ACTIVE: u32 = 15;
pub const OP_ANIM_FADE_OUT: u32 = 16;
pub const OP_ANIM_IS_FADING: u32 = 17;
pub const OP_ANIM_SET_WEIGHT: u32 = 18;
pub const OP_ANIM_FADE_IN: u32 = 19;
pub const OP_ANIM_SET_LOOP: u32 = 20;
pub const OP_ANIM_REL_TIME: u32 = 21;
pub const OP_ANIM_SET_REL_TIME: u32 = 22;
pub const OP_ANIM_SET_TIME: u32 = 23;
pub const OP_ANIM_IS_LOOPING: u32 = 24;
pub const OP_ANIM_PREV_TIME: u32 = 25;
pub const OP_ANIM_EVENT_TIME: u32 = 26;
pub const OP_ANIM_TIME: u32 = 27;
pub const OP_ANIM_SET_SPEED: u32 = 28;
pub const OP_ANIM_LENGTH: u32 = 29;
pub const OP_SOUND_EXISTS: u32 = 40;
pub const OP_SOUND_FADE_OUT: u32 = 41;
pub const OP_SOUND_CREATE: u32 = 42;
pub const OP_SOUND_ID: u32 = 43;
pub const OP_AMBIENT_EMPTY: u32 = 44;
pub const OP_CAM_COLLIDE_MESH: u32 = 50;
pub const OP_CAM_POS: u32 = 51;
pub const OP_PLAYER_SANITY: u32 = 52;
pub const OP_SUBMESH_NUM: u32 = 53;
pub const OP_ADD_GLOW: u32 = 54;
pub const OP_LIGHT_LEVEL_AT: u32 = 55;
pub const OP_MESH_VISIBLE: u32 = 56;
pub const OP_FADE_TO_SMOKE: u32 = 60;
pub const OP_GROUND_RAY: u32 = 62;
pub const OP_SET_ENTITY_OFFSET: u32 = 63;
pub const OP_DEBUG_DAMAGE: u32 = 70;
pub const OP_DAMAGE_MUL: u32 = 71;
pub const OP_ON_DAMAGE: u32 = 72;
pub const OP_CALLBACK_DEATH: u32 = 73;
pub const OP_ATTACK_SHAPE: u32 = 74;
pub const OP_ATTACK_HIT_SOUND: u32 = 75;
pub const OP_ON_SET_ACTIVE_SPECIFIC: u32 = 80;
pub const OP_BODY_EXISTS: u32 = 81;
pub const OP_MESH_EXISTS: u32 = 82;
pub const OP_MESH_SET_ACTIVE: u32 = 83;
pub const OP_ON_RESET_PROPERTIES: u32 = 84;
pub const OP_BODY_SET_FEET: u32 = 85;
pub const OP_FRUSTUM_BODY: u32 = 90;
pub const OP_FRUSTUM_BODY_AT: u32 = 91;
pub const OP_FOOD_BEGIN: u32 = 100;
pub const OP_FOOD_IS_OBJECT: u32 = 101;
pub const OP_FOOD_IS_FOOD: u32 = 102;
pub const OP_FOOD_LOWEST_Y: u32 = 103;
pub const OP_FOOD_POS: u32 = 104;
pub const OP_BODY_RIGHT_UP_FWD: u32 = 110;

/// Mirrors `eLuxEnemyType`.
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Kind {
    Grunt = 0,
    WaterLurker = 1,
    ManPig = 2,
}

impl Kind {
    pub fn from_i32(v: i32) -> Option<Self> {
        match v {
            0 => Some(Self::Grunt),
            1 => Some(Self::WaterLurker),
            2 => Some(Self::ManPig),
            _ => None,
        }
    }
}

#[repr(C)]
pub struct Base {
    pub c: *mut Ctx,
    pub cur_anim: *mut c_void,
    pub cur_sound: *mut c_void,
    pub kind: i32,
    pub glow_alpha: f32,
    pub glow_goal: f32,
    pub glow_update_count: f32,
    pub glow_max_dist: f32,
    pub hallucination_end_dist: f32,
    pub ground_ray_count: f32,
    pub anim_speed_mul: f32,
    pub ambient_count: f32,
    pub move_speed_anim_mul: f32,
    pub ground_n: i32,
    pub sound_state: i32,
    pub cur_sound_id: i32,
    pub toughness: i32,
    pub ground: [f32; 4],
    pub ambient_min: [f32; 4],
    pub ambient_max: [f32; 4],
    pub hallucination: bool,
    pub align_ground_ray: bool,
    pub use_animations: bool,
    pub anim_speed_dependant: bool,
    pub use_move_anim_when_over: bool,
    pub causes_sanity: bool,
    pub causes_sanity_default: bool,
    pub pad: u8,
}

/// Register file of the A4 table: engine handles in `p`, scalars in `i`, vectors in slots of three floats.
#[repr(C)]
pub struct Io {
    pub p: [*mut c_void; 3],
    pub i: [i32; 4],
    pub f: [f32; 12],
}

impl Io {
    pub fn new() -> Self {
        Self {
            p: [std::ptr::null_mut(); 3],
            i: [0; 4],
            f: [0.0; 12],
        }
    }
}

#[repr(C)]
pub struct Table {
    pub ctx: *mut c_void,
    pub call: unsafe extern "C" fn(*mut c_void, u32, *mut Io) -> i32,
}

pub struct H<'a>(pub &'a Table);

impl H<'_> {
    pub fn call(&self, op: u32, io: &mut Io) -> i32 {
        // SAFETY: the C++ caller guarantees the table accepts its `ctx` for the call.
        unsafe { (self.0.call)(self.0.ctx, op, io) }
    }

    pub fn q(&self, op: u32) -> i32 {
        self.call(op, &mut Io::new())
    }

    pub fn qf(&self, op: u32) -> f32 {
        let mut io = Io::new();
        self.call(op, &mut io);
        io.f[0]
    }

    pub fn q3(&self, op: u32) -> Vector3f {
        let mut io = Io::new();
        self.call(op, &mut io);
        v3(io.f[0], io.f[1], io.f[2])
    }

    pub fn qi(&self, op: u32, i: i32) -> i32 {
        let mut io = Io::new();
        io.i[0] = i;
        self.call(op, &mut io)
    }

    pub fn qp(&self, op: u32, p: *mut c_void) -> i32 {
        let mut io = Io::new();
        io.p[0] = p;
        self.call(op, &mut io)
    }

    pub fn pf(&self, op: u32, p: *mut c_void, f: f32) -> i32 {
        let mut io = Io::new();
        io.p[0] = p;
        io.f[0] = f;
        self.call(op, &mut io)
    }

    pub fn pi(&self, op: u32, p: *mut c_void, i: i32) -> i32 {
        let mut io = Io::new();
        io.p[0] = p;
        io.i[0] = i;
        self.call(op, &mut io)
    }

    pub fn pq(&self, op: u32, p: *mut c_void) -> f32 {
        let mut io = Io::new();
        io.p[0] = p;
        self.call(op, &mut io);
        io.f[0]
    }

    pub fn at(&self, op: u32, v: Vector3f) -> (i32, Io) {
        let mut io = Io::new();
        io.f[..3].copy_from_slice(&[v.x, v.y, v.z]);
        let r = self.call(op, &mut io);
        (r, io)
    }
}

/// Routes the T0 enemy kernels' host ops through the A4 table.
pub struct Ga3Via<'a>(pub &'a H<'a>);

impl Host for Ga3Via<'_> {
    fn call(&mut self, op: u32, io: &mut Ga3Io) -> i32 {
        let mut a = Io::new();
        a.p[0] = std::ptr::from_mut(io).cast();
        a.i[0] = i32::try_from(op).unwrap_or(0);
        self.0.call(OP_GA3, &mut a)
    }
}
