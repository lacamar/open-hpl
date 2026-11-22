//! A8: Rust-owned field tables of the Amnesia save classes (what `kBeginSerialize` declared in C++).
//! The tables decide class parent, field names, order, value type and container kind; the mirrored
//! gcc layouts (`layout*`) supply member offsets and sizes.

use crate::system::ffi::OhplSerField;
use crate::system::serialize::{M_ARRAY, T_NULL};
use std::ffi::CStr;
use std::fmt::Write as _;

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

/// Registered table of a class: Rust field table plus the gcc layout. `None` when the class or one of its fields has no mirror.
pub fn layout_table(name: &[u8]) -> Option<Vec<OhplSerField>> {
    let c = find(name)?;
    let mut out = c
        .fields
        .iter()
        .map(|f| {
            let (offset, size) = super::layout::field(name, f.name.to_bytes())
                .or_else(|| super::layout_ar::field(name, f.name.to_bytes()))
                .or_else(|| super::layout_gcc::field(name, f.name.to_bytes()))?;
            Some(OhplSerField {
                name: f.name.as_ptr(),
                class_name: f.class.as_ptr(),
                offset,
                size,
                ty: f.ty,
                main: f.main,
                array_size: f.array,
            })
        })
        .collect::<Option<Vec<_>>>()?;
    out.push(OhplSerField {
        name: std::ptr::null(),
        class_name: std::ptr::null(),
        offset: 0,
        size: 0,
        ty: T_NULL,
        main: 0,
        array_size: 0,
    });
    Some(out)
}
