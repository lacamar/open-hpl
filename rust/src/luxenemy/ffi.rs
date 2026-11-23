//! C ABI of the enemy core, mirrored in `amnesia/src/game/RustLuxEnemy.h`.
#![allow(unused_unsafe)]

use super::ctx::{Ctx, Env};
use super::grunt::{self, GruntHandler};
use super::host::{Host, Io, v_of};
use super::machine::{Handler, Msg, Queue, update_state_machine};
use super::mover::{MoverParams, MoverState};
use super::nav;
use super::pathfinder::{self, PfWin};
use crate::math::Vector3f;
use std::ffi::c_void;

const _: () = {
    use std::mem::{offset_of, size_of};
    assert!(size_of::<Msg>() == 36 && offset_of!(Msg, state) == 32);
    assert!(size_of::<Io>() == 84 && offset_of!(Io, v) == 48);
    assert!(size_of::<MoverState>() == 32);
    assert!(size_of::<MoverParams>() == 56);
    assert!(size_of::<PfWin>() == 612);
};

#[repr(C)]
pub struct OhplGa3Host {
    pub ctx: *mut c_void,
    pub call: unsafe extern "C" fn(*mut c_void, u32, *mut Io) -> i32,
    pub nav: u64,
}

pub(super) struct CHost<'a>(pub(super) &'a OhplGa3Host);

impl Host for CHost<'_> {
    fn call(&mut self, op: u32, io: &mut Io) -> i32 {
        let h = self.0;
        // SAFETY: the caller of the entry point guarantees the table accepts its `ctx`.
        let mut inner = |op: u32, io: &mut Io| unsafe { (h.call)(h.ctx, op, io) };
        let nav = nav::ptr(h.nav);
        if !nav.is_null() {
            // SAFETY: `nav` is live (resolved just now) and the engine ops cannot destroy it.
            if let Some(r) = unsafe { nav::intercept(nav, &mut inner, op, io) } {
                return r;
            }
        }
        inner(op, io)
    }
}

type StateCb = unsafe extern "C" fn(*mut c_void, i32, i32, *const Msg) -> bool;

struct CHandler {
    cb: StateCb,
    ctx: *mut c_void,
}

impl Handler for CHandler {
    fn event(&mut self, state: i32, ev: i32, msg: Option<&Msg>) -> bool {
        let p = msg.map_or(std::ptr::null(), std::ptr::from_ref);
        // SAFETY: `cb` and `ctx` come from the C++ caller; `p` outlives the call.
        unsafe { (self.cb)(self.ctx, state, ev, p) }
    }
}

macro_rules! host {
    ($h:expr) => {
        // SAFETY: the pointer is a live table for the call.
        &mut CHost(unsafe { &*$h })
    };
}

macro_rules! env {
    ($e:ident, $c:expr, $h:expr) => {
        // SAFETY: both pointers are live for the call.
        let mut __ch = CHost(unsafe { &*$h });
        let $e = &mut Env::new(unsafe { &*$c }, &mut __ch);
    };
}

unsafe fn v3p(p: *const f32) -> Vector3f {
    // SAFETY: `p` points at three floats.
    unsafe { v_of([*p, *p.add(1), *p.add(2)]) }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga3_queue_new() -> *mut Queue {
    Box::into_raw(Box::default())
}

/// # Safety
/// `q` comes from `ohpl_ga3_queue_new` and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga3_queue_free(q: *mut Queue) {
    if !q.is_null() {
        // SAFETY: allocated by `Box::into_raw` above.
        drop(unsafe { Box::from_raw(q) });
    }
}

/// # Safety
/// `q` is a live queue.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga3_queue_len(q: *const Queue) -> usize {
    // SAFETY: caller contract.
    unsafe { (&*q).msgs.len() }
}

/// # Safety
/// `q` is a live queue and `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga3_queue_get(q: *const Queue, i: usize, out: *mut Msg) -> bool {
    // SAFETY: caller contract.
    match unsafe { (&*q).msgs.get(i) } {
        Some(m) => {
            // SAFETY: caller contract.
            unsafe { *out = *m };
            true
        }
        None => false,
    }
}

/// # Safety
/// `q` is a live queue and `m` is readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga3_queue_push_raw(q: *mut Queue, m: *const Msg) {
    // SAFETY: caller contract.
    unsafe { (&mut *q).msgs.push(*m) };
}

/// # Safety
/// `c` and `h` are live; `v` is three floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga3_send_message(
    c: *const Ctx,
    h: *const OhplGa3Host,
    ty: i32,
    time: f32,
    local: bool,
    v: *const f32,
    f: f32,
    i: i32,
) {
    env!(e, c, h);
    // SAFETY: caller contract.
    let v = unsafe { v3p(v) };
    e.send(ty, time, local, v, f, i);
}

/// `cb` null runs the Grunt handler in Rust.
///
/// # Safety
/// `c` and `h` are live for the call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga3_update_state_machine(
    c: *const Ctx,
    h: *const OhplGa3Host,
    dt: f32,
    cb: Option<StateCb>,
    cb_ctx: *mut c_void,
) {
    env!(e, c, h);
    let st = e.st();
    let q = e.c.queue;
    match cb {
        Some(cb) => update_state_machine(&st, q, dt, &mut CHandler { cb, ctx: cb_ctx }),
        None => update_state_machine(&st, q, dt, &mut GruntHandler { e }),
    }
}

/// # Safety
/// `c`, `h` are live; `m` is null or readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga3_grunt_event(
    c: *const Ctx,
    h: *const OhplGa3Host,
    state: i32,
    ev: i32,
    m: *const Msg,
) -> bool {
    use super::machine::Handler;
    env!(e, c, h);
    // SAFETY: caller contract.
    let m = unsafe { m.as_ref() };
    GruntHandler { e }.event(state, ev, m)
}

/// # Safety
/// `c`, `h` are live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga3_grunt_player_is_detected(
    c: *const Ctx,
    h: *const OhplGa3Host,
) -> bool {
    {
        env!(e, c, h);
        grunt::player_is_detected(e)
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_ga3_grunt_damage_mul(cur: i32) -> f32 {
    grunt::damage_mul(cur)
}

fn mover(nav: u64) -> *mut MoverState {
    let n = nav::ptr(nav);
    // SAFETY: a live nav pointer; only a field address is taken.
    if n.is_null() {
        std::ptr::null_mut()
    } else {
        unsafe { &raw mut (*n).mover }
    }
}

/// # Safety
/// `p`, `h` are live; `nav` is a live handle.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga3_mover_update(
    nav: u64,
    p: *const MoverParams,
    dt: f32,
    h: *const OhplGa3Host,
) {
    let m = mover(nav);
    if m.is_null() {
        return;
    }
    // SAFETY: caller contract.
    unsafe { (*m).on_update(&*p, dt, host!(h)) };
    nav::step_effects(host!(h));
}

/// # Safety
/// `p`, `h` are live; `nav` is a live handle.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga3_mover_update_move_animation(
    nav: u64,
    p: *const MoverParams,
    h: *const OhplGa3Host,
) {
    let m = mover(nav);
    if !m.is_null() {
        // SAFETY: caller contract.
        unsafe { (*m).update_move_animation(&*p, host!(h)) };
    }
}

/// # Safety
/// `p`, `h` are live; `nav` is a live handle.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga3_mover_speed_mul(
    nav: u64,
    p: *const MoverParams,
    h: *const OhplGa3Host,
) -> f32 {
    let m = mover(nav);
    if m.is_null() {
        return 1.0;
    }
    // SAFETY: caller contract.
    unsafe { (*m).calculate_speed_mul(&*p, host!(h)) }
}

/// # Safety
/// `h` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga3_mover_move_speed(h: *const OhplGa3Host) -> f32 {
    MoverState::move_speed(host!(h))
}

/// # Safety
/// `h` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga3_mover_wanted_speed_amount(h: *const OhplGa3Host) -> f32 {
    MoverState::wanted_speed_amount(host!(h))
}

macro_rules! sense_fn {
    ($name:ident, $m:ident $(, $a:ident: $t:ty)*) => {
        /// # Safety
        /// `c`, `h` are live.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name(c: *const Ctx, h: *const OhplGa3Host $(, $a: $t)*) {
            env!(e, c, h);
            e.$m($($a),*);
        }
    };
}

sense_fn!(ohpl_ga3_update_can_see_player, update_can_see_player, dt: f32);
sense_fn!(ohpl_ga3_update_player_in_range, update_player_in_range);
sense_fn!(ohpl_ga3_update_check_stuck_at_door, update_check_stuck_at_door, dt: f32);
sense_fn!(ohpl_ga3_update_check_last_player_pos, update_check_last_player_pos, dt: f32);
sense_fn!(ohpl_ga3_update_regen_health, update_regen_health, dt: f32);

/// `detected_now` is the enemy type's `PlayerIsDetected()`; it is only evaluated when the player lives.
///
/// # Safety
/// `c`, `h` are live; `cb` accepts `cb_ctx`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga3_update_player_detected(
    c: *const Ctx,
    h: *const OhplGa3Host,
    cb: unsafe extern "C" fn(*mut c_void) -> bool,
    cb_ctx: *mut c_void,
) {
    // SAFETY: caller contract.
    env!(e, c, h);
    e.update_player_detected(|_| unsafe { cb(cb_ctx) });
}

/// # Safety
/// `c`, `h`, `p` are live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga3_update_char_body(
    c: *const Ctx,
    h: *const OhplGa3Host,
    p: *const MoverParams,
) {
    // SAFETY: caller contract.
    env!(e, c, h);
    e.update_char_body(unsafe { &*p });
}

/// # Safety
/// `c`, `h`, `p` are live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga3_update_moving(c: *const Ctx, h: *const OhplGa3Host) {
    env!(e, c, h);
    pathfinder::update_moving(e);
}

/// Index of the picked node in the host's last iteration, or -1.
///
/// # Safety
/// `c`, `h` are live; `p` is four floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga3_search_for_player_node(
    c: *const Ctx,
    h: *const OhplGa3Host,
    iters: i32,
    a: f32,
    b: f32,
    d: f32,
    p: *const [f32; 4],
) -> i32 {
    // SAFETY: caller contract.
    env!(e, c, h);
    e.search_for_player_node(iters, a, b, d, unsafe { &*p })
        .unwrap_or(-1)
}

/// # Safety
/// `c`, `h` are live; `p` is three floats; `start` is three floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga3_patrol_around_player_node(
    c: *const Ctx,
    h: *const OhplGa3Host,
    iters: i32,
    a: f32,
    b: f32,
    d: f32,
    p: *const [f32; 3],
    start: *const f32,
) -> i32 {
    // SAFETY: caller contract.
    env!(e, c, h);
    e.patrol_around_player_node(iters, a, b, d, unsafe { &*p }, unsafe { v3p(start) })
        .unwrap_or(-1)
}

/// # Safety
/// `c`, `h` are live; `pos`, `size` are three floats; `src` is null or three floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga3_line_of_sight(
    c: *const Ctx,
    h: *const OhplGa3Host,
    pos: *const f32,
    size: *const f32,
    check_fov: bool,
    src: *const f32,
) -> bool {
    // SAFETY: caller contract.
    let source = if src.is_null() {
        None
    } else {
        Some(unsafe { v3p(src) })
    };
    env!(e, c, h);
    e.line_of_sight(unsafe { v3p(pos) }, unsafe { v3p(size) }, check_fov, source)
}

/// # Safety
/// `c`, `h` are live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga3_dist_to_player(c: *const Ctx, h: *const OhplGa3Host) -> f32 {
    env!(e, c, h);
    e.dist_to_player()
}

/// # Safety
/// `c`, `h` are live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga3_dist_to_player_2d(c: *const Ctx, h: *const OhplGa3Host) -> f32 {
    env!(e, c, h);
    e.dist_to_player_2d()
}

/// # Safety
/// `c`, `h` are live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ga3_player_movement_toward_enemy(
    c: *const Ctx,
    h: *const OhplGa3Host,
) -> f32 {
    env!(e, c, h);
    e.player_movement_toward_enemy_amount()
}
