use super::s4::{
    DebugCm, DebugPf, DebugSm, PathBlob, Track, debug_text, nav_path_text, path_load, path_save,
};

fn hex(s: &str) -> Vec<u8> {
    if s == "-" {
        return Vec::new();
    }
    (0..s.len() / 2)
        .map(|i| u8::from_str_radix(&s[2 * i..2 * i + 2], 16).unwrap())
        .collect()
}

fn f(s: &str) -> f32 {
    f32::from_bits(u32::from_str_radix(s, 16).unwrap())
}

fn b(s: &str) -> bool {
    s == "1"
}

/// P layout: moving g0 g1 g2 exact result in_map end_cb n {node min max anim loop}* idx act pau loop at wait freq cb
fn parse_blob(t: &[&str]) -> PathBlob {
    let mut it = t.iter();
    let mut n = || *it.next().unwrap();
    let mut p = PathBlob {
        moving: b(n()),
        ..PathBlob::default()
    };
    p.goal = [f(n()), f(n()), f(n())];
    p.exact = b(n());
    p.result = hex(n());
    p.in_map = b(n());
    p.end_cb = hex(n());
    let nt: usize = n().parse().unwrap();
    for _ in 0..nt {
        p.tracks.push(Track {
            node: hex(n()),
            min_wait: f(n()),
            max_wait: f(n()),
            anim: hex(n()),
            looped: b(n()),
        });
    }
    p.track_idx = n().parse().unwrap();
    p.track_active = b(n());
    p.track_paused = b(n());
    p.track_loop = b(n());
    p.at_node = b(n());
    p.track_wait = f(n());
    p.track_freq = f(n());
    p.track_cb = hex(n());
    p
}

fn cur_from(t: &[&str]) -> PathBlob {
    PathBlob {
        track_idx: t[0].parse().unwrap(),
        track_active: b(t[1]),
        track_paused: b(t[2]),
        track_loop: b(t[3]),
        at_node: b(t[4]),
        track_wait: f(t[5]),
        track_freq: f(t[6]),
        ..PathBlob::default()
    }
}

fn bits_eq(a: &PathBlob, w: &PathBlob) -> bool {
    let fb = |x: f32| x.to_bits();
    a.moving == w.moving
        && a.goal.map(fb) == w.goal.map(fb)
        && a.exact == w.exact
        && a.result == w.result
        && a.in_map == w.in_map
        && a.end_cb == w.end_cb
        && a.tracks.len() == w.tracks.len()
        && a.tracks.iter().zip(&w.tracks).all(|(x, y)| {
            x.node == y.node
                && x.anim == y.anim
                && x.looped == y.looped
                && fb(x.min_wait) == fb(y.min_wait)
                && fb(x.max_wait) == fb(y.max_wait)
        })
        && a.track_idx == w.track_idx
        && (a.track_active, a.track_paused, a.track_loop, a.at_node)
            == (w.track_active, w.track_paused, w.track_loop, w.at_node)
        && fb(a.track_wait) == fb(w.track_wait)
        && fb(a.track_freq) == fb(w.track_freq)
        && a.track_cb == w.track_cb
}

#[test]
fn path_blob_and_text_match_cpp() {
    let data = include_str!("../../tests/golden/S4/s4.txt");
    let (mut saves, mut loads, mut navs, mut dbgs) = (0, 0, 0, 0);
    for line in data.lines() {
        let (head, rest) = line.split_once(' ').unwrap_or((line, ""));
        let (args, res) = rest.rsplit_once(" | ").unwrap_or((rest, ""));
        let t: Vec<&str> = args.split_whitespace().collect();
        let r: Vec<&str> = res.split_whitespace().collect();
        match head {
            "P" => {
                let blob = parse_blob(&t);
                assert_eq!(path_save(&blob), hex(r[0]), "{line}");
                saves += 1;
            }
            "L" => {
                let cur = cur_from(&t[1..8]);
                let want = parse_blob(&r);
                let mut got = path_load(&hex(t[0]), &cur);
                if !got.moving {
                    // Stop(): the parsed goal, flags and result callback are discarded
                    (got.goal, got.exact, got.in_map, got.result) =
                        ([0.0; 3], false, false, Vec::new());
                }
                assert!(bits_eq(&got, &want), "{line}\n{got:?}\n{want:?}");
                loads += 1;
            }
            "N" => {
                let n: usize = t[0].parse().unwrap();
                let nodes: Vec<[f32; 3]> = (0..n)
                    .map(|i| [f(t[1 + 3 * i]), f(t[2 + 3 * i]), f(t[3 + 3 * i])])
                    .collect();
                assert_eq!(nav_path_text(&nodes), hex(r[0]), "{line}");
                navs += 1;
            }
            "D" => {
                let i = |k: usize| -> i32 { t[k].parse().unwrap() };
                let fl: Vec<f32> = t[23..34].iter().map(|s| f(s)).collect();
                let (cur, prev, next, anim) = (hex(t[3]), hex(t[4]), hex(t[5]), hex(t[6]));
                let sm = DebugSm {
                    cur: &cur,
                    prev: &prev,
                    next: &next,
                    timers: i(7),
                };
                let pf = DebugPf {
                    moving: i(8) != 0,
                    track_active: i(9) != 0,
                    paused: i(10) != 0,
                    at_node: i(11) != 0,
                    path_idx: i(16),
                    path_len: i(17),
                    track_idx: i(18),
                    track_len: i(19),
                    goal: [fl[0], fl[1], fl[2]],
                    wait: fl[6],
                };
                let cm = DebugCm {
                    moving: i(12) != 0,
                    turning: i(13) != 0,
                    wall: i(14) != 0,
                    object_avoid: i(15) != 0,
                    anim: &anim,
                    speed_state: i(20),
                    idle_extra_count: i(21),
                    wall_hits: i(22),
                    goal: [fl[3], fl[4], fl[5]],
                    fwd: fl[7],
                    stuck: fl[8],
                    idle_extra_wait: fl[9],
                    roll: fl[10],
                };
                let got = debug_text(
                    (i(0) != 0).then_some(&sm),
                    (i(1) != 0).then_some(&pf),
                    (i(2) != 0).then_some(&cm),
                );
                assert_eq!(got, hex(r[0]), "{line}");
                dbgs += 1;
            }
            _ => {}
        }
    }
    assert!(saves >= 72 && loads >= 432 && navs >= 12 && dbgs >= 48);
}
