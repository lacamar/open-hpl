//! Replays the A12a driver script (`rust/tests/golden/A12a/script.txt`) against the Rust effect
//! handler, effect renderer and insanity clock. The expected text is the dump the original C++
//! wrote on Amnesia 02_library (`OHPL_A12A_DUMP`, fixed 1/60 step, frames 1..299). The fake host
//! logs every call in the dump's format and answers from the results recorded there.

use super::fxobj::{Ctx, Handler, State};
use super::host::{Fx, Hud};
use super::postfx::{self, Clock};
use super::rend::{self, Rend};
use super::renderer::{self, Op as ROp, Outline};
use crate::obj::Arena;
use crate::types::Vector3f;
use std::cell::{Cell, RefCell};
use std::collections::{HashMap, VecDeque};

const SCRIPT: &str = include_str!("../../tests/golden/A12a/script.txt");
const EXPECTED: &str = include_str!("../../tests/golden/A12a/expected.txt");
const DT: f32 = 1.0 / 60.0;
const LAST_FRAME: i32 = 299;

fn hf(x: f32) -> String {
    format!("{:08x}", x.to_bits())
}

fn hd(x: f64) -> String {
    format!("{:016x}", x.to_bits())
}

fn unhf(s: &str) -> f32 {
    f32::from_bits(u32::from_str_radix(s, 16).unwrap_or(0))
}

fn esc(units: &[u32]) -> String {
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

fn escb(b: &[u8]) -> String {
    esc(&b.iter().map(|&c| u32::from(c)).collect::<Vec<_>>())
}

fn unesc(s: &str) -> Vec<u32> {
    let c: Vec<char> = s.chars().collect();
    let mut o = Vec::new();
    let mut i = 0;
    while i < c.len() {
        if c[i] == '\\' {
            let h: String = c[i + 2..i + 6].iter().collect();
            o.push(u32::from_str_radix(&h, 16).unwrap());
            i += 6;
        } else {
            o.push(u32::from(c[i]));
            i += 1;
        }
    }
    o
}

fn fs(v: &[f32]) -> String {
    v.iter().map(|x| hf(*x)).collect::<Vec<_>>().join(" ")
}

struct Rec {
    lines: RefCell<Vec<String>>,
    ans: RefCell<HashMap<String, VecDeque<String>>>,
    frame: Cell<i32>,
    time: Cell<f64>,
}

impl Rec {
    fn new() -> Self {
        Self::from_text(EXPECTED)
    }

    fn from_text(text: &str) -> Self {
        let mut ans: HashMap<String, VecDeque<String>> = HashMap::new();
        for l in text.lines() {
            if let Some((head, rhs)) = l.split_once(" => ").or_else(|| l.split_once(" =>")) {
                let name = head.split(' ').next().unwrap_or("").to_string();
                ans.entry(name).or_default().push_back(rhs.to_string());
            }
        }
        Self {
            lines: RefCell::new(Vec::new()),
            ans: RefCell::new(ans),
            frame: Cell::new(0),
            time: Cell::new(0.0),
        }
    }

    fn log(&self, s: String) {
        self.lines.borrow_mut().push(s);
    }

    fn rlog(&self, s: String) {
        if self.frame.get() >= 10 {
            self.log(s);
        }
    }

    fn pop(&self, name: &str) -> String {
        self.ans
            .borrow_mut()
            .get_mut(name)
            .and_then(VecDeque::pop_front)
            .unwrap_or_else(|| panic!("no recorded answer left for {name}"))
    }

    fn pop_flag(&self, name: &str) -> bool {
        self.pop(name) == "1"
    }
}

fn rows_of(a: &str) -> Vec<Vec<u32>> {
    if a.is_empty() {
        return vec![Vec::new()];
    }
    a.split('|').map(unesc).collect()
}

impl Fx for Rec {
    fn snd_valid(&self, e: usize, _id: i32) -> bool {
        let r = self.pop_flag("snd_valid");
        self.log(format!("snd_valid {} => {}", u8::from(e != 0), u8::from(r)));
        r
    }

    fn snd_play_gui(&self, file: &[u8]) -> (usize, i32) {
        let r = self.pop_flag("snd_play_gui");
        self.log(format!("snd_play_gui {} => {}", escb(file), u8::from(r)));
        (usize::from(r), 1)
    }

    fn snd_play_3d(&self, file: &[u8], p: [f32; 3], min: f32, max: f32) -> (usize, i32) {
        let r = self.pop_flag("snd_play_3d");
        self.log(format!(
            "snd_play_3d {} {} {} {} => {}",
            escb(file),
            fs(&p),
            hf(min),
            hf(max),
            u8::from(r)
        ));
        (usize::from(r), 1)
    }

    fn snd_fade_out(&self, _e: usize, speed: f32) {
        self.log(format!("snd_fade_out 1 {}", hf(speed)));
    }

    fn snd_stop(&self, _e: usize) {
        self.log("snd_stop 1".into());
    }

    fn snd_pause(&self, _e: usize, on: bool) {
        self.log(format!("snd_pause 1 {}", u8::from(on)));
    }

    fn snd_volmul(&self, _e: usize, v: f32) {
        self.log(format!("snd_volmul 1 {}", hf(v)));
    }

    fn snd_elapsed(&self, _e: usize) -> f64 {
        let a = self.pop("snd_elapsed");
        self.log(format!("snd_elapsed 1 => {a}"));
        f64::from_bits(u64::from_str_radix(&a, 16).unwrap_or(0))
    }

    fn snd_set_elapsed(&self, _e: usize, t: f64) {
        self.log(format!("snd_set_elapsed 1 {}", hd(t)));
    }

    fn snd_commentary(&self, mode: i32) {
        self.log(format!("snd_commentary {mode}"));
    }

    fn snd_reset_world(&self) {
        self.log("snd_reset_world".into());
    }

    fn gui_sound(&self, name: &[u8]) {
        self.log(format!("gui_sound {}", escb(name)));
    }

    fn translate(&self, cat: &[u8], entry: &[u8]) -> Vec<u32> {
        let a = self.pop("translate");
        self.log(format!("translate {} {} => {a}", escb(cat), escb(entry)));
        unesc(&a)
    }

    fn wrap(&self, font: usize, w: f32, l: f32, s: [f32; 2], t: &[u32]) -> Vec<Vec<u32>> {
        let a = self.pop("wrap");
        self.log(format!(
            "wrap {} {} {} {} {} => {a}",
            u8::from(font != 0),
            hf(w),
            hf(l),
            fs(&s),
            esc(t)
        ));
        rows_of(&a)
    }

    fn font_load(&self) -> usize {
        let a = self.pop("font_load");
        self.log(format!("font_load => {a}"));
        1
    }

    fn font_destroy(&self, _f: usize) {
        self.log("font_destroy".into());
    }

    fn gfx_rect(&self, additive: bool) -> usize {
        let a = self.pop("gfx_rect");
        self.log(format!(
            "gfx_rect {} => {a}",
            if additive { "add" } else { "mod" }
        ));
        1
    }

    fn hud(&self) -> Hud {
        let a = self.pop("hud");
        self.log(format!("hud => {a}"));
        let v: Vec<f32> = a.split(' ').map(unhf).collect();
        Hud {
            start: [v[0], v[1], v[2]],
            size: [v[3], v[4]],
        }
    }

    fn draw_gfx(&self, _set: usize, _g: usize, p: [f32; 3], s: [f32; 2], c: [f32; 4]) {
        self.log(format!("draw_gfx 1 {} {} {}", fs(&p), fs(&s), fs(&c)));
    }

    fn draw_font(
        &self,
        font: usize,
        p: [f32; 3],
        s: [f32; 2],
        c: [f32; 4],
        center: bool,
        t: &[u32],
    ) {
        self.log(format!(
            "draw_font {} {} {} {} {} {}",
            u8::from(font != 0),
            fs(&p),
            fs(&s),
            fs(&c),
            u8::from(center),
            esc(t)
        ));
    }

    fn cfg_f32(&self, sec: &[u8], key: &[u8], _def: f32) -> f32 {
        let a = self.pop("cfg_f32");
        self.log(format!("cfg_f32 {} {} => {a}", escb(sec), escb(key)));
        unhf(&a)
    }

    fn cfg_vec2(&self, sec: &[u8], key: &[u8], _def: f32) -> [f32; 2] {
        let a = self.pop("cfg_vec2");
        self.log(format!("cfg_vec2 {} {} => {a}", escb(sec), escb(key)));
        let v: Vec<f32> = a.split(' ').map(unhf).collect();
        [v[0], v[1]]
    }

    fn cfg_str(&self, sec: &[u8], key: &[u8], _def: &[u8]) -> Vec<u8> {
        let a = self.pop("cfg_str");
        self.log(format!("cfg_str {} {} => {a}", escb(sec), escb(key)));
        unesc(&a).iter().map(|&c| c as u8).collect()
    }

    fn cfg_color(&self, sec: &[u8], key: &[u8]) -> [f32; 4] {
        let a = self.pop("cfg_color");
        self.log(format!("cfg_color {} {} => {a}", escb(sec), escb(key)));
        let v: Vec<f32> = a.split(' ').map(unhf).collect();
        [v[0], v[1], v[2], v[3]]
    }

    fn subtitles(&self) -> bool {
        let r = self.pop_flag("subtitles");
        self.log(format!("subtitles => {}", u8::from(r)));
        r
    }

    fn game_time(&self) -> f64 {
        self.log("game_time".into());
        self.time.get()
    }

    fn player_fov(&self, mul: f32, speed: f32) {
        self.log(format!("player_fov {} {}", hf(mul), hf(speed)));
    }

    fn player_active(&self, on: bool) {
        self.log(format!("player_active {}", u8::from(on)));
    }

    fn player_shake(&self, v: Option<[f32; 3]>) {
        self.log(if v.is_some() {
            "player_shake".into()
        } else {
            "player_shake_clear".into()
        });
    }

    fn map_icon(&self, id: i32) {
        self.log(format!("map_icon {id}"));
    }

    fn map_enemy_reset(&self) {
        self.log("map_enemy_reset".into());
    }

    fn map_script(&self, name: &[u8]) {
        self.log(format!("map_script {}", escb(name)));
    }

    fn post(&self, kind: i32, op: i32, a: f32, b: f32) {
        let name = ["params", "active", "reset"][op as usize];
        self.log(format!("post {kind} {name} {} {}", hf(a), hf(b)));
    }

    fn r_reflection(&self, _fns: usize) -> bool {
        let r = self.pop_flag("r_reflection");
        self.rlog(format!("r_reflection => {}", u8::from(r)));
        r
    }

    fn r_collides(&self, _fns: usize, _obj: usize) -> bool {
        let r = self.pop_flag("r_collides");
        self.rlog(format!("r_collides => {}", u8::from(r)));
        r
    }

    fn r_outline(&self, _fns: usize, _obj: usize) -> Outline {
        let a = self.pop("r_outline");
        self.rlog(format!("r_outline => {a}"));
        let t: Vec<&str> = a.split(' ').collect();
        let f = |i: usize| unhf(t[i]);
        let v3 = |i: usize| Vector3f {
            x: f(i),
            y: f(i + 1),
            z: f(i + 2),
        };
        let mut o = Outline {
            collides: t[0].parse().unwrap(),
            has_alpha: t[1].parse().unwrap(),
            min: v3(2),
            max: v3(5),
            local_min: v3(8),
            local_max: v3(11),
            local_center: v3(14),
            ..Outline::default()
        };
        for r in 0..4 {
            for c in 0..4 {
                o.world.m[r][c] = f(17 + r * 4 + c);
            }
        }
        o
    }

    fn r_clip(&self, _fns: usize, _min: Vector3f, _max: Vector3f) -> [i32; 4] {
        let a = self.pop("r_clip");
        self.rlog(format!("r_clip => {a}"));
        let v: Vec<i32> = a.split(' ').map(|x| x.parse().unwrap()).collect();
        [v[0], v[1], v[2], v[3]]
    }

    fn r_replay(&self, _fns: usize, _objs: &[usize], ops: &[ROp], _clip: [i32; 4]) {
        for op in ops {
            self.rlog(format!("R {} {} {} {}", op.kind, op.i, op.j, fs(&op.f)));
        }
    }

    fn p_replay(&self, _e: usize, _input: usize, _fb: usize, ops: &[postfx::Op]) {
        for op in ops {
            let masked = op.kind == postfx::op::FLOAT && (op.b == 1 || op.b == 3);
            let f0 = if masked { "~".to_string() } else { hf(op.f[0]) };
            self.rlog(format!(
                "P {} {} {} {} {}",
                op.kind,
                op.a,
                op.b,
                f0,
                hf(op.f[1])
            ));
        }
    }
}

struct Sim<'a> {
    rec: &'a Rec,
    arena: &'a RefCell<Arena<Handler>>,
    h: crate::obj::Handle<Handler>,
    rend: Rend,
    clock: Clock,
    ins_active: bool,
    log_pos: usize,
}

impl Sim<'_> {
    fn ctx(&self) -> Ctx<'_> {
        Ctx {
            arena: self.arena,
            h: self.h,
            fx: self.rec,
        }
    }
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

fn run(sim: &mut Sim, line: &str) {
    sim.rec.log(format!("cmd{line}"));
    let mut t = tokens(line);
    let cmd = t.next().unwrap_or("");
    let c = sim.ctx();
    let obj = 1usize;
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
            for i in 0..n {
                entries[i] = st(t.next());
                delays[i] = num(t.next());
            }
            let use_pos = num::<i32>(t.next()) != 0;
            let pos = [num(t.next()), num(t.next()), num(t.next())];
            let (min, max): (f32, f32) = (num(t.next()), num(t.next()));
            let (prio, rm): (i32, i32) = (num(t.next()), num(t.next()));
            let e: [&[u8]; 7] = std::array::from_fn(|i| entries[i].as_slice());
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
            sim.clock.wave_alpha = num(t.next());
            sim.clock.zoom_alpha = num(t.next());
            sim.clock.wave_speed = num(t.next());
            let _ = on;
        }
        "rflash" => sim.rend.flash.push((obj, num(t.next()))),
        "rglow" => sim.rend.glow.push((obj, num(t.next()))),
        "routline" => sim.rend.outline.push(obj),
        "rclear_outline" => sim.rend.outline.clear(),
        "rreset" => sim.rend.reset(),
        other => panic!("unknown command {other}"),
    }
}

fn log_state(sim: &Sim, frame: i32) {
    let s: State = sim.ctx().state();
    let rec = sim.rec;
    rec.log(format!(
        "fade {} {} {} {}",
        hf(s.fade.goal),
        hf(s.fade.alpha),
        hf(s.fade.speed),
        s.fade.active
    ));
    let fl = |name: &str, f: &super::effects::Flash| {
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
    };
    rec.log(fl("flash", &s.flash));
    rec.log(fl("sanity", &s.sanity));
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
        rec.log(o);
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
    rec.log(o);
    rec.log(format!("shake act={} n={}", s.shake_active, s.shake_count));
    rec.log(format!(
        "voice act={} pause={} n={} rows={} queue={} entries={} vol={}",
        s.voice_active,
        s.voice_paused,
        s.voice_count,
        s.voice_rows,
        s.voice_queue,
        s.voice_entries,
        hf(s.voice_vol)
    ));
    rec.log(format!(
        "commentary act={} icon={}",
        s.commentary_active, s.commentary_icon
    ));
    rec.log(format!("paused {}", s.player_paused));
    if frame >= 3 {
        rec.log(format!(
            "insanity act={} {} {} {}",
            u8::from(sim.ins_active),
            hf(sim.clock.wave_alpha),
            hf(sim.clock.zoom_alpha),
            hf(sim.clock.wave_speed)
        ));
    }
}

/// Whether the engine reported the insanity post effect active at `frame`: scripts and the game
/// both toggle it, so the recorded line is the source of truth.
fn recorded_ins_active(frame: i32) -> bool {
    let head = format!("tick {frame}");
    let mut inside = false;
    for l in EXPECTED.lines() {
        if l == head {
            inside = true;
        } else if inside && l.starts_with("tick ") {
            break;
        } else if inside && let Some(r) = l.strip_prefix("insanity act=") {
            return r.starts_with('1');
        }
    }
    false
}

/// Render passes the original made in the segment after `tick <frame>`, in order.
fn passes(frame: i32) -> Vec<Option<(f32, f32, bool)>> {
    let head = format!("tick {frame}");
    let mut out = Vec::new();
    let mut inside = false;
    for l in EXPECTED.lines() {
        if l == head {
            inside = true;
            continue;
        }
        if inside && l.starts_with("tick ") {
            break;
        }
        if inside {
            if l.starts_with("r_reflection") {
                out.push(None);
            } else if let Some(r) = l.strip_prefix("p_in ") {
                let v: Vec<&str> = r.split(' ').collect();
                out.push(Some((unhf(v[0]), unhf(v[1]), v[2] == "1")));
            }
        }
    }
    out
}

#[test]
fn golden_replay_matches_cpp_dump() {
    let rec = Rec::new();
    let arena: RefCell<Arena<Handler>> = RefCell::new(Arena::default());
    let h = arena
        .borrow_mut()
        .insert(Handler::new(&rec))
        .expect("arena slot");
    let mut sim = Sim {
        rec: &rec,
        arena: &arena,
        h,
        rend: Rend::new(0xFF),
        clock: Clock::default(),
        ins_active: false,
        log_pos: 0,
    };
    sim.ctx().reset();
    sim.ctx().load_fonts();
    sim.ctx().reset();

    let cmds: Vec<(i32, i32, &str)> = SCRIPT
        .lines()
        .filter(|l| !l.is_empty() && !l.starts_with('#'))
        .map(|l| {
            let (span, rest) = l.split_once(' ').unwrap_or((l, ""));
            let (a, b) = span.split_once('-').unwrap_or((span, span));
            let rest = &l[l.len() - rest.len() - 1..];
            (a.parse().unwrap(), b.parse().unwrap(), rest)
        })
        .collect();

    for frame in 1..=LAST_FRAME {
        rec.frame.set(frame);
        sim.rend.clear_lists();
        sim.rend.outline.clear();
        for (a, b, line) in &cmds {
            if frame >= *a && frame <= *b {
                run(&mut sim, line);
            }
        }
        sim.ins_active = recorded_ins_active(frame);
        sim.ctx().update(DT);
        sim.rend.osc.update(DT);
        if sim.ins_active {
            rend::ins_update(&mut sim.clock, DT);
        }
        rec.log(format!("tick {frame}"));
        log_state(&sim, frame);
        sim.ctx().draw();
        for p in passes(frame) {
            match p {
                None => rend::render_trans(&rec, 0, &sim.rend),
                Some((sx, sy, ok)) => {
                    rec.rlog(format!("p_in {} {} {}", hf(sx), hf(sy), u8::from(ok)));
                    rend::ins_render(&rec, &sim.clock, [sx, sy], ok, 0, 0, 0);
                }
            }
        }
        sim.log_pos += 1;
    }

    let got = rec.lines.borrow();
    let want: Vec<&str> = EXPECTED.lines().collect();
    let limit = got.len().min(want.len());
    for i in 0..limit {
        assert_eq!(got[i].trim_end(), want[i], "line {}", i + 1);
    }
    assert_eq!(got.len(), want.len(), "line count");
    let _ = renderer::PROG_FLASH;
}

const CTOR_ANSWERS: &str = "gfx_rect mod => 1
gfx_rect add => 1
gfx_rect add => 1
cfg_color Player_General SanityGain_Color => 00000000 00000000 00000000 3f800000
cfg_str Player_General SanityGain_Sound =>
cfg_f32 Player_General SanityGain_FadeInTime => 3f800000
cfg_f32 Player_General SanityGain_FadeOutTime => 3f800000
cfg_vec2 Effects VoiceTextFontSize => 41900000 41900000
cfg_f32 Effects VoiceTextRowWidth => 44160000
gfx_rect add => 1
";

fn mark(rec: &Rec) -> usize {
    rec.lines.borrow().len()
}

fn since(rec: &Rec, at: usize) -> Vec<String> {
    rec.lines.borrow().get(at..).unwrap_or(&[]).to_vec()
}

/// Live sound entries, which the dump (no audio in the sweep) never reaches: a higher priority
/// voice interrupts the playing one, the interrupted time is carried and restarts mid-stream.
#[test]
fn voice_interrupt_with_live_entries() {
    let rec = Rec::from_text(&format!(
        "{CTOR_ANSWERS}snd_valid 0 => 0
snd_valid 0 => 0
snd_valid 0 => 0
snd_valid 0 => 0
snd_play_gui a.ogg => 1
snd_valid 1 => 1
snd_valid 0 => 0
snd_valid 1 => 1
snd_valid 0 => 0
snd_elapsed 1 => {}
",
        hd(1.5)
    ));
    rec.time.set(10.0);
    let arena: RefCell<Arena<Handler>> = RefCell::new(Arena::default());
    let h = arena
        .borrow_mut()
        .insert(Handler::new(&rec))
        .expect("arena slot");
    let c = Ctx {
        arena: &arena,
        h,
        fx: &rec,
    };
    let none: [&[u8]; 7] = [b""; 7];
    let add = |file: &[u8], prio: i32| {
        c.voice_add(
            file, b"", b"", none, [0.0; 7], false, [0.0; 3], 0.0, 0.0, prio, true,
        );
    };
    add(b"a.ogg", 1);
    c.update(DT);
    assert_eq!(c.with(|s| s.voice.voice_entry), Some(1));
    // queued behind the playing voice, then interrupted by a higher priority one
    add(b"c.ogg", 1);
    let at = mark(&rec);
    add(b"b.ogg", 5);
    let log = since(&rec, at);
    assert!(
        log.contains(&"snd_fade_out 1 3f000000".to_string()),
        "{log:?}"
    );
    assert!(
        log.contains(&format!("snd_elapsed 1 => {}", hd(1.5))),
        "{log:?}"
    );
    let (cur, intr, n, front) = c
        .with(|s| {
            let f = s.voice.list.first().cloned().unwrap_or_default();
            (
                f.current_time,
                f.interrupted_at,
                s.voice.list.len(),
                f.voice_file,
            )
        })
        .expect("handler");
    assert_eq!((n, front.as_slice()), (1, &b"b.ogg"[..]));
    // the interrupted voice (c) was popped, b carries the interruption time
    assert_eq!((cur, intr), (0.0, 10.0));
    assert_eq!(c.with(|s| s.voice.voice_entry), Some(0));
}

#[test]
fn commentary_start_stop_with_live_entry() {
    let rec = Rec::from_text(&format!(
        "{CTOR_ANSWERS}snd_play_gui talk.ogg => 1
snd_valid 1 => 1
"
    ));
    let arena: RefCell<Arena<Handler>> = RefCell::new(Arena::default());
    let h = arena
        .borrow_mut()
        .insert(Handler::new(&rec))
        .expect("arena slot");
    let c = Ctx {
        arena: &arena,
        h,
        fx: &rec,
    };
    let at = mark(&rec);
    c.commentary_start(7, b"talk.ogg");
    assert!(c.with(|s| s.commentary_active).unwrap_or(false));
    c.commentary_stop();
    let log = since(&rec, at);
    assert_eq!(
        log,
        [
            "snd_play_gui talk.ogg => 1",
            "snd_commentary 1",
            "snd_valid 1 => 1",
            "snd_fade_out 1 3f800000",
            "snd_commentary 2",
        ]
    );
    assert!(!c.with(|s| s.commentary_active).unwrap_or(true));
}
