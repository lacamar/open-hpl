//! C ABI for the XML layer (`HPL2/core/include/system/RustXml.h`). `OhplXmlDoc` handles are owned
//! by the C++ side until `ohpl_xml_doc_free`; byte arguments are (pointer, length) pairs.

use super::lang::{self, LangSink};
use super::{Doc, NodeId, c_str, config_get, config_set};
use crate::string_ffi::{Sink, emit, units};
use std::ffi::{c_char, c_void};

/// # Safety
/// `doc` comes from `ohpl_xml_doc_new` and is not used afterwards.
unsafe fn doc_mut<'a>(doc: *mut Doc) -> &'a mut Doc {
    // SAFETY: caller contract.
    unsafe { &mut *doc }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_xml_doc_new() -> *mut Doc {
    Box::into_raw(Box::new(Doc::new()))
}

/// # Safety
/// `doc` comes from `ohpl_xml_doc_new` and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_xml_doc_free(doc: *mut Doc) {
    // SAFETY: caller contract.
    drop(unsafe { Box::from_raw(doc) });
}

/// `TiXmlDocument::Parse` on a NUL-terminated string; true when it parsed without error.
///
/// # Safety
/// `doc` is live; `s` is NUL-terminated.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_xml_doc_parse(doc: *mut Doc, s: *const c_char) -> bool {
    // SAFETY: caller contract.
    let (doc, s) = unsafe { (doc_mut(doc), std::ffi::CStr::from_ptr(s)) };
    doc.parse(s.to_bytes());
    !doc.error()
}

/// `TiXmlDocument::LoadFile(FILE*)` once the caller read `len` bytes; true when it loaded.
///
/// # Safety
/// `doc` is live; `data` points to `len` readable bytes (or `len` is 0).
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_xml_doc_load(
    doc: *mut Doc,
    data: *const u8,
    len: usize,
    read_ok: bool,
) -> bool {
    // SAFETY: caller contract.
    let (doc, data) = unsafe { (doc_mut(doc), units(data, len)) };
    doc.load(data, read_ok)
}

/// # Safety
/// `doc` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_xml_doc_error(doc: *const Doc) -> bool {
    // SAFETY: caller contract.
    unsafe { &*doc }.error()
}

/// Writes the error row and column, then delivers the description through `sink`.
///
/// # Safety
/// `doc` is live; `row` and `col` are writable; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_xml_doc_error_info(
    doc: *const Doc,
    row: *mut i32,
    col: *mut i32,
    sink: Sink<u8>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    unsafe {
        let e = (*doc).error_info();
        row.write(e.row);
        col.write(e.col);
        emit(sink, ctx, e.desc.as_bytes());
    }
}

#[repr(C)]
pub struct OhplXmlTreeSink {
    ctx: *mut c_void,
    begin: unsafe extern "C" fn(*mut c_void, *const u8, usize),
    attr: unsafe extern "C" fn(*mut c_void, *const u8, usize, *const u8, usize),
    end: unsafe extern "C" fn(*mut c_void),
}

fn walk(doc: &Doc, id: NodeId, s: &OhplXmlTreeSink) {
    let name = c_str(&doc.node(id).value);
    // SAFETY: the sink contract of `ohpl_xml_doc_walk`.
    unsafe { (s.begin)(s.ctx, name.as_ptr(), name.len()) };
    for a in doc.attrs(id) {
        let (n, v) = (c_str(&a.name), c_str(&a.value));
        // SAFETY: as above.
        unsafe { (s.attr)(s.ctx, n.as_ptr(), n.len(), v.as_ptr(), v.len()) };
    }
    if let Some(t) = doc.text(id) {
        // SAFETY: as above.
        unsafe { (s.attr)(s.ctx, b"_Text".as_ptr(), 5, t.as_ptr(), t.len()) };
    }
    let mut c = doc.first_child_element(id, None);
    while let Some(e) = c {
        walk(doc, e, s);
        c = doc.next_sibling_element(e, None);
    }
    // SAFETY: as above.
    unsafe { (s.end)(s.ctx) };
}

/// Replays the root element and its element descendants (`_Text` carried as an attribute);
/// false when the document has no root element.
///
/// # Safety
/// `doc` is live; `sink` holds callbacks accepting its `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_xml_doc_walk(doc: *const Doc, sink: *const OhplXmlTreeSink) -> bool {
    // SAFETY: caller contract.
    let (doc, sink) = unsafe { (&*doc, &*sink) };
    match doc.first_child_element(super::DOC, None) {
        Some(root) => {
            walk(doc, root, sink);
            true
        }
        None => false,
    }
}

/// Appends an element under `parent` (0 is the document) and returns its id.
///
/// # Safety
/// `doc` is live; `name` points to `n` readable bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_xml_doc_add_element(
    doc: *mut Doc,
    parent: u32,
    name: *const u8,
    n: usize,
) -> u32 {
    // SAFETY: caller contract.
    let (doc, name) = unsafe { (doc_mut(doc), units(name, n)) };
    doc.add_element(parent, name)
}

/// # Safety
/// `doc` is live; pointers valid for their lengths.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_xml_doc_set_value(doc: *mut Doc, id: u32, name: *const u8, n: usize) {
    // SAFETY: caller contract.
    let (doc, name) = unsafe { (doc_mut(doc), units(name, n)) };
    doc.set_value(id, name);
}

/// # Safety
/// `doc` is live; pointers valid for their lengths.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_xml_doc_set_attr(
    doc: *mut Doc,
    id: u32,
    name: *const u8,
    nn: usize,
    val: *const u8,
    vn: usize,
) {
    // SAFETY: caller contract.
    let (doc, name, val) = unsafe { (doc_mut(doc), units(name, nn), units(val, vn)) };
    doc.set_attribute(id, name, val);
}

/// `TiXmlDocument::SaveFile` payload (indented, with BOM when the source had one).
///
/// # Safety
/// `doc` is live; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_xml_doc_print_file(
    doc: *const Doc,
    sink: Sink<u8>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, &(*doc).print_file()) };
}

/// `operator<<(std::string&, TiXmlDocument&)`.
///
/// # Safety
/// `doc` is live; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_xml_doc_print_stream(
    doc: *const Doc,
    sink: Sink<u8>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, &(*doc).print_stream()) };
}

/// Delivers the attribute through `sink` and returns true, or returns false when absent.
///
/// # Safety
/// `doc` is live; pointers valid for their lengths; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_xml_config_get(
    doc: *const Doc,
    level: *const u8,
    ln: usize,
    name: *const u8,
    nn: usize,
    sink: Sink<u8>,
    ctx: *mut c_void,
) -> bool {
    // SAFETY: caller contract.
    unsafe {
        match config_get(&*doc, units(level, ln), units(name, nn)) {
            Some(v) => {
                emit(sink, ctx, v);
                true
            }
            None => false,
        }
    }
}

/// # Safety
/// `doc` is live; pointers valid for their lengths.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_xml_config_set(
    doc: *mut Doc,
    level: *const u8,
    ln: usize,
    name: *const u8,
    nn: usize,
    val: *const u8,
    vn: usize,
) {
    // SAFETY: caller contract.
    let (doc, level, name, val) = unsafe {
        (
            doc_mut(doc),
            units(level, ln),
            units(name, nn),
            units(val, vn),
        )
    };
    config_set(doc, level, name, val);
}

#[repr(C)]
pub struct OhplLangSink {
    ctx: *mut c_void,
    directory: unsafe extern "C" fn(*mut c_void, *const u8, usize, bool),
    no_resources: unsafe extern "C" fn(*mut c_void),
    category: unsafe extern "C" fn(*mut c_void, *const u8, usize),
    entry: unsafe extern "C" fn(*mut c_void, *const u8, usize, *const i32, usize),
}

impl LangSink for &OhplLangSink {
    fn directory(&mut self, path: &[u8], add: bool) {
        // SAFETY: the sink contract of `ohpl_xml_lang_read`.
        unsafe { (self.directory)(self.ctx, path.as_ptr(), path.len(), add) }
    }
    fn no_resources(&mut self) {
        // SAFETY: as above.
        unsafe { (self.no_resources)(self.ctx) }
    }
    fn category(&mut self, name: &[u8]) {
        // SAFETY: as above.
        unsafe { (self.category)(self.ctx, name.as_ptr(), name.len()) }
    }
    fn entry(&mut self, name: &[u8], text: &[i32]) {
        // SAFETY: as above.
        unsafe {
            (self.entry)(
                self.ctx,
                name.as_ptr(),
                name.len(),
                text.as_ptr(),
                text.len(),
            )
        }
    }
}

/// Parses a `.lang` file image and streams its contents to `sink`; false when it does not load.
///
/// # Safety
/// `data` points to `len` readable bytes (or `len` is 0); `sink` holds callbacks accepting its `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_xml_lang_read(
    data: *const u8,
    len: usize,
    read_ok: bool,
    add_dirs: bool,
    sink: *const OhplLangSink,
) -> bool {
    let mut doc = Doc::new();
    // SAFETY: caller contract.
    let (data, mut sink) = unsafe { (units(data, len), &*sink) };
    doc.load(data, read_ok) && lang::read(&doc, add_dirs, &mut sink).is_some()
}

/// `GetDecodedString`.
///
/// # Safety
/// `s` points to `n` readable bytes; `sink` accepts `ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_xml_lang_decode(
    s: *const u8,
    n: usize,
    sink: Sink<i32>,
    ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    unsafe { emit(sink, ctx, &lang::decode_string(units(s, n))) };
}
