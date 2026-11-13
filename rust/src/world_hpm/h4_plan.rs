//! Decisions of `cHpmWorldBuilder` (`WorldLoaderHpmBuilder.cpp`): what to create, with which
//! transform, in which order. The C++ side executes the plans against the engine.

use crate::math::{
    Matrixf, Quaternion, Vector3f, fma, matrix_inverse, matrix_mul, matrix_quaternion,
    matrix_rotate, matrix_scale,
};
use std::cmp::Ordering;

const XYZ: i32 = 0;

fn set_translation(m: &mut Matrixf, t: Vector3f) {
    for (row, v) in m.m.iter_mut().zip([t.x, t.y, t.z]) {
        if let Some(c) = row.get_mut(3) {
            *c = v;
        }
    }
}

/// `MatrixRotate(rot, XYZ)` with `SetTranslation(pos)`.
pub fn rot_trans(rot: Vector3f, pos: Vector3f) -> Matrixf {
    let mut m = matrix_rotate(rot, XYZ);
    set_translation(&mut m, pos);
    m
}

pub struct StaticXform {
    pub world: Matrixf,
    pub body: Matrixf,
}

/// Static object and plane: world matrix `rotate * scale`, the entity position set apart.
pub fn static_xform(rot: Vector3f, scale: Vector3f, pos: Vector3f) -> StaticXform {
    StaticXform {
        world: matrix_mul(&matrix_rotate(rot, XYZ), &matrix_scale(scale)),
        body: rot_trans(rot, pos),
    }
}

/// A static object stays a combine candidate unless a bone rest pose was applied to it.
pub fn is_combine_candidate(has_pose: bool, pose_applied: bool) -> bool {
    !has_pose || !pose_applied
}

pub struct LightMaskPlan {
    pub center: Vector3f,
    pub extent: Vector3f,
}

pub fn light_mask(
    rot: Vector3f,
    size: Vector3f,
    scale: Vector3f,
    world_pos: Vector3f,
) -> LightMaskPlan {
    let r = matrix_rotate(rot, XYZ);
    let half = Vector3f {
        x: size.x * scale.x * 0.5,
        y: size.y * scale.y * 0.5,
        z: size.z * scale.z * 0.5,
    };
    let ext = |row: [f32; 4]| {
        let [a, b, c, _] = row;
        2.0 * fma(c.abs(), half.z, fma(a.abs(), half.x, b.abs() * half.y))
    };
    let [r0, r1, r2, _] = r.m;
    LightMaskPlan {
        center: world_pos,
        extent: Vector3f {
            x: ext(r0),
            y: ext(r1),
            z: ext(r2),
        },
    }
}

pub struct ExposurePlan {
    pub inv_transform: Matrixf,
    pub half_size: Vector3f,
}

pub fn exposure_area(rot: Vector3f, world_pos: Vector3f, scale: Vector3f) -> ExposurePlan {
    ExposurePlan {
        inv_transform: matrix_inverse(&rot_trans(rot, world_pos)),
        half_size: Vector3f {
            x: scale.x * 0.5,
            y: scale.y * 0.5,
            z: scale.z * 0.5,
        },
    }
}

/// gcc fuses `x*x + y*y` as `fma(x, x, y*y)`.
fn length(v: Vector3f) -> f32 {
    fma(v.z, v.z, fma(v.x, v.x, v.y * v.y)).sqrt()
}

/// Largest of the min and max corner lengths over the submesh bounds; 0 without submeshes.
pub fn detail_mesh_radius(bounds: &[(Vector3f, Vector3f)]) -> f32 {
    let mut lo = Vector3f {
        x: 1e9,
        y: 1e9,
        z: 1e9,
    };
    let mut hi = Vector3f {
        x: -1e9,
        y: -1e9,
        z: -1e9,
    };
    for (mn, mx) in bounds {
        lo = Vector3f {
            x: lo.x.min(mn.x),
            y: lo.y.min(mn.y),
            z: lo.z.min(mn.z),
        };
        hi = Vector3f {
            x: hi.x.max(mx.x),
            y: hi.y.max(mx.y),
            z: hi.z.max(mx.z),
        };
    }
    let (a, b) = (length(lo), length(hi));
    if a > b { a } else { b }
}

/// Instance scale is the stored radius over the mesh radius; unscaled without either.
pub fn detail_scale(radius: Option<f32>, mesh_radius: f32) -> f32 {
    match radius {
        Some(r) if mesh_radius > 0.0 => r / mesh_radius,
        _ => 1.0,
    }
}

/// `quat` is stored w, x, y, z.
pub fn detail_matrix(quat: [f32; 4], pos: Vector3f, scale: f32) -> Matrixf {
    let [w, x, y, z] = quat;
    let q = Quaternion {
        v: Vector3f { x, y, z },
        w,
    };
    let mut m = matrix_mul(
        &matrix_quaternion(&q),
        &matrix_scale(Vector3f {
            x: scale,
            y: scale,
            z: scale,
        }),
    );
    set_translation(&mut m, pos);
    m
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct SubMeshPlan {
    pub is_collider_mesh: bool,
    pub is_char_collider_mesh: bool,
    pub hide: bool,
}

fn starts(name: &[u8], prefix: &[u8]) -> bool {
    name.starts_with(prefix)
}

pub fn has_collider_mesh<'a>(names: impl IntoIterator<Item = &'a [u8]>) -> bool {
    names.into_iter().any(|n| starts(n, b"_collider"))
}

pub fn sub_mesh_plan(name: &[u8]) -> SubMeshPlan {
    let ch = starts(name, b"_charcollider");
    let col = ch || starts(name, b"_collider");
    SubMeshPlan {
        is_collider_mesh: col,
        is_char_collider_mesh: ch,
        hide: col || starts(name, b"_noncharcollider"),
    }
}

/// Whether the submesh triangles go into a static batch, given its colliders.
pub fn sub_mesh_batches(
    has_collider_mesh: bool,
    plan: SubMeshPlan,
    collider_is_char: impl IntoIterator<Item = bool>,
) -> bool {
    let mut triangles = !has_collider_mesh;
    for c in collider_is_char {
        if !c {
            triangles = false;
        }
    }
    triangles || plan.is_collider_mesh
}

pub const STATIC_BATCH_MAX_INDICES: usize = 50_000;

pub fn batch_must_flush(batch_indices: usize, adding: usize) -> bool {
    batch_indices + adding > STATIC_BATCH_MAX_INDICES
}

fn cell(v: f32, div: f32) -> i32 {
    (v / div).floor() as i32
}

/// Mesh batch cell: 16 units per axis.
pub fn mesh_cell(center: Vector3f) -> [i32; 3] {
    [
        cell(center.x, 16.0),
        cell(center.y, 16.0),
        cell(center.z, 16.0),
    ]
}

/// Entity body batch cell: 8 x 8 x 24.
pub fn body_cell(pos: Vector3f) -> [i32; 3] {
    [cell(pos.x, 8.0), cell(pos.y, 8.0), cell(pos.z, 24.0)]
}

pub struct Cand<'a> {
    pub ent: u32,
    pub cell: [i32; 3],
    pub mat: u64,
    pub shadow: bool,
    pub one_sided: bool,
    pub layout: &'a [i32],
    pub v: [f32; 9],
    pub culled_dist: bool,
    pub culled_fog: bool,
}

fn cmp_f(a: &[f32], b: &[f32]) -> Ordering {
    for (x, y) in a.iter().zip(b) {
        if x < y {
            return Ordering::Less;
        }
        if y < x {
            return Ordering::Greater;
        }
    }
    Ordering::Equal
}

fn key_cmp(a: &Cand, b: &Cand) -> Ordering {
    a.cell
        .cmp(&b.cell)
        .then(a.mat.cmp(&b.mat))
        .then(a.shadow.cmp(&b.shadow))
        .then(a.one_sided.cmp(&b.one_sided))
        .then(a.layout.cmp(b.layout))
        .then_with(|| cmp_f(&a.v, &b.v))
        .then(a.culled_dist.cmp(&b.culled_dist))
        .then(a.culled_fog.cmp(&b.culled_fog))
}

#[derive(Debug, Default, PartialEq, Eq)]
pub struct CombinePlan {
    /// Candidate indices, grouped in `std::map` key order, input order within a group.
    pub order: Vec<u32>,
    pub group_sizes: Vec<u32>,
    /// Per mesh entity: every submesh was combined away.
    pub destroy: Vec<bool>,
    pub combined_subs: u32,
}

/// Groups of two or more candidates are merged; entities all of whose submeshes merged are destroyed.
pub fn combine_plan(cands: &[Cand], ent_sub_num: &[u32]) -> CombinePlan {
    let mut idx: Vec<u32> = (0..cands.len() as u32).collect();
    let at = |i: u32| cands.get(i as usize);
    idx.sort_by(|&a, &b| match (at(a), at(b)) {
        (Some(x), Some(y)) => key_cmp(x, y),
        _ => Ordering::Equal,
    });
    let mut plan = CombinePlan::default();
    let mut counts = vec![0u32; ent_sub_num.len()];
    let mut i = 0;
    while i < idx.len() {
        let mut j = i + 1;
        while let (Some(&p), Some(&q)) = (idx.get(j - 1), idx.get(j)) {
            match (at(p), at(q)) {
                (Some(x), Some(y)) if key_cmp(x, y) == Ordering::Equal => j += 1,
                _ => break,
            }
        }
        if j - i >= 2 {
            let group = idx.get(i..j).unwrap_or_default();
            plan.group_sizes.push(group.len() as u32);
            for &c in group {
                plan.order.push(c);
                if let Some(n) = at(c).and_then(|c| counts.get_mut(c.ent as usize)) {
                    *n += 1;
                    plan.combined_subs += 1;
                }
            }
        }
        i = j;
    }
    plan.destroy = counts
        .iter()
        .zip(ent_sub_num)
        .map(|(&c, &n)| c > 0 && c == n)
        .collect();
    plan
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[repr(u32)]
pub enum FinishStep {
    FlushStaticBatches,
    BuildShapeBodies,
    CombineStaticMeshes,
    ObjectDone,
    CompileWorld,
}

/// Order of `cHpmWorldBuilder::Finish`; an object-done callback follows each flushed batch.
pub fn finish_steps(no_mesh_combine: bool) -> Vec<FinishStep> {
    let mut v = vec![FinishStep::FlushStaticBatches, FinishStep::BuildShapeBodies];
    if !no_mesh_combine {
        v.push(FinishStep::CombineStaticMeshes);
    }
    v.push(FinishStep::ObjectDone);
    v.push(FinishStep::CompileWorld);
    v
}
