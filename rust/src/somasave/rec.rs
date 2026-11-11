//! C-layout records of the save world, shared by the encoder (filled by C++), the decoder
//! (filled here, handed to C++ callbacks) and `RustSomaSave.h`. Pointers are borrowed for the
//! duration of the call that carries the record.

pub type Mat = [f32; 16];
pub type Vec3 = [f32; 3];
pub type Col = [f32; 4];

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Str {
    pub p: *const u8,
    pub n: usize,
}

impl Str {
    pub const EMPTY: Str = Str {
        p: std::ptr::null(),
        n: 0,
    };

    pub fn new(b: &[u8]) -> Str {
        Str {
            p: b.as_ptr(),
            n: b.len(),
        }
    }

    pub fn get(&self) -> &[u8] {
        if self.n == 0 || self.p.is_null() {
            return &[];
        }
        // SAFETY: the record's creator keeps `p..p+n` alive while the record is in use.
        unsafe { std::slice::from_raw_parts(self.p, self.n) }
    }
}

#[repr(C)]
pub struct Slice<T> {
    pub p: *const T,
    pub n: usize,
}

impl<T> Clone for Slice<T> {
    fn clone(&self) -> Self {
        *self
    }
}
impl<T> Copy for Slice<T> {}

impl<T> Slice<T> {
    pub const EMPTY: Slice<T> = Slice {
        p: std::ptr::null(),
        n: 0,
    };

    pub fn new(v: &[T]) -> Slice<T> {
        Slice {
            p: v.as_ptr(),
            n: v.len(),
        }
    }

    pub fn get(&self) -> &[T] {
        if self.n == 0 || self.p.is_null() {
            return &[];
        }
        // SAFETY: the record's creator keeps `p..p+n` alive while the record is in use.
        unsafe { std::slice::from_raw_parts(self.p, self.n) }
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Player {
    pub present: u8,
    pub char_crouching: i32,
    pub feet: Vec3,
    pub yaw: f32,
    pub pitch: f32,
    pub state: i32,
    pub move_state: i32,
    pub gravity_active: i32,
    pub gravity: Vec3,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct MapTimer {
    pub name: Str,
    pub func: Str,
    pub user_str: Str,
    pub time: f32,
    pub user_float: f32,
    pub length: f32,
    pub user_int: i32,
    pub paused: bool,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Spawned {
    pub name: Str,
    pub file: Str,
    pub mtx: Mat,
    pub scale: Vec3,
    pub id: [u8; 12],
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Collide {
    pub child: Str,
    pub func: Str,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Body {
    pub mtx: Mat,
    pub mass: f32,
    pub active: bool,
    pub gravity: bool,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Attach {
    pub parent: Str,
    pub body: Str,
    pub socket: Str,
    pub parent_prev: Mat,
    pub offset: Mat,
    pub use_rotation: bool,
    pub locked: bool,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Joint {
    pub name: Str,
    pub limits: [f32; 2],
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Timer {
    pub func: Str,
    pub id: u64,
    pub length: f32,
    pub time_left: f32,
    pub repeat: bool,
    pub paused: bool,
}

pub type Fader = [u8; 32];
pub type ImTimer = [u8; 16];
pub type ImState = [u8; 56];

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Timers {
    pub timers: Slice<Timer>,
    pub faders: Slice<Fader>,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct VarPair {
    pub key: Str,
    pub value: Str,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct ImStatePair {
    pub id: u64,
    pub state: ImState,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Anim {
    pub name: Str,
    pub time: f32,
    pub weight: f32,
    pub speed: f32,
    pub fade: f32,
    pub active: bool,
    pub looping: bool,
    pub paused: bool,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Conn {
    pub name: Str,
    pub entity: Str,
    pub states_used: i32,
    pub invert: bool,
}

/// `WriteEntity` / `ReadEntity` up to and including the timers.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EntityHead {
    pub name: Str,
    pub parent_name: Str,
    pub interact_cb: Str,
    pub look_cb: Str,
    pub collide: Slice<Collide>,
    pub bodies: Slice<Body>,
    pub dyn_mass: Slice<f32>,
    pub joints: Slice<Joint>,
    pub timers: Timers,
    pub attach: Attach,
    pub mtx: Mat,
    pub parent_id: [u8; 12],
    pub parent_type: i32,
    pub look_max: f32,
    pub look_delay: f32,
    pub active: bool,
    pub interaction_disabled: bool,
    pub interacted_with: bool,
    pub effects_active: bool,
    pub interact_auto_remove: bool,
    pub look_auto_remove: bool,
    pub look_center: bool,
    pub look_ray: bool,
    pub static_physics: bool,
    pub has_attach: bool,
    pub has_mtx: bool,
}

/// `WriteEntity` / `ReadEntity` after the script.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EntityTail {
    pub vars: Slice<VarPair>,
    pub on_gui: Str,
    pub imgui_timers: Slice<ImTimer>,
    pub imgui_states: Slice<ImStatePair>,
    pub anims: Slice<Anim>,
    pub conn_cb: Str,
    pub conns: Slice<Conn>,
    pub gui_active: bool,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Light {
    pub name: Str,
    pub color: Col,
    pub radius: f32,
    pub brightness: f32,
    pub active: bool,
    pub visible: bool,
    pub flicker: bool,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Particle {
    pub name: Str,
    pub data: Str,
    pub size: Vec3,
    pub mtx: Mat,
    pub color: Col,
    pub brightness: f32,
    pub fade: [f32; 4],
    pub dead: u32,
    pub active: bool,
    pub visible: bool,
    pub fade_at_distance: bool,
    pub owned: bool,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Sound {
    pub name: Str,
    pub data: Str,
    pub params: Slice<f32>,
    pub pos: Vec3,
    pub min_dist: f32,
    pub max_dist: f32,
    pub volume: f32,
    pub fade: f32,
    pub volume_mul: f32,
    pub remove: bool,
    pub active: bool,
    pub stopped: bool,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Billboard {
    pub name: Str,
    pub color: Col,
    pub visible: bool,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Agent {
    pub name: Str,
    pub path: Str,
    pub mtx: Mat,
    pub state: i32,
    pub yaw: f32,
    pub senses: bool,
    pub detection: bool,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Fog {
    pub colors: [Col; 3],
    pub floats: [f32; 9],
    pub flags: [bool; 5],
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct CamTex {
    pub name: Str,
    pub attached: Str,
    pub rot: Mat,
    pub pos: Vec3,
    pub size: [i32; 2],
    pub fps: u32,
    pub fov: f32,
    pub near: f32,
    pub far: f32,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct FogArea {
    pub name: Str,
    pub visible: bool,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Colliding {
    pub parent: Str,
    pub child: Str,
    pub func: Str,
}

pub type FadeValue = [u8; 20];

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Camera {
    pub pos: Vec3,
    pub limits: [f32; 4],
    pub fov_mul: FadeValue,
    pub aspect_mul: FadeValue,
    pub detached: u8,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct OwnedLight {
    pub name: Str,
    pub color: Col,
    pub radius: f32,
    pub visible: bool,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Tone {
    pub exposure: f32,
    pub white_cut: f32,
}

/// Everything of `WriteWorld` that C++ can gather up front; entities and other scriptables are
/// pulled one at a time through the host.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct World {
    pub player: Player,
    pub map_timers: Slice<MapTimer>,
    pub spawned: Slice<Spawned>,
    pub lights: Slice<Light>,
    pub particles: Slice<Particle>,
    pub sounds: Slice<Sound>,
    pub billboards: Slice<Billboard>,
    pub agents: Slice<Agent>,
    pub cam_textures: Slice<CamTex>,
    pub fog_areas: Slice<FogArea>,
    pub colliding: Slice<Colliding>,
    pub owned_lights: Slice<OwnedLight>,
    pub display_name: Str,
    pub fog: Fog,
    pub camera: Camera,
    pub tone: Tone,
    pub health: f32,
    pub entity_count: usize,
    pub other_count: usize,
    pub underwater: bool,
    pub has_camera: bool,
    pub has_tone: bool,
}
