//! Per-enemy mover and pathfinder state (`cLuxEnemyMover`, `cLuxEnemyPathfinder`): owned here, one boxed `Nav` per
//! enemy in an arena, addressed from C++ by handle bits. Engine objects (A* handler, node container, nodes) are held
//! as addresses and reached through the host table.

use super::ctx::Ctx;
use super::host::{Host, Io, OP_BODY_POS, OP_BODY_SIZE, a_of, v_of};
use super::mover::MoverState;
use super::pathfinder::PfWin;
use crate::math::{Vector3f, v3};
use crate::obj::{Arena, Handle};
use std::sync::{Mutex, MutexGuard};

pub const OP_PF_MOVE_TO: u32 = 43;
pub const OP_PF_STOP: u32 = 44;
pub const OP_PF_PATH_LEN: u32 = 45;
pub const OP_PF_PATH_BACK: u32 = 46;
pub const OP_PF_PATH_POP: u32 = 47;
pub const OP_PF_PATH_CLEAR: u32 = 48;
pub const OP_NC_PRESENT: u32 = 49;
pub const OP_NC_COUNT: u32 = 50;
pub const OP_NC_ITER_BEGIN: u32 = 51;
pub const OP_NC_ITER_NODE: u32 = 52;
pub const OP_NC_FREE_PATH: u32 = 53;
pub const OP_NC_COMMIT_LAST: u32 = 54;

pub const OP_ASTAR_PATH: u32 = 110;
pub const OP_SCRATCH_AT: u32 = 111;
pub const OP_NODE_POS: u32 = 112;
pub const OP_NC_AT_CENTER: u32 = 113;
pub const OP_NC_ITER: u32 = 114;
pub const OP_NODE_USED_AS_GOAL: u32 = 115;
pub const OP_NC_FREE_PATH_AT: u32 = 116;
pub const OP_NC_NODE_COUNT: u32 = 117;
pub const OP_WATER_STEP_CHECK: u32 = 118;
pub const OP_WATER_STEP_FX: u32 = 119;
pub const OP_NC_NODE_AT: u32 = 120;
pub const OP_NODE_EDGE_NUM: u32 = 121;
pub const OP_NODE_EDGE_NODE: u32 = 122;
pub const OP_NC_COLLIDE_Y: u32 = 123;
pub const OP_DRAW: u32 = 124;

pub struct Nav {
    pub mover: MoverState,
    pub moving: bool,
    pub goal: [f32; 3],
    pub win: PfWin,
    pub(super) path: Vec<usize>,
    pub(super) cands: Vec<usize>,
    pub(super) skip_mode: i32,
    pub(super) last_search: usize,
    pub(super) nc: usize,
    pub(super) astar: usize,
}

impl Nav {
    pub(super) fn new() -> Self {
        Self {
            mover: MoverState::default(),
            moving: false,
            goal: [0.0; 3],
            win: PfWin::default(),
            path: Vec::new(),
            cands: Vec::new(),
            skip_mode: 2,
            last_search: 0,
            nc: 0,
            astar: 0,
        }
    }
}

static NAVS: Mutex<Option<Arena<Box<Nav>>>> = Mutex::new(None);

fn navs() -> MutexGuard<'static, Option<Arena<Box<Nav>>>> {
    NAVS.lock()
        .unwrap_or_else(std::sync::PoisonError::into_inner)
}

pub fn create() -> u64 {
    let nav = Box::new(Nav::new());
    navs()
        .get_or_insert_with(Arena::default)
        .insert(nav)
        .map_or(0, Handle::to_bits)
}

/// Returns the A* handler address the caller must destroy.
pub fn destroy(bits: u64) -> usize {
    let Some(h) = Handle::from_bits(bits) else {
        return 0;
    };
    let nav = navs().as_mut().and_then(|a| a.remove(h));
    nav.map_or(0, |n| n.astar)
}

/// Stable address of the nav (the box never moves); null for a stale handle. Callers alias it only
/// through short accesses, as the enemy Ctx pointers do.
pub fn ptr(bits: u64) -> *mut Nav {
    let Some(h) = Handle::from_bits(bits) else {
        return std::ptr::null_mut();
    };
    navs()
        .as_mut()
        .and_then(|a| a.get_mut(h))
        .map_or(std::ptr::null_mut(), |b| &raw mut **b)
}

/// # Safety
/// `nav` is a live pointer from `ptr`.
pub unsafe fn fill_ctx(nav: *mut Nav, c: &mut Ctx) {
    // SAFETY: caller contract; only field addresses are taken.
    unsafe {
        c.mover = &raw mut (*nav).mover;
        c.pf_moving = &raw mut (*nav).moving;
        c.pf_goal = (&raw mut (*nav).goal).cast::<f32>();
        c.pf_win = &raw mut (*nav).win;
    }
}

fn put_ptr(io: &mut Io, slot: usize, p: usize) {
    if let Some([lo, hi]) = io.i.get_mut(slot..slot + 2) {
        *lo = p as u32 as i32;
        *hi = (p >> 32) as u32 as i32;
    }
}

fn get_ptr(io: &Io, slot: usize) -> usize {
    match io.i.get(slot..slot + 2) {
        Some([lo, hi]) => (*lo as u32 as usize) | ((*hi as u32 as usize) << 32),
        _ => 0,
    }
}

type Inner<'a> = &'a mut dyn FnMut(u32, &mut Io) -> i32;

fn engine(inner: Inner, op: u32, build: impl FnOnce(&mut Io)) -> (i32, Io) {
    let mut io = Io::default();
    build(&mut io);
    let r = inner(op, &mut io);
    (r, io)
}

fn node_pos(inner: Inner, node: usize) -> Vector3f {
    let (_, io) = engine(inner, OP_NODE_POS, |io| put_ptr(io, 0, node));
    v_of(io.v[0])
}

fn fetch(inner: Inner, n: i32) -> Vec<usize> {
    (0..n.max(0))
        .map(|k| {
            let (_, io) = engine(inner, OP_SCRATCH_AT, |io| io.i[0] = k);
            get_ptr(&io, 0)
        })
        .collect()
}

/// The ops whose state lives here; `inner` is the C++ host call for the engine side. Only field
/// references are formed, because the Ctx pointers alias the same box.
///
/// # Safety
/// `nav` is a live pointer from `ptr`.
pub unsafe fn intercept(nav: *mut Nav, inner: Inner, op: u32, io: &mut Io) -> Option<i32> {
    // SAFETY: caller contract for every field access below.
    unsafe {
        Some(match op {
            OP_PF_MOVE_TO => move_to(nav, inner, v_of(io.v[0])),
            OP_PF_STOP => {
                (*nav).moving = false;
                (*nav).path.clear();
                (*nav).win.len = 0;
                0
            }
            OP_PF_PATH_LEN => i32::try_from((*nav).path.len()).unwrap_or(i32::MAX),
            OP_PF_PATH_BACK => {
                if let Some(&n) = (*nav).path.last() {
                    io.v[0] = a_of(node_pos(inner, n));
                }
                0
            }
            OP_PF_PATH_POP => {
                (*nav).path.pop();
                0
            }
            OP_PF_PATH_CLEAR => {
                (*nav).path.clear();
                0
            }
            OP_NC_PRESENT => i32::from((*nav).nc != 0),
            OP_NC_COUNT => engine(inner, OP_NC_NODE_COUNT, |i| put_ptr(i, 0, (*nav).nc)).0,
            OP_NC_ITER_BEGIN => {
                (*nav).skip_mode = io.i[0];
                let (n, _) = engine(inner, OP_NC_ITER, |i| {
                    put_ptr(i, 0, (*nav).nc);
                    i.v[0] = io.v[0];
                    i.f[0] = io.f[0];
                });
                (*nav).cands = fetch(inner, n);
                n
            }
            OP_NC_ITER_NODE => {
                let node = cand(nav, io.i[0]);
                let skip = if (*nav).skip_mode == 1 {
                    (*nav).last_search
                } else {
                    0
                };
                if node == skip {
                    return Some(0);
                }
                if io.i[1] != 0 {
                    let (used, _) = engine(inner, OP_NODE_USED_AS_GOAL, |i| put_ptr(i, 0, node));
                    if used != 0 {
                        return Some(0);
                    }
                }
                io.v[0] = a_of(node_pos(inner, node));
                1
            }
            OP_NC_FREE_PATH => {
                engine(inner, OP_NC_FREE_PATH_AT, |i| {
                    put_ptr(i, 2, (*nav).nc);
                    i.i[0] = io.i[0];
                    i.i[1] = io.i[1];
                    i.v[0] = io.v[0];
                    i.v[1] = io.v[1];
                })
                .0
            }
            OP_NC_COMMIT_LAST => {
                (*nav).last_search = cand(nav, io.i[0]);
                0
            }
            _ => return None,
        })
    }
}

unsafe fn move_to(nav: *mut Nav, inner: Inner, pos: Vector3f) -> i32 {
    // SAFETY: caller contract.
    unsafe {
        (*nav).win.len = 0;
        (*nav).path.clear();
        (*nav).moving = true;
        (*nav).goal = a_of(pos);
        if (*nav).astar == 0 {
            return 0;
        }
        let (_, io) = engine(inner, OP_BODY_POS, |_| {});
        let mut start = v_of(io.v[0]);
        let nc = (*nav).nc;
        let at_center = nc != 0 && engine(inner, OP_NC_AT_CENTER, |i| put_ptr(i, 0, nc)).0 != 0;
        if !at_center {
            let (_, sz) = engine(inner, OP_BODY_SIZE, |_| {});
            start = v3(start.x, start.y - v_of(sz.v[0]).y / 2.0, start.z);
        }
        start.y += 0.01;
        let astar = (*nav).astar;
        let (ok, io) = engine(inner, OP_ASTAR_PATH, |i| {
            put_ptr(i, 0, astar);
            i.v[0] = a_of(start);
            i.v[1] = a_of(pos);
        });
        (*nav).path = fetch(inner, io.i[2]);
        ok
    }
}

/// Water splash on animation step events, as `cLuxEnemyMover::UpdateStepEffects` did.
pub fn step_effects(h: &mut dyn Host) {
    if h.call(OP_WATER_STEP_CHECK, &mut Io::default()) != 0 {
        h.call(OP_WATER_STEP_FX, &mut Io::default());
    }
}

fn host_node_pos(h: &mut dyn Host, node: usize) -> Vector3f {
    let mut io = Io::default();
    put_ptr(&mut io, 0, node);
    h.call(OP_NODE_POS, &mut io);
    v_of(io.v[0])
}

fn draw(h: &mut dyn Host, kind: i32, a: Vector3f, b: Vector3f, r: f32) {
    let mut io = Io::default();
    io.i[0] = kind;
    io.v[0] = a_of(a);
    io.v[1] = a_of(b);
    io.f[0] = r;
    h.call(OP_DRAW, &mut io);
}

fn query(h: &mut dyn Host, op: u32, i: i32, p: usize) -> (i32, Io) {
    let mut io = Io::default();
    put_ptr(&mut io, 0, p);
    io.i[2] = i;
    let r = h.call(op, &mut io);
    (r, io)
}

/// `cLuxEnemyPathfinder::OnRenderSolid`: node graph, goal and path through the host's draw op
/// (kinds 0/1 grey sphere/line, 2/3 magenta sphere/line).
///
/// # Safety
/// `nav` is a live pointer from `ptr`.
pub unsafe fn render_debug(nav: *mut Nav, h: &mut dyn Host, body_pos: Vector3f) {
    // SAFETY: caller contract; the path copy ends the borrow before host calls.
    let (nc, moving, goal, path) =
        unsafe { ((*nav).nc, (*nav).moving, (*nav).goal, (*nav).path.clone()) };
    if nc == 0 {
        return;
    }
    let n = query(h, OP_NC_NODE_COUNT, 0, nc).0;
    for i in 0..n {
        let node = get_ptr(&query(h, OP_NC_NODE_AT, i, nc).1, 0);
        let pos = host_node_pos(h, node);
        draw(h, 0, pos, pos, 0.3);
        let edges = query(h, OP_NODE_EDGE_NUM, 0, node).0;
        for j in 0..edges {
            let to = get_ptr(&query(h, OP_NODE_EDGE_NODE, j, node).1, 0);
            let to = host_node_pos(h, to);
            draw(h, 1, pos, to, 0.0);
        }
    }
    if !moving {
        return;
    }
    let at_center = query(h, OP_NC_AT_CENTER, 0, nc).0 != 0;
    let lift = if at_center {
        0.0
    } else {
        query(h, OP_NC_COLLIDE_Y, 0, nc).1.f[0] / 2.0
    };
    let up = |p: Vector3f| v3(p.x, p.y + lift, p.z);
    let goal = up(v3(goal[0], goal[1], goal[2]));
    draw(h, 2, goal, goal, 0.2);
    let mut last = goal;
    for n in path {
        let p = up(host_node_pos(h, n));
        draw(h, 2, p, p, 0.2);
        draw(h, 3, last, p, 0.0);
        last = p;
    }
    draw(h, 2, body_pos, body_pos, 0.2);
    draw(h, 3, last, body_pos, 0.0);
}

/// # Safety
/// `nav` is a live pointer from `ptr`.
pub unsafe fn cand(nav: *mut Nav, i: i32) -> usize {
    // SAFETY: caller contract; the reference ends with the call.
    let cands: &Vec<usize> = unsafe { &(*nav).cands };
    usize::try_from(i)
        .ok()
        .and_then(|i| cands.get(i).copied())
        .unwrap_or(0)
}
