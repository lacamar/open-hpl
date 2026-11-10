use super::*;

fn params() -> Params {
    Params {
        min_leaf_objects: 12,
        min_side_length: 3.0,
        max_side_length: 30.0,
        max_intersection_amount: 0.03,
        max_volume_calc_objects: 200,
        min_force_intersection_relative_size: 0.8,
    }
}

fn boxed(c: Vector3f, h: Vector3f) -> Object {
    Object {
        min: v3(c.x - h.x, c.y - h.y, c.z - h.z),
        max: v3(c.x + h.x, c.y + h.y, c.z + h.z),
        center: c,
    }
}

fn depth(t: &Tree, mut i: usize) -> usize {
    let mut d = 0;
    while let Some(n) = t.nodes.get(i) {
        if n.parent == NO_PARENT {
            break;
        }
        i = n.parent as usize;
        d += 1;
    }
    d
}

/// One line per node in the format of the C++ harness: depth, min, max, radius, objects.
fn dump(t: &Tree) -> Vec<String> {
    t.nodes
        .iter()
        .enumerate()
        .map(|(i, n)| {
            let objs = t
                .objects
                .iter()
                .skip(n.first_object as usize)
                .take(n.object_count as usize)
                .map(u32::to_string)
                .collect::<Vec<_>>()
                .join(" ");
            format!(
                "{} {} {} {} {} {} {} r={} objs {}",
                depth(t, i),
                n.min.x,
                n.min.y,
                n.min.z,
                n.max.x,
                n.max.y,
                n.max.z,
                n.radius,
                objs
            )
        })
        .collect()
}

/// Compares text lines, radius to 5 digits (the recording printed `%g`).
fn assert_dump(t: &Tree, golden: &str) {
    let got = dump(t);
    let want: Vec<&str> = golden.lines().collect();
    assert_eq!(got.len(), want.len(), "node count");
    for (g, w) in got.iter().zip(want) {
        let (gh, gt) = g.split_once(" r=").unwrap_or((g, ""));
        let (wh, wt) = w.split_once(" r=").unwrap_or((w, ""));
        assert_eq!(gh, wh);
        let (gr, go) = gt.split_once(' ').unwrap_or((gt, ""));
        let (wr, wo) = wt.split_once(' ').unwrap_or((wt, ""));
        let (gr, wr): (f32, f32) = (
            gr.parse().unwrap_or(f32::NAN),
            wr.parse().unwrap_or(f32::NAN),
        );
        assert!((gr - wr).abs() < 1e-4, "radius {gr} vs {wr}");
        assert_eq!(go.trim_end(), wo.trim_end());
    }
}

const GRID: &str = "\
0 -2.5 -2.5 -2.5 37.5 7.5 17.5 r=22.9129 objs
1 -2.5 -2.5 -2.5 17.5 7.5 17.5 r=15 objs
2 -2.5 -2.5 -2.5 7.5 7.5 17.5 r=12.2474 objs
3 -2.5 -2.5 -2.5 7.5 7.5 7.5 r=8.66025 objs
4 -2.5 -2.5 -2.5 2.5 7.5 7.5 r=7.5 objs 0 8 16 24
4 2.5 -2.5 -2.5 7.5 7.5 7.5 r=7.5 objs 1 9 17 25
3 -2.5 -2.5 7.5 7.5 7.5 17.5 r=8.66025 objs
4 -2.5 -2.5 7.5 2.5 7.5 17.5 r=7.5 objs 32 40 48 56
4 2.5 -2.5 7.5 7.5 7.5 17.5 r=7.5 objs 33 41 49 57
2 7.5 -2.5 -2.5 17.5 7.5 17.5 r=12.2474 objs
3 7.5 -2.5 -2.5 17.5 7.5 7.5 r=8.66025 objs
4 7.5 -2.5 -2.5 12.5 7.5 7.5 r=7.5 objs 2 10 18 26
4 12.5 -2.5 -2.5 17.5 7.5 7.5 r=7.5 objs 3 11 19 27
3 7.5 -2.5 7.5 17.5 7.5 17.5 r=8.66025 objs
4 7.5 -2.5 7.5 12.5 7.5 17.5 r=7.5 objs 34 42 50 58
4 12.5 -2.5 7.5 17.5 7.5 17.5 r=7.5 objs 35 43 51 59
1 17.5 -2.5 -2.5 37.5 7.5 17.5 r=15 objs
2 17.5 -2.5 -2.5 27.5 7.5 17.5 r=12.2474 objs
3 17.5 -2.5 -2.5 27.5 7.5 7.5 r=8.66025 objs
4 17.5 -2.5 -2.5 22.5 7.5 7.5 r=7.5 objs 4 12 20 28
4 22.5 -2.5 -2.5 27.5 7.5 7.5 r=7.5 objs 5 13 21 29
3 17.5 -2.5 7.5 27.5 7.5 17.5 r=8.66025 objs
4 17.5 -2.5 7.5 22.5 7.5 17.5 r=7.5 objs 36 44 52 60
4 22.5 -2.5 7.5 27.5 7.5 17.5 r=7.5 objs 37 45 53 61
2 27.5 -2.5 -2.5 37.5 7.5 17.5 r=12.2474 objs
3 27.5 -2.5 -2.5 37.5 7.5 7.5 r=8.66025 objs
4 27.5 -2.5 -2.5 32.5 7.5 7.5 r=7.5 objs 6 14 22 30
4 32.5 -2.5 -2.5 37.5 7.5 7.5 r=7.5 objs 7 15 23 31
3 27.5 -2.5 7.5 37.5 7.5 17.5 r=8.66025 objs
4 27.5 -2.5 7.5 32.5 7.5 17.5 r=7.5 objs 38 46 54 62
4 32.5 -2.5 7.5 37.5 7.5 17.5 r=7.5 objs 39 47 55 63";

const SCATTER: &str = "\
0 -4 -1 -1.5 40 3 41.5 r=30.8261 objs
1 -4 -1 -1.5 19 3 41.5 r=24.4643 objs
2 -1 -1 -1.5 19 3 25.5 r=16.9189 objs
3 -1 -1 -1.5 16 1 17.5 r=12.7867 objs 0 36
3 8 -1 18.5 19 3 25.5 r=6.81909 objs 23 34 10 21 32
2 -4 -1 26.5 13 3 41.5 r=11.5109 objs
3 -4 -1 30.5 7 3 41.5 r=8.03119 objs
4 -4 -1 30.5 7 2 41.5 r=7.92149 objs 39 15 28 13
4 -3 1 34.5 6 3 41.5 r=5.78792 objs 2 26
3 3 -1 26.5 13 3 37.5 r=7.6974 objs
4 3 -1 26.5 13 3 29.5 r=5.59017 objs 30 19 8
4 4 -1 30.5 12 3 37.5 r=5.67891 objs 17 4 6
1 15 -1 -1.5 40 3 41.5 r=24.9499 objs
2 22 -1 -1.5 40 3 9.5 r=10.7355 objs
3 28 -1 -1.5 40 3 5.5 r=7.22842 objs 11 22 33 9 20
3 22 -1 2.5 34 3 9.5 r=7.22842 objs 31 7 18 29
2 15 -1 10.5 38 3 41.5 r=19.4036 objs
3 15 -1 10.5 29 3 13.5 r=7.43303 objs 38 5 16 27
3 16 -1 14.5 38 3 41.5 r=17.5285 objs
4 16 -1 14.5 28 3 21.5 r=7.22842 objs 3 14 25 1 12
4 29 -1 34.5 38 3 41.5 r=6.04152 objs 37 35 24";

// Both recorded from the original C++ (g++ -O2 -mcpu=apple-m2): a tie-heavy grid through the
// volume heuristic, and a scatter whose big nodes take the std::sort median path.
#[test]
fn compile_matches_cpp_grid() {
    let objs: Vec<Object> = (0..64)
        .map(|i| {
            boxed(
                v3(
                    ((i % 8) * 5) as f32,
                    (((i >> 3) % 2) * 5) as f32,
                    ((i >> 4) * 5) as f32,
                ),
                v3(2.5, 2.5, 2.5),
            )
        })
        .collect();
    let p = Params {
        min_leaf_objects: 4,
        ..params()
    };
    assert_dump(&compile(&objs, &p), GRID);
}

#[test]
fn compile_matches_cpp_scatter_with_sort_path() {
    let objs: Vec<Object> = (0..40)
        .map(|i| {
            let x = ((i * 7) % 13) as f32 * 3.0;
            let z = ((i * 5) % 11) as f32 * 4.0;
            boxed(v3(x, (i % 3) as f32, z), v3(1.0 + (i % 4) as f32, 1.0, 1.5))
        })
        .collect();
    let p = Params {
        min_leaf_objects: 3,
        max_volume_calc_objects: 10,
        ..params()
    };
    assert_dump(&compile(&objs, &p), SCATTER);
}

#[test]
fn compile_empty_and_single() {
    let t = compile(&[], &params());
    assert_eq!(t.nodes.len(), 1);
    assert_eq!(t.objects.len(), 0);
    assert_eq!(
        t.nodes.first().map(|n| (n.min, n.max)),
        Some((v3(BIG, BIG, BIG), v3(-BIG, -BIG, -BIG)))
    );
    let one = [boxed(v3(1.0, 2.0, 3.0), v3(1.0, 1.0, 1.0))];
    let t = compile(&one, &params());
    assert_eq!(t.objects, vec![0]);
}

#[test]
fn compile_places_every_object_once_even_for_degenerate_boxes() {
    let mut s = 0x2545F4914F6CDD1Du64;
    let mut next = move || {
        s ^= s >> 12;
        s ^= s << 25;
        s ^= s >> 27;
        (s.wrapping_mul(0x2545F4914F6CDD1D) >> 40) as f32 / 16_777_216.0
    };
    for round in 0..60 {
        let n = 1 + round * 7;
        let objs: Vec<Object> = (0..n)
            .map(|i| {
                let c = v3(
                    next() * 400.0 - 200.0,
                    next() * 40.0,
                    next() * 400.0 - 200.0,
                );
                let h = match i % 9 {
                    0 => v3(0.0, 0.0, 0.0),
                    1 => v3(f32::INFINITY, 1.0, 1.0),
                    2 => v3(f32::NAN, 1.0, 1.0),
                    3 => v3(1e30, 1e30, 1e30),
                    _ => v3(next() * 5.0, next() * 5.0, next() * 5.0),
                };
                let c = if i % 11 == 0 {
                    v3(f32::NAN, 0.0, 0.0)
                } else {
                    c
                };
                boxed(c, h)
            })
            .collect();
        let t = compile(
            &objs,
            &Params {
                max_volume_calc_objects: 20,
                ..params()
            },
        );
        let mut seen = vec![0u8; n];
        for &o in &t.objects {
            *seen.get_mut(o as usize).unwrap() += 1;
        }
        assert!(seen.iter().all(|&c| c == 1), "round {round}");
        for (i, node) in t.nodes.iter().enumerate() {
            assert!(node.parent == NO_PARENT || (node.parent as usize) < i);
        }
    }
}

#[test]
fn split_group_boundaries() {
    let size = v3(10.0, 10.0, 10.0);
    let g = |lo: f32, hi: f32| split_group(v3(lo, 0.0, 0.0), v3(hi, 1.0, 1.0), 5.0, 0, size, 0.2);
    assert_eq!(g(5.0, 7.0), 0);
    assert_eq!(g(1.0, 5.0), 1);
    assert_eq!(g(4.0, 6.0), 1);
    assert_eq!(g(3.0, 9.0), 0);
    assert_eq!(g(0.0, 10.0), 2);
    // zero-size node: the ratio is inf or NaN, never within the threshold
    assert_eq!(
        split_group(
            v3(4.0, 0.0, 0.0),
            v3(6.0, 1.0, 1.0),
            5.0,
            0,
            v3(0.0, 0.0, 0.0),
            0.2
        ),
        2
    );
    // axis outside 0..3 reads 0 for every value: both bounds equal the plane
    assert_eq!(
        split_group(v3(4.0, 0.0, 0.0), v3(6.0, 1.0, 1.0), 0.0, -1, size, 0.2),
        0
    );
}

#[test]
fn longest_axis_prefers_earlier_on_ties() {
    assert_eq!(longest_axis(v3(3.0, 3.0, 3.0)), 0);
    assert_eq!(longest_axis(v3(1.0, 2.0, 2.0)), 1);
    assert_eq!(longest_axis(v3(1.0, 2.0, 3.0)), 2);
    assert_eq!(longest_axis(v3(f32::NAN, 1.0, 2.0)), 0);
}

#[test]
fn at_edge_uses_epsilon() {
    let (n0, n1) = (v3(0.0, 0.0, 0.0), v3(10.0, 10.0, 10.0));
    assert!(at_edge(v3(0.00005, 5.0, 5.0), v3(6.0, 6.0, 6.0), n0, n1));
    assert!(!at_edge(v3(0.0002, 5.0, 5.0), v3(6.0, 6.0, 6.0), n0, n1));
    assert!(at_edge(v3(1.0, 1.0, 1.0), v3(2.0, 2.0, 9.99995), n0, n1));
    assert!(!at_edge(v3(f32::NAN, 1.0, 1.0), v3(2.0, 2.0, 2.0), n0, n1));
}
