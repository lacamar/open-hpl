//! C ABI of the H6 graphics state, declared in `graphics/RustGraphicsH6.h`.

use super::anim::KeyFrame;
use super::animdata::{AnimInfo, MeshInfo, Track};
use super::skel::Skeleton;
use crate::types::Matrixf;
use std::slice;

fn bytes<'a>(p: *const u8, n: usize) -> &'a [u8] {
    if p.is_null() || n == 0 {
        &[]
    } else {
        // SAFETY: callers pass `n` readable bytes at `p`.
        unsafe { slice::from_raw_parts(p, n) }
    }
}

fn own<T>(v: T) -> *mut T {
    Box::into_raw(Box::new(v))
}

/// # Safety
/// `p` is null or came from the matching `*_new` and is not used again.
unsafe fn release<T>(p: *mut T) {
    if !p.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(p) });
    }
}

fn mat_ptr(m: Option<&Matrixf>) -> *const Matrixf {
    m.map_or(std::ptr::null(), std::ptr::from_ref)
}

fn id(i: i32) -> usize {
    usize::try_from(i).unwrap_or(usize::MAX)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h6_skel_new() -> *mut Skeleton {
    own(Skeleton::new())
}

/// # Safety
/// `s` from `ohpl_h6_skel_new`, freed once.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h6_skel_free(s: *mut Skeleton) {
    // SAFETY: caller contract.
    unsafe { release(s) }
}

/// # Safety
/// `s` live; name/sid readable for their lengths.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h6_skel_create_child(
    s: *mut Skeleton,
    parent: i32,
    name: *const u8,
    name_len: usize,
    sid: *const u8,
    sid_len: usize,
) -> i32 {
    // SAFETY: caller contract.
    let Some(s) = (unsafe { s.as_mut() }) else {
        return -1;
    };
    s.create_child(id(parent), bytes(name, name_len), bytes(sid, sid_len))
        .map_or(-1, |i| i as i32)
}

/// # Safety
/// `s` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h6_skel_num(s: *const Skeleton) -> i32 {
    // SAFETY: caller contract.
    unsafe { s.as_ref() }.map_or(0, Skeleton::num)
}

/// Bone id at order position `idx`, -1 when out of range.
///
/// # Safety
/// `s` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h6_skel_by_index(s: *const Skeleton, idx: i32) -> i32 {
    // SAFETY: caller contract.
    unsafe { s.as_ref() }
        .and_then(|s| s.by_index(idx))
        .map_or(-1, |i| i as i32)
}

/// # Safety
/// `s` live; name readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h6_skel_index_by_name(
    s: *const Skeleton,
    name: *const u8,
    len: usize,
) -> i32 {
    // SAFETY: caller contract.
    unsafe { s.as_ref() }.map_or(-1, |s| s.index_by_name(bytes(name, len)))
}

/// # Safety
/// `s` live; sid readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h6_skel_index_by_sid(
    s: *const Skeleton,
    sid: *const u8,
    len: usize,
) -> i32 {
    // SAFETY: caller contract.
    unsafe { s.as_ref() }.map_or(-1, |s| s.index_by_sid(bytes(sid, len)))
}

/// # Safety
/// `s` live, `m` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h6_bone_set_transform(s: *mut Skeleton, b: i32, m: *const Matrixf) {
    // SAFETY: caller contract.
    if let (Some(s), Some(m)) = (unsafe { s.as_mut() }, unsafe { m.as_ref() }) {
        s.set_transform(id(b), m);
    }
}

/// # Safety
/// `s` live, `m` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h6_bone_set_unscaled(s: *mut Skeleton, b: i32, m: *const Matrixf) {
    // SAFETY: caller contract.
    if let (Some(s), Some(m)) = (unsafe { s.as_mut() }, unsafe { m.as_ref() }) {
        s.set_unscaled(id(b), m);
    }
}

/// # Safety
/// `s` live, `m` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h6_bone_set_inv_bind(s: *mut Skeleton, b: i32, m: *const Matrixf) {
    // SAFETY: caller contract.
    if let (Some(s), Some(m)) = (unsafe { s.as_mut() }, unsafe { m.as_ref() }) {
        s.set_inv_bind(id(b), m);
    }
}

/// # Safety
/// `s` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h6_bone_needs_update_unscaled(s: *mut Skeleton, b: i32) {
    // SAFETY: caller contract.
    if let Some(s) = unsafe { s.as_mut() } {
        s.needs_update_unscaled(id(b));
    }
}

/// # Safety
/// `s` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h6_bone_detach(s: *mut Skeleton, b: i32) {
    // SAFETY: caller contract.
    if let Some(s) = unsafe { s.as_mut() } {
        s.detach(id(b));
    }
}

macro_rules! bone_getter {
    ($name:ident, $method:ident) => {
        /// Matrix owned by the skeleton, valid until it is freed.
        ///
        /// # Safety
        /// `s` live.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name(s: *mut Skeleton, b: i32) -> *const Matrixf {
            // SAFETY: caller contract.
            mat_ptr(unsafe { s.as_mut() }.and_then(|s| s.$method(id(b))))
        }
    };
}

bone_getter!(ohpl_h6_bone_local, local);
bone_getter!(ohpl_h6_bone_local_unscaled, local_unscaled);
bone_getter!(ohpl_h6_bone_world, world);
bone_getter!(ohpl_h6_bone_inv_world, inv_world);
bone_getter!(ohpl_h6_bone_world_unscaled, world_unscaled);
bone_getter!(ohpl_h6_bone_inv_bind, inv_bind);

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h6_track_new() -> *mut Track {
    own(Track::new())
}

/// # Safety
/// `t` from `ohpl_h6_track_new`, freed once.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h6_track_free(t: *mut Track) {
    // SAFETY: caller contract.
    unsafe { release(t) }
}

/// # Safety
/// `t` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h6_track_reserve(t: *mut Track, n: i32) {
    // SAFETY: caller contract.
    if let Some(t) = unsafe { t.as_mut() } {
        t.reserve(usize::try_from(n).unwrap_or(0));
    }
}

/// # Safety
/// `t` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h6_track_create(t: *mut Track, time: f32) -> *mut KeyFrame {
    // SAFETY: caller contract.
    unsafe { t.as_mut() }.map_or(std::ptr::null_mut(), |t| t.create(time))
}

/// # Safety
/// `t` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h6_track_clear(t: *mut Track) {
    // SAFETY: caller contract.
    if let Some(t) = unsafe { t.as_mut() } {
        t.clear();
    }
}

/// # Safety
/// `t` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h6_track_num(t: *const Track) -> i32 {
    // SAFETY: caller contract.
    unsafe { t.as_ref() }.map_or(0, |t| t.len() as i32)
}

/// Null when `i` is out of range.
///
/// # Safety
/// `t` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h6_track_frame(t: *const Track, i: i32) -> *mut KeyFrame {
    // SAFETY: caller contract.
    unsafe { t.as_ref() }.map_or(std::ptr::null_mut(), |t| t.frame(id(i)))
}

/// # Safety
/// `t` live, `out` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h6_track_interpolate(
    t: *const Track,
    length: f32,
    time: f32,
    out: *mut KeyFrame,
) {
    // SAFETY: caller contract.
    if let (Some(t), Some(out)) = (unsafe { t.as_ref() }, unsafe { out.as_mut() }) {
        *out = t.interpolate(length, time);
    }
}

/// Weight; `idx_a`/`idx_b` receive the frame indices.
///
/// # Safety
/// `t` live, both outputs writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h6_track_frames_at_time(
    t: *const Track,
    length: f32,
    time: f32,
    idx_a: *mut usize,
    idx_b: *mut usize,
) -> f32 {
    // SAFETY: caller contract.
    let (Some(t), Some(a), Some(b)) = (unsafe { t.as_ref() }, unsafe { idx_a.as_mut() }, unsafe {
        idx_b.as_mut()
    }) else {
        return 0.0;
    };
    let (ia, ib, w) = t.frames_at_time(length, time);
    *a = ia;
    *b = ib;
    w
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h6_anim_new() -> *mut AnimInfo {
    own(AnimInfo::default())
}

/// # Safety
/// `a` from `ohpl_h6_anim_new`, freed once.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h6_anim_free(a: *mut AnimInfo) {
    // SAFETY: caller contract.
    unsafe { release(a) }
}

/// # Safety
/// `a` live; name readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h6_anim_add_track(a: *mut AnimInfo, name: *const u8, len: usize) {
    // SAFETY: caller contract.
    if let Some(a) = unsafe { a.as_mut() } {
        a.add_track(bytes(name, len));
    }
}

/// # Safety
/// `a` live; name readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h6_anim_track_by_name(
    a: *const AnimInfo,
    name: *const u8,
    len: usize,
) -> i32 {
    // SAFETY: caller contract.
    unsafe { a.as_ref() }.map_or(-1, |a| a.track_by_name(bytes(name, len)))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h6_mesh_new() -> *mut MeshInfo {
    own(MeshInfo::default())
}

/// # Safety
/// `m` from `ohpl_h6_mesh_new`, freed once.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h6_mesh_free(m: *mut MeshInfo) {
    // SAFETY: caller contract.
    unsafe { release(m) }
}

/// # Safety
/// `m` live; name readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h6_mesh_add_sub(m: *mut MeshInfo, name: *const u8, len: usize) {
    // SAFETY: caller contract.
    if let Some(m) = unsafe { m.as_mut() } {
        m.add_sub(bytes(name, len));
    }
}

/// # Safety
/// `m` live; name readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h6_mesh_sub_index(
    m: *const MeshInfo,
    name: *const u8,
    len: usize,
) -> i32 {
    // SAFETY: caller contract.
    unsafe { m.as_ref() }.map_or(-1, |m| m.sub_index(bytes(name, len)))
}

/// # Safety
/// `m` live; name readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h6_mesh_sub_by_name(
    m: *const MeshInfo,
    name: *const u8,
    len: usize,
) -> i32 {
    // SAFETY: caller contract.
    unsafe { m.as_ref() }.map_or(-1, |m| m.sub_by_name(bytes(name, len)))
}

/// # Safety
/// `m` live; name readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h6_mesh_add_animation(m: *mut MeshInfo, name: *const u8, len: usize) {
    // SAFETY: caller contract.
    if let Some(m) = unsafe { m.as_mut() } {
        m.add_animation(bytes(name, len));
    }
}

/// # Safety
/// `m` live; name readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h6_mesh_animation_index(
    m: *const MeshInfo,
    name: *const u8,
    len: usize,
) -> i32 {
    // SAFETY: caller contract.
    unsafe { m.as_ref() }.map_or(-1, |m| m.animation_index(bytes(name, len)))
}

/// # Safety
/// `m` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h6_mesh_clear_animations(m: *mut MeshInfo) {
    // SAFETY: caller contract.
    if let Some(m) = unsafe { m.as_mut() } {
        m.clear_animations();
    }
}
