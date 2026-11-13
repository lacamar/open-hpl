//! Replays `rust/tests/golden/H4/h4.txt`: digests of the pre-port `cHpmWorldBuilder` decisions
//! (`harness/h4_probe.cpp`) over every `.hpm` map of the Steam game data (skipped where absent) and
//! over a synthetic corpus for the combine, collider and batch decisions (always run).

use super::abi::{self, Cstr};
use super::el::El;
use super::h4_plan::{self as plan, Cand};
use super::parse::{Builder, FileRead, parse_map, widen};
use crate::types::{Matrixf, Vector3f};
use std::ffi::CStr;
use std::path::{Path, PathBuf};

const GAMES: [(&str, &str); 3] = [
    ("soma", "SOMA"),
    ("rebirth", "Amnesia Rebirth"),
    ("bunker", "Amnesia The Bunker"),
];
const GOLDEN: &str = include_str!("../../tests/golden/H4/h4.txt");

struct Fnv {
    h: u64,
    lines: usize,
}

impl Fnv {
    fn new() -> Self {
        Self {
            h: 14_695_981_039_346_656_037,
            lines: 0,
        }
    }

    fn line(&mut self, s: &str) {
        if std::env::var_os("H4_DUMP").is_some() {
            eprintln!("{s}");
        }
        for &b in s.as_bytes().iter().chain(b"\n") {
            self.h = (self.h ^ u64::from(b)).wrapping_mul(1_099_511_628_211);
        }
        self.lines += 1;
    }

    fn digest(&self, name: &str) -> String {
        format!("{name}\t{}\t{:016x}", self.lines, self.h)
    }
}

fn x(f: f32) -> String {
    format!("{:08x}", f.to_bits())
}

fn m(mat: &Matrixf) -> String {
    mat.m
        .iter()
        .flatten()
        .map(|&f| x(f))
        .collect::<Vec<_>>()
        .join(",")
}

fn v(a: f32, b: f32, c: f32) -> String {
    format!("{},{},{}", x(a), x(b), x(c))
}

fn i3(a: [i32; 3]) -> String {
    format!("{},{},{}", a[0], a[1], a[2])
}

fn s(p: Cstr) -> String {
    if p.is_null() {
        return String::new();
    }
    // SAFETY: the parser passes NUL-terminated strings that outlive the call.
    unsafe { CStr::from_ptr(p) }.to_string_lossy().into_owned()
}

fn floats<'a>(p: *const f32, n: usize) -> &'a [f32] {
    if n == 0 {
        return &[];
    }
    // SAFETY: the parser passes `n` readable floats that outlive the call.
    unsafe { std::slice::from_raw_parts(p, n) }
}

fn v3(a: [f32; 3]) -> Vector3f {
    Vector3f {
        x: a[0],
        y: a[1],
        z: a[2],
    }
}

struct Probe(Fnv);

impl Builder for Probe {
    fn now_ms(&mut self) -> u64 {
        0
    }
    fn object_done(&mut self) {}
    fn warn(&mut self, _: &[u8]) {}
    fn log(&mut self, _: &[u8]) {}
    fn read_file(&mut self, path: &[u32]) -> FileRead {
        let p: String = path
            .iter()
            .map(|&c| char::from_u32(c).unwrap_or('?'))
            .collect();
        match std::fs::read(p) {
            Ok(d) => FileRead::Read(d),
            Err(_) => FileRead::NotOpened,
        }
    }
    fn global_fog(&mut self, _: &abi::Fog) {}
    fn distance_cull(&mut self, _: &abi::DistanceCull) {}
    fn dir_light(&mut self, _: &abi::DirLight) {}
    fn env_particles(&mut self, _: &abi::EnvParticles) {}
    fn post_effects(&mut self, _: &abi::PostEffects) {}
    fn sky_box(&mut self, _: &abi::SkyBox) {}

    fn static_object(&mut self, o: &abi::StaticObject) -> bool {
        let t = plan::static_xform(v3(o.rotation), v3(o.scale), v3(o.position));
        let pose_applied = o.has_pose && (o.id & 1) == 0;
        let cand = plan::is_combine_candidate(o.has_pose, pose_applied);
        self.0.line(&format!(
            "static|{}|{}|{}|{}|{}|{}|{}",
            s(o.name),
            s(o.filename),
            m(&t.world),
            m(&t.body),
            u8::from(cand),
            i3(plan::mesh_cell(v3(o.position))),
            i3(plan::body_cell(v3(o.position)))
        ));
        true
    }
    fn plane(&mut self, p: &abi::Plane) -> bool {
        let t = plan::static_xform(v3(p.rotation), v3(p.scale), v3(p.position));
        self.0.line(&format!(
            "plane|{}|{}|{}|{}",
            s(p.name),
            s(p.material),
            m(&t.world),
            m(&t.body)
        ));
        true
    }
    fn entity(&mut self, e: &abi::Entity, _: &El) -> bool {
        let t = plan::rot_trans(v3(e.rotation), v3(e.position));
        self.0
            .line(&format!("entity|{}|{}|{}", s(e.name), s(e.filename), m(&t)));
        true
    }
    fn light(&mut self, _: &El) -> bool {
        self.0.line("light");
        true
    }
    fn area(&mut self, a: &abi::Area, _: &El) -> bool {
        let t = plan::rot_trans(v3(a.rotation), v3(a.position));
        self.0
            .line(&format!("area|{}|{}|{}", s(a.name), s(a.area_type), m(&t)));
        true
    }
    fn sound(&mut self, _: &El, _: bool) -> bool {
        self.0.line("sound");
        true
    }
    fn decal(&mut self, _: &abi::Decal, _: Option<&El>) -> bool {
        self.0.line("decal");
        true
    }
    fn billboard(&mut self, _: &El) -> bool {
        self.0.line("billboard");
        true
    }
    fn particle_system(&mut self, _: &El, _: Cstr) -> bool {
        self.0.line("ps");
        true
    }
    fn fog_area(&mut self, _: &El) -> bool {
        self.0.line("fogarea");
        true
    }
    fn lens_flare(&mut self, _: &El) -> bool {
        self.0.line("lensflare");
        true
    }
    fn light_mask_box(&mut self, k: &abi::LightMask) {
        let p = plan::light_mask(v3(k.rotation), v3(k.size), v3(k.scale), v3(k.world_pos));
        self.0.line(&format!(
            "mask|{}|{}|{}",
            k.id,
            v(p.center.x, p.center.y, p.center.z),
            v(p.extent.x, p.extent.y, p.extent.z)
        ));
    }
    fn connect_lights(&mut self) -> (i32, i32) {
        (0, 0)
    }
    fn connect_light_masks(&mut self) {}
    fn detail_meshes(&mut self, d: &abi::DetailMeshes) -> i32 {
        let n = usize::try_from(d.num).unwrap_or(0);
        let radii = floats(d.radii, d.num_radii);
        let pos = floats(d.positions, n * 3);
        let rot = floats(d.rotations, n * 4);
        let mut mesh_radius = 0.0f32;
        for i in 0..n {
            if mesh_radius == 0.0 {
                let bounds: Vec<_> = (0..n.min(4))
                    .map(|j| {
                        let r = radii.get(j).copied().unwrap_or(1.0);
                        let j1 = (j + 1) as f32;
                        (v3([-r * j1, -r, -r * 0.5]), v3([r, r * 2.0, r]))
                    })
                    .collect();
                mesh_radius = plan::detail_mesh_radius(&bounds);
            }
            let scale = plan::detail_scale(radii.get(i).copied(), mesh_radius);
            let q: Vec<f32> = rot.iter().skip(i * 4).take(4).copied().collect();
            let p: Vec<f32> = pos.iter().skip(i * 3).take(3).copied().collect();
            let mtx = match (<[f32; 4]>::try_from(q), <[f32; 3]>::try_from(p)) {
                (Ok(q), Ok(p)) => plan::detail_matrix(q, v3(p), scale),
                _ => Matrixf::default(),
            };
            self.0.line(&format!(
                "detail|{}|{}|{}|{}|{}",
                s(d.file),
                d.first_index + i as i32,
                x(mesh_radius),
                x(scale),
                m(&mtx)
            ));
        }
        d.num
    }
    fn exposure_area(&mut self, a: &abi::ExposureArea) {
        let p = plan::exposure_area(v3(a.rotation), v3(a.world_pos), v3(a.scale));
        self.0.line(&format!(
            "expo|{}|{}|{}",
            s(a.name),
            m(&p.inv_transform),
            v(p.half_size.x, p.half_size.y, p.half_size.z)
        ));
    }
    fn terrain(&mut self, _: &abi::Terrain) {
        self.0.line("terrain");
    }
    fn track_report(&mut self, _: Cstr, _: i32, _: i32, _: i32, _: bool, _: &[abi::Skip]) {}
}

struct Rng(u64);

impl Rng {
    fn next(&mut self) -> u32 {
        self.0 = self
            .0
            .wrapping_mul(6_364_136_223_846_793_005)
            .wrapping_add(1_442_695_040_888_963_407);
        (self.0 >> 33) as u32
    }
}

fn synth() -> Fnv {
    let mut h = Fnv::new();
    let mut r = Rng(0x9e37_79b9_7f4a_7c15);
    const VALS: [f32; 3] = [0.0, 0.5, 1.0];
    for _ in 0..400 {
        let n = 1 + r.next() % 40;
        let ents = 1 + r.next() % 8;
        let mut sub_count = vec![0u32; ents as usize];
        let mut pool: Vec<i32> = Vec::new();
        struct Raw {
            ent: u32,
            cell: [i32; 3],
            mat: u64,
            shadow: bool,
            one_sided: bool,
            lay: (usize, usize),
            v: [f32; 9],
            d: bool,
            f: bool,
        }
        let mut raws = Vec::new();
        for _ in 0..n {
            let ent = r.next() % ents;
            if let Some(c) = sub_count.get_mut(ent as usize) {
                *c += 1;
            }
            let cell = [
                (r.next() % 3) as i32 - 1,
                (r.next() % 2) as i32,
                (r.next() % 2) as i32,
            ];
            let mat = u64::from((r.next() % 3 + 1) * 0x1000);
            let shadow = (r.next() & 1) != 0;
            let one_sided = (r.next() & 1) != 0;
            let off = pool.len();
            for _ in 0..r.next() % 4 {
                pool.push((r.next() % 3) as i32);
            }
            let lay = (off, pool.len());
            let mut vv = [0.0f32; 9];
            for e in &mut vv {
                *e = VALS.get((r.next() % 3) as usize).copied().unwrap_or(0.0);
            }
            let d = (r.next() & 1) != 0;
            let f = (r.next() & 1) != 0;
            raws.push(Raw {
                ent,
                cell,
                mat,
                shadow,
                one_sided,
                lay,
                v: vv,
                d,
                f,
            });
        }
        let sub_num: Vec<u32> = sub_count
            .iter()
            .map(|&c| c + u32::from((r.next() & 3) == 0))
            .collect();
        let cands: Vec<Cand> = raws
            .iter()
            .map(|c| Cand {
                ent: c.ent,
                cell: c.cell,
                mat: c.mat,
                shadow: c.shadow,
                one_sided: c.one_sided,
                layout: pool.get(c.lay.0..c.lay.1).unwrap_or_default(),
                v: c.v,
                culled_dist: c.d,
                culled_fog: c.f,
            })
            .collect();
        let p = plan::combine_plan(&cands, &sub_num);
        let mut line = format!("combine|{n}|");
        let mut at = 0usize;
        for &g in &p.group_sizes {
            line += &format!("g{g}:");
            for &i in p.order.iter().skip(at).take(g as usize) {
                line += &format!("{i},");
            }
            at += g as usize;
        }
        line += "|d:";
        for &d in &p.destroy {
            line += if d { "1" } else { "0" };
        }
        line += &format!("|{}", p.combined_subs);
        h.line(&line);
    }
    let names = [
        "",
        "body",
        "_collider",
        "_collider_1",
        "_charcollider",
        "_charcollider2",
        "_noncharcollider",
        "_noncharcollider_a",
        "_col",
        "_charcol",
        "_Collider",
        "x_collider",
        "_noncharcollide",
    ];
    for n in names {
        let b = n.as_bytes();
        let has = plan::has_collider_mesh([b]);
        let sp = plan::sub_mesh_plan(b);
        let t = |f: bool| if f { "1" } else { "0" };
        h.line(&format!(
            "name|{n}|{}{}{}{}",
            t(has),
            t(sp.is_char_collider_mesh),
            t(sp.is_collider_mesh),
            t(sp.hide)
        ));
        for has_col in 0..2u32 {
            for mask in 0..8u32 {
                for nc in 0..=3u32 {
                    let batches = plan::sub_mesh_batches(
                        has_col != 0,
                        sp,
                        (0..nc).map(|j| (mask >> j) & 1 != 0),
                    );
                    h.line(&format!(
                        "batches|{has_col}{mask}{nc}{}|{}",
                        u8::from(sp.is_collider_mesh),
                        t(batches)
                    ));
                }
            }
        }
    }
    for _ in 0..4000 {
        let fx = ((r.next() % 4000) as i32 - 2000) as f32 / 7.0;
        let fy = ((r.next() % 4000) as i32 - 2000) as f32 / 3.0;
        let fz = ((r.next() % 4000) as i32 - 2000) as f32 / 11.0;
        let p = Vector3f {
            x: fx,
            y: fy,
            z: fz,
        };
        h.line(&format!(
            "cell|{}|{}",
            i3(plan::mesh_cell(p)),
            i3(plan::body_cell(p))
        ));
        let cur = (r.next() % 60000) as usize;
        let add = (r.next() % 20000) as usize;
        h.line(&format!(
            "flush|{}",
            if plan::batch_must_flush(cur, add) {
                "1"
            } else {
                "0"
            }
        ));
    }
    h
}

fn maps(root: &Path) -> Vec<PathBuf> {
    fn walk(dir: &Path, out: &mut Vec<PathBuf>) {
        let Ok(rd) = std::fs::read_dir(dir) else {
            return;
        };
        for e in rd.flatten() {
            let p = e.path();
            if p.is_dir() {
                walk(&p, out);
            } else if p.extension().is_some_and(|x| x == "hpm") {
                out.push(p);
            }
        }
    }
    let mut out = Vec::new();
    walk(root, &mut out);
    out.sort();
    out
}

#[test]
fn synthetic_decisions_match_cpp() {
    let want = GOLDEN
        .lines()
        .find(|l| l.starts_with("synth\t"))
        .unwrap_or_default();
    assert_eq!(synth().digest("synth"), want);
}

#[test]
#[ignore = "parses every .hpm map (~7 min debug); run with --ignored"]
fn hpm_maps_match_cpp() {
    let common = std::env::var_os("OHPL_H4_CORPUS").map_or_else(
        || {
            PathBuf::from(std::env::var_os("HOME").unwrap_or_default())
                .join(".local/share/Steam/steamapps/common")
        },
        PathBuf::from,
    );
    let mut compared = 0;
    for (tag, dir) in GAMES {
        let root = common.join(dir);
        if !root.is_dir() {
            continue;
        }
        for map in maps(&root) {
            let rel = map
                .strip_prefix(&root)
                .unwrap_or(&map)
                .to_string_lossy()
                .into_owned();
            let key = format!("{tag}:{rel}");
            let mut probe = Probe(Fnv::new());
            parse_map(&mut probe, &widen(&map.to_string_lossy()));
            let want = GOLDEN
                .lines()
                .find(|l| l.split('\t').next() == Some(key.as_str()));
            assert_eq!(Some(probe.0.digest(&key).as_str()), want, "{key}");
            compared += 1;
        }
    }
    eprintln!("hpm maps compared: {compared}");
}
