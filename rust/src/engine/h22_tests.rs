//! H22 replays: the headless-control transcripts (hc_*.txt) and frame-loop traces (run_*.txt) the
//! original C++ produced (rust/tests/golden/H22/harness), plus init/exit order.

use super::frame::Loop;
use super::h22_engine::{self as eng, RunState};
use super::h22_engine_ffi::{CHost, op};
use super::h22_hc::{Ev, HcHost, HcServer, LogLevel, MouseButton, Shot};
use std::cell::{Cell, RefCell};
use std::ffi::c_void;
use std::io::{Read, Write};
use std::os::unix::net::UnixStream;
use std::path::{Path, PathBuf};

fn golden_dir() -> PathBuf {
    Path::new(env!("CARGO_MANIFEST_DIR")).join("tests/golden/H22")
}

fn golden_files(prefix: &str) -> Vec<PathBuf> {
    let mut v: Vec<PathBuf> = std::fs::read_dir(golden_dir())
        .map(|d| {
            d.filter_map(Result::ok)
                .map(|e| e.path())
                .filter(|p| {
                    p.file_name()
                        .and_then(|n| n.to_str())
                        .is_some_and(|n| n.starts_with(prefix) && n.ends_with(".txt"))
                })
                .collect()
        })
        .unwrap_or_default();
    v.sort();
    v
}

// ---- frame loop ----

const OP_WINDOW_MOUSE: i32 = 100;
const OP_WINDOW_VISIBLE: i32 = 101;

const OP_NAMES: [&str; 40] = [
    "LOG",
    "LOG_MEDIUM",
    "BROADCAST",
    "RUN_MESSAGE",
    "CONTAINER_IS_DEFAULT",
    "LT_RESET",
    "WANT_UPDATE",
    "END_UPDATE_LOOP",
    "STEP_SIZE",
    "GAME_IS_DONE",
    "WAIT_OUT_OF_FOCUS",
    "WINDOW_INPUT_FOCUS",
    "SLEEP",
    "INPUT_UPDATE",
    "PAUSED",
    "HAS_HEADLESS",
    "HC_LOGIC_STEP",
    "HC_UPDATE",
    "QUIT_POSTED",
    "RESET_QUIT",
    "WINDOW_FOCUS_BITS",
    "TAKE_PLUGGED",
    "TAKE_REMOVED",
    "UPDATE_LOG_ACTIVE",
    "CLEAR_UPDATE_LOG",
    "RENDER_ONCE",
    "LIMIT_FPS",
    "SWAP",
    "TIMING",
    "FT_START",
    "FT_STOP",
    "FT_SEC",
    "SCENE_RENDER",
    "DEVHUD_RENDER",
    "FLUSH",
    "DIAG_END_FRAME",
    "DIAG_ADD_TIMING",
    "FPS_ADD",
    "APP_TIME",
    "STEADY_NS",
];

type Trace = Vec<(String, Vec<f64>, f64)>;
type ShaderRec = (Vec<u8>, Vec<u8>, bool, Vec<u8>);

/// The scripted host of h22_run.cpp.
struct RunMock {
    s: u64,
    budget: i64,
    calls: i64,
    done: bool,
    paused: bool,
    quit: bool,
    render_once: bool,
    limit_fps: bool,
    headless: bool,
    wait_focus: bool,
    update_log: bool,
    app_time: u64,
    steady_ns: i64,
    trace: Trace,
}

impl RunMock {
    fn new(seed: u32) -> Self {
        let mut m = Self {
            s: 0x9E37_79B9_7F4A_7C15u64.wrapping_mul(u64::from(seed) + 1),
            budget: 0,
            calls: 0,
            done: false,
            paused: false,
            quit: false,
            render_once: false,
            limit_fps: !seed.is_multiple_of(5),
            headless: seed.is_multiple_of(2),
            wait_focus: seed.is_multiple_of(3),
            update_log: seed % 4 == 1,
            app_time: 1000,
            steady_ns: 5_000_000_000,
            trace: Vec::new(),
        };
        for _ in 0..4 {
            m.rnd();
        }
        m.budget = 120 + (m.rnd() % 500) as i64 + if seed % 7 == 2 { 3000 } else { 0 };
        m
    }

    fn rnd(&mut self) -> u64 {
        self.s ^= self.s >> 12;
        self.s ^= self.s << 25;
        self.s ^= self.s >> 27;
        self.s.wrapping_mul(0x2545_F491_4F6C_DD1D)
    }

    fn pm(&mut self, n: u64) -> bool {
        (self.rnd() >> 11) % 1000 < n
    }

    fn op(&mut self, o: i32, a: [f64; 4]) -> f64 {
        match o {
            op::HAS_HEADLESS => return f64::from(u8::from(self.headless)),
            op::WAIT_OUT_OF_FOCUS => return f64::from(u8::from(self.wait_focus)),
            op::RENDER_ONCE => return f64::from(u8::from(self.render_once)),
            op::LIMIT_FPS => return f64::from(u8::from(self.limit_fps)),
            op::WINDOW_FOCUS_BITS => {
                let i = self.op(op::WINDOW_INPUT_FOCUS, [0.0; 4]) != 0.0;
                let m = self.op(OP_WINDOW_MOUSE, [0.0; 4]) != 0.0;
                let v = self.op(OP_WINDOW_VISIBLE, [0.0; 4]) != 0.0;
                return f64::from(u8::from(i) | (u8::from(m) << 1) | (u8::from(v) << 2));
            }
            _ => {}
        }
        self.calls += 1;
        let mut r = 0.0;
        match o {
            op::GAME_IS_DONE => {
                if self.calls >= self.budget {
                    self.done = true;
                }
                r = f64::from(u8::from(self.done));
            }
            op::WANT_UPDATE => r = f64::from(u8::from(self.pm(450))),
            op::STEP_SIZE => r = f64::from(1.0f32 / 60.0f32),
            op::PAUSED => r = f64::from(u8::from(self.paused)),
            op::WINDOW_INPUT_FOCUS | OP_WINDOW_MOUSE => r = f64::from(u8::from(self.pm(500))),
            OP_WINDOW_VISIBLE => r = f64::from(u8::from(self.pm(700))),
            op::QUIT_POSTED => r = f64::from(u8::from(self.quit)),
            op::RESET_QUIT => self.quit = false,
            op::TAKE_PLUGGED | op::TAKE_REMOVED => r = f64::from(u8::from(self.pm(60))),
            op::UPDATE_LOG_ACTIVE => r = f64::from(u8::from(self.update_log)),
            op::CONTAINER_IS_DEFAULT => r = f64::from(u8::from(self.pm(600))),
            op::FT_SEC => r = 0.001 * f64::from(1 + (self.rnd() % 30) as i32),
            op::APP_TIME => {
                self.app_time += 1 + self.rnd() % 25;
                r = self.app_time as f64;
            }
            op::STEADY_NS => {
                self.steady_ns += 100_000 + (self.rnd() % 3_000_000) as i64;
                r = self.steady_ns as f64;
            }
            op::RUN_MESSAGE => {
                let v = self.rnd() % 1000;
                if v < 20 {
                    self.paused = !self.paused;
                } else if v < 40 {
                    self.render_once = !self.render_once;
                } else if v < 60 {
                    self.limit_fps = !self.limit_fps;
                } else if v < 75 {
                    self.quit = true;
                }
            }
            _ => {}
        }
        let name = match o {
            OP_WINDOW_MOUSE => "WINDOW_MOUSE_FOCUS",
            OP_WINDOW_VISIBLE => "WINDOW_VISIBLE",
            _ => OP_NAMES.get(o as usize).copied().unwrap_or("?"),
        };
        let args = a.into_iter().filter(|x| *x != 0.0).collect();
        self.trace.push((name.to_string(), args, r));
        r
    }
}

unsafe extern "C" fn run_call(ctx: *mut c_void, o: i32, a0: f64, a1: f64, a2: f64, a3: f64) -> f64 {
    if o == op::STEP {
        return 0.0;
    }
    // SAFETY: `ctx` is the RefCell<RunMock> of the test; no other borrow is live across the call.
    unsafe {
        (*ctx.cast::<RefCell<RunMock>>())
            .borrow_mut()
            .op(o, [a0, a1, a2, a3])
    }
}

fn parse_trace_line(line: &str) -> Option<(String, Vec<f64>, f64)> {
    let (head, res) = match line.split_once(" -> ") {
        Some((h, r)) => (h, r.parse::<f64>().ok()?),
        None => (line, 0.0),
    };
    let mut it = head.split(' ');
    let name = it.next()?.to_string();
    let args = it
        .map(|t| t.parse::<f64>().ok())
        .collect::<Option<Vec<_>>>()?;
    Some((name, args, res))
}

fn same(a: f64, b: f64) -> bool {
    a.to_bits() == b.to_bits()
}

#[test]
fn run_traces_match_cpp() {
    let files = golden_files("run_");
    assert!(files.len() >= 8, "run goldens missing");
    let mut ops = 0;
    for f in files {
        let seed: u32 = f
            .file_stem()
            .and_then(|s| s.to_str())
            .and_then(|s| s.strip_prefix("run_"))
            .and_then(|s| s.parse().ok())
            .expect("seed in file name");
        let text = std::fs::read_to_string(&f).expect("golden");
        let mock = RefCell::new(RunMock::new(seed));
        let mut lp = Loop::default();
        let mut ft = 0.0f32;
        let mut bits = 0u32;
        let host = CHost::new(run_call, std::ptr::from_ref(&mock).cast_mut().cast());
        eng::run(
            &host,
            &RunState {
                lp: &raw mut lp,
                frame_time: &raw mut ft,
                focus_bits: &raw mut bits,
            },
        );
        let m = mock.borrow();
        let mut expect: Trace = Vec::new();
        let mut state = Vec::new();
        for line in text.lines() {
            if line.starts_with("loop ") || line.starts_with("frameTime=") {
                state.push(line);
            } else {
                expect.push(parse_trace_line(line).expect("trace line"));
            }
        }
        assert_eq!(expect.len(), m.trace.len(), "{f:?}: op count");
        for (i, (e, g)) in expect.iter().zip(&m.trace).enumerate() {
            let ok = e.0 == g.0
                && e.1.len() == g.1.len()
                && e.1.iter().zip(&g.1).all(|(a, b)| same(*a, *b))
                && same(e.2, g.2);
            assert!(ok, "{f:?}: op {i}: want {e:?} got {g:?}");
        }
        ops += expect.len();
        let want = state.join(" ");
        let got = format!(
            "loop gameTime={} numOfTimes={} logicMs={} swapMs={} steps={} clearCheck={} isUpdated={} bufferSwap={} swappedOnce={}",
            lp.game_time,
            lp.num_of_times,
            lp.logic_ms,
            lp.swap_ms,
            lp.steps,
            lp.clear_check,
            i32::from(lp.is_updated),
            i32::from(lp.buffer_swap),
            i32::from(lp.swapped_once)
        );
        let kv = |s: &str| -> Vec<(String, String)> {
            s.split(' ')
                .skip(1)
                .filter_map(|t| {
                    t.split_once('=')
                        .map(|(k, v)| (k.to_string(), v.to_string()))
                })
                .collect()
        };
        for ((k, w), (_, g)) in kv(&want).iter().zip(kv(&got).iter()) {
            match (w.parse::<f64>(), g.parse::<f64>()) {
                (Ok(w), Ok(g)) if k != "frameTime" && k != "focusBits" => {
                    assert!(same(w, g), "{f:?}: {k}: {w} vs {g}");
                }
                _ => {}
            }
        }
        let ft_want: f64 = want
            .split("frameTime=")
            .nth(1)
            .and_then(|s| s.split(' ').next())
            .and_then(|s| s.parse().ok())
            .expect("frameTime");
        assert_eq!(ft, ft_want as f32, "{f:?}: frame time");
        let bits_want: u32 = want
            .split("focusBits=")
            .nth(1)
            .and_then(|s| s.trim().parse().ok())
            .expect("focusBits");
        assert_eq!(bits, bits_want, "{f:?}: focus bits");
    }
    assert!(ops > 1000);
}

// ---- init / exit order ----

#[test]
fn init_exit_steps_match_source_order() {
    let want = std::fs::read_to_string(golden_dir().join("steps.txt")).expect("steps.txt");
    let got: Vec<String> = eng::init_steps()
        .iter()
        .map(|s| format!("init {s:?}"))
        .chain(eng::exit_steps().iter().map(|s| format!("exit {s:?}")))
        .collect();
    let want: Vec<&str> = want.lines().collect();
    assert_eq!(want, got);
}

// ---- headless control ----

const KEYS: [(&[u8], i32, i32); 22] = [
    (b"a", 97, 4),
    (b"b", 98, 5),
    (b"z", 122, 29),
    (b"A", 97, 4),
    (b"0", 48, 39),
    (b"9", 57, 38),
    (b"Return", 13, 40),
    (b"Space", 32, 44),
    (b"Escape", 27, 41),
    (b"Tab", 9, 43),
    (b"Left", 1_073_741_904, 80),
    (b"Right", 1_073_741_903, 79),
    (b"Up", 1_073_741_906, 82),
    (b"Down", 1_073_741_905, 81),
    (b"F1", 1_073_741_882, 58),
    (b"F12", 1_073_741_893, 69),
    (b"F5", 1_073_741_886, 62),
    (b"Left Shift", 1_073_742_049, 225),
    (b"Backspace", 8, 42),
    (b"RETURN", 13, 40),
    (b"return", 13, 40),
    (b"\xc3\xa9", 233, 0),
];

fn hex(b: &[u8]) -> String {
    b.iter().map(|x| format!("{x:02x}")).collect()
}

fn unescape(s: &[u8]) -> Vec<u8> {
    let mut o = Vec::new();
    let mut i = 0;
    while i < s.len() {
        let c = s.get(i).copied().unwrap_or_default();
        if c == b'\\' && i + 1 < s.len() {
            i += 1;
            let c = s.get(i).copied().unwrap_or_default();
            if c == b'n' {
                o.push(b'\n');
            } else if c == b'x' && i + 2 < s.len() {
                let h = std::str::from_utf8(s.get(i + 1..i + 3).unwrap_or_default()).unwrap_or("0");
                o.push(u8::from_str_radix(h, 16).unwrap_or(0));
                i += 2;
            } else {
                o.push(c);
            }
        } else {
            o.push(c);
        }
        i += 1;
    }
    o
}

#[derive(Default)]
struct Knobs {
    copy_ok: Cell<bool>,
    save_ok: Cell<bool>,
    resize_ok: Cell<bool>,
    float_ok: Cell<bool>,
    gamma100: Cell<i32>,
}

struct FakeHost {
    out: RefCell<Vec<String>>,
    k: Knobs,
    frames: Cell<u32>,
    shaders: RefCell<Vec<ShaderRec>>,
    events: RefCell<Vec<Ev>>,
    injected: RefCell<Vec<Vec<u8>>>,
    exited: Cell<bool>,
    dev_hud: Cell<bool>,
    server: Cell<*const HcServer>,
}

impl FakeHost {
    fn new() -> Self {
        let h = Self {
            out: RefCell::new(Vec::new()),
            k: Knobs::default(),
            frames: Cell::new(0),
            shaders: RefCell::new(Vec::new()),
            events: RefCell::new(Vec::new()),
            injected: RefCell::new(Vec::new()),
            exited: Cell::new(false),
            dev_hud: Cell::new(false),
            server: Cell::new(std::ptr::null()),
        };
        h.k.copy_ok.set(true);
        h.k.save_ok.set(true);
        h.k.resize_ok.set(true);
        h.k.float_ok.set(true);
        h.k.gamma100.set(100);
        h
    }

    fn say(&self, s: impl Into<String>) {
        let s: String = s.into();
        self.out
            .borrow_mut()
            .extend(s.split('\n').map(str::to_string));
    }

    fn pixels(&self) -> Vec<u8> {
        (0..32u32).map(|i| (i * 37 + 11) as u8).collect()
    }
}

impl HcHost for FakeHost {
    fn now_ms(&self) -> u64 {
        std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .map_or(0, |d| d.as_millis() as u64)
    }
    fn rendered_frames(&self) -> u32 {
        self.frames.get()
    }
    fn log(&self, _level: LogLevel, _msg: &[u8]) {}
    fn exit(&self) {
        self.exited.set(true);
    }
    fn set_wait_if_out_of_focus(&self, _on: bool) {}
    fn dev_hud_active(&self) -> bool {
        self.dev_hud.get()
    }
    fn set_dev_hud_active(&self, on: bool) {
        self.dev_hud.set(on);
    }
    fn screen_float_stats(&self) -> Option<(f32, f32, f32)> {
        self.k.float_ok.get().then_some((1.5, 0.25, 0.125))
    }
    fn force_window_size(&self, w: i32, h: i32) -> bool {
        self.say(format!("state resize {w}x{h}"));
        self.k.resize_ok.get()
    }
    fn shader_fail_count(&self) -> i32 {
        self.shaders.borrow().iter().filter(|s| !s.2).count() as i32
    }
    fn shader_report_json(&self, failed_only: bool) -> Vec<u8> {
        let mut out = b"[".to_vec();
        let mut first = true;
        for (name, stage, ok, log) in self.shaders.borrow().iter() {
            if failed_only && *ok {
                continue;
            }
            if !first {
                out.push(b',');
            }
            first = false;
            out.extend_from_slice(b"{\"name\":\"");
            out.extend(crate::system::json_escape(name));
            out.extend_from_slice(b"\",\"stage\":\"");
            out.extend_from_slice(stage);
            out.extend_from_slice(b"\",\"ok\":");
            out.extend_from_slice(if *ok { b"true" } else { b"false" });
            out.extend_from_slice(b",\"log\":\"");
            out.extend(crate::system::json_escape(log));
            out.extend_from_slice(b"\"}");
        }
        out.push(b']');
        out
    }
    fn frame_stats(&self) -> Option<(Vec<u8>, Vec<u8>)> {
        self.k.copy_ok.get().then(|| {
            (
                crate::system::frame_stats_json(&self.pixels(), 4, 2, 4),
                b"{}".to_vec(),
            )
        })
    }
    fn xdg_cache_home(&self) -> Vec<u8> {
        b"$XDG/".to_vec()
    }
    fn ensure_folder(&self, _dir: &[u8]) {}
    fn screenshot(&self, path: &[u8]) -> Shot {
        if !self.k.copy_ok.get() {
            return Shot::CopyFailed;
        }
        let mut px = self.pixels();
        for i in (3..px.len()).step_by(4) {
            if let Some(p) = px.get_mut(i) {
                *p = 255;
            }
        }
        let g = self.k.gamma100.get() as f32 / 100.0;
        if g > 0.0 && (g - 1.0).abs() >= 0.001 {
            let lut = crate::system::gamma_lut(g);
            let mut i = 0;
            while i + 2 < px.len() {
                for c in 0..3 {
                    if let Some(p) = px.get_mut(i + c) {
                        *p = lut.get(usize::from(*p)).copied().unwrap_or(*p);
                    }
                }
                i += 4;
            }
        }
        let ext = crate::string::to_lower(crate::string::file_ext(path));
        if ext != b"bmp" {
            let mut msg = b"ERROR: No loader for file extension '".to_vec();
            msg.extend_from_slice(&ext);
            msg.extend_from_slice(b"' found!\n");
            let p = self.server.get();
            if !p.is_null() {
                // SAFETY: set right after `HcServer::new`, cleared before the server drops.
                unsafe { (*p).push_log_line(&msg) };
            }
            return Shot::SaveFailed;
        }
        let mut h: u64 = 1_469_598_103_934_665_603;
        for b in &px {
            h = (h ^ u64::from(*b)).wrapping_mul(1_099_511_628_211);
        }
        self.say(format!(
            "state shot {} bpp=4 n={} h={h:016x}",
            String::from_utf8_lossy(path),
            px.len()
        ));
        if self.k.save_ok.get() {
            Shot::Saved
        } else {
            Shot::SaveFailed
        }
    }
    fn key_from_name(&self, name: &[u8]) -> i32 {
        KEYS.iter().find(|k| k.0 == name).map_or(0, |k| k.1)
    }
    fn scancode_from_key(&self, key: i32) -> i32 {
        KEYS.iter().find(|k| k.1 == key).map_or(0, |k| k.2)
    }
    fn push_event(&self, ev: &Ev) {
        match ev {
            Ev::Text(t) => self.injected.borrow_mut().push(t.clone()),
            other => self.events.borrow_mut().push(other.clone()),
        }
    }
    fn run_message(&self, msg: i32) {
        match msg {
            11 => self.say("state msg got_input_focus"),
            14 => self.say("state msg lost_input_focus"),
            _ => {}
        }
    }
}

/// The SDL2 compat layer on SDL3 carries pointer coordinates as floats.
fn sdl3_int(v: i32) -> i32 {
    let f = v as f32;
    if f >= 2_147_483_648.0 {
        i32::MIN
    } else {
        f as i32
    }
}

fn describe(ev: &Ev) -> String {
    match ev {
        Ev::Key {
            down,
            sym,
            scancode,
        } => format!(
            "ev key type={} state={} repeat=0 sym={sym} sc={scancode} mod=0",
            if *down { "300" } else { "301" },
            i32::from(*down)
        ),
        Ev::Motion { x, y, xrel, yrel } => format!(
            "ev motion x={} y={} xrel={} yrel={}",
            sdl3_int(*x),
            sdl3_int(*y),
            sdl3_int(*xrel),
            sdl3_int(*yrel)
        ),
        Ev::Button { button, down, x, y } => format!(
            "ev button type={} button={} state={} clicks=1 x={} y={}",
            if *down { "401" } else { "402" },
            match button {
                MouseButton::Left => 1,
                MouseButton::Middle => 2,
                MouseButton::Right => 3,
            },
            i32::from(*down),
            sdl3_int(*x),
            sdl3_int(*y)
        ),
        Ev::Text(_) => String::new(),
    }
}

fn int_token(s: &str) -> (i32, &str) {
    let s = s.trim_start();
    let end = s
        .char_indices()
        .find(|(i, c)| !(c.is_ascii_digit() || (*i == 0 && (*c == '-' || *c == '+'))))
        .map_or(s.len(), |(i, _)| i);
    let (n, rest) = s.split_at(end);
    (n.parse().unwrap_or(0), rest)
}

fn replay_hc(golden: &str, id: usize) -> Vec<String> {
    let host = FakeHost::new();
    let dir = std::env::var("XDG_RUNTIME_DIR")
        .unwrap_or_else(|_| std::env::temp_dir().display().to_string());
    let path = format!("{dir}/ohpl-h22t-{}-{id}.sock", std::process::id());
    let server = HcServer::new(&host, path.as_bytes());
    assert!(server.is_listening());
    host.server.set(&raw const server);
    let mut client = UnixStream::connect(&path).expect("connect");
    client.set_nonblocking(true).expect("nonblocking");
    let mut recv = Vec::new();
    let mut loading = false;
    let mut exit_seen = false;
    let mut hud = false;
    let mut sent = 0u64;
    let mut splitter = crate::system::headless::LineSplitter::default();

    let drain = |recv: &mut Vec<u8>, client: &mut UnixStream, host: &FakeHost| {
        let mut buf = [0u8; 4096];
        while let Ok(n) = client.read(&mut buf) {
            if n == 0 {
                break;
            }
            recv.extend_from_slice(buf.get(..n).unwrap_or_default());
        }
        while let Some(nl) = recv.iter().position(|&c| c == b'\n') {
            let line: Vec<u8> = recv.drain(..=nl).collect();
            let line = line.get(..nl).unwrap_or_default();
            host.say(format!("< {}", String::from_utf8_lossy(line)));
        }
    };
    let effects = |host: &FakeHost, exit_seen: &mut bool, hud: &mut bool| {
        for ev in host.events.take() {
            host.say(describe(&ev));
        }
        for t in host.injected.take() {
            host.say(format!("inj {}", hex(&t)));
        }
        if host.exited.get() && !*exit_seen {
            *exit_seen = true;
            host.say("state exit");
        }
        if host.dev_hud.get() != *hud {
            *hud = host.dev_hud.get();
            host.say(format!("state dev_hud {}", u8::from(*hud)));
        }
    };
    macro_rules! pump {
        ($n:expr) => {
            for _ in 0..$n {
                server.update(&host, loading);
                server.logic_step(&host);
                effects(&host, &mut exit_seen, &mut hud);
                drain(&mut recv, &mut client, &host);
            }
        };
    }

    for line in golden.lines() {
        let head = line.split(' ').next().unwrap_or_default();
        if !matches!(
            head,
            ">" | ">>" | "settle" | "pump" | "loading" | "frames" | "log" | "set" | "shader"
        ) {
            continue;
        }
        host.say(line);
        let (cmd, rest) = line.split_once(' ').unwrap_or((line, ""));
        match cmd {
            ">" | ">>" => {
                let data = unescape(rest.as_bytes());
                sent += splitter.push(&data).len() as u64;
                client.write_all(&data).expect("send");
            }
            "settle" => {
                let t = std::time::Instant::now();
                while server.enqueued() < sent && t.elapsed().as_secs() < 5 {
                    std::thread::sleep(std::time::Duration::from_micros(200));
                }
                assert_eq!(
                    server.enqueued(),
                    sent,
                    "server did not queue every request"
                );
            }
            "pump" => {
                let n: i32 = rest.trim().parse().unwrap_or(0);
                pump!(n);
            }
            "loading" => loading = rest.trim().parse::<i32>().unwrap_or(0) != 0,
            "frames" => host
                .frames
                .set(host.frames.get() + rest.trim().parse::<u32>().unwrap_or(0)),
            "log" => server.push_log_line(&unescape(rest.as_bytes())),
            "set" => {
                let mut t = rest.split_whitespace();
                let k = t.next().unwrap_or_default();
                let v: i32 = t.next().and_then(|v| v.parse().ok()).unwrap_or(0);
                match k {
                    "copy_ok" => host.k.copy_ok.set(v != 0),
                    "save_ok" => host.k.save_ok.set(v != 0),
                    "resize_ok" => host.k.resize_ok.set(v != 0),
                    "float_ok" => host.k.float_ok.set(v != 0),
                    "gamma100" => host.k.gamma100.set(v),
                    _ => {}
                }
            }
            "shader" => {
                let rest = rest.trim_start();
                let (name, rest) = rest.split_once(' ').unwrap_or((rest, ""));
                let rest = rest.trim_start();
                let (stage, rest) = rest.split_once(' ').unwrap_or((rest, ""));
                let (ok, log) = int_token(rest);
                let log = log.get(1..).unwrap_or_default();
                host.shaders.borrow_mut().push((
                    unescape(name.as_bytes()),
                    stage.as_bytes().to_vec(),
                    ok != 0,
                    unescape(log.as_bytes()),
                ));
            }
            _ => {}
        }
        drain(&mut recv, &mut client, &host);
    }
    pump!(2);
    host.server.set(std::ptr::null());
    drop(server);
    host.out.take()
}

#[test]
fn headless_transcripts_match_cpp() {
    let files = golden_files("hc_");
    assert!(files.len() >= 4, "hc goldens missing");
    for (id, f) in files.iter().enumerate() {
        let golden = std::fs::read_to_string(f).expect("golden");
        let got = replay_hc(&golden, id);
        let want: Vec<&str> = golden.lines().collect();
        let first_diff = want.iter().zip(&got).position(|(a, b)| a != b);
        assert!(
            first_diff.is_none() && want.len() == got.len(),
            "{f:?}: line {:?}: want {:?} got {:?} (lines {} vs {})",
            first_diff,
            first_diff.and_then(|i| want.get(i)),
            first_diff.and_then(|i| got.get(i)),
            want.len(),
            got.len()
        );
    }
}

#[test]
fn drag_path_matches_sscanf_loop() {
    use super::h22_hc::parse_drag_path;
    let p = |s: &str| {
        parse_drag_path(s.as_bytes())
            .into_iter()
            .collect::<Vec<_>>()
    };
    assert_eq!(p("1,2;3,4"), [(1, 2), (3, 4)]);
    assert_eq!(p(" 7 , 8"), []);
    assert_eq!(p("7 ,8"), []);
    assert_eq!(p("7, 8;9,10"), [(7, 8), (9, 10)]);
    assert_eq!(p("1,2;;3,4"), [(1, 2)]);
    assert_eq!(p("+5,-6"), [(5, -6)]);
}

#[test]
fn text_chunks_split_on_utf8_boundaries() {
    use super::h22_hc::text_chunks;
    let s = "é".repeat(20);
    let c = text_chunks(s.as_bytes());
    assert!(
        c.iter()
            .all(|p| std::str::from_utf8(p).is_ok() && p.len() <= 31)
    );
    assert_eq!(c.concat(), s.as_bytes());
    assert!(text_chunks(b"").is_empty());
}
