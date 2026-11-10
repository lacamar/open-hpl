use super::headless::{LineSplitter, json_escape as hc_escape, parse_flat_json, response_json};
use super::logic_timer::LogicTimer;
use super::serialize::*;
use super::{frame_stats_json, gamma_lut, json_escape, num, pixel_stats_json};
use crate::xml::Doc;
use std::cell::RefCell;
use std::mem::offset_of;
use std::sync::Arc;

// ---- headless protocol ----

fn parsed(line: &str) -> Vec<(String, String)> {
    parse_flat_json(line.as_bytes())
        .into_iter()
        .map(|(k, v)| {
            (
                String::from_utf8_lossy(&k).into_owned(),
                String::from_utf8_lossy(&v).into_owned(),
            )
        })
        .collect()
}

fn kv(k: &str, v: &str) -> (String, String) {
    (k.to_owned(), v.to_owned())
}

#[test]
fn parses_flat_objects() {
    let cases: [(&str, Vec<(String, String)>); 12] = [
        (r#"{"cmd":"ping"}"#, vec![kv("cmd", "ping")]),
        (
            "  \t{ \"cmd\" : \"input\" , \"n\" : 12 , \"b\":true }",
            vec![kv("b", "true"), kv("cmd", "input"), kv("n", "12")],
        ),
        (
            r#"{"a":"x\ny\t\"q\"\\ \z"}"#,
            vec![kv("a", "x\ny\t\"q\"\\ z")],
        ),
        (r#"{"a":1,"a":2}"#, vec![kv("a", "2")]),
        ("", vec![]),
        ("cmd", vec![]),
        (r#"{"a":1 "b":2}"#, vec![kv("a", "1")]),
        (r#"{"a":"unterminated"#, vec![kv("a", "unterminated")]),
        (r#"{"a"}"#, vec![]),
        (r#"{a:1}"#, vec![]),
        (r#"{"a":1,}"#, vec![kv("a", "1")]),
        (r#"{"a":"tail\"#, vec![kv("a", "tail\\")]),
    ];
    for (line, want) in cases {
        assert_eq!(parsed(line), want, "line: {line:?}");
    }
}

#[test]
fn bare_tokens_stop_at_separators() {
    assert_eq!(
        parsed(r#"{"x":-1.5e3,"y":null}"#),
        vec![kv("x", "-1.5e3"), kv("y", "null")]
    );
    assert_eq!(parsed("{\"x\":\u{b}7\u{c}}"), vec![kv("x", "7")]);
}

#[test]
fn response_json_shapes() {
    let fields: [(&[u8], &[u8]); 2] = [(b"frames", b"3"), (b"timed_out", b"false")];
    assert_eq!(
        response_json(true, b"ignored", &fields),
        br#"{"ok":true,"frames":3,"timed_out":false}"#
    );
    assert_eq!(
        response_json(false, b"bad \"x\"\n\x01", &[]),
        br#"{"ok":false,"error":"bad \"x\"\n"}"#
    );
    assert_eq!(response_json(false, b"", &[]), br#"{"ok":false}"#);
    assert_eq!(hc_escape(b"a\tb\r\\\x1f"), br"a\tb\r\\");
}

#[test]
fn splitter_handles_partial_and_empty_lines() {
    let mut s = LineSplitter::default();
    assert!(s.push(b"{\"a\"").is_empty());
    assert_eq!(
        s.push(b":1}\n\n{\"b\":2}\n{\"c\""),
        vec![b"{\"a\":1}".to_vec(), b"{\"b\":2}".to_vec()]
    );
    assert_eq!(s.push(b":3}\n"), vec![b"{\"c\":3}".to_vec()]);
    assert!(s.push(b"\n\n").is_empty());
}

// ---- diagnostics ----

#[test]
fn diag_json_helpers() {
    assert_eq!(json_escape(b"a\"b\\c\nd\te\x01"), br#"a\"b\\c\nd e "#);
    assert_eq!(num(1.5), b"1.5");
    assert_eq!(num(2.0), b"2");
    assert_eq!(num(f32::NAN), b"null");
    assert_eq!(num(f32::NEG_INFINITY), b"null");
    assert_eq!(num(0.1234567), b"0.123457");
}

#[test]
fn pixel_stats_literal() {
    let px = [0.0, 1.0, f32::NAN, 2.0, 4.0, 1.0, f32::NAN, 2.0];
    let want = concat!(
        r#"{"width":2,"height":1,"#,
        r#""r":{"min":0,"max":4,"mean":2,"nan":0,"zero":1},"#,
        r#""g":{"min":1,"max":1,"mean":1,"nan":0,"zero":0},"#,
        r#""b":{"min":0,"max":0,"mean":0,"nan":2,"zero":0},"#,
        r#""a":{"min":2,"max":2,"mean":2,"nan":0,"zero":0}}"#
    );
    assert_eq!(pixel_stats_json(&px, 2, 1), want.as_bytes());
}

#[test]
fn frame_stats_literal() {
    let px = [0, 0, 0, 255, 255, 255, 255, 0, 255, 128, 128, 128];
    let want = concat!(
        r#"{"width":4,"height":1,"lum_mean":113.75,"lum_min":0,"lum_max":255,"#,
        r#""black_frac":0.25,"white_frac":0.25,"magenta_frac":0.25,"#,
        r#""lum_hist16":[0.25,0,0,0,0.25,0,0,0,0.25,0,0,0,0,0,0,0.25]}"#
    );
    let got = frame_stats_json(&px, 4, 1, 3);
    assert_eq!(String::from_utf8_lossy(&got), want);
    assert_eq!(
        frame_stats_json(&[], 0, 0, 3),
        br#"{"width":0,"height":0,"lum_mean":0,"lum_min":255,"lum_max":0,"black_frac":0,"white_frac":0,"magenta_frac":0,"lum_hist16":[0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]}"#
    );
}

#[test]
fn gamma_lut_endpoints() {
    let lut = gamma_lut(2.0);
    assert_eq!((lut[0], lut[255]), (0, 255));
    assert_eq!(lut[64], 128);
}

// ---- logic timer (fake clock) ----

#[test]
fn logic_timer_steps_with_the_clock() {
    let mut t = LogicTimer::new(20, 1000);
    assert_eq!((t.max_updates, t.updates_per_sec()), (2, 20));
    assert_eq!(t.step_size(), 0.05);
    assert!(!t.want_update(1000), "not behind yet");
    t.end_update_loop(1000);
    assert!(t.want_update(1001));
    assert_eq!(t.local_time, 1050.0);
    assert!(!t.want_update(1001));
    t.end_update_loop(1001);
    // far behind: two catch-up updates, then the cap resets the clock
    assert!(t.want_update(2000));
    assert!(t.want_update(2000));
    assert!(!t.want_update(2000));
    t.end_update_loop(2000);
    assert_eq!(t.local_time, 2000.0);
    t.speed_mul = 2.0;
    assert!(t.want_update(2001));
    assert_eq!(t.local_time, 2025.0);
    t.set_updates_per_sec(40, 3000);
    assert_eq!((t.updates_per_sec(), t.local_time), (40, 3000.0));
}

// ---- serialization over a fake object model ----

const NAMES: [&str; 4] = ["UtLeaf", "UtRec", "UtChild", "UtEmptyKid"];

#[repr(C)]
struct Leaf {
    cls: usize,
    a: i32,
}

#[repr(C)]
struct Rec {
    cls: usize,
    flag: u8,
    n: i32,
    f: f32,
    v: [f32; 3],
    c: [i32; 4],
    name: Vec<u8>,
    ints: [i32; 3],
    leaf: Leaf,
    pleaf: *mut Leaf,
    list: Vec<i32>,
}

#[repr(C)]
struct Child {
    rec: Rec,
    extra: i32,
}

#[derive(Default)]
struct Fake {
    logs: RefCell<Vec<String>>,
}

impl Host for Fake {
    fn log(&self, level: Level, msg: &[u8]) {
        self.logs
            .borrow_mut()
            .push(format!("{level:?}:{}", String::from_utf8_lossy(msg)));
    }
    fn top_class(&self, obj: Obj) -> Vec<u8> {
        // SAFETY: every test object starts with its class index.
        let i = unsafe { *obj.cast::<usize>() };
        NAMES.get(i).map_or(Vec::new(), |n| n.as_bytes().to_vec())
    }
    fn destroy(&self, obj: Obj) {
        // SAFETY: only `Leaf`s made by `create_leaf` are destroyed.
        drop(unsafe { Box::from_raw(obj.cast::<Leaf>()) });
    }
    fn tmp_new(&self, _: u32, size: usize) -> Obj {
        Box::into_raw(vec![0_u8; size.max(8)].into_boxed_slice()).cast()
    }
    fn tmp_free(&self, _: u32, _: Obj) {}
    fn str_get(&self, p: Obj) -> Vec<u8> {
        // SAFETY: string fields are `Vec<u8>`.
        unsafe { (*p.cast::<Vec<u8>>()).clone() }
    }
    fn str_set(&self, p: Obj, v: &[u8]) {
        // SAFETY: string fields are `Vec<u8>`.
        unsafe { *p.cast::<Vec<u8>>() = v.to_vec() }
    }
    fn wstr_get(&self, _: Obj) -> Vec<u32> {
        Vec::new()
    }
    fn wstr_set(&self, _: Obj, _: &[u32]) {}
    fn string_size(&self) -> usize {
        size_of::<Vec<u8>>()
    }
    fn wstring_size(&self) -> usize {
        size_of::<Vec<u32>>()
    }
    fn cont_size(&self, cont: Obj) -> usize {
        // SAFETY: containers are `Vec<i32>`.
        unsafe { (*cont.cast::<Vec<i32>>()).len() }
    }
    fn cont_clear(&self, cont: Obj) {
        // SAFETY: containers are `Vec<i32>`.
        unsafe { (*cont.cast::<Vec<i32>>()).clear() }
    }
    fn cont_add_ptr(&self, _: Obj, _: Obj) {}
    fn cont_add_class(&self, cont: Obj, elem: Obj) {
        // SAFETY: containers are `Vec<i32>` and `elem` holds an `i32`.
        unsafe { (*cont.cast::<Vec<i32>>()).push(*elem.cast::<i32>()) }
    }
    fn cont_iter(&self, cont: Obj) -> Obj {
        Box::into_raw(Box::new((cont, 0_usize))).cast()
    }
    fn iter_next(&self, it: Obj) -> Option<Obj> {
        // SAFETY: `it` is the `(container, index)` pair made by `cont_iter`.
        unsafe {
            let (cont, i) = &mut *it.cast::<(Obj, usize)>();
            let v = &mut *cont.cast::<Vec<i32>>();
            let p = v.get_mut(*i)?;
            *i += 1;
            Some((p as *mut i32).cast())
        }
    }
    fn iter_free(&self, it: Obj) {
        // SAFETY: made by `cont_iter`.
        drop(unsafe { Box::from_raw(it.cast::<(Obj, usize)>()) });
    }
}

unsafe extern "C" fn create_leaf() -> Obj {
    Box::into_raw(Box::new(Leaf { cls: 0, a: 0 })).cast()
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

fn register_all() {
    use std::sync::Once;
    static ONCE: Once = Once::new();
    ONCE.call_once(|| {
        let class = |name: &str, parent: &str, fields: Vec<Arc<Field>>, create| Class {
            name: name.as_bytes().to_vec(),
            parent: parent.as_bytes().to_vec(),
            fields,
            size: 0,
            create,
        };
        register(class(
            "UtLeaf",
            "",
            vec![field("a", offset_of!(Leaf, a), T_INT32, M_VAR, 0)],
            Some(create_leaf),
        ));
        register(class(
            "UtRec",
            "",
            vec![
                field("flag", offset_of!(Rec, flag), T_BOOL, M_VAR, 0),
                field("n", offset_of!(Rec, n), T_INT32, M_VAR, 0),
                field("f", offset_of!(Rec, f), T_FLOAT32, M_VAR, 0),
                field("v", offset_of!(Rec, v), T_VEC3F, M_VAR, 0),
                field("c", offset_of!(Rec, c), T_RECT2L, M_VAR, 0),
                field("name", offset_of!(Rec, name), T_STRING, M_VAR, 0),
                field("ints", offset_of!(Rec, ints), T_INT32, M_ARRAY, 3),
                field("leaf", offset_of!(Rec, leaf), T_CLASS, M_VAR, 0),
                field("pleaf", offset_of!(Rec, pleaf), T_CLASS_PTR, M_VAR, 0),
                field("list", offset_of!(Rec, list), T_INT32, M_CONTAINER, 0),
            ],
            None,
        ));
        register(class(
            "UtChild",
            "UtRec",
            vec![field("extra", size_of::<Rec>(), T_INT32, M_VAR, 0)],
            None,
        ));
        register(class("UtEmptyKid", "UtRec", Vec::new(), None));
    });
}

fn rec(cls: usize) -> Rec {
    Rec {
        cls,
        flag: 1,
        n: -7,
        f: 0.5,
        v: [1.0, 2.5, -0.0],
        c: [1, 2, 3, 4],
        name: b"a<b>&\"".to_vec(),
        ints: [10, 20, 30],
        leaf: Leaf { cls: 0, a: 99 },
        pleaf: std::ptr::null_mut(),
        list: vec![5, 6],
    }
}

fn saved(h: &Fake, obj: Obj) -> String {
    let mut doc = Doc::new();
    // SAFETY: the test objects match the registered layouts.
    unsafe { Walker::new(h) }.save_document(&mut doc, obj, b"SaveGame");
    String::from_utf8_lossy(&doc.print_file()).into_owned()
}

#[test]
fn saves_every_field_shape_in_table_order() {
    register_all();
    let h = Fake::default();
    let mut r = rec(1);
    let xml = saved(&h, (&raw mut r).cast());
    let want = concat!(
        "<SaveGame>\n",
        "    <class type=\"UtRec\" name=\"\">\n",
        "        <var type=\"1\" name=\"flag\" val=\"true\" />\n",
        "        <var type=\"2\" name=\"n\" val=\"-7\" />\n",
        "        <var type=\"3\" name=\"f\" val=\"0.500000\" />\n",
        "        <var type=\"8\" name=\"v\" val=\"1.000000 2.500000 -0.000000\" />\n",
        "        <var type=\"11\" name=\"c\" val=\"1 2 3 4\" />\n",
        "        <var type=\"4\" name=\"name\" val='a&lt;b&gt;&amp;&quot;' />\n",
        "        <array type=\"2\" name=\"ints\" size=\"3\">\n",
        "            <var val=\"10\" />\n",
        "            <var val=\"20\" />\n",
        "            <var val=\"30\" />\n",
        "        </array>\n",
        "        <class type=\"UtLeaf\" name=\"leaf\">\n",
        "            <var type=\"2\" name=\"a\" val=\"99\" />\n",
        "        </class>\n",
        "        <container type=\"2\" name=\"list\">\n",
        "            <var val=\"5\" />\n",
        "            <var val=\"6\" />\n",
        "        </container>\n",
        "    </class>\n",
        "</SaveGame>\n"
    );
    assert_eq!(xml, want);
    assert!(h.logs.borrow().is_empty());
}

#[test]
fn round_trips_through_the_document() {
    register_all();
    let h = Fake::default();
    let mut a = rec(1);
    a.pleaf = Box::into_raw(Box::new(Leaf { cls: 0, a: 4 }));
    let mut doc = Doc::new();
    // SAFETY: the test objects match the registered layouts.
    let w = unsafe { Walker::new(&h) };
    w.save_document(&mut doc, (&raw mut a).cast(), b"SaveGame");
    let mut b = rec(1);
    b.flag = 0;
    b.n = 0;
    b.name.clear();
    b.list = vec![1, 2, 3];
    b.leaf.a = 0;
    b.ints = [0; 3];
    w.load_document(&doc, (&raw mut b).cast());
    assert_eq!(
        (b.flag, b.n, b.f, b.v, b.c),
        (1, -7, 0.5, [1.0, 2.5, 0.0], [1, 2, 3, 4])
    );
    assert_eq!(
        (b.name.as_slice(), b.ints, b.leaf.a),
        (&b"a<b>&\""[..], [10, 20, 30], 99)
    );
    assert_eq!(b.list, vec![5, 6]);
    assert!(!b.pleaf.is_null());
    // SAFETY: created by `create_leaf`, freed once.
    unsafe {
        assert_eq!((*b.pleaf).a, 4);
        drop(Box::from_raw(b.pleaf));
    }
    // SAFETY: allocated above, freed once.
    drop(unsafe { Box::from_raw(a.pleaf) });
}

#[test]
fn class_pointer_field_is_created_when_null_and_reused_otherwise() {
    register_all();
    let h = Fake::default();
    let xml = "<SaveGame><class type=\"UtRec\" name=\"\"><class_ptr type=\"UtLeaf\" name=\"pleaf\">\
               <var type=\"2\" name=\"a\" val=\"41\"/></class_ptr></class></SaveGame>";
    let mut doc = Doc::new();
    doc.parse(xml.as_bytes());
    // SAFETY: the test objects match the registered layouts.
    let w = unsafe { Walker::new(&h) };
    let mut b = rec(1);
    w.load_document(&doc, (&raw mut b).cast());
    assert!(!b.pleaf.is_null());
    // SAFETY: created by `create_leaf`.
    assert_eq!(unsafe { (*b.pleaf).a }, 41);
    let first = b.pleaf;
    w.load_document(&doc, (&raw mut b).cast());
    assert_eq!(b.pleaf, first);
    // SAFETY: created by `create_leaf`, freed once.
    drop(unsafe { Box::from_raw(b.pleaf) });

    let null = "<SaveGame><class type=\"UtRec\" name=\"\"><class_ptr type=\"UtLeaf\" name=\"pleaf\" null=\"true\"/></class></SaveGame>";
    doc.clear();
    doc.parse(null.as_bytes());
    w.load_document(&doc, (&raw mut b).cast());
    assert!(b.pleaf.is_null());
}

#[test]
fn child_classes_save_own_fields_then_parent_and_empty_ones_start_at_the_parent() {
    register_all();
    let h = Fake::default();
    let mut c = Child {
        rec: rec(2),
        extra: 3,
    };
    let xml = saved(&h, (&raw mut c).cast());
    let extra = xml.find("name=\"extra\"").unwrap_or(usize::MAX);
    let flag = xml.find("name=\"flag\"").unwrap_or(0);
    assert!(extra < flag, "own fields come first:\n{xml}");
    let mut e = rec(3);
    let xml = saved(&h, (&raw mut e).cast());
    assert!(
        xml.contains("type=\"UtEmptyKid\"") && xml.contains("name=\"flag\""),
        "{xml}"
    );
}

#[test]
fn loading_skips_unknown_fields_and_classes_with_a_warning() {
    register_all();
    let h = Fake::default();
    let xml = "<SaveGame><class type=\"UtRec\" name=\"\"><var type=\"2\" name=\"nope\" val=\"1\"/>\
               <var type=\"2\" name=\"n\" val=\" 12abc\"/><var type=\"1\" name=\"flag\" val=\"TRUE\"/>\
               <var type=\"8\" name=\"v\" val=\"1,2\"/><var name=\"f\"/></class></SaveGame>";
    let mut doc = Doc::new();
    doc.parse(xml.as_bytes());
    let mut b = rec(1);
    b.flag = 0;
    // SAFETY: the test objects match the registered layouts.
    unsafe { Walker::new(&h) }.load_document(&doc, (&raw mut b).cast());
    assert_eq!((b.n, b.flag, b.f), (12, 1, 0.5));
    assert_eq!(b.v, [1.0, 2.0, 0.0], "missing components read as 0");
    assert_eq!(
        h.logs.borrow().as_slice(),
        ["Warning:Couldn't find member field 'nope' in class 'UtRec'\n"]
    );

    let mut unknown = rec(42);
    let h = Fake::default();
    let xml = saved(&h, (&raw mut unknown).cast());
    assert!(xml.contains("type=\"\""), "{xml}");
    assert_eq!(h.logs.borrow().len(), 1);
}

#[test]
fn null_object_saves_only_the_root() {
    register_all();
    let h = Fake::default();
    assert_eq!(saved(&h, std::ptr::null_mut()), "<SaveGame />\n");
}
