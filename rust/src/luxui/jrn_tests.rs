//! Journal object against a mock host: the content and save-data half of the A11c driver (`LuxA11cDump.cpp`)
//! is replayed and compared with the dump of the original C++ journal (`rust/tests/golden/A11c/state.txt`), then the
//! widget session is exercised (lists, pages, callbacks) with a recording host.

use super::invhost::{self as host, A, WStr, op};
use super::jrn;
use std::cell::RefCell;
use std::fmt::Write;
use std::sync::Mutex;

const STATE: &str = include_str!("../../tests/golden/A11c/state.txt");

static LOCK: Mutex<()> = Mutex::new(());

#[derive(Default)]
struct Mock {
    next: usize,
    log: Vec<(i32, i32, usize)>,
    visible: Vec<(usize, bool)>,
    sounds: Vec<Vec<u8>>,
    containers: Vec<Vec<u8>>,
    wide: Vec<u32>,
    bytes: Vec<u8>,
}

thread_local! {
    static M: RefCell<Mock> = RefCell::new(Mock::default());
}

fn bytes_of(s: super::event::Str) -> Vec<u8> {
    if s.p.is_null() || s.n == 0 {
        return Vec::new();
    }
    // SAFETY: the caller passes a live `n`-byte buffer.
    unsafe { std::slice::from_raw_parts(s.p, s.n) }.to_vec()
}

unsafe extern "C" fn mock(code: i32, a: *mut A) {
    // SAFETY: the journal passes a live argument block.
    let a = unsafe { &mut *a };
    let journal = code >= host::JOURNAL_BASE;
    let code = code % host::JOURNAL_BASE;
    M.with(|m| {
        let mut m = m.borrow_mut();
        m.log.push((code, a.i[0], a.p[0]));
        match code {
            op::TRANSLATE => {
                let key = bytes_of(a.s[1]);
                m.wide = key.iter().map(|&c| u32::from(c)).collect();
                a.rw = WStr::of(&m.wide);
            }
            op::GUI_INIT => {
                a.r = 1;
                a.f[..6].copy_from_slice(&[800.0, 600.0, 0.0, 0.0, 1920.0, 1080.0]);
            }
            op::WIDGET_NEW2 | op::GFX_NEW | op::FONT_LOAD => {
                m.next += 1;
                a.r = m.next;
            }
            op::FONT_WRAP => a.i[0] = 1,
            op::FONT_ROW => {
                m.wide = vec![u32::from(b'x')];
                a.rw = WStr::of(&m.wide);
            }
            op::ITEM_EVENT => {
                if a.i[0] == host::ev::SOUND {
                    m.sounds.push(bytes_of(a.s[0]));
                }
            }
            op::GAME => {
                if a.i[0] == host::game::SET_CONTAINER {
                    m.containers.push(bytes_of(a.s[0]));
                }
                if a.i[0] == host::game::MAP_FOLDER {
                    m.bytes = b"maps/".to_vec();
                }
            }
            op::JGAME => {
                if a.i[0] == host::jg::GAME_NAME {
                    m.bytes = b"Amnesia - The Dark Descent -".to_vec();
                    a.rs = super::event::Str::of(&m.bytes);
                }
            }
            op::WIDGET_X => {
                if a.i[0] == host::wx::SET_VISIBLE {
                    m.visible.push((a.p[0], a.i[1] != 0));
                }
                if a.i[0] == host::wx::IS_VISIBLE {
                    a.r = 1;
                }
            }
            _ => {}
        }
        let _ = journal;
    });
}

fn setup() -> std::sync::MutexGuard<'static, ()> {
    let g = LOCK
        .lock()
        .unwrap_or_else(std::sync::PoisonError::into_inner);
    M.with(|m| *m.borrow_mut() = Mock::default());
    host::set_host(Some(mock));
    jrn::create();
    g
}

fn teardown() {
    jrn::destroy();
    host::set_host(None);
}

fn show(b: &[u8]) -> String {
    String::from_utf8_lossy(b).into_owned()
}

fn snap(tag: &str, out: &mut String) {
    let (cat, entry, ty) = jrn::last_read();
    let _ = writeln!(
        out,
        "{tag} last={cat},{entry},{ty} notes={} conts={} quests={}",
        jrn::note_count(),
        jrn::cont_count(),
        jrn::quest_count()
    );
    for n in 0..jrn::note_count() {
        let f = |k| show(&jrn::note_field(n, k));
        let _ = writeln!(out, "  n[{n}] {} {} {} {}", f(0), f(1), f(2), f(3));
    }
    for c in 0..jrn::cont_count() {
        let _ = writeln!(
            out,
            "  c[{c}] {} {}",
            show(&jrn::cont_type(c)),
            jrn::diary_count(c)
        );
        for d in 0..jrn::diary_count(c) {
            let f = |k| show(&jrn::diary_field(c, d, k));
            let _ = writeln!(out, "    d[{d}] {} {} {} {}", f(0), f(1), f(2), f(3));
        }
    }
    for q in 0..jrn::quest_count() {
        let f = |k| show(&jrn::quest_field(q, k));
        let _ = writeln!(
            out,
            "  q[{q}] {} {} {} {}",
            i32::from(jrn::quest_active(q)),
            f(0),
            f(1),
            f(2)
        );
    }
}

fn restore(out: &mut String) {
    let (cat, entry, ty) = jrn::last_read();
    let notes: Vec<_> = (0..jrn::note_count())
        .map(|n| (0..4).map(|k| jrn::note_field(n, k)).collect::<Vec<_>>())
        .collect();
    let conts: Vec<_> = (0..jrn::cont_count())
        .map(|c| {
            (
                jrn::cont_type(c),
                (0..jrn::diary_count(c))
                    .map(|d| {
                        (0..4)
                            .map(|k| jrn::diary_field(c, d, k))
                            .collect::<Vec<_>>()
                    })
                    .collect::<Vec<_>>(),
            )
        })
        .collect();
    let quests: Vec<_> = (0..jrn::quest_count())
        .map(|q| {
            (
                jrn::quest_active(q),
                (0..3).map(|k| jrn::quest_field(q, k)).collect::<Vec<_>>(),
            )
        })
        .collect();
    jrn::reset();
    snap("reset", out);
    jrn::load_begin(cat, entry, ty);
    let entry_of = |f: &[Vec<u8>]| jrn::Entry {
        name: f[0].clone(),
        text: f[1].clone(),
        icon: f[2].clone(),
        image: f[3].clone(),
    };
    for n in &notes {
        jrn::load_note(entry_of(n));
    }
    for (ty, ds) in conts {
        jrn::load_container(ty);
        for d in &ds {
            jrn::load_diary(entry_of(d));
        }
    }
    for (active, f) in quests {
        jrn::load_quest(jrn::Quest {
            active,
            name: f[0].clone(),
            name_entry: f[1].clone(),
            text_entry: f[2].clone(),
        });
    }
    snap("restored", out);
}

#[test]
fn content_and_save_match_cpp_dump() {
    let _g = setup();
    let mut out = String::new();
    jrn::reset();
    jrn::add_note(b"CH01L03_VillageTales", b"papernote.tga");
    jrn::add_note(b"CH01L11_AccountOfHowling", b"letter.tga");
    jrn::add_note(b"CH01L03_WineCellar", b"old_note.tga");
    let diary = |name: &[u8]| jrn::add_diary(name, b"diary_page.tga").map_or(0, |d| d.1);
    diary(b"CH01L03_Daniel");
    let n = diary(b"CH01L03_Daniel");
    let _ = writeln!(out, "diary idx {n}");
    let n = diary(b"CH01L06_Daniel02");
    let _ = writeln!(out, "diary idx {n}");
    for (name, want) in [
        (&b"00Trail"[..], true),
        (b"00Trail", false),
        (b"02Web", true),
        (b"03Books", true),
    ] {
        let got = jrn::add_quest(name, name);
        assert_eq!(got, want);
        let _ = writeln!(
            out,
            "quest {} {}",
            if want { "add" } else { "again" },
            i32::from(got)
        );
    }
    let _ = writeln!(out, "quest off {}", i32::from(jrn::disable_quest(b"02Web")));
    let _ = writeln!(
        out,
        "quest off again {}",
        i32::from(jrn::disable_quest(b"02Web"))
    );
    snap("added", &mut out);
    restore(&mut out);
    jrn::open_diary_idx(0, 1, false);
    snap("final", &mut out);
    jrn::open_note_idx(1, false);
    let n = jrn::add_diary(b"CH01L03_Daniel", b"diary_page.tga").map_or(0, |d| d.1);
    let _ = writeln!(out, "diary idx {n}");
    jrn::add_quest(b"04FindChemicals", b"04FindChemicals");
    snap("end", &mut out);
    teardown();

    let first = out.lines().zip(STATE.lines()).position(|(a, b)| a != b);
    assert!(out == STATE, "first differing line {first:?}");
}

#[test]
fn quest_state_and_disable() {
    let _g = setup();
    jrn::reset();
    assert_eq!(jrn::quest_state(b"q"), -1);
    assert!(jrn::add_quest(b"q", b"q"));
    assert_eq!(jrn::quest_state(b"q"), 1);
    assert!(jrn::disable_quest(b"q"));
    assert_eq!(jrn::quest_state(b"q"), 0);
    assert!(!jrn::disable_quest(b"q"));
    assert!(!jrn::disable_quest(b"missing"));
    teardown();
}

#[test]
fn diary_numbers_follow_the_container() {
    let _g = setup();
    jrn::reset();
    assert_eq!(jrn::add_diary(b"a", b"x.tga").map(|d| d.1), Some(1));
    assert_eq!(jrn::add_diary(b"b", b"x.tga").map(|d| d.1), Some(1));
    assert_eq!(jrn::add_diary(b"a", b"x.tga").map(|d| d.1), Some(2));
    assert_eq!(jrn::diary_field(0, 1, 0), b"Diary_a_Name2");
    assert_eq!(jrn::diary_field(1, 0, 3), b"x_large.tga");
    teardown();
}

#[test]
fn last_read_follows_opens() {
    let _g = setup();
    jrn::reset();
    let n = jrn::add_note(b"n", b"i.tga");
    let d = jrn::add_diary(b"c", b"i.tga");
    jrn::add_diary(b"c", b"i.tga");
    let (Some(n), Some((d, _))) = (n, d) else {
        return;
    };
    jrn::open_note(n.to_bits(), false);
    assert_eq!(jrn::last_read(), (-1, 0, 0));
    jrn::open_diary(d.to_bits(), false);
    assert_eq!(jrn::last_read(), (0, 0, 1));
    jrn::open_diary_idx(0, 1, false);
    assert_eq!(jrn::last_read(), (0, 1, 1));
    teardown();
}

#[test]
fn enter_builds_the_session_and_leave_destroys_it() {
    let _g = setup();
    jrn::reset();
    jrn::add_note(b"n", b"i.tga");
    jrn::on_enter();
    let widgets = M.with(|m| m.borrow().next);
    assert!(widgets > 30, "{widgets}");
    jrn::on_leave();
    let destroyed = M.with(|m| {
        m.borrow()
            .log
            .iter()
            .filter(|l| l.0 == op::WIDGET_X && l.1 == host::wx::DESTROY)
            .count()
    });
    assert!(destroyed > 20, "{destroyed}");
    teardown();
}

#[test]
fn exit_walks_up_one_state_per_press() {
    let _g = setup();
    jrn::reset();
    jrn::add_note(b"n", b"i.tga");
    jrn::on_enter();
    jrn::open_note_idx(0, false);
    jrn::exit_pressed(false);
    jrn::exit_pressed(false);
    jrn::exit_pressed(false);
    let closed = M.with(|m| m.borrow().sounds.iter().any(|s| s == b"journal_close"));
    assert!(closed);
    jrn::update(5.0);
    let to_default = M.with(|m| m.borrow().containers.last().cloned());
    assert_eq!(to_default.as_deref(), Some(&b"Default"[..]));
    teardown();
}

#[test]
fn from_inventory_exit_returns_to_inventory() {
    let _g = setup();
    jrn::reset();
    jrn::set_opened_from_inventory(true);
    jrn::on_enter();
    jrn::exit_pressed(true);
    jrn::update(5.0);
    let last = M.with(|m| m.borrow().containers.last().cloned());
    assert_eq!(last.as_deref(), Some(&b"Inventory"[..]));
    teardown();
}
