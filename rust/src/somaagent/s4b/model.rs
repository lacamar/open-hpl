//! Rust-owned SOMA agent objects: one `Agent` per entity, its components boxed so the pointer a
//! script holds stays valid.

use super::super::agent::AgentState;
use super::super::charmover::{CharMoverState, SpeedState, WallSample};
use super::super::misc::{ProxState, SoundListener};
use super::super::pathfinder::PathfinderState;
use super::super::statemachine::{SmExtra, SmPod};
use crate::math::{Matrixf, Quaternion, Vector3f, to_rad, v3};
use std::cell::Cell;
use std::collections::BTreeMap;
use std::ffi::c_void;

pub const COMP_PATHFINDER: i32 = 1;
pub const COMP_CHAR_MOVER: i32 = 2;
pub const COMP_SOUND_LISTENER: i32 = 3;
pub const COMP_STATE_MACHINE: i32 = 4;
pub const COMP_HEAD_TRACKER: i32 = 5;
pub const COMP_FORCE_EMITTER: i32 = 6;
pub const COMP_BARK_MACHINE: i32 = 7;
pub const COMP_BACKBONE_TAIL: i32 = 8;
pub const COMP_LIGHT_SENSOR: i32 = 9;
pub const COMP_PROXIMITY_SENSOR: i32 = 10;
pub const COMP_ATTACK_DAMAGE: i32 = 11;
pub const COMP_FOOT_PLACEMENT: i32 = 12;
pub const COMP_HAND_PLACEMENT: i32 = 13;
pub const COMP_EDGE_GLOW: i32 = 1000;

#[cfg(not(test))]
use crate::somascript::s2_as::{ohpl_as2_str_assign, ohpl_as2_str_ctor, ohpl_as2_str_dtor};

/// A string scripts read by reference (`const tString&`): the bytes plus a lazily built
/// `std::string` mirror the engine can point at.
#[derive(Default)]
pub struct Tstr {
    v: Vec<u8>,
    #[cfg_attr(test, allow(dead_code))]
    mirror: Cell<usize>,
}

impl Tstr {
    pub fn new(b: &[u8]) -> Self {
        Self {
            v: b.to_vec(),
            mirror: Cell::new(0),
        }
    }

    pub fn get(&self) -> &[u8] {
        &self.v
    }

    pub fn set(&mut self, b: &[u8]) {
        self.v.clear();
        self.v.extend_from_slice(b);
    }

    #[cfg(not(test))]
    pub fn mirror(&self) -> *mut c_void {
        if self.mirror.get() == 0 {
            let mem = Box::into_raw(Box::new([0u64; 4])).cast::<c_void>();
            // SAFETY: 32 zeroed bytes of 8-aligned storage hold a `std::string`.
            unsafe { ohpl_as2_str_ctor(mem, self.v.as_ptr(), self.v.len()) };
            self.mirror.set(mem as usize);
        } else {
            // SAFETY: the mirror is a live `std::string` this value owns.
            unsafe {
                ohpl_as2_str_assign(
                    self.mirror.get() as *mut c_void,
                    self.v.as_ptr(),
                    self.v.len(),
                )
            };
        }
        self.mirror.get() as *mut c_void
    }

    #[cfg(test)]
    pub fn mirror(&self) -> *mut c_void {
        std::ptr::null_mut()
    }
}

impl Drop for Tstr {
    fn drop(&mut self) {
        #[cfg(not(test))]
        if self.mirror.get() != 0 {
            let p = self.mirror.get() as *mut c_void;
            // SAFETY: the mirror was built in `mirror` and is freed once.
            unsafe {
                ohpl_as2_str_dtor(p);
                drop(Box::from_raw(p.cast::<[u64; 4]>()));
            }
        }
    }
}

/// `cLuxEntityMessageData` in the official layout: vec at 12, int at 36, id at 44, string at 56.
pub struct Msg(Box<[u64; 16]>);

impl Msg {
    pub fn new() -> Self {
        #[allow(unused_mut)]
        let mut b = Box::new([0u64; 16]);
        #[cfg(not(test))]
        // SAFETY: bytes 56..88 are zeroed, 8-aligned storage for the `std::string` member.
        unsafe {
            ohpl_as2_str_ctor(b.as_mut_ptr().add(7).cast(), std::ptr::null(), 0);
        }
        Self(b)
    }

    pub fn ptr(&self) -> *mut c_void {
        self.0.as_ptr().cast_mut().cast()
    }

    fn bytes(&mut self) -> &mut [u8] {
        // SAFETY: the block is 128 bytes of plain storage.
        unsafe { std::slice::from_raw_parts_mut(self.0.as_mut_ptr().cast::<u8>(), 128) }
    }

    fn put(&mut self, off: usize, b: &[u8]) {
        if let Some(d) = self.bytes().get_mut(off..off + b.len()) {
            d.copy_from_slice(b);
        }
    }

    pub fn set(&mut self, v: Vector3f, i: i32) {
        for (k, f) in [v.x, v.y, v.z].iter().enumerate() {
            self.put(12 + 4 * k, &f.to_ne_bytes());
        }
        self.put(36, &i.to_ne_bytes());
    }

    pub fn clear_id(&mut self) {
        if let Some(d) = self.bytes().get_mut(44..56) {
            d.fill(0);
        }
    }
}

impl Drop for Msg {
    fn drop(&mut self) {
        #[cfg(not(test))]
        // SAFETY: the string member was constructed in `new`.
        unsafe {
            ohpl_as2_str_dtor(self.0.as_mut_ptr().add(7).cast());
        }
    }
}

pub struct Sm {
    pub pod: SmPod,
    pub x: Box<SmExtra>,
    pub states: BTreeMap<i32, Vec<u8>>,
    pub subs: BTreeMap<i32, Vec<u8>>,
}

impl Sm {
    pub fn new() -> Box<Self> {
        let mut x = Box::new(SmExtra::new());
        let pod = SmPod {
            cur: -1,
            prev: -1,
            next: -1,
            sub_cur: -1,
            sub_prev: -1,
            sub_next: -1,
            in_sub: false,
            rs: &raw mut *x,
        };
        Box::new(Self {
            pod,
            x,
            states: BTreeMap::new(),
            subs: BTreeMap::new(),
        })
    }
}

pub struct Cm {
    pub st: CharMoverState,
    pub body: usize,
    pub idle: Vec<u8>,
    pub walk: Vec<u8>,
    pub run: Vec<u8>,
    pub backward: Vec<u8>,
    pub speeds: BTreeMap<i32, SpeedState>,
    pub edit: i32,
    pub turned_cb: Vec<u8>,
    pub idle_extra: Vec<u8>,
    pub walls: Vec<WallSample>,
}

#[derive(Default)]
pub struct TrackNode {
    pub node: Tstr,
    pub min_wait: f32,
    pub max_wait: f32,
    pub anim: Tstr,
    pub looped: bool,
}

impl Cm {
    pub fn new(body: usize) -> Box<Self> {
        Box::new(Self {
            st: CharMoverState::default(),
            body,
            idle: b"Idle".to_vec(),
            walk: b"Walk".to_vec(),
            run: b"Run".to_vec(),
            backward: Vec::new(),
            speeds: BTreeMap::new(),
            edit: -1,
            turned_cb: Vec::new(),
            idle_extra: Vec::new(),
            walls: Vec::new(),
        })
    }
}

pub struct Pf {
    pub st: PathfinderState,
    pub container: Vec<u8>,
    pub nodes: usize,
    pub max_height: f32,
    pub max_edge: f32,
    pub at_center: bool,
    pub path: Vec<Vector3f>,
    pub result_cb: Vec<u8>,
    pub end_cb: Vec<u8>,
    pub node_array: Vec<usize>,
    pub node_array_dist: Vec<f32>,
    #[allow(clippy::vec_box)] // scripts hold track-node addresses
    pub track: Vec<Box<TrackNode>>,
    pub track_cb: Tstr,
}

impl Pf {
    pub fn new() -> Box<Self> {
        Box::new(Self {
            st: PathfinderState::default(),
            container: Vec::new(),
            nodes: 0,
            max_height: 1.0,
            max_edge: 5.0,
            at_center: false,
            path: Vec::new(),
            result_cb: Vec::new(),
            end_cb: Vec::new(),
            node_array: Vec::new(),
            node_array_dist: Vec::new(),
            track: Vec::new(),
            track_cb: Tstr::default(),
        })
    }
}

#[derive(Default, Clone)]
pub struct BarkState {
    pub sound: Vec<u8>,
    pub min: f32,
    pub max: f32,
}

pub struct Bark {
    pub states: BTreeMap<i32, BarkState>,
    pub edit: i32,
    pub cur: i32,
    pub count: f32,
}

pub struct Fe {
    pub field: usize,
    pub body: usize,
    pub offset: Vector3f,
    pub at_foot: bool,
    pub min_speed: f32,
    pub max_speed: f32,
}

pub struct Bt {
    pub bones: Vec<(usize, f32)>,
    pub trail: Vec<(Quaternion, f32)>,
    pub count: usize,
    pub head: usize,
    pub length: f32,
    pub timer: f32,
    pub interval: f32,
    pub stiffness: f32,
    pub last_pos: Vector3f,
    pub last: Quaternion,
}

pub struct Prox {
    pub states: Vec<ProxState>,
    pub timer: f32,
}

#[derive(Clone)]
pub struct Shape {
    pub socket: i32,
    pub offset: Vector3f,
    pub radius: f32,
}

#[derive(Clone)]
pub struct AtkType {
    pub exists: bool,
    pub active: bool,
    pub duration: f32,
    pub freq: f32,
    pub timer: f32,
    pub instance: i32,
    pub force: f32,
    pub max_impulse: f32,
    pub min: f32,
    pub max: f32,
    pub damage_type: i32,
    pub player: bool,
    pub lethal: bool,
    pub shapes: Vec<Shape>,
}

impl Default for AtkType {
    fn default() -> Self {
        Self {
            exists: false,
            active: false,
            duration: 0.0,
            freq: 0.0,
            timer: 0.0,
            instance: -1,
            force: 100.0,
            max_impulse: 10.0,
            min: 1.0,
            max: 1.0,
            damage_type: 0,
            player: true,
            lethal: true,
            shapes: Vec::new(),
        }
    }
}

pub struct Atk {
    pub types: Vec<AtkType>,
    pub cur: i32,
}

pub struct Glow {
    pub color: [f32; 4],
    pub thickness: f32,
    pub alpha: f32,
    pub light_limit: f32,
}

pub enum K {
    Generic,
    User(usize),
    Sm(Box<Sm>),
    Cm(Box<Cm>),
    Pf(Box<Pf>),
    Sl(SoundListener),
    Ht { track: usize, max_angle: f32 },
    Fe(Fe),
    Bark(Bark),
    Bt(Box<Bt>),
    Ls { sensitivity: f32 },
    Prox(Prox),
    Atk(Atk),
    Glow(Glow),
}

pub struct Comp {
    pub ent: usize,
    pub ty: i32,
    pub name: Tstr,
    pub active: bool,
    pub foot: [bool; 4],
    pub k: K,
}

impl Comp {
    pub fn new(ent: usize, ty: i32, k: K) -> Box<Self> {
        Box::new(Self {
            ent,
            ty,
            name: Tstr::default(),
            active: !matches!(k, K::Ht { .. }),
            foot: [true; 4],
            k,
        })
    }
}

pub struct Agent {
    pub ent: usize,
    pub body: usize,
    pub pos_bone: usize,
    pub st: AgentState,
    pub mesh_offset: Matrixf,
    pub msg: Msg,
    pub message_cb: Vec<u8>,
    pub auto_disable_cb: Vec<u8>,
    #[allow(clippy::vec_box)] // scripts hold component addresses
    pub comps: Vec<Box<Comp>>,
}

impl Agent {
    pub fn new(ent: usize) -> Box<Self> {
        let st = AgentState {
            fov: to_rad(90.0),
            ..AgentState::default()
        };
        Box::new(Self {
            ent,
            body: 0,
            pos_bone: 0,
            st,
            mesh_offset: crate::math::IDENTITY,
            msg: Msg::new(),
            message_cb: Vec::new(),
            auto_disable_cb: Vec::new(),
            comps: Vec::new(),
        })
    }

    pub fn find_ptr(&self, ty: i32) -> *mut Comp {
        self.comps
            .iter()
            .find(|c| c.ty == ty)
            .map_or(std::ptr::null_mut(), |c| {
                std::ptr::from_ref::<Comp>(c).cast_mut()
            })
    }
}

pub fn zero3() -> Vector3f {
    v3(0.0, 0.0, 0.0)
}
