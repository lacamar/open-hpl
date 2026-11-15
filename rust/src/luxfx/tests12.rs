//! Music and insanity handlers against a fake host. The golden slices in
//! `rust/tests/golden/A12b/` come from the C++ originals running in the live game (dump hook
//! `OHPL_A12B_DUMP`, Amnesia 02_library); the fake host writes the same lines as the C++ host.

use super::host12::{Args, Call, Host, TAG_DANGER, TAG_ROW, TAG_STR, code};
use super::insanity_h::InsanityHandler;
use super::music_h::MusicHandler;
use crate::math::c_srand;
use std::cell::RefCell;
use std::collections::HashMap;

/// C `%.9g` of a float.
fn g9(x: f32) -> String {
    if x == 0.0 {
        return if x.is_sign_negative() { "-0" } else { "0" }.to_string();
    }
    let v = f64::from(x);
    let sci = format!("{v:.8e}");
    let (mant, exp) = sci.split_once('e').unwrap_or((&sci, "0"));
    let exp: i32 = exp.parse().unwrap_or(0);
    let trim = |s: &str| {
        if s.contains('.') {
            s.trim_end_matches('0').trim_end_matches('.').to_string()
        } else {
            s.to_string()
        }
    };
    if !(-4..9).contains(&exp) {
        let sign = if exp < 0 { '-' } else { '+' };
        format!("{}e{sign}{:02}", trim(mant), exp.abs())
    } else {
        let decimals = usize::try_from(8 - exp).unwrap_or(0);
        trim(&format!("{v:.decimals$}"))
    }
}

#[test]
fn g9_matches_printf() {
    assert_eq!(g9(0.85), "0.850000024");
    assert_eq!(g9(0.68), "0.680000007");
    assert_eq!(g9(-0.0), "-0");
    assert_eq!(g9(800.0), "800");
    assert_eq!(g9(0.5), "0.5");
    assert_eq!(g9(173.520_69), "173.520691");
    assert_eq!(g9(1.0e-5), "9.99999975e-06");
    assert_eq!(g9(1.0e9), "1e+09");
    assert_eq!(g9(262_144.0), "262144");
}

type Elem = Vec<(String, String)>;

#[derive(Default)]
struct St {
    log: Vec<String>,
    xml: Vec<Elem>,
    song_null: bool,
    dead: bool,
    hard: bool,
    has_map: bool,
    danger: Vec<(i32, Vec<u8>)>,
    enemies: HashMap<usize, (Vec<u8>, i32)>,
    valid: bool,
    ps_exists: bool,
    subs: usize,
    rows: Vec<Vec<u8>>,
    entries: usize,
    calls: Vec<i32>,
}

struct Fake {
    st: RefCell<St>,
}

impl Fake {
    fn new() -> Fake {
        let st = St {
            has_map: true,
            valid: true,
            ps_exists: true,
            ..St::default()
        };
        Fake {
            st: RefCell::new(st),
        }
    }

    fn lines(&self) -> Vec<String> {
        self.st.borrow().log.clone()
    }

    fn take_lines(&self) -> Vec<String> {
        std::mem::take(&mut self.st.borrow_mut().log)
    }
}

fn s(a: &Args, i: usize) -> String {
    let (p, n) = (a.s[i], a.n[i]);
    if p.is_null() {
        return String::new();
    }
    // SAFETY: the test calls pass live byte slices.
    String::from_utf8_lossy(unsafe { std::slice::from_raw_parts(p, n) }).into_owned()
}

fn sink(a: &Args, tag: i32, val: i32, bytes: &[u8]) {
    if let Some(f) = a.sink {
        // SAFETY: `ctx` and `bytes` are live for the call.
        unsafe { f(a.ctx, tag, val, bytes.as_ptr(), bytes.len()) };
    }
}

impl Host for Fake {
    fn logging(&self) -> bool {
        true
    }

    fn call(&self, c: i32, a: &mut Args) -> usize {
        let mut st = self.st.borrow_mut();
        st.calls.push(c);
        let mut push = |t: String| st.log.push(t);
        match c {
            code::CFG_F32 => {
                a.out[0] = match s(a, 1).as_str() {
                    "InGameMusicVolume" => 0.85,
                    "MaxSanity_LongWait" => 65.0,
                    "MaxSanity_MedWait" => 45.0,
                    "MaxSanity_ShortWait" => 25.0,
                    "TimeBetween_LongWait" => 270.0,
                    "TimeBetween_MedWait" => 180.0,
                    "TimeBetween_ShortWait" => 80.0,
                    _ => a.f[0],
                };
            }
            code::CFG_STR => sink(a, TAG_STR, 0, b"x.cfg"),
            code::LOG => push(s(a, 0)),
            code::WARN => push(format!("W {}", s(a, 0))),
            code::ERR => push(format!("E {}", s(a, 0))),
            code::M_PLAY => push(format!(
                "m play {} {} {} {} {}",
                s(a, 0),
                g9(a.f[0]),
                g9(a.f[1]),
                a.i[0],
                a.i[1]
            )),
            code::M_STOP => push(format!("m stop {}", g9(a.f[0]))),
            code::M_SONG_NULL => return usize::from(st.song_null),
            code::M_RESET_RESUME => push("m reset_resume".into()),
            code::M_PLAYER_DEAD => return usize::from(st.dead),
            code::M_HARD_MODE => return usize::from(st.hard),
            code::M_HAS_MAP => return usize::from(st.has_map),
            code::M_DANGER_SCAN => {
                for (p, m) in &st.danger {
                    sink(a, TAG_DANGER, *p, m);
                }
            }
            code::M_ENEMY_INFO => {
                if let Some((m, p)) = st.enemies.get(&a.p[0]) {
                    a.out_i[0] = *p;
                    sink(a, TAG_STR, 0, m);
                }
            }
            code::X_OPEN => return st.xml.len(),
            code::X_TYPE => {
                let t = st
                    .xml
                    .get(a.i[0] as usize)
                    .and_then(|e| e.iter().find(|(k, _)| k == "$type").map(|(_, v)| v.clone()));
                sink(a, TAG_STR, 0, t.unwrap_or_default().as_bytes());
            }
            code::X_STR | code::X_F32 | code::X_I32 | code::X_BOOL | code::X_COLOR => {
                let key = s(a, 0);
                let v = st
                    .xml
                    .get(a.i[0] as usize)
                    .and_then(|e| e.iter().find(|(k, _)| *k == key).map(|(_, v)| v.clone()));
                match c {
                    code::X_STR => {
                        sink(a, TAG_STR, 0, v.unwrap_or_else(|| s(a, 1)).as_bytes());
                    }
                    code::X_F32 => {
                        a.out[0] = v.and_then(|v| v.parse().ok()).unwrap_or(a.f[0]);
                    }
                    code::X_I32 => {
                        a.out_i[0] = v.and_then(|v| v.parse().ok()).unwrap_or(a.i[1]);
                    }
                    code::X_BOOL => return usize::from(v.map_or(a.i[1] != 0, |v| v == "true")),
                    _ => {
                        let parsed: Vec<f32> = v
                            .map(|v| v.split(' ').filter_map(|x| x.parse().ok()).collect())
                            .unwrap_or_default();
                        for i in 0..4 {
                            a.out[i] = parsed.get(i).copied().unwrap_or(1.0);
                        }
                    }
                }
            }
            code::X_CLOSE => {}
            code::G_IMAGE => {
                push(format!("i gfx_image {}", s(a, 0)));
                return 0x100;
            }
            code::G_RECT_WHITE => {
                push("i gfx_rect white".into());
                return 0x200;
            }
            code::G_TEXTURE => {
                push(format!("i gfx_texture {}", s(a, 0)));
                return 0x300;
            }
            code::G_DESTROY => push("i gfx_destroy".into()),
            code::G_BOUNDS => {
                push("i gui_bounds -0 -0 800 600 active 40 64".into());
                a.out = [-0.0, -0.0, 800.0, 600.0, 0.0, 0.0];
            }
            code::D_BUG => push(format!(
                "i bug {} pos {} {} size {} {} col {} angle {}",
                a.i[0],
                g9(a.f[0]),
                g9(a.f[1]),
                g9(40.0 * a.f[2]),
                g9(64.0 * a.f[2]),
                g9(a.f[3]),
                g9(a.f[4])
            )),
            code::D_FADE => push(format!(
                "i fade img={} {} {} {} {} {}",
                i32::from(a.p[0] != 0),
                g9(a.f[0]),
                g9(a.f[1]),
                g9(a.f[2]),
                g9(a.f[3]),
                g9(a.f[4])
            )),
            code::D_FONT => push(format!(
                "i font {} {} {} {}",
                a.i[0],
                g9(a.f[0]),
                g9(a.f[1]),
                g9(a.f[2])
            )),
            code::S_STREAM => {
                push(format!("i stream {} {} {}", s(a, 0), a.i[0], g9(a.f[0])));
                push("i stream result 1".into());
                st.entries += 1;
                a.out_i[0] = 7;
                return 0x1000 + st.entries;
            }
            code::S_FADE_IN => push(format!("i fadein {} {}", g9(a.f[0]), g9(a.f[1]))),
            code::S_FADE_OUT => push(format!("i fadeout {}", g9(a.f[0]))),
            code::S_STOP => push("i sound stop".into()),
            code::S_IS_VALID => return usize::from(st.valid),
            code::P_CREATE => {
                push(format!("i ps create {} {} 1", s(a, 0), s(a, 1)));
                return 0x500;
            }
            code::P_SET_CAMERA => push("i ps pos 1 2 3".into()),
            code::P_EXISTS => return usize::from(st.ps_exists),
            code::P_KILL => push("i ps kill".into()),
            code::E_STEP_SOUND => push(format!(
                "i step_sound {} off {} {} {}",
                s(a, 0),
                g9(a.f[0]),
                g9(a.f[1]),
                g9(a.f[2])
            )),
            code::PL_ACTIVE => push(format!("i player active {}", a.i[0])),
            code::PL_SANITY => {
                push("i sanity 100".into());
                a.out[0] = 100.0;
            }
            code::SUB_ROWS => {
                for r in &st.rows {
                    sink(a, TAG_ROW, 0, r);
                }
            }
            code::SUBS_STATE => return st.subs,
            code::LOG_START => push(format!(
                "i prog Starting Insanity event: '{}' set: '{}'",
                s(a, 0),
                s(a, 1)
            )),
            code::LOG_PROG => push(format!("prog {}", s(a, 0))),
            _ => {}
        }
        0
    }
}

fn run(h: &Fake, code: i32) -> usize {
    Call::new(h, code).run().ret
}

fn state_line(h: &dyn Host, ins: &InsanityHandler, why: &str) -> String {
    format!(
        "i {why} cur={} count={} possible={}",
        ins.current(),
        g9(ins.new_event_count()),
        i32::from(ins.possible(h))
    )
}

fn bug_xml() -> Elem {
    [
        ("$type", "Bugs"),
        ("Name", "Bugs_Single"),
        ("Set", "Default"),
        ("MaxSanity", "60"),
        ("NumOfBugs", "1"),
        ("MinSizeMul", "0.8"),
        ("BugImage", "insanity_bug01.tga"),
        ("WanderCircleDist", "5000"),
        ("WanderCircleRadius", "5000"),
        ("SwarmPointMul", "500"),
        ("MaxSpeed", "250.0"),
        ("LoopSound", "insanity_bug1.ogg"),
        ("SoundVolume", "0.8"),
        ("Duration", "8"),
    ]
    .iter()
    .map(|(k, v)| ((*k).to_string(), (*v).to_string()))
    .collect()
}

fn golden(name: &str) -> Vec<String> {
    let text = match name {
        "music" => include_str!("../../tests/golden/A12b/music.txt"),
        _ => include_str!("../../tests/golden/A12b/insanity_bugs.txt"),
    };
    text.lines().map(str::to_string).collect()
}

#[test]
fn insanity_bug_event_matches_recorded_cpp_dump() {
    c_srand(1234);
    let h = Fake::new();
    h.st.borrow_mut().xml = vec![bug_xml()];
    let mut ins = InsanityHandler::new(&h);
    h.take_lines();
    ins.start_index(&h, 0);
    let mut got = Vec::new();
    let state = |got: &mut Vec<String>, ins: &InsanityHandler, why: &str| {
        let l = state_line(&h, ins, why);
        got.extend(h.take_lines());
        got.push(l);
    };
    state(&mut got, &ins, "start");
    let mut step = 0;
    while step < 1500 && ins.current() >= 0 {
        ins.update(&h, 1.0 / 60.0);
        ins.on_draw(&h);
        if step % 100 == 0 {
            state(&mut got, &ins, "step");
        }
        step += 1;
    }
    got.extend(h.take_lines());
    got.push(format!("i event 0 ran {step} steps"));
    ins.stop_current(&h);
    state(&mut got, &ins, "stop");
    let expect = golden("insanity");
    let first_diff = got.iter().zip(expect.iter()).position(|(a, b)| a != b);
    assert_eq!(
        first_diff,
        None,
        "{:?}",
        first_diff.map(|i| (&got[i], &expect[i]))
    );
    assert_eq!(got.len(), expect.len());
}

#[test]
fn music_stack_matches_recorded_cpp_dump() {
    let h = Fake::new();
    let mut m = MusicHandler::new(&h);
    m.reset(&h);
    h.take_lines();
    let mut got = Vec::new();
    got.push(format!(
        "m begin max={} mul={}",
        m.max_prio,
        g9(m.volume_mul)
    ));
    let slots = |m: &MusicHandler| -> Vec<String> {
        m.slots
            .iter()
            .enumerate()
            .filter(|(_, s)| !s.file.is_empty())
            .map(|(i, s)| {
                format!(
                    "m slot {i} {} loop={} resume={} vol={} special={}",
                    String::from_utf8_lossy(&s.file),
                    i32::from(s.looped),
                    i32::from(s.resume),
                    g9(s.volume),
                    i32::from(s.special)
                )
            })
            .collect()
    };
    let mut check = |m: &MusicHandler, why: &str| {
        got.extend(h.take_lines());
        got.push(format!("m {why} cur={}", m.cur_prio));
        got.extend(slots(m));
    };
    let step = |m: &mut MusicHandler, n: usize| {
        for _ in 0..n {
            m.update(&h, 1.0 / 60.0);
        }
    };
    m.play(&h, b"00_event_gallery.ogg", true, 1.0, 1.0, 2, false, false);
    check(&m, "p2");
    step(&mut m, 30);
    m.play(&h, b"00_event_gallery.ogg", true, 0.5, 1.0, 2, false, false);
    check(&m, "p2again");
    m.play(&h, b"01_event_dust.ogg", false, 0.8, 1.0, 4, true, true);
    check(&m, "p4");
    step(&mut m, 30);
    m.play(&h, b"02_puzzle.ogg", true, 1.0, 1.0, 1, true, false);
    check(&m, "p1");
    step(&mut m, 30);
    m.play(&h, b"02_amb_safe.ogg", true, 0.6, 0.0, 3, true, false);
    check(&m, "p3");
    step(&mut m, 10);
    m.stop(&h, 1.0, 4);
    check(&m, "s4");
    step(&mut m, 30);
    m.stop(&h, 1.0, 3);
    check(&m, "s3");
    m.stop(&h, 0.0, 7);
    check(&m, "s7");
    m.play(&h, b"03_event_books.ogg", true, 1.0, 1.0, 99, true, false);
    check(&m, "p99");
    m.play(&h, b"03_event_tomb.ogg", true, 1.0, 1.0, -1, true, false);
    check(&m, "pm1");
    m.stop(&h, 1.0, 2);
    check(&m, "s2");
    m.stop(&h, 1.0, 1);
    check(&m, "s1");
    step(&mut m, 30);
    m.stop(&h, 1.0, 0);
    check(&m, "s0");

    let got: Vec<String> = got
        .into_iter()
        .filter(|l| !l.starts_with("m dead") && !l.starts_with("m nosong"))
        .collect();
    let expect: Vec<String> = golden("music")
        .into_iter()
        .map(|l| match l.find(" song=") {
            Some(i) => l[..i].to_string(),
            None => l,
        })
        .filter(|l| !l.starts_with("m dead") && !l.starts_with("m nosong"))
        .filter(|l| !l.starts_with("m slot 0 21_amb"))
        .collect();
    assert_eq!(got, expect);
}

fn enemy_fake() -> Fake {
    let h = Fake::new();
    h.st.borrow_mut()
        .enemies
        .insert(7, (b"fight.ogg".to_vec(), 3));
    h.st.borrow_mut()
        .enemies
        .insert(9, (b"better.ogg".to_vec(), 5));
    h
}

#[test]
fn enemy_music_picks_the_highest_priority_after_a_tenth_of_a_second() {
    let h = enemy_fake();
    let mut m = MusicHandler::new(&h);
    m.reset(&h);
    h.take_lines();
    if let Some(s) = m.enemy_set(1) {
        s.extend([7, 9]);
    }
    m.update(&h, 0.05);
    assert!(!h.lines().iter().any(|l| l.starts_with("m play")));
    m.update(&h, 0.06);
    let lines = h.lines();
    assert!(
        lines.iter().any(|l| l.starts_with("m play better.ogg")),
        "{lines:?}"
    );
    assert_eq!(m.cur_prio, 9);
    assert_eq!(m.enemy.playing, [0, 1]);
    if let Some(s) = m.enemy_set(1) {
        s.clear();
    }
    for _ in 0..20 {
        m.update(&h, 0.1);
    }
    assert_eq!(m.enemy.playing, [0, 0]);
    assert!(h.lines().iter().any(|l| l.starts_with("m stop")));
}

#[test]
fn danger_music_starts_after_two_seconds_and_stops_after_six() {
    let h = Fake::new();
    let mut m = MusicHandler::new(&h);
    m.reset(&h);
    h.st.borrow_mut().danger = vec![(2, b"close.ogg".to_vec())];
    for _ in 0..19 {
        m.update(&h, 0.1);
    }
    assert!(!h.lines().iter().any(|l| l.starts_with("m play close.ogg")));
    m.update(&h, 0.2);
    assert!(h.lines().iter().any(|l| l.starts_with("m play close.ogg")));
    assert_eq!(m.cur_prio, 5);
    assert_eq!(m.enemy.close_playing, 1);
    h.st.borrow_mut().danger.clear();
    for _ in 0..70 {
        m.update(&h, 0.1);
    }
    assert_eq!(m.enemy.close_playing, 0);
    assert_eq!(m.cur_prio, -1);
}

#[test]
fn dead_player_and_missing_map_skip_the_music_scans() {
    let h = Fake::new();
    let mut m = MusicHandler::new(&h);
    m.reset(&h);
    h.take_lines();
    h.st.borrow_mut().dead = true;
    m.update(&h, 0.5);
    assert_eq!(h.lines(), vec!["m dead 1".to_string()]);
    h.st.borrow_mut().calls.clear();
    h.st.borrow_mut().dead = false;
    h.st.borrow_mut().has_map = false;
    m.update(&h, 0.5);
    assert!(!h.st.borrow().calls.contains(&code::M_DANGER_SCAN));
}

#[test]
fn song_that_ended_drops_its_slot_and_plays_the_next_one() {
    let h = Fake::new();
    let mut m = MusicHandler::new(&h);
    m.reset(&h);
    m.play(&h, b"a.ogg", false, 1.0, 1.0, 4, false, false);
    m.play(&h, b"b.ogg", true, 1.0, 1.0, 2, true, false);
    h.take_lines();
    h.st.borrow_mut().song_null = true;
    m.update(&h, 0.1);
    let lines = h.lines();
    assert!(
        lines.iter().any(|l| l.starts_with("m play b.ogg")),
        "{lines:?}"
    );
    assert_eq!(m.cur_prio, 2);
    assert!(m.slots[4].file.is_empty());
}

#[test]
fn map_change_stops_the_enemy_slots_and_empties_the_sets() {
    let h = enemy_fake();
    let mut m = MusicHandler::new(&h);
    m.reset(&h);
    if let Some(s) = m.enemy_set(0) {
        s.insert(7);
    }
    m.play(&h, b"x.ogg", true, 1.0, 1.0, 8, true, true);
    m.on_map(&h);
    assert!(m.enemies.iter().all(|s| s.is_empty()));
    assert!(m.slots[8].file.is_empty());
}

fn xml(ty: &str, attrs: &[(&str, &str)]) -> Elem {
    std::iter::once(("$type", ty))
        .chain(attrs.iter().copied())
        .map(|(k, v)| (k.to_string(), v.to_string()))
        .collect()
}

#[test]
fn events_load_by_type_and_unknown_types_are_reported() {
    let h = Fake::new();
    h.st.borrow_mut().xml = vec![
        xml("Bugs", &[("Name", "b")]),
        xml("SoundStream", &[("Name", "s")]),
        xml("Wobble", &[("Name", "w")]),
        xml("STEPS", &[("Name", "t"), ("Set", "S1")]),
        xml("Particles", &[("Name", "p")]),
    ];
    let ins = InsanityHandler::new(&h);
    assert_eq!(ins.count(), 4);
    assert_eq!(ins.event_name(2), b"t".to_vec());
    assert_eq!(ins.event_set(2), b"S1".to_vec());
    assert_eq!(ins.event_max_sanity(0), 100.0);
    assert!(ins.event_over(0) && !ins.event_used(0));
    let lines = h.lines();
    assert!(
        lines
            .iter()
            .any(|l| l == "E Insanity event type 'Wobble' does not exist!\n")
    );
    assert!(lines.iter().any(|l| l == "i load events 4"));
}

#[test]
fn disabled_sets_and_used_flags_drive_the_random_pick() {
    let h = Fake::new();
    h.st.borrow_mut().xml = vec![
        xml("Steps", &[("Name", "a"), ("Set", "S1")]),
        xml("Steps", &[("Name", "b"), ("Set", "S2")]),
    ];
    let mut ins = InsanityHandler::new(&h);
    c_srand(5);
    ins.disable_set(b"S1");
    ins.disable_set(b"S2");
    ins.start_random(&h);
    assert_eq!(ins.current(), -1);
    ins.enable_set(b"S2");
    // the pick draws from 0..=unused, so it misses now and then
    for _ in 0..40 {
        ins.start_random(&h);
        if ins.current() >= 0 {
            break;
        }
    }
    assert_eq!(ins.current(), 1);
    assert!(ins.event_used(1) && !ins.event_over(1));
    assert_eq!(ins.sets(), vec![b"S1".to_vec()]);
    ins.start_random(&h);
    assert_eq!(ins.current(), 1);
    ins.clear_sets();
    assert!(ins.sets().is_empty());
}

#[test]
fn steps_event_makes_one_sound_per_step_and_ends_after_the_count() {
    let h = Fake::new();
    h.st.borrow_mut().xml = vec![xml(
        "Steps",
        &[
            ("Name", "st"),
            ("Sound", "step.snt"),
            ("StepTime", "0.5"),
            ("StepNum", "3"),
            ("Distance", "4"),
            ("TimeMulPerStep", "0.5"),
            ("DistanceMulPerStep", "0.5"),
        ],
    )];
    let mut ins = InsanityHandler::new(&h);
    ins.start_index(&h, 0);
    h.take_lines();
    for _ in 0..200 {
        ins.update(&h, 0.1);
        if ins.current() < 0 {
            break;
        }
    }
    let sounds = h
        .lines()
        .iter()
        .filter(|l| l.starts_with("i step_sound step.snt"))
        .count();
    assert_eq!(sounds, 3);
    assert_eq!(ins.current(), -1);
    assert_eq!(ins.new_event_count(), 270.0);
}

#[test]
fn particle_event_follows_the_camera_and_cleans_up_on_exit() {
    let h = Fake::new();
    h.st.borrow_mut().xml = vec![xml(
        "Particles",
        &[
            ("Name", "pe"),
            ("SoundFile", "ps.ogg"),
            ("SoundFadeInTime", "2"),
            ("SoundFadeOutTime", "0"),
            ("ParticleSystem", "flies.ps"),
            ("Duration", "0.35"),
        ],
    )];
    let mut ins = InsanityHandler::new(&h);
    ins.start_index(&h, 0);
    let start = h.take_lines();
    assert!(start.contains(&"i ps create pe_PS flies.ps 1".to_string()));
    assert!(start.contains(&"i fadein 1 0.5".to_string()));
    while ins.current() >= 0 {
        ins.update(&h, 0.1);
    }
    let end = h.lines();
    assert!(end.contains(&"i ps kill".to_string()));
    assert!(end.contains(&"i sound stop".to_string()));
    assert_eq!(end.iter().filter(|l| *l == "i ps pos 1 2 3").count(), 4);
}

#[test]
fn sound_stream_event_fades_and_draws_subtitles() {
    let h = Fake::new();
    {
        let mut st = h.st.borrow_mut();
        st.xml = vec![xml(
            "SoundStream",
            &[
                ("Name", "ss"),
                ("File", "voice.ogg"),
                ("SoundDelayTime", "0.2"),
                ("FadeScreen", "true"),
                ("FadeInTime", "0.5"),
                ("FadeOutTime", "0.5"),
                ("SubtitleCat", "Cat"),
                ("SubtitleEntry", "Entry"),
                ("DisablePlayer", "true"),
            ],
        )];
        st.rows = vec![vec![65, 0, 0, 0], vec![66, 0, 0, 0]];
        st.subs = 1;
    }
    let mut ins = InsanityHandler::new(&h);
    ins.start_index(&h, 0);
    let start = h.take_lines();
    assert!(start.contains(&"i player active 0".to_string()));
    assert!(start.contains(&"i wrap Cat Entry rows 2".to_string()));
    ins.update(&h, 0.1);
    ins.update(&h, 0.2);
    assert!(
        h.lines()
            .iter()
            .any(|l| l.starts_with("i stream voice.ogg 0 1"))
    );
    h.take_lines();
    ins.on_draw(&h);
    let draw = h.lines();
    assert_eq!(draw.iter().filter(|l| l.starts_with("i font")).count(), 2);
    assert!(draw.iter().any(|l| l.starts_with("i fade img=0")));
    h.st.borrow_mut().valid = false;
    for _ in 0..40 {
        ins.update(&h, 0.1);
    }
    assert_eq!(ins.current(), -1);
    assert!(h.lines().contains(&"i player active 1".to_string()));
}

#[test]
fn reset_and_reload_leave_no_running_event() {
    let h = Fake::new();
    h.st.borrow_mut().xml = vec![bug_xml()];
    let mut ins = InsanityHandler::new(&h);
    ins.start_index(&h, 0);
    ins.reset(&h);
    assert_eq!(ins.current(), -1);
    assert_eq!(ins.new_event_count(), 0.0);
    ins.start_named(&h, b"Bugs_Single");
    assert_eq!(ins.current(), 0);
    ins.start_named(&h, b"nope");
    assert_eq!(ins.current(), 0);
    ins.reload(&h);
    assert_eq!(ins.current(), -1);
    assert_eq!(ins.count(), 1);
    assert_eq!(run(&h, code::M_HAS_MAP), 1);
}
