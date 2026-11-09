//! Language-file reader: walks a parsed `.lang` document and reports resource directories,
//! categories and decoded entries in document order. Name-keyed merging stays with the caller.
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

use super::dom::*;
use crate::string::{to_bool, token_to_int};

pub trait LangSink {
    fn directory(&mut self, path: &[u8], add_sub_dirs: bool);
    fn no_resources(&mut self);
    fn category(&mut self, name: &[u8]);
    fn entry(&mut self, name: &[u8], text: &[i32]);
}

/// `GetDecodedString`: `[br]`, `[uNNN]` and two-byte UTF-8 into wide characters.
pub fn decode_string(s: &[u8]) -> Vec<i32> {
    let at = |i: usize| s.get(i).copied().unwrap_or(0);
    let mut out = Vec::new();
    let mut i = 0;
    while i < s.len() {
        let c = s[i];
        if c == b'[' {
            let mut cmd = Vec::new();
            let mut count = 1;
            while at(i + count) != b']' && i + count < s.len() && count < 16 {
                cmd.push(at(i + count));
                count += 1;
            }
            let found = if cmd == b"br" {
                out.push(i32::from(b'\n'));
                true
            } else if cmd.first() == Some(&b'u') {
                out.push(token_to_int(&cmd[1..]));
                true
            } else {
                false
            };
            if found {
                i += count;
            } else {
                out.push(i32::from(c));
            }
        } else if c >= 128 {
            out.push(i32::from(c & 0x1f) << 6 | i32::from(at(i + 1) & 0x3f));
            i += 1;
        } else {
            out.push(i32::from(c));
        }
        i += 1;
    }
    out
}

/// `None` when the document has no root element.
pub fn read(doc: &Doc, add_dirs: bool, sink: &mut dyn LangSink) -> Option<()> {
    let root = doc.first_child_element(DOC, None)?;
    if add_dirs {
        match doc.first_child_element(root, Some(b"RESOURCES")) {
            Some(res) => {
                let mut dir = doc.first_child_element(res, Some(b"Directory"));
                while let Some(d) = dir {
                    let path = doc.attribute(d, b"Path").unwrap_or_default();
                    if !path.is_empty() {
                        let add_sub = doc.attribute(d, b"AddSubDirs").is_none_or(to_bool);
                        let path = if matches!(path[0], b'/' | b'\\') {
                            &path[1..]
                        } else {
                            path
                        };
                        sink.directory(path, add_sub);
                    }
                    dir = doc.next_sibling_element(d, Some(b"Directory"));
                }
            }
            None => sink.no_resources(),
        }
    }
    let mut cat = doc.first_child_element(root, Some(b"CATEGORY"));
    while let Some(c) = cat {
        sink.category(doc.attribute(c, b"Name").unwrap_or_default());
        let mut entry = doc.first_child_element(c, Some(b"Entry"));
        while let Some(e) = entry {
            let text = match doc.node(e).first {
                Some(f) if matches!(doc.node(f).kind, Kind::Text { .. }) => {
                    decode_string(c_str(&doc.node(f).value))
                }
                _ => Vec::new(),
            };
            sink.entry(doc.attribute(e, b"Name").unwrap_or_default(), &text);
            entry = doc.next_sibling_element(e, Some(b"Entry"));
        }
        cat = doc.next_sibling_element(c, Some(b"CATEGORY"));
    }
    Some(())
}
