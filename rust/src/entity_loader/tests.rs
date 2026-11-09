use super::*;
use crate::types::{Matrixf, Vector3f};
use crate::xml::{DOC, Doc};

const ID: Matrixf = Matrixf {
    m: [
        [1.0, 0.0, 0.0, 0.0],
        [0.0, 1.0, 0.0, 0.0],
        [0.0, 0.0, 1.0, 0.0],
        [0.0, 0.0, 0.0, 1.0],
    ],
};

#[derive(Default)]
struct Rec {
    log: Vec<String>,
    next: usize,
    skeleton: bool,
    ids: Vec<(usize, i32)>,
}

impl Rec {
    fn new_handle(&mut self) -> Handle {
        self.next += 1;
        Handle::new(self.next).expect("nonzero")
    }

    fn put(&mut self, s: impl Into<String>) {
        self.log.push(s.into());
    }
}

fn text(b: &[u8]) -> String {
    String::from_utf8_lossy(b).into_owned()
}

impl Builder for Rec {
    fn error(&mut self, m: &[u8]) {
        self.put(format!("error {}", text(m)));
    }
    fn warning(&mut self, m: &[u8]) {
        self.put(format!("warning {}", text(m)));
    }
    fn before_load(&mut self) {
        self.put("before_load");
    }
    fn after_load(&mut self) {
        self.put("after_load");
    }
    fn before_joints(&mut self) -> bool {
        self.put("before_joints");
        false
    }
    fn flags(&mut self) -> Flags {
        Flags {
            animations: true,
            particle_systems: true,
            billboards: true,
            sounds: true,
            lights: true,
            ..Flags::default()
        }
    }
    fn create_mesh(&mut self, file: &[u8]) -> Option<MeshInfo> {
        self.put(format!("create_mesh {}", text(file)));
        Some(MeshInfo {
            skeleton: self.skeleton,
            anim_num: 0,
        })
    }
    fn create_mesh_entity(&mut self, name: &[u8]) -> Option<Handle> {
        self.put(format!("create_mesh_entity {}", text(name)));
        Some(self.new_handle())
    }
    fn set_matrix(&mut self, ent: Handle, m: &Matrixf) {
        self.put(format!("set_matrix {} {:?}", ent.get(), m.m[0]));
    }
    fn set_shadow_caster(&mut self, _: Handle) {}
    fn update_logic(&mut self, _: Handle) {
        self.put("update_logic");
    }
    fn get_sub_mesh_entity(&mut self, name: &[u8]) -> Option<Handle> {
        if name == b"missing" {
            return None;
        }
        Some(self.new_handle())
    }
    fn warn_missing_sub_mesh(&mut self, name: &[u8]) {
        self.put(format!("missing_sub_mesh {}", text(name)));
    }
    fn get_sub_mesh_model_scale(&mut self, _: Handle) -> Vector3f {
        Vector3f {
            x: 1.0,
            y: 1.0,
            z: 1.0,
        }
    }
    fn set_world_matrix(&mut self, ent: Handle, m: &Matrixf) {
        self.put(format!("set_world_matrix {} {:?}", ent.get(), m.m[0]));
    }
    fn set_unique_id(&mut self, ent: Handle, id: i32) {
        self.ids.retain(|&(e, _)| e != ent.get());
        self.ids.push((ent.get(), id));
        self.put(format!("set_unique_id {} {id}", ent.get()));
    }
    fn set_sub_mesh_material(&mut self, _: Handle, file: &[u8]) {
        self.put(format!("material {}", text(file)));
    }
    fn add_animation(&mut self, file: &[u8], _: &[u8], _: f32) -> Option<Handle> {
        self.put(format!("add_animation {}", text(file)));
        Some(self.new_handle())
    }
    fn set_special_event_time(&mut self, _: Handle, _: f32) {}
    fn add_animation_event(
        &mut self,
        _: Handle,
        t: f32,
        ty: AnimEventType,
        v: &[u8],
        _: &[u8],
        _: &[u8],
    ) {
        self.put(format!("event {t} {ty:?} {}", text(v)));
    }
    fn load_world_entity(&mut self, kind: WorldEntity, _: u32, prefix: &[u8]) -> Option<Handle> {
        self.put(format!("world_entity {kind:?} {}", text(prefix)));
        Some(self.new_handle())
    }
    fn get_local_position(&mut self, _: Handle) -> Vector3f {
        Vector3f {
            x: 1.0,
            y: 2.0,
            z: 3.0,
        }
    }
    fn set_position(&mut self, _: Handle, p: Vector3f) {
        self.put(format!("set_position {} {} {}", p.x, p.y, p.z));
    }
    fn link_light_billboards(&mut self) {}
    fn get_bone_state(&mut self, _: &[u8]) -> Option<Handle> {
        None
    }
    fn get_bone_state_num(&mut self) -> i32 {
        0
    }
    fn warn_skeleton_mismatch(&mut self) {}
    fn get_bone_world_matrix(&mut self, _: Handle) -> Matrixf {
        ID
    }
    fn get_bone_name(&mut self, _: Handle) -> Vec<u8> {
        b"bone".to_vec()
    }
    fn get_unique_id(&mut self, ent: Handle) -> i32 {
        self.ids
            .iter()
            .find(|&&(e, _)| e == ent.get())
            .map_or(0, |&(_, id)| id)
    }
    fn is_sub_mesh(&mut self, _: Handle) -> bool {
        true
    }
    fn get_local_matrix(&mut self, _: Handle) -> Matrixf {
        ID
    }
    fn get_world_matrix(&mut self, _: Handle) -> Matrixf {
        ID
    }
    fn get_entity_name(&mut self, _: Handle) -> Vec<u8> {
        b"ent".to_vec()
    }
    fn set_node_animation(&mut self) {}
    fn attach_entity_child(&mut self, parent: Handle, _: &Matrixf, child: Handle) {
        self.put(format!("attach {} {}", parent.get(), child.get()));
    }
    fn attach_bone_child(&mut self, _: Handle, _: &Matrixf, _: Handle) {}
    fn attach_bone_to_body(&mut self, _: Handle, _: &Matrixf, _: Handle) {}
    fn create_box_shape(&mut self, size: Vector3f, _: &Matrixf) -> Option<Handle> {
        self.put(format!("box {} {} {}", size.x, size.y, size.z));
        Some(self.new_handle())
    }
    fn create_sphere_shape(&mut self, _: Vector3f, _: &Matrixf) -> Option<Handle> {
        Some(self.new_handle())
    }
    fn create_cylinder_shape(&mut self, _: f32, _: f32, _: &Matrixf) -> Option<Handle> {
        Some(self.new_handle())
    }
    fn create_capsule_shape(&mut self, _: f32, _: f32, _: &Matrixf) -> Option<Handle> {
        Some(self.new_handle())
    }
    fn create_compound_shape(&mut self, shapes: &[Handle]) -> Option<Handle> {
        self.put(format!("compound {}", shapes.len()));
        Some(self.new_handle())
    }
    fn inc_shape_user_count(&mut self, _: Handle) {}
    fn destroy_shape(&mut self, s: Handle) {
        self.put(format!("destroy_shape {}", s.get()));
    }
    fn create_body(&mut self, name: &[u8], _: Handle) -> Option<Handle> {
        self.put(format!("create_body {}", text(name)));
        Some(self.new_handle())
    }
    fn set_body_properties(&mut self, body: Handle, p: &BodyProps) {
        self.put(format!(
            "body_props {} mass={} id={}",
            body.get(),
            p.mass,
            p.id
        ));
    }
    fn set_body_material(&mut self, _: Handle, _: &[u8]) {}
    fn register_body(&mut self, _: Handle) {}
    fn create_joint(
        &mut self,
        kind: JointKind,
        name: &[u8],
        _: Vector3f,
        _: Vector3f,
        parent: Option<Handle>,
        child: Handle,
    ) -> Option<Handle> {
        self.put(format!(
            "joint {kind:?} {} parent={:?} child={}",
            text(name),
            parent.map(Handle::get),
            child.get()
        ));
        Some(self.new_handle())
    }
    fn set_hinge_limits(&mut self, _: Handle, min: f32, max: f32) {
        self.put(format!("hinge {min} {max}"));
    }
    fn set_ball_limits(&mut self, _: Handle, _: f32, _: f32) {}
    fn set_slider_limits(&mut self, _: Handle, _: f32, _: f32) {}
    fn set_screw_limits(&mut self, _: Handle, _: f32, _: f32) {}
    fn set_joint_properties(&mut self, _: Handle, p: &JointProps) {
        self.put(format!(
            "joint_props max_speed={} min_speed={}",
            p.max_limit_max_speed, p.min_limit_max_speed
        ));
    }
    fn register_joint(&mut self, _: Handle) {}
    fn get_animation_state_num(&mut self) -> i32 {
        0
    }
    fn play_first_animation(&mut self) {}
    fn set_skeleton_physics_active(&mut self) {}
    fn load_user_variables(&mut self) {
        self.put("user_variables");
    }
}

fn run(xml: &str, skeleton: bool) -> (bool, Vec<String>) {
    let mut doc = Doc::new();
    doc.parse(xml.as_bytes());
    assert!(!doc.error());
    let root = doc.first_child_element(DOC, None).expect("root");
    let mut rec = Rec {
        skeleton,
        ..Rec::default()
    };
    let p = Params {
        name: b"crate",
        file_name: b"crate.ent",
        full_path: b"/g/crate.ent",
        full_path_dir: b"/g/",
        transform: ID,
        scale: Vector3f {
            x: 2.0,
            y: 2.0,
            z: 2.0,
        },
        physics_world: true,
    };
    let ok = parse(&mut rec, &doc, root, &p);
    (ok, rec.log)
}

#[test]
fn missing_model_data_bails_before_hook() {
    let (ok, log) = run("<Entity/>", false);
    assert!(!ok);
    assert_eq!(log, ["error Couldn't load element ModelData"]);
}

#[test]
fn missing_mesh_bails_after_before_load() {
    let (ok, log) = run("<Entity><ModelData/></Entity>", false);
    assert!(!ok);
    assert_eq!(log, ["before_load", "error Couldn't load element Mesh"]);
}

#[test]
fn mesh_without_path_gets_entity_directory() {
    let (ok, log) = run(
        r#"<Entity><ModelData><Mesh Filename="crate.dae"/></ModelData></Entity>"#,
        false,
    );
    assert!(ok);
    assert_eq!(log[1], "create_mesh /g/crate.dae");
    assert_eq!(log.last().map(String::as_str), Some("after_load"));
}

#[test]
fn unknown_joint_type_logs_and_falls_back_to_ball() {
    let xml = r#"<Entity><ModelData><Mesh Filename="a.dae"/>
        <Shapes><Shape ID="1" ShapeType="Box" Scale="1 1 1"/></Shapes>
        <Bodies><Body ID="1" Name="b"><Shape ID="1"/></Body></Bodies>
        <Joints><JointWobble Name="j" ConnectedChildBodyID="1"/></Joints>
        </ModelData></Entity>"#;
    let (ok, log) = run(xml, false);
    assert!(ok);
    assert!(log.contains(&"error Joint type 'JointWobble' does not exist!\n".to_owned()));
    assert!(
        log.iter()
            .any(|l| l.starts_with("joint Ball crate_j parent=None"))
    );
}

#[test]
fn missing_child_body_skips_joint() {
    let xml = r#"<Entity><ModelData><Mesh Filename="a.dae"/>
        <Joints><JointHinge Name="j" ConnectedChildBodyID="9"/></Joints>
        </ModelData></Entity>"#;
    let (_, log) = run(xml, false);
    assert!(
        log.contains(&"error Could not find child body with ID 9 for joint 'crate_j'\n".to_owned())
    );
    assert!(!log.iter().any(|l| l.starts_with("joint ")));
}

#[test]
fn min_limit_speed_fixup_writes_max_limit() {
    let xml = r#"<Entity><ModelData><Mesh Filename="a.dae"/>
        <Shapes><Shape ID="1" ShapeType="Box"/></Shapes>
        <Bodies><Body ID="1" Name="b"><Shape ID="1"/></Body></Bodies>
        <Joints><JointHinge Name="j" ConnectedChildBodyID="1" MinLimitMaxSpeed="0"/></Joints>
        </ModelData></Entity>"#;
    let (_, log) = run(xml, false);
    assert!(log.contains(&"joint_props max_speed=0.01 min_speed=0".to_owned()));
}

#[test]
fn unused_shapes_are_destroyed_and_two_shapes_compound() {
    let xml = r#"<Entity><ModelData><Mesh Filename="a.dae"/>
        <Shapes><Shape ID="1" ShapeType="Box"/><Shape ID="2" ShapeType="sphere"/><Shape ID="3" ShapeType="capsule"/></Shapes>
        <Bodies><Body ID="1" Name="b"><Shape ID="1"/><Shape ID="2"/></Body></Bodies>
        </ModelData></Entity>"#;
    let (_, log) = run(xml, false);
    assert!(log.contains(&"compound 2".to_owned()));
    assert_eq!(
        log.iter()
            .filter(|l| l.starts_with("destroy_shape"))
            .count(),
        1
    );
}

#[test]
fn world_entities_are_scaled_and_unknown_logged() {
    let xml = r#"<Entity><ModelData><Mesh Filename="a.dae"/>
        <Entities><Sound/><Banana/></Entities></ModelData></Entity>"#;
    let (_, log) = run(xml, false);
    assert!(log.contains(&"world_entity Sound crate_".to_owned()));
    assert!(log.contains(&"set_position 2 4 6".to_owned()));
    assert!(log.contains(&"error Entity world entity type 'Banana' is unknown!\n".to_owned()));
}

#[test]
fn sub_mesh_ids_fall_back_and_ids_reset() {
    let xml = r#"<Entity><ModelData><Mesh Filename="a.dae">
        <SubMesh Name="a" SubMeshID="7" WorldPos="1 0 0"/><SubMesh Name="missing"/></Mesh></ModelData></Entity>"#;
    let (_, log) = run(xml, false);
    assert!(log.contains(&"set_unique_id 2 7".to_owned()));
    assert!(log.contains(&"missing_sub_mesh missing".to_owned()));
    assert!(log.contains(&"set_unique_id 2 -1".to_owned()));
}
