use super::collada::{Collada, Fail, Opts, ParseError, parse};

fn all() -> Opts {
    Opts {
        convert_unit: true,
        convert_unit_any: false,
        images: true,
        textures: true,
        materials: true,
        lights: true,
        geometry: true,
        controllers: true,
        anims: true,
        scene: true,
    }
}

const QUAD: &str = r##"<?xml version="1.0"?>
<COLLADA>
 <asset><contributor><authoring_tool>FBX COLLADA exporter</authoring_tool></contributor>
  <unit meter="0.01" name="cm"/><up_axis>Z_UP</up_axis></asset>
 <library_lights><light id="L" name="lamp"><technique_common><point><color>1 0.5 0.25</color></point></technique_common></light></library_lights>
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

fn run(xml: &str, o: Opts) -> Result<Collada, ParseError> {
    parse(xml.as_bytes(), true, o)
}

#[test]
fn quad_geometry_and_scene() {
    let c = run(QUAD, all()).unwrap();
    assert!(c.z_to_y);
    assert_eq!(c.unit_scale, 0.01);
    assert_eq!(c.images[0].source, b"a.png");
    assert_eq!(c.textures[0].image, b"img");
    assert_eq!(c.materials[0].texture, b"#fx");
    assert_eq!(c.lights[0].color, [1.0, 0.5, 0.25]);
    let g = &c.geometries[0];
    assert_eq!(g.name, b"quad");
    assert_eq!(g.vertices.len(), 4);
    assert_eq!(g.indices, [0, 1, 2, 0, 2, 3]);
    assert_eq!(g.vertices[2].pos, [-1.0, 0.0, 1.0]);
    assert_eq!(g.vertices[2].norm, [-0.0, 1.0, 0.0]);
    assert_eq!(g.vertices[2].tex, [1.0, 0.0, 0.0]);
    assert_eq!(g.tangents.len(), 16);
    let n = &c.scene.nodes;
    assert_eq!((n.len(), n[1].parent), (2, 0));
    assert_eq!(n[0].scale, [2.0, 2.0, 2.0]);
    assert_eq!(n[0].source, b"g-lib");
    assert_eq!(n[0].mtx.m[0][3], 1.0);
    assert_eq!(n[0].mtx.m[1][3], 3.0);
    assert_eq!(n[0].mtx.m[0][0], 2.0);
}

#[test]
fn shared_vertex_splits_on_distinct_uv() {
    let xml = QUAD.replace(
        "<p>0 0 0 1 0 1 2 0 2 0 0 0 2 0 2 3 0 3</p>",
        "<p>0 0 0 1 0 1 2 0 2 0 0 3 2 0 2 3 0 3</p>",
    );
    let c = run(&xml, all()).unwrap();
    let g = &c.geometries[0];
    assert_eq!(g.vertices.len(), 5);
    assert_eq!(g.indices, [0, 1, 2, 4, 2, 3]);
}

#[test]
fn no_geometry_when_not_requested() {
    let o = Opts {
        geometry: false,
        ..all()
    };
    assert!(run(QUAD, o).unwrap().geometries.is_empty());
}

#[test]
fn rejects_what_the_original_crashes_on() {
    let bad = |xml: &str| matches!(run(xml, all()), Err(ParseError::Fail(Fail(_))));
    assert!(bad(
        "<COLLADA><library_materials><material id='m'/></library_materials></COLLADA>"
    ));
    assert!(bad("<COLLADA><asset><up_axis/></asset></COLLADA>"));
    assert_eq!(run("<a", all()).unwrap_err(), ParseError::Xml);
    assert_eq!(parse(b"", true, all()).unwrap_err(), ParseError::Xml);
}

#[test]
fn controller_pairs_and_blank_joint_names() {
    let xml = r##"<COLLADA><library_controllers><controller id="c"><skin source="#g">
      <source id="j"><Name_array count="2">root a b root</Name_array></source>
      <source id="w"><float_array count="2">0.5 1</float_array></source>
      <joints><input semantic="JOINT" source="#j"/></joints>
      <vertex_weights><input semantic="JOINT" offset="0" source="#j"/><input semantic="WEIGHT" offset="1" source="#w"/>
       <vcount>2 1</vcount><v>0 0 1 1 0 0</v></vertex_weights></skin></controller></library_controllers></COLLADA>"##;
    let o = Opts {
        geometry: true,
        controllers: true,
        ..Opts::default()
    };
    let c = run(xml, o).unwrap();
    let k = &c.controllers[0];
    assert_eq!(k.joints, [b"root a b".to_vec()]);
    assert_eq!(k.weights, [0.5, 1.0]);
    assert_eq!(k.pairs, [vec![(0, 0), (1, 1)], vec![(0, 0)]]);
    assert!(k.bind.is_some());
}

#[test]
fn animation_groups_by_target_node() {
    let xml = r##"<COLLADA><library_animations><animation id="a"><animation id="a1">
      <source id="t"><float_array count="2">0 1</float_array></source>
      <sampler id="s"><input semantic="INPUT" source="#t"/><input semantic="OUTPUT" source="#t"/></sampler>
      <channel source="#s" target="bone/translate.X"/></animation></animation></library_animations>
      <library_visual_scenes><visual_scene/></library_visual_scenes></COLLADA>"##;
    let c = run(xml, all()).unwrap();
    let a = &c.animations[0];
    assert_eq!(a.target_node, b"bone");
    assert_eq!(a.samplers[0].target, b"bone/translate.X");
    assert_eq!(a.sources[0].values, [0.0, 1.0]);
    assert_eq!(c.scene.delta, -1.0);
}

#[test]
fn texcoord_index_past_its_array_reads_zero() {
    let xml = QUAD.replace(
        "<p>0 0 0 1 0 1 2 0 2 0 0 0 2 0 2 3 0 3</p>",
        "<p>0 0 0 1 0 1 2 0 2 0 0 0 2 0 2 3 0 9</p>",
    );
    let c = run(&xml, all()).unwrap();
    let g = &c.geometries[0];
    assert_eq!(g.vertices[3].tex, [0.0, 1.0, 0.0]);
    assert!(c.log.iter().any(|(_, m)| m.ends_with(b"using zero\n")));
}

#[test]
fn position_index_past_its_array_is_rejected() {
    let xml = QUAD.replace(
        "<p>0 0 0 1 0 1 2 0 2 0 0 0 2 0 2 3 0 3</p>",
        "<p>0 0 0 1 0 1 2 0 2 0 0 0 2 0 2 9 0 3</p>",
    );
    assert!(matches!(run(&xml, all()), Err(ParseError::Fail(_))));
}

#[test]
fn early_exit_keeps_source_arrays() {
    let xml = QUAD.replace(
        "<input semantic=\"NORMAL\" source=\"#nrm\" offset=\"1\"/>",
        "",
    );
    let c = run(&xml, all()).unwrap();
    assert_eq!(c.geometries[0].arrays[0].data.len(), 4);
    assert!(c.geometries[0].vertices.is_empty());
}
