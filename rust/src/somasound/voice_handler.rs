//! `cSomaLuxVoiceHandler`: the voice scenes' owner. The engine side (sound entries, GUI set, font,
//! animation states, script arrays) is one table of thunks (`soma/src/game/SomaLuxVoice.cpp`).

use super::dump;
use super::host as s7;
use super::voice::{UpdateEvent, VISEMES, VoiceHost, Voices, decode_text, spectrum};
use crate::backend::media::audio::Vorbis;
use crate::somascript::s2_as::{
    Engine, Frame, Soma, ohpl_as2_wstr_data, ohpl_as2_wstr_len, str_bytes,
};
use crate::string::wide_to_utf8;
use crate::string_ffi::units;
use std::cell::RefCell;
use std::collections::{BTreeMap, BTreeSet};
use std::ffi::{c_long, c_void};
use std::sync::OnceLock;

type Sink = unsafe extern "C" fn(*mut c_void, *const u8, usize);
type RowSink = unsafe extern "C" fn(*mut c_void, *const u32, usize);

#[repr(C)]
pub struct Host {
    pub entry_valid: unsafe extern "C" fn(usize, i32) -> bool,
    pub entry_set_volume_mul: unsafe extern "C" fn(usize, f32),
    pub entry_fade_out: unsafe extern "C" fn(usize, f32),
    pub entry_set_paused: unsafe extern "C" fn(usize, bool),
    pub channel_set_position: unsafe extern "C" fn(usize, *const f32),
    pub channel_filter: unsafe extern "C" fn(usize, bool, f32),
    pub channel_total: unsafe extern "C" fn(usize) -> f64,
    pub channel_elapsed: unsafe extern "C" fn(usize) -> f64,
    pub listener_pos: unsafe extern "C" fn(*mut f32),
    pub map_entity_pos: unsafe extern "C" fn(*const u8, usize, *mut f32) -> bool,
    pub map_current: unsafe extern "C" fn() -> bool,
    pub file_exists: unsafe extern "C" fn(*const u8, usize) -> bool,
    pub read_file: unsafe extern "C" fn(*const u8, usize, Sink, *mut c_void) -> bool,
    pub play_3d: unsafe extern "C" fn(
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
        unsafe extern "C" fn(*const u8, usize, f32, i32, *mut usize, *mut i32) -> bool,
    pub effect_play: unsafe extern "C" fn(*const u8, usize, f32, i32, *mut usize, *mut i32) -> bool,
    pub effect_live: unsafe extern "C" fn(usize, i32) -> bool,
    pub effect_set_volume_mul: unsafe extern "C" fn(usize, f32),
    pub effect_fade_out: unsafe extern "C" fn(usize, f32),
    pub effect_set_paused: unsafe extern "C" fn(usize, bool),
    pub effect_is_playing: unsafe extern "C" fn(usize) -> bool,
    pub effect_total: unsafe extern "C" fn(usize) -> f32,
    pub effect_elapsed: unsafe extern "C" fn(usize) -> f32,
    pub error: unsafe extern "C" fn(*const u8, usize),
    pub wrap_rows:
        unsafe extern "C" fn(*const u8, usize, f32, f32, *const f32, RowSink, *mut c_void),
    pub draw_fade: unsafe extern "C" fn(f32),
    pub subtitles_on: unsafe extern "C" fn() -> bool,
    pub begin_subtitles: unsafe extern "C" fn(*mut f32),
    pub draw_font: unsafe extern "C" fn(*const u32, usize, *const f32, *const f32, *const f32),
    pub game_paused: unsafe extern "C" fn() -> bool,
    pub lip_apply: unsafe extern "C" fn(*const u8, usize, *const f32) -> bool,
    pub map_call: unsafe extern "C" fn(*const u8, usize, *const s7::CallArg, usize, i32) -> i32,
    pub arr_size: unsafe extern "C" fn(*const c_void) -> u32,
    pub arr_at: unsafe extern "C" fn(*const c_void, u32) -> *mut c_void,
    pub arr_resize: unsafe extern "C" fn(*mut c_void, u32),
    pub str_ref: unsafe extern "C" fn(*const u8, usize) -> *const c_void,
}

static HOST: OnceLock<&'static Host> = OnceLock::new();

fn host() -> Option<&'static Host> {
    HOST.get().copied()
}

unsafe extern "C" {
    fn ohpl_vorbis_open_mem(data: *const u8, len: usize) -> *mut c_void;
    fn ohpl_vorbis_info(v: *mut Vorbis, ch: *mut i32, rate: *mut c_long, total: *mut c_long);
    fn ohpl_vorbis_read_float(v: *mut c_void, planes: *mut *mut *mut f32, frames: i32) -> c_long;
    fn ohpl_vorbis_clear_mem(v: *mut c_void);
}

#[derive(Default)]
struct Pcm {
    file: Option<Vec<u8>>,
    data: Vec<f32>,
    channels: i32,
    rate: i32,
}

#[derive(Default)]
struct Handler {
    v: Voices,
    on_done: BTreeMap<u64, Box<dyn FnOnce()>>,
    next_token: u64,
    lip: BTreeSet<Vec<u8>>,
    played: BTreeSet<Vec<u8>>,
    rows: Vec<Vec<Vec<u32>>>,
    pcm: Pcm,
}

thread_local! {
    static H: RefCell<Option<Handler>> = const { RefCell::new(None) };
}

fn with<R>(f: impl FnOnce(&mut Handler) -> R) -> Option<R> {
    H.with(|c| c.try_borrow_mut().ok().and_then(|mut b| b.as_mut().map(f)))
}

pub fn present() -> bool {
    H.with(|c| c.try_borrow().map_or(true, |b| b.is_some()))
}

/// The voice host with the handler's played set and row cache, as `Voices` sees it.
struct Ops<'a> {
    h: &'static Host,
    played: &'a mut BTreeSet<Vec<u8>>,
    rows: &'a mut Vec<Vec<Vec<u32>>>,
}

impl Handler {
    fn split(&mut self) -> Option<(&mut Voices, Ops<'_>)> {
        Some((
            &mut self.v,
            Ops {
                h: host()?,
                played: &mut self.played,
                rows: &mut self.rows,
            },
        ))
    }
}

fn collect(f: impl FnOnce(Sink, *mut c_void) -> bool) -> Option<Vec<u8>> {
    unsafe extern "C" fn sink(c: *mut c_void, p: *const u8, n: usize) {
        // SAFETY: `c` is the `Vec` below; `p` holds `n` bytes.
        unsafe { (*c.cast::<Vec<u8>>()).extend_from_slice(units(p, n)) };
    }
    let mut v: Vec<u8> = Vec::new();
    f(sink, (&raw mut v).cast()).then_some(v)
}

// SAFETY (all calls below): the table's thunks accept the entries the sound layer handed out.
impl VoiceHost for Ops<'_> {
    fn entry_valid(&mut self, entry: usize, id: i32) -> bool {
        unsafe { (self.h.entry_valid)(entry, id) }
    }
    fn entry_set_volume_mul(&mut self, entry: usize, vol: f32) {
        unsafe { (self.h.entry_set_volume_mul)(entry, vol) }
    }
    fn entry_fade_out(&mut self, entry: usize, speed: f32) {
        unsafe { (self.h.entry_fade_out)(entry, speed) }
    }
    fn entry_set_paused(&mut self, entry: usize, on: bool) {
        unsafe { (self.h.entry_set_paused)(entry, on) }
    }
    fn channel_set_position(&mut self, entry: usize, pos: [f32; 3]) {
        unsafe { (self.h.channel_set_position)(entry, pos.as_ptr()) }
    }
    fn channel_filter(&mut self, entry: usize, high: bool, gain: f32) {
        unsafe { (self.h.channel_filter)(entry, high, gain) }
    }
    fn channel_total(&mut self, entry: usize) -> f64 {
        unsafe { (self.h.channel_total)(entry) }
    }
    fn channel_elapsed(&mut self, entry: usize) -> f64 {
        unsafe { (self.h.channel_elapsed)(entry) }
    }
    fn listener_pos(&mut self) -> [f32; 3] {
        let mut b = [0.0f32; 3];
        unsafe { (self.h.listener_pos)(b.as_mut_ptr()) };
        b
    }
    fn map_entity_pos(&mut self, name: &[u8]) -> Option<[f32; 3]> {
        let mut b = [0.0f32; 3];
        unsafe { (self.h.map_entity_pos)(name.as_ptr(), name.len(), b.as_mut_ptr()) }.then_some(b)
    }
    fn map_current(&mut self) -> bool {
        unsafe { (self.h.map_current)() }
    }
    fn file_exists(&mut self, file: &[u8]) -> bool {
        unsafe { (self.h.file_exists)(file.as_ptr(), file.len()) }
    }
    fn read_file(&mut self, file: &[u8]) -> Option<Vec<u8>> {
        read_file(self.h, file)
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
            (self.h.play_3d)(
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
        unsafe { (self.h.play_gui_stream)(file.as_ptr(), file.len(), vol, ty, &mut e, &mut id) }
            .then_some((e, id))
    }
    fn effect_play(&mut self, file: &[u8], vol: f32, ty: i32) -> Option<(usize, i32)> {
        let (mut e, mut id) = (0usize, 0i32);
        unsafe { (self.h.effect_play)(file.as_ptr(), file.len(), vol, ty, &mut e, &mut id) }
            .then_some((e, id))
    }
    fn effect_live(&mut self, effect: usize, id: i32) -> bool {
        unsafe { (self.h.effect_live)(effect, id) }
    }
    fn effect_set_volume_mul(&mut self, effect: usize, vol: f32) {
        unsafe { (self.h.effect_set_volume_mul)(effect, vol) }
    }
    fn effect_fade_out(&mut self, effect: usize, speed: f32) {
        unsafe { (self.h.effect_fade_out)(effect, speed) }
    }
    fn effect_set_paused(&mut self, effect: usize, on: bool) {
        unsafe { (self.h.effect_set_paused)(effect, on) }
    }
    fn effect_is_playing(&mut self, effect: usize) -> bool {
        unsafe { (self.h.effect_is_playing)(effect) }
    }
    fn effect_total(&mut self, effect: usize) -> f32 {
        unsafe { (self.h.effect_total)(effect) }
    }
    fn effect_elapsed(&mut self, effect: usize) -> f32 {
        unsafe { (self.h.effect_elapsed)(effect) }
    }
    fn played_has(&mut self, key: &[u8]) -> bool {
        self.played.contains(key)
    }
    fn played_insert(&mut self, key: &[u8]) -> bool {
        self.played.insert(key.to_vec())
    }
    fn log(&mut self, text: &[u8]) {
        s7::log(text);
    }
    fn error(&mut self, text: &[u8]) {
        unsafe { (self.h.error)(text.as_ptr(), text.len()) }
    }
    fn wrap_rows(&mut self, text: &[u8], width: f32, line_height: f32, size: [f32; 2]) -> usize {
        unsafe extern "C" fn sink(c: *mut c_void, p: *const u32, n: usize) {
            // SAFETY: `c` is the row list below; `p` holds `n` wide characters.
            unsafe {
                (*c.cast::<Vec<Vec<u32>>>()).push(std::slice::from_raw_parts(p, n).to_vec());
            }
        }
        let mut rows: Vec<Vec<u32>> = Vec::new();
        unsafe {
            (self.h.wrap_rows)(
                text.as_ptr(),
                text.len(),
                width,
                line_height,
                size.as_ptr(),
                sink,
                (&raw mut rows).cast(),
            );
        }
        let n = rows.len();
        self.rows.push(rows);
        n
    }
}

fn read_file(h: &Host, file: &[u8]) -> Option<Vec<u8>> {
    // SAFETY: `file` is valid for the call; the sink appends to the `Vec`.
    collect(|sink, ctx| unsafe { (h.read_file)(file.as_ptr(), file.len(), sink, ctx) })
}

fn read_text(file: &[u8]) -> Option<Vec<u8>> {
    read_file(host()?, file).map(|d| decode_text(&d))
}

fn load_lang_file(file: &[u8]) {
    if let Some(t) = read_text(file) {
        with(|h| h.v.load_lang(&t));
    }
}

fn load_voice_file(file: &[u8], set: &[u8]) -> bool {
    let count = read_text(file).and_then(|t| with(|h| h.v.load_voice(&t, set)).flatten());
    match count {
        Some(n) => {
            s7::log(format!("SOMA voice: {n} subjects from '{}'\n", lossy(file)).as_bytes());
            true
        }
        None => {
            s7::warning(format!("SOMA voice: could not load '{}'\n", lossy(file)).as_bytes());
            false
        }
    }
}

fn lossy(b: &[u8]) -> String {
    String::from_utf8_lossy(b).into_owned()
}

fn dump_text(tag: &str, text: &str) {
    dump::text(tag.as_bytes(), text.as_bytes());
}

fn take_done(token: u64) -> Option<Box<dyn FnOnce()>> {
    with(|h| h.on_done.remove(&token)).flatten()
}

/// The handler's creation: the global subjects and texts.
pub fn create() {
    H.with(|c| {
        if let Ok(mut b) = c.try_borrow_mut() {
            *b = Some(Handler {
                v: Voices::new(),
                next_token: 1,
                ..Handler::default()
            });
        }
    });
    load_voice_file(b"global_voice.voice", b"global");
    load_lang_file(b"global_voice.lang");
}

pub fn destroy() {
    H.with(|c| {
        if let Ok(mut b) = c.try_borrow_mut() {
            *b = None;
        }
    });
}

pub fn load_map_file(map: &[u8]) {
    if with(|h| h.v.mark_set(map)) != Some(true) {
        return;
    }
    load_voice_file(&[map, b".voice"].concat(), map);
    load_lang_file(&[map, b".lang"].concat());
}

pub fn play(
    subject: &[u8],
    line: i32,
    callback: &[u8],
    prio: i32,
    on_done: Option<Box<dyn FnOnce()>>,
    while_paused: bool,
) -> bool {
    let tag = |r: &str, scene: &str| {
        dump_text(
            "VP",
            &format!(
                "{} {line} {} {prio} {r}{scene}",
                lossy(subject),
                lossy(callback)
            ),
        );
    };
    let checked = with(|h| {
        let (v, mut ops) = h.split()?;
        Some(v.play_check(&mut ops, subject, prio))
    })
    .flatten();
    if checked.is_none_or(|r| r <= 0) {
        tag("check-fail", "");
        return false;
    }
    let scene = with(|h| h.v.scene_of_subject(subject)).unwrap_or_default();
    stop_scene(&scene);
    let token = on_done.map_or(0, |f| {
        with(|h| {
            let t = h.next_token;
            h.next_token += 1;
            h.on_done.insert(t, f);
            t
        })
        .unwrap_or(0)
    });
    let ok = with(|h| {
        let (v, mut ops) = h.split()?;
        Some(v.play_start(&mut ops, subject, line, callback, prio, token, while_paused))
    })
    .flatten()
    .unwrap_or(false);
    if !ok && token != 0 {
        take_done(token);
    }
    tag(
        if ok { "ok" } else { "fail" },
        &format!(" scene={}", lossy(&scene)),
    );
    ok
}

fn stop_scene(scene: &[u8]) {
    loop {
        let ev = with(|h| {
            let (v, mut ops) = h.split()?;
            v.stop_next(&mut ops, scene)
        })
        .flatten();
        let Some(ev) = ev else { break };
        if ev.on_done != 0
            && let Some(f) = take_done(ev.on_done)
        {
            f();
        }
    }
}

pub fn stop(scene: &[u8]) {
    stop_scene(scene);
}

pub fn stop_all() {
    with(|h| {
        if let Some((v, mut ops)) = h.split() {
            v.stop_all(&mut ops);
        }
        h.on_done.clear();
    });
}

fn q<R>(default: R, f: impl FnOnce(&Voices) -> R) -> R {
    with(|h| f(&h.v)).unwrap_or(default)
}

fn qh<R>(default: R, f: impl FnOnce(&mut Voices, &mut Ops<'_>) -> R) -> R {
    with(|h| {
        let (v, mut ops) = h.split()?;
        Some(f(v, &mut ops))
    })
    .flatten()
    .unwrap_or(default)
}

pub fn subject_line_count(subject: &[u8]) -> i32 {
    q(0, |v| v.subject_line_count(subject))
}

pub fn scene_involving_character(name: &[u8]) -> bool {
    q(false, |v| v.scene_involving_character_is_active(name))
}

pub fn subject_involves(subject: &[u8], name: &[u8]) -> bool {
    q(false, |v| v.subject_involves_character(subject, name))
}

pub fn fade_to(goal: f32, time: f32) {
    with(|h| h.v.fade_to(goal, time));
}

pub fn reapply_pauses() {
    qh((), |v, o| v.reapply_pauses(o));
}

pub fn clear_played_lines() {
    with(|h| h.played.clear());
}

fn script_call(decl: &str, args: &[s7::CallArg], default: i32) -> i32 {
    host().map_or(default, |h| {
        // SAFETY: `decl` and the argument strings are valid for the call.
        unsafe {
            (h.map_call)(
                decl.as_ptr(),
                decl.len(),
                args.as_ptr(),
                args.len(),
                default,
            )
        }
    })
}

fn arg_str(b: &[u8]) -> s7::CallArg {
    s7::CallArg {
        kind: 0,
        i: 0,
        s: s7::Str {
            p: b.as_ptr(),
            n: b.len(),
        },
    }
}

fn arg_num(kind: u8, i: i32) -> s7::CallArg {
    s7::CallArg {
        kind,
        i,
        s: s7::Str {
            p: std::ptr::null(),
            n: 0,
        },
    }
}

fn dispatch(ev: &UpdateEvent) {
    match ev {
        UpdateEvent::Finish {
            callback,
            scene,
            name,
            on_done,
        } => {
            dump_text(
                "VE",
                &format!("0 {} {} {} 0 0", lossy(callback), lossy(scene), lossy(name)),
            );
            let done = if *on_done != 0 {
                take_done(*on_done)
            } else {
                None
            };
            if !callback.is_empty() {
                script_call(
                    &format!(
                        "void {}(const tString&in, const tString&in)",
                        lossy(callback)
                    ),
                    &[arg_str(scene), arg_str(name)],
                    -1,
                );
            }
            if let Some(f) = done {
                f();
            }
        }
        UpdateEvent::Line {
            func,
            scene,
            name,
            line,
            start,
        } => {
            dump_text(
                "VE",
                &format!(
                    "1 {} {} {} {line} {}",
                    lossy(func),
                    lossy(scene),
                    lossy(name),
                    i32::from(*start)
                ),
            );
            script_call(
                &format!(
                    "void {}(const tString&in, const tString&in, int, bool)",
                    lossy(func)
                ),
                &[
                    arg_str(scene),
                    arg_str(name),
                    arg_num(2, *line),
                    arg_num(1, i32::from(*start)),
                ],
                -1,
            );
        }
    }
}

pub fn update(dt: f32, game_paused: bool) {
    qh((), |v, o| v.update_begin(o, dt, game_paused));
    while let Some(ev) = qh(None, |v, o| v.update_step(o)) {
        dispatch(&ev);
    }
    if game_paused {
        return;
    }
    update_lipsync();
    update_speaking_callbacks();
}

fn update_speaking_callbacks() {
    let changed = with(|h| h.v.speaking_changes()).unwrap_or_default();
    for (name, started) in changed {
        let Some(func) = with(|h| h.v.speaking_func(&name)).flatten() else {
            continue;
        };
        let keep = script_call(
            &format!("bool {}(const tString&in, bool)", lossy(&func)),
            &[arg_str(&name), arg_num(1, i32::from(started))],
            1,
        ) != 0;
        with(|h| h.v.speaking_done(&name, &func, keep));
    }
}

fn update_lipsync() {
    let Some(host) = host() else { return };
    let weights = qh(BTreeMap::new(), |v, o| v.lip_weights(o));
    let all: BTreeSet<Vec<u8>> = with(|h| h.lip.clone())
        .unwrap_or_default()
        .into_iter()
        .chain(weights.keys().cloned())
        .collect();
    let mut keep = BTreeSet::new();
    for name in all {
        let w = weights.get(&name);
        // SAFETY: `name` and the weights are valid for the call.
        let ok = unsafe {
            (host.lip_apply)(
                name.as_ptr(),
                name.len(),
                w.map_or(std::ptr::null(), |w| w.as_ptr()),
            )
        };
        if !ok {
            continue;
        }
        if let Some(w) = w {
            let mut s = lossy(&name);
            s.push(' ');
            for x in w.iter().take(VISEMES) {
                s.push_str(&format!("{:08x} ", x.to_bits()));
            }
            dump_text("VL", &s);
            keep.insert(name);
        }
    }
    with(|h| h.lip = keep);
}

/// `OnDraw`: the fade overlay, then the frame's subtitles.
pub fn draw() {
    let Some(host) = host() else { return };
    let alpha = q(0.0, |v| v.fade_alpha);
    // SAFETY: plain calls.
    if alpha > 0.0 && unsafe { (host.map_current)() } {
        unsafe { (host.draw_fade)(alpha) };
    }
    if !unsafe { (host.subtitles_on)() } {
        with(|h| h.v.drop_immediate());
        return;
    }
    let mut screen = [0.0f32; 2];
    // SAFETY: two floats are writable.
    unsafe { (host.begin_subtitles)(screen.as_mut_ptr()) };
    let scale = screen[1] / 768.0;
    let size = [26.0 * scale, 26.0 * scale];
    let paused = unsafe { (host.game_paused)() };
    let Some(draws) = with(|h| {
        h.rows.clear();
        let (v, mut ops) = h.split()?;
        Some(v.subtitles(&mut ops, screen[1], paused))
    })
    .flatten() else {
        return;
    };
    let rows = with(|h| std::mem::take(&mut h.rows)).unwrap_or_default();
    for (d, rows) in draws.iter().zip(&rows) {
        let color_on = d.color.is_some();
        let text = rows
            .iter()
            .map(|r| String::from_utf8_lossy(&wide_to_utf8(r)).into_owned() + "|")
            .collect::<String>();
        dump_text(
            "VS",
            &format!(
                "{:08x} {:08x} {} {} => {text}",
                d.alpha.to_bits(),
                d.y.to_bits(),
                i32::from(color_on),
                lossy(&d.text)
            ),
        );
        let col = d.color.unwrap_or([1.0, 1.0, 1.0, d.alpha]);
        for (r, row) in rows.iter().enumerate() {
            let pos = [screen[0] * 0.5, size[1].mul_add(r as f32, d.y), 5.0];
            let shadow = [pos[0] + scale, pos[1] + scale, pos[2] - 0.1];
            // SAFETY: the row, vectors and colours are valid for the calls.
            unsafe {
                (host.draw_font)(
                    row.as_ptr(),
                    row.len(),
                    shadow.as_ptr(),
                    size.as_ptr(),
                    [0.0, 0.0, 0.0, d.alpha].as_ptr(),
                );
                (host.draw_font)(
                    row.as_ptr(),
                    row.len(),
                    pos.as_ptr(),
                    size.as_ptr(),
                    col.as_ptr(),
                );
            }
        }
    }
}

fn load_pcm(h: &mut Handler, file: &[u8]) -> bool {
    if h.pcm.file.as_deref() == Some(file) {
        return h.pcm.channels > 0;
    }
    h.pcm = Pcm {
        file: Some(file.to_vec()),
        ..Pcm::default()
    };
    let Some(data) = host().and_then(|ho| read_file(ho, file)) else {
        return false;
    };
    // SAFETY: `data` outlives the handle, which is cleared below.
    let v = unsafe { ohpl_vorbis_open_mem(data.as_ptr(), data.len()) };
    if v.is_null() {
        return false;
    }
    let (mut ch, mut rate, mut total) = (0i32, 0 as c_long, 0 as c_long);
    // SAFETY: `v` is live.
    unsafe { ohpl_vorbis_info(v.cast(), &mut ch, &mut rate, &mut total) };
    h.pcm.channels = ch;
    h.pcm.rate = rate as i32;
    let chans = usize::try_from(ch).unwrap_or(0);
    loop {
        let mut planes: *mut *mut f32 = std::ptr::null_mut();
        // SAFETY: `v` is live; `planes` receives the decoder's channel table for this read.
        let n = unsafe { ohpl_vorbis_read_float(v, &mut planes, 4096) };
        if n <= 0 || planes.is_null() {
            break;
        }
        for i in 0..n as usize {
            for c in 0..chans {
                // SAFETY: the decoder returned `ch` planes of `n` floats.
                h.pcm.data.push(unsafe { *(*planes.add(c)).add(i) });
            }
        }
    }
    // SAFETY: `v` came from `ohpl_vorbis_open_mem`.
    unsafe { ohpl_vorbis_clear_mem(v) };
    true
}

/// `GetSpectrum`: empty without a speaking target, zeros without a decodable voice.
fn spectrum_of(by_scene: bool, key: &[u8], num: i32) -> Vec<f32> {
    with(|h| {
        let t = if by_scene {
            h.v.spectrum_by_scene(key)
        } else {
            h.v.spectrum_by_character(key)
        };
        let Some((entry, id, file)) = t else {
            return Vec::new();
        };
        let zeros = vec![0.0; usize::try_from(num).unwrap_or(0)];
        let Some(host) = host() else { return zeros };
        // SAFETY: plain calls on an entry the sound layer gave out.
        if entry == 0 || !unsafe { (host.entry_valid)(entry, id) } || !load_pcm(h, &file) {
            return zeros;
        }
        let elapsed = unsafe { (host.channel_elapsed)(entry) };
        spectrum(&h.pcm.data, h.pcm.channels, h.pcm.rate, elapsed, num)
    })
    .unwrap_or_default()
}

/// # Safety
/// `ops` points at a table that lives for the whole process.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s5b_voice_install_host(ops: *const Host) {
    // SAFETY: caller contract.
    if let Some(o) = unsafe { ops.as_ref() } {
        let _ = HOST.set(o);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5b_voice_create() {
    create();
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5b_voice_destroy() {
    destroy();
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5b_voice_present() -> bool {
    present()
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5b_voice_update(dt: f32, game_paused: bool) {
    update(dt, game_paused);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5b_voice_draw() {
    draw();
}

/// # Safety
/// `map` is readable for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s5b_voice_load_map_file(map: *const u8, n: usize) {
    // SAFETY: caller contract.
    load_map_file(unsafe { units(map, n) });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5b_voice_reapply_pauses() {
    reapply_pauses();
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5b_voice_fade_to(goal: f32, time: f32) {
    fade_to(goal, time);
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5b_voice_clear_played() {
    clear_played_lines();
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_s5b_voice_played_count() -> usize {
    with(|h| h.played.len()).unwrap_or(0)
}

/// # Safety
/// `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s5b_voice_played_at(i: usize, sink: Sink, ctx: *mut c_void) {
    if let Some(l) = with(|h| h.played.iter().nth(i).cloned()).flatten() {
        // SAFETY: caller contract.
        unsafe { sink(ctx, l.as_ptr(), l.len()) };
    }
}

/// # Safety
/// `key` is readable for `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s5b_voice_played_insert(key: *const u8, n: usize) {
    // SAFETY: caller contract.
    let k = unsafe { units(key, n) }.to_vec();
    with(|h| h.played.insert(k));
}

fn text<'a>(f: &Frame, i: usize) -> &'a [u8] {
    // SAFETY: the declaration passes a `const tString &in` in slot `i`.
    unsafe { str_bytes(f.ptr_arg(i)) }
}

fn str_ref(b: &[u8]) -> *const c_void {
    host().map_or(std::ptr::null(), |h| {
        // SAFETY: plain call; the host keeps the string until the next call.
        unsafe { (h.str_ref)(b.as_ptr(), b.len()) }
    })
}

fn wide(p: *mut c_void) -> Vec<u32> {
    // SAFETY: `p` is a live `tWString`.
    unsafe { units_u32(ohpl_as2_wstr_data(p), ohpl_as2_wstr_len(p)) }.to_vec()
}

unsafe fn units_u32<'a>(p: *const u32, n: usize) -> &'a [u32] {
    if p.is_null() || n == 0 {
        &[]
    } else {
        // SAFETY: caller contract.
        unsafe { std::slice::from_raw_parts(p, n) }
    }
}

static VOICE_TAG: u8 = 0;
static DIALOG_TAG: u8 = 0;

fn register_fades(s: &Soma) {
    let t = "cLuxEffectHandler";
    s.method(t, "void FadeIn(float afTime)", |f| {
        if present() {
            fade_to(0.0, f.arg(0));
        }
    });
    s.method(t, "void FadeOut(float afTime)", |f| {
        if present() {
            fade_to(1.0, f.arg(0));
        }
    });
    s.method(t, "bool IsFading()", |f| {
        f.ret(q(false, |v| v.is_fading()));
    });
    s.method(t, "float GetFadeAlpha()", |f| {
        f.ret(q(0.0f32, |v| v.fade_alpha))
    });
}

fn register_spectrum(s: &Soma, t: &str, decl: &str, by_scene: bool) {
    s.method(t, decl, move |f| {
        let (key, arr, num) = (text(f, 0), f.ptr_arg(1), f.arg::<i32>(2));
        if !present() {
            return;
        }
        let v = spectrum_of(by_scene, key, num);
        let Some(h) = host() else { return };
        // SAFETY: `arr` is the script array of the `array<float>&out` argument.
        unsafe {
            (h.arr_resize)(arr, v.len() as u32);
            for (i, x) in v.iter().enumerate() {
                (h.arr_at)(arr, i as u32).cast::<f32>().write(*x);
            }
        }
    });
}

pub fn register(e: Engine) {
    let s = Soma(e);
    s.func("cLuxVoiceHandler@ cLux_GetVoiceHandler()", |f| {
        f.ret(&raw const VOICE_TAG);
    });
    s.func("cLuxDialogHandler@ cLux_GetDialogHandler()", |f| {
        f.ret(&raw const DIALOG_TAG);
    });
    register_fades(&s);
    let t = "cLuxVoiceHandler";
    s.method(
        t,
        "bool Play(const tString &in asSubject, int alSpecificLine, const tString &in asCallback, int alPrio)",
        |f| f.ret(present() && play(text(f, 0), f.arg(1), text(f, 2), f.arg(3), None, false)),
    );
    s.method(
        t,
        "bool Play(const tString &in asSubject, int alSpecificLine, const tString &in asCallback, int alPrio, bool abPlayWhilePaused)",
        |f| {
            f.ret(
                present()
                    && play(text(f, 0), f.arg(1), text(f, 2), f.arg(3), None, f.arg(4)),
            );
        },
    );
    s.method(
        t,
        "void AddSubtitle(const array<tWString> &in avText, const tWString &in asCharacterName, const cColor &in aColor, int alDrawnChars, bool abVoiceSoundMissing, bool abIsPlayer = false, bool abPlayWhilePaused = false, bool abPositional = false, const cVector3f &in avPosition = 0)",
        |f| {
            let Some(h) = host() else { return };
            let arr = f.ptr_arg(0).cast_const();
            // SAFETY: `arr` is the `array<tWString>` argument; its elements are wide strings.
            let rows: Vec<Vec<u8>> = (0..unsafe { (h.arr_size)(arr) })
                .map(|i| wide_to_utf8(&wide(unsafe { (h.arr_at)(arr, i) })))
                .collect();
            let name = wide_to_utf8(&wide(f.ptr_arg(1)));
            let color = *f.rf::<[f32; 4]>(2);
            let drawn = f.arg::<i32>(3);
            with(|h| h.v.add_subtitle(rows, &name, color, drawn));
        },
    );
    s.method(
        t,
        "void AdvanceFromCurrentSound(const tString&in asScene)",
        |f| {
            let sc = text(f, 0);
            qh((), |v, o| v.skip_current_line(o, sc));
        },
    );
    s.method(
        t,
        "float GetTimeLeftOnLastLineOfPlayingSubject(const tString&in asName)",
        |f| {
            let n = text(f, 0);
            f.ret(if present() {
                qh(0.0, |v, o| v.time_left_on_last_line(o, n))
            } else {
                -1.0f32
            });
        },
    );
    s.method(
        t,
        "int GetSubjectLineNumber(const tString &in asSubject)",
        |f| f.ret(subject_line_count(text(f, 0))),
    );
    s.method(
        t,
        "const tString& GetSubjectSceneName(const tString &in asSubject)",
        |f| {
            let sc = q(Vec::new(), |v| v.subject_scene(text(f, 0)).to_vec());
            f.ret_ref(str_ref(&sc).cast_mut());
        },
    );
    s.method(t, "bool SubjectExists(const tString &in asSubject)", |f| {
        f.ret(q(false, |v| v.subject_exists(text(f, 0))))
    });
    s.method(t, "void StopAll()", |_| {
        if present() {
            stop_all();
        }
    });
    s.method(t, "void Stop(const tString&in asScene)", |f| {
        if present() {
            stop(text(f, 0));
        }
    });
    for decl in [
        "void SkipCurrentLine(const tString&in asScene)",
        "void SkipCurrentSound(const tString&in asScene)",
    ] {
        s.method(t, decl, |f| {
            let sc = text(f, 0);
            qh((), |v, o| v.skip_current_line(o, sc));
        });
    }
    s.method(
        t,
        "void SetPaused(const tString&in asScene, bool abX)",
        |f| {
            let (sc, on) = (text(f, 0), f.arg::<bool>(1));
            qh((), |v, o| v.set_paused(o, sc, on));
        },
    );
    s.method(t, "void SetPausedAll(bool abX)", |f| {
        let on = f.arg::<bool>(0);
        qh((), |v, o| v.set_paused_all(o, on));
    });
    s.method(
        t,
        "bool CharacterIsSpeaking(const tString&in asName)",
        |f| {
            f.ret(q(false, |v| v.character_is_speaking(text(f, 0))));
        },
    );
    s.method(
        t,
        "void AddCharacterSpeakingCallback(const tString&in asCharacter, const tString&in asCallback)",
        |f| {
            with(|h| h.v.set_speaking_callback(text(f, 0), text(f, 1)));
        },
    );
    s.method(
        t,
        "void RemoveCharacterSpeakingCallback(const tString&in asCharacter)",
        |f| {
            with(|h| h.v.set_speaking_callback(text(f, 0), b""));
        },
    );
    register_spectrum(
        &s,
        t,
        "void GetSpectrumFromSpeakingCharacter(const tString&in asCharacter, array<float>&out aDestArray, int alNumSamples=64)",
        false,
    );
    register_spectrum(
        &s,
        t,
        "void GetSpectrumFromScene(const tString&in asScene, array<float>&out aDestArray, int alNumSamples=64)",
        true,
    );
    s.method(t, "bool SubjectIsPlaying(const tString&in asName)", |f| {
        f.ret(q(false, |v| v.subject_is_playing(text(f, 0))));
    });
    s.method(t, "bool SceneIsActive(const tString&in asScene)", |f| {
        f.ret(q(false, |v| v.scene_is_active(text(f, 0))));
    });
    s.method(
        t,
        "bool SceneInvolvingCharacterIsActive(const tString&in asCharacter)",
        |f| f.ret(scene_involving_character(text(f, 0))),
    );
    s.method(
        t,
        "void FadeSceneVolumeTo(const tString&in asScene, float afVolume, float afTime)",
        |f| {
            with(|h| h.v.fade_scene_volume_to(text(f, 0), f.arg(1), f.arg(2)));
        },
    );
    s.method(t, "void SetFocusScene(const tString&in asScene)", |f| {
        let sc = text(f, 0);
        qh((), |v, o| v.set_focus_scene(o, sc));
    });
    s.method(
        "cLuxMap",
        "void SetVoiceSource(const tString &in asCharacter, const tString &in asEntityName, float afMinDistance, float afMaxDistance, bool abUse3D, float afMaxPlayerListeningRange, float afMinFreq = 22000, float afMaxFreq = 22000, uint aFrequencyFlags = 0)",
        |f| {
            with(|h| {
                h.v.set_source(
                    text(f, 0),
                    text(f, 1),
                    f.arg(2),
                    f.arg(3),
                    f.arg(4),
                    f.arg(5),
                    f.arg(6),
                    f.arg(7),
                    f.arg::<u32>(8) as i32,
                );
            });
        },
    );
    s.method(t, "bool AnySceneIsActive()", |f| {
        f.ret(q(false, |v| v.any_active()));
    });
}

/// # Safety
/// `engine` is the live `asIScriptEngine`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_s5b_register_voice_natives(engine: *mut c_void) {
    register(Engine(engine));
}
