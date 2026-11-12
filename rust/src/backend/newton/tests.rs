use super::joint::hinge_geometry;
use super::transposed;
use crate::types::{Matrixf, Vector3f};

const GOLDEN: &str = include_str!("../../../tests/golden/L4/newton_scenes.txt");

fn bits(tok: &str) -> u32 {
    u32::from_str_radix(tok, 16).unwrap_or(0)
}

fn matrix(toks: &[&str]) -> Matrixf {
    let mut m = Matrixf::default();
    for (i, t) in toks.iter().enumerate() {
        if let Some(row) = m.m.get_mut(i / 4)
            && let Some(e) = row.get_mut(i % 4)
        {
            *e = f32::from_bits(bits(t));
        }
    }
    m
}

fn vec3(toks: &[&str]) -> [u32; 3] {
    let at = |i: usize| bits(toks.get(i).copied().unwrap_or("0"));
    [at(0), at(1), at(2)]
}

fn vbits(v: Vector3f) -> [u32; 3] {
    [v.x.to_bits(), v.y.to_bits(), v.z.to_bits()]
}

#[test]
fn hinge_geometry_matches_cpp_bit_for_bit() {
    let mut cases = 0;
    for line in GOLDEN.lines().filter(|l| l.starts_with("hinge ")) {
        let toks: Vec<&str> = line.split_whitespace().skip(1).collect();
        let (ins, outs) = toks.split_at(64);
        let outs = &outs[1..];
        let m = |k: usize| matrix(&ins[k * 16..k * 16 + 16]);
        let g = hinge_geometry(&m(0), &m(1), &m(2), &m(3));
        let want = [g.p0, g.p1, g.right0, g.up0, g.forward0, g.q0, g.q1];
        for (i, v) in want.iter().enumerate() {
            assert_eq!(vbits(*v), vec3(&outs[i * 3..]), "case {cases} vector {i}");
        }
        assert_eq!(g.angle.to_bits(), bits(outs[21]), "case {cases} angle");
        assert_eq!(g.dist_sqr.to_bits(), bits(outs[22]), "case {cases} dist");
        cases += 1;
    }
    assert_eq!(cases, 48);
}

#[test]
fn golden_scenes_are_complete() {
    let ends: Vec<&str> = GOLDEN.lines().filter(|l| l.starts_with("end ")).collect();
    let names: Vec<&str> = ends
        .iter()
        .filter_map(|l| l.split_whitespace().nth(1))
        .collect();
    assert_eq!(
        names,
        [
            "stack",
            "pile",
            "joints",
            "contacts",
            "queries",
            "sleep",
            "buoyancy",
            "character",
            "stress",
            "serialized",
            "joints2"
        ]
    );
    for l in ends {
        let hash = l.split_whitespace().last().unwrap_or("");
        assert_eq!(hash.len(), 16, "{l}");
        assert!(u64::from_str_radix(hash, 16).is_ok(), "{l}");
    }
}

#[test]
fn newton_layout_is_the_transpose() {
    let mut m = Matrixf::default();
    for (i, e) in m.m.iter_mut().flatten().enumerate() {
        *e = i as f32;
    }
    let t = transposed(&m);
    assert_eq!(t[1], 4.0);
    assert_eq!(t[4], 1.0);
    assert_eq!(t[11], 14.0);
    assert_eq!(transposed(&Matrixf { m: [[0.0; 4]; 4] }), [0.0; 16]);
}

#[test]
fn forces_accumulate_once_per_world_pass() {
    use super::reg::{self, BodyRec, WorldRec, reg};
    use crate::types::Vector3f;
    use std::ptr;
    let zero = Vector3f::default();
    let (w, b) = {
        let mut r = reg().expect("registry");
        let w = r
            .worlds
            .insert(WorldRec {
                nw: ptr::null_mut(),
                max_step: 0.0,
            })
            .expect("world");
        let b = r
            .bodies
            .insert(BodyRec {
                nb: ptr::null_mut(),
                nw: ptr::null_mut(),
                cpp: 0,
                world: w,
                mass: 0.0,
                gravity: true,
                max_linear: 0.0,
                max_angular: 0.0,
                force: zero,
                torque: zero,
                forced: false,
                buoyancy_force: zero,
                buoyancy_torque: zero,
            })
            .expect("body");
        (w, b)
    };
    let f = Vector3f {
        x: 1.0,
        y: 2.0,
        z: 3.0,
    };
    // SAFETY: `f` is a valid vector.
    unsafe {
        super::body::ohpl_ntb_add_force(b.to_bits(), &f);
        super::body::ohpl_ntb_add_force(b.to_bits(), &f);
    }
    let (_, rec) = reg::body(b.to_bits()).expect("rec");
    assert_eq!((rec.force.x, rec.force.y, rec.force.z), (2.0, 4.0, 6.0));
    let r = reg().expect("registry");
    assert_eq!(
        r.forced
            .iter()
            .filter(|(wh, bh)| *wh == w && *bh == b)
            .count(),
        1
    );
}
