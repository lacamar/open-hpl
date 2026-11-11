use super::devhud::*;
use super::frame::*;
use super::multi_settings::*;
use super::save::*;
use super::setup_vars::SetupVars;
use super::updater::{self, AddResult, Updater, Visit};
use std::cell::RefCell;

fn u8s(s: &str) -> Vec<u32> {
    s.chars().map(u32::from).collect()
}

fn log_of(
    u: &mut Updater,
    f: impl FnOnce(*mut Updater, &mut dyn FnMut(usize, Visit<'_>)),
) -> Vec<String> {
    let log = RefCell::new(Vec::new());
    let p: *mut Updater = u;
    f(p, &mut |w, v| {
        log.borrow_mut().push(match v {
            Visit::Message(m, _) => format!("{w}:m{m}"),
            Visit::TimedUpdate(_) => format!("{w}:upd"),
            Visit::Leave(n) => format!("{w}:leave:{}", String::from_utf8_lossy(n)),
            Visit::Enter(n) => format!("{w}:enter:{}", String::from_utf8_lossy(n)),
        });
    });
    log.into_inner()
}

fn sample() -> Updater {
    let mut u = Updater::default();
    u.add_global(1);
    u.add_container(b"Default");
    u.add_container(b"Alpha");
    u.add_update(b"Default", 10);
    u.add_update(b"Alpha", 20);
    u.add_update(b"Default", 11);
    u.add_global(2);
    u
}

#[test]
fn updater_add_results() {
    let mut u = Updater::default();
    assert_eq!(u.add_update(b"x", 1), AddResult::NoContainer);
    assert_eq!(u.add_update(b"x", 0), AddResult::Null);
    u.add_container(b"x");
    assert_eq!(u.add_update(b"x", 1), AddResult::Added);
    assert_eq!(u.current_name(), b"");
}

#[test]
fn run_message_globals_then_current() {
    let mut u = sample();
    // SAFETY: the pointer is the live `u`.
    assert!(unsafe { updater::set_container(&raw mut u, b"Default", &mut |_, _| {}) });
    let l = log_of(&mut u, |p, cb| unsafe {
        updater::run_message(p, 4, 0.5, cb)
    });
    assert_eq!(l, ["1:m4", "2:m4", "10:m4", "11:m4"]);
    let l = log_of(&mut u, |p, cb| unsafe {
        updater::run_message(p, updater::MSG_UPDATE, 0.5, cb)
    });
    assert_eq!(l, ["1:upd", "2:upd", "10:upd", "11:upd"]);
}

#[test]
fn run_message_without_container_runs_globals_only() {
    let mut u = sample();
    let l = log_of(&mut u, |p, cb| unsafe {
        updater::run_message(p, 4, 0.0, cb)
    });
    assert_eq!(l, ["1:m4", "2:m4"]);
}

#[test]
fn broadcast_visits_containers_in_key_order() {
    let mut u = sample();
    let l = log_of(&mut u, |p, cb| unsafe { updater::broadcast(p, 8, 0.0, cb) });
    assert_eq!(l, ["1:m8", "2:m8", "20:m8", "10:m8", "11:m8"]);
}

#[test]
fn set_container_messages_and_names() {
    let mut u = sample();
    let l = log_of(&mut u, |p, cb| {
        assert!(unsafe { updater::set_container(p, b"Default", cb) });
    });
    assert_eq!(l, ["10:enter:", "11:enter:"]);
    let l = log_of(&mut u, |p, cb| {
        assert!(unsafe { updater::set_container(p, b"Alpha", cb) });
    });
    assert_eq!(l, ["10:leave:Alpha", "11:leave:Alpha", "20:enter:Default"]);
    let l = log_of(&mut u, |p, cb| {
        assert!(unsafe { updater::set_container(p, b"Alpha", cb) });
        assert!(!unsafe { updater::set_container(p, b"nope", cb) });
    });
    assert!(l.is_empty());
    assert_eq!(u.current_name(), b"Alpha");
}

#[test]
fn name_is_new_during_leave() {
    let mut u = sample();
    // SAFETY: the pointer is the live `u`.
    unsafe { updater::set_container(&raw mut u, b"Default", &mut |_, _| {}) };
    let p: *mut Updater = &mut u;
    let seen = RefCell::new(Vec::new());
    // SAFETY: `p` is live; the visitor only reads through it.
    unsafe {
        updater::set_container(p, b"Alpha", &mut |_, v| {
            if matches!(v, Visit::Leave(_)) {
                seen.borrow_mut().push((*p).current_name().to_vec());
            }
        });
    }
    assert_eq!(seen.into_inner(), [b"Alpha".to_vec(), b"Alpha".to_vec()]);
}

#[test]
fn updatable_added_mid_run_is_reached() {
    let mut u = sample();
    let p: *mut Updater = &mut u;
    let seen = RefCell::new(Vec::new());
    // SAFETY: `p` is live; the visitor mutates through it between steps only.
    unsafe {
        updater::run_message(p, 4, 0.0, &mut |w, _| {
            seen.borrow_mut().push(w);
            if w == 2 {
                (*p).add_global(3);
            }
        });
    }
    assert_eq!(seen.into_inner(), [1, 2, 3]);
}

#[test]
fn container_switch_stops_current_list() {
    let mut u = sample();
    let p: *mut Updater = &mut u;
    // SAFETY: `p` is live; the visitor re-enters the updater between steps.
    unsafe {
        updater::set_container(p, b"Default", &mut |_, _| {});
        let seen = RefCell::new(Vec::new());
        updater::run_message(p, 4, 0.0, &mut |w, _| {
            seen.borrow_mut().push(w);
            if w == 10 {
                updater::set_container(p, b"Alpha", &mut |_, _| {});
            }
        });
        assert_eq!(seen.into_inner(), [1, 2, 10]);
    }
}

#[test]
fn container_added_during_broadcast_is_visited_if_later() {
    let mut u = sample();
    let p: *mut Updater = &mut u;
    let seen = RefCell::new(Vec::new());
    // SAFETY: `p` is live; the visitor mutates through it between steps only.
    unsafe {
        updater::broadcast(p, 8, 0.0, &mut |w, _| {
            seen.borrow_mut().push(w);
            if w == 20 {
                (*p).add_container(b"Zed");
                (*p).add_update(b"Zed", 30);
                (*p).add_container(b"Aaa");
                (*p).add_update(b"Aaa", 40);
            }
        });
    }
    assert_eq!(seen.into_inner(), [1, 2, 20, 10, 11, 30]);
}

#[test]
fn fps_counter_matches_float_pipeline() {
    let mut f = Fps::new(1234);
    assert_eq!(f.fps, 60.0);
    assert_eq!(f.frametime_start, 1.234);
    f.add_frame(1500);
    assert_eq!(f.frame_counter, 1);
    f.add_frame(2300);
    assert_eq!(f.frame_counter, 0);
    assert_eq!(f.fps, 2.0 / (2.3f32 - 1.234f32));
    assert_eq!(f.frametime_start, 2.3);
    f.add_frame(2300);
    assert_eq!(f.frame_counter, 1);
}

#[test]
fn fps_zero_rate_and_nan_rate() {
    let mut f = Fps::new(0);
    f.update_rate = 0.0;
    f.add_frame(0);
    assert!(f.fps.is_nan() || f.fps.is_infinite());
    let mut g = Fps::new(0);
    g.update_rate = f32::NAN;
    g.add_frame(5000);
    assert_eq!((g.fps, g.frame_counter), (60.0, 1));
    assert_eq!(Fps { fps: 0.0, ..g }.avg_frame_ms(), f32::INFINITY);
}

#[test]
fn loop_state_machine() {
    let mut l = Loop::default();
    assert!(l.is_updated && !l.buffer_swap);
    l.begin_run();
    assert!(!l.take_swap());
    assert!(l.want_render(true));
    l.step_begin();
    l.step_begin();
    l.add_logic_ms(1.5);
    l.add_swap_ms(0.25);
    let t = l.frame_rendered();
    assert_eq!((t.logic_ms, t.swap_ms, t.steps), (1.5, 0.25, 2));
    assert!(!l.want_render(true) && l.want_render(false));
    assert!(l.take_swap() && !l.take_swap());
    assert!(!l.skip_render_once(true));
    assert!(l.swap_done(true) && !l.swap_done(false));
    assert!(l.skip_render_once(true) && !l.skip_render_once(false));
    l.advance_game_time(0.5);
    l.begin_run();
    assert_eq!(l.game_time, 0.5);
    assert_eq!(l.num_of_times, 0.0);
}

#[test]
fn update_log_ticks_every_twentieth() {
    let mut l = Loop::default();
    let hits: Vec<usize> = (1..=45).filter(|_| l.update_log_tick()).collect();
    assert_eq!(hits.len(), 2);
    l.clear_check = i32::MAX;
    assert!(!l.update_log_tick());
    assert_eq!(l.clear_check, i32::MIN);
}

#[test]
fn mean_framerate_and_frame_time() {
    let l = Loop {
        num_of_times: 10.0,
        ..Loop::default()
    };
    assert_eq!(l.mean_framerate(2000), 5.0);
    assert!(l.mean_framerate(0).is_infinite());
    assert!(frame_time(f64::NAN).is_nan());
    assert_eq!(frame_time(1e300), f32::INFINITY);
}

#[test]
fn focus_messages_order() {
    assert!(focus_messages([true; 3], [true; 3]).is_empty());
    assert_eq!(
        focus_messages([true, false, true], [false, true, false]),
        [
            MSG_APP_LOST_INPUT_FOCUS,
            MSG_APP_GOT_MOUSE_FOCUS,
            MSG_APP_LOST_VISIBILITY
        ]
    );
    assert_eq!(
        focus_messages([false; 3], [true; 3]),
        [
            MSG_APP_GOT_INPUT_FOCUS,
            MSG_APP_GOT_MOUSE_FOCUS,
            MSG_APP_GOT_VISIBILITY
        ]
    );
}

#[test]
fn engine_flags_and_types() {
    assert!(dev_hud_active(false, Some(b'1')));
    assert!(!dev_hud_active(false, Some(b'0')) && !dev_hud_active(false, None));
    assert!(dev_hud_active(true, None));
    assert_eq!(engine_type_from_string(b"Vector3"), 3);
    assert_eq!(engine_type_from_string(b"vector3"), 8);
    assert_eq!(engine_type_from_string(b""), 8);
}

#[test]
fn setup_vars_first_insert_wins() {
    let mut v = SetupVars::default();
    v.add_string(b"a", b"1");
    v.add_string(b"a", b"2");
    v.add_int(b"i", -7);
    v.add_float(b"f", 1.5);
    v.add_bool(b"b", true);
    assert_eq!(v.get_string(b"a"), b"1");
    assert_eq!(v.get_string(b"missing"), b"");
    assert_eq!(v.get_int(b"i", 0), -7);
    assert_eq!(v.get_float(b"f", 0.0), 1.5);
    assert_eq!(v.get_string(b"f"), b"1.50000000000000000000");
    assert!(v.get_bool(b"b", false));
    assert_eq!(v.get_int(b"missing", 9), 9);
    v.add_string(b"empty", b"");
    assert_eq!(v.get_float(b"empty", 3.0), 3.0);
    v.add_string(b"t", b"TRUE");
    assert!(v.get_bool(b"t", false));
    v.add_string(b"junk", b"12abc");
    assert_eq!(v.get_int(b"junk", 0), 12);
}

fn entry(val: f32, types: u32) -> Entry {
    Entry {
        val,
        dest: val,
        speed: 0.0,
        types,
        destroy_at_dest: false,
    }
}

#[test]
fn entry_layout_matches_cpp() {
    assert_eq!(ENTRY_OFFSETS, [0, 4, 8, 12, 16]);
    assert_eq!(size_of::<Entry>(), 20);
}

#[test]
fn calc_results_modes() {
    let s = [
        Some(entry(2.0, 1)),
        None,
        Some(entry(3.0, 3)),
        Some(entry(5.0, 2)),
    ];
    assert_eq!(calc_results(&s, 1, 0, -1.0), 6.0);
    assert_eq!(calc_results(&s, 3, 1, -1.0), 2.0);
    assert_eq!(calc_results(&s, 3, 2, -1.0), 5.0);
    assert_eq!(calc_results(&s, 3, 3, -1.0), 2.5);
    assert_eq!(calc_results(&s, 4, 3, -1.0), -1.0);
    assert_eq!(calc_results(&[], 1, 0, 7.0), 7.0);
    assert_eq!(calc_results(&s, 1, 9, -1.0), 0.0);
}

#[test]
fn entry_fade_and_update() {
    let mut e = entry(1.0, 1);
    e.fade_to(0.0, 0.5, true);
    assert_eq!(e.speed, -0.5);
    assert!(!e.update(1.0));
    assert_eq!(e.val, 0.5);
    assert!(e.update(2.0));
    assert_eq!(e.val, 0.0);
    let mut up = entry(0.0, 1);
    up.fade_to(1.0, -3.0, false);
    assert_eq!(up.speed, 3.0);
    assert!(!up.update(1.0));
    assert_eq!(up.val, 1.0);
    up.set_val_and_dest(4.0);
    assert_eq!((up.val, up.dest), (4.0, 4.0));
}

#[test]
fn slot_decisions() {
    use SlotAction::*;
    assert_eq!(get_slot_action(-1, 3, false, true), Null);
    assert_eq!(get_slot_action(3, 3, false, true), ResizeAndCreate);
    assert_eq!(get_slot_action(3, 3, false, false), Null);
    assert_eq!(get_slot_action(1, 3, false, true), Create);
    assert_eq!(get_slot_action(1, 3, false, false), Null);
    assert_eq!(get_slot_action(1, 3, true, false), Existing);
    assert_eq!(create_slot(&[true, false, true]), 1);
    assert_eq!(create_slot(&[true, true]), 2);
    assert_eq!(create_slot(&[]), 0);
}

#[test]
fn registry_orders_by_key_then_insertion() {
    let mut r = Registry::default();
    for (k, o) in [(5, 50), (1, 10), (5, 51), (3, 30), (1, 11)] {
        r.add(k, o);
    }
    assert_eq!((r.get(5), r.get(2), r.len()), (Some(50), None, 5));
    let mut c = Cursor::default();
    assert!(r.has_next(&c));
    assert_eq!(r.peek(&c), Some(10));
    let order: Vec<usize> = std::iter::from_fn(|| r.next(&mut c)).collect();
    assert_eq!(order, [10, 11, 30, 50, 51]);
    assert!(!r.has_next(&c));
    r.clear();
    assert!(r.is_empty());
}

#[test]
fn registry_iterator_sees_later_inserts() {
    let mut r = Registry::default();
    r.add(1, 10);
    r.add(5, 50);
    let mut c = Cursor::default();
    assert_eq!(r.next(&mut c), Some(10));
    r.add(3, 30);
    r.add(0, 1);
    assert_eq!(r.next(&mut c), Some(30));
    assert_eq!(r.next(&mut c), Some(50));
}

#[test]
fn set_up_all_visits_added_objects_and_returns_max() {
    let mut r = Registry::default();
    r.add(1, 10);
    r.add(2, 20);
    let p: *mut Registry = &mut r;
    let ids = [(10usize, 4), (20, -3), (30, 9)];
    // SAFETY: `p` is live; the visitor adds through it between steps.
    let max = unsafe {
        set_up_all(p, &mut |o| {
            if o == 10 {
                (*p).add(7, 30);
            }
            ids.iter().find(|(x, _)| *x == o).map_or(0, |(_, id)| *id)
        })
    };
    assert_eq!(max, 9);
    assert_eq!(unsafe { set_up_all(&raw mut r, &mut |_| -5) }, 0);
}

#[test]
fn save_ids_wrap_to_zero() {
    let mut c = 0;
    assert_eq!((next_id(&mut c), next_id(&mut c), c), (0, 1, 2));
    c = i32::MAX;
    assert_eq!(next_id(&mut c), i32::MAX);
    assert_eq!(c, 0);
}

#[test]
fn hud_geometry() {
    assert_eq!(hud_font_size(720), 14.0);
    assert_eq!(hud_font_size(2160), 36.0);
    let l = hud_layout(20.0, 100.0, 5);
    assert_eq!(
        (l.pad, l.line_h, l.bg_w, l.bg_h),
        (10.0, 25.0, 120.0, 145.0)
    );
    assert_eq!((l.text_x, l.text_y), (20.0, 20.0));
    let m = bug_metrics(1920, 1080);
    assert_eq!((m.font, m.pad, m.line_h), (24.0, 18.0, 30.0));
    assert_eq!((m.box_w, m.text_w), (1152.0, 1116.0));
    assert_eq!(toast_box_w(&m, 2000.0), 2036.0);
    assert_eq!(toast_box_w(&m, 10.0), 1152.0);
    let b = bug_box(&m, 1152.0, 2, 1920, 1080);
    assert_eq!((b.x, b.y, b.h), (384.0, 270.0, 156.0));
    assert_eq!((b.text_x, b.text_y), (402.0, 288.0));
}

#[test]
fn stats_line_is_printf_text() {
    assert_eq!(
        stats_line(1920, 1080, 59.6, 16.78),
        b"1920x1080  |  60 FPS  16.8 ms"
    );
    assert_eq!(
        stats_line(1, 2, f32::INFINITY, f32::NAN),
        b"1x2  |  inf FPS  nan ms"
    );
}

#[test]
fn toast_and_cursor() {
    assert!(!toast_active(0, 5));
    assert!(toast_active(1000, 4999));
    assert!(!toast_active(1000, 5000));
    assert_eq!((cursor_char(499), cursor_char(500)), (32, 95));
    assert!(!should_render(false, false, false) && should_render(false, true, false));
}

#[test]
fn wrap_cuts_at_last_space() {
    let mut width = |l: &[u32]| l.len() as f32;
    let w = wrap_bug_text(&u8s("hello big world"), 8.0, &mut width);
    let s: Vec<String> = w
        .iter()
        .map(|l| l.iter().filter_map(|c| char::from_u32(*c)).collect())
        .collect();
    assert_eq!(s, ["hello ", "big ", "world"]);
    let w = wrap_bug_text(&u8s("abcdefghijkl\nx"), 5.0, &mut width);
    assert_eq!(w.len(), 4);
    let w = wrap_bug_text(&u8s(" abcdefgh"), 4.0, &mut width);
    assert_eq!(w.first().map(Vec::len), Some(4));
    assert_eq!(wrap_bug_text(&[], 5.0, &mut width), [Vec::<u32>::new()]);
}

#[test]
fn wrap_single_wide_char_pushes_empty_lines() {
    let w = wrap_bug_text(&u8s("ab"), -1.0, &mut |_| 1.0);
    assert_eq!(w, [vec![], u8s("a"), u8s("b")]);
}

#[test]
fn ctrl_backspace_words() {
    let run = |s: &str| {
        let mut v = u8s(s);
        ctrl_backspace(&mut v);
        v
    };
    assert_eq!(run("foo bar"), u8s("foo "));
    assert_eq!(run("foo bar  "), u8s("foo "));
    assert_eq!(run("foo\nbar"), u8s("foo\n"));
    assert_eq!(run("word"), u8s(""));
    assert_eq!(run("  "), u8s(" "));
    assert_eq!(run(""), vec![0]);
}

#[test]
fn key_routing() {
    use KeyAction::*;
    assert_eq!(key_action(27, false, false, true), Close);
    assert_eq!(key_action(13, false, true, true), NewLine);
    assert_eq!(key_action(0x4000_0058, false, false, true), Save);
    assert_eq!(key_action(8, true, false, true), CtrlBackspace);
    assert_eq!(key_action(8, false, false, true), Nothing);
    assert_eq!(key_action(8, false, false, false), Backspace);
    assert_eq!(key_action(118, true, false, false), PasteIfClipboard);
    assert_eq!(key_action(118, false, false, false), Nothing);
    assert!(swallowed_event(EV_MOUSEWHEEL) && !swallowed_event(EV_KEYDOWN));
    assert!(opens_report(EV_KEYDOWN, SDLK_F9, false));
    assert!(
        !opens_report(EV_KEYDOWN, SDLK_F9, true) && !opens_report(EV_TEXTINPUT, SDLK_F9, false)
    );
}

#[test]
fn system_info_text() {
    assert_eq!(first_line(b"Model X\nsecond"), b"Model X");
    assert_eq!(first_line(b"ab\0cd\n"), b"ab");
    assert_eq!(first_line(b""), b"");
    assert_eq!(
        os_name(b"NAME=x\nPRETTY_NAME=\"Fedora Linux 45\"\nID=f"),
        b"Fedora Linux 45"
    );
    assert_eq!(os_name(b"NAME=x\n"), b"Linux");
    assert_eq!(os_name(b""), b"Linux");
    assert_eq!(os_name(b"X_PRETTY_NAME=a\nPRETTY_NAME=b"), b"b");
    assert_eq!(mem_gb(4_000_000, 16384), 61);
    let lines = system_lines(&SystemInfo {
        version: b"1.2.3",
        device: b"Mac",
        machine: b"aarch64",
        cores: 10,
        mem_gb: 32,
        os: b"Fedora",
        kernel: b"7.2",
    });
    assert_eq!(lines[0], b"Open HPL 1.2.3");
    assert_eq!(lines[1], b"Mac  |  aarch64, 10 cores, 32 GB");
    assert_eq!(lines[2], b"Fedora  |  Linux 7.2");
}

#[test]
fn report_names_and_env() {
    let taken = ["20260101-120000", "20260101-120000-2"];
    let n = unique_name(b"20260101-120000", &mut |s| {
        taken.iter().any(|t| t.as_bytes() == s)
    });
    assert_eq!(n, b"20260101-120000-3");
    assert_eq!(
        files_json(true, true),
        br#""screenshot":"screenshot.png","log":"hpl.log""#
    );
    assert_eq!(files_json(false, true), br#""log":"hpl.log""#);
    assert_eq!(files_json(false, false), b"");
    assert_eq!(
        env_pair(b"OPENHPL_A=b=c"),
        Some((&b"OPENHPL_A"[..], &b"b=c"[..]))
    );
    assert_eq!(env_pair(b"OHPL_X"), Some((&b"OHPL_X"[..], &b"OHPL_X"[..])));
    assert_eq!(env_pair(b"HOME=/"), None);
    assert_eq!(cmdline_args(b"a\0\0b\0"), [&b"a"[..], b"", b"b"]);
    assert_eq!(cmdline_args(b"a\0b"), [&b"a"[..], b"b"]);
    assert!(cmdline_args(b"").is_empty());
}

#[test]
fn report_json_shape() {
    let args: [&[u8]; 1] = [b"game \"x\""];
    let env: [(&[u8], &[u8]); 1] = [(b"OHPL_A", b"1")];
    let sys: [&[u8]; 2] = [b"a", b"b"];
    let rep = Report {
        id: b"id",
        time: b"t",
        description: b"line\nbreak",
        version: b"1",
        exe: b"/bin/x",
        cwd: b"/",
        args: &args,
        env: &env,
        system: &sys,
        resolution: (1280, 720),
        fps: 60.0,
        frame_ms: 16.5,
        game_time: 2.0,
        scene: Some(Scene {
            map: b"m.hpm",
            pos: [1.0, 2.0, 3.0],
            forward: [0.0, 0.0, -1.0],
            world_json: b"{}",
            render_json: b"{\"r\":1}",
        }),
        files: b"",
    };
    let want = "{\"id\":\"id\",\"time\":\"t\",\"description\":\"line\\nbreak\",\"version\":\"1\",\"exe\":\"/bin/x\",\"cwd\":\"/\",\"args\":[\"game \\\"x\\\"\"],\"env\":{\"OHPL_A\":\"1\"},\"system\":[\"a\",\"b\"],\"resolution\":[1280,720],\"fps\":60.00000000000000000000,\"frame_ms\":16.50000000000000000000,\"game_time\":2.00000000000000000000,\"map\":\"m.hpm\",\"camera\":{\"pos\":[1.00000000000000000000,2.00000000000000000000,3.00000000000000000000],\"forward\":[0.00000000000000000000,0.00000000000000000000,-1.00000000000000000000]},\"world\":{},\"render\":{\"r\":1},\"files\":{}}";
    assert_eq!(String::from_utf8_lossy(&report_json(&rep)), want);
    let rep = Report { scene: None, ..rep };
    assert!(report_json(&rep).ends_with(b",\"files\":{}}"));
    assert_eq!(
        index_line(b"i", b"", b"d", b"/p"),
        b"{\"id\":\"i\",\"map\":\"\",\"description\":\"d\",\"dir\":\"/p\"}\n"
    );
}
