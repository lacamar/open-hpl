//! A7 golden replay: the seeded op scripts of `rust/tests/golden/A7/harness/a7_harness.cpp`, run
//! against the Rust-owned map arena and area blocks without the C++. `a7_ops.txt` was recorded from
//! the std-container originals; re-record: `A7_RECORD=1 cargo test a7_ops_match_golden`.

use super::a7_area::{
    A7Area, T_EXAMINE, T_FLASHBACK, T_INSANITY, T_LADDER, T_LIQUID, T_SCRIPT, T_SIGN, T_SLIME,
    T_STICKY,
};
use super::a7_map::{A7Map, Entity, Timer, UseItem, W_DOOMED};
use super::area::ExamineIn;
use crate::math::v3;
use crate::obj::Foreign;
use crate::types::{Matrixf, Vector3f};
use std::cell::RefCell;
use std::collections::HashMap;
use std::ffi::c_void;
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

    fn n(&mut self, k: u32) -> u32 {
        self.next() % k
    }
}

const NAMES: [&str; 10] = [
    "Door_A", "door_a", "Lamp_1", "LAMP_1", "x", "Y", "Door_B", "pos_1", "Pos_1", "start",
];
const K_ENT: i32 = 0;
const K_DIS: i32 = 1;
const K_LAMP: i32 = 2;
const K_START: i32 = 3;
const K_POS: i32 = 4;
const K_USE: i32 = 5;

fn bits(f: f32) -> String {
    format!("{:08x}", f.to_bits())
}

fn lower(s: &str) -> String {
    s.to_ascii_lowercase()
}

struct Obj {
    name: String,
    id: i32,
    doomed: bool,
}

thread_local! {
    static TRACE: RefCell<Vec<String>> = const { RefCell::new(Vec::new()) };
    static LIVE: RefCell<HashMap<usize, (i32, String)>> = RefCell::new(HashMap::new());
}

fn t(s: String) {
    TRACE.with(|x| x.borrow_mut().push(s));
}

extern "C" fn drop_thunk(p: *mut c_void) {
    let hit = LIVE.with(|l| l.borrow().get(&(p as usize)).cloned());
    if let Some((tag, name)) = hit {
        t(format!("drop {tag} {name}"));
    }
}

struct Run {
    // the boxes keep object addresses stable
    #[allow(clippy::vec_box)]
    objs: Vec<Box<Obj>>,
    m: A7Map,
    serial: i32,
    flags: HashMap<usize, i32>,
}

impl Run {
    fn obj(&mut self, tag: i32, name: &str, id: i32) -> usize {
        let b = Box::new(Obj {
            name: name.to_string(),
            id,
            doomed: false,
        });
        let p = std::ptr::from_ref::<Obj>(&b) as usize;
        LIVE.with(|l| l.borrow_mut().insert(p, (tag, name.to_string())));
        self.objs.push(b);
        p
    }

    fn o(&self, p: usize) -> Option<&Obj> {
        self.objs
            .iter()
            .find(|b| std::ptr::from_ref::<Obj>(b) as usize == p)
            .map(|b| &**b)
    }

    fn n(&self, p: usize) -> String {
        match self.o(p) {
            Some(o) => format!("{}#{}", o.name, o.id),
            None => "-".to_string(),
        }
    }

    fn name(&self, p: usize) -> String {
        self.o(p).map(|o| o.name.clone()).unwrap_or_default()
    }

    fn mark_doomed(&mut self, p: usize) -> bool {
        match self
            .objs
            .iter_mut()
            .find(|b| std::ptr::from_ref::<Obj>(b) as usize == p)
        {
            Some(o) if !o.doomed => {
                o.doomed = true;
                true
            }
            _ => false,
        }
    }

    fn doom(&mut self, p: usize) {
        if self.mark_doomed(p) {
            self.m.doom_push(p);
        }
    }

    fn typed(p: usize) -> usize {
        p + 8
    }

    fn from_typed(p: usize) -> usize {
        p - 8
    }

    fn walk(&self, which: u32) -> Vec<usize> {
        let w = [0u32, 1, 3][which as usize];
        self.m
            .list_snapshot(w)
            .into_iter()
            .map(|p| if which == 0 { p } else { Self::from_typed(p) })
            .collect()
    }

    fn timers_str(&self, with_func: bool) -> String {
        let mut s = String::new();
        for i in 0..self.m.timer_len() {
            if let Some(tm) = self.m.timer_at(i) {
                let name = String::from_utf8_lossy(&tm.name);
                let func = String::from_utf8_lossy(&tm.func);
                if with_func {
                    let _ = write!(s, " {name}:{func}:{}", bits(tm.count));
                } else {
                    let _ = write!(s, " {name}:{}", bits(tm.count));
                }
                s.push_str(if tm.destroy_me { "!" } else { "" });
            }
        }
        s
    }

    fn script(&mut self, r: &mut Rng, func: &str, name: &str) {
        t(format!("script {func}({name})"));
        match r.n(6) {
            0 => {
                let cn = format!("cb{}", self.serial);
                self.serial += 1;
                let ct = r.f(0.05);
                let cf = format!("fn{}", r.n(3));
                self.m.timer_add(cn.as_bytes(), ct, cf.as_bytes());
            }
            1 => {
                let n = NAMES[r.n(10) as usize];
                self.m.timer_remove(n.as_bytes());
            }
            2 => self.m.timer_remove(name.as_bytes()),
            3 => {
                let n = NAMES[r.n(10) as usize];
                match self.m.timer_get(n.as_bytes()) {
                    Some(c) => t(format!("get {}", bits(c))),
                    None => t("get none".to_string()),
                }
            }
            _ => {}
        }
    }

    fn timers_update(&mut self, step: f32, r: &mut Rng) {
        self.m.timers_begin(step);
        let mut cursor = 0usize;
        while let Some((n, f)) = self.m.timer_poll(&mut cursor) {
            let n = String::from_utf8_lossy(&n).into_owned();
            let f = String::from_utf8_lossy(&f).into_owned();
            self.script(r, &f, &n);
            self.m.timer_finish(cursor);
        }
        self.m.timers_end();
    }

    fn update_doomed(&mut self, r: &mut Rng) {
        let mut cur = self.m.cursor_begin(W_DOOMED);
        loop {
            let e = self.m.cursor_get(W_DOOMED, cur);
            if e == 0 {
                break;
            }
            t(format!("doomed {}", self.name(e)));
            if r.n(4) == 0 {
                let all = self.walk(0);
                if !all.is_empty() {
                    let pick = all[r.n(all.len() as u32) as usize];
                    self.doom(pick);
                }
            }
            let kf = self.flags.get(&e).copied().unwrap_or(0);
            let enemy = kf & 1 != 0;
            let sticky = kf & 2 != 0;
            let en = Entity {
                p: e,
                typed: Self::typed(e),
            };
            self.m
                .unlink_entity(en, enemy, if enemy { false } else { sticky }, sticky);
            drop_thunk(e as *mut c_void);
            cur = self.m.cursor_next(W_DOOMED, cur);
        }
        self.m.list_clear(W_DOOMED);
    }

    fn dissolve_update(&mut self, step: f32) {
        let out = self.m.dissolve_step(step);
        for (obj, alpha, remove) in out {
            t(format!("cover {} {}", self.name(obj), bits(alpha)));
            if remove {
                drop_thunk(obj as *mut c_void);
            }
        }
    }

    fn lamp_update(&mut self, step: f32) {
        for conn in self.m.lamp_snapshot() {
            t(format!("lamp {} {}", self.name(conn), bits(step)));
        }
    }

    fn run(&mut self, seed: u64) {
        let mut r = Rng(seed);
        let mut live: Vec<usize> = Vec::new();
        for n in 0..400 {
            let k = r.n(22);
            let head = format!("s{n} k{k} ");
            match k {
                0..=2 => {
                    let en = NAMES[r.n(10) as usize];
                    let fresh = r.b();
                    let id = if fresh {
                        self.m.free_entity_id()
                    } else {
                        r.n(6) as i32
                    };
                    let e = self.obj(K_ENT, en, id);
                    let kf = if r.n(5) == 0 {
                        1
                    } else if r.n(5) == 0 {
                        2
                    } else {
                        0
                    };
                    let enemy = kf & 1 != 0;
                    let sticky = kf & 2 != 0;
                    self.flags.insert(e, kf);
                    let ent = Entity {
                        p: e,
                        typed: Self::typed(e),
                    };
                    self.m.add_entity(
                        ent,
                        en.as_bytes(),
                        id,
                        enemy,
                        if enemy { false } else { sticky },
                        sticky,
                    );
                    live.push(e);
                    t(format!(
                        "{head}add {} kf{kf} latest {}",
                        self.n(e),
                        self.n(self.m.latest())
                    ));
                }
                3 | 4 => {
                    let low = r.b();
                    let fnm = NAMES[r.n(10) as usize];
                    let by_name = self
                        .m
                        .find_name(if low { lower(fnm) } else { fnm.to_string() }.as_bytes());
                    let by_id = self.m.find_id(r.n(8) as i32);
                    t(format!("{head}find {} {}", self.n(by_name), self.n(by_id)));
                }
                5 => {
                    if !live.is_empty() {
                        let e = live[r.n(live.len() as u32) as usize];
                        t(format!(
                            "{head}exists {} {}",
                            u8::from(self.m.exists(e)),
                            self.n(e)
                        ));
                    }
                }
                6 => t(format!("{head}freeid {}", self.m.free_entity_id())),
                7 => {
                    if r.n(4) == 0 {
                        self.m.reset_latest();
                        t(format!("{head}reset {}", self.n(self.m.latest())));
                    }
                }
                8 | 9 => {
                    if !live.is_empty() {
                        let e = live[r.n(live.len() as u32) as usize];
                        self.doom(e);
                        t(format!("{head}doom {}", self.n(e)));
                    }
                }
                10 => {
                    self.update_doomed(&mut r);
                    live = self.walk(0);
                    t(format!("{head}after doomed live={}", live.len()));
                }
                11 => {
                    let mut s = format!("{head}walk");
                    for w in 0..3 {
                        s.push_str(" |");
                        for o in self.walk(w) {
                            let _ = write!(s, " {}", self.n(o));
                        }
                    }
                    t(s);
                }
                12 | 13 => {
                    let tn = NAMES[r.n(10) as usize];
                    let mut tt = r.f(0.2);
                    if r.n(4) == 0 {
                        tt -= 0.1;
                    }
                    let tf = format!("fn{}", r.n(3));
                    self.m.timer_add(tn.as_bytes(), tt, tf.as_bytes());
                    t(format!("{head}timer+"));
                }
                14 => {
                    let n = NAMES[r.n(10) as usize];
                    self.m.timer_remove(n.as_bytes());
                    t(format!("{head}timer-{}", self.timers_str(false)));
                }
                15 => {
                    let step = r.f(0.05);
                    self.timers_update(step, &mut r);
                    t(format!("{head}tick{}", self.timers_str(true)));
                }
                16 => {
                    let name = format!("use{}", self.serial);
                    self.serial += 1;
                    let o = self.obj(K_USE, &name, 0);
                    let i = NAMES[r.n(4) as usize];
                    let e = NAMES[r.n(4) as usize];
                    self.m.use_item_add(UseItem {
                        ptr: o,
                        name: name.as_bytes().to_vec(),
                        item: i.as_bytes().to_vec(),
                        entity: e.as_bytes().to_vec(),
                    });
                    let found = self.m.use_item_find(i.as_bytes(), e.as_bytes());
                    t(format!("{head}use+ {name} {i} {e} find {}", self.n(found)));
                }
                17 => {
                    if r.b() {
                        let nm = format!("use{}", r.n((self.serial + 1) as u32));
                        let hit = self.m.use_item_remove_name(nm.as_bytes());
                        if let Some(p) = hit {
                            drop_thunk(p as *mut c_void);
                        }
                        t(format!("{head}use-name {}", u8::from(hit.is_some())));
                    } else {
                        let ui = NAMES[r.n(4) as usize];
                        let ue = NAMES[r.n(4) as usize];
                        let o = self.m.use_item_find(ui.as_bytes(), ue.as_bytes());
                        if o != 0 {
                            let nm = self.name(o);
                            let arg = if r.b() {
                                nm.clone()
                            } else {
                                "other".to_string()
                            };
                            if let Some(p) = self.m.use_item_remove_ptr(o, arg.as_bytes()) {
                                drop_thunk(p as *mut c_void);
                            }
                            let again = self
                                .m
                                .use_item_find(NAMES[0].as_bytes(), NAMES[0].as_bytes());
                            t(format!("{head}use-ptr {nm} again {}", self.n(again)));
                        }
                    }
                }
                18 => {
                    let name = format!("dis{}", self.serial);
                    self.serial += 1;
                    let o = self.obj(K_DIS, &name, 0);
                    let tm = r.f(0.3);
                    self.m.dissolve_add(o, tm);
                    if r.n(3) == 0 {
                        for _ in 0..3 {
                            let st = r.f(0.2);
                            self.dissolve_update(st);
                        }
                    }
                    t(format!("{head}dissolve"));
                }
                19 => {
                    let name = format!("lamp{}", self.serial);
                    self.serial += 1;
                    let o = self.obj(K_LAMP, &name, 0);
                    let l = r.n(8) as usize;
                    if self.m.lamp_find(l + 1) == 0 {
                        self.m.lamp_add(o, l + 1);
                    }
                    t(format!(
                        "{head}lamp find {}",
                        self.n(self.m.lamp_find(l + 1))
                    ));
                    let st = r.f(0.05);
                    self.lamp_update(st);
                }
                20 => {
                    let pos = r.b();
                    let nm = NAMES[r.n(10) as usize];
                    let o = self.obj(if pos { K_POS } else { K_START }, nm, 0);
                    if pos {
                        self.m.pos_add(o, nm.as_bytes());
                    } else {
                        self.m.start_add(o, nm.as_bytes());
                    }
                    let sf = NAMES[r.n(10) as usize];
                    let pf = NAMES[r.n(10) as usize];
                    let mut s = format!(
                        "{head}{} {} first {} find {} {} order",
                        if pos { "pos+" } else { "start+" },
                        self.n(o),
                        self.n(self.m.start_first()),
                        self.n(self.m.start_find(sf.as_bytes())),
                        self.n(self.m.pos_find(pf.as_bytes()))
                    );
                    for i in 0..self.m.start_num() {
                        let _ = write!(s, " {}", self.name(self.m.start_at(i)));
                    }
                    t(s);
                }
                _ => {
                    let show = r.b();
                    let changed = self.m.commentary_sync(show).is_some();
                    t(format!("{head}commentary {}", u8::from(changed)));
                    if r.n(40) == 0 {
                        let mut saved: Vec<Timer> = (0..self.m.timer_len())
                            .filter_map(|i| self.m.timer_at(i).cloned())
                            .collect();
                        for tm in &mut saved {
                            tm.count += 1.0;
                        }
                        self.m.timers_clear();
                        for tm in saved {
                            self.m.timer_push_raw(tm);
                        }
                        t(format!("{head}timers-set {}", self.m.timer_len()));
                    }
                    if r.n(60) == 0 {
                        self.destroy_all();
                        live.clear();
                        t(format!("{head}destroy-all"));
                    }
                }
            }
        }
        self.teardown();
        t("teardown".to_string());
    }

    fn destroy_all(&mut self) {
        self.m.timers_clear();
        for c in self.m.lamp_take() {
            drop_thunk(c as *mut c_void);
        }
        self.m.set_deleting_all(true);
        for e in self.m.list_snapshot(0) {
            drop_thunk(e as *mut c_void);
        }
        self.m.set_deleting_all(false);
        self.m.after_destroy_all();
    }

    fn teardown(&mut self) {
        self.m.timers_clear();
        for c in self.m.lamp_take() {
            drop_thunk(c as *mut c_void);
        }
        self.m.set_deleting_all(true);
        for e in self.m.list_snapshot(0) {
            drop_thunk(e as *mut c_void);
        }
        self.m.set_deleting_all(false);
        self.m.after_destroy_all();
        for v in [
            self.m.start_take(),
            self.m.pos_take(),
            self.m.use_items_take(),
            self.m.dissolve_take(),
        ] {
            for p in v {
                drop_thunk(p as *mut c_void);
            }
        }
    }
}

fn vec_s(v: Vector3f) -> String {
    format!("{},{},{}", bits(v.x), bits(v.y), bits(v.z))
}

fn rand_mtx(r: &mut Rng) -> Matrixf {
    let mut m = Matrixf::default();
    for x in 0..4 {
        for y in 0..4 {
            m.m[x][y] = if x == y { 1.0 } else { r.f(0.5) - 0.5 };
        }
    }
    m
}

fn rand_v(r: &mut Rng, scale: f32) -> Vector3f {
    let x = r.f(scale);
    let y = r.f(scale);
    let z = r.f(scale);
    v3(x, y, z)
}

fn areas(seed: u64) {
    let mut r = Rng(seed);
    let engine = 1.0f32 / 60.0;
    for round in 0..40 {
        let kind = round % 9;
        let Some(mut a) = A7Area::new(kind) else {
            continue;
        };
        let head = format!("area {round} kind {kind}");
        match kind {
            T_INSANITY => {
                if let super::a7_area::Pod::Insanity(s) = &mut a.pod {
                    s.check_time_min = 0.25;
                    s.check_time_max = 0.25;
                    s.check_collision_count = r.f(0.3);
                }
                for _ in 0..60 {
                    let step = r.f(0.05);
                    let go = a.tick(step, engine);
                    let c = match &a.pod {
                        super::a7_area::Pod::Insanity(s) => s.check_collision_count,
                        _ => 0.0,
                    };
                    t(format!("{head} tick {} {}", u8::from(go), bits(c)));
                }
            }
            T_SLIME => {
                if let super::a7_area::Pod::Slime(s) = &mut a.pod {
                    s.min_check_attack_time = 0.5;
                    s.max_check_attack_time = 0.5;
                    s.check_collision_count = r.f(0.5);
                }
                for _ in 0..60 {
                    let step = r.f(0.08);
                    let go = a.tick(step, engine);
                    let c = match &a.pod {
                        super::a7_area::Pod::Slime(s) => s.check_collision_count,
                        _ => 0.0,
                    };
                    t(format!("{head} tick {} {}", u8::from(go), bits(c)));
                }
            }
            T_FLASHBACK => {
                if let super::a7_area::Pod::Flashback(s) = &mut a.pod {
                    s.check_collision_count = r.f(0.2);
                }
                for _ in 0..60 {
                    let step = if r.b() { engine } else { r.f(0.05) / 100.0 };
                    let go = a.tick(step, engine);
                    let c = match &a.pod {
                        super::a7_area::Pod::Flashback(s) => s.check_collision_count,
                        _ => 0.0,
                    };
                    t(format!("{head} tick {} {}", u8::from(go), bits(c)));
                }
            }
            T_EXAMINE => {
                if let super::a7_area::Pod::Examine(s) = &mut a.pod {
                    s.insane_limit = 30.0;
                }
                for _ in 0..80 {
                    if r.n(3) == 0 {
                        let i = ExamineIn {
                            has_cat: r.b(),
                            has_insane_cat: r.b(),
                            has_insane_entry: r.b(),
                            has_sound: r.b(),
                            has_insane_sound: r.b(),
                        };
                        let sanity = r.f(50.0);
                        let out = a.examine_interact(i, sanity);
                        let c = match &a.pod {
                            super::a7_area::Pod::Examine(s) => s.play_sound_count,
                            _ => 0.0,
                        };
                        t(format!(
                            "{head} interact {}{}{}{} {}",
                            u8::from(out.show_text),
                            u8::from(out.insane_text),
                            u8::from(out.play_sound),
                            u8::from(out.insane_sound),
                            bits(c)
                        ));
                    } else {
                        let step = r.f(0.1);
                        a.tick(step, engine);
                        let c = match &a.pod {
                            super::a7_area::Pod::Examine(s) => s.play_sound_count,
                            _ => 0.0,
                        };
                        t(format!("{head} tick {}", bits(c)));
                    }
                }
            }
            T_LIQUID => {
                for _ in 0..30 {
                    let py = r.f(10.0);
                    let sy = r.f(4.0);
                    a.liquid_setup(py, sy);
                    let step = if r.b() { engine } else { r.f(0.05) / 100.0 };
                    let go = a.tick(step, engine);
                    let (d, tc) = match &a.pod {
                        super::a7_area::Pod::Liquid(s) => (s.surface_plane[3], s.time_count),
                        _ => (0.0, 0.0),
                    };
                    t(format!(
                        "{head} liquid {} {} {}",
                        u8::from(go),
                        bits(d),
                        bits(tc)
                    ));
                }
            }
            T_LADDER => {
                for _ in 0..20 {
                    let m = rand_mtx(&mut r);
                    let py = r.f(10.0);
                    let sy = r.f(4.0);
                    a.ladder_setup(&m, py, sy);
                    let ap = rand_v(&mut r, 5.0);
                    let cy = r.f(8.0);
                    let out = a.ladder_start(ap, v3(0.5, 1.8, 0.5), cy);
                    if let super::a7_area::Pod::Ladder(s) = &a.pod {
                        t(format!(
                            "{head} ladder {} {} {} start {}",
                            vec_s(s.forward),
                            bits(s.max_y),
                            bits(s.min_y),
                            vec_s(out)
                        ));
                    }
                }
            }
            T_STICKY => {
                let can_detach = r.b();
                let move_body = r.b();
                let rotate_body = r.b();
                if let super::a7_area::Pod::Sticky(s) = &mut a.pod {
                    s.pose_time = 0.2;
                    s.can_detach = can_detach;
                    s.move_body = move_body;
                    s.rotate_body = rotate_body;
                    s.set_mtx_time = 1.0;
                }
                for _ in 0..30 {
                    let m = rand_mtx(&mut r);
                    if r.n(3) == 0 {
                        let grav = r.b();
                        let mass = r.f(5.0);
                        let saved = r.n(3) as i32 - 1;
                        a.sticky_attach(m, grav, mass, (saved >= 0).then_some(saved != 0));
                        if let super::a7_area::Pod::Sticky(s) = &a.pod {
                            t(format!(
                                "{head} attach {} {} {}",
                                u8::from(s.attached_body_gravity),
                                bits(s.attached_body_mass),
                                u8::from(s.attached_entity_full_game_saved)
                            ));
                        }
                    }
                    let ap = rand_v(&mut r, 3.0);
                    let mc = rand_v(&mut r, 1.0);
                    let at = rand_v(&mut r, 3.0);
                    let step = r.f(0.1);
                    let out = a.sticky_step(m, ap, mc, at, step);
                    let ran = out.is_some();
                    let o = out.unwrap_or_default();
                    let cd = a.sticky_can_detach();
                    t(format!(
                        "{head} step {} {} {} {} {} {} detach {}",
                        u8::from(ran),
                        o.mode,
                        bits(o.set_time),
                        u8::from(o.done),
                        bits(o.mtx.m[0][0]),
                        vec_s(o.pos),
                        u8::from(cd)
                    ));
                }
            }
            _ => {
                let item_interaction = r.b();
                if let super::a7_area::Pod::Script(s) = &mut a.pod {
                    s.item_interaction = item_interaction;
                }
                for _ in 0..20 {
                    let cb = r.b();
                    let custom = r.n(4) as i32 + 10;
                    let ci = a.can_interact(cb);
                    let cross = a.crosshair(cb, custom, &[10, 11, 12, 13]);
                    let oi = kind == T_SCRIPT && a.on_interact_script(cb);
                    t(format!(
                        "{head} iface {} {} {}",
                        u8::from(ci),
                        cross,
                        u8::from(oi)
                    ));
                }
            }
        }
        let bp = rand_v(&mut r, 3.0);
        let pp = rand_v(&mut r, 3.0);
        a.attach(bp, pp);
        let mv = a.follow(pp, bp);
        t(format!(
            "{head} follow {} {}",
            vec_s(a.base.rel_offset),
            vec_s(mv)
        ));
    }
}

fn maps(seed: u64) {
    let mut r = Rng(seed);
    let mut m = A7Map::default();
    m.set_foreign(
        0,
        Foreign {
            drop: Some(drop_thunk),
            ..Foreign::default()
        },
    );
    let mut pool: Vec<(usize, String)> = Vec::new();
    let mut objs: Vec<Box<u8>> = Vec::new();
    for i in 0..60 {
        let k = r.n(5);
        if k < 2 {
            let b = Box::new(0u8);
            let p = std::ptr::from_ref::<u8>(&b) as usize;
            objs.push(b);
            let name = format!("map{i}");
            LIVE.with(|l| l.borrow_mut().insert(p, (0, name.clone())));
            m.list_add(p);
            pool.push((p, name.clone()));
            t(format!("load {name}"));
        } else if k < 4 && !pool.is_empty() {
            let (p, name) = pool[r.n(pool.len() as u32) as usize].clone();
            m.list_remove(p);
            drop_thunk(p as *mut c_void);
            t(format!("destroy {name}"));
        } else {
            for p in m.list_snapshot(0) {
                drop_thunk(p as *mut c_void);
            }
            m.after_destroy_all();
            t("reset".to_string());
        }
    }
    for p in m.list_snapshot(0) {
        drop_thunk(p as *mut c_void);
    }
}

fn trace_all() -> String {
    let mut out = String::new();
    for seed in 1..=24u64 {
        TRACE.with(|x| x.borrow_mut().clear());
        LIVE.with(|l| l.borrow_mut().clear());
        t(format!("seed {seed}"));
        let mut m = A7Map::default();
        for kind in 0..6 {
            m.set_foreign(
                kind,
                Foreign {
                    drop: Some(drop_thunk),
                    ..Foreign::default()
                },
            );
        }
        let mut run = Run {
            objs: Vec::new(),
            m,
            serial: 0,
            flags: HashMap::new(),
        };
        run.run(seed);
        TRACE.with(|x| {
            for l in x.borrow().iter() {
                out.push_str(l);
                out.push('\n');
            }
        });
    }
    for seed in 1..=6u64 {
        TRACE.with(|x| x.borrow_mut().clear());
        t(format!("area seed {seed}"));
        areas(seed);
        TRACE.with(|x| {
            for l in x.borrow().iter() {
                out.push_str(l);
                out.push('\n');
            }
        });
    }
    for seed in 1..=4u64 {
        TRACE.with(|x| x.borrow_mut().clear());
        t(format!("maps seed {seed}"));
        maps(seed);
        TRACE.with(|x| {
            for l in x.borrow().iter() {
                out.push_str(l);
                out.push('\n');
            }
        });
    }
    out
}

#[test]
fn a7_ops_match_golden() {
    let path = concat!(env!("CARGO_MANIFEST_DIR"), "/tests/golden/A7/a7_ops.txt");
    let got = trace_all();
    if std::env::var_os("A7_RECORD").is_some() {
        std::fs::write(path, &got).expect("write golden");
        return;
    }
    let want = std::fs::read_to_string(path).expect("golden present");
    let first = got.lines().zip(want.lines()).position(|(a, b)| a != b);
    assert!(got == want, "a7 ops golden differs at line {first:?}");
}

#[test]
fn area_arena_pointers_are_stable_and_freed() {
    let mut arena = super::a7_area::AreaArena::default();
    let a = arena.alloc(T_SIGN).expect("alloc");
    let b = arena.alloc(T_LADDER).expect("alloc");
    assert_eq!(arena.len(), 2);
    // SAFETY: both blocks are live and from this arena.
    unsafe {
        (*a).base.body_id = 7;
        assert_eq!((*a).base.body_id, 7);
        arena.free(a);
        assert_eq!(arena.len(), 1);
        arena.free(b);
    }
    assert!(arena.is_empty());
}
