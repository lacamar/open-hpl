use super::event::Out;
use super::hint::{self, HintState};
use super::inventory::{self as inv, InvState};
use super::itemtype::{self, Item, Player};
use super::journal::{self as jrn, Pages};
use super::keyentry::{self, KeyAct, Route};
use super::message::{self as msg, MessageState};
use super::{commentary, event};

fn w(s: &str) -> Vec<u32> {
    s.chars().map(|c| c as u32).collect()
}

fn player() -> Player {
    Player {
        coins: 5,
        health: 40.0,
        sanity: 100.0,
        lamp_oil: 95.0,
        has_lantern: true,
        lantern_active: false,
        use_item_state: false,
    }
}

fn item(amount: f32) -> Item<'static> {
    Item {
        name: b"potion_1",
        sval: b"Note1",
        extra: b"OnRead",
        amount,
    }
}

#[test]
fn key_chars_and_paste() {
    assert!(keyentry::verify_char(b'a'));
    assert!(keyentry::verify_char(b'Z'));
    assert!(!keyentry::verify_char(b'0'));
    assert!(!keyentry::verify_char(b'O'));
    assert!(!keyentry::verify_char(0xe9));
    assert_eq!(
        keyentry::paste_filter(b"ab0-cdOzzzzzzzzzzzzzzzzzzzzzz"),
        b"ABCDZZZZZZZZZZZZZ"
    );
    assert_eq!(keyentry::route(true, false, true), Route::Base);
    assert_eq!(keyentry::route(true, false, false), Route::Key);
    assert_eq!(keyentry::route(false, true, false), Route::Paste);
}

#[test]
fn key_action_bounds() {
    assert_eq!(keyentry::key_action(false, 0, 0, 0, b'a'), KeyAct::Base);
    assert_eq!(
        keyentry::key_action(true, 3, 5, 2, b'q'),
        KeyAct::Insert {
            pos: 2,
            epos: 5,
            ch: b'Q'
        }
    );
    assert_eq!(
        keyentry::key_action(true, 16, 16, 16, b'a'),
        KeyAct::Swallow
    );
    assert_eq!(keyentry::key_action(true, 4, 4, 4, b'0'), KeyAct::Swallow);
    assert_eq!(
        keyentry::key_action(true, 16, 3, 4, b'a'),
        KeyAct::Insert {
            pos: 3,
            epos: 4,
            ch: b'A'
        }
    );
}

#[test]
fn item_table() {
    assert_eq!(itemtype::from_string(b"LAMPoil"), Some(itemtype::LAMP_OIL));
    assert_eq!(itemtype::from_string(b"nope"), None);
    let t = itemtype::type_info(itemtype::TINDERBOX);
    assert!(t.is_some_and(|t| t.has_count && t.max_count == 99));
    assert!(itemtype::type_info(itemtype::NOTE).is_some_and(|t| !t.show_pickup));
    assert!(itemtype::type_info(itemtype::TYPE_COUNT).is_none());
}

#[test]
fn item_use_events() {
    let mut o = Out::default();
    itemtype::on_use(itemtype::HEALTH, &item(70.0), &player(), &mut o);
    let kinds: Vec<u32> = o.evs.iter().map(|e| e.kind).collect();
    assert_eq!(
        kinds,
        [
            itemtype::EV_SET_HEALTH,
            itemtype::EV_REMOVE_ITEM,
            itemtype::EV_SOUND
        ]
    );
    assert_eq!(o.evs[0].v[0], 100.0);

    let mut o = Out::default();
    let mut p = player();
    p.has_lantern = false;
    itemtype::on_use(itemtype::LAMP_OIL, &item(5.0), &p, &mut o);
    assert_eq!(o.evs.len(), 1);
    assert_eq!(o.evs[0].kind, itemtype::EV_MESSAGE_KEY);

    let mut o = Out::default();
    itemtype::on_use(itemtype::SANITY, &item(5.0), &player(), &mut o);
    assert!(o.evs.is_empty());
}

#[test]
fn item_pickup_events() {
    let mut o = Out::default();
    assert!(itemtype::before_add(
        itemtype::COINS,
        &item(7.9),
        &player(),
        &mut o
    ));
    assert_eq!(o.evs[0].a, 12);
    let mut o = Out::default();
    assert!(!itemtype::before_add(
        itemtype::PUZZLE,
        &item(1.0),
        &player(),
        &mut o
    ));
    assert_eq!(o.evs.len(), 2);
    let mut o = Out::default();
    assert!(itemtype::note_before_add(false, &item(2.0), &mut o));
    assert_eq!(o.evs.iter().map(|e| e.kind).collect::<Vec<_>>().len(), 5);
    assert_eq!(o.evs[3].a, 1);
    let mut o = Out::default();
    assert!(itemtype::diary_before_add(false, 3, &item(1.0), &mut o));
    assert_eq!(o.evs.len(), 2);
}

#[test]
fn hint_timeline() {
    let mut s = HintState::default();
    hint::init(&mut s, 10.0, [8.0, 9.0], 2.0);
    s.active = 1;
    assert!(hint::can_add(&s, false, false));
    assert!(!hint::can_add(&s, true, false));
    hint::start(&mut s, 0.0, 3.0);
    assert_eq!(s.show_count, 6.0);
    assert!(!hint::can_add(&s, false, false));
    for _ in 0..10 {
        hint::update(&mut s, 0.1);
    }
    assert_eq!(s.alpha, 1.0);
    assert!(s.show_count < 5.1);
    s.show_count = 0.0;
    hint::update(&mut s, 0.5);
    assert!((s.alpha - 0.75).abs() < 1e-6);
    assert!(hint::set_active(&mut s, false));
    assert_eq!(s.alpha, 0.0);
    assert_eq!(hint::row_pos(&s, 1, 0)[0], 400.0);
    assert_eq!(hint::row_pos(&s, 2, 1)[0], 60.0);
}

#[test]
fn icons_are_stripped() {
    let t = w("Press $GamepadButton.A$ now $Hat.0.Up");
    let mut calls = Vec::new();
    let out = hint::strip_icons(&t, &mut |c, p| {
        calls.push((c.to_vec(), p));
        w("  ")
    });
    assert_eq!(out, w("Press    now "));
    assert_eq!(calls.len(), 1);
    assert_eq!(calls[0].1, 6);
}

#[test]
fn message_quest_sound_once() {
    let mut s = MessageState::default();
    msg::reset(&mut s);
    msg::start_quest_added(&mut s);
    let mut sounds = 0;
    for _ in 0..120 {
        let mut o = Out::default();
        msg::update(&mut s, 0.05, &mut o);
        sounds += o
            .evs
            .iter()
            .filter(|e| e.kind == msg::EV_QUEST_SOUND)
            .count();
    }
    assert_eq!(sounds, 1);
    assert_eq!(s.quest_active, 0);
    assert_eq!(s.quest_alpha, 0.0);
}

#[test]
fn message_pause_and_yes_no() {
    let mut s = MessageState {
        font_size: [10.0, 12.0],
        ..Default::default()
    };
    msg::reset(&mut s);
    let mut o = Out::default();
    msg::start_pause_message(&mut s, true, &mut o);
    assert_eq!(o.evs[0].a, 0);
    msg::start_pause_message(&mut s, true, &mut o);
    assert_eq!(o.evs.len(), 1);
    let mut o = Out::default();
    msg::do_action(&mut s, msg::ACTION_ATTACK, true, true, &mut o);
    assert_eq!(o.evs.len(), 2);
    assert_eq!((o.evs[1].kind, o.evs[1].a), (msg::EV_CALLBACK, 0));
    msg::update(&mut s, 1.0, &mut Out::default());
    assert_eq!(s.pause_alpha, 0.0);
    s.pause_alpha = 1.0;
    s.yes_no = 0;
    let mut o = Out::default();
    msg::draw_pause(&s, 2, &mut o);
    assert_eq!(o.evs.len(), 1 + 2);
    s.yes_no = 1;
    let mut o = Out::default();
    msg::draw_pause(&s, 2, &mut o);
    assert_eq!(o.evs.len(), 1 + 2 + 4);
}

#[test]
fn commentary_spin() {
    let y = commentary::spin(0.0, true, 0.2);
    assert!((y - 0.5).abs() < 1e-6);
    assert_eq!(commentary::spin(0.0, false, 0.2), 0.0);
    assert_eq!(commentary::spin(3.1, false, 1.0), 0.0);
    assert_eq!(commentary::set_playing(true, true), commentary::PS_NONE);
    assert_eq!(commentary::set_playing(false, true), commentary::PS_CREATE);
    assert_eq!(commentary::focus_text(b"Tom", b"Hi"), b"Tom\n\"Hi\"");
    assert_eq!(commentary::comment_file(b"a/b.cdf"), b"a/b.comment");
}

#[test]
fn inventory_fades_and_exit() {
    let mut s = InvState::default();
    inv::init(&mut s, 0.5, 0.5, [[0.0; 3]; 3]);
    inv::reset(&mut s);
    s.active = 1;
    for _ in 0..20 {
        assert_eq!(inv::update_alpha(&mut s, 0.1), inv::EXIT_NONE);
    }
    assert_eq!(s.alpha, 1.0);
    s.enter_from_journal = 1;
    s.message_active = 1;
    assert_eq!(inv::exit_pressed(&mut s), inv::PRESS_CLEAR_MESSAGE);
    assert_eq!(inv::open_journal(&mut s), inv::PRESS_EXIT);
    assert_eq!(s.exit_to_journal, 1);
    let mut exit = 0;
    for _ in 0..20 {
        exit = inv::update_alpha(&mut s, 0.1);
    }
    assert_eq!(exit, inv::EXIT_TO_JOURNAL);
    assert!(!inv::leave(&mut s));
    assert!(inv::leave(&mut s));
}

#[test]
fn inventory_add_plans() {
    let plan = |ty, existing, count| {
        inv::add_plan(&inv::AddIn {
            ty,
            subtype: b"x",
            existing,
            existing_count: count,
        })
    };
    assert_eq!(plan(itemtype::HEALTH, true, 99), inv::ADD_REJECT);
    assert_eq!(plan(itemtype::HEALTH, true, 3), inv::ADD_COUNT);
    assert_eq!(plan(itemtype::HEALTH, false, 0), inv::ADD_CREATE);
    assert_eq!(plan(itemtype::PUZZLE, true, 9), inv::ADD_CREATE);
    assert_eq!(inv::tinderbox_count(b"").0, b"0");
    assert_eq!(inv::tinderbox_count(b"149"), (b"150".to_vec(), true));
    assert!(inv::remove_one(true, 0));
    assert!(!inv::remove_one(true, 2));
    assert_eq!(
        inv::special_combine(itemtype::LANTERN, itemtype::LAMP_OIL),
        2
    );
    assert_eq!(
        inv::special_combine(itemtype::LAMP_OIL, itemtype::LANTERN),
        1
    );
    assert_eq!(
        inv::special_combine(itemtype::LAMP_OIL, itemtype::LAMP_OIL),
        0
    );
    assert!(inv::combine_matches(b"a", b"b", b"b", b"a"));
    assert_eq!(
        inv::run_script_command(b"f", b"a", b"b"),
        b"f(\"a\", \"b\")"
    );
}

#[test]
fn inventory_hover_rules() {
    assert_eq!(
        inv::mouse_over(9, false, true, false, false),
        inv::MO_INVALID
    );
    assert_eq!(inv::mouse_over(1, false, true, false, false), inv::MO_SHOW);
    assert_eq!(
        inv::mouse_over(1, false, false, true, true),
        inv::MO_HIDE_AND_CLEAR
    );
    assert_eq!(inv::mouse_over(1, true, true, true, false), inv::MO_HIDE);
    assert_eq!(inv::ui_button_nav(inv::BUTTON_NEXT_PAGE, 0), inv::NAV_NONE);
    assert_eq!(inv::ui_button_nav(inv::BUTTON_NEXT_PAGE, 3), inv::NAV_OIL);
    assert_eq!(
        inv::ui_button_nav(inv::BUTTON_PREV_PAGE, 2),
        inv::NAV_SANITY
    );
    assert_eq!(
        inv::ui_button_nav(inv::BUTTON_PREV_PAGE, 7),
        inv::NAV_HEALTH
    );
    assert_eq!(
        inv::slot_mouse_enter(true, false, true, false),
        inv::ENTER_COMBINE
    );
    assert_eq!(
        inv::slot_mouse_enter(false, false, false, false),
        inv::ENTER_CLEAR
    );
    assert_eq!(inv::status_index(74.9), 2);
    let mut s = InvState::default();
    inv::set_picked(&mut s, 1.0);
    assert_eq!(
        inv::slot_ui_release(&s, 1, true, 1.2),
        inv::RELEASE_DOUBLE_CLICK
    );
    inv::note_current_widget(&mut s, 2.0);
    assert_eq!(
        inv::slot_ui_release(&s, 1, true, 1.2),
        inv::RELEASE_MOUSE_UP
    );
    assert_eq!(inv::slot_ui_release(&s, 4, true, 1.2), inv::RELEASE_IGNORE);
}

#[test]
fn inventory_frame_layout() {
    let mut o = Out::default();
    inv::draw_frame(
        [10.0, 20.0, 1.0],
        [100.0, 50.0],
        [8.0, 9.0],
        [7.0, 6.0],
        [5.0, 4.0],
        0.5,
        &mut o,
    );
    assert_eq!(o.evs.len(), 8);
    assert_eq!(o.evs[0].v[..3], [10.0, 20.0, 1.0]);
    assert_eq!(o.evs[4].v[..5], [18.0, 20.0 + 9.0 + 50.0, 1.0, 100.0, 4.0]);
    assert_eq!(o.evs[5].v[0], 118.0);
    let mut o = Out::default();
    let g = inv::GridIn {
        widget_size: [100.0, 50.0],
        h_ext: [1.0, 2.0],
        v_ext: [3.0, 4.0],
        separation: [5.0, 6.0, 0.0],
        slot_size: [20.0, 10.0],
        start: [0.0, 0.0, 0.0],
        frame_hpad: [0.0; 2],
        frame_vpad: [0.0; 2],
        frame_offset: [0.0; 3],
        grid_offset: [0.0; 3],
        color: [1.0, 1.0, 1.0, 0.5],
        rows: 3,
        cols: 4,
        alpha: 0.5,
    };
    inv::draw_grid(&g, &mut o);
    assert_eq!(o.evs.len(), 2 + 3);
    assert_eq!(o.evs[0].v[8], 0.25);
}

#[test]
fn oil_bubbles_restart_when_all_out() {
    let mut s = InvState::default();
    inv::init(&mut s, 1.0, 1.0, [[0.0, -100.0, 0.0]; 3]);
    assert!(inv::oil_update(&mut s, [0.0; 3], [10.0, 100.0], 0.0, 0.1));
    assert!(!inv::oil_update(
        &mut s,
        [5.0, 50.0, 0.0],
        [10.0, 100.0],
        50.0,
        0.1
    ));
    assert_eq!(s.bubble_pos[0][1], 150.0);
    assert!(s.bubble_pos[1][0] >= 5.0 && s.bubble_pos[1][0] <= 15.0);
    assert!(s.bubble_speed[2] >= 25.0 && s.bubble_speed[2] <= 62.5);
}

fn rows(v: &[&str]) -> Vec<Vec<u32>> {
    v.iter().map(|r| w(r)).collect()
}

#[test]
fn journal_text_pages() {
    let mut p = Pages::default();
    p.load_text(
        &rows(&["a", "b", "[new_page]", "c", "[voice x]", "d", "e", "f"]),
        2,
    );
    let got: Vec<Vec<String>> = p
        .pages
        .iter()
        .map(|pg| {
            pg.rows
                .iter()
                .map(|r| r.iter().filter_map(|&c| char::from_u32(c)).collect())
                .collect()
        })
        .collect();
    assert_eq!(
        got,
        [vec!["a", "b"], vec!["c", ""], vec!["d", "e"], vec!["f"],]
    );
}

#[test]
fn journal_narration_pages() {
    let mut p = Pages::default();
    let errors = p.load_narration(&rows(&[
        "[voice a.ogg]",
        "one",
        "[voice ",
        "b.ogg]",
        "two",
        "[voice a b]",
        "[new_page]",
        "tail",
    ]));
    assert_eq!(p.pages.len(), 3);
    assert_eq!(p.pages[0].voice, b"a.ogg");
    assert_eq!(p.pages[0].rows.len(), 1);
    assert_eq!(p.pages[1].voice, b"b.ogg");
    assert!(p.pages[2].voice.is_empty());
    assert_eq!(p.pages[2].rows.len(), 1);
    assert_eq!(errors.len(), 1);
    assert_eq!(errors[0].1, 3);
}

#[test]
fn journal_navigation() {
    assert_eq!(
        jrn::exit_pressed(false, true, false, 0, 0, 0).0,
        jrn::PRESS_NONE
    );
    assert_eq!(
        jrn::exit_pressed(true, false, false, jrn::STATE_OPEN_NARRATED, 0, 3),
        (jrn::PRESS_NEXT_PAGE, 1)
    );
    assert_eq!(
        jrn::exit_pressed(true, false, false, jrn::STATE_OPEN_NARRATED, 2, 3).0,
        jrn::PRESS_CHANGE_STATE
    );
    assert_eq!(
        jrn::exit_pressed(true, true, false, jrn::STATE_NOTES, 0, 0).0,
        jrn::PRESS_EXIT_HINT
    );
    assert_eq!(
        jrn::exit_pressed(true, false, false, jrn::STATE_MAIN, 0, 0).0,
        jrn::PRESS_EXIT
    );
    assert_eq!(
        jrn::exit_pressed(true, false, false, jrn::STATE_OPEN_DIARY, 0, 0),
        (jrn::PRESS_CHANGE_STATE, jrn::STATE_DIARIES)
    );
    assert_eq!(jrn::note_arrows(0, 1), (false, false));
    assert_eq!(jrn::note_arrows(1, 3), (true, true));
    assert_eq!(
        jrn::ui_button(jrn::STATE_OPEN_NOTE, jrn::BUTTON_SECONDARY, false, true),
        Some(jrn::BTN_EXIT)
    );
    assert_eq!(
        jrn::ui_button(jrn::STATE_NOTES, jrn::BUTTON_SECONDARY, false, true),
        None
    );
    assert_eq!(
        jrn::ui_arrow(jrn::STATE_OPEN_NOTE, jrn::ARROW_DOWN, false, false, true),
        Some(jrn::BTN_EXIT)
    );
    assert_eq!(
        jrn::ui_arrow(jrn::STATE_OPEN_NOTE, jrn::ARROW_DOWN, false, false, false),
        None
    );
}

#[test]
fn journal_lists() {
    let (v, pages) = jrn::layout_list(14);
    assert_eq!(pages, 2);
    assert_eq!(v[11].y, 430.0);
    assert_eq!(v[12], jrn::Placed { page: 1, y: 100.0 });
    assert_eq!(jrn::layout_list(0).1, 0);
    assert_eq!(jrn::start_page(0), -1);
    let (q, pages) = jrn::layout_quests(&[2, 3, 1, 4, 5]);
    assert_eq!(q[0].y, 100.0);
    assert_eq!(q[1].y, 100.0 + 2.0 * 21.0 + 30.0);
    assert!(pages >= 2);
    let lp = jrn::set_list_page(&jrn::ListPageIn {
        page: -1,
        pages: 3,
        state: jrn::STATE_NOTES,
    });
    assert!(lp.no_pages);
    let lp = jrn::set_list_page(&jrn::ListPageIn {
        page: 2,
        pages: 3,
        state: jrn::STATE_QUEST_LOG,
    });
    assert!(lp.back && !lp.forward && lp.default_is_return);
}

#[test]
fn journal_records() {
    assert_eq!(jrn::note_entries(b"X").0, b"Note_X_Name");
    assert_eq!(jrn::diary_entries(b"D", 3).1, b"Diary_D_Text3");
    assert_eq!(jrn::quest_entries(b"Q").1, b"Quest_Q_Text");
    assert_eq!(
        jrn::image_files(b"a/n.tga"),
        (b"a/n_large.tga".to_vec(), b"a/n_icon.tga".to_vec())
    );
    assert_eq!(
        jrn::archivist(b"Amnesia - Justine -", 9),
        jrn::ARCHIVIST_JUSTINE
    );
    assert_eq!(
        jrn::archivist(b"Amnesia - Justine -", 21),
        jrn::ARCHIVIST_NONE
    );
    assert_eq!(jrn::find_diary(&[2, 3], &[10, 11, 12, 13, 14], 13), (1, 1));
    assert_eq!(jrn::find_diary(&[2], &[10, 11], 99), (-1, -1));
    assert_eq!(jrn::last_read(1, 0, 5, 0, &[2]), 0);
    assert_eq!(jrn::last_read(0, -1, 1, 2, &[]), 1);
    assert_eq!(jrn::disable_quest(true, false), jrn::QUEST_ALREADY_OFF);
}

#[test]
fn journal_effect_math() {
    let e = jrn::hover_effect(0.0, 1.0, 1.0);
    assert!((e.t - 0.65).abs() < 1e-6);
    assert!((e.x_add - (1.0 + (1.0 - e.t) * 5.0)).abs() < 1e-6);
    assert_eq!(jrn::hover_alpha(0.0, true, 0.1), 0.4);
    assert_eq!(jrn::state_alpha(1.0, false, 1.0), 0.0);
    let mut a = 0.9;
    assert_eq!(jrn::update_alpha(&mut a, true, false, 1.0), jrn::EXIT_NONE);
    assert_eq!(a, 1.0);
    let mut a = 0.1;
    assert_eq!(
        jrn::update_alpha(&mut a, false, true, 1.0),
        jrn::EXIT_TO_INVENTORY
    );
}

#[test]
fn events_flush_clips() {
    let mut o = Out::default();
    for i in 0..100 {
        o.push(event::Ev::ints(1, i, 0));
    }
    assert_eq!(o.evs.len(), event::EV_MAX);
    let mut buf = vec![event::Ev::new(0); 4];
    // SAFETY: `buf` has 4 slots.
    let n = unsafe { event::flush(&o, buf.as_mut_ptr(), 4) };
    assert_eq!(n, 4);
    assert_eq!(buf[3].a, 3);
}
