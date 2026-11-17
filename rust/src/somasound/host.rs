//! The C++ side of the sound layer: engine objects (sound handler, music handler, resources, world,
//! sound entities, map) reached through one table of thunks installed at startup
//! (`soma/src/game/SomaSoundHost.cpp`).

use super::instance::{EntityInfo, Filter, Host, Listener, PlayReq};
use crate::sound::own::{PlayArgs, Rec, entity_of, handler};
use crate::types::Vector3f;
use std::ffi::c_void;
use std::sync::OnceLock;

pub type Sink = unsafe extern "C" fn(*mut c_void, *const u8, usize);

#[repr(C)]
pub struct S7Host {
    pub entity_active: unsafe extern "C" fn(u8, i32, i32) -> bool,
    pub find_file: unsafe extern "C" fn(*const u8, usize, Sink, *mut c_void) -> bool,
    pub add_resource_dir: unsafe extern "C" fn(*const u8, usize),
    pub data_new: unsafe extern "C" fn(*const u8, usize) -> usize,
    pub install_updater: unsafe extern "C" fn(),
    pub install_hooks: unsafe extern "C" fn(),
    pub install_resolver: unsafe extern "C" fn(),
    pub shell_string: unsafe extern "C" fn(usize, u32, *const u8, usize) -> *const c_void,
    pub shell_position_ref: unsafe extern "C" fn(usize) -> *const c_void,
    pub shell_new: unsafe extern "C" fn(usize, usize, i32) -> usize,
    pub shell_delete: unsafe extern "C" fn(usize),
    pub shell_handle: unsafe extern "C" fn(usize) -> usize,
    pub shell_set_position: unsafe extern "C" fn(usize, *const f32),
    pub world_create_sound:
        unsafe extern "C" fn(usize, *const u8, usize, *const u8, usize, bool) -> usize,
    pub world_sound_by_creation: unsafe extern "C" fn(usize, i32) -> usize,
    pub object_id: unsafe extern "C" fn(usize, *mut u8),
    pub log: unsafe extern "C" fn(*const u8, usize),
    pub warning: unsafe extern "C" fn(*const u8, usize),
    pub scape_world: unsafe extern "C" fn(usize) -> usize,
    pub scape_areas: unsafe extern "C" fn(
        usize,
        unsafe extern "C" fn(*mut c_void, *const AreaDefC),
        *mut c_void,
    ),
    pub world_sound_by_name: unsafe extern "C" fn(usize, *const u8, usize) -> usize,
    pub area_state: unsafe extern "C" fn(usize, *mut bool, *mut f32),
    pub camera_pos: unsafe extern "C" fn(*mut f32) -> bool,
    pub body_pos: unsafe extern "C" fn(*mut f32) -> bool,
    pub apply_reverb: unsafe extern "C" fn(*const f32),
    pub ent_name: unsafe extern "C" fn(usize, Sink, *mut c_void),
    pub prefix_ref: unsafe extern "C" fn(*const u8, usize) -> *const c_void,
    pub script_call: unsafe extern "C" fn(*const u8, usize, *const CallArg, usize, bool) -> i32,
}

/// One argument of a map script call: kind 0 string, 1 bool, 2 int.
#[repr(C)]
pub struct CallArg {
    pub kind: u8,
    pub i: i32,
    pub s: Str,
}

#[repr(C)]
pub struct Str {
    pub p: *const u8,
    pub n: usize,
}

#[repr(C)]
pub struct AreaDefC {
    pub ent: usize,
    pub level: Str,
    pub prio: i32,
    pub bg: Str,
    pub bg_volume: f32,
    pub bg_fade_in: f32,
    pub bg_trans_speed: f32,
    pub parent_vol_mul: f32,
    pub in_names: Str,
    pub out_names: Str,
    pub ent_fade_in: f32,
    pub ent_fade_out: f32,
    pub prefix: Str,
    pub prefix_prio: i32,
    pub use_reverb: bool,
    pub reverb_prio: i32,
    pub reverb_type: Str,
    pub reverb_amount: f32,
    pub reverb_fade: f32,
}

static HOST: OnceLock<&'static S7Host> = OnceLock::new();

/// # Safety
/// `ops` points at a table that lives for the whole process.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s7_install_host(ops: *const S7Host) {
    // SAFETY: caller contract.
    if let Some(o) = unsafe { ops.as_ref() } {
        let _ = HOST.set(o);
    }
}

pub fn ops() -> Option<&'static S7Host> {
    HOST.get().copied()
}

pub fn log(text: &[u8]) {
    if let Some(h) = ops() {
        // SAFETY: `text` is valid for the call.
        unsafe { (h.log)(text.as_ptr(), text.len()) };
    }
}

pub fn warning(text: &[u8]) {
    if let Some(h) = ops() {
        // SAFETY: `text` is valid for the call.
        unsafe { (h.warning)(text.as_ptr(), text.len()) };
    }
}

pub fn find_file(name: &[u8]) -> Option<Vec<u8>> {
    unsafe extern "C" fn sink(ctx: *mut c_void, p: *const u8, n: usize) {
        // SAFETY: `ctx` is the `Vec` below; the host passes `n` valid bytes.
        unsafe { (*ctx.cast::<Vec<u8>>()).extend_from_slice(std::slice::from_raw_parts(p, n)) }
    }
    let h = ops()?;
    let mut v = Vec::new();
    // SAFETY: `v` outlives the call.
    unsafe {
        (h.find_file)(
            name.as_ptr(),
            name.len(),
            sink,
            std::ptr::from_mut(&mut v).cast(),
        )
    }
    .then_some(v)
    .filter(|v| !v.is_empty())
}

/// The sound handler as the instance code sees it: the owned handler, called directly.
pub struct Engine;

fn rec<'a>(entry: usize) -> Option<&'a mut Rec> {
    // SAFETY: `entry` is a `cSoundEntry*` the handler gave out (a `Rec` address) and checked with `valid`.
    unsafe { (entry as *mut Rec).as_mut() }
}

impl Host for Engine {
    fn valid(&mut self, entry: usize, id: i32) -> bool {
        handler().is_some_and(|h| h.is_valid(entry as *const Rec, id))
    }

    fn play(&mut self, req: &PlayReq) -> Option<(usize, i32)> {
        let h = handler()?;
        // kind 0 plays in the world, 1 a GUI stream, otherwise a GUI sound
        let (relative, is_3d, min_dist, max_dist, stream) = match req.kind {
            0 => (false, true, 1e5, 2e5, req.stream),
            1 => (true, false, 1.0, 1000.0, true),
            _ => (true, false, 1.0, 1000.0, false),
        };
        let args = PlayArgs {
            name: req.file,
            looping: req.looped,
            volume: 0.0,
            pos: Vector3f {
                x: req.pos[0],
                y: req.pos[1],
                z: req.pos[2],
            },
            min_dist,
            max_dist,
            entry_type: req.entry_type as u32,
            relative,
            is_3d,
            priority_modifier: 0,
            stream,
        };
        let p = h.play(&args, &mut false);
        rec(p as usize).map(|r| (p as usize, r.id))
    }

    fn stop(&mut self, entry: usize) {
        if let (Some(h), Some(r)) = (handler(), rec(entry)) {
            h.stop_rec(r);
        }
    }

    fn pause(&mut self, entry: usize, on: bool) {
        if let Some(r) = rec(entry) {
            r.set_paused(on);
        }
    }

    fn total_time(&mut self, entry: usize) -> f64 {
        rec(entry).map_or(0.0, |r| r.total_time())
    }

    fn set_looping(&mut self, entry: usize, on: bool) {
        if let Some(r) = rec(entry) {
            r.set_looping(on);
        }
    }

    fn set_filter(&mut self, entry: usize, which: Filter, gain: f32) {
        if let Some(r) = rec(entry) {
            r.set_filter(which as i32, gain);
        }
    }

    fn set_position(&mut self, entry: usize, pos: [f32; 3]) {
        if let Some(r) = rec(entry) {
            r.set_position(Vector3f {
                x: pos[0],
                y: pos[1],
                z: pos[2],
            });
        }
    }

    fn set_default_volume(&mut self, entry: usize, vol: f32) {
        if let Some(r) = rec(entry) {
            r.state.normal_volume = vol;
        }
    }

    fn set_default_speed(&mut self, entry: usize, speed: f32) {
        if let Some(r) = rec(entry) {
            r.state.normal_speed = speed;
        }
    }

    fn listener(&mut self) -> Listener {
        let (pos, fwd, up) = handler().map_or_else(Default::default, |h| h.listener());
        Listener { pos, fwd, up }
    }

    fn entity(&mut self, entity: usize) -> EntityInfo {
        // SAFETY: `entity` is a live `cSoundEntity*` registered with the sound layer.
        let Some(e) = (unsafe { entity_of(entity).as_ref() }) else {
            return EntityInfo::default();
        };
        let p = e.world_position();
        EntityInfo {
            world_pos: [p.x, p.y, p.z],
            min_dist: e.min_distance,
            max_dist: e.max_distance,
            volume: e.volume,
        }
    }
}
