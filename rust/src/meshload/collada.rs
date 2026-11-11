//! Collada `.dae` extraction (`cMeshLoaderCollada::FillStructures` and the `Load*` helpers):
//! XML in, plain data out. Input the original crashes, hangs or reads out of bounds on is
//! reported as `Fail` instead.

use crate::math::{matrix_mul, matrix_scale, to_rad};
use crate::string;
use crate::types::{Matrixf, Quaternion, Vector3f};
use crate::xml::{DOC, Doc, Kind, NodeId, c_str};
use std::hint::black_box;

pub type Bytes = Vec<u8>;

#[derive(Debug, PartialEq, Eq)]
pub struct Fail(pub &'static str);

#[derive(Debug, PartialEq, Eq)]
pub enum ParseError {
    Xml,
    Fail(Fail),
}

pub type Res<T> = Result<T, Fail>;

pub const LOG: u8 = 0;
pub const WARNING: u8 = 1;
pub const ERROR: u8 = 2;

#[derive(Debug, Default, Clone, Copy)]
pub struct Opts {
    pub convert_unit: bool,
    pub convert_unit_any: bool,
    pub images: bool,
    pub textures: bool,
    pub materials: bool,
    pub lights: bool,
    pub geometry: bool,
    pub controllers: bool,
    pub anims: bool,
    pub scene: bool,
}

#[derive(Debug, Default, PartialEq)]
pub struct Image {
    pub id: Bytes,
    pub name: Bytes,
    pub source: Bytes,
}

#[derive(Debug, Default, PartialEq)]
pub struct Texture {
    pub id: Bytes,
    pub image: Bytes,
}

#[derive(Debug, Default, PartialEq)]
pub struct Material {
    pub id: Bytes,
    pub name: Bytes,
    pub texture: Bytes,
}

#[derive(Debug, Default, PartialEq)]
pub struct Light {
    pub id: Bytes,
    pub name: Bytes,
    pub kind: Bytes,
    pub color: [f32; 3],
    pub angle: f32,
    pub angle_uninit: bool,
}

#[derive(Debug, Default, PartialEq)]
pub struct VtxArray {
    pub id: Bytes,
    pub kind: Bytes,
    pub in_vertex: bool,
    /// Only filled where the original returns before its `Clear()`.
    pub data: Vec<[f32; 3]>,
}

#[derive(Debug, Default, Clone, Copy, PartialEq)]
pub struct Vertex {
    pub pos: [f32; 3],
    pub tex: [f32; 3],
    pub norm: [f32; 3],
    pub col: [f32; 4],
}

#[derive(Debug, PartialEq)]
pub struct Geometry {
    pub id: Bytes,
    pub name: Bytes,
    pub material: Bytes,
    pub arrays: Vec<VtxArray>,
    pub pos_idx_num: i32,
    pub norm_idx_num: i32,
    pub tex_idx_num: i32,
    pub pos_array_idx: i32,
    pub norm_array_idx: i32,
    pub tex_array_idx: i32,
    pub vertices: Vec<Vertex>,
    pub indices: Vec<u32>,
    pub tangents: Vec<f32>,
}

impl Default for Geometry {
    fn default() -> Self {
        Self {
            id: Bytes::new(),
            name: Bytes::new(),
            material: Bytes::new(),
            arrays: Vec::new(),
            pos_idx_num: -1,
            norm_idx_num: -1,
            tex_idx_num: -1,
            pos_array_idx: -1,
            norm_array_idx: -1,
            tex_array_idx: -1,
            vertices: Vec::new(),
            indices: Vec::new(),
            tangents: Vec::new(),
        }
    }
}

#[derive(Debug, Default, PartialEq)]
pub struct Controller {
    pub target: Bytes,
    pub id: Bytes,
    pub bind: Option<[f32; 16]>,
    pub joints: Vec<Bytes>,
    pub weights: Vec<f32>,
    pub matrices: Vec<[f32; 16]>,
    pub pairs: Vec<Vec<(i32, i32)>>,
}

#[derive(Debug, Default, PartialEq)]
pub struct Channel {
    pub id: Bytes,
    pub target: Bytes,
    pub source: Bytes,
}

#[derive(Debug, Default, PartialEq)]
pub struct Sampler {
    pub id: Bytes,
    pub time_array: Bytes,
    pub value_array: Bytes,
    pub target: Bytes,
}

#[derive(Debug, Default, PartialEq)]
pub struct AnimSource {
    pub id: Bytes,
    pub values: Vec<f32>,
}

#[derive(Debug, Default, PartialEq)]
pub struct Animation {
    pub id: Bytes,
    pub target_node: Bytes,
    pub channels: Vec<Channel>,
    pub samplers: Vec<Sampler>,
    pub sources: Vec<AnimSource>,
}

#[derive(Debug, Default, PartialEq)]
pub struct Transform {
    pub sid: Bytes,
    pub kind: Bytes,
    pub values: Vec<f32>,
}

#[derive(Debug, PartialEq)]
pub struct Node {
    pub id: Bytes,
    pub name: Bytes,
    pub sid: Bytes,
    pub kind: Bytes,
    pub instance_material: Bytes,
    pub source: Bytes,
    pub source_is_file: bool,
    pub source_is_light: bool,
    pub mtx: Matrixf,
    pub mtx_world: Matrixf,
    pub scale: [f32; 3],
    pub parent: i32,
    pub transforms: Vec<Transform>,
}

#[derive(Debug, Default, PartialEq)]
pub struct Scene {
    pub start: f32,
    pub end: f32,
    pub delta: f32,
    pub nodes: Vec<Node>,
}

#[derive(Debug, Default, PartialEq)]
pub struct Collada {
    pub z_to_y: bool,
    pub unit_scale: f32,
    pub images: Vec<Image>,
    pub textures: Vec<Texture>,
    pub materials: Vec<Material>,
    pub lights: Vec<Light>,
    pub geometries: Vec<Geometry>,
    pub controllers: Vec<Controller>,
    pub animations: Vec<Animation>,
    pub scene: Scene,
    pub log: Vec<(u8, Bytes)>,
}

pub(super) struct Ctx<'a> {
    pub d: &'a Doc,
    pub o: Opts,
    pub out: Collada,
}

pub(super) fn identity() -> Matrixf {
    let mut m = Matrixf::default();
    for (i, row) in m.m.iter_mut().enumerate() {
        if let Some(c) = row.get_mut(i) {
            *c = 1.0;
        }
    }
    m
}

pub(super) fn floats(s: &[u8]) -> Vec<f32> {
    string::split(s, None, true)
        .map(string::token_to_float)
        .collect()
}

pub(super) fn ints(s: &[u8]) -> Vec<i32> {
    string::split(s, None, true)
        .map(string::token_to_int)
        .collect()
}

pub(super) fn get<T: Copy>(v: &[T], i: i64, what: &'static str) -> Res<T> {
    usize::try_from(i)
        .ok()
        .and_then(|i| v.get(i))
        .copied()
        .ok_or(Fail(what))
}

pub(super) fn lower(s: &[u8]) -> Bytes {
    string::to_lower(s)
}

pub(super) fn sub(s: &[u8], start: i32, count: i32) -> &[u8] {
    string::sub(s, start, count, false).unwrap_or_default()
}

/// `GetAdress`: drops a leading `#`.
pub(super) fn addr(s: &mut Bytes) {
    if s.first() == Some(&b'#') {
        s.remove(0);
    }
}

pub(super) fn cat(parts: &[&[u8]]) -> Bytes {
    parts.concat()
}

impl<'a> Ctx<'a> {
    pub fn el(&self, p: NodeId, name: &str) -> Option<NodeId> {
        self.d.first_child_element(p, Some(name.as_bytes()))
    }

    pub fn nx(&self, n: NodeId, name: &str) -> Option<NodeId> {
        self.d.next_sibling_element(n, Some(name.as_bytes()))
    }

    pub fn any(&self, p: NodeId) -> Option<NodeId> {
        self.d.first_child_element(p, None)
    }

    pub fn nx_any(&self, n: NodeId) -> Option<NodeId> {
        self.d.next_sibling_element(n, None)
    }

    pub fn at(&self, n: NodeId, name: &str) -> Option<&'a [u8]> {
        self.d.attribute(n, name.as_bytes())
    }

    pub fn sa(&self, n: NodeId, name: &str) -> Bytes {
        self.at(n, name).unwrap_or_default().to_vec()
    }

    pub fn ia(&self, n: NodeId, name: &str, def: i32) -> i32 {
        self.at(n, name).map_or(def, string::token_to_int)
    }

    pub fn value(&self, n: NodeId) -> &'a [u8] {
        c_str(&self.d.node(n).value)
    }

    /// `FirstChild()->ToText()`: `Ok(None)` when the first child is not text; the original
    /// dereferences null when there is no child at all.
    pub fn text(&self, n: NodeId) -> Res<Option<&'a [u8]>> {
        let first = self.d.node(n).first.ok_or(Fail("element without child"))?;
        let node = self.d.node(first);
        Ok(matches!(node.kind, Kind::Text { .. }).then(|| c_str(&node.value)))
    }

    /// `->ToText()->Value()`: null text dereferenced.
    pub fn text_req(&self, n: NodeId) -> Res<&'a [u8]> {
        self.text(n)?.ok_or(Fail("child is not text"))
    }

    pub fn log(&mut self, level: u8, msg: Bytes) {
        self.out.log.push((level, msg));
    }

    pub fn light_exists(&self, id: &[u8]) -> bool {
        self.out.lights.iter().any(|l| l.id == id)
    }
}

pub fn parse(data: &[u8], read_ok: bool, o: Opts) -> Result<Collada, ParseError> {
    let mut doc = Doc::new();
    if !doc.load(data, read_ok) {
        return Err(ParseError::Xml);
    }
    let mut c = Ctx {
        d: &doc,
        o,
        out: Collada {
            unit_scale: 1.0,
            ..Collada::default()
        },
    };
    c.fill().map_err(ParseError::Fail)?;
    Ok(c.out)
}

impl Ctx<'_> {
    fn fill(&mut self) -> Res<()> {
        let root = self.any(DOC).ok_or(Fail("no root element"))?;

        if let Some(asset) = self.el(root, "asset") {
            self.asset(asset)?;
        } else {
            self.log(ERROR, b"Couldn't find asset element!\n".to_vec());
        }

        let mut lib = self.any(root);
        while let Some(l) = lib {
            let (ty, val) = (self.sa(l, "type"), self.value(l));
            if (ty == b"LIGHT" || val == b"library_lights") && self.o.lights {
                self.load_lights(l)?;
            }
            lib = self.nx_any(l);
        }

        if self.o.scene {
            self.load_scene(root)?;
        }

        let mut lib = self.any(root);
        while let Some(l) = lib {
            let (ty, val) = (self.sa(l, "type"), self.value(l));
            let is = |t: &[u8], v: &[u8]| ty == t || val == v;
            if is(b"IMAGE", b"library_images") && self.o.images {
                self.load_images(l)?;
            } else if is(b"TEXTURE", b"library_effects") && self.o.textures {
                self.load_textures(l)?;
            } else if is(b"MATERIAL", b"library_materials") && self.o.materials {
                self.load_materials(l)?;
            } else if is(b"GEOMETRY", b"library_geometries") && self.o.geometry {
                self.load_geometry(l)?;
            } else if is(b"ANIMATION", b"library_animations") && self.o.scene && self.o.anims {
                self.load_animations(l)?;
            } else if is(b"CONTROLLER", b"library_controllers")
                && self.o.geometry
                && self.o.controllers
            {
                self.load_controllers(l)?;
            }
            lib = self.nx_any(l);
        }
        Ok(())
    }

    fn asset(&mut self, asset: NodeId) -> Res<()> {
        if let Some(up) = self.el(asset, "up_axis") {
            self.out.z_to_y = self.text_req(up)? == b"Z_UP";
        }
        let cont = match self.el(asset, "contributor") {
            Some(c) => self.el(c, "authoring_tool"),
            None => self.el(asset, "authoring_tool"),
        };
        let mut get_unit = false;
        if let Some(c) = cont {
            let first = self.d.node(c).first.ok_or(Fail("authoring_tool empty"))?;
            get_unit = self.value(first) == b"FBX COLLADA exporter";
        }
        if let Some(unit) = self.el(asset, "unit")
            && (get_unit || self.o.convert_unit_any)
            && self.o.convert_unit
        {
            let mut v = 1.0f32;
            for a in self.d.attrs(unit) {
                if c_str(&a.name) == b"meter" {
                    v = string::token_to_float(c_str(&a.value));
                }
            }
            self.out.unit_scale = v;
        }
        Ok(())
    }

    fn load_images(&mut self, lib: NodeId) -> Res<()> {
        let mut e = self.el(lib, "image");
        while let Some(img) = e {
            let source = match self.el(img, "init_from") {
                Some(init) if self.d.node(init).first.is_some() => self.text_req(init)?.to_vec(),
                Some(_) => Bytes::new(),
                None => self.sa(img, "source"),
            };
            self.out.images.push(Image {
                id: self.sa(img, "id"),
                name: self.sa(img, "name"),
                source,
            });
            e = self.nx(img, "image");
        }
        Ok(())
    }

    fn load_textures(&mut self, lib: NodeId) -> Res<()> {
        let mut e = self.any(lib);
        while let Some(t) = e {
            if let Some(tex) = self.texture(t)? {
                self.out.textures.push(tex);
            }
            e = self.nx_any(t);
        }
        Ok(())
    }

    /// `None` where the original `continue`s without adding the texture.
    fn texture(&mut self, t: NodeId) -> Res<Option<Texture>> {
        let mut tex = Texture {
            id: self.sa(t, "id"),
            image: Bytes::new(),
        };
        let Some(profile) = self.el(t, "profile_COMMON") else {
            let mut tech = self.el(t, "technique");
            while let Some(te) = tech {
                let mut input = self.el(te, "input");
                while let Some(i) = input {
                    if self.sa(i, "semantic") == b"IMAGE" {
                        tex.image = self.sa(i, "source");
                    }
                    input = self.nx(i, "input");
                }
                tech = self.nx(te, "technique");
            }
            return Ok(Some(tex));
        };

        let mut params: Vec<(Bytes, Bytes)> = Vec::new();
        let mut np = self.el(profile, "newparam");
        while let Some(n) = np {
            let sid = self.at(n, "sid").ok_or(Fail("newparam without sid"))?;
            let mut data = Bytes::new();
            if let Some(child) = self.any(n) {
                let ty = self.value(child);
                let name = match ty {
                    b"surface" => Some("init_from"),
                    b"sampler2D" => Some("source"),
                    _ => None,
                };
                if let Some(name) = name {
                    if let Some(v) = self.el(child, name) {
                        data = self.text_req(v)?.to_vec();
                    } else {
                        let msg = cat(&[
                            b"Data element '",
                            name.as_bytes(),
                            b"' missing from newparam '",
                            sid,
                            b"'\n",
                        ]);
                        self.log(WARNING, msg);
                    }
                }
            }
            params.push((sid.to_vec(), data));
            np = self.nx(n, "newparam");
        }

        let Some(tech) = self.el(profile, "technique") else {
            self.log(WARNING, b"No effect technique element found!\n".to_vec());
            return Ok(None);
        };
        let none = |s: &mut Self| {
            s.log(WARNING, b"No effect type element found!\n".to_vec());
            Ok(None)
        };
        let Some(mut ty) = self.any(tech) else {
            return none(self);
        };
        if self.value(ty) == b"extra" {
            match self.nx_any(ty) {
                Some(n) => ty = n,
                None => return none(self),
            }
        }
        if let Some(diffuse) = self.el(ty, "diffuse") {
            tex.image = match self.el(diffuse, "texture") {
                Some(lt) => final_source(&params, self.sa(lt, "texture"))?,
                None => Bytes::new(),
            };
        } else {
            self.log(WARNING, b"No diffuse effect element!\n".to_vec());
        }
        Ok(Some(tex))
    }

    fn load_materials(&mut self, lib: NodeId) -> Res<()> {
        let mut e = self.el(lib, "material");
        while let Some(m) = e {
            let mut mat = Material {
                id: self.sa(m, "id"),
                name: self.sa(m, "name"),
                texture: Bytes::new(),
            };
            if let Some(inst) = self.el(m, "instance_effect") {
                mat.texture = self.sa(inst, "url");
            } else {
                let pass = self
                    .el(m, "shader")
                    .and_then(|s| self.el(s, "technique"))
                    .and_then(|t| self.el(t, "pass"));
                // The original `continue`s here without advancing: an endless loop.
                let pass = pass.ok_or(Fail("collada 1.3 material without pass loops forever"))?;
                let mut input = self.el(pass, "input");
                while let Some(i) = input {
                    if self.sa(i, "semantic") == b"TEXTURE" {
                        mat.texture = self.sa(i, "source");
                    }
                    input = self.nx(i, "input");
                }
            }
            self.out.materials.push(mat);
            e = self.nx(m, "material");
        }
        Ok(())
    }

    fn load_lights(&mut self, lib: NodeId) -> Res<()> {
        let mut e = self.el(lib, "light");
        while let Some(l) = e {
            if let Some(light) = self.light(l)? {
                self.out.lights.push(light);
            }
            e = self.nx(l, "light");
        }
        Ok(())
    }

    fn light(&mut self, l: NodeId) -> Res<Option<Light>> {
        let mut light = Light {
            id: self.sa(l, "id"),
            name: self.sa(l, "name"),
            ..Light::default()
        };
        let rgb = |v: Vec<f32>| -> Res<[f32; 3]> {
            match v.as_slice() {
                [r, g, b, ..] => Ok([*r, *g, *b]),
                _ => Err(Fail("light colour has fewer than 3 values")),
            }
        };
        if let Some(tc) = self.el(l, "technique_common") {
            let Some(ty) = self.any(tc) else {
                self.log(LOG, b"No Type element found!\n".to_vec());
                return Ok(None);
            };
            light.kind = self.value(ty).to_vec();
            light.color = match self.el(ty, "color") {
                Some(p) => rgb(floats(self.text_req(p)?))?,
                None => [1.0; 3],
            };
            light.angle = match self.el(ty, "falloff_angle") {
                Some(p) => string::token_to_float(self.text_req(p)?),
                None => 0.0,
            };
        } else {
            light.kind = lower(&self.sa(l, "type"));
            light.angle_uninit = true;
            let mut p = self.el(l, "param");
            while let Some(pe) = p {
                let name = self.sa(pe, "name");
                let text = self.text_req(pe)?;
                if name == b"COLOR" {
                    light.color = rgb(floats(text))?;
                } else if name == b"ANGLE" {
                    light.angle = string::token_to_float(text);
                    light.angle_uninit = false;
                }
                p = self.nx(pe, "param");
            }
        }
        Ok(Some(light))
    }
}

fn final_source(params: &[(Bytes, Bytes)], id: Bytes) -> Res<Bytes> {
    let mut cur = id;
    for _ in 0..=params.len() {
        match params.iter().find(|p| p.0 == cur) {
            Some(p) => cur = p.1.clone(),
            None => return Ok(cur),
        }
    }
    Err(Fail("newparam reference cycle recurses forever"))
}

pub(super) fn mat_from(v: &[f32]) -> Res<Matrixf> {
    let mut m = Matrixf::default();
    for (i, row) in m.m.iter_mut().enumerate() {
        for (j, c) in row.iter_mut().enumerate() {
            *c = get(v, (i * 4 + j) as i64, "matrix has fewer than 16 values")?;
        }
    }
    Ok(m)
}

unsafe extern "C" {
    safe fn ohpl_math_matrix_translate(t: &Vector3f, out: &mut Matrixf);
    safe fn ohpl_math_matrix_quaternion(q: &Quaternion, out: &mut Matrixf);
    safe fn ohpl_quaternion_from_angle_axis(q: &mut Quaternion, angle: f32, axis: &Vector3f);
}

pub(super) fn translate_matrix(t: Vector3f) -> Matrixf {
    let mut m = Matrixf::default();
    ohpl_math_matrix_translate(&t, &mut m);
    m
}

pub(super) fn rotation_matrix(angle_deg: f32, axis: Vector3f) -> Matrixf {
    let mut q = Quaternion::default();
    let mut m = Matrixf::default();
    ohpl_quaternion_from_angle_axis(&mut q, to_rad(angle_deg), &axis);
    ohpl_math_matrix_quaternion(&q, &mut m);
    m
}

pub(super) fn scale_matrix(s: Vector3f) -> Matrixf {
    matrix_scale(s)
}

/// Opaque operands keep LLVM from folding the identity entries, which changes NaN payloads.
pub(super) fn mul(a: &Matrixf, b: &Matrixf) -> Matrixf {
    matrix_mul(black_box(a), black_box(b))
}

pub(super) fn flat16(m: &Matrixf) -> [f32; 16] {
    let mut out = [0.0f32; 16];
    for (d, s) in out.iter_mut().zip(m.m.iter().flatten()) {
        *d = *s;
    }
    out
}
