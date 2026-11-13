//! Replays `tests/golden/A2/helpers.bin`: per helper driver call the state before and after, the return value and the host
//! traffic in order. Recorded from the C++ harness (`tests/golden/A2/README.txt`), where C++ and Rust gave identical output.

use super::a2b_helpers::{
    self as h, CamDirState, DarkState, HostB, HurtState, LanternState, LightState, LookAtState,
};
use super::helpers::{Collapse, Flashback};
use crate::types::Vector3f;
use std::ffi::c_void;

const GOLDEN: &[u8] = include_bytes!(concat!(
    env!("CARGO_MANIFEST_DIR"),
    "/tests/golden/A2/helpers.bin"
));

struct Replay {
    ev: Vec<u32>,
    pos: usize,
    bad: usize,
}

fn rp(ctx: *mut c_void) -> &'static mut Replay {
    // SAFETY: ctx is the `Replay` the test passes, alive for the call.
    unsafe { &mut *ctx.cast::<Replay>() }
}

extern "C" fn rop(ctx: *mut c_void, k: u32, i: i32, a: f32, b: f32, c: f32, d: f32) {
    let r = rp(ctx);
    let got = [
        1,
        k,
        i as u32,
        a.to_bits(),
        b.to_bits(),
        c.to_bits(),
        d.to_bits(),
    ];
    if r.ev.get(r.pos..r.pos + 7) != Some(&got[..]) {
        r.bad += 1;
    }
    r.pos += 7;
}

extern "C" fn rq(ctx: *mut c_void, k: u32, a: f32, b: f32) -> f32 {
    let r = rp(ctx);
    let v = match r.ev.get(r.pos..r.pos + 5) {
        Some([0, kind, x, y, v]) if *kind == k && *x == a.to_bits() && *y == b.to_bits() => *v,
        _ => {
            r.bad += 1;
            0
        }
    };
    r.pos += 5;
    f32::from_bits(v)
}

extern "C" fn rf(ctx: *mut c_void, k: u32, buf: *mut f32) {
    let r = rp(ctx);
    let n = match r.ev.get(r.pos..r.pos + 3) {
        Some([2, kind, n]) if *kind == k => *n as usize,
        _ => {
            r.bad += 1;
            r.pos += 3;
            return;
        }
    };
    // SAFETY: the host buffer has `n` floats for this kind.
    let b = unsafe { std::slice::from_raw_parts_mut(buf, n) };
    let inp = r.ev.get(r.pos + 3..r.pos + 3 + n);
    let out = r.ev.get(r.pos + 3 + n..r.pos + 3 + 2 * n);
    match (inp, out) {
        (Some(i), Some(o)) => {
            if b.iter().map(|f| f.to_bits()).ne(i.iter().copied()) {
                r.bad += 1;
            }
            for (d, s) in b.iter_mut().zip(o) {
                *d = f32::from_bits(*s);
            }
        }
        _ => r.bad += 1,
    }
    r.pos += 3 + 2 * n;
}

fn word(b: &[u8], at: usize) -> u32 {
    b.get(at..at + 4)
        .and_then(|w| <[u8; 4]>::try_from(w).ok())
        .map_or(0, u32::from_le_bytes)
}

fn run<S>(id: u32, a: &[u32; 5], st: &mut S, hb: &HostB) -> u32 {
    let s = std::ptr::from_mut(st);
    let x = f32::from_bits(a[0]);
    let (b0, b1, b2, b3) = (a[0] != 0, a[1] != 0, a[2] != 0, a[3] != 0);
    // SAFETY: `s` points at a state of the type the id names; `hb` outlives the call; the host only touches the replay.
    unsafe {
        match id {
            0 => h::collapse_update(s.cast(), hb, x),
            1 => h::camdir_update(s.cast(), hb, x),
            2 => h::hurt_update(s.cast(), hb, x),
            3 => h::lookat_update(s.cast(), hb, x),
            4 => h::flashback_update(s.cast(), hb, x),
            5 => h::light_update(s.cast(), hb, x),
            6 => h::dark_update(s.cast(), hb, x),
            7 => h::lantern_update(s.cast(), hb, x),
            8 => h::lantern_set_active(s.cast(), hb, b0, b1, b2, b3),
            9 => h::lantern_set_disabled(s.cast(), hb, b0),
            10 => h::dark_set_active(s.cast(), hb, b0),
            11 => h::collapse_clear(s.cast(), b0, b1),
            12 => return h::camdir_add(s.cast(), b0, f32::from_bits(a[1])).to_bits(),
            13 => h::camdir_reset(s.cast()),
            14 => h::hurt_reset(s.cast()),
            15 => h::lookat_reset(s.cast()),
            16 => h::lookat_set_target(
                s.cast(),
                Vector3f {
                    x,
                    y: f32::from_bits(a[1]),
                    z: f32::from_bits(a[2]),
                },
                f32::from_bits(a[3]),
                f32::from_bits(a[4]),
            ),
            17 => h::lookat_set_active(s.cast(), b0),
            18 => h::flashback_reset(s.cast()),
            19 => h::light_reset(s.cast()),
            20 => (*s.cast::<LightState>()).update_count = 0.0,
            21 => h::dark_reset(s.cast()),
            22 => h::dark_map_enter(s.cast()),
            23 => h::lantern_reset(s.cast()),
            24 => return u32::from(x <= (*s.cast::<DarkState>()).k.min_darkness_light_level),
            _ => {}
        }
    }
    0
}

fn size(id: u32) -> usize {
    use std::mem::size_of;
    match id {
        0 | 11 => size_of::<Collapse>(),
        1 | 12 | 13 => size_of::<CamDirState>(),
        2 | 14 => size_of::<HurtState>(),
        3 | 15..=17 => size_of::<LookAtState>(),
        4 | 18 => size_of::<Flashback>(),
        5 | 19 | 20 => size_of::<LightState>(),
        6 | 10 | 21 | 22 | 24 => size_of::<DarkState>(),
        _ => size_of::<LanternState>(),
    }
}

#[repr(C, align(16))]
struct Buf([u8; 2100]);

#[test]
fn a2b_helpers_replay() {
    let mut at = 0;
    let mut calls = [0usize; 25];
    while at < GOLDEN.len() {
        let id = word(GOLDEN, at);
        let a = [1, 2, 3, 4, 5].map(|i| word(GOLDEN, at + 4 * i));
        let (ret, n) = (word(GOLDEN, at + 24), word(GOLDEN, at + 28) as usize);
        at += 32;
        let sz = size(id);
        let mut st = Buf([0; 2100]);
        st.0[..sz].copy_from_slice(&GOLDEN[at..at + sz]);
        at += sz;
        let ev: Vec<u32> = (0..n).map(|i| word(GOLDEN, at + 4 * i)).collect();
        at += 4 * n;
        let after = &GOLDEN[at..at + sz];
        at += sz;
        let mut r = Replay { ev, pos: 0, bad: 0 };
        let hb = HostB {
            ctx: (&raw mut r).cast(),
            op: rop,
            query: rq,
            fill: rf,
        };
        let got = run(id, &a, &mut st, &hb);
        assert_eq!(r.bad, 0, "host traffic differs, call id {id}");
        assert_eq!(r.pos, n, "host call count differs, call id {id}");
        assert_eq!(&st.0[..sz], after, "state differs, call id {id}");
        if id == 12 || id == 24 {
            assert_eq!(got, ret, "return differs, call id {id}");
        }
        if let Some(c) = calls.get_mut(id as usize) {
            *c += 1;
        }
    }
    assert!(calls.iter().all(|c| *c >= 12), "{calls:?}");
}

#[test]
fn collapse_stop_keeps_random_count() {
    let mut c = Collapse {
        random_count: 3.0,
        active: 1,
        state: 2,
        ..Collapse::default()
    };
    // SAFETY: `c` is valid and unshared.
    unsafe { h::collapse_clear(&mut c, false, false) };
    assert_eq!((c.active, c.state, c.random_count), (0, 0, 3.0));
    // SAFETY: as above.
    unsafe { h::collapse_clear(&mut c, true, true) };
    assert_eq!((c.active, c.random_count), (1, 0.0));
}
