//! C ABI of the owned sound objects, mirrored in `HPL2/core/include/sound/RustSound.h`.
//! `cSoundEntry*` is a `*mut Rec`, the data record a `*mut Data`, the sound entity a `*mut Entity`.

use super::channel::{CChannel, Channel};
use super::music::{Music, MusicHost, Song};
use super::own::{
    Data, Entity, Handler, PlayArgs, Rec, SndHost, ent_fade_in, ent_fade_out, ent_free, ent_play,
    ent_stop, ent_update, register_data, register_entity, set_handler, set_lazy_load,
    unregister_data,
};
use crate::engine::multi_settings::Entry as GsEntry;
use crate::string_ffi::units;
use crate::types::Vector3f;
use crate::xml::Doc;
use std::ffi::c_void;
use std::sync::atomic::{AtomicPtr, Ordering};

const _: () = {
    use std::mem::{offset_of, size_of};
    assert!(size_of::<Song>() == 24 && offset_of!(Song, max_volume) == 8);
    assert!(size_of::<GsEntry>() == 20);
};

macro_rules! r {
    ($p:expr) => {
        // SAFETY: caller contract of every export below: the pointer is live and not aliased.
        unsafe { &mut *$p }
    };
}

/// # Safety
/// `host` is a static table.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndh_new(
    host: *const SndHost,
    lowlevel: *mut c_void,
    resources: *mut c_void,
) -> *mut Handler {
    // SAFETY: caller contract.
    let h = Box::into_raw(Box::new(unsafe { Handler::new(host, lowlevel, resources) }));
    set_handler(h);
    h
}

/// # Safety
/// `h` came from `ohpl_sndh_new`; all entities are freed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndh_free(h: *mut Handler) {
    set_handler(std::ptr::null_mut());
    // SAFETY: caller contract.
    let mut h = unsafe { Box::from_raw(h) };
    h.clear();
}

/// # Safety
/// `h` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndh_set_world(h: *mut Handler, world: *mut c_void) {
    r!(h).set_world(world);
}

/// # Safety
/// `h` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndh_lowlevel(h: *mut Handler) -> *mut c_void {
    r!(h).lowlevel()
}

/// # Safety
/// `h` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndh_set_silent(h: *mut Handler, silent: bool) {
    r!(h).set_silent(silent);
}

/// # Safety
/// `h` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndh_silent(h: *mut Handler) -> bool {
    r!(h).silent()
}

/// # Safety
/// `h` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndh_global_volume(h: *mut Handler, ty: u32) -> f32 {
    r!(h).global_volume(ty)
}

/// # Safety
/// `h` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndh_global_speed(h: *mut Handler, ty: u32) -> f32 {
    r!(h).global_speed(ty)
}

/// # Safety
/// `h` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndh_update(h: *mut Handler, dt: f32) {
    r!(h).update(dt);
}

/// A new entry or null; `not_enough` receives whether the channel limit was the reason.
///
/// # Safety
/// `h` is live; `name` readable for `len`; `pos` readable; `not_enough` writable or null.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndh_play(
    h: *mut Handler,
    name: *const u8,
    len: usize,
    looping: bool,
    volume: f32,
    pos: *const Vector3f,
    min_dist: f32,
    max_dist: f32,
    entry_type: u32,
    relative: bool,
    is_3d: bool,
    priority_modifier: i32,
    stream: bool,
    not_enough: *mut bool,
) -> *mut Rec {
    let mut ne = false;
    let args = PlayArgs {
        // SAFETY: caller contract.
        name: unsafe { units(name, len) },
        looping,
        volume,
        // SAFETY: caller contract.
        pos: unsafe { *pos },
        min_dist,
        max_dist,
        entry_type,
        relative,
        is_3d,
        priority_modifier,
        stream,
    };
    let rec = r!(h).play(&args, &mut ne);
    if !not_enough.is_null() {
        // SAFETY: caller contract.
        unsafe { *not_enough = ne };
    }
    rec
}

/// # Safety
/// `h` is live; `name` readable for `len`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndh_stop(h: *mut Handler, name: *const u8, len: usize) -> bool {
    // SAFETY: caller contract.
    r!(h).stop(unsafe { units(name, len) })
}

/// # Safety
/// `h` is live; `name` readable for `len`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndh_is_playing(
    h: *mut Handler,
    name: *const u8,
    len: usize,
) -> bool {
    // SAFETY: caller contract.
    r!(h).is_playing(unsafe { units(name, len) })
}

/// Applies `op` (0 stop, 1 pause, 2 resume, 3 fade out) to every entry of `mask`.
///
/// # Safety
/// `h` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndh_each(
    h: *mut Handler,
    mask: u32,
    op: i32,
    speed: f32,
    disable_stop: bool,
) {
    r!(h).for_each(mask, |hh, rec| match op {
        0 => hh.stop_rec(rec),
        1 => hh.set_paused(rec, true),
        2 => hh.set_paused(rec, false),
        _ => {
            rec.state.fade_out(speed);
            if disable_stop {
                rec.state.stop_disabled = true;
            }
        }
    });
}

/// # Safety
/// `h` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndh_is_valid(h: *mut Handler, rec: *const Rec, id: i32) -> bool {
    r!(h).is_valid(rec, id)
}

/// # Safety
/// `h` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndh_count(h: *mut Handler) -> usize {
    r!(h).len()
}

/// # Safety
/// `h` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndh_entry_at(h: *mut Handler, i: usize) -> *mut Rec {
    r!(h).entry_at(i)
}

/// # Safety
/// `h` is live; `len` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndh_recent(h: *mut Handler, i: usize, len: *mut usize) -> *const u8 {
    let line = r!(h).recent().get(i);
    // SAFETY: caller contract.
    unsafe { *len = line.map_or(0, Vec::len) };
    line.map_or(std::ptr::null(), |l| l.as_ptr())
}

/// # Safety
/// `h` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndh_recent_count(h: *mut Handler) -> usize {
    r!(h).recent().len()
}

/// `kind` 0 volume, 1 speed. The entry has the layout of `cMultipleSettingsHandler::cGSEntry`.
///
/// # Safety
/// `h` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndh_gs_entry(
    h: *mut Handler,
    kind: usize,
    id: i32,
    force: bool,
) -> *mut GsEntry {
    r!(h).setting_entry(kind.min(1), id, force)
}

/// # Safety
/// `h` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndh_gs_create(h: *mut Handler, kind: usize) -> i32 {
    r!(h).setting_create(kind.min(1))
}

/// # Safety
/// `h` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndh_gs_exists(h: *mut Handler, kind: usize, id: i32) -> bool {
    r!(h).setting_exists(kind.min(1), id)
}

/// # Safety
/// `h` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndh_gs_destroy(h: *mut Handler, kind: usize, id: i32) {
    r!(h).setting_destroy(kind.min(1), id);
}

/// # Safety
/// `h` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndh_gs_calc(
    h: *mut Handler,
    kind: usize,
    types: u32,
    calc: i32,
    default: f32,
) -> f32 {
    r!(h).setting_calc(kind.min(1), types, calc, default)
}

/// # Safety
/// `h` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndh_gs_set(
    h: *mut Handler,
    kind: usize,
    val: f32,
    types: u32,
    id: i32,
) -> i32 {
    r!(h).set_global(kind.min(1), val, types, id)
}

/// # Safety
/// `h` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndh_gs_fade(
    h: *mut Handler,
    kind: usize,
    dest: f32,
    speed: f32,
    types: u32,
    id: i32,
    destroy: bool,
) -> i32 {
    r!(h).fade_global(kind.min(1), dest, speed, types, id, destroy)
}

/// Lowercase name, valid until the entry is freed.
///
/// # Safety
/// `e` is live; `len` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndent_name(e: *mut Rec, len: *mut usize) -> *const u8 {
    let e = r!(e);
    // SAFETY: caller contract.
    unsafe { *len = e.name.len() };
    e.name.as_ptr()
}

/// # Safety
/// `e` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndent_id(e: *mut Rec) -> i32 {
    r!(e).id
}

/// # Safety
/// `e` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndent_channel(e: *mut Rec) -> *mut c_void {
    r!(e).channel
}

/// Sets one field: 0 volume mul (also its fade target), 1 speed mul (same), 2 default volume,
/// 3 default speed, otherwise stop disabled.
///
/// # Safety
/// `e` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndent_set(e: *mut Rec, what: i32, v: f32) {
    let s = &mut r!(e).state;
    match what {
        0 => {
            s.volume_mul = v;
            s.volume_fade_dest = v;
        }
        1 => {
            s.speed_mul = v;
            s.speed_fade_dest = v;
        }
        2 => s.normal_volume = v,
        3 => s.normal_speed = v,
        _ => s.stop_disabled = v != 0.0,
    }
}

/// 0 volume mul, 1 first time, otherwise stop disabled.
///
/// # Safety
/// `e` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndent_get(e: *mut Rec, what: i32) -> f32 {
    let s = &r!(e).state;
    match what {
        0 => s.volume_mul,
        1 => f32::from(u8::from(s.first_time)),
        _ => f32::from(u8::from(s.stop_disabled)),
    }
}

/// # Safety
/// `e` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndent_type(e: *mut Rec) -> u32 {
    r!(e).state.entry_type
}

/// # Safety
/// `e` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndent_stop(e: *mut Rec) {
    let e = r!(e);
    // SAFETY: the owning handler is live.
    unsafe { &*e.handler() }.stop_rec(e);
}

/// # Safety
/// `e` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndent_set_paused(e: *mut Rec, paused: bool) {
    r!(e).chan().set_paused(paused);
}

/// Fade: 0 volume, 1 speed (`dest`, `speed`), 2 out (`speed`), otherwise in (`dest` = volume mul).
///
/// # Safety
/// `e` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndent_fade(e: *mut Rec, what: i32, dest: f32, speed: f32) {
    let s = &mut r!(e).state;
    match what {
        0 => s.fade_volume_mul_to(dest, speed),
        1 => s.fade_speed_mul_to(dest, speed),
        2 => s.fade_out(speed),
        _ => s.fade_in(dest, speed),
    }
}

/// `shell` is the `cSoundEntityData` the record belongs to.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_sndd_new(shell: *mut c_void) -> *mut Data {
    let d = Box::into_raw(Box::<Data>::default());
    register_data(shell, d);
    d
}

/// # Safety
/// `d` came from `ohpl_sndd_new`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndd_free(d: *mut Data) {
    unregister_data(d);
    // SAFETY: caller contract.
    drop(unsafe { Box::from_raw(d) });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_sndd_set_lazy(lazy: bool) {
    set_lazy_load(lazy);
}

/// Reads a number: 0 volume, 1 max distance, 2 min distance, 3 block volume mul, 4 random,
/// 5 interval, 6 priority, 7 fade start, 8 fade stop, 9 stream, 10 loop, 11 use 3D, 12 blockable.
///
/// # Safety
/// `d` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndd_get(d: *mut Data, what: i32) -> f32 {
    let d = r!(d);
    let b = |x: bool| f32::from(u8::from(x));
    match what {
        0 => d.volume,
        1 => d.max_distance,
        2 => d.min_distance,
        3 => d.block_volume_mul,
        4 => d.random,
        5 => d.interval,
        6 => d.priority as f32,
        7 => b(d.fade_start),
        8 => b(d.fade_stop),
        9 => b(d.stream),
        10 => b(d.looping),
        11 => b(d.use_3d),
        _ => b(d.blockable),
    }
}

/// # Safety
/// `d` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndd_set(d: *mut Data, what: i32, v: f32) {
    let d = r!(d);
    let b = v != 0.0;
    match what {
        0 => d.volume = v,
        1 => d.max_distance = v,
        2 => d.min_distance = v,
        3 => d.block_volume_mul = v,
        4 => d.random = v,
        5 => d.interval = v,
        6 => d.priority = v as i32,
        7 => d.fade_start = b,
        8 => d.fade_stop = b,
        9 => d.stream = b,
        10 => d.looping = b,
        11 => d.use_3d = b,
        _ => d.blockable = b,
    }
}

/// # Safety
/// `d` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndd_num(d: *mut Data, ty: usize) -> usize {
    r!(d).num(ty)
}

/// Name at `idx` (empty when out of range) or, with `random`, a random pick.
///
/// # Safety
/// `d` is live; `len` writable. The name is valid until the data changes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndd_name(
    d: *mut Data,
    ty: usize,
    idx: i32,
    random: bool,
    skip_previous: bool,
    len: *mut usize,
) -> *const u8 {
    let d = r!(d);
    let n = if random {
        d.random_name(ty, skip_previous)
    } else {
        d.name(ty, idx)
    };
    // SAFETY: caller contract.
    unsafe { *len = n.len() };
    n.as_ptr()
}

/// Sets the name at `idx`, or appends when `idx` is negative.
///
/// # Safety
/// `d` is live; `name` readable for `len`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndd_put_name(
    d: *mut Data,
    ty: usize,
    idx: i32,
    name: *const u8,
    len: usize,
) {
    // SAFETY: caller contract.
    let name = unsafe { units(name, len) };
    if idx < 0 {
        r!(d).add_name(ty, name);
    } else {
        r!(d).set_name(ty, idx, name);
    }
}

/// # Safety
/// `d` is live; `host` is a static table.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndd_preload(
    d: *mut Data,
    host: *const SndHost,
    resources: *mut c_void,
) {
    // SAFETY: caller contract.
    let d = r!(d);
    // SAFETY: caller contract.
    unsafe { d.preload(&*host, resources) };
}

/// 0 on success, 1 XML failure, 2 no root, 3 no SOUNDS, 4 no PROPERTIES.
///
/// # Safety
/// `d` is live; `doc` is live when `loaded`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndd_load(d: *mut Data, doc: *const Doc, loaded: bool) -> i32 {
    // SAFETY: caller contract.
    r!(d).load(if loaded {
        unsafe { doc.as_ref() }
    } else {
        None
    })
}

/// # Safety
/// `h` and `d` are live; `shell` is the `cSoundEntity`, `data_shell` the `cSoundEntityData`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndse_new(
    h: *mut Handler,
    d: *mut Data,
    data_shell: *mut c_void,
    shell: *mut c_void,
    world: *mut c_void,
    remove_when_over: bool,
    creation_id: i32,
) -> *mut Entity {
    // SAFETY: caller contract.
    let e = Box::into_raw(unsafe {
        Entity::new(
            h,
            d,
            data_shell,
            shell,
            world,
            remove_when_over,
            creation_id,
        )
    });
    register_entity(shell, e);
    e
}

/// # Safety
/// `e` came from `ohpl_sndse_new` and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndse_free(e: *mut Entity) {
    // SAFETY: caller contract.
    unsafe { ent_free(e) };
}

/// # Safety
/// `e` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndse_play(e: *mut Entity, play_start: bool) {
    // SAFETY: caller contract.
    unsafe { ent_play(e, play_start) };
}

/// # Safety
/// `e` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndse_stop(e: *mut Entity, play_end: bool) {
    // SAFETY: caller contract.
    unsafe { ent_stop(e, play_end) };
}

/// # Safety
/// `e` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndse_fade_in(e: *mut Entity, speed: f32) {
    // SAFETY: caller contract.
    unsafe { ent_fade_in(e, speed) };
}

/// # Safety
/// `e` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndse_fade_out(e: *mut Entity, speed: f32) {
    // SAFETY: caller contract.
    unsafe { ent_fade_out(e, speed) };
}

/// # Safety
/// `e` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndse_update(e: *mut Entity, dt: f32) {
    // SAFETY: caller contract.
    unsafe { ent_update(e, dt) };
}

/// # Safety
/// `e` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndse_is_stopped(e: *mut Entity) -> bool {
    r!(e).is_stopped()
}

/// 0 fading out, 1 remove when over, 2 force GUI, otherwise creation id.
///
/// # Safety
/// `e` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndse_flag(e: *mut Entity, what: i32) -> i32 {
    let e = r!(e);
    match what {
        0 => i32::from(e.state.fading_out),
        1 => i32::from(e.state.remove_when_over),
        2 => i32::from(e.force_gui),
        _ => e.creation_id,
    }
}

/// # Safety
/// `e` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndse_set_force_gui(e: *mut Entity, gui: bool) {
    r!(e).force_gui = gui;
}

/// 0 volume, 1 min distance, otherwise max distance.
///
/// # Safety
/// `e` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndse_get(e: *mut Entity, what: i32) -> f32 {
    let e = r!(e);
    match what {
        0 => e.volume,
        1 => e.min_distance,
        _ => e.max_distance,
    }
}

/// # Safety
/// `e` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndse_set(e: *mut Entity, what: i32, v: f32) {
    let e = r!(e);
    match what {
        0 => e.volume = v,
        1 => e.min_distance = v,
        _ => e.max_distance = v,
    }
}

/// # Safety
/// `e` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndse_entry(e: *mut Entity, ty: usize, check: bool) -> *mut Rec {
    r!(e).entry(ty, check)
}

/// # Safety
/// `e` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndse_set_event(e: *mut Entity, ev: *mut c_void) {
    r!(e).set_event(ev);
}

/// # Safety
/// `e` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_sndse_event(e: *mut Entity) -> *mut c_void {
    r!(e).event()
}

/// A song as the game sees it: the `Song` header first, the file name after.
#[repr(C)]
pub struct SongRec {
    song: Song,
    name: Vec<u8>,
}

struct CMusicHost<'a> {
    host: &'a SndHost,
    res: *mut c_void,
}

impl<'a> MusicHost for CMusicHost<'a> {
    type Stream = CChannel<'a>;

    fn new_song(&mut self) -> *mut Song {
        Box::into_raw(Box::new(SongRec {
            song: Song {
                stream: std::ptr::null_mut(),
                max_volume: 1.0,
                volume: 0.0,
                volume_add: 0.01,
                looping: false,
            },
            name: Vec::new(),
        }))
        .cast()
    }

    fn delete_song(&mut self, song: *mut Song) {
        // SAFETY: `song` is a `SongRec` from `new_song`; its stream is a live channel or null.
        unsafe {
            let rec = Box::from_raw(song.cast::<SongRec>());
            if !rec.song.stream.is_null() {
                (self.host.delete_channel)(rec.song.stream);
            }
        }
    }

    fn create_stream(&mut self, name: &[u8], looping: bool) -> Option<*mut c_void> {
        // SAFETY: table contract.
        let s = unsafe { (self.host.create_stream)(self.res, name.as_ptr(), name.len(), looping) };
        (!s.is_null()).then_some(s)
    }

    fn set_song_name(&mut self, song: *mut Song, name: &[u8]) {
        // SAFETY: `song` is a `SongRec` from `new_song`.
        unsafe { (*song.cast::<SongRec>()).name = name.to_vec() };
    }

    fn set_lock(&mut self, lock: *mut c_void, name: &[u8], volume: f32, looping: bool) {
        // SAFETY: table contract; `lock` was handed to `ohpl_music_lock`.
        unsafe { (self.host.set_lock)(lock, name.as_ptr(), name.len(), volume, looping) };
    }

    unsafe fn stream(&self, stream: *mut c_void) -> CChannel<'a> {
        // SAFETY: the table's channel ops are static; `stream` is live per the trait contract.
        unsafe { CChannel::new(&*self.host.channel, stream) }
    }
}

/// `cMusicHandler`: the music state with the host table and resources it plays through.
pub struct MusicOwner {
    music: Music,
    host: *const SndHost,
    res: *mut c_void,
}

static MUSIC: AtomicPtr<MusicOwner> = AtomicPtr::new(std::ptr::null_mut());

/// The engine's music handler, for game code that plays music directly.
pub fn music() -> Option<&'static mut MusicOwner> {
    // SAFETY: set from `ohpl_music_new`, cleared before the handler is freed; engine thread only.
    unsafe { MUSIC.load(Ordering::Acquire).as_mut() }
}

impl MusicOwner {
    pub fn play(
        &mut self,
        name: &[u8],
        volume: f32,
        fade_step: f32,
        looping: bool,
        resume: bool,
    ) -> bool {
        let (music, mut host) = self.split();
        music.play(&mut host, name, volume, fade_step, looping, resume)
    }

    pub fn stop(&mut self, fade_step: f32) {
        let (music, host) = self.split();
        music.stop(&host, fade_step);
    }

    pub fn has_song(&self) -> bool {
        !self.music.main_song().is_null()
    }

    pub fn fade_volume_mul(&mut self, dest: f32, speed: f32) {
        self.music.fade_volume_mul(dest, speed);
    }

    pub fn volume_mul(&self) -> f32 {
        self.music.volume_mul()
    }

    fn split(&mut self) -> (&mut Music, CMusicHost<'_>) {
        // SAFETY: `ohpl_music_new` contract: the table is static.
        let host = unsafe { &*self.host };
        (
            &mut self.music,
            CMusicHost {
                host,
                res: self.res,
            },
        )
    }
}

/// # Safety
/// `host` is a static table.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_music_new(host: *const SndHost, res: *mut c_void) -> *mut MusicOwner {
    let m = Box::into_raw(Box::new(MusicOwner {
        music: Music::new(),
        host,
        res,
    }));
    MUSIC.store(m, Ordering::Release);
    m
}

/// # Safety
/// `m` came from `ohpl_music_new` and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_music_free(m: *mut MusicOwner) {
    MUSIC.store(std::ptr::null_mut(), Ordering::Release);
    // SAFETY: caller contract.
    let mut m = unsafe { Box::from_raw(m) };
    let (music, mut host) = m.split();
    music.destroy(&mut host);
}

/// # Safety
/// `m` is live; `name` readable for `len`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_music_play(
    m: *mut MusicOwner,
    name: *const u8,
    len: usize,
    volume: f32,
    fade_step: f32,
    looping: bool,
    resume: bool,
) -> bool {
    let (music, mut host) = r!(m).split();
    // SAFETY: caller contract.
    music.play(
        &mut host,
        unsafe { units(name, len) },
        volume,
        fade_step,
        looping,
        resume,
    )
}

/// # Safety
/// `m` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_music_stop(m: *mut MusicOwner, fade_step: f32) {
    let (music, host) = r!(m).split();
    music.stop(&host, fade_step);
}

/// # Safety
/// `m` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_music_set_paused(m: *mut MusicOwner, paused: bool) {
    let (music, host) = r!(m).split();
    music.set_paused(&host, paused);
}

/// # Safety
/// `m` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_music_lock(m: *mut MusicOwner, lock: *mut c_void) {
    r!(m).music.lock(lock);
}

/// # Safety
/// `m` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_music_fade_volume_mul(m: *mut MusicOwner, dest: f32, speed: f32) {
    r!(m).music.fade_volume_mul(dest, speed);
}

/// # Safety
/// `m` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_music_set_volume_mul(m: *mut MusicOwner, mul: f32) {
    r!(m).music.set_volume_mul(mul);
}

/// # Safety
/// `m` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_music_volume_mul(m: *mut MusicOwner) -> f32 {
    r!(m).music.volume_mul()
}

/// The main song record (header layout `Song`), or null.
///
/// # Safety
/// `m` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_music_main_song(m: *mut MusicOwner) -> *mut Song {
    r!(m).music.main_song()
}

/// The file name of a song.
///
/// # Safety
/// `s` is a song of a live handler; `len` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_music_song_name(s: *mut Song, len: *mut usize) -> *const u8 {
    // SAFETY: caller contract.
    let rec = unsafe { &*s.cast::<SongRec>() };
    // SAFETY: caller contract.
    unsafe { *len = rec.name.len() };
    rec.name.as_ptr()
}

/// # Safety
/// `m` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_music_reset_resume(m: *mut MusicOwner) {
    r!(m).music.reset_resume();
}

/// # Safety
/// `m` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_music_update(m: *mut MusicOwner, dt: f32) {
    let (music, mut host) = r!(m).split();
    music.update(&mut host, dt);
}
