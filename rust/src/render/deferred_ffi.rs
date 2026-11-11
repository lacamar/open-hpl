//! C ABI for the deferred renderer port, declared in `graphics/RustRendererDeferred.h`.
//!
//! Plan handles are boxed; the shim frees each exactly once. Pointers are valid for the call, and a
//! null pointer makes the call a no-op returning the zero value.

use std::slice;

use super::deferred::{self as dr, DlOp, Feed, Geom, Op, Plan, Query, State, Step};
use super::deferred_post::{CopyIn, plan_copy_accum};
use crate::types::{Vector2f, Vector2l};

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct OpC {
    pub kind: u32,
    pub a: i32,
    pub b: i32,
    pub c: i32,
    pub d: i32,
    pub e: i32,
    pub f: [f32; 12],
}

fn pad<const N: usize>(f: [f32; N]) -> [f32; 12] {
    let mut out = [0.0; 12];
    for (o, v) in out.iter_mut().zip(f) {
        *o = v;
    }
    out
}

fn op(kind: u32, a: i32, b: i32, c: i32, d: i32, e: i32, f: [f32; 4]) -> OpC {
    OpC {
        kind,
        a,
        b,
        c,
        d,
        e,
        f: pad(f),
    }
}

const Z4: [f32; 4] = [0.0; 4];

fn geom_id(g: Geom) -> i32 {
    match g {
        Geom::SphereNearPlane => 0,
        Geom::FrustumNearPlane => 1,
        Geom::LineIntersection => 2,
        Geom::BoxNearPlane => 3,
        Geom::QueryFetch => 4,
        Geom::QuerySamples => 5,
        Geom::ShadowNeedsUpdate => 6,
    }
}

fn dl_op(o: DlOp) -> OpC {
    let k = 32;
    match o {
        DlOp::ClearPairs => op(k, 0, 0, 0, 0, 0, Z4),
        DlOp::ClearTempLights => op(k, 1, 0, 0, 0, 0, Z4),
        DlOp::SetMinLargeArea(v) => op(k, 2, v, 0, 0, 0, Z4),
        DlOp::Light(i) => op(k, 3, i, 0, 0, 0, Z4),
        DlOp::AcquireQuery(i) => op(k, 4, i, 0, 0, 0, Z4),
        DlOp::SetModelView(i) => op(k, 5, i, 0, 0, 0, Z4),
        DlOp::SetShape(v) => op(k, 6, v, 0, 0, 0, Z4),
        DlOp::QueryBegin(i) => op(k, 7, i, 0, 0, 0, Z4),
        DlOp::QueryEnd(i) => op(k, 8, i, 0, 0, 0, Z4),
        DlOp::DrawCurrent => op(k, 9, 0, 0, 0, 0, Z4),
        DlOp::Flush => op(k, 10, 0, 0, 0, 0, Z4),
        DlOp::InvView => op(k, 11, 0, 0, 0, 0, Z4),
        DlOp::UpdateFullscreenQuad { pos, size } => OpC {
            kind: k,
            a: 12,
            b: 0,
            c: 0,
            d: 0,
            e: 0,
            f: pad([pos[0], pos[1], pos[2], size[0], size[1]]),
        },
        DlOp::LightMatrix(i) => op(k, 13, i, 0, 0, 0, Z4),
        DlOp::QueryLog {
            idx,
            samples,
            visible,
        } => op(k, 14, idx, samples, i32::from(visible), 0, Z4),
        DlOp::ReleaseQuery(i) => op(k, 15, i, 0, 0, 0, Z4),
        DlOp::Lists => op(k, 16, 0, 0, 0, 0, Z4),
        DlOp::ClearShadowTexture => op(k, 17, 0, 0, 0, 0, Z4),
        DlOp::ShadowData => op(k, 18, 0, 0, 0, 0, Z4),
        DlOp::RenderShadowMap => op(k, 19, 0, 0, 0, 0, Z4),
    }
}

fn state_id(s: State) -> i32 {
    match s {
        State::DepthTest => 0,
        State::DepthWrite => 1,
        State::DepthFunc => 2,
        State::Blend => 3,
        State::Alpha => 4,
        State::Channel => 5,
    }
}

fn query_id(q: Query) -> i32 {
    match q {
        Query::Headroom => 0,
        Query::FogAfterAreas => 1,
        Query::FogIsSkybox => 2,
        Query::DepthOfField => 3,
    }
}

fn step_id(s: Step) -> (i32, i32) {
    use Step::*;
    match s {
        SetupRenderVariables => (0, 0),
        SetupGBuffer => (1, 0),
        CheckVisibleZ => (2, 0),
        CheckVisibleStatic => (3, 0),
        CheckVisibleDynamic => (4, 0),
        FetchDelayedOcclusion => (5, 0),
        AssignOcclusionQueries => (6, 0),
        SetupLights => (7, 0),
        CompileOcclusion => (8, 0),
        CompileBruteForce => (9, 0),
        RenderZ => (10, 0),
        RenderDelayedOcclusionQueries => (11, 0),
        RenderGbuffer => (12, 0),
        RenderGbufferContent => (13, 0),
        RenderDecals => (14, 0),
        CallbackPostGBuffer => (15, 0),
        RenderLights => (16, 0),
        RenderIllumination => (17, 0),
        RenderFog => (18, 0),
        RenderFullScreenFog => (19, 0),
        RenderEdgeSmooth => (20, 0),
        RenderBasicSkyBox => (21, 0),
        CallbackPostSolid => (22, 0),
        UpdateLensFlares => (23, 0),
        RenderEnvironmentParticles(b) => (24, i32::from(b)),
        RenderTranslucent(n) => (25, n),
        RenderDepthOfField => (26, 0),
        RenderMultiIrisAndGlare => (27, 0),
        CallbackPostTranslucent => (28, 0),
        RetrieveLightOcclusion => (29, 0),
        CopyAccumToPostEffects => (30, 0),
    }
}

impl From<Op> for OpC {
    fn from(o: Op) -> Self {
        match o {
            Op::State { which, val } => op(0, state_id(which), val, 0, 0, 0, Z4),
            Op::FlatProjection => op(1, 0, 0, 0, 0, 0, Z4),
            Op::SetFb { fb, use_pos } => op(2, fb, i32::from(use_pos), 0, 0, 0, Z4),
            Op::TexRangeNull { first } => op(3, first, 0, 0, 0, 0, Z4),
            Op::Program { prog } => op(4, prog, 0, 0, 0, 0, Z4),
            Op::Tex { unit, tex } => op(5, unit, tex, 0, 0, 0, Z4),
            Op::AcquireTemp {
                fb,
                tex,
                size,
                fmt,
                idx,
            } => op(6, fb, tex, size.x, size.y, fmt | (idx << 8), Z4),
            Op::Float { prog, id, v } => op(7, prog, id, 0, 0, 0, [v, 0.0, 0.0, 0.0]),
            Op::Int { prog, id, v } => op(8, prog, id, v, 0, 0, Z4),
            Op::Vec2 { prog, id, v } => op(9, prog, id, 0, 0, 0, [v[0], v[1], 0.0, 0.0]),
            Op::Vec3 { prog, id, v } => op(10, prog, id, 0, 0, 0, [v[0], v[1], v[2], 0.0]),
            Op::Vec4 { prog, id, v } => op(11, prog, id, 0, 0, 0, v),
            Op::FloatArr { prog, id, arr, len } => op(12, prog, id, arr, len, 0, Z4),
            Op::Quad { min, max } => op(13, 0, 0, 0, 0, 0, [min[0], min[1], max[0], max[1]]),
            Op::SetGBuffer { comp } => op(14, comp, 0, 0, 0, 0, Z4),
            Op::ClearDepthValue { v } => op(15, 0, 0, 0, 0, 0, [v, 0.0, 0.0, 0.0]),
            Op::ClearColorValue { rgba } => op(16, 0, 0, 0, 0, 0, rgba),
            Op::ClearColorSettings => op(17, 0, 0, 0, 0, 0, Z4),
            Op::ClearFb { flags, use_pos } => op(18, flags, i32::from(use_pos), 0, 0, 0, Z4),
            Op::Ask(q) => op(19, query_id(q), 0, 0, 0, 0, Z4),
            Op::TryToneMap { combo } => op(20, combo, 0, 0, 0, 0, Z4),
            Op::Step(s) => {
                let (id, arg) = step_id(s);
                op(21, id, arg, 0, 0, 0, Z4)
            }
            Op::FlatProjectionMinMax { min, max } => OpC {
                kind: 22,
                f: pad([min[0], min[1], min[2], max[0], max[1], max[2]]),
                ..OpC::default()
            },
            Op::NormalFrustumProjection => op(23, 0, 0, 0, 0, 0, Z4),
            Op::QuadAt {
                pos,
                size,
                min,
                max,
            } => OpC {
                kind: 24,
                f: pad([
                    pos[0], pos[1], pos[2], size[0], size[1], min[0], min[1], max[0], max[1],
                ]),
                ..OpC::default()
            },
            Op::CopyFbToTex { tex, size } => op(25, tex, size.x, size.y, 0, 0, Z4),
            Op::SetAccumBuffer => op(26, 0, 0, 0, 0, 0, Z4),
            Op::Matrix { prog, id, arr } => op(27, prog, id, arr, 0, 0, Z4),
            Op::InvViewMatrix { prog, id } => op(28, prog, id, 0, 0, 0, Z4),
            Op::ScreenPositionVars { prog } => op(29, prog, 0, 0, 0, 0, Z4),
            Op::GenProgram { slot, mode, flags } => op(30, slot, mode, flags as i32, 0, 0, Z4),
            Op::AskGeom { q, idx, f } => op(31, geom_id(q), idx, 0, 0, 0, f),
            Op::Dl(d) => dl_op(d),
        }
    }
}

fn boxed(plan: Plan) -> *mut Plan {
    Box::into_raw(Box::new(plan))
}

#[repr(C)]
pub struct CopyInC {
    pub screen: Vector2l,
    pub screen_f: Vector2f,
    pub rt_pos: Vector2l,
    pub rt_size: Vector2l,
    pub rect_type: bool,
    pub use_fxaa: bool,
    pub fxaa_ok: bool,
    pub tonemap_ok: bool,
    pub trail_ok: bool,
    pub hpl35: bool,
    pub grading: bool,
    pub grading_target: bool,
    pub noise: bool,
    pub bloom: bool,
    pub bloom_programs_ok: bool,
    pub srgb: bool,
    pub trail_alpha: f32,
    pub skip_passes: i32,
    pub noise_width: i32,
    pub key: f32,
    pub exposure: f32,
    pub white_cut: f32,
    pub gamma: f32,
    pub bloom_bright: f32,
    pub bloom_width: f32,
    pub bloom_tint: [f32; 4],
    pub grading_blend: f32,
    pub film_intensity: f32,
    pub frame_time: f32,
    pub blur_samples: i32,
    pub layer_weights: *const f32,
    pub layer_count: usize,
}

/// # Safety
/// `i` is null or valid, with `layer_weights` pointing at `layer_count` floats; `grain` is null or valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dr_plan_copy_accum(
    i: *const CopyInC,
    grain: *const dr::FilmGrain,
) -> *mut Plan {
    // SAFETY: the caller's contract.
    let (Some(i), Some(grain)) = (unsafe { i.as_ref() }, unsafe { grain.as_ref() }) else {
        return std::ptr::null_mut();
    };
    let layers = if i.layer_weights.is_null() || i.layer_count == 0 {
        &[]
    } else {
        // SAFETY: the caller's contract.
        unsafe { slice::from_raw_parts(i.layer_weights, i.layer_count) }
    };
    boxed(plan_copy_accum(
        CopyIn {
            screen: i.screen,
            screen_f: i.screen_f,
            rt_pos: i.rt_pos,
            rt_size: i.rt_size,
            rect_type: i.rect_type,
            use_fxaa: i.use_fxaa,
            fxaa_ok: i.fxaa_ok,
            tonemap_ok: i.tonemap_ok,
            trail_ok: i.trail_ok,
            trail_alpha: i.trail_alpha,
            skip_passes: i.skip_passes,
            hpl35: i.hpl35,
            layer_weights: layers.to_vec(),
            grading: i.grading,
            grading_target: i.grading_target,
            noise: i.noise,
            noise_width: i.noise_width,
            bloom: i.bloom,
            bloom_programs_ok: i.bloom_programs_ok,
            srgb: i.srgb,
            key: i.key,
            exposure: i.exposure,
            white_cut: i.white_cut,
            gamma: i.gamma,
            bloom_bright: i.bloom_bright,
            bloom_width: i.bloom_width,
            bloom_tint: i.bloom_tint,
            grading_blend: i.grading_blend,
            film_intensity: i.film_intensity,
            frame_time: i.frame_time,
            blur_samples: i.blur_samples,
        },
        *grain,
    ))
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_dr_plan_setup_gbuffer(
    gbuffer_64bit: bool,
    linear_depth_idx: i32,
) -> *mut Plan {
    boxed(dr::plan_setup_gbuffer(gbuffer_64bit, linear_depth_idx))
}

/// 0 `ZBegin`, 1 `GbufferBegin`, 2 `GbufferEnd`.
#[unsafe(no_mangle)]
pub extern "C" fn ohpl_dr_plan_pass(kind: u32) -> *mut Plan {
    let p = match kind {
        0 => dr::Pass::ZBegin,
        1 => dr::Pass::GbufferBegin,
        _ => dr::Pass::GbufferEnd,
    };
    boxed(dr::plan_pass(p))
}

#[repr(C)]
pub struct ObjectsInC {
    pub use_occlusion_culling: bool,
    pub use_delayed_occlusion: bool,
    pub debug_render_buffers: bool,
    pub occlusion_test_large_lights: bool,
    pub send_to_post_effects: bool,
    pub tonemap_ok: bool,
    pub is_reflection: bool,
    pub skip_passes: i32,
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_dr_plan_objects(i: ObjectsInC) -> *mut Plan {
    boxed(dr::plan_objects(dr::ObjectsIn {
        use_occlusion_culling: i.use_occlusion_culling,
        use_delayed_occlusion: i.use_delayed_occlusion,
        debug_render_buffers: i.debug_render_buffers,
        skip_passes: i.skip_passes,
        occlusion_test_large_lights: i.occlusion_test_large_lights,
        send_to_post_effects: i.send_to_post_effects,
        tonemap_ok: i.tonemap_ok,
        is_reflection: i.is_reflection,
    }))
}

/// Writes the next op to `out`; false when the plan is exhausted.
///
/// # Safety
/// `plan` is null or a live handle from an `ohpl_dr_plan_*` call; `out` is null or valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dr_plan_next(plan: *mut Plan, out: *mut OpC) -> bool {
    // SAFETY: the caller's contract.
    let (Some(plan), Some(out)) = (unsafe { plan.as_mut() }, unsafe { out.as_mut() }) else {
        return false;
    };
    match plan.next_op() {
        Some(o) => {
            *out = o.into();
            true
        }
        None => false,
    }
}

/// Answers the last `Ask`/`TryToneMap` op with a flag.
///
/// # Safety
/// `plan` is null or a live handle.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dr_plan_feed_bool(plan: *mut Plan, v: bool) {
    // SAFETY: the caller's contract.
    if let Some(p) = unsafe { plan.as_mut() } {
        p.feed(Feed::Bool(v));
    }
}

/// Answers the last `Ask` op with an int.
///
/// # Safety
/// `plan` is null or a live handle.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dr_plan_feed_int(plan: *mut Plan, v: i32) {
    // SAFETY: the caller's contract.
    if let Some(p) = unsafe { plan.as_mut() } {
        p.feed(Feed::Int(v));
    }
}

/// Answers the last `Ask` op with a vector.
///
/// # Safety
/// `plan` is null or a live handle.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dr_plan_feed_vec3(plan: *mut Plan, x: f32, y: f32, z: f32) {
    // SAFETY: the caller's contract.
    if let Some(p) = unsafe { plan.as_mut() } {
        p.feed(Feed::Vec3([x, y, z]));
    }
}

/// Answers the last `Ask` op with a float.
///
/// # Safety
/// `plan` is null or a live handle.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dr_plan_feed_float(plan: *mut Plan, v: f32) {
    // SAFETY: the caller's contract.
    if let Some(p) = unsafe { plan.as_mut() } {
        p.feed(Feed::Float(v));
    }
}

/// Float array `idx` of a `FloatArr` op; valid until the plan is freed.
///
/// # Safety
/// `plan` is null or a live handle.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dr_plan_array(plan: *const Plan, idx: i32) -> *const f32 {
    // SAFETY: the caller's contract.
    unsafe { plan.as_ref() }.map_or(std::ptr::null(), |p| p.array(idx).as_ptr())
}

/// Film grain state after the plan ran.
///
/// # Safety
/// `plan` is null or a live handle; `out` is null or valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dr_plan_film_grain(plan: *const Plan, out: *mut dr::FilmGrain) {
    // SAFETY: the caller's contract.
    if let (Some(p), Some(out)) = (unsafe { plan.as_ref() }, unsafe { out.as_mut() }) {
        *out = p.res.film_grain;
    }
}

/// Whether the plan used up the image trail alpha.
///
/// # Safety
/// `plan` is null or a live handle.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dr_plan_trail_consumed(plan: *const Plan) -> bool {
    // SAFETY: the caller's contract.
    unsafe { plan.as_ref() }.is_some_and(|p| p.res.trail_consumed)
}

/// # Safety
/// `plan` is null or a live handle, freed exactly once.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dr_plan_free(plan: *mut Plan) {
    if !plan.is_null() {
        // SAFETY: the caller's contract; the pointer came from `Box::into_raw`.
        drop(unsafe { Box::from_raw(plan) });
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_dr_film_grain_init(out: &mut dr::FilmGrain) {
    *out = dr::FilmGrain::new();
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_dr_cdiv(a: i32, b: i32) -> i32 {
    dr::cdiv(a, b)
}

#[repr(C)]
pub struct FarPlaneC {
    pub plane: f32,
    pub top: f32,
    pub bottom: f32,
    pub right: f32,
    pub left: f32,
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_dr_far_plane(fov: f32, aspect: f32, far: f32, out: &mut FarPlaneC) {
    let f = dr::far_plane(fov, aspect, far);
    *out = FarPlaneC {
        plane: f.plane,
        top: f.top,
        bottom: f.bottom,
        right: f.right,
        left: f.left,
    };
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_dr_accum_quad_uv(
    rt_pos: Vector2l,
    rt_size: Vector2l,
    screen: Vector2f,
    rect: bool,
    out_min: &mut Vector2f,
    out_max: &mut Vector2f,
) {
    let (min, max) = dr::accum_quad_uv(rt_pos, rt_size, screen, rect);
    *out_min = Vector2f {
        x: min[0],
        y: min[1],
    };
    *out_max = Vector2f {
        x: max[0],
        y: max[1],
    };
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_dr_screen_position_vars(
    far: &FarPlaneC,
    rt_size: Vector2l,
    rt_pos: Vector2l,
    gbuffer_size: Vector2l,
    out_to_far: &mut [f32; 4],
    out_inv_size: &mut [f32; 2],
) {
    let far = dr::FarPlane {
        plane: far.plane,
        top: far.top,
        bottom: far.bottom,
        right: far.right,
        left: far.left,
    };
    (*out_to_far, *out_inv_size) = dr::screen_position_vars(&far, rt_size, rt_pos, gbuffer_size);
}

#[repr(C)]
pub struct ShadowCfgC {
    pub high: Vector2l,
    pub medium: Vector2l,
    pub low: Vector2l,
    pub jitter_size: i32,
    pub jitter_samples: i32,
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_dr_shadow_cfg(
    resolution: i32,
    quality: i32,
    shader_model_4: bool,
    out: &mut ShadowCfgC,
) {
    let c = dr::shadow_cfg(resolution, quality, shader_model_4);
    *out = ShadowCfgC {
        high: c.high,
        medium: c.medium,
        low: c.low,
        jitter_size: c.jitter_size,
        jitter_samples: c.jitter_samples,
    };
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_dr_dof_rows(samples: i32) -> i32 {
    dr::dof_rows(samples)
}

/// Fills the `64 * rows * 4` float gauss table; false (nothing written) when `len` is not that size.
///
/// # Safety
/// `out` is null or points at `len` floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dr_dof_table(
    samples: i32,
    screen_x: f32,
    out: *mut f32,
    len: usize,
) -> bool {
    if out.is_null() {
        return false;
    }
    let table = dr::dof_table(samples, screen_x);
    if table.len() != len {
        return false;
    }
    // SAFETY: the caller's contract.
    unsafe { slice::from_raw_parts_mut(out, len) }.copy_from_slice(&table);
    true
}

#[repr(C)]
pub struct SsaoMipsC {
    pub mip_size: Vector2l,
    pub mips: i32,
    pub levels: i32,
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_dr_ssao_mips(size: Vector2l, out: &mut SsaoMipsC) {
    let m = dr::ssao_mips(size);
    *out = SsaoMipsC {
        mip_size: m.mip_size,
        mips: m.mips,
        levels: m.levels,
    };
}

/// Fills 400 xyz positions (1200 floats); false when `len` is not 1200.
///
/// # Safety
/// `out` is null or points at `len` floats.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dr_glare_positions(out: *mut f32, len: usize) -> bool {
    if out.is_null() || len != dr::GLARE_VERTS * 3 {
        return false;
    }
    let v = dr::glare_positions();
    // SAFETY: the caller's contract.
    unsafe { slice::from_raw_parts_mut(out, len) }.copy_from_slice(&v);
    true
}

/// Fills the 2166 glare triangle indices; false when `len` is not 2166.
///
/// # Safety
/// `out` is null or points at `len` ints.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dr_glare_indices(out: *mut u32, len: usize) -> bool {
    if out.is_null() || len != dr::GLARE_INDICES {
        return false;
    }
    let v = dr::glare_indices();
    // SAFETY: the caller's contract.
    unsafe { slice::from_raw_parts_mut(out, len) }.copy_from_slice(&v);
    true
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_dr_batch_sizes(
    vertices: i32,
    indices: i32,
    lights: i32,
    out_vertices: &mut i32,
    out_indices: &mut i32,
) {
    (*out_vertices, *out_indices) = dr::batch_sizes(vertices, indices, lights);
}

#[repr(C)]
pub struct DebugTexC {
    pub kind: i32,
    pub a: i32,
    pub b: i32,
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_dr_debug_gbuffer_texture(
    idx: i32,
    shadow_counts: &[i32; 3],
    out: &mut DebugTexC,
) {
    let t = dr::debug_gbuffer_texture(idx, *shadow_counts);
    *out = DebugTexC {
        kind: t.kind,
        a: t.a,
        b: t.b,
    };
}

/// Writes the tone map define ids to `out` (room for `cap`); returns how many there are.
///
/// # Safety
/// `out` is null or points at `cap` ints.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_dr_tonemap_defines(
    combo: i32,
    hpl35: bool,
    out: *mut i32,
    cap: usize,
) -> usize {
    let v = dr::tonemap_defines(combo, hpl35);
    if !out.is_null() {
        // SAFETY: the caller's contract.
        let out = unsafe { slice::from_raw_parts_mut(out, cap) };
        for (dst, src) in out.iter_mut().zip(&v) {
            *dst = *src;
        }
    }
    v.len()
}
