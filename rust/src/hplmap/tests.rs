use super::*;
use crate::xml::{DOC, Doc, Kind, NodeId};

struct Dom(Doc);

impl Dom {
    fn new(xml: &str) -> Self {
        let mut doc = Doc::new();
        doc.parse(xml.as_bytes());
        assert!(!doc.error(), "test xml must parse");
        Self(doc)
    }
}

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

type MeshRecord = (Vec<u8>, Vec<(i32, Vec<u8>)>, Vec<u32>);

#[derive(Default)]
struct Recorder {
    events: Vec<String>,
    cache_meshes: Vec<MeshRecord>,
}

fn text(s: &[u8]) -> String {
    String::from_utf8_lossy(s).into_owned()
}

impl Builder<NodeId> for Recorder {
    fn log(&mut self, msg: &[u8]) {
        self.events.push(format!("log {}", text(msg)));
    }
    fn warning(&mut self, msg: &[u8]) {
        self.events.push(format!("warning {}", text(msg)));
    }
    fn error(&mut self, msg: &[u8]) {
        self.events.push(format!("error {}", text(msg)));
    }
    fn phase(&mut self, phase: Phase, begin: bool) {
        self.events.push(format!("phase {phase:?} {begin}"));
    }
    fn set_fog(&mut self, fog: Fog) {
        self.events.push(format!("fog {fog:?}"));
    }
    fn set_skybox(&mut self, active: bool, color: Color, texture: &[u8]) {
        self.events
            .push(format!("skybox {active} {color:?} {}", text(texture)));
    }
    fn static_begin(&mut self) {
        self.events.push("static_begin".into());
    }
    fn static_object(&mut self, o: &StaticObject) {
        self.events.push(format!(
            "object {} {} {:?} {} {} {}",
            text(o.name),
            text(o.file),
            o.at,
            o.collides,
            o.cast_shadows,
            o.id
        ));
    }
    fn static_plane(&mut self, p: &Plane) {
        self.events.push(format!(
            "plane {} {} {:?} {:?}",
            text(p.name),
            text(p.material),
            p.start_corner,
            p.uv
        ));
    }
    fn static_decal(&mut self, d: &Decal<NodeId>) {
        self.events.push(format!(
            "decal {} {} {} {:?}",
            text(d.name),
            text(d.material),
            d.id,
            d.mesh
        ));
    }
    fn static_loading_done(&mut self) {
        self.events.push("loading_done".into());
    }
    fn static_combo(&mut self, group_id: i32, ids: &[i32]) {
        self.events.push(format!("combo {group_id} {ids:?}"));
    }
    fn static_compile(&mut self) {
        self.events.push("compile".into());
    }
    fn entities_begin(&mut self) {
        self.events.push("entities_begin".into());
    }
    fn entities_end(&mut self) {
        self.events.push("entities_end".into());
    }
    fn entity(&mut self, e: &Entity<NodeId>) {
        self.events.push(format!(
            "entity {} {} {} {} {:?}",
            text(e.name),
            e.id,
            e.active,
            text(e.file),
            e.user_vars
        ));
    }
    fn area(&mut self, a: &Entity<NodeId>) {
        self.events.push(format!(
            "area {} {} {}",
            text(a.name),
            text(a.file),
            a.user_vars.is_some()
        ));
    }
    fn node(&mut self, kind: NodeKind, _node: NodeId) {
        self.events.push(format!("node {kind:?}"));
    }
    fn cache_header(&mut self, s: CacheSummary) {
        self.events.push(format!("header {s:?}"));
    }
    fn cache_mesh_body(&mut self, b: &CacheBody, buffer: *mut BinaryBuffer) {
        // SAFETY: the buffer outlives `read_cache`, which is the only caller.
        let blob = unsafe { &mut *buffer }.get_i32();
        self.events.push(format!(
            "body {} {} blob={blob}",
            text(b.name),
            text(b.material)
        ));
    }
    fn cache_shape_body(&mut self, b: &ShapeBody) {
        self.events.push(format!(
            "shapes {} {} {}",
            text(b.material),
            b.colliders.len(),
            b.char_collider
        ));
    }
    fn cache_mesh(&mut self, m: &CacheMesh) {
        self.events.push(format!("mesh {}", text(m.name)));
        self.cache_meshes.push((
            m.name.to_vec(),
            m.arrays.iter().map(|a| (a.kind, a.data.to_vec())).collect(),
            m.indices.to_vec(),
        ));
    }
}

fn parse(xml: &str, flags: u32, cache_loaded: bool) -> (Parsed, Vec<String>) {
    let dom = Dom::new(xml);
    let mut rec = Recorder::default();
    let options = Options {
        flags,
        log_cache: false,
        file: b"x.map",
    };
    let parsed = parse_map(&dom, DOC, &mut rec, &options, cache_loaded);
    (parsed, rec.events)
}

const MAP: &str = r#"<MapData FogActive="true" FogColor="0.5 0.5 0.5 1" SkyBoxTexture="sky">
<MapContents>
<FileIndex_StaticObjects NumOfFiles="2"><F Id="0" Path="a.dae"/><F Id="1" Path="b.dae"/></FileIndex_StaticObjects>
<FileIndex_Decals NumOfFiles="1"><F Id="0" Path="decal.mat"/></FileIndex_Decals>
<FileIndex_Entities NumOfFiles="1"><F Id="0" Path="e.ent"/></FileIndex_Entities>
<StaticObjects>
<Obj Name="o0" FileIndex="1" WorldPos="1 2 3" Scale="2 2 2" ID="4"/>
<Obj Name="o1" Filename="direct.dae" MeshFilename="ignored.dae" Collides="false"/>
<Obj Name="bad" FileIndex="9"/>
<Obj Name="far" Filename="f.dae" WorldPos="20000 0 0"/>
</StaticObjects>
<Primitives>
<Plane Name="p" Material="m.mat" StartCorner="0 0 0" EndCorner="1 0 1" Corner2UV="1 0"/>
<Box Name="b"/>
</Primitives>
<Decals>
<Decal Name="d0" MaterialIndex="0"><DecalMesh/></Decal>
<Decal Name="d1" MaterialIndex="5"/>
</Decals>
<StaticObjectCombos><Combo ID="7" ObjIds="1, 2 3"/></StaticObjectCombos>
<Entities>
<Entity Name="e0" FileIndex="0" ID="3"><UserVariables/></Entity>
<Area Name="a0" AreaType="Script"/>
<PointLight Name="l"/>
<Light/>
<Billboard/>
<Weird/>
</Entities>
</MapContents>
</MapData>"#;

#[test]
fn parses_a_whole_map_in_original_order() {
    let (parsed, events) = parse(MAP, 0, false);
    assert_eq!(parsed, Parsed::Ok);
    let kinds: Vec<&str> = events
        .iter()
        .map(|e| e.split(' ').next().unwrap())
        .collect();
    assert_eq!(
        kinds,
        [
            "fog",
            "skybox",
            "phase",
            "static_begin",
            "phase",
            "object",
            "object",
            "error",
            "warning",
            "phase",
            "phase",
            "plane",
            "phase",
            "phase",
            "decal",
            "error",
            "phase",
            "loading_done",
            "phase",
            "combo",
            "phase",
            "compile",
            "phase",
            "phase",
            "entities_begin",
            "entity",
            "area",
            "node",
            "error",
            "node",
            "error",
            "entities_end",
            "phase"
        ]
    );
    assert!(events[5].starts_with("object o0 b.dae"));
    assert!(events[6].contains("direct.dae"));
    assert!(events[7].contains("File index: 9 does is out of bounds!"));
    assert!(
        events[8].contains(
            "The object far has an invalid position: (20000.000000 : 0.000000 : 0.000000)!"
        )
    );
    assert!(events.iter().any(|e| e == "combo 7 [1, 2, 3]"));
    assert!(
        events
            .iter()
            .any(|e| e.contains("Unknown entity type 'Weird'"))
    );
}

#[test]
fn flags_and_cache_skip_stages() {
    let (_, events) = parse(MAP, FLAG_NO_ENTITIES, true);
    assert!(
        events
            .iter()
            .all(|e| !e.starts_with("static_begin") && !e.starts_with("entity"))
    );

    let (_, events) = parse(MAP, FLAG_NO_GAME_ENTITIES | FLAG_NO_LIGHTS, false);
    assert!(
        events
            .iter()
            .all(|e| !e.starts_with("entity ") && !e.starts_with("area"))
    );
    assert_eq!(events.iter().filter(|e| e.starts_with("node")).count(), 1);

    let (_, events) = parse(MAP, FLAG_FAST_STATIC_LOAD, false);
    assert!(events.iter().all(|e| !e.starts_with("decal")));
}

#[test]
fn missing_nodes_are_reported() {
    assert_eq!(parse("<Other/>", 0, false).0, Parsed::Abort);
    assert_eq!(parse("<MapData/>", 0, false).0, Parsed::NoContents);
    assert_eq!(
        parse("<MapData><MapContents/></MapData>", 0, false).0,
        Parsed::Abort
    );
    let negative =
        "<MapData><MapContents><FileIndex_Decals NumOfFiles=\"-1\"/></MapContents></MapData>";
    assert_eq!(parse(negative, 0, false).0, Parsed::Abort);
    let ok = "<MapData><MapContents><StaticObjects/><Entities/></MapContents></MapData>";
    assert_eq!(parse(ok, 0, false).0, Parsed::Ok);
}

#[test]
fn attribute_defaults_match_the_original() {
    let (_, events) = parse(
        r#"<MapData FogColor="1 2" SkyBoxActive="TRUE"><MapContents><StaticObjects/><Entities/></MapContents></MapData>"#,
        0,
        true,
    );
    assert!(events[0].contains("active: false") && events[0].contains("falloff_exp: 1.0"));
    assert!(events[0].contains("r: 1.0, g: 1.0, b: 1.0, a: 1.0"));
    assert!(events[1].starts_with("skybox true"));
}

struct Source {
    buffer: *mut BinaryBuffer,
    meshes: Vec<(Vec<CacheArray<'static>>, Vec<u32>)>,
    next: usize,
    errors: Vec<String>,
}

static POSITIONS: [f32; 4] = [0.0, 1.0, -2.5, 4.0];
static NORMALS: [f32; 6] = [0.0, 1.0, 0.5, 0.0, 0.25, 1.0];

fn bytes_of(f: &'static [f32]) -> &'static [u8] {
    // SAFETY: f32 has no padding and the slice is 'static.
    unsafe { std::slice::from_raw_parts(f.as_ptr().cast(), std::mem::size_of_val(f)) }
}

impl CacheSource for Source {
    fn error(&mut self, msg: &[u8]) {
        self.errors.push(text(msg));
    }
    fn counts(&mut self) -> CacheSummary {
        CacheSummary {
            mesh_bodies: 1,
            shape_bodies: 1,
            mesh_entities: self.meshes.len() as i32,
        }
    }
    fn next_body(&mut self) -> BodyOut<'_> {
        BodyOut {
            name: b"body",
            material: b"mat",
            blocks_light: true,
            char_collider: false,
        }
    }
    fn write_body_shape(&mut self) {
        // SAFETY: the buffer is live for the whole write.
        let b = unsafe { &mut *self.buffer };
        b.add_i32(99);
    }
    fn next_shape_body(&mut self) -> ShapeBody<'_> {
        ShapeBody {
            material: b"smat",
            transform: Matrixf::default(),
            char_collider: true,
            blocks_light: false,
            colliders: &[],
        }
    }
    fn next_mesh(&mut self) -> MeshOut<'_> {
        let (arrays, indices) = &self.meshes[self.next];
        self.next += 1;
        MeshOut {
            name: b"mesh",
            material: b"",
            cast_shadows: true,
            vertex_num: 2,
            arrays,
            indices,
        }
    }
}

fn written() -> (BinaryBuffer, Vec<String>) {
    let array = |kind, format, elem_num, data: &'static [u8]| CacheArray {
        kind,
        format,
        prog_var: -1,
        elem_num,
        compression: 0,
        count: 2 * elem_num as usize,
        data,
    };
    let mut buffer = BinaryBuffer::new();
    let mut source = Source {
        buffer: &raw mut buffer,
        meshes: vec![(
            vec![
                array(1, 0, 2, bytes_of(&POSITIONS)),
                array(0, 0, 3, bytes_of(&NORMALS)),
            ],
            vec![0, 1, 2],
        )],
        next: 0,
        errors: Vec::new(),
    };
    // SAFETY: `buffer` is live and only reached through `source` during the call.
    unsafe { write_cache(&raw mut buffer, &mut source) };
    let errors = source.errors;
    (buffer, errors)
}

#[test]
fn cache_round_trips_and_patches_the_total_size() {
    let (mut buffer, errors) = written();
    assert!(errors.is_empty());
    let size = buffer.size();
    buffer.set_pos(0);
    assert_eq!(buffer.get_i32() as u32, 0xF441_451F);
    assert_eq!(buffer.get_i32(), 13);
    assert_eq!(buffer.get_i32() as usize, size);
    buffer.set_pos(0);

    let mut rec = Recorder::default();
    let options = Options {
        flags: 0,
        log_cache: false,
        file: b"f.map",
    };
    // SAFETY: `buffer` is live and not otherwise borrowed.
    assert!(unsafe { read_cache(&raw mut buffer, &mut rec, &options) });
    assert_eq!(
        rec.events,
        [
            "header CacheSummary { mesh_bodies: 1, shape_bodies: 1, mesh_entities: 1 }",
            "body body mat blob=99",
            "shapes smat 0 true",
            "mesh mesh"
        ]
    );
    let (_, arrays, indices) = &rec.cache_meshes[0];
    assert_eq!(indices, &[0, 1, 2]);
    assert_eq!(arrays[0], (1, bytes_of(&POSITIONS).to_vec()));
    // Normals are stored as signed bytes (`n * 127`) and come back through the platform `char`.
    let normals: Vec<f32> = arrays[1]
        .1
        .chunks_exact(4)
        .map(|c| f32::from_ne_bytes([c[0], c[1], c[2], c[3]]))
        .collect();
    assert_eq!(normals, [0.0, 1.0, 64.0 / 127.0, 0.0, 32.0 / 127.0, 1.0]);
}

#[test]
fn cache_rejects_bad_headers() {
    let reject = |edit: &dyn Fn(&mut Vec<u8>), expect: &str| {
        let (mut buffer, _) = written();
        // SAFETY: the pointer and size describe the buffer's live contents.
        let mut bytes =
            unsafe { std::slice::from_raw_parts(buffer.data_ptr(), buffer.size()) }.to_vec();
        edit(&mut bytes);
        buffer.assign(&bytes);
        let mut rec = Recorder::default();
        let options = Options {
            flags: 0,
            log_cache: false,
            file: b"f.map",
        };
        // SAFETY: `buffer` is live and not otherwise borrowed.
        assert!(!unsafe { read_cache(&raw mut buffer, &mut rec, &options) });
        assert_eq!(rec.events.len(), 1, "{:?}", rec.events);
        assert!(rec.events[0].contains(expect), "{:?}", rec.events);
    };
    reject(
        &|b| b[0] ^= 1,
        "magic number (F441451E instead of F441451F)",
    );
    reject(&|b| b[4] = 12, "Is 12, newest is 13");
    reject(&|b| b.push(0), "is truncated or corrupt");
    reject(&|b| b[15] = 0x7f, "invalid counts");
}

#[test]
fn fast_float_conversions_round_like_the_original() {
    assert_eq!(cache::fast_pos_and_neg(63.5), 64);
    assert_eq!(cache::fast_pos_and_neg(-63.5), -64);
    assert_eq!(cache::fast_positive(254.9), 255);
}
