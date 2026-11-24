//! S4b dump hook (`OHPL_S4B_DUMP`, script `OHPL_S4B_SCRIPT`): one line per agent update, plus
//! scripted wake, player, state, message, save and load commands.

use super::api;
use super::core::{ent_active, send_message};
use super::io::{Io, op};
use super::natives_c::change_state;
use super::pod;
use crate::math::{Vector3f, c_srand, v3};
use crate::somascript::cfmt::fmt_g9;
use std::cell::RefCell;
use std::collections::BTreeMap;
use std::fs::{File, OpenOptions};
use std::io::Write;

#[derive(Default)]
struct State {
    file: Option<File>,
    buf: Vec<u8>,
    init: bool,
    tick: i32,
    ended: bool,
    seen: BTreeMap<usize, Vec<u8>>,
    prev: Vec<(Vec<u8>, usize)>,
    lines: Option<Vec<(i32, String)>>,
}

thread_local! {
    static ST: RefCell<State> = RefCell::new(State::default());
}

impl State {
    fn open(&mut self) -> bool {
        if !self.init {
            self.init = true;
            self.file = std::env::var_os("OHPL_S4B_DUMP")
                .filter(|p| !p.is_empty())
                .and_then(|p| OpenOptions::new().append(true).create(true).open(p).ok());
        }
        self.file.is_some()
    }

    fn live(&mut self) -> bool {
        self.open() && !self.ended
    }

    fn flush(&mut self) {
        if let Some(f) = self.file.as_mut() {
            let _ = f.write_all(&self.buf);
            let _ = f.flush();
        }
        self.buf.clear();
    }
}

fn g(v: f32) -> Vec<u8> {
    fmt_g9(v)
}

fn g3(v: Vector3f) -> Vec<u8> {
    [g(v.x), g(v.y), g(v.z)].join(&b',')
}

fn log(ent: usize, kind: &str, rest: &[u8]) {
    ST.with(|s| {
        let mut s = s.borrow_mut();
        if s.live() {
            s.buf.extend_from_slice(kind.as_bytes());
            s.buf.push(b' ');
            s.buf.extend_from_slice(&pod::name(ent));
            s.buf.push(b' ');
            s.buf.extend_from_slice(rest);
            s.buf.push(b'\n');
        }
    });
}

pub fn log_call(ent: usize, decl: &[u8]) {
    log(ent, "call", decl);
}

pub fn log_ev(ent: usize, kind: u32, a: i32, b: i32) {
    log(ent, "ev", format!("{kind} {a} {b}").as_bytes());
}

pub fn log_msg(ent: usize, id: i32, v: Vector3f, x: i32) {
    let mut r = format!("{id} ").into_bytes();
    r.extend_from_slice(&g3(v));
    r.extend_from_slice(format!(" {x}").as_bytes());
    log(ent, "msg", &r);
}

pub(super) fn atoi(s: &str) -> i32 {
    let t = s.trim_start();
    let n = t
        .bytes()
        .enumerate()
        .take_while(|&(i, c)| c.is_ascii_digit() || (i == 0 && (c == b'-' || c == b'+')))
        .count();
    t.get(..n).and_then(|d| d.parse().ok()).unwrap_or(0)
}

fn script() -> Vec<(i32, String)> {
    let Some(p) = std::env::var_os("OHPL_S4B_SCRIPT") else {
        return Vec::new();
    };
    std::fs::read_to_string(p)
        .unwrap_or_default()
        .lines()
        .filter_map(|l| l.split_once(' ').map(|(_, c)| (atoi(l), c.to_string())))
        .collect()
}

fn find_ent(name: &str) -> usize {
    Io::new().s(0, name.as_bytes()).go(op::E_MAP_FIND).ptr(0)
}

fn emit(b: &[u8]) {
    ST.with(|s| {
        let mut s = s.borrow_mut();
        s.buf.extend_from_slice(b);
        if s.ended {
            s.flush();
        }
    });
}

fn cmd(c: &str) {
    let arg = |p: &str| {
        c.strip_prefix(p)
            .and_then(|r| r.split_whitespace().next().map(str::to_owned))
    };
    if c == "END" {
        ST.with(|s| {
            let mut s = s.borrow_mut();
            s.buf.extend_from_slice(b"END\n");
            s.flush();
            s.ended = true;
        });
    } else if let Some(n) = arg("wake") {
        let e = find_ent(&n);
        if e != 0 {
            Io::new().p(0, e).i(0, 1).go(op::E_SET_ACTIVE);
        }
    } else if let Some(r) = c.strip_prefix("player") {
        let v: Vec<f32> = r
            .split_whitespace()
            .take(3)
            .filter_map(|x| x.parse().ok())
            .collect();
        if let [x, y, z] = v[..] {
            Io::new().v(0, v3(x, y, z)).go(op::D_PLAYER_FEET);
        }
    } else if let Some(r) = c.strip_prefix("state") {
        let mut it = r.split_whitespace();
        if let (Some(n), Some(Ok(st))) = (it.next(), it.next().map(str::parse::<i32>)) {
            let e = find_ent(n);
            if e != 0 {
                change_state(e, st);
            }
        }
    } else if let Some(n) = arg("save") {
        let ok = Io::new().i(0, 0).s(0, n.as_bytes()).go(op::D_SAVE).int(0);
        emit(format!("SAVED {ok}\n").as_bytes());
    } else if let Some(n) = arg("load") {
        let ok = Io::new().i(0, 1).s(0, n.as_bytes()).go(op::D_SAVE).int(0);
        emit(format!("LOAD {ok}\n").as_bytes());
        ST.with(|s| {
            let mut s = s.borrow_mut();
            s.prev.clear();
            s.seen.clear();
        });
    } else if c == "dumpsave" {
        let prev = ST.with(|s| s.borrow().prev.clone());
        for (name, p) in prev {
            let path = api::save_path(p);
            let Some(pos) = api::get_pos(p) else { continue };
            let (yaw, sen, det) = api::save_extra(p);
            let mut l = b"SAVEAGENT ".to_vec();
            l.extend_from_slice(&name);
            l.extend_from_slice(format!(" st={} pos=", api::get_state(p)).as_bytes());
            l.extend_from_slice(&g3(pos));
            l.extend_from_slice(b" yaw=");
            l.extend_from_slice(&g(yaw));
            l.extend_from_slice(
                format!(" s={} d={} path=", i32::from(sen), i32::from(det)).as_bytes(),
            );
            for b in path {
                l.extend_from_slice(format!("{b:02x}").as_bytes());
            }
            l.push(b'\n');
            emit(&l);
        }
    } else if let Some(r) = c.strip_prefix("msg") {
        let mut it = r.split_whitespace();
        if let (Some(n), Some(Ok(m))) = (it.next(), it.next().map(str::parse::<i32>)) {
            let e = find_ent(n);
            if e != 0 {
                send_message(e, m, v3(0.0, 0.0, 0.0), 0);
            }
        }
    }
}

pub fn begin(ent: usize) {
    let todo = ST.with(|s| {
        let mut s = s.borrow_mut();
        if !s.live() {
            return None;
        }
        let name = pod::name(ent);
        if s.seen.contains_key(&ent) {
            let mut prev: Vec<(Vec<u8>, usize)> =
                s.seen.iter().map(|(&p, n)| (n.clone(), p)).collect();
            prev.sort_by(|a, b| a.0.cmp(&b.0));
            s.prev = prev;
            s.seen.clear();
        }
        s.seen.insert(ent, name.clone());
        let mut todo = Vec::new();
        if s.seen.len() == 1 {
            s.tick += 1;
            if s.tick == 1 {
                let m = Io::new().go(op::D_MAP_NAME);
                s.buf.extend_from_slice(b"M ");
                s.buf.extend_from_slice(m.out_s(0));
                s.buf.push(b'\n');
            }
            let tick = s.tick;
            s.buf.extend_from_slice(format!("T {tick}\n").as_bytes());
            todo = s
                .lines
                .get_or_insert_with(script)
                .iter()
                .filter(|l| l.0 == tick)
                .map(|l| l.1.clone())
                .collect();
        }
        Some((name, todo, s.tick))
    });
    let Some((name, todo, tick)) = todo else {
        return;
    };
    for c in &todo {
        cmd(c);
    }
    let mut seed = (tick as u32).wrapping_mul(2_654_435_761);
    for b in name {
        seed = seed.wrapping_mul(31).wrapping_add(u32::from(b));
    }
    c_srand(seed);
}

pub fn end(ent: usize) {
    ST.with(|s| {
        let mut s = s.borrow_mut();
        let body = api::body_of(ent);
        if !s.live() || body == 0 {
            return;
        }
        let p = Io::new().p(1, body).i(0, 0).go(op::B_GET3).out_v(0);
        let v = Io::new().p(1, body).f(0, 1.0 / 60.0).go(op::B_VEL).out_v(0);
        let yaw = Io::new().p(1, body).i(0, 0).go(op::B_GETF).fl(0);
        let an = Io::new().p(0, ent).go(op::D_ANIM);
        let (an_name, an_t) = if an.int(0) != 0 {
            (an.out_s(0).to_vec(), an.fl(0))
        } else {
            (b"-".to_vec(), -1.0)
        };
        s.buf.extend_from_slice(&pod::name(ent));
        s.buf
            .extend_from_slice(format!(" a={} pos=", i32::from(ent_active(ent))).as_bytes());
        s.buf.extend_from_slice(&g3(p));
        s.buf.extend_from_slice(b" vel=");
        s.buf.extend_from_slice(&g3(v));
        s.buf.extend_from_slice(b" yaw=");
        s.buf.extend_from_slice(&g(yaw));
        s.buf.extend_from_slice(b" anim=");
        s.buf.extend_from_slice(&an_name);
        s.buf.push(b'@');
        s.buf.extend_from_slice(&g(an_t));
        s.buf.extend_from_slice(b" | ");
        s.buf.extend_from_slice(&api::debug(ent));
        s.buf.push(b'\n');
        s.flush();
    });
}
