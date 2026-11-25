//! C ABI for `graphics/`, mirrored in `HPL2/core/include/graphics/RustMesh.h`.

use super::meshgen::{self, MeshData, PlaneError};
use crate::types::{Vector2f, Vector3f};

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
    unsafe { crate::ffi::slice(p, n) }
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
