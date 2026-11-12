//! C ABI for `ai/mod.rs`, mirrored in `HPL2/core/include/ai/RustAI.h`.
//! The graph is owned by the `cAINodeContainer` that created it; nodes are addressed by insertion index.

use super::{CanAddFn, CastFn, Graph, Params};
use crate::types::Vector3f;
use std::ffi::c_void;

/// # Safety
/// `size` valid; `cast` accepts `ctx` for the graph's lifetime.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ai_graph_new(
    cast: CastFn,
    ctx: *mut c_void,
    size: *const Vector3f,
) -> *mut Graph {
    // SAFETY: caller contract.
    let size = unsafe { size.as_ref() }.copied().unwrap_or_default();
    Box::into_raw(Box::new(Graph::new(cast, ctx, size)))
}

/// # Safety
/// `g` comes from `ohpl_ai_graph_new` and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ai_graph_free(g: *mut Graph) {
    if !g.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(g) });
    }
}

/// # Safety
/// `g` live; `pos` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ai_graph_add_node(g: *mut Graph, pos: *const Vector3f, key: u64) {
    // SAFETY: caller contract.
    if let (Some(g), Some(pos)) = unsafe { (g.as_mut(), pos.as_ref()) } {
        g.add_node(*pos, key);
    }
}

/// # Safety
/// `g` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ai_graph_set_params(
    g: *mut Graph,
    max_edges: i32,
    min_edges: i32,
    max_edge_dist: f32,
    max_height: f32,
    at_center: bool,
) {
    // SAFETY: caller contract.
    if let Some(g) = unsafe { g.as_mut() } {
        g.params = Params {
            max_edges,
            min_edges,
            max_edge_dist,
            max_height,
            at_center,
        };
    }
}

/// # Safety
/// `g` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ai_graph_build_grid(g: *mut Graph) {
    // SAFETY: caller contract.
    if let Some(g) = unsafe { g.as_mut() } {
        g.build_grid();
    }
}

/// # Safety
/// `g` live; its cast callback must not re-enter the graph mutably.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ai_graph_compile(g: *mut Graph) {
    // SAFETY: caller contract.
    if let Some(g) = unsafe { g.as_mut() } {
        g.compile();
    }
}

/// # Safety
/// `g` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ai_graph_edge_count(g: *const Graph, node: u32) -> u32 {
    // SAFETY: caller contract.
    unsafe { g.as_ref() }.map_or(0, |g| g.edges(node).len() as u32)
}

/// # Safety
/// `g` live; outputs valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ai_graph_edge(
    g: *const Graph,
    node: u32,
    index: u32,
    to: *mut u32,
    dist: *mut f32,
    sqr: *mut f32,
) -> bool {
    // SAFETY: caller contract.
    let (Some(g), Some(to), Some(dist), Some(sqr)) = (
        unsafe { g.as_ref() },
        unsafe { to.as_mut() },
        unsafe { dist.as_mut() },
        unsafe { sqr.as_mut() },
    ) else {
        return false;
    };
    match g.edges(node).get(index as usize) {
        Some(e) => {
            *to = e.to;
            *dist = e.dist;
            *sqr = e.sqr;
            true
        }
        None => false,
    }
}

/// # Safety
/// `g` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ai_graph_add_edge(
    g: *mut Graph,
    from: u32,
    to: u32,
    dist: f32,
    sqr: f32,
) -> bool {
    // SAFETY: caller contract.
    unsafe { g.as_mut() }.is_some_and(|g| g.add_edge(from, to, dist, sqr))
}

/// # Safety
/// `g` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ai_graph_connect(g: *mut Graph, from: u32, to: u32) -> bool {
    // SAFETY: caller contract.
    unsafe { g.as_mut() }.is_some_and(|g| g.connect(from, to))
}

/// # Safety
/// `g` live; `start` and `end` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ai_graph_free_path(
    g: *const Graph,
    start: *const Vector3f,
    end: *const Vector3f,
    ray_num: i32,
    flags: u32,
) -> bool {
    // SAFETY: caller contract.
    match unsafe { (g.as_ref(), start.as_ref(), end.as_ref()) } {
        (Some(g), Some(s), Some(e)) => g.free_path(*s, *e, ray_num, flags),
        _ => true,
    }
}

/// Stores node indices in the graph's result buffer; returns the count.
///
/// # Safety
/// `g` live; `pos` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ai_graph_query(
    g: *const Graph,
    pos: *const Vector3f,
    radius: f32,
) -> usize {
    // SAFETY: caller contract.
    let (Some(g), Some(pos)) = (unsafe { g.as_ref() }, unsafe { pos.as_ref() }) else {
        return 0;
    };
    let found = g.query(*pos, radius);
    let n = found.len();
    if let Ok(mut r) = g.result.try_borrow_mut() {
        *r = found;
    }
    n
}

/// A* from `start` to `goal`. Returns whether a path exists; the path (goal first, empty when the
/// direct line is free) is left in the result buffer.
///
/// # Safety
/// `g` live; `start` and `goal` valid; `can_add` accepts `user`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ai_graph_astar(
    g: *const Graph,
    start: *const Vector3f,
    goal: *const Vector3f,
    max_iter: i32,
    can_add: Option<CanAddFn>,
    user: *mut c_void,
) -> bool {
    // SAFETY: caller contract.
    let (Some(g), Some(start), Some(goal)) =
        (unsafe { g.as_ref() }, unsafe { start.as_ref() }, unsafe {
            goal.as_ref()
        })
    else {
        return false;
    };
    // SAFETY: caller contract.
    match unsafe { g.astar(*start, *goal, max_iter, can_add, user) } {
        Some(path) => {
            if let Ok(mut r) = g.result.try_borrow_mut() {
                *r = path;
            }
            true
        }
        None => false,
    }
}

/// Copies up to `cap` entries of the result buffer; returns the buffer length.
///
/// # Safety
/// `g` live; `out` valid for `cap` writes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ai_graph_result(g: *const Graph, out: *mut u32, cap: usize) -> usize {
    // SAFETY: caller contract.
    let Some(g) = (unsafe { g.as_ref() }) else {
        return 0;
    };
    let Ok(r) = g.result.try_borrow() else {
        return 0;
    };
    if !out.is_null() {
        // SAFETY: caller contract, `n <= cap`.
        let dst = unsafe { std::slice::from_raw_parts_mut(out, cap.min(r.len())) };
        for (d, s) in dst.iter_mut().zip(r.iter()) {
            *d = *s;
        }
    }
    r.len()
}

/// # Safety
/// `g` live; `name` readable for `len`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ai_graph_name_node(
    g: *mut Graph,
    node: u32,
    name: *const u8,
    len: usize,
    id: i32,
) {
    // SAFETY: caller contract.
    if let Some(g) = unsafe { g.as_mut() } {
        g.name_node(node, unsafe { std::slice::from_raw_parts(name, len) }, id);
    }
}

/// Node index or -1.
///
/// # Safety
/// `g` live; `name` readable for `len`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ai_graph_find_name(
    g: *const Graph,
    name: *const u8,
    len: usize,
) -> i64 {
    // SAFETY: caller contract.
    unsafe { g.as_ref() }
        .and_then(|g| g.find_name(unsafe { std::slice::from_raw_parts(name, len) }))
        .map_or(-1, i64::from)
}

/// Node index or -1.
///
/// # Safety
/// `g` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ai_graph_find_id(g: *const Graph, id: i32) -> i64 {
    // SAFETY: caller contract.
    unsafe { g.as_ref() }
        .and_then(|g| g.find_id(id))
        .map_or(-1, i64::from)
}
