use super::abi::recorder::{Recorder, builder};
use super::abi::{Cfg, Sink, Str};
use super::{collada, msh};
use std::path::{Path, PathBuf};

fn cfg() -> Cfg {
    Cfg {
        bone_unit_scale: 1,
        keep_hpl3_anims: 0,
        convert_unit: 1,
        convert_unit_any: 0,
        unscaled_skeleton: 0,
        load_and_save_msh: 0,
        force_cache_skip_saving: 0,
        use_fastload: 0,
        mesh_flags: 0,
        mesh_cache_dir: Str::NONE,
        working_dir: Str::NONE,
        fastload_material: Str::NONE,
    }
}

fn scratch(name: &str) -> PathBuf {
    let dir = Path::new(env!("CARGO_MANIFEST_DIR")).join("target/l6-tests");
    std::fs::create_dir_all(&dir).unwrap();
    dir.join(name)
}

#[derive(Default)]
struct W(Vec<u8>);

impl W {
    fn i32(&mut self, v: i32) -> &mut Self {
        self.0.extend(v.to_le_bytes());
        self
    }
    fn i16(&mut self, v: i16) -> &mut Self {
        self.0.extend(v.to_le_bytes());
        self
    }
    fn u8(&mut self, v: u8) -> &mut Self {
        self.0.push(v);
        self
    }
    fn f32(&mut self, v: f32) -> &mut Self {
        self.0.extend(v.to_le_bytes());
        self
    }
    fn f32s(&mut self, v: &[f32]) -> &mut Self {
        v.iter().for_each(|&x| {
            self.f32(x);
        });
        self
    }
    fn str(&mut self, s: &str) -> &mut Self {
        self.0.extend(s.as_bytes());
        self.0.push(0);
        self
    }
    fn mat(&mut self) -> &mut Self {
        self.f32s(&[
            1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1.,
        ])
    }
    fn array(&mut self, kind: i16, format: i16, elem: i32, data: &[f32]) -> &mut Self {
        self.i16(kind).i16(format).i32(0).i32(elem).f32s(data)
    }
}

fn tiny_msh() -> Vec<u8> {
    let mut w = W::default();
    w.i32(0x7603_4569).i32(7).i32(1).i32(1).u8(1);
    w.i32(1).str("root").str("sid").mat().f32(0.25).i32(0);
    w.i32(1).str("node").mat().i32(7).i32(0);
    w.str("sub").str("mat.mat").mat().f32s(&[1., 2., 3.]).u8(1);
    w.i32(1).i16(2).mat().f32s(&[1., 1., 1.]).u8(0);
    w.i32(1).i32(0).i32(1).f32(0.5);
    w.i32(2).i32(2);
    w.array(1, 0, 3, &[0., 0., 0., 1., 0., 0.]);
    w.array(0, 0, 3, &[0., 0., 1., 0., 0., 1.]);
    w.i32(3).i32(0).i32(1).i32(2);
    w.i32(1).str("anim").f32(2.0).i32(1);
    w.str("track").i16(3).i32(1);
    w.f32(1.0).f32s(&[1., 2., 3., 0., 0., 0., 1.]);
    w.0
}

const QUAD: &str = r##"<?xml version="1.0"?>
<COLLADA>
 <asset><unit meter="0.01" name="cm"/><up_axis>Z_UP</up_axis></asset>
 <library_images><image id="img" name="img"><init_from>a.png</init_from></image></library_images>
 <library_effects><effect id="fx"><profile_COMMON><newparam sid="s"><surface type="2D"><init_from>img</init_from></surface></newparam>
  <newparam sid="smp"><sampler2D><source>s</source></sampler2D></newparam>
  <technique sid="t"><phong><diffuse><texture texture="smp" texcoord="UV"/></diffuse></phong></technique></profile_COMMON></effect></library_effects>
 <library_materials><material id="m" name="mat"><instance_effect url="#fx"/></material></library_materials>
 <library_geometries><geometry id="g-lib" name="quad"><mesh>
  <source id="pos"><float_array id="pa" count="12">0 0 0 1 0 0 1 1 0 0 1 0</float_array>
   <technique_common><accessor source="#pa" count="4" stride="3"/></technique_common></source>
  <source id="nrm"><float_array id="na" count="3">0 0 1</float_array>
   <technique_common><accessor source="#na" count="1" stride="3"/></technique_common></source>
  <source id="uv"><float_array id="ua" count="8">0 0 1 0 1 1 0 1</float_array>
   <technique_common><accessor source="#ua" count="4" stride="2"/></technique_common></source>
  <vertices id="v"><input semantic="POSITION" source="#pos"/></vertices>
  <triangles count="2" material="m">
   <input semantic="VERTEX" source="#v" offset="0"/><input semantic="NORMAL" source="#nrm" offset="1"/>
   <input semantic="TEXCOORD" source="#uv" offset="2"/>
   <p>0 0 0 1 0 1 2 0 2 0 0 0 2 0 2 3 0 3</p></triangles></mesh></geometry></library_geometries>
 <library_visual_scenes><visual_scene id="vs">
  <node id="n" name="quad"><translate>1 2 3</translate><scale>2 2 2</scale><instance_geometry url="#g-lib"/>
   <node id="c" name="child"><rotate>0 0 1 90</rotate></node></node></visual_scene></library_visual_scenes>
</COLLADA>"##;

fn run(f: impl Fn(&Cfg, Sink, &[u8]) -> i32, file: &Path) -> (i32, Vec<String>) {
    let mut rec = Recorder::default();
    let b = builder(&mut rec);
    let path = file.to_str().unwrap().as_bytes().to_vec();
    let code = f(&cfg(), Sink(&b), &path);
    (code, rec.lines)
}

fn has(lines: &[String], prefix: &str) -> usize {
    lines.iter().filter(|l| l.starts_with(prefix)).count()
}

#[test]
fn msh_drives_the_builder_in_file_order() {
    let file = scratch("tiny.msh");
    std::fs::write(&file, tiny_msh()).unwrap();
    let (code, lines) = run(msh::load_mesh, &file);
    assert_eq!(code, 0);
    let verbs: Vec<&str> = lines.iter().map(|l| l.split(' ').next().unwrap()).collect();
    assert_eq!(
        verbs,
        [
            "mesh_new",
            "skeleton_begin",
            "bone",
            "skeleton_end",
            "node",
            "sub_new",
            "material_create",
            "sub_material",
            "sub_collider",
            "sub_pairs",
            "sub_vb_new",
            "sub_vb_array",
            "sub_vb_array",
            "sub_vb_indices",
            "sub_vb_compile",
            "sub_vb_attach",
            "sub_compile",
            "mesh_compile",
            "anim_new",
            "anim_track",
            "anim_end",
        ]
    );
    assert!(lines[0].starts_with("mesh_new tiny.msh "));
    assert!(lines[5].starts_with("sub_new sub "));
    assert!(
        lines[5].contains(" 3f800000,40000000,40400000, "),
        "{}",
        lines[5]
    );
}

#[test]
fn msh_errors_are_logged_like_the_original() {
    let file = scratch("bad.msh");
    std::fs::write(&file, [0u8; 16]).unwrap();
    let (code, lines) = run(msh::load_mesh, &file);
    assert_eq!(code, 1);
    assert!(lines[0].contains("does not have right MSH magic number"));
    let (code, lines) = run(msh::load_mesh, &scratch("missing.msh"));
    assert_eq!(code, 1);
    assert!(lines[0].starts_with("log 2 Could not open binary file"));
    assert!(lines[1].starts_with("log 2 Could not load file"));
}

#[test]
fn collada_quad_builds_one_sub_mesh_and_its_nodes() {
    let file = scratch("quad.dae");
    std::fs::write(&file, QUAD).unwrap();
    let (code, lines) = run(collada::load_mesh, &file);
    assert_eq!(code, 0, "{lines:#?}");
    assert_eq!(has(&lines, "sub_new quad "), 1);
    assert_eq!(has(&lines, "sub_vb_array"), 5);
    assert_eq!(has(&lines, "sub_vb_transform"), 1);
    assert_eq!(has(&lines, "node"), 2);
    assert_eq!(has(&lines, "material_create"), 1);
    assert!(lines.last().unwrap().starts_with("mesh_compile"));
    assert_eq!(has(&lines, "skeleton_begin"), 0);
}

#[test]
fn collada_missing_file_fails_silently() {
    let (code, lines) = run(collada::load_mesh, &scratch("missing.dae"));
    assert_eq!(code, 1);
    assert_eq!(lines.len(), 1);
    assert!(lines[0].starts_with("log 0 Cache out of date!"));
}

fn fnv(lines: &[String]) -> u32 {
    let h = lines
        .iter()
        .flat_map(|l| l.bytes().chain(*b"\n"))
        .fold(0xcbf2_9ce4_8422_2325u64, |h, b| {
            (h ^ u64::from(b)).wrapping_mul(0x0100_0000_01b3)
        });
    (h ^ (h >> 32)) as u32
}

/// Replays a sample of the installed games' meshes through the recording builder and compares
/// the call streams with `tests/golden/L6/streams.txt` (written with `OHPL_L6_RECORD=1`).
#[test]
fn census_sample_streams_match_the_golden_file() {
    let root = std::env::var_os("OHPL_STEAM_COMMON").map_or_else(
        || {
            PathBuf::from(std::env::var_os("HOME").unwrap_or_default())
                .join(".local/share/Steam/steamapps/common")
        },
        PathBuf::from,
    );
    let games = [
        "Amnesia The Dark Descent",
        "Machine for Pigs",
        "SOMA",
        "Amnesia Rebirth",
        "Amnesia The Bunker",
    ];
    let mut files = Vec::new();
    for g in games {
        walk(&root.join(g), &mut files);
    }
    if files.is_empty() {
        return;
    }
    files.sort();
    let stride: usize = std::env::var("OHPL_L6_STRIDE")
        .ok()
        .and_then(|s| s.parse().ok())
        .unwrap_or(250);
    let golden = Path::new(env!("CARGO_MANIFEST_DIR")).join("tests/golden/L6/streams.txt");
    let record = std::env::var_os("OHPL_L6_RECORD").is_some();
    let mut out = String::new();
    for f in files.iter().step_by(stride) {
        let ext = f.extension().and_then(|e| e.to_str()).unwrap_or("");
        let (_, lines) = match ext {
            "msh" => run(msh::load_mesh, f),
            _ => run(collada::load_mesh, f),
        };
        let rel = f.strip_prefix(&root).unwrap().to_str().unwrap();
        out.push_str(&format!(
            "{:08x} {:08x} {}\n",
            fnv(&[rel.to_string()]),
            fnv(&lines),
            lines.len()
        ));
    }
    if record {
        std::fs::write(&golden, &out).unwrap();
    } else if let Ok(want) = std::fs::read_to_string(&golden) {
        let have: std::collections::HashSet<&str> = out.lines().collect();
        let missing: Vec<&str> = want.lines().filter(|l| !have.contains(l)).collect();
        assert!(missing.is_empty() || stride != 250, "{missing:?}");
    }
}

fn walk(dir: &Path, out: &mut Vec<PathBuf>) {
    let Ok(rd) = std::fs::read_dir(dir) else {
        return;
    };
    for e in rd.flatten() {
        let p = e.path();
        if p.is_dir() {
            walk(&p, out);
        } else if matches!(p.extension().and_then(|x| x.to_str()), Some("msh" | "dae")) {
            out.push(p);
        }
    }
}
