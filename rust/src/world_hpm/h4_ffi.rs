//! C ABI of the builder plans (`HPL2/core/include/resources/RustWorldH4.h`). Every `*const [f32; N]`
//! argument points to N readable floats.

use super::h4_plan::{self as plan, Cand, FinishStep};
use crate::types::{Matrixf, Vector3f};

#[repr(C)]
pub struct H4Xform {
    pub world: Matrixf,
    pub body: Matrixf,
}

#[repr(C)]
pub struct H4LightMask {
    pub center: [f32; 3],
    pub extent: [f32; 3],
}

#[repr(C)]
pub struct H4Exposure {
    pub inv_transform: Matrixf,
    pub half_size: [f32; 3],
}

#[repr(C)]
pub struct H4SubMesh {
    pub is_collider_mesh: bool,
    pub is_char_collider_mesh: bool,
    pub hide: bool,
}

#[repr(C)]
pub struct H4Cand {
    pub ent: u32,
    pub cell: [i32; 3],
    pub mat: u64,
    pub shadow: bool,
    pub one_sided: bool,
    pub culled_dist: bool,
    pub culled_fog: bool,
    pub layout_off: u32,
    pub layout_len: u32,
    pub v: [f32; 9],
}

#[repr(C)]
pub struct H4CombineOut {
    pub order: *mut u32,
    pub group_sizes: *mut u32,
    pub num_groups: u32,
    pub destroy: *mut bool,
    pub combined_subs: u32,
}

/// # Safety
/// `a` points to three readable floats.
unsafe fn v3(a: *const [f32; 3]) -> Vector3f {
    // SAFETY: caller contract.
    let [x, y, z] = unsafe { *a };
    Vector3f { x, y, z }
}

fn arr(v: Vector3f) -> [f32; 3] {
    [v.x, v.y, v.z]
}

/// # Safety
/// `p` points to `n` readable `T`, or `n` is 0.
unsafe fn slice<'a, T>(p: *const T, n: usize) -> &'a [T] {
    if p.is_null() || n == 0 {
        &[]
    } else {
        // SAFETY: caller contract.
        unsafe { std::slice::from_raw_parts(p, n) }
    }
}

/// # Safety
/// `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h4_static_xform(
    rot: *const [f32; 3],
    scale: *const [f32; 3],
    pos: *const [f32; 3],
    out: *mut H4Xform,
) {
    // SAFETY: caller contract.
    let x = unsafe { plan::static_xform(v3(rot), v3(scale), v3(pos)) };
    // SAFETY: caller contract.
    unsafe {
        *out = H4Xform {
            world: x.world,
            body: x.body,
        }
    };
}

/// # Safety
/// `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h4_rot_trans(
    rot: *const [f32; 3],
    pos: *const [f32; 3],
    out: *mut Matrixf,
) {
    // SAFETY: caller contract.
    unsafe { *out = plan::rot_trans(v3(rot), v3(pos)) };
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h4_is_combine_candidate(has_pose: bool, pose_applied: bool) -> bool {
    plan::is_combine_candidate(has_pose, pose_applied)
}

/// # Safety
/// `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h4_light_mask(
    rot: *const [f32; 3],
    size: *const [f32; 3],
    scale: *const [f32; 3],
    world_pos: *const [f32; 3],
    out: *mut H4LightMask,
) {
    // SAFETY: caller contract.
    let p = unsafe { plan::light_mask(v3(rot), v3(size), v3(scale), v3(world_pos)) };
    // SAFETY: caller contract.
    unsafe {
        *out = H4LightMask {
            center: arr(p.center),
            extent: arr(p.extent),
        }
    };
}

/// # Safety
/// `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h4_exposure_area(
    rot: *const [f32; 3],
    world_pos: *const [f32; 3],
    scale: *const [f32; 3],
    out: *mut H4Exposure,
) {
    // SAFETY: caller contract.
    let p = unsafe { plan::exposure_area(v3(rot), v3(world_pos), v3(scale)) };
    // SAFETY: caller contract.
    unsafe {
        *out = H4Exposure {
            inv_transform: p.inv_transform,
            half_size: arr(p.half_size),
        }
    };
}

/// `mins` and `maxs` hold `n` xyz triples, one per submesh.
///
/// # Safety
/// Both arrays hold `3 * n` floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h4_detail_mesh_radius(
    mins: *const f32,
    maxs: *const f32,
    n: usize,
) -> f32 {
    // SAFETY: caller contract.
    let (lo, hi) = unsafe { (slice(mins, n * 3), slice(maxs, n * 3)) };
    let bounds: Vec<_> = lo
        .chunks_exact(3)
        .zip(hi.chunks_exact(3))
        .filter_map(|(a, b)| match (a, b) {
            ([ax, ay, az], [bx, by, bz]) => Some((
                Vector3f {
                    x: *ax,
                    y: *ay,
                    z: *az,
                },
                Vector3f {
                    x: *bx,
                    y: *by,
                    z: *bz,
                },
            )),
            _ => None,
        })
        .collect();
    plan::detail_mesh_radius(&bounds)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h4_detail_scale(has_radius: bool, radius: f32, mesh_radius: f32) -> f32 {
    plan::detail_scale(has_radius.then_some(radius), mesh_radius)
}

/// # Safety
/// `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h4_detail_matrix(
    quat_wxyz: *const [f32; 4],
    pos: *const [f32; 3],
    scale: f32,
    out: *mut Matrixf,
) {
    // SAFETY: caller contract.
    unsafe { *out = plan::detail_matrix(*quat_wxyz, v3(pos), scale) };
}

/// `name` is NUL-terminated.
///
/// # Safety
/// `name` is a valid C string; `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h4_sub_mesh_plan(name: *const std::ffi::c_char, out: *mut H4SubMesh) {
    // SAFETY: caller contract.
    let n = unsafe { std::ffi::CStr::from_ptr(name) }.to_bytes();
    let p = plan::sub_mesh_plan(n);
    // SAFETY: caller contract.
    unsafe {
        *out = H4SubMesh {
            is_collider_mesh: p.is_collider_mesh,
            is_char_collider_mesh: p.is_char_collider_mesh,
            hide: p.hide,
        }
    };
}

/// # Safety
/// `name` is a valid C string.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h4_is_collider_name(name: *const std::ffi::c_char) -> bool {
    // SAFETY: caller contract.
    let n = unsafe { std::ffi::CStr::from_ptr(name) }.to_bytes();
    plan::has_collider_mesh([n])
}

/// # Safety
/// `collider_is_char` holds `n` bools.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h4_sub_mesh_batches(
    has_collider_mesh: bool,
    is_collider_mesh: bool,
    collider_is_char: *const bool,
    n: usize,
) -> bool {
    let p = plan::SubMeshPlan {
        is_collider_mesh,
        is_char_collider_mesh: false,
        hide: false,
    };
    // SAFETY: caller contract.
    let c = unsafe { slice(collider_is_char, n) };
    plan::sub_mesh_batches(has_collider_mesh, p, c.iter().copied())
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h4_batch_must_flush(batch_indices: usize, adding: usize) -> bool {
    plan::batch_must_flush(batch_indices, adding)
}

/// # Safety
/// `out` is writable (3 ints).
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h4_mesh_cell(center: *const [f32; 3], out: *mut [i32; 3]) {
    // SAFETY: caller contract.
    unsafe { *out = plan::mesh_cell(v3(center)) };
}

/// # Safety
/// `out` is writable (3 ints).
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h4_body_cell(pos: *const [f32; 3], out: *mut [i32; 3]) {
    // SAFETY: caller contract.
    unsafe { *out = plan::body_cell(v3(pos)) };
}

/// `layouts` is the pool the candidates index; `out.order` and `out.group_sizes` hold `n`
/// entries, `out.destroy` one per entity.
///
/// # Safety
/// Every array holds the stated count.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h4_combine_plan(
    cands: *const H4Cand,
    n: usize,
    layouts: *const i32,
    num_layout: usize,
    ent_sub_num: *const u32,
    num_ents: usize,
    out: *mut H4CombineOut,
) {
    // SAFETY: caller contract.
    let (raw, pool, subs, out) = unsafe {
        (
            slice(cands, n),
            slice(layouts, num_layout),
            slice(ent_sub_num, num_ents),
            &mut *out,
        )
    };
    let c: Vec<Cand> = raw
        .iter()
        .map(|r| {
            let off = r.layout_off as usize;
            Cand {
                ent: r.ent,
                cell: r.cell,
                mat: r.mat,
                shadow: r.shadow,
                one_sided: r.one_sided,
                layout: pool
                    .get(off..off + r.layout_len as usize)
                    .unwrap_or_default(),
                v: r.v,
                culled_dist: r.culled_dist,
                culled_fog: r.culled_fog,
            }
        })
        .collect();
    let p = plan::combine_plan(&c, subs);
    out.num_groups = p.group_sizes.len() as u32;
    out.combined_subs = p.combined_subs;
    // SAFETY: the output arrays hold `n` and `num_ents` entries.
    unsafe {
        std::ptr::copy_nonoverlapping(p.order.as_ptr(), out.order, p.order.len());
        std::ptr::copy_nonoverlapping(p.group_sizes.as_ptr(), out.group_sizes, p.group_sizes.len());
        std::ptr::copy_nonoverlapping(p.destroy.as_ptr(), out.destroy, p.destroy.len());
    }
}

/// Writes up to 8 steps (`FinishStep` values) and returns how many.
///
/// # Safety
/// `out` holds 8 entries.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h4_finish_steps(
    no_mesh_combine: bool,
    out: *mut FinishStep,
) -> usize {
    let s = plan::finish_steps(no_mesh_combine);
    // SAFETY: caller contract; the plan has at most 5 steps.
    unsafe { std::ptr::copy_nonoverlapping(s.as_ptr(), out, s.len()) };
    s.len()
}
