//! C ABI for the sound layer, mirrored in `HPL2/core/include/sound/RustSound.h`.
//! The C++ `cSoundEntry`, `cSoundHandler` and `cMusicHandler` stay as thin shells owning the objects.

use super::channel::{CChannel, CHost, ChannelOps, HostOps};
use super::entity_data::{self, LoadError, Names, Props};
use super::entry::{EntryState, Step};
use super::music::{Music, MusicHost, Song};
use super::play::{self, PlayParams};
use crate::string_ffi::units;
use crate::types::{Matrixf, Vector3f};
use crate::xml::Doc;
use std::ffi::c_void;

const _: () = {
    use std::mem::{offset_of, size_of};
    assert!(size_of::<EntryState>() == 56 && offset_of!(EntryState, entry_type) == 44);
    assert!(size_of::<PlayParams>() == 32 && offset_of!(PlayParams, entry_type) == 24);
    assert!(size_of::<Props>() == 36 && offset_of!(Props, priority) == 24);
    assert!(size_of::<Song>() == 24 && offset_of!(Song, max_volume) == 8);
};

#[repr(C)]
pub struct SntSink {
    pub ctx: *mut c_void,
    pub add_name: unsafe extern "C" fn(*mut c_void, u32, *const u8, usize),
}

/// Loads a `.snt` document: 0 on success, 1 when the XML did not load (`loaded` false or a parse
/// error), 2 no root element, 3 no `SOUNDS`, 4 no `PROPERTIES`. File names reach `sink` once
/// `SOUNDS` was read, even when 4 follows; `props` is written only on 0.
///
/// # Safety
/// `doc` is live; `sink` accepts its `ctx`; `props` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_snt_load(
    doc: *const Doc,
    loaded: bool,
    sink: *const SntSink,
    props: *mut Props,
) -> i32 {
    if !loaded {
        return 1;
    }
    // SAFETY: caller contract.
    let (doc, sink) = unsafe { (&*doc, &*sink) };
    let mut names = Names::default();
    let result = entity_data::parse(doc, &mut names);
    for (ty, list) in (0u32..).zip(&names) {
        for name in list {
            // SAFETY: caller contract.
            unsafe { (sink.add_name)(sink.ctx, ty, name.as_ptr(), name.len()) };
        }
    }
    match result {
        Ok(p) => {
            // SAFETY: caller contract.
            unsafe { props.write(p) };
            0
        }
        Err(LoadError::Xml) => 1,
        Err(LoadError::NoRoot) => 2,
        Err(LoadError::NoSounds) => 3,
        Err(LoadError::NoProperties) => 4,
    }
}

/// The index `GetRandomSoundName` picks (-1 for an empty list); `previous` is updated when the
/// pick should be remembered.
///
/// # Safety
/// `previous` is readable and writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_snd_random_index(
    size: i32,
    previous: *mut i32,
    skip_previous: bool,
) -> i32 {
    // SAFETY: caller contract.
    let prev = unsafe { *previous };
    match entity_data::random_index(size, prev, skip_previous) {
        None => -1,
        Some((idx, remember)) => {
            if remember {
                // SAFETY: caller contract.
                unsafe { *previous = idx };
            }
            idx
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_snd_name_index(size: i32, idx: i32) -> i32 {
    entity_data::name_index(size, idx)
        .and_then(|i| i32::try_from(i).ok())
        .unwrap_or(-1)
}

/// # Safety
/// `state` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_snd_entry_init(
    state: *mut EntryState,
    volume: f32,
    entry_type: u32,
    is_3d: bool,
    stream: bool,
) {
    // SAFETY: caller contract.
    unsafe { state.write(EntryState::new(volume, entry_type, is_3d, stream)) };
}

/// 1 while the entry lives, 0 when its channel finished, 2 when it finished and the entry's
/// priority-release callback is due; the caller then stops the channel itself.
///
/// # Safety
/// `state` is live and not aliased during the call; `ch_ops`/`ch` and `host_ops`/`handler` are
/// valid pairs.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_snd_entry_update(
    state: *mut EntryState,
    dt: f32,
    ch_ops: *const ChannelOps,
    ch: *mut c_void,
    host_ops: *const HostOps,
    handler: *mut c_void,
) -> i32 {
    // SAFETY: caller contract.
    let (state, mut ch, mut host) = unsafe {
        (
            &mut *state,
            CChannel::new(&*ch_ops, ch),
            CHost::new(&*host_ops, handler),
        )
    };
    match state.update(dt, &mut ch, &mut host) {
        Step::Alive => 1,
        Step::Finished { release: false } => 0,
        Step::Finished { release: true } => 2,
    }
}

/// # Safety
/// `state` is live; `ch_ops`/`ch` are a valid pair.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_snd_entry_stop(
    state: *mut EntryState,
    ch_ops: *const ChannelOps,
    ch: *mut c_void,
) {
    // SAFETY: caller contract.
    let (state, mut ch) = unsafe { (&mut *state, CChannel::new(&*ch_ops, ch)) };
    state.stop(&mut ch);
}

/// # Safety
/// `state` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_snd_entry_fade_volume(state: *mut EntryState, dest: f32, speed: f32) {
    // SAFETY: caller contract.
    unsafe { &mut *state }.fade_volume_mul_to(dest, speed);
}

/// # Safety
/// `state` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_snd_entry_fade_speed(state: *mut EntryState, dest: f32, speed: f32) {
    // SAFETY: caller contract.
    unsafe { &mut *state }.fade_speed_mul_to(dest, speed);
}

/// # Safety
/// `state` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_snd_entry_fade_out(state: *mut EntryState, speed: f32) {
    // SAFETY: caller contract.
    unsafe { &mut *state }.fade_out(speed);
}

/// # Safety
/// `state` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_snd_entry_fade_in(
    state: *mut EntryState,
    volume_mul: f32,
    speed: f32,
) {
    // SAFETY: caller contract.
    unsafe { &mut *state }.fade_in(volume_mul, speed);
}

/// # Safety
/// `pos` and `listener` are readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_snd_play_priority(
    pos: *const Vector3f,
    listener: *const Vector3f,
    min_dist: f32,
    max_dist: f32,
    is_3d: bool,
    relative: bool,
) -> i32 {
    // SAFETY: caller contract.
    let (pos, listener) = unsafe { (*pos, *listener) };
    play::distance_priority(pos, listener, min_dist, max_dist, is_3d, relative)
}

/// # Safety
/// `params` and `listener` are readable; `ch_ops`/`ch` are a valid pair.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_snd_play_setup(
    params: *const PlayParams,
    dist_priority: i32,
    listener: *const Matrixf,
    ch_ops: *const ChannelOps,
    ch: *mut c_void,
) {
    // SAFETY: caller contract.
    let (params, listener, mut ch) = unsafe { (&*params, &*listener, CChannel::new(&*ch_ops, ch)) };
    play::setup_channel(&mut ch, params, dist_priority, listener);
}

#[repr(C)]
pub struct MusicHostOps {
    pub channel: *const ChannelOps,
    pub new_song: unsafe extern "C" fn(*mut c_void) -> *mut Song,
    pub delete_song: unsafe extern "C" fn(*mut c_void, *mut Song),
    pub create_stream: unsafe extern "C" fn(*mut c_void, *const u8, usize, bool) -> *mut c_void,
    pub set_song_name: unsafe extern "C" fn(*mut c_void, *mut Song, *const u8, usize),
    pub set_lock: unsafe extern "C" fn(*mut c_void, *mut c_void, *const u8, usize, f32, bool),
}

struct CMusicHost<'a> {
    ops: &'a MusicHostOps,
    ctx: *mut c_void,
}

impl<'a> CMusicHost<'a> {
    /// # Safety
    /// `ops` and `ctx` are a valid pair, and `ops.channel` points to a valid table.
    unsafe fn new(ops: *const MusicHostOps, ctx: *mut c_void) -> Self {
        // SAFETY: caller contract.
        Self {
            ops: unsafe { &*ops },
            ctx,
        }
    }
}

impl<'a> MusicHost for CMusicHost<'a> {
    type Stream = CChannel<'a>;

    fn new_song(&mut self) -> *mut Song {
        // SAFETY: `CMusicHost::new` contract.
        unsafe { (self.ops.new_song)(self.ctx) }
    }

    fn delete_song(&mut self, song: *mut Song) {
        // SAFETY: `CMusicHost::new` contract; `song` came from `new_song`.
        unsafe { (self.ops.delete_song)(self.ctx, song) }
    }

    fn create_stream(&mut self, name: &[u8], looping: bool) -> Option<*mut c_void> {
        // SAFETY: `CMusicHost::new` contract.
        let stream =
            unsafe { (self.ops.create_stream)(self.ctx, name.as_ptr(), name.len(), looping) };
        (!stream.is_null()).then_some(stream)
    }

    fn set_song_name(&mut self, song: *mut Song, name: &[u8]) {
        // SAFETY: `CMusicHost::new` contract.
        unsafe { (self.ops.set_song_name)(self.ctx, song, name.as_ptr(), name.len()) }
    }

    fn set_lock(&mut self, lock: *mut c_void, name: &[u8], volume: f32, looping: bool) {
        // SAFETY: `CMusicHost::new` contract; `lock` was handed to `ohpl_music_lock`.
        unsafe { (self.ops.set_lock)(self.ctx, lock, name.as_ptr(), name.len(), volume, looping) }
    }

    unsafe fn stream(&self, stream: *mut c_void) -> CChannel<'a> {
        // SAFETY: `CMusicHost::new` contract on the table; `stream` is live per the trait contract.
        unsafe { CChannel::new(&*self.ops.channel, stream) }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_music_new() -> *mut Music {
    Box::into_raw(Box::new(Music::new()))
}

/// Deletes every song through the host, then the handler.
///
/// # Safety
/// `music` comes from `ohpl_music_new` and is not used afterwards; `ops`/`ctx` are a valid pair.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_music_free(
    music: *mut Music,
    ops: *const MusicHostOps,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    let (mut music, mut host) = unsafe { (Box::from_raw(music), CMusicHost::new(ops, ctx)) };
    music.destroy(&mut host);
}

/// # Safety
/// `music` is live; `name` is readable for `len` bytes; `ops`/`ctx` are a valid pair.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_music_play(
    music: *mut Music,
    ops: *const MusicHostOps,
    ctx: *mut c_void,
    name: *const u8,
    len: usize,
    volume: f32,
    fade_step: f32,
    looping: bool,
    resume: bool,
) -> bool {
    // SAFETY: caller contract.
    let (music, mut host, name) =
        unsafe { (&mut *music, CMusicHost::new(ops, ctx), units(name, len)) };
    music.play(&mut host, name, volume, fade_step, looping, resume)
}

/// # Safety
/// `music` is live; `ops`/`ctx` are a valid pair.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_music_stop(
    music: *mut Music,
    ops: *const MusicHostOps,
    ctx: *mut c_void,
    fade_step: f32,
) {
    // SAFETY: caller contract.
    let (music, host) = unsafe { (&mut *music, CMusicHost::new(ops, ctx)) };
    music.stop(&host, fade_step);
}

/// # Safety
/// `music` is live; `ops`/`ctx` are a valid pair.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_music_set_paused(
    music: *mut Music,
    ops: *const MusicHostOps,
    ctx: *mut c_void,
    paused: bool,
) {
    // SAFETY: caller contract.
    let (music, host) = unsafe { (&mut *music, CMusicHost::new(ops, ctx)) };
    music.set_paused(&host, paused);
}

/// # Safety
/// `music` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_music_lock(music: *mut Music, lock: *mut c_void) {
    // SAFETY: caller contract.
    unsafe { &mut *music }.lock(lock);
}

/// # Safety
/// `music` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_music_fade_volume_mul(music: *mut Music, dest: f32, speed: f32) {
    // SAFETY: caller contract.
    unsafe { &mut *music }.fade_volume_mul(dest, speed);
}

/// # Safety
/// `music` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_music_set_volume_mul(music: *mut Music, mul: f32) {
    // SAFETY: caller contract.
    unsafe { &mut *music }.set_volume_mul(mul);
}

/// # Safety
/// `music` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_music_volume_mul(music: *const Music) -> f32 {
    // SAFETY: caller contract.
    unsafe { &*music }.volume_mul()
}

/// The main song, or null.
///
/// # Safety
/// `music` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_music_main_song(music: *const Music) -> *mut Song {
    // SAFETY: caller contract.
    unsafe { &*music }.main_song()
}

/// # Safety
/// `music` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_music_reset_resume(music: *mut Music) {
    // SAFETY: caller contract.
    unsafe { &mut *music }.reset_resume();
}

/// # Safety
/// `music` is live; `ops`/`ctx` are a valid pair.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_music_update(
    music: *mut Music,
    ops: *const MusicHostOps,
    ctx: *mut c_void,
    dt: f32,
) {
    // SAFETY: caller contract.
    let (music, mut host) = unsafe { (&mut *music, CMusicHost::new(ops, ctx)) };
    music.update(&mut host, dt);
}
