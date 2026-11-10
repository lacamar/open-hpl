//! C ABI for the `.msh` reader, mirrored in `HPL2/core/include/impl/RustMeshLoadMsh.h`.

use super::msh::{self, Anim, BonePair, Key, Mesh, MshError, Reason};
use std::ffi::c_char;
use std::slice;

#[repr(C)]
struct RStr {
    p: *const u8,
    n: usize,
}

const _: () = {
    use std::mem::size_of;
    assert!(size_of::<RNode>() == 88 && size_of::<RBone>() == 104 && size_of::<RCollider>() == 84);
    assert!(size_of::<RArray>() == 48 && size_of::<RTrack>() == 40 && size_of::<RAnim>() == 24);
    assert!(size_of::<RSub>() == 192 && size_of::<Key>() == 32 && size_of::<BonePair>() == 12);
};

fn rstr(s: &[u8]) -> RStr {
    RStr {
        p: s.as_ptr(),
        n: s.len(),
    }
}

#[repr(C)]
struct RNode {
    name: RStr,
    m: [f32; 16],
    flags: i32,
    parent: i32,
}

#[repr(C)]
struct RBone {
    name: RStr,
    sid: RStr,
    m: [f32; 16],
    unit_scale: f32,
    parent: i32,
}

#[repr(C)]
struct RCollider {
    m: [f32; 16],
    size: [f32; 3],
    kind: i32,
    char_collider: i32,
}

#[repr(C)]
struct RArray {
    kind: i32,
    format: i32,
    prog_var: i32,
    elem_num: i32,
    repeats: i32,
    count: usize,
    data: *const u8,
    bytes: usize,
}

#[repr(C)]
struct RSub {
    name: RStr,
    material: RStr,
    m: [f32; 16],
    scale: [f32; 3],
    collide_shape: i32,
    colliders: *const RCollider,
    n_colliders: usize,
    pairs: *const BonePair,
    n_pairs: usize,
    arrays: *const RArray,
    n_arrays: usize,
    skipped: *const i32,
    n_skipped: usize,
    indices: *const u32,
    n_indices: usize,
}

#[repr(C)]
struct RTrack {
    name: RStr,
    flags: i32,
    keys: *const Key,
    n_keys: usize,
}

#[repr(C)]
struct RAnim {
    length: f32,
    tracks: *const RTrack,
    n_tracks: usize,
}

#[expect(dead_code, reason = "owns the buffers the views point into")]
struct AnimViews {
    anim: Anim,
    tracks: Vec<RTrack>,
}

fn track_views(anim: &Anim) -> Vec<RTrack> {
    anim.tracks
        .iter()
        .map(|t| RTrack {
            name: rstr(&t.name),
            flags: i32::from(t.flags),
            keys: t.keys.as_ptr(),
            n_keys: t.keys.len(),
        })
        .collect()
}

fn anim_view(anim: &Anim, tracks: &[RTrack]) -> RAnim {
    RAnim {
        length: anim.length,
        tracks: tracks.as_ptr(),
        n_tracks: tracks.len(),
    }
}

#[expect(dead_code, reason = "owns the buffers the views point into")]
struct MeshOwner {
    mesh: Mesh,
    nodes: Vec<RNode>,
    bones: Vec<RBone>,
    subs: Vec<RSub>,
    colliders: Vec<Vec<RCollider>>,
    arrays: Vec<Vec<RArray>>,
    anims: Vec<RAnim>,
    tracks: Vec<Vec<RTrack>>,
}

/// The C++ declares only the leading fields; the owner is freed by `ohpl_msh_free`.
#[repr(C)]
pub struct OhplMshMesh {
    hpl3: i32,
    has_skeleton: i32,
    bones: *const RBone,
    n_bones: usize,
    nodes: *const RNode,
    n_nodes: usize,
    subs: *const RSub,
    n_subs: usize,
    anims: *const RAnim,
    n_anims: usize,
    _owner: Box<MeshOwner>,
}

#[repr(C)]
pub struct OhplMshAnim {
    view: RAnim,
    _owner: Box<AnimViews>,
}

fn build(mesh: Mesh) -> Box<OhplMshMesh> {
    let nodes: Vec<RNode> = mesh
        .nodes
        .iter()
        .map(|n| RNode {
            name: rstr(&n.name),
            m: n.matrix,
            flags: n.flags,
            parent: n.parent,
        })
        .collect();
    let bones: Vec<RBone> = mesh
        .bones
        .iter()
        .flatten()
        .map(|b| RBone {
            name: rstr(&b.name),
            sid: rstr(&b.sid),
            m: b.matrix,
            unit_scale: b.unit_scale,
            parent: b.parent,
        })
        .collect();
    let colliders: Vec<Vec<RCollider>> = mesh
        .submeshes
        .iter()
        .map(|s| {
            s.colliders
                .iter()
                .map(|c| RCollider {
                    m: c.matrix,
                    size: c.size,
                    kind: i32::from(c.kind),
                    char_collider: i32::from(c.char_collider),
                })
                .collect()
        })
        .collect();
    let arrays: Vec<Vec<RArray>> = mesh
        .submeshes
        .iter()
        .map(|s| {
            s.arrays
                .iter()
                .map(|a| RArray {
                    kind: a.kind,
                    format: a.format,
                    prog_var: a.prog_var,
                    elem_num: a.elem_num,
                    repeats: a.repeats,
                    count: a.count,
                    data: a.data.as_ptr(),
                    bytes: a.data.len(),
                })
                .collect()
        })
        .collect();
    let subs: Vec<RSub> = mesh
        .submeshes
        .iter()
        .zip(colliders.iter().zip(&arrays))
        .map(|(s, (c, a))| RSub {
            name: rstr(&s.name),
            material: rstr(&s.material),
            m: s.matrix,
            scale: s.scale,
            collide_shape: i32::from(s.collide_shape),
            colliders: c.as_ptr(),
            n_colliders: c.len(),
            pairs: s.bone_pairs.as_ptr(),
            n_pairs: s.bone_pairs.len(),
            arrays: a.as_ptr(),
            n_arrays: a.len(),
            skipped: s.skipped.as_ptr(),
            n_skipped: s.skipped.len(),
            indices: s.indices.as_ptr(),
            n_indices: s.indices.len(),
        })
        .collect();
    let tracks: Vec<Vec<RTrack>> = mesh.animations.iter().map(track_views).collect();
    let anims: Vec<RAnim> = mesh
        .animations
        .iter()
        .zip(&tracks)
        .map(|(a, t)| anim_view(a, t))
        .collect();
    Box::new(OhplMshMesh {
        hpl3: i32::from(mesh.hpl3),
        has_skeleton: i32::from(mesh.bones.is_some()),
        bones: bones.as_ptr(),
        n_bones: bones.len(),
        nodes: nodes.as_ptr(),
        n_nodes: nodes.len(),
        subs: subs.as_ptr(),
        n_subs: subs.len(),
        anims: anims.as_ptr(),
        n_anims: anims.len(),
        _owner: Box::new(MeshOwner {
            mesh,
            nodes,
            bones,
            subs,
            colliders,
            arrays,
            anims,
            tracks,
        }),
    })
}

fn code(e: MshError) -> i32 {
    match e {
        MshError::BadMagic => 1,
        MshError::BadVersion => 2,
        MshError::Unsupported(reason) => {
            let at = Reason::ALL.iter().position(|(r, _)| *r == reason);
            at.and_then(|i| i32::try_from(i).ok()).map_or(3, |i| 3 + i)
        }
    }
}

/// The text for an `ohpl_msh_parse` result of 3 or more; never null.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_msh_reason(code: i32) -> *const c_char {
    let at = usize::try_from(code.saturating_sub(3)).unwrap_or(0);
    Reason::ALL
        .get(at)
        .map_or(c"unsupported", |(_, t)| t)
        .as_ptr()
}

fn input<'a>(data: *const u8, len: usize) -> &'a [u8] {
    if data.is_null() || len == 0 {
        return &[];
    }
    // SAFETY: callers pass `len` readable bytes at `data`.
    unsafe { slice::from_raw_parts(data, len) }
}

/// Returns 0 and a mesh in `out`, 1 for a bad magic number, 2 for a bad version, 3 or more for
/// input the C++ loader would crash or throw on (see `ohpl_msh_reason`).
///
/// # Safety
/// `data` points to `len` readable bytes; `out` is writable. Free the mesh with `ohpl_msh_free`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_msh_parse(
    data: *const u8,
    len: usize,
    bone_unit_scale: bool,
    keep_hpl3_anims: bool,
    out: *mut *mut OhplMshMesh,
) -> i32 {
    match msh::parse_mesh(input(data, len), bone_unit_scale, keep_hpl3_anims) {
        Ok(mesh) => {
            // SAFETY: caller contract.
            unsafe { *out = Box::into_raw(build(mesh)) };
            0
        }
        Err(e) => code(e),
    }
}

/// # Safety
/// `mesh` came from `ohpl_msh_parse` and is freed once; null is ignored.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_msh_free(mesh: *mut OhplMshMesh) {
    if !mesh.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(mesh) });
    }
}

/// Same codes as `ohpl_msh_parse`; the animation is version 7 only.
///
/// # Safety
/// `data` points to `len` readable bytes; `out` is writable. Free with `ohpl_msh_anim_free`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_msh_parse_anim(
    data: *const u8,
    len: usize,
    out: *mut *mut OhplMshAnim,
) -> i32 {
    match msh::parse_animation(input(data, len)) {
        Ok(anim) => {
            let tracks = track_views(&anim);
            let view = anim_view(&anim, &tracks);
            let handle = Box::new(OhplMshAnim {
                view,
                _owner: Box::new(AnimViews { anim, tracks }),
            });
            // SAFETY: caller contract.
            unsafe { *out = Box::into_raw(handle) };
            0
        }
        Err(e) => code(e),
    }
}

/// # Safety
/// `anim` came from `ohpl_msh_parse_anim` and is freed once; null is ignored.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_msh_anim_free(anim: *mut OhplMshAnim) {
    if !anim.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(anim) });
    }
}
