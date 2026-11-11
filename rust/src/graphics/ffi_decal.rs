//! C ABI for `decal.rs`, mirrored in `HPL2/core/include/graphics/RustDecal.h`.

use super::decal::{self, ClipParams, Counts, DecalOut, MeshView, UvParams};
use crate::types::Vector3f;
use std::slice;

fn slice_of<'a, T>(p: *const T, n: usize) -> &'a [T] {
    if p.is_null() || n == 0 {
        &[]
    } else {
        // SAFETY: callers pass `n` readable elements at `p`.
        unsafe { slice::from_raw_parts(p, n) }
    }
}

#[repr(C)]
pub struct OhplDecalMesh {
    positions: *const f32,
    position_len: usize,
    pos_stride: u32,
    normals: *const f32,
    normal_len: usize,
    nrm_stride: u32,
    indices: *const u32,
    index_num: u32,
}

/// Array views the C++ side reads; the arrays are owned by the trailing `data`.
#[repr(C)]
pub struct OhplDecalOut {
    positions: *const Vector3f,
    normals: *const Vector3f,
    vertex_count: u32,
    indices: *const u32,
    index_count: u32,
    data: DecalOut,
}

/// # Safety
/// `params`, `mesh` and `counts` are valid; `mesh` arrays hold the stated lengths. Free the
/// result with `ohpl_decal_out_free`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_decal_clip_mesh(
    params: *const ClipParams,
    mesh: *const OhplDecalMesh,
    counts: *mut Counts,
) -> *mut OhplDecalOut {
    // SAFETY: caller contract.
    let (params, mesh, counts) = unsafe { (&*params, &*mesh, &mut *counts) };
    let view = MeshView {
        positions: slice_of(mesh.positions, mesh.position_len),
        pos_stride: mesh.pos_stride as usize,
        normals: slice_of(mesh.normals, mesh.normal_len),
        nrm_stride: mesh.nrm_stride as usize,
        indices: slice_of(mesh.indices, mesh.index_num as usize),
    };
    let mut data = DecalOut::default();
    decal::clip_mesh(params, &view, counts, &mut data);
    let mut out = Box::new(OhplDecalOut {
        positions: std::ptr::null(),
        normals: std::ptr::null(),
        vertex_count: data.positions.len() as u32,
        indices: std::ptr::null(),
        index_count: data.indices.len() as u32,
        data,
    });
    out.positions = out.data.positions.as_ptr();
    out.normals = out.data.normals.as_ptr();
    out.indices = out.data.indices.as_ptr();
    Box::into_raw(out)
}

/// # Safety
/// `out` came from `ohpl_decal_clip_mesh` and is freed once; null is ignored.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_decal_out_free(out: *mut OhplDecalOut) {
    if !out.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(out) });
    }
}

/// # Safety
/// `positions` holds `position_len` floats, `params` is valid and `out` has room for
/// `vertex_num` vectors.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_decal_uvs(
    positions: *const f32,
    position_len: usize,
    stride: u32,
    vertex_num: u32,
    params: *const UvParams,
    out: *mut Vector3f,
) {
    let mut uvs = Vec::with_capacity(vertex_num as usize);
    // SAFETY: caller contract.
    let params = unsafe { &*params };
    decal::uvs(
        slice_of(positions, position_len),
        stride as usize,
        vertex_num as usize,
        params,
        &mut uvs,
    );
    if !out.is_null() {
        // SAFETY: `out` has room for `vertex_num` vectors and `uvs` holds exactly that many.
        unsafe { std::ptr::copy_nonoverlapping(uvs.as_ptr(), out, uvs.len()) };
    }
}

const _: () = {
    use std::mem::size_of;
    assert!(size_of::<Counts>() == 8);
    assert!(size_of::<ClipParams>() == 232);
    assert!(size_of::<UvParams>() == 56);
};
