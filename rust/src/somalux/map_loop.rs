//! Per-frame loops of `cSomaLuxMap` (`Update`, `PostUpdate`, look-at and collide callbacks). Entity
//! state is read from the `Core`/`Names` pods; script calls and engine-object updates go through
//! the S9 host ops `MAP` and `ENT` (SomaLux.cpp).

use super::roster::Roster;
use super::s5_input_ffi::Io;
use super::s5_scriptable::ohpl_s5_ts_idle;
use super::s5c_game::{Ops, with_ops};
use super::s9_map::{ohpl_s9_map_stamp, timers_update};
use crate::somaent::ent_core::Core;
use crate::somaent::names::{Names, ohpl_se_names_set};
use std::ffi::c_void;

pub mod op {
    pub const MAP: u32 = 116;
    pub const ENT: u32 = 117;
}

pub mod map_op {
    pub const DUMP_ENTER: i32 = 0;
    pub const DISSOLVE: i32 = 1;
    pub const CALL_F: i32 = 2;
    pub const CRITTER_FRAME: i32 = 3;
    pub const LIGHT_CONN: i32 = 4;
    pub const SOUNDSCAPE: i32 = 5;
    pub const PENDING: i32 = 6;
    pub const TICK: i32 = 7;
    pub const LOOK_CAMERA: i32 = 8;
    pub const LOOKAT_CALL: i32 = 9;
}

pub mod ent_op {
    pub const HAS_SCRIPT: i32 = 0;
    pub const TIMERS: i32 = 1;
    pub const CALL_F: i32 = 2;
    pub const AUTOSLEEP: i32 = 3;
    pub const CRITTER: i32 = 4;
    pub const COMPONENTS: i32 = 5;
    pub const AGENT: i32 = 6;
    pub const CHECK_COLLISION: i32 = 7;
    pub const LIQUID: i32 = 8;
    pub const ANIMATION: i32 = 9;
    pub const SOCKETS: i32 = 10;
    pub const MOVE: i32 = 11;
    pub const CHAR_COLLISION: i32 = 12;
    pub const PROC_ANIMATIONS: i32 = 13;
    pub const EFFECT_COLOR: i32 = 14;
    pub const LIQUID_AMOUNT: i32 = 15;
    pub const GUI: i32 = 16;
    pub const ATTACHMENT: i32 = 17;
    pub const LOOKS_AT: i32 = 18;
    pub const COLL_NUM: i32 = 19;
    pub const COLL_GET: i32 = 20;
    pub const COLL_REMOVE: i32 = 21;
    pub const COLL_WAS: i32 = 22;
    pub const COLL_NOW: i32 = 23;
    pub const COLL_SWAP: i32 = 24;
    pub const COLL_CALL: i32 = 25;
}

const TYPE_AREA: i32 = 1;
const TYPE_LIQUID_AREA: i32 = 2;
const TYPE_CRITTER: i32 = 4;
const TYPE_AGENT: i32 = 5;
const STR_LOOK_AT_CALLBACK: usize = 8;
const LOOK_AT_PERIOD: f32 = 0.3;

fn map_op(o: &dyn Ops, sub: i32, map: usize, f: f32, s: &[u8]) -> (i32, Io) {
    let mut io = Io::new();
    io.p[0] = map as *mut c_void;
    io.i[0] = sub;
    io.f[0] = f;
    io.s = s.as_ptr();
    io.n = s.len();
    o.go(op::MAP, io)
}

fn ent_op(o: &dyn Ops, sub: i32, ent: usize, f: f32) -> i32 {
    ent_op_io(o, sub, ent, f, &[], 0, 0).0
}

fn ent_op_io(
    o: &dyn Ops,
    sub: i32,
    ent: usize,
    f: f32,
    s: &[u8],
    other: usize,
    n: i32,
) -> (i32, Io) {
    let mut io = Io::new();
    io.p[0] = ent as *mut c_void;
    io.p[1] = other as *mut c_void;
    io.i[0] = sub;
    io.i[1] = n;
    io.f[0] = f;
    io.s = s.as_ptr();
    io.n = s.len();
    o.go(op::ENT, io)
}

fn on(roster: &Roster, i: usize, f: impl FnOnce(usize, *mut Core)) {
    if let Some((ent, core, _)) = roster.entry(i) {
        f(ent, core);
    }
}

fn active(core: *mut Core) -> bool {
    // SAFETY: the pod outlives the roster entry.
    unsafe { (*core).mbActive }
}

fn script_f(o: &dyn Ops, map: usize, decl: &[u8], dt: f32) {
    map_op(o, map_op::CALL_F, map, dt, decl);
}

pub fn update(o: &dyn Ops, map: usize, roster: &Roster, dt: f32) {
    if ohpl_s9_map_stamp(map) == 0 {
        map_op(o, map_op::DUMP_ENTER, map, 0.0, &[]);
    }
    map_op(o, map_op::DISSOLVE, map, dt, &[]);
    timers_update(o, map, dt);
    script_f(o, map, b"void Update(float afTimeStep)", dt);
    script_f(o, map, b"void VariableUpdate(float afDeltaTime)", dt);
    map_op(o, map_op::CRITTER_FRAME, map, 0.0, &[]);
    for (ent, core, _) in roster.snapshot() {
        // SAFETY: the pod outlives the roster entry; no reference is held across a host op.
        let c = unsafe { &mut *core };
        if ent_op(o, ent_op::HAS_SCRIPT, ent, 0.0) == 0 || !c.mbActive {
            continue;
        }
        ent_op(o, ent_op::TIMERS, ent, dt);
        let area = c.meType == TYPE_AREA;
        // SAFETY: `ent` is a live script object registered with the timer table.
        let idle = area && !c.mbCheckCollision && unsafe { ohpl_s5_ts_idle(ent as *mut c_void) };
        if area && !idle {
            c.mbSleeping = false;
        }
        if !c.mbSleeping {
            ent_op(o, ent_op::CALL_F, ent, dt);
            c.mfVariableUpdateAcc += dt;
            if c.mfVariableUpdateAcc > c.mfVariableUpdateRate && c.mfVariableUpdateRate >= 0.0 {
                let acc = c.mfVariableUpdateAcc;
                ent_op_io(o, ent_op::CALL_F, ent, acc, &[], 0, 1);
                c.mfVariableUpdateAcc = 0.0;
            }
            if idle && c.mbAutoSleep && ent_op(o, ent_op::AUTOSLEEP, ent, 0.0) != 0 {
                c.mbSleeping = true;
            }
        }
        match c.meType {
            TYPE_CRITTER => {
                ent_op(o, ent_op::CRITTER, ent, dt);
                ent_op(o, ent_op::COMPONENTS, ent, dt);
            }
            TYPE_AGENT => {
                ent_op(o, ent_op::AGENT, ent, dt);
            }
            TYPE_AREA => {
                ent_op(o, ent_op::CHECK_COLLISION, ent, dt);
            }
            TYPE_LIQUID_AREA => {
                ent_op(o, ent_op::LIQUID, ent, dt);
            }
            _ => {}
        }
        if c.meType != TYPE_CRITTER && c.meType != TYPE_AGENT && !c.mbSleeping {
            ent_op(o, ent_op::COMPONENTS, ent, dt);
        }
    }
    for i in 0..roster.len() {
        on(roster, i, |e, c| {
            if active(c) {
                ent_op(o, ent_op::ANIMATION, e, dt);
            }
        });
        on(roster, i, |e, _| {
            ent_op(o, ent_op::SOCKETS, e, dt);
        });
        on(roster, i, |e, c| {
            if active(c) {
                ent_op(o, ent_op::MOVE, e, dt);
                ent_op(o, ent_op::CHAR_COLLISION, e, dt);
                ent_op(o, ent_op::PROC_ANIMATIONS, e, dt);
            }
        });
        on(roster, i, |e, _| {
            ent_op(o, ent_op::EFFECT_COLOR, e, dt);
            ent_op(o, ent_op::LIQUID_AMOUNT, e, dt);
            ent_op(o, ent_op::GUI, e, dt);
        });
    }
}

pub fn post_update(o: &dyn Ops, map: usize, roster: &mut Roster, dt: f32) {
    for (ent, _, _) in roster.snapshot() {
        ent_op(o, ent_op::ATTACHMENT, ent, dt);
    }
    map_op(o, map_op::LIGHT_CONN, map, 0.0, &[]);
    map_op(o, map_op::SOUNDSCAPE, map, dt, &[]);
    look_at(o, map, roster, dt);
    collide(o, roster);
    script_f(o, map, b"void PostUpdate(float afTimeStep)", dt);
    for i in 0..roster.len() {
        on(roster, i, |e, c| {
            if ent_op(o, ent_op::HAS_SCRIPT, e, 0.0) != 0 && active(c) {
                ent_op_io(o, ent_op::CALL_F, e, dt, &[], 0, 2);
            }
        });
    }
    map_op(o, map_op::PENDING, map, 0.0, &[]);
    roster.flush();
    map_op(o, map_op::TICK, map, 0.0, &[]);
}

fn look_at(o: &dyn Ops, map: usize, roster: &Roster, dt: f32) {
    let cam = map_op(o, map_op::LOOK_CAMERA, map, 0.0, &[]).1.p[1] as usize;
    if cam == 0 {
        return;
    }
    let mut i = 0;
    while let Some((ent, core, names)) = roster.entry(i) {
        i += 1;
        // SAFETY: the pods outlive the roster entry; fields are re-read after every host op.
        let c = unsafe { &mut *core };
        let callback = || unsafe { (*names).str_at(STR_LOOK_AT_CALLBACK).to_vec() };
        if (callback().is_empty() && !c.mbForceLookAtCheck) || !c.mbActive {
            continue;
        }
        if c.mfLookAtCheckTimer > 0.0 {
            c.mfLookAtCheckTimer -= dt;
            continue;
        }
        c.mfLookAtCheckTimer = LOOK_AT_PERIOD;
        let now = ent_op_io(o, ent_op::LOOKS_AT, ent, 0.0, &[], cam, 0).0 != 0;
        if callback().is_empty() {
            c.mbLookedAt = now;
            continue;
        }
        if now != c.mbLookedAt {
            c.mbLookedAt = now;
            if c.mfLookAtCountdown >= 0.0 {
                c.mfLookAtCountdown = -1.0;
                continue;
            }
            c.mfLookAtCountdown = c.mfLookAtDelay.max(0.0);
        }
        if c.mfLookAtCountdown < 0.0 {
            continue;
        }
        c.mfLookAtCountdown -= LOOK_AT_PERIOD;
        if c.mfLookAtCountdown >= 0.0 {
            continue;
        }
        let func = callback();
        if now && c.mbLookAtCallbackAutoRemove {
            // SAFETY: `names` is the entity's live box.
            unsafe {
                ohpl_se_names_set(names as *mut Names, STR_LOOK_AT_CALLBACK, b"".as_ptr(), 0)
            };
        }
        let name = unsafe { (*names).name().to_vec() };
        let joined = [func.as_slice(), name.as_slice()].join(&0u8);
        let mut io = Io::new();
        io.p[0] = map as *mut c_void;
        io.i[0] = map_op::LOOKAT_CALL;
        io.i[1] = if now { 1 } else { -1 };
        io.s = joined.as_ptr();
        io.n = joined.len();
        o.go(op::MAP, io);
    }
}

fn collide(o: &dyn Ops, roster: &mut Roster) {
    let mut e = 0;
    while let Some((parent, core, _)) = roster.entry(e) {
        e += 1;
        if ent_op(o, ent_op::COLL_NUM, parent, 0.0) == 0 || !active(core) {
            continue;
        }
        let mut i = 0;
        while i < ent_op(o, ent_op::COLL_NUM, parent, 0.0) {
            let (_, got) = ent_op_io(o, ent_op::COLL_GET, parent, 0.0, &[], 0, i);
            // SAFETY: the host returns the child and function name joined by NUL, valid until the next op.
            let cb = unsafe { std::slice::from_raw_parts(got.s, got.n) }.to_vec();
            let (child_pat, func) =
                cb.split_at(cb.iter().position(|&b| b == 0).unwrap_or(cb.len()));
            let func = func.get(1..).unwrap_or(&[]);
            let mut remove = false;
            let children = roster.children(child_pat).to_vec();
            for child in children {
                if remove {
                    break;
                }
                let Some((_, ccore, _)) = roster.find(child) else {
                    continue;
                };
                if child == parent || !active(ccore) {
                    continue;
                }
                let was = ent_op_io(o, ent_op::COLL_WAS, parent, 0.0, func, child, 0).0 != 0;
                let (hit, _) = ent_op_io(o, ent_op::COLL_NOW, parent, 0.0, func, child, 0);
                let now = hit != 0;
                if now == was {
                    continue;
                }
                let keep = ent_op_io(
                    o,
                    ent_op::COLL_CALL,
                    parent,
                    0.0,
                    func,
                    child,
                    if now { 1 } else { -1 },
                )
                .0;
                remove = keep == 0;
            }
            if remove {
                ent_op_io(o, ent_op::COLL_REMOVE, parent, 0.0, &cb, 0, 0);
                i -= 1;
            }
            i += 1;
        }
    }
    ent_op(o, ent_op::COLL_SWAP, 0, 0.0);
}

/// # Safety
/// `roster` is the map's live roster.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_slx_map_update(map: usize, roster: *const Roster, dt: f32) {
    // SAFETY: caller contract.
    let r = unsafe { &*roster };
    let _ = with_ops(|o| update(o, map, r, dt));
}

/// # Safety
/// `roster` is the map's live roster.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_slx_map_post_update(map: usize, roster: *mut Roster, dt: f32) {
    // SAFETY: caller contract.
    let r = unsafe { &mut *roster };
    let _ = with_ops(|o| post_update(o, map, r, dt));
}
