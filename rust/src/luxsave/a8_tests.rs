use super::a8_table::{find, render};
use super::a8_tables::CLASSES;
use crate::system::serialize::{M_ARRAY, M_CONTAINER, T_CLASS, T_CLASS_PTR};

const GOLDEN: &str = include_str!("../../tests/golden/A8/tables.txt");

#[test]
fn tables_match_golden() {
    let all: String = CLASSES.iter().map(render).collect();
    assert_eq!(all, GOLDEN);
    assert_eq!(CLASSES.len(), 115);
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

#[test]
fn ladder_leave_at_top_count_is_not_serialized() {
    let c = find(b"cLuxPlayerState_Ladder_SaveData").unwrap();
    assert!(
        c.fields
            .iter()
            .all(|f| f.name.to_bytes() != b"mfLeaveAtTopCount")
    );
}

#[test]
fn every_mirrored_class_has_a_layout_table() {
    for c in super::layout::CLASSES {
        let t = super::a8_table::layout_table(c.as_bytes());
        assert!(t.is_some(), "{c}");
        assert!(super::layout::size(c.as_bytes()).is_some());
    }
}

#[test]
fn ar_layouts_have_tables() {
    for c in super::layout_ar::CLASSES {
        assert!(super::a8_table::layout_table(c.as_bytes()).is_some(), "{c}");
    }
}

#[test]
fn ar_layouts_match_gcc() {
    let mut n = 0;
    for l in include_str!("layout_ar_gcc.txt").lines() {
        let w: Vec<&str> = l.split(' ').collect();
        let (c, num) = (w[1].as_bytes(), |s: &str| s.parse::<usize>().unwrap());
        if w[0] == "C" {
            assert_eq!(super::layout_ar::size(c), Some(num(w[2])), "{}", w[1]);
        } else {
            let got = super::layout_ar::field(c, w[2].as_bytes());
            assert_eq!(got, Some((num(w[3]), num(w[4]))), "{} {}", w[1], w[2]);
        }
        n += 1;
    }
    assert_eq!(n, 612);
}

#[test]
fn gcc_layouts_have_tables_of_matching_length() {
    let mut n = 0;
    for c in super::layout_gcc::classes() {
        let t = super::a8_table::layout_table(c.as_bytes()).unwrap_or_else(|| panic!("{c}"));
        assert_eq!(t.len(), find(c.as_bytes()).unwrap().fields.len() + 1, "{c}");
        let size = super::layout_gcc::size(c.as_bytes()).unwrap();
        assert!(t.iter().all(|f| f.offset + f.size <= size), "{c}");
        n += 1;
    }
    assert_eq!(n, 44);
}
