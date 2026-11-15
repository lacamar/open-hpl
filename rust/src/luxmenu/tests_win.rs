use super::fake::{FAKE, Node, reset};
use super::ffi_win::ohpl_a10a_cb;
use super::host::{msg, op};
use super::keyconfig_win as kcw;
use super::options_win as ow;
use super::store::CbData;

fn nodes(kind: i32) -> Vec<usize> {
    FAKE.with(|f| {
        let f = f.borrow();
        (1..f.nodes.len())
            .filter(|&i| f.nodes.get(i).is_some_and(|n| n.kind == kind))
            .collect()
    })
}

fn node(id: usize) -> Node {
    FAKE.with(|f| f.borrow().nodes.get(id).cloned().unwrap_or_default())
}

fn text(id: usize) -> String {
    super::host::narrow(&node(id).text)
}

fn items(id: usize) -> Vec<String> {
    node(id)
        .items
        .iter()
        .map(|t| super::host::narrow(t))
        .collect()
}

fn fire(id: usize, message: i32, data: CbData) -> i32 {
    let token = node(id)
        .callbacks
        .iter()
        .find(|c| c.0 == message)
        .map_or(0, |c| c.1);
    let data = CbData {
        msg: message,
        ..data
    };
    // SAFETY: `data` is a live record.
    unsafe { ohpl_a10a_cb(token, id, &data) }
}

fn log() -> Vec<String> {
    FAKE.with(|f| f.borrow().log.clone())
}

fn open_options() -> u64 {
    reset();
    let me = ow::create(1);
    ow::create_gui(me);
    ow::on_set_active(me, true);
    me
}

fn button(label: &str) -> usize {
    let key = format!("MainMenu.{label}");
    nodes(op::BUTTON)
        .into_iter()
        .find(|&b| text(b) == key)
        .unwrap_or(0)
}

#[test]
fn options_tree_and_initial_values() {
    open_options();
    assert_eq!(nodes(op::WINDOW).len(), 1);
    assert_eq!(nodes(op::TABFRAME).len(), 1);
    let combos: Vec<Vec<String>> = nodes(op::COMBO).into_iter().map(items).collect();
    assert!(combos.contains(&vec!["1x".into(), "2x".into(), "3x".into(), "4x".into()]));
    assert!(combos.contains(&vec!["OpenGL".into(), "Vulkan".into()]));
    assert!(combos.contains(&vec![
        "SDR".into(),
        "HDR10".into(),
        "scRGB HDR".into(),
        "Wide color (P3)".into()
    ]));
    let aniso: Vec<String> = ["OptionsMenu.Off", "2X", "4X", "8X", "16X"]
        .map(String::from)
        .to_vec();
    assert!(combos.contains(&aniso));
    let gui_scale = nodes(op::COMBO)
        .into_iter()
        .find(|&c| items(c).first().is_some_and(|t| t == "1x"));
    assert_eq!(gui_scale.map(|c| node(c).selected), Some(1));
    let res = nodes(op::COMBO)
        .into_iter()
        .find(|&c| items(c).len() == 3 && items(c).get(2).is_some_and(|t| t == "1920x1080"));
    assert_eq!(res.map(|c| node(c).selected), Some(2));
    let hints = nodes(op::CHECKBOX)
        .into_iter()
        .find(|&c| text(c) == "OptionsMenu.ShowHints");
    assert_eq!(hints.map(|c| node(c).checked), Some(true));
}

#[test]
fn options_ok_applies_and_closes() {
    open_options();
    let hints = nodes(op::CHECKBOX)
        .into_iter()
        .find(|&c| text(c) == "OptionsMenu.ShowHints")
        .unwrap_or(0);
    FAKE.with(|f| f.borrow_mut().nodes[hints].checked = false);
    fire(button("OK"), msg::BUTTON_PRESSED, CbData::default());
    let applied = FAKE.with(|f| f.borrow().applied);
    let v = applied.expect("apply");
    assert_eq!(
        (v.show_hints, v.res_x, v.res_y, v.gui_scale),
        (0, 1920, 1080, 2)
    );
    assert_eq!(v.aniso, 4.0);
    assert_eq!(v.ssao_samples, 8);
    assert_eq!(&v.lang.bytes(), b"english.lang");
    let l = log();
    assert!(l.iter().any(|s| s == "op 104"));
    assert_eq!(l.last().map(String::as_str), Some("window 7"));
}

#[test]
fn options_cancel_restores_live_values() {
    open_options();
    fire(button("Cancel"), msg::BUTTON_PRESSED, CbData::default());
    let l = log();
    assert!(l.iter().any(|s| s == "op 107 1"));
    assert!(l.iter().any(|s| s == "op 108 1"));
    assert!(l.iter().any(|s| s == "op 110 0.5"));
    assert_eq!(l.last().map(String::as_str), Some("window 7"));
}

#[test]
fn options_ui_cancel_needs_secondary() {
    open_options();
    let cancel = button("Cancel");
    let data = CbData {
        lval: 1,
        ..CbData::default()
    };
    assert_eq!(fire(cancel, msg::UI_BUTTON, data), 0);
    let data = CbData {
        lval: 2,
        ..CbData::default()
    };
    assert_eq!(fire(cancel, msg::UI_BUTTON, data), 1);
}

#[test]
fn options_toggle_switches_graphics_pages() {
    open_options();
    let toggle = button_by_prefix("OptionsMenu.AdvancedOptions");
    let before: Vec<usize> = nodes(op::DUMMY)
        .into_iter()
        .filter(|&d| node(d).visible)
        .collect();
    assert_eq!(before.len(), 1);
    fire(toggle, msg::BUTTON_PRESSED, CbData::default());
    assert_eq!(text(toggle), "OptionsMenu.BasicOptions");
    let after: Vec<usize> = nodes(op::DUMMY)
        .into_iter()
        .filter(|&d| node(d).visible)
        .collect();
    assert_eq!(after.len(), 1);
    assert_ne!(before, after);
}

fn button_by_prefix(t: &str) -> usize {
    nodes(op::BUTTON)
        .into_iter()
        .find(|&b| text(b) == t)
        .unwrap_or(0)
}

fn open_keyconfig() -> u64 {
    reset();
    let me = kcw::create(1);
    kcw::create_gui(me);
    kcw::on_set_active(me, true);
    me
}

fn entry_inputs() -> Vec<usize> {
    nodes(op::LABEL)
        .into_iter()
        .filter(|&l| node(l).callbacks.iter().any(|c| c.0 == msg::KEY_PRESS))
        .collect()
}

#[test]
fn keyconfig_lists_configurable_actions() {
    open_keyconfig();
    assert_eq!(entry_inputs().len(), 5 * 2);
    assert!(text(entry_inputs()[0]).starts_with("ButtonNames."));
}

#[test]
fn keyconfig_rebinds_and_applies() {
    open_keyconfig();
    let first = entry_inputs()[0];
    fire(
        first,
        msg::MOUSE_DOWN,
        CbData {
            lval: 1,
            ..CbData::default()
        },
    );
    assert_eq!(text(first), "...");
    fire(
        first,
        msg::KEY_PRESS,
        CbData {
            key: 30,
            ..CbData::default()
        },
    );
    assert_ne!(text(first), "...");
    fire(
        button_by_prefix("Global.OK"),
        msg::BUTTON_PRESSED,
        CbData::default(),
    );
    let l = log();
    assert!(l.iter().any(|s| s == "clear 10"));
    assert!(l.iter().any(|s| s == "add 10 1 30 0"));
    assert_eq!(l.last().map(String::as_str), Some("window 1"));
}

#[test]
fn keyconfig_marks_duplicate_inputs_red() {
    open_keyconfig();
    let inputs = entry_inputs();
    let (a, b) = (inputs[0], inputs[2]);
    for w in [a, b] {
        fire(
            w,
            msg::MOUSE_DOWN,
            CbData {
                lval: 1,
                ..CbData::default()
            },
        );
        fire(
            w,
            msg::KEY_PRESS,
            CbData {
                key: 30,
                ..CbData::default()
            },
        );
    }
    assert_eq!(node(a).color, [1.0, 0.0, 0.0, 1.0]);
    assert_eq!(node(b).color, [1.0, 0.0, 0.0, 1.0]);
    assert_eq!(node(inputs[4]).color, [1.0, 1.0, 1.0, 1.0]);
}

#[test]
fn keyconfig_escape_cancels_waiting() {
    let me = open_keyconfig();
    let first = entry_inputs()[0];
    fire(
        first,
        msg::MOUSE_DOWN,
        CbData {
            lval: 1,
            ..CbData::default()
        },
    );
    fire(
        first,
        msg::KEY_PRESS,
        CbData {
            key: super::host::KEY_ESCAPE,
            ..CbData::default()
        },
    );
    assert_ne!(text(first), "...");
    kcw::exit_pressed(me);
    assert!(!log().iter().any(|s| s == "window 1"));
    kcw::exit_pressed(me);
    assert!(log().iter().any(|s| s == "window 1"));
}

const CPP_DUMP: &str = include_str!("../../tests/golden/A10a/cpp_00_rainy_hall.txt");

fn cpp_section(name: &str) -> Vec<&'static str> {
    let mut out = Vec::new();
    let mut inside = false;
    for l in CPP_DUMP.lines() {
        if let Some(n) = l.strip_prefix("== ") {
            inside = n == name;
        } else if inside {
            out.push(l);
        }
    }
    out
}

fn cpp_count(section: &[&str], ty: i32) -> usize {
    let key = format!(" type={ty} ");
    section.iter().filter(|l| l.contains(&key)).count()
}

fn cpp_combo_items(section: &[&str]) -> Vec<Vec<String>> {
    section
        .iter()
        .filter_map(|l| {
            let list = l
                .split(" combo=")
                .nth(1)?
                .split_once('[')?
                .1
                .strip_suffix(']')?;
            Some(
                list.split("\",\"")
                    .map(|s| s.trim_matches('"').to_string())
                    .collect(),
            )
        })
        .collect()
}

#[test]
fn options_tree_matches_recorded_cpp_census() {
    let cpp = cpp_section("options");
    open_options();
    assert_eq!(nodes(op::COMBO).len(), cpp_count(&cpp, 10));
    assert_eq!(nodes(op::CHECKBOX).len(), cpp_count(&cpp, 7));
    assert_eq!(nodes(op::GROUP).len(), cpp_count(&cpp, 17));
    assert_eq!(nodes(op::TABFRAME).len(), cpp_count(&cpp, 16));
    let theirs = cpp_combo_items(&cpp);
    for list in nodes(op::COMBO)
        .into_iter()
        .map(items)
        .filter(|l| l.iter().all(|t| !t.contains('.')))
    {
        if list.len() > 1 && list.iter().all(|t| t.len() <= 16) && list != ["1920x1080"] {
            let known = ["1x", "OpenGL", "SDR", "4"];
            if known.contains(&list[0].as_str()) {
                assert!(
                    theirs.contains(&list),
                    "{list:?} missing from the C++ record"
                );
            }
        }
    }
}
