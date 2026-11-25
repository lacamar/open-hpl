//! The MapData walk of `cWorldLoaderHplMap`.

use super::{
    Builder, Decal, Entity, FLAG_FAST_STATIC_LOAD, FLAG_NO_ENTITIES, FLAG_NO_GAME_ENTITIES,
    FLAG_NO_LIGHTS, Fog, NodeKind, Options, Parsed, Phase, Placement, Plane, StaticObject, Xml,
};
use crate::somascript::cfmt::fmt_f;
use crate::string;
use crate::types::{Color, Vector2f, Vector3f};
use crate::xml::AttrRead;

const MAX_FILE_INDICES: i32 = 1 << 20;
const MESSAGE_LIMIT: usize = 4095;

/// A message as the C++ parser formats it: truncated to its 4096-byte buffer.
pub(super) fn message(mut msg: Vec<u8>) -> Vec<u8> {
    msg.truncate(MESSAGE_LIMIT);
    msg
}

pub(super) fn note(parts: &[&[u8]]) -> Vec<u8> {
    message(parts.concat())
}

struct Reader<'a, X: Xml>(&'a X);

struct At<'a, X: Xml>(&'a X, X::Node);

impl<X: Xml> AttrRead for At<'_, X> {
    fn attr(&self, name: &[u8]) -> Option<&[u8]> {
        self.0.attr(self.1, name)
    }
}

impl<X: Xml> At<'_, X> {
    fn vec3(&self, name: &[u8], default: f32) -> Vector3f {
        AttrRead::vec3(
            self,
            name,
            Vector3f {
                x: default,
                y: default,
                z: default,
            },
        )
    }

    fn vec2(&self, name: &[u8]) -> Vector2f {
        AttrRead::vec2(self, name, Vector2f::default())
    }
}

impl<X: Xml> Reader<'_, X> {
    fn at(&self, n: X::Node) -> At<'_, X> {
        At(self.0, n)
    }

    fn placement(&self, n: X::Node) -> Placement {
        let at = self.at(n);
        Placement {
            pos: at.vec3(b"WorldPos", 0.0),
            scale: at.vec3(b"Scale", 1.0),
            rot: at.vec3(b"Rotation", 0.0),
        }
    }
}

const WHITE: Color = Color {
    r: 1.0,
    g: 1.0,
    b: 1.0,
    a: 1.0,
};

struct Parser<'a, X: Xml, B: Builder<X::Node>> {
    read: Reader<'a, X>,
    xml: &'a X,
    builder: &'a mut B,
    flags: u32,
    abort: bool,
    static_objects: Vec<Vec<u8>>,
    entities: Vec<Vec<u8>>,
    decals: Vec<Vec<u8>>,
}

/// Walks `doc` (the document node) and builds through `builder`.
pub fn parse_map<X: Xml, B: Builder<X::Node>>(
    xml: &X,
    doc: X::Node,
    builder: &mut B,
    options: &Options,
    cache_loaded: bool,
) -> Parsed {
    Parser {
        read: Reader(xml),
        xml,
        builder,
        flags: options.flags,
        abort: false,
        static_objects: Vec::new(),
        entities: Vec::new(),
        decals: Vec::new(),
    }
    .parse(doc, cache_loaded)
}

impl<X: Xml, B: Builder<X::Node>> Parser<'_, X, B> {
    fn parse(&mut self, doc: X::Node, cache_loaded: bool) -> Parsed {
        let Some(data) = self.xml.first_child(doc, b"MapData") else {
            return Parsed::Abort;
        };
        let r = self.read.at(data);

        let fog = Fog {
            active: r.bool(b"FogActive", false),
            color: r.color(b"FogColor", WHITE),
            falloff_exp: r.float(b"FogFalloffExp", 1.0),
            start: r.float(b"FogStart", 0.0),
            end: r.float(b"FogEnd", 0.0),
            culling: r.bool(b"FogCulling", true),
        };
        self.builder.set_fog(fog);

        let active = r.bool(b"SkyBoxActive", false);
        let color = r.color(b"SkyBoxColor", WHITE);
        let texture = r.string(b"SkyBoxTexture", b"");
        self.builder.set_skybox(active, color, &texture);

        let Some(contents) = self.xml.first_child(data, b"MapContents") else {
            return Parsed::NoContents;
        };

        self.decals = self.file_index_table(contents, b"FileIndex_Decals");
        self.entities = self.file_index_table(contents, b"FileIndex_Entities");
        self.static_objects = self.file_index_table(contents, b"FileIndex_StaticObjects");
        if self.abort {
            return Parsed::Abort;
        }

        if !cache_loaded {
            self.builder.phase(Phase::Static, true);
            self.load_static_objects(contents);
            if self.abort {
                return Parsed::Abort;
            }
            self.builder.phase(Phase::Static, false);
        }

        if self.flags & FLAG_NO_ENTITIES == 0 {
            self.builder.phase(Phase::Entities, true);
            self.load_entities(contents);
            if self.abort {
                return Parsed::Abort;
            }
            self.builder.phase(Phase::Entities, false);
        }
        Parsed::Ok
    }

    fn file_index_table(&mut self, contents: X::Node, name: &[u8]) -> Vec<Vec<u8>> {
        let Some(table) = self.xml.first_child(contents, name) else {
            return Vec::new();
        };
        let num = self.read.at(table).int(b"NumOfFiles", 0);
        if !(0..=MAX_FILE_INDICES).contains(&num) {
            self.abort = true;
            return Vec::new();
        }
        let mut files = vec![Vec::new(); num as usize];
        for child in self.xml.children(table) {
            let idx = self.read.at(child).int(b"Id", 0);
            let path = self.read.at(child).string(b"Path", b"");
            if let Some(slot) = usize::try_from(idx).ok().and_then(|i| files.get_mut(i)) {
                *slot = path;
            }
        }
        files
    }

    fn transform_valid(&mut self, name: &[u8], pos: Vector3f) -> bool {
        if pos.x.abs() > 10000.0 || pos.y.abs() > 10000.0 || pos.z.abs() > 10000.0 {
            let at = [fmt_f(pos.x, 64), fmt_f(pos.y, 64), fmt_f(pos.z, 64)].join(&b" : "[..]);
            let msg = note(&[
                b"The object ",
                name,
                b" has an invalid position: (",
                &at,
                b")!\n",
            ]);
            self.builder.warning(&msg);
            return false;
        }
        true
    }

    fn error(&mut self, parts: &[&[u8]]) {
        self.builder.error(&note(parts));
    }

    fn load_static_objects(&mut self, contents: X::Node) {
        self.builder.static_begin();

        let Some(objects) = self.xml.first_child(contents, b"StaticObjects") else {
            self.abort = true;
            return;
        };

        self.builder.phase(Phase::StaticObjects, true);
        for node in self.xml.children(objects) {
            self.static_object(node);
        }
        self.builder.phase(Phase::StaticObjects, false);

        if let Some(primitives) = self.xml.first_child(contents, b"Primitives") {
            self.builder.phase(Phase::Primitives, true);
            for node in self.xml.children(primitives) {
                self.primitive(node);
            }
            self.builder.phase(Phase::Primitives, false);
        }

        if let Some(decals) = self.xml.first_child(contents, b"Decals")
            && self.flags & FLAG_FAST_STATIC_LOAD == 0
        {
            self.builder.phase(Phase::Decals, true);
            for node in self.xml.children(decals) {
                self.decal(node);
            }
            self.builder.phase(Phase::Decals, false);
        }

        self.builder.static_loading_done();

        if let Some(combos) = self.xml.first_child(contents, b"StaticObjectCombos") {
            self.builder.phase(Phase::Combos, true);
            for node in self.xml.children(combos) {
                let group = self.read.at(node).int(b"ID", -1);
                let ids = self.read.at(node).string(b"ObjIds", b"");
                let ids: Vec<i32> = string::split(&ids, None, true)
                    .map(string::token_to_int)
                    .collect();
                self.builder.static_combo(group, &ids);
            }
            self.builder.phase(Phase::Combos, false);
        }

        self.builder.static_compile();
    }

    fn static_object(&mut self, node: X::Node) {
        let name = self.read.at(node).string(b"Name", b"");

        let index = self.read.at(node).int(b"FileIndex", -1);
        let file = if index < 0 {
            self.read.at(node).string(b"Filename", b"")
        } else if let Some(file) = self.static_objects.get(index as usize) {
            file.clone()
        } else {
            self.error(&[
                b"Could not load static object '",
                &name,
                b"', File index: ",
                index.to_string().as_bytes(),
                b" does is out of bounds!\n",
            ]);
            return;
        };

        let at = self.read.placement(node);
        let collides = self.read.at(node).bool(b"Collides", true);
        let cast_shadows = self.read.at(node).bool(b"CastShadows", true);
        let id = self.read.at(node).int(b"ID", -1);

        if !self.transform_valid(&name, at.pos) {
            return;
        }
        self.builder.static_object(&StaticObject {
            name: &name,
            file: &file,
            at,
            collides,
            cast_shadows,
            id,
        });
    }

    fn primitive(&mut self, node: X::Node) {
        let kind = self.xml.value(node).to_vec();
        let name = self.read.at(node).string(b"Name", b"");
        let material = self.read.at(node).string(b"Material", b"");
        let cast_shadows = self.read.at(node).bool(b"CastShadows", true);
        let collides = self.read.at(node).bool(b"Collides", true);
        let id = self.read.at(node).int(b"ID", -1);
        let at = self.read.placement(node);

        if !self.transform_valid(&name, at.pos) || kind != b"Plane" {
            return;
        }
        let uv = std::array::from_fn(|i| {
            self.read
                .at(node)
                .vec2(format!("Corner{}UV", i + 1).as_bytes())
        });
        self.builder.static_plane(&Plane {
            name: &name,
            material: &material,
            cast_shadows,
            collides,
            id,
            at,
            start_corner: self.read.at(node).vec3(b"StartCorner", 0.0),
            end_corner: self.read.at(node).vec3(b"EndCorner", 0.0),
            uv,
        });
    }

    fn decal(&mut self, node: X::Node) {
        let name = self.read.at(node).string(b"Name", b"");
        let color = self.read.at(node).color(b"Color", WHITE);
        let id = self.read.at(node).int(b"ID", -1);

        let index = self.read.at(node).int(b"MaterialIndex", -1);
        let material = if index < 0 {
            self.read.at(node).string(b"Material", b"")
        } else if let Some(material) = self.decals.get(index as usize) {
            material.clone()
        } else {
            self.error(&[
                b"Could not load decal '",
                &name,
                b"'! File index ",
                index.to_string().as_bytes(),
                b" is out of bounds!\n",
            ]);
            return;
        };

        let mesh = self.xml.first_child(node, b"DecalMesh");
        self.builder.static_decal(&Decal {
            name: &name,
            color,
            id,
            material: &material,
            mesh,
        });
    }

    fn load_entities(&mut self, contents: X::Node) {
        self.builder.entities_begin();

        let Some(entities) = self.xml.first_child(contents, b"Entities") else {
            self.abort = true;
            return;
        };
        for node in self.xml.children(entities) {
            self.loaded_entity(node);
        }

        self.builder.entities_end();
    }

    fn loaded_entity(&mut self, node: X::Node) {
        let kind = self.xml.value(node).to_vec();

        if kind == b"Entity" || kind == b"Area" {
            let name = self.read.at(node).string(b"Name", b"");
            let id = self.read.at(node).int(b"ID", 0);
            let active = self.read.at(node).bool(b"Active", true);
            let at = self.read.placement(node);

            if !self.transform_valid(&name, at.pos) || self.flags & FLAG_NO_GAME_ENTITIES != 0 {
                return;
            }
            let user_vars = self.xml.first_child(node, b"UserVariables");

            if kind == b"Entity" {
                let index = self.read.at(node).int(b"FileIndex", -1);
                let file = if index < 0 {
                    self.read.at(node).string(b"Filename", b"")
                } else if let Some(file) = self.entities.get(index as usize) {
                    file.clone()
                } else {
                    self.error(&[
                        b"Could not load entity '",
                        &name,
                        b"'!\n File index ",
                        index.to_string().as_bytes(),
                        b" is out of bounds!\n",
                    ]);
                    return;
                };
                self.builder.entity(&Entity {
                    name: &name,
                    id,
                    active,
                    at,
                    file: &file,
                    user_vars,
                });
            } else {
                let area_type = self.read.at(node).string(b"AreaType", b"");
                self.builder.area(&Entity {
                    name: &name,
                    id,
                    active,
                    at,
                    file: &area_type,
                    user_vars,
                });
            }
            return;
        }

        let simple = match kind.as_slice() {
            b"FogArea" => Some(NodeKind::FogArea),
            b"ParticleSystem" => Some(NodeKind::ParticleSystem),
            b"Sound" => Some(NodeKind::Sound),
            b"Billboard" => Some(NodeKind::Billboard),
            _ => None,
        };
        if let Some(simple) = simple {
            self.builder.node(simple, node);
        } else if string::last_pos(&kind, &b"Light"[..]) > 0 {
            if self.flags & FLAG_NO_LIGHTS == 0 {
                self.builder.node(NodeKind::Light, node);
            }
        } else {
            self.error(&[b"Unknown entity type '", &kind, b"'\n"]);
        }
    }
}
