//! `cCamera` matrix construction and picking rays; dirty flags and cached matrices stay in C++.

use crate::math::{
    IDENTITY, fma, matrix_inverse, matrix_mul as mul_raw, matrix_mul_vec, normalize3, rotate_z,
};
use crate::types::{Matrixf, Vector2f, Vector3f};
use std::hint::black_box;

unsafe extern "C" {
    safe fn ohpl_math_matrix_translate(t: &Vector3f, out: &mut Matrixf);
    safe fn ohpl_math_matrix_rotate_x(a: f32, out: &mut Matrixf);
    safe fn ohpl_math_matrix_rotate_y(a: f32, out: &mut Matrixf);
    safe fn ohpl_math_matrix_perspective_projection(
        near: f32,
        far: f32,
        fov: f32,
        aspect: f32,
        inf: bool,
        out: &mut Matrixf,
    );
    safe fn ohpl_math_matrix_orthographic_projection(
        near: f32,
        far: f32,
        view: &Vector2f,
        out: &mut Matrixf,
    );
    fn tanf(x: f32) -> f32;
}

fn tan(x: f32) -> f32 {
    // SAFETY: pure libm function on a plain float.
    unsafe { tanf(x) }
}

pub const ROTATE_EULER: i32 = 0;
pub const ROTATE_MATRIX: i32 = 1;
pub const MOVE_FLY: i32 = 1;
pub const PROJECTION_PERSPECTIVE: i32 = 0;
pub const PROJECTION_ORTHOGRAPHIC: i32 = 1;

// Opaque operands keep LLVM from folding the identity/zero entries, which changes which NaN
// payload an fma propagates.
fn matrix_mul(a: &Matrixf, b: &Matrixf) -> Matrixf {
    mul_raw(black_box(a), black_box(b))
}

fn rotate_x(a: f32) -> Matrixf {
    let mut m = IDENTITY;
    ohpl_math_matrix_rotate_x(a, &mut m);
    m
}

fn rotate_y(a: f32) -> Matrixf {
    let mut m = IDENTITY;
    ohpl_math_matrix_rotate_y(a, &mut m);
    m
}

fn translate(t: Vector3f) -> Matrixf {
    let mut m = IDENTITY;
    ohpl_math_matrix_translate(&t, &mut m);
    m
}

#[derive(Clone, Copy)]
pub struct ViewParams {
    pub rotate_mode: i32,
    pub position: Vector3f,
    pub yaw: f32,
    pub pitch: f32,
    pub roll: f32,
    pub rotation: Matrixf,
}

/// `cCamera::GetViewMatrix` rebuild; unknown rotate modes give identity.
pub fn view_matrix(p: &ViewParams) -> Matrixf {
    // gcc folds the C++ `pos * -1` into a negation: NaN signs flip.
    let neg = Vector3f {
        x: -p.position.x,
        y: -p.position.y,
        z: -p.position.z,
    };
    match p.rotate_mode {
        ROTATE_EULER => {
            let mut v = matrix_mul(&translate(neg), &IDENTITY);
            v = matrix_mul(&rotate_y(-p.yaw), &v);
            v = matrix_mul(&rotate_x(-p.pitch), &v);
            matrix_mul(&rotate_z(-p.roll), &v)
        }
        ROTATE_MATRIX => matrix_mul(&p.rotation, &translate(neg)),
        _ => IDENTITY,
    }
}

/// `cCamera::UpdateMoveMatrix` rebuild; `None` leaves the matrix untouched (non-euler mode).
pub fn move_matrix(rotate_mode: i32, move_mode: i32, yaw: f32, pitch: f32) -> Option<Matrixf> {
    if rotate_mode != ROTATE_EULER {
        return None;
    }
    let m = rotate_y(-yaw);
    Some(if move_mode == MOVE_FLY {
        matrix_mul(&rotate_x(-pitch), &m)
    } else {
        m
    })
}

#[derive(Clone, Copy)]
pub struct ProjectionParams {
    pub kind: i32,
    pub near: f32,
    pub far: f32,
    pub fov: f32,
    pub aspect: f32,
    pub inf_far: bool,
    pub view_size: Vector2f,
}

/// `cCamera::GetProjectionMatrix` rebuild.
pub fn projection_matrix(p: &ProjectionParams) -> Matrixf {
    let mut m = IDENTITY;
    if p.kind == PROJECTION_PERSPECTIVE {
        ohpl_math_matrix_perspective_projection(p.near, p.far, p.fov, p.aspect, p.inf_far, &mut m);
    } else {
        ohpl_math_matrix_orthographic_projection(p.near, p.far, &p.view_size, &mut m);
    }
    m
}

#[derive(Clone, Copy)]
pub struct CameraBasis {
    pub rotation: Matrixf,
    pub position: Vector3f,
    pub forward: Vector3f,
    pub up: Vector3f,
    pub right: Vector3f,
}

/// `cCamera::UnProjectHelper`: `(position, direction)`, `None` for an unknown projection type.
pub fn unproject(
    p: &ProjectionParams,
    screen: Vector2f,
    virtual_size: Vector2f,
    c: &CameraBasis,
) -> Option<(Vector3f, Vector3f)> {
    let nx = (screen.x / virtual_size.x) - 0.5;
    let ny = 0.5 - (screen.y / virtual_size.y);
    if p.kind == PROJECTION_PERSPECTIVE {
        let slope = tan(p.fov * 0.5);
        let vy = slope * p.near * 2.0;
        let vx = vy * p.aspect;
        let dir = Vector3f {
            x: nx * vx,
            y: ny * vy,
            z: -p.near,
        };
        let mut dir = matrix_mul_vec(&matrix_inverse(&c.rotation), dir);
        normalize3(&mut dir);
        Some((c.position, dir))
    } else if p.kind == PROJECTION_ORTHOGRAPHIC {
        let (f, r, u) = (c.forward, c.right, c.up);
        let mut pos = c.position;
        pos.x = fma(f.x, p.near, pos.x);
        pos.y = fma(f.y, p.near, pos.y);
        pos.z = fma(f.z, p.near, pos.z);
        pos.x = fma(r.x * nx, p.view_size.x, pos.x);
        pos.y = fma(r.y * nx, p.view_size.x, pos.y);
        pos.z = fma(r.z * nx, p.view_size.x, pos.z);
        pos.x = fma(u.x * ny, p.view_size.y, pos.x);
        pos.y = fma(u.y * ny, p.view_size.y, pos.y);
        pos.z = fma(u.z * ny, p.view_size.y, pos.z);
        Some((pos, f))
    } else {
        None
    }
}
