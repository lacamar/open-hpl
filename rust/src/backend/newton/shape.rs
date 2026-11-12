//! `cCollideShapeNewton`: collision creation, composition and (de)serialization.

use super::host::host;
use super::reg::{self};
use super::transposed;
use crate::sys::newton::{
    NewtonCollision, NewtonCollisionSerialize, NewtonCreateBox, NewtonCreateCapsule,
    NewtonCreateCollisionFromSerialization, NewtonCreateCompoundCollision, NewtonCreateCylinder,
    NewtonCreateHeightFieldCollision, NewtonCreateNull, NewtonCreateSceneCollision,
    NewtonCreateSphere, NewtonCreateTreeCollision, NewtonReleaseCollision,
    NewtonSceneCollisionCreateProxy, NewtonSceneCollisionOptimize, NewtonTreeCollisionAddFace,
    NewtonTreeCollisionBeginBuild, NewtonTreeCollisionEndBuild,
};
use crate::types::{Matrixf, Vector3f};
use std::ffi::{c_int, c_void};
use std::ptr;

const NULL: i32 = 0;
const BOX: i32 = 1;
const SPHERE: i32 = 2;
const CYLINDER: i32 = 3;
const CAPSULE: i32 = 4;

/// # Safety
/// `size` is a valid vector; `offset` is null or a valid matrix.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_nts_create(
    w: u64,
    kind: i32,
    size: *const Vector3f,
    offset: *const Matrixf,
) -> *mut NewtonCollision {
    // SAFETY: caller contract.
    let (Some((_, world)), Some(size)) = (reg::world(w), unsafe { size.as_ref() }) else {
        return ptr::null_mut();
    };
    // SAFETY: caller contract.
    let t = unsafe { offset.as_ref() }.map(transposed);
    let m = t.as_ref().map_or(ptr::null(), |t| t.as_ptr());
    let nw = world.nw;
    // SAFETY: `nw` is a live world; `m` is null or 16 floats.
    unsafe {
        match kind {
            NULL => NewtonCreateNull(nw),
            BOX => NewtonCreateBox(nw, size.x, size.y, size.z, 0, m),
            SPHERE => NewtonCreateSphere(nw, size.x, size.x, size.x, 0, m),
            CYLINDER => NewtonCreateCylinder(nw, size.x, size.y, 0, m),
            CAPSULE => NewtonCreateCapsule(nw, size.x, size.y, 0, m),
            _ => ptr::null_mut(),
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_nts_release(w: u64, coll: *mut NewtonCollision) {
    if let Some((_, world)) = reg::world(w) {
        // SAFETY: `coll` was created in this world and is released once.
        unsafe { NewtonReleaseCollision(world.nw, coll) };
    }
}

/// # Safety
/// `colls` points to `n` live collisions.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_nts_compound(
    w: u64,
    colls: *const *mut NewtonCollision,
    n: usize,
) -> *mut NewtonCollision {
    let Some((_, world)) = reg::world(w) else {
        return ptr::null_mut();
    };
    // SAFETY: caller contract.
    unsafe { NewtonCreateCompoundCollision(world.nw, c_int::try_from(n).unwrap_or(0), colls, 0) }
}

/// # Safety
/// `colls` points to `n` live collisions.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_nts_static_scene(
    w: u64,
    colls: *const *mut NewtonCollision,
    n: usize,
) -> *mut NewtonCollision {
    let Some((_, world)) = reg::world(w) else {
        return ptr::null_mut();
    };
    // SAFETY: plain Newton call.
    let scene = unsafe { NewtonCreateSceneCollision(world.nw, 0) };
    // SAFETY: caller contract.
    let list = unsafe { std::slice::from_raw_parts(colls, n) };
    let mut identity = transposed(&Matrixf {
        m: [
            [1.0, 0.0, 0.0, 0.0],
            [0.0, 1.0, 0.0, 0.0],
            [0.0, 0.0, 1.0, 0.0],
            [0.0, 0.0, 0.0, 1.0],
        ],
    });
    for &c in list {
        // SAFETY: `scene` and `c` are live collisions; the matrix is 16 floats.
        unsafe { NewtonSceneCollisionCreateProxy(scene, c, identity.as_mut_ptr()) };
    }
    // SAFETY: `scene` is live.
    unsafe { NewtonSceneCollisionOptimize(scene) };
    scene
}

/// Triangle soup with reversed winding, as `CreateFromVertices` builds it.
///
/// # Safety
/// `indices` has `index_num` entries; `vertices` has `vertex_num * stride` floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_nts_tree(
    w: u64,
    indices: *const u32,
    index_num: i32,
    vertices: *const f32,
    stride: i32,
    vertex_num: i32,
) -> *mut NewtonCollision {
    let Some((_, world)) = reg::world(w) else {
        return ptr::null_mut();
    };
    let (n, stride_n, vtx_n) = (
        usize::try_from(index_num).unwrap_or(0),
        usize::try_from(stride).unwrap_or(0),
        usize::try_from(vertex_num).unwrap_or(0),
    );
    // SAFETY: caller contract.
    let (idx, vtx) = unsafe {
        (
            std::slice::from_raw_parts(indices, n),
            std::slice::from_raw_parts(vertices, vtx_n * stride_n),
        )
    };
    // SAFETY: plain Newton calls on the collision created here.
    let tree = unsafe { NewtonCreateTreeCollision(world.nw, 0) };
    unsafe { NewtonTreeCollisionBeginBuild(tree) };
    let mut tri = [0.0f32; 9];
    for base in (0..n).step_by(3) {
        for corner in 0..3usize {
            let at = idx
                .get(base + 2 - corner)
                .map_or(0, |&i| i as usize * stride_n);
            for k in 0..3usize {
                if let Some(slot) = tri.get_mut(corner * 3 + k) {
                    *slot = vtx.get(at + k).copied().unwrap_or(0.0);
                }
            }
        }
        // SAFETY: `tree` is mid-build; `tri` holds three vertices.
        unsafe { NewtonTreeCollisionAddFace(tree, 3, tri.as_ptr(), 12, 1) };
    }
    // SAFETY: `tree` is mid-build.
    unsafe { NewtonTreeCollisionEndBuild(tree, 0) };
    tree
}

/// # Safety
/// `elevation` has `size * size` entries.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_nts_height_field(
    w: u64,
    size: i32,
    elevation: *const u16,
    unit: f32,
    vertical: f32,
) -> *mut NewtonCollision {
    let Some((_, world)) = reg::world(w) else {
        return ptr::null_mut();
    };
    let side = usize::try_from(size).unwrap_or(0);
    let attributes = vec![0i8; side * side];
    // SAFETY: caller contract; Newton copies the maps.
    unsafe {
        NewtonCreateHeightFieldCollision(
            world.nw,
            size,
            size,
            1,
            elevation,
            attributes.as_ptr(),
            unit,
            vertical,
            6,
        )
    }
}

unsafe extern "C" fn write_buffer(handle: *mut c_void, data: *const c_void, size: usize) {
    if let Some(host) = host() {
        // SAFETY: host callback; Newton passes `size` readable bytes.
        unsafe { (host.buf_add)(handle as usize, data, size >> 2) };
    }
}

unsafe extern "C" fn read_buffer(handle: *mut c_void, data: *mut c_void, size: usize) {
    if let Some(host) = host() {
        // SAFETY: host callback; Newton passes `size` writable bytes.
        unsafe { (host.buf_get)(handle as usize, data, size >> 2) };
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_nts_serialize(w: u64, coll: *const NewtonCollision, buf: usize) {
    if let Some((_, world)) = reg::world(w) {
        // SAFETY: the callback only forwards to the host buffer.
        unsafe { NewtonCollisionSerialize(world.nw, coll, Some(write_buffer), buf as *mut c_void) };
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_nts_deserialize(w: u64, buf: usize) -> *mut NewtonCollision {
    let Some((_, world)) = reg::world(w) else {
        return ptr::null_mut();
    };
    // SAFETY: the callback only forwards to the host buffer.
    unsafe {
        NewtonCreateCollisionFromSerialization(world.nw, Some(read_buffer), buf as *mut c_void)
    }
}
