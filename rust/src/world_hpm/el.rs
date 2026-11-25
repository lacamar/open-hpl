//! `cXmlElement` as the C++ loader sees it: attributes in a map (a later duplicate wins, element
//! text is the `_Text` attribute), children being elements only.

use crate::xml::{AttrRead, DOC, Doc, NodeId, c_str};
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
}

impl AttrRead for El {
    fn attr(&self, name: &[u8]) -> Option<&[u8]> {
        self.attrs.get(name).map(Vec::as_slice)
    }
}
