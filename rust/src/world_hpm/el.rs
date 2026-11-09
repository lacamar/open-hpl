//! `cXmlElement` as the C++ loader sees it: attributes in a map (a later duplicate wins, element
//! text is the `_Text` attribute), children being elements only.

use crate::string;
use crate::types::{Color, Vector2f, Vector3f};
use crate::xml::{DOC, Doc, NodeId, c_str};
use std::collections::BTreeMap;

#[derive(Debug, Default)]
pub struct El {
    pub name: Vec<u8>,
    pub attrs: BTreeMap<Vec<u8>, Vec<u8>>,
    pub children: Vec<El>,
}

impl El {
    pub fn from_doc(doc: &Doc) -> Option<Self> {
        doc.first_child_element(DOC, None)
            .map(|root| Self::from_node(doc, root))
    }

    fn from_node(doc: &Doc, id: NodeId) -> Self {
        let mut attrs = BTreeMap::new();
        for a in doc.attrs(id) {
            attrs.insert(c_str(&a.name).to_vec(), c_str(&a.value).to_vec());
        }
        if let Some(text) = doc.text(id) {
            attrs.insert(b"_Text".to_vec(), text.to_vec());
        }
        let mut children = Vec::new();
        let mut c = doc.first_child_element(id, None);
        while let Some(e) = c {
            children.push(Self::from_node(doc, e));
            c = doc.next_sibling_element(e, None);
        }
        Self {
            name: c_str(&doc.node(id).value).to_vec(),
            attrs,
            children,
        }
    }

    pub fn first(&self, name: &[u8]) -> Option<&El> {
        self.children.iter().find(|c| c.name == name)
    }

    pub fn attr(&self, name: &[u8]) -> Option<&[u8]> {
        self.attrs.get(name).map(Vec::as_slice)
    }

    pub fn string(&self, name: &[u8]) -> Vec<u8> {
        self.string_or(name, b"")
    }

    pub fn string_or(&self, name: &[u8], default: &[u8]) -> Vec<u8> {
        self.attr(name).unwrap_or(default).to_vec()
    }

    pub fn float(&self, name: &[u8], default: f32) -> f32 {
        self.attr(name).map_or(default, string::token_to_float)
    }

    pub fn int(&self, name: &[u8], default: i32) -> i32 {
        self.attr(name).map_or(default, string::token_to_int)
    }

    pub fn boolean(&self, name: &[u8], default: bool) -> bool {
        self.attr(name).map_or(default, string::to_bool)
    }

    pub fn vec2(&self, name: &[u8], default: Vector2f) -> Vector2f {
        let mut v = [0.0f32; 2];
        match self.attr(name) {
            Some(s) if string::floats_exact(s, &mut v) => Vector2f { x: v[0], y: v[1] },
            _ => default,
        }
    }

    pub fn vec3(&self, name: &[u8], default: Vector3f) -> Vector3f {
        let mut v = [0.0f32; 3];
        match self.attr(name) {
            Some(s) if string::floats_exact(s, &mut v) => Vector3f {
                x: v[0],
                y: v[1],
                z: v[2],
            },
            _ => default,
        }
    }

    pub fn color(&self, name: &[u8], default: Color) -> Color {
        let mut v = [0.0f32; 4];
        match self.attr(name) {
            Some(s) if string::floats_exact(s, &mut v) => Color {
                r: v[0],
                g: v[1],
                b: v[2],
                a: v[3],
            },
            _ => default,
        }
    }
}
