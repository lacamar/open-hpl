//! `iLuxHandObject`: the offset matrix and animation names of `LoadSettings`, and the mesh and
//! child-matrix decisions of the loader. Entities, meshes and lights stay in C++.

use super::opc as op_c;
use super::vars::{Vars, put_var};
use crate::math::{matrix_inverse, matrix_mul, matrix_rotate, matrix_scale, to_rad, v3};
use crate::types::{Matrixf, Vector3f};

fn rad(v: Vector3f) -> Vector3f {
    v3(to_rad(v.x), to_rad(v.y), to_rad(v.z))
}

/// `LoadSettings`: the offset matrix; animation and bone names go to their slots.
pub fn load_settings(v: &dyn Vars) -> Matrixf {
    let scale = v.vec3("OffsetScale", v3(1.0, 1.0, 1.0));
    let rotation = rad(v.vec3("OffsetRotation", v3(0.0, 0.0, 0.0)));
    let position = v.vec3("OffsetPosition", v3(0.0, 0.0, 0.0));
    let mut m = matrix_mul(&matrix_rotate(rotation, 0), &matrix_scale(scale));
    m.m[0][3] = position.x;
    m.m[1][3] = position.y;
    m.m[2][3] = position.z;
    put_var(v, op_c::SLOT_HAND_ANIM_IDLE, "HandsAnim_Idle");
    put_var(v, op_c::SLOT_HAND_ANIM_DRAW, "HandsAnim_Draw");
    put_var(v, op_c::SLOT_HAND_ANIM_HOLSTER, "HandsAnim_Holster");
    v.put(
        op_c::SLOT_HAND_BONE,
        &v.string("AttachBoneName", b"attachpoint"),
    );
    m
}

/// `LoadMainData`: the model file name.
pub fn load_main(v: &dyn Vars) {
    put_var(v, op_c::SLOT_HAND_MODEL, "Model");
}

/// `AfterLoad`: the object keeps the loader's mesh when it has none yet (and takes a user count).
pub fn adopt_mesh(has_mesh: bool) -> bool {
    !has_mesh
}

/// `AfterLoad`: inverse of the entity's local matrix, applied to every billboard and light.
pub fn model_inverse(entity_local: &Matrixf) -> Matrixf {
    matrix_inverse(entity_local)
}

pub fn child_local(inv_model: &Matrixf, child: &Matrixf) -> Matrixf {
    matrix_mul(inv_model, child)
}

/// `CreateEntity`: an object that already has its mesh entity is left alone.
pub fn create_needed(has_entity: bool) -> bool {
    !has_entity
}
