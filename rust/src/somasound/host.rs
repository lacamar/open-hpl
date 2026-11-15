//! The C++ side of the sound layer: engine objects (sound handler, music handler, resources, world,
//! sound entities, map) reached through one table of thunks installed at startup
//! (`soma/src/game/SomaSoundHost.cpp`).

use super::instance::{EntityInfo, Filter, Host, Listener, PlayReq};
use std::ffi::c_void;
use std::sync::OnceLock;

#[repr(C)]
pub struct PlayReqC {
    pub kind: u8,
    pub file: *const u8,
    pub file_len: usize,
    pub looped: bool,
    pub pos: [f32; 3],
    pub entry_type: i32,
    pub stream: bool,
}

pub type Sink = unsafe extern "C" fn(*mut c_void, *const u8, usize);

#[repr(C)]
pub struct S7Host {
    pub valid: unsafe extern "C" fn(usize, i32) -> bool,
    pub play: unsafe extern "C" fn(*const PlayReqC, *mut usize, *mut i32) -> bool,
    pub stop: unsafe extern "C" fn(usize),
    pub pause: unsafe extern "C" fn(usize, bool),
    pub total_time: unsafe extern "C" fn(usize) -> f64,
    pub set_looping: unsafe extern "C" fn(usize, bool),
    pub set_filter: unsafe extern "C" fn(usize, i32, f32),
    pub set_position: unsafe extern "C" fn(usize, *const f32),
    pub set_default_volume: unsafe extern "C" fn(usize, f32),
    pub set_default_speed: unsafe extern "C" fn(usize, f32),
    pub listener: unsafe extern "C" fn(*mut f32),
    pub entity: unsafe extern "C" fn(usize, *mut f32),
    pub set_global_volume: unsafe extern "C" fn(f32, u32, i32) -> i32,
    pub global_volume_of: unsafe extern "C" fn(i32) -> f32,
    pub fade_global_volume: unsafe extern "C" fn(f32, f32, u32, i32, bool) -> i32,
    pub set_global_speed: unsafe extern "C" fn(f32, u32, i32) -> i32,
    pub global_speed_of: unsafe extern "C" fn(i32) -> f32,
    pub fade_global_speed: unsafe extern "C" fn(f32, f32, u32, i32, bool) -> i32,
    pub fade_out_all: unsafe extern "C" fn(u32, f32),
    pub music_play: unsafe extern "C" fn(*const u8, usize, f32, f32, bool, bool),
    pub music_stop: unsafe extern "C" fn(f32),
    pub music_has_song: unsafe extern "C" fn() -> bool,
    pub music_fade_volume_mul: unsafe extern "C" fn(f32, f32),
    pub music_volume_mul: unsafe extern "C" fn() -> f32,
    pub entity_active: unsafe extern "C" fn(u8, i32, i32) -> bool,
    pub find_file: unsafe extern "C" fn(*const u8, usize, Sink, *mut c_void) -> bool,
    pub add_resource_dir: unsafe extern "C" fn(*const u8, usize),
    pub data_new: unsafe extern "C" fn(*const u8, usize) -> usize,
    pub data_add_sound: unsafe extern "C" fn(usize, *const u8, usize),
    pub data_setup: unsafe extern "C" fn(usize, f32, f32, bool, bool),
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
    pub se_event: unsafe extern "C" fn(usize) -> usize,
    pub se_fade: unsafe extern "C" fn(usize, bool, f32),
    pub se_main_fade_in: unsafe extern "C" fn(usize, f32, f32),
    pub se_data_loop: unsafe extern "C" fn(usize) -> i32,
    pub se_prop: unsafe extern "C" fn(usize, u32, f32) -> f32,
    pub se_reset_props: unsafe extern "C" fn(usize),
    pub log: unsafe extern "C" fn(*const u8, usize),
    pub warning: unsafe extern "C" fn(*const u8, usize),
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

/// The sound handler as the instance code sees it.
pub struct Engine;

// SAFETY (all calls below): the installed table's thunks accept any entry value the handler gave out.
impl Host for Engine {
    fn valid(&mut self, entry: usize, id: i32) -> bool {
        ops().is_some_and(|h| unsafe { (h.valid)(entry, id) })
    }

    fn play(&mut self, req: &PlayReq) -> Option<(usize, i32)> {
        let h = ops()?;
        let r = PlayReqC {
            kind: req.kind,
            file: req.file.as_ptr(),
            file_len: req.file.len(),
            looped: req.looped,
            pos: req.pos,
            entry_type: req.entry_type,
            stream: req.stream,
        };
        let (mut entry, mut id) = (0usize, 0i32);
        unsafe { (h.play)(&r, &mut entry, &mut id) }.then_some((entry, id))
    }

    fn stop(&mut self, entry: usize) {
        if let Some(h) = ops() {
            unsafe { (h.stop)(entry) }
        }
    }

    fn pause(&mut self, entry: usize, on: bool) {
        if let Some(h) = ops() {
            unsafe { (h.pause)(entry, on) }
        }
    }

    fn total_time(&mut self, entry: usize) -> f64 {
        ops().map_or(0.0, |h| unsafe { (h.total_time)(entry) })
    }

    fn set_looping(&mut self, entry: usize, on: bool) {
        if let Some(h) = ops() {
            unsafe { (h.set_looping)(entry, on) }
        }
    }

    fn set_filter(&mut self, entry: usize, which: Filter, gain: f32) {
        if let Some(h) = ops() {
            unsafe { (h.set_filter)(entry, which as i32, gain) }
        }
    }

    fn set_position(&mut self, entry: usize, pos: [f32; 3]) {
        if let Some(h) = ops() {
            unsafe { (h.set_position)(entry, pos.as_ptr()) }
        }
    }

    fn set_default_volume(&mut self, entry: usize, vol: f32) {
        if let Some(h) = ops() {
            unsafe { (h.set_default_volume)(entry, vol) }
        }
    }

    fn set_default_speed(&mut self, entry: usize, speed: f32) {
        if let Some(h) = ops() {
            unsafe { (h.set_default_speed)(entry, speed) }
        }
    }

    fn listener(&mut self) -> Listener {
        let mut b = [0.0f32; 9];
        if let Some(h) = ops() {
            unsafe { (h.listener)(b.as_mut_ptr()) };
        }
        Listener {
            pos: [b[0], b[1], b[2]],
            fwd: [b[3], b[4], b[5]],
            up: [b[6], b[7], b[8]],
        }
    }

    fn entity(&mut self, entity: usize) -> EntityInfo {
        let mut b = [0.0f32; 6];
        if let Some(h) = ops() {
            unsafe { (h.entity)(entity, b.as_mut_ptr()) };
        }
        EntityInfo {
            world_pos: [b[0], b[1], b[2]],
            min_dist: b[3],
            max_dist: b[4],
            volume: b[5],
        }
    }
}
