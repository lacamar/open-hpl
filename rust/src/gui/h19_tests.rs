//! Replays the H19 base golden scripts (recorded from the C++ `iWidget`, rust/tests/golden/H19)
//! through `h19_widget` against a model of the harness' probe widgets, comparing every line.

use super::h19_widget::{self as hw, Base, Host, Op};
use super::layout;
use super::set::{Msg, W};
use crate::math::{Rect2f, Vector2f, Vector3f};
use std::cell::RefCell;
use std::ffi::c_void;
use std::fmt::Write;
use std::path::Path;

struct Probe {
    base: Box<Base>,
    base_point: bool,
    kids: Vec<usize>,
    text: i32,
}

struct Mock {
    w: Vec<Probe>,
    regions: Vec<Rect2f>,
    cur: usize,
    sort: bool,
    last_s: Vec<String>,
    last_g: Vec<String>,
}

thread_local! {
    static LOG: RefCell<String> = const { RefCell::new(String::new()) };
    static CB: RefCell<u64> = const { RefCell::new(0) };
    static BASES: RefCell<Vec<(*mut Base, bool)>> = const { RefCell::new(Vec::new()) };
}

fn log(s: &str) {
    LOG.with(|l| l.borrow_mut().push_str(s));
}

fn dec(pct: u64) -> bool {
    CB.with(|c| {
        let mut c = c.borrow_mut();
        *c = c
            .wrapping_mul(6364136223846793005)
            .wrapping_add(1442695040888963407);
        (*c >> 33) % 100 < pct
    })
}

fn idx(w: W) -> i64 {
    w as i64 - 1
}

fn bits(f: f32) -> String {
    format!("{:08x}", f.to_bits())
}

fn tag(d: &Msg) -> i32 {
    d.0.get(64..68)
        .and_then(|b| <[u8; 4]>::try_from(b).ok())
        .map_or(0, i32::from_ne_bytes)
}

fn data(d: &Msg) -> String {
    let t = tag(d);
    if matches!(t, 9 | 10 | 12) {
        return format!(" tagged={t}");
    }
    let p = d.pos();
    format!(
        " pos={},{} val={} key={},{}",
        bits(p[0]),
        bits(p[1]),
        d.val(),
        d.key(),
        d.modifier()
    )
}

fn f32_at(d: &Msg, at: usize) -> f32 {
    d.0.get(at..at + 4)
        .and_then(|b| <[u8; 4]>::try_from(b).ok())
        .map_or(0.0, f32::from_ne_bytes)
}

extern "C" fn callback(obj: *mut c_void, w: W, d: *const Msg) -> bool {
    let msg = obj as usize as i32;
    // SAFETY: the base passes a live message.
    let d = unsafe { *d };
    let ret = dec(30);
    log(&format!("cb w{} m{msg}", idx(w)));
    if msg <= 8 || msg == 14 || msg == 15 || (17..=21).contains(&msg) {
        log(&data(&d));
    }
    if msg == 10 {
        log(&format!(" dt={}", bits(f32_at(&d, 48))));
    }
    log(&format!(" tag={} ret={}\n", tag(&d), u8::from(ret)));
    let i = usize::try_from(idx(w)).unwrap_or(usize::MAX);
    if msg == 11 {
        let target = BASES.with(|b| {
            let mut b = b.borrow_mut();
            b.get_mut(i).and_then(|(p, more)| {
                if *more {
                    None
                } else {
                    *more = true;
                    Some(*p)
                }
            })
        });
        if let Some(p) = target {
            // SAFETY: `p` is a live probe base from `BASES`.
            unsafe { hw::add_callback_to(p, 11, 11usize as *mut c_void, callback) };
        }
    }
    ret
}

fn rect_str(r: &Rect2f) -> String {
    format!("{},{},{},{}", bits(r.x), bits(r.y), bits(r.w), bits(r.h))
}

impl Mock {
    fn clip(&self, h: usize) -> String {
        self.regions.get(h - 1).map_or("null".into(), rect_str)
    }

    fn probe(&mut self, w: W) -> &mut Probe {
        &mut self.w[w - 1]
    }

    fn v(&mut self, w: W, name: &str, d: &Msg, pct: u64) -> bool {
        log(&format!("ev w{} {name}{}", idx(w), data(d)));
        let r = dec(pct);
        log(&format!(" ret={}\n", u8::from(r)));
        r
    }
}

const NAMES: [(i32, &str); 15] = [
    (0, "move"),
    (1, "down"),
    (2, "up"),
    (3, "dbl"),
    (4, "enter"),
    (5, "leave"),
    (8, "lost"),
    (14, "key"),
    (17, "arrowp"),
    (18, "arrowr"),
    (19, "btnp"),
    (20, "btnr"),
    (21, "btnd"),
    (22, "navget"),
    (23, "navlose"),
];

impl Host for Mock {
    fn base(&mut self, w: W) -> *mut Base {
        &mut *self.probe(w).base
    }

    fn call(&mut self, w: W, op: Op, other: W, d: &Msg, f: f32) -> bool {
        let i = idx(w);
        match op {
            Op::OnMessage => {
                log(&format!("ev w{i} onmsg m{other}{}", data(d)));
                let r = dec(8);
                log(&format!(" tag={} ret={}\n", tag(d), u8::from(r)));
                r
            }
            Op::Virtual => {
                let m = other as i32;
                if m == 6 || m == 7 {
                    let name = if m == 6 { "gotfocus" } else { "gottab" };
                    log(&format!("ev w{i} {name}{}", data(d)));
                    let r = self.probe(w).base.enabled;
                    log(&format!(" ret={}\n", u8::from(r)));
                    return r;
                }
                let name = NAMES.iter().find(|(k, _)| *k == m).map_or("?", |(_, n)| *n);
                self.v(w, name, d, 35)
            }
            Op::ChangePosition => {
                log(&format!(
                    "ev w{i} chpos n={}\n",
                    self.probe(w).base.position_count
                ));
                false
            }
            Op::ChangeSize => {
                log(&format!("ev w{i} chsize\n"));
                false
            }
            Op::ChangeText => {
                log(&format!(
                    "ev w{i} chtext changed={}\n",
                    u8::from(self.probe(w).base.text_changed)
                ));
                false
            }
            Op::RemoveChild => {
                hw::remove_child(self, w, other);
                false
            }
            Op::ChildUpdate | Op::OnAttachChild | Op::OnRemoveChild => {
                let n = match op {
                    Op::ChildUpdate => "childupd",
                    Op::OnAttachChild => "attach",
                    _ => "remove",
                };
                log(&format!("ev w{i} {n} c={}\n", idx(other)));
                false
            }
            Op::ChildGotFocus | Op::ChildGotTabFocus | Op::ChildLostFocus => {
                let n = match op {
                    Op::ChildGotFocus => "childfocus",
                    Op::ChildGotTabFocus => "childtab",
                    _ => "childlost",
                };
                log(&format!("ev w{i} {n} c={}{}\n", idx(other), data(d)));
                hw::forward_to_parent(self, w, op, other, d);
                false
            }
            Op::Update => {
                log(&format!("ev w{i} update dt={}\n", bits(f)));
                false
            }
            Op::Draw | Op::DrawAfterClip => {
                let n = if op == Op::Draw { "draw" } else { "drawac" };
                log(&format!(
                    "ev w{i} {n} dt={} clip={} cur={}\n",
                    bits(f),
                    self.clip(other),
                    self.clip(self.cur)
                ));
                false
            }
            Op::PointIsInside => {
                let p = d.pos();
                log(&format!(
                    "ev w{i} inside p={},{} oc={}",
                    bits(p[0]),
                    bits(p[1]),
                    u8::from(f != 0.0)
                ));
                let r = if self.probe(w).base_point {
                    hw::point_is_inside(self, w, Vector2f { x: p[0], y: p[1] }, f != 0.0)
                } else {
                    dec(50)
                };
                log(&format!(" ret={}\n", u8::from(r)));
                r
            }
        }
    }

    fn child_begin(&mut self, _w: W) -> usize {
        0
    }
    fn child_end(&mut self, w: W, it: usize) -> bool {
        it >= self.probe(w).kids.len()
    }
    fn child_get(&mut self, w: W, it: usize) -> W {
        self.probe(w).kids.get(it).copied().unwrap_or(0)
    }
    fn child_next(&mut self, _w: W, it: usize) -> usize {
        it + 1
    }
    fn child_erase(&mut self, w: W, it: usize) {
        self.probe(w).kids.remove(it);
    }
    fn child_push(&mut self, w: W, child: W) {
        self.probe(w).kids.push(child);
    }
    fn needs_sorting(&mut self, _w: W) {
        self.sort = true;
    }
    fn virtual_size(&mut self, _w: W) -> Vector2f {
        Vector2f { x: 640.0, y: 480.0 }
    }
    fn clip_child(&mut self, _w: W, clip: usize, pos: Vector3f, size: Vector2f) -> usize {
        let parent = self.regions[clip - 1];
        self.regions
            .push(layout::child_clip_rect(&parent, pos, size));
        self.regions.len()
    }
    fn set_clip(&mut self, _w: W, clip: usize) {
        self.cur = clip;
    }
}

fn h3(v: Vector3f) -> String {
    format!("{},{},{}", bits(v.x), bits(v.y), bits(v.z))
}

fn emit(last: &mut Vec<String>, i: usize, line: String, out: &mut String) {
    if last.len() <= i {
        last.resize(i + 1, String::new());
    }
    if last.get(i).is_some_and(|l| *l == line) {
        return;
    }
    out.push_str(&line);
    out.push('\n');
    if let Some(l) = last.get_mut(i) {
        *l = line;
    }
}

fn state(m: &mut Mock, out: &mut String) {
    for i in 0..m.w.len() {
        let kids: String = m.w[i]
            .kids
            .iter()
            .map(|k| format!("{},", idx(*k)))
            .collect();
        let b = &m.w[i].base;
        let par = if b.parent == 0 { -1 } else { idx(b.parent) };
        let line = format!(
            "s {i} par={par} pos={} co={} sc={} sz={},{} vis={} en={} aff={} clip={} conn={} cbd={} over={} upd={} cnt={} txt={} kids={kids}",
            h3(b.pos),
            h3(b.children_offset),
            h3(b.scroll),
            bits(b.size.x),
            bits(b.size.y),
            u8::from(b.visible),
            u8::from(b.enabled),
            u8::from(b.affected_by_scroll),
            u8::from(b.clips),
            u8::from(b.connected_to_children),
            u8::from(b.callbacks_disabled),
            u8::from(b.mouse_over),
            u8::from(b.position_updated),
            b.position_count,
            m.w[i].text
        );
        emit(&mut m.last_s, i, line, out);
    }
    for i in 0..m.w.len() {
        let g = hw::global_position(m, i + 1);
        emit(&mut m.last_g, i, format!("g {i} {}", h3(g)), out);
    }
    let _ = writeln!(out, "sort={}", u8::from(m.sort));
    m.sort = false;
}

fn hexf(s: &str) -> f32 {
    f32::from_bits(u32::from_str_radix(s, 16).unwrap_or(0))
}

fn num(s: &str) -> i32 {
    s.parse().unwrap_or(0)
}

fn flush_log(out: &mut String) {
    LOG.with(|l| out.push_str(&std::mem::take(&mut *l.borrow_mut())));
}

fn run_op(m: &mut Mock, f: &[&str]) -> i32 {
    let a = f.get(1).map_or(0, |s| num(s)) as usize;
    let wa = a + 1;
    let arg = |k: usize| f.get(k).copied().unwrap_or("0");
    let v3 = |k: usize| Vector3f {
        x: hexf(arg(k)),
        y: hexf(arg(k + 1)),
        z: hexf(arg(k + 2)),
    };
    let flag = num(arg(2)) != 0;
    let b = num(arg(2)) as usize;
    match f[0] {
        "skip" => {}
        "attach" => hw::attach_child(m, wa, b + 1),
        "remove" => hw::remove_child(m, wa, b + 1),
        "setpos" => hw::set_position(m, wa, v3(2)),
        "setgpos" => hw::set_global_position(m, wa, v3(2)),
        "vis" => hw::set_visible(m, wa, flag),
        "en" => hw::set_enabled(m, wa, flag),
        "text" => {
            let t = num(arg(2));
            if m.w[a].text != t {
                m.w[a].text = t;
                hw::text_changed_notify(m, wa);
            }
        }
        "size" => hw::set_size(
            m,
            wa,
            Vector2f {
                x: hexf(arg(2)),
                y: hexf(arg(3)),
            },
        ),
        "choff" => hw::set_children_offset(m, wa, v3(2)),
        "scroll" => hw::set_scroll_amount(m, wa, v3(2)),
        "aff" => hw::set_affected_by_scroll(m, wa, flag),
        "clip" => m.w[a].base.clips = flag,
        "cbd" => m.w[a].base.callbacks_disabled = flag,
        "conn" => m.w[a].base.connected_to_children = flag,
        "msg" => {
            let mut d = Msg::zero();
            d.set_pos([hexf(arg(5)), hexf(arg(6))]);
            d.set_val(num(arg(7)));
            d.set_key(num(arg(8)), num(arg(9)));
            return i32::from(hw::process_message(
                m,
                wa,
                num(arg(2)),
                &d,
                num(arg(3)) != 0,
                num(arg(4)) != 0,
            ));
        }
        "update" => hw::update(m, wa, hexf(arg(2))),
        "draw" => {
            m.cur = 1;
            hw::draw(m, wa, hexf(arg(2)), 1);
            log(&format!("after cur={}\n", m.clip(m.cur)));
            m.cur = 1;
            m.regions.truncate(1);
        }
        "inside" => {
            let p = Vector2f {
                x: hexf(arg(2)),
                y: hexf(arg(3)),
            };
            return i32::from(hw::point_is_inside(m, wa, p, num(arg(4)) != 0));
        }
        "conns" => return i32::from(hw::is_connected_to(m, wa, b + 1, num(arg(3)) != 0)),
        "setnav" => {
            let to = if num(arg(4)) != 0 {
                0
            } else {
                num(arg(3)) as usize + 1
            };
            hw::set_focus_navigation(m, wa, num(arg(2)), to);
        }
        "getnav" => {
            let n = idx(hw::focus_navigation(m, wa, num(arg(2)))) as i32;
            return n * 2 + i32::from(hw::has_focus_navigation(m, wa));
        }
        "focus" => {
            let mut d = Msg::zero();
            d.set_pos([hexf(arg(3)), hexf(arg(4))]);
            let (msg, op) = match num(arg(2)) {
                0 => (6, Op::ChildGotFocus),
                1 => (7, Op::ChildGotTabFocus),
                _ => (8, Op::ChildLostFocus),
            };
            return i32::from(hw::focus_message(m, wa, msg, op, &d));
        }
        "center" => hw::center_global_position_in_set(m, wa),
        "chain" => {
            return i32::from(hw::is_visible(m, wa))
                | (i32::from(hw::is_enabled(m, wa)) << 1)
                | (i32::from(hw::clips_graphics(m, wa)) << 2);
        }
        _ => {}
    }
    -1
}

fn replay(src: &str) -> String {
    let mut out = String::new();
    let mut started = false;
    let mut m = Mock {
        w: Vec::new(),
        regions: vec![Rect2f {
            x: 0.0,
            y: 0.0,
            w: -1.0,
            h: -1.0,
        }],
        cur: 1,
        sort: false,
        last_s: Vec::new(),
        last_g: Vec::new(),
    };
    BASES.with(|b| b.borrow_mut().clear());
    LOG.with(|l| l.borrow_mut().clear());
    for line in src.lines() {
        let f: Vec<&str> = line.split(' ').collect();
        match f[0] {
            "cb" | "w" | "n" | "seed" if started => {}
            "seed" => {
                let seed: u64 = f[1].parse().unwrap_or(0);
                CB.with(|c| *c.borrow_mut() = seed.wrapping_mul(104729).wrapping_add(3));
                out.push_str(line);
                out.push('\n');
            }
            "n" => {
                out.push_str(line);
                out.push('\n');
            }
            "w" => {
                let mut base = Box::new(Base {
                    pos: Vector3f::default(),
                    global: Vector3f::default(),
                    children_offset: Vector3f::default(),
                    scroll: Vector3f::default(),
                    size: Vector2f::default(),
                    parent: 0,
                    focus_nav: [0; 4],
                    callbacks: std::ptr::null_mut(),
                    position_count: 0,
                    affected_by_scroll: true,
                    enabled: true,
                    visible: true,
                    mouse_over: false,
                    clips: false,
                    connected_to_children: true,
                    callbacks_disabled: false,
                    position_updated: true,
                    text_changed: false,
                });
                base.init();
                base.callbacks = hw::callbacks_new();
                let p: *mut Base = &mut *base;
                BASES.with(|b| b.borrow_mut().push((p, false)));
                m.w.push(Probe {
                    base,
                    base_point: f[2] == "base=1",
                    kids: Vec::new(),
                    text: 0,
                });
                out.push_str(line);
                out.push('\n');
            }
            "cb" => {
                let (i, msg) = (num(f[1]) as usize, num(f[2]));
                let p: *mut Base = &mut *m.w[i].base;
                // SAFETY: `p` is the live base of probe `i`.
                unsafe { hw::add_callback_to(p, msg, msg as usize as *mut c_void, callback) };
                out.push_str(line);
                out.push('\n');
            }
            "op" => {
                if !started {
                    started = true;
                    state(&mut m, &mut out);
                }
                out.push_str(line);
                out.push('\n');
                let ret = run_op(&mut m, &f[1..]);
                flush_log(&mut out);
                let _ = writeln!(out, "= ret={ret}");
                state(&mut m, &mut out);
            }
            _ => {}
        }
    }
    for p in &mut m.w {
        // SAFETY: from `callbacks_new`, unused afterwards.
        unsafe { hw::callbacks_free(p.base.callbacks) };
        p.base.callbacks = std::ptr::null_mut();
    }
    out
}

#[test]
fn base_golden_scripts() {
    let dir = Path::new(env!("CARGO_MANIFEST_DIR")).join("tests/golden/H19");
    let mut files: Vec<_> = std::fs::read_dir(&dir)
        .map(|d| d.filter_map(Result::ok).map(|e| e.path()).collect())
        .unwrap_or_default();
    files.sort();
    let mut n = 0;
    for p in files {
        let name = p.file_name().and_then(|s| s.to_str()).unwrap_or("");
        if !name.starts_with("base_") {
            continue;
        }
        let want = std::fs::read_to_string(&p).unwrap_or_default();
        let got = replay(&want);
        if let Ok(dir) = std::env::var("H19_DUMP") {
            let _ = std::fs::write(Path::new(&dir).join(name), &got);
        }
        for (k, (a, b)) in want.lines().zip(got.lines()).enumerate() {
            assert_eq!(a, b, "{name} line {}", k + 1);
        }
        assert_eq!(want.lines().count(), got.lines().count(), "{name} length");
        n += 1;
    }
    assert!(n >= 8, "golden files found: {n}");
}
