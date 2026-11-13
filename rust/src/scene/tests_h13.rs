//! Replay of `rust/tests/golden/H13`, recorded by `HPL2/tests/SceneH13Tests.cpp`:
//! `reg_plan.txt` (world registry sequences, static-combine plans) and `maps.txt` (one parse-trace
//! hash per Amnesia TDD / AMFP `.map`; replayed only when the game data is installed).

use super::h13_world::{BodyItem, DYNAMIC_MESH, EntityRef, MeshItem, Reg, STATIC_MESH};
use super::h13_world::{plan_bodies, plan_meshes};
use crate::binary_buffer::BinaryBuffer;
use crate::hplmap::{
    Builder, CacheBody, CacheMesh, CacheSummary, Decal, Entity, Fog, NodeKind, Options, Parsed,
    Phase, Plane, ShapeBody, StaticObject, Xml, parse_map,
};
use crate::types::{Color, Vector3f};
use crate::xml::{DOC, Doc, Kind, NodeId};

const REG_PLAN: &[u8] = include_bytes!("../../tests/golden/H13/reg_plan.txt");
const MAPS: &str = include_str!("../../tests/golden/H13/maps.txt");

fn lines(data: &[u8]) -> impl Iterator<Item = &[u8]> {
    data.split(|&b| b == b'\n').filter(|l| !l.is_empty())
}

fn words(line: &[u8]) -> Vec<&[u8]> {
    line.split(|&b| b == b' ').collect()
}

fn num(w: &[u8]) -> i64 {
    std::str::from_utf8(w)
        .ok()
        .and_then(|s| s.parse().ok())
        .unwrap_or(i64::MIN)
}

fn ptr(e: i64) -> usize {
    usize::try_from(e + 1).unwrap_or(0)
}

fn ent_of(p: usize) -> i64 {
    i64::try_from(p).map_or(-1, |p| p - 1)
}

struct Pool {
    names: Vec<Vec<u8>>,
    ids: Vec<i32>,
}

fn state(reg: &Reg, k: usize) -> String {
    let n = reg.len(k);
    let mut s = format!("s {k} {n}:");
    for i in 0..n {
        s.push_str(&format!(" {}", ent_of(reg.get(k, i))));
    }
    s
}

fn replay_reg<'a>(it: &mut std::iter::Peekable<impl Iterator<Item = &'a [u8]>>) -> usize {
    let mut pool = Pool {
        names: Vec::new(),
        ids: Vec::new(),
    };
    while let Some(l) = it.peek().copied() {
        if !l.starts_with(b"ent ") {
            break;
        }
        it.next();
        let w = words(l);
        pool.names.push(w[2].to_vec());
        pool.ids.push(num(w[3]) as i32);
    }
    let mut reg = Reg::default();
    let mut ops = 0;
    while let Some(l) = it.peek().copied() {
        if l.starts_with(b"reg ") || l.starts_with(b"walk ") || l.starts_with(b"plan ") {
            break;
        }
        it.next();
        let w = words(l);
        if w.first() == Some(&&b"s"[..]) {
            let k = num(w[1]) as usize;
            assert_eq!(state(&reg, k).as_bytes(), l, "state dump");
            continue;
        }
        let eq = w.iter().position(|x| *x == b"=").expect("result");
        let op = std::str::from_utf8(w[0]).expect("op");
        let k = num(w[1]) as usize;
        let e = num(w[2]);
        let want = num(w[eq + 1]);
        let p = ptr(e);
        let name = |q: usize| pool.names[q - 1].as_slice();
        let uid = |q: usize| pool.ids[q - 1];
        let key = pool.names[e as usize].clone();
        let got: i64 = match op {
            "A" => {
                reg.add(k, p);
                0
            }
            "R" => i64::from(reg.remove_first(k, p)),
            "D" => {
                reg.make_dynamic(p);
                0
            }
            "S" => {
                let set = [p, ptr(num(w[3])), ptr(num(w[4]))];
                reg.remove_set(k, &set);
                0
            }
            "C" => {
                reg.clear(k);
                0
            }
            "N" | "NL" => ent_of(reg.find_name(k, &key, op == "NL", name)),
            "I" => ent_of(reg.find_id(k, pool.ids[e as usize], uid)),
            "X" => i64::from(reg.index_of(k, p)),
            "F" => ent_of(reg.first_by_name(k, name)),
            "T" => i64::from(reg.area_insert(&key, p)),
            "G" => ent_of(reg.area_get(&key)),
            "Z" => {
                reg.area_clear();
                0
            }
            other => panic!("op {other}"),
        };
        assert_eq!(got, want, "{}", String::from_utf8_lossy(l));
        ops += 1;
    }
    ops
}

fn runs(plan: &[(i32, i32)]) -> String {
    plan.iter().map(|(a, b)| format!(" {a}-{b}")).collect()
}

fn replay_plan<'a>(it: &mut std::iter::Peekable<impl Iterator<Item = &'a [u8]>>, n: usize) {
    let mut m = Vec::new();
    let mut b = Vec::new();
    for _ in 0..n {
        let w = words(it.next().expect("item"));
        m.push(MeshItem {
            material: num(w[1]) as usize,
            shadow: num(w[2]) != 0,
            combine: num(w[3]) != 0,
        });
        b.push(BodyItem {
            phys_material: num(w[5]) as usize,
            shadow: num(w[6]) != 0,
            char_collider: num(w[7]) != 0,
            collides: num(w[8]) != 0,
            index_num: num(w[9]) as i32,
        });
    }
    let pm = String::from_utf8_lossy(it.next().expect("pm")).into_owned();
    let pb = String::from_utf8_lossy(it.next().expect("pb")).into_owned();
    assert_eq!(format!("pm{}", runs(&plan_meshes(&m))), pm);
    assert_eq!(format!("pb{}", runs(&plan_bodies(&b, n))), pb);
}

#[test]
fn registry_and_planner_match_cpp_golden() {
    let mut it = lines(REG_PLAN).peekable();
    let (mut seeds, mut ops, mut plans, mut walks) = (0, 0, 0, 0);
    while let Some(l) = it.next() {
        if l.starts_with(b"reg ") {
            seeds += 1;
            ops += replay_reg(&mut it);
        } else if l.starts_with(b"walk ") {
            // Op stream is RNG-driven in the harness (std::list oracle vs Rust cursors in process).
            walks += 1;
            while it
                .next_if(|n| !n.starts_with(b"plan ") && !n.starts_with(b"walk "))
                .is_some()
            {}
        } else if l.starts_with(b"plan ") {
            plans += 1;
            replay_plan(&mut it, num(words(l)[2]) as usize);
        } else {
            panic!("unexpected {}", String::from_utf8_lossy(l));
        }
    }
    assert_eq!((seeds, walks, plans), (6, 6, 400));
    assert!(ops > 2000, "{ops}");
}

#[test]
fn make_dynamic_appends_even_when_not_static() {
    let mut r = Reg::default();
    r.add(STATIC_MESH, 1);
    r.add(STATIC_MESH, 2);
    r.make_dynamic(2);
    r.make_dynamic(9);
    assert_eq!((r.get(STATIC_MESH, 0), r.len(STATIC_MESH)), (1, 1));
    assert_eq!((r.get(DYNAMIC_MESH, 0), r.get(DYNAMIC_MESH, 1)), (2, 9));
}

#[test]
fn area_insert_keeps_first_and_orders_unsigned() {
    let mut r = Reg::default();
    assert!(r.area_insert(b"a", 1));
    assert!(!r.area_insert(b"a", 2));
    assert_eq!(r.area_get(b"a"), 1);
    let names: [&[u8]; 3] = [b"\xe9z", b"z", b"A"];
    r.add(0, 1);
    r.add(0, 2);
    r.add(0, 3);
    assert_eq!(r.first_by_name(0, |p| names[p - 1]), 3);
}

#[test]
fn body_plan_splits_on_index_limit() {
    let it = |idx| BodyItem {
        phys_material: 1,
        shadow: false,
        char_collider: false,
        collides: true,
        index_num: idx,
    };
    let items = [it(30_000), it(30_000), it(10), it(10)];
    assert_eq!(plan_bodies(&items, 4), vec![(0, 1), (2, 3)]);
}

struct Dom(Doc);

impl Xml for Dom {
    type Node = NodeId;
    fn first_child(&self, node: NodeId, name: &[u8]) -> Option<NodeId> {
        self.0.first_child_element(node, Some(name))
    }
    fn children(&self, node: NodeId) -> Vec<NodeId> {
        self.0
            .children(node)
            .filter(|&c| self.0.node(c).kind == Kind::Element)
            .collect()
    }
    fn attr(&self, node: NodeId, name: &[u8]) -> Option<&[u8]> {
        self.0.attribute(node, name)
    }
    fn value(&self, node: NodeId) -> &[u8] {
        &self.0.node(node).value
    }
}

struct Trace<'a> {
    xml: &'a Dom,
    events: u32,
    hash: u64,
}

fn b(v: f32) -> String {
    format!("{:08x}", v.to_bits())
}

fn v3(v: Vector3f) -> String {
    format!("{} {} {}", b(v.x), b(v.y), b(v.z))
}

fn c4(c: Color) -> String {
    format!("{} {} {} {}", b(c.r), b(c.g), b(c.b), b(c.a))
}

impl Trace<'_> {
    fn line(&mut self, s: &[u8]) {
        self.events += 1;
        for &c in s.iter().chain(b"\n") {
            self.hash ^= u64::from(c);
            self.hash = self.hash.wrapping_mul(1_099_511_628_211);
        }
    }

    fn text(&mut self, head: &str, s: &[u8]) {
        let mut l = head.as_bytes().to_vec();
        l.push(b' ');
        l.extend_from_slice(s);
        self.line(&l);
    }

    fn attr(&self, n: NodeId, name: &[u8]) -> Vec<u8> {
        self.xml.attr(n, name).map_or(b"-".to_vec(), <[u8]>::to_vec)
    }

    fn node_line(&mut self, kind: i32, n: NodeId) {
        let mut l = format!("node {kind} ").into_bytes();
        l.extend(self.attr(n, b"Name"));
        l.push(b' ');
        l.extend(self.attr(n, b"ID"));
        self.line(&l);
    }

    fn joined(&mut self, parts: &[&[u8]]) {
        self.line(&parts.join(&b' '));
    }
}

fn i(v: bool) -> String {
    i32::from(v).to_string()
}

impl Builder<NodeId> for Trace<'_> {
    fn log(&mut self, m: &[u8]) {
        self.text("log", m);
    }
    fn warning(&mut self, m: &[u8]) {
        self.text("warn", m);
    }
    fn error(&mut self, m: &[u8]) {
        self.text("err", m);
    }
    fn phase(&mut self, p: Phase, begin: bool) {
        self.line(format!("phase {} {}", p as i32, i(begin)).as_bytes());
    }
    fn set_fog(&mut self, f: Fog) {
        let s = format!(
            "fog {} {} {} {} {} {}",
            i(f.active),
            c4(f.color),
            b(f.falloff_exp),
            b(f.start),
            b(f.end),
            i(f.culling)
        );
        self.line(s.as_bytes());
    }
    fn set_skybox(&mut self, a: bool, c: Color, t: &[u8]) {
        self.joined(&[b"sky", i(a).as_bytes(), c4(c).as_bytes(), t]);
    }
    fn static_begin(&mut self) {
        self.line(b"sbegin");
    }
    fn static_object(&mut self, o: &StaticObject) {
        let tail = format!(
            "{} {} {} {} {} {}",
            v3(o.at.pos),
            v3(o.at.rot),
            v3(o.at.scale),
            i(o.collides),
            i(o.cast_shadows),
            o.id
        );
        self.joined(&[b"obj", o.name, o.file, tail.as_bytes()]);
    }
    fn static_plane(&mut self, p: &Plane) {
        let head = format!("{} {} {}", i(p.cast_shadows), i(p.collides), p.id);
        let mut tail = format!(
            "{} {} {} {} {}",
            v3(p.at.pos),
            v3(p.at.scale),
            v3(p.at.rot),
            v3(p.start_corner),
            v3(p.end_corner)
        );
        for uv in p.uv {
            tail.push_str(&format!(" {} {}", b(uv.x), b(uv.y)));
        }
        self.joined(&[
            b"plane",
            p.name,
            p.material,
            head.as_bytes(),
            tail.as_bytes(),
        ]);
    }
    fn static_decal(&mut self, d: &Decal<NodeId>) {
        let a = format!("{} {}", c4(d.color), d.id);
        self.joined(&[
            b"decal",
            d.name,
            a.as_bytes(),
            d.material,
            i(d.mesh.is_some()).as_bytes(),
        ]);
    }
    fn static_loading_done(&mut self) {
        self.line(b"sdone");
    }
    fn static_combo(&mut self, g: i32, ids: &[i32]) {
        let mut s = format!("combo {g}");
        for id in ids {
            s.push_str(&format!(" {id}"));
        }
        self.line(s.as_bytes());
    }
    fn static_compile(&mut self) {
        self.line(b"scompile");
    }
    fn entities_begin(&mut self) {
        self.line(b"ebegin");
    }
    fn entities_end(&mut self) {
        self.line(b"eend");
    }
    fn entity(&mut self, e: &Entity<NodeId>) {
        self.entity_line("ent", e);
    }
    fn area(&mut self, e: &Entity<NodeId>) {
        self.entity_line("area", e);
    }
    fn node(&mut self, kind: NodeKind, n: NodeId) {
        self.node_line(kind as i32, n);
    }
    fn cache_header(&mut self, _: CacheSummary) {}
    fn cache_mesh_body(&mut self, _: &CacheBody, _: *mut BinaryBuffer) {}
    fn cache_shape_body(&mut self, _: &ShapeBody) {}
    fn cache_mesh(&mut self, _: &CacheMesh) {}
}

impl Trace<'_> {
    fn entity_line(&mut self, head: &str, e: &Entity<NodeId>) {
        let a = format!("{} {}", e.id, i(e.active));
        let t = format!("{} {} {}", v3(e.at.pos), v3(e.at.rot), v3(e.at.scale));
        self.joined(&[
            head.as_bytes(),
            e.name,
            a.as_bytes(),
            t.as_bytes(),
            e.file,
            i(e.user_vars.is_some()).as_bytes(),
        ]);
    }
}

#[test]
fn map_parse_traces_match_cpp_golden() {
    let Some(home) = std::env::var_os("HOME") else {
        return;
    };
    let root = std::path::Path::new(&home).join(".local/share/Steam/steamapps/common");
    let (mut checked, mut missing) = (0, 0);
    for l in MAPS.lines() {
        let w: Vec<&str> = l.split(' ').collect();
        let tail = &w[w.len() - 3..];
        let rel = w[1..w.len() - 3].join(" ");
        let Ok(data) = std::fs::read(root.join(&rel)) else {
            missing += 1;
            continue;
        };
        let mut doc = Doc::new();
        doc.parse(&data);
        assert!(!doc.error(), "{rel}");
        let dom = Dom(doc);
        let mut t = Trace {
            xml: &dom,
            events: 0,
            hash: 14_695_981_039_346_656_037,
        };
        let opts = Options {
            flags: 0,
            log_cache: false,
            file: b"x.map",
        };
        // The C++ document object is the root element itself.
        let root_el = dom.0.first_child_element(DOC, None).expect("root element");
        let res = match parse_map(&dom, root_el, &mut t, &opts, false) {
            Parsed::Ok => 0,
            Parsed::NoContents => 1,
            Parsed::Abort => 2,
        };
        let got = [
            format!("res={res}"),
            format!("events={}", t.events),
            format!("hash={:016x}", t.hash),
        ];
        assert_eq!(got.as_slice(), tail, "{rel}");
        checked += 1;
    }
    eprintln!("maps checked {checked}, data missing {missing}");
}

#[test]
fn cursor_walk_matches_std_list_with_erase_and_append() {
    let mut reg = Reg::default();
    for p in 1..=5usize {
        reg.add(1, p);
    }
    let mut seen = Vec::new();
    let mut at = reg.begin(1);
    let mut appended = false;
    while let Some(c) = at {
        let p = reg.resolve(c);
        seen.push(p);
        if p == 2 {
            at = reg.erase(c);
            continue;
        }
        if p == 4 && !appended {
            reg.add(1, 6);
            appended = true;
        }
        at = reg.next(c);
    }
    assert_eq!(seen, [1, 2, 3, 4, 5, 6]);
    assert_eq!(reg.snapshot(1), [1, 3, 4, 5, 6]);
    let c = reg.begin(1).expect("first");
    assert_eq!(EntityRef::from_bits(1, c.to_bits()), Some(c));
    assert_eq!(EntityRef::from_bits(1, 0), None);
    reg.clear(1);
    assert_eq!(reg.resolve(c), 0);
}

thread_local! {
    static DROPS: std::cell::RefCell<Vec<usize>> = const { std::cell::RefCell::new(Vec::new()) };
}

unsafe extern "C" fn record_drop(p: *mut std::ffi::c_void) {
    DROPS.with(|d| d.borrow_mut().push(p as usize));
}

#[test]
fn destroy_runs_foreign_drop_in_creation_order() {
    use super::ffi_h13::{
        ohpl_h13_reg_add, ohpl_h13_reg_destroy, ohpl_h13_reg_destroy_all, ohpl_h13_reg_free,
        ohpl_h13_reg_len, ohpl_h13_reg_new, ohpl_h13_reg_set_foreign,
    };
    DROPS.with(|d| d.borrow_mut().clear());
    let reg = ohpl_h13_reg_new();
    // SAFETY: `reg` is live until freed below; entities are opaque addresses.
    unsafe {
        ohpl_h13_reg_set_foreign(reg, 9, None, None, Some(record_drop));
        for p in [10usize, 20, 30] {
            ohpl_h13_reg_add(reg, 9, p as *mut _);
        }
        assert!(ohpl_h13_reg_destroy(reg, 9, 20 as *mut _, true));
        assert!(!ohpl_h13_reg_destroy(reg, 9, 99 as *mut _, true));
        assert!(!ohpl_h13_reg_destroy(reg, 9, 98 as *mut _, false));
        ohpl_h13_reg_destroy_all(reg, 9);
        assert_eq!(ohpl_h13_reg_len(reg, 9), 0);
        ohpl_h13_reg_free(reg);
    }
    DROPS.with(|d| assert_eq!(*d.borrow(), [20, 98, 10, 30]));
}
