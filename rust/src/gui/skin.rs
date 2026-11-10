//! `cGuiSkin::LoadFromFile`: the `.skin` XML parsed into plain data. Creating fonts and gfx
//! elements from it stays in C++.

use super::gfx::{ANIM_LOOP, ANIM_OSCILLATE, ANIM_RANDOM, ANIM_STOP_AT_END};
use super::guicore::MATERIAL_ALPHA;
use crate::string::{self, to_bool, to_lower, token_to_float};
use crate::xml::{DOC, Doc, NodeId, c_str};

#[derive(Debug, PartialEq)]
pub struct Attribute {
    pub type_name: Vec<u8>,
    pub value: [f32; 3],
}

#[derive(Debug, PartialEq)]
pub struct Font {
    pub type_name: Vec<u8>,
    pub file: Vec<u8>,
    pub size: [f32; 2],
    pub color: [f32; 4],
}

#[derive(Debug, PartialEq)]
pub struct Gfx {
    pub type_name: Vec<u8>,
    pub file: Vec<u8>,
    pub offset: [f32; 2],
    pub active_size: [f32; 2],
    pub color: [f32; 4],
    pub material: u32,
    /// Lower-cased name the C++ warns about when the material is unknown.
    pub material_warn: Option<Vec<u8>>,
    pub animated: bool,
    pub frame_time: f32,
    pub anim_mode: i32,
    pub anim_warn: Option<Vec<u8>>,
}

impl Gfx {
    /// The skin only sets an active size when its width is non-negative.
    pub fn has_active_size(&self) -> bool {
        self.active_size[0] >= 0.0
    }
}

#[derive(Debug, Default, PartialEq)]
pub struct Skin {
    pub attributes: Vec<Attribute>,
    pub fonts: Vec<Font>,
    pub gfx: Vec<Gfx>,
}

#[derive(Debug, PartialEq, Eq)]
pub enum SkinError {
    Xml,
    /// No root element, or no `Attributes`, `Fonts` or `GfxElements` section; C++ dereferences
    /// the null element.
    MissingSection,
}

fn attr<'a>(doc: &'a Doc, e: NodeId, name: &[u8]) -> Option<&'a [u8]> {
    doc.attribute(e, name).map(c_str)
}

fn vector2(doc: &Doc, e: NodeId, name: &[u8], default: f32) -> [f32; 2] {
    let mut v = [0.0; 2];
    match attr(doc, e, name) {
        Some(s) if string::floats_exact(s, &mut v) => v,
        _ => [default; 2],
    }
}

fn color(doc: &Doc, e: NodeId, name: &[u8]) -> [f32; 4] {
    let mut v = [0.0; 4];
    match attr(doc, e, name) {
        Some(s) if string::floats_exact(s, &mut v) => v,
        _ => [1.0; 4],
    }
}

fn type_name(doc: &Doc, e: NodeId) -> Vec<u8> {
    attr(doc, e, b"type").unwrap_or_default().to_vec()
}

fn file_name(doc: &Doc, e: NodeId) -> Vec<u8> {
    attr(doc, e, b"file").unwrap_or_default().to_vec()
}

fn children(doc: &Doc, section: NodeId) -> impl Iterator<Item = NodeId> {
    std::iter::successors(doc.first_child_element(section, None), |&e| {
        doc.next_sibling_element(e, None)
    })
}

/// `ToAnimType`: unknown names fall back to loop and are returned for the warning.
fn anim_type(name: Option<&[u8]>) -> (i32, Option<Vec<u8>>) {
    let Some(name) = name else {
        return (ANIM_LOOP, None);
    };
    let low = to_lower(name);
    let ty = match low.as_slice() {
        b"loop" => ANIM_LOOP,
        b"oscillate" => ANIM_OSCILLATE,
        b"random" => ANIM_RANDOM,
        b"stop_at_end" => ANIM_STOP_AT_END,
        _ => return (ANIM_LOOP, Some(low)),
    };
    (ty, None)
}

/// `ToMaterial`: unknown names fall back to alpha and are returned for the warning.
fn material(name: Option<&[u8]>) -> (u32, Option<Vec<u8>>) {
    let Some(name) = name else {
        return (MATERIAL_ALPHA, None);
    };
    let low = to_lower(name);
    let ty = match low.as_slice() {
        b"alpha" => MATERIAL_ALPHA,
        b"diffuse" => 0,
        b"font_normal" => 2,
        b"additive" => 3,
        b"modulative" => 4,
        b"premulalpha" => 5,
        _ => return (MATERIAL_ALPHA, Some(low)),
    };
    (ty, None)
}

fn attribute(doc: &Doc, e: NodeId) -> Attribute {
    let value = attr(doc, e, b"value").unwrap_or_default();
    let mut v = [0.0f32; 3];
    for (slot, tok) in v
        .iter_mut()
        .zip(string::split(value, Some(&b" "[..]), true))
    {
        *slot = token_to_float(tok);
    }
    Attribute {
        type_name: type_name(doc, e),
        value: v,
    }
}

fn font(doc: &Doc, e: NodeId) -> Font {
    Font {
        type_name: type_name(doc, e),
        file: file_name(doc, e),
        size: vector2(doc, e, b"size", 1.0),
        color: color(doc, e, b"color"),
    }
}

fn gfx(doc: &Doc, e: NodeId) -> Gfx {
    let (material, material_warn) = material(attr(doc, e, b"material"));
    let (anim_mode, anim_warn) = anim_type(attr(doc, e, b"anim_mode"));
    Gfx {
        type_name: type_name(doc, e),
        file: file_name(doc, e),
        offset: vector2(doc, e, b"offset", 0.0),
        active_size: vector2(doc, e, b"active_size", -1.0),
        color: color(doc, e, b"color"),
        material,
        material_warn,
        animated: attr(doc, e, b"animated").is_some_and(to_bool),
        frame_time: attr(doc, e, b"anim_frame_time").map_or(1.0, token_to_float),
        anim_mode,
        anim_warn,
    }
}

/// Parses the file's bytes. `read_ok` is false when reading the file came up short.
pub fn parse(data: &[u8], read_ok: bool) -> Result<Skin, SkinError> {
    let mut doc = Doc::new();
    if !doc.load(data, read_ok) {
        return Err(SkinError::Xml);
    }
    let section = |name: &[u8]| {
        let root = doc.first_child_element(DOC, None)?;
        doc.first_child_element(root, Some(name))
    };
    let (Some(attributes), Some(fonts), Some(gfxs)) = (
        section(b"Attributes"),
        section(b"Fonts"),
        section(b"GfxElements"),
    ) else {
        return Err(SkinError::MissingSection);
    };
    Ok(Skin {
        attributes: children(&doc, attributes)
            .map(|e| attribute(&doc, e))
            .collect(),
        fonts: children(&doc, fonts).map(|e| font(&doc, e)).collect(),
        gfx: children(&doc, gfxs).map(|e| gfx(&doc, e)).collect(),
    })
}
