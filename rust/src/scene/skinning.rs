//! CPU skinning and bone-matrix kernels of `cSubMeshEntity` / `cMeshEntity`.

use crate::math::{fma, matrix_mul};
use crate::types::{Matrixf, Vector3f};

const MAX_INFLUENCES: usize = 4;

/// Bind-pose input and skinned output arrays of one sub mesh. Positions use `pos_stride`
/// floats per vertex, normals 3, tangents 4; the weight and bone tables hold 4 entries per vertex.
pub struct SkinJob<'a> {
    pub bind_pos: &'a [f32],
    pub bind_normal: &'a [f32],
    pub bind_tangent: &'a [f32],
    pub skin_pos: &'a mut [f32],
    pub skin_normal: &'a mut [f32],
    pub skin_tangent: &'a mut [f32],
    pub weights: &'a [f32],
    pub bones: &'a [u8],
    pub matrices: &'a [Matrixf],
    pub pos_stride: usize,
}

type Row = [f32; 4];

#[inline(always)]
fn transform(m: &[Row; 4], s: &[f32]) -> [f32; 3] {
    let [x, y, z, ..] = *s else { return [0.0; 3] };
    let row = |r: &Row| fma(r[2], z, fma(r[0], x, r[1] * y)) + r[3];
    [row(&m[0]), row(&m[1]), row(&m[2])]
}

#[inline(always)]
fn rotate(m: &[Row; 4], s: &[f32]) -> [f32; 3] {
    let [x, y, z, ..] = *s else { return [0.0; 3] };
    let row = |r: &Row| fma(r[2], z, fma(r[0], x, r[1] * y));
    [row(&m[0]), row(&m[1]), row(&m[2])]
}

#[inline(always)]
fn store_set(d: &mut [f32], v: [f32; 3], w: f32) {
    if let [a, b, c, ..] = d {
        (*a, *b, *c) = (v[0] * w, v[1] * w, v[2] * w);
    }
}

#[inline(always)]
fn store_add(d: &mut [f32], v: [f32; 3], w: f32) {
    if let [a, b, c, ..] = d {
        (*a, *b, *c) = (fma(v[0], w, *a), fma(v[1], w, *b), fma(v[2], w, *c));
    }
}

/// `cSubMeshEntity::UpdateGraphicsForFrame` vertex loop. Vertices whose first weight is 0 keep
/// their previous output; an influence naming a bone outside `matrices` is skipped (the C++
/// reads past the vector).
pub fn skin(job: SkinJob<'_>) {
    match job.pos_stride {
        3 => skin_n::<3>(job),
        4 => skin_n::<4>(job),
        _ => skin_n::<0>(job),
    }
}

/// `STRIDE == 0` selects the runtime `pos_stride`.
fn skin_n<const STRIDE: usize>(job: SkinJob<'_>) {
    let SkinJob {
        bind_pos,
        bind_normal,
        bind_tangent,
        skin_pos,
        skin_normal,
        skin_tangent,
        weights,
        bones,
        matrices,
        pos_stride,
    } = job;
    let stride = if STRIDE == 0 {
        pos_stride.max(1)
    } else {
        STRIDE
    };
    let (weights, _) = weights.as_chunks::<MAX_INFLUENCES>();
    let (bones, _) = bones.as_chunks::<MAX_INFLUENCES>();
    let verts = bind_pos
        .chunks_exact(stride)
        .zip(skin_pos.chunks_exact_mut(stride))
        .zip(
            bind_normal
                .chunks_exact(3)
                .zip(skin_normal.chunks_exact_mut(3)),
        )
        .zip(
            bind_tangent
                .chunks_exact(4)
                .zip(skin_tangent.chunks_exact_mut(4)),
        )
        .zip(weights.iter().zip(bones));
    for ((((bp, sp), (bn, sn)), (bt, st)), (w, b)) in verts {
        if w[0] == 0.0 {
            continue;
        }
        let mut first = true;
        for (&weight, &bone) in w.iter().zip(b) {
            if weight == 0.0 {
                break;
            }
            let Some(m) = matrices.get(usize::from(bone)) else {
                continue;
            };
            let m = &m.m;
            let (p, n, t) = (transform(m, bp), rotate(m, bn), rotate(m, bt));
            if first {
                store_set(sp, p, weight);
                store_set(sn, n, weight);
                store_set(st, t, weight);
                first = false;
            } else {
                store_add(sp, p, weight);
                store_add(sn, n, weight);
                store_add(st, t, weight);
            }
        }
    }
}

/// `cMeshEntity::UpdateGraphicsForFrame` bone loop: `out[i] = inv_world * world[i] * inv_bind[i]`.
/// Returns how many matrices changed bitwise.
pub fn bone_matrices(
    inv_world: &Matrixf,
    world: &[&Matrixf],
    inv_bind: &[&Matrixf],
    out: &mut [Matrixf],
) -> u32 {
    let bits = |m: &Matrixf| m.m.map(|r| r.map(f32::to_bits));
    let mut changed = 0;
    for ((w, ib), o) in world.iter().zip(inv_bind).zip(out) {
        let local = matrix_mul(inv_world, w);
        let bone = matrix_mul(&local, ib);
        if bits(&bone) != bits(o) {
            *o = bone;
            changed += 1;
        }
    }
    changed
}

/// `cMeshEntity::GetAABBFromBones`; bones without a positive radius are ignored.
pub fn aabb_from_bones(world: &[&Matrixf], radii: &[f32]) -> (Vector3f, Vector3f) {
    let pos = |m: &Matrixf| Vector3f {
        x: m.m[0][3],
        y: m.m[1][3],
        z: m.m[2][3],
    };
    let splat = |s: f32| Vector3f { x: s, y: s, z: s };
    let (mut min, mut max) = (splat(10000.0), splat(-10000.0));
    for (m, &r) in world.iter().zip(radii) {
        if r <= 0.0 {
            continue;
        }
        let p = pos(m);
        let hi = Vector3f {
            x: p.x + r,
            y: p.y + r,
            z: p.z + r,
        };
        let lo = Vector3f {
            x: p.x - r,
            y: p.y - r,
            z: p.z - r,
        };
        if max.x < hi.x {
            max.x = hi.x;
        }
        if max.y < hi.y {
            max.y = hi.y;
        }
        if max.z < hi.z {
            max.z = hi.z;
        }
        if min.x > lo.x {
            min.x = lo.x;
        }
        if min.y > lo.y {
            min.y = lo.y;
        }
        if min.z > lo.z {
            min.z = lo.z;
        }
    }
    if min.x > max.x
        && let Some(m) = world.first()
    {
        min = pos(m);
        max = min;
    }
    (min, max)
}
