//! XML layer: a tinyXML-compatible DOM, parser and writers, plus the `.cfg` and `.lang` readers
//! built on it. Parity target is the vendored tinyXML 2.5, quirks included.

mod dom;
mod ffi;
pub mod lang;
mod parse;
mod print;

pub use dom::{Attr, DOC, Doc, ErrorInfo, Kind, Node, NodeId, c_str};
pub use print::encode_string;

/// `cConfigFile::GetCharArray`: attribute `name` of the first top-level element called `level`.
pub fn config_get<'a>(doc: &'a Doc, level: &[u8], name: &[u8]) -> Option<&'a [u8]> {
    let e = doc.first_child_element(DOC, Some(level))?;
    doc.attribute(e, name)
}

/// `cConfigFile::SetString`: creates the level element when missing.
pub fn config_set(doc: &mut Doc, level: &[u8], name: &[u8], value: &[u8]) {
    let e = doc
        .first_child_element(DOC, Some(level))
        .unwrap_or_else(|| doc.add_element(DOC, level));
    doc.set_attribute(e, name, value);
}

#[cfg(test)]
mod tests;
