//! The A12a dump driver (`OHPL_A12A_DUMP`, `OHPL_A12A_SCRIPT`): scripted effect commands, a fixed
//! 1/60 step per drawn frame and one text line per host call, written from the live game. The
//! same `run` and `log_state` replay against a fake host in `tests_a12a.rs`.

use super::effects::Flash;
use super::fxffi::{
    fx, ohpl_a12a_fx_update, ohpl_a12a_ins_get, ohpl_a12a_ins_set, ohpl_a12a_ins_update,
    ohpl_a12a_rend_add, ohpl_a12a_rend_clear, ohpl_a12a_rend_update,
};
use super::fxobj::{Ctx, State};
use super::host::host;
use std::cell::{Cell, RefCell};
use std::fs::File;
use std::io::Write;

pub const DT: f32 = 1.0 / 60.0;

pub fn hf(x: f32) -> String {
    format!("{:08x}", x.to_bits())
}

pub fn hd(x: f64) -> String {
    format!("{:016x}", x.to_bits())
}

pub fn fs(v: &[f32]) -> String {
    v.iter().map(|x| hf(*x)).collect::<Vec<_>>().join(" ")
}

pub fn esc(units: &[u32]) -> String {
    let mut o = String::new();
    for &c in units {
        if (33..127).contains(&c) && c != 92 && c != 124 {
            o.push(char::from_u32(c).unwrap_or('?'));
        } else {
            o.push_str(&format!("\\u{c:04x}"));
        }
    }
    o
}

pub fn escb(b: &[u8]) -> String {
    esc(&b.iter().map(|&c| u32::from(c)).collect::<Vec<_>>())
}

struct Cmd {
    from: i32,
    to: i32,
    line: String,
}

#[derive(Default)]
struct Dump {
    file: Option<File>,
    opened: bool,
    frame: i32,
    in_tick: bool,
    cmds: Option<Vec<Cmd>>,
}

thread_local! {
    static DUMP: RefCell<Dump> = RefCell::new(Dump::default());
    static HANDLES: Cell<(u64, u64)> = const { Cell::new((0, 0)) };
}

/// Effect renderer and insanity clock handles of the running game, for the tick.
pub fn set_handles(rend: Option<u64>, clock: Option<u64>) {
    HANDLES.with(|h| {
        let (r, c) = h.get();
        h.set((rend.unwrap_or(r), clock.unwrap_or(c)));
    });
}

fn with<R>(f: impl FnOnce(&mut Dump) -> R) -> R {
    DUMP.with(|d| {
        let mut d = d.borrow_mut();
        if !d.opened {
            d.opened = true;
            d.file = std::env::var_os("OHPL_A12A_DUMP")
                .filter(|p| !p.is_empty())
                .and_then(|p| File::create(p).ok());
        }
        f(&mut d)
    })
}

pub fn enabled() -> bool {
    with(|d| d.file.is_some())
}

/// The handlers skip their own update while the dump drives them.
pub fn gated() -> bool {
    with(|d| d.file.is_some() && !d.in_tick)
}

pub fn log(line: &str) {
    with(|d| {
        if let Some(f) = d.file.as_mut() {
            let _ = writeln!(f, "{line}");
        }
    });
}

/// A render-path line: only from frame 10 on.
pub fn rlog(line: impl FnOnce() -> String) {
    if with(|d| d.file.is_some() && d.frame >= 10) {
        log(&line());
    }
}

pub trait Env {
    fn log(&self, line: String);
    /// Wave alpha, zoom alpha, wave speed, then the post effect on or off.
    fn insanity(&mut self, on: bool, wave: f32, zoom: f32, speed: f32);
    fn first_object(&self) -> usize;
    /// `kind` 0 flash, 1 glow, 2 outline.
    fn rend_add(&mut self, kind: i32, obj: usize, alpha: f32);
    fn rend_clear(&mut self, op: i32);
    fn ins_state(&self) -> (bool, f32, f32, f32);
}

fn tokens(line: &str) -> std::str::SplitWhitespace<'_> {
    line.split_whitespace()
}

fn st(t: Option<&str>) -> Vec<u8> {
    match t {
        Some("-") | None => Vec::new(),
        Some(s) => s.as_bytes().to_vec(),
    }
}

fn num<T: std::str::FromStr + Default>(t: Option<&str>) -> T {
    t.and_then(|s| s.parse().ok()).unwrap_or_default()
}

pub fn run(env: &mut dyn Env, c: &Ctx, line: &str) {
    env.log(format!("cmd{line}"));
    let mut t = tokens(line);
    let cmd = t.next().unwrap_or("");
    match cmd {
        "fade_in" => c.fade_in(num(t.next())),
        "fade_out" => c.fade_out(num(t.next())),
        "fade_direct" => c.fade_direct(num(t.next())),
        "flash" => c.flash_start(num(t.next()), num(t.next()), num(t.next())),
        "sanity" => c.sanity_start(),
        "emotion" => {
            let (cat, entry) = (st(t.next()), st(t.next()));
            c.emotion_start(&cat, &entry, b"");
        }
        "emotion_act" => c.emotion_action(num::<i32>(t.next()) != 0),
        "radial" => c.amount_fade_to(0, num(t.next()), num(t.next())),
        "radial_dist" => c.radial_start_dist(num(t.next())),
        "sepia" => c.amount_fade_to(1, num(t.next()), num(t.next())),
        "trail" => c.amount_fade_to(2, num(t.next()), num(t.next())),
        "shake" => c.shake_start(num(t.next()), num(t.next()), num(t.next()), num(t.next())),
        "voice" | "multivoice" => {
            let (vf, ef, cat) = (st(t.next()), st(t.next()), st(t.next()));
            let n = if cmd == "voice" { 1 } else { 7 };
            let mut entries: [Vec<u8>; 7] = Default::default();
            let mut delays = [0.0f32; 7];
            for (e, d) in entries.iter_mut().zip(delays.iter_mut()).take(n) {
                *e = st(t.next());
                *d = num(t.next());
            }
            let use_pos = num::<i32>(t.next()) != 0;
            let pos = [num(t.next()), num(t.next()), num(t.next())];
            let (min, max): (f32, f32) = (num(t.next()), num(t.next()));
            let (prio, rm): (i32, i32) = (num(t.next()), num(t.next()));
            let e: [&[u8]; 7] = entries.each_ref().map(Vec::as_slice);
            c.voice_add(
                &vf,
                &ef,
                &cat,
                e,
                delays,
                use_pos,
                pos,
                min,
                max,
                prio,
                rm != 0,
            );
        }
        "voice_stop" => c.voice_stop(num(t.next())),
        "voice_pause" => c.voice_pause(),
        "voice_unpause" => c.voice_unpause(),
        "voice_vol" => c.voice_set_volume(num(t.next())),
        "voice_cb" => {
            let cb = st(t.next());
            c.with(|s| s.voice.over_cb = cb);
        }
        "commentary" => {
            let file = st(t.next());
            let icon: i32 = num(t.next());
            c.commentary_start(icon, &file);
        }
        "commentary_stop" => c.commentary_stop(),
        "map_leave" => c.on_map_leave(),
        "reset" => c.reset(),
        "player_paused" => c.set_player_paused(num::<i32>(t.next()) != 0),
        "insanity" => {
            let on: i32 = num(t.next());
            let (a, b, s): (f32, f32, f32) = (num(t.next()), num(t.next()), num(t.next()));
            env.insanity(on != 0, a, b, s);
        }
        "rflash" | "rglow" => {
            let a: f32 = num(t.next());
            let p = env.first_object();
            if p != 0 {
                env.rend_add(i32::from(cmd == "rglow"), p, a);
            }
        }
        "routline" => {
            let p = env.first_object();
            if p != 0 {
                env.rend_add(2, p, 0.0);
            }
        }
        "rclear_outline" => env.rend_clear(2),
        "rreset" => env.rend_clear(0),
        _ => {}
    }
}

fn flash_line(name: &str, f: &Flash) -> String {
    let mut o = format!("{name} a={} act={}", hf(f.alpha), f.active);
    if f.active != 0 {
        let cnt = if f.step >= 1 {
            hf(f.count)
        } else {
            "-".to_string()
        };
        o += &format!(
            " step={} c={cnt} {} {} {}",
            f.step,
            hf(f.in_speed),
            hf(f.white_speed),
            hf(f.out_speed)
        );
    }
    o
}

pub fn log_state(env: &dyn Env, c: &Ctx, frame: i32) {
    let s: State = c.state();
    env.log(format!(
        "fade {} {} {} {}",
        hf(s.fade.goal),
        hf(s.fade.alpha),
        hf(s.fade.speed),
        s.fade.active
    ));
    env.log(flash_line("flash", &s.flash));
    env.log(flash_line("sanity", &s.sanity));
    for (name, a) in [
        ("trail", &s.trail),
        ("sepia", &s.sepia),
        ("radial", &s.radial),
    ] {
        let mut o = format!(
            "{name} {} {} {} {}",
            hf(a.amount),
            hf(a.goal),
            hf(a.speed),
            a.active
        );
        if name == "radial" {
            o += &format!(" {}", hf(s.radial_start));
        }
        env.log(o);
    }
    let e = &s.emotion;
    let mut o = format!("emotion act={}", e.active);
    if e.active != 0 {
        let cnt = if e.step >= 1 {
            hf(e.count)
        } else {
            "-".to_string()
        };
        o += &format!(
            " {} {} {cnt} {} {}",
            hf(e.alpha),
            e.step,
            hf(e.text_alpha),
            hf(e.text_time)
        );
    }
    env.log(o);
    env.log(format!("shake act={} n={}", s.shake_active, s.shake_count));
    env.log(format!(
        "voice act={} pause={} n={} rows={} queue={} entries={} vol={}",
        s.voice_active,
        s.voice_paused,
        s.voice_count,
        s.voice_rows,
        s.voice_queue,
        s.voice_entries,
        hf(s.voice_vol)
    ));
    env.log(format!(
        "commentary act={} icon={}",
        s.commentary_active, s.commentary_icon
    ));
    env.log(format!("paused {}", s.player_paused));
    if frame >= 3 {
        let (on, wave, zoom, speed) = env.ins_state();
        env.log(format!(
            "insanity act={} {} {} {}",
            u8::from(on),
            hf(wave),
            hf(zoom),
            hf(speed)
        ));
    }
}

const OP_MAP_READY: i32 = 0;
const OP_FIRST_OBJECT: i32 = 1;
const OP_INS_ACTIVE: i32 = 2;
const OP_INS_SET_ACTIVE: i32 = 3;

struct Live {
    rend: u64,
    clock: u64,
}

fn query(op: i32, arg: f32) -> usize {
    host().map_or(0, |h| h.dump(op, arg))
}

impl Env for Live {
    fn log(&self, line: String) {
        log(&line);
    }

    fn insanity(&mut self, on: bool, wave: f32, zoom: f32, speed: f32) {
        ohpl_a12a_ins_set(self.clock, 0, wave);
        ohpl_a12a_ins_set(self.clock, 1, zoom);
        ohpl_a12a_ins_set(self.clock, 2, speed);
        query(OP_INS_SET_ACTIVE, f32::from(u8::from(on)));
    }

    fn first_object(&self) -> usize {
        query(OP_FIRST_OBJECT, 0.0)
    }

    fn rend_add(&mut self, kind: i32, obj: usize, alpha: f32) {
        ohpl_a12a_rend_add(self.rend, kind, obj, alpha);
    }

    fn rend_clear(&mut self, op: i32) {
        ohpl_a12a_rend_clear(self.rend, op);
    }

    fn ins_state(&self) -> (bool, f32, f32, f32) {
        let mut v = [0.0f32; 5];
        // SAFETY: `v` has room for five floats.
        unsafe { ohpl_a12a_ins_get(self.clock, v.as_mut_ptr()) };
        (query(OP_INS_ACTIVE, 0.0) != 0, v[2], v[3], v[4])
    }
}

fn atoi(s: &str) -> i32 {
    let (neg, d) = s.strip_prefix('-').map_or((false, s), |r| (true, r));
    let n = d
        .chars()
        .take_while(char::is_ascii_digit)
        .fold(0i32, |a, c| {
            a.wrapping_mul(10)
                .wrapping_add(c.to_digit(10).unwrap_or(0) as i32)
        });
    if neg { -n } else { n }
}

fn load_script() -> Vec<Cmd> {
    let Some(text) =
        std::env::var_os("OHPL_A12A_SCRIPT").and_then(|p| std::fs::read_to_string(p).ok())
    else {
        return Vec::new();
    };
    text.lines()
        .filter(|l| !l.is_empty() && !l.starts_with('#'))
        .map(|l| {
            let t = l.trim_start();
            let (span, rest) = t.split_once(char::is_whitespace).unwrap_or((t, ""));
            let from = atoi(span);
            let to = span.split_once('-').map_or(from, |(_, b)| atoi(b));
            let line = if t.len() > span.len() {
                format!(" {rest}")
            } else {
                String::new()
            };
            Cmd { from, to, line }
        })
        .collect()
}

/// One drawn frame of the dump: scripted commands, a fixed step of the handlers, the state lines.
pub fn tick(fx_h: u64) {
    if !enabled() {
        return;
    }
    let Some(host) = host() else { return };
    if host.dump(OP_MAP_READY, 0.0) == 0 {
        with(|d| d.frame = 0);
        return;
    }
    let (rend, clock) = HANDLES.with(Cell::get);
    let mut live = Live { rend, clock };
    let (frame, lines) = with(|d| {
        d.frame += 1;
        let f = d.frame;
        let cmds = d.cmds.get_or_insert_with(load_script);
        (
            f,
            cmds.iter()
                .filter(|c| f >= c.from && f <= c.to)
                .map(|c| c.line.clone())
                .collect::<Vec<_>>(),
        )
    });
    for line in &lines {
        fx(fx_h, (), |c| run(&mut live, c, line));
    }
    with(|d| d.in_tick = true);
    ohpl_a12a_fx_update(fx_h, DT);
    ohpl_a12a_rend_update(rend, DT);
    if host.dump(OP_INS_ACTIVE, 0.0) != 0 {
        ohpl_a12a_ins_update(clock, DT);
    }
    with(|d| d.in_tick = false);
    log(&format!("tick {frame}"));
    fx(fx_h, (), |c| log_state(&live, c, frame));
}
