//! Host call table of the map family (A13a): one `op` entry `LuxMapHost.cpp` implements for every engine
//! object the map helper, map and map handler still reach (physics, world, renderables, player, scripts, sound).
//! Calls may re-enter Rust, so no reference to map state is held across [`call`].

use std::sync::atomic::{AtomicUsize, Ordering};

#[repr(C)]
#[derive(Clone, Copy)]
pub struct S {
    pub p: *const u8,
    pub n: usize,
}

impl S {
    pub const NONE: S = S {
        p: std::ptr::null(),
        n: 0,
    };

    pub fn of(s: &[u8]) -> S {
        S {
            p: s.as_ptr(),
            n: s.len(),
        }
    }
}

/// Argument and result block of one host op; unused slots stay zero.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct A {
    pub p: [usize; 6],
    pub i: [i32; 4],
    pub f: [f32; 12],
    pub m: [f32; 16],
    pub s: [S; 2],
    pub r: usize,
    pub rs: [S; 2],
}

impl A {
    pub fn new() -> A {
        A {
            p: [0; 6],
            i: [0; 4],
            f: [0.0; 12],
            m: [0.0; 16],
            s: [S::NONE; 2],
            r: 0,
            rs: [S::NONE; 2],
        }
    }

    pub fn ret_bytes(&self, k: usize) -> Vec<u8> {
        self.rs
            .get(k)
            .map_or_else(Vec::new, |s| slice_of(s.p, s.n).to_vec())
    }
}

impl Default for A {
    fn default() -> A {
        A::new()
    }
}

pub(crate) fn slice_of<'a, T>(p: *const T, n: usize) -> &'a [T] {
    if p.is_null() || n == 0 {
        return &[];
    }
    // SAFETY: the host returns a live `n`-unit buffer valid until the next op.
    unsafe { std::slice::from_raw_parts(p, n) }
}

pub type OpFn = unsafe extern "C" fn(i32, *mut A);

static HOST: AtomicUsize = AtomicUsize::new(0);

pub fn set_host(f: Option<OpFn>) {
    HOST.store(f.map_or(0, |f| f as usize), Ordering::Release);
}

pub fn call(op: i32, a: &mut A) {
    let h = HOST.load(Ordering::Acquire);
    if h == 0 {
        return;
    }
    // SAFETY: `h` is the function pointer stored by `set_host`.
    let f = unsafe { std::mem::transmute::<usize, OpFn>(h) };
    // SAFETY: `a` is a live argument block.
    unsafe { f(op, a) }
}

pub mod op {
    pub const CUR_MAP: i32 = 1;
    pub const RAY_CAST: i32 = 2;
    pub const BODY_INFO: i32 = 3;
    pub const BODY_SUBMESHES: i32 = 4;
    pub const ENT_INFO: i32 = 5;
    pub const ENT_CAN_INTERACT: i32 = 6;
    pub const BODY_POS: i32 = 7;
    pub const SHAPE_BODIES: i32 = 8;
    pub const SHAPE_COLLIDE: i32 = 9;
    pub const BODY_IMPULSE: i32 = 10;
    pub const BODY_PUSH: i32 = 11;
    pub const ENEMY_HIT: i32 = 12;
    pub const BODY_HIT: i32 = 13;
    pub const FX_AT: i32 = 14;
    pub const ENT_DAMAGE: i32 = 15;
    pub const PLAYER_DAMAGE: i32 = 16;
    pub const LIGHT_ROOT: i32 = 17;
    pub const NODE_INFO: i32 = 18;
    pub const LIGHT_TEST: i32 = 19;
    pub const LIGHT_PROPS: i32 = 20;
    pub const PLAYER_AMB_LIGHT: i32 = 21;
    pub const SET_LOADING: i32 = 22;
    pub const LOAD_CFG: i32 = 23;
    pub const FILE_EXISTS: i32 = 24;
    pub const SCRIPT_CREATE: i32 = 25;
    pub const SCRIPT_DESTROY: i32 = 26;
    pub const SCRIPT_RUN: i32 = 27;
    pub const WORLD_LOAD: i32 = 28;
    pub const WORLD_DESTROY: i32 = 29;
    pub const LOG: i32 = 30;
    pub const HARD_MODE: i32 = 31;
    pub const ENT_KIND: i32 = 32;
    pub const ENT_CALL: i32 = 33;
    pub const ENT_DELETE: i32 = 34;
    pub const ENEMY: i32 = 35;
    pub const DOOR: i32 = 36;
    pub const STICKY: i32 = 37;
    pub const PROP_BODY: i32 = 38;
    pub const COLLIDER: i32 = 39;
    pub const BODY_PAIR: i32 = 40;
    pub const PLAYER: i32 = 41;
    pub const FADE_IN: i32 = 42;
    pub const MUSIC: i32 = 43;
    pub const COMPLETION_SHOW: i32 = 44;
    pub const QUEST_VALUE: i32 = 45;
    pub const DISSOLVE_NEW: i32 = 46;
    pub const DISSOLVE_ALPHA: i32 = 47;
    pub const LAMP_NEW: i32 = 48;
    pub const LAMP_ADD: i32 = 49;
    pub const LAMP_UPDATE: i32 = 50;
    pub const WORLD_CREATE_ENTITY: i32 = 51;
    pub const LOADING_MAP: i32 = 52;
    pub const IS_CURRENT_MAP: i32 = 53;
    pub const UPDATE_HOOKS: i32 = 54;
    pub const SHOW_ENTITY_INFO: i32 = 55;
    pub const SHOW_COMMENTARY: i32 = 56;
}

pub mod body {
    pub const IS_CHAR: i32 = 1;
    pub const COLLIDE: i32 = 2;
    pub const BLOCKS_LIGHT: i32 = 4;
    pub const COLLIDE_CHAR: i32 = 8;
    pub const ACTIVE: i32 = 16;
    pub const PLAYER_BODY: i32 = 32;
}

/// Tests share the one global host slot.
#[cfg(test)]
pub(crate) static TEST_LOCK: std::sync::Mutex<()> = std::sync::Mutex::new(());
