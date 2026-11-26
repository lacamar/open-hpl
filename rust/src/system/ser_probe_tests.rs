use super::serialize::*;
use crate::xml::Doc;
use std::mem::offset_of;
use std::sync::Arc;

#[repr(C)]
struct Inner {
    cls: usize,
    a: i32,
    b: f32,
}

#[repr(C)]
struct Probe {
    cls: usize,
    flag: bool,
    num: i32,
    fnum: f32,
    s: Vec<u8>,
    ws: Vec<u32>,
    vec: [f32; 3],
    col: [f32; 4],
    arr: [i32; 3],
    inner: Inner,
    pinner: *mut Inner,
}

struct Names;

impl Host for Names {
    fn log(&self, _: Level, _: &[u8]) {}
    fn top_class(&self, obj: Obj) -> Vec<u8> {
        match unsafe { *obj.cast::<usize>() } {
            0 => b"cProbeInner".to_vec(),
            _ => b"cProbe".to_vec(),
        }
    }
    fn destroy(&self, _: Obj) {}
    fn tmp_new(&self, _: u32, _: usize) -> Obj {
        std::ptr::null_mut()
    }
    fn tmp_free(&self, _: u32, _: Obj) {}
    fn str_get(&self, p: Obj) -> Vec<u8> {
        unsafe { (*p.cast::<Vec<u8>>()).clone() }
    }
    fn str_set(&self, _: Obj, _: &[u8]) {}
    fn wstr_get(&self, p: Obj) -> Vec<u32> {
        unsafe { (*p.cast::<Vec<u32>>()).clone() }
    }
    fn wstr_set(&self, _: Obj, _: &[u32]) {}
    fn string_size(&self) -> usize {
        size_of::<Vec<u8>>()
    }
    fn wstring_size(&self) -> usize {
        size_of::<Vec<u32>>()
    }
    fn cont_size(&self, _: Obj) -> usize {
        0
    }
    fn cont_clear(&self, _: Obj) {}
    fn cont_add_ptr(&self, _: Obj, _: Obj) {}
    fn cont_add_class(&self, _: Obj, _: Obj) {}
    fn cont_iter(&self, _: Obj) -> Obj {
        std::ptr::null_mut()
    }
    fn iter_next(&self, _: Obj) -> Option<Obj> {
        None
    }
    fn iter_free(&self, _: Obj) {}
}

fn field(name: &str, offset: usize, ty: u32, main: u32, array_size: usize) -> Arc<Field> {
    Arc::new(Field {
        name: name.as_bytes().to_vec(),
        class_name: Vec::new(),
        offset,
        size: 0,
        ty,
        main,
        array_size,
    })
}

#[test]
fn h1_probe_matches_cpp_golden() {
    register(Class {
        name: b"cProbeInner".to_vec(),
        parent: Vec::new(),
        fields: vec![
            field("mlA", offset_of!(Inner, a), T_INT32, M_VAR, 0),
            field("mfB", offset_of!(Inner, b), T_FLOAT32, M_VAR, 0),
        ],
        size: size_of::<Inner>(),
        create: None,
    });
    register(Class {
        name: b"cProbe".to_vec(),
        parent: Vec::new(),
        fields: vec![
            field("mbFlag", offset_of!(Probe, flag), T_BOOL, M_VAR, 0),
            field("mlNum", offset_of!(Probe, num), T_INT32, M_VAR, 0),
            field("mfNum", offset_of!(Probe, fnum), T_FLOAT32, M_VAR, 0),
            field("msStr", offset_of!(Probe, s), T_STRING, M_VAR, 0),
            field("msWStr", offset_of!(Probe, ws), T_WSTRING, M_VAR, 0),
            field("mvVec", offset_of!(Probe, vec), T_VEC3F, M_VAR, 0),
            field("mCol", offset_of!(Probe, col), T_COLOR, M_VAR, 0),
            field("mvArr", offset_of!(Probe, arr), T_INT32, M_ARRAY, 3),
            field("mInner", offset_of!(Probe, inner), T_CLASS, M_VAR, 0),
            field("mpInner", offset_of!(Probe, pinner), T_CLASS_PTR, M_VAR, 0),
        ],
        size: size_of::<Probe>(),
        create: None,
    });
    let mut pinner = Inner {
        cls: 0,
        a: 11,
        b: 2.0,
    };
    let mut p = Probe {
        cls: 1,
        flag: true,
        num: -42,
        fnum: 3.25,
        s: b"a<b>&\"q\"".to_vec(),
        ws: "w\u{e5}\u{4e2d}".chars().map(u32::from).collect(),
        vec: [1.5, -2.0, 3.0],
        col: [0.25, 0.5, 0.75, 1.0],
        arr: [7, 8, 9],
        inner: Inner {
            cls: 0,
            a: 5,
            b: 0.5,
        },
        pinner: &raw mut pinner,
    };
    let mut doc = Doc::new();
    unsafe { Walker::new(&Names) }.save_document(&mut doc, (&raw mut p).cast(), b"Probe");
    let golden = include_bytes!("../../tests/golden/H1/ser_probe.xml");
    assert_eq!(
        String::from_utf8_lossy(&doc.print_file()),
        String::from_utf8_lossy(golden)
    );
}
