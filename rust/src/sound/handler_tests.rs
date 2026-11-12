//! Replays `tests/golden/H16/sound.txt`, recorded from the C++ `cSoundHandler` (rust/tests/golden/H16/gen),
//! with the Rust handler state, a fake channel and the shell glue below. `g` lines (global tables
//! of the C++ settings handler) and `global` results are replay input.

use super::channel::{Channel, EntryHost};
use super::entry::{EntryState, Step};
use super::handler::{self, Handler, Shells};
use super::play::{PlayParams, distance_priority, setup_channel};
use crate::types::{Matrixf, Vector3f};
use std::cell::RefCell;
use std::ffi::c_void;

const GOLDEN: &str = include_str!("../../tests/golden/H16/sound.txt");

thread_local! {
    static EV: RefCell<String> = const { RefCell::new(String::new()) };
}

fn ev(s: String) {
    EV.with(|e| {
        let mut e = e.borrow_mut();
        e.push_str("  ");
        e.push_str(&s);
        e.push('\n');
    });
}

fn hx(s: &str) -> u32 {
    u32::from_str_radix(s, 16).unwrap_or(0)
}

fn hf(s: &str) -> f32 {
    f32::from_bits(hx(s))
}

fn b(f: f32) -> u32 {
    f.to_bits()
}

fn v3(x: f32, y: f32, z: f32) -> Vector3f {
    crate::math::v3(x, y, z)
}

struct Ch {
    id: i32,
    playing: bool,
    paused: bool,
    stop_used: bool,
    looping: bool,
    relative: bool,
    speed: f32,
    volume: f32,
    priority: i32,
    pos: Vector3f,
    rel: Vector3f,
    min: f32,
    max: f32,
}

impl Ch {
    fn new(id: i32) -> Self {
        Self {
            id,
            playing: false,
            paused: true,
            stop_used: false,
            looping: false,
            relative: false,
            speed: 1.0,
            volume: 1.0,
            priority: 0,
            pos: v3(0.0, 0.0, 0.0),
            rel: v3(0.0, 0.0, 0.0),
            min: 0.0,
            max: 0.0,
        }
    }
}

impl Drop for Ch {
    fn drop(&mut self) {
        ev(format!("c{} delete", self.id));
    }
}

impl Channel for Ch {
    fn play(&mut self) {
        ev(format!("c{} play", self.id));
        self.playing = true;
        self.paused = false;
    }
    fn stop(&mut self) {
        ev(format!("c{} stop", self.id));
        self.playing = false;
        self.stop_used = true;
    }
    fn set_paused(&mut self, paused: bool) {
        ev(format!("c{} paused {}", self.id, u8::from(paused)));
        self.paused = paused;
    }
    fn paused(&self) -> bool {
        self.paused
    }
    fn is_playing(&mut self) -> bool {
        self.playing
    }
    fn stop_used(&self) -> bool {
        self.stop_used
    }
    fn speed(&self) -> f32 {
        self.speed
    }
    fn set_speed(&mut self, speed: f32) {
        ev(format!("c{} speed {:08x}", self.id, b(speed)));
        self.speed = speed;
    }
    fn volume(&self) -> f32 {
        self.volume
    }
    fn set_volume(&mut self, volume: f32) {
        ev(format!("c{} volume {:08x}", self.id, b(volume)));
        self.volume = volume;
    }
    fn looping(&self) -> bool {
        self.looping
    }
    fn set_looping(&mut self, looping: bool) {
        ev(format!("c{} looping {}", self.id, u8::from(looping)));
        self.looping = looping;
    }
    fn set_3d(&mut self, is_3d: bool) {
        ev(format!("c{} 3d {}", self.id, u8::from(is_3d)));
    }
    fn set_priority(&mut self, priority: i32) {
        ev(format!("c{} prio {priority}", self.id));
        self.priority = priority;
    }
    fn set_priority_modifier(&mut self, _: i32) {
        self.set_priority(self.priority);
    }
    fn set_affected_by_env(&mut self, affected: bool) {
        ev(format!("c{} env {}", self.id, u8::from(affected)));
    }
    fn position_is_relative(&self) -> bool {
        self.relative
    }
    fn set_position_is_relative(&mut self, relative: bool) {
        ev(format!("c{} relative {}", self.id, u8::from(relative)));
        self.relative = relative;
    }
    fn position(&self) -> Vector3f {
        self.pos
    }
    fn set_position(&mut self, p: Vector3f) {
        ev(format!(
            "c{} pos {:08x} {:08x} {:08x}",
            self.id,
            b(p.x),
            b(p.y),
            b(p.z)
        ));
        self.pos = p;
    }
    fn rel_position(&self) -> Vector3f {
        self.rel
    }
    fn set_rel_position(&mut self, p: Vector3f) {
        self.rel = p;
    }
    fn min_distance(&self) -> f32 {
        self.min
    }
    fn set_min_distance(&mut self, dist: f32) {
        ev(format!("c{} min {:08x}", self.id, b(dist)));
        self.min = dist;
    }
    fn max_distance(&self) -> f32 {
        self.max
    }
    fn set_max_distance(&mut self, dist: f32) {
        ev(format!("c{} max {:08x}", self.id, b(dist)));
        self.max = dist;
    }
    fn block_volume_mul(&self) -> f32 {
        1.0
    }
    fn elapsed_time(&self) -> f64 {
        0.0
    }
    fn total_time(&self) -> f64 {
        0.0
    }
    fn set_elapsed_time(&mut self, _: f64) {}
}

struct Host {
    h: *const Handler,
    listener: Vector3f,
    matrix: Matrixf,
}

impl EntryHost for Host {
    fn global_speed(&self, t: u32) -> f32 {
        // SAFETY: the handler outlives the update call.
        unsafe { &*self.h }.global_speed(t)
    }
    fn global_volume(&self, t: u32) -> f32 {
        // SAFETY: as above.
        unsafe { &*self.h }.global_volume(t)
    }
    fn silent(&self) -> bool {
        // SAFETY: as above.
        unsafe { &*self.h }.silent()
    }
    fn is_blocked(&mut self, _: Vector3f) -> bool {
        false
    }
    fn listener_position(&self) -> Vector3f {
        self.listener
    }
    fn listener_matrix(&self) -> Matrixf {
        self.matrix
    }
}

struct Shell {
    ch: Ch,
    state: *mut EntryState,
    entry: handler::EntryHandle,
    cb: i32,
    id: i32,
    name: String,
}

struct World {
    h: *mut Handler,
    shells: Vec<Option<Shell>>,
    listener: Vector3f,
    matrix: Matrixf,
    next_ch: i32,
    refs: Vec<(*mut c_void, i32)>,
    gv: [f32; 32],
    gs: [f32; 32],
}

fn sh(p: *mut c_void) -> usize {
    (p as usize).wrapping_sub(1)
}

const ALL: u32 = u32::MAX;

impl World {
    fn new() -> Self {
        Self {
            h: Box::into_raw(Box::default()),
            shells: Vec::new(),
            listener: v3(0.0, 0.0, 0.0),
            matrix: crate::math::IDENTITY,
            next_ch: 0,
            refs: Vec::new(),
            gv: [1.0; 32],
            gs: [1.0; 32],
        }
    }

    fn handler(&mut self) -> &mut Handler {
        // SAFETY: `h` is live until `finish`; callers never hold two of these across a re-entry.
        unsafe { &mut *self.h }
    }

    fn shell(&mut self, p: *mut c_void) -> &mut Shell {
        let idx = sh(p);
        self.shells
            .get_mut(idx)
            .and_then(Option::as_mut)
            .expect("live shell")
    }

    fn find(&mut self, name: &str) -> *mut c_void {
        self.handler().find(name.to_ascii_lowercase().as_bytes())
    }

    #[expect(clippy::too_many_arguments, reason = "mirrors cSoundHandler::Play")]
    fn play(
        &mut self,
        name: &str,
        looping: bool,
        volume: f32,
        pos: Vector3f,
        min: f32,
        max: f32,
        entry_type: u32,
        relative: bool,
        is_3d: bool,
        prio: i32,
        stream: bool,
    ) -> (Option<i32>, bool) {
        let dist_prio = distance_priority(pos, self.listener, min, max, is_3d, relative);
        let mut ch = match name {
            "missing" => {
                ev("log 2 ERROR: Can't find sound 'missing'!".into());
                return (None, false);
            }
            "full" => {
                ev("log 1 WARNING: Could not start sound 'full', too many sounds playing!".into());
                return (None, true);
            }
            _ => {
                let c = Ch::new(self.next_ch);
                self.next_ch += 1;
                ev(format!(
                    "c{} create {name} prio {} stream {}",
                    c.id,
                    dist_prio + prio,
                    u8::from(stream)
                ));
                c
            }
        };
        let params = PlayParams {
            pos,
            min_dist: min,
            max_dist: max,
            priority_modifier: prio,
            entry_type,
            looping,
            relative,
            is_3d,
            silent: self.handler().silent(),
        };
        setup_channel(&mut ch, &params, dist_prio, &self.matrix);

        let idx = self.shells.len();
        let lower = name.to_ascii_lowercase();
        let h = self.handler();
        let st = EntryState::new(volume, entry_type, is_3d, stream);
        let entry = h
            .new_entry((idx + 1) as *mut c_void, lower.as_bytes(), st)
            .expect("entry");
        let id = h.entry_id(entry);
        let state = h.state_ptr(entry);
        self.shells.push(Some(Shell {
            ch,
            state,
            entry,
            cb: 0,
            id,
            name: lower,
        }));
        self.handler().push_back(entry);
        (Some(id), false)
    }

    fn entry_stop(&mut self, p: *mut c_void) {
        let s = self.shell(p);
        // SAFETY: `state` lives until the shell is deleted.
        unsafe { &mut *s.state }.stop(&mut s.ch);
    }

    fn for_each(&mut self, mask: u32, mut f: impl FnMut(&mut Self, *mut c_void)) {
        let mut i = 0;
        loop {
            let (stop, hit) = {
                let h = self.handler();
                let p = h.shell_at(i);
                if p.is_null() {
                    (true, None)
                } else {
                    let ty = self.shell(p).state_type();
                    (false, (ty & mask != 0).then_some(p))
                }
            };
            if stop {
                break;
            }
            if let Some(p) = hit {
                f(self, p);
            }
            i += 1;
        }
    }

    fn dump(&mut self, out: &mut String) {
        let n = self.handler().len();
        out.push_str(&format!("st n={n}"));
        for i in 0..n {
            let p = self.handler().shell_at(i);
            let s = self.shell(p);
            // SAFETY: as in `entry_stop`.
            let st = unsafe { &*s.state };
            out.push_str(&format!(
                " [{} {} t{} c{} p{}{}{} first{} vm{:08x} v{:08x} sp{:08x} pr{}]",
                s.id,
                s.name,
                st.entry_type,
                s.ch.id,
                u8::from(s.ch.playing),
                u8::from(s.ch.paused),
                u8::from(s.ch.looping),
                u8::from(st.first_time),
                b(st.volume_mul),
                b(s.ch.volume),
                b(s.ch.speed),
                s.ch.priority
            ));
        }
        out.push('\n');
    }

    fn callback(&mut self, mode: i32, id: i32) {
        ev(format!("cb {id} mode {mode}"));
        match mode {
            1 => {
                self.play(
                    "cbplay",
                    false,
                    0.5,
                    v3(1.0, 0.0, 0.0),
                    1.0,
                    10.0,
                    1,
                    false,
                    true,
                    0,
                    false,
                );
            }
            2 => self.for_each(ALL, |w, p| w.entry_stop(p)),
            3 => self.for_each(ALL, |w, p| {
                let s = w.shell(p);
                // SAFETY: as in `entry_stop`.
                unsafe { &mut *s.state }.fade_out(0.5);
            }),
            4 => {
                let p = self.find("snd_a");
                if !p.is_null() {
                    self.entry_stop(p);
                }
            }
            _ => {}
        }
    }

    fn finish(&mut self, out: &mut String) {
        while !self.handler().is_empty() {
            let p = self.handler().shell_at(0);
            self.delete(p);
        }
        EV.with(|e| out.push_str(&e.take()));
    }
}

impl Shell {
    fn state_type(&self) -> u32 {
        // SAFETY: `state` lives until the shell is deleted.
        unsafe { &*self.state }.entry_type
    }
}

impl Shells for World {
    fn update(&mut self, p: *mut c_void, dt: f32) -> bool {
        let (h, listener, matrix) = (self.h, self.listener, self.matrix);
        let s = self.shell(p);
        let mut host = Host {
            h,
            listener,
            matrix,
        };
        // SAFETY: as in `entry_stop`.
        let step = unsafe { &mut *s.state }.update(dt, &mut s.ch, &mut host);
        match step {
            Step::Alive => true,
            Step::Finished { release } => {
                let (cb, id) = (s.cb, s.id);
                if release && cb != 0 {
                    self.callback(cb, id);
                }
                self.shell(p).ch.stop();
                false
            }
        }
    }

    fn delete(&mut self, p: *mut c_void) {
        let idx = sh(p);
        if let Some(mut s) = self.shells.get_mut(idx).and_then(Option::take) {
            s.ch.stop();
            let entry = s.entry;
            drop(s);
            self.handler().free_entry(entry);
        }
    }
}

impl Drop for World {
    fn drop(&mut self) {
        // SAFETY: allocated in `new`, freed once.
        drop(unsafe { Box::from_raw(self.h) });
    }
}

fn entry_op(world: &mut World, w: &[&str]) {
    let r: usize = w[2].parse().expect("ref");
    let (p, id) = world.refs[r];
    let valid = world.handler().is_valid(p, id);
    if valid {
        let a = |k: usize| hf(w[k]);
        let on = |k: usize| w[k] == "1";
        let s = world.shell(p);
        // SAFETY: `state` lives until the shell is deleted.
        let st = unsafe { &mut *s.state };
        match w[1] {
            "fadevol" => st.fade_volume_mul_to(a(3), a(4)),
            "fadespeed" => st.fade_speed_mul_to(a(3), a(4)),
            "fadeout" => st.fade_out(a(3)),
            "fadein" => st.fade_in(a(3), a(4)),
            "setvolmul" => {
                st.volume_mul = a(3);
                st.volume_fade_dest = a(3);
            }
            "setdefault" => {
                st.normal_volume = a(3);
                st.normal_speed = a(4);
                st.speed_mul = a(3);
                st.speed_fade_dest = a(3);
            }
            "stopentry" => st.stop(&mut s.ch),
            "stopdisabled" => st.stop_disabled = on(3),
            _ => s.ch.set_paused(on(3)),
        }
    }
    let sd = if valid {
        i32::from(
            // SAFETY: as above.
            unsafe { &*world.shell(p).state }.stop_disabled,
        )
    } else {
        -1
    };
    ev(format!("valid {} stopdisabled {sd}", u8::from(valid)));
}

fn run() -> String {
    let lines: Vec<&str> = GOLDEN.lines().collect();
    let mut out = String::new();
    let mut world = World::new();
    let mut i = 0;
    while i < lines.len() {
        let line = lines[i];
        i += 1;
        if line.starts_with("seed ") {
            world = World::new();
            EV.with(|e| e.take());
            out.push_str(line);
            out.push('\n');
            continue;
        }
        if line == "end" {
            world.finish(&mut out);
            out.push_str("end\n");
            continue;
        }
        if !line.starts_with("op ") {
            continue;
        }
        let w: Vec<&str> = line.split(' ').collect();
        out.push_str(line);
        out.push('\n');
        while i < lines.len() && lines[i].starts_with("g ") {
            let g: Vec<&str> = lines[i].split(' ').collect();
            let slot: usize = g[1].parse().expect("slot");
            world.gv[slot] = hf(g[2]);
            world.gs[slot] = hf(g[3]);
            out.push_str(lines[i]);
            out.push('\n');
            i += 1;
        }
        let u = |k: usize| w[k].parse::<u32>().expect("uint");
        match w[1] {
            "play" => {
                let (id, not_enough) = world.play(
                    w[2],
                    w[3] == "1",
                    hf(w[4]),
                    v3(hf(w[5]), hf(w[6]), hf(w[7])),
                    hf(w[8]),
                    hf(w[9]),
                    u(10),
                    w[11] == "1",
                    w[12] == "1",
                    w[13].parse().expect("prio"),
                    w[14] == "1",
                );
                let cb: i32 = w[15].parse().expect("cb");
                if let Some(id) = id {
                    let idx = world.shells.len() - 1;
                    let p = (idx + 1) as *mut c_void;
                    world.shell(p).cb = cb;
                    world.refs.push((p, id));
                }
                ev(format!(
                    "play -> {} notenough {}",
                    id.unwrap_or(-1),
                    u8::from(not_enough)
                ));
            }
            "tick" => {
                let (gv, gs) = (world.gv, world.gs);
                world.handler().set_globals(&gv, &gs);
                let hp = world.h;
                // SAFETY: `hp` is live; `world` re-enters it only through the raw pointer.
                unsafe { handler::update_all(hp, hf(w[2]), &mut world) };
            }
            "stop" => {
                let p = world.find(w[2]);
                if !p.is_null() {
                    world.entry_stop(p);
                }
                ev(format!("stop -> {}", u8::from(!p.is_null())));
            }
            "stopall" => world.for_each(u(2), |w, p| w.entry_stop(p)),
            "pauseall" | "resumeall" => {
                let pause = w[1] == "pauseall";
                world.for_each(u(2), |w, p| w.shell(p).ch.set_paused(pause));
            }
            "fadeoutall" => {
                let (speed, disable) = (hf(w[3]), w[4] == "1");
                world.for_each(u(2), |w, p| {
                    // SAFETY: `state` lives until the shell is deleted.
                    let st = unsafe { &mut *w.shell(p).state };
                    st.fade_out(speed);
                    if disable {
                        st.stop_disabled = true;
                    }
                });
            }
            "fadevol" | "fadespeed" | "fadeout" | "fadein" | "setvolmul" | "setdefault"
            | "stopentry" | "stopdisabled" | "pause" => entry_op(&mut world, &w),
            "steal" | "end" => {
                let (p, _) = world.refs[u(2) as usize];
                let ch = &mut world.shell(p).ch;
                ch.playing = false;
                ch.stop_used |= w[1] == "end";
            }
            "global" => {
                // The settings handler is C++; its result is replay input.
                let rec = lines.iter().skip(i).find(|l| l.starts_with("  global ->"));
                ev(rec.map_or(String::new(), |l| l.trim_start().to_string()));
            }
            "silent" => world.handler().set_silent(w[2] == "1"),
            "listener" => {
                world.listener = v3(hf(w[2]), hf(w[3]), hf(w[4]));
                let mut m = Matrixf::default();
                for (k, f) in w[5..21].iter().enumerate() {
                    if let Some(c) = m.m.get_mut(k / 4).and_then(|r| r.get_mut(k % 4)) {
                        *c = hf(f);
                    }
                }
                world.matrix = m;
            }
            "query" => {
                let p = world.find(w[2]);
                let playing = !p.is_null() && world.shell(p).ch.is_playing();
                ev(format!("isplaying {}", u8::from(playing)));
                let t = u(3);
                let h = world.handler();
                ev(format!(
                    "global {:08x} {:08x}",
                    b(h.global_volume(t)),
                    b(h.global_speed(t))
                ));
            }
            _ => {}
        }
        EV.with(|e| out.push_str(&e.take()));
        world.dump(&mut out);
    }
    out
}

#[test]
fn replay_matches_golden() {
    let out = run();
    for (n, (a, g)) in out.lines().zip(GOLDEN.lines()).enumerate() {
        assert_eq!(a, g, "line {}", n + 1);
    }
    assert_eq!(out.lines().count(), GOLDEN.lines().count());
}
