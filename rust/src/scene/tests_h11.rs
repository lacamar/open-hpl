//! H11: unit tests and replay of `tests/golden/H11/{mesh,light,fog}_*.txt`, recorded from the C++
//! path of `rust/tests/golden/H11/harness/h11_script.cpp` over real game meshes and animations.
//! The replay drives `h11::update_logic` and friends over a mock entity (animation states and
//! events only); bone matrices, skinned vertices and bounding volumes ("~" lines) are C++-only
//! output, compared live by `harness/run.sh`.

use super::animstate::AnimCore;
use super::h11::*;
use crate::math::IDENTITY;
use crate::types::{Matrixf, Vector3f};
use std::fs;
use std::path::PathBuf;

fn bits(w: &str) -> f32 {
    f32::from_bits(u32::from_str_radix(w, 16).unwrap())
}

struct Ev {
    time: f32,
    kind: i32,
    value: String,
}

struct Model {
    #[allow(clippy::vec_box)] // `ptrs` point into the boxes, which must not move
    cores: Vec<Box<AnimCore>>,
    ptrs: Vec<*mut AnimCore>,
    events: Vec<Vec<Ev>>,
    skel: bool,
    bones: usize,
    rest: bool,
    calls: Vec<String>,
    out: Vec<String>,
    ctl: *mut MeshCtl,
}

impl Model {
    fn new(skel: bool, bones: usize) -> Self {
        Self {
            cores: Vec::new(),
            ptrs: Vec::new(),
            events: Vec::new(),
            skel,
            bones,
            rest: false,
            calls: Vec::new(),
            out: Vec::new(),
            ctl: std::ptr::null_mut(),
        }
    }

    fn add_state(&mut self, length: f32, base: f32) {
        let mut c = Box::new(AnimCore::new(length));
        c.base_speed = base;
        self.ptrs.push(&raw mut *c);
        self.cores.push(c);
        self.events.push(Vec::new());
    }
}

impl MeshHost for Model {
    fn cores(&mut self) -> &[*mut AnimCore] {
        &self.ptrs
    }
    fn state_count(&mut self) -> usize {
        self.cores.len()
    }
    fn bone_count(&mut self) -> usize {
        self.bones
    }
    fn has_skeleton(&mut self) -> bool {
        self.skel
    }
    fn has_rest_pose(&mut self) -> bool {
        self.rest
    }
    fn bone_reset(&mut self, bone: usize, map_pose: bool, reset_temp: bool) {
        self.calls
            .push(format!("reset {bone} {map_pose} {reset_temp}"));
    }
    fn physics_from_bodies(&mut self) {
        self.calls.push("bodies".into());
    }
    fn physics_mix(&mut self, bone: usize, weight: f32) {
        self.calls
            .push(format!("mix {bone} {:08x}", weight.to_bits()));
    }
    fn physics_deactivate(&mut self) {
        self.calls.push("deactivate".into());
        if !self.ctl.is_null() {
            // SAFETY: the test points `ctl` at a `MeshCtl` that outlives the model.
            skeleton_physics_set(unsafe { Ctl::new(self.ctl) }, false);
        }
    }
    fn skeleton_apply(&mut self, state: usize, weight: f32) {
        self.calls
            .push(format!("apply {state} {:08x}", weight.to_bits()));
    }
    fn node_apply(&mut self, state: usize, weight: f32) {
        self.calls
            .push(format!("napply {state} {:08x}", weight.to_bits()));
    }
    fn bone_nodes_update(&mut self) {
        self.calls.push("bonenodes".into());
    }
    fn bone_entity_children(&mut self) {
        self.calls.push("children".into());
    }
    fn colliders_update(&mut self) {
        self.calls.push("colliders".into());
    }
    fn sub_transform_updated(&mut self) {
        self.calls.push("subxf".into());
    }
    fn node_reset(&mut self) {
        self.calls.push("nreset".into());
    }
    fn node_update(&mut self) {
        self.calls.push("nupdate".into());
    }
    fn node_restore(&mut self) {
        self.calls.push("nrestore".into());
    }
    fn after_anim(&mut self, dt: f32) {
        self.out.push(format!("e after {:08x}", dt.to_bits()));
    }
    fn sub_update_logic(&mut self, _dt: f32) {}
    fn event_count(&mut self, state: usize) -> usize {
        self.events.get(state).map_or(0, Vec::len)
    }
    fn event_time(&mut self, state: usize, event: usize) -> f32 {
        self.events[state][event].time
    }
    fn handle_event(&mut self, state: usize, event: usize) {
        let e = &self.events[state][event];
        self.out.push(format!(
            "e event {:08x} {} {}",
            e.time.to_bits(),
            e.kind,
            e.value
        ));
    }
}

struct Fnv(u64);

impl Fnv {
    fn new() -> Self {
        Self(0xcbf2_9ce4_8422_2325)
    }
    fn u(&mut self, v: u32) {
        for b in v.to_le_bytes() {
            self.0 = (self.0 ^ u64::from(b)).wrapping_mul(0x0100_0000_01b3);
        }
    }
    fn f(&mut self, v: f32) {
        self.u(v.to_bits());
    }
}

fn state_hash(m: &Model, ctl: &MeshCtl) -> u64 {
    let mut h = Fnv::new();
    h.u(m.cores.len() as u32);
    for c in &m.cores {
        for v in [
            c.length,
            c.weight,
            c.speed,
            c.base_speed,
            c.time_pos,
            c.prev_time_pos,
            c.special_event_time,
            c.fade_step,
        ] {
            h.f(v);
        }
        h.u(u32::from(c.active)
            | u32::from(c.looping) << 1
            | u32::from(c.paused) << 2
            | u32::from(c.layer) << 3
            | u32::from(c.is_over()) << 4);
    }
    h.u(u32::from(ctl.normalize));
    h.u(u32::from(ctl.physics));
    h.f(ctl.physics_weight);
    h.0
}

fn replay_mesh(text: &str) -> usize {
    let mut model = Model::new(false, 0);
    let mut ctl = MeshCtl::new();
    let mut is_static = false;
    let mut expected: Vec<String> = Vec::new();
    let mut ops = 0;
    let mut pending_rest: Option<usize> = None;
    let mut last = String::new();
    for line in text.lines() {
        let w: Vec<&str> = line.split(' ').collect();
        match *w.first().unwrap_or(&"") {
            "m" => {
                let kv = |k: &str| -> usize {
                    w.iter()
                        .find_map(|x| x.strip_prefix(k))
                        .unwrap()
                        .parse()
                        .unwrap()
                };
                model = Model::new(kv("skel=") == 1, kv("bones="));
                model.ctl = &raw mut ctl;
            }
            "s" => model.add_state(bits(w[2]), bits(w[3])),
            "x" => {
                let i: usize = w[1].parse().unwrap();
                let value = w[4..].join(" ");
                model.events[i].push(Ev {
                    time: bits(w[2]),
                    kind: w[3].parse().unwrap(),
                    value,
                });
            }
            "o" => {
                ops += 1;
                last = line.to_string();
                model.out.clear();
                expected.clear();
                pending_rest = None;
                // SAFETY: `ctl` outlives the `Ctl`; nothing else touches it meanwhile.
                let c = unsafe { Ctl::new(&raw mut ctl) };
                let n = |i: usize| -> usize { w[i].parse().unwrap() };
                let flag = |i: usize| w[i] == "1";
                let ptrs = model.ptrs.clone();
                // SAFETY: the pointers are the model's boxed, distinct cores.
                unsafe {
                    match w[1] {
                        "update" => update_logic(&mut model, c, is_static, bits(w[2])),
                        "play" => play(&ptrs, n(2) as i32, flag(3), flag(4)),
                        "playfade" => play_fade_to(&ptrs, n(2) as i32, flag(3), bits(w[4])),
                        "stop" => stop(&ptrs),
                        "active" => model.cores[n(2)].set_active(flag(3)),
                        "weight" => model.cores[n(2)].weight = bits(w[3]),
                        "time" => model.cores[n(2)].set_time_position(bits(w[3])),
                        "loop" => model.cores[n(2)].looping = flag(3),
                        "pause" => model.cores[n(2)].paused = flag(3),
                        "fadespeed" => model.cores[n(2)].fade_speed(bits(w[3]), bits(w[4])),
                        "layer" => model.cores[n(2)].layer = flag(3),
                        "fadein" => model.cores[n(2)].fade_in(bits(w[3])),
                        "fadeout" => model.cores[n(2)].fade_out(bits(w[3])),
                        "normalize" => ctl.normalize = flag(2),
                        "skelphys" if model.skel => skeleton_physics_set(c, flag(2)),
                        "physfade" if model.skel => {
                            skeleton_physics_fade(c, bits(w[2]));
                        }
                        "physweight" => ctl.physics_weight = bits(w[2]),
                        "restpose" => pending_rest = Some(n(2)),
                        "static" => is_static = flag(2),
                        _ => {}
                    }
                }
            }
            "=" if pending_rest.is_some() => {
                if w[1] == "1" {
                    model.rest = true;
                    ctl.updated_bones = false;
                }
                pending_rest = None;
            }
            "e" => expected.push(line.to_string()),
            "h" => {
                assert_eq!(model.out, expected, "events at op {ops}: {last}");
                let got = state_hash(&model, &ctl);
                assert_eq!(
                    format!("{got:016x}"),
                    w[1],
                    "state hash at op {ops}: {last}"
                );
                model.out.clear();
                expected.clear();
            }
            "r" => {
                let f: Vec<f32> = w[1..]
                    .iter()
                    .filter(|x| **x != "->")
                    .map(|x| bits(x))
                    .collect();
                let (inp, out) = f.split_at(7 + 16 + 1);
                let (vals, rest) = inp.split_at(7);
                let (local, unit) = rest.split_at(16);
                let mut l = Matrixf::default();
                for (i, v) in local.iter().enumerate() {
                    l.m[i / 4][i % 4] = *v;
                }
                let mut o = [Matrixf::default()];
                assert!(rest_pose_from_map(vals, &[l], unit, &mut o));
                let got: Vec<u32> = o[0].m.iter().flatten().map(|v| v.to_bits()).collect();
                let want: Vec<u32> = out.iter().map(|v| v.to_bits()).collect();
                assert_eq!(got, want, "rest pose at op {ops}");
            }
            "c" => {
                let f: Vec<f32> = w[1..]
                    .iter()
                    .filter(|x| **x != "->")
                    .map(|x| bits(x))
                    .collect();
                let mat = |s: &[f32]| {
                    let mut m = Matrixf::default();
                    for (i, v) in s.iter().enumerate() {
                        m.m[i / 4][i % 4] = *v;
                    }
                    m
                };
                let (m, p, y) = transform_from_skeleton(&mat(&f[0..16]), &mat(&f[16..32]));
                let got: Vec<u32> =
                    m.m.iter()
                        .flatten()
                        .copied()
                        .chain([p.x, p.y, p.z, y])
                        .map(f32::to_bits)
                        .collect();
                let want: Vec<u32> = f[32..].iter().map(|v| v.to_bits()).collect();
                assert_eq!(got, want, "transform from skeleton at op {ops}");
            }
            _ => {}
        }
    }
    ops
}

struct Check {
    what: usize,
    mask: u32,
    objs: Vec<(i32, bool)>,
}

fn golden(prefix: &str) -> Vec<(String, String)> {
    let dir = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("tests/golden/H11");
    let mut files: Vec<_> = fs::read_dir(dir)
        .unwrap()
        .map(|e| e.unwrap().path())
        .filter(|p| {
            let n = p.file_name().unwrap().to_string_lossy().into_owned();
            n.starts_with(prefix) && n.ends_with(".txt")
        })
        .collect();
    files.sort();
    files
        .into_iter()
        .map(|p| (p.display().to_string(), fs::read_to_string(&p).unwrap()))
        .collect()
}

#[test]
fn h11_mesh_golden() {
    let files = golden("mesh_");
    assert!(files.len() >= 20, "golden files missing");
    let ops: usize = files.iter().map(|(_, t)| replay_mesh(t)).sum();
    assert!(ops > 5000);
}

#[test]
fn h11_light_golden() {
    let files = golden("light_");
    assert!(files.len() >= 12, "golden files missing");
    let mut checks = 0;
    for (name, text) in files {
        let mut cache = ShadowCache::default();
        let mut check: Option<Check> = None;
        for line in text.lines() {
            let w: Vec<&str> = line.split(' ').collect();
            match (w[0], w.get(1).copied().unwrap_or("")) {
                ("o", "shadowadd") => cache.add(w[2].parse().unwrap(), w[3].parse().unwrap()),
                ("o", "shadowcheck") => {
                    let objs = w[4..]
                        .chunks(2)
                        .map(|c| (c[0].parse().unwrap(), c[1] == "1"))
                        .collect();
                    let what: usize = w[2].parse().unwrap();
                    let mask: u32 = w[3].parse().unwrap();
                    let list = |objs: &Vec<(i32, bool)>| {
                        objs.iter()
                            .enumerate()
                            .filter(|(i, _)| mask >> i & 1 == 1)
                            .map(|(i, &(count, is_static))| Caster {
                                object: i,
                                count,
                                is_static,
                            })
                            .collect::<Vec<_>>()
                    };
                    match what {
                        3 => cache.set_from(&list(&objs)),
                        4 => cache.clear(),
                        _ => check = Some(Check { what, mask, objs }),
                    }
                }
                ("=", "lit") => {
                    let kind: u32 = w[2].parse().unwrap();
                    let (r, g, b, a) = (bits(w[3]), bits(w[4]), bits(w[5]), bits(w[6]));
                    let (bright, radius) = (bits(w[7]), bits(w[8]));
                    let (hpl3, visible) = (w[9] == "1", w[10] == "1");
                    let lit = light_is_lit(r, g, b, a, bright, hpl3);
                    let vis = if kind == 2 {
                        lit && visible
                    } else {
                        light_is_visible(lit, radius, visible)
                    };
                    assert_eq!((w[12] == "1", w[13] == "1"), (lit, vis), "{name}: {line}");
                    checks += 1;
                }
                ("=", _) => {
                    let Check { what, mask, objs } = check.take().unwrap();
                    let list: Vec<Caster> = objs
                        .iter()
                        .enumerate()
                        .filter(|(i, _)| mask >> i & 1 == 1)
                        .map(|(i, &(count, is_static))| Caster {
                            object: i,
                            count,
                            is_static,
                        })
                        .collect();
                    if what == 0 {
                        let got: Vec<bool> = objs
                            .iter()
                            .enumerate()
                            .map(|(i, o)| cache.is_valid(i, o.0))
                            .collect();
                        let want: Vec<bool> = w[1..].iter().map(|x| *x == "1").collect();
                        assert_eq!(got, want, "{name}: {line}");
                    } else {
                        assert_eq!(cache.unchanged(&list), w[1] == "1", "{name}: {line}");
                    }
                    checks += 1;
                }
                _ => {}
            }
        }
    }
    assert!(checks > 3000);
}

#[test]
fn h11_fog_golden() {
    let files = golden("fog_");
    assert!(files.len() >= 3, "golden files missing");
    for (name, text) in files {
        for line in text.lines().filter(|l| l.starts_with("= fog")) {
            let w: Vec<&str> = line.split(' ').collect();
            let size = Vector3f {
                x: bits(w[2]),
                y: bits(w[3]),
                z: bits(w[4]),
            };
            let bv = fog_bounds_size(size);
            let got = [
                bv.x * 0.5,
                bv.y * 0.5,
                bv.z * 0.5,
                bv.x * -0.5,
                bv.y * -0.5,
                bv.z * -0.5,
            ]
            .map(f32::to_bits);
            let want =
                [w[6], w[7], w[8], w[9], w[10], w[11]].map(|x| u32::from_str_radix(x, 16).unwrap());
            assert_eq!(got, want, "{name}: {line}");
        }
    }
}

#[allow(clippy::vec_box)] // the pointers must stay valid
fn one_core() -> (Vec<Box<AnimCore>>, Vec<*mut AnimCore>) {
    let mut v: Vec<Box<AnimCore>> = (0..3).map(|_| Box::new(AnimCore::new(2.0))).collect();
    let p = v.iter_mut().map(|b| &raw mut **b).collect();
    (v, p)
}

#[test]
fn play_and_fade() {
    let (v, p) = one_core();
    // SAFETY: boxed, distinct cores.
    unsafe {
        play(&p, 1, true, false);
        play_fade_to(&p, 2, false, 0.5);
        assert!(v[1].fade_step < 0.0 && v[2].active && v[2].weight == 0.0 && v[2].fade_step == 2.0);
        play(&p, 0, false, true);
        assert!(v[0].active && !v[1].active && !v[2].active && v[0].weight == 1.0);
        play(&p, 7, false, false);
        play(&p, -1, false, false);
        stop(&p);
    }
    assert!(v.iter().all(|c| !c.active && c.time_pos == 0.0));
}

#[test]
fn physics_fade_flags() {
    let mut ctl = MeshCtl::new();
    // SAFETY: local `ctl` outlives `c`.
    let c = unsafe { Ctl::new(&raw mut ctl) };
    assert!(!skeleton_physics_fade(c, 2.0));
    skeleton_physics_set(c, true);
    assert!(skeleton_physics_fade(c, 2.0));
    assert!(ctl.fading && ctl.physics_fade_speed == 0.5);
    skeleton_physics_set(c, false);
    assert!(!ctl.fading && ctl.physics_weight == 1.0 && !ctl.updated_bones);
}

#[test]
fn physics_fade_ends_in_deactivate() {
    let mut m = Model::new(true, 2);
    m.add_state(1.0, 1.0);
    let mut ctl = MeshCtl::new();
    ctl.physics = true;
    ctl.fading = true;
    ctl.physics_fade_speed = 4.0;
    ctl.physics_weight = 0.5;
    // SAFETY: local `ctl` outlives `c`.
    let c = unsafe { Ctl::new(&raw mut ctl) };
    update_logic(&mut m, c, false, 0.25);
    assert!(m.calls.contains(&"deactivate".to_string()));
    assert!(!ctl.fading && ctl.physics_weight == 1.0);
    m.calls.clear();
    update_logic(&mut m, c, true, 0.25);
    assert!(m.calls.is_empty());
}

#[test]
fn graphics_gates() {
    let mut ctl = MeshCtl::new();
    // SAFETY: local `ctl` outlives `c`.
    let c = unsafe { Ctl::new(&raw mut ctl) };
    assert_eq!(graphics_gate(c, true, 5), 3);
    assert_eq!(graphics_gate(c, true, 5), 0);
    ctl.matrices_need_update = true;
    // SAFETY: as above; re-derived after the direct write.
    let c = unsafe { Ctl::new(&raw mut ctl) };
    assert_eq!(graphics_gate(c, true, 5), 1);
    assert_eq!(graphics_gate(c, false, 6), 1);

    let (mut updated, mut ver) = (false, -1);
    assert!(sub_skin_gate(&mut updated, &mut ver, 3, false));
    assert!(!sub_skin_gate(&mut updated, &mut ver, 3, false));
    assert!(!sub_skin_gate(&mut updated, &mut ver, 4, true));
    assert!(sub_skin_gate(&mut updated, &mut ver, 4, false));

    let mut n = 0;
    assert!(static_null_matrix(&mut n, &IDENTITY) && n == 1);
    assert!(static_null_matrix(&mut n, &IDENTITY));
    n = 0;
    let mut moved = IDENTITY;
    moved.m[0][3] = 1.0;
    assert!(!static_null_matrix(&mut n, &moved) && n == -1);
}

#[test]
fn spot_gates() {
    let mut s = SpotCtl::new();
    assert!(s.view_stale(1) && !s.view_stale(1));
    assert!(s.view_proj_stale(1) && s.frustum_stale(1));
    assert!(s.projection_stale() && !s.projection_stale());
    assert!(s.view_proj_stale(1) && s.frustum_stale(1));
    s.view_proj_done(1);
    s.frustum_done(1);
    assert!(!s.view_proj_stale(1) && !s.frustum_stale(1));
    assert!(s.view_proj_stale(2) && s.frustum_stale(2));
    s.projection_updated = true;
    assert!(s.view_proj_stale(1) && s.frustum_stale(1));
}

#[test]
fn shadow_cache_insert_keeps_first_count() {
    let mut c = ShadowCache::default();
    c.add(1, 10);
    c.add(1, 11);
    assert!(c.is_valid(1, 10) && !c.is_valid(1, 11));
    let list = [
        Caster {
            object: 1,
            count: 10,
            is_static: false,
        },
        Caster {
            object: 2,
            count: 3,
            is_static: true,
        },
    ];
    assert!(c.unchanged(&list));
    c.set_from(&list);
    assert_eq!(c.len(), 1);
    c.clear();
    assert!(!c.unchanged(&list));
}

#[test]
fn bounds_and_mask() {
    let v = |x, y, z| Vector3f { x, y, z };
    let (lo, hi) = bounds_union(
        &[v(0.0, 0.0, 0.0), v(-1.0, 2.0, 0.5)],
        &[v(1.0, 1.0, 1.0), v(0.5, 3.0, 0.2)],
    )
    .unwrap();
    assert_eq!((lo, hi), (v(-1.0, 0.0, 0.0), v(1.0, 3.0, 1.0)));
    assert!(bounds_union(&[], &[]).is_none());
    let (lo, hi) = light_mask_bounds(
        v(-5.0, -5.0, -5.0),
        v(5.0, 5.0, 5.0),
        v(1.0, 0.0, 0.0),
        v(2.0, 20.0, 20.0),
    );
    assert_eq!((lo, hi), (v(0.0, -5.0, -5.0), v(2.0, 5.0, 5.0)));
}

#[test]
fn rest_pose_rejects_wrong_count() {
    let mut out = [Matrixf::default()];
    assert!(!rest_pose_from_map(
        &[0.0; 6],
        &[IDENTITY],
        &[1.0],
        &mut out
    ));
}
