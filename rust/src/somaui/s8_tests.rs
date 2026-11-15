//! Replays the real-engine dumps of the C++ splash and gamma screen (`tests/golden/S8/<game>.txt`,
//! from `scripts/s8-scenario.sh` on the pre-port code): the host answers what the engine answered
//! and the classes must log the very same calls.

use super::s8::{self, E, op};
use super::s8_trace::{Tracer, parse_line};
use crate::luxmenu2::host::{Arg, Host, Res};
use std::cell::{Cell, RefCell};
use std::collections::{HashMap, VecDeque};
use std::io::Write;
use std::sync::{Arc, Mutex};

struct Buf(Arc<Mutex<Vec<u8>>>);

impl Write for Buf {
    fn write(&mut self, b: &[u8]) -> std::io::Result<usize> {
        if let Ok(mut v) = self.0.lock() {
            v.extend_from_slice(b);
        }
        Ok(b.len())
    }

    fn flush(&mut self) -> std::io::Result<()> {
        Ok(())
    }
}

struct Fake<'a> {
    tr: &'a Tracer,
    answers: RefCell<HashMap<i32, VecDeque<Res>>>,
    ticks: usize,
    h: Cell<u64>,
    splash_done: Cell<bool>,
    gamma_done: Cell<bool>,
}

impl Host for Fake<'_> {
    fn call(&self, o: i32, _args: &[Arg]) -> Res {
        match o {
            op::LOAD_MENU => {
                for _ in 0..self.ticks {
                    s8::splash_load_draw(&E::traced(self, self.tr), self.h.get(), true);
                }
                Res::default()
            }
            op::KEY_POLL => Res {
                u: [u64::from(u32::MAX), 0, 0, 0, 0, 0, 0, 0],
                s: Vec::new(),
            },
            op::BASE_SPLASH_DONE => {
                self.splash_done.set(true);
                Res::default()
            }
            op::BASE_GAMMA_DONE => {
                self.gamma_done.set(true);
                Res::default()
            }
            _ => self
                .answers
                .borrow_mut()
                .get_mut(&o)
                .and_then(VecDeque::pop_front)
                .unwrap_or_default(),
        }
    }
}

fn replay(golden: &str) {
    let mut answers: HashMap<i32, VecDeque<Res>> = HashMap::new();
    for l in golden.lines() {
        if let Some((o, r)) = parse_line(l) {
            answers.entry(o).or_default().push_back(r);
        }
    }
    let out = Arc::new(Mutex::new(Vec::new()));
    let tr = Tracer::new(Box::new(Buf(out.clone())), Some(30));
    let fake = Fake {
        tr: &tr,
        answers: RefCell::new(answers),
        ticks: golden.lines().filter(|l| *l == "load_tick").count(),
        h: Cell::new(0),
        splash_done: Cell::new(false),
        gamma_done: Cell::new(false),
    };
    let e = E::traced(&fake, &tr);
    let sp = s8::splash_new(&e, &|h| fake.h.set(h));
    for _ in 0..100_000 {
        if fake.splash_done.get() {
            break;
        }
        s8::splash_update(&e, sp, 0.0);
        s8::splash_draw(&e, sp);
    }
    let g = s8::gamma_new(&e, &|_| {});
    for _ in 0..100_000 {
        if fake.gamma_done.get() {
            break;
        }
        s8::gamma_update(&e, g);
        s8::gamma_draw(&e, g);
    }
    s8::gamma_delete(g);
    let got = out
        .lock()
        .map(|v| String::from_utf8_lossy(&v).into_owned())
        .unwrap_or_default();
    for (i, (a, b)) in got.lines().zip(golden.lines()).enumerate() {
        assert_eq!(a, b, "line {}", i + 1);
    }
    assert_eq!(got.lines().count(), golden.lines().count());
}

#[test]
fn golden_soma() {
    replay(include_str!("../../tests/golden/S8/soma.txt"));
}

#[test]
fn golden_rebirth() {
    replay(include_str!("../../tests/golden/S8/rebirth.txt"));
}

#[test]
fn golden_bunker() {
    replay(include_str!("../../tests/golden/S8/bunker.txt"));
}
