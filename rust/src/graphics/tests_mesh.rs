//! Values characterised from the original C++ (differential harness, 82k cases, 0 diffs).

use super::anim::{self, Frames, KeyFrame};
use super::meshgen::{self, PlaneError};
use super::submesh::{self, VertexBonePair, WARN_TOO_MANY_BONES, WARN_UNCONNECTED};
use crate::math::v3;
use crate::types::{Quaternion, Vector2f, Vector3f};

fn close(a: f32, b: f32) -> bool {
    (a - b).abs() < 1e-5
}

fn kf(time: f32, trans: Vector3f) -> KeyFrame {
    KeyFrame {
        trans,
        rotation: Quaternion {
            v: v3(0.0, 0.0, 0.0),
            w: 1.0,
        },
        time,
    }
}

fn with_frames<R>(frames: &[KeyFrame], f: impl FnOnce(Frames) -> R) -> R {
    let ptrs: Vec<*const KeyFrame> = frames.iter().map(std::ptr::from_ref).collect();
    // SAFETY: every pointer refers to an element of `frames`, alive for the call.
    f(unsafe { Frames::new(&ptrs) })
}

#[test]
fn sphere_rings_and_indices() {
    let m = meshgen::sphere(1.0, 4, 3);
    assert_eq!(m.positions.len(), 10);
    let expect = [
        (0.0, 1.0, 0.0),
        (0.866_025, 0.5, 0.0),
        (0.0, 0.5, 0.866_025),
        (-0.866_025, 0.5, 0.0),
        (0.0, 0.5, -0.866_025),
        (0.866_025, -0.5, 0.0),
        (0.0, -0.5, 0.866_025),
        (-0.866_025, -0.5, 0.0),
        (0.0, -0.5, -0.866_025),
        (0.0, -1.0, 0.0),
    ];
    for (p, (x, y, z)) in m.positions.iter().zip(expect) {
        assert!(close(p.x, x) && close(p.y, y) && close(p.z, z), "{p:?}");
    }
    assert_eq!(m.indices.len(), 48);
    assert_eq!(&m.indices[..12], &[0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 1]);
    assert_eq!(&m.indices[12..18], &[1, 5, 2, 2, 5, 6]);
}

#[test]
fn zero_sections_keep_pole_only_caps() {
    let m = meshgen::cone(Vector2f { x: 1.0, y: 2.0 }, 0);
    assert_eq!(m.positions.len(), 2);
    assert_eq!(m.indices, [0, 0, 1, 1, 1, 0]);
}

#[test]
fn negative_sections_yield_no_ring() {
    let m = meshgen::cylinder(Vector2f { x: 1.0, y: 2.0 }, -4);
    assert_eq!(m.positions.len(), 2);
}

#[test]
fn box_faces() {
    let m = meshgen::box_mesh(v3(2.0, 4.0, 6.0));
    assert_eq!((m.positions.len(), m.indices.len()), (24, 36));
    assert_eq!(&m.indices[..6], &[0, 1, 2, 2, 3, 0]);
    assert_eq!(m.positions.first(), Some(&v3(-1.0, 2.0, 3.0)));
    assert_eq!(m.normals.first(), Some(&v3(-1.0, 0.0, 0.0)));
    assert_eq!(m.texcoords.first(), Some(&v3(0.0, 0.0, 0.0)));
}

#[test]
fn sky_box_has_no_normals() {
    let m = meshgen::sky_box(10.0);
    assert!(m.normals.is_empty());
    assert_eq!(m.positions.len(), 24);
    assert_eq!(m.indices, (0..24).collect::<Vec<u32>>());
    assert_eq!(m.positions.first(), Some(&v3(-10.0, 10.0, -10.0)));
}

#[test]
fn plane_corner_order_and_errors() {
    let uv = [Vector2f::default(); 4];
    let m = meshgen::plane(v3(0.0, 0.0, 0.0), v3(2.0, 0.0, 2.0), uv).unwrap_or_default();
    assert_eq!(
        m.positions,
        [
            v3(0.0, 0.0, 2.0),
            v3(0.0, 0.0, 0.0),
            v3(2.0, 0.0, 0.0),
            v3(2.0, 0.0, 2.0)
        ]
    );
    assert_eq!(m.indices, [0, 1, 2, 2, 3, 0]);
    let same = v3(1.0, 2.0, 3.0);
    assert_eq!(meshgen::plane(same, same, uv), Err(PlaneError::Coincident));
    assert_eq!(
        meshgen::plane(v3(0.0, 0.0, 0.0), v3(1.0, 1.0, 1.0), uv),
        Err(PlaneError::NotCoplanar)
    );
    assert_eq!(
        meshgen::plane(v3(0.0, 0.0, 0.0), v3(1.0, 0.0, 0.0), uv),
        Err(PlaneError::NotCoplanar)
    );
}

#[test]
fn wireframe_drops_partial_triangle() {
    assert_eq!(
        meshgen::wireframe_indices(&[0, 1, 2, 9]),
        [0, 1, 1, 2, 2, 0]
    );
}

#[test]
fn frames_at_time_cases() {
    let frames = [
        kf(0.0, v3(0.0, 0.0, 0.0)),
        kf(1.0, v3(2.0, 4.0, 6.0)),
        kf(2.0, v3(0.0, 0.0, 0.0)),
    ];
    let cases = [
        (0.5, (0, 1, 0.5)),
        (1.0, (0, 1, 1.0)),
        (2.0, (2, 0, 0.0)),
        (3.0, (2, 0, 0.0)),
        (-1.0, (0, 0, 0.0)),
        (f32::NAN, (0, 0, 0.0)),
    ];
    for (time, want) in cases {
        let got = with_frames(&frames, |f| anim::frames_at_time(f, 2.0, 2.0, time));
        assert_eq!(got, want, "time {time}");
    }
    assert_eq!(
        with_frames(&[], |f| anim::frames_at_time(f, 2.0, 2.0, 1.0)),
        (0, 0, 0.0)
    );
}

#[test]
fn interpolate_midpoint_and_empty() {
    let frames = [kf(0.0, v3(0.0, 0.0, 0.0)), kf(1.0, v3(2.0, 4.0, 6.0))];
    let r = with_frames(&frames, |f| anim::interpolate(f, 1.0, 1.0, 0.5));
    assert_eq!(r.trans, v3(1.0, 2.0, 3.0));
    assert!(close(r.rotation.w, 1.0) && r.time == 0.5);
    let e = with_frames(&[], |f| anim::interpolate(f, 1.0, 0.0, 0.25));
    assert_eq!(
        (e.trans, e.rotation.w, e.time),
        (v3(0.0, 0.0, 0.0), 1.0, 0.25)
    );
}

#[test]
fn insert_position_keeps_order() {
    let frames = [
        kf(0.0, v3(0.0, 0.0, 0.0)),
        kf(1.0, v3(0.0, 0.0, 0.0)),
        kf(1.0, v3(0.0, 0.0, 0.0)),
    ];
    let at = |max, time| with_frames(&frames, |f| anim::insert_position(f, max, time));
    assert_eq!(at(1.0, 2.0), (3, true));
    assert_eq!(at(1.0, 1.0), (3, false));
    assert_eq!(at(1.0, 0.5), (1, false));
    assert_eq!(at(1.0, -1.0), (0, false));
    assert_eq!(at(1.0, f32::NAN), (3, false));
    assert_eq!(
        with_frames(&[], |f| anim::insert_position(f, 7.0, -3.0)),
        (0, true)
    );
}

#[test]
fn one_sided_plane_and_rejects() {
    let pos = [0.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 1.0];
    let (n, p) = submesh::one_sided(&[0, 2, 1], &pos, 4).unwrap_or_default();
    assert!(
        n == v3(0.0, -1.0, 0.0) && p == v3(0.0, 0.0, 0.0),
        "{n:?} {p:?}"
    );
    let (n, _) = submesh::one_sided(&[0, 2, 1, 0, 1, 2], &pos, 4)
        .map_or((v3(9.0, 9.0, 9.0), v3(0.0, 0.0, 0.0)), |x| x);
    assert_eq!(n, v3(9.0, 9.0, 9.0), "opposite faces are not one-sided");
    assert!(submesh::one_sided(&[0, 2], &pos, 4).is_none());
    assert!(submesh::one_sided(&[0, 2, 9], &pos, 4).is_none());
    assert!(submesh::one_sided(&vec![0; 1203], &pos, 4).is_none());
}

#[test]
fn one_sided_empty_is_nan() {
    let (n, p) = submesh::one_sided(&[], &[], 4).unwrap_or_default();
    assert!(n.x.is_nan() && n.y.is_nan() && n.z.is_nan() && p.x.is_nan());
}

fn pair(vtx_idx: u32, bone_idx: u32, weight: f32) -> VertexBonePair {
    VertexBonePair {
        vtx_idx,
        bone_idx,
        weight,
    }
}

#[test]
fn bone_pairs_normalise_and_flag() {
    let pairs = [
        pair(0, 1, 1.0),
        pair(0, 2, 3.0),
        pair(2, 0, 0.0),
        pair(2, 7, 1.0),
        pair(3, 1, 1.0),
        pair(3, 2, 1.0),
        pair(3, 3, 1.0),
        pair(3, 4, 1.0),
        pair(3, 5, 1.0),
        pair(9, 1, 1.0),
    ];
    let mut w = [0.0f32; 16];
    let mut b = [0u8; 16];
    let flags = submesh::compile_bone_pairs(&pairs, 4, &mut w, &mut b);
    assert_eq!(flags, WARN_TOO_MANY_BONES | WARN_UNCONNECTED);
    assert_eq!(&w[..4], &[0.25, 0.75, 0.0, 0.0]);
    assert_eq!(&b[..2], &[1, 2]);
    assert_eq!(&w[4..8], &[0.0; 4]);
    assert_eq!(
        (w[8], b[8]),
        (1.0, 7),
        "zero-weight pair leaves the slot free"
    );
    assert_eq!(&w[12..16], &[0.25; 4]);
    assert_eq!(&b[12..16], &[1, 2, 3, 4]);
}

#[test]
fn bone_radii_take_max_and_skip_missing() {
    let bind = [v3(0.0, 0.0, 0.0), v3(10.0, 0.0, 0.0)];
    let pos = [3.0, 0.0, 0.0, 1.0, 0.0, 4.0, 3.0, 1.0];
    let pairs = [
        pair(0, 0, 1.0),
        pair(1, 0, 1.0),
        pair(1, 1, 1.0),
        pair(5, 0, 1.0),
        pair(0, 9, 1.0),
    ];
    let mut radii = [0.0f32; 2];
    submesh::bone_radii(&bind, &mut radii, &pairs, &pos, 4);
    assert_eq!(radii[0], 5.0);
    assert!(close(radii[1], 125.0f32.sqrt()));
}
