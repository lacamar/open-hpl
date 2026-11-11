//! `cRopeEntity` bounds, colours and camera-facing quad strip.

use crate::math::{Vector3f, cross3, expand_aabb, fma, len3, normalize3, sub3, v3};
use crate::types::Color;

/// Fills `out` (4 floats per vertex) with the rope colour.
pub fn set_colors(c: Color, multiply_alpha: bool, out: &mut [f32]) {
    let (r, g, b) = if multiply_alpha {
        (c.r * c.a, c.g * c.a, c.b * c.a)
    } else {
        (c.r, c.g, c.b)
    };
    for v in out.chunks_exact_mut(4) {
        v.copy_from_slice(&[r, g, b, c.a]);
    }
}

pub fn bounds(points: &[Vector3f], radius: f32) -> (Vector3f, Vector3f) {
    let mut min = v3(1_000_000.0, 1_000_000.0, 1_000_000.0);
    let mut max = v3(-1_000_000.0, -1_000_000.0, -1_000_000.0);
    for p in points {
        expand_aabb(&mut min, &mut max, *p, *p);
    }
    (
        v3(min.x - radius, min.y - radius, min.z - radius),
        v3(max.x + radius, max.y + radius, max.z + radius),
    )
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct RopeParams {
    pub forward: Vector3f,
    pub radius: f32,
    pub segment_length: f32,
    pub tile_amount: f32,
    pub tile_size: f32,
}

pub struct RopeOut<'a> {
    pub pos: &'a mut [f32],
    pub uv: &'a mut [f32],
    pub normal: &'a mut [f32],
    pub tangent: &'a mut [f32],
}

const POS_ADD: [(f32, f32); 4] = [(1.0, 0.0), (-1.0, 0.0), (-1.0, 1.0), (1.0, 1.0)];

fn offset(prev: f32, right: f32, up: f32, add: (f32, f32), p: &RopeParams, len: f32) -> f32 {
    fma(up * add.1, len, fma(right * add.0, p.radius, prev))
}

/// One quad per consecutive point pair, starting at the first. Returns the number of quads.
pub fn quads(points: &[Vector3f], p: &RopeParams, out: RopeOut) -> usize {
    let first = points.first().copied().unwrap_or_default();
    let tex = [
        (1.0, 1.0 * p.tile_amount),
        (0.0, 1.0 * p.tile_amount),
        (0.0, 0.0),
        (1.0, 0.0),
    ];
    let cells = out
        .pos
        .chunks_exact_mut(16)
        .zip(out.uv.chunks_exact_mut(12))
        .zip(out.normal.chunks_exact_mut(12))
        .zip(out.tangent.chunks_exact_mut(16));
    let mut prev = first;
    let mut written = 0;
    for (index, (point, (((qp, qu), qn), qt))) in points.iter().skip(1).zip(cells).enumerate() {
        let delta = sub3(*point, prev);
        let len = len3(delta);
        let up = v3(delta.x / len, delta.y / len, delta.z / len);
        let mut right = cross3(up, p.forward);
        normalize3(&mut right);
        let fwd = cross3(right, up);

        for (vert, add) in qp.chunks_exact_mut(4).zip(POS_ADD) {
            vert.copy_from_slice(&[
                offset(prev.x, right.x, up.x, add, p, len),
                offset(prev.y, right.y, up.y, add, p, len),
                offset(prev.z, right.z, up.z, add, p, len),
                1.0,
            ]);
        }

        let first = index == 0 && (len < p.segment_length || p.segment_length == 0.0);
        for (i, (vert, (tx, ty))) in qu.chunks_exact_mut(3).zip(tex).enumerate() {
            let ty = match (first, i < 2) {
                (true, true) if p.segment_length == 0.0 => ty - (1.0 - len / p.tile_size),
                (true, true) => fma(-(1.0 - len / p.segment_length), p.tile_amount, ty),
                _ => ty,
            };
            vert.copy_from_slice(&[tx, ty, 0.0]);
        }

        for n in qn.chunks_exact_mut(3) {
            n.copy_from_slice(&[fwd.x, fwd.y, fwd.z]);
        }
        for t in qt.chunks_exact_mut(4) {
            t.copy_from_slice(&[right.x, right.y, right.z, 1.0]);
        }
        prev = *point;
        written += 1;
    }
    written
}
