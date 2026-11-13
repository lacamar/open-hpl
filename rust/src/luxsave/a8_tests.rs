use super::a8_table::{find, refine, render};
use super::a8_tables::CLASSES;
use crate::system::ffi::OhplSerField;
use crate::system::serialize::{M_ARRAY, M_CONTAINER, T_CLASS, T_CLASS_PTR, T_NULL};

const GOLDEN: &str = include_str!("../../tests/golden/A8/tables.txt");

#[test]
fn tables_match_golden() {
    let all: String = CLASSES.iter().map(render).collect();
    assert_eq!(all, GOLDEN);
    assert_eq!(CLASSES.len(), 112);
}

#[test]
fn table_invariants() {
    assert!(
        CLASSES
            .windows(2)
            .all(|w| w[0].name.to_bytes() < w[1].name.to_bytes())
    );
    for c in CLASSES {
        for f in c.fields {
            let is_class = f.ty == T_CLASS || f.ty == T_CLASS_PTR;
            assert!(
                f.class.is_empty() || (is_class && f.main == M_CONTAINER),
                "{:?}.{:?}",
                c.name,
                f.name
            );
            assert!(f.main != M_ARRAY || f.array > 0);
        }
    }
}

fn cpp_table(name: &str) -> Vec<OhplSerField> {
    let c = find(name.as_bytes()).unwrap();
    let mut v: Vec<OhplSerField> = c
        .fields
        .iter()
        .enumerate()
        .map(|(i, f)| OhplSerField {
            name: f.name.as_ptr(),
            class_name: f.class.as_ptr(),
            offset: 8 + i * 4,
            size: 4,
            ty: f.ty,
            main: f.main,
            array_size: f.array,
        })
        .collect();
    v.push(OhplSerField {
        name: c"".as_ptr(),
        class_name: c"".as_ptr(),
        offset: 0,
        size: 0,
        ty: T_NULL,
        main: 0,
        array_size: 0,
    });
    v
}

#[test]
fn refine_keeps_cpp_layout() {
    let c = find(b"cLuxPlayerState_Ladder_SaveData").unwrap();
    let cpp = cpp_table("cLuxPlayerState_Ladder_SaveData");
    let want: Vec<(usize, u32)> = cpp.iter().map(|f| (f.offset, f.ty)).collect();
    let out = unsafe { refine(c.name.as_ptr(), c.parent.as_ptr(), cpp) };
    let got: Vec<(usize, u32)> = out.iter().map(|f| (f.offset, f.ty)).collect();
    assert_eq!(got, want);
    assert_eq!(out.len(), c.fields.len() + 1);
    assert_eq!(out[0].name, c.fields[0].name.as_ptr());
}

#[test]
fn refine_falls_back_on_mismatch() {
    let c = find(b"cLuxPlayerState_Ladder_SaveData").unwrap();
    let mut cpp = cpp_table("cLuxPlayerState_Ladder_SaveData");
    cpp[1].ty += 1;
    let ptr = cpp[0].name;
    let out = unsafe { refine(c.name.as_ptr(), c.parent.as_ptr(), cpp) };
    assert_eq!(out[0].name, ptr);
    assert_ne!(out[1].ty, c.fields[1].ty);
    let out = unsafe {
        refine(
            c"cUnknown".as_ptr(),
            c"".as_ptr(),
            cpp_table("cLuxSavedMap"),
        )
    };
    assert_eq!(out.len(), find(b"cLuxSavedMap").unwrap().fields.len() + 1);
}

#[test]
fn ladder_leave_at_top_count_is_not_serialized() {
    let c = find(b"cLuxPlayerState_Ladder_SaveData").unwrap();
    assert!(
        c.fields
            .iter()
            .all(|f| f.name.to_bytes() != b"mfLeaveAtTopCount")
    );
}
