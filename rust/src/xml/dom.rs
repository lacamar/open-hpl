//! tinyXML-shaped DOM: documents hold elements, text, comments, declarations and unknown nodes in
//! document order, and elements keep attributes in insertion order.
#![allow(
    clippy::indexing_slicing,
    clippy::unwrap_used,
    clippy::expect_used,
    clippy::panic,
    clippy::unreachable,
    clippy::integer_division_remainder_used,
    clippy::string_slice,
    reason = "panic audit pending (open-hpl-smj.23)"
)]

pub type NodeId = u32;
pub const DOC: NodeId = 0;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Enc {
    Unknown,
    Utf8,
    Legacy,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Kind {
    Document,
    Element,
    Comment,
    Text {
        cdata: bool,
    },
    Declaration {
        version: Vec<u8>,
        encoding: Vec<u8>,
        standalone: Vec<u8>,
    },
    Unknown,
}

#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Attr {
    pub name: Vec<u8>,
    pub value: Vec<u8>,
}

#[derive(Clone, Debug)]
pub struct Node {
    pub kind: Kind,
    pub value: Vec<u8>,
    pub parent: Option<NodeId>,
    pub first: Option<NodeId>,
    pub last: Option<NodeId>,
    pub next: Option<NodeId>,
    pub attrs: Vec<Attr>,
}

impl Node {
    fn new(kind: Kind, value: Vec<u8>) -> Self {
        Self {
            kind,
            value,
            parent: None,
            first: None,
            last: None,
            next: None,
            attrs: Vec::new(),
        }
    }
}

pub const ERR_OPENING_FILE: i32 = 2;
pub const ERR_PARSING_ELEMENT: i32 = 3;
pub const ERR_FAILED_TO_READ_ELEMENT_NAME: i32 = 4;
pub const ERR_READING_ELEMENT_VALUE: i32 = 5;
pub const ERR_READING_ATTRIBUTES: i32 = 6;
pub const ERR_PARSING_EMPTY: i32 = 7;
pub const ERR_READING_END_TAG: i32 = 8;
pub const ERR_PARSING_UNKNOWN: i32 = 9;
pub const ERR_PARSING_COMMENT: i32 = 10;
pub const ERR_PARSING_DECLARATION: i32 = 11;
pub const ERR_DOCUMENT_EMPTY: i32 = 12;
pub const ERR_PARSING_CDATA: i32 = 14;

const ERROR_STRINGS: [&str; 16] = [
    "No error",
    "Error",
    "Failed to open file",
    "Error parsing Element.",
    "Failed to read Element name",
    "Error reading Element value.",
    "Error reading Attributes.",
    "Error: empty tag.",
    "Error reading end tag.",
    "Error parsing Unknown.",
    "Error parsing Comment.",
    "Error parsing Declaration.",
    "Error document empty.",
    "Error null (0) or unexpected EOF found in input stream.",
    "Error parsing CDATA.",
    "Error when TiXmlDocument added to document, because TiXmlDocument can only be at the root.",
];

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct ErrorInfo {
    pub id: i32,
    pub desc: String,
    pub row: i32,
    pub col: i32,
}

impl Default for ErrorInfo {
    /// A fresh document reports row and column 1 with an empty description.
    fn default() -> Self {
        Self {
            id: 0,
            desc: String::new(),
            row: 1,
            col: 1,
        }
    }
}

#[derive(Debug)]
pub struct Doc {
    pub(super) nodes: Vec<Node>,
    pub(super) error: bool,
    pub(super) err: ErrorInfo,
    pub(super) use_bom: bool,
}

impl Default for Doc {
    fn default() -> Self {
        Self::new()
    }
}

/// Bytes up to the first NUL: what a `const char*` consumer sees.
pub fn c_str(s: &[u8]) -> &[u8] {
    &s[..s.iter().position(|&b| b == 0).unwrap_or(s.len())]
}

impl Doc {
    pub fn new() -> Self {
        Self {
            nodes: vec![Node::new(Kind::Document, Vec::new())],
            error: false,
            err: ErrorInfo::default(),
            use_bom: false,
        }
    }

    pub fn clear(&mut self) {
        self.nodes.truncate(1);
        self.nodes[0].first = None;
        self.nodes[0].last = None;
    }

    pub fn error(&self) -> bool {
        self.error
    }

    /// `ErrorId`, `ErrorDesc`, `ErrorRow()+1`-style row and column, as tinyXML reports them.
    pub fn error_info(&self) -> &ErrorInfo {
        &self.err
    }

    pub(super) fn clear_error(&mut self) {
        self.error = false;
        self.err = ErrorInfo::default();
    }

    /// Row and column are the cursor, `None` meaning an unknown location (reported as 0, 0).
    pub(super) fn set_error(&mut self, id: i32, at: Option<(i32, i32)>) {
        if self.error {
            return;
        }
        self.error = true;
        let (row, col) = at.unwrap_or((-1, -1));
        self.err = ErrorInfo {
            id,
            desc: ERROR_STRINGS[id as usize].to_owned(),
            row: row + 1,
            col: col + 1,
        };
    }

    pub fn node(&self, id: NodeId) -> &Node {
        &self.nodes[id as usize]
    }

    pub(super) fn alloc(&mut self, kind: Kind, value: Vec<u8>) -> NodeId {
        self.nodes.push(Node::new(kind, value));
        (self.nodes.len() - 1) as NodeId
    }

    pub(super) fn link_end_child(&mut self, parent: NodeId, child: NodeId) {
        let last = self.nodes[parent as usize].last;
        self.nodes[child as usize].parent = Some(parent);
        self.nodes[child as usize].next = None;
        match last {
            Some(l) => self.nodes[l as usize].next = Some(child),
            None => self.nodes[parent as usize].first = Some(child),
        }
        self.nodes[parent as usize].last = Some(child);
    }

    pub fn add_element(&mut self, parent: NodeId, name: &[u8]) -> NodeId {
        let id = self.alloc(Kind::Element, c_str(name).to_vec());
        self.link_end_child(parent, id);
        id
    }

    pub fn set_value(&mut self, id: NodeId, value: &[u8]) {
        self.nodes[id as usize].value = c_str(value).to_vec();
    }

    pub fn children(&self, id: NodeId) -> impl Iterator<Item = NodeId> + '_ {
        std::iter::successors(self.node(id).first, |&c| self.node(c).next)
    }

    fn is_element_named(&self, id: NodeId, name: Option<&[u8]>) -> bool {
        let n = self.node(id);
        n.kind == Kind::Element && name.is_none_or(|nm| c_str(&n.value) == c_str(nm))
    }

    pub fn first_child_element(&self, parent: NodeId, name: Option<&[u8]>) -> Option<NodeId> {
        self.children(parent)
            .find(|&c| self.is_element_named(c, name))
    }

    pub fn next_sibling_element(&self, id: NodeId, name: Option<&[u8]>) -> Option<NodeId> {
        std::iter::successors(self.node(id).next, |&c| self.node(c).next)
            .find(|&c| self.is_element_named(c, name))
    }

    /// `TiXmlElement::GetText`: the value of the first child when that child is a text node.
    pub fn text(&self, id: NodeId) -> Option<&[u8]> {
        let first = self.node(self.node(id).first?);
        matches!(first.kind, Kind::Text { .. }).then(|| c_str(&first.value))
    }

    /// `TiXmlAttributeSet::Next` ends the list at an attribute whose successor has an empty name
    /// and value, which is how it recognises its sentinel.
    pub fn attrs(&self, id: NodeId) -> impl Iterator<Item = &Attr> {
        let a = &self.node(id).attrs;
        let end = a
            .iter()
            .skip(1)
            .position(|x| x.name.is_empty() && x.value.is_empty())
            .map_or(a.len(), |p| p + 1);
        a[..end].iter()
    }

    pub fn attribute(&self, id: NodeId, name: &[u8]) -> Option<&[u8]> {
        self.node(id)
            .attrs
            .iter()
            .find(|a| c_str(&a.name) == c_str(name))
            .map(|a| c_str(&a.value))
    }

    pub fn set_attribute(&mut self, id: NodeId, name: &[u8], value: &[u8]) {
        let (name, value) = (c_str(name), c_str(value));
        let attrs = &mut self.nodes[id as usize].attrs;
        match attrs.iter_mut().find(|a| c_str(&a.name) == name) {
            Some(a) => a.value = value.to_vec(),
            None => attrs.push(Attr {
                name: name.to_vec(),
                value: value.to_vec(),
            }),
        }
    }
}
