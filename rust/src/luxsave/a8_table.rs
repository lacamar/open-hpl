//! A8: Rust-owned field tables of the Amnesia save classes (what `kBeginSerialize` declares in C++).
//! The tables here decide class parent, field names, order, value type and container kind; C++ only
//! supplies the member offsets and sizes (layout of C++ objects), matched by position after the two
//! agree. A class that disagrees keeps its C++ table and is reported.

use crate::system::ffi::OhplSerField;
use crate::system::serialize::{M_ARRAY, T_NULL};
use std::ffi::{CStr, c_char};
use std::fmt::Write as _;
use std::io::Write as _;

pub struct F {
    pub name: &'static CStr,
    pub class: &'static CStr,
    pub ty: u32,
    pub main: u32,
    pub array: usize,
}

pub struct Cls {
    pub name: &'static CStr,
    pub parent: &'static CStr,
    pub fields: &'static [F],
}

pub fn find(name: &[u8]) -> Option<&'static Cls> {
    super::a8_tables::CLASSES
        .binary_search_by(|c| c.name.to_bytes().cmp(name))
        .ok()
        .and_then(|i| super::a8_tables::CLASSES.get(i))
}

fn cstr<'a>(p: *const c_char) -> &'a [u8] {
    if p.is_null() {
        return &[];
    }
    // SAFETY: callers pass NUL-terminated strings that outlive the call.
    unsafe { CStr::from_ptr(p) }.to_bytes()
}

fn line(out: &mut String, name: &[u8], class: &[u8], ty: u32, main: u32, array: usize) {
    let class = if class.is_empty() {
        "-".into()
    } else {
        String::from_utf8_lossy(class)
    };
    let array = if main == M_ARRAY { array } else { 0 };
    let _ = writeln!(
        out,
        "  {} {class} {ty} {main} {array}",
        String::from_utf8_lossy(name)
    );
}

fn head(name: &[u8], parent: &[u8]) -> String {
    let sep = if parent.is_empty() { "" } else { " : " };
    format!(
        "class {}{sep}{}\n",
        String::from_utf8_lossy(name),
        String::from_utf8_lossy(parent)
    )
}

/// The golden text of one class (`rust/tests/golden/A8/tables.txt`).
pub fn render(c: &Cls) -> String {
    let mut out = head(c.name.to_bytes(), c.parent.to_bytes());
    for f in c.fields {
        line(
            &mut out,
            f.name.to_bytes(),
            f.class.to_bytes(),
            f.ty,
            f.main,
            f.array,
        );
    }
    out
}

fn render_cpp(name: &[u8], parent: &[u8], cpp: &[OhplSerField]) -> String {
    let mut out = head(name, parent);
    for f in cpp.iter().take_while(|f| f.ty != T_NULL) {
        line(
            &mut out,
            cstr(f.name),
            cstr(f.class_name),
            f.ty,
            f.main,
            f.array_size,
        );
    }
    out
}

fn agrees(c: &Cls, parent: &[u8], cpp: &[OhplSerField]) -> bool {
    let n = cpp.iter().take_while(|f| f.ty != T_NULL).count();
    c.parent.to_bytes() == parent
        && n == c.fields.len()
        && cpp.len() == n + 1
        && c.fields.iter().zip(cpp).all(|(f, e)| {
            f.name.to_bytes() == cstr(e.name)
                && f.class.to_bytes() == cstr(e.class_name)
                && f.ty == e.ty
                && f.main == e.main
                && (f.main != M_ARRAY || f.array == e.array_size)
        })
}

fn dump(text: &str) {
    let Some(path) = std::env::var_os("OHPL_A8_TABLE_DUMP") else {
        return;
    };
    if let Ok(mut f) = std::fs::OpenOptions::new()
        .create(true)
        .append(true)
        .open(path)
    {
        let _ = f.write_all(text.as_bytes());
    }
}

/// Builds the registered table of `name` from the Rust table, taking offsets and sizes from the C++
/// table. Classes without a Rust table, or whose C++ table disagrees, keep the C++ table.
///
/// # Safety
/// `name` and `parent` are NUL-terminated; `cpp` strings are NUL-terminated and outlive the process.
pub unsafe fn refine(
    name: *const c_char,
    parent: *const c_char,
    cpp: Vec<OhplSerField>,
) -> Vec<OhplSerField> {
    let Some(c) = find(cstr(name)) else {
        return cpp;
    };
    if std::env::var_os("OHPL_A8_TABLE_DUMP").is_some() {
        dump(&render_cpp(cstr(name), cstr(parent), &cpp));
    }
    if !agrees(c, cstr(parent), &cpp) {
        eprintln!(
            "A8: table of {} differs from the C++ table; keeping C++",
            c.name.to_string_lossy()
        );
        return cpp;
    }
    let mut out: Vec<OhplSerField> = c
        .fields
        .iter()
        .zip(&cpp)
        .map(|(f, e)| OhplSerField {
            name: f.name.as_ptr(),
            class_name: f.class.as_ptr(),
            offset: e.offset,
            size: e.size,
            ty: f.ty,
            main: f.main,
            array_size: if f.main == M_ARRAY {
                f.array
            } else {
                e.array_size
            },
        })
        .collect();
    if let Some(t) = cpp.last() {
        out.push(OhplSerField {
            name: t.name,
            class_name: t.class_name,
            offset: t.offset,
            size: t.size,
            ty: t.ty,
            main: t.main,
            array_size: t.array_size,
        });
    }
    out
}
