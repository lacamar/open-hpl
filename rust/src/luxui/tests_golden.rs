//! Replays the A11b driver (`dump.rs`) against the Rust handlers. Engine answers (fonts,
//! wrapping, strings, configs) were recorded from the live engine (`OHPL_A11B_ANSWERS`); the
//! expected effects are the dump of the original C++ handlers on Amnesia 00_rainy_hall
//! (`OHPL_A11B_DUMP`, frame step fixed at 1/60).

use super::hint_handler::{Hint, HintHandler};
use super::host::{Hud, Slot, Ui};
use super::msg_handler::{MessageHandler, Msg};
use crate::obj::Arena;
use std::cell::RefCell;
use std::collections::HashMap;

const ANSWERS: &str = include_str!("../../tests/golden/A11b/answers.txt");
const EXPECTED: &str = include_str!("../../tests/golden/A11b/expected.txt");

fn w(s: &str) -> Vec<u32> {
    s.chars().map(u32::from).collect()
}

fn esc(t: &[u32]) -> String {
    let mut o = String::new();
    for &c in t {
        if (32..127).contains(&c) && c != 92 && c != 124 {
            o.push(char::from_u32(c).unwrap_or('?'));
        } else {
            o.push_str(&format!("\\u{c:04x}"));
        }
    }
    o
}

fn escb(t: &[u8]) -> String {
    esc(&t.iter().map(|&c| u32::from(c)).collect::<Vec<_>>())
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

fn num(f: f32) -> String {
    format!("{f:e}")
}

struct Golden {
    ans: HashMap<(String, String), String>,
    gfx: RefCell<Vec<String>>,
    log: RefCell<Vec<String>>,
}

impl Golden {
    fn new() -> Self {
        let mut ans = HashMap::new();
        for l in ANSWERS.lines() {
            let p: Vec<&str> = l.split('\t').collect();
            ans.insert((p[0].to_string(), p[1].to_string()), p[2].to_string());
        }
        Self {
            ans,
            gfx: RefCell::new(Vec::new()),
            log: RefCell::new(Vec::new()),
        }
    }

    fn get(&self, op: &str, k: &str) -> String {
        self.ans
            .get(&(op.to_string(), k.to_string()))
            .unwrap_or_else(|| panic!("no recorded answer {op} {k}"))
            .clone()
    }

    fn floats(&self, op: &str, k: &str) -> Vec<f32> {
        self.get(op, k)
            .split('|')
            .map(|x| x.parse().unwrap())
            .collect()
    }

    fn make(&self, n: &str) -> usize {
        let mut g = self.gfx.borrow_mut();
        g.push(n.to_string());
        g.len()
    }

    fn push(&self, l: String) {
        self.log.borrow_mut().push(l);
    }
}

fn fl(v: &[f32]) -> String {
    v.iter().map(|x| num(*x)).collect::<Vec<_>>().join(" ")
}

impl Ui for Golden {
    fn menu_str(&self, s: &[u8], k: &[u8], d: &[u8]) -> Vec<u8> {
        let r = self.get("menu_str", &format!("{}|{}|{}", escb(s), escb(k), escb(d)));
        unesc(&r).iter().map(|&c| c as u8).collect()
    }
    fn menu_f32(&self, s: &[u8], k: &[u8], d: f32) -> f32 {
        let key = format!("{}|{}|{}", escb(s), escb(k), fmt9(d));
        self.get("menu_f32", &key).parse().unwrap()
    }
    fn menu_vec2(&self, s: &[u8], k: &[u8], d: f32) -> [f32; 2] {
        let key = format!("{}|{}|{}", escb(s), escb(k), fmt9(d));
        let v = self.floats("menu_vec2", &key);
        [v[0], v[1]]
    }
    fn user_bool(&self, s: &[u8], k: &[u8], d: bool) -> bool {
        self.get(
            "user_bool",
            &format!("{}|{}|{}", escb(s), escb(k), u8::from(d)),
        ) == "1"
    }
    fn set_user_bool(&self, _: &[u8], _: &[u8], _: bool) {}
    fn gfx_rect(&self, _: [f32; 4]) -> usize {
        self.make("rect")
    }
    fn gfx_image(&self, f: &[u8]) -> usize {
        self.make(&escb(f))
    }
    fn gfx_texture(&self, f: &[u8]) -> usize {
        self.make(&escb(f))
    }
    fn gfx_size(&self, g: usize, which: i32) -> [f32; 2] {
        let n = self.gfx.borrow()[g - 1].clone();
        let v = self.floats("gfx_size", &format!("{n}|{which}"));
        [v[0], v[1]]
    }
    fn wrap_rows(&self, _: usize, wd: f32, ln: f32, sz: [f32; 2], t: &[u32]) -> Vec<Vec<u32>> {
        let key = format!(
            "{}|{}|{}|{}|{}",
            fmt9(wd),
            fmt9(ln),
            fmt9(sz[0]),
            fmt9(sz[1]),
            esc(t)
        );
        let r = self.get("wrap_rows", &key);
        let (n, rows) = r.split_once('|').unwrap();
        if n == "0" {
            return Vec::new();
        }
        rows.split("\\u0001").map(unesc).collect()
    }
    fn font_len(&self, _: usize, sz: [f32; 2], t: &[u32]) -> f32 {
        let key = format!("{}|{}|{}", fmt9(sz[0]), fmt9(sz[1]), esc(t));
        self.get("font_len", &key).parse().unwrap()
    }
    fn font_range(&self, _: usize) -> (u32, u32) {
        let r = self.get("font_range", "");
        let (a, b) = r.split_once('|').unwrap();
        (a.parse().unwrap(), b.parse().unwrap())
    }
    fn font_glyph(&self, _: usize, i: u32) -> Option<([f32; 2], f32)> {
        let r = self.get("font_glyph", &i.to_string());
        if r == "none" {
            return None;
        }
        let v = self.floats("font_glyph", &i.to_string());
        Some(([v[0], v[1]], v[2]))
    }
    fn parse_string(&self, t: &[u32]) -> Vec<u32> {
        unesc(&self.get("parse_string", &esc(t)))
    }
    fn string_duration(&self, t: &[u32]) -> f32 {
        self.get("string_duration", &esc(t)).parse().unwrap()
    }
    fn gui_sound(&self, n: &[u8]) {
        self.push(format!("snd {}", String::from_utf8_lossy(n)));
    }
    fn set_update_active(&self, on: bool) {
        self.push(format!("upd {}", u8::from(on)));
    }
    fn input_name(&self, a: &[u8]) -> Vec<u32> {
        unesc(&self.get("input_name", &escb(a)))
    }
    fn hud(&self) -> Hud {
        let v = self.floats("hud", "");
        Hud {
            center: [v[0], v[1]],
            start: [v[2], v[3], v[4]],
        }
    }
    fn ptest(&self) -> bool {
        self.get("ptest", "") == "1"
    }
    fn draw_gfx(&self, _: usize, _: usize, p: [f32; 3], sz: Option<[f32; 2]>, c: [f32; 4], m: i32) {
        let s = sz.unwrap_or([-1.0, -1.0]);
        self.push(format!("G {} {} {} {m}", fl(&p), fl(&s), fl(&c)));
    }
    fn draw_font(
        &self,
        _: usize,
        _: usize,
        p: [f32; 3],
        sz: [f32; 2],
        c: [f32; 4],
        a: i32,
        m: i32,
        t: &[u32],
        sp: bool,
    ) {
        let mut text = if sp { vec![32] } else { Vec::new() };
        text.extend_from_slice(t);
        self.push(format!(
            "F {} {} {} {a} {m} |{}",
            fl(&p),
            fl(&sz),
            fl(&c),
            esc(&text)
        ));
    }
    fn translate(&self, c: &[u8], k: &[u8]) -> Vec<u32> {
        unesc(&self.get("translate", &format!("{}|{}", escb(c), escb(k))))
    }
    fn chinese(&self) -> bool {
        self.get("chinese", "") == "1"
    }
    fn callback(&self, cb: usize, yes: bool) {
        self.push(format!("callback {} {}", u8::from(cb != 0), u8::from(yes)));
        if cb != 0 {
            self.push(format!("cb {}", u8::from(yes)));
        }
    }
}

fn fmt9(f: f32) -> String {
    c_g9(f)
}

/// `%.9g` of a float, as the C++ side prints it.
fn c_g9(f: f32) -> String {
    let d = f64::from(f);
    if d == 0.0 {
        return "0".into();
    }
    let exp = d.abs().log10().floor() as i32;
    let s = if (-5..9).contains(&exp) {
        let dec = (8 - exp).max(0) as usize;
        format!("{d:.dec$}")
    } else {
        let m = format!("{d:.8e}");
        let (mant, e) = m.split_once('e').unwrap();
        let mant = if mant.contains('.') {
            mant.trim_end_matches('0').trim_end_matches('.')
        } else {
            mant
        };
        let e: i32 = e.parse().unwrap();
        return format!("{mant}e{}{:02}", if e < 0 { '-' } else { '+' }, e.abs());
    };
    if s.contains('.') {
        s.trim_end_matches('0').trim_end_matches('.').to_string()
    } else {
        s
    }
}

fn norm(l: &str) -> Vec<String> {
    let (head, text) = l.split_once('|').map_or((l, None), |(a, b)| (a, Some(b)));
    let mut v: Vec<String> = head
        .split_whitespace()
        .map(|t| match t.parse::<f32>() {
            Ok(f) => format!("{:08x}", f.to_bits()),
            Err(_) => t.to_string(),
        })
        .collect();
    if let Some(t) = text {
        v.push(format!("|{t}"));
    }
    v
}

#[test]
fn replay_matches_original_cpp_dump() {
    let ui = Golden::new();
    let hints = RefCell::new(Arena::default());
    let msgs = RefCell::new(Arena::default());
    let mut h = HintHandler::new(&ui);
    h.font = 1;
    let hh = hints.borrow_mut().insert(h).unwrap();
    let mut m = MessageHandler::new(&ui);
    m.font = 1;
    let mh = msgs.borrow_mut().insert(m).unwrap();
    ui.log.borrow_mut().clear();
    let hint = Hint {
        ui: &ui,
        c: Slot {
            arena: &hints,
            h: hh,
        },
    };
    let msg = Msg {
        ui: &ui,
        c: Slot {
            arena: &msgs,
            h: mh,
        },
    };
    hint.c.with(|s| s.s.active = 1);
    msg.c.with(MessageHandler::reset);
    msg.c.with(|s| {
        s.load_user_config(&ui);
    });
    let frames = EXPECTED
        .lines()
        .filter_map(|l| l.strip_prefix("tick "))
        .map(|l| l.split(' ').next().unwrap().parse::<u32>().unwrap())
        .max()
        .unwrap();
    let dt = 1.0f32 / 60.0;
    let b = |x: bool| u8::from(x);
    for f in 1..=frames {
        match f {
            3 => {
                let r = hint.add(b"a11b_a", &w("Hint one"), 2.0);
                ui.push(format!("add a {}", b(r)));
                let r = hint.add(b"a11b_a", &w("Hint one"), 2.0);
                ui.push(format!("add a again {}", b(r)));
            }
            6 => msg.set_message(
                &w("Game message one that is somewhat long and has to wrap over several rows of the screen"),
                2.0,
            ),
            9 => {
                hint.c.with(|s| s.block(b"a11b_b"));
                let r = hint.add(b"a11b_b", &w("Blocked"), 1.0);
                ui.push(format!("add b blocked {}", b(r)));
                hint.c.with(|s| s.unblock(b"a11b_b"));
                let r = hint.c.with(|s| s.remove(b"a11b_a")).unwrap();
                ui.push(format!("remove a {}", b(r)));
                let r = hint.c.with(|s| s.remove(b"zz")).unwrap();
                ui.push(format!("remove zz {}", b(r)));
            }
            12 => msg.c.with(MessageHandler::start_quest_added).unwrap(),
            15 => msg.start_pause(
                &w("Pause message text that is long enough to wrap around the pause box more than once, really"),
                true,
                1,
            ),
            40 => msg.do_action(0, true),
            41 => msg.do_action(0, false),
            45 => msg.start_pause(&w("Second pause"), false, 0),
            50 => msg.do_action(1, true),
            55 => msg.start_pause(&w("Third pause"), true, 1),
            60 => msg.do_action(1, true),
            65 => msg.set_pause_active(true),
            70 => msg.set_pause_active(false),
            85 => {
                let r = hint.add(
                    b"a11b_c",
                    &w("Press $ButInteract and $ButAttack and $ButJump to go on with a long line of text that wraps over rows"),
                    0.5,
                );
                ui.push(format!("add c {}", b(r)));
            }
            80 => hint.c.with(|s| s.set_active(false)).unwrap(),
            82 => hint.c.with(|s| s.set_active(true)).unwrap(),
            250 => {
                let r = hint.add(b"a11b_d", &w("Short $ButInteract hint"), 0.5);
                ui.push(format!("add d {}", b(r)));
            }
            330 => {
                let r = hint.add(b"a11b_e", &w("Duration derived from the string $ButLean"), 0.0);
                ui.push(format!("add e {}", b(r)));
            }
            100 => {
                msg.c.with(MessageHandler::reset).unwrap();
                hint.c.with(HintHandler::reset).unwrap();
            }
            _ => {}
        }
        let st = |i| msg.c.with(|s| ui_flag(s, i)).unwrap();
        ui.push(format!(
            "tick {f} hint={} msg={} pause={} sub={} esub={}",
            b(hint.c.with(|s| s.s.active != 0).unwrap()),
            b(st(0)),
            b(st(1)),
            b(st(2)),
            b(st(3))
        ));
        msg.update(dt);
        msg.draw();
        hint.update(dt);
        hint.draw(0);
    }
    let got: Vec<Vec<String>> = ui.log.borrow().iter().map(|l| norm(l)).collect();
    let want: Vec<Vec<String>> = EXPECTED
        .lines()
        .filter(|l| !l.starts_with("given ") && !l.starts_with("blocked "))
        .map(norm)
        .collect();
    assert_eq!(got.len(), want.len());
    for (i, (g, e)) in got.iter().zip(&want).enumerate() {
        assert_eq!(g, e, "line {i}");
    }
}

fn ui_flag(s: &MessageHandler, i: i32) -> bool {
    match i {
        0 => s.is_message_active(),
        1 => s.s.pause_active != 0,
        2 => s.s.show_subtitles != 0,
        _ => s.s.show_effect_subtitles != 0,
    }
}
