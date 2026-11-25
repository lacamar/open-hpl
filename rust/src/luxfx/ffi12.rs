//! C ABI of the music and insanity handlers (`amnesia/src/game/RustLuxFxA12b.h`). The handlers
//! live in thread-local arenas and the C++ shells hold the handle bits. A handler is taken out of
//! its slot while it runs, so a re-entrant call finds nothing instead of aliasing it.

use super::host12::{CHost, Host};
use super::insanity_h::InsanityHandler;
use super::music_h::{GameMusic, MusicHandler};
use crate::luxmap::mhost::slice_of as bytes;
use crate::obj::{Arena, Handle};
use std::cell::RefCell;
use std::ffi::c_void;
use std::thread::LocalKey;

type Slots<T> = RefCell<Arena<Option<T>>>;
type StrSink = unsafe extern "C" fn(*mut c_void, *const u8, usize);

thread_local! {
    static MUSIC: Slots<MusicHandler> = RefCell::new(Arena::default());
    static INSANITY: Slots<InsanityHandler> = RefCell::new(Arena::default());
}

fn put<T>(arena: &'static LocalKey<Slots<T>>, v: T) -> u64 {
    arena.with(|a| {
        a.try_borrow_mut()
            .ok()
            .and_then(|mut a| a.insert(Some(v)))
            .map_or(0, Handle::to_bits)
    })
}

fn free<T>(arena: &'static LocalKey<Slots<T>>, bits: u64) {
    if let Some(h) = Handle::from_bits(bits) {
        let old = arena.with(|a| a.try_borrow_mut().ok().and_then(|mut a| a.remove(h)));
        drop(old);
    }
}

fn with<T, R>(
    arena: &'static LocalKey<Slots<T>>,
    bits: u64,
    none: R,
    f: impl FnOnce(&mut T) -> R,
) -> R {
    let Some(h) = Handle::from_bits(bits) else {
        return none;
    };
    let taken = arena.with(|a| {
        a.try_borrow_mut()
            .ok()
            .and_then(|mut a| a.get_mut(h).and_then(Option::take))
    });
    let Some(mut v) = taken else {
        return none;
    };
    let r = f(&mut v);
    arena.with(|a| {
        if let Ok(mut a) = a.try_borrow_mut()
            && let Some(slot) = a.get_mut(h)
        {
            *slot = Some(v);
        }
    });
    r
}

static HOST: CHost = CHost;

fn host() -> &'static dyn Host {
    &HOST
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct MusicState {
    pub max_prio: i32,
    pub slot_count: i32,
    pub cur_prio: i32,
    pub close_playing: u32,
    pub playing: [u32; 2],
    pub volume_mul: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct SlotRec {
    pub looped: u32,
    pub resume: u32,
    pub special: u32,
    pub volume: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct EventInfo {
    pub max_sanity: f32,
    pub used: u32,
    pub over: u32,
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12b_music_new() -> u64 {
    put(&MUSIC, MusicHandler::new(host()))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12b_music_free(h: u64) {
    free(&MUSIC, h);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12b_music_reset(h: u64) {
    with(&MUSIC, h, (), |m| m.reset(host()));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12b_music_update(h: u64, dt: f32) {
    with(&MUSIC, h, (), |m| m.update(host(), dt));
}

/// # Safety
/// `file` holds `n` readable bytes.
#[allow(clippy::too_many_arguments)]
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a12b_music_play(
    h: u64,
    file: *const u8,
    n: usize,
    looped: i32,
    volume: f32,
    fade: f32,
    prio: i32,
    resume: i32,
    special: i32,
) {
    let file = bytes(file, n);
    with(&MUSIC, h, (), |m| {
        m.play(
            host(),
            file,
            looped != 0,
            volume,
            fade,
            prio,
            resume != 0,
            special != 0,
        );
    });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12b_music_stop(h: u64, fade: f32, prio: i32) {
    with(&MUSIC, h, (), |m| m.stop(host(), fade, prio));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12b_music_on_map(h: u64) {
    with(&MUSIC, h, (), |m| m.on_map(host()));
}

/// `op`: 0 add, 1 remove, 2 contains.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12b_music_enemy(h: u64, op: i32, kind: i32, enemy: usize) -> i32 {
    with(&MUSIC, h, 0, |m| {
        let Some(set) = usize::try_from(kind).ok().and_then(|k| m.enemy_set(k)) else {
            return 0;
        };
        match op {
            0 => i32::from(set.insert(enemy)),
            1 => i32::from(set.remove(&enemy)),
            _ => i32::from(set.contains(&enemy)),
        }
    })
}

/// # Safety
/// `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a12b_music_state(h: u64, out: *mut MusicState) -> i32 {
    with(&MUSIC, h, 0, |m| {
        if out.is_null() {
            return 0;
        }
        let st = MusicState {
            max_prio: m.max_prio,
            slot_count: m.slots.len() as i32,
            cur_prio: m.cur_prio,
            close_playing: m.enemy.close_playing,
            playing: m.enemy.playing,
            volume_mul: m.volume_mul,
        };
        // SAFETY: `out` is writable per the contract.
        unsafe { out.write(st) };
        1
    })
}

/// Restores the fields a save holds: current priority and the playing flags.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12b_music_set_state(
    h: u64,
    cur_prio: i32,
    close_playing: i32,
    attack: i32,
    search: i32,
) {
    with(&MUSIC, h, (), |m| {
        m.cur_prio = cur_prio;
        m.enemy.close_playing = u32::from(close_playing != 0);
        m.enemy.playing[1] = u32::from(attack != 0);
        m.enemy.playing[0] = u32::from(search != 0);
    });
}

/// # Safety
/// `out` is writable; `sink` may be called once with the slot's file.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a12b_music_slot(
    h: u64,
    idx: i32,
    out: *mut SlotRec,
    sink: Option<StrSink>,
    ctx: *mut c_void,
) -> i32 {
    with(&MUSIC, h, 0, |m| {
        let Some(s) = usize::try_from(idx).ok().and_then(|i| m.slots.get(i)) else {
            return 0;
        };
        if !out.is_null() {
            let rec = SlotRec {
                looped: u32::from(s.looped),
                resume: u32::from(s.resume),
                special: u32::from(s.special),
                volume: s.volume,
            };
            // SAFETY: `out` is writable per the contract.
            unsafe { out.write(rec) };
        }
        if let Some(sink) = sink {
            // SAFETY: the sink accepts a byte range.
            unsafe { sink(ctx, s.file.as_ptr(), s.file.len()) };
        }
        1
    })
}

/// # Safety
/// `rec` is readable; `file` holds `n` readable bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a12b_music_set_slot(
    h: u64,
    idx: i32,
    rec: *const SlotRec,
    file: *const u8,
    n: usize,
) {
    if rec.is_null() {
        return;
    }
    // SAFETY: `rec` is readable per the contract.
    let rec = unsafe { rec.read() };
    let file = bytes(file, n).to_vec();
    with(&MUSIC, h, (), |m| {
        if let Some(s) = usize::try_from(idx).ok().and_then(|i| m.slots.get_mut(i)) {
            *s = GameMusic {
                file,
                looped: rec.looped != 0,
                resume: rec.resume != 0,
                volume: rec.volume,
                special: rec.special != 0,
            };
        }
    });
}

/// # Safety
/// `out` holds `cap` writable words.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a12b_music_enemies(
    h: u64,
    kind: i32,
    out: *mut usize,
    cap: usize,
) -> usize {
    with(&MUSIC, h, 0, |m| {
        let Some(set) = usize::try_from(kind).ok().and_then(|k| m.enemy_set(k)) else {
            return 0;
        };
        for (i, e) in set.iter().take(cap).enumerate() {
            // SAFETY: `i < cap`.
            unsafe { out.add(i).write(*e) };
        }
        set.len()
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12b_ins_new() -> u64 {
    put(&INSANITY, InsanityHandler::new(host()))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12b_ins_free(h: u64) {
    free(&INSANITY, h);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12b_ins_reset(h: u64) {
    with(&INSANITY, h, (), |m| m.reset(host()));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12b_ins_update(h: u64, dt: f32) {
    with(&INSANITY, h, (), |m| m.update(host(), dt));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12b_ins_draw(h: u64) {
    with(&INSANITY, h, (), |m| m.on_draw(host()));
}

/// `mode`: 0 random event, 1 by index, 2 by name.
///
/// # Safety
/// `name` holds `n` readable bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a12b_ins_start(
    h: u64,
    mode: i32,
    idx: i32,
    name: *const u8,
    n: usize,
) {
    let name = bytes(name, n);
    with(&INSANITY, h, (), |m| match mode {
        0 => m.start_random(host()),
        1 => m.start_index(host(), idx),
        _ => m.start_named(host(), name),
    });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12b_ins_stop(h: u64) {
    with(&INSANITY, h, (), |m| m.stop_current(host()));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12b_ins_possible(h: u64) -> i32 {
    with(&INSANITY, h, 0, |m| i32::from(m.possible(host())))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12b_ins_reload(h: u64) {
    with(&INSANITY, h, (), |m| m.reload(host()));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12b_ins_count(h: u64) -> i32 {
    with(&INSANITY, h, 0, |m| m.count())
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12b_ins_current(h: u64) -> i32 {
    with(&INSANITY, h, -1, |m| m.current())
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12b_ins_new_count(h: u64) -> f32 {
    with(&INSANITY, h, 0.0, |m| m.new_event_count())
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12b_ins_set_new_count(h: u64, v: f32) {
    with(&INSANITY, h, (), |m| m.set_new_event_count(v));
}

/// Name then set go to `sink` as two calls.
///
/// # Safety
/// `out` is writable; `sink` accepts byte ranges.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a12b_ins_event(
    h: u64,
    idx: i32,
    out: *mut EventInfo,
    sink: Option<StrSink>,
    ctx: *mut c_void,
) -> i32 {
    with(&INSANITY, h, 0, |m| {
        if idx < 0 || idx >= m.count() {
            return 0;
        }
        if !out.is_null() {
            let info = EventInfo {
                max_sanity: m.event_max_sanity(idx),
                used: u32::from(m.event_used(idx)),
                over: u32::from(m.event_over(idx)),
            };
            // SAFETY: `out` is writable per the contract.
            unsafe { out.write(info) };
        }
        if let Some(sink) = sink {
            for s in [m.event_name(idx), m.event_set(idx)] {
                // SAFETY: the sink accepts a byte range.
                unsafe { sink(ctx, s.as_ptr(), s.len()) };
            }
        }
        1
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12b_ins_set_used(h: u64, idx: i32, used: i32) {
    with(&INSANITY, h, (), |m| m.set_event_used(idx, used != 0));
}

/// `op`: 0 disable, 1 enable, 2 clear all.
///
/// # Safety
/// `name` holds `n` readable bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a12b_ins_set(h: u64, op: i32, name: *const u8, n: usize) {
    let name = bytes(name, n);
    with(&INSANITY, h, (), |m| match op {
        0 => m.disable_set(name),
        1 => m.enable_set(name),
        _ => m.clear_sets(),
    });
}

/// # Safety
/// `sink` accepts byte ranges.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a12b_ins_sets(h: u64, sink: Option<StrSink>, ctx: *mut c_void) {
    let Some(sink) = sink else { return };
    for s in with(&INSANITY, h, Vec::new(), |m| m.sets()) {
        // SAFETY: the sink accepts a byte range.
        unsafe { sink(ctx, s.as_ptr(), s.len()) };
    }
}
