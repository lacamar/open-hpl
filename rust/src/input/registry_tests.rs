//! Replays the C++ `cInput`/`cAction` golden trace (tests/golden/H17) through the Rust registry.

use super::*;
use crate::input::{Event, key_name, mouse_button_name};

const SCRIPT: &str = include_str!("../../tests/golden/H17/script.txt");
const TRACE: &str = include_str!("../../tests/golden/H17/trace.txt");

struct Sim {
    kb: Box<Keyboard>,
    mouse: Box<Mouse>,
    #[allow(clippy::vec_box)] // registry holds pointers to the pads
    pads: Vec<Box<Gamepad>>,
    reg: Registry,
    table: Vec<Option<Handle<Action>>>,
    events: Vec<Event>,
    now: u64,
}

impl Sim {
    fn new(npads: usize) -> Self {
        let mut s = Self {
            kb: Box::new(Keyboard::new()),
            mouse: Box::new(Mouse::new()),
            pads: Vec::new(),
            reg: Registry::new(),
            table: Vec::new(),
            events: Vec::new(),
            now: 0,
        };
        // SAFETY: the boxed devices live as long as `s` and are never moved out.
        unsafe { s.reg.set_devices(&*s.kb, &*s.mouse) };
        s.set_pads(npads);
        s
    }

    fn set_pads(&mut self, n: usize) {
        self.pads = (0..n as i32)
            .map(|i| Box::new(Gamepad::new(i, 100 + i, 21, 6, 27)))
            .collect();
        let ptrs: Vec<*const Gamepad> = self.pads.iter().map(|p| &**p as *const Gamepad).collect();
        // SAFETY: the pads are boxed and replaced only through this function.
        unsafe { self.reg.set_pads(&ptrs) };
    }

    fn act(&self, k: usize) -> Option<Handle<Action>> {
        self.table.get(k).copied().flatten()
    }

    fn index_of(&self, h: Option<Handle<Action>>) -> i32 {
        match h {
            None => -1,
            Some(h) => self
                .table
                .iter()
                .position(|t| *t == Some(h))
                .map_or(-2, |i| i as i32),
        }
    }

    fn states(&mut self, h: Handle<Action>) -> String {
        let a = self.reg.is_down(h);
        let b = self.reg.was_triggered(h);
        let c = self.reg.became_triggered(h);
        let d = self.reg.double_triggered(h, 0.25);
        format!(
            "{}{}{}{}",
            u8::from(a),
            u8::from(b),
            u8::from(c),
            u8::from(d)
        )
    }

    fn lookup(&mut self, h: Option<Handle<Action>>) -> String {
        match h {
            Some(h) => self.states(h),
            None => "0000".into(),
        }
    }
}

type Tokens<'a> = std::str::SplitWhitespace<'a>;

fn num<T: std::str::FromStr + Default>(it: &mut Tokens<'_>) -> T {
    it.next().and_then(|t| t.parse().ok()).unwrap_or_default()
}

fn word<'a>(it: &mut Tokens<'a>) -> &'a str {
    it.next().unwrap_or("")
}

fn spec(kind: u32, it: &mut Tokens<'_>) -> SubSpec {
    let mut s = SubSpec {
        kind,
        pad: 0,
        id: 0,
        range: 0,
        min: 0.0,
        max: 0.0,
    };
    match kind {
        SUB_KEY | SUB_MOUSE => s.id = num(it),
        SUB_PAD_BUTTON => {
            s.pad = num(it);
            s.id = num(it);
        }
        SUB_PAD_AXIS => {
            s.pad = num(it);
            s.id = num(it);
            s.range = num(it);
            s.min = num(it);
            s.max = num(it);
        }
        _ => {
            s.pad = num(it);
            s.id = num(it);
            s.range = num(it);
        }
    }
    s
}

fn event(it: &mut Tokens<'_>) -> Event {
    Event {
        kind: num(it),
        sym: num(it),
        key_mod: num(it),
        state: num(it),
        button: num(it),
        axis_value: num(it),
        x: num(it),
        y: num(it),
        xrel: num(it),
        yrel: num(it),
        wheel_y: num(it),
        which: num(it),
        text: [0; 32],
    }
}

fn replay() -> Vec<String> {
    let mut out = Vec::new();
    let mut sim = Sim::new(0);
    let mut n = 0;
    for line in SCRIPT.lines() {
        if line.is_empty() || line.starts_with('#') {
            continue;
        }
        n += 1;
        let mut it = line.split_whitespace();
        match word(&mut it) {
            "new" => sim = Sim::new(num(&mut it)),
            "refresh" => {
                let p: usize = num(&mut it);
                sim.set_pads(p);
                out.push(format!("{n} pads {p}"));
            }
            "create" => {
                let name = word(&mut it);
                let id: i32 = num(&mut it);
                let h = sim.reg.create(name.as_bytes(), id, core::ptr::null_mut());
                sim.table.push(h);
            }
            "bind" => {
                let k: usize = num(&mut it);
                let kind = match word(&mut it) {
                    "key" => SUB_KEY,
                    "mouse" => SUB_MOUSE,
                    "padbtn" => SUB_PAD_BUTTON,
                    "padaxis" => SUB_PAD_AXIS,
                    _ => SUB_PAD_HAT,
                };
                let s = spec(kind, &mut it);
                if let Some(h) = sim.act(k) {
                    sim.reg.add_sub(h, s);
                }
            }
            "clear" => {
                if let Some(h) = sim.act(num(&mut it)) {
                    sim.reg.clear_subs(h);
                }
            }
            "destroy" => {
                let k: usize = num(&mut it);
                if let Some(h) = sim.act(k) {
                    sim.reg.destroy(h);
                    sim.table[k] = None;
                }
            }
            "getn" => {
                let i = sim.index_of(sim.reg.find_name(word(&mut it).as_bytes()));
                out.push(format!("{n} getn {i}"));
            }
            "getid" => {
                let i = sim.index_of(sim.reg.find_id(num(&mut it)));
                out.push(format!("{n} getid {i}"));
            }
            "ev" => {
                let e = event(&mut it);
                sim.events.push(e);
            }
            "update" => {
                let dt: f32 = num(&mut it);
                sim.now += 16;
                sim.mouse.take_first_time();
                sim.mouse.update(&sim.events, false, Default::default());
                sim.kb.update(&sim.events, 0);
                for p in &mut sim.pads {
                    p.update(&sim.events, sim.now);
                }
                sim.reg.update_all(dt);
                sim.events.clear();
            }
            "q" => {
                let k: usize = num(&mut it);
                if let Some(h) = sim.act(k) {
                    out.push(format!("{n} q {k} {}", sim.states(h)));
                }
            }
            "qa" => {
                for k in 0..sim.table.len() {
                    if let Some(h) = sim.act(k) {
                        out.push(format!("{n} q {k} {}", sim.states(h)));
                    }
                }
            }
            "qn" => {
                let h = sim.reg.find_name(word(&mut it).as_bytes());
                out.push(format!("{n} qn {}", sim.lookup(h)));
            }
            "qi" => {
                let h = sim.reg.find_id(num(&mut it));
                out.push(format!("{n} qi {}", sim.lookup(h)));
            }
            "reset" => sim.reg.reset_all(),
            "resetone" => {
                if let Some(h) = sim.act(num(&mut it)) {
                    sim.reg.reset_to_current_state(h);
                }
            }
            "any" => out.push(format!(
                "{n} any {}",
                u8::from(sim.reg.first_down().is_some())
            )),
            "sub" => out.push(match sim.reg.first_down() {
                Some((SUB_KEY, id)) => format!("{n} sub Keyboard {}", key_name(id)),
                Some((_, id)) => format!("{n} sub MouseButton {}", mouse_button_name(id)),
                None => format!("{n} sub none"),
            }),
            _ => {}
        }
    }
    out
}

#[test]
fn matches_cpp_trace() {
    let want: Vec<&str> = TRACE.lines().filter(|l| !l.contains(" ~ ")).collect();
    let got = replay();
    assert_eq!(got.len(), want.len());
    for (g, w) in got.iter().zip(&want) {
        assert_eq!(g, w);
    }
}

#[test]
fn trace_has_activity() {
    assert!(TRACE.lines().filter(|l| l.contains(" q ")).count() > 5000);
    assert!(TRACE.contains(" 1010") && TRACE.contains(" 1011") && TRACE.contains(" 0100"));
}

#[test]
fn stale_handles_are_inert() {
    let mut r = Registry::new();
    let Some(h) = r.create(b"a", -1, core::ptr::null_mut()) else {
        return;
    };
    r.destroy(h);
    assert!(!r.is_down(h));
    assert!(r.find_name(b"a").is_none() && r.find_id(-1).is_none());
    assert!(!r.double_triggered(h, 1.0));
}
