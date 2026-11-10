//! C ABI for `light.rs`, `lensflare.rs`, `beam.rs` and `rope.rs`, declared in
//! `HPL2/core/include/scene/RustLight.h`.
//!
//! Pointers are non-null, valid and aligned for the call; outputs do not alias inputs. Float
//! buffers have the capacity stated by the accompanying count.

use super::beam;
use super::lensflare::{self, EntIn, FlareIn, Kind, ViewIn, VisState};
use super::light::{self, FlickerCfg, LightAnim, LightHost, LightLive};
use super::rope::{self, RopeOut, RopeParams};
use crate::types::{Color, Matrixf, Vector2f, Vector3f};
use std::ffi::c_void;

const _: () = {
    use std::mem::{offset_of, size_of};
    assert!(size_of::<LightAnim>() == 144 && offset_of!(LightAnim, flickering) == 140);
    assert!(size_of::<LightLive>() == 24 && size_of::<FlickerCfg>() == 56);
    assert!(size_of::<VisState>() == 16 && size_of::<RopeParams>() == 28);
    assert!(size_of::<FlareIn>() == 152 && offset_of!(FlareIn, brightness) == 144);
    assert!(offset_of!(FlareIn, color) == 4 && size_of::<EntIn>() == 24);
    assert!(size_of::<ViewIn>() == 144);
};

/// # Safety
/// `p` valid for `n` reads when `n > 0`.
unsafe fn slice<'a, T>(p: *const T, n: i32) -> &'a [T] {
    match usize::try_from(n) {
        // SAFETY: caller contract.
        Ok(n) if n > 0 && !p.is_null() => unsafe { std::slice::from_raw_parts(p, n) },
        _ => &[],
    }
}

/// # Safety
/// `p` valid for `n` writes when `n > 0`.
unsafe fn slice_mut<'a, T>(p: *mut T, n: i32) -> &'a mut [T] {
    match usize::try_from(n) {
        // SAFETY: caller contract.
        Ok(n) if n > 0 && !p.is_null() => unsafe { std::slice::from_raw_parts_mut(p, n) },
        _ => &mut [],
    }
}

/// Callback table into `iLight`; every entry is non-null and valid for the call.
#[repr(C)]
pub struct HostVt {
    ctx: *mut c_void,
    ease: unsafe extern "C" fn(*mut c_void, f32) -> f32,
    set_brightness: unsafe extern "C" fn(*mut c_void, f32),
    set_radius: unsafe extern "C" fn(*mut c_void, f32),
    set_diffuse: unsafe extern "C" fn(*mut c_void, *const Color),
    assign_diffuse: unsafe extern "C" fn(*mut c_void, *const Color),
    raw_radius: unsafe extern "C" fn(*mut c_void, f32),
    flicker_sound: unsafe extern "C" fn(*mut c_void, bool),
    flicker_event: unsafe extern "C" fn(*mut c_void, bool),
}

impl LightHost for HostVt {
    fn ease(&mut self, t: f32) -> f32 {
        // SAFETY: table contract.
        unsafe { (self.ease)(self.ctx, t) }
    }
    fn set_brightness(&mut self, v: f32) {
        // SAFETY: table contract.
        unsafe { (self.set_brightness)(self.ctx, v) }
    }
    fn set_radius(&mut self, v: f32) {
        // SAFETY: table contract.
        unsafe { (self.set_radius)(self.ctx, v) }
    }
    fn set_diffuse(&mut self, c: Color) {
        // SAFETY: table contract; `c` outlives the call.
        unsafe { (self.set_diffuse)(self.ctx, &c) }
    }
    fn assign_diffuse(&mut self, c: Color) {
        // SAFETY: table contract; `c` outlives the call.
        unsafe { (self.assign_diffuse)(self.ctx, &c) }
    }
    fn raw_radius(&mut self, v: f32) {
        // SAFETY: table contract.
        unsafe { (self.raw_radius)(self.ctx, v) }
    }
    fn flicker_sound(&mut self, on: bool) {
        // SAFETY: table contract.
        unsafe { (self.flicker_sound)(self.ctx, on) }
    }
    fn flicker_event(&mut self, on: bool) {
        // SAFETY: table contract.
        unsafe { (self.flicker_event)(self.ctx, on) }
    }
}

/// # Safety
/// All pointers valid; `host` callbacks must not touch `anim` or `live`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_light_update(
    anim: *mut LightAnim,
    live: *mut LightLive,
    has_on_sound: bool,
    has_off_sound: bool,
    host: *mut HostVt,
    dt: f32,
) {
    // SAFETY: caller contract.
    unsafe {
        light::update_light(
            &mut *anim,
            &mut *live,
            has_on_sound,
            has_off_sound,
            &mut *host,
            dt,
        )
    }
}

/// # Safety
/// All pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_light_fade_brightness_to(
    anim: *mut LightAnim,
    live: *const LightLive,
    dest: f32,
    time: f32,
) {
    // SAFETY: caller contract.
    unsafe { light::fade_brightness_to(&mut *anim, &*live, dest, time) }
}

/// # Safety
/// All pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_light_fade_to(
    anim: *mut LightAnim,
    live: *const LightLive,
    col: *const Color,
    radius: f32,
    time: f32,
) {
    // SAFETY: caller contract.
    unsafe { light::fade_to(&mut *anim, &*live, *col, radius, time) }
}

/// # Safety
/// All pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_light_set_flicker(
    anim: *mut LightAnim,
    live: *const LightLive,
    cfg: *const FlickerCfg,
) {
    // SAFETY: caller contract.
    unsafe { light::set_flicker(&mut *anim, &*live, &*cfg) }
}

/// # Safety
/// `out` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_light_spot_projection(
    fov: f32,
    aspect: f32,
    near: f32,
    shadow_near: f32,
    radius: f32,
    out: *mut Matrixf,
) {
    // SAFETY: caller contract.
    unsafe { *out = light::spot_projection(fov, aspect, near, shadow_near, radius) }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_light_spot_near(near: f32, shadow_near: f32) -> f32 {
    light::spot_near(near, shadow_near)
}

/// # Safety
/// Both outputs valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_light_spot_half_fov(fov: f32, tan: *mut f32, cos: *mut f32) {
    let (t, c) = light::spot_half_fov(fov);
    // SAFETY: caller contract.
    unsafe {
        *tan = t;
        *cos = c;
    }
}

/// # Safety
/// All pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_light_spot_view_proj(
    proj: *const Matrixf,
    view: *const Matrixf,
    out: *mut Matrixf,
) {
    // SAFETY: caller contract.
    unsafe { *out = light::spot_view_proj(&*proj, &*view) }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_light_box_fade_speed(time: f32) -> f32 {
    light::box_fade_speed(time)
}

/// Returns true once the fade has finished.
///
/// # Safety
/// `bands`, `from` and `target` point at 9 vectors each; `t` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_light_box_fade_step(
    bands: *mut [Vector3f; 9],
    from: *const [Vector3f; 9],
    target: *const [Vector3f; 9],
    t: *mut f32,
    speed: f32,
    dt: f32,
) -> bool {
    // SAFETY: caller contract.
    unsafe { light::box_fade_step(&mut *bands, &*from, &*target, &mut *t, speed, dt) }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_flare_fade_in_rate(fade: f32, time: f32) -> f32 {
    lensflare::fade_in_rate(fade, time)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_flare_fade_out_rate(fade: f32, time: f32) -> f32 {
    lensflare::fade_out_rate(fade, time)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_flare_update_fade(fade: f32, rate: f32, dt: f32) -> f32 {
    lensflare::update_fade(fade, rate, dt)
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_flare_outer_cos(angle: f32, inner_cos: f32) -> f32 {
    lensflare::outer_cos(angle, inner_cos)
}

/// # Safety
/// Both outputs valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_flare_set_inner_fov(
    angle: f32,
    outer_cos: f32,
    inner: *mut f32,
    outer: *mut f32,
) {
    let (i, o) = lensflare::set_inner_fov(angle, outer_cos);
    // SAFETY: caller contract.
    unsafe {
        *inner = i;
        *outer = o;
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_flare_clamp01(x: f32) -> f32 {
    lensflare::clamp01(x)
}

/// # Safety
/// `state` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_flare_update_visibility(state: *mut VisState, dt: f32, frame: i32) {
    // SAFETY: caller contract.
    unsafe { lensflare::update_visibility(&mut *state, dt, frame) }
}

/// # Safety
/// Both pointers valid, three elements each.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_flare_bv_radius(
    active: *const [bool; 3],
    size: *const [Vector2f; 3],
) -> f32 {
    // SAFETY: caller contract.
    unsafe { lensflare::bv_radius(&*active, &*size) }
}

/// # Safety
/// All pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_flare_color(
    f: *const FlareIn,
    e: *const EntIn,
    v: *const ViewIn,
    kind: i32,
    vis: f32,
    out: *mut Color,
) {
    let Some(kind) = Kind::from_i32(kind) else {
        return;
    };
    // SAFETY: caller contract.
    unsafe { *out = lensflare::color_and_brightness(&*f, &*e, &*v, kind, vis) }
}

/// # Safety
/// All pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_flare_glare(
    f: *const FlareIn,
    e: *const EntIn,
    v: *const ViewIn,
    scale: f32,
    out: *mut Color,
    screen: *mut Vector3f,
) {
    // SAFETY: caller contract.
    unsafe {
        let (c, s) = lensflare::glare(&*f, &*e, &*v, scale);
        *out = c;
        *screen = s;
    }
}

/// # Safety
/// All pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_flare_size_rotation(
    f: *const FlareIn,
    e: *const EntIn,
    v: *const ViewIn,
    kind: i32,
    vis: f32,
    out: *mut Matrixf,
) {
    let Some(kind) = Kind::from_i32(kind) else {
        return;
    };
    // SAFETY: caller contract.
    unsafe { *out = lensflare::size_and_rotation(&*f, &*e, &*v, kind, vis) }
}

/// # Safety
/// All pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_flare_billboard(
    f: *const FlareIn,
    e: *const EntIn,
    v: *const ViewIn,
    out: *mut Matrixf,
) {
    // SAFETY: caller contract.
    unsafe { *out = lensflare::billboard_matrix(&*f, &*e, &*v) }
}

/// # Safety
/// All pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_flare_type_color(
    f: *const FlareIn,
    e: *const EntIn,
    v: *const ViewIn,
    kind: i32,
    vis: f32,
    out: *mut Color,
) -> bool {
    let Some(kind) = Kind::from_i32(kind) else {
        return false;
    };
    // SAFETY: caller contract.
    match unsafe { lensflare::type_color(&*f, &*e, &*v, kind, vis) } {
        Some(c) => {
            // SAFETY: caller contract.
            unsafe { *out = c };
            true
        }
        None => false,
    }
}

/// # Safety
/// Both rectangle corners valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_flare_occlusion_target(
    vis: f32,
    total: f32,
    vmin: *const Vector3f,
    vmax: *const Vector3f,
) -> f32 {
    // SAFETY: caller contract.
    unsafe { lensflare::occlusion_target(vis, total, *vmin, *vmax) }
}

/// Returns false when the flare is out of range; otherwise writes the distance-scaled box.
///
/// # Safety
/// All pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_flare_collide_box(
    f: *const FlareIn,
    v: *const ViewIn,
    world_center: *const Vector3f,
    local_min: *const Vector3f,
    local_max: *const Vector3f,
    local_center: *const Vector3f,
    out_min: *mut Vector3f,
    out_max: *mut Vector3f,
) -> bool {
    // SAFETY: caller contract.
    unsafe {
        match lensflare::collide_box(
            &*f,
            &*v,
            *world_center,
            *local_min,
            *local_max,
            *local_center,
        ) {
            Some((a, b)) => {
                *out_min = a;
                *out_max = b;
                true
            }
            None => false,
        }
    }
}

/// Fills up to `quads` quads; returns how many were written.
///
/// # Safety
/// All pointers valid; `pos`, `col` hold `quads * 16` floats, `uv` `quads * 12`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_flare_multi_iris(
    f: *const FlareIn,
    e: *const EntIn,
    v: *const ViewIn,
    vis: f32,
    pos: *mut f32,
    col: *mut f32,
    uv: *mut f32,
    quads: i32,
) -> i32 {
    let q = quads.max(0);
    // SAFETY: caller contract.
    let n = unsafe {
        lensflare::multi_iris(
            &*f,
            &*e,
            &*v,
            vis,
            slice_mut(pos, q.saturating_mul(16)),
            slice_mut(col, q.saturating_mul(16)),
            slice_mut(uv, q.saturating_mul(12)),
        )
    };
    i32::try_from(n).unwrap_or(i32::MAX)
}

/// # Safety
/// `c` valid; `out` holds 8 floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_beam_set_edge_colors(
    c: *const Color,
    multiply_alpha: bool,
    out: *mut f32,
) {
    // SAFETY: caller contract.
    unsafe { beam::set_edge_colors(*c, multiply_alpha, slice_mut(out, 8)) }
}

/// # Safety
/// All pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_beam_bounds(
    start: *const Vector3f,
    end: *const Vector3f,
    width: f32,
    min: *mut Vector3f,
    max: *mut Vector3f,
) {
    // SAFETY: caller contract.
    unsafe {
        let (a, b) = beam::bounds(*start, *end, width);
        *min = a;
        *max = b;
    }
}

/// Writes xyz of 4 positions (stride 4) and xy of 4 uvs (stride 3), as `UpdateGraphicsForFrame`.
///
/// # Safety
/// All pointers valid; `pos` holds 16 floats, `uv` 12.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_beam_geometry(
    start: *const Vector3f,
    end: *const Vector3f,
    size_x: f32,
    size_y: f32,
    tile_height: bool,
    pos: *mut f32,
    uv: *mut f32,
    axis: *mut Vector3f,
    mid: *mut Vector3f,
) {
    // SAFETY: caller contract.
    unsafe {
        let g = beam::geometry(*start, *end, [size_x, size_y], tile_height);
        for (dst, src) in slice_mut(pos, 16).chunks_exact_mut(4).zip(g.pos) {
            for (d, s) in dst.iter_mut().zip(src) {
                *d = s;
            }
        }
        for (dst, src) in slice_mut(uv, 12).chunks_exact_mut(3).zip(g.uv) {
            for (d, s) in dst.iter_mut().zip(src) {
                *d = s;
            }
        }
        *axis = g.axis;
        *mid = g.mid;
    }
}

/// # Safety
/// All pointers valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_beam_model_matrix(
    world: *const Matrixf,
    world_pos: *const Vector3f,
    camera: *const Vector3f,
    axis: *const Vector3f,
    mid: *const Vector3f,
    out: *mut Matrixf,
) {
    // SAFETY: caller contract.
    unsafe { *out = beam::model_matrix(&*world, *world_pos, *camera, *axis, *mid) }
}

/// # Safety
/// `c` valid; `out` holds `vertices * 4` floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rope_set_colors(
    c: *const Color,
    multiply_alpha: bool,
    out: *mut f32,
    vertices: i32,
) {
    // SAFETY: caller contract.
    unsafe {
        rope::set_colors(
            *c,
            multiply_alpha,
            slice_mut(out, vertices.saturating_mul(4)),
        )
    }
}

/// # Safety
/// `points` holds `n` vectors; outputs valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rope_bounds(
    points: *const Vector3f,
    n: i32,
    radius: f32,
    min: *mut Vector3f,
    max: *mut Vector3f,
) {
    // SAFETY: caller contract.
    unsafe {
        let (a, b) = rope::bounds(slice(points, n), radius);
        *min = a;
        *max = b;
    }
}

/// Writes up to `max_quads` quads and returns how many.
///
/// # Safety
/// `points` holds `n` vectors; `pos` and `tangent` hold `max_quads * 16` floats, `uv` and
/// `normal` `max_quads * 12`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_rope_quads(
    points: *const Vector3f,
    n: i32,
    params: *const RopeParams,
    pos: *mut f32,
    uv: *mut f32,
    normal: *mut f32,
    tangent: *mut f32,
    max_quads: i32,
) -> i32 {
    let q = max_quads.max(0);
    // SAFETY: caller contract.
    let written = unsafe {
        rope::quads(
            slice(points, n),
            &*params,
            RopeOut {
                pos: slice_mut(pos, q.saturating_mul(16)),
                uv: slice_mut(uv, q.saturating_mul(12)),
                normal: slice_mut(normal, q.saturating_mul(12)),
                tangent: slice_mut(tangent, q.saturating_mul(16)),
            },
        )
    };
    i32::try_from(written).unwrap_or(i32::MAX)
}
