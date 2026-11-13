//! Replays `tests/golden/A3/golden.bin`: every `ohpl_a3_*` entry the C++ harness drove (state before as a diff, arguments, host
//! traffic in order, result, state after). Recorded from the harness's Rust side (`tests/golden/A3/README.txt`), where the same
//! inputs gave byte-identical output on the C++ path with the T0 kernels.

use super::a3_ffi as ffi;
use super::a3_host::{Host, Io, Ptr, op};
use super::a3_state::Base;
use crate::types::{Vector2f, Vector3f};
use std::collections::HashMap;
use std::ffi::{CStr, c_void};

const GOLDEN: &[u8] = include_bytes!(concat!(
    env!("CARGO_MANIFEST_DIR"),
    "/tests/golden/A3/golden.bin"
));

const EXT_WORDS: [usize; 2] = [18, 19];

struct Replay<'a> {
    w: &'a [u32],
    pos: usize,
    bad: usize,
    first: String,
    host_calls: usize,
    cur: (u32, u32),
}

impl Replay<'_> {
    fn next(&mut self) -> u32 {
        let v = self.w.get(self.pos).copied().unwrap_or(u32::MAX);
        self.pos += 1;
        v
    }

    fn fail(&mut self, what: String) {
        if self.bad == 0 {
            self.first = format!(
                "{what} at word {} (entry {} obj {})",
                self.pos, self.cur.0, self.cur.1
            );
        }
        self.bad += 1;
    }
}

/// Bit-exact except NaN sign: which NaN operand LLVM propagates depends on opt-level and codegen units.
fn same(a: u32, b: u32) -> bool {
    let nan = |x: u32| x & 0x7F80_0000 == 0x7F80_0000 && x & 0x007F_FFFF != 0;
    a == b || (nan(a) && nan(b))
}

fn fnv(p: Ptr) -> u32 {
    // SAFETY: the A3 drivers only pass `&'static CStr` pointers for the config op.
    let s = unsafe { CStr::from_ptr(p.cast()) };
    s.to_bytes().iter().fold(2_166_136_261u32, |h, b| {
        (h ^ u32::from(*b)).wrapping_mul(16_777_619)
    })
}

fn io_words(io: &Io, op: u32) -> [u32; 30] {
    let mut w = [0u32; 30];
    for (k, p) in io.p.iter().enumerate() {
        let cfg = op == op::CFG_FLOAT && k < 2;
        if let Some(d) = w.get_mut(k) {
            *d = if cfg { fnv(*p) } else { *p as usize as u32 };
        }
    }
    for (k, i) in io.i.iter().enumerate() {
        if let Some(d) = w.get_mut(3 + k) {
            *d = *i as u32;
        }
    }
    for (k, f) in io.f.iter().enumerate() {
        if let Some(d) = w.get_mut(6 + k) {
            *d = f.to_bits();
        }
    }
    w
}

extern "C" fn rcall(ctx: Ptr, op: u32, io: *mut Io) -> i32 {
    // SAFETY: ctx is the `Replay` the test passes and io is the driver's live block.
    let (r, io) = unsafe { (&mut *ctx.cast::<Replay>(), &mut *io) };
    r.host_calls += 1;
    if r.next() != 0xC1 {
        r.fail("expected a host call".into());
        return 0;
    }
    let rec_op = r.next();
    if rec_op != op {
        r.fail(format!("host op {op} but recorded {rec_op}"));
    }
    let mask = r.next();
    let got = io_words(io, op);
    for (k, g) in got.iter().enumerate() {
        let want = if mask & (1 << k) != 0 { r.next() } else { 0 };
        if !same(want, *g) {
            r.fail(format!("op {op} in word {k}: {g:#x} vs {want:#x}"));
        }
    }
    let ret = r.next() as i32;
    let omask = r.next();
    for k in 0..30 {
        if omask & (1 << k) == 0 {
            continue;
        }
        let v = r.next();
        match k {
            0..=2 => {
                if let Some(p) = io.p.get_mut(k) {
                    *p = v as usize as Ptr;
                }
            }
            3..=5 => {
                if let Some(i) = io.i.get_mut(k - 3) {
                    *i = v as i32;
                }
            }
            _ => {
                if let Some(f) = io.f.get_mut(k - 6) {
                    *f = f32::from_bits(v);
                }
            }
        }
    }
    ret
}

struct Obj {
    buf: Vec<u64>,
    exp: Vec<u32>,
}

impl Obj {
    fn base(&mut self) -> *mut Base {
        self.buf.as_mut_ptr().cast()
    }

    fn words(&self) -> Vec<u32> {
        let mut out = Vec::with_capacity(self.buf.len() * 2);
        for (i, v) in self.buf.iter().enumerate() {
            let (mut lo, mut hi) = (*v as u32, (*v >> 32) as u32);
            if i == 9 {
                lo = u32::from(*v != 0);
                hi = 0;
            }
            out.push(lo);
            out.push(hi);
        }
        out
    }

    fn set_word(&mut self, idx: usize, val: u32) {
        let Some(slot) = self.buf.get_mut(idx / 2) else {
            return;
        };
        let shift = (idx % 2) * 32;
        *slot = (*slot & !(0xFFFF_FFFFu64 << shift)) | (u64::from(val) << shift);
    }
}

fn apply_diff(r: &mut Replay, exp: &mut [u32]) -> Vec<(usize, u32)> {
    let n = r.next() as usize;
    let mut d = Vec::with_capacity(n);
    for _ in 0..n {
        let (i, v) = (r.next() as usize, r.next());
        if let Some(e) = exp.get_mut(i) {
            *e = v;
        }
        d.push((i, v));
    }
    d
}

fn bf(w: u32) -> f32 {
    f32::from_bits(w)
}

#[test]
fn golden_replay() {
    let words: Vec<u32> = GOLDEN
        .chunks_exact(4)
        .map(|c| u32::from_le_bytes([c[0], c[1], c[2], c[3]]))
        .collect();
    let mut r = Replay {
        w: &words,
        pos: 0,
        bad: 0,
        first: String::new(),
        host_calls: 0,
        cur: (0, 0),
    };
    let mut objs: HashMap<u32, Obj> = HashMap::new();
    let mut calls = 0usize;
    while r.pos < words.len() && r.bad == 0 {
        assert_eq!(r.next(), 0xC0, "call marker at {}", r.pos);
        let (fnid, oid, kind, size) = (r.next(), r.next(), r.next(), r.next() as usize);
        r.cur = (fnid, oid);
        let nargs = r.next() as usize;
        let args: Vec<u32> = (0..nargs).map(|_| r.next()).collect();
        if fnid == 1 {
            objs.insert(
                oid,
                Obj {
                    buf: vec![0; size / 2],
                    exp: vec![0; size],
                },
            );
        }
        let mut o = objs.remove(&oid).expect("object");
        let diff = apply_diff(&mut r, &mut o.exp);
        for (i, v) in diff {
            if !EXT_WORDS.contains(&i) {
                o.set_word(i, v);
            }
        }
        let host = Host {
            ctx: (&raw mut r).cast::<c_void>(),
            call: rcall,
        };
        let b = o.base();
        let a = |i: usize| args.get(i).copied().unwrap_or(0);
        let p = |i: usize| a(i) as usize as Ptr;
        let mut outs = [0u32; 2];
        let mut mass = bf(a(1));
        let mut collide = a(2) != 0;
        // SAFETY: `b` points at a zero-initialised POD sized for the kind recorded at `init`, `host` outlives the call.
        let ret: u32 = unsafe {
            match fnid {
                1 => {
                    ffi::ohpl_a3_init(b, &host, kind as i32);
                    0
                }
                2 => {
                    ffi::ohpl_a3_free(b);
                    0
                }
                3 => {
                    ffi::ohpl_a3_enter(b, &host, a(0) as i32);
                    0
                }
                4 => {
                    ffi::ohpl_a3_leave(b, &host, a(0) as i32);
                    0
                }
                5 => {
                    ffi::ohpl_a3_map_leave(b);
                    0
                }
                6 => {
                    ffi::ohpl_a3_update(b, &host, bf(a(0)));
                    0
                }
                7 => {
                    ffi::ohpl_a3_post_update(b, &host, bf(a(0)));
                    0
                }
                8 => u32::from(ffi::ohpl_a3_do_action(b, &host, a(0) as i32, a(1) != 0)),
                9 => {
                    ffi::ohpl_a3_scroll(b, &host, bf(a(0)));
                    0
                }
                10 => u32::from(ffi::ohpl_a3_on_move(b, &host, a(0) as i32, bf(a(1)))),
                11 => u32::from(ffi::ohpl_a3_add_yaw(b, &host, bf(a(0)))),
                12 => u32::from(ffi::ohpl_a3_add_pitch(b, &host, bf(a(0)))),
                13 => u32::from(ffi::ohpl_a3_on_run(b, &host, a(0) != 0)),
                14 => u32::from(ffi::ohpl_a3_on_crouch(b, &host, a(0) != 0)),
                15 => u32::from(ffi::ohpl_a3_on_jump(b, &host, a(0) != 0)),
                16 => ffi::ohpl_a3_crosshair(b, &host) as u32,
                17 => {
                    ffi::ohpl_a3_draw(b, &host, p(0), bf(a(1)));
                    0
                }
                18 => {
                    let pos = Vector3f {
                        x: bf(a(1)),
                        y: bf(a(2)),
                        z: bf(a(3)),
                    };
                    let size = Vector2f {
                        x: bf(a(4)),
                        y: bf(a(5)),
                    };
                    u32::from(ffi::ohpl_a3_draw_crosshair(b, &host, p(0), &pos, &size))
                }
                19 => {
                    mass = bf(a(1));
                    collide = a(2) != 0;
                    ffi::ohpl_a3_save_body(b, p(0), &mut mass, &mut collide);
                    outs = [mass.to_bits(), u32::from(collide)];
                    0
                }
                20 => u32::from(ffi::ohpl_a3_allow_buoyancy(b, &host, p(0))),
                21 => {
                    ffi::ohpl_a3_destroy_entity(b, &host, p(0));
                    0
                }
                22 => {
                    ffi::ohpl_a3_attach_sticky(b, &host, p(0));
                    0
                }
                23 => ffi::ohpl_a3_draw_debug(b, &host, p(0), p(1), bf(a(2))).to_bits(),
                other => panic!("unknown entry {other}"),
            }
        };
        let _ = (mass, collide);
        calls += 1;
        if r.next() != 0xC2 {
            r.fail(format!("entry {fnid}: expected the end marker"));
            break;
        }
        let want = r.next();
        if want != ret {
            r.fail(format!("entry {fnid} result {ret:#x} vs {want:#x}"));
        }
        let nouts = r.next() as usize;
        for k in 0..nouts {
            let v = r.next();
            if !outs.get(k).is_some_and(|o| same(*o, v)) {
                r.fail(format!("entry {fnid} out {k}"));
            }
        }
        apply_diff(&mut r, &mut o.exp);
        let now = o.words();
        if let Some(i) = now.iter().zip(&o.exp).position(|(x, y)| !same(*x, *y)) {
            let (x, y) = (now.get(i).copied(), o.exp.get(i).copied());
            r.fail(format!("entry {fnid} state word {i}: {x:?} vs {y:?}"));
        }
        if fnid == 2 {
            continue;
        }
        objs.insert(oid, o);
    }
    assert_eq!(r.bad, 0, "{}", r.first);
    assert!(
        calls > 1000 && r.host_calls > 4000,
        "{calls} {}",
        r.host_calls
    );
}

macro_rules! ops {
    ($($n:ident),* $(,)?) => {
        &[$((stringify!($n), op::$n)),*]
    };
}

#[test]
fn ops_match_header() {
    let header = include_str!(concat!(
        env!("CARGO_MANIFEST_DIR"),
        "/../amnesia/src/game/RustLuxPlayerA3.h"
    ));
    let rust: &[(&str, u32)] = ops![
        PREV_STATE,
        CHANGE_STATE,
        CHANGE_MOVE_STATE,
        SET_INTERACT_MUL,
        DEFAULT_MASS,
        HAND_DRAWN,
        SET_FOCUS,
        SET_FOCUS_TEXT,
        ICON_STYLE_DEFAULT,
        MOVE_ALLOWS_INTERACTION,
        HANDS_DO_ACTION,
        CUR_MOVE_STATE,
        MOVE_NORMAL_GET,
        MOVE_NORMAL_CMD,
        LANTERN_ACTIVE,
        LANTERN_SET,
        CAM_V3,
        CAM_ANGLES,
        CAM_PITCH_LIMITS,
        CAM_ADD_PITCH,
        CAM_ADD_YAW,
        CAM_SET_PITCH_LIMITS,
        CAM_SET_YAW_LIMITS,
        CB_V3,
        CB_SET_POS,
        CB_SET_YAW,
        CB_SET_MASS,
        CB_FLAG,
        CB_CMD,
        CB_FITS,
        CB_RAY,
        CB_MOVE,
        CB_MAX_SPEED,
        CB_SHAPE_COLLIDE,
        CB_EXISTS,
        BODY_MASS,
        BODY_V3,
        BODY_MATRIX,
        BODY_VEL_AT,
        BODY_FLAG,
        BODY_SET_V3,
        BODY_SET_MASS,
        BODY_SET_FLAG,
        BODY_ADD,
        BODY_JOINT_NUM,
        BODY_JOINT,
        BODY_USER_DATA,
        JOINT_V3,
        JOINT_BODY,
        ENT_IS_PROP,
        ENT_MAX_FOCUS,
        ENT_CAN_INTERACT,
        ENT_DISABLED,
        ENT_CROSSHAIR,
        ENT_ON_INTERACT,
        ENT_INTERACT_CB,
        ENT_FOCUS_DRAW,
        PROP_IS_ITEM,
        PROP_SET_INTERACTED,
        PROP_SET_DISABLE_COLL,
        PROP_BODY_NUM,
        PROP_BODY,
        PROP_CONNECTED,
        PROP_EFFECT_MESH,
        PROP_DATA,
        DATA_F,
        MESH_SUB_NUM,
        MESH_SUB_PARENT,
        MESH_SUB_OUTLINE,
        FX_CLEAR_OUTLINE,
        CLOSEST_ENTITY,
        MAP_EXISTS,
        USE_CB_GET,
        USE_CB_RUN,
        USE_CB_REMOVE,
        MESSAGE,
        GUI_DRAW_GFX,
        IN_SMOOTH_MOUSE,
        IN_RESET_SMOOTH,
        IN_SMOOTH_POS,
        IN_INVERT_PITCH,
        IN_ROTATE_TRIGGERED,
        LADDER_V3,
        LADDER_SOUND,
        CFG_FLOAT,
        DEBUG_ROW,
        VARS_ITEM_TAKE,
        VARS_INTERACT,
        VARS_LADDER,
        PROP_AS_ENTITY,
    ];
    let mut n = 0;
    for line in header.lines() {
        let Some(rest) = line.trim().strip_prefix("kOhplA3Op_") else {
            continue;
        };
        let (name, val) = rest.split_once(" = ").expect("enum line");
        let val: u32 = val.trim_end_matches(',').parse().expect("value");
        assert!(rust.contains(&(name, val)), "{name} = {val}");
        n += 1;
    }
    assert_eq!(n, rust.len());
}
