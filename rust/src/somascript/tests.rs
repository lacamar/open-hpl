use super::api::{self, Enum};
use super::builder::{self, Builder};
use super::check;
use super::globals::{self, Globals};
use super::math;
use super::runtime;
use super::stream::Stream;
use super::strings;

fn b(s: &str) -> Vec<u8> {
    s.as_bytes().to_vec()
}

#[test]
fn find_matches_std_string() {
    assert_eq!(strings::find(b"abcabc", b"", 6), Some(6));
    assert_eq!(strings::find(b"abcabc", b"", 7), None);
    assert_eq!(strings::find(b"abcabc", b"bc", 2), Some(4));
    assert_eq!(strings::rfind(b"abcabc", b"bc"), Some(4));
    assert_eq!(strings::rfind(b"abc", b""), Some(3));
    assert_eq!(strings::find_char(b"a\xffb", 0xff, false), 1);
    assert_eq!(strings::count_chars(b"aaaa", b"aa"), 2);
    assert_eq!(strings::count_chars(b"aaaa", b""), 0);
}

#[test]
fn float_text_formats() {
    assert_eq!(strings::float_to_string(1.5, -1, false), b("1.5"));
    assert_eq!(strings::float_to_string(1.5, 3, true), b("1.5"));
    assert_eq!(strings::float_to_string(2.0, 3, true), b("2"));
    assert_eq!(strings::float_to_string(f32::NAN, 2, false), b("nan"));
    assert_eq!(strings::float_to_string(1.5e20, -1, true), b("1.5e+2"));
    assert_eq!(strings::int_padded(7, 3), b("007"));
    assert_eq!(
        strings::seconds_to_clock(3725.5, true, true, true, true),
        b("01:02:05:50")
    );
    assert_eq!(
        strings::seconds_to_clock(-5.0, false, false, true, false),
        b("0-5")
    );
}

#[test]
fn split_and_suffix() {
    let t = strings::split(b"a, b\tc,,d", b"");
    assert_eq!(t, vec![b"a".as_slice(), b"b", b"c", b"d"]);
    let t = strings::split(b"a--b-", b"-");
    assert_eq!(t, vec![b"a".as_slice(), b"b"]);
    assert_eq!(strings::numeric_suffix(b"door12"), b"12");
    assert_eq!(strings::numeric_suffix(b"123"), b"123");
    assert_eq!(strings::numeric_suffix_int(b"x", 5), 5);
    assert_eq!(strings::numeric_suffix_int(b"x42", 5), 42);
    let w: Vec<u32> = "ab99999999999999999999".chars().map(u32::from).collect();
    assert_eq!(strings::numeric_suffix_int_wide(&w, 0), -1);
    assert_eq!(strings::hash(b"abc"), 193485963);
}

#[test]
fn wide_widening_is_unsigned() {
    assert_eq!(strings::widen(b"a\xff"), vec![97, 0xff]);
    let s: Vec<u32> = "aXbX".chars().map(u32::from).collect();
    let r: Vec<u32> = "XX".chars().map(u32::from).collect();
    let n: Vec<u32> = "XYX".chars().map(u32::from).collect();
    assert_eq!(strings::replace_all_wide(&s, &r[..1], &n), {
        let t: Vec<u32> = "aXYXbXYX".chars().map(u32::from).collect();
        t
    });
}

#[test]
fn tostr_forms() {
    assert_eq!(
        strings::tostr(strings::TOSTR_FLOATS, &[1.0, 2.5], &[]),
        b("(1.000000 : 2.500000)")
    );
    assert_eq!(
        strings::tostr(strings::TOSTR_RECT_L, &[], &[1, 2, 3, 4]),
        b("((1 : 2)(3 : 4))")
    );
    assert_eq!(
        strings::tostr(strings::TOSTR_ID, &[], &[1, -2, 3]),
        b("(1 -2 3)")
    );
    let m: Vec<f32> = (0..16).map(|i| i as f32).collect();
    let s = strings::tostr(strings::TOSTR_MATRIX, &m, &[]);
    assert!(s.starts_with(b"([0.000000 : 1.000000 : 2.000000 : 3.000000] [4.000000"));
    assert_eq!(strings::tostr(strings::TOSTR_BOOL, &[], &[0]), b("false"));
    assert_eq!(
        strings::tostr(strings::TOSTR_UINT, &[], &[-1]),
        b("4294967295")
    );
}

#[test]
fn math_values() {
    assert_eq!(math::sigmoid_curve(0.5), 0.5);
    assert_eq!(math::interpolate_linear(1.0, 3.0, 0.5), 2.0);
    assert_eq!(math::get_bit(33), 2);
    assert_eq!(math::set_bit_flag(0b101, 0b010, true), 0b111);
    assert!(!math::get_bit_flag(0b101, 0b010));
    assert_eq!(math::vector3_max_element([1.0, f32::NAN, 0.5]), 1.0);
    assert_eq!(math::rgb_to_hex([1.0, 0.0, 0.5]), *b"FF0080");
    assert_eq!(math::catmull_rom([0.0], [1.0], [2.0], [3.0], 0.5), [1.5]);
    assert!(math::aabb_sphere_collision(
        [0.0; 3],
        [1.0; 3],
        [2.0, 0.5, 0.5],
        1.5
    ));
    assert!(!math::aabb_sphere_collision(
        [0.0; 3],
        [1.0; 3],
        [5.0, 0.5, 0.5],
        1.0
    ));
}

#[test]
fn pid_integrates_history() {
    let mut p = Box::new(unsafe { std::mem::zeroed::<math::Pid<1>>() });
    p.p = 1.0;
    p.i = 1.0;
    p.d = 0.0;
    p.set_error_num(3);
    let a = p.output([2.0], 0.5);
    assert_eq!(a, [3.0]);
    let b2 = p.output([2.0], 0.5);
    assert_eq!(b2, [4.0]);
    assert_eq!(p.last_integral, [2.0]);
    p.set_error_num(1000);
    assert_eq!(p.error_num, 64);
    let mut q = Box::new(unsafe { std::mem::zeroed::<math::Pid<3>>() });
    assert_eq!(q.output([1.0, 2.0, 3.0], 1.0), [0.0; 3]);
    assert_eq!(q.error_num, 10);
}

#[test]
fn class_and_attribute_scan() {
    let mut code = b("shared class Foo\n{\n  [nosave] int mA;\n [volatile]  float mB = 1;\n}\n#include \"x/y.hps\"\nint z;\r\n").to_vec();
    let before = code.clone();
    let inc = builder::preprocess(&mut code);
    assert_eq!(inc, vec![b("x/y.hps")]);
    assert_eq!(code.len(), before.len());
    assert!(!code.windows(8).any(|w| w == b"#include"));
    assert!(!code.windows(8).any(|w| w == b"[nosave]"));
    assert_eq!(code.iter().filter(|&&c| c == b'\n').count(), 7);
    assert!(builder::is_nosave(b"Foo::mA"));
    assert!(builder::is_nosave(b"Foo::mB"));
    assert!(!builder::is_nosave(b"Foo::mC"));
}

#[test]
fn includes_resolve_in_order() {
    let dir = std::env::temp_dir().join(format!("gs6-builder-{}", std::process::id()));
    let script = dir.join("script");
    std::fs::create_dir_all(script.join("sub")).unwrap();
    std::fs::write(
        script.join("main.hps"),
        "#include \"sub/Inc.hps\"\n#include \"missing.hps\"\n#include \"inc2.hps\"\nvoid f(){}\n",
    )
    .unwrap();
    std::fs::write(
        script.join("sub/inc.hps"),
        "#include \"inc2.hps\"\nint a;\n",
    )
    .unwrap();
    std::fs::write(script.join("inc2.hps"), "int b;\n").unwrap();
    let dir_s = dir.to_str().unwrap().as_bytes().to_vec();
    let bld = Builder::new(&dir_s, b"soma");
    let main = [dir_s.as_slice(), b"/script/main.hps"].concat();
    let c = bld.collect(&main).unwrap();
    let names: Vec<_> = c
        .sections
        .iter()
        .map(|(n, _)| String::from_utf8_lossy(n).into_owned())
        .collect();
    assert_eq!(names.len(), 3);
    assert!(names[1].ends_with("script/sub/inc.hps"));
    assert!(names[2].ends_with("script/inc2.hps"));
    assert!(String::from_utf8_lossy(&c.missing).starts_with("missing.hps (from "));
    assert_eq!(
        bld.resolve(b"INC2.HPS", b"nowhere"),
        [dir_s.as_slice(), b"/script/inc2.hps"].concat()
    );
    assert!(bld.collect(b"/nonexistent/file.hps").is_none());
    std::fs::remove_dir_all(&dir).unwrap();
}

#[test]
fn compat_patches_follow_game() {
    let mut warnings = Vec::new();
    let mut code = b("x .length- y").to_vec();
    builder::apply_patches(
        &mut code,
        b"/g/script/03_02_omicron_inside.hps",
        b"soma",
        &mut warnings,
    );
    assert_eq!(code, b("x .length()- y"));
    assert_eq!(warnings.len(), 1);
}

#[test]
fn cache_key_depends_on_sections() {
    let s1 = vec![(b("a"), b("x"))];
    let s2 = vec![(b("a"), b("y"))];
    let k = builder::cache_key(1, 2, 3, &s1);
    assert_eq!(k.len(), 16);
    assert_ne!(k, builder::cache_key(1, 2, 3, &s2));
    assert_ne!(k, builder::cache_key(2, 2, 3, &s1));
}

#[test]
fn api_declaration_helpers() {
    assert_eq!(
        api::out_defaults_to_void(b"void f(int &out a = foo(1,2), int b)"),
        b("void f(int &out a = void, int b)")
    );
    assert_eq!(api::no_auto_handles(b"cX@+ Get()"), b("cX@ Get()"));
    let sig = api::parse_decl(b"const tString &Get(int a, const cVector3f &in v = 1) const");
    assert_eq!(sig.name, b("Get"));
    assert_eq!(sig.tail, b("const"));
    assert_eq!(sig.types.len(), 3);
    assert_eq!(sig.defaults, vec![false, false, true]);
    assert_eq!(api::decl_name(b"void cLux_Do(int)"), b("cLux_Do"));
}

#[test]
fn native_decls_bind_to_compatible_api_entries() {
    let no_enum = |_: &[u8]| false;
    let is_enum = |s: &[u8]| s == b"eKind";
    let cands: Vec<&[u8]> = vec![b"void F(eKind a)"];
    assert_eq!(
        api::native_decl_pick(&cands, &no_enum, b"void F(int a)"),
        None
    );
    assert_eq!(
        api::native_decl_pick(&cands, &is_enum, b"void F(int a)"),
        Some(0)
    );
    let longer: Vec<&[u8]> = vec![b"void F(int a, int b = 2)"];
    assert_eq!(
        api::native_decl_pick(&longer, &no_enum, b"void F(int a)"),
        Some(0)
    );
    let same: Vec<&[u8]> = vec![b"void F(int a)", b"void F(float a)"];
    assert_eq!(
        api::native_decl_pick(&same, &no_enum, b"void F(int a)"),
        None
    );
    let void_ret: Vec<&[u8]> = vec![b"void F(int a)"];
    assert_eq!(
        api::native_decl_pick(&void_ret, &no_enum, b"bool F(int a)"),
        Some(0)
    );
}

#[test]
fn enums_rebase_onto_base_values() {
    let e = |n: &str, v: &[(&str, i32)]| Enum {
        name: b(n),
        values: v.iter().map(|(a, c)| (b(a), *c)).collect(),
    };
    let base = vec![e("eA", &[("x", 0), ("y", 1), ("eA_LastEnum", 2)])];
    let mut game = vec![e(
        "eA",
        &[("x", 0), ("new", 1), ("y", 2), ("eA_LastEnum", 3)],
    )];
    api::rebase_enums(&mut game, &base);
    let v: Vec<i32> = game[0].values.iter().map(|(_, v)| *v).collect();
    assert_eq!(v, vec![0, 2, 1, 3]);
}

#[test]
fn api_text_parses_into_records() {
    let text = b"# c\nE\teA\nV\tx\t3\nT\tcVector2f\tvalue\t0\nO\tcVector2f\nM\tvoid f(int &out a = 1)\nP\tfloat x\t4\nGP\tint g\nG\tvoid h()\t\n";
    let mut kinds = Vec::new();
    api::load(text, |r| kinds.push((r.kind, r.type_idx, r.a.clone(), r.n)));
    assert_eq!(kinds[0].0, api::REC_ENUM);
    assert_eq!(kinds[1], (api::REC_ENUM_VALUE, 0, b("x"), 3));
    assert_eq!(kinds[2], (api::REC_TYPE, 0, b("cVector2f"), 8));
    assert_eq!(
        kinds[3],
        (api::REC_METHOD, 0, b("void f(int &out a = void)"), 0)
    );
    assert_eq!(kinds[4], (api::REC_PROP, 0, b("float x"), 4));
    assert_eq!(kinds[6], (api::REC_GLOBAL, 0, b("void h()"), 0));
}

#[test]
fn stub_and_property_decisions() {
    assert!(api::is_lookup_name(b"Entity_FindByName"));
    assert!(api::is_lookup_name(b"FindAny"));
    assert!(!api::is_lookup_name(b"Entity_Get"));
    assert_eq!(api::stub_return_kind(false, false, true), api::STUB_HANDLE);
    assert!(api::is_script_struct(b"ref", b"cLuxFoo", true));
    assert!(!api::is_script_struct(b"ref", b"cFrustum", true));
    assert!(api::is_copy_ctor(b"const cX &in", b"cX"));
    assert!(api::is_copy_ctor(b"const cX&in", b"cX"));
    let en = |s: &[u8]| s == b"eK";
    let (g, s) = api::property_accessors(b"eK mKind;", &en).unwrap();
    assert_eq!(g, b("eK get_mKind() const property"));
    assert_eq!(s.unwrap(), b("void set_mKind(eK) property"));
    let (g, s) = api::property_accessors(b"const tString@ mName", &en).unwrap();
    assert_eq!(g, b("tString@ get_mName() const property"));
    assert!(s.is_none());
    let (_, s) = api::property_accessors(b"cVector3f mPos", &en).unwrap();
    assert_eq!(s.unwrap(), b("void set_mPos(const cVector3f &in) property"));
    assert_eq!(api::prop_name(b"float mX;"), b("mX"));
}

#[test]
fn stream_reads_like_istream() {
    let mut s = Stream::new(b"  12 -7 2.5e1 abc");
    let (mut i, mut j, mut f) = (0, 0, 0.0f32);
    s.int(&mut i);
    s.int(&mut j);
    s.float(&mut f);
    assert_eq!((i, j, f), (12, -7, 25.0));
    assert!(s.good());
    assert_eq!(s.token(), Some(b"abc".as_slice()));
    assert!(!s.good());
    let mut s = Stream::new(b"nan 1");
    let mut f = 9.0f32;
    s.float(&mut f);
    assert_eq!(f, 0.0);
    assert!(s.failed());
    let mut t = true;
    let mut s = Stream::new(b"2");
    s.boolean(&mut t);
    assert!(t && s.failed());
    let mut n = 0usize;
    let mut s = Stream::new(b"-1");
    s.size(&mut n);
    assert_eq!(n, usize::MAX);
}

fn sample() -> Globals {
    let mut g = Globals::default();
    let v = g.vars.entry(b("name with space")).or_default();
    v.s = b("hi\nthere");
    v.b = true;
    v.i = -3;
    v.f = 0.1;
    v.v2 = [1.5, -2.0];
    v.v3 = [1.0, 2.0, 3.0];
    v.v4 = [1.0, 2.0, 3.0, 4.0];
    v.c = [0.1, 0.2, 0.3, 0.4];
    v.id = (7, -8, 9);
    v.m[5] = 3.25;
    g
}

#[test]
fn globals_round_trip() {
    let g = sample();
    let text = globals::serialize(&g);
    let mut h = Globals::default();
    globals::deserialize(&mut h, &text);
    assert_eq!(h.vars, g.vars);
    assert_eq!(globals::serialize(&h), text);
}

#[test]
fn globals_survive_garbage() {
    let text = globals::serialize(&sample());
    for cut in 0..text.len() {
        let mut h = Globals::default();
        globals::deserialize(&mut h, text.get(..cut).unwrap_or_default());
    }
    for bad in [
        &b"var\t99999999999999999999:x"[..],
        b"var\t3:abc2:de nan inf",
        b"junk\nvar",
        b"var\t-1:",
    ] {
        let mut h = Globals::default();
        globals::deserialize(&mut h, bad);
    }
}

#[test]
fn secret_code_xors_first_three_bytes() {
    let mut g = Globals::default();
    assert!(!globals::secret_code(&mut g, [1, 2, 3], b"ab"));
    assert!(globals::secret_code(&mut g, [1, 2, 3], b"abcd"));
    assert_eq!(g.args[&0].s, vec![b'a' ^ 1, b'b' ^ 2, b'c' ^ 3]);
}

#[test]
fn check_helpers() {
    assert_eq!(check::json_escape(b"a\"b\\c\nd\te"), b("a\\\"b\\\\c\\nde"));
    assert_eq!(
        check::from_script_dir(b"/g/script/a.hps:1"),
        b("script/a.hps:1")
    );
    assert_eq!(check::from_script_dir(b"plain"), b("plain"));
    let cfg = b("x \"b/Z.HPS\" y \"a.hps\" \"nothing\" \"a.hps\" \".hps\" \"q\n.hps\"");
    let r = check::config_scripts(&[cfg.as_slice()]);
    assert_eq!(r, vec![b("a.hps"), b("b/Z.HPS"), b("q\n.hps")]);
}

#[test]
fn runtime_helpers() {
    assert_eq!(
        runtime::split_global_marker(b"void $F()"),
        (true, b("void F()"))
    );
    assert_eq!(
        runtime::split_global_marker(b"void F()"),
        (false, b("void F()"))
    );
    let d = runtime::callback_decls(b"$Go");
    assert!(d.global);
    assert_eq!(d.with_arg, b("void Go(const tString &in)"));
    assert!(!runtime::callback_decls(b"$").global);
    let mods: Vec<(&[u8], bool)> =
        vec![(b"a/x.hps", false), (b"b/x.hps", true), (b"c/x.hps", true)];
    assert_eq!(runtime::pick_module(&mods, b"x.hps"), Some(1));
    assert_eq!(runtime::pick_module(&mods, b"zz"), None);
    let lines = runtime::stub_report(&[(b"f".as_slice(), 2), (b"g", 9), (b"h", 2)], 2);
    assert_eq!(
        lines[0],
        b("SOMA script: 3 unimplemented API functions called\n")
    );
    assert_eq!(lines[1], b("       9  g\n"));
    assert_eq!(lines[2], b("       2  h\n"));
    assert_eq!(lines.len(), 3);
    let q = runtime::ApiLookup {
        game: b"rebirth",
        current_game: b"soma",
        env: Some(b"/env"),
        exe: Some(b"/opt/bin/game"),
        source_dir: Some(b"/src"),
    };
    let c = runtime::api_candidates(&q);
    assert_eq!(c[0], b("/opt/bin/../share/open-hpl/rebirth/script_api.txt"));
    assert_eq!(c.last().unwrap(), &b("/src/rebirth/data/script_api.txt"));
}
