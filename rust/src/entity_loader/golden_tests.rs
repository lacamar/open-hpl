//! Replays `rust/tests/golden/H3/ent.txt`: call-stream digests of the pre-port C++ `.ent` parser
//! (`harness/ent_probe.cpp`) over every `.ent` of the Steam game data; skipped where the data is absent.
//! The recorder mirrors the probe's builder: same answers, same text.

use super::*;
use crate::types::{Matrixf, Vector3f};
use crate::xml::{DOC, Doc};
use std::path::{Path, PathBuf};

const GAMES: [(&str, &str); 4] = [
    ("soma", "SOMA"),
    ("amnesia", "Amnesia The Dark Descent"),
    ("rebirth", "Amnesia Rebirth"),
    ("bunker", "Amnesia The Bunker"),
];
const BASIS: u64 = 1_469_598_103_934_665_603;

fn fnv(bytes: &[u8]) -> u64 {
    bytes.iter().fold(BASIS, |h, &b| {
        (h ^ u64::from(b)).wrapping_mul(1_099_511_628_211)
    })
}

fn f(x: f32) -> String {
    if x.is_nan() {
        format!("nan:{:08x}", x.to_bits())
    } else {
        format!("{:08x}", x.to_bits())
    }
}

/// Drops NaN bits: debug and release builds order NaN operands differently (the harness compares them raw).
fn without_nan_bits(log: &[u8]) -> Vec<u8> {
    let mut out = Vec::with_capacity(log.len());
    let mut i = 0;
    while i < log.len() {
        if log.get(i..i + 4) == Some(b"nan:") && i + 12 <= log.len() {
            out.extend_from_slice(b"nan");
            i += 12;
        } else {
            out.extend(log.get(i));
            i += 1;
        }
    }
    out
}

fn v3(v: Vector3f) -> String {
    format!("{},{},{}", f(v.x), f(v.y), f(v.z))
}

fn m16(m: &Matrixf) -> String {
    m.m.iter()
        .flatten()
        .map(|&x| f(x))
        .collect::<Vec<_>>()
        .join(",")
}

fn b(x: bool) -> &'static str {
    if x { "1" } else { "0" }
}

fn sum(s: &[u8]) -> u32 {
    s.iter()
        .fold(0u32, |h, &c| h.wrapping_mul(31).wrapping_add(u32::from(c)))
}

fn text(s: &[u8]) -> String {
    String::from_utf8_lossy(s).into_owned()
}

const ID: Matrixf = Matrixf {
    m: [
        [1.0, 0.0, 0.0, 0.0],
        [0.0, 1.0, 0.0, 0.0],
        [0.0, 0.0, 1.0, 0.0],
        [0.0, 0.0, 0.0, 1.0],
    ],
};

struct Rec<'a> {
    doc: &'a Doc,
    full: bool,
    log: Vec<u8>,
    calls: usize,
    next: usize,
}

impl Rec<'_> {
    fn put(&mut self, s: impl AsRef<[u8]>) {
        self.log.extend_from_slice(s.as_ref());
        self.log.push(b'\n');
        self.calls += 1;
    }

    fn new_handle(&mut self) -> Option<Handle> {
        self.next += 1;
        Handle::new(self.next)
    }

    fn maybe(&mut self, s: &[u8], modulus: u32) -> Option<Handle> {
        if sum(s).checked_rem(modulus) == Some(0) {
            None
        } else {
            self.new_handle()
        }
    }
}

fn n(h: Handle) -> usize {
    h.get()
}

impl Builder for Rec<'_> {
    fn error(&mut self, m: &[u8]) {
        self.put([b"error ", m].concat());
    }
    fn warning(&mut self, m: &[u8]) {
        self.put([b"warning ", m].concat());
    }
    fn before_load(&mut self) {
        self.put("before_load");
    }
    fn after_load(&mut self) {
        self.put("after_load");
    }
    fn before_joints(&mut self) -> bool {
        self.put("before_joints");
        self.full
    }
    fn flags(&mut self) -> Flags {
        let x = self.full;
        Flags {
            animations: x,
            particle_systems: x,
            billboards: x,
            sounds: x,
            lights: x,
            sub_mesh_scale_includes_model_scale: x,
            sub_mesh_instance_scale_after_rotation: x,
            sub_mesh_materials: x,
            share_body_shapes: x,
        }
    }

    fn create_mesh(&mut self, file: &[u8]) -> Option<MeshInfo> {
        let h = sum(file);
        let info = MeshInfo {
            skeleton: h & 1 != 0,
            anim_num: i32::try_from((h >> 1).checked_rem(3).unwrap_or(0)).unwrap_or(0),
        };
        self.put(
            [
                b"create_mesh ",
                file,
                format!(" -> {} {}", b(info.skeleton), info.anim_num).as_bytes(),
            ]
            .concat(),
        );
        (h.checked_rem(7) != Some(0)).then_some(info)
    }
    fn create_mesh_entity(&mut self, name: &[u8]) -> Option<Handle> {
        self.put([b"mesh_entity ", name].concat());
        self.new_handle()
    }
    fn set_matrix(&mut self, e: Handle, m: &Matrixf) {
        self.put(format!("set_matrix {} {}", n(e), m16(m)));
    }
    fn set_shadow_caster(&mut self, e: Handle) {
        self.put(format!("shadow_caster {}", n(e)));
    }
    fn update_logic(&mut self, e: Handle) {
        self.put(format!("update_logic {}", n(e)));
    }
    fn get_sub_mesh_entity(&mut self, name: &[u8]) -> Option<Handle> {
        self.put([b"get_sub_mesh ", name].concat());
        self.maybe(name, 4)
    }
    fn warn_missing_sub_mesh(&mut self, name: &[u8]) {
        self.put([b"warn_missing_sub_mesh ", name].concat());
    }
    fn get_sub_mesh_model_scale(&mut self, s: Handle) -> Vector3f {
        self.put(format!("sub_mesh_model_scale {}", n(s)));
        Vector3f {
            x: 1.5,
            y: 1.0,
            z: 0.5,
        }
    }
    fn set_world_matrix(&mut self, e: Handle, m: &Matrixf) {
        self.put(format!("set_world_matrix {} {}", n(e), m16(m)));
    }
    fn set_unique_id(&mut self, e: Handle, id: i32) {
        self.put(format!("set_unique_id {} {id}", n(e)));
    }
    fn set_sub_mesh_material(&mut self, s: Handle, file: &[u8]) {
        self.put([format!("sub_mesh_material {} ", n(s)).as_bytes(), file].concat());
    }

    fn add_animation(&mut self, file: &[u8], name: &[u8], speed: f32) -> Option<Handle> {
        self.put(
            [
                b"add_animation ",
                file,
                b" ",
                name,
                b" ",
                f(speed).as_bytes(),
            ]
            .concat(),
        );
        self.maybe(&[name, file].concat(), 5)
    }
    fn set_special_event_time(&mut self, s: Handle, t: f32) {
        self.put(format!("special_event_time {} {}", n(s), f(t)));
    }
    fn add_animation_event(
        &mut self,
        s: Handle,
        t: f32,
        ty: AnimEventType,
        value: &[u8],
        name: &[u8],
        dest: &[u8],
    ) {
        self.put(
            [
                format!("anim_event {} {} {} ", n(s), f(t), ty as i32).as_bytes(),
                value,
                b" ",
                name,
                b" ",
                dest,
            ]
            .concat(),
        );
    }

    fn load_world_entity(&mut self, kind: WorldEntity, elem: u32, prefix: &[u8]) -> Option<Handle> {
        let name = self.doc.node(elem).value.clone();
        self.put(
            [
                format!("world_entity {} ", kind as i32).as_bytes(),
                &name,
                b" ",
                prefix,
            ]
            .concat(),
        );
        self.maybe(prefix, 6)
    }
    fn get_local_position(&mut self, e: Handle) -> Vector3f {
        self.put(format!("local_position {}", n(e)));
        Vector3f {
            x: 1.0,
            y: 2.0,
            z: 3.0,
        }
    }
    fn set_position(&mut self, e: Handle, p: Vector3f) {
        self.put(format!("set_position {} {}", n(e), v3(p)));
    }
    fn link_light_billboards(&mut self) {
        self.put("link_light_billboards");
    }

    fn get_bone_state(&mut self, name: &[u8]) -> Option<Handle> {
        self.put([b"bone_state ", name].concat());
        self.maybe(name, 3)
    }
    fn get_bone_state_num(&mut self) -> i32 {
        self.put("bone_state_num");
        3
    }
    fn warn_skeleton_mismatch(&mut self) {
        self.put("warn_skeleton_mismatch");
    }
    fn get_bone_world_matrix(&mut self, bone: Handle) -> Matrixf {
        self.put(format!("bone_world_matrix {}", n(bone)));
        ID
    }
    fn get_bone_name(&mut self, bone: Handle) -> Vec<u8> {
        self.put(format!("bone_name {}", n(bone)));
        format!("bone{}", n(bone)).into_bytes()
    }

    fn get_unique_id(&mut self, e: Handle) -> i32 {
        self.put(format!("get_unique_id {}", n(e)));
        i32::try_from(n(e) * 10).unwrap_or(0)
    }
    fn is_sub_mesh(&mut self, e: Handle) -> bool {
        self.put(format!("is_sub_mesh {}", n(e)));
        n(e) & 1 != 0
    }
    fn get_local_matrix(&mut self, e: Handle) -> Matrixf {
        self.put(format!("local_matrix {}", n(e)));
        ID
    }
    fn get_world_matrix(&mut self, e: Handle) -> Matrixf {
        self.put(format!("world_matrix {}", n(e)));
        ID
    }
    fn get_entity_name(&mut self, e: Handle) -> Vec<u8> {
        self.put(format!("entity_name {}", n(e)));
        format!("ent{}", n(e)).into_bytes()
    }
    fn set_node_animation(&mut self) {
        self.put("set_node_animation");
    }
    fn attach_entity_child(&mut self, p: Handle, m: &Matrixf, c: Handle) {
        self.put(format!("attach_entity {} {} {}", n(p), m16(m), n(c)));
    }
    fn attach_bone_child(&mut self, bone: Handle, m: &Matrixf, c: Handle) {
        self.put(format!("attach_bone {} {} {}", n(bone), m16(m), n(c)));
    }
    fn attach_bone_to_body(&mut self, body: Handle, m: &Matrixf, bone: Handle) {
        self.put(format!(
            "attach_bone_body {} {} {}",
            n(body),
            m16(m),
            n(bone)
        ));
    }

    fn create_box_shape(&mut self, s: Vector3f, m: &Matrixf) -> Option<Handle> {
        self.put(format!("box {} {}", v3(s), m16(m)));
        self.new_handle()
    }
    fn create_sphere_shape(&mut self, s: Vector3f, m: &Matrixf) -> Option<Handle> {
        self.put(format!("sphere {} {}", v3(s), m16(m)));
        self.new_handle()
    }
    fn create_cylinder_shape(&mut self, r: f32, h: f32, m: &Matrixf) -> Option<Handle> {
        self.put(format!("cylinder {} {} {}", f(r), f(h), m16(m)));
        self.new_handle()
    }
    fn create_capsule_shape(&mut self, r: f32, h: f32, m: &Matrixf) -> Option<Handle> {
        self.put(format!("capsule {} {} {}", f(r), f(h), m16(m)));
        self.new_handle()
    }
    fn create_compound_shape(&mut self, shapes: &[Handle]) -> Option<Handle> {
        let list: String = shapes.iter().map(|&s| format!(" {}", n(s))).collect();
        self.put(format!("compound{list}"));
        self.new_handle()
    }
    fn inc_shape_user_count(&mut self, s: Handle) {
        self.put(format!("inc_shape {}", n(s)));
    }
    fn destroy_shape(&mut self, s: Handle) {
        self.put(format!("destroy_shape {}", n(s)));
    }

    fn create_body(&mut self, name: &[u8], shape: Handle) -> Option<Handle> {
        self.put([b"body ", name, format!(" {}", n(shape)).as_bytes()].concat());
        self.new_handle()
    }
    fn set_body_properties(&mut self, body: Handle, p: &BodyProps) {
        self.put(format!(
            "body_props {} {} {} {} {} {} {} {} {}{}{}{}{}{}{}{}{} {}",
            n(body),
            m16(&p.matrix),
            f(p.mass),
            f(p.angular_damping),
            f(p.linear_damping),
            f(p.buoyancy_density_mul),
            f(p.max_angular_speed),
            f(p.max_linear_speed),
            b(p.blocks_sound),
            b(p.collide_character),
            b(p.collide),
            b(p.gravity),
            b(p.continuous_collision),
            b(p.pushed_by_character_gravity),
            b(p.volatile),
            b(p.use_surface_effects),
            b(p.can_attach_character),
            p.id
        ));
    }
    fn set_body_material(&mut self, body: Handle, name: &[u8]) {
        self.put([format!("body_material {} ", n(body)).as_bytes(), name].concat());
    }
    fn register_body(&mut self, body: Handle) {
        self.put(format!("register_body {}", n(body)));
    }

    fn create_joint(
        &mut self,
        kind: JointKind,
        name: &[u8],
        pivot: Vector3f,
        pin: Vector3f,
        parent: Option<Handle>,
        child: Handle,
    ) -> Option<Handle> {
        let k = match kind {
            JointKind::Hinge => "hinge",
            JointKind::Ball => "ball",
            JointKind::Slider => "slider",
            JointKind::Screw => "screw",
        };
        self.put(
            [
                format!("joint_{k} ").as_bytes(),
                name,
                format!(
                    " {} {} {} {}",
                    v3(pivot),
                    v3(pin),
                    parent.map_or(0, n),
                    n(child)
                )
                .as_bytes(),
            ]
            .concat(),
        );
        self.new_handle()
    }
    fn set_hinge_limits(&mut self, j: Handle, a: f32, c: f32) {
        self.put(format!("hinge_limits {} {} {}", n(j), f(a), f(c)));
    }
    fn set_ball_limits(&mut self, j: Handle, a: f32, c: f32) {
        self.put(format!("ball_limits {} {} {}", n(j), f(a), f(c)));
    }
    fn set_slider_limits(&mut self, j: Handle, a: f32, c: f32) {
        self.put(format!("slider_limits {} {} {}", n(j), f(a), f(c)));
    }
    fn set_screw_limits(&mut self, j: Handle, a: f32, c: f32) {
        self.put(format!("screw_limits {} {} {}", n(j), f(a), f(c)));
    }
    fn set_joint_properties(&mut self, j: Handle, p: &JointProps) {
        let sounds = [
            &p.move_sound,
            &p.break_sound,
            &p.max_limit_sound,
            &p.min_limit_sound,
        ]
        .iter()
        .map(|s| text(s))
        .collect::<Vec<_>>()
        .join("|");
        let floats = [
            p.min_move_speed,
            p.min_move_freq,
            p.min_move_volume,
            p.min_move_freq_speed,
            p.max_move_freq,
            p.max_move_volume,
            p.max_move_freq_speed,
            p.middle_move_speed,
            p.middle_move_volume,
            p.break_force,
            p.limit_auto_sleep_dist,
            p.max_limit_max_speed,
            p.max_limit_min_speed,
            p.min_limit_max_speed,
            p.min_limit_min_speed,
        ]
        .map(f)
        .join(" ");
        let bools: String = [
            p.angular_move,
            p.sticky_min_limit,
            p.sticky_max_limit,
            p.breakable,
            p.limit_auto_sleep,
            p.collide_bodies,
        ]
        .map(b)
        .concat();
        // The probe prints the sound strings raw; non-UTF-8 bytes are not expected in these fields.
        self.put(format!(
            "joint_props {} {sounds} {floats} {bools} {} {}",
            n(j),
            p.limit_auto_sleep_num_steps,
            p.id
        ));
    }
    fn register_joint(&mut self, j: Handle) {
        self.put(format!("register_joint {}", n(j)));
    }

    fn get_animation_state_num(&mut self) -> i32 {
        self.put("animation_state_num");
        2
    }
    fn play_first_animation(&mut self) {
        self.put("play_first_animation");
    }
    fn set_skeleton_physics_active(&mut self) {
        self.put("skeleton_physics_active");
    }
    fn load_user_variables(&mut self) {
        self.put("load_user_variables");
    }
}

fn game_root(key: &str) -> Option<PathBuf> {
    let base = std::env::var_os("OHPL_H3_CORPUS").map_or_else(
        || {
            Path::new(&std::env::var_os("HOME").unwrap_or_default())
                .join(".local/share/Steam/steamapps/common")
        },
        PathBuf::from,
    );
    let root = base.join(GAMES.iter().find(|g| g.0 == key)?.1);
    root.is_dir().then_some(root)
}

/// `(calls, digest)` of one configuration, as `ent_probe` prints it.
fn digest(doc: &Doc, root: u32, path: &str, full: bool) -> (usize, u64) {
    let slash = path.rfind('/').map_or(0, |i| i + 1);
    let base = path.get(slash..).unwrap_or(path);
    let name = base
        .rfind('.')
        .map_or(base, |i| base.get(..i).unwrap_or(base));
    let dir = path.get(..slash.saturating_sub(1)).unwrap_or("");
    let p = Params {
        name: name.as_bytes(),
        file_name: base.as_bytes(),
        full_path: path.as_bytes(),
        full_path_dir: dir.as_bytes(),
        transform: ID,
        scale: Vector3f {
            x: 1.0,
            y: 1.0,
            z: 1.0,
        },
        physics_world: full,
    };
    let mut rec = Rec {
        doc,
        full,
        log: Vec::new(),
        calls: 0,
        next: 0,
    };
    let done = parse(&mut rec, doc, root, &p);
    rec.log
        .extend_from_slice(if done { b"result 1\n" } else { b"result 0\n" });
    if let Some(d) = std::env::var_os("OHPL_H3_DUMP") {
        std::fs::write(d, &rec.log).ok();
    }
    (rec.calls, fnv(&without_nan_bits(&rec.log)))
}

#[test]
fn ent_parse_matches_cpp() {
    let golden = std::fs::read_to_string(
        Path::new(env!("CARGO_MANIFEST_DIR")).join("tests/golden/H3/ent.txt"),
    )
    .unwrap_or_default();
    let (mut files, mut calls) = (0usize, 0usize);
    for line in golden.lines() {
        let f: Vec<&str> = line.split('\t').collect();
        let (Some(&head), Some(_)) = (f.first(), f.get(1)) else {
            panic!("bad line {line}");
        };
        let (game, rel) = head.split_once(':').unwrap_or_default();
        let Some(root) = game_root(game) else {
            continue;
        };
        let full = root.join(rel);
        let data = std::fs::read(&full).unwrap_or_default();
        let mut doc = Doc::new();
        let ok = doc.load(&data, true);
        let Some(top) = ok.then(|| doc.first_child_element(DOC, None)).flatten() else {
            assert_eq!(f.get(1), Some(&"noroot"), "{head}");
            continue;
        };
        let path = format!("{}/{rel}", root.display());
        for cfg in 0..2 {
            let (c, d) = digest(&doc, top, &path, cfg == 0);
            let want = (f.get(1 + 2 * cfg), f.get(2 + 2 * cfg));
            assert_eq!(
                want,
                (
                    Some(&c.to_string().as_str()),
                    Some(&format!("{d:016x}").as_str())
                ),
                "{head} cfg {cfg}"
            );
            calls += c;
        }
        files += 1;
    }
    eprintln!("ent golden: {files} files, {calls} calls");
}
