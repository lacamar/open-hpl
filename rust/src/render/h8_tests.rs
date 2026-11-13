//! Replays the C++ recordings under `tests/golden/H8` (no C++, no game data).

use super::h8::{GateIn, PostIn, Rect2l, clip_pad, gates, post};
use crate::math::Matrixf;
use crate::types::Vector3f;
use std::path::PathBuf;

fn golden(name: &str) -> String {
    let mut p = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    p.push("tests/golden/H8");
    p.push(name);
    std::fs::read_to_string(&p).unwrap_or_else(|e| panic!("{}: {e}", p.display()))
}

fn f(s: &str) -> f32 {
    f32::from_bits(u32::from_str_radix(s, 16).expect("hex"))
}

fn v(w: &[&str]) -> Vector3f {
    Vector3f {
        x: f(w[0]),
        y: f(w[1]),
        z: f(w[2]),
    }
}

fn n(s: &str) -> i32 {
    s.parse().expect("int")
}

#[test]
fn list_add_object_replay() {
    let mut calls = 0;
    for seed in 0..12 {
        for line in golden(&format!("list_{seed:03}.txt"))
            .lines()
            .filter(|l| l.starts_with("A "))
        {
            let (inp, out) = line[2..].split_once(" |").expect("sep");
            let w: Vec<&str> = inp.split(' ').collect();
            let o: Vec<&str> = out.split_whitespace().collect();
            let mut m = Matrixf::default();
            for (i, x) in w[20..36].iter().enumerate() {
                m.m[i / 4][i % 4] = f(x);
            }
            let (frame, mfc, ofc) = (n(w[36]), n(w[37]), n(w[38]));
            let g = GateIn {
                has_material: w[1] == "1",
                has_type: w[2] == "1",
                translucent: w[3] == "1",
                decal: w[4] == "1",
                frame,
                material_frame: mfc,
                object_frame: ofc,
            };
            assert_eq!(gates(&g), o[0].parse::<u32>().expect("gates"), "{line}");
            if o[1] == "skip" {
                continue;
            }
            let p = PostIn {
                render_type: n(w[0]),
                has_material: g.has_material,
                has_type: g.has_type,
                translucent: g.translucent,
                decal: g.decal,
                has_illum_texture: w[5] == "1",
                uses_occlusion_query: w[6] == "1",
                illumination: f(w[7]),
                bv_min: v(&w[8..11]),
                bv_max: v(&w[11..14]),
                bv_center: v(&w[14..17]),
                origin: v(&w[17..20]),
                view: m,
            };
            let r = post(&p);
            assert_eq!(
                r.view_z.to_bits(),
                u32::from_str_radix(o[1], 16).expect("z"),
                "{line}"
            );
            assert_eq!(r.lists, o[2].parse::<u32>().expect("lists"), "{line}");
            calls += 1;
        }
    }
    assert!(calls > 800, "{calls}");
}

#[test]
fn clip_pad_replay() {
    let mut rows = 0;
    for seed in 0..12 {
        for line in golden(&format!("clip_{seed:03}.txt")).lines() {
            let (inp, out) = line.split_once(" | ").expect("sep");
            let a: Vec<&str> = inp.split(' ').collect();
            let b: Vec<i32> = out.split(' ').map(n).collect();
            let r = Rect2l {
                x: n(a[0]),
                y: n(a[1]),
                w: n(a[2]),
                h: n(a[3]),
            };
            let got = clip_pad(r, (n(a[4]), n(a[5])), f(a[6]));
            assert_eq!(
                got,
                Rect2l {
                    x: b[0],
                    y: b[1],
                    w: b[2],
                    h: b[3]
                },
                "{line}"
            );
            rows += 1;
        }
    }
    assert_eq!(rows, 24000);
}
