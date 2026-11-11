//! C ABI of the entity loader decisions, mirrored in `soma/src/game/RustSomaUi.h`.

use super::loaders::{self, ElementId, JointsStep};

const _: () = {
    use std::mem::size_of;
    assert!(size_of::<ElementId>() == 12);
};

/// Byte string view; the bytes are not necessarily UTF-8.
unsafe fn bytes<'a>(p: *const u8, n: usize) -> &'a [u8] {
    if n == 0 {
        &[]
    } else {
        // SAFETY: caller contract.
        unsafe { std::slice::from_raw_parts(p, n) }
    }
}

/// # Safety
/// `name` has `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_load_kind(name: *const u8, n: usize) -> i32 {
    // SAFETY: caller contract.
    loaders::kind_from_entity_type(unsafe { bytes(name, n) }) as i32
}

/// # Safety
/// `masses` has `n` floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_load_is_dynamic(
    spawned: bool,
    has_anim: bool,
    has_proc_anim: bool,
    masses: *const f32,
    n: usize,
) -> bool {
    let m: &[f32] = if n == 0 {
        &[]
    } else {
        // SAFETY: caller contract.
        unsafe { std::slice::from_raw_parts(masses, n) }
    };
    loaders::is_dynamic(spawned, has_anim, has_proc_anim, m)
}

/// # Safety
/// `uid` has `n` ints; `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_load_element_id(
    uid: *const i32,
    n: usize,
    id_attr: i32,
    out: *mut ElementId,
) {
    let u: &[i32] = if n == 0 {
        &[]
    } else {
        // SAFETY: caller contract.
        unsafe { std::slice::from_raw_parts(uid, n) }
    };
    // SAFETY: caller contract.
    unsafe { *out = loaders::element_id(u, id_attr) }
}

/// True when the rest pose is to be set from the map values.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs7_load_before_joints(has_pose: bool, has_skeleton: bool) -> bool {
    loaders::before_joints(has_pose, has_skeleton) == JointsStep::SetRestPose
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs7_load_align_bodies(rest_pose_set: bool) -> bool {
    loaders::align_bodies(rest_pose_set)
}

/// # Safety
/// `s` has 3 floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_load_mean_scale(s: *const f32) -> f32 {
    // SAFETY: caller contract.
    loaders::mean_scale(unsafe { s.cast::<[f32; 3]>().read() })
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs7_load_reparent_children(has_mesh: bool, bodies: usize) -> bool {
    loaders::reparent_children(has_mesh, bodies)
}

/// # Safety
/// Vectors have 3 floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_load_scaled_offset(
    local: *const f32,
    scale: *const f32,
    out: *mut f32,
) {
    // SAFETY: caller contract.
    unsafe {
        let r = loaders::scaled_offset(
            local.cast::<[f32; 3]>().read(),
            scale.cast::<[f32; 3]>().read(),
        );
        out.cast::<[f32; 3]>().write(r);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs7_load_random_anim_start(is_prop: bool, randomize: bool) -> bool {
    loaders::random_anim_start(is_prop, randomize)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs7_load_freeze_props(
    is_prop: bool,
    active: bool,
    disable_var: bool,
    skeleton_physics: bool,
) -> bool {
    loaders::freeze_props(is_prop, active, disable_var, skeleton_physics)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_gs7_load_no_gravity_underwater(is_prop: bool, var: bool) -> bool {
    loaders::no_gravity_underwater(is_prop, var)
}

/// Offset of the socket name inside a `Socket_` bone name, -1 for other bones.
///
/// # Safety
/// `name` has `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_load_socket_offset(name: *const u8, n: usize) -> isize {
    // SAFETY: caller contract.
    let b = unsafe { bytes(name, n) };
    loaders::socket_bone_name(b).map_or(-1, |t| (b.len() - t.len()) as isize)
}

/// # Safety
/// Colours have 4 floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_load_color_mul(col: *const f32, mul: *const f32, out: *mut f32) {
    // SAFETY: caller contract.
    unsafe {
        let r = loaders::color_mul(col.cast::<[f32; 4]>().read(), mul.cast::<[f32; 4]>().read());
        out.cast::<[f32; 4]>().write(r);
    }
}

/// # Safety
/// Colours have 4 floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_load_color_mul_all(
    col: *const f32,
    mul: *const f32,
    out: *mut f32,
) {
    // SAFETY: caller contract.
    unsafe {
        let r =
            loaders::color_mul_all(col.cast::<[f32; 4]>().read(), mul.cast::<[f32; 4]>().read());
        out.cast::<[f32; 4]>().write(r);
    }
}

/// # Safety
/// `name` has `n` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_gs7_load_default_show_mesh(name: *const u8, n: usize) -> bool {
    // SAFETY: caller contract.
    loaders::default_show_mesh(unsafe { bytes(name, n) })
}
