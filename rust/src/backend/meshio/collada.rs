//! `cMeshLoaderCollada::LoadMesh` / `LoadAnimation`: cache lookup, parse (`crate::meshload`), then
//! the skeleton, sub mesh, node and animation decisions; the builder creates the engine objects.

use super::abi::{self, Array, Bone, Cfg, Collider, Sink, VbCfg};
use super::assimp::read_hpl3_anm;
use super::msh::{self, text};
use crate::math::{
    fma, matrix_inverse, matrix_mul, matrix_mul_vec, matrix_rotate, matrix_scale,
    matrix_to_euler_angles, normalize3, quat_from_rotation_matrix, quat_to_rotation_matrix, to_deg,
    to_rad, v3,
};
use crate::meshload::Key;
use crate::meshload::collada::{self as parse, Collada, Controller, Geometry, Node, ParseError};
use crate::string;
use crate::types::{Matrixf, Quaternion, Vector3f};
use std::ffi::OsStr;
use std::os::unix::ffi::OsStrExt;

const NO_GEOMETRY: u32 = 1;
const NO_MATERIAL: u32 = 2;
const TRANSLATE_ROTATE: i32 = 3;
const SHAPE_BOX: i32 = 1;
const SHAPE_SPHERE: i32 = 2;
const SHAPE_CYLINDER: i32 = 3;
const SHAPE_CAPSULE: i32 = 4;
const ELEMENT_NORMAL: i32 = 0;
const ELEMENT_POSITION: i32 = 1;
const ELEMENT_COLOR0: i32 = 2;
const ELEMENT_TANGENT: i32 = 4;
const ELEMENT_TEXTURE0: i32 = 5;

/// Returned when the C++ loader has to take over (its XML structure cache is newer than the file).
pub const FALLBACK: i32 = 2;

fn identity() -> Matrixf {
    matrix_scale(v3(1.0, 1.0, 1.0))
}

fn z_to_y() -> Matrixf {
    Matrixf {
        m: [
            [-1.0, 0.0, 0.0, 0.0],
            [0.0, 0.0, 1.0, 0.0],
            [0.0, 1.0, 0.0, 0.0],
            [0.0, 0.0, 0.0, 1.0],
        ],
    }
}

fn flat(m: &Matrixf) -> [f32; 16] {
    let mut out = [0.0; 16];
    for (d, s) in out.iter_mut().zip(m.m.iter().flatten()) {
        *d = *s;
    }
    out
}

fn mat(a: &[f32; 16]) -> Matrixf {
    let mut m = Matrixf::default();
    for (d, s) in m.m.iter_mut().flatten().zip(a) {
        *d = *s;
    }
    m
}

fn rotation(m: &Matrixf) -> Matrixf {
    let mut r = identity();
    for (i, row) in r.m.iter_mut().enumerate().take(3) {
        for (j, c) in row.iter_mut().enumerate().take(3) {
            *c = m.m.get(i).and_then(|r| r.get(j)).copied().unwrap_or(0.0);
        }
    }
    r
}

fn translation(m: &Matrixf) -> Vector3f {
    v3(m.m[0][3], m.m[1][3], m.m[2][3])
}

fn set_translation(m: &mut Matrixf, t: Vector3f) {
    m.m[0][3] = t.x;
    m.m[1][3] = t.y;
    m.m[2][3] = t.z;
}

fn scaled(v: Vector3f, s: f32) -> Vector3f {
    v3(v.x * s, v.y * s, v.z * s)
}

fn sub(a: Vector3f, b: Vector3f) -> Vector3f {
    v3(a.x - b.x, a.y - b.y, a.z - b.z)
}

/// `a*(1-t) + b*t` as gcc contracts it.
fn lerp(a: f32, b: f32, t: f32) -> f32 {
    fma(a, 1.0 - t, b * t)
}

fn quat_of(m: &Matrixf) -> Quaternion {
    let mut q = Quaternion::default();
    quat_from_rotation_matrix(&mut q, m);
    q
}

fn key(time: f32, trans: Vector3f, q: &Quaternion) -> Key {
    Key {
        time,
        trans: [trans.x, trans.y, trans.z],
        rot: [q.v.x, q.v.y, q.v.z, q.w],
    }
}

fn atof(s: &[u8]) -> f64 {
    let s = std::str::from_utf8(s).unwrap_or("");
    let s = s.trim_start();
    (1..=s.len())
        .rev()
        .filter(|&n| s.is_char_boundary(n))
        .find_map(|n| s.get(..n).and_then(|p| p.parse::<f64>().ok()))
        .unwrap_or(0.0)
}

fn find(hay: &[u8], needle: &[u8], from: usize) -> Option<usize> {
    hay.get(from..)?
        .windows(needle.len())
        .position(|w| w == needle)
        .map(|p| p + from)
}

// ---- cache lookup ----

fn external_cache_file(sink: Sink, cfg: &Cfg, path: &[u8]) -> Vec<u8> {
    // SAFETY: the config strings live for the call.
    let dir = unsafe { cfg.mesh_cache_dir.bytes() };
    if dir.is_empty() {
        return sink.cache_file(path, b"msh");
    }
    let flat: Vec<u8> = path
        .iter()
        .map(|&c| {
            if matches!(c, b'/' | b'\\' | b':') {
                b'_'
            } else {
                c
            }
        })
        .collect();
    let ext: &[u8] = if cfg.unscaled_skeleton != 0 {
        b".v12.msh"
    } else {
        b".v5.msh"
    };
    text(&[dir, &flat, ext])
}

fn try_cache(
    cfg: &Cfg,
    sink: Sink,
    path: &[u8],
    ext: &[u8],
    anim: bool,
    load: impl Fn(&[u8]) -> bool,
) -> bool {
    if cfg.load_and_save_msh == 0 {
        return false;
    }
    let mut cached = external_cache_file(sink, cfg, path);
    if anim {
        cached = string::set_file_ext(&cached, ext);
    }
    if sink.file_exists(&cached) && sink.file_date(&cached) > sink.file_date(path) && load(&cached)
    {
        return true;
    }
    // SAFETY: the config strings live for the call.
    if unsafe { cfg.mesh_cache_dir.bytes() }.is_empty() {
        let sibling = string::set_file_ext(path, ext);
        let newer = sink.file_date(&sibling) > sink.file_date(path);
        if (cfg.force_cache_skip_saving != 0 || newer || !sink.file_exists(path)) && load(&sibling)
        {
            return true;
        }
    }
    false
}

// ---- file ----

enum Read {
    Ok(Box<Collada>),
    Fail,
    Fallback,
}

fn read_collada(cfg: &Cfg, sink: Sink, path: &[u8], opts: parse::Opts) -> Read {
    let _ = cfg;
    if collada_cache_newer(sink, path) {
        return Read::Fallback;
    }
    sink.log(
        abi::LOG,
        &text(&[b"Cache out of date! Reloading collada file '", path, b"'\n"]),
    );
    let Ok(data) = std::fs::read(OsStr::from_bytes(path)) else {
        return Read::Fail;
    };
    match parse::parse(&data, true, opts) {
        Ok(c) => {
            for (level, msg) in &c.log {
                sink.log(i32::from(*level), msg);
            }
            Read::Ok(Box::new(c))
        }
        Err(ParseError::Xml) => {
            sink.log(
                abi::ERROR,
                &text(&[b"Couldn't load Collada XML file '", path, b"'!\n"]),
            );
            Read::Fail
        }
        Err(ParseError::Fail(parse::Fail(why))) => {
            sink.log(
                abi::ERROR,
                &text(&[
                    b"Couldn't parse Collada file '",
                    path,
                    b"': ",
                    why.as_bytes(),
                    b"\n",
                ]),
            );
            Read::Fail
        }
    }
}

// ---- scene queries ----

struct Scene<'a>(&'a parse::Scene);

impl<'a> Scene<'a> {
    fn node(&self, id: &[u8]) -> Option<&'a Node> {
        self.0.nodes.iter().find(|n| n.id == id || n.name == id)
    }

    fn node_from_source(&self, src: &[u8]) -> Option<&'a Node> {
        self.0.nodes.iter().find(|n| n.source == src)
    }

    fn roots(&self) -> impl Iterator<Item = (usize, &'a Node)> {
        self.0
            .nodes
            .iter()
            .enumerate()
            .filter(|(_, n)| n.parent < 0)
    }

    fn children(&self, at: usize) -> impl Iterator<Item = (usize, &'a Node)> {
        self.0
            .nodes
            .iter()
            .enumerate()
            .filter(move |(_, n)| usize::try_from(n.parent).is_ok_and(|p| p == at))
    }

    fn parent(&self, n: &Node) -> Option<&'a Node> {
        usize::try_from(n.parent)
            .ok()
            .and_then(|p| self.0.nodes.get(p))
    }
}

// ---- skeleton ----

struct SBone {
    name: Vec<u8>,
    sid: Vec<u8>,
    parent: i32,
    m: Matrixf,
    value: i32,
    unit_scale: Option<f32>,
}

#[derive(Default)]
struct Skel {
    bones: Vec<SBone>,
}

impl Skel {
    fn add(&mut self, scene: &Scene, at: usize, node: &Node, parent: i32) {
        let me = self.bones.len() as i32;
        self.bones.push(SBone {
            name: node.id.clone(),
            sid: node.sid.clone(),
            parent,
            m: node.mtx,
            value: 0,
            unit_scale: None,
        });
        for (c, n) in scene.children(at) {
            self.add(scene, c, n, me);
        }
    }

    fn by_sid(&self, sid: &[u8]) -> Option<usize> {
        self.bones.iter().position(|b| b.sid == sid)
    }

    fn by_name(&self, name: &[u8]) -> Option<usize> {
        self.bones.iter().position(|b| b.name == name)
    }

    fn roots(&self) -> Vec<usize> {
        (0..self.bones.len())
            .filter(|&i| self.bones.get(i).is_some_and(|b| b.parent < 0))
            .collect()
    }

    fn children(&self, at: usize) -> Vec<usize> {
        (0..self.bones.len())
            .filter(|&i| {
                self.bones
                    .get(i)
                    .is_some_and(|b| usize::try_from(b.parent).is_ok_and(|p| p == at))
            })
            .collect()
    }

    fn world(&self, at: usize) -> Matrixf {
        let Some(b) = self.bones.get(at) else {
            return identity();
        };
        let parent = usize::try_from(b.parent)
            .ok()
            .map_or_else(identity, |p| self.world(p));
        matrix_mul(&parent, &b.m)
    }

    fn calc_local(&mut self, at: usize, parent_global: &Matrixf) {
        let Some(b) = self.bones.get(at) else { return };
        let global = if b.value == 0 {
            self.world(at)
        } else {
            let global = b.m;
            let local = matrix_mul(&matrix_inverse(parent_global), &global);
            if let Some(b) = self.bones.get_mut(at) {
                b.m = local;
            }
            global
        };
        for c in self.children(at) {
            self.calc_local(c, &global);
        }
    }

    fn unscale(&mut self, at: usize, unit: f32, root_rot: &Matrixf, is_root: bool) {
        if let Some(b) = self.bones.get_mut(at) {
            let mut m = b.m;
            let t = scaled(translation(&m), unit);
            set_translation(&mut m, t);
            b.m = if is_root { matrix_mul(root_rot, &m) } else { m };
            if !is_root {
                b.unit_scale = Some(unit);
            }
        }
        for c in self.children(at) {
            self.unscale(c, unit, root_rot, false);
        }
    }
}

fn build_skeleton(
    c: &Collada,
    scene: &Scene,
    sink: Sink,
    log_missing: &[u8],
) -> Result<Option<Skel>, ()> {
    if c.controllers.is_empty() {
        return Ok(None);
    }
    let mut sk = Skel::default();
    for (i, n) in scene.roots() {
        sk.add(scene, i, n, -1);
    }
    for ctrl in &c.controllers {
        for (j, joint) in ctrl.joints.iter().enumerate() {
            if let Some(b) = sk.by_sid(joint) {
                let m = ctrl.matrices.get(j).ok_or(())?;
                if let Some(bone) = sk.bones.get_mut(b) {
                    bone.m = matrix_inverse(&mat(m));
                    bone.value = 1;
                }
            } else {
                sink.log(
                    abi::LOG,
                    &text(&[b"Bone '", joint, b"' does not exist", log_missing, b"\n"]),
                );
            }
        }
    }
    for r in sk.roots() {
        sk.calc_local(r, &identity());
    }
    Ok(Some(sk))
}

fn emit_skeleton(sink: Sink, sk: &Skel) {
    sink.skeleton_begin();
    for b in &sk.bones {
        sink.bone(&Bone {
            name: abi::Str::new(&b.name),
            sid: abi::Str::new(&b.sid),
            m: flat(&b.m),
            inv_bind: [0.0; 16],
            unit_scale: b.unit_scale.unwrap_or(1.0),
            parent: b.parent,
            value: b.value,
            has_unit_scale: b.unit_scale.is_some().into(),
            has_inv_bind: 0,
        });
    }
    sink.skeleton_end(false);
}

// ---- animation ----

struct Track {
    name: Vec<u8>,
    keys: Vec<Key>,
    unit_scale: Option<f32>,
}

#[derive(Clone, Copy, Default)]
struct Temp {
    trans: Vector3f,
    rot: Vector3f,
    time: f32,
}

fn temp_at(temps: &[Temp], time: f32) -> Option<usize> {
    temps.iter().position(|t| t.time == time)
}

fn anim_times(time: f32, times: &[f32]) -> (f32, f32) {
    let (mut before, mut after) = (-1.0, -1.0);
    for &t in times {
        if t <= time {
            before = t;
        } else if t >= time {
            after = t;
        }
    }
    (before, after)
}

fn comp(v: &mut Vector3f, i: usize) -> &mut f32 {
    match i {
        0 => &mut v.x,
        1 => &mut v.y,
        _ => &mut v.z,
    }
}

enum Fill {
    Axis(usize),
    All,
    Trans,
}

fn blend(dst: &mut Temp, b: &Temp, a: &Temp, t: f32, what: &Fill) {
    let mix =
        |x: Vector3f, y: Vector3f| v3(lerp(x.x, y.x, t), lerp(x.y, y.y, t), lerp(x.z, y.z, t));
    match what {
        Fill::Trans => dst.trans = mix(b.trans, a.trans),
        Fill::Axis(i) => {
            let (mut bb, mut aa) = (b.rot, a.rot);
            *comp(&mut dst.rot, *i) = lerp(*comp(&mut bb, *i), *comp(&mut aa, *i), t);
        }
        Fill::All => {
            dst.rot = mix(b.rot, a.rot);
            dst.trans = mix(b.trans, a.trans);
        }
    }
}

fn copy_from(dst: &mut Temp, from: &Temp, what: &Fill) {
    match what {
        Fill::Trans => dst.trans = from.trans,
        Fill::Axis(i) => {
            let mut f = from.rot;
            *comp(&mut dst.rot, *i) = *comp(&mut f, *i);
        }
        Fill::All => {
            dst.rot = from.rot;
            dst.trans = from.trans;
        }
    }
}

fn fill_missing(temps: &mut [Temp], times: &[f32], what: &Fill) {
    for j in 0..temps.len() {
        let Some(&cur) = temps.get(j) else { return };
        let (before, after) = anim_times(cur.time, times);
        if before == cur.time {
            continue;
        }
        let src = |t: f32| temp_at(temps, t).and_then(|i| temps.get(i)).copied();
        let (b, a) = (src(before), src(after));
        let Some(dst) = temps.get_mut(j) else { return };
        if before >= 0.0 && after >= 0.0 {
            if let (Some(b), Some(a)) = (b, a) {
                let t = (cur.time - b.time) / (a.time - b.time);
                blend(dst, &b, &a, t, what);
            }
        } else if before < 0.0
            && after >= 0.0
            && let Some(a) = a
        {
            copy_from(dst, &a, what);
        } else if before >= 0.0
            && after < 0.0
            && let Some(b) = b
        {
            copy_from(dst, &b, what);
        }
    }
}

fn source<'a>(a: &'a parse::Animation, id: &[u8]) -> Option<&'a [f32]> {
    a.sources
        .iter()
        .find(|s| s.id == id)
        .map(|s| s.values.as_slice())
}

fn vec_at(values: &[f32], i: usize) -> Option<f32> {
    values.get(i).copied()
}

fn create_anim_track(
    sink: Sink,
    skel: Option<&Skel>,
    a: &parse::Animation,
    scene: &Scene,
    start: f32,
) -> Option<Track> {
    let err = |parts: &[&[u8]]| sink.log(abi::ERROR, &text(parts));
    let Some(node) = scene.node(&a.target_node) else {
        err(&[
            b"Couldn't find node '",
            &a.target_node,
            b"' for animation id '",
            &a.id,
            b"'\n",
        ]);
        return None;
    };
    let mut bone = None;
    if let Some(sk) = skel {
        let Some(b) = sk.by_name(&a.target_node).and_then(|i| sk.bones.get(i)) else {
            err(&[b"Couldn't find bone '", &a.target_node, b"'\n"]);
            return None;
        };
        bone = Some(b);
    }
    let mut times: Vec<f32> = Vec::new();
    for s in &a.samplers {
        let Some(v) = source(a, &s.time_array) else {
            err(&[b"Time array not found in sampler '", &s.id, b"'!\n"]);
            return None;
        };
        times.extend_from_slice(v);
    }
    times.sort_by(f32::total_cmp);
    times.dedup();
    let mut temps: Vec<Temp> = times
        .iter()
        .map(|&time| Temp {
            time,
            ..Temp::default()
        })
        .collect();
    let (mut loaded_t, mut loaded_r) = (false, false);

    for s in &a.samplers {
        let Some(tv) = source(a, &s.time_array) else {
            err(&[b"Time array not found!\n"]);
            return None;
        };
        let target = string::set_file_ext(string::file_name(&s.target), b"");
        let ext = string::to_lower(string::file_ext(&s.target));
        let Some(tr) = node.transforms.iter().find(|t| t.sid == target) else {
            continue;
        };
        let Some(vv) = source(a, &s.value_array) else {
            err(&[b"Value array not found!\n"]);
            return None;
        };
        let bad = || {
            err(&[b"Animation sampler data is too short\n"]);
            None::<Track>
        };
        match tr.kind.as_slice() {
            b"translate" => {
                loaded_t = true;
                for (j, &t) in tv.iter().enumerate() {
                    let vt = translation(&node.mtx);
                    let Some(i) = temp_at(&temps, t) else {
                        err(&[b"Code error at 187\n"]);
                        return None;
                    };
                    let g = |k: usize| vec_at(vv, k);
                    let tr3 = match ext.as_slice() {
                        b"" => match (g(j * 3), g(j * 3 + 1), g(j * 3 + 2)) {
                            (Some(x), Some(y), Some(z)) => v3(x, y, z),
                            _ => return bad(),
                        },
                        b"x" => v3(g(j).or_else(|| bad().map(|_| 0.0))?, vt.y, vt.z),
                        b"y" => v3(vt.x, g(j).or_else(|| bad().map(|_| 0.0))?, vt.z),
                        b"z" => v3(vt.x, vt.y, g(j).or_else(|| bad().map(|_| 0.0))?),
                        _ => temps.get(i).map_or_else(Vector3f::default, |t| t.trans),
                    };
                    if let Some(d) = temps.get_mut(i) {
                        d.trans = tr3;
                        if let Some(b) = bone {
                            d.trans = sub(d.trans, translation(&b.m));
                        }
                    }
                }
                fill_missing(&mut temps, tv, &Fill::Trans);
            }
            b"rotate" => {
                loaded_r = true;
                let axis = |k: usize| tr.values.get(k).copied().unwrap_or(0.0);
                for (j, &t) in tv.iter().enumerate() {
                    let Some(i) = temp_at(&temps, t) else {
                        err(&[b"Code error at 295\n"]);
                        return None;
                    };
                    let Some(angle) = vec_at(vv, j) else {
                        return bad();
                    };
                    if let Some(d) = temps.get_mut(i) {
                        d.rot = v3(
                            fma(axis(0), angle, d.rot.x),
                            fma(axis(1), angle, d.rot.y),
                            fma(axis(2), angle, d.rot.z),
                        );
                    }
                }
                let lane = if axis(0) > 0.001 {
                    0
                } else if axis(1) > 0.001 {
                    1
                } else if axis(2) > 0.001 {
                    2
                } else {
                    0
                };
                fill_missing(&mut temps, tv, &Fill::Axis(lane));
            }
            b"matrix" => {
                loaded_r = true;
                loaded_t = true;
                for (j, &t) in tv.iter().enumerate() {
                    let Some(i) = temp_at(&temps, t) else {
                        err(&[b"Code error at 372\n"]);
                        return None;
                    };
                    let Some(raw) = vv
                        .get(j * 16..j * 16 + 16)
                        .and_then(|r| <[f32; 16]>::try_from(r).ok())
                    else {
                        return bad();
                    };
                    let mut frame = mat(&raw);
                    if let Some(b) = bone {
                        let inv = matrix_inverse(&b.m);
                        let change = sub(translation(&frame), translation(&b.m));
                        frame = matrix_mul(&rotation(&inv), &rotation(&frame));
                        set_translation(&mut frame, change);
                    }
                    let e = matrix_to_euler_angles(&frame);
                    if let Some(d) = temps.get_mut(i) {
                        let ft = translation(&frame);
                        d.trans = v3(d.trans.x + ft.x, d.trans.y + ft.y, d.trans.z + ft.z);
                        d.rot = v3(
                            d.rot.x + to_deg(e.x),
                            d.rot.y + to_deg(e.y),
                            d.rot.z + to_deg(e.z),
                        );
                    }
                }
                fill_missing(&mut temps, tv, &Fill::All);
            }
            _ => {}
        }
    }

    let mut keys = Vec::with_capacity(temps.len());
    for t in &temps {
        let local = node.mtx;
        let mut rot_change = rotation(&local);
        let mut trans_change = translation(&local);
        if let Some(b) = bone {
            let inv = matrix_inverse(&b.m);
            rot_change = matrix_mul(&rotation(&local), &rotation(&inv));
            trans_change = sub(translation(&local), translation(&b.m));
        }
        let trans = if loaded_t { t.trans } else { trans_change };
        let q = if loaded_r {
            let rad = v3(to_rad(t.rot.x), to_rad(t.rot.y), to_rad(t.rot.z));
            quat_of(&matrix_rotate(rad, 0))
        } else {
            quat_of(&rot_change)
        };
        keys.push(key(t.time - start, trans, &q));
    }
    Some(Track {
        name: a.target_node.clone(),
        keys,
        unit_scale: None,
    })
}

fn clean_up_non_animated(
    sink: Sink,
    tracks: &mut Vec<Track>,
    skel: Option<&Skel>,
    scene: &Scene,
    z_up: bool,
    unit: f32,
) {
    if let Some(sk) = skel {
        for b in &sk.bones {
            if tracks.iter().any(|t| t.name == b.name) {
                continue;
            }
            let Some(node) = scene.node(&b.name) else {
                sink.log(
                    abi::WARNING,
                    &text(&[b"Couldn't find node for bone '", &b.name, b"'\n"]),
                );
                continue;
            };
            let local = node.mtx;
            let inv = matrix_inverse(&b.m);
            let rot_change = matrix_mul(&rotation(&local), &rotation(&inv));
            let trans_change = sub(translation(&local), translation(&b.m));
            tracks.push(Track {
                name: b.name.clone(),
                keys: vec![key(0.0, trans_change, &quat_of(&rot_change))],
                unit_scale: None,
            });
        }
        return;
    }
    for node in &scene.0.nodes {
        if let Some(t) = tracks.iter_mut().find(|t| t.name == node.name) {
            if z_up || unit != 1.0 {
                for k in &mut t.keys {
                    if z_up {
                        let q = Quaternion {
                            v: v3(k.rot[0], k.rot[1], k.rot[2]),
                            w: k.rot[3],
                        };
                        let mut m = Matrixf::default();
                        quat_to_rotation_matrix(&q, &mut m);
                        let q = quat_of(&matrix_mul(&m, &z_to_y()));
                        k.rot = [q.v.x, q.v.y, q.v.z, q.w];
                    }
                    k.trans = [k.trans[0] * unit, k.trans[1] * unit, k.trans[2] * unit];
                }
            }
        } else {
            let mut m = node.mtx;
            let trans = translation(&m);
            set_translation(&mut m, v3(0.0, 0.0, 0.0));
            tracks.push(Track {
                name: node.id.clone(),
                keys: vec![key(0.0, trans, &quat_of(&m))],
                unit_scale: Some(-1.0),
            });
        }
    }
}

fn build_animation(
    sink: Sink,
    c: &Collada,
    skel: Option<&Skel>,
    scene: &Scene,
) -> Option<Vec<Track>> {
    if c.animations.is_empty() {
        return None;
    }
    let mut tracks = Vec::new();
    let (mut start, mut end) = (9_999_999.0f32, 0.0f32);
    for a in &c.animations {
        let Some(t) = create_anim_track(sink, skel, a, scene, c.scene.start) else {
            continue;
        };
        if let (Some(first), Some(last)) = (t.keys.first(), t.keys.last()) {
            if start > first.time {
                start = first.time;
            }
            if end < last.time {
                end = last.time;
            }
        }
        tracks.push(t);
    }
    let _ = (start, end);
    Some(tracks)
}

fn anim_length(c: &Collada, tracks: &[Track]) -> f32 {
    if c.scene.delta >= 0.0 {
        return c.scene.delta;
    }
    let (mut start, mut end) = (9_999_999.0f32, 0.0f32);
    for t in tracks {
        if let (Some(first), Some(last)) = (t.keys.first(), t.keys.last()) {
            if start > first.time {
                start = first.time;
            }
            if end < last.time {
                end = last.time;
            }
        }
    }
    end - start
}

fn emit_track(sink: Sink, t: &Track) {
    sink.anim_track(&t.name, TRANSLATE_ROTATE, &t.keys);
    if let Some(u) = t.unit_scale
        && u >= 0.0
    {
        sink.anim_unit_scale(u);
    }
}

// ---- mesh ----

fn node_from_controller<'a>(
    sink: Sink,
    geom_id: &[u8],
    c: &Collada,
    scene: &Scene<'a>,
) -> Option<&'a Node> {
    let mut control_id: &[u8] = b"";
    let mut guess = false;
    for ctrl in &c.controllers {
        if ctrl.target == geom_id {
            control_id = &ctrl.id;
            guess = false;
        } else if control_id.is_empty() && ctrl.target.is_empty() {
            control_id = &ctrl.id;
            guess = true;
        }
    }
    if guess {
        sink.log(
            abi::WARNING,
            &text(&[
                b"No controller for for geometry ",
                geom_id,
                b", guessing on ",
                control_id,
                b" target = ''\n",
            ]),
        );
    }
    if control_id.is_empty() {
        sink.log(abi::WARNING, b"No controller refered to the geometry!\n");
        return None;
    }
    let node = scene.node_from_source(control_id);
    if node.is_none() {
        sink.log(
            abi::WARNING,
            &text(&[b"No node for controller '", control_id, b"'\n"]),
        );
    }
    node
}

fn material_texture_file(sink: Sink, c: &Collada, material: &[u8]) -> Vec<u8> {
    if material.is_empty() {
        return Vec::new();
    }
    let addr = |s: &[u8]| -> Vec<u8> {
        if s.first() == Some(&b'#') {
            s.get(1..).unwrap_or_default().to_vec()
        } else {
            s.to_vec()
        }
    };
    let tex_id = c
        .materials
        .iter()
        .find(|m| m.id == material)
        .map_or_else(Vec::new, |m| addr(&m.texture));
    if tex_id.is_empty() {
        sink.log(
            abi::WARNING,
            &text(&[b"Material '", material, b"' was not found!\n"]),
        );
        return Vec::new();
    }
    let file_id = c
        .textures
        .iter()
        .find(|t| t.id == tex_id)
        .map_or_else(Vec::new, |t| addr(&t.image));
    if file_id.is_empty() {
        sink.log(
            abi::WARNING,
            &text(&[b"Texture '", &tex_id, b"' was not found!\n"]),
        );
        return Vec::new();
    }
    if let Some(i) = c.images.iter().find(|i| i.id == file_id) {
        return string::file_name(&i.source).to_vec();
    }
    sink.log(
        abi::WARNING,
        &text(&[b"Couldn't file image file id '", &file_id, b"'\n"]),
    );
    Vec::new()
}

struct SubInfo {
    name: Vec<u8>,
    scale: Vector3f,
}

struct ColliderOut {
    kind: i32,
    size: Vector3f,
    m: Matrixf,
    char_collider: bool,
    group: Vec<u8>,
}

fn f32_bytes(v: &[f32]) -> Vec<u8> {
    v.iter().flat_map(|f| f.to_ne_bytes()).collect()
}

fn emit_vertex_buffer(sink: Sink, g: &Geometry) {
    let n = g.vertices.len();
    let mut pos = Vec::with_capacity(n * 4);
    let mut nrm = Vec::with_capacity(n * 3);
    let mut tex = Vec::with_capacity(n * 3);
    let mut col = Vec::with_capacity(n * 4);
    for v in &g.vertices {
        pos.extend_from_slice(&[v.pos[0], v.pos[1], v.pos[2], 1.0]);
        nrm.extend_from_slice(&v.norm);
        tex.extend_from_slice(&v.tex);
        col.extend_from_slice(&[1.0, 1.0, 1.0, 1.0]);
    }
    let mut idx: Vec<u32> = Vec::with_capacity(g.indices.len());
    for (ci, chunk) in g.indices.chunks(3).enumerate() {
        for k in 0..chunk.len() {
            let at = ci * 3 + 2 - k;
            idx.push(g.indices.get(at).copied().unwrap_or(0));
        }
    }
    sink.sub_vb_new(&VbCfg {
        draw_type: 0,
        usage_type: 0,
        reserve_vtx: n as i32,
        reserve_idx: g.indices.len() as i32,
    });
    let array = |kind: i32, elem: i32, data: &[f32]| {
        let bytes = f32_bytes(data);
        sink.sub_vb_array(&Array {
            kind,
            format: 0,
            prog_var: 0,
            elem_num: elem,
            repeats: 0,
            count: data.len(),
            data: bytes.as_ptr(),
            bytes: bytes.len(),
        });
    };
    array(ELEMENT_POSITION, 4, &pos);
    array(ELEMENT_NORMAL, 3, &nrm);
    array(ELEMENT_TEXTURE0, 3, &tex);
    array(ELEMENT_COLOR0, 4, &col);
    array(ELEMENT_TANGENT, 4, &g.tangents);
    sink.sub_vb_indices(&idx);
    sink.sub_vb_compile();
}

fn special_collider(
    sink: Sink,
    name: &[u8],
    geom: &Geometry,
    scene: &Scene,
) -> Result<Option<(bool, Option<ColliderOut>)>, ()> {
    // (create mesh, collider)
    let seps = b"_";
    let parts: Vec<&[u8]> = string::split(name, Some(seps), true).collect();
    let special = string::to_lower(parts.first().ok_or(())?);
    let type_name = parts.get(1).copied().unwrap_or_default();
    if !((special == b"collider" || special == b"charcollider")
        && parts.len() > 1
        && type_name != b"mesh")
    {
        return Ok(Some((true, None)));
    }
    let mut min = v3(100_000.0, 100_000.0, 100_000.0);
    let mut max = v3(-100_000.0, -100_000.0, -100_000.0);
    for v in &geom.vertices {
        let [x, y, z] = v.pos;
        if x < min.x {
            min.x = x;
        }
        if x > max.x {
            max.x = x;
        }
        if y < min.y {
            min.y = y;
        }
        if y > max.y {
            max.y = y;
        }
        if z < min.z {
            min.z = z;
        }
        if z > max.z {
            max.z = z;
        }
    }
    let corners = [
        v3(max.x, max.y, max.z),
        v3(max.x, max.y, min.z),
        v3(max.x, min.y, max.z),
        v3(max.x, min.y, min.z),
        v3(min.x, max.y, max.z),
        v3(min.x, max.y, min.z),
        v3(min.x, min.y, max.z),
        v3(min.x, min.y, min.z),
    ]
    .map(|c| matrix_mul_vec(&identity(), c));
    let (mut bmin, mut bmax) = (corners[0], corners[0]);
    for c in &corners[1..] {
        if c.x < bmin.x {
            bmin.x = c.x;
        } else if c.x > bmax.x {
            bmax.x = c.x;
        }
        if c.y < bmin.y {
            bmin.y = c.y;
        } else if c.y > bmax.y {
            bmax.y = c.y;
        }
        if c.z < bmin.z {
            bmin.z = c.z;
        } else if c.z > bmax.z {
            bmax.z = c.z;
        }
    }
    let size = sub(bmax, bmin);
    let pivot = sub(bmax, scaled(size, 0.5));

    let shape = string::to_lower(type_name);
    let mut kind = SHAPE_BOX;
    let mut shape_size = size;
    match shape.as_slice() {
        b"box" => kind = SHAPE_BOX,
        b"sphere" => {
            kind = SHAPE_SPHERE;
            shape_size = v3(shape_size.x * 0.5, shape_size.y * 0.5, shape_size.z * 0.5);
        }
        b"capsule" => {
            kind = SHAPE_CAPSULE;
            shape_size.x *= 0.5;
        }
        b"cylinder" => {
            kind = SHAPE_CYLINDER;
            shape_size.x *= 0.5;
        }
        _ => {}
    }
    let Some(node) = scene.node_from_source(&geom.id) else {
        sink.log(
            abi::WARNING,
            &text(&[
                b"No node for geometry '",
                &geom.id,
                b"' when creating collider!\n",
            ]),
        );
        return Ok(None);
    };
    let group = scene.parent(node).map_or_else(Vec::new, |p| p.name.clone());
    let wt = translation(&node.mtx_world);
    let center = v3(wt.x + pivot.x, wt.y + pivot.y, wt.z + pivot.z);
    let mut offset = node.mtx_world;
    if center.x != wt.x || center.y != wt.y || center.z != wt.z {
        let off = sub(center, wt);
        let off = v3(
            off.x * node.scale[0],
            off.y * node.scale[1],
            off.z * node.scale[2],
        );
        let rot_off = matrix_mul_vec(&rotation(&offset), off);
        let t = translation(&offset);
        set_translation(
            &mut offset,
            v3(t.x + rot_off.x, t.y + rot_off.y, t.z + rot_off.z),
        );
    }
    let size = v3(
        shape_size.x * node.scale[0],
        shape_size.y * node.scale[1],
        shape_size.z * node.scale[2],
    );
    if kind == SHAPE_CYLINDER || kind == SHAPE_CAPSULE {
        offset = matrix_mul(&offset, &crate::math::rotate_z(to_rad(90.0)));
    }
    Ok(Some((
        false,
        Some(ColliderOut {
            kind,
            size,
            m: offset,
            char_collider: special == b"charcollider",
            group,
        }),
    )))
}

pub fn load_mesh(cfg: &Cfg, sink: Sink, path: &[u8]) -> i32 {
    if try_cache(cfg, sink, path, b"msh", false, |cached| {
        if msh::load_mesh(cfg, sink, cached) == 0 {
            sink.mesh_set_path(path);
            true
        } else {
            false
        }
    }) {
        return 0;
    }
    let flags = cfg.mesh_flags;
    let opts = parse::Opts {
        convert_unit: cfg.convert_unit != 0,
        convert_unit_any: cfg.convert_unit_any != 0,
        images: true,
        textures: true,
        materials: true,
        lights: true,
        geometry: flags & NO_GEOMETRY == 0,
        controllers: true,
        anims: true,
        scene: true,
    };
    let mut c = match read_collada(cfg, sink, path, opts) {
        Read::Ok(c) => c,
        Read::Fail => return 1,
        Read::Fallback => return FALLBACK,
    };
    for g in &mut c.geometries {
        for v in &mut g.vertices {
            let mut n = v3(v.norm[0], v.norm[1], v.norm[2]);
            normalize3(&mut n);
            v.norm = [n.x, n.y, n.z];
        }
    }
    let scene = Scene(&c.scene);
    let mut skel = match build_skeleton(&c, &scene, sink, b"") {
        Ok(s) => s,
        Err(()) => {
            sink.error("Collada controller refers to a missing joint matrix\n");
            return 1;
        }
    };

    sink.mesh_new(path, path);
    let mut subs: Vec<SubInfo> = Vec::new();
    let mut colliders: Vec<ColliderOut> = Vec::new();
    let unit = c.unit_scale;
    // SAFETY: the config strings live for the call.
    let fast = unsafe { cfg.fastload_material.bytes() };
    for (gi, geom) in c.geometries.iter().enumerate() {
        let node = match scene.node_from_source(&geom.id) {
            Some(n) => n,
            None => match node_from_controller(sink, &geom.id, &c, &scene) {
                Some(n) => n,
                None => {
                    sink.log(
                        abi::ERROR,
                        &text(&[b"No node with geometry id '", &geom.id, b"'\n"]),
                    );
                    continue;
                }
            },
        };
        let node_name: &[u8] = if node.name.is_empty() {
            &node.id
        } else {
            &node.name
        };
        let mut collide = false;
        if node_name.first() == Some(&b'_') {
            collide = true;
            match special_collider(sink, node_name, geom, &scene) {
                Err(()) => {
                    sink.error("Collada node name has no parts\n");
                    sink.mesh_drop();
                    return 1;
                }
                Ok(None) => continue,
                Ok(Some((create, coll))) => {
                    if let Some(co) = coll {
                        colliders.push(co);
                    }
                    if !create {
                        continue;
                    }
                }
            }
        }
        sink.sub_new(
            node_name,
            Some(&flat(&node.mtx)),
            Some(&node.scale),
            Some(collide),
        );
        subs.push(SubInfo {
            name: node_name.to_vec(),
            scale: v3(node.scale[0], node.scale[1], node.scale[2]),
        });
        emit_vertex_buffer(sink, geom);
        sink.sub_vb_attach();

        let node_material: &[u8] = if node.instance_material.is_empty() {
            &geom.material
        } else {
            &node.instance_material
        };
        let mut mat_name = material_texture_file(sink, &c, node_material);
        if mat_name.is_empty() {
            sink.sub_material(b"", 0);
        } else {
            // SAFETY: the builder keeps its answer until the next call; copied by `relative_dir`.
            let rel = sink.relative_dir(path);
            mat_name = string::set_file_path(&mat_name, &rel);
            let recorded = string::set_file_ext(&mat_name, b"mat");
            if cfg.use_fastload != 0 {
                mat_name = fast.to_vec();
            }
            let mut handle = 0;
            if flags & NO_MATERIAL == 0 && !mat_name.is_empty() {
                handle = sink.material_create(&mat_name);
                if handle == 0 {
                    sink.log(
                        abi::ERROR,
                        &text(&[
                            b"Couldn't create material '",
                            &mat_name,
                            b"' for object '",
                            &geom.name,
                            b"'\n",
                        ]),
                    );
                }
            }
            sink.sub_material(&recorded, handle);
        }

        let mut ctrl: Option<&Controller> = c.controllers.iter().find(|k| k.target == geom.id);
        if ctrl.is_none() {
            ctrl = c.controllers.iter().find(|k| k.target.is_empty());
            if ctrl.is_some() {
                sink.log(abi::WARNING, b"Guessing controller as target=''!\n");
            }
        }
        if let (Some(k), Some(sk)) = (ctrl, skel.as_ref()) {
            if !k.pairs.is_empty() {
                sink.error("Skinned Collada geometry without an MSH cache cannot be loaded\n");
                sink.mesh_drop();
                return 1;
            }
            let bind = k.bind.map_or_else(identity, |b| mat(&b));
            let _ = sk;
            sink.sub_vb_transform(&flat(&matrix_mul(
                &matrix_scale(v3(unit, unit, unit)),
                &bind,
            )));
        } else {
            let s = v3(
                node.scale[0] * unit,
                node.scale[1] * unit,
                node.scale[2] * unit,
            );
            sink.sub_vb_transform(&flat(&matrix_scale(s)));
        }
        sink.sub_compile();
        sink.sub_vb_compile();
        let _ = gi;
    }

    // colliders
    if !colliders.is_empty() && !subs.is_empty() {
        for co in &colliders {
            let at = if co.group.is_empty() {
                Some(0)
            } else {
                subs.iter().position(|s| s.name == co.group)
            };
            let Some(at) = at else {
                sink.log(
                    abi::LOG,
                    &text(&[b"Sub mesh '", &co.group, b"' for collider was not found!\n"]),
                );
                continue;
            };
            sink.sub_select(at as i32);
            sink.sub_collider(&Collider {
                m: flat(&co.m),
                size: [co.size.x, co.size.y, co.size.z],
                kind: co.kind,
                char_collider: co.char_collider.into(),
            });
        }
    }

    // node hierarchy
    if skel.is_none() {
        let mut out: Vec<(i32, Vec<u8>, Matrixf, i32)> = Vec::new();
        fn walk(
            scene: &Scene,
            at: usize,
            n: &Node,
            parent: i32,
            unit: f32,
            out: &mut Vec<(i32, Vec<u8>, Matrixf, i32)>,
        ) {
            let name = if n.name.is_empty() {
                n.id.clone()
            } else {
                n.name.clone()
            };
            let flags = i32::from(!n.source.is_empty() && !n.source_is_light);
            let mut m = n.mtx;
            let t = translation(&m);
            set_translation(&mut m, scaled(t, unit));
            let me = out.len() as i32;
            out.push((parent, name, m, flags));
            for (c, cn) in scene.children(at) {
                walk(scene, c, cn, me, unit, out);
            }
        }
        for (i, n) in scene.roots() {
            walk(&scene, i, n, -1, unit, &mut out);
        }
        for (parent, name, m, flags) in &mut out {
            let mut node_m = *m;
            if let Some(s) = subs.iter().find(|s| s.name == *name) {
                node_m = matrix_mul(&node_m, &matrix_inverse(&matrix_scale(s.scale)));
            } else if !name.is_empty() && name.first() != Some(&b'_') && *flags != 0 {
                sink.log(
                    abi::ERROR,
                    &text(&[b"Cannot find submesh '", name, b"' in node!\n"]),
                );
            }
            sink.node(*parent, name, &flat(&node_m), *flags);
        }
    }

    // animation
    let mut tracks = build_animation(sink, &c, skel.as_ref(), &scene);
    if let Some(t) = &mut tracks {
        clean_up_non_animated(sink, t, skel.as_ref(), &scene, c.z_to_y, unit);
    }

    // skeleton scale / rotation, applied to the animation tracks too
    if let Some(sk) = &mut skel {
        if cfg.unscaled_skeleton != 0 && (c.z_to_y || unit != 1.0) {
            let rot = if c.z_to_y { z_to_y() } else { identity() };
            for r in sk.roots() {
                sk.unscale(r, unit, &rot, true);
            }
            for t in tracks.iter_mut().flatten() {
                for k in &mut t.keys {
                    k.trans = [k.trans[0] * unit, k.trans[1] * unit, k.trans[2] * unit];
                }
            }
        } else if c.z_to_y || unit != 1.0 {
            let mut m = matrix_scale(v3(unit, unit, unit));
            if c.z_to_y {
                m = matrix_mul(&m, &z_to_y());
            }
            for r in sk.roots() {
                if let Some(b) = sk.bones.get_mut(r) {
                    b.m = matrix_mul(&m, &b.m);
                }
                let idx = sk.bones.get(r).and_then(|b| sk.by_sid(&b.sid));
                if let Some(t) = idx.and_then(|i| tracks.as_mut().and_then(|t| t.get_mut(i))) {
                    for k in &mut t.keys {
                        k.trans = [k.trans[0] * unit, k.trans[1] * unit, k.trans[2] * unit];
                    }
                }
            }
        }
    }
    if let Some(sk) = &skel {
        emit_skeleton(sink, sk);
    }
    if let Some(t) = &tracks {
        sink.anim_new(
            path,
            string::file_name(path),
            Some(b"Default"),
            anim_length(&c, t),
            t.len(),
        );
        for tr in t {
            emit_track(sink, tr);
        }
        sink.anim_end(true);
    }
    sink.mesh_compile();
    if cfg.load_and_save_msh != 0 {
        let cache = external_cache_file(sink, cfg, path);
        sink.save_mesh(&cache);
    }
    0
}

fn collada_cache_newer(sink: Sink, path: &[u8]) -> bool {
    let cache = string::set_file_ext(path, b"dae_cache");
    sink.file_exists(&cache)
        && sink.file_exists(path)
        && sink.file_date(&cache) > sink.file_date(path)
}

fn hpl3_top_nodes(xml: &[u8]) -> Vec<Vec<u8>> {
    let mut top = Vec::new();
    let mut pos = find(xml, b"<visual_scene", 0);
    let mut depth = 0i32;
    while let Some(p) = pos.and_then(|p| find(xml, b"node", p)) {
        let at = |i: usize| xml.get(i).copied();
        let close = p > 1 && at(p - 1) == Some(b'/') && at(p - 2) == Some(b'<');
        let open = p > 0 && at(p - 1) == Some(b'<') && matches!(at(p + 4), Some(b' ' | b'>'));
        let Some(end) = find(xml, b">", p) else { break };
        if close {
            depth -= 1;
        } else if open {
            depth += 1;
            if depth == 1
                && let Some(name) = find(xml, b"name=\"", p)
                && name < end
            {
                let from = name + 6;
                let to = find(xml, b"\"", from).unwrap_or(xml.len());
                top.push(xml.get(from..to).unwrap_or_default().to_vec());
            }
            if at(end - 1) == Some(b'/') {
                depth -= 1;
            }
        }
        pos = Some(end);
    }
    top
}

fn hpl3_anim(cfg: &Cfg, sink: Sink, path: &[u8]) -> Option<(f32, Vec<Track>)> {
    let anm = string::set_file_ext(path, b"anm");
    let magic = std::fs::File::open(OsStr::from_bytes(&anm))
        .ok()
        .and_then(|mut f| {
            let mut m = [0u8; 4];
            std::io::Read::read_exact(&mut f, &mut m).ok().map(|()| m)
        });
    if magic.as_ref() != Some(b"iE\x03v") {
        return None;
    }
    let (length, raw) = match read_hpl3_anm(&anm) {
        Ok(Some(a)) => a,
        Ok(None) => return None,
        Err(()) => {
            sink.log(abi::ERROR, &text(&[b"Corrupt animation '", &anm, b"'\n"]));
            return None;
        }
    };
    let mut tracks: Vec<Track> = raw
        .into_iter()
        .map(|(name, keys)| Track {
            name,
            keys,
            unit_scale: None,
        })
        .collect();
    let xml = std::fs::read(OsStr::from_bytes(path)).unwrap_or_default();
    let mut unit = 1.0f32;
    if let Some(u) = find(&xml, b"<unit meter=\"", 0) {
        unit = atof(xml.get(u + 13..).unwrap_or_default()) as f32;
    }
    let top = hpl3_top_nodes(&xml);
    if unit > 0.0 && unit != 1.0 && cfg.unscaled_skeleton == 0 {
        for t in tracks.iter_mut().filter(|t| !top.contains(&t.name)) {
            for k in &mut t.keys {
                k.trans = [k.trans[0] / unit, k.trans[1] / unit, k.trans[2] / unit];
            }
        }
    }
    Some((length, tracks))
}

pub fn load_anim(cfg: &Cfg, sink: Sink, path: &[u8]) -> i32 {
    let file = string::file_name(path);
    if let Some((length, tracks)) = hpl3_anim(cfg, sink, path) {
        sink.anim_new(file, file, Some(b"Default"), length, tracks.len());
        for t in &tracks {
            emit_track(sink, t);
        }
        sink.anim_end(false);
        return 0;
    }
    if try_cache(cfg, sink, path, b"anm", true, |cached| {
        if msh::load_anim(sink, cached) == 0 {
            sink.anim_set_path(path);
            true
        } else {
            false
        }
    }) {
        return 0;
    }
    let opts = parse::Opts {
        convert_unit: cfg.convert_unit != 0,
        convert_unit_any: cfg.convert_unit_any != 0,
        geometry: true,
        controllers: true,
        anims: true,
        scene: true,
        ..parse::Opts::default()
    };
    let c = match read_collada(cfg, sink, path, opts) {
        Read::Ok(c) => c,
        Read::Fail => return 1,
        Read::Fallback => return FALLBACK,
    };
    let scene = Scene(&c.scene);
    let skel = match build_skeleton(&c, &scene, sink, b".") {
        Ok(s) => s,
        Err(()) => {
            sink.error("Collada controller refers to a missing joint matrix\n");
            return 1;
        }
    };
    let Some(mut tracks) = build_animation(sink, &c, skel.as_ref(), &scene) else {
        return 1;
    };
    clean_up_non_animated(
        sink,
        &mut tracks,
        skel.as_ref(),
        &scene,
        c.z_to_y,
        c.unit_scale,
    );
    let unit = c.unit_scale;
    if let Some(sk) = &skel {
        let scale = |t: &mut Track| {
            for k in &mut t.keys {
                k.trans = [k.trans[0] * unit, k.trans[1] * unit, k.trans[2] * unit];
            }
        };
        if cfg.unscaled_skeleton != 0 && unit != 1.0 {
            tracks.iter_mut().for_each(scale);
        } else if c.z_to_y || unit != 1.0 {
            for r in sk.roots() {
                let idx = sk.bones.get(r).and_then(|b| sk.by_sid(&b.sid));
                if let Some(t) = idx.and_then(|i| tracks.get_mut(i)) {
                    scale(t);
                }
            }
        }
    }
    sink.anim_new(path, file, None, anim_length(&c, &tracks), tracks.len());
    for t in &tracks {
        emit_track(sink, t);
    }
    sink.anim_end(false);
    if cfg.load_and_save_msh != 0 {
        let cache = string::set_file_ext(&external_cache_file(sink, cfg, path), b"anm");
        sink.save_anim(&cache);
    }
    0
}
