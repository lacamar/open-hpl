use super::*;
use crate::luxbase::ffi::{Arg, Ret, ohpl_ga9_ret_bytes, ohpl_ga9_ret_wide};
use std::cell::RefCell;
use std::collections::HashMap;
use std::ffi::{c_char, c_int, c_void};

unsafe extern "C" {
    fn snprintf(buf: *mut c_char, n: usize, fmt: *const c_char, ...) -> c_int;
}

fn c_fmt(x: f64, prec: i32) -> String {
    let mut buf = [0u8; 512];
    // SAFETY: `buf` is large enough for the values below; the format takes `int, double`.
    unsafe {
        snprintf(
            buf.as_mut_ptr().cast::<c_char>(),
            buf.len(),
            c"%.*f".as_ptr(),
            prec,
            x,
        )
    };
    let end = buf.iter().position(|&c| c == 0).unwrap_or(buf.len());
    String::from_utf8_lossy(&buf[..end]).into_owned()
}

#[test]
fn float_text_matches_libc() {
    let vals = [
        0.0,
        -0.0,
        1.0,
        -1.5,
        0.1,
        0.005,
        0.015,
        0.025,
        2.5,
        3.5,
        100.0,
        1e-7,
        123456.789,
        -987654.321,
        f64::from(f32::MAX),
        f64::from(f32::MIN_POSITIVE),
        f64::from(0.893_091_1_f32),
        f64::from(-3.141_582_7_f32),
        f64::NAN,
        -f64::NAN,
        f64::INFINITY,
        f64::NEG_INFINITY,
    ];
    for v in vals {
        for p in [0, 1, 2, 6] {
            assert_eq!(cf(v, p as usize), c_fmt(v, p), "{v} at {p}");
        }
    }
}

#[test]
fn vec3_text_matches_libc() {
    let s = vec3_str([0.0, 0.893_091_1, -64.520_004]);
    assert_eq!(s, "0.000000 : 0.893091 : -64.520004");
}

#[test]
fn pixel_format_names_match_the_enum() {
    assert_eq!(pixel_format(0), "Unknown");
    assert_eq!(pixel_format(3), "LuminanceALpha");
    assert_eq!(pixel_format(8), "DXT1");
    assert_eq!(pixel_format(12), "DXT5");
    assert_eq!(pixel_format(13), "Unknown");
    assert_eq!(pixel_format(14), "Depth16");
    assert_eq!(pixel_format(26), "RGBA32");
    assert_eq!(pixel_format(27), "Unknown");
    assert_eq!(pixel_format(-1), "Unknown");
}

#[test]
fn box_checks_keep_the_min_for_both_ends() {
    let ob = ([1.0, 1.0, 1.0], [9.0, 9.0, 9.0]);
    assert!(inside_box(ob, [0.0; 3], [2.0; 3]));
    assert!(!inside_box(ob, [1.5, 0.0, 0.0], [2.0; 3]));
    assert!(!inside_box(ob, [0.0; 3], [0.5, 2.0, 2.0]));
    assert_eq!(outside_amount(ob, [0.0; 3], [2.0; 3]), [0.0; 3]);
    assert_eq!(
        outside_amount(ob, [1.5, 0.0, 0.0], [2.0; 3]),
        [-0.5, 0.0, 0.0]
    );
    assert_eq!(
        outside_amount(ob, [0.0; 3], [0.5, 2.0, 2.0]),
        [0.5, 0.0, 0.0]
    );
}

#[derive(Default)]
struct Fake {
    widgets: RefCell<Vec<Widget>>,
    draws: RefCell<Vec<(f32, f32, String)>>,
    ops: RefCell<Vec<u32>>,
    q: HashMap<i32, f64>,
    qs: HashMap<i32, &'static str>,
}

#[derive(Clone, Debug, Default)]
struct Widget {
    kind: i32,
    text: String,
    pos: [f32; 3],
    size: [f32; 2],
    uv: i32,
    checked: bool,
    sized: bool,
}

fn argw(a: &Arg) -> String {
    // SAFETY: the callers pass `aw` arguments: `n` wide units at `p`.
    let w = unsafe { std::slice::from_raw_parts(a.p.cast::<u32>(), a.n) };
    String::from_utf8_lossy(&string::wide_to_utf8(w)).into_owned()
}

unsafe extern "C" fn fake_call(ctx: *mut c_void, op: u32, a: *const Arg, n: usize, r: *mut Ret) {
    // SAFETY: `ctx` is the `Fake` of the test and outlives the call; `a` holds `n` arguments.
    let (fake, a, r) = unsafe {
        (
            &*ctx.cast::<Fake>(),
            std::slice::from_raw_parts(a, n),
            &mut *r,
        )
    };
    fake.ops.borrow_mut().push(op);
    let i = |k: usize| a.get(k).map_or(0, |a| a.i);
    match op {
        dop::CFG_DEBUG => r.i = 1,
        dop::GUI_VIRT_W => r.f = 800.0,
        dop::OCCL_GET => r.i = 1,
        dop::W_NEW => {
            let f = |k: usize| a.get(k).map_or(0.0, |a| a.f);
            let mut w = fake.widgets.borrow_mut();
            w.push(Widget {
                kind: i(0) as i32,
                text: a.get(7).map(argw).unwrap_or_default(),
                pos: [f(1), f(2), f(3)],
                size: [f(4), f(5)],
                ..Widget::default()
            });
            r.i = w.len() as i64 - 1;
        }
        dop::W_CHECKED => {
            if let Some(w) = fake.widgets.borrow_mut().get_mut(i(0) as usize) {
                w.checked = i(1) != 0;
            }
        }
        dop::W_UV => {
            if let Some(w) = fake.widgets.borrow_mut().get_mut(i(0) as usize) {
                w.uv = i(1) as i32;
            }
        }
        dop::W_SIZE => {
            if let Some(w) = fake.widgets.borrow_mut().get_mut(i(0) as usize) {
                w.size = [a[1].f, a[2].f];
                w.sized = true;
            }
        }
        dop::INS_COUNT | dop::W_COMBO_GET => r.i = 0,
        dop::DRAW_FMT => fake.draws.borrow_mut().push((a[0].f, a[1].f, argw(&a[3]))),
        dop::Q => r.d = fake.q.get(&(i(0) as i32)).copied().unwrap_or(0.0),
        dop::QS => {
            let s = fake.qs.get(&(i(0) as i32)).copied().unwrap_or("");
            let w = string::utf8_to_wide(s.as_bytes());
            // SAFETY: `r` is the live `Ret` of this call.
            unsafe { ohpl_ga9_ret_wide(r, w.as_ptr(), w.len()) };
        }
        dop::STATE_DRAW => r.f = a[0].f + 30.0,
        dop::SND_SNAP | dop::ENT_SNAP => r.i = 0,
        dop::LOG => {}
        _ => {
            // SAFETY: as above.
            unsafe { ohpl_ga9_ret_bytes(r, std::ptr::null(), 0) };
        }
    }
}

fn host_of(f: &Fake) -> Host {
    Host {
        ctx: std::ptr::from_ref(f).cast_mut().cast(),
        call: fake_call,
    }
}

fn debug_with(f: impl FnOnce(&mut Debug)) -> Box<Debug> {
    let mut d = Box::new(Debug::default());
    d.f.reload_from_current_position = true;
    f(&mut d);
    d
}

fn golden_line(w: &Widget) -> String {
    let kind = [
        "window", "frame", "group", "checkbox", "button", "combobox", "slider",
    ]
    .get(w.kind as usize)
    .copied()
    .unwrap_or("label");
    let text = if matches!(w.kind, 5 | 6) {
        "*"
    } else {
        &w.text
    };
    let mut s = format!("{kind} '{text}'");
    s += &format!(
        " pos={},{},{} uv={} checked={}",
        w.pos[0],
        w.pos[1],
        w.pos[2],
        w.uv,
        i32::from(w.checked)
    );
    if matches!(w.kind, 0 | 1 | 2 | 4) {
        s += &format!(" size={},{}", w.size[0], w.size[1]);
    }
    s
}

/// The `w` lines of a real C++ run (rust/tests/golden/A12d/slice.py): the widgets the handler
/// creates, in order, with the positions, sizes, user values and checks it gives them.
#[test]
fn window_matches_the_cpp_dump() {
    let fake = Fake::default();
    let mut d = debug_with(|_| {});
    create_gui_window(&raw mut *d, &host_of(&fake));
    let got: Vec<String> = fake.widgets.borrow().iter().map(golden_line).collect();
    let want: Vec<String> = include_str!("../../tests/golden/A12d/tree.txt")
        .lines()
        .map(str::to_owned)
        .collect();
    // the golden prints floats with %.9g: compare as numbers
    let norm = |l: &str| -> String {
        l.split(' ')
            .map(|t| {
                t.split_once('=').map_or_else(
                    || t.to_owned(),
                    |(k, v)| {
                        let nums: Vec<String> = v
                            .split(',')
                            .map(|n| {
                                n.parse::<f32>()
                                    .map_or_else(|_| n.to_owned(), |x| format!("{x}"))
                            })
                            .collect();
                        format!("{k}={}", nums.join(","))
                    },
                )
            })
            .collect::<Vec<_>>()
            .join(" ")
    };
    assert_eq!(got.len(), want.len());
    for (g, w) in got.iter().zip(&want) {
        assert_eq!(norm(g), norm(w));
    }
    assert!(d.ui.acts.len() > 20);
}

fn q_player() -> HashMap<i32, f64> {
    HashMap::from([
        (q::ON_GROUND, 1.0),
        (q::MASS, 70.0),
        (q::INTERACT_MUL, 1.0),
        (q::AVG_SPEED, 0.131_328),
        (q::MOVE_NORMAL, 1.0),
        (q::MOVE_MUL, 1.0),
        (q::RUN_MUL, 1.0),
        (q::EXT_LIGHT, f64::from(0.486_018_f32)),
        (q::NORM_LIGHT, f64::from(0.328_978_f32)),
        (q::HEALTH, 100.0),
        (q::OIL, 100.0),
        (q::SANITY, 100.0),
    ])
}

/// Lines of the real C++ overlay (the a12d dump, 02_library, all overlays on).
#[test]
fn player_overlay_matches_the_cpp_dump() {
    let fake = Fake {
        q: q_player(),
        qs: HashMap::from([
            (q::S_POS, "0.000000 : 0.893091 : 1.000000"),
            (q::S_FORCE_VEL, "0.000000 : 0.000000 : 0.000000"),
        ]),
        ..Fake::default()
    };
    let end = draw_player_info(&host_of(&fake), 18.0);
    let got: Vec<(f32, String)> = fake
        .draws
        .borrow()
        .iter()
        .map(|(_, y, t)| (*y, t.clone()))
        .collect();
    let want = [
        (
            18.0,
            "Position: 0.000000 : 0.893091 : 1.000000 OnGround: 1 Moved: 0 Mass: 70.000000 ActiveSize: 0\n",
        ),
        (
            33.0,
            "MoveSpeed: 0.000000, 0.000000 (0.000000) Mul: 1.000000 AvgSpeed: 0.131328\n",
        ),
        (48.0, "Climbing: 0\n"),
        (
            63.0,
            "Crouching: 0 Running: 0 MoveSpeedMul 1.000000 RunSpeedMul 1.000000\n",
        ),
        (78.0, "ForceVel: 0.000000 : 0.000000 : 0.000000\n"),
        (93.0, "ExtLightLevel: 0.486018 NormalLightlevel: 0.328978\n"),
        (108.0, "Health: 100.000000 Terror: 0.000000\n"),
        (123.0, "Oil: 100.000000 Sanity: 100.000000 Tinderboxes: 0"),
        (138.0, "State: 0\n"),
        (153.0, "HeadBob: 0.000000 cos: 1.000000 sin: 0.000000\n"),
        (
            168.0,
            "Sanity Between event time: 0.000000 AtLowSanityCount: 0.000000\n",
        ),
        (183.0, "Hardcore mode: 0 "),
    ];
    assert_eq!(got.len(), want.len());
    for ((gy, gt), (wy, wt)) in got.iter().zip(want) {
        assert_eq!(*gy, wy);
        assert_eq!(gt, wt);
    }
    assert_eq!(end, 198.0 + 30.0);
}

#[test]
fn frame_time_line_comes_first_and_moves_the_cursor() {
    let fake = Fake {
        q: HashMap::from([(q::FRAME_MS, 53.5), (q::FPS, 18.7)]),
        ..Fake::default()
    };
    let mut d = debug_with(|d| d.f.show_fps = true);
    on_draw(&raw mut *d, &host_of(&fake));
    let draws = fake.draws.borrow();
    assert_eq!(draws.len(), 1);
    assert_eq!(draws[0].2, "FrameTime: 53.5ms FPS: 18.7\n");
}

#[test]
fn off_flags_draw_nothing_and_touch_no_engine_state() {
    let fake = Fake::default();
    let mut d = debug_with(|_| {});
    on_draw(&raw mut *d, &host_of(&fake));
    assert!(fake.draws.borrow().is_empty());
    assert!(fake.ops.borrow().is_empty());
    render_solid(&raw mut *d, &host_of(&fake));
    assert!(fake.ops.borrow().is_empty());
}

#[test]
fn head_bob_sin_cos_use_the_float_functions() {
    let fake = Fake {
        q: {
            let mut m = q_player();
            m.insert(q::HEADBOB, f64::from(1.25_f32));
            m
        },
        ..Fake::default()
    };
    draw_player_info(&host_of(&fake), 0.0);
    let t = fake
        .draws
        .borrow()
        .iter()
        .find(|d| d.2.starts_with("HeadBob"))
        .map(|d| d.2.clone())
        .unwrap_or_default();
    let b = 1.25_f32;
    assert_eq!(
        t,
        format!(
            "HeadBob: 1.250000 cos: {} sin: {}\n",
            c_fmt(f64::from(b.cos()), 6),
            c_fmt(f64::from(b.sin()), 6)
        )
    );
}
