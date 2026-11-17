//! The host-op channel to the C++ engine objects (bodies, meshes, physics, scripts, nodes).

use crate::math::{Vector3f, v3};
use std::sync::atomic::{AtomicUsize, Ordering};

#[repr(C)]
pub struct Io {
    pub p: [usize; 4],
    pub i: [i32; 8],
    pub f: [f32; 16],
    pub a: [u64; 4],
    pub s: [*const u8; 2],
    pub n: [usize; 2],
}

type OpFn = unsafe extern "C" fn(u32, *mut Io) -> i32;

static OPS: AtomicUsize = AtomicUsize::new(0);

pub fn install(f: Option<OpFn>) {
    OPS.store(f.map_or(0, |f| f as usize), Ordering::Release);
}

impl Default for Io {
    fn default() -> Self {
        Self::new()
    }
}

impl Io {
    pub fn fl(&self, k: usize) -> f32 {
        self.f.get(k).copied().unwrap_or(0.0)
    }

    pub fn int(&self, k: usize) -> i32 {
        self.i.get(k).copied().unwrap_or(0)
    }

    pub fn ptr(&self, k: usize) -> usize {
        self.p.get(k).copied().unwrap_or(0)
    }

    pub fn new() -> Self {
        Self {
            p: [0; 4],
            i: [0; 8],
            f: [0.0; 16],
            a: [0; 4],
            s: [std::ptr::null(); 2],
            n: [0; 2],
        }
    }

    pub fn p(mut self, k: usize, v: usize) -> Self {
        if let Some(s) = self.p.get_mut(k) {
            *s = v;
        }
        self
    }

    pub fn i(mut self, k: usize, v: i32) -> Self {
        if let Some(s) = self.i.get_mut(k) {
            *s = v;
        }
        self
    }

    pub fn f(mut self, k: usize, v: f32) -> Self {
        if let Some(s) = self.f.get_mut(k) {
            *s = v;
        }
        self
    }

    pub fn v(self, k: usize, v: Vector3f) -> Self {
        self.f(k, v.x).f(k + 1, v.y).f(k + 2, v.z)
    }

    pub fn a(mut self, k: usize, v: u64) -> Self {
        if let Some(s) = self.a.get_mut(k) {
            *s = v;
        }
        self
    }

    pub fn s(mut self, k: usize, b: &[u8]) -> Self {
        if let (Some(s), Some(n)) = (self.s.get_mut(k), self.n.get_mut(k)) {
            *s = b.as_ptr();
            *n = b.len();
        }
        self
    }

    pub fn out_v(&self, k: usize) -> Vector3f {
        v3(self.fl(k), self.fl(k + 1), self.fl(k + 2))
    }

    pub fn out_s(&self, k: usize) -> &[u8] {
        let (Some(&p), Some(&n)) = (self.s.get(k), self.n.get(k)) else {
            return &[];
        };
        if p.is_null() || n == 0 {
            &[]
        } else {
            // SAFETY: the op returns a string that stays valid until the next op.
            unsafe { std::slice::from_raw_parts(p, n) }
        }
    }

    pub fn go(mut self, op: u32) -> Self {
        let f = OPS.load(Ordering::Acquire);
        if f != 0 {
            // SAFETY: `f` was installed by the host; `self` outlives the call.
            unsafe {
                let f: OpFn = std::mem::transmute::<usize, OpFn>(f);
                self.i[7] = f(op, &raw mut self);
            }
        }
        self
    }

    pub fn ret(&self) -> i32 {
        self.int(7)
    }
}

pub mod op {
    pub const E_POS: u32 = 1;
    pub const E_SCALE_Y: u32 = 2;
    pub const E_FLAGS: u32 = 3;
    pub const E_NAME: u32 = 4;
    pub const E_SET_ACTIVE: u32 = 5;
    pub const E_MESH_SET_MATRIX: u32 = 6;
    pub const E_ANIM_CURRENT: u32 = 7;
    pub const E_PLAY_ANIM: u32 = 8;
    pub const E_ANIM_SET_SPEED: u32 = 9;
    pub const E_ANIM_RESTART_RAND: u32 = 10;
    pub const E_ANIM_LEN: u32 = 11;
    pub const E_ANIM_EXISTS: u32 = 12;
    pub const E_ANIM_CUR_NAME: u32 = 13;
    pub const E_CALL: u32 = 14;
    pub const E_MAP_ENT_COUNT: u32 = 15;
    pub const E_DOOR_HIT: u32 = 16;
    pub const E_ID_COPY: u32 = 17;
    pub const E_MAP_FIND: u32 = 18;
    pub const E_SOCKET_FIND: u32 = 19;
    pub const E_SOCKET_COUNT: u32 = 20;
    pub const E_ATTACK_MATRIX: u32 = 21;
    pub const E_SAME_MAP: u32 = 22;
    pub const E_PARTICLE: u32 = 23;
    pub const E_DOOR_FLAGS: u32 = 24;
    pub const E_SCALE3: u32 = 25;
    pub const E_ONLOAD: u32 = 26;
    pub const E_VARS: u32 = 27;

    pub const B_GET3: u32 = 40;
    pub const B_GETF: u32 = 41;
    pub const B_FLAGS: u32 = 42;
    pub const B_SET_YAW: u32 = 43;
    pub const B_SET3: u32 = 44;
    pub const B_SETF: u32 = 45;
    pub const B_SETB: u32 = 46;
    pub const B_VEL: u32 = 47;
    pub const B_MOVE_FWD: u32 = 48;
    pub const B_CREATE: u32 = 49;
    pub const B_DESTROY: u32 = 50;
    pub const B_BONE_POS: u32 = 51;

    pub const P_INFO: u32 = 60;
    pub const P_VEL: u32 = 61;
    pub const P_DAMAGE: u32 = 62;
    pub const P_FORCE: u32 = 63;

    pub const W_LOS: u32 = 70;
    pub const W_PLAYER_LOS: u32 = 71;
    pub const W_ON_SCREEN: u32 = 72;
    pub const W_FLOOR: u32 = 73;
    pub const W_GROUND: u32 = 74;
    pub const W_RAYCAST: u32 = 75;
    pub const W_AVOID_QUERY: u32 = 76;
    pub const W_AVOID_GET: u32 = 77;
    pub const W_BARK: u32 = 78;
    pub const W_RAND_F: u32 = 79;
    pub const W_RAND_L: u32 = 80;

    pub const ND_GET: u32 = 90;
    pub const ND_PATH: u32 = 91;
    pub const ND_PATH_AT: u32 = 92;
    pub const ND_BY_NAME: u32 = 93;
    pub const ND_COUNT: u32 = 94;
    pub const ND_AT: u32 = 95;
    pub const ND_FREE_PATH: u32 = 96;
    pub const ND_CONTAINER: u32 = 97;
    pub const ND_ITER_NEW: u32 = 98;
    pub const ND_ITER_HAS: u32 = 99;
    pub const ND_ITER_NEXT: u32 = 100;
    pub const ND_CLEAR: u32 = 101;
    pub const ND_NODE_POS: u32 = 102;
    pub const ND_AT_MAP: u32 = 103;

    pub const FF_NEW: u32 = 110;
    pub const FF_DESTROY: u32 = 111;
    pub const FF_SET_ACTIVE: u32 = 112;
    pub const FF_SET: u32 = 113;
    pub const FF_FADE: u32 = 114;
    pub const FF_IS_DEAD: u32 = 115;
    pub const FF_SET_POS: u32 = 116;

    pub const BN_BY_NAME: u32 = 120;
    pub const BN_WORLD_POS: u32 = 121;
    pub const BN_SET_PRE: u32 = 122;
    pub const BN_USE_PRE: u32 = 123;
    pub const M_WORLD_POS: u32 = 124;
    pub const M_ROT_QUAT: u32 = 125;
    pub const M_VISIBLE: u32 = 126;

    pub const G_REGISTER: u32 = 130;
    pub const G_ADD: u32 = 131;

    pub const V_STR: u32 = 140;
    pub const V_FLOAT: u32 = 141;
    pub const V_INT: u32 = 142;
    pub const V_BOOL: u32 = 143;
    pub const V_VEC: u32 = 144;
    pub const ARR_LEN: u32 = 146;
    pub const ARR_AT: u32 = 147;

    pub const U_NEW: u32 = 150;
    pub const U_FREE: u32 = 151;
    pub const U_SCRIPT: u32 = 152;
    pub const U_UPDATE: u32 = 153;
    pub const U_SCRIPT_CALL: u32 = 154;
    pub const U_TIMERS: u32 = 155;
    pub const LOG_ERR: u32 = 160;
    pub const LOG_WARN: u32 = 161;
    pub const DUMP: u32 = 162;
    pub const DUMP_ON: u32 = 163;
}
