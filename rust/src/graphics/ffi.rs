//! C ABI for `graphics/`, mirrored in `HPL2/core/include/graphics/RustMesh.h`.

use super::anim::{self, Frames, KeyFrame};
use super::meshgen::{self, MeshData, PlaneError};
use super::submesh::{self, VertexBonePair};
use crate::types::{Vector2f, Vector3f};
use std::slice;

/// Array views the C++ side reads (it declares only these fields); the arrays are owned by the
/// trailing `data` and freed with `ohpl_meshgen_free`.
#[repr(C)]
pub struct OhplMeshData {
    positions: *const Vector3f,
    normals: *const Vector3f,
    texcoords: *const Vector3f,
    vertex_count: u32,
    indices: *const u32,
    index_count: u32,
    data: MeshData,
}

fn leak(data: MeshData) -> *mut OhplMeshData {
    let mut m = Box::new(OhplMeshData {
        positions: std::ptr::null(),
        normals: std::ptr::null(),
        texcoords: std::ptr::null(),
        vertex_count: data.positions.len() as u32,
        indices: std::ptr::null(),
        index_count: data.indices.len() as u32,
        data,
    });
    m.positions = m.data.positions.as_ptr();
    m.normals = m.data.normals.as_ptr();
    m.texcoords = m.data.texcoords.as_ptr();
    m.indices = m.data.indices.as_ptr();
    Box::into_raw(m)
}

fn slice_of<'a, T>(p: *const T, n: usize) -> &'a [T] {
    if p.is_null() || n == 0 {
        &[]
    } else {
        // SAFETY: callers pass `n` readable elements at `p`.
        unsafe { slice::from_raw_parts(p, n) }
    }
}

fn slice_mut<'a, T>(p: *mut T, n: usize) -> &'a mut [T] {
    if p.is_null() || n == 0 {
        &mut []
    } else {
        // SAFETY: callers pass `n` writable elements at `p`.
        unsafe { slice::from_raw_parts_mut(p, n) }
    }
}

/// # Safety
/// `size` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_meshgen_box(size: *const Vector3f) -> *mut OhplMeshData {
    // SAFETY: caller contract.
    leak(meshgen::box_mesh(unsafe { *size }))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_meshgen_sky_box(size: f32) -> *mut OhplMeshData {
    leak(meshgen::sky_box(size))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_meshgen_sphere(
    radius: f32,
    sections: i32,
    slices: i32,
) -> *mut OhplMeshData {
    leak(meshgen::sphere(radius, sections, slices))
}

/// # Safety
/// `size` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_meshgen_cylinder(
    size: *const Vector2f,
    sections: i32,
) -> *mut OhplMeshData {
    // SAFETY: caller contract.
    leak(meshgen::cylinder(unsafe { *size }, sections))
}

/// # Safety
/// `size` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_meshgen_capsule(
    size: *const Vector2f,
    sections: i32,
    slices: i32,
) -> *mut OhplMeshData {
    // SAFETY: caller contract.
    leak(meshgen::capsule(unsafe { *size }, sections, slices))
}

/// # Safety
/// `size` is valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_meshgen_cone(
    size: *const Vector2f,
    sections: i32,
) -> *mut OhplMeshData {
    // SAFETY: caller contract.
    leak(meshgen::cone(unsafe { *size }, sections))
}

/// Returns 0 and a mesh in `out`, 1 for coincident corners, 2 for non-coplanar corners.
///
/// # Safety
/// `c1`, `c2` valid; `uv` points to 4 `Vector2f`; `out` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_meshgen_plane(
    c1: *const Vector3f,
    c2: *const Vector3f,
    uv: *const Vector2f,
    out: *mut *mut OhplMeshData,
) -> i32 {
    // SAFETY: caller contract.
    let (c1, c2, uv) = unsafe { (*c1, *c2, slice_of(uv, 4)) };
    let mut corners = [Vector2f::default(); 4];
    for (d, s) in corners.iter_mut().zip(uv) {
        *d = *s;
    }
    match meshgen::plane(c1, c2, corners) {
        Ok(m) => {
            // SAFETY: caller contract.
            unsafe { *out = leak(m) };
            0
        }
        Err(PlaneError::Coincident) => 1,
        Err(PlaneError::NotCoplanar) => 2,
    }
}

/// # Safety
/// `indices` points to `count` readable entries.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_meshgen_wireframe(
    indices: *const u32,
    count: usize,
) -> *mut OhplMeshData {
    leak(MeshData {
        indices: meshgen::wireframe_indices(slice_of(indices, count)),
        ..MeshData::default()
    })
}

/// # Safety
/// `m` comes from an `ohpl_meshgen_*` call (or is null) and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_meshgen_free(m: *mut OhplMeshData) {
    if !m.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(m) });
    }
}

/// # Safety
/// `frames` points to `count` valid `KeyFrame` pointers; `out_a`/`out_b` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_anim_frames_at_time(
    frames: *const *const KeyFrame,
    count: usize,
    anim_length: f32,
    max_frame_time: f32,
    time: f32,
    out_a: *mut usize,
    out_b: *mut usize,
) -> f32 {
    // SAFETY: caller contract.
    let frames = unsafe { Frames::new(slice_of(frames, count)) };
    let (a, b, t) = anim::frames_at_time(frames, anim_length, max_frame_time, time);
    // SAFETY: caller contract.
    unsafe {
        *out_a = a;
        *out_b = b;
    }
    t
}

/// # Safety
/// `frames` points to `count` valid `KeyFrame` pointers; `out` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_anim_interpolate(
    frames: *const *const KeyFrame,
    count: usize,
    anim_length: f32,
    max_frame_time: f32,
    time: f32,
    out: *mut KeyFrame,
) {
    // SAFETY: caller contract.
    unsafe {
        let frames = Frames::new(slice_of(frames, count));
        *out = anim::interpolate(frames, anim_length, max_frame_time, time);
    }
}

/// # Safety
/// `frames` points to `count` valid `KeyFrame` pointers; `update_max` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_anim_insert_position(
    frames: *const *const KeyFrame,
    count: usize,
    max_frame_time: f32,
    time: f32,
    update_max: *mut bool,
) -> usize {
    // SAFETY: caller contract.
    unsafe {
        let frames = Frames::new(slice_of(frames, count));
        let (pos, update) = anim::insert_position(frames, max_frame_time, time);
        *update_max = update;
        pos
    }
}

/// # Safety
/// `indices` has `index_count` entries, `positions` `position_len` floats; `normal`/`point` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_submesh_one_sided(
    indices: *const u32,
    index_count: usize,
    positions: *const f32,
    position_len: usize,
    stride: i32,
    normal: *mut Vector3f,
    point: *mut Vector3f,
) -> bool {
    match submesh::one_sided(
        slice_of(indices, index_count),
        slice_of(positions, position_len),
        stride,
    ) {
        Some((n, p)) => {
            // SAFETY: caller contract.
            unsafe {
                *normal = n;
                *point = p;
            }
            true
        }
        None => false,
    }
}

/// Returns `WARN_*` flags: 1 more than 4 bones on a vertex, 2 unconnected vertices.
///
/// # Safety
/// `pairs` has `pair_count` entries; `weights` and `bones` have `vertex_count * 4` entries.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_submesh_compile_bone_pairs(
    pairs: *const VertexBonePair,
    pair_count: usize,
    vertex_count: usize,
    weights: *mut f32,
    bones: *mut u8,
) -> u32 {
    let n = vertex_count.saturating_mul(4);
    submesh::compile_bone_pairs(
        slice_of(pairs, pair_count),
        vertex_count,
        slice_mut(weights, n),
        slice_mut(bones, n),
    )
}

/// # Safety
/// `bind_pos` and `radii` have `bone_count` entries; `pairs` `pair_count`; `positions` `position_len`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mesh_bone_radii(
    bind_pos: *const Vector3f,
    radii: *mut f32,
    bone_count: usize,
    pairs: *const VertexBonePair,
    pair_count: usize,
    positions: *const f32,
    position_len: usize,
    stride: i32,
) {
    submesh::bone_radii(
        slice_of(bind_pos, bone_count),
        slice_mut(radii, bone_count),
        slice_of(pairs, pair_count),
        slice_of(positions, position_len),
        stride,
    );
}
