//! Node/entity/renderable matrix composition. The C++ classes keep their lists, flags and vtables;
//! these are the multi-step matrix computations their methods used to chain through `cMath`.

use crate::math::{
    IDENTITY, len3, matrix_inverse, matrix_mul as mul_raw, matrix_rotate, matrix_scale,
};
use crate::types::{Matrixf, Quaternion, Vector3f};
use std::hint::black_box;

// Opaque operands keep LLVM from folding constant entries, which changes the NaN payload an fma
// propagates.
fn matrix_mul(a: &Matrixf, b: &Matrixf) -> Matrixf {
    mul_raw(black_box(a), black_box(b))
}

unsafe extern "C" {
    pub(super) safe fn ohpl_math_matrix_quaternion(q: &Quaternion, out: &mut Matrixf);
    pub(super) safe fn ohpl_math_matrix_to_quaternion(m: &Matrixf, out: &mut Quaternion);
    pub(super) safe fn ohpl_math_quaternion_mul(
        a: &Quaternion,
        b: &Quaternion,
        out: &mut Quaternion,
    );
}

/// `matrix_mul(inverse(parent), world)`: `SetWorldMatrix` of nodes and entities with a parent.
pub fn world_to_local(parent_world: &Matrixf, world: &Matrixf) -> Matrixf {
    matrix_mul(&matrix_inverse(parent_world), world)
}

/// `cNode3D::UpdateMatrix`: animation rotation applied after the local rotation, translation added.
pub fn update_matrix(local: &Matrixf, rotation: &Quaternion, translation: Vector3f) -> Matrixf {
    let mut m = *local;
    let pos = Vector3f {
        x: m.m[0][3],
        y: m.m[1][3],
        z: m.m[2][3],
    };
    m.m[0][3] = 0.0;
    m.m[1][3] = 0.0;
    m.m[2][3] = 0.0;
    let mut rot = IDENTITY;
    ohpl_math_matrix_quaternion(rotation, &mut rot);
    let mut out = matrix_mul(&m, &rot);
    out.m[0][3] = pos.x + translation.x;
    out.m[1][3] = pos.y + translation.y;
    out.m[2][3] = pos.z + translation.z;
    out
}

/// `cNode3D::AddRotation(euler, order)`: new rotation quaternion (`order` as the C++ enum value).
pub fn add_rotation(euler: Vector3f, order: i32, current: &Quaternion) -> Quaternion {
    let mut q = Quaternion::default();
    ohpl_math_matrix_to_quaternion(&matrix_rotate(euler, order), &mut q);
    let mut out = Quaternion::default();
    ohpl_math_quaternion_mul(&q, current, &mut out);
    out
}

/// `cFogArea::GetModelMatrix`.
pub fn scaled_world(world: &Matrixf, size: Vector3f) -> Matrixf {
    mul_raw(world, &matrix_scale(size))
}

/// `iRenderable::GetInvModelMatrix`.
pub fn inverse(m: &Matrixf) -> Matrixf {
    matrix_inverse(m)
}

/// `iRenderable::GetCalcScale`: lengths of the right, up and forward rows.
pub fn calc_scale(m: &Matrixf) -> Vector3f {
    let len = |r: &[f32; 4]| {
        len3(Vector3f {
            x: r[0],
            y: r[1],
            z: r[2],
        })
    };
    Vector3f {
        x: len(&m.m[0]),
        y: len(&m.m[1]),
        z: len(&m.m[2]),
    }
}
