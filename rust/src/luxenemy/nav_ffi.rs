//! C ABI of the enemy mover/pathfinder state (`RustLuxEnemy.h`, `ohpl_a4b_*`): create/destroy, the shell's
//! save-data and node accessors and the debug draw.

use super::ctx::Ctx;
use super::ffi::{CHost, OhplGa3Host};
use super::nav::{self, Nav};
use crate::math::v3;

#[repr(C)]
pub struct MoverSave {
    pub turning: bool,
    pub override_move: bool,
    pub turn_goal_angle: f32,
    pub turn_speed: f32,
    pub turn_break_acc: f32,
    pub stuck_counter: f32,
    pub move_state: i32,
}

fn with<R>(bits: u64, d: R, f: impl FnOnce(*mut Nav) -> R) -> R {
    let n = nav::ptr(bits);
    if n.is_null() { d } else { f(n) }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a4b_nav_new() -> u64 {
    nav::create()
}

/// The A* handler address the caller destroys (0 when none).
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a4b_nav_free(nav: u64) -> usize {
    nav::destroy(nav)
}

/// # Safety
/// `c` is a live context.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a4b_nav_fill_ctx(nav: u64, c: *mut Ctx) {
    // SAFETY: caller contract; `n` is live for the call.
    with(nav, (), |n| unsafe { nav::fill_ctx(n, &mut *c) });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a4b_nav_set_nodes(nav: u64, nc: usize, astar: usize) {
    // SAFETY: `n` is live for the call.
    with(nav, (), |n| unsafe {
        (*n).nc = nc;
        (*n).astar = astar;
    });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a4b_nav_nc(nav: u64) -> usize {
    // SAFETY: `n` is live for the call.
    with(nav, 0, |n| unsafe { (*n).nc })
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a4b_nav_cand(nav: u64, idx: i32) -> usize {
    // SAFETY: `n` is live for the call.
    with(nav, 0, |n| unsafe { nav::cand(n, idx) })
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a4b_nav_path_front(nav: u64) -> usize {
    // SAFETY: `n` is live for the call.
    with(nav, 0, |n| unsafe {
        (*n).path.first().copied().unwrap_or(0)
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a4b_nav_path_len(nav: u64) -> usize {
    // SAFETY: `n` is live for the call.
    with(nav, 0, |n| unsafe { (*n).path.len() })
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a4b_nav_path_at(nav: u64, i: usize) -> usize {
    // SAFETY: `n` is live for the call.
    with(nav, 0, |n| {
        // SAFETY: `n` is live for the call; the reference ends with it.
        let path: &Vec<usize> = unsafe { &(*n).path };
        path.get(i).copied().unwrap_or(0)
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a4b_nav_path_push(nav: u64, node: usize) {
    // SAFETY: `n` is live for the call.
    with(nav, (), |n| unsafe { (*n).path.push(node) });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a4b_nav_stop(nav: u64) {
    // SAFETY: `n` is live for the call.
    with(nav, (), |n| unsafe {
        (*n).moving = false;
        (*n).path.clear();
        (*n).win.len = 0;
    });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a4b_nav_moving(nav: u64) -> bool {
    // SAFETY: `n` is live for the call.
    with(nav, false, |n| unsafe { (*n).moving })
}

/// # Safety
/// `out` is three writable floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a4b_nav_goal(nav: u64, out: *mut f32) {
    // SAFETY: caller contract; `n` is live for the call.
    with(nav, (), |n| unsafe {
        out.cast::<[f32; 3]>().write((*n).goal)
    });
}

/// # Safety
/// `goal` is three floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a4b_nav_set_goal(nav: u64, moving: bool, goal: *const f32) {
    // SAFETY: caller contract; `n` is live for the call.
    with(nav, (), |n| unsafe {
        (*n).moving = moving;
        (*n).goal = goal.cast::<[f32; 3]>().read();
    });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a4b_nav_win_len(nav: u64) -> i32 {
    // SAFETY: `n` is live for the call.
    with(nav, 0, |n| unsafe { (*n).win.len })
}

/// # Safety
/// `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a4b_mover_get(nav: u64, out: *mut MoverSave) {
    // SAFETY: caller contract; `n` is live for the call.
    with(nav, (), |n| unsafe {
        let m = &(*n).mover;
        out.write(MoverSave {
            turning: m.turning,
            override_move: m.override_move,
            turn_goal_angle: m.turn_goal_angle,
            turn_speed: m.turn_speed,
            turn_break_acc: m.turn_break_acc,
            stuck_counter: m.stuck_counter,
            move_state: m.move_state,
        });
    });
}

/// # Safety
/// `s` is readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a4b_mover_set(nav: u64, s: *const MoverSave) {
    // SAFETY: caller contract; `n` is live for the call.
    with(nav, (), |n| unsafe {
        let (m, s) = (&mut (*n).mover, &*s);
        m.turning = s.turning;
        m.override_move = s.override_move;
        m.turn_goal_angle = s.turn_goal_angle;
        m.turn_speed = s.turn_speed;
        m.turn_break_acc = s.turn_break_acc;
        m.stuck_counter = s.stuck_counter;
        m.move_state = s.move_state;
    });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a4b_mover_set_move_state(nav: u64, s: i32) {
    // SAFETY: `n` is live for the call.
    with(nav, (), |n| unsafe { (*n).mover.move_state = s });
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a4b_mover_stuck(nav: u64) -> f32 {
    // SAFETY: `n` is live for the call.
    with(nav, 0.0, |n| unsafe { (*n).mover.stuck_counter })
}

/// # Safety
/// `h` is a live host table; `body_pos` is three floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a4b_nav_render(
    nav: u64,
    h: *const OhplGa3Host,
    body_pos: *const f32,
) {
    // SAFETY: caller contract.
    let p = unsafe { v3(*body_pos, *body_pos.add(1), *body_pos.add(2)) };
    // SAFETY: caller contract; `n` is live for the call.
    with(nav, (), |n| unsafe {
        nav::render_debug(n, &mut CHost(&*h), p)
    });
}
