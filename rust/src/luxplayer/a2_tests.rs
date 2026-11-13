//! Replays `tests/golden/A2/core.bin`: per driver call the Core before and after, and the host traffic in order.
//! Recorded from the C++ test harness (`tests/golden/A2/README.txt`), where the same inputs gave identical C++ and Rust output.

use super::a2_core::{self, Core, Host};
use std::ffi::c_void;

const GOLDEN: &[u8] = include_bytes!(concat!(
    env!("CARGO_MANIFEST_DIR"),
    "/tests/golden/A2/core.bin"
));
const CORE: usize = std::mem::size_of::<Core>();

struct Replay {
    ev: Vec<u32>,
    pos: usize,
    bad: usize,
}

extern "C" fn rop(ctx: *mut c_void, k: u32, i: i32, a: f32, b: f32, c: f32, d: f32) {
    // SAFETY: ctx is the `Replay` the test passes.
    let r = unsafe { &mut *ctx.cast::<Replay>() };
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

extern "C" fn rq(ctx: *mut c_void, k: u32) -> f32 {
    // SAFETY: ctx is the `Replay` the test passes.
    let r = unsafe { &mut *ctx.cast::<Replay>() };
    let v = match r.ev.get(r.pos..r.pos + 3) {
        Some([0, kind, v]) if *kind == k => *v,
        _ => {
            r.bad += 1;
            0
        }
    };
    r.pos += 3;
    f32::from_bits(v)
}

fn word(b: &[u8], at: usize) -> u32 {
    u32::from_le_bytes([b[at], b[at + 1], b[at + 2], b[at + 3]])
}

fn core_from(b: &[u8]) -> Core {
    let mut c = Core::default();
    // SAFETY: Core is repr(C) of f32/u32 fields only, any bit pattern is valid; sizes match.
    unsafe { std::ptr::copy_nonoverlapping(b.as_ptr(), (&raw mut c).cast::<u8>(), CORE) };
    c
}

fn core_bytes(c: &Core) -> &[u8] {
    // SAFETY: plain repr(C) data.
    unsafe { std::slice::from_raw_parts((c as *const Core).cast::<u8>(), CORE) }
}

#[test]
fn a2_core_replay() {
    let mut at = 0;
    let mut calls = [0usize; 6];
    while at < GOLDEN.len() {
        let id = word(GOLDEN, at);
        let a: Vec<u32> = (1..5).map(|i| word(GOLDEN, at + 4 * i)).collect();
        let n = word(GOLDEN, at + 20) as usize;
        at += 24;
        let mut core = core_from(&GOLDEN[at..at + CORE]);
        at += CORE;
        let ev: Vec<u32> = (0..n).map(|i| word(GOLDEN, at + 4 * i)).collect();
        at += 4 * n;
        let after = &GOLDEN[at..at + CORE];
        at += CORE;
        let mut r = Replay { ev, pos: 0, bad: 0 };
        let h = Host {
            ctx: (&raw mut r).cast(),
            op: rop,
            query: rq,
        };
        let dt = f32::from_bits(a[0]);
        // SAFETY: `core` and `h` outlive the call; the host only touches `r`.
        unsafe {
            match id {
                0 => a2_core::vitals_run(
                    &raw mut core,
                    &h,
                    a[0] as i32,
                    f32::from_bits(a[1]),
                    a[2] as i32,
                    a[3],
                ),
                1 => a2_core::spin_head(&raw mut core, &h, dt),
                2 => a2_core::update_camera(&raw mut core, &h, dt),
                3 => a2_core::update_head_spin(&raw mut core, &h, dt),
                4 => a2_core::update_terror(&raw mut core, &h, dt),
                5 => a2_core::update_focus_alpha(&raw mut core, &h, dt),
                _ => panic!("bad id"),
            }
        }
        assert_eq!(r.bad, 0, "host traffic differs, call id {id}");
        assert_eq!(r.pos, n, "host call count differs, call id {id}");
        assert_eq!(core_bytes(&core), after, "core differs, call id {id}");
        if let Some(c) = calls.get_mut(id as usize) {
            *c += 1;
        }
    }
    assert!(calls.iter().all(|c| *c > 50), "{calls:?}");
}
