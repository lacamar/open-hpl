//! Inventory object tests (A11a): the C++ baseline dump replayed through the Rust object with a mock host,
//! widget callbacks, draw order and the lifecycle.

use super::invffi as ffi;
use super::invgui as gui;
use super::invhost::{self as host, A, WStr, game, op, set, wd};
use super::invobj::{self as obj, bits};
use super::itemtype;
use crate::string::latin1_to_wide as widen;
use std::cell::RefCell;
use std::sync::Mutex;

// shared with jrn_tests: both install the one invhost::HOST
pub(super) static LOCK: Mutex<()> = Mutex::new(());

type Draw = (usize, [f32; 3], [f32; 2], [f32; 4], i32);

#[derive(Default)]
struct Mock {
    next: usize,
    calls: Vec<(i32, i32)>,
    draws: Vec<Draw>,
    texts: Vec<Vec<u32>>,
    events: Vec<(i32, i32, Vec<u8>)>,
    ret_w: Vec<u32>,
    ret_b: Vec<u8>,
    tinder: Option<Vec<u8>>,
    time: f64,
    triggered: [bool; 2],
    sizes: std::collections::HashMap<usize, [f32; 2]>,
    messages: Vec<Vec<u32>>,
    logs: Vec<Vec<u8>>,
    containers: Vec<Vec<u8>>,
    callbacks: Vec<(usize, i32, usize)>,
    user_values: std::collections::HashMap<usize, i32>,
    focused: usize,
    achievements: Vec<i32>,
}

thread_local! {
    static M: RefCell<Mock> = RefCell::new(Mock::default());
}

fn bytes_of(s: super::event::Str) -> Vec<u8> {
    if s.p.is_null() || s.n == 0 {
        return Vec::new();
    }
    // SAFETY: the caller's argument block points at live buffers.
    unsafe { std::slice::from_raw_parts(s.p, s.n).to_vec() }
}

fn wide_of(w: WStr) -> Vec<u32> {
    if w.p.is_null() || w.n == 0 {
        return Vec::new();
    }
    // SAFETY: the caller's argument block points at live buffers.
    unsafe { std::slice::from_raw_parts(w.p, w.n).to_vec() }
}

unsafe extern "C" fn mock(o: i32, a: *mut A) {
    // SAFETY: the caller passes a live block.
    let a = unsafe { &mut *a };
    M.with(|m| {
        let mut m = m.borrow_mut();
        m.calls.push((o, a.i[0]));
        a.rs = super::event::Str::NONE;
        a.rw = WStr::NONE;
        match o {
            op::CFG => {
                if a.i[0] == host::cfg::STRING {
                    m.ret_b = bytes_of(a.s[1]);
                    a.rs = super::event::Str::of(&m.ret_b);
                }
            }
            op::TRANSLATE => {
                m.ret_w = widen(&bytes_of(a.s[1]));
                a.rw = WStr::of(&m.ret_w);
            }
            op::PARSE_STRING => {
                m.ret_w = wide_of(a.w[0]);
                a.rw = WStr::of(&m.ret_w);
            }
            op::STRING_DURATION => a.f[0] = 2.0,
            op::GUI_INIT => {
                a.r = 7;
                a.f[..6].copy_from_slice(&[800.0, 600.0, 0.0, 0.0, 800.0, 600.0]);
            }
            op::GFX_NEW => {
                m.next += 1;
                a.r = 1000 + m.next;
                let id = a.r;
                m.sizes.insert(id, [32.0, 32.0]);
            }
            op::GFX_OP if a.i[0] == host::gfx::SIZES => {
                let s = m.sizes.get(&a.p[0]).copied().unwrap_or([16.0; 2]);
                a.f[..4].copy_from_slice(&[s[0], s[1], s[0], s[1]]);
            }
            op::DRAW_GFX => m.draws.push((
                a.p[0],
                [a.f[0], a.f[1], a.f[2]],
                [a.f[3], a.f[4]],
                [a.f[5], a.f[6], a.f[7], a.f[8]],
                a.i[0],
            )),
            op::DRAW_FONT => m.texts.push(wide_of(a.w[0])),
            op::FONT_LOAD => a.r = 55,
            op::FONT_WRAP => a.i[0] = 2,
            op::FONT_ROW => {
                m.ret_w = widen(format!("row{}", a.i[0]).as_bytes());
                a.rw = WStr::of(&m.ret_w);
            }
            op::WIDGET_NEW => {
                m.next += 1;
                a.r = 2000 + m.next;
                let id = a.r;
                m.sizes.insert(id, [64.0, 64.0]);
            }
            op::WIDGET_CB => m.callbacks.push((a.p[0], a.i[0], a.p[1])),
            op::WIDGET_GET => match a.i[0] {
                wd::GET_SIZE => {
                    let s = m.sizes.get(&a.p[0]).copied().unwrap_or([64.0; 2]);
                    a.f[..2].copy_from_slice(&s);
                }
                wd::GET_GLOBAL => a.f[..3].copy_from_slice(&[100.0, 200.0, 3.0]),
                wd::GET_USER_VALUE => {
                    a.r = m.user_values.get(&a.p[0]).copied().unwrap_or(0) as usize
                }
                wd::GET_COLOR_MUL => a.f[..4].copy_from_slice(&[1.0; 4]),
                _ => {}
            },
            op::WIDGET_SET => {
                if a.i[0] == wd::SET_USER_VALUE {
                    m.user_values.insert(a.p[0], a.i[1]);
                }
            }
            op::SET_OP => match a.i[0] {
                set::MOUSE_POS => a.f[..2].copy_from_slice(&[150.0, 250.0]),
                set::FOCUSED_WIDGET => m.focused = a.p[0],
                set::GET_FOCUSED => a.r = m.focused,
                _ => {}
            },
            op::LL => {
                if a.i[0] == host::ll::SCREEN_SIZE_INT {
                    a.i[1] = 800;
                    a.i[2] = 600;
                }
                m.next += 1;
                a.r = 3000 + m.next;
            }
            op::SCRIPT => {
                if a.i[0] == 0 {
                    a.r = 9;
                }
            }
            op::GAME => match a.i[0] {
                game::TRIGGERED => a.r = usize::from(m.triggered[a.i[1] as usize]),
                game::GAME_TIME => a.d = m.time,
                game::TINDER_GET => {
                    a.i[1] = i32::from(m.tinder.is_some());
                    if let Some(t) = m.tinder.clone() {
                        m.ret_b = t;
                        a.rs = super::event::Str::of(&m.ret_b);
                    }
                }
                game::TINDER_SET => m.tinder = Some(bytes_of(a.s[0])),
                game::SET_MESSAGE => m.messages.push(wide_of(a.w[0])),
                game::SET_CONTAINER => m.containers.push(bytes_of(a.s[0])),
                game::ACHIEVEMENT => m.achievements.push(a.i[1]),
                game::MAP_FOLDER => {
                    m.ret_b = b"maps/main/".to_vec();
                    a.rs = super::event::Str::of(&m.ret_b);
                }
                game::HEALTH => a.f[0] = 40.0,
                game::SANITY => a.f[0] = 80.0,
                game::LAMP_OIL => a.f[0] = 50.0,
                _ => {}
            },
            op::ITEM_EVENT => m.events.push((a.i[0], a.i[1], bytes_of(a.s[0]))),
            op::LOG => m.logs.push(bytes_of(a.s[0])),
            _ => {}
        }
    });
}

fn setup() -> std::sync::MutexGuard<'static, ()> {
    let g = LOCK
        .lock()
        .unwrap_or_else(std::sync::PoisonError::into_inner);
    M.with(|m| *m.borrow_mut() = Mock::default());
    host::set_host(Some(mock));
    ffi::ohpl_inv_new();
    super::jrn::create();
    g
}

fn teardown() {
    super::jrn::destroy();
    ffi::ohpl_inv_free();
    host::set_host(None);
}

fn st(s: &str) -> super::event::Str {
    super::event::Str::of(s.as_bytes())
}

fn add(name: &str, ty: i32, sub: &str, amount: f32, val: &str, extra: &str) -> (usize, i32) {
    let mut rm = 0;
    // SAFETY: `rm` is writable.
    let h = unsafe {
        ffi::ohpl_inv_add_item(
            st(name),
            ty,
            st(sub),
            st("agrippa_head.tga"),
            amount,
            st(val),
            st(extra),
            &mut rm,
        )
    };
    (h, rm)
}

fn dump_items(out: &mut String, tag: &str, equip: bool) {
    let i = obj::g().expect("inventory");
    out.push_str(&format!(
        "{tag} n={} eq={} dis={} msg={} picked={}\n",
        i.order.len(),
        if equip { i.index_of(i.equipped) } else { -2 },
        i32::from(i.disabled),
        i32::from(i.st.message_active != 0),
        i32::from(i.picked.is_some())
    ));
    for (n, h) in i.order.iter().enumerate() {
        let it = i.items.get(*h).expect("item");
        let s = |b: &[u8]| String::from_utf8_lossy(b).into_owned();
        out.push_str(&format!(
            "  [{n}] name={} type={} sub={} val={} extra={} img={} cnt={} amt={} ne={} de={} gfx={}\n",
            s(&it.name),
            it.ty,
            s(&it.sub),
            s(&it.val),
            s(&it.extra),
            s(&it.image_name),
            it.count,
            it.amount,
            s(&it.name_entry),
            s(&it.desc_entry),
            i32::from(it.image != 0)
        ));
    }
}

/// The sequence of `OhplA11aInvDump` (rust/tests/golden/A11a/cpp-baseline-hook.patch), the part that does not go
/// through the save-data classes.
fn replay_dump() -> String {
    let mut out = String::new();
    for (n, want) in [
        "Puzzle",
        "coins",
        "NOTE",
        "Diary",
        "Lantern",
        "Health",
        "Sanity",
        "LampOil",
        "Tinderbox",
        "HandObject",
    ]
    .iter()
    .enumerate()
    {
        let t = ffi::ohpl_inv_type_from_string(st(want));
        let info = obj::info_of(t).expect("type");
        out.push_str(&format!(
            "type {want} -> {t} name={} count={} max={} pickup={} hasmax={}\n",
            String::from_utf8_lossy(info.name),
            i32::from(info.has_count),
            info.max_count,
            i32::from(info.show_pickup),
            i32::from(itemtype::has_max_amount(t))
        ));
        assert_eq!(t, n as i32);
    }
    obj::reset();
    M.with(|m| m.borrow_mut().tinder = None);
    let adds: [(&str, i32, &str, f32, &str, &str); 12] = [
        ("puz_a", itemtype::PUZZLE, "agrippa_head", 0.0, "", ""),
        (
            "puz_b",
            itemtype::PUZZLE,
            "bone_saw",
            1.0,
            "L02_Shipment",
            "x",
        ),
        ("hp_a", itemtype::HEALTH, "health", 25.0, "", ""),
        ("hp_b", itemtype::HEALTH, "health", 25.0, "", ""),
        ("san_a", itemtype::SANITY, "sanity", 30.0, "", ""),
        ("oil_a", itemtype::LAMP_OIL, "oil", 20.0, "", ""),
        ("oil_b", itemtype::LAMP_OIL, "oil", 20.0, "", ""),
        ("lantern", itemtype::LANTERN, "lantern", 0.0, "", ""),
        (
            "hand_a",
            itemtype::HAND_OBJECT,
            "bucket_of_tar",
            0.0,
            "",
            "",
        ),
        ("hand_b", itemtype::HAND_OBJECT, "bone_saw", 0.0, "", ""),
        ("tb_a", itemtype::TINDERBOX, "tinderbox", 0.0, "", ""),
        ("coins", itemtype::COINS, "coins_large", 10.0, "", ""),
    ];
    for (name, ty, sub, amount, val, extra) in adds {
        let (h, rm) = add(name, ty, sub, amount, val, extra);
        let idx = ffi::ohpl_inv_item_index(h);
        out.push_str(&format!(
            "add {name} -> {} remove={rm} idx={idx}\n",
            i32::from(h != 0)
        ));
        dump_items(&mut out, " state", true);
    }
    for _ in 0..100 {
        add("hp_x", itemtype::HEALTH, "health", 25.0, "", "");
    }
    dump_items(&mut out, "capped", true);
    let has = |t| ffi::ohpl_inv_has_of_type(t);
    out.push_str(&format!(
        "has lantern={} oil={} tinder={} note={}\n",
        has(itemtype::LANTERN),
        has(itemtype::LAMP_OIL),
        has(itemtype::TINDERBOX),
        has(itemtype::NOTE)
    ));
    out.push_str(&format!(
        "get puz_b={} sub bone_saw={} nothing={}\n",
        ffi::ohpl_inv_item_index(ffi::ohpl_inv_get_by_name(st("puz_b"))),
        ffi::ohpl_inv_item_index(ffi::ohpl_inv_get_by_subtype(st("bone_saw"))),
        i32::from(ffi::ohpl_inv_get_by_name(st("nothing")) != 0)
    ));
    ffi::ohpl_inv_set_equipped(ffi::ohpl_inv_get_by_name(st("hand_b")));
    ffi::ohpl_inv_add_combine(st("cb1"), st("puz_a"), st("puz_b"), st("CombineFn"), 1);
    ffi::ohpl_inv_add_combine(st("cb2"), st("hp_a"), st("oil_a"), st("OtherFn"), 0);
    ffi::ohpl_inv_remove_combine(st("cb2"));
    ffi::ohpl_inv_set_disabled(1);
    for (a, b) in [
        ("puz_a", "puz_b"),
        ("puz_b", "puz_a"),
        ("hp_a", "oil_a"),
        ("puz_a", "puz_a"),
    ] {
        let c = ffi::ohpl_inv_find_combine(st(a), st(b));
        let i = obj::g().expect("inventory");
        let label = usize::try_from(c)
            .ok()
            .and_then(|n| i.combine.get(n))
            .map_or("none".to_owned(), |cb| {
                format!(
                    "{},{}",
                    String::from_utf8_lossy(&cb.name),
                    String::from_utf8_lossy(&cb.func)
                )
            });
        out.push_str(&format!("comb {a},{b} -> {label}\n"));
    }
    out
}

#[test]
fn baseline_dump_replays() {
    let _g = setup();
    let got = replay_dump();
    let want = include_str!("../../tests/golden/A11a/inv-dump.txt");
    let head: Vec<&str> = want.lines().take(got.lines().count()).collect();
    for (n, (a, b)) in got.lines().zip(head.iter()).enumerate() {
        assert_eq!(a, *b, "line {n}");
    }
    assert_eq!(got.lines().count(), head.len());
    teardown();
}

#[test]
fn removal_semantics() {
    let _g = setup();
    let (a, _) = add("hp_a", itemtype::HEALTH, "health", 25.0, "", "");
    add("hp_b", itemtype::HEALTH, "health", 25.0, "", "");
    assert_eq!(ffi::ohpl_inv_item_count(a), 2);
    ffi::ohpl_inv_remove(a);
    assert_eq!(ffi::ohpl_inv_item_count(a), 1);
    ffi::ohpl_inv_remove(a);
    assert_eq!(ffi::ohpl_inv_item_num(), 0);
    ffi::ohpl_inv_remove(a);
    ffi::ohpl_inv_remove_by_name(st("missing"));
    assert!(M.with(|m| {
        m.borrow()
            .logs
            .iter()
            .any(|l| l.starts_with(b"Item 'missing'"))
    }));
    ffi::ohpl_inv_remove_combine(st("none"));
    teardown();
}

#[test]
fn item_cap_message() {
    let _g = setup();
    for _ in 0..99 {
        add("hp", itemtype::HEALTH, "health", 25.0, "", "");
    }
    let (h, rm) = add("hp", itemtype::HEALTH, "health", 25.0, "", "");
    assert_eq!((h, rm), (0, 0));
    assert_eq!(M.with(|m| m.borrow().messages.len()), 1);
    teardown();
}

#[test]
fn stale_handle_is_null() {
    let _g = setup();
    let (a, _) = add("p", itemtype::PUZZLE, "p", 0.0, "", "");
    ffi::ohpl_inv_remove(a);
    assert_eq!(ffi::ohpl_inv_item_type(a), 0);
    assert_eq!(ffi::ohpl_inv_item_index(a), -1);
    assert_eq!(bits(None), 0);
    teardown();
}

#[test]
fn tinderbox_counts_and_unlocks() {
    let _g = setup();
    M.with(|m| m.borrow_mut().tinder = Some(b"148".to_vec()));
    add("tb", itemtype::TINDERBOX, "tb", 0.0, "", "");
    assert_eq!(M.with(|m| m.borrow().tinder.clone()), Some(b"149".to_vec()));
    assert!(M.with(|m| m.borrow().achievements.is_empty()));
    add("tb", itemtype::TINDERBOX, "tb", 0.0, "", "");
    assert_eq!(M.with(|m| m.borrow().achievements.clone()), vec![0]);
    teardown();
}

#[test]
fn gui_creation_registers_callbacks_per_slot() {
    let _g = setup();
    let (slots, cbs) = M.with(|m| {
        let m = m.borrow();
        (
            m.calls.iter().filter(|c| c.0 == op::WIDGET_NEW).count(),
            m.callbacks.len(),
        )
    });
    assert_eq!(slots, 5 + 1 + 18);
    assert_eq!(cbs, (5 + 4 + 5 + 4 + 6) + 5 * 2 + 1 + 18 * 12);
    assert_eq!(obj::g().expect("inventory").gui.slots.len(), 18);
    teardown();
}

#[test]
fn slot_double_click_uses_health_and_removes() {
    let _g = setup();
    let (h, _) = add("hp", itemtype::HEALTH, "health", 25.0, "", "");
    assert_ne!(h, 0);
    let slot0 = obj::g().expect("inventory").gui.slots[0].widget;
    let tok = M.with(|m| {
        m.borrow()
            .callbacks
            .iter()
            .find(|c| c.0 == slot0 && c.1 == 2)
            .map(|c| c.2)
            .expect("double click callback")
    });
    assert_eq!(ffi::ohpl_inv_cb(tok, slot0, 0, 0.0), 1);
    let ev = M.with(|m| m.borrow().events.clone());
    assert!(ev.iter().any(|e| e.0 == host::ev::SET_HEALTH));
    assert!(
        ev.iter()
            .any(|e| e.0 == host::ev::SOUND && e.2 == b"ui_use_health")
    );
    assert_eq!(ffi::ohpl_inv_item_num(), 0);
    teardown();
}

#[test]
fn combine_without_callback_shows_message() {
    let _g = setup();
    add("a", itemtype::PUZZLE, "a", 0.0, "", "");
    let (b, _) = add("b", itemtype::PUZZLE, "b", 0.0, "", "");
    obj::set_picked(super::invobj::from_bits(b), [1.0, 2.0]);
    let slot0 = obj::g().expect("inventory").gui.slots[0].widget;
    let tok = M
        .with(|m| {
            m.borrow()
                .callbacks
                .iter()
                .find(|c| c.0 == slot0 && c.1 == 1)
                .map(|c| c.2)
        })
        .expect("up");
    ffi::ohpl_inv_cb(tok, slot0, 0, 0.0);
    assert_eq!(ffi::ohpl_inv_message_active(), 1);
    teardown();
}

#[test]
fn draw_order_of_main_screen() {
    let _g = setup();
    ffi::ohpl_inv_on_draw(0.016);
    let n = M.with(|m| m.borrow().draws.len());
    assert!(n >= 8, "frame draws {n}");
    teardown();
}

#[test]
fn enter_and_leave_container_flow() {
    let _g = setup();
    ffi::ohpl_inv_on_enter();
    let i = obj::g().expect("inventory");
    assert_eq!(i.st.active, 1);
    assert_ne!(i.current_widget, 0);
    assert_ne!(i.gui.bg_gfx, 0);
    ffi::ohpl_inv_exit_pressed();
    assert_eq!(obj::g().expect("inventory").st.active, 0);
    ffi::ohpl_inv_update(0.1);
    ffi::ohpl_inv_on_leave();
    assert_eq!(obj::g().expect("inventory").gui.bg_gfx, 0);
    teardown();
}

#[test]
fn gui_token_layout() {
    assert_eq!(std::mem::size_of::<A>(), 208);
    assert_eq!(gui::MAT_DEFAULT, -1);
}

#[test]
fn host_constants_match_cpp() {
    let cpp = include_str!("../../../amnesia/src/game/LuxInventoryHost.cpp");
    for (group, tbl) in host::TABLES {
        for (name, v) in *tbl {
            let key = format!("kA11a_{group}_{name} =");
            if cpp.contains(&key) {
                assert!(cpp.contains(&format!("{key} {v}")), "{key} {v}");
            }
        }
    }
}
