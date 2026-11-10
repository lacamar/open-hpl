//! `cBillboard` kernels: the per-frustum model matrix and the vertex colour/position rewrites.
//! Float expressions follow the gcc build (`fma` marks a contracted `a*b + c`).

use super::particles::{rotation_part, translation};
use crate::math::{cross3, fma, matrix_mul_vec, normalize3, v3};
use crate::types::{Color, Matrixf, Vector2f, Vector3f};

pub const POINT: i32 = 0;
pub const FIXED_AXIS: i32 = 2;

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct ModelIn {
    pub kind: i32,
    pub forward_offset: f32,
    pub world: Matrixf,
    pub cam_origin: Vector3f,
    pub view_up: Vector3f,
    pub axis: Vector3f,
}

/// `cBillboard::GetModelMatrix` with a frustum. The flag is the C++ "Right vector is not correct"
/// warning condition.
pub fn model_matrix(i: &ModelIn) -> (Matrixf, bool) {
    if i.kind == FIXED_AXIS {
        return (i.world, false);
    }
    let mut m = i.world;
    let pos = translation(&i.world);
    let mut cam_fwd = v3(
        i.cam_origin.x - pos.x,
        i.cam_origin.y - pos.y,
        i.cam_origin.z - pos.z,
    );
    normalize3(&mut cam_fwd);

    let mut warn = false;
    let (right, up, fwd);
    if i.kind == POINT {
        fwd = cam_fwd;
        right = cross3(i.view_up, fwd);
        up = cross3(fwd, right);
    } else {
        let mut u = matrix_mul_vec(&rotation_part(&i.world), i.axis);
        normalize3(&mut u);
        warn = u == cam_fwd;
        let mut r = cross3(u, cam_fwd);
        normalize3(&mut r);
        up = u;
        right = r;
        fwd = cross3(right, up);
    }

    if i.forward_offset != 0.0 {
        let o = i.forward_offset;
        let t = v3(
            fma(cam_fwd.x, o, pos.x),
            fma(cam_fwd.y, o, pos.y),
            fma(cam_fwd.z, o, pos.z),
        );
        set_translation(&mut m, t);
    }

    for (col, v) in [(0, right), (1, up), (2, fwd)] {
        for (row, x) in [v.x, v.y, v.z].into_iter().enumerate() {
            if let Some(e) = m.m.get_mut(row).and_then(|r| r.get_mut(col)) {
                *e = x;
            }
        }
    }
    (m, warn)
}

fn set_translation(m: &mut Matrixf, t: Vector3f) {
    for (row, x) in [t.x, t.y, t.z].into_iter().enumerate() {
        if let Some(e) = m.m.get_mut(row).and_then(|r| r.get_mut(3)) {
            *e = x;
        }
    }
}

/// `UpdateVertexColors`: four identical RGBA vertices.
pub fn vertex_colors(
    color: Color,
    brightness: f32,
    halo_alpha: f32,
    hdr: bool,
    out: &mut [f32; 16],
) {
    let rgb_mul = halo_alpha * brightness;
    let mut c = color;
    if hdr {
        c = Color {
            r: c.r * c.r,
            g: c.g * c.g,
            b: c.b * c.b,
            a: c.a,
        };
    }
    let v = [
        c.r * rgb_mul,
        c.g * rgb_mul,
        c.b * rgb_mul,
        color.a * halo_alpha,
    ];
    for q in out.chunks_exact_mut(4) {
        q.copy_from_slice(&v);
    }
}

/// `SetSize`: the four quad corners, stride 4.
pub fn vertex_positions(size: Vector2f, out: &mut [f32; 16]) {
    let (hx, hy) = (size.x / 2.0, size.y / 2.0);
    let corners = [(hx, -hy), (-hx, -hy), (-hx, hy), (hx, hy)];
    for (q, (x, y)) in out.chunks_exact_mut(4).zip(corners) {
        if let [a, b, c, ..] = q {
            *a = x;
            *b = y;
            *c = 0.0;
        }
    }
}
