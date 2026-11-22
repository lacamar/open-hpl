//! `cAINodeContainer::SaveToFile` / `LoadFromFile`: the `.ainodes` edge cache.

use super::Graph;
use crate::string::{float_to_string, int_to_string, token_to_float, token_to_int};
use crate::xml::{DOC, Doc};
use std::ffi::{OsStr, c_void};
use std::os::unix::ffi::OsStrExt;
use std::path::Path;

impl Graph {
    pub fn save_xml(&self) -> Vec<u8> {
        let mut doc = Doc::new();
        let root = doc.add_element(DOC, b"AINodes");
        for n in &self.nodes {
            let node = doc.add_element(root, b"Node");
            // SAFETY: `c` is live until the node drops.
            let id = unsafe { (*n.c).id };
            doc.set_attribute(node, b"Name", &n.name);
            doc.set_attribute(node, b"ID", &int_to_string(id, 0));
            for e in &n.edges {
                let edge = doc.add_element(node, b"Edge");
                let to = self.nodes.get(e.to as usize).map_or(&[][..], |t| &t.name);
                doc.set_attribute(edge, b"Node", to);
                doc.set_attribute(edge, b"Distance", &float_to_string(e.dist, 20, false));
            }
        }
        doc.print_file()
    }

    /// Edge lines of an `.ainodes` file onto the nodes it names.
    pub fn load_edges(&mut self, doc: &Doc, root: u32) {
        let mut cur = doc.first_child_element(root, Some(b"Node"));
        while let Some(n) = cur {
            cur = doc.next_sibling_element(n, Some(b"Node"));
            let id = doc.attribute(n, b"ID").map_or(-1, token_to_int);
            let Some(from) = self.find_id(id) else {
                continue;
            };
            let mut e = doc.first_child_element(n, Some(b"Edge"));
            while let Some(edge) = e {
                e = doc.next_sibling_element(edge, Some(b"Edge"));
                let name = doc.attribute(edge, b"Node").unwrap_or_default();
                let Some(to) = self.find_name(name) else {
                    continue;
                };
                let d = doc.attribute(edge, b"Distance").map_or(0.0, token_to_float);
                self.add_edge(from, to, d, d * d);
            }
        }
    }
}

fn path<'a>(p: *const u8, len: usize) -> &'a Path {
    // SAFETY: callers pass a readable buffer that outlives the call.
    Path::new(OsStr::from_bytes(unsafe {
        std::slice::from_raw_parts(p, len)
    }))
}

/// # Safety
/// `g` live; `file` readable for `len`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ai_graph_save(g: *const Graph, file: *const u8, len: usize) -> bool {
    // SAFETY: caller contract.
    let Some(g) = (unsafe { g.as_ref() }) else {
        return false;
    };
    std::fs::write(path(file, len), g.save_xml()).is_ok()
}

/// Builds the grid, then reads edges from `file`. 0: done or no file; 1: unreadable XML;
/// 2: no root element. For 1 and 2 `log` receives the parse failure line.
///
/// # Safety
/// `g` live; `file` readable for `len`; `log` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ai_graph_load(
    g: *mut Graph,
    file: *const u8,
    len: usize,
    log: unsafe extern "C" fn(*mut c_void, *const u8, usize),
    ctx: *mut c_void,
) -> i32 {
    // SAFETY: caller contract.
    let Some(g) = (unsafe { g.as_mut() }) else {
        return 0;
    };
    g.build_grid();
    let Ok(data) = std::fs::read(path(file, len)) else {
        return 0;
    };
    let mut doc = Doc::new();
    let (code, row, col, desc) = if !doc.load(&data, true) {
        let e = doc.error_info();
        (1, e.row, e.col, e.desc.clone())
    } else if doc.first_child_element(DOC, None).is_none() {
        (2, 0, 0, "No root element".to_owned())
    } else {
        let root = doc.first_child_element(DOC, None).unwrap_or(DOC);
        g.load_edges(&doc, root);
        return 0;
    };
    let mut msg = b"Failed parsing of XML document ".to_vec();
    msg.extend_from_slice(path(file, len).as_os_str().as_bytes());
    msg.extend_from_slice(format!(" in line {row}, column {col}: {desc}\n").as_bytes());
    // SAFETY: caller contract.
    unsafe { log(ctx, msg.as_ptr(), msg.len()) };
    code
}
