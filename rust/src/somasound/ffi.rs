//! C ABI of the SOMA sound layer, mirrored in `soma/src/game/RustSomaSound.h`.

use super::events::Events;
use super::instance::{Bank, EntityInfo, Filter, Host, Listener, PlayReq, State};
use crate::string_ffi::units;
use std::collections::BTreeMap;
use std::ffi::c_void;

const _: () = {
    use std::mem::{offset_of, size_of};
    assert!(
        size_of::<State>() == 104
            && offset_of!(State, fade_dest) == 76
            && offset_of!(State, volume_mul_dest) == 52
    );
};

#[repr(C)]
pub struct OhplGs5PlayReq {
    pub kind: u8,
    pub file: *const u8,
    pub file_len: usize,
    pub looped: bool,
    pub pos: [f32; 3],
    pub entry_type: i32,
    pub stream: bool,
}

type Ctx = *mut c_void;

#[repr(C)]
pub struct OhplGs5Host {
    pub ctx: Ctx,
    pub valid: unsafe extern "C" fn(Ctx, usize, i32) -> bool,
    pub play: unsafe extern "C" fn(Ctx, *const OhplGs5PlayReq, *mut usize, *mut i32) -> bool,
    pub stop: unsafe extern "C" fn(Ctx, usize),
    pub pause: unsafe extern "C" fn(Ctx, usize, bool),
    pub total_time: unsafe extern "C" fn(Ctx, usize) -> f64,
    pub set_looping: unsafe extern "C" fn(Ctx, usize, bool),
    pub set_filter: unsafe extern "C" fn(Ctx, usize, i32, f32),
    pub set_position: unsafe extern "C" fn(Ctx, usize, *const f32),
    pub set_default_volume: unsafe extern "C" fn(Ctx, usize, f32),
    pub set_default_speed: unsafe extern "C" fn(Ctx, usize, f32),
    pub listener: unsafe extern "C" fn(Ctx, *mut f32),
    pub entity: unsafe extern "C" fn(Ctx, usize, *mut f32),
}

struct CHost<'a>(&'a OhplGs5Host);

// SAFETY (all calls below): the caller of the `ohpl_gs5_*` entry point guarantees the table's
// callbacks accept its `ctx`.
impl Host for CHost<'_> {
    fn valid(&mut self, entry: usize, id: i32) -> bool {
        unsafe { (self.0.valid)(self.0.ctx, entry, id) }
    }

    fn play(&mut self, req: &PlayReq) -> Option<(usize, i32)> {
        let r = OhplGs5PlayReq {
            kind: req.kind,
            file: req.file.as_ptr(),
            file_len: req.file.len(),
            looped: req.looped,
            pos: req.pos,
            entry_type: req.entry_type,
            stream: req.stream,
        };
        let (mut entry, mut id) = (0usize, 0i32);
        unsafe { (self.0.play)(self.0.ctx, &r, &mut entry, &mut id) }.then_some((entry, id))
    }

    fn stop(&mut self, entry: usize) {
        unsafe { (self.0.stop)(self.0.ctx, entry) }
    }

    fn pause(&mut self, entry: usize, on: bool) {
        unsafe { (self.0.pause)(self.0.ctx, entry, on) }
    }

    fn total_time(&mut self, entry: usize) -> f64 {
        unsafe { (self.0.total_time)(self.0.ctx, entry) }
    }

    fn set_looping(&mut self, entry: usize, on: bool) {
        unsafe { (self.0.set_looping)(self.0.ctx, entry, on) }
    }

    fn set_filter(&mut self, entry: usize, which: Filter, gain: f32) {
        unsafe { (self.0.set_filter)(self.0.ctx, entry, which as i32, gain) }
    }

    fn set_position(&mut self, entry: usize, pos: [f32; 3]) {
        unsafe { (self.0.set_position)(self.0.ctx, entry, pos.as_ptr()) }
    }

    fn set_default_volume(&mut self, entry: usize, vol: f32) {
        unsafe { (self.0.set_default_volume)(self.0.ctx, entry, vol) }
    }

    fn set_default_speed(&mut self, entry: usize, speed: f32) {
        unsafe { (self.0.set_default_speed)(self.0.ctx, entry, speed) }
    }

    fn listener(&mut self) -> Listener {
        let mut b = [0.0f32; 9];
        unsafe { (self.0.listener)(self.0.ctx, b.as_mut_ptr()) };
        Listener {
            pos: [b[0], b[1], b[2]],
            fwd: [b[3], b[4], b[5]],
            up: [b[6], b[7], b[8]],
        }
    }

    fn entity(&mut self, entity: usize) -> EntityInfo {
        let mut b = [0.0f32; 6];
        unsafe { (self.0.entity)(self.0.ctx, entity, b.as_mut_ptr()) };
        EntityInfo {
            world_pos: [b[0], b[1], b[2]],
            min_dist: b[3],
            max_dist: b[4],
            volume: b[5],
        }
    }
}

/// # Safety
/// `b` is a live bank from `ohpl_gs5_bank_new`.
unsafe fn bank<'a>(b: *mut Bank) -> &'a mut Bank {
    // SAFETY: caller contract.
    unsafe { &mut *b }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs5_bank_new() -> *mut Bank {
    Box::into_raw(Box::default())
}

/// # Safety
/// `b` came from `ohpl_gs5_bank_new` and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_bank_free(b: *mut Bank) {
    if !b.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(b) });
    }
}

pub type Sink2 = unsafe extern "C" fn(Ctx, *const u8, usize);
pub type Sink4 = unsafe extern "C" fn(Ctx, *const u8, usize, *const u8, usize);

/// # Safety
/// `b` is live; `key` is readable for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_ev_mark_project(b: *mut Bank, key: *const u8, n: usize) -> bool {
    // SAFETY: caller contract.
    unsafe { bank(b).events.mark_project(units(key, n)) }
}

/// Parses a project document read by the caller. False when it did not load; `name` receives the
/// project's own name and `count` the number of events read.
///
/// # Safety
/// Pointers are readable for their lengths; `name` accepts `ctx`; `count` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_ev_load_fdp(
    b: *mut Bank,
    data: *const u8,
    n: usize,
    read_ok: bool,
    project: *const u8,
    pn: usize,
    name: Sink2,
    ctx: Ctx,
    count: *mut i32,
) -> bool {
    // SAFETY: caller contract.
    let (bank, data, project) = unsafe { (bank(b), units(data, n), units(project, pn)) };
    match bank.events.load_fdp(data, read_ok, project) {
        Some((nm, c)) => {
            // SAFETY: caller contract.
            unsafe {
                name(ctx, nm.as_ptr(), nm.len());
                *count = c;
            }
            true
        }
        None => false,
    }
}

/// Emits every (bank, sample) pair project `key` needs, banks and samples sorted.
///
/// # Safety
/// As above; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_ev_wanted(
    b: *mut Bank,
    key: *const u8,
    n: usize,
    sink: Sink4,
    ctx: Ctx,
) {
    // SAFETY: caller contract.
    let (bank, key) = unsafe { (bank(b), units(key, n)) };
    for (bk, samples) in bank.events.wanted(key) {
        for s in samples {
            // SAFETY: caller contract.
            unsafe { sink(ctx, bk.as_ptr(), bk.len(), s.as_ptr(), s.len()) };
        }
    }
}

#[repr(C)]
pub struct OhplGs5FilePair {
    pub key: *const u8,
    pub key_len: usize,
    pub file: *const u8,
    pub file_len: usize,
}

/// # Safety
/// `pairs` holds `count` pairs readable for their lengths.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_ev_assign(
    b: *mut Bank,
    key: *const u8,
    n: usize,
    pairs: *const OhplGs5FilePair,
    count: usize,
) {
    // SAFETY: caller contract.
    let (bank, key) = unsafe { (bank(b), units(key, n)) };
    let mut files = BTreeMap::new();
    for i in 0..count {
        // SAFETY: caller contract.
        let p = unsafe { &*pairs.add(i) };
        // SAFETY: caller contract.
        let (k, f) = unsafe { (units(p.key, p.key_len), units(p.file, p.file_len)) };
        files.insert(k.to_vec(), f.to_vec());
    }
    bank.events.assign_files(key, &files);
}

/// The length of the project part of `name`, or -1 when `GetEvent` returns NULL up front.
///
/// # Safety
/// `name` is readable for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_ev_project_len(name: *const u8, n: usize) -> i64 {
    // SAFETY: caller contract.
    let name = unsafe { units(name, n) };
    Events::project_of(name).map_or(-1, |p| p.len() as i64)
}

/// # Safety
/// `b` is live; `name` is readable for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_ev_find(b: *mut Bank, name: *const u8, n: usize) -> i64 {
    // SAFETY: caller contract.
    let (bank, name) = unsafe { (bank(b), units(name, n)) };
    bank.events.find(name).map_or(-1, |i| i as i64)
}

/// # Safety
/// `b` is live; `file` is readable for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_ev_file_event(
    b: *mut Bank,
    file: *const u8,
    n: usize,
    looped: bool,
    stream: bool,
) -> usize {
    // SAFETY: caller contract.
    let (bank, file) = unsafe { (bank(b), units(file, n)) };
    bank.events.file_event(file, looped, stream)
}

#[repr(C)]
pub struct OhplGs5EventInfo {
    pub has_files: bool,
    pub has_defs: bool,
    pub is_3d: bool,
    pub one_shot: bool,
    pub min_dist: f32,
    pub max_dist: f32,
    pub data: usize,
    pub param_count: usize,
}

/// # Safety
/// `b` is live; `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_ev_info(
    b: *mut Bank,
    idx: usize,
    out: *mut OhplGs5EventInfo,
) -> bool {
    // SAFETY: caller contract.
    let bank = unsafe { bank(b) };
    let Some(ev) = bank.events.list.get(idx) else {
        return false;
    };
    // SAFETY: caller contract.
    unsafe {
        out.write(OhplGs5EventInfo {
            has_files: ev.has_files(),
            has_defs: !ev.defs.is_empty(),
            is_3d: ev.is_3d,
            one_shot: ev.one_shot,
            min_dist: ev.min_dist,
            max_dist: ev.max_dist,
            data: ev.data,
            param_count: ev.params.len(),
        });
    }
    true
}

/// Every non-empty file of the event's definitions, in order.
///
/// # Safety
/// `b` is live; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_ev_files(b: *mut Bank, idx: usize, sink: Sink2, ctx: Ctx) {
    // SAFETY: caller contract.
    let bank = unsafe { bank(b) };
    let Some(ev) = bank.events.list.get(idx) else {
        return;
    };
    for f in ev
        .defs
        .iter()
        .flat_map(|d| &d.files)
        .filter(|f| !f.is_empty())
    {
        // SAFETY: caller contract.
        unsafe { sink(ctx, f.as_ptr(), f.len()) };
    }
}

/// # Safety
/// `b` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_ev_set_data(b: *mut Bank, idx: usize, data: usize) {
    // SAFETY: caller contract.
    unsafe { bank(b).events.set_data(idx, data) }
}

/// # Safety
/// `b` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_ev_of_data(b: *mut Bank, data: usize) -> i64 {
    // SAFETY: caller contract.
    unsafe { bank(b).events.event_of_data(data) }.map_or(-1, |i| i as i64)
}

#[repr(C)]
pub struct OhplGs5ParamDef {
    pub min: f32,
    pub max: f32,
    pub velocity: f32,
    pub seek: f32,
    pub sustain: f32,
    pub loop_mode: i32,
    pub builtin: i32,
}

/// # Safety
/// `b` is live; `out` is writable; `name` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_inst_param_def(
    b: *mut Bank,
    h: usize,
    idx: i32,
    out: *mut OhplGs5ParamDef,
    name: Sink2,
    ctx: Ctx,
) -> bool {
    // SAFETY: caller contract.
    let bank = unsafe { bank(b) };
    let p = bank
        .inst(h)
        .and_then(|i| bank.events.list.get(i.st.event))
        .and_then(|e| usize::try_from(idx).ok().and_then(|k| e.params.get(k)));
    let Some(p) = p else { return false };
    // SAFETY: caller contract.
    unsafe {
        out.write(OhplGs5ParamDef {
            min: p.min,
            max: p.max,
            velocity: p.velocity,
            seek: p.seek,
            sustain: p.sustain,
            loop_mode: p.loop_mode,
            builtin: p.builtin,
        });
        name(ctx, p.name.as_ptr(), p.name.len());
    }
    true
}

/// # Safety
/// `b` is live; `host` is a valid table.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_inst_new(
    b: *mut Bank,
    event: usize,
    entity: usize,
    has_entity: bool,
    entry_type: i32,
) -> usize {
    // SAFETY: caller contract.
    unsafe { bank(b).create(event, entity, has_entity, entry_type) }
}

/// The scalar state of instance `h`; the pointer stays valid until the instance is destroyed.
///
/// # Safety
/// `b` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_inst_state(b: *mut Bank, h: usize) -> *mut State {
    // SAFETY: caller contract.
    unsafe { bank(b) }
        .inst_mut(h)
        .map_or(std::ptr::null_mut(), |i| &raw mut i.st)
}

macro_rules! with_host {
    ($(#[$m:meta])* fn $name:ident($($a:ident: $t:ty),*) => |$bank:ident, $host:ident, $h:ident| $body:expr) => {
        $(#[$m])*
        /// # Safety
        /// `b` is live; `host` is a valid table.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name(b: *mut Bank, host: *const OhplGs5Host, $h: usize $(, $a: $t)*) {
            // SAFETY: caller contract.
            let ($bank, mut $host) = unsafe { (bank(b), CHost(&*host)) };
            let $host: &mut dyn Host = &mut $host;
            $body
        }
    };
}

with_host!(fn ohpl_gs5_inst_destroy() => |bank, host, h| bank.destroy(host, h));
with_host!(fn ohpl_gs5_inst_play() => |bank, host, h| bank.play(host, h));
with_host!(fn ohpl_gs5_inst_stop() => |bank, host, h| bank.stop(host, h));
with_host!(fn ohpl_gs5_inst_stop_now() => |bank, host, h| bank.stop_now(host, h));
with_host!(fn ohpl_gs5_inst_fade_out(speed: f32) => |bank, host, h| bank.fade_out(host, h, speed));
with_host!(fn ohpl_gs5_inst_fade_in_to(mul: f32, speed: f32) => |bank, host, h| bank.fade_in_to(host, h, mul, speed));
with_host!(fn ohpl_gs5_inst_set_paused(on: bool) => |bank, host, h| bank.set_paused(host, h, on));
with_host!(fn ohpl_gs5_inst_update(dt: f32) => |bank, host, h| bank.update(host, h, dt));

/// # Safety
/// `b` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_inst_fade_volume_mul_to(
    b: *mut Bank,
    h: usize,
    dest: f32,
    speed: f32,
) {
    // SAFETY: caller contract.
    unsafe { bank(b).fade_volume_mul_to(h, dest, speed) }
}

/// # Safety
/// `b` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_inst_fade_speed_mul_to(
    b: *mut Bank,
    h: usize,
    dest: f32,
    speed: f32,
) {
    // SAFETY: caller contract.
    unsafe { bank(b).fade_speed_mul_to(h, dest, speed) }
}

/// # Safety
/// `b` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_inst_set_param(b: *mut Bank, h: usize, idx: i32, value: f32) {
    // SAFETY: caller contract.
    unsafe { bank(b).set_param(h, idx, value) }
}

/// # Safety
/// `b` is live; `name` is readable for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_inst_param_index(
    b: *mut Bank,
    h: usize,
    name: *const u8,
    n: usize,
) -> i32 {
    // SAFETY: caller contract.
    unsafe { bank(b).param_index(h, units(name, n)) }
}

/// # Safety
/// `b` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_inst_param_value(b: *mut Bank, h: usize, idx: i32) -> f32 {
    // SAFETY: caller contract.
    unsafe { bank(b) }
        .inst(h)
        .and_then(|i| usize::try_from(idx).ok().and_then(|k| i.param_value.get(k)))
        .copied()
        .unwrap_or(0.0)
}

/// # Safety
/// `b` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_inst_param_count(b: *mut Bank, h: usize) -> i32 {
    // SAFETY: caller contract.
    unsafe { bank(b) }
        .inst(h)
        .map_or(0, |i| i.param_value.len() as i32)
}

/// # Safety
/// `b` is live; `host` is a valid table.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_inst_total_time(
    b: *mut Bank,
    host: *const OhplGs5Host,
    h: usize,
) -> f32 {
    // SAFETY: caller contract.
    let (bank, mut host) = unsafe { (bank(b), CHost(&*host)) };
    bank.total_time(&mut host, h)
}

/// Voice count; with `out` also the filter gains (HF, LF, send) of voice `i`.
///
/// # Safety
/// `b` is live; `out` is null or holds three floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_inst_voice(
    b: *mut Bank,
    h: usize,
    i: usize,
    out: *mut f32,
) -> i32 {
    // SAFETY: caller contract.
    let bank = unsafe { bank(b) };
    let Some(inst) = bank.inst(h) else { return 0 };
    if let (false, Some(v)) = (out.is_null(), inst.voices.get(i)) {
        // SAFETY: caller contract.
        unsafe { out.copy_from_nonoverlapping([v.gain_hf, v.gain_lf, v.send_gain].as_ptr(), 3) };
    }
    inst.voices.len() as i32
}

/// The comparison dump of every event (`Events::dump`).
///
/// # Safety
/// `b` is live; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_ev_dump(b: *mut Bank, sink: Sink2, ctx: Ctx) {
    // SAFETY: caller contract.
    let text = unsafe { bank(b) }.events.dump();
    // SAFETY: caller contract.
    unsafe { sink(ctx, text.as_ptr(), text.len()) };
}

/// The event's own name (`cEvent::msName`).
///
/// # Safety
/// `b` is live; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_ev_name(b: *mut Bank, idx: usize, sink: Sink2, ctx: Ctx) {
    // SAFETY: caller contract.
    if let Some(e) = unsafe { bank(b) }.events.list.get(idx) {
        // SAFETY: caller contract.
        unsafe { sink(ctx, e.name.as_ptr(), e.name.len()) };
    }
}
