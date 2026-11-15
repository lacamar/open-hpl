//! The input handler replayed against the recorded C++ call stream. The golden
//! (`rust/tests/golden/A12c/input_slice.txt`) is a slice of the scripted dump of the C++ original
//! running in the live game (`OHPL_A12C_DUMP`, Amnesia 02_library): every engine call it made
//! with the answer it got. The replay feeds those answers to the Rust handler and requires the
//! same calls in the same order. The settings are the ones the a12b dump saves before the a12c
//! pass loads them (the gate runs them in that order).

use super::input::{
    STATE_CREDITS, STATE_DEBUG, STATE_DEMO_END, STATE_GAME, STATE_INVENTORY, STATE_JOURNAL,
    STATE_LOAD_SCREEN, STATE_MAIN_MENU, STATE_PRE_MENU,
};
use super::input_h::{InEnv, InputHandler};
use std::cell::{Cell, RefCell};

const GOLDEN: &str = include_str!("../../tests/golden/A12c/input_slice.txt");

struct Replay {
    lines: Vec<&'static str>,
    pos: Cell<usize>,
    live: Cell<bool>,
    err: RefCell<Option<String>>,
}

impl Replay {
    fn next(&self, what: &str) -> Vec<&'static str> {
        let i = self.pos.get();
        let line = self.lines.get(i).copied().unwrap_or("");
        self.pos.set(i + 1);
        let f: Vec<&str> = line.split(' ').collect();
        if f.first() != Some(&"c") || f.get(1).copied() != what.split(' ').next() {
            self.fail(format!("line {i}: expected {what}, golden has '{line}'"));
        }
        f
    }

    fn fail(&self, m: String) {
        let mut e = self.err.borrow_mut();
        if e.is_none() {
            *e = Some(m);
        }
    }

    fn num(&self, f: &[&str], i: usize) -> f64 {
        f.get(i).and_then(|s| s.parse().ok()).unwrap_or(f64::NAN)
    }

    fn check(&self, f: &[&str], i: usize, want: f64) {
        let got = self.num(f, i);
        if got != want && !(got.is_nan() && want.is_nan()) {
            self.fail(format!(
                "line {}: arg {i}: golden {got}, rust {want} ({f:?})",
                self.pos.get() - 1
            ));
        }
    }
}

impl InEnv for Replay {
    fn trig(&self, kind: i32, action: i32) -> bool {
        let f = self.next("t");
        self.check(&f, 2, f64::from(kind));
        self.check(&f, 3, f64::from(action));
        self.num(&f, 4) != 0.0
    }
    fn val(&self, what: i32, idx: i32) -> f64 {
        let f = self.next("v");
        self.check(&f, 2, f64::from(what));
        self.check(&f, 3, f64::from(idx));
        self.num(&f, 4)
    }
    fn q(&self, q: i32) -> bool {
        if !self.live.get() {
            return false;
        }
        let f = self.next("q");
        self.check(&f, 2, f64::from(q));
        self.num(&f, 3) != 0.0
    }
    fn fx(&self, op: i32, a: i32, b: i32, f: f32) {
        let l = self.next("fx");
        self.check(&l, 2, f64::from(op));
        self.check(&l, 3, f64::from(a));
        self.check(&l, 4, f64::from(b));
        if (self.num(&l, 5) as f32) != f {
            self.fail(format!(
                "line {}: float arg golden {}, rust {f}",
                self.pos.get() - 1,
                self.num(&l, 5)
            ));
        }
    }
    fn shot_dir(&self) -> Vec<u32> {
        self.next("shotdir");
        "/shots".chars().map(u32::from).collect()
    }
    fn shot_ext(&self) -> Vec<u8> {
        b"jpg".to_vec()
    }
    fn file_exists(&self, _path: &[u32]) -> bool {
        let f = self.next("exists");
        self.num(&f, 2) != 0.0
    }
    fn shot_save(&self, path: &[u32]) {
        let f = self.next("shotsave");
        self.check(&f, 2, path.len() as f64);
    }
    fn game_cfg_int(&self, _: &[u8], _: &[u8], _: i32) -> i32 {
        7
    }
    fn game_cfg_float(&self, _: &[u8], _: &[u8], _: f32) -> f32 {
        0.7
    }
}

fn replay(golden: &'static str) -> (Replay, usize) {
    let r = Replay {
        lines: golden.lines().collect(),
        pos: Cell::new(0),
        live: Cell::new(false),
        err: RefCell::new(None),
    };
    let h = InputHandler::new(&r);
    h.apply(&super::input::Settings {
        invert_mouse: true,
        smooth_mouse: true,
        mouse_sensitivity: 2.5,
        gamepad_walk_sensitivity: 1.0,
        gamepad_look_sensitivity: 1.5,
        gamepad_look_invert: false,
    });
    r.live.set(true);
    let (mut steps, mut begins) = (0, 0);
    while r.pos.get() < r.lines.len() {
        let line = r.lines.get(r.pos.get()).copied().unwrap_or("");
        r.pos.set(r.pos.get() + 1);
        let f: Vec<&str> = line.split(' ').collect();
        match (f.first().copied(), f.get(1).copied()) {
            (Some("s"), Some("begin")) => {
                begins += 1;
                h.change_state(&r, f.get(2).and_then(|s| s.parse().ok()).unwrap_or(0));
            }
            (Some("s"), Some("step")) => {
                steps += 1;
                h.update(&r);
            }
            (Some("s"), Some("state")) => {
                let want: i32 = f.get(2).and_then(|s| s.parse().ok()).unwrap_or(-1);
                if h.state() != want {
                    r.fail(format!(
                        "line {}: state {} want {want}",
                        r.pos.get() - 1,
                        h.state()
                    ));
                }
            }
            (Some("c"), Some("fx")) if f.get(2) == Some(&"0") => {
                r.pos.set(r.pos.get() - 1);
                h.post_render(&r);
            }
            (Some("c"), _) => r.fail(format!(
                "line {}: unconsumed call '{line}'",
                r.pos.get() - 1
            )),
            _ => {}
        }
        if r.err.borrow().is_some() {
            break;
        }
    }
    assert_eq!(*r.err.borrow(), None);
    (r, steps * 100 + begins)
}

#[test]
fn replays_the_recorded_update_stream() {
    let (_, n) = replay(GOLDEN);
    assert!(n / 100 > 300, "golden holds {} steps", n / 100);
    assert_eq!(n % 100, 9);
}

#[test]
fn state_numbers_match_the_enum() {
    let all = [
        STATE_GAME,
        STATE_MAIN_MENU,
        STATE_INVENTORY,
        STATE_JOURNAL,
        STATE_DEBUG,
        STATE_PRE_MENU,
        STATE_CREDITS,
        STATE_DEMO_END,
        STATE_LOAD_SCREEN,
    ];
    assert_eq!(all, [1, 2, 3, 4, 5, 6, 7, 8, 9]);
}
