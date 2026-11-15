//! Replay of real-engine dumps of the pre-menu, credits, load screen and progress log: the C++ classes
//! were traced running the game (`scripts/a10b-scenario.sh`), the Rust classes must issue the same
//! engine calls given the same events and engine answers.

use super::a10b_cr as cr;
use super::a10b_log as log;
use super::a10b_ls as ls;
use super::a10b_pm as pm;
use super::host::{self, Arg, Eng, Host, Res, Rk, Tracer};
use crate::binary_buffer::BinaryBuffer;
use std::cell::{Cell, RefCell};
use std::collections::HashMap;
use std::io::Write;
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::{Arc, Mutex};

static SERIAL: Mutex<()> = Mutex::new(());

fn inflate(z: &[u8]) -> Vec<String> {
    let mut b = BinaryBuffer::new();
    assert!(b.decompress_and_add(z));
    let mut out = vec![0u8; b.size()];
    b.set_pos(0);
    assert!(b.get_data(&mut out));
    String::from_utf8(out)
        .unwrap()
        .lines()
        .map(str::to_string)
        .collect()
}

fn unquote(s: &str) -> Vec<u32> {
    let inner = &s[1..s.len() - 1];
    let b = inner.as_bytes();
    let mut out = Vec::new();
    let mut i = 0;
    while i < b.len() {
        if b[i] == b'\\' && b[i + 1] == b'x' {
            out.push(u32::from_str_radix(&inner[i + 2..i + 4], 16).unwrap());
            i += 4;
        } else if b[i] == b'\\' && b[i + 1] == b'u' {
            let end = inner[i..].find('}').unwrap() + i;
            out.push(u32::from_str_radix(&inner[i + 3..end], 16).unwrap());
            i = end + 1;
        } else {
            out.push(u32::from(b[i]));
            i += 1;
        }
    }
    out
}

/// Splits on spaces outside quotes.
fn tokens(line: &str) -> Vec<String> {
    let mut out = Vec::new();
    let mut cur = String::new();
    let mut quoted = false;
    for c in line.chars() {
        if c == '"' {
            quoted = !quoted;
        }
        if c == ' ' && !quoted {
            out.push(std::mem::take(&mut cur));
        } else {
            cur.push(c);
        }
    }
    out.push(cur);
    out
}

fn split_commas(s: &str) -> Vec<String> {
    let mut out = Vec::new();
    let mut cur = String::new();
    let mut quoted = false;
    for c in s.chars() {
        if c == '"' {
            quoted = !quoted;
        }
        if c == ',' && !quoted {
            out.push(std::mem::take(&mut cur));
        } else {
            cur.push(c);
        }
    }
    out.push(cur);
    out
}

fn fbits(t: &str) -> u64 {
    u64::from(u32::from_str_radix(&t[1..], 16).unwrap())
}

fn parse_res(kind: Rk, text: &str) -> Res {
    let mut r = Res::default();
    match kind {
        Rk::None => {}
        Rk::P => {
            let n: u64 = text[1..].parse().unwrap();
            r.u[0] = if n == 0 { 0 } else { 0x10000 + n * 16 };
        }
        Rk::I => r.u[0] = text.parse::<i64>().unwrap() as u64,
        Rk::B => r.u[0] = text.parse().unwrap(),
        Rk::F(_) => {
            for (i, t) in text.split(',').enumerate() {
                r.u[i] = fbits(t);
            }
        }
        Rk::U(_) => {
            for (i, t) in text.split(',').enumerate() {
                r.u[i] = t.parse().unwrap();
            }
        }
        Rk::Str => r.s.push(unquote(text)),
        Rk::Strs => {
            if !text.is_empty() {
                r.s = split_commas(text).iter().map(|t| unquote(t)).collect();
            }
        }
    }
    r
}

struct Replay<'a> {
    lines: &'a [String],
    pos: Arc<AtomicUsize>,
    pm_handle: Cell<u64>,
    tr: &'a Tracer,
    handles: RefCell<HashMap<&'static str, u64>>,
}

impl Replay<'_> {
    fn peek(&self) -> &str {
        self.lines
            .get(self.pos.load(Ordering::SeqCst))
            .map_or("", String::as_str)
    }
}

impl Host for Replay<'_> {
    fn call(&self, op: i32, args: &[Arg]) -> Res {
        self.run_events();
        // A slider change with callback runs the slider-move callback before the call returns.
        if op == host::SLIDER_SET
            && matches!(args.get(2), Some(Arg::B(true)))
            && self.peek().starts_with("slider_get ")
        {
            let e = Eng {
                h: self,
                tr: Some(self.tr),
            };
            pm::callback(&e, self.pm_handle.get(), pm::TAG_GAMMA_CHANGE, 0);
        }
        let (name, kind) = host::rk(op);
        let line = self.peek().to_string();
        assert!(
            line.starts_with(name),
            "call {name} against golden line {} {line}",
            self.pos.load(Ordering::SeqCst)
        );
        match line.split_once(" -> ") {
            Some((_, res)) => parse_res(kind, res),
            None => Res::default(),
        }
    }
}

/// Checks every traced line against the golden at the shared position and advances it.
struct Sink {
    lines: Arc<Vec<String>>,
    pos: Arc<AtomicUsize>,
    partial: String,
}

impl Write for Sink {
    fn write(&mut self, b: &[u8]) -> std::io::Result<usize> {
        self.partial.push_str(std::str::from_utf8(b).unwrap());
        while let Some(i) = self.partial.find('\n') {
            let line: String = self.partial.drain(..=i).collect();
            let line = line.trim_end_matches('\n');
            let at = self.pos.fetch_add(1, Ordering::SeqCst);
            assert_eq!(
                Some(line),
                self.lines.get(at).map(String::as_str),
                "golden line {at}"
            );
        }
        Ok(b.len())
    }

    fn flush(&mut self) -> std::io::Result<()> {
        Ok(())
    }
}

impl Replay<'_> {
    fn handle_or(&self, k: &str) -> u64 {
        self.handles.borrow().get(k).copied().unwrap_or(0)
    }

    /// Dispatches the events at the head of the golden; a host call that switches container runs the
    /// leave and enter events of the classes inside the call, ahead of its own trace line.
    fn run_events(&self) {
        while self.peek().starts_with("ev ") {
            let line = self.peek().to_string();
            let e = Eng {
                h: self,
                tr: Some(self.tr),
            };
            let t = tokens(&line);
            let f = |i: usize| f32::from_bits(fbits(&t[i]) as u32);
            let s = |i: usize| -> Vec<u8> { unquote(&t[i]).iter().map(|u| *u as u8).collect() };
            let n = |i: usize| t[i].parse::<i32>().unwrap();
            let (pm_h, cr_h, ls_h, log_h) = (
                self.handle_or("pm"),
                self.handle_or("cr"),
                self.handle_or("ls"),
                self.handle_or("log"),
            );
            match t[1].as_str() {
                "pm_new" => {
                    let r = pm::new(&e, &|x| self.pm_handle.set(x));
                    self.handles.borrow_mut().insert("pm", r);
                }
                "pm_enter" => pm::on_enter(&e, pm_h),
                "pm_leave" => pm::on_leave(&e, pm_h),
                "pm_update" => pm::update(&e, pm_h, f(2)),
                "pm_draw" => pm::on_draw(&e, pm_h),
                "pm_button" => pm::button_pressed(&e, pm_h),
                "pm_focus_lost" => pm::focus_lost(&e),
                "pm_focus_got" => pm::focus_got(&e),
                "cr_new" => {
                    let r = cr::cr_new(&e);
                    self.handles.borrow_mut().insert("cr", r);
                }
                "cr_load_fonts" => cr::cr_load_fonts(&e, cr_h),
                "cr_reset" => cr::cr_reset(&e, cr_h),
                "cr_enter" => cr::cr_on_enter(&e, cr_h),
                "cr_leave" => cr::cr_on_leave(&e, cr_h),
                "cr_update" => cr::cr_update(&e, cr_h, f(2)),
                "cr_draw" => cr::cr_on_draw(&e, cr_h),
                "cr_add_key" => cr::cr_add_key(&e, cr_h, n(2)),
                "cr_setup" => cr::cr_setup(&e, cr_h, &s(2), n(3) != 0, &s(4), &s(5), n(6)),
                "cr_exit_pressed" => cr::cr_exit_pressed(&e, cr_h),
                "cr_focus_lost" => cr::cr_focus_lost(&e),
                "cr_focus_got" => cr::cr_focus_got(&e),
                "ls_new" => {
                    let r = ls::new(&e);
                    self.handles.borrow_mut().insert("ls", r);
                }
                "ls_load_fonts" => ls::load_fonts(&e, ls_h),
                "ls_reset" => ls::reset(&e, ls_h),
                "ls_update" => ls::update(&e, ls_h, f(2)),
                "ls_enter" => ls::on_enter(&e, ls_h),
                "ls_leave" => ls::on_leave(&e, ls_h),
                "ls_draw" => ls::on_draw(&e, ls_h),
                "ls_setup" => ls::setup_load_text(&e, ls_h, &s(2), &s(3), n(4), &s(5)),
                "ls_load_done" => ls::game_screen_load_done(&e, ls_h, &s(2), f(3)),
                "ls_draw_game_screen" => ls::draw_game_screen_public(&e, ls_h),
                "ls_draw_menu_screen" => ls::draw_menu_screen(&e, ls_h),
                "ls_exit_pressed" => ls::exit_pressed(&e, ls_h),
                "log_load_user_config" => {
                    let lh = log::new();
                    self.handles.borrow_mut().insert("log", lh);
                    log::load_user_config(&e, lh);
                }
                "log_save_user_config" => log::save_user_config(&e, log_h),
                "log_create" => log::create_and_reset(&e, log_h),
                "log_add" => log::add_log(&e, log_h, n(2), &s(3)),
                other => panic!("unknown event {other}"),
            }
        }
    }
}

fn replay(z: &[u8]) {
    let _g = SERIAL.lock().unwrap_or_else(|e| e.into_inner());
    let lines = Arc::new(inflate(z));
    let pos = Arc::new(AtomicUsize::new(0));
    let tracer = Tracer::new(Box::new(Sink {
        lines: lines.clone(),
        pos: pos.clone(),
        partial: String::new(),
    }));
    let rp = Replay {
        lines: &lines,
        pos: pos.clone(),
        pm_handle: Cell::new(0),
        tr: &tracer,
        handles: RefCell::new(HashMap::new()),
    };
    rp.run_events();
    assert_eq!(
        pos.load(Ordering::SeqCst),
        lines.len(),
        "stray golden line {}",
        rp.peek()
    );
}

#[test]
fn premenu_boot_replays_the_cpp_dump() {
    replay(include_bytes!("../../tests/golden/A10b/boot-amnesia.txt.z"));
}

#[test]
fn premenu_boot_of_the_pig_replays_the_cpp_dump() {
    replay(include_bytes!("../../tests/golden/A10b/boot-amfp.txt.z"));
}

#[test]
fn map_change_and_credits_replay_the_cpp_dump() {
    replay(include_bytes!("../../tests/golden/A10b/play-amnesia.txt.z"));
}

#[test]
fn header_lists_every_host_op() {
    let hdr = include_str!("../../../amnesia/src/game/RustLuxMenuA10b.h");
    for (id, name, _) in host::OPS {
        let upper = name.to_uppercase();
        let want = format!("kOhplA10b_{upper} = {id},");
        assert!(
            hdr.lines().any(|l| l.trim() == want),
            "{want} missing from RustLuxMenuA10b.h (op {name})"
        );
    }
}

struct Canned {
    next: Cell<usize>,
}

impl Host for Canned {
    fn call(&self, op: i32, _args: &[Arg]) -> Res {
        let mut r = Res::default();
        match host::rk(op).1 {
            Rk::P => {
                self.next.set(self.next.get() + 1);
                r.u[0] = 0x2000 + self.next.get() as u64 * 16;
            }
            Rk::Str => r.s.push(vec![u32::from(b'u')]),
            _ => {}
        }
        r
    }
}

fn lines_of(buf: &Arc<Mutex<Vec<u8>>>) -> Vec<String> {
    String::from_utf8(buf.lock().unwrap().clone())
        .unwrap()
        .lines()
        .map(str::to_string)
        .collect()
}

struct Mem(Arc<Mutex<Vec<u8>>>);

impl Write for Mem {
    fn write(&mut self, b: &[u8]) -> std::io::Result<usize> {
        self.0.lock().unwrap().extend_from_slice(b);
        Ok(b.len())
    }

    fn flush(&mut self) -> std::io::Result<()> {
        Ok(())
    }
}

/// The demo end is not built into the shipped games (`LUX_DEMO_VERSION`), so no real dump exists.
#[test]
fn demo_end_hover_fade_exit_and_browser() {
    let buf = Arc::new(Mutex::new(Vec::new()));
    let tr = Tracer::new(Box::new(Mem(buf.clone())));
    let host = Canned { next: Cell::new(0) };
    let e = Eng {
        h: &host,
        tr: Some(&tr),
    };
    let h = cr::de_new(&e);
    let at = lines_of(&buf)
        .iter()
        .filter(|l| l.starts_with("widget_callback"))
        .count();
    assert_eq!(at, 10);
    cr::de_callback(&e, h, cr::TAG_ENTER, 0x2000 + 16 * 5, 0.0);
    cr::de_callback(&e, h, cr::TAG_UPDATE, 0x2000 + 16 * 5, 0.25);
    let hover = lines_of(&buf).pop().unwrap();
    assert!(
        hover.ends_with("f3f400000 f00000000 f00000000 f3f400000"),
        "{hover}"
    );
    cr::de_exit(&e, h, true);
    for _ in 0..200 {
        cr::de_update(&e, h, 0.1);
    }
    let lines = lines_of(&buf);
    let exit = lines.iter().position(|l| l == "engine_exit").unwrap();
    assert_eq!(lines[exit + 1], "open_browser \"u\"");
    assert!(
        lines.iter().filter(|l| *l == "engine_exit").count() > 1,
        "the exit repeats every update"
    );
}
