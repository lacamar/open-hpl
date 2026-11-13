//! Replays the C++ recording under `tests/golden/H9` (no C++): fog and light program-combo setup.

use super::h9::{
    FOG_FEATURES, FOG_VARS, Feature, LIGHT_FEATURES, LIGHT_VARS, Value, fog_defaults,
    light_defaults,
};
use std::fmt::Write as _;
use std::path::PathBuf;

fn golden() -> String {
    let mut p = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    p.push("tests/golden/H9/combos.txt");
    std::fs::read_to_string(&p).unwrap_or_else(|e| panic!("{}: {e}", p.display()))
}

fn name(b: &[u8]) -> &str {
    std::str::from_utf8(&b[..b.len() - 1]).expect("ascii")
}

fn dump(
    out: &mut String,
    mode: (&str, &str, &str),
    features: &[Feature],
    defaults: Vec<super::h9::Define>,
    vars: &[(&[u8], i32)],
) {
    let _ = writeln!(out, "mode {} vtx {} frag {}", mode.0, mode.1, mode.2);
    for f in features {
        let _ = writeln!(out, "f {} {} {}", name(f.name), f.stages, f.needed);
    }
    let mut d: Vec<(String, String)> = defaults
        .iter()
        .map(|d| {
            let v = match d.value {
                Value::Flag => String::new(),
                Value::Int(n) => n.to_string(),
                Value::Float(f) => format!("{:.6}", f64::from(f)),
            };
            (name(d.name).to_owned(), v)
        })
        .collect();
    d.sort();
    for (k, v) in d {
        let _ = writeln!(out, "d {k}={v}");
    }
    for (n, id) in vars {
        let _ = writeln!(out, "v {} {id}", name(n));
    }
}

#[test]
fn combos_match_cpp() {
    let mut expect = String::new();
    for packed in [0, 1] {
        let _ = writeln!(expect, "fog packed {packed}");
        dump(
            &mut expect,
            ("Fog", "deferred_fog_vtx.glsl", "deferred_fog_frag.glsl"),
            &FOG_FEATURES,
            fog_defaults(packed == 0),
            &FOG_VARS,
        );
    }
    let sizes = [0, 1, 2, 4, 8, 16, 64];
    let samples = [0, 1, 7, 8, 16, 33];
    for q in 0..3 {
        for g64 in [0, 1] {
            for rt in [3, 4] {
                for &a in &sizes {
                    for &b in &samples {
                        let _ = writeln!(expect, "light q {q} g64 {g64} rt {rt} jitter {a} {b}");
                        dump(
                            &mut expect,
                            (
                                "Lights",
                                "deferred_base_vtx.glsl",
                                "deferred_light_frag.glsl",
                            ),
                            &LIGHT_FEATURES,
                            light_defaults(q, g64 == 1, rt == 4, a, b),
                            &LIGHT_VARS,
                        );
                    }
                }
            }
        }
    }
    let got: String = golden()
        .lines()
        .filter(|l| !l.starts_with("combos"))
        .map(|l| format!("{l}\n"))
        .collect();
    for (i, (a, b)) in expect.lines().zip(got.lines()).enumerate() {
        assert_eq!(a, b, "line {}", i + 1);
    }
    assert_eq!(expect.lines().count(), got.lines().count());
}
