//! `cBeam` quad geometry and colours.

use crate::math::{Matrixf, Vector3f, cross3, fma, len3, normalize3, sub3, v3};
use crate::types::Color;

#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct BeamGeometry {
    pub axis: Vector3f,
    pub mid: Vector3f,
    pub pos: [[f32; 3]; 4],
    pub uv: [[f32; 2]; 4],
}

fn vertex_color(c: Color, multiply_alpha: bool) -> [f32; 4] {
    if multiply_alpha {
        [c.r * c.a, c.g * c.a, c.b * c.a, c.a]
    } else {
        [c.r, c.g, c.b, c.a]
    }
}

/// Fills `out` (two vertex colours, 8 floats) for the lower or upper beam edge.
pub fn set_edge_colors(c: Color, multiply_alpha: bool, out: &mut [f32]) {
    let rgba = vertex_color(c, multiply_alpha);
    for v in out.chunks_exact_mut(4).take(2) {
        v.copy_from_slice(&rgba);
    }
}

/// `cBeam::GetBoundingVolume` local box around both ends, padded by the beam width.
pub fn bounds(start: Vector3f, end: Vector3f, width: f32) -> (Vector3f, Vector3f) {
    let mut max = start;
    let mut min = start;
    if max.x < end.x {
        max.x = end.x;
    }
    if max.y < end.y {
        max.y = end.y;
    }
    if max.z < end.z {
        max.z = end.z;
    }
    if min.x > end.x {
        min.x = end.x;
    }
    if min.y > end.y {
        min.y = end.y;
    }
    if min.z > end.z {
        min.z = end.z;
    }
    (
        v3(min.x - width, min.y - width, min.z - width),
        v3(max.x + width, max.y + width, max.z + width),
    )
}

pub fn geometry(start: Vector3f, end: Vector3f, size: [f32; 2], tile_height: bool) -> BeamGeometry {
    let mut axis = sub3(end, start);
    let mid = v3(
        fma(axis.x, 0.5, start.x),
        fma(axis.y, 0.5, start.y),
        fma(axis.z, 0.5, start.z),
    );
    let dist = len3(axis);
    normalize3(&mut axis);

    let (hx, hy) = (size[0] / 2.0, dist / 2.0);
    let top = if tile_height { -dist / size[1] } else { 0.0 };
    BeamGeometry {
        axis,
        mid,
        pos: [
            [hx, -hy, 0.0],
            [-hx, -hy, 0.0],
            [-hx, hy, 0.0],
            [hx, hy, 0.0],
        ],
        uv: [[1.0, 1.0], [0.0, 1.0], [0.0, top], [1.0, top]],
    }
}

pub fn model_matrix(
    world: &Matrixf,
    world_pos: Vector3f,
    camera: Vector3f,
    axis: Vector3f,
    mid: Vector3f,
) -> Matrixf {
    let mut m = *world;
    let mut cam_fwd = sub3(camera, world_pos);
    normalize3(&mut cam_fwd);
    let up = axis;
    let mut right = cross3(up, cam_fwd);
    normalize3(&mut right);
    let fwd = cross3(right, up);
    let cols = [right, up, fwd];
    for (c, v) in cols.iter().enumerate() {
        for (row, value) in m.m.iter_mut().zip([v.x, v.y, v.z]) {
            if let Some(cell) = row.get_mut(c) {
                *cell = value;
            }
        }
    }
    for (row, value) in m.m.iter_mut().zip([mid.x, mid.y, mid.z]) {
        if let Some(cell) = row.get_mut(3) {
            *cell = value;
        }
    }
    m
}
