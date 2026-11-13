//! H12 pass 2: per-frame decisions of `cBillboard`, `cBeam`, `cRopeEntity`, `cLensFlare` and
//! `cSoundEntity::PlaySound`. Vertex maths stay in `billboard.rs`, `beam.rs`, `rope.rs` and
//! `lensflare.rs`; this module owns the gates and the plans the C++ shells follow.

use crate::types::{Color, Vector3f};

/// `IsVisible` of `cBillboard`, `cBeam` and `cRopeEntity`.
pub fn is_visible(c: Color, visible: bool) -> bool {
    if c.r <= 0.0 && c.g <= 0.0 && c.b <= 0.0 {
        return false;
    }
    visible
}

/// `cBillboard::SetAxis` quick fix: the new bounding size for a non up-pointing axis.
pub fn bb_axis_size(is_axis_type: bool, axis: Vector3f, size_x: f32, size_y: f32) -> Option<f32> {
    if !is_axis_type || (axis.x == 0.0 && axis.y == 1.0 && axis.z == 0.0) {
        return None;
    }
    let mut m = size_x;
    if m < size_y {
        m = size_y;
    }
    Some(m * std::f32::consts::SQRT_2)
}

pub const HALO_SIZE: u32 = 1;
pub const HALO_TRANSFORM: u32 = 2;

/// Source-volume refresh of `cBillboard::RetrieveOcculsionQuery`.
pub fn bb_halo_gates(size_updated: &mut bool, bv_count: &mut i32, transform_count: i32) -> u32 {
    let mut r = 0;
    if *size_updated {
        *size_updated = false;
        r |= HALO_SIZE;
    }
    if *bv_count != transform_count {
        *bv_count = transform_count;
        r |= HALO_TRANSFORM;
    }
    r
}

/// Halo alpha once samples are visible: `clip` is the normalised clip rect of the source volume.
pub fn bb_halo_alpha(samples: i32, max_samples: i32, clip: Option<(Vector3f, Vector3f)>) -> f32 {
    let mut alpha = samples as f32 / max_samples as f32;
    let Some((mut vmin, mut vmax)) = clip else {
        return 0.0;
    };
    let total = (vmax.x - vmin.x) * (vmax.y - vmin.y);
    // Comparisons, not max/min: a NaN edge stays NaN as in C++.
    if vmin.x < -1.0 {
        vmin.x = -1.0;
    }
    if vmin.y < -1.0 {
        vmin.y = -1.0;
    }
    if vmax.x > 1.0 {
        vmax.x = 1.0;
    }
    if vmax.y > 1.0 {
        vmax.y = 1.0;
    }
    let inside = (vmax.x - vmin.x) * (vmax.y - vmin.y);
    if total > 0.0 {
        alpha *= inside / total;
    } else {
        alpha = 0.0;
    }
    alpha
}

/// `cBeam::UpdateGraphicsForFrame` gate. The C++ never stores the start/end counts, so they stay -1.
pub fn beam_needs_geometry(start_count: i32, end_count: i32, transform_count: i32) -> bool {
    !(start_count == transform_count && end_count == transform_count)
}

/// Points `cRopeEntity::UpdateGraphicsForViewport` collects from `available` particles.
pub fn rope_points(available: i32, max_segments: i32) -> i32 {
    available.clamp(0, max_segments.max(0))
}

/// `SetElementNum((count-1)*6)`, including the -6 of an empty rope.
pub fn rope_elements(count: i32) -> i32 {
    count.wrapping_sub(1).wrapping_mul(6)
}

pub const IRIS_NONE: u32 = 0;
pub const IRIS_REUSE: u32 = 1;
pub const IRIS_RECREATE: u32 = 2;
pub const IRIS_CREATE: u32 = 3;

/// `cLensFlare::PrepareMultiIrisVertexBuffer`; `existing_vertices` is -1 without a buffer.
pub fn iris_plan(active: bool, count: i32, existing_vertices: i32) -> u32 {
    if !active || count <= 0 {
        return IRIS_NONE;
    }
    if existing_vertices < 0 {
        IRIS_CREATE
    } else if existing_vertices != count.wrapping_mul(4) {
        IRIS_RECREATE
    } else {
        IRIS_REUSE
    }
}

/// Halo (1) wins, then the first active type, else Halo.
pub fn first_active_flare(active: &[bool; 3]) -> i32 {
    if active[1] {
        return 1;
    }
    active.iter().position(|a| *a).map_or(1, |i| i as i32)
}

pub fn any_flare_active(fade: f32, active: &[bool; 3]) -> bool {
    fade > 0.0 && active.iter().any(|a| *a)
}

/// `cSoundEntity::PlaySound` routing: bit 0 GUI entry type, bit 1 relative, bit 2 entity range.
pub fn snd_play_route(force_gui: bool, use_3d: bool) -> u32 {
    if force_gui {
        0b011
    } else if use_3d {
        0b100
    } else {
        0b010
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h12b_is_visible(c: &Color, visible: bool) -> bool {
    is_visible(*c, visible)
}

/// Writes `out` and returns true when the bounding size changes.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h12b_bb_axis_size(
    is_axis_type: bool,
    axis: &Vector3f,
    sx: f32,
    sy: f32,
    out: &mut f32,
) -> bool {
    bb_axis_size(is_axis_type, *axis, sx, sy).is_some_and(|v| {
        *out = v;
        true
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h12b_bb_halo_gates(
    size_updated: &mut bool,
    bv_count: &mut i32,
    transform_count: i32,
) -> u32 {
    bb_halo_gates(size_updated, bv_count, transform_count)
}

/// # Safety
/// `vmin` and `vmax` are valid unless `clip_ok` is false.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h12b_bb_halo_alpha(
    samples: i32,
    max_samples: i32,
    clip_ok: bool,
    vmin: *const Vector3f,
    vmax: *const Vector3f,
) -> f32 {
    // SAFETY: caller contract.
    let clip = clip_ok.then(|| unsafe { (*vmin, *vmax) });
    bb_halo_alpha(samples, max_samples, clip)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h12b_beam_needs_geometry(start: i32, end: i32, transform: i32) -> bool {
    beam_needs_geometry(start, end, transform)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h12b_rope_points(available: i32, max_segments: i32) -> i32 {
    rope_points(available, max_segments)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h12b_rope_elements(count: i32) -> i32 {
    rope_elements(count)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h12b_iris_plan(active: bool, count: i32, existing_vertices: i32) -> u32 {
    iris_plan(active, count, existing_vertices)
}

/// # Safety
/// `active` holds three bools.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h12b_flare_first_active(active: *const bool) -> i32 {
    // SAFETY: caller contract.
    first_active_flare(&unsafe { *active.cast::<[bool; 3]>() })
}

/// # Safety
/// `active` holds three bools.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_h12b_flare_any_active(fade: f32, active: *const bool) -> bool {
    // SAFETY: caller contract.
    any_flare_active(fade, &unsafe { *active.cast::<[bool; 3]>() })
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_h12b_snd_play_route(force_gui: bool, use_3d: bool) -> u32 {
    snd_play_route(force_gui, use_3d)
}
