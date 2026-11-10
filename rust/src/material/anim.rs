//! `cMaterial::UpdateAnimations`: the UV matrix at an animation time.

use super::common::matrix_translate;
use crate::math::{IDENTITY, matrix_mul, matrix_rotate, sin, v3};
use crate::types::{Matrixf, Vector3f};

pub(crate) const TRANSLATE: i32 = 0;
pub(crate) const SIN: i32 = 1;
pub(crate) const ROTATE: i32 = 2;

/// `cMaterialUvAnimation`
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct UvAnim {
    pub kind: i32,
    pub speed: f32,
    pub amp: f32,
    pub axis: i32,
}

const _: () = assert!(std::mem::size_of::<UvAnim>() == 16);

fn axis_vector(axis: i32) -> Vector3f {
    match axis {
        0 => v3(1.0, 0.0, 0.0),
        1 => v3(0.0, 1.0, 0.0),
        2 => v3(0.0, 0.0, 1.0),
        _ => v3(0.0, 0.0, 0.0),
    }
}

fn scale(v: Vector3f, s: f32) -> Vector3f {
    v3(v.x * s, v.y * s, v.z * s)
}

pub fn uv_matrix(anims: &[UvAnim], time: f32) -> Matrixf {
    let mut m = IDENTITY;
    for a in anims {
        let dir = axis_vector(a.axis);
        match a.kind {
            TRANSLATE => {
                let add = matrix_translate(scale(scale(dir, a.speed), time));
                m = matrix_mul(&m, &add);
            }
            SIN => {
                let add = matrix_translate(scale(scale(dir, sin(time * a.speed)), a.amp));
                m = matrix_mul(&m, &add);
            }
            ROTATE => {
                let rot = matrix_rotate(scale(scale(dir, a.speed), time), 0);
                m = matrix_mul(&m, &rot);
            }
            _ => {}
        }
    }
    m
}
