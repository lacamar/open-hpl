//! S5: scriptable timers/faders replayed against the C++ self-test dump (`OHPL_S5_DUMP`, `st` lines),
//! and the input handler against a fake `cInput`.

use super::s5_input::{self as input, Action, InHost, Preset};
use super::s5_scriptable::Table;
use std::cell::RefCell;
use std::collections::{BTreeMap, BTreeSet};
use std::ffi::c_void;

const GOLDEN: &str = include_str!("../../tests/golden/S5/timers.txt");

struct Lcg(u64);

impl Lcg {
    fn next(&mut self) -> u32 {
        self.0 = (self.0.wrapping_mul(1_103_515_245).wrapping_add(12345)) & 0x7fff_ffff;
        ((self.0 >> 8) & 0xffff) as u32
    }

    fn f(&mut self) -> f32 {
        self.next() as f32 / 65536.0
    }

    fn flag(&mut self) -> bool {
        self.next() & 1 != 0
    }
}

fn float(tok: &str) -> f32 {
    if tok.trim_start_matches('-').eq_ignore_ascii_case("nan") {
        f32::NAN
    } else {
        tok.parse().unwrap_or(f32::INFINITY)
    }
}

fn same(a: f32, b: f32) -> bool {
    (a.is_nan() && b.is_nan()) || a.to_bits() == b.to_bits()
}

#[test]
fn timers_match_cpp_self_test() {
    let ids = [
        1u64,
        2,
        3,
        super::scriptable::hash64(b"a"),
        super::scriptable::hash64(b"bb"),
    ];
    let funcs: [&[u8]; 3] = [b"", b"F", b"G"];
    let mut golden = GOLDEN.lines().filter(|l| l.starts_with("st "));
    let mut rng = Lcg(12345);
    let mut t = Table::default();
    let mut checked = 0;
    for _ in 0..600 {
        let id = ids[(rng.next() % 5) as usize];
        let kind = rng.next() % 16;
        let clear = kind == 15 && rng.next().is_multiple_of(8);
        let line = if kind == 15 && !clear {
            ""
        } else {
            golden.next().unwrap_or_default()
        };
        let tok: Vec<&str> = line.split(' ').collect();
        let arg = |i: usize| tok.get(i).copied().unwrap_or_default();
        let num = |i: usize| float(arg(i));
        let b = |i: usize| arg(i) == "1";
        match kind {
            0 | 1 => {
                let time = rng.f() * 2.0;
                let f = funcs[(rng.next() % 3) as usize];
                let (c, r) = (rng.flag(), rng.flag());
                assert_eq!(arg(1), "add", "{line}");
                assert!(same(num(3), time), "{line}");
                assert_eq!(arg(4).as_bytes(), f);
                t.add(id, time, f, c, r);
                assert_eq!((b(5), b(6)), (c, r), "{line}");
            }
            2 => {
                t.remove(id);
                assert_eq!(arg(1), "rem");
            }
            3 => {
                assert_eq!(b(3), t.exists(id), "{line}");
            }
            4 => assert!(same(num(3), t.time_left(id)), "{line}"),
            5 => {
                let p = rng.flag();
                t.set_paused(id, p);
                assert_eq!(arg(1), "pause");
                assert_eq!(b(3), p, "{line}");
            }
            6 => {
                let l = rng.f() * 2.0;
                assert!(same(num(3), l), "{line}");
                assert_eq!(b(4), t.has_passed(id, l), "{line}");
            }
            7 => {
                let e = (rng.next() % 31) as i32;
                let a = rng.flag();
                assert!(same(num(5), t.value(id, -1.0, 3.0, e, a)), "{line}");
            }
            8 => {
                let g = rng.f() * 4.0 - 1.0;
                let time = if rng.next().is_multiple_of(4) {
                    0.0
                } else {
                    rng.f() * 2.0
                };
                let (r, k) = (rng.flag(), rng.flag());
                assert!(same(num(3), g) && same(num(4), time), "{line}");
                t.fade_to(id, g, time, r, k);
            }
            9 => {
                let x = rng.f() * 2.0 - 1.0;
                let k = rng.flag();
                assert!(same(num(3), x), "{line}");
                t.set_fader(id, x, k);
            }
            10 => {
                let p = rng.flag();
                t.fader_paused(id, p);
                assert_eq!(b(3), p, "{line}");
            }
            11 => {
                let e = (rng.next() % 31) as i32;
                let a = rng.flag();
                assert!(same(num(5), t.fader_value(id, 0.0, 1.0, e, a)), "{line}");
            }
            12..=14 => {
                let dt = rng.f() * 0.5;
                assert!(same(num(2), dt), "{line}");
                t.step_timers(dt);
                t.step_faders(dt);
                assert_eq!(arg(3), format!("idle={}", u8::from(t.idle())), "{line}");
            }
            _ => {
                if clear {
                    t.clear_timers();
                    t.clear_faders();
                    assert_eq!(arg(1), "clear");
                }
            }
        }
        let state = golden.next().unwrap_or_default();
        assert_eq!(
            state
                .strip_prefix("st state ")
                .unwrap_or_default()
                .as_bytes(),
            t.describe().as_slice(),
            "after {line}"
        );
        checked += 1;
    }
    assert_eq!(checked, 600);
}

#[derive(Default)]
struct Fake {
    ids: RefCell<BTreeMap<i32, Vec<u8>>>,
    down: RefCell<BTreeSet<i32>>,
    events: RefCell<Vec<String>>,
    mouse: RefCell<(i32, i32)>,
}

impl Fake {
    fn ev(&self, s: String) {
        self.events.borrow_mut().push(s);
    }
}

impl InHost for Fake {
    fn action_exists_id(&self, id: i32) -> bool {
        self.ids.borrow().contains_key(&id)
    }
    fn action_exists_name(&self, name: &[u8]) -> bool {
        self.ids.borrow().values().any(|n| n == name)
    }
    fn action_destroy_id(&self, id: i32) {
        self.ev(format!("destroy {id}"));
        self.ids.borrow_mut().remove(&id);
    }
    fn action_destroy_name(&self, name: &[u8]) {
        self.ev(format!("destroy {}", String::from_utf8_lossy(name)));
        self.ids.borrow_mut().retain(|_, n| n != name);
    }
    fn action_create(&self, name: &[u8], id: i32) {
        self.ids.borrow_mut().insert(id, name.to_vec());
    }
    fn action_create_mouse(&self, name: &[u8], id: i32, _button: i32) {
        self.ids.borrow_mut().insert(id, name.to_vec());
    }
    fn action_id_of(&self, name: &[u8]) -> Option<i32> {
        self.ids
            .borrow()
            .iter()
            .find(|(_, n)| *n == name)
            .map(|(i, _)| *i)
    }
    fn action_triggered(&self, id: i32) -> bool {
        self.down.borrow().contains(&id)
    }
    fn add_key(&self, id: i32, key: &[u8]) {
        self.ev(format!("key {id} {}", String::from_utf8_lossy(key)));
    }
    fn add_button(&self, id: i32, button: &[u8]) {
        self.ev(format!("button {id} {}", String::from_utf8_lossy(button)));
    }
    fn key_down(&self, _i: i32) -> bool {
        false
    }
    fn key_name(&self, _i: i32) -> Vec<u8> {
        Vec::new()
    }
    fn button_down(&self, _i: i32) -> bool {
        false
    }
    fn button_name(&self, _i: i32) -> Vec<u8> {
        Vec::new()
    }
    fn warn(&self, text: &[u8]) {
        self.ev(format!("warn {}", String::from_utf8_lossy(text)));
    }
    fn log_actions(&self, count: usize) {
        self.ev(format!("log {count}"));
    }
    fn call(&self, decl: &[u8]) {
        self.ev(format!("call {}", String::from_utf8_lossy(decl)));
    }
    fn call_key_config(&self) {
        self.ev("keyconfig".into());
    }
    fn broadcast_action(&self, id: i32, pressed: bool) {
        self.ev(format!("action {id} {}", u8::from(pressed)));
    }
    fn broadcast_analog(&self, id: i32, v: [f32; 3]) {
        self.ev(format!("analog {id} {v:?}"));
    }
    fn mouse_rel(&self) -> (i32, i32) {
        *self.mouse.borrow()
    }
    fn screen_height(&self) -> f32 {
        100.0
    }
    fn user_config(&self) -> Option<(bool, bool, f32, f32)> {
        None
    }
    fn arr_size(&self, _a: *const c_void) -> usize {
        0
    }
    fn arr_at(&self, _a: *const c_void, _i: usize) -> *const c_void {
        std::ptr::null()
    }
    fn str_ref(&self, _t: &[u8]) -> *mut c_void {
        std::ptr::null_mut()
    }
    fn engine_action_name(&self, id: i32) -> Option<Vec<u8>> {
        self.ids.borrow().get(&id).cloned()
    }
    fn engine_action_binds(&self, _id: i32) -> Vec<u8> {
        Vec::new()
    }
    fn dump(&self, line: &[u8]) {
        self.ev(String::from_utf8_lossy(line).into_owned());
    }
}

fn take(h: &Fake) -> Vec<String> {
    std::mem::take(&mut *h.events.borrow_mut())
}

#[test]
fn init_creates_mouse_actions_once() {
    let h = Fake::default();
    input::init(&h, 7, 0.7, 4, 3);
    assert_eq!(h.ids.borrow().len(), 5);
    assert!(h.action_exists_id(199_007));
    input::init(&h, 7, 0.7, 4, 3);
    assert_eq!(h.ids.borrow().len(), 5);
}

#[test]
fn create_action_replaces_id_and_name() {
    let h = Fake::default();
    input::init(&h, 7, 0.7, 0, 0);
    input::create_action(&h, Action::plain(b"Jump", 3));
    input::create_action(&h, Action::plain(b"Crouch", 3));
    assert_eq!(input::action_name(3, false), b"Crouch");
    input::create_action(&h, Action::plain(b"Crouch", 9));
    assert_eq!(input::action_name(9, false), b"Crouch");
    // the name clash destroys the engine action only; the old table row stays
    assert!(h.action_exists_id(9) && !h.action_exists_id(3));
    assert_eq!(input::action_name(3, false), b"Crouch");
}

#[test]
fn key_config_reload_destroys_then_rebuilds() {
    let h = Fake::default();
    input::init(&h, 7, 0.7, 0, 0);
    input::create_action(&h, Action::plain(b"Jump", 3));
    take(&h);
    input::load_key_config(&h);
    assert_eq!(
        take(&h),
        [
            "destroy 3",
            "call void CreateActions()",
            "keyconfig",
            "log 0"
        ]
    );
    input::create_action_input(&h, b"Keyboard.W", 77);
    assert!(take(&h)[0].starts_with("warn Could not add input Keyboard.W for action 77"));
}

#[test]
fn edges_and_analog_broadcast() {
    let h = Fake::default();
    input::init(&h, 7, 0.7, 0, 0);
    input::create_action(&h, Action::plain(b"Jump", 3));
    input::create_action(
        &h,
        Action {
            axis: 1,
            mul: 2.0,
            analog_id: 7,
            ..Action::plain(b"Look", 4)
        },
    );
    input::create_action(
        &h,
        Action {
            analog_id: 8,
            gamepad: true,
            ..Action::plain(b"Pad", 5)
        },
    );
    take(&h);
    h.down.borrow_mut().extend([3, 4, 5]);
    input::latch(&h);
    input::update(&h, true);
    assert_eq!(
        take(&h),
        ["action 3 1", "action 4 1", "analog 7 [0.0, 2.0, 0.0]"]
    );
    input::latch(&h);
    input::update(&h, true);
    assert_eq!(take(&h), ["analog 7 [0.0, 2.0, 0.0]"]);
    h.down.borrow_mut().clear();
    input::latch(&h);
    input::update(&h, false);
    assert!(take(&h).is_empty());
    input::update(&h, true);
    assert_eq!(take(&h), ["action 3 0", "action 4 0"]);
    assert!(input::became_down(3) || input::was_triggered(3));
}

#[test]
fn clear_gui_consumes_menu_edges() {
    let h = Fake::default();
    input::init(&h, 7, 0.7, 0, 0);
    input::create_action(&h, Action::plain(b"MenuOk", 10));
    input::create_action(&h, Action::plain(b"Fire", 11));
    h.down.borrow_mut().extend([10, 11]);
    input::latch(&h);
    input::clear_gui_input();
    assert!(!input::became_down(10));
    assert!(input::became_down(11));
}

#[test]
fn mouse_look_smooths_and_inverts() {
    let h = Fake::default();
    input::init(&h, 2, 0.5, 0, 0);
    input::tune(|s| s.invert_mouse = true);
    *h.mouse.borrow_mut() = (10, 20);
    input::update(&h, true);
    assert_eq!(take(&h), ["analog 0 [0.1, -0.2, 0.0]"]);
    *h.mouse.borrow_mut() = (0, 0);
    input::update(&h, true);
    let ev = take(&h);
    assert_eq!(ev.len(), 1, "{ev:?}");
    input::tune(|s| s.smooth_mouse = false);
    input::update(&h, true);
    assert!(take(&h).is_empty());
}

#[test]
fn gamepad_tables() {
    let h = Fake::default();
    input::init(&h, 7, 0.7, 0, 0);
    input::create_action(&h, Action::plain(b"Jump", 3));
    input::create_action(
        &h,
        Action {
            gamepad: true,
            ..Action::plain(b"JumpPad", 4)
        },
    );
    input::create_profile(
        b"pad",
        b"X",
        vec![b"A".to_vec(), b"B".to_vec()],
        vec![b"LS".to_vec()],
        vec![1],
    );
    input::add_preset(
        b"pad",
        Preset {
            name: b"main".to_vec(),
            actions: vec![3, 4],
            bindings: vec![b"Gamepad.Button.0".to_vec(), b"Gamepad.Button.1".to_vec()],
            analog: vec![false, true],
        },
    );
    input::set_mapping(b"pad", b"main");
    assert_eq!(input::mapping_action_num(), 2);
    assert_eq!(
        input::mapping_action(1).primary,
        Some((b"Gamepad.Button.1".to_vec(), true))
    );
    assert!(!input::mapping_action(5).found);
    let mut out = Vec::new();
    input::actions_for_control(b"pad", b"main", b"Gamepad.Button.1", &mut out);
    assert_eq!(out, b"JumpPad");
    assert_eq!(
        input::layout(b"Gamepad.Button.Button.1"),
        (b"X".to_vec(), b"B".to_vec())
    );
    assert_eq!(input::layout(b"Keyboard.W"), (Vec::new(), Vec::new()));
    input::dump_state(&h);
    let lines = take(&h);
    assert!(
        lines
            .iter()
            .any(|l| l == "in profile pad X [A,B,] [LS,] [1,]"),
        "{lines:?}"
    );
    assert!(
        lines
            .iter()
            .any(|l| l == "in control pad main Gamepad.Button.0 -> Jump"),
        "{lines:?}"
    );
}
