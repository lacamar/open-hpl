//! Hand-worked decal cases; bit-exactness against the original C++ is covered by the
//! differential harness (see the commit message).

use super::decal::{ClipParams, Counts, DecalOut, MeshView, UvParams, clip_mesh, uvs};
use crate::math::{IDENTITY, Planef, v3};
use crate::types::{Vector2l, Vector3f};

fn unit_box_planes() -> [Planef; 6] {
    let p = |a, b, c| Planef { a, b, c, d: 1.0 };
    [
        p(1.0, 0.0, 0.0),
        p(-1.0, 0.0, 0.0),
        p(0.0, 1.0, 0.0),
        p(0.0, -1.0, 0.0),
        p(0.0, 0.0, 1.0),
        p(0.0, 0.0, -1.0),
    ]
}

fn params(max_tris: i32) -> ClipParams {
    ClipParams {
        planes: unit_box_planes(),
        world: IDENTITY,
        normal_rot: IDENTITY,
        offset: 0.0,
        max_tris,
    }
}

const FLOOR_POS: [f32; 12] = [
    -5.0, 0.0, -5.0, 1.0, 15.0, 0.0, -5.0, 1.0, -5.0, 0.0, 15.0, 1.0,
];
const NORMALS: [f32; 9] = [0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0];

fn run(p: &ClipParams, pos: &[f32], idx: &[u32]) -> (Counts, DecalOut) {
    let mesh = MeshView {
        positions: pos,
        pos_stride: 4,
        normals: &NORMALS,
        nrm_stride: 3,
        indices: idx,
    };
    let mut counts = Counts::default();
    let mut out = DecalOut::default();
    clip_mesh(p, &mesh, &mut counts, &mut out);
    (counts, out)
}

#[test]
fn floor_clips_to_the_box_footprint() {
    let (counts, out) = run(&params(250), &FLOOR_POS, &[0, 1, 2]);
    assert_eq!(
        counts,
        Counts {
            vertices: 4,
            triangles: 2
        }
    );
    assert_eq!(out.indices, [0, 1, 2, 0, 2, 3]);
    let mut corners: Vec<(i32, i32)> = out
        .positions
        .iter()
        .map(|v| {
            assert_eq!(v.y, 0.0);
            (v.x as i32, v.z as i32)
        })
        .collect();
    corners.sort_unstable();
    assert_eq!(corners, [(-1, -1), (-1, 1), (1, -1), (1, 1)]);
    assert!(out.normals.iter().all(|n| *n == v3(0.0, 1.0, 0.0)));
}

#[test]
fn offset_lifts_along_the_normal() {
    let mut p = params(250);
    p.offset = 0.5;
    let (_, out) = run(&p, &FLOOR_POS, &[0, 1, 2]);
    assert!(out.positions.iter().all(|v| v.y == 0.5));
}

#[test]
fn triangle_limit_rejects_whole_polygon() {
    let (counts, out) = run(&params(1), &FLOOR_POS, &[0, 1, 2]);
    assert_eq!(counts, Counts::default());
    assert!(out.indices.is_empty() && out.positions.is_empty());
}

#[test]
fn backfacing_and_outside_triangles_are_skipped() {
    let (counts, _) = run(&params(250), &FLOOR_POS, &[0, 2, 1]);
    assert_eq!(counts, Counts::default());
    let far: Vec<f32> = FLOOR_POS
        .chunks(4)
        .flat_map(|v| [v[0] + 100.0, v[1], v[2], v[3]])
        .collect();
    let (counts, _) = run(&params(250), &far, &[0, 1, 2]);
    assert_eq!(counts, Counts::default());
}

#[test]
fn out_of_range_index_and_partial_triangle_do_not_panic() {
    let (counts, out) = run(&params(250), &FLOOR_POS, &[0, 1, 9]);
    assert_eq!(counts, Counts::default());
    assert!(out.indices.is_empty());
    let (counts, _) = run(&params(250), &FLOOR_POS, &[0, 1]);
    assert_eq!(counts, Counts::default());
}

#[test]
fn nan_vertex_does_not_panic() {
    let mut pos = FLOOR_POS;
    pos[4] = f32::NAN;
    let _ = run(&params(250), &pos, &[0, 1, 2]);
}

fn uv_params(sub_div: Vector2l, current: i32) -> UvParams {
    UvParams {
        position: v3(0.0, 0.0, 0.0),
        right: v3(1.0, 0.0, 0.0),
        forward: v3(0.0, 0.0, 1.0),
        size_x: 2.0,
        size_z: 2.0,
        sub_div,
        current,
    }
}

fn uv_of(pos: [f32; 4], p: &UvParams) -> Vector3f {
    let mut out = Vec::new();
    uvs(&pos, 4, 1, p, &mut out);
    out.first().copied().unwrap_or_default()
}

#[test]
fn uvs_map_box_to_unit_square() {
    let p = uv_params(Vector2l { x: 1, y: 1 }, 0);
    assert_eq!(uv_of([0.0, 0.0, 0.0, 1.0], &p), v3(0.5, 0.5, 0.0));
    assert_eq!(uv_of([1.0, 0.0, 1.0, 1.0], &p), v3(1.0, 0.0, 0.0));
    assert_eq!(uv_of([-9.0, 0.0, -9.0, 1.0], &p), v3(0.0, 1.0, 0.0));
}

#[test]
fn uvs_select_sub_division_cell() {
    let p = uv_params(Vector2l { x: 2, y: 2 }, 3);
    assert_eq!(uv_of([0.0, 0.0, 0.0, 1.0], &p), v3(0.75, 0.25, 0.0));
}

#[test]
fn uvs_zero_sub_division_does_not_panic() {
    let p = uv_params(Vector2l { x: 0, y: 0 }, 5);
    let _ = uv_of([0.0, 0.0, 0.0, 1.0], &p);
}
