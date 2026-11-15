//! C ABI of the effect handler, effect renderer and insanity post effect clock
//! (`amnesia/src/game/RustLuxFxA12a.h`). Objects live in thread-local arenas; the C++ shells hold
//! the handle bits.

use super::fxobj::{Ctx, Handler, State, VoiceData};
use super::host::host;
use super::postfx::Clock;
use super::rend::{self, Rend};
use crate::obj::{Arena, Handle};
use std::cell::RefCell;
use std::ffi::c_void;

type Sink = unsafe extern "C" fn(*mut c_void, *const u8, usize);

thread_local! {
    static HANDLERS: RefCell<Arena<Handler>> = RefCell::new(Arena::default());
    static RENDS: RefCell<Arena<Rend>> = RefCell::new(Arena::default());
    static CLOCKS: RefCell<Arena<Clock>> = RefCell::new(Arena::default());
}

const _: () = {
    use std::mem::size_of;
    assert!(size_of::<State>() == 196);
    assert!(size_of::<VoiceIn>() == 224);
    assert!(size_of::<VoiceSave>() == 224);
};

fn bytes<'a>(p: *const u8, n: usize) -> &'a [u8] {
    if p.is_null() || n == 0 {
        return &[];
    }
    // SAFETY: the caller passes `n` readable bytes.
    unsafe { std::slice::from_raw_parts(p, n) }
}

fn units<'a>(p: *const u32, n: usize) -> &'a [u32] {
    if p.is_null() || n == 0 {
        return &[];
    }
    // SAFETY: the caller passes `n` readable units.
    unsafe { std::slice::from_raw_parts(p, n) }
}

fn fx<R>(bits: u64, none: R, f: impl FnOnce(&Ctx) -> R) -> R {
    let (Some(h), Some(host)) = (Handle::from_bits(bits), host()) else {
        return none;
    };
    HANDLERS.with(|a| {
        f(&Ctx {
            arena: a,
            h,
            fx: &host,
        })
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12a_fx_new() -> u64 {
    let Some(host) = host() else { return 0 };
    let h = Handler::new(&host);
    HANDLERS.with(|a| a.borrow_mut().insert(h).map_or(0, Handle::to_bits))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12a_fx_free(h: u64) {
    if let Some(h) = Handle::from_bits(h) {
        let old = HANDLERS.with(|a| a.borrow_mut().remove(h));
        drop(old);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12a_fx_update(h: u64, dt: f32) {
    fx(h, (), |c| c.update(dt));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12a_fx_draw(h: u64) {
    fx(h, (), |c| c.draw());
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12a_fx_reset(h: u64) {
    fx(h, (), |c| c.reset());
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12a_fx_map_leave(h: u64) {
    fx(h, (), |c| c.on_map_leave());
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12a_fx_action(h: u64, pressed: i32) {
    fx(h, (), |c| c.do_action(pressed != 0));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12a_fx_fonts(h: u64, load: i32) {
    fx(h, (), |c| {
        if load != 0 {
            c.load_fonts();
        } else {
            c.clear_fonts();
        }
    });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12a_fx_paused_set(h: u64, on: i32) {
    fx(h, (), |c| c.set_player_paused(on != 0));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12a_fx_paused_get(h: u64) -> i32 {
    fx(h, 0, |c| c.with(|s| i32::from(s.paused)).unwrap_or(0))
}

/// Effect indices: 0 fade, 1 flash, 2 sanity flash, 3 voice, 4 image trail, 5 shake, 6 sepia,
/// 7 radial blur, 8 emotion flash, 9 commentary.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12a_fx_active(h: u64, which: i32) -> i32 {
    fx(h, 0, |c| {
        c.with(|s| {
            i32::from(match which {
                0 => s.fade.active != 0,
                1 => s.flash.active != 0,
                2 => s.sanity.active != 0,
                3 => s.voice.active,
                4 => s.trail.active != 0,
                5 => s.shake_active,
                6 => s.sepia.active != 0,
                7 => s.radial.active != 0,
                8 => s.emotion.active != 0,
                9 => s.commentary_active,
                _ => false,
            })
        })
        .unwrap_or(0)
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12a_fx_fade(h: u64, op: i32, t: f32) -> i32 {
    fx(h, 0, |c| match op {
        0 => {
            c.fade_in(t);
            0
        }
        1 => {
            c.fade_out(t);
            0
        }
        2 => {
            c.fade_direct(t);
            0
        }
        _ => i32::from(c.is_fading()),
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12a_fx_flash_start(h: u64, a: f32, b: f32, c: f32) {
    fx(h, (), |x| x.flash_start(a, b, c));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12a_fx_sanity(h: u64, op: i32, dt: f32, set: usize) {
    fx(h, (), |c| match op {
        0 => c.sanity_start(),
        1 => c.sanity_update(dt),
        _ => c.sanity_draw(set),
    });
}

/// `kind` 0 radial blur, 1 sepia, 2 image trail.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12a_fx_fade_to(h: u64, kind: i32, goal: f32, speed: f32) {
    fx(h, (), |c| c.amount_fade_to(kind, goal, speed));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12a_fx_radial_dist(h: u64, d: f32) {
    fx(h, (), |c| c.radial_start_dist(d));
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12a_fx_shake(h: u64, amount: f32, time: f32, fade_in: f32, fade_out: f32) {
    fx(h, (), |c| c.shake_start(amount, time, fade_in, fade_out));
}

/// # Safety
/// Every string is `n` readable bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a12a_fx_emotion_start(
    h: u64,
    cat: *const u8,
    cn: usize,
    entry: *const u8,
    en: usize,
    sound: *const u8,
    sn: usize,
) {
    fx(h, (), |c| {
        c.emotion_start(bytes(cat, cn), bytes(entry, en), bytes(sound, sn));
    });
}

/// `op` 0 start, 1 stop.
///
/// # Safety
/// `file` is `n` readable bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a12a_fx_commentary(
    h: u64,
    op: i32,
    file: *const u8,
    n: usize,
    icon: i32,
) {
    fx(h, (), |c| {
        if op == 0 {
            c.commentary_start(icon, bytes(file, n));
        } else {
            c.commentary_stop();
        }
    });
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Str {
    pub p: *const u8,
    pub n: usize,
}

/// `AddMultiSubbedVoice` arguments.
#[repr(C)]
pub struct VoiceIn {
    pub voice: Str,
    pub effect: Str,
    pub cat: Str,
    pub entries: [Str; 7],
    pub delays: [f32; 7],
    pub use_pos: i32,
    pub pos: [f32; 3],
    pub min: f32,
    pub max: f32,
    pub priority: i32,
    pub remove_interrupted: i32,
}

fn s(x: &Str) -> &[u8] {
    bytes(x.p, x.n)
}

/// # Safety
/// `v` points to a valid `VoiceIn` whose strings are readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a12a_fx_voice_add(h: u64, v: *const VoiceIn) {
    // SAFETY: caller contract.
    let Some(v) = (unsafe { v.as_ref() }) else {
        return;
    };
    let e = &v.entries;
    let [e0, e1, e2, e3, e4, e5, e6] = [0, 1, 2, 3, 4, 5, 6].map(|i| e.get(i).map_or(&[][..], s));
    fx(h, (), |c| {
        c.voice_add(
            s(&v.voice),
            s(&v.effect),
            s(&v.cat),
            [e0, e1, e2, e3, e4, e5, e6],
            v.delays,
            v.use_pos != 0,
            v.pos,
            v.min,
            v.max,
            v.priority,
            v.remove_interrupted != 0,
        );
    });
}

/// `op` 0 stop (arg = fade speed), 1 pause, 2 unpause, 3 volume (arg = mul), 4 done playing.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12a_fx_voice(h: u64, op: i32, arg: f32) -> i32 {
    fx(h, 0, |c| match op {
        0 => {
            c.voice_stop(arg);
            0
        }
        1 => {
            c.voice_pause();
            0
        }
        2 => {
            c.voice_unpause();
            0
        }
        3 => {
            c.voice_set_volume(arg);
            0
        }
        _ => i32::from(c.voice_done_playing()),
    })
}

/// # Safety
/// `p` is `n` readable bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a12a_fx_voice_cb_set(h: u64, p: *const u8, n: usize) {
    fx(h, (), |c| {
        c.with(|s| s.voice.over_cb = bytes(p, n).to_vec());
    });
}

/// # Safety
/// `sink` accepts the bytes handed to it.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a12a_fx_voice_cb_get(h: u64, sink: Sink, ctx: *mut c_void) {
    let cb = fx(h, Vec::new(), |c| {
        c.with(|s| s.voice.over_cb.clone()).unwrap_or_default()
    });
    // SAFETY: caller contract.
    unsafe { sink(ctx, cb.as_ptr(), cb.len()) };
}

/// # Safety
/// `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a12a_fx_get_state(h: u64, out: *mut State) {
    let st = fx(h, State::default(), |c| c.state());
    // SAFETY: caller contract.
    if let Some(o) = unsafe { out.as_mut() } {
        *o = st;
    }
}

/// # Safety
/// `st` is readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a12a_fx_set_state(h: u64, st: *const State) {
    // SAFETY: caller contract.
    if let Some(st) = unsafe { st.as_ref() } {
        fx(h, (), |c| c.set_state(st));
    }
}

/// One queued voice for the save game; strings point into Rust memory until the next change.
#[repr(C)]
pub struct VoiceSave {
    pub texts: [Texts; 7],
    pub delays: [f32; 7],
    pub voice: Str,
    pub effect: Str,
    pub use_pos: i32,
    pub pos: [f32; 3],
    pub min: f32,
    pub max: f32,
    pub priority: i32,
    pub current_time: f64,
    pub interrupted_at: f64,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Texts {
    pub p: *const u32,
    pub n: usize,
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12a_fx_voice_count(h: u64) -> usize {
    fx(h, 0, |c| c.with(|s| s.voice.list.len()).unwrap_or(0))
}

/// # Safety
/// `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a12a_fx_voice_get(h: u64, i: usize, out: *mut VoiceSave) -> i32 {
    // SAFETY: caller contract.
    let Some(o) = (unsafe { out.as_mut() }) else {
        return 0;
    };
    fx(h, 0, |c| {
        c.with(|s| {
            let Some(v) = s.voice.list.get(i) else {
                return 0;
            };
            for (dst, src) in o.texts.iter_mut().zip(v.texts.iter()) {
                *dst = Texts {
                    p: src.as_ptr(),
                    n: src.len(),
                };
            }
            o.delays = v.delays;
            o.voice = Str {
                p: v.voice_file.as_ptr(),
                n: v.voice_file.len(),
            };
            o.effect = Str {
                p: v.effect_file.as_ptr(),
                n: v.effect_file.len(),
            };
            o.use_pos = i32::from(v.use_pos);
            o.pos = v.pos;
            o.min = v.min;
            o.max = v.max;
            o.priority = v.priority;
            o.current_time = v.current_time;
            o.interrupted_at = v.interrupted_at;
            1
        })
        .unwrap_or(0)
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12a_fx_voice_clear(h: u64) {
    fx(h, (), |c| {
        c.with(|s| s.voice.list.clear());
    });
}

/// # Safety
/// `v` is readable with readable strings.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a12a_fx_voice_push(h: u64, v: *const VoiceSave) {
    // SAFETY: caller contract.
    let Some(v) = (unsafe { v.as_ref() }) else {
        return;
    };
    let mut d = VoiceData {
        delays: v.delays,
        voice_file: s(&v.voice).to_vec(),
        effect_file: s(&v.effect).to_vec(),
        use_pos: v.use_pos != 0,
        pos: v.pos,
        min: v.min,
        max: v.max,
        priority: v.priority,
        current_time: v.current_time,
        interrupted_at: v.interrupted_at,
        ..VoiceData::default()
    };
    for (dst, src) in d.texts.iter_mut().zip(v.texts.iter()) {
        *dst = units(src.p, src.n).to_vec();
    }
    fx(h, (), |c| {
        c.with(|s| s.voice.list.push(d));
    });
}

// ---------------------------------------------------------------- renderer

fn rend<R>(bits: u64, none: R, f: impl FnOnce(&mut Rend) -> R) -> R {
    let Some(h) = Handle::from_bits(bits) else {
        return none;
    };
    RENDS.with(|a| match a.try_borrow_mut() {
        Ok(mut a) => a.get_mut(h).map_or(none, f),
        Err(_) => none,
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12a_rend_new(programs: u32) -> u64 {
    RENDS.with(|a| {
        a.borrow_mut()
            .insert(Rend::new(programs))
            .map_or(0, Handle::to_bits)
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12a_rend_free(h: u64) {
    if let Some(h) = Handle::from_bits(h) {
        let old = RENDS.with(|a| a.borrow_mut().remove(h));
        drop(old);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12a_rend_update(h: u64, dt: f32) {
    rend(h, (), |r| r.osc.update(dt));
}

/// `op` 0 reset, 1 clear render lists, 2 clear outline objects.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12a_rend_clear(h: u64, op: i32) {
    rend(h, (), |r| match op {
        0 => r.reset(),
        1 => r.clear_lists(),
        _ => r.outline.clear(),
    });
}

/// `kind` 0 flash, 1 enemy glow, 2 outline (alpha unused).
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12a_rend_add(h: u64, kind: i32, obj: usize, alpha: f32) {
    rend(h, (), |r| match kind {
        0 => r.flash.push((obj, alpha)),
        1 => r.glow.push((obj, alpha)),
        _ => r.outline.push(obj),
    });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12a_rend_render(h: u64, fns: usize) {
    let (Some(snap), Some(host)) = (rend(h, None, |r| Some(r.clone())), host()) else {
        return;
    };
    rend::render_trans(&host, fns, &snap);
}

// ---------------------------------------------------------------- insanity clock

fn clock<R>(bits: u64, none: R, f: impl FnOnce(&mut Clock) -> R) -> R {
    let Some(h) = Handle::from_bits(bits) else {
        return none;
    };
    CLOCKS.with(|a| match a.try_borrow_mut() {
        Ok(mut a) => a.get_mut(h).map_or(none, f),
        Err(_) => none,
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12a_ins_new() -> u64 {
    CLOCKS.with(|a| {
        a.borrow_mut()
            .insert(Clock::default())
            .map_or(0, Handle::to_bits)
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12a_ins_free(h: u64) {
    if let Some(h) = Handle::from_bits(h) {
        CLOCKS.with(|a| a.borrow_mut().remove(h));
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12a_ins_update(h: u64, dt: f32) {
    clock(h, (), |c| rend::ins_update(c, dt));
}

/// `which` 0 wave alpha, 1 zoom alpha, 2 wave speed.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12a_ins_set(h: u64, which: i32, v: f32) {
    clock(h, (), |c| match which {
        0 => c.wave_alpha = v,
        1 => c.zoom_alpha = v,
        _ => c.wave_speed = v,
    });
}

/// # Safety
/// `out` has room for five floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a12a_ins_get(h: u64, out: *mut f32) {
    let c = clock(h, Clock::default(), |c| *c);
    if out.is_null() {
        return;
    }
    // SAFETY: caller contract.
    let o = unsafe { std::slice::from_raw_parts_mut(out, 5) };
    for (d, v) in o
        .iter_mut()
        .zip([c.t, c.anim_count, c.wave_alpha, c.zoom_alpha, c.wave_speed])
    {
        *d = v;
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a12a_ins_render(
    h: u64,
    sx: f32,
    sy: f32,
    program_ok: i32,
    effect: usize,
    input: usize,
    fb: usize,
) {
    let (c, Some(host)) = (clock(h, Clock::default(), |c| *c), host()) else {
        return;
    };
    rend::ins_render(&host, &c, [sx, sy], program_ok != 0, effect, input, fb);
}
