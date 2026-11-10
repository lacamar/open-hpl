//! `cSubMesh` one-sided check and bone-weight compile, `cMesh` bone bounding radii.

use crate::math::{cross3, dot3, normalize3, v3};
use crate::types::Vector3f;

/// Mirror of `hpl::cVertexBonePair`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct VertexBonePair {
    pub vtx_idx: u32,
    pub bone_idx: u32,
    pub weight: f32,
}

const _: () = assert!(std::mem::size_of::<VertexBonePair>() == 12);

const MAX_ONE_SIDED_INDICES: usize = 400 * 3;

/// Averaged normal and point when every triangle faces the same way.
///
/// The C++ reads beyond the buffers for an index count that is not a multiple of 3 or an index
/// past the vertices; those return `None` here. An empty index list yields NaN normal and point
/// (0/0), as in C++.
pub fn one_sided(indices: &[u32], positions: &[f32], stride: i32) -> Option<(Vector3f, Vector3f)> {
    if indices.len() > MAX_ONE_SIDED_INDICES {
        return None;
    }
    let vertex = |idx: u32| -> Option<Vector3f> {
        let off = usize::try_from((idx as i32).wrapping_mul(stride)).ok()?;
        let p = positions.get(off..off.checked_add(3)?)?;
        match *p {
            [x, y, z] => Some(v3(x, y, z)),
            _ => None,
        }
    };
    let mut first: Option<Vector3f> = None;
    let mut normal_sum = v3(0.0, 0.0, 0.0);
    let mut pos_sum = v3(0.0, 0.0, 0.0);
    let mut count = 0.0f32;
    for tri in indices.chunks(3) {
        let [i0, i1, i2] = *tri else { return None };
        let (p0, p1, p2) = (vertex(i0)?, vertex(i1)?, vertex(i2)?);
        let e1 = v3(p1.x - p0.x, p1.y - p0.y, p1.z - p0.z);
        let e2 = v3(p2.x - p0.x, p2.y - p0.y, p2.z - p0.z);
        let mut normal = cross3(e2, e1);
        normalize3(&mut normal);
        pos_sum = v3(pos_sum.x + p0.x, pos_sum.y + p0.y, pos_sum.z + p0.z);
        match first {
            None => {
                first = Some(normal);
                normal_sum = normal;
            }
            Some(f) => {
                if dot3(f, normal) < 0.9 {
                    return None;
                }
                normal_sum = v3(
                    normal_sum.x + normal.x,
                    normal_sum.y + normal.y,
                    normal_sum.z + normal.z,
                );
            }
        }
        count += 1.0;
    }
    let mut n = v3(
        normal_sum.x / count,
        normal_sum.y / count,
        normal_sum.z / count,
    );
    normalize3(&mut n);
    Some((
        n,
        v3(pos_sum.x / count, pos_sum.y / count, pos_sum.z / count),
    ))
}

pub const WARN_TOO_MANY_BONES: u32 = 1;
pub const WARN_UNCONNECTED: u32 = 2;

/// Fills the 4-slot weight/bone arrays of `vertex_count` vertices (`weights` zeroed by the caller)
/// and normalises the weights. Returns `WARN_*` flags.
pub fn compile_bone_pairs(
    pairs: &[VertexBonePair],
    vertex_count: usize,
    weights: &mut [f32],
    bones: &mut [u8],
) -> u32 {
    let mut flags = 0;
    for pair in pairs {
        if pair.vtx_idx as usize >= vertex_count {
            continue;
        }
        let base = pair.vtx_idx as usize * 4;
        let (Some(w), Some(b)) = (
            weights.get_mut(base..base + 4),
            bones.get_mut(base..base + 4),
        ) else {
            continue;
        };
        match w.iter().position(|&x| x == 0.0) {
            Some(slot) => {
                if let (Some(ws), Some(bs)) = (w.get_mut(slot), b.get_mut(slot)) {
                    *ws = pair.weight;
                    *bs = pair.bone_idx as u8;
                }
            }
            None => flags |= WARN_TOO_MANY_BONES,
        }
    }
    for w in weights.chunks_exact_mut(4).take(vertex_count) {
        if w.first().is_none_or(|&x| x == 0.0) {
            flags |= WARN_UNCONNECTED;
            continue;
        }
        let used = w.iter().take_while(|&&x| x != 0.0).count();
        let total = w.iter().take(used).fold(0.0f32, |t, &x| t + x);
        for x in w.iter_mut().take(used) {
            *x /= total;
        }
    }
    flags
}

/// Raises each bone's radius to the largest distance to a bound vertex. Pairs naming a missing
/// bone or vertex are skipped (out-of-bounds reads in C++).
pub fn bone_radii(
    bind_pos: &[Vector3f],
    radii: &mut [f32],
    pairs: &[VertexBonePair],
    positions: &[f32],
    stride: i32,
) {
    for pair in pairs {
        let off = pair.vtx_idx.wrapping_mul(stride as u32) as usize;
        let Some(&[x, y, z]) = positions.get(off..off + 3).and_then(|p| p.as_array()) else {
            continue;
        };
        let bone = pair.bone_idx as usize;
        let (Some(bind), Some(radius)) = (bind_pos.get(bone), radii.get_mut(bone)) else {
            continue;
        };
        let d = v3(bind.x - x, bind.y - y, bind.z - z);
        let dist_sqr = dot3(d, d);
        if dist_sqr > *radius * *radius {
            *radius = dist_sqr.sqrt();
        }
    }
}
