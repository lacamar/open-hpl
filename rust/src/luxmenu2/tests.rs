use super::credits::{self, Credits, DemoEnd, Row};
use super::loadscreen::Load;
use super::menu::{self, Menu, Times};
use super::pages;
use super::premenu::{self, Pre};
use crate::luxsave::names::Date;

fn w(s: &str) -> Vec<u32> {
    s.bytes().map(u32::from).collect()
}

#[test]
fn window_switch_two_steps() {
    let mut m = Menu::new();
    assert_eq!(menu::win_begin(&m, menu::WIN_LAST), -2);
    assert_eq!(menu::win_begin(&m, 1), menu::WIN_LAST);
    assert!(!menu::win_commit(&mut m, 1));
    assert_eq!(menu::win_begin(&m, 3), 1);
    assert!(menu::win_commit(&mut m, menu::WIN_LAST));
}

#[test]
fn exit_fades_to_black_then_exits() {
    let mut m = Menu::new();
    m.enter();
    m.fade_alpha = 0.0;
    let t = Times {
        fade_in: 1.0,
        out_fast: 0.5,
        out_slow: 2.0,
        fov: 0.1,
        zoomed_fov: 1.0,
    };
    assert_eq!(
        menu::exit_menu(&mut m, menu::EXIT_START),
        menu::XM_ZOOM_SOUND
    );
    let mut frames = 0;
    loop {
        let b = menu::update_base(&mut m, 0.25, &t, true);
        assert!(b.set_fov);
        if b.exit {
            break;
        }
        frames += 1;
        assert!(frames < 100);
    }
    assert_eq!(m.fade_alpha, 1.0);
    assert_eq!(frames, 8);
}

#[test]
fn fade_in_clamps_at_zero() {
    let mut m = Menu::new();
    m.enter();
    let t = Times {
        fade_in: 0.5,
        out_fast: 1.0,
        out_slow: 1.0,
        fov: 0.0,
        zoomed_fov: 0.0,
    };
    for _ in 0..5 {
        menu::update_base(&mut m, 0.25, &t, false);
    }
    assert_eq!(m.fade_alpha, 0.0);
}

#[test]
fn top_menu_plan_by_state() {
    use menu::*;
    assert_eq!(plan(0), [L_START, L_CUSTOM, L_OPTIONS, L_PROFILE, L_EXIT]);
    assert_eq!(plan(P_NO_CUSTOM), [L_START, L_OPTIONS, L_PROFILE, L_EXIT]);
    assert_eq!(
        plan(P_SAVE_EXISTS),
        [
            L_CONTINUE, L_START, L_LOAD, L_CUSTOM, L_OPTIONS, L_PROFILE, L_EXIT
        ]
    );
    assert_eq!(plan(P_MAP), [L_BACK, L_OPTIONS, L_TO_MENU, L_AND_SAVE]);
    assert_eq!(plan(P_MAP | P_HARD), [L_BACK, L_SAVE, L_OPTIONS, L_TO_MENU]);
    assert_eq!(plan(P_MAP | P_EXIT_DIRECT), [L_BACK, L_OPTIONS, L_EXIT]);
    assert_eq!(plan(P_MAP | P_DEAD), [L_BACK, L_OPTIONS, L_TO_MENU]);
}

#[test]
fn hash_substitution_and_tinderboxes() {
    assert_eq!(
        menu::subst_hash(&w("Costs # boxes"), 4),
        Some(w("Costs 4 boxes"))
    );
    assert_eq!(menu::subst_hash(&w("none"), 4), None);
    assert_eq!(menu::pay_save(3), None);
    assert_eq!(menu::pay_save(9), Some(5));
}

#[test]
fn font_fit_shrinks_long_labels() {
    let (mul, add) = menu::font_fit(&[100.0, 400.0], &[5, 10], 1000.0, [20.0, 30.0]);
    assert_eq!(mul, 1.75);
    assert_eq!(add, 30.0 * mul * 1.3);
    assert_eq!(menu::font_fit(&[10.0], &[5], 1000.0, [20.0, 30.0]).0, 1.0);
}

#[test]
fn saves_newest_first_stable() {
    let d = |m: i32| Date {
        month: m,
        ..Date::default()
    };
    assert_eq!(pages::sort_saves(&[d(1), d(3), d(3), d(2)]), [1, 2, 3, 0]);
    assert_eq!(pages::load_index(-1, 3), None);
    assert_eq!(pages::load_index(3, 3), None);
    assert_eq!(pages::load_index(2, 3), Some(2));
}

#[test]
fn story_buttons_hide_save_ones() {
    let mut a = [false; 4];
    let mut y = [0.0; 4];
    pages::story_buttons(false, 100.0, &[30.0; 4], &mut a, &mut y);
    assert_eq!(a, [false, true, false, true]);
    assert_eq!((y[1], y[3]), (100.0, 136.0));
    pages::story_buttons(true, 100.0, &[30.0; 4], &mut a, &mut y);
    assert_eq!(y, [100.0, 136.0, 172.0, 208.0]);
}

#[test]
fn start_folders_quirk() {
    assert_eq!(pages::start_folders(false, true), pages::FOLDER_CUSTOM);
    assert_eq!(pages::start_folders(false, false), pages::FOLDER_CUSTOM);
    assert_eq!(pages::start_folders(true, false), pages::FOLDER_PROFILE);
    assert_eq!(pages::start_folders(true, true), 0);
}

#[test]
fn premenu_runs_sections_then_final() {
    let mut p = Pre::new();
    p.enter();
    let only = [false, true, false];
    let mut seen = Vec::new();
    for _ in 0..200 {
        p.update_actions(0.1);
        let s = p.update_state(false, &only);
        match s.act {
            premenu::ACT_SECTION => seen.push(s.section),
            premenu::ACT_SET_TIMER => p.timer = 0.2,
            premenu::ACT_CHECK_SHOW => p.check_show(false, false),
            premenu::ACT_FINAL => break,
            _ => {}
        }
    }
    assert_eq!(seen, [0, 2]);
    assert_eq!(p.state, premenu::S_FINAL);
    p.update_actions(0.1);
    assert!(p.exit);
}

#[test]
fn premenu_skip_respects_flag() {
    let mut p = Pre::new();
    p.state = premenu::S_SHOW;
    p.button_pressed(true, false);
    assert_eq!(p.state, premenu::S_SHOW);
    p.button_pressed(false, false);
    assert_eq!(p.state, premenu::S_FAST_FADE_OUT);
}

#[test]
fn gamma_slider_round_trip() {
    let (max, bar) = premenu::gamma_slider();
    assert_eq!((max, bar), (34, 9));
    for v in 0..=max {
        assert_eq!(
            premenu::gamma_to_slider(premenu::slider_to_gamma(v, max), max),
            v
        );
    }
    assert_eq!(premenu::parse_align(b"CeNtEr"), Some(2));
    assert_eq!(premenu::parse_align(b"top"), None);
}

#[test]
fn credits_scroll_to_end() {
    let rows = vec![
        Row {
            len: 10,
            first: u32::from(b'a')
        };
        3
    ];
    let mut c = Credits::new();
    let mut stops = 0;
    let mut finished = false;
    for _ in 0..4000 {
        let f = c.update(0.05, &rows, 40.0, 1.0);
        stops += u32::from(f & credits::UP_STOP_MUSIC != 0);
        if c.state == 3 && c.time >= 3.0 {
            assert!(!c.exit_pressed() && c.state == 4);
        }
        if f & credits::UP_FINISH != 0 {
            finished = true;
            break;
        }
    }
    assert!(finished && stops > 0);
}

#[test]
fn credits_draw_skips_offscreen_and_blank() {
    let rows = [
        Row {
            len: 5,
            first: u32::from(b'*'),
        },
        Row { len: 0, first: 0 },
        Row {
            len: 5,
            first: u32::from(b'x'),
        },
    ];
    let v = credits::draw_rows(100.0, &rows, [14.0, 16.0]);
    assert_eq!(v.len(), 2);
    assert!(v.first().is_some_and(|r| r.header));
    assert_eq!(v.get(1).map(|r| r.y), Some(100.0 + 16.0 + 14.0 + 14.0));
    assert!(credits::draw_rows(700.0, &rows, [14.0, 16.0]).is_empty());
}

#[test]
fn demo_end_fades_and_repeats_exit() {
    let mut d = DemoEnd::new();
    for _ in 0..5 {
        d.update(0.5);
    }
    assert_eq!(d.fade_alpha, 0.0);
    d.exit(true);
    let mut flags = 0;
    for _ in 0..10 {
        flags = d.update(0.5);
    }
    assert_eq!(flags, credits::DE_EXIT | credits::DE_BROWSER);
}

#[test]
fn load_screen_holds_then_exits() {
    let mut l = Load::new();
    l.load_done(Some(3.0), 1.0);
    assert_eq!(l.extra_time, 2.0);
    let mut t = 0.0;
    while !l.update_game(0.1, 0.5) {
        t += 0.1;
        assert!(t < 10.0);
    }
    assert!(t > 2.0);
}

#[test]
fn load_text_does_not_repeat_until_exhausted() {
    let mut l = Load::new();
    l.setup(false, 4);
    let mut seen = std::collections::BTreeSet::new();
    for _ in 0..4 {
        assert!(seen.insert(l.text_suffix()));
    }
    assert_eq!(l.prev_bits, 0b1111);
    l.setup(false, 1);
    assert!(l.text_suffix().is_empty());
}

#[test]
fn tenth_load_text_keeps_the_double_zero_quirk() {
    let mut l = Load::new();
    l.setup(false, 10);
    l.prev_bits = 0b0111111111;
    assert_eq!(l.text_suffix(), b"010");
}

#[test]
fn premenu_button_after_final_stays_final() {
    let mut p = Pre::new();
    p.state = premenu::S_FAST_FADE_OUT;
    p.alpha = 1.0;
    p.section = 2;
    let s = p.update_state(true, &[false, false, false]);
    assert_eq!((s.act, p.state), (premenu::ACT_FINAL, premenu::S_FINAL));
    p.button_pressed(false, true);
    p.alpha = 1.0;
    let s = p.update_state(true, &[false, false, false]);
    assert_eq!((s.act, p.state), (premenu::ACT_FINAL, premenu::S_FINAL));
}

#[test]
fn ui_buttons_and_popups() {
    assert_eq!(
        (
            pages::ui_switch(1),
            pages::ui_switch(2),
            pages::ui_switch(3),
            pages::ui_switch(0)
        ),
        (1, 2, 0, 0)
    );
    assert_eq!(
        (menu::top_press(3), menu::top_press(2), menu::top_press(4)),
        (1, 2, 0)
    );
    assert!(menu::popup_ok(0) && !menu::popup_ok(1));
    assert_eq!(pages::mode_gold(true), (false, true));
    assert_eq!(
        pages::story_fallbacks(true, false, true),
        pages::FB_NAME | pages::FB_DESC
    );
}

#[test]
fn exit_flow_decisions() {
    let mut m = Menu::new();
    assert_eq!(menu::exit_pressed(&m, true), menu::EXIT_RETURN_TO_GAME);
    assert_eq!(menu::exit_pressed(&m, false), menu::EXIT_NONE);
    m.window = 1;
    assert_eq!(menu::exit_pressed(&m, false), menu::EXIT_WINDOW);
    assert_eq!(
        menu::exit_menu(&mut m, menu::EXIT_TO_MENU),
        menu::XM_ACHIEVEMENT
    );
    assert_eq!(
        menu::on_menu_exit(menu::EXIT_QUIT_GAME, true),
        menu::OME_DEMO_END
    );
    assert_eq!(
        menu::on_menu_exit(menu::EXIT_QUIT_GAME, false),
        menu::OME_ENGINE_EXIT
    );
    assert_eq!(menu::on_menu_exit(6, false), menu::OME_NONE);
}

#[test]
fn background_rect_fills_height_on_wide_images() {
    let (x, w) = premenu::background_rect(1600.0, 900.0);
    assert!(x < 0.0 && w > 800.0);
    let (x, w) = premenu::background_rect(800.0, 600.0);
    assert!(x.abs() < 1e-3 && (w - 800.0).abs() < 1e-3);
}
