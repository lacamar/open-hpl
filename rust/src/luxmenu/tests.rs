use super::keyconfig::{self as kc, Sub};
use super::options::{self as opt, Mode};
use super::profile::{self as pf, Date};

fn w(s: &str) -> Vec<u32> {
    s.chars().map(|c| c as u32).collect()
}

#[test]
fn profile_trim_and_sort() {
    assert_eq!(pf::trim(&w("  a b  ")), (2, 5));
    assert_eq!(pf::trim(&w("   ")), (3, 3));
    assert_eq!(pf::trim(&[]), (0, 0));
    let d = |y, s| Date {
        year: y,
        seconds: s,
        ..Date::default()
    };
    assert_eq!(
        pf::sort_order(&[d(2020, 1), d(2019, 5), d(2020, 0)]),
        vec![1, 2, 0]
    );
    assert_eq!(pf::select_target(-1, 3), None);
    assert_eq!(pf::select_target(3, 3), None);
    assert_eq!(pf::select_target(2, 3), Some(2));
    assert_eq!(pf::delete_plan(0, true, false).new_selection, -1);
    assert_eq!(pf::activate(0), pf::Activate::CreateWindow);
    assert!(!pf::window_ui_press(true, true, false));
}

#[test]
fn option_tables() {
    assert_eq!(opt::aniso_index(3.0), 2);
    assert_eq!(opt::aniso_index(100.0), 0);
    assert_eq!(opt::aniso_from_index(9), 1.0);
    assert_eq!(opt::ssao_from_index(7), 3);
    assert_eq!(opt::ssao_index(33), 0);
    assert_eq!(opt::slider_setup(0.3, 2.0, 0.05), (34, 9));
    assert_eq!(opt::slider_to_int(5.0, 0.3, 2.0, 34), 34);
    assert_eq!(opt::slider_label(9.0, 0.0, 1.0), w("1"));
    assert_eq!(opt::slider_label(0.25, 0.0, 1.0), w("0.25"));
    assert_eq!(opt::color_index(b"ScRGB"), 2);
    assert_eq!(opt::color_name(9), "P3");
    assert_eq!(opt::renderer_index(b"VULKAN"), 1);
    assert_eq!(opt::gui_scale_index(9, 4), 3);
    assert_eq!(opt::flip_index(opt::flip_index(1, 3), 3), 1);
    assert_eq!(opt::sound_device_index(5, &[1, 2], 3), 2);
    assert!(opt::needs_restart(false, true, true));
    assert!(!opt::needs_restart(true, true, true));
}

#[test]
fn resolution_dedup_keeps_last_with_first_rate() {
    let m = |d, w, h, r| Mode {
        display: d,
        width: w,
        height: h,
        refresh: r,
    };
    let v = vec![
        m(0, 800, 600, 60),
        m(0, 800, 600, 75),
        m(0, 1024, 768, 60),
        m(1, 1024, 768, 30),
    ];
    assert!(opt::multi_display(&v));
    assert_eq!(opt::dedup_modes(&v), vec![(1, 60), (2, 60), (3, 30)]);
    let v = vec![m(0, 800, 600, 60), m(0, 1024, 768, 60), m(1, 1024, 768, 30)];
    assert_eq!(opt::current_mode(&v, 0, 1024, 768), 1);
    assert_eq!(opt::current_mode(&v, 2, 1, 1), -1);
    assert_eq!(opt::resolution_label(1, 2, true), w("1x2 (Custom)"));
}

#[test]
fn tip_fade_in_and_out() {
    let t = opt::Tip {
        restart: true,
        text_reset: true,
    };
    let s = opt::tip_step(t, 0.9, 0.1);
    assert!(s.reset_scroll);
    assert!(!s.tip.restart && !s.tip.text_reset);
    assert!(matches!(s.act, opt::TipAct::FadeIn { clamp: true, .. }));
    let idle = opt::tip_step(
        opt::Tip {
            restart: false,
            text_reset: false,
        },
        0.0,
        0.1,
    );
    assert!(idle.tip.text_reset);
}

#[test]
fn key_matching_and_conflicts() {
    let k = |id| Sub::dev(kc::DEV_KEYBOARD, id);
    let a = [k(10), Sub::EMPTY];
    let b = [Sub::pad(kc::GP_HAT, 1, 4.0), k(10)];
    assert_eq!(kc::key_pos(&a, 10), 0);
    assert_eq!(kc::key_pos(&a, 11), kc::POS_LAST);
    assert_eq!(kc::pad_hat_pos(&b, 1, 4), 0);
    assert_eq!(kc::sharing(&a, &b), vec![0]);
    assert_eq!(kc::sharing(&b, &a), vec![1]);
    assert!(!kc::has_sub_actions(&[Sub::EMPTY, Sub::EMPTY]));

    let subs = [k(1), Sub::EMPTY, k(1), Sub::EMPTY, Sub::EMPTY, Sub::EMPTY];
    let mut red = Vec::new();
    let bad = kc::check_inputs(&subs, &[3], |e, p, r| {
        if r {
            red.push((e, p));
        }
    });
    assert!(bad);
    assert_eq!(red, vec![(0, 0), (1, 0), (2, 0), (2, 1), (2, 0), (2, 1)]);
}

#[test]
fn rebinding() {
    let mut s = [Sub::EMPTY, Sub::EMPTY];
    kc::set(&mut s, 1, kc::Bind::Key(42));
    assert_eq!(s[1], Sub::dev(kc::DEV_KEYBOARD, 42));
    kc::set(&mut s, 0, kc::Bind::Key(42));
    assert_eq!(s[0], Sub::EMPTY);
    kc::set(&mut s, kc::POS_LAST, kc::Bind::Mouse(1));
    kc::set(&mut s, 0, kc::Bind::PadAxis(3, 2));
    assert_eq!(s[0], Sub::pad(kc::GP_AXIS, 3, 2.0));
    let ops = kc::update_ops(&s, true).unwrap_or_default();
    assert_eq!(ops, vec![kc::Op::PadAxis(3, 2), kc::Op::Key(42)]);
    assert!(kc::update_ops(&s, false).is_none());
}

#[test]
fn string_to_sub_and_back() {
    let s = kc::from_string(b"Keyboard.Space", -1);
    assert_eq!(s.kind, kc::DEV_KEYBOARD);
    assert_eq!(kc::label(&s), b"Space".to_vec());
    let s = kc::from_string(b"Mouse.Left", -1);
    assert_eq!(s.kind, kc::DEV_MOUSE);
    assert_eq!(kc::from_string(b"Nothing.X", -1), Sub::EMPTY);
    assert_eq!(
        kc::from_string(b"GamepadButton", 5),
        Sub::pad(kc::GP_BUTTON, 5, 1.0)
    );
    assert_eq!(
        kc::from_string(b"Keyboard", 7),
        Sub::dev(kc::DEV_KEYBOARD, 7)
    );
    assert_eq!(kc::pad_hat_pos(&[], 0, 0), kc::POS_LAST);
    assert_eq!(crate::input::pad_hat_from_name(b"?"), kc::HAT_LAST);
    assert_eq!(crate::input::pad_axis_from_name(b"?"), kc::AXIS_LAST);
}

#[test]
fn defaults_fill_and_clear_in_place() {
    let mut s = [Sub::pad(kc::GP_HAT, 2, 4.0), Sub::pad(kc::GP_HAT, 3, 8.0)];
    kc::fill_default(&[(b"Keyboard.Space".as_slice(), -1)], &mut s, None);
    assert_eq!(s[0].kind, kc::DEV_KEYBOARD);
    assert_eq!(
        (s[1].kind, s[1].id, s[1].gtype),
        (kc::DEV_NONE, -1, kc::GP_HAT)
    );
}

#[test]
fn decisions() {
    assert_eq!(kc::key_press(true, 1, 1), kc::KeyAct::Cancel);
    assert_eq!(kc::pad_press(true, 0.0, true, 1, 1), kc::PadAct::Pass);
    assert_eq!(kc::pad_press(true, 1.0, true, 1, 1), kc::PadAct::Cancel);
    assert_eq!(kc::set_waiting(true, true), kc::Waiting::Ignore);
    assert_eq!(kc::category_button(false, 4, 4, 8, 0, 3), Some(0));
    assert_eq!(kc::category_button(false, 8, 4, 8, 2, 3), Some(2));
    assert_eq!(kc::category_button(true, 8, 4, 8, 1, 3), None);
    assert_eq!(kc::exit_pressed(false, true), kc::Exit::ClearCancelled);
    let (f, sets) = kc::tip_fade(
        0.1,
        kc::Fade {
            out: true,
            restore: true,
        },
        0.5,
    );
    assert!(!f.out && !f.restore);
    assert_eq!(sets, vec![0.0, 1.0]);
    assert!(!kc::binds_pad(b"LeanLeft"));
}
