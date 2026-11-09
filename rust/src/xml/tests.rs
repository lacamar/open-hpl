//! Expectations are tinyXML 2.5 behaviour, confirmed against the vendored C++ by the differential
//! harness over the five games' XML-family files.

use super::*;

fn parse(s: &[u8]) -> Doc {
    let mut d = Doc::new();
    d.parse(s);
    d
}

fn root(d: &Doc) -> NodeId {
    d.first_child_element(DOC, None).unwrap()
}

fn text_of(s: &[u8]) -> Vec<u8> {
    let d = parse(s);
    assert!(!d.error(), "{:?}", d.error_info());
    d.text(root(&d)).unwrap().to_vec()
}

#[test]
fn named_entities_decode() {
    assert_eq!(text_of(b"<a>&amp;&lt;&gt;&quot;&apos;</a>"), b"&<>\"'");
}

#[test]
fn unknown_entity_loses_its_ampersand() {
    assert_eq!(text_of(b"<a>x &bogus; y</a>"), b"x bogus; y");
}

#[test]
fn numeric_references_follow_encoding() {
    assert_eq!(text_of(b"<a>&#65;&#x42;</a>"), b"AB");
    assert_eq!(text_of(b"<a>&#233;</a>"), [0xe9]);
    assert_eq!(text_of(b"\xef\xbb\xbf<a>&#233;</a>"), [0xc3, 0xa9]);
    assert_eq!(
        text_of(b"<?xml version=\"1.0\" encoding=\"UTF-8\"?><a>&#x20AC;</a>"),
        [0xe2, 0x82, 0xac]
    );
}

#[test]
fn text_whitespace_condenses() {
    assert_eq!(
        text_of(b"<a>  one \r\n\t two   three  </a>"),
        b"one two three"
    );
}

#[test]
fn blank_text_is_dropped_and_cdata_kept_verbatim() {
    let d = parse(b"<a> \n <b/> </a>");
    assert_eq!(d.node(root(&d)).first, d.node(root(&d)).last);
    assert_eq!(text_of(b"<a><![CDATA[ x <y> & ]]></a>"), b" x <y> & ");
}

#[test]
fn errors_report_first_failure() {
    for (src, desc) in [
        (&b"<a><b></a>"[..], "Error reading end tag."),
        (b"<a b=\"1\" b=\"2\"/>", "Error parsing Element."),
        (b"", "Error document empty."),
        (b"   ", "Error document empty."),
        (b"<a b/>", "Error reading Attributes."),
        (b"<a b=x\"y/>", "Error reading Attributes."),
        (b"<a/ >", "Error: empty tag."),
        (b"<a><![CDATA[x</a>", "Error reading Element value."),
    ] {
        let d = parse(src);
        assert!(d.error(), "{src:?}");
        assert_eq!(d.error_info().desc, desc, "{src:?}");
    }
}

#[test]
fn error_position_is_one_based() {
    let d = parse(b"<a>\n  <b></c></a>");
    assert_eq!((d.error_info().row, d.error_info().col), (2, 6));
}

#[test]
fn text_running_into_end_of_input_fails_instead_of_looping() {
    assert!(parse(b"<a>text").error());
    assert!(parse(b"<a>x").error());
}

#[test]
fn trailing_garbage_after_the_root_is_ignored() {
    let d = parse(b"<a/>trailing");
    assert!(!d.error());
    assert_eq!(d.print_stream(), b"<a />");
}

#[test]
fn malformed_declaration_ends_parsing_silently() {
    let d = parse(b"<?xml version=\"1.0\" <a/>");
    assert!(!d.error());
    assert!(d.first_child_element(DOC, None).is_none());
}

#[test]
fn attributes_keep_document_order() {
    let d = parse(b"<a z=\"1\" a=\"2\" m=\"3\"/>");
    let names: Vec<_> = d.attrs(root(&d)).map(|a| a.name.clone()).collect();
    assert_eq!(names, [b"z".to_vec(), b"a".to_vec(), b"m".to_vec()]);
}

#[test]
fn attribute_lookup_stops_at_nul() {
    let d = parse(b"<a b=\"x&#0;y\"/>");
    assert_eq!(d.attribute(root(&d), b"b").unwrap(), b"x");
    assert_eq!(d.print_stream(), b"<a b=\"x&#x00;y\" />");
}

#[test]
fn file_printer_indents_with_four_spaces() {
    let d = parse(
        b"<?xml version=\"1.0\" encoding=\"UTF-8\"?><a x=\"1\"><!--c--><b/><c>t</c><d><e/></d></a>",
    );
    let expect = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<a x=\"1\">\n    <!--c-->\n    <b />\n    <c>t</c>\n    <d>\n        <e />\n    </d>\n</a>\n";
    assert_eq!(String::from_utf8(d.print_file()).unwrap(), expect);
}

#[test]
fn stream_printer_has_no_whitespace() {
    let d = parse(b"<a x=\"1\">\n<b/>\n<c>t</c></a>");
    assert_eq!(d.print_stream(), b"<a x=\"1\"><b /><c>t</c></a>");
}

#[test]
fn printer_escapes_and_picks_quotes() {
    let d = parse(b"<a b='x\"y' c=\"&lt;&amp;'\">\x01\t</a>");
    assert!(!d.error());
    assert_eq!(
        d.print_stream(),
        b"<a b='x&quot;y' c=\"&lt;&amp;&apos;\">&#x01;</a>"
    );
}

#[test]
fn hex_character_references_in_values_pass_through() {
    let mut d = Doc::new();
    let e = d.add_element(DOC, b"a");
    d.set_attribute(e, b"v", b"&#x41;&#x42 &");
    assert_eq!(d.print_stream(), b"<a v=\"&#x41;&#x42 &amp;\" />");
}

#[test]
fn bom_survives_a_save() {
    let d = parse(b"\xef\xbb\xbf<a/>");
    assert_eq!(d.print_file(), b"\xef\xbb\xbf<a />\n");
}

#[test]
fn load_normalizes_line_endings_and_cuts_at_nul() {
    let mut d = Doc::new();
    assert!(d.load(b"<a>x\r\ny\rz</a>\0<b/>", true));
    assert_eq!(d.text(root(&d)).unwrap(), b"x y z");
    assert!(!Doc::new().load(b"", true));
    let mut d = Doc::new();
    assert!(!d.load(b"<a/>", false));
    assert_eq!(d.error_info().desc, "Failed to open file");
}

#[test]
fn fresh_document_error_info_is_row_one_col_one() {
    let d = Doc::new();
    assert_eq!(
        (
            d.error_info().row,
            d.error_info().col,
            d.error_info().desc.as_str()
        ),
        (1, 1, "")
    );
}

#[test]
fn config_set_and_get() {
    let mut d = parse(b"<!--keep--><Main A=\"1\"/>");
    config_set(&mut d, b"Main", b"B", b"2");
    config_set(&mut d, b"Other", b"C", b"3");
    config_set(&mut d, b"Main", b"A", b"9");
    assert_eq!(config_get(&d, b"Main", b"A"), Some(&b"9"[..]));
    assert_eq!(config_get(&d, b"Main", b"X"), None);
    assert_eq!(config_get(&d, b"Nope", b"A"), None);
    assert_eq!(
        d.print_file(),
        b"<!--keep-->\n<Main A=\"9\" B=\"2\" />\n<Other C=\"3\" />\n"
    );
}

#[test]
fn lang_decoding() {
    use lang::decode_string;
    assert_eq!(decode_string(b"a[br]b"), [97, 10, 98]);
    assert_eq!(decode_string(b"[u233][u]x"), [233, 0, 120]);
    assert_eq!(decode_string(b"\xc3\xa9"), [233]);
    assert_eq!(decode_string(b"[x] [br"), [91, 120, 93, 32, 10]);
    assert_eq!(decode_string(b"["), [91]);
}

#[derive(Default)]
struct Events(Vec<String>);

impl lang::LangSink for Events {
    fn directory(&mut self, path: &[u8], add: bool) {
        self.0
            .push(format!("dir {} {add}", String::from_utf8_lossy(path)));
    }
    fn no_resources(&mut self) {
        self.0.push("none".into());
    }
    fn category(&mut self, name: &[u8]) {
        self.0
            .push(format!("cat {}", String::from_utf8_lossy(name)));
    }
    fn entry(&mut self, name: &[u8], text: &[i32]) {
        self.0
            .push(format!("entry {} {text:?}", String::from_utf8_lossy(name)));
    }
}

#[test]
fn lang_walk() {
    let d = parse(
        b"<L><RESOURCES><Directory Path=\"/a/b\" AddSubDirs=\"false\"/><Directory Path=\"\"/><Directory Path=\"c\"/></RESOURCES>\
          <CATEGORY Name=\"C\"><Entry Name=\"e\">hi[br]</Entry><Entry Name=\"f\"/><Entry Name=\"g\"><!--x-->t</Entry></CATEGORY></L>",
    );
    let mut ev = Events::default();
    lang::read(&d, true, &mut ev).unwrap();
    assert_eq!(
        ev.0,
        [
            "dir a/b false",
            "dir c true",
            "cat C",
            "entry e [104, 105, 10]",
            "entry f []",
            "entry g []"
        ]
    );
    let mut ev = Events::default();
    lang::read(&parse(b"<L/>"), true, &mut ev).unwrap();
    assert_eq!(ev.0, ["none"]);
    assert!(lang::read(&parse(b"<!--c-->"), true, &mut Events::default()).is_none());
}

#[test]
fn deep_nesting_is_capped() {
    // Runs on the default 2 MB test-thread stack, well under the engine's 8 MB main stack.
    let ok = |n: usize| {
        !parse(format!("<a>{}{}</a>", "<b>".repeat(n), "</b>".repeat(n)).as_bytes()).error()
    };
    assert!(ok(500));
    assert!(!ok(513));
}

#[test]
fn unknown_node_ids_are_inert() {
    let mut d = Doc::new();
    let bad: NodeId = 99;
    d.set_value(bad, b"x");
    d.set_attribute(bad, b"a", b"b");
    let orphan = d.add_element(bad, b"e");
    assert!(d.node(bad).first.is_none());
    assert!(d.node(orphan).parent.is_none());
    assert!(d.node(DOC).first.is_none());
    assert_eq!(d.first_child_element(bad, None), None);
}

#[test]
fn encode_string_truncated_hex_entity() {
    for (inp, want) in [
        (&b"&#x"[..], &b"&#x"[..]),
        (b"&#x41", b"&#x41"),
        (b"&#x41;<", b"&#x41;&lt;"),
        (b"&#", b"&amp;#"),
    ] {
        let mut out = Vec::new();
        encode_string(inp, &mut out);
        assert_eq!(out, want, "input: {inp:?}");
    }
}
