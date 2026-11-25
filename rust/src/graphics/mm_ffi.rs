//! C ABI of `cSubMesh` and `cMesh`, declared in `graphics/RustMeshMm.h`.

use super::mm::{Collider, Host, Mesh, P, Sub, collide_shape};
use super::submesh::VertexBonePair;
use crate::types::{Matrixf, Vector3f};

fn bytes<'a>(p: *const u8, n: usize) -> &'a [u8] {
    unsafe { crate::ffi::slice(p, n) }
}

fn own<T>(v: T) -> *mut T {
    Box::into_raw(Box::new(v))
}

/// # Safety
/// `p` is null or from the matching `*_new`, freed once.
unsafe fn release<T>(p: *mut T) {
    if !p.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(p) });
    }
}

fn idx(i: i32) -> usize {
    usize::try_from(i).unwrap_or(usize::MAX)
}

fn count(n: usize) -> i32 {
    i32::try_from(n).unwrap_or(i32::MAX)
}

/// # Safety
/// `host` outlives the sub-mesh; `name` is readable for `len` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mm_sub_new(
    host: *const Host,
    manager: P,
    name: *const u8,
    len: usize,
) -> *mut Sub {
    // SAFETY: caller contract.
    match unsafe { host.as_ref() } {
        Some(h) => own(Sub::new(h, manager, bytes(name, len))),
        None => core::ptr::null_mut(),
    }
}

/// # Safety
/// `s` is null or from `ohpl_mm_sub_new`, freed once.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mm_sub_free(s: *mut Sub) {
    // SAFETY: caller contract.
    unsafe { release(s) }
}

/// # Safety
/// `s` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mm_sub_set_material(s: *mut Sub, m: P) {
    // SAFETY: caller contract.
    if let Some(s) = unsafe { s.as_mut() } {
        s.set_material(m);
    }
}

/// # Safety
/// `s` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mm_sub_compile(s: *mut Sub) {
    // SAFETY: caller contract.
    if let Some(s) = unsafe { s.as_mut() } {
        s.compile();
    }
}

/// # Safety
/// `s` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mm_sub_pair_num(s: *const Sub) -> i32 {
    // SAFETY: caller contract.
    unsafe { s.as_ref() }.map_or(0, |s| count(s.pairs().len()))
}

/// # Safety
/// `s` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mm_sub_pair(s: *mut Sub, i: i32) -> *mut VertexBonePair {
    // SAFETY: caller contract.
    unsafe { s.as_mut() }
        .and_then(|s| s.pair_mut(idx(i)))
        .map_or(core::ptr::null_mut(), |p| &raw mut *p)
}

/// # Safety
/// `s` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mm_sub_resize_pairs(s: *mut Sub, n: i32) {
    // SAFETY: caller contract.
    if let Some(s) = unsafe { s.as_mut() } {
        s.resize_pairs(idx(n));
    }
}

/// # Safety
/// `s` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mm_sub_add_pair(s: *mut Sub, vtx: u32, bone: u32, weight: f32) {
    // SAFETY: caller contract.
    if let Some(s) = unsafe { s.as_mut() } {
        s.add_pair(VertexBonePair {
            vtx_idx: vtx,
            bone_idx: bone,
            weight,
        });
    }
}

/// # Safety
/// `s` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mm_sub_clear_pairs(s: *mut Sub) {
    // SAFETY: caller contract.
    if let Some(s) = unsafe { s.as_mut() } {
        s.clear_pairs();
    }
}

/// # Safety
/// `s` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mm_sub_create_collider(s: *mut Sub, kind: i32) -> *mut Collider {
    // SAFETY: caller contract.
    unsafe { s.as_mut() }.map_or(core::ptr::null_mut(), |s| s.create_collider(kind))
}

/// # Safety
/// `s` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mm_sub_collider(s: *mut Sub, i: i32) -> *mut Collider {
    // SAFETY: caller contract.
    unsafe { s.as_mut() }.map_or(core::ptr::null_mut(), |s| s.collider(idx(i)))
}

/// # Safety
/// `s` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mm_sub_collider_num(s: *const Sub) -> i32 {
    // SAFETY: caller contract.
    unsafe { s.as_ref() }.map_or(0, |s| count(s.collider_count()))
}

/// # Safety
/// `s` is live; `world` is a live iPhysicsWorld.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mm_sub_create_collide_shape(s: *mut Sub, world: P) -> P {
    // SAFETY: caller contract.
    unsafe { s.as_mut() }.map_or(core::ptr::null_mut(), |s| s.create_collide_shape(world))
}

/// # Safety
/// `host`, `c`, `mul` are valid; `offset` is null or a live matrix.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mm_collide_shape(
    host: *const Host,
    c: *mut Collider,
    world: P,
    mul: *const Vector3f,
    offset: *mut Matrixf,
) -> P {
    // SAFETY: caller contract.
    match unsafe { (host.as_ref(), c.as_mut(), mul.as_ref()) } {
        (Some(h), Some(c), Some(m)) => collide_shape(h, c, world, *m, offset),
        _ => core::ptr::null_mut(),
    }
}

/// # Safety
/// `host` outlives the mesh.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mm_mesh_new(host: *const Host, owner: P, manager: P) -> *mut Mesh {
    // SAFETY: caller contract.
    match unsafe { host.as_ref() } {
        Some(h) => own(Mesh::new(h, owner, manager)),
        None => core::ptr::null_mut(),
    }
}

/// # Safety
/// `m` is null or from `ohpl_mm_mesh_new`, freed once.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mm_mesh_free(m: *mut Mesh) {
    // SAFETY: caller contract.
    unsafe { release(m) }
}

/// # Safety
/// `m` is live; `sub` is the box of `shell`; `name` is readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mm_mesh_add_sub(
    m: *mut Mesh,
    shell: P,
    sub: *mut Sub,
    name: *const u8,
    len: usize,
) {
    // SAFETY: caller contract.
    if let Some(m) = unsafe { m.as_mut() } {
        m.add_sub(shell, sub, bytes(name, len));
    }
}

/// # Safety
/// `m` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mm_mesh_sub(m: *const Mesh, i: u32) -> P {
    // SAFETY: caller contract.
    unsafe { m.as_ref() }.map_or(core::ptr::null_mut(), |m| {
        m.sub(usize::try_from(i).unwrap_or(usize::MAX))
    })
}

/// # Safety
/// `m` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mm_mesh_sub_num(m: *const Mesh) -> i32 {
    // SAFETY: caller contract.
    unsafe { m.as_ref() }.map_or(0, |m| count(m.sub_count()))
}

/// # Safety
/// `m` is live; `name` is readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mm_mesh_sub_index(
    m: *const Mesh,
    name: *const u8,
    len: usize,
) -> i32 {
    // SAFETY: caller contract.
    unsafe { m.as_ref() }.map_or(-1, |m| m.sub_index(bytes(name, len)))
}

/// # Safety
/// `m` is live; `name` is readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mm_mesh_sub_by_name(
    m: *const Mesh,
    name: *const u8,
    len: usize,
) -> P {
    // SAFETY: caller contract.
    unsafe { m.as_ref() }.map_or(core::ptr::null_mut(), |m| m.sub_by_name(bytes(name, len)))
}

/// # Safety
/// `m` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mm_mesh_triangle_count(m: *const Mesh) -> i32 {
    // SAFETY: caller contract.
    unsafe { m.as_ref() }.map_or(0, Mesh::triangle_count)
}

/// # Safety
/// `m` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mm_mesh_set_skeleton(m: *mut Mesh, s: P) {
    // SAFETY: caller contract.
    if let Some(m) = unsafe { m.as_mut() } {
        m.set_skeleton(s);
    }
}

/// # Safety
/// `m` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mm_mesh_skeleton(m: *const Mesh) -> P {
    // SAFETY: caller contract.
    unsafe { m.as_ref() }.map_or(core::ptr::null_mut(), Mesh::skeleton)
}

/// # Safety
/// `m` is live; `name` is readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mm_mesh_add_animation(
    m: *mut Mesh,
    a: P,
    name: *const u8,
    len: usize,
) {
    // SAFETY: caller contract.
    if let Some(m) = unsafe { m.as_mut() } {
        m.add_animation(a, bytes(name, len));
    }
}

/// # Safety
/// `m` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mm_mesh_animation(m: *const Mesh, i: i32) -> P {
    // SAFETY: caller contract.
    unsafe { m.as_ref() }.map_or(core::ptr::null_mut(), |m| m.animation(idx(i)))
}

/// # Safety
/// `m` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mm_mesh_animation_num(m: *const Mesh) -> i32 {
    // SAFETY: caller contract.
    unsafe { m.as_ref() }.map_or(0, |m| count(m.animation_count()))
}

/// # Safety
/// `m` is live; `name` is readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mm_mesh_animation_index(
    m: *const Mesh,
    name: *const u8,
    len: usize,
) -> i32 {
    // SAFETY: caller contract.
    unsafe { m.as_ref() }.map_or(-1, |m| m.animation_index(bytes(name, len)))
}

/// # Safety
/// `m` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mm_mesh_clear_animations(m: *mut Mesh, delete_all: bool) {
    // SAFETY: caller contract.
    if let Some(m) = unsafe { m.as_mut() } {
        m.clear_animations(delete_all);
    }
}

/// # Safety
/// `m` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mm_mesh_compile_bones(m: *mut Mesh) {
    // SAFETY: caller contract.
    if let Some(m) = unsafe { m.as_mut() } {
        m.compile_bones();
    }
}

/// # Safety
/// `m` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mm_mesh_radius(m: *const Mesh, i: i32) -> f32 {
    // SAFETY: caller contract.
    unsafe { m.as_ref() }.map_or(0.0, |m| m.radius(idx(i)))
}

/// # Safety
/// `m` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mm_mesh_root(m: *const Mesh) -> P {
    // SAFETY: caller contract.
    unsafe { m.as_ref() }.map_or(core::ptr::null_mut(), Mesh::root)
}

/// # Safety
/// `m` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mm_mesh_add_node(m: *mut Mesh, n: P) {
    // SAFETY: caller contract.
    if let Some(m) = unsafe { m.as_mut() } {
        m.add_node(n);
    }
}

/// # Safety
/// `m` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mm_mesh_node_num(m: *const Mesh) -> i32 {
    // SAFETY: caller contract.
    unsafe { m.as_ref() }.map_or(0, |m| count(m.node_count()))
}

/// # Safety
/// `m` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mm_mesh_node(m: *const Mesh, i: i32) -> P {
    // SAFETY: caller contract.
    unsafe { m.as_ref() }.map_or(core::ptr::null_mut(), |m| m.node(idx(i)))
}

/// # Safety
/// `m` is live; `name` is readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_mm_mesh_node_by_name(
    m: *const Mesh,
    name: *const u8,
    len: usize,
) -> P {
    // SAFETY: caller contract.
    unsafe { m.as_ref() }.map_or(core::ptr::null_mut(), |m| m.node_by_name(bytes(name, len)))
}

const _: () = assert!(core::mem::size_of::<Collider>() == 84);
