//! C ABI for `h11b_mesh.rs`, declared in `HPL2/core/include/scene/RustSceneH11.h`.
//!
//! Pointers are non-null, valid and aligned for the call; `HostCall` may re-enter the engine.

use super::h11::MeshCtl;
use super::h11b_mesh::{self as m, Host, HostCall, Names, Op};

/// # Safety
/// `vt` valid for the call.
unsafe fn host<'a>(vt: *const HostCall) -> Option<Host<'a>> {
    // SAFETY: caller contract.
    unsafe { vt.as_ref() }.map(Host)
}

macro_rules! entry {
    ($(fn $name:ident($($a:ident: $t:ty),*) $(-> $r:ty)? => $f:path;)*) => {$(
        /// # Safety
        /// `vt` valid, pointers valid for the call.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name(vt: *const HostCall $(, $a: $t)*) $(-> $r)? {
            // SAFETY: caller contract.
            match unsafe { host(vt) } {
                Some(h) => $f(&h $(, $a)*),
                None => Default::default(),
            }
        }
    )*};
}

entry! {
    fn ohpl_h11b_physics_from_bodies() => m::physics_from_bodies;
    fn ohpl_h11b_bone_nodes_update() => m::bone_nodes_update;
    fn ohpl_h11b_node_update() => m::node_update;
    fn ohpl_h11b_physics_active(c: *mut MeshCtl, active: bool) => m::physics_active;
    fn ohpl_h11b_physics_fade(c: *mut MeshCtl, time: f32) => m::physics_fade;
    fn ohpl_h11b_colliders_active(c: *mut MeshCtl, active: bool) => m::colliders_active;
    fn ohpl_h11b_align_bodies() => m::align_bodies;
    fn ohpl_h11b_check_colliders(args: usize, has_pos: bool, has_num: bool) -> bool => m::check_colliders;
    fn ohpl_h11b_bounding_volume() => m::bounding_volume;
    fn ohpl_h11b_animation_event(event: usize) => m::animation_event;
}

/// # Safety
/// `vt` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h11b_fan_out(
    vt: *const HostCall,
    op: i32,
    a: usize,
    b: usize,
    f: f32,
) {
    let op = match op {
        28 => Op::SubVisible,
        29 => Op::SubStatic,
        30 => Op::SubIllum,
        31 => Op::SubCoverage,
        32 => Op::SubLiquid,
        33 => Op::SubAutoLiquid,
        34 => Op::SubRenderFlag,
        35 => Op::SubColorMul,
        36 => Op::SubIllumColor,
        _ => return,
    };
    // SAFETY: caller contract.
    if let Some(h) = unsafe { host(vt) } {
        m::fan_out(&h, op, a, b, f);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h11b_names_new() -> *mut Names {
    Box::into_raw(Box::default())
}

/// # Safety
/// `n` from `ohpl_h11b_names_new` or null; not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h11b_names_free(n: *mut Names) {
    if !n.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(n) });
    }
}

/// # Safety
/// `p` valid for `len` bytes.
unsafe fn bytes<'a>(p: *const u8, len: usize) -> &'a [u8] {
    if p.is_null() {
        return &[];
    }
    // SAFETY: caller contract.
    unsafe { std::slice::from_raw_parts(p, len) }
}

/// # Safety
/// `n` valid; `p` valid for `len` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h11b_names_insert(
    n: *mut Names,
    kind: i32,
    p: *const u8,
    len: usize,
    idx: i32,
) {
    // SAFETY: caller contract.
    if let (Some(n), Ok(kind)) = (unsafe { n.as_mut() }, usize::try_from(kind)) {
        // SAFETY: caller contract.
        n.insert(kind, unsafe { bytes(p, len) }, idx);
    }
}

/// # Safety
/// `n` valid; `p` valid for `len` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h11b_names_find(
    n: *const Names,
    kind: i32,
    p: *const u8,
    len: usize,
) -> i32 {
    // SAFETY: caller contract.
    match (unsafe { n.as_ref() }, usize::try_from(kind)) {
        // SAFETY: caller contract.
        (Some(n), Ok(kind)) => n.find(kind, unsafe { bytes(p, len) }),
        _ => -1,
    }
}

/// # Safety
/// `vt`, `n` valid; `p` valid for `len` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h11b_play_name(
    vt: *const HostCall,
    n: *const Names,
    p: *const u8,
    len: usize,
    looping: bool,
    arg: f32,
    mode: i32,
    stop_prev: bool,
) {
    // SAFETY: caller contract.
    if let (Some(h), Some(n)) = (unsafe { host(vt) }, unsafe { n.as_ref() }) {
        // SAFETY: caller contract.
        m::play_name(
            &h,
            n,
            unsafe { bytes(p, len) },
            looping,
            arg,
            mode,
            stop_prev,
        );
    }
}

/// # Safety
/// `vt`, `n` valid; `p` valid for `len` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h11b_anim_is_over(
    vt: *const HostCall,
    n: *const Names,
    p: *const u8,
    len: usize,
) -> bool {
    // SAFETY: caller contract.
    match (unsafe { host(vt) }, unsafe { n.as_ref() }) {
        // SAFETY: caller contract.
        (Some(h), Some(n)) => m::anim_is_over(&h, n, unsafe { bytes(p, len) }),
        _ => false,
    }
}
