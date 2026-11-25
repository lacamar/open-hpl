//! Main menu against the fake host: the states and top label layout of the recorded C++ dump
//! (`OHPL_A9_MENUDUMP`, golden `A9b/menu_dump_cpp.txt`), popup flows, exit sequences and the blur
//! render pass counts.

use super::host::{code, flag, input, music, updater};
use super::*;
use crate::luxmenu::fake::{self as fk, FAKE, Fake};
use crate::luxmenu::host::{Args, op};
use crate::luxmenu::store::resolve;
use crate::luxmenu::win::fake as win_fake;
use std::cell::RefCell;
use std::collections::HashMap;

const GOLDEN: &str = include_str!("../../../tests/golden/A9b/menu_dump_cpp.txt");

#[derive(Default)]
struct Mm {
    next: usize,
    calls: Vec<(i32, i32, usize)>,
    flags: HashMap<i32, bool>,
    map_loaded: bool,
    tinder: i32,
    names: HashMap<usize, String>,
}

thread_local! {
    static MM: RefCell<Mm> = RefCell::new(Mm::default());
}

fn mm<R>(f: impl FnOnce(&mut Mm) -> R) -> R {
    MM.with(|m| f(&mut m.borrow_mut()))
}

fn text(a: &Args, i: usize) -> String {
    let (p, n) = (a.s[i], a.n[i]);
    if p.is_null() || n == 0 {
        return String::new();
    }
    // SAFETY: the builder keeps the string alive for the call.
    let u = unsafe { std::slice::from_raw_parts(p, n) };
    u.iter().filter_map(|&c| char::from_u32(c)).collect()
}

fn out(f: &mut Fake, a: &mut Args, s: &str) {
    f.out = s.chars().map(u32::from).collect();
    a.s[0] = f.out.as_ptr();
    a.n[0] = f.out.len();
}

pub(crate) fn call(f: &mut Fake, c: i32, a: &mut Args) -> usize {
    mm(|m| m.calls.push((c, a.i[0], a.p[0])));
    let id = |m: &mut Mm| {
        m.next += 1;
        0x1000 + m.next
    };
    match c {
        code::SKIN_NEW
        | code::SET_NEW
        | code::VP_NEW
        | code::GFX_RECT
        | code::GFX_IMAGE
        | code::GFX_TEX
        | code::FONT_LOAD
        | code::WORLD_LOAD
        | code::CAMERA_NEW
        | code::TEX_NEW
        | code::FB_NEW
        | code::PROG_NEW => mm(id),
        code::LABEL_NAMED => {
            let w = fk::create(f, op::LABEL, a);
            let name = text(a, 1);
            mm(|m| m.names.insert(w, name));
            w
        }
        code::CFG => {
            let (sec, key) = (text(a, 0), text(a, 1));
            let _ = sec;
            match a.i[0] {
                0 => {
                    a.f[0] = match key.as_str() {
                        "MainFadeInTime" => 1.0,
                        "MainFadeOutTimeFast" => 0.5,
                        "MainFadeOutTimeSlow" => 2.0,
                        "TopMenuFadeInTime" => 1.0,
                        "TopMenuFadeOutTime" => 0.25,
                        _ => a.f[0],
                    };
                }
                1 => {
                    let v = match key.as_str() {
                        "TopMenuStartRelativePos" => [0.8, 0.325],
                        "TopMenuFontRelativeSize" => [0.04, 0.05],
                        "TopMenuStartRelativePosInGame" => [0.5, 0.2],
                        "MainMenuLogoStartRelativePos" => [0.1, 0.1],
                        _ => [0.4, 0.4],
                    };
                    a.f[..2].copy_from_slice(&v);
                }
                _ => {
                    let v = match key.as_str() {
                        "Music" => "menu.ogg",
                        "ZoomSound" => "zoom.snt",
                        "MenuLogo" => "logo.tga",
                        "TopMenuFont" => "font.fnt",
                        _ => "",
                    };
                    out(f, a, v);
                }
            }
            0
        }
        code::SET_VSIZE | code::SCREEN_SIZE => {
            a.f[0] = 800.0;
            a.f[1] = 600.0;
            a.i[0] = 800;
            a.i[1] = 600;
            0
        }
        code::GFX_SIZE => {
            a.f[0] = 200.0;
            a.f[1] = 100.0;
            0
        }
        code::WIDGET_FONT_LEN => {
            a.f[0] = text(a, 0).len() as f32 * 10.0;
            0
        }
        code::WIDGET_NAME => {
            let n = mm(|m| m.names.get(&a.p[0]).cloned().unwrap_or_default());
            out(f, a, &n);
            0
        }
        code::WIDGET_HAS_FOCUS => usize::from(a.p[0] == f.focused),
        code::WIDGET_ENABLED => usize::from(fk::node(f, a.p[0]).enabled),
        code::WIDGET_NAV => fk::node(f, a.p[0]).nav[(a.i[0] & 3) as usize],
        code::FLAG => mm(|m| usize::from(m.flags.get(&a.i[0]).copied().unwrap_or(false))),
        code::MAP_LOADED | code::HAS_MAP => mm(|m| usize::from(m.map_loaded)),
        code::TINDER => mm(|m| {
            if a.i[0] == 0 {
                m.tinder as usize
            } else {
                m.tinder = a.i[1];
                0
            }
        }),
        code::MAP_NAME => {
            out(f, a, "00_rainy_hall");
            0
        }
        code::SET_GET_FOCUSED => f.focused,
        code::PROCESS_MOUSE_DOWN => 1,
        _ => 0,
    }
}

fn reset() {
    fk::reset();
    win_fake::reset();
    MM.with(|m| *m.borrow_mut() = Mm::default());
    mm(|m| {
        m.tinder = 5;
        m.flags.insert(flag::ALLOW_HARD, true);
        m.flags.insert(flag::NO_CUSTOM_STORY, true);
        m.flags.insert(flag::NO_USER_CONFIG, true);
    });
}

fn menu() -> Rc<Menu> {
    reset();
    let bits = new();
    let me = lookup(bits).expect("menu");
    me.load_fonts();
    me.create_gui();
    me
}

fn calls() -> Vec<(i32, i32)> {
    mm(|m| m.calls.iter().map(|c| (c.0, c.1)).collect())
}

fn clear_calls() {
    mm(|m| m.calls.clear());
}

fn count(c: i32) -> usize {
    mm(|m| m.calls.iter().filter(|x| x.0 == c).count())
}

fn labels(me: &Menu) -> Vec<(usize, String)> {
    me.labels
        .borrow()
        .iter()
        .map(|l| {
            let t = FAKE.with(|f| f.borrow().nodes[l.0].text.clone());
            (l.0, t.iter().filter_map(|&c| char::from_u32(c)).collect())
        })
        .collect()
}

fn press(w: usize, id: i32) {
    let tokens: Vec<usize> = FAKE.with(|f| {
        f.borrow().nodes[w]
            .callbacks
            .iter()
            .filter(|c| c.0 == id)
            .map(|c| c.1)
            .collect()
    });
    for t in tokens {
        let cb = resolve(t).expect("token");
        let data = CbData {
            msg: id,
            ..CbData::default()
        };
        callback(&cb, Wd(w), &data);
    }
}

fn answer_popup(val: i32) {
    let last = FAKE.with(|f| {
        f.borrow().log.iter().rev().find_map(|l| {
            l.strip_prefix("msgbox ")
                .and_then(|t| t.parse::<usize>().ok())
        })
    });
    let cb = resolve(last.expect("popup")).expect("token");
    let data = CbData {
        lval: val,
        ..CbData::default()
    };
    callback(&cb, Wd(0), &data);
}

fn golden_headers() -> Vec<(String, i32, i32, f32, f32)> {
    GOLDEN
        .lines()
        .filter(|l| l.starts_with("== ") && l.contains(" win="))
        .map(|l| {
            let field = |k: &str| {
                l.split(' ')
                    .find_map(|t| t.strip_prefix(k))
                    .unwrap_or("0")
                    .to_string()
            };
            (
                l.split(' ').nth(1).unwrap_or("").to_string(),
                field("win=").parse().unwrap_or(-1),
                field("topvis=").parse().unwrap_or(-1),
                field("ta=").parse().unwrap_or(-1.0),
                field("fa=").parse().unwrap_or(-1.0),
            )
        })
        .collect()
}

fn golden_state(name: &str) -> (i32, i32, f32, f32) {
    golden_headers()
        .into_iter()
        .find(|h| h.0 == name)
        .map(|h| (h.1, h.2, h.3, h.4))
        .expect("golden step")
}

fn check(me: &Menu, name: &str) {
    let s = me.state();
    let (win, vis, ta, fa) = golden_state(name);
    assert_eq!(
        (
            s.window,
            i32::from(s.top_visible),
            s.top_alpha,
            s.fade_alpha
        ),
        (win, vis, ta, fa),
        "step {name}"
    );
}

#[test]
fn top_labels_match_the_cpp_layout() {
    let me = menu();
    check(&me, "created");
    let l = labels(&me);
    assert_eq!(l.len(), 5);
    let golden: Vec<&str> = GOLDEN
        .lines()
        .skip(1)
        .take_while(|l| !l.starts_with("=="))
        .collect();
    assert_eq!(golden.len(), 5);
    let num = |s: &str, k: &str| -> Vec<f32> {
        s.split(' ')
            .find_map(|t| t.strip_prefix(k))
            .unwrap_or("")
            .split(',')
            .filter_map(|v| v.parse().ok())
            .collect()
    };
    let (g0, g1) = (num(golden[0], "p="), num(golden[1], "p="));
    let row = g1[1] - g0[1];
    let gs = num(golden[0], "s=");
    assert_eq!(g0[0] + gs[0] * 0.5, 640.0);
    for (i, (id, _)) in l.iter().enumerate() {
        let (p, s) = FAKE.with(|f| {
            let f = f.borrow();
            (f.nodes[*id].pos, f.nodes[*id].size)
        });
        assert_eq!(p[0] + s[0] * 0.5, 640.0, "label {i} centred on the start x");
        assert_eq!(p[1], 195.0 + row * i as f32, "label {i} row");
        assert_eq!(p[2], 2.0);
    }
    let order: Vec<&str> = l.iter().map(|(_, t)| t.as_str()).collect();
    assert_eq!(
        order,
        [
            "MainMenu.Start Game",
            "MainMenu.Custom Map",
            "MainMenu.Options",
            "MainMenu.Change Profile",
            "MainMenu.Exit"
        ]
    );
}

#[test]
fn window_and_popup_steps_replay_the_cpp_dump() {
    let me = menu();
    me.set_window_active(m::WIN_LAST);
    check(&me, "top");
    for _ in 0..20 {
        me.update(1.0 / 60.0);
    }
    check(&me, "tick");
    let l = labels(&me);
    let press_label = |i: usize| press(l[i].0, msg::MOUSE_DOWN);

    press_label(2);
    check(&me, "press_options");
    me.set_window_active(m::WIN_LAST);
    check(&me, "top2");
    press_label(3);
    check(&me, "press_profile");
    me.set_window_active(m::WIN_LAST);
    check(&me, "top3");
    press_label(1);
    check(&me, "press_custom");
    me.set_window_active(m::WIN_LAST);
    check(&me, "top4");
    press_label(0);
    check(&me, "press_start");
    assert!(!flag_hard());
    me.set_window_active(m::WIN_LAST);
    check(&me, "top5");

    press_label(4);
    assert!(!me.state().top_visible);
    for _ in 0..20 {
        me.update(1.0 / 60.0);
    }
    check(&me, "exit_tick");
    answer_popup(1);
    check(&me, "exit_no");
    for _ in 0..20 {
        me.update(1.0 / 60.0);
    }
    let (_, _, ta, _) = golden_state("exit_tick2");
    assert!((me.state().top_alpha - ta).abs() < 1e-6);
}

fn flag_hard() -> bool {
    mm(|m| m.flags.get(&flag::HARD).copied().unwrap_or(false))
}

#[test]
fn quit_popup_starts_the_exit_and_the_fade_ends_in_engine_exit() {
    let me = menu();
    let l = labels(&me);
    press(l[4].0, msg::MOUSE_DOWN);
    answer_popup(0);
    assert!(me.state().exiting);
    assert_eq!(me.state().exit_msg, m::EXIT_QUIT_GAME);
    clear_calls();
    for _ in 0..400 {
        me.update(1.0 / 60.0);
    }
    assert!(calls().contains(&(code::UPDATER, updater::ENGINE_EXIT)));
}

#[test]
fn start_exit_runs_the_start_sequence_after_the_slow_fade() {
    let me = menu();
    let l = labels(&me);
    mm(|m| m.flags.insert(flag::ALLOW_HARD, false));
    press(l[0].0, msg::MOUSE_DOWN);
    answer_popup(0);
    assert_eq!(me.state().exit_msg, m::EXIT_START);
    clear_calls();
    me.update(0.0);
    for _ in 0..500 {
        me.update(1.0 / 60.0);
    }
    let seq: Vec<i32> = calls()
        .into_iter()
        .filter(|c| {
            matches!(
                c.0,
                code::INPUT_STATE | code::PROFILE | code::UPDATER | code::START_GAME
            )
        })
        .map(|c| c.0 * 10 + c.1)
        .collect();
    assert_eq!(
        seq[..6],
        [
            code::INPUT_STATE * 10 + input::GAME,
            code::PROFILE * 10,
            code::PROFILE * 10 + 1,
            code::PROFILE * 10 + 2,
            code::UPDATER * 10 + updater::DEFAULT,
            code::START_GAME * 10,
        ]
    );
}

#[test]
fn back_to_game_and_to_menu_with_a_map() {
    reset();
    mm(|m| {
        m.map_loaded = true;
        m.flags.insert(flag::NO_USER_CONFIG, false);
    });
    let me = lookup(new()).expect("menu");
    me.load_fonts();
    me.create_gui();
    let l = labels(&me);
    let names: Vec<&str> = l.iter().map(|(_, t)| t.as_str()).collect();
    assert_eq!(
        names,
        [
            "MainMenu.Back To Game",
            "MainMenu.Options",
            "MainMenu.ExitToMainMenu",
            "MainMenu.ExitAndSave"
        ]
    );
    me.exit_pressed();
    assert!(me.state().exiting);
    assert_eq!(me.state().exit_msg, m::EXIT_RETURN);
    clear_calls();
    for _ in 0..120 {
        me.update(1.0 / 60.0);
    }
    let c = calls();
    assert!(c.contains(&(code::INPUT_STATE, input::GAME)));
    assert!(c.contains(&(code::UPDATER, updater::DEFAULT)));

    let me = lookup(new()).expect("menu");
    me.load_fonts();
    me.create_gui();
    let l = labels(&me);
    press(l[2].0, msg::MOUSE_DOWN);
    clear_calls();
    answer_popup(0);
    assert_eq!(me.state().exit_msg, m::EXIT_TO_MENU);
    assert!(calls().contains(&(code::MAP_NAME, 0)));
    assert!(!calls().contains(&(code::UNLOCK_NOPE, 0)) || count(code::UNLOCK_NOPE) == 1);
    clear_calls();
    for _ in 0..120 {
        me.update(1.0 / 60.0);
    }
    let c: Vec<i32> = calls().iter().map(|c| c.0).collect();
    let pos = |code: i32| c.iter().position(|&x| x == code).expect("call");
    assert!(pos(code::DRAW_MENU_SCREEN) < pos(code::VP_ACTIVE));
    assert!(c.contains(&code::GUI_FOCUS));
}

#[test]
fn hard_mode_save_pays_and_disables_the_label() {
    reset();
    mm(|m| {
        m.map_loaded = true;
        m.tinder = 6;
        m.flags.insert(flag::HARD, true);
        m.flags.insert(flag::NO_USER_CONFIG, false);
    });
    let me = lookup(new()).expect("menu");
    me.load_fonts();
    me.create_gui();
    let l = labels(&me);
    assert_eq!(l[1].1, "MainMenu.Save");
    let save = l[1].0;
    assert!(FAKE.with(|f| f.borrow().nodes[save].enabled));
    press(save, msg::MOUSE_DOWN);
    clear_calls();
    answer_popup(0);
    assert_eq!(mm(|m| m.tinder), 2);
    assert_eq!(count(code::SAVE), 1);
    assert!(!FAKE.with(|f| f.borrow().nodes[save].enabled));
    let (above, below) = (l[0].0, l[2].0);
    FAKE.with(|f| {
        let f = f.borrow();
        assert_eq!(f.nodes[above].nav[2], below);
        assert_eq!(f.nodes[below].nav[0], above);
    });
}

#[test]
fn blur_render_runs_seven_passes_and_frees_its_temporaries() {
    reset();
    mm(|m| {
        m.map_loaded = true;
        m.flags.insert(flag::NO_USER_CONFIG, false);
    });
    let me = lookup(new()).expect("menu");
    clear_calls();
    me.enter();
    assert_eq!(count(code::PROG_BIND), 28);
    assert_eq!(count(code::GFX_QUAD), 14);
    assert_eq!(count(code::FB_NEW), 2);
    assert_eq!(count(code::FB_DESTROY), 2);
    assert_eq!(count(code::TEX_NEW), 3);
    assert_eq!(count(code::TEX_DESTROY), 1);
    assert_eq!(count(code::GFX_FLUSH), 1);
    assert_eq!(count(code::MUSIC), 0);
    assert_eq!(count(code::PAUSE_MAP), 1);
    me.leave();
    assert_eq!(count(code::TEX_DESTROY), 3);
    assert_eq!(count(code::GFX_DESTROY), 2);
    assert_eq!(count(code::MUSIC), 0);
    let _ = music::PLAY;
}

#[test]
fn background_world_for_the_title_screen() {
    let me = menu();
    clear_calls();
    me.enter();
    assert_eq!(count(code::MUSIC), 1);
    assert_eq!(count(code::CAMERA_PLACE), 1);
    assert_eq!(count(code::VP_CAMERA), 1);
    assert_eq!(me.dump_info().world, 0);
    assert_ne!(me.dump_info().camera, 0);
    let s = me.dump_info();
    assert_eq!(s.logo_size, [480.0, 240.0]);
}
