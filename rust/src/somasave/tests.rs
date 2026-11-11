use super::base::{self, CfgHost, ConfigVals};
use super::codec::{In, Out};
use super::header::{self, HeaderOut, LoadStatus};
use super::names::{self, Date, NameState, NamesHost};
use super::rec::*;
use super::script::{self, Addr, Kind, ScriptHost};
use super::world::{self, ReadHost, WriteHost};
use std::collections::HashSet;

fn rng(seed: &mut u64) -> u64 {
    *seed ^= *seed << 13;
    *seed ^= *seed >> 7;
    *seed ^= *seed << 17;
    *seed
}

#[test]
fn codec_failures_keep_cursor() {
    let mut o = Out::default();
    o.u32(7);
    o.str(b"ab");
    let mut i = In::new(&o.s);
    assert_eq!(i.u32(), 7);
    assert_eq!(i.str(), b"ab");
    assert!(i.ok && !i.left());
    assert_eq!(i.u32(), 0);
    assert!(!i.ok);
    let mut j = In::new(&[3, 0, 0, 0, b'x']);
    assert_eq!(j.str(), b"");
    assert!(!j.ok);
    assert_eq!(j.p, 4);
}

#[test]
fn skip_object_unchecked_length() {
    let mut o = Out::default();
    o.u32(1);
    o.str(b"n");
    o.str(b"d");
    o.u32(1000);
    let mut i = In::new(&o.s);
    i.skip_object();
    assert!(i.ok);
    assert!(i.p > o.s.len());
    assert_eq!(i.u32(), 0);
    assert!(!i.ok);
}

#[derive(Clone)]
enum Val {
    Bytes(Vec<u8>),
    Str(Vec<u8>),
    WStr(Vec<u32>),
    Array(i32, Vec<Addr>),
    Set(Vec<Vec<u8>>),
    Handle(Option<Addr>),
    Obj(Vec<u8>, Vec<(Vec<u8>, i32, bool, Addr)>),
}

const T_INT: i32 = 4;
const T_FLOAT: i32 = 10;
const T_ENUM: i32 = 100;
const T_STR: i32 = 101;
const T_WSTR: i32 = 102;
const T_ARR_INT: i32 = 103;
const T_SET: i32 = 104;
const T_RAW: i32 = 105;
const T_INNER: i32 = 106;
const T_HANDLE: i32 = 107 | script::TYPEID_OBJHANDLE;
const T_ARR_STR: i32 = 108;

#[derive(Default)]
struct Fake {
    vals: Vec<Val>,
}

impl Fake {
    fn add(&mut self, v: Val) -> Addr {
        self.vals.push(v);
        self.vals.len()
    }

    fn get(&self, a: Addr) -> &Val {
        &self.vals[a - 1]
    }

    fn get_mut(&mut self, a: Addr) -> &mut Val {
        &mut self.vals[a - 1]
    }

    fn new_value(&mut self, tid: i32) -> Addr {
        let v = match tid {
            T_INT | T_FLOAT | T_ENUM => Val::Bytes(vec![0; 4]),
            T_RAW => Val::Bytes(vec![0; 12]),
            T_STR => Val::Str(vec![]),
            T_WSTR => Val::WStr(vec![]),
            T_ARR_INT => Val::Array(T_INT, vec![]),
            T_ARR_STR => Val::Array(T_STR, vec![]),
            T_SET => Val::Set(vec![]),
            T_HANDLE => Val::Handle(None),
            T_INNER => return self.new_obj(b"Inner"),
            _ => Val::Bytes(vec![]),
        };
        self.add(v)
    }

    fn new_obj(&mut self, class: &[u8]) -> Addr {
        let props: Vec<(&[u8], i32, bool)> = if class == b"Inner" {
            vec![
                (b"x", T_INT, false),
                (b"skip", T_INT, true),
                (b"s", T_STR, false),
            ]
        } else {
            vec![
                (b"i", T_INT, false),
                (b"f", T_FLOAT, false),
                (b"e", T_ENUM, false),
                (b"s", T_STR, false),
                (b"w", T_WSTR, false),
                (b"a", T_ARR_INT, false),
                (b"set", T_SET, false),
                (b"raw", T_RAW, false),
                (b"inner", T_INNER, false),
                (b"h", T_HANDLE, false),
                (b"as", T_ARR_STR, false),
                (b"nosave", T_INT, true),
            ]
        };
        let mut ps = Vec::new();
        for (n, t, ns) in props {
            let a = self.new_value(t);
            ps.push((n.to_vec(), t, ns, a));
        }
        self.add(Val::Obj(class.to_vec(), ps))
    }

    fn fill(&mut self, a: Addr, tid: i32, seed: &mut u64) {
        let r = rng(seed);
        let bytes = |s: &mut u64, n: usize| (0..n).map(|_| rng(s) as u8).collect::<Vec<_>>();
        match tid {
            T_INT | T_FLOAT | T_ENUM => *self.get_mut(a) = Val::Bytes(bytes(seed, 4)),
            T_RAW => *self.get_mut(a) = Val::Bytes(bytes(seed, 12)),
            T_STR => *self.get_mut(a) = Val::Str(bytes(seed, (r % 9) as usize)),
            T_WSTR => *self.get_mut(a) = Val::WStr((0..r % 5).map(|_| rng(seed) as u32).collect()),
            T_ARR_INT => {
                let items: Vec<Addr> = (0..r % 6)
                    .map(|_| self.add(Val::Bytes(vec![0; 4])))
                    .collect();
                for &x in &items {
                    self.fill(x, T_INT, seed);
                }
                *self.get_mut(a) = Val::Array(T_INT, items);
            }
            T_ARR_STR => {
                let items: Vec<Addr> = (0..r % 4).map(|_| self.add(Val::Str(vec![]))).collect();
                for &x in &items {
                    self.fill(x, T_STR, seed);
                }
                *self.get_mut(a) = Val::Array(T_STR, items);
            }
            T_SET => {
                let mut v: Vec<Vec<u8>> = (0..r % 4).map(|_| bytes(seed, 3)).collect();
                v.sort();
                *self.get_mut(a) = Val::Set(v);
            }
            T_INNER => self.fill_obj(a, seed),
            _ => {}
        }
    }

    fn fill_obj(&mut self, obj: Addr, seed: &mut u64) {
        let Val::Obj(_, props) = self.get(obj).clone() else {
            return;
        };
        for (_, t, _, a) in props {
            if t == T_HANDLE {
                let inner = self.new_obj(b"Inner");
                self.fill_obj(inner, seed);
                *self.get_mut(a) = Val::Handle(Some(inner));
            } else {
                self.fill(a, t, seed);
            }
        }
    }
}

impl ScriptHost for Fake {
    fn handle_obj(&mut self, addr: Addr, _t: i32) -> Option<Addr> {
        match self.get(addr) {
            Val::Handle(h) => *h,
            _ => None,
        }
    }

    fn resolve_handle(&mut self, addr: Addr, _t: i32, class: &[u8]) -> Option<Addr> {
        if class != b"Inner" {
            return None;
        }
        if matches!(self.get(addr), Val::Handle(None)) {
            let o = self.new_obj(b"Inner");
            *self.get_mut(addr) = Val::Handle(Some(o));
        }
        match self.get(addr) {
            Val::Handle(Some(o)) => Some(*o),
            _ => None,
        }
    }

    fn obj_class(&mut self, obj: Addr) -> Vec<u8> {
        match self.get(obj) {
            Val::Obj(c, _) => c.clone(),
            _ => vec![],
        }
    }

    fn prim_size(&mut self, t: i32) -> usize {
        if t == T_FLOAT || t == T_INT { 4 } else { 0 }
    }

    fn kind(&mut self, t: i32) -> Kind {
        match t {
            T_ENUM => Kind::Enum,
            T_INNER => Kind::ScriptObj,
            T_STR => Kind::Str,
            T_WSTR => Kind::WStr,
            T_ARR_INT | T_ARR_STR => Kind::Array,
            T_SET => Kind::StrSet,
            T_RAW => Kind::Raw(12),
            _ => Kind::None,
        }
    }

    fn read_mem(&mut self, addr: Addr, n: usize) -> Vec<u8> {
        match self.get(addr) {
            Val::Bytes(b) => b.get(..n).unwrap_or_default().to_vec(),
            _ => vec![],
        }
    }

    fn write_mem(&mut self, addr: Addr, d: &[u8]) {
        *self.get_mut(addr) = Val::Bytes(d.to_vec());
    }

    fn string_get(&mut self, a: Addr) -> Vec<u8> {
        match self.get(a) {
            Val::Str(s) => s.clone(),
            _ => vec![],
        }
    }

    fn string_set(&mut self, a: Addr, s: &[u8]) {
        *self.get_mut(a) = Val::Str(s.to_vec());
    }

    fn wstring_get(&mut self, a: Addr) -> Vec<u32> {
        match self.get(a) {
            Val::WStr(s) => s.clone(),
            _ => vec![],
        }
    }

    fn wstring_set(&mut self, a: Addr, s: &[u32]) {
        *self.get_mut(a) = Val::WStr(s.to_vec());
    }

    fn array_len(&mut self, a: Addr) -> u32 {
        match self.get(a) {
            Val::Array(_, v) => v.len() as u32,
            _ => 0,
        }
    }

    fn array_elem_type(&mut self, a: Addr) -> i32 {
        match self.get(a) {
            Val::Array(t, _) => *t,
            _ => 0,
        }
    }

    fn array_at(&mut self, a: Addr, i: u32) -> Addr {
        match self.get(a) {
            Val::Array(_, v) => v[i as usize],
            _ => 0,
        }
    }

    fn array_resize(&mut self, a: Addr, n: u32) {
        let Val::Array(t, v) = self.get(a).clone() else {
            return;
        };
        let mut v = v;
        v.truncate(n as usize);
        while v.len() < n as usize {
            let x = self.new_value(t);
            v.push(x);
        }
        *self.get_mut(a) = Val::Array(t, v);
    }

    fn set_items(&mut self, a: Addr) -> Vec<Vec<u8>> {
        match self.get(a) {
            Val::Set(v) => v.clone(),
            _ => vec![],
        }
    }

    fn set_clear(&mut self, a: Addr) {
        *self.get_mut(a) = Val::Set(vec![]);
    }

    fn set_insert(&mut self, a: Addr, s: &[u8]) {
        if let Val::Set(v) = self.get_mut(a) {
            v.push(s.to_vec());
            v.sort();
        }
    }

    fn prop_count(&mut self, o: Addr) -> u32 {
        match self.get(o) {
            Val::Obj(_, p) => p.len() as u32,
            _ => 0,
        }
    }

    fn prop_saved(&mut self, o: Addr, i: u32) -> bool {
        match self.get(o) {
            Val::Obj(_, p) => !p[i as usize].2,
            _ => false,
        }
    }

    fn prop_name(&mut self, o: Addr, i: u32) -> Vec<u8> {
        match self.get(o) {
            Val::Obj(_, p) => p[i as usize].0.clone(),
            _ => vec![],
        }
    }

    fn prop_type_id(&mut self, o: Addr, i: u32) -> i32 {
        match self.get(o) {
            Val::Obj(_, p) => p[i as usize].1,
            _ => 0,
        }
    }

    fn prop_addr(&mut self, o: Addr, i: u32) -> Addr {
        match self.get(o) {
            Val::Obj(_, p) => p[i as usize].3,
            _ => 0,
        }
    }

    fn type_decl(&mut self, _o: Addr, t: i32) -> Vec<u8> {
        format!("T{t}").into_bytes()
    }

    fn skipped_script(&mut self, class: &[u8], body: &[u8]) {
        if std::env::var_os("OHPL_GS4_DUMP").is_some() {
            eprintln!("SCRIPT {}", String::from_utf8_lossy(class));
            let mut i = In::new(body);
            let n = i.u32();
            let mut k = 0;
            while k < n && i.ok {
                let (name, decl, len) = (i.str(), i.str(), i.u32());
                eprintln!(
                    "PROP {} {} {} {}",
                    String::from_utf8_lossy(class),
                    String::from_utf8_lossy(name),
                    String::from_utf8_lossy(decl),
                    len
                );
                i.p += len as usize;
                k += 1;
            }
        }
    }
}

fn script_roundtrip(seed: u64) {
    let mut seed = seed | 1;
    let mut f = Fake::default();
    let a = f.new_obj(b"Outer");
    f.fill_obj(a, &mut seed);
    let mut o = Out::default();
    script::write_script(&mut f, &mut o, Some(a));
    let b = f.new_obj(b"Outer");
    let mut i = In::new(&o.s);
    script::read_script(&mut f, &mut i, 31, Some(b));
    assert!(i.ok && !i.left());
    let mut o2 = Out::default();
    script::write_script(&mut f, &mut o2, Some(b));
    assert_eq!(o.s, o2.s);
    let mut skip = In::new(&o.s);
    script::read_script(&mut f, &mut skip, 31, None);
    assert!(skip.ok && !skip.left());
}

#[test]
fn script_roundtrip_random() {
    for s in 1..200u64 {
        script_roundtrip(s * 0x9E37_79B9);
    }
}

#[test]
fn script_known_bytes_and_cycles() {
    let mut f = Fake::default();
    let inner = f.new_obj(b"Inner");
    if let Val::Obj(_, p) = f.get(inner).clone() {
        *f.get_mut(p[0].3) = Val::Bytes(7i32.to_ne_bytes().to_vec());
        *f.get_mut(p[2].3) = Val::Str(b"hi".to_vec());
    }
    let mut o = Out::default();
    script::write_script(&mut f, &mut o, Some(inner));
    let mut want = vec![1u8];
    want.extend(5u32.to_ne_bytes());
    want.extend(b"Inner");
    want.extend(2u32.to_ne_bytes());
    let mut p0 = Vec::new();
    p0.extend(1u32.to_ne_bytes());
    p0.extend(b"x");
    p0.extend(2u32.to_ne_bytes());
    p0.extend(b"T4");
    p0.extend(4u32.to_ne_bytes());
    p0.extend(7i32.to_ne_bytes());
    want.extend(p0);
    want.extend(1u32.to_ne_bytes());
    want.extend(b"s");
    want.extend(4u32.to_ne_bytes());
    want.extend(b"T101");
    want.extend(6u32.to_ne_bytes());
    want.extend(2u32.to_ne_bytes());
    want.extend(b"hi");
    assert_eq!(o.s, want);
    let mut visited = HashSet::new();
    visited.insert(inner);
    let mut w = Out::default();
    let h = f.add(Val::Handle(Some(inner)));
    script::write_value(&mut f, &mut w, T_HANDLE, h, &mut visited);
    assert_eq!(w.s, [0]);
}

struct Names;

impl NamesHost for Names {
    fn translate_level(&mut self, key: &[u8]) -> Vec<u32> {
        let mut v: Vec<u32> = "L:".bytes().map(u32::from).collect();
        v.extend(key.iter().map(|&c| u32::from(c)));
        v
    }

    fn local_mtime(&mut self, _f: &[u32]) -> [i32; 6] {
        [3, 4, 2026, 5, 6, 7]
    }
}

fn w(s: &str) -> Vec<u32> {
    s.bytes().map(u32::from).collect()
}

#[test]
fn save_names() {
    let mut st = NameState::default();
    let d = Date {
        seconds: 9,
        minutes: 8,
        hours: 7,
        month_day: 6,
        month: 10,
        year: 2026,
    };
    let n1 = names::save_name(&mut st, &w("AutoSave"), b"", &d, &[]);
    assert_eq!(n1, w("AutoSave_NULL_2026_11_6_7_8_9_0.sav"));
    let n2 = names::save_name(&mut st, &w("AutoSave"), b"map", &d, &w("sfx"));
    assert_eq!(n2, w("AutoSave_map_2026_11_6_7_8_9_0_sfx.sav"));
    assert_eq!(st.count, 1);
    let n3 = names::save_name(&mut st, &w("A"), b"m", &d, &[]);
    assert_eq!(n3, w("A_m_2026_11_6_7_8_9_1.sav"));
    let d2 = Date { seconds: 10, ..d };
    names::save_name(&mut st, &w("A"), b"m", &d2, &[]);
    assert_eq!(st.count, 0);
}

#[test]
fn proper_names() {
    let mut h = Names;
    let (n, d, s) = names::proper_name(&mut h, &w("AutoSave_02-03_2026_11_9_22_4_1_0_x.sav"));
    assert_eq!(n, w("AutoSave - L:02-03 - "));
    assert_eq!(d, b"9/11-2026 22:04:01");
    assert_eq!(s, w("x"));
    let (n, d, s) = names::proper_name(&mut h, &w("CheckPoint.sav"));
    assert_eq!(n, w("CheckPoint"));
    assert_eq!(d, b"3/4/2026 5:06:07");
    assert!(s.is_empty());
}

#[test]
fn list_order_and_rotation() {
    let mut h = Names;
    let mk = |y| Date {
        year: y,
        ..Date::default()
    };
    let files = vec![
        (w("a.sav"), mk(1)),
        (w("b.sav"), mk(3)),
        (w("c.sav"), mk(2)),
        (w("d.sav"), mk(3)),
    ];
    let l = names::list_saves(&mut h, &files);
    let years: Vec<i32> = l.iter().map(|s| s.date.year).collect();
    assert_eq!(years, [3, 3, 2, 1]);
    let sufs: Vec<&[u32]> = vec![&[], &[1], &[], &[]];
    assert_eq!(names::rotation_victims(&sufs, &[], 2), vec![2, 3]);
    assert_eq!(names::rotation_victims(&sufs, &[], 1), vec![0, 2, 3]);
}

struct Cfg;

impl CfgHost for Cfg {
    fn get_float(&mut self, l: &str, k: &str, d: f32) -> f32 {
        if (l, k) == ("Sound", "Volume") {
            0.5
        } else {
            d
        }
    }
    fn get_bool(&mut self, l: &str, k: &str, d: bool) -> bool {
        if (l, k) == ("Screen", "Vsync") {
            true
        } else {
            d
        }
    }
    fn get_int(&mut self, l: &str, k: &str, d: i32) -> i32 {
        if (l, k) == ("Screen", "Width") {
            1920
        } else {
            d
        }
    }
}

#[test]
fn base_decisions() {
    assert_eq!(base::game_id(b"x Rebirth Bunker"), "rebirth");
    assert_eq!(base::game_id(b"Bunker"), "bunker");
    assert_eq!(base::game_id(b""), "soma");
    let mut c = ConfigVals::default();
    base::load_config(&mut Cfg, &mut c);
    assert_eq!(
        (c.master_volume, c.vsync, c.screen_width, c.screen_height),
        (0.5, true, 1920, 720)
    );
    assert_eq!(base::init_config_file(b""), w("config/main_init.cfg"));
    assert_eq!(base::init_config_file(b"x.cfg"), w("x.cfg"));
    let (dirs, f) = base::log_dirs(&w("/s/"), "bunker", true, 42);
    assert_eq!(dirs, vec![w("/s/open-hpl/"), w("/s/open-hpl/bunker/")]);
    assert_eq!(f, w("/s/open-hpl/bunker/hpl-42.log"));
    assert_eq!(base::save_dirs(&w("/d/"), "soma").len(), 3);
    assert!(base::use_real_player(Some(b"0")) && !base::use_real_player(Some(b"1")));
    assert_eq!(
        base::boot_map(Some(b"m.hpm"), None),
        Some((b"m.hpm".to_vec(), b"*".to_vec()))
    );
    assert_eq!(base::boot_map(Some(b""), None), None);
    assert_eq!(base::engine_plan("bunker").gbuffer_textures, Some(5));
    assert_eq!(base::main_plan(None, None, None), base::MainPlan::Engine);
}

#[test]
fn header_roundtrip_and_errors() {
    let mut o = Out::default();
    let visited: [&[u8]; 2] = [b"a.hpm", b"b.hpm"];
    header::write_header(
        &mut o,
        &HeaderOut {
            map: b"m.hpm",
            start_pos: b"p",
            visited: &visited,
            vars: b"v",
            exploration: true,
            preload: b"pre",
            played: &[b"l1"],
        },
    );
    o.u8(0xAB);
    let (st, h) = header::read_header(&o.s, true);
    assert_eq!(st, LoadStatus::Ok);
    assert_eq!(
        (h.version, h.map.as_slice(), h.preload.as_slice()),
        (31, &b"m.hpm"[..], &b"pre"[..])
    );
    assert_eq!(h.visited.len(), 2);
    assert_eq!(o.s.get(h.body), Some(&0xAB));
    assert_eq!(header::read_header(&o.s, false).0, LoadStatus::Unreadable);
    assert_eq!(
        header::read_header(&o.s[..20], true).0,
        LoadStatus::Rejected
    );
    let mut bad = o.s.clone();
    bad[7] = b'0' + 32;
    assert_eq!(header::read_header(&bad, true).0, LoadStatus::Unreadable);
    assert_eq!(header::read_header(&[], true).0, LoadStatus::Unreadable);
}

#[derive(Default)]
struct Log {
    ctx: &'static str,
    entities: Vec<Vec<u8>>,
    ents_done: usize,
    lights: usize,
    particles: usize,
    sounds: usize,
    agents: usize,
    has_list: Vec<bool>,
    health: Option<f32>,
    display: Vec<u8>,
    camera: bool,
    tone: bool,
    player: bool,
    fog: bool,
    underwater: Option<bool>,
    timers: usize,
    spawned: usize,
    others: usize,
    cam_tex: usize,
    fog_areas: usize,
    colliding: usize,
    owned: usize,
    billboards: usize,
    tail_vars: usize,
    anims: usize,
    bodies: usize,
    joints: usize,
    scripts: Fake,
}

impl ReadHost for Log {
    fn script(&mut self) -> &mut dyn ScriptHost {
        &mut self.scripts
    }
    fn has_player(&mut self) -> bool {
        true
    }
    fn has_camera(&mut self) -> bool {
        true
    }
    fn map_timers_begin(&mut self) {}
    fn map_timer(&mut self, _t: &MapTimer) {
        self.timers += 1;
    }
    fn map_script(&mut self) -> Option<Addr> {
        self.ctx = "MAP";
        None
    }
    fn spawned(&mut self, _s: &Spawned) {
        self.spawned += 1;
    }
    fn entity_begin(&mut self, name: &[u8]) -> usize {
        if std::env::var_os("OHPL_GS4_DUMP").is_some() {
            eprintln!(
                "ENT {}",
                name.iter().map(|b| format!("{b:02x}")).collect::<String>()
            );
        }
        self.entities.push(name.to_vec());
        self.entities.len()
    }
    fn entity_head(&mut self, _h: usize, e: &EntityHead) {
        if std::env::var_os("OHPL_GS4_DUMP").is_some() {
            eprintln!("ENTSHAPE {} {}", e.bodies.get().len(), e.joints.get().len());
        }
        self.bodies += e.bodies.get().len();
        self.joints += e.joints.get().len();
    }
    fn entity_script(&mut self, _h: usize) -> Option<Addr> {
        self.ctx = "ENTSCRIPT";
        None
    }
    fn entity_tail(&mut self, _h: usize, _e: &EntityHead, t: &EntityTail) {
        self.tail_vars += t.vars.get().len();
        self.anims += t.anims.get().len();
    }
    fn entities_done(&mut self, saved: &[Str]) {
        self.ents_done = saved.len();
    }
    fn other_begin(&mut self, k: &[u8]) -> usize {
        if std::env::var_os("OHPL_GS4_DUMP").is_some() {
            eprintln!(
                "OTHER {}",
                k.iter().map(|b| format!("{b:02x}")).collect::<String>()
            );
        }
        self.others += 1;
        0
    }
    fn other_timers(&mut self, _h: usize, _t: &Timers) {}
    fn other_reset(&mut self, _h: usize) {}
    fn other_script(&mut self, _h: usize) -> Option<Addr> {
        self.ctx = "OTHERSCRIPT";
        None
    }
    fn light(&mut self, _l: &Light) {
        self.lights += 1;
    }
    fn particles_begin(&mut self) {}
    fn particle(&mut self, _p: &Particle) {
        self.particles += 1;
    }
    fn particles_end(&mut self, h: bool) {
        self.has_list.push(h);
    }
    fn sounds_begin(&mut self) {}
    fn sound(&mut self, _s: &Sound) {
        self.sounds += 1;
    }
    fn sounds_end(&mut self, h: bool) {
        self.has_list.push(h);
    }
    fn billboard(&mut self, _b: &Billboard) {
        self.billboards += 1;
    }
    fn player_apply(&mut self, _p: &Player) {
        self.player = true;
    }
    fn health(&mut self, h: f32) {
        self.health = Some(h);
    }
    fn agent(&mut self, _a: &Agent) {
        self.agents += 1;
    }
    fn underwater(&mut self, b: bool) {
        self.underwater = Some(b);
    }
    fn fog(&mut self, _f: &Fog) {
        self.fog = true;
    }
    fn cam_texture(&mut self, _c: &CamTex) {
        self.cam_tex += 1;
    }
    fn fog_area(&mut self, _f: &FogArea) {
        self.fog_areas += 1;
    }
    fn colliding(&mut self, _c: &Colliding) {
        self.colliding += 1;
    }
    fn display_name(&mut self, n: &[u8]) {
        self.display = n.to_vec();
    }
    fn camera(&mut self, _c: &Camera) {
        self.camera = true;
    }
    fn owned_light(&mut self, _l: &OwnedLight) {
        self.owned += 1;
    }
    fn tone(&mut self, _t: &Tone) {
        self.tone = true;
    }
}

struct EntSrc {
    heads: Vec<EntityHead>,
    tails: Vec<EntityTail>,
}

impl WriteHost for EntSrc {
    fn map_script(&mut self, o: &mut Out) {
        o.u8(0);
    }
    fn entity_head(&mut self, i: usize) -> EntityHead {
        self.heads[i]
    }
    fn entity_script(&mut self, _i: usize, o: &mut Out) {
        o.u8(0);
    }
    fn entity_tail(&mut self, i: usize) -> EntityTail {
        self.tails[i]
    }
    fn other_key(&mut self, _i: usize) -> Str {
        Str::new(b"key|Cls")
    }
    fn other_timers(&mut self, _i: usize) -> Timers {
        Timers {
            timers: Slice::EMPTY,
            faders: Slice::EMPTY,
        }
    }
    fn other_script(&mut self, _i: usize, o: &mut Out) {
        o.u8(0);
    }
}

fn zero<T>() -> T {
    // SAFETY: only used for the all-zero POD records below (null pointers, zero lengths).
    unsafe { std::mem::zeroed() }
}

#[test]
fn world_roundtrip_counts() {
    let names: Vec<Vec<u8>> = (0..5).map(|i| format!("ent{i}").into_bytes()).collect();
    let bodies = [Body {
        mtx: [1.0; 16],
        mass: 2.0,
        active: true,
        gravity: true,
    }; 2];
    let vars = [VarPair {
        key: Str::new(b"k"),
        value: Str::new(b"\xff\xfe"),
    }];
    let anims = [zero::<Anim>(); 3];
    let mut src = EntSrc {
        heads: vec![],
        tails: vec![],
    };
    for n in &names {
        let mut h = zero::<EntityHead>();
        h.name = Str::new(n);
        h.bodies = Slice::new(&bodies);
        let mut t = zero::<EntityTail>();
        t.vars = Slice::new(&vars);
        t.anims = Slice::new(&anims);
        src.heads.push(h);
        src.tails.push(t);
    }
    let lights = [zero::<Light>(); 3];
    let mut wd = zero::<World>();
    wd.player.present = 1;
    wd.player.gravity_active = 1;
    wd.entity_count = 5;
    wd.other_count = 2;
    wd.lights = Slice::new(&lights);
    wd.health = 0.5;
    wd.has_camera = true;
    wd.has_tone = true;
    wd.underwater = true;
    wd.display_name = Str::new(b"disp");
    let mut o = Out::default();
    world::write_world(&mut o, &wd, &mut src);
    for cut in 0..o.s.len() {
        let mut l = Log::default();
        let mut i = In::new(&o.s[..cut]);
        world::read_world(&mut i, 31, &mut l);
        assert!(!i.ok || cut >= o.s.len(), "cut {cut}");
    }
    let mut l = Log::default();
    let mut i = In::new(&o.s);
    world::read_world(&mut i, 31, &mut l);
    assert!(i.ok && !i.left());
    assert_eq!(l.entities.len(), 5);
    assert_eq!(l.ents_done, 5);
    assert_eq!((l.bodies, l.tail_vars, l.anims), (10, 5, 15));
    assert_eq!((l.lights, l.others), (3, 2));
    assert_eq!(l.health, Some(0.5));
    assert_eq!(l.underwater, Some(true));
    assert!(l.camera && l.tone && l.player && l.fog);
    assert_eq!(l.display, b"disp");
    assert_eq!(l.has_list, [true, true]);
}

/// `OHPL_GS4_SAVES=<dir of copied .sav files>`: every real save parses to its last byte.
#[test]
fn real_saves_parse_fully() {
    let Some(dir) = std::env::var_os("OHPL_GS4_SAVES") else {
        return;
    };
    let mut n = 0;
    for e in std::fs::read_dir(dir).into_iter().flatten().flatten() {
        let Ok(data) = std::fs::read(e.path()) else {
            continue;
        };
        let (st, h) = header::read_header(&data, true);
        assert_eq!(st, LoadStatus::Ok);
        let mut i = In::new(data.get(h.body..).unwrap_or_default());
        let mut l = Log::default();
        world::read_world(&mut i, h.version, &mut l);
        if h.version >= 27 {
            i.str();
        }
        assert!(
            i.ok && !i.left(),
            "{:?} v{} ok={} p={}",
            e.path(),
            h.version,
            i.ok,
            i.p
        );
        n += 1;
    }
    eprintln!("real saves parsed: {n}");
}

/// Mirrored by the `static_assert`s in `RustSomaSave.h`.
#[test]
fn layout_sizes() {
    use std::mem::size_of;
    assert_eq!(size_of::<Player>(), 52);
    assert_eq!(size_of::<MapTimer>(), 72);
    assert_eq!(size_of::<Spawned>(), 120);
    assert_eq!(size_of::<Body>(), 72);
    assert_eq!(size_of::<Attach>(), 184);
    assert_eq!(size_of::<Timer>(), 40);
    assert_eq!(size_of::<Anim>(), 40);
    assert_eq!(size_of::<Conn>(), 40);
    assert_eq!(size_of::<EntityHead>(), 448);
    assert_eq!(size_of::<EntityTail>(), 120);
    assert_eq!(size_of::<Light>(), 48);
    assert_eq!(size_of::<Particle>(), 152);
    assert_eq!(size_of::<Sound>(), 88);
    assert_eq!(size_of::<Billboard>(), 40);
    assert_eq!(size_of::<Agent>(), 112);
    assert_eq!(size_of::<Fog>(), 92);
    assert_eq!(size_of::<CamTex>(), 136);
    assert_eq!(size_of::<Camera>(), 72);
    assert_eq!(size_of::<OwnedLight>(), 40);
    assert_eq!(size_of::<World>(), 448);
    assert_eq!(size_of::<ImStatePair>(), 64);
}
