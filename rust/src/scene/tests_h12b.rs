//! Replay of `rust/tests/golden/H12/dec.txt`, recorded from the original C++ expressions by
//! `HPL2/tests/SceneH12bTests.cpp`: gate and plan inputs in, decisions out. Plus the particle
//! system lifecycle against a recording host.

use super::h12b_ent as ent;
use super::h12b_psys::{self as psys, PeCtl, PsHost};
use crate::types::{Color, Vector3f};
use std::ffi::c_void;

const GOLDEN: &str = include_str!("../../tests/golden/H12/dec.txt");

fn f(tok: &str) -> f32 {
    f32::from_bits(u32::from_str_radix(tok, 16).unwrap())
}

fn i(tok: &str) -> i32 {
    tok.parse().unwrap()
}

fn b(tok: &str) -> bool {
    i(tok) != 0
}

fn n(v: impl Into<i64>) -> String {
    v.into().to_string()
}

fn ctl(sleep: i32, dir: i32, axis: i32, gfx: bool, bv: bool) -> PeCtl {
    PeCtl {
        sleep_count: sleep,
        direction_update_count: dir,
        axis_update_count: axis,
        update_gfx: gfx,
        update_bv: bv,
    }
}

fn run(name: &str, a: &[&str]) -> String {
    match name {
        "pe_update" => {
            let mut c = ctl(i(a[0]), i(a[5]), -1, false, false);
            let r = psys::update_gate(&mut c, b(a[1]), b(a[2]), b(a[3]), b(a[4]), i(a[6]));
            format!(
                "{} {} {} {} {}",
                r,
                c.sleep_count,
                c.direction_update_count,
                n(c.update_gfx),
                n(c.update_bv)
            )
        }
        "pe_bv" => {
            let mut c = ctl(300, -1, i(a[2]), true, b(a[0]));
            let r = psys::bv_gate(&mut c, b(a[1]), i(a[3]));
            format!("{} {} {}", r, c.axis_update_count, n(c.update_bv))
        }
        "pe_axis" => {
            let mut c = ctl(300, -1, i(a[2]), true, true);
            let r = psys::axis_gate(&mut c, b(a[1]), i(a[3]));
            format!("{} {}", n(r), c.axis_update_count)
        }
        "pe_model" => psys::model_kind(b(a[0]), b(a[1])).to_string(),
        "psdata" => {
            let (root, ex, mask, nc) = (b(a[0]), i(a[1]) as usize, i(a[2]), i(a[3]) as usize);
            let is: Vec<bool> = (0..nc).map(|k| (mask >> k) & 1 != 0).collect();
            let mut load = vec![false; nc];
            let r = psys::data_load_plan(root, &is, ex, &mut load);
            let l: String = load.iter().map(|x| if *x { '1' } else { '0' }).collect();
            format!("{l} {}", n(r)).trim_start().to_string()
        }
        "psdata_create" => n(i(a[0]) > 0),
        "visible" => {
            let c = Color {
                r: f(a[0]),
                g: f(a[1]),
                b: f(a[2]),
                a: f(a[3]),
            };
            n(ent::is_visible(c, b(a[4])))
        }
        "bb_axis" => {
            let axis = Vector3f {
                x: f(a[1]),
                y: f(a[2]),
                z: f(a[3]),
            };
            match ent::bb_axis_size(b(a[0]), axis, f(a[4]), f(a[5])) {
                Some(v) => format!("1 {:08x}", v.to_bits()),
                None => "0".into(),
            }
        }
        "bb_halo_alpha" => {
            let v = |x, y| Vector3f {
                x: f(x),
                y: f(y),
                z: 0.0,
            };
            let clip = b(a[2]).then(|| (v(a[3], a[4]), v(a[5], a[6])));
            format!(
                "{:08x}",
                ent::bb_halo_alpha(i(a[0]), i(a[1]), clip).to_bits()
            )
        }
        "bb_halo_gates" => {
            let (mut su, mut bc) = (b(a[0]), i(a[1]));
            let r = ent::bb_halo_gates(&mut su, &mut bc, i(a[2]));
            format!("{} {} {}", r, n(su), bc)
        }
        "beam_geom" => n(ent::beam_needs_geometry(i(a[0]), i(a[1]), i(a[2]))),
        "rope_points" => ent::rope_points(i(a[0]), i(a[1])).to_string(),
        "rope_elems" => {
            let c = ent::rope_points(i(a[0]), i(a[1]));
            ent::rope_elements(c).to_string()
        }
        "iris" => ent::iris_plan(b(a[0]), i(a[1]), i(a[2])).to_string(),
        "flare" => {
            let m = i(a[0]);
            let v = [m & 1 != 0, m & 2 != 0, m & 4 != 0];
            format!(
                "{} {}",
                ent::first_active_flare(&v),
                n(ent::any_flare_active(f(a[1]), &v))
            )
        }
        "snd_route" => {
            let r = ent::snd_play_route(b(a[0]), b(a[1]));
            format!("{} {} {}", r & 1, (r >> 1) & 1, (r >> 2) & 1)
        }
        other => panic!("unknown probe {other}"),
    }
}

#[test]
fn decisions_match_cpp() {
    let mut count = 0;
    for line in GOLDEN.lines() {
        let rest = line.strip_prefix("dec ").unwrap();
        let (key, want) = rest.split_once(" ->").unwrap();
        let toks: Vec<&str> = key.split(' ').collect();
        let got = run(toks[0], &toks[1..]);
        assert_eq!(got, want.trim(), "{key}");
        count += 1;
    }
    assert!(count > 900);
}

struct Rec {
    log: Vec<String>,
    count: i32,
    warm: Vec<(f32, f32)>,
}

unsafe extern "C" fn count(c: *mut c_void) -> i32 {
    // SAFETY: test host: `c` is the `Rec` under test.
    unsafe { &*c.cast::<Rec>() }.count
}

unsafe extern "C" fn warm_time(c: *mut c_void, k: i32) -> f32 {
    // SAFETY: as above.
    unsafe { &*c.cast::<Rec>() }.warm[k as usize].0
}

unsafe extern "C" fn warm_steps(c: *mut c_void, k: i32) -> f32 {
    // SAFETY: as above.
    unsafe { &*c.cast::<Rec>() }.warm[k as usize].1
}

unsafe extern "C" fn update(c: *mut c_void, k: i32, dt: f32) {
    // SAFETY: as above.
    unsafe { &mut *c.cast::<Rec>() }
        .log
        .push(format!("u{k}:{:08x}", dt.to_bits()));
}

unsafe extern "C" fn apply(c: *mut c_void, k: i32, op: i32, arg: bool) {
    // SAFETY: as above.
    unsafe { &mut *c.cast::<Rec>() }
        .log
        .push(format!("a{k}:{op}:{}", u8::from(arg)));
}

unsafe extern "C" fn query(c: *mut c_void, k: i32, q: i32) -> bool {
    // SAFETY: as above.
    let r = unsafe { &mut *c.cast::<Rec>() };
    r.log.push(format!("q{k}:{q}"));
    k != 1
}

fn host(r: &mut Rec) -> PsHost {
    PsHost {
        ctx: std::ptr::from_mut(r).cast(),
        emitter_count: count,
        warm_up_time: warm_time,
        warm_up_steps: warm_steps,
        update,
        apply,
        query,
    }
}

#[test]
fn system_lifecycle() {
    let mut r = Rec {
        log: vec![],
        count: 2,
        warm: vec![(0.0, 20.0), (0.5, 4.0)],
    };
    let h = host(&mut r);
    let mut first = true;
    psys::update_logic(&h, &mut first, true, 0.25);
    assert!(!first);
    let step = format!("{:08x}", 0.25f32.to_bits());
    // warm-up of emitter 1: 0.5 s in 0.25 s steps (two), then the real step.
    assert_eq!(
        r.log.join(" "),
        format!("u0:{step} u1:{step} u1:{step} u1:{step}")
    );
    r.log.clear();
    let h = host(&mut r);
    psys::update_logic(&h, &mut first, true, 0.25);
    assert_eq!(r.log.len(), 2);
    r.log.clear();
    let h = host(&mut r);
    psys::update_logic(&h, &mut first, false, 0.25);
    assert!(r.log.is_empty());

    let h = host(&mut r);
    assert!(!h.all(0));
    assert_eq!(r.log, ["q0:0", "q1:0"]);
    r.log.clear();
    let mut vis = true;
    let h = host(&mut r);
    psys::set_visible(&h, &mut vis, true);
    assert!(r.log.is_empty());
    psys::set_visible(&h, &mut vis, false);
    assert!(!vis);
    assert_eq!(r.log, ["a0:0:0", "a1:0:0"]);
}

#[test]
fn empty_system_is_dead() {
    let mut r = Rec {
        log: vec![],
        count: 0,
        warm: vec![],
    };
    let h = host(&mut r);
    assert!(h.all(0));
}
