//! A5 golden replay. The reference chain composes the GA4b helpers the way the C++ shells did before
//! the A5 port (verified against C++ in GA4b); `rust/tests/golden/A5/small_props.txt` holds its
//! output. Re-record: `A5_RECORD=1 cargo test a5_small_props_match_golden`.

use super::a5_small::{self as s, A5Button, A5Door, A5Oil, A5Photocell, OilHost, PhotoHost};
use super::ops::{Op, Ops, Pod};
use super::smallprops as sp;
use crate::math::v3;
use crate::types::Vector3f;
use std::cell::RefCell;
use std::fmt::Write;

struct Rng(u64);

impl Rng {
    fn next(&mut self) -> u32 {
        self.0 = self
            .0
            .wrapping_mul(6364136223846793005)
            .wrapping_add(1442695040888963407);
        (self.0 >> 33) as u32
    }

    fn f(&mut self, scale: f32) -> f32 {
        (self.next() % 2001) as f32 / 1000.0 * scale
    }

    fn b(&mut self) -> bool {
        self.next() & 1 == 1
    }
}

fn op_line(o: &Op) -> String {
    format!("op {} {} {} {:08x}", o.code, o.i, o.j, o.f[0].to_bits())
}

fn run<R>(f: impl FnOnce(&mut Ops) -> R) -> (R, Vec<String>) {
    let log = RefCell::new(Vec::new());
    let mut push = |o: &Op| log.borrow_mut().push(op_line(o));
    let r = f(&mut Ops(&mut push));
    (r, log.into_inner())
}

fn button(seed: u64, out: &mut String) {
    let mut r = Rng(seed);
    let mut st = A5Button::new();
    let mut ref_on = st.on;
    for _ in 0..200 {
        let k = r.next() % 4;
        let a = r.b();
        let c = (r.next() % 3) as i32 - 1;
        let (lines, want): (Vec<String>, Vec<String>) = match k {
            0 => {
                let (_, l) = run(|o| s::button_interact(Pod::new(&mut st), o));
                let n = !ref_on;
                (l, reference_set(&mut ref_on, n, true))
            }
            1 => {
                let (_, l) = run(|o| s::button_set_on(Pod::new(&mut st), a, false, o));
                (l, reference_set(&mut ref_on, a, false))
            }
            2 => {
                let (_, l) = run(|o| s::button_conn(Pod::new(&mut st), c, o));
                let w = match sp::button_conn_state(c) {
                    Some(on) => reference_set(&mut ref_on, on, true),
                    None => vec![],
                };
                (l, w)
            }
            _ => {
                st.can_on = a;
                st.can_off = !a;
                (vec![], vec![])
            }
        };
        assert_eq!(lines, want);
        assert_eq!(st.on, ref_on);
        assert_eq!(
            s::button_can_interact(&st),
            sp::button_can_interact(st.on, st.can_on, st.can_off)
        );
        let _ = writeln!(out, "b {k} {} {}", st.on, lines.join(";"));
    }
}

fn reference_set(on: &mut bool, new: bool, fx: bool) -> Vec<String> {
    match sp::button_set(on, new) {
        Some(c) => vec![
            op_line(&Op::new(s::a5op::CONN_STATE).i(c)),
            op_line(
                &Op::new(s::a5op::SET_EFFECTS)
                    .i(i32::from(*on))
                    .j(i32::from(fx)),
            ),
        ],
        None => vec![],
    }
}

struct Photo {
    levels: Vec<f32>,
    asked: usize,
    illum: Option<f32>,
}

impl PhotoHost for Photo {
    fn bounds(&mut self) -> (Vector3f, Vector3f) {
        (v3(1.0, 2.0, 3.0), v3(10.0, 20.0, 30.0))
    }

    fn light_at(&mut self, _: Vector3f) -> f32 {
        self.asked += 1;
        self.levels[(self.asked - 1) % self.levels.len()]
    }

    fn illumination(&mut self) -> Option<f32> {
        self.illum
    }
}

fn photocell(seed: u64, out: &mut String) {
    let mut r = Rng(seed);
    let mut st = A5Photocell {
        on_limit: r.f(0.5),
        off_limit: r.f(0.25),
        ..A5Photocell::default()
    };
    let mut rf = st;
    for _ in 0..200 {
        let dt = r.f(0.02);
        let lv: Vec<f32> = (0..6).map(|_| r.f(0.5)).collect();
        let illum = r.b().then(|| r.f(0.5));
        let mut host = Photo {
            levels: lv.clone(),
            asked: 0,
            illum,
        };
        let (_, lines) = run(|o| s::photocell_update(Pod::new(&mut st), dt, &mut host, o));
        let mut want = Vec::new();
        if sp::photo_tick(&mut rf.check_count, dt) {
            rf.light_level = sp::photo_level(&lv);
            let c = sp::photo_switch(rf.light_level, &mut rf.lit, rf.on_limit, rf.off_limit);
            if c != 0 {
                want.push(op_line(&Op::new(s::a5op::CONN_STATE).i(c)));
            }
        }
        let goal = sp::photo_goal(rf.light_level, rf.on_limit, rf.off_limit);
        if let Some(cur) = illum
            && cur != goal
        {
            want.push(op_line(
                &Op::new(s::a5op::PHOTO_ILLUM).f(sp::photo_illum_step(cur, goal, dt)),
            ));
        }
        assert_eq!(lines, want);
        assert_eq!(st, rf);
        let _ = writeln!(
            out,
            "p {:08x} {} {}",
            st.light_level.to_bits(),
            st.lit,
            lines.join(";")
        );
    }
}

fn door(seed: u64, out: &mut String) {
    let mut r = Rng(seed);
    let mut st = A5Door::default();
    let mut rf = st;
    for _ in 0..200 {
        st.locked = r.b();
        rf.locked = st.locked;
        if r.b() {
            let dt = r.f(0.5);
            s::door_update(&mut st, dt);
            rf.locked_count = sp::door_tick(rf.locked_count, dt);
        }
        let (_, lines) = run(|o| s::door_interact(Pod::new(&mut st), o));
        let act = sp::door_interact(rf.locked, rf.locked_count);
        let mut want = Vec::new();
        if act.locked_feedback {
            want.push(op_line(&Op::new(s::a5op::DOOR_LOCKED_FEEDBACK)));
            rf.locked_count = act.count;
        }
        if act.change_map {
            want.push(op_line(&Op::new(s::a5op::DOOR_CHANGE_MAP)));
        }
        assert_eq!(lines, want);
        assert_eq!(st, rf);
        let _ = writeln!(
            out,
            "d {:08x} {}",
            st.locked_count.to_bits(),
            lines.join(";")
        );
    }
}

struct Oil {
    hard: bool,
    lamp: f32,
    total: f32,
}

impl OilHost for Oil {
    fn hard_mode(&mut self) -> bool {
        self.hard
    }

    fn lamp_oil(&mut self) -> f32 {
        self.lamp
    }

    fn total_oil(&mut self) -> f32 {
        self.total
    }
}

fn oil_reference(st: &mut A5Oil, h: &Oil) -> (bool, Vec<String>, f32) {
    use s::a5op as o;
    if !st.calculated {
        st.amount = if h.hard {
            10.0
        } else {
            sp::oil_amount(h.total)
        };
        st.calculated = true;
    }
    let f = sp::oil_fill(st.amount, h.lamp);
    let l = |c: i32, i: i32, v: f32| op_line(&Op::new(c).i(i).f(v));
    if f.result == sp::OIL_EMPTY {
        return (
            false,
            vec![
                l(o::OIL_SOUND, o::SND_EMPTY, 0.0),
                l(o::OIL_MESSAGE, o::MSG_EMPTY, 0.0),
            ],
            st.amount,
        );
    }
    if f.result == sp::OIL_FULL {
        return (
            false,
            vec![l(o::OIL_MESSAGE, o::MSG_LANTERN_FULL, 0.0)],
            st.amount,
        );
    }
    st.amount = f.barrel;
    let msg = [
        o::MSG_REFILL_FULL,
        o::MSG_REFILL_025,
        o::MSG_REFILL_050,
        o::MSG_REFILL_075,
        o::MSG_REFILL_100,
    ][usize::try_from(f.tier).unwrap_or(4).min(4)];
    (
        true,
        vec![
            l(o::OIL_SET_LAMP, 0, f.lamp),
            l(o::OIL_SOUND, o::SND_FILL, 0.0),
            l(o::OIL_MESSAGE, msg, 0.0),
        ],
        st.amount,
    )
}

fn oil(seed: u64, out: &mut String) {
    let mut r = Rng(seed);
    for _ in 0..60 {
        let mut host = Oil {
            hard: r.next().is_multiple_of(4),
            lamp: r.f(50.0),
            total: r.f(100.0),
        };
        let mut st = A5Oil::default();
        let mut rf = st;
        for _ in 0..5 {
            let (res, lines) = run(|o| s::oil_interact(&mut st, &mut host, o));
            let (want_res, want, amount) = oil_reference(&mut rf, &host);
            assert_eq!((res, &lines), (want_res, &want));
            assert_eq!(st, rf);
            let _ = amount;
            if r.b() {
                host.lamp = r.f(50.0);
            }
            let tier = s::oil_focus_tier(&mut st, &mut host);
            let _ = writeln!(
                out,
                "o {res} {:08x} {tier} {}",
                st.amount.to_bits(),
                lines.join(";")
            );
        }
    }
}

fn record() -> String {
    let mut out = String::new();
    for seed in 1..=8 {
        button(seed, &mut out);
        photocell(seed, &mut out);
        door(seed, &mut out);
        oil(seed, &mut out);
    }
    out.lines()
        .fold(String::new(), |a, l| a + l.trim_end() + "\n")
}

#[test]
fn a5_small_props_match_golden() {
    let got = record();
    let path = concat!(
        env!("CARGO_MANIFEST_DIR"),
        "/tests/golden/A5/small_props.txt"
    );
    if std::env::var_os("A5_RECORD").is_some() {
        std::fs::write(path, &got).unwrap();
        return;
    }
    let want = std::fs::read_to_string(path).unwrap();
    assert!(got == want, "small_props golden differs");
}
