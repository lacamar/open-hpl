//! C ABI of the terrain and undergrowth bakers (`HPL2/core/include/resources/RustLdTerrain.h`).

use super::ld_terrain::{AreaIn, Group, GroupOut, LdPatchOut, Mat, Terrain, build_undergrowth};
use crate::types::Vector3f;

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
/// `data` points to `len` readable bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ld_terrain_new(
    data: *const u8,
    len: usize,
    size: i32,
    unit: f32,
    max_height: f32,
) -> *mut Terrain {
    // SAFETY: caller contract.
    let d = unsafe { slice(data, len) };
    Terrain::new(d, size, unit, max_height)
        .map_or(std::ptr::null_mut(), |t| Box::into_raw(Box::new(t)))
}

/// # Safety
/// `t` came from `ohpl_ld_terrain_new` and is live; `data` points to `len` readable bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ld_terrain_set_hole(
    t: *mut Terrain,
    data: *const u8,
    len: usize,
    channel: i32,
) -> bool {
    // SAFETY: caller contract.
    let (t, d) = unsafe { (t.as_mut(), slice(data, len)) };
    t.is_some_and(|t| t.set_hole(d.to_vec(), channel))
}

/// # Safety
/// `t` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ld_terrain_has_hole(t: *const Terrain) -> bool {
    // SAFETY: caller contract.
    unsafe { t.as_ref() }.is_some_and(Terrain::has_hole)
}

/// The arrays stay valid until the next patch call or `ohpl_ld_terrain_free`.
///
/// # Safety
/// `t` is live; `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ld_terrain_patch(
    t: *mut Terrain,
    x0: i32,
    z0: i32,
    w: i32,
    h: i32,
    tile: f32,
    out: *mut LdPatchOut,
) {
    // SAFETY: caller contract.
    if let (Some(t), Some(out)) = unsafe { (t.as_mut(), out.as_mut()) } {
        *out = t.build_patch(x0, z0, w, h, tile).out();
    }
}

/// # Safety
/// `t` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ld_terrain_sink_holes(t: *mut Terrain) {
    // SAFETY: caller contract.
    if let Some(t) = unsafe { t.as_mut() } {
        t.sink_holes();
    }
}

/// # Safety
/// `t` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ld_terrain_elev(t: *const Terrain) -> *const u16 {
    // SAFETY: caller contract.
    unsafe { t.as_ref() }.map_or(std::ptr::null(), |t| t.elev().as_ptr())
}

/// `size * size` heights, NaN where the map has no ground.
///
/// # Safety
/// `t` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ld_terrain_heights(t: *const Terrain) -> *const f32 {
    // SAFETY: caller contract.
    unsafe { t.as_ref() }.map_or(std::ptr::null(), |t| t.heights().as_ptr())
}

/// # Safety
/// `t` came from `ohpl_ld_terrain_new` and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ld_terrain_free(t: *mut Terrain) {
    if !t.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(t) });
    }
}

#[repr(C)]
pub struct LdUgMat {
    pos: *const f32,
    uv: *const f32,
    num_verts: u32,
    idx: *const u32,
    num_idx: u32,
    sub_div: [f32; 2],
    min_color: [f32; 4],
    max_color: [f32; 4],
    density: f32,
    align: f32,
    base_size: [f32; 3],
    min_size_mul: [f32; 3],
    max_size_mul: [f32; 3],
}

#[repr(C)]
pub struct LdUgArea {
    mat: i32,
    sub: bool,
    max_influence: f32,
    fade_border: f32,
    circle: bool,
    center: [f32; 2],
    radius: f32,
    points: *const f32,
    num_points: usize,
}

/// Returns null when no area survives.
///
/// # Safety
/// `t` is live; `mats` and `areas` point to the given counts of readable structs whose arrays are readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ld_ug_build(
    t: *const Terrain,
    grid: f32,
    num_mats: usize,
    mats: *const LdUgMat,
    num_areas: usize,
    areas: *const LdUgArea,
) -> *mut Vec<Group> {
    // SAFETY: caller contract.
    let (Some(t), m, a) = (
        unsafe { t.as_ref() },
        unsafe { slice(mats, num_mats) },
        unsafe { slice(areas, num_areas) },
    ) else {
        return std::ptr::null_mut();
    };
    let mats: Vec<Mat> = m
        .iter()
        .map(|m| {
            // SAFETY: caller contract.
            let (pos, uv, idx) = unsafe {
                (
                    slice(m.pos, m.num_verts as usize * 3),
                    slice(m.uv, m.num_verts as usize * 3),
                    slice(m.idx, m.num_idx as usize),
                )
            };
            let v3s = |s: &[f32]| -> Vec<Vector3f> {
                s.as_chunks::<3>()
                    .0
                    .iter()
                    .map(|c| Vector3f {
                        x: c.first().copied().unwrap_or_default(),
                        y: c.get(1).copied().unwrap_or_default(),
                        z: c.get(2).copied().unwrap_or_default(),
                    })
                    .collect()
            };
            Mat {
                pos: v3s(pos),
                uv: v3s(uv),
                idx: idx.to_vec(),
                sub_div: m.sub_div,
                min_color: m.min_color,
                max_color: m.max_color,
                density: m.density,
                align: m.align,
                base_size: m.base_size,
                min_size_mul: m.min_size_mul,
                max_size_mul: m.max_size_mul,
            }
        })
        .collect();
    let areas: Vec<AreaIn> = a
        .iter()
        // SAFETY: caller contract.
        .map(|a| AreaIn {
            mat: a.mat,
            sub: a.sub,
            max_influence: a.max_influence,
            fade_border: a.fade_border,
            circle: a.circle,
            center: a.center,
            radius: a.radius,
            points: unsafe { slice(a.points, a.num_points * 2) }.to_vec(),
        })
        .collect();
    build_undergrowth(t, grid, &mats, &areas)
        .map_or(std::ptr::null_mut(), |g| Box::into_raw(Box::new(g)))
}

/// # Safety
/// `g` came from `ohpl_ld_ug_build`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ld_ug_num(g: *const Vec<Group>) -> usize {
    // SAFETY: caller contract.
    unsafe { g.as_ref() }.map_or(0, Vec::len)
}

/// # Safety
/// `g` came from `ohpl_ld_ug_build`; `out` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ld_ug_group(
    g: *const Vec<Group>,
    i: usize,
    out: *mut GroupOut,
) -> bool {
    // SAFETY: caller contract.
    match unsafe { (g.as_ref().and_then(|g| g.get(i)), out.as_mut()) } {
        (Some(g), Some(out)) => {
            *out = g.out();
            true
        }
        _ => false,
    }
}

/// # Safety
/// `g` came from `ohpl_ld_ug_build` and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ld_ug_free(g: *mut Vec<Group>) {
    if !g.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(g) });
    }
}
