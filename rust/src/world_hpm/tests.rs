use super::abi::{self, Cstr};
use super::el::El;
use super::parse::{Builder, FileRead, parse_map, widen};
use std::collections::HashMap;
use std::ffi::CStr;

fn s(p: Cstr) -> String {
    if p.is_null() {
        return "<null>".into();
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

#[derive(Default)]
struct Rec {
    files: HashMap<String, Vec<u8>>,
    ev: Vec<String>,
    mesh_ok: bool,
    clock: u64,
}

impl Rec {
    fn with(files: &[(&str, &str)]) -> Self {
        Self {
            files: files
                .iter()
                .map(|(k, v)| ((*k).to_owned(), v.as_bytes().to_vec()))
                .collect(),
            mesh_ok: true,
            ..Self::default()
        }
    }

    fn run(&mut self) -> &[String] {
        parse_map(self, &widen("map.hpm"));
        &self.ev
    }

    fn has(&self, prefix: &str) -> bool {
        self.ev.iter().any(|e| e.starts_with(prefix))
    }

    fn report(&self, track: &str) -> &str {
        self.ev
            .iter()
            .find(|e| e.starts_with(&format!("report {track} ")))
            .map_or("", String::as_str)
    }
}

impl Builder for Rec {
    fn now_ms(&mut self) -> u64 {
        self.clock += 5;
        self.clock
    }
    fn object_done(&mut self) {}
    fn warn(&mut self, m: &[u8]) {
        self.ev.push(format!("warn {}", String::from_utf8_lossy(m)));
    }
    fn log(&mut self, m: &[u8]) {
        self.ev.push(format!("log {}", String::from_utf8_lossy(m)));
    }
    fn read_file(&mut self, path: &[u32]) -> FileRead {
        let p: String = path
            .iter()
            .map(|&c| char::from_u32(c).unwrap_or('?'))
            .collect();
        match self.files.get(&p) {
            Some(d) => FileRead::Read(d.clone()),
            None => FileRead::NotOpened,
        }
    }
    fn global_fog(&mut self, v: &abi::Fog) {
        self.ev.push(format!("fog {} {:?}", v.active, v.color));
    }
    fn distance_cull(&mut self, v: &abi::DistanceCull) {
        self.ev.push(format!("cull {} {}", v.active, v.screen_size));
    }
    fn dir_light(&mut self, v: &abi::DirLight) {
        self.ev.push(format!("dir {:?}", v.direction));
    }
    fn env_particles(&mut self, v: &abi::EnvParticles) {
        self.ev
            .push(format!("env {} {:?}", s(v.texture), v.sub_div_uv));
    }
    fn post_effects(&mut self, v: &abi::PostEffects) {
        self.ev.push(format!("post {}", v.tone_mapping_white_cut));
    }
    fn sky_box(&mut self, v: &abi::SkyBox) {
        self.ev.push(format!("sky {}", s(v.texture)));
    }
    fn static_object(&mut self, v: &abi::StaticObject) -> bool {
        self.ev.push(format!(
            "static {} {} {:?} pose={:?}",
            s(v.name),
            s(v.filename),
            v.position,
            v.has_pose.then(|| floats(v.pose, v.num_pose).to_vec())
        ));
        self.mesh_ok
    }
    fn plane(&mut self, v: &abi::Plane) -> bool {
        self.ev.push(format!("plane {} {:?}", s(v.name), v.uv));
        true
    }
    fn entity(&mut self, v: &abi::Entity, el: &El) -> bool {
        self.ev.push(format!(
            "entity {} {} id={} static={} user={}",
            s(v.name),
            s(v.filename),
            v.id,
            v.is_static,
            el.first(b"UserVariables").is_some()
        ));
        true
    }
    fn light(&mut self, _: &El) -> bool {
        false
    }
    fn area(&mut self, v: &abi::Area, _: &El) -> bool {
        self.ev.push(format!("area {}", s(v.area_type)));
        s(v.area_type) != "Nope"
    }
    fn sound(&mut self, el: &El, active: bool) -> bool {
        self.ev
            .push(format!("sound active={active} {:?}", el.attr(b"Name")));
        false
    }
    fn decal(&mut self, v: &abi::Decal, mesh: Option<&El>) -> bool {
        self.ev.push(format!(
            "decal {} {} mesh={}",
            s(v.name),
            s(v.material),
            mesh.is_some()
        ));
        true
    }
    fn billboard(&mut self, _: &El) -> bool {
        true
    }
    fn particle_system(&mut self, _: &El, light: Cstr) -> bool {
        self.ev.push(format!("ps light={}", s(light)));
        true
    }
    fn fog_area(&mut self, _: &El) -> bool {
        true
    }
    fn lens_flare(&mut self, _: &El) -> bool {
        true
    }
    fn light_mask_box(&mut self, v: &abi::LightMask) {
        self.ev.push(format!("mask {}", v.id));
    }
    fn connect_lights(&mut self) -> (i32, i32) {
        (2, 0)
    }
    fn connect_light_masks(&mut self) {}
    fn detail_meshes(&mut self, v: &abi::DetailMeshes) -> i32 {
        self.ev.push(format!(
            "detail {} n={} first={} pos={:?} radii={:?}",
            s(v.file),
            v.num,
            v.first_index,
            floats(v.positions, 3 * v.num as usize),
            floats(v.radii, v.num_radii)
        ));
        if self.mesh_ok { v.num } else { 1 }
    }
    fn exposure_area(&mut self, v: &abi::ExposureArea) {
        self.ev
            .push(format!("exposure {} {}", s(v.name), v.white_cut));
    }
    fn terrain(&mut self, v: &abi::Terrain) {
        // SAFETY: the parser passes `num_layers`, `num_decals` and `num_areas` readable elements.
        let (layers, decals, areas) = unsafe {
            (
                std::slice::from_raw_parts(v.layers, v.num_layers),
                std::slice::from_raw_parts(v.decals, v.num_decals),
                std::slice::from_raw_parts(v.areas, v.num_areas),
            )
        };
        let layer: Vec<_> = layers
            .iter()
            .map(|l| {
                (
                    s(l.id),
                    l.num_mats,
                    s(l.mats[0].file),
                    l.mats[0].tile_amount,
                )
            })
            .collect();
        let decal: Vec<_> = decals.iter().map(|d| (s(d.file), d.position)).collect();
        let area: Vec<_> = areas
            .iter()
            .map(|a| {
                (
                    s(a.file),
                    a.circle,
                    a.sub,
                    floats(a.points, a.num_points * 2).to_vec(),
                )
            })
            .collect();
        self.ev.push(format!(
            "terrain size={} base={} layers={layer:?} details={} decals={decal:?} areas={area:?}",
            v.height_map_size,
            s(v.base_material_file),
            v.num_details
        ));
    }
    fn track_report(
        &mut self,
        track: Cstr,
        x: i32,
        c: i32,
        ms: i32,
        missing: bool,
        skipped: &[abi::Skip],
    ) {
        let skipped: Vec<_> = skipped.iter().map(|k| (s(k.reason), k.count)).collect();
        self.ev.push(format!(
            "report {} xml={x} created={c} ms={ms} missing={missing} {skipped:?}",
            s(track)
        ));
    }
}

const STATIC: &str = r#"<HPLMapTrack_StaticObject><Section>
<FileIndex_StaticObjects NumOfFiles="2"><F Id="0" Path="a.dae"/><F Id="1" Path="b.dae"/></FileIndex_StaticObjects>
<Objects>
<StaticObject Name="o1" FileIndex="1" WorldPos="1 2 3"><Pose Hash="7">0.5 1 2</Pose></StaticObject>
<StaticObject Name="o2" FileIndex="5"/>
<StaticObject Name="far" Filename="c.dae" WorldPos="20000 0 0"/>
<Wrong/>
</Objects></Section></HPLMapTrack_StaticObject>"#;

#[test]
fn static_objects_resolve_file_index_and_report_skips() {
    let mut r = Rec::with(&[("map.hpm", "<HPLMap/>"), ("map.hpm_StaticObject", STATIC)]);
    r.run();
    assert!(
        r.has("static o1 b.dae [1.0, 2.0, 3.0] pose=Some([0.5, 1.0, 2.0])"),
        "{:?}",
        r.ev
    );
    assert!(r.has("warn SOMA hpm: static object 'o2' has out-of-bounds FileIndex 5\n"));
    assert!(r.has("warn SOMA hpm: object 'far' has an invalid position: (20000.000000 : 0.000000 : 0.000000)!\n"));
    let report = r.report("StaticObject");
    for part in [
        "xml=4 created=1",
        "(\"bad_transform\", 1)",
        "(\"file_index_out_of_bounds\", 1)",
        "(\"unsupported_element:Wrong\", 1)",
    ] {
        assert!(report.contains(part), "{report}");
    }
    assert!(r.report("Entity").contains("missing=true"));
}

#[test]
fn failed_creation_and_reasons() {
    let mut r = Rec::with(&[("map.hpm", "<HPLMap/>"), ("map.hpm_StaticObject", STATIC)]);
    r.mesh_ok = false;
    r.run();
    assert!(
        r.report("StaticObject")
            .contains("(\"mesh_missing:b.dae\", 1)")
    );

    let sounds = r#"<HPLMapTrack_Sound><Section><Objects>
<Sound Active="false" SoundEntityFile=""/><Sound SoundEntityFile="x.snt"/></Objects></Section></HPLMapTrack_Sound>"#;
    let areas = r#"<HPLMapTrack_Area><Section><Objects>
<Area AreaType="Nope"/><Area AreaType="Ok"/></Objects></Section></HPLMapTrack_Area>"#;
    let mut r = Rec::with(&[
        ("map.hpm_Sound", sounds),
        ("map.hpm_Area", areas),
        (
            "map.hpm_Light",
            "<HPLMapTrack_Light><Section><Objects><Light/></Objects></Section></HPLMapTrack_Light>",
        ),
    ]);
    r.run();
    assert!(
        r.report("Sound")
            .contains("(\"empty_file\", 1), (\"load_failed\", 1)")
    );
    assert!(r.report("Area").contains("(\"no_area_loader:Nope\", 1)"));
    assert!(r.report("Light").contains("(\"load_failed:Light\", 1)"));
    assert!(
        r.has("warn SOMA hpm: root file 'map.hpm' has no HPLMap element!")
            || r.has("warn SOMA hpm: could not open/parse 'map.hpm'")
    );
}

#[test]
fn broken_xml_is_logged_and_skipped() {
    let mut r = Rec::with(&[
        ("map.hpm", "<HPLMap/>"),
        (
            "map.hpm_Entity",
            "<HPLMapTrack_Entity><Section><Objects><Entity Na",
        ),
        ("map.hpm_Area", ""),
    ]);
    r.run();
    assert!(
        r.has("log Failed parsing of XML document map.hpm_Entity in line"),
        "{:?}",
        r.ev
    );
    assert!(r.has("warn SOMA hpm: could not open/parse 'map.hpm_Entity'"));
    assert!(r.has(
        "log Failed parsing of XML document map.hpm_Area in line 0, column 0: Error document empty."
    ));
    assert!(r.report("Entity").contains("missing=true"));
}

#[test]
fn wrong_root_warns() {
    let mut r = Rec::with(&[("map.hpm", "<Other/>"), ("map.hpm_Sound", "<Nope/>")]);
    r.run();
    assert!(r.has(
        "warn SOMA hpm: sidecar file has unexpected root element 'Other' (expected 'HPLMap')"
    ));
    assert!(r.has("warn SOMA hpm: sidecar file has unexpected root element 'Nope' (expected 'HPLMapTrack_Sound')"));
}

#[test]
fn global_settings_use_defaults() {
    let hpm = r#"<HPLMap><GlobalSettings><Fog Active="TRUE" Color="1 0.5 0 1"/><DistanceCulling/>
<DirLight Direction="bad"/><EnvParticles Texture="p.dds" SubDivUV="2 4"/><PostEffects/><SkyBox Texture="s.dds"/></GlobalSettings></HPLMap>"#;
    let mut r = Rec::with(&[("map.hpm", hpm)]);
    r.run();
    assert!(r.has("fog true [1.0, 0.5, 0.0, 1.0]"));
    assert!(r.has("cull false 0.05"));
    assert!(r.has("dir [0.0, -1.0, 0.0]"));
    assert!(r.has("env p.dds [2.0, 4.0]"));
    assert!(r.has("post 3.5"));
    assert!(r.has("sky s.dds"));
}

#[test]
fn detail_meshes_short_data_and_partial_mesh_failure() {
    let xml = r#"<HPLMapTrack_DetailMeshes><DetailMeshes><Sections><Section>
<Mesh File="m.msh" NumOfInstances="2"><DetailMeshEntityPositions>1 2 3 4 5 6</DetailMeshEntityPositions>
<DetailMeshEntityRotations>0 0 0 1 0 0 0 1</DetailMeshEntityRotations><DetailMeshEntityRadii>9</DetailMeshEntityRadii></Mesh>
<Mesh File="short.msh" NumOfInstances="3"><DetailMeshEntityPositions>1 2 3</DetailMeshEntityPositions></Mesh>
</Section></Sections></DetailMeshes></HPLMapTrack_DetailMeshes>"#;
    let mut r = Rec::with(&[("map.hpm_DetailMeshes", xml)]);
    r.run();
    assert!(r.has("detail m.msh n=2 first=0 pos=[1.0, 2.0, 3.0, 4.0, 5.0, 6.0] radii=[9.0]"));
    assert!(r.report("DetailMeshes").contains("xml=5 created=2"));
    assert!(
        r.report("DetailMeshes")
            .contains("(\"instance_data_short:short.msh\", 3)")
    );

    let mut r = Rec::with(&[("map.hpm_DetailMeshes", xml)]);
    r.mesh_ok = false;
    r.run();
    assert!(r.report("DetailMeshes").contains("created=1"));
    assert!(
        r.report("DetailMeshes")
            .contains("(\"mesh_missing:m.msh\", 1)")
    );
}

#[test]
fn terrain_flattens_sections() {
    let xml = r#"<HPLMapTrack_Terrain><Terrain Active="true" HeightMapSize="64" BaseMaterialFile="base.mat">
<BlendLayers><L ID="3"><M File="a.mat" TileAmount="4"/><M/><M/><M/><M File="fifth.mat"/></L></BlendLayers>
<DetailTextures><D File="d.dds"/></DetailTextures>
<Sections><S><FileIndex_TerrainDecals><F Path="x.mat"/></FileIndex_TerrainDecals>
<Decals><D FileIndex="0" Position="1 2"/><D FileIndex="3"/></Decals>
<FileIndex_UndergrowthMaterials NumOfFiles="1"><F Id="0" Path="g.mat"/></FileIndex_UndergrowthMaterials>
<Undergrowth><Circle FileIndex="0" Radius="2"/><Poly FileIndex="0" BlendType="SUB"><P Coords="0 1"/><P Coords="2 3"/></Poly><Circle FileIndex="4"/></Undergrowth>
</S></Sections></Terrain></HPLMapTrack_Terrain>"#;
    let mut r = Rec::with(&[("map.hpm_Terrain", xml)]);
    r.run();
    let t =
        r.ev.iter()
            .find(|e| e.starts_with("terrain"))
            .cloned()
            .unwrap_or_default();
    assert!(
        t.contains("size=64 base=base.mat layers=[(\"3\", 4, \"a.mat\", 4.0)] details=1"),
        "{t}"
    );
    assert!(t.contains("decals=[(\"x.mat\", [1.0, 2.0])]"), "{t}");
    assert!(
        t.contains(
            "areas=[(\"g.mat\", true, false, []), (\"g.mat\", false, true, [0.0, 1.0, 2.0, 3.0])]"
        ),
        "{t}"
    );

    let mut r = Rec::with(&[("map.hpm_Terrain", &xml.replace("true", "false"))]);
    r.run();
    assert!(!r.has("terrain"));
}

#[test]
fn connect_lights_adds_skips_and_light_masks_read_id() {
    let masks = r#"<HPLMapTrack_LightMask><Section><Objects><LightMaskBox ID=" 42x"/><LightMaskBox ID="-1"/></Objects></Section></HPLMapTrack_LightMask>"#;
    let mut r = Rec::with(&[("map.hpm_LightMask", masks)]);
    r.run();
    assert!(r.has("mask 42"));
    assert!(r.has("mask 4294967295"));
    assert!(
        r.report("Billboard")
            .contains("(\"connect_light_missing\", 2)")
    );
    assert!(!r.report("ParticleSystem").contains("connect_light_missing"));
}

#[test]
fn element_text_is_the_text_attribute_and_text_overrides_attribute() {
    let doc = {
        let mut d = crate::xml::Doc::new();
        d.parse(br#"<A X="1" _Text="attr">body<B/>text</A>"#);
        d
    };
    let el = El::from_doc(&doc);
    let el = el.as_ref();
    assert_eq!(el.map(|e| e.string(b"_Text")), Some(b"body".to_vec()));
    assert_eq!(el.map(|e| e.children.len()), Some(1));
}

#[test]
fn detail_meshes_huge_count_is_short() {
    let xml = r#"<HPLMapTrack_DetailMeshes><DetailMeshes><Sections><Section>
<Mesh File="big.msh" NumOfInstances="1000000000"><DetailMeshEntityPositions>1 2 3</DetailMeshEntityPositions>
<DetailMeshEntityRotations>0 0 0 1</DetailMeshEntityRotations></Mesh>
</Section></Sections></DetailMeshes></HPLMapTrack_DetailMeshes>"#;
    let mut r = Rec::with(&[("map.hpm_DetailMeshes", xml)]);
    r.run();
    assert!(!r.has("detail big.msh"));
    assert!(
        r.report("DetailMeshes")
            .contains("instance_data_short:big.msh")
    );
}
