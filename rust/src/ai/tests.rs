use super::*;
use crate::math::v3;

// Expected values were recorded from the original C++ (g++ -O2, aarch64) with the same cast rule.

/// Blocks every crossing of x = 2.
unsafe extern "C" fn cast_wall(
    _: *mut c_void,
    _: u32,
    s: *const Vector3f,
    e: *const Vector3f,
) -> bool {
    // SAFETY: valid ray ends.
    let (s, e) = unsafe { (*s, *e) };
    (s.x - 2.0) * (e.x - 2.0) < 0.0
}

/// Mirrors the harness mock: blocks crossings of x = 1.5 for z within (-0.5, 1.5).
unsafe extern "C" fn cast_lattice(
    _: *mut c_void,
    _: u32,
    s: *const Vector3f,
    e: *const Vector3f,
) -> bool {
    // SAFETY: valid ray ends.
    let (s, e) = unsafe { (*s, *e) };
    (s.x - 1.5) * (e.x - 1.5) < 0.0 && s.z > -0.5 && s.z < 1.5 && e.z > -0.5 && e.z < 1.5
}

unsafe extern "C" fn cast_record(
    ctx: *mut c_void,
    flags: u32,
    s: *const Vector3f,
    e: *const Vector3f,
) -> bool {
    // SAFETY: tests pass a `Vec<(u32, Vector3f, Vector3f)>` as ctx and valid ray ends.
    unsafe { (*ctx.cast::<Vec<(u32, Vector3f, Vector3f)>>()).push((flags, *s, *e)) };
    false
}

fn lattice() -> Graph {
    let mut g = Graph::new(cast_lattice, std::ptr::null_mut(), v3(0.6, 1.5, 0.6));
    g.params.max_edge_dist = 1.5;
    g.params.max_edges = 3;
    g.params.min_edges = 1;
    for z in 0..3 {
        for x in 0..4 {
            g.add_node(v3(x as f32, 0.0, z as f32), (z * 4 + x + 1) as u64);
        }
    }
    g.compile();
    g
}

fn targets(g: &Graph, node: u32) -> Vec<u32> {
    g.edges(node).iter().map(|e| e.to).collect()
}

#[test]
fn compile_orders_equal_distances_like_libstdcxx() {
    let g = lattice();
    // n00: n10 n01 n11; n11: n10 n01 n12; n32: n31 n22 n21
    assert_eq!(targets(&g, 0), [1, 4, 5]);
    assert_eq!(targets(&g, 5), [1, 4, 9]);
    assert_eq!(targets(&g, 11), [7, 10, 6]);
    let diag = g.edges(0).last().copied();
    assert_eq!(
        diag,
        Some(Edge {
            dist: std::f32::consts::SQRT_2,
            sqr: 2.0,
            to: 5
        })
    );
}

#[test]
fn query_walks_cells_row_major() {
    let g = lattice();
    assert_eq!(g.query(v3(2.0, 0.0, 1.0), 1.0), (0..12).collect::<Vec<_>>());
}

#[test]
fn astar_cases() {
    let g = lattice();
    let no_user = std::ptr::null_mut();
    let path = |a: Vector3f, b: Vector3f, it: i32| unsafe { g.astar(a, b, it, None, no_user) };
    assert_eq!(
        path(v3(0.0, 0.0, 0.0), v3(3.0, 0.0, 0.0), -1),
        Some(vec![9])
    );
    assert_eq!(path(v3(0.0, 0.0, 0.0), v3(0.5, 0.0, 0.0), -1), Some(vec![]));
    assert_eq!(path(v3(0.0, 0.0, 0.0), v3(30.0, 0.0, 0.0), -1), None);
    assert_eq!(path(v3(0.0, 0.0, 0.0), v3(3.0, 0.0, 2.0), 1), Some(vec![]));
}

fn crossing_graph(key_a: u64, key_b: u64) -> Graph {
    let mut g = Graph::new(cast_wall, std::ptr::null_mut(), v3(0.6, 1.5, 0.6));
    for (p, k) in [
        (v3(1.0, 0.0, 1.0), key_a),
        (v3(1.0, 0.0, -1.0), key_b),
        (v3(3.0, 0.0, 1.0), 10 + key_a),
        (v3(3.0, 0.0, -1.0), 10 + key_b),
    ] {
        g.add_node(p, k);
    }
    g.build_grid();
    assert!(g.add_edge(0, 2, 2.0, 4.0) && g.add_edge(1, 3, 2.0, 4.0));
    g
}

#[test]
fn equal_cost_ties_follow_node_key_order() {
    for (ka, kb, expect) in [(1, 2, [2u32, 0]), (2, 1, [3, 1])] {
        let g = crossing_graph(ka, kb);
        // SAFETY: no callback.
        let path = unsafe {
            g.astar(
                v3(0.0, 0.0, 0.0),
                v3(4.0, 0.0, 0.0),
                -1,
                None,
                std::ptr::null_mut(),
            )
        };
        assert_eq!(path, Some(expect.to_vec()), "keys {ka} {kb}");
    }
}

#[test]
fn free_path_ray_setup() {
    let mut rays: Vec<(u32, Vector3f, Vector3f)> = Vec::new();
    let mut g = Graph::new(
        cast_record,
        std::ptr::from_mut(&mut rays).cast(),
        v3(1.0, 2.0, 0.5),
    );
    g.params.at_center = false;
    assert!(g.free_path(v3(0.0, 0.0, 0.0), v3(0.0, 0.0, 10.0), 9, 6));
    assert_eq!(rays.len(), 5);
    assert!(rays.iter().all(|r| r.0 == 6));
    assert_eq!(
        rays.first().map(|r| (r.1, r.2)),
        Some((v3(0.0, 1.0, 0.0), v3(0.0, 1.0, 10.0)))
    );
    rays.clear();
    assert!(g.free_path(v3(0.0, 0.0, 0.0), v3(0.0, 0.0, 10.0), 0, 0));
    assert!(rays.is_empty());
}

#[test]
fn empty_graph_is_inert() {
    let mut g = Graph::new(cast_lattice, std::ptr::null_mut(), v3(1.0, 1.0, 1.0));
    g.compile();
    assert!(g.query(v3(0.0, 0.0, 0.0), 5.0).is_empty());
    // SAFETY: no callback.
    let path = unsafe {
        g.astar(
            v3(0.0, 0.0, 0.0),
            v3(9.0, 0.0, 0.0),
            -1,
            None,
            std::ptr::null_mut(),
        )
    };
    assert_eq!(path, None);
}
