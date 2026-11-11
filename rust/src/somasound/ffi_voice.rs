//! C ABI of the voice scenes, mirrored in `soma/src/game/RustSomaSound.h`.

use super::voice::{UpdateEvent, VISEMES, VoiceHost, Voices, decode_text, spectrum};
use crate::string_ffi::units;
use std::ffi::c_void;

type Ctx = *mut c_void;
type Sink2 = unsafe extern "C" fn(Ctx, *const u8, usize);

#[repr(C)]
pub struct OhplGs5VoiceHost {
    pub ctx: Ctx,
    pub entry_valid: unsafe extern "C" fn(Ctx, usize, i32) -> bool,
    pub entry_set_volume_mul: unsafe extern "C" fn(Ctx, usize, f32),
    pub entry_fade_out: unsafe extern "C" fn(Ctx, usize, f32),
    pub entry_set_paused: unsafe extern "C" fn(Ctx, usize, bool),
    pub channel_set_position: unsafe extern "C" fn(Ctx, usize, *const f32),
    pub channel_filter: unsafe extern "C" fn(Ctx, usize, bool, f32),
    pub channel_total: unsafe extern "C" fn(Ctx, usize) -> f64,
    pub channel_elapsed: unsafe extern "C" fn(Ctx, usize) -> f64,
    pub listener_pos: unsafe extern "C" fn(Ctx, *mut f32),
    pub map_entity_pos: unsafe extern "C" fn(Ctx, *const u8, usize, *mut f32) -> bool,
    pub map_current: unsafe extern "C" fn(Ctx) -> bool,
    pub file_exists: unsafe extern "C" fn(Ctx, *const u8, usize) -> bool,
    pub read_file: unsafe extern "C" fn(Ctx, *const u8, usize, Sink2, Ctx) -> bool,
    pub play_3d: unsafe extern "C" fn(
        Ctx,
        *const u8,
        usize,
        f32,
        *const f32,
        f32,
        f32,
        i32,
        *mut usize,
        *mut i32,
    ) -> bool,
    pub play_gui_stream:
        unsafe extern "C" fn(Ctx, *const u8, usize, f32, i32, *mut usize, *mut i32) -> bool,
    pub effect_play:
        unsafe extern "C" fn(Ctx, *const u8, usize, f32, i32, *mut usize, *mut i32) -> bool,
    pub effect_live: unsafe extern "C" fn(Ctx, usize, i32) -> bool,
    pub effect_set_volume_mul: unsafe extern "C" fn(Ctx, usize, f32),
    pub effect_fade_out: unsafe extern "C" fn(Ctx, usize, f32),
    pub effect_set_paused: unsafe extern "C" fn(Ctx, usize, bool),
    pub effect_is_playing: unsafe extern "C" fn(Ctx, usize) -> bool,
    pub effect_total: unsafe extern "C" fn(Ctx, usize) -> f32,
    pub effect_elapsed: unsafe extern "C" fn(Ctx, usize) -> f32,
    pub played_has: unsafe extern "C" fn(Ctx, *const u8, usize) -> bool,
    pub played_insert: unsafe extern "C" fn(Ctx, *const u8, usize) -> bool,
    pub log: unsafe extern "C" fn(Ctx, *const u8, usize),
    pub error: unsafe extern "C" fn(Ctx, *const u8, usize),
    pub wrap_rows: unsafe extern "C" fn(Ctx, *const u8, usize, f32, f32, *const f32) -> usize,
}

struct CHost<'a>(&'a OhplGs5VoiceHost);

fn collect(h: &OhplGs5VoiceHost, f: impl FnOnce(Sink2, Ctx)) -> Vec<u8> {
    unsafe extern "C" fn sink(c: Ctx, p: *const u8, n: usize) {
        // SAFETY: `c` is the `Vec` below; `p` holds `n` bytes.
        unsafe { (*c.cast::<Vec<u8>>()).extend_from_slice(units(p, n)) };
    }
    let _ = h;
    let mut v: Vec<u8> = Vec::new();
    f(sink, (&raw mut v).cast());
    v
}

// SAFETY (all calls below): the entry point's caller guarantees the table accepts its `ctx`.
impl VoiceHost for CHost<'_> {
    fn entry_valid(&mut self, entry: usize, id: i32) -> bool {
        unsafe { (self.0.entry_valid)(self.0.ctx, entry, id) }
    }
    fn entry_set_volume_mul(&mut self, entry: usize, vol: f32) {
        unsafe { (self.0.entry_set_volume_mul)(self.0.ctx, entry, vol) }
    }
    fn entry_fade_out(&mut self, entry: usize, speed: f32) {
        unsafe { (self.0.entry_fade_out)(self.0.ctx, entry, speed) }
    }
    fn entry_set_paused(&mut self, entry: usize, on: bool) {
        unsafe { (self.0.entry_set_paused)(self.0.ctx, entry, on) }
    }
    fn channel_set_position(&mut self, entry: usize, pos: [f32; 3]) {
        unsafe { (self.0.channel_set_position)(self.0.ctx, entry, pos.as_ptr()) }
    }
    fn channel_filter(&mut self, entry: usize, high: bool, gain: f32) {
        unsafe { (self.0.channel_filter)(self.0.ctx, entry, high, gain) }
    }
    fn channel_total(&mut self, entry: usize) -> f64 {
        unsafe { (self.0.channel_total)(self.0.ctx, entry) }
    }
    fn channel_elapsed(&mut self, entry: usize) -> f64 {
        unsafe { (self.0.channel_elapsed)(self.0.ctx, entry) }
    }
    fn listener_pos(&mut self) -> [f32; 3] {
        let mut b = [0.0f32; 3];
        unsafe { (self.0.listener_pos)(self.0.ctx, b.as_mut_ptr()) };
        b
    }
    fn map_entity_pos(&mut self, name: &[u8]) -> Option<[f32; 3]> {
        let mut b = [0.0f32; 3];
        unsafe { (self.0.map_entity_pos)(self.0.ctx, name.as_ptr(), name.len(), b.as_mut_ptr()) }
            .then_some(b)
    }
    fn map_current(&mut self) -> bool {
        unsafe { (self.0.map_current)(self.0.ctx) }
    }
    fn file_exists(&mut self, file: &[u8]) -> bool {
        unsafe { (self.0.file_exists)(self.0.ctx, file.as_ptr(), file.len()) }
    }
    fn read_file(&mut self, file: &[u8]) -> Option<Vec<u8>> {
        let mut opened = false;
        let data = collect(self.0, |sink, c| {
            opened = unsafe { (self.0.read_file)(self.0.ctx, file.as_ptr(), file.len(), sink, c) };
        });
        opened.then_some(data)
    }
    fn play_3d(
        &mut self,
        file: &[u8],
        vol: f32,
        pos: [f32; 3],
        min: f32,
        max: f32,
        ty: i32,
    ) -> Option<(usize, i32)> {
        let (mut e, mut id) = (0usize, 0i32);
        unsafe {
            (self.0.play_3d)(
                self.0.ctx,
                file.as_ptr(),
                file.len(),
                vol,
                pos.as_ptr(),
                min,
                max,
                ty,
                &mut e,
                &mut id,
            )
        }
        .then_some((e, id))
    }
    fn play_gui_stream(&mut self, file: &[u8], vol: f32, ty: i32) -> Option<(usize, i32)> {
        let (mut e, mut id) = (0usize, 0i32);
        unsafe {
            (self.0.play_gui_stream)(
                self.0.ctx,
                file.as_ptr(),
                file.len(),
                vol,
                ty,
                &mut e,
                &mut id,
            )
        }
        .then_some((e, id))
    }
    fn effect_play(&mut self, file: &[u8], vol: f32, ty: i32) -> Option<(usize, i32)> {
        let (mut e, mut id) = (0usize, 0i32);
        unsafe {
            (self.0.effect_play)(
                self.0.ctx,
                file.as_ptr(),
                file.len(),
                vol,
                ty,
                &mut e,
                &mut id,
            )
        }
        .then_some((e, id))
    }
    fn effect_live(&mut self, effect: usize, id: i32) -> bool {
        unsafe { (self.0.effect_live)(self.0.ctx, effect, id) }
    }
    fn effect_set_volume_mul(&mut self, effect: usize, vol: f32) {
        unsafe { (self.0.effect_set_volume_mul)(self.0.ctx, effect, vol) }
    }
    fn effect_fade_out(&mut self, effect: usize, speed: f32) {
        unsafe { (self.0.effect_fade_out)(self.0.ctx, effect, speed) }
    }
    fn effect_set_paused(&mut self, effect: usize, on: bool) {
        unsafe { (self.0.effect_set_paused)(self.0.ctx, effect, on) }
    }
    fn effect_is_playing(&mut self, effect: usize) -> bool {
        unsafe { (self.0.effect_is_playing)(self.0.ctx, effect) }
    }
    fn effect_total(&mut self, effect: usize) -> f32 {
        unsafe { (self.0.effect_total)(self.0.ctx, effect) }
    }
    fn effect_elapsed(&mut self, effect: usize) -> f32 {
        unsafe { (self.0.effect_elapsed)(self.0.ctx, effect) }
    }
    fn played_has(&mut self, key: &[u8]) -> bool {
        unsafe { (self.0.played_has)(self.0.ctx, key.as_ptr(), key.len()) }
    }
    fn played_insert(&mut self, key: &[u8]) -> bool {
        unsafe { (self.0.played_insert)(self.0.ctx, key.as_ptr(), key.len()) }
    }
    fn log(&mut self, text: &[u8]) {
        unsafe { (self.0.log)(self.0.ctx, text.as_ptr(), text.len()) }
    }
    fn error(&mut self, text: &[u8]) {
        unsafe { (self.0.error)(self.0.ctx, text.as_ptr(), text.len()) }
    }
    fn wrap_rows(&mut self, text: &[u8], width: f32, line_height: f32, size: [f32; 2]) -> usize {
        unsafe {
            (self.0.wrap_rows)(
                self.0.ctx,
                text.as_ptr(),
                text.len(),
                width,
                line_height,
                size.as_ptr(),
            )
        }
    }
}

/// # Safety
/// `v` is a live handle.
unsafe fn voices<'a>(v: *mut Voices) -> &'a mut Voices {
    // SAFETY: caller contract.
    unsafe { &mut *v }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs5_voice_new() -> *mut Voices {
    Box::into_raw(Box::new(Voices::new()))
}

/// # Safety
/// `v` came from `ohpl_gs5_voice_new`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_voice_free(v: *mut Voices) {
    if !v.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(v) });
    }
}

/// `ReadTextFile`'s decoding of a file's bytes.
///
/// # Safety
/// `data` is readable for `n` bytes; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_decode_text(data: *const u8, n: usize, sink: Sink2, ctx: Ctx) {
    // SAFETY: caller contract.
    let out = decode_text(unsafe { units(data, n) });
    // SAFETY: caller contract.
    unsafe { sink(ctx, out.as_ptr(), out.len()) };
}

/// # Safety
/// `v` is live; `set` is readable for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_voice_mark_set(v: *mut Voices, set: *const u8, n: usize) -> bool {
    // SAFETY: caller contract.
    unsafe { voices(v).mark_set(units(set, n)) }
}

/// # Safety
/// `v` is live; pointers are readable for their lengths.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_voice_load_lang(v: *mut Voices, text: *const u8, n: usize) {
    // SAFETY: caller contract.
    unsafe { voices(v).load_lang(units(text, n)) }
}

/// The number of subjects loaded, -1 when the text did not load.
///
/// # Safety
/// `v` is live; pointers are readable for their lengths.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_voice_load_voice(
    v: *mut Voices,
    text: *const u8,
    n: usize,
    set: *const u8,
    sn: usize,
) -> i64 {
    // SAFETY: caller contract.
    unsafe { voices(v).load_voice(units(text, n), units(set, sn)) }.map_or(-1, |c| c as i64)
}

macro_rules! host {
    ($h:ident) => {
        // SAFETY: caller contract.
        CHost(unsafe { &*$h })
    };
}

/// # Safety
/// `v` is live; `name` is readable for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_voice_subject_exists(
    v: *mut Voices,
    name: *const u8,
    n: usize,
) -> bool {
    // SAFETY: caller contract.
    unsafe { voices(v).subject_exists(units(name, n)) }
}

/// # Safety
/// `v` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_voice_any_active(v: *mut Voices) -> bool {
    // SAFETY: caller contract.
    unsafe { voices(v).any_active() }
}

/// 1: stop the subject's scene then call `play_start`; 0: busy; -1: unknown subject.
///
/// # Safety
/// `v` is live; `host` is a valid table.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_voice_play_check(
    v: *mut Voices,
    host: *const OhplGs5VoiceHost,
    subject: *const u8,
    n: usize,
    prio: i32,
) -> i32 {
    let mut h = host!(host);
    // SAFETY: caller contract.
    unsafe { voices(v).play_check(&mut h, units(subject, n), prio) }
}

/// # Safety
/// `v` is live; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_voice_scene_of(
    v: *mut Voices,
    subject: *const u8,
    n: usize,
    sink: Sink2,
    ctx: Ctx,
) {
    // SAFETY: caller contract.
    let s = unsafe { voices(v).scene_of_subject(units(subject, n)) };
    // SAFETY: caller contract.
    unsafe { sink(ctx, s.as_ptr(), s.len()) };
}

/// # Safety
/// `v` is live; `host` is a valid table; pointers are readable for their lengths.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_voice_play_start(
    v: *mut Voices,
    host: *const OhplGs5VoiceHost,
    subject: *const u8,
    n: usize,
    line: i32,
    callback: *const u8,
    cn: usize,
    prio: i32,
    on_done: u64,
    play_while_paused: bool,
) -> bool {
    let mut h = host!(host);
    // SAFETY: caller contract.
    unsafe {
        voices(v).play_start(
            &mut h,
            units(subject, n),
            line,
            units(callback, cn),
            prio,
            on_done,
            play_while_paused,
        )
    }
}

/// The completion token of the next playing of `scene` stopped and removed, -1 when none is left.
///
/// # Safety
/// `v` is live; `host` is a valid table.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_voice_stop_next(
    v: *mut Voices,
    host: *const OhplGs5VoiceHost,
    scene: *const u8,
    n: usize,
) -> i64 {
    let mut h = host!(host);
    // SAFETY: caller contract.
    unsafe { voices(v).stop_next(&mut h, units(scene, n)) }.map_or(-1, |e| e.on_done as i64)
}

/// # Safety
/// `v` is live; `host` is a valid table.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_voice_stop_all(v: *mut Voices, host: *const OhplGs5VoiceHost) {
    let mut h = host!(host);
    // SAFETY: caller contract.
    unsafe { voices(v).stop_all(&mut h) }
}

/// # Safety
/// `v` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_voice_reset_state(v: *mut Voices) {
    // SAFETY: caller contract.
    unsafe { voices(v).reset_state() }
}

/// # Safety
/// `v` is live; `host` is a valid table.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_voice_skip_line(
    v: *mut Voices,
    host: *const OhplGs5VoiceHost,
    scene: *const u8,
    n: usize,
) {
    let mut h = host!(host);
    // SAFETY: caller contract.
    unsafe { voices(v).skip_current_line(&mut h, units(scene, n)) }
}

/// # Safety
/// `v` is live; `name` is readable for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_voice_character_is_speaking(
    v: *mut Voices,
    name: *const u8,
    n: usize,
) -> bool {
    // SAFETY: caller contract.
    unsafe { voices(v).character_is_speaking(units(name, n)) }
}

/// # Safety
/// `v` is live; pointers are readable for their lengths.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_voice_set_source(
    v: *mut Voices,
    character: *const u8,
    cn: usize,
    entity: *const u8,
    en: usize,
    min: f32,
    max: f32,
    use_3d: bool,
    listen_range: f32,
    min_freq: f32,
    max_freq: f32,
    freq_flags: i32,
) {
    // SAFETY: caller contract.
    unsafe {
        voices(v).set_source(
            units(character, cn),
            units(entity, en),
            min,
            max,
            use_3d,
            listen_range,
            min_freq,
            max_freq,
            freq_flags,
        );
    }
}

/// # Safety
/// `v` is live; `host` is a valid table.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_voice_set_focus_scene(
    v: *mut Voices,
    host: *const OhplGs5VoiceHost,
    scene: *const u8,
    n: usize,
) {
    let mut h = host!(host);
    // SAFETY: caller contract.
    unsafe { voices(v).set_focus_scene(&mut h, units(scene, n)) }
}

/// # Safety
/// `v` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_voice_fade_scene_volume(
    v: *mut Voices,
    scene: *const u8,
    n: usize,
    volume: f32,
    time: f32,
) {
    // SAFETY: caller contract.
    unsafe { voices(v).fade_scene_volume_to(units(scene, n), volume, time) }
}

/// # Safety
/// `v` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_voice_fade_to(v: *mut Voices, goal: f32, time: f32) {
    // SAFETY: caller contract.
    unsafe { voices(v).fade_to(goal, time) }
}

/// # Safety
/// `v` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_voice_fade_alpha(v: *mut Voices) -> f32 {
    // SAFETY: caller contract.
    unsafe { voices(v).fade_alpha }
}

/// # Safety
/// `v` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_voice_is_fading(v: *mut Voices) -> bool {
    // SAFETY: caller contract.
    unsafe { voices(v).is_fading() }
}

/// # Safety
/// `v` is live; `host` is a valid table.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_voice_update_begin(
    v: *mut Voices,
    host: *const OhplGs5VoiceHost,
    dt: f32,
    game_paused: bool,
) {
    let mut h = host!(host);
    // SAFETY: caller contract.
    unsafe { voices(v).update_begin(&mut h, dt, game_paused) }
}

#[repr(C)]
pub struct OhplGs5VoiceEvent {
    pub kind: i32,
    pub func: *const u8,
    pub func_len: usize,
    pub scene: *const u8,
    pub scene_len: usize,
    pub name: *const u8,
    pub name_len: usize,
    pub on_done: u64,
    pub line: i32,
    pub start: bool,
}

/// Runs the per-scene loop until a callback is due: true with `sink` called for it, false at the
/// end of the loop.
///
/// # Safety
/// `v` is live; `host` is a valid table; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_voice_update_step(
    v: *mut Voices,
    host: *const OhplGs5VoiceHost,
    sink: unsafe extern "C" fn(Ctx, *const OhplGs5VoiceEvent),
    ctx: Ctx,
) -> bool {
    let mut h = host!(host);
    // SAFETY: caller contract.
    let Some(ev) = (unsafe { voices(v).update_step(&mut h) }) else {
        return false;
    };
    let e = match &ev {
        UpdateEvent::Finish {
            callback,
            scene,
            name,
            on_done,
        } => OhplGs5VoiceEvent {
            kind: 0,
            func: callback.as_ptr(),
            func_len: callback.len(),
            scene: scene.as_ptr(),
            scene_len: scene.len(),
            name: name.as_ptr(),
            name_len: name.len(),
            on_done: *on_done,
            line: 0,
            start: false,
        },
        UpdateEvent::Line {
            func,
            scene,
            name,
            line,
            start,
        } => OhplGs5VoiceEvent {
            kind: 1,
            func: func.as_ptr(),
            func_len: func.len(),
            scene: scene.as_ptr(),
            scene_len: scene.len(),
            name: name.as_ptr(),
            name_len: name.len(),
            on_done: 0,
            line: *line,
            start: *start,
        },
    };
    // SAFETY: caller contract.
    unsafe { sink(ctx, &e) };
    true
}

/// Who started (true) or stopped speaking since the last call.
///
/// # Safety
/// `v` is live; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_voice_speaking_changes(
    v: *mut Voices,
    sink: unsafe extern "C" fn(Ctx, *const u8, usize, bool),
    ctx: Ctx,
) {
    // SAFETY: caller contract.
    for (name, started) in unsafe { voices(v).speaking_changes() } {
        // SAFETY: caller contract.
        unsafe { sink(ctx, name.as_ptr(), name.len(), started) };
    }
}

/// # Safety
/// `v` is live; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_voice_speaking_func(
    v: *mut Voices,
    name: *const u8,
    n: usize,
    sink: Sink2,
    ctx: Ctx,
) -> bool {
    // SAFETY: caller contract.
    let Some(f) = (unsafe { voices(v).speaking_func(units(name, n)) }) else {
        return false;
    };
    // SAFETY: caller contract.
    unsafe { sink(ctx, f.as_ptr(), f.len()) };
    true
}

/// # Safety
/// `v` is live; pointers are readable for their lengths.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_voice_speaking_done(
    v: *mut Voices,
    name: *const u8,
    n: usize,
    func: *const u8,
    fnn: usize,
    keep: bool,
) {
    // SAFETY: caller contract.
    unsafe { voices(v).speaking_done(units(name, n), units(func, fnn), keep) }
}

/// # Safety
/// `v` is live; pointers are readable for their lengths.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_voice_set_speaking_callback(
    v: *mut Voices,
    name: *const u8,
    n: usize,
    func: *const u8,
    fnn: usize,
) {
    // SAFETY: caller contract.
    unsafe { voices(v).set_speaking_callback(units(name, n), units(func, fnn)) }
}

#[repr(C)]
pub struct OhplGs5Str {
    pub p: *const u8,
    pub n: usize,
}

/// # Safety
/// `v` is live; `rows` holds `count` strings; `name` is readable for `nn` bytes; `color` holds 4 floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_voice_add_subtitle(
    v: *mut Voices,
    rows: *const OhplGs5Str,
    count: usize,
    name: *const u8,
    nn: usize,
    color: *const f32,
    drawn_chars: i32,
) {
    let mut r = Vec::new();
    for i in 0..count {
        // SAFETY: caller contract.
        let s = unsafe { &*rows.add(i) };
        // SAFETY: caller contract.
        r.push(unsafe { units(s.p, s.n) }.to_vec());
    }
    // SAFETY: caller contract.
    let c = unsafe { std::slice::from_raw_parts(color, 4) };
    let col = c.try_into().unwrap_or([0.0; 4]);
    // SAFETY: caller contract.
    unsafe { voices(v).add_subtitle(r, units(name, nn), col, drawn_chars) }
}

pub type SubtitleSink =
    unsafe extern "C" fn(Ctx, *const u8, usize, f32, bool, *const f32, f32, usize);

/// # Safety
/// `v` is live; `host` is a valid table; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_voice_subtitles(
    v: *mut Voices,
    host: *const OhplGs5VoiceHost,
    screen_h: f32,
    game_paused: bool,
    sink: SubtitleSink,
    ctx: Ctx,
) {
    let mut h = host!(host);
    // SAFETY: caller contract.
    for d in unsafe { voices(v).subtitles(&mut h, screen_h, game_paused) } {
        let color = d.color.unwrap_or([0.0; 4]);
        // SAFETY: caller contract.
        unsafe {
            sink(
                ctx,
                d.text.as_ptr(),
                d.text.len(),
                d.alpha,
                d.color.is_some(),
                color.as_ptr(),
                d.y,
                d.rows,
            )
        };
    }
}

macro_rules! query_str {
    ($name:ident, $m:ident) => {
        /// # Safety
        /// `v` is live; `s` is readable for `n` bytes.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name(v: *mut Voices, s: *const u8, n: usize) -> bool {
            // SAFETY: caller contract.
            unsafe { voices(v).$m(units(s, n)) }
        }
    };
}

query_str!(ohpl_gs5_voice_subject_is_playing, subject_is_playing);
query_str!(ohpl_gs5_voice_scene_is_active, scene_is_active);
query_str!(
    ohpl_gs5_voice_scene_involving_character_is_active,
    scene_involving_character_is_active
);

/// # Safety
/// `v` is live; pointers are readable for their lengths.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_voice_subject_involves(
    v: *mut Voices,
    s: *const u8,
    n: usize,
    c: *const u8,
    cn: usize,
) -> bool {
    // SAFETY: caller contract.
    unsafe { voices(v).subject_involves_character(units(s, n), units(c, cn)) }
}

/// # Safety
/// `v` is live; `s` is readable for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_voice_subject_line_count(
    v: *mut Voices,
    s: *const u8,
    n: usize,
) -> i32 {
    // SAFETY: caller contract.
    unsafe { voices(v).subject_line_count(units(s, n)) }
}

/// # Safety
/// `v` is live; `s` is readable for `n` bytes; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_voice_subject_scene(
    v: *mut Voices,
    s: *const u8,
    n: usize,
    sink: Sink2,
    ctx: Ctx,
) {
    // SAFETY: caller contract.
    let scene = unsafe { voices(v).subject_scene(units(s, n)) };
    // SAFETY: caller contract.
    unsafe { sink(ctx, scene.as_ptr(), scene.len()) };
}

/// # Safety
/// `v` is live; `host` is a valid table; `s` is readable for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_voice_time_left(
    v: *mut Voices,
    host: *const OhplGs5VoiceHost,
    s: *const u8,
    n: usize,
) -> f32 {
    let mut h = host!(host);
    // SAFETY: caller contract.
    unsafe { voices(v).time_left_on_last_line(&mut h, units(s, n)) }
}

/// The entry and file `GetSpectrum` reads for a character (`by_scene` false) or a scene.
///
/// # Safety
/// `v` is live; `s` is readable for `n` bytes; `entry`/`id` are writable; `file` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_voice_spectrum_target(
    v: *mut Voices,
    by_scene: bool,
    s: *const u8,
    n: usize,
    entry: *mut usize,
    id: *mut i32,
    file: Sink2,
    ctx: Ctx,
) -> bool {
    // SAFETY: caller contract.
    let (vo, key) = unsafe { (voices(v), units(s, n)) };
    let Some((e, i, f)) = (if by_scene {
        vo.spectrum_by_scene(key)
    } else {
        vo.spectrum_by_character(key)
    }) else {
        return false;
    };
    // SAFETY: caller contract.
    unsafe {
        *entry = e;
        *id = i;
        file(ctx, f.as_ptr(), f.len());
    }
    true
}

macro_rules! pause_fn {
    ($name:ident, $m:ident $(, $a:ident: $t:ty)*) => {
        /// # Safety
        /// `v` is live; `host` is a valid table.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name(v: *mut Voices, host: *const OhplGs5VoiceHost $(, $a: $t)*) {
            let mut h = host!(host);
            // SAFETY: caller contract.
            unsafe { voices(v).$m(&mut h $(, $a)*) }
        }
    };
}

pause_fn!(ohpl_gs5_voice_set_paused_all, set_paused_all, on: bool);

/// # Safety
/// `v` is live; `host` is a valid table; `scene` is readable for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_voice_set_paused(
    v: *mut Voices,
    host: *const OhplGs5VoiceHost,
    scene: *const u8,
    n: usize,
    on: bool,
) {
    let mut h = host!(host);
    // SAFETY: caller contract.
    unsafe { voices(v).set_paused(&mut h, units(scene, n), on) }
}

/// # Safety
/// `v` is live; `host` is a valid table.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_voice_reapply_pauses(
    v: *mut Voices,
    host: *const OhplGs5VoiceHost,
) {
    let mut h = host!(host);
    // SAFETY: caller contract.
    unsafe { voices(v).reapply_pauses(&mut h) }
}

/// The viseme weights per speaking entity.
///
/// # Safety
/// `v` is live; `host` is a valid table; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_voice_lip_weights(
    v: *mut Voices,
    host: *const OhplGs5VoiceHost,
    sink: unsafe extern "C" fn(Ctx, *const u8, usize, *const f32),
    ctx: Ctx,
) {
    let mut h = host!(host);
    // SAFETY: caller contract.
    for (name, w) in unsafe { voices(v).lip_weights(&mut h) } {
        let w: [f32; VISEMES] = w;
        // SAFETY: caller contract.
        unsafe { sink(ctx, name.as_ptr(), name.len(), w.as_ptr()) };
    }
}

/// `GetSpectrum`'s transform of decoded interleaved PCM.
///
/// # Safety
/// `pcm` holds `n` floats; `out` holds `num` floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_spectrum(
    pcm: *const f32,
    n: usize,
    channels: i32,
    rate: i32,
    elapsed: f64,
    num: i32,
    out: *mut f32,
) {
    // SAFETY: caller contract.
    let r = spectrum(unsafe { units(pcm, n) }, channels, rate, elapsed, num);
    // SAFETY: caller contract.
    unsafe { out.copy_from_nonoverlapping(r.as_ptr(), r.len()) };
}

/// Discards the immediate subtitles of a frame that draws no subtitles.
///
/// # Safety
/// `v` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs5_voice_drop_immediate(v: *mut Voices) {
    // SAFETY: caller contract.
    unsafe { voices(v).drop_immediate() }
}
