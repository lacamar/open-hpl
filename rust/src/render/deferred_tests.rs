use super::deferred::*;
use super::deferred_post::*;
use crate::types::{Vector2f, Vector2l};

fn v2l(x: i32, y: i32) -> Vector2l {
    Vector2l { x, y }
}

fn ops(mut p: Plan) -> Vec<Op> {
    std::iter::from_fn(|| p.next_op()).collect()
}

/// Runs a plan, answering every query with `answer`.
fn run(mut p: Plan, answer: impl Fn(&Op) -> Feed) -> Vec<Op> {
    let mut v = Vec::new();
    while let Some(o) = p.next_op() {
        if matches!(o, Op::Ask(_) | Op::TryToneMap { .. }) {
            p.feed(answer(&o));
        }
        v.push(o);
    }
    v
}

fn steps(p: Plan, fog_after: bool, fog_skybox: bool, dof: bool) -> Vec<Step> {
    run(p, |o| match o {
        Op::Ask(Query::FogAfterAreas) => Feed::Bool(fog_after),
        Op::Ask(Query::FogIsSkybox) => Feed::Bool(fog_skybox),
        Op::Ask(Query::DepthOfField) => Feed::Bool(dof),
        _ => Feed::Bool(false),
    })
    .into_iter()
    .filter_map(|o| if let Op::Step(s) = o { Some(s) } else { None })
    .collect()
}

#[test]
fn cdiv_follows_aarch64_sdiv() {
    assert_eq!(cdiv(7, 0), 0);
    assert_eq!(cdiv(-7, 2), -3);
    assert_eq!(cdiv(i32::MIN, -1), i32::MIN);
    assert_eq!(div_size(v2l(1280, 721), 4), v2l(320, 180));
}

#[test]
fn objects_brute_force_order() {
    use Step::*;
    let p = plan_objects(ObjectsIn {
        debug_render_buffers: false,
        occlusion_test_large_lights: true,
        ..Default::default()
    });
    assert_eq!(
        steps(p, false, false, false),
        [
            SetupRenderVariables,
            SetupGBuffer,
            CheckVisibleStatic,
            CheckVisibleDynamic,
            CompileBruteForce,
            RenderZ,
            AssignOcclusionQueries,
            SetupLights,
            RenderGbuffer,
            RenderDecals,
            CallbackPostGBuffer,
            RenderLights,
            RenderIllumination,
            RenderFullScreenFog,
            RenderFog,
            RenderEdgeSmooth,
            RenderBasicSkyBox,
            CallbackPostSolid,
            UpdateLensFlares,
            RenderEnvironmentParticles(true),
            RenderDepthOfField,
            RenderTranslucent(0),
            RenderMultiIrisAndGlare,
            CallbackPostTranslucent,
            RetrieveLightOcclusion,
        ]
    );
}

#[test]
fn objects_occlusion_dof_and_post_copy() {
    use Step::*;
    let p = plan_objects(ObjectsIn {
        use_occlusion_culling: true,
        send_to_post_effects: true,
        tonemap_ok: true,
        skip_passes: 1 | 4,
        ..Default::default()
    });
    assert_eq!(
        steps(p, true, true, true),
        [
            SetupRenderVariables,
            SetupGBuffer,
            CheckVisibleZ,
            AssignOcclusionQueries,
            SetupLights,
            CompileOcclusion,
            RenderGbuffer,
            CallbackPostGBuffer,
            RenderLights,
            RenderIllumination,
            RenderFog,
            RenderFullScreenFog,
            RenderEdgeSmooth,
            CallbackPostSolid,
            UpdateLensFlares,
            RenderEnvironmentParticles(true),
            RenderTranslucent(1),
            RenderDepthOfField,
            RenderTranslucent(2),
            RenderEnvironmentParticles(false),
            RenderMultiIrisAndGlare,
            CallbackPostTranslucent,
            CopyAccumToPostEffects,
        ]
    );
}

#[test]
fn objects_debug_buffers_stop_after_gbuffer() {
    let p = plan_objects(ObjectsIn {
        debug_render_buffers: true,
        ..Default::default()
    });
    let s = steps(p, false, false, false);
    assert_eq!(s.last(), Some(&Step::RenderGbufferContent));
    assert!(!s.contains(&Step::RenderLights));
}

#[test]
fn setup_gbuffer_clears_by_type() {
    let a = ops(plan_setup_gbuffer(false, 2));
    assert_eq!(
        a,
        [
            Op::SetGBuffer { comp: comp::FULL },
            Op::ClearDepthValue { v: 1.0 },
            Op::ClearFb {
                flags: clear::DEPTH,
                use_pos: true
            },
        ]
    );
    let b = ops(plan_setup_gbuffer(true, 3));
    assert_eq!(b.len(), 9);
    assert_eq!(b.last(), Some(&Op::ClearColorSettings));
    let c = ops(plan_setup_gbuffer(true, 2));
    assert_eq!(c.len(), 5);
}

#[test]
fn passes_set_expected_state() {
    let z = ops(plan_pass(Pass::ZBegin));
    assert_eq!(z.len(), 6);
    assert_eq!(z.last(), Some(&Op::TexRangeNull { first: 0 }));
    assert_eq!(
        ops(plan_pass(Pass::GbufferEnd)),
        [Op::State {
            which: State::DepthFunc,
            val: st::FUNC_LESS_OR_EQUAL
        }]
    );
}

fn copy_in() -> CopyIn {
    CopyIn {
        screen: v2l(1280, 720),
        screen_f: Vector2f {
            x: 1280.0,
            y: 720.0,
        },
        rt_size: v2l(1280, 720),
        tonemap_ok: true,
        gamma: 2.2,
        key: 0.5,
        exposure: 1.0,
        white_cut: 3.5,
        blur_samples: 10,
        ..Default::default()
    }
}

fn answer_tonemap(ok_from: usize) -> impl Fn(&Op) -> Feed {
    let tries = std::cell::Cell::new(0usize);
    move |o| match o {
        Op::TryToneMap { .. } => {
            let n = tries.get();
            tries.set(n + 1);
            Feed::Bool(n >= ok_from)
        }
        _ => Feed::Float(1.0),
    }
}

#[test]
fn copy_without_tonemap_is_plain_copy() {
    let mut i = copy_in();
    i.tonemap_ok = false;
    let o = run(plan_copy_accum(i, FilmGrain::new()), answer_tonemap(0));
    assert!(
        o.iter()
            .all(|o| !matches!(o, Op::Ask(_) | Op::TryToneMap { .. }))
    );
    assert!(o.contains(&Op::Tex {
        unit: 0,
        tex: tex::ACCUM
    }));
    assert_eq!(o.last(), Some(&Op::Program { prog: NONE }));
}

#[test]
fn tonemap_fallback_chain_drops_bits() {
    let mut i = copy_in();
    i.grading = true;
    i.grading_target = true;
    i.srgb = true;
    i.noise = true;
    i.noise_width = 64;
    let o = run(plan_copy_accum(i, FilmGrain::new()), answer_tonemap(2));
    let combos: Vec<i32> = o
        .iter()
        .filter_map(|o| {
            if let Op::TryToneMap { combo } = o {
                Some(*combo)
            } else {
                None
            }
        })
        .collect();
    assert_eq!(combos, [1 | 4 | 8 | 16, 1, 0]);
    assert!(!o.iter().any(|o| matches!(o, Op::Vec4 { .. })));
}

#[test]
fn tonemap_failing_everywhere_skips_to_copy() {
    let o = run(
        plan_copy_accum(copy_in(), FilmGrain::new()),
        answer_tonemap(usize::MAX),
    );
    assert!(!o.iter().any(|o| matches!(
        o,
        Op::Float {
            prog: prog::TONEMAP,
            ..
        }
    )));
    assert!(o.iter().any(|o| matches!(o, Op::Quad { .. })));
}

#[test]
fn bloom_blur_weights_are_normalised() {
    let mut i = copy_in();
    i.bloom = true;
    i.bloom_programs_ok = true;
    i.bloom_width = 128.0;
    i.bloom_bright = 0.75;
    let mut p = plan_copy_accum(i, FilmGrain::new());
    let answer = answer_tonemap(0);
    let mut weight_arrays = Vec::new();
    while let Some(o) = p.next_op() {
        if matches!(o, Op::Ask(_) | Op::TryToneMap { .. }) {
            p.feed(answer(&o));
        }
        if let Op::FloatArr { id, arr, len, .. } = o
            && id == var::AV_WEIGHTS
        {
            weight_arrays.push((p.array(arr).to_vec(), len));
        }
    }
    assert_eq!(weight_arrays.len(), 8);
    for (w, len) in weight_arrays.iter().skip(1).step_by(1) {
        assert_eq!(w.len() as i32, *len);
    }
    let blur_pass = &weight_arrays[1].0;
    let sum: f32 = blur_pass[0] + 2.0 * blur_pass[1..].iter().sum::<f32>();
    assert!((sum - 1.0).abs() < 1e-5, "{sum}");
}

#[test]
fn image_trail_consumes_alpha_and_blends_below_one() {
    let mut i = copy_in();
    i.trail_ok = true;
    i.trail_alpha = 0.5;
    let mut p = plan_copy_accum(i.clone(), FilmGrain::new());
    let answer = answer_tonemap(0);
    let mut all = Vec::new();
    while let Some(o) = p.next_op() {
        if matches!(o, Op::Ask(_) | Op::TryToneMap { .. }) {
            p.feed(answer(&o));
        }
        all.push(o);
    }
    assert!(p.res.trail_consumed);
    assert!(all.contains(&Op::State {
        which: State::Blend,
        val: st::BLEND_ALPHA
    }));
    i.trail_alpha = 1.5;
    let o = run(plan_copy_accum(i, FilmGrain::new()), answer_tonemap(0));
    assert!(o.contains(&Op::Float {
        prog: prog::TRAIL,
        id: var::AF_ALPHA,
        v: 1.0
    }));
}

#[test]
fn film_grain_seeds_once_and_wraps_time() {
    let mut i = copy_in();
    i.noise = true;
    i.noise_width = 128;
    i.frame_time = 0.5;
    let mut p = plan_copy_accum(i, FilmGrain::new());
    let answer = answer_tonemap(0);
    while let Some(o) = p.next_op() {
        if matches!(o, Op::Ask(_) | Op::TryToneMap { .. }) {
            p.feed(answer(&o));
        }
    }
    let g = p.res.film_grain;
    assert!(g.t >= 0.0 && g.t < 128.0);
    assert_ne!(g.transform[1], [0.0; 4]);
}

#[test]
fn tonemap_defines_follow_combo_bits() {
    use define::*;
    assert_eq!(tonemap_defines(0, false), [USE_UV]);
    assert_eq!(
        tonemap_defines(63, false),
        [
            USE_UV,
            COLOR_GRADING,
            BLOOM,
            FILM_GRAIN,
            SRGB,
            BLEND_GRADING,
            HDR_OUTPUT
        ]
    );
    assert_eq!(
        tonemap_defines(4 | 16, true),
        [USE_UV, NOISE, LAYERED_GRADING]
    );
}

#[test]
fn shadow_cfg_tables() {
    let c = shadow_cfg(2, 2, true);
    assert_eq!((c.high.x, c.medium.x, c.low.x), (1024, 512, 256));
    assert_eq!((c.jitter_size, c.jitter_samples), (32, 32));
    let c = shadow_cfg(0, 1, false);
    assert_eq!((c.high.x, c.medium.x, c.low.x), (256, 256, 128));
    assert_eq!((c.jitter_size, c.jitter_samples), (32, 4));
    assert_eq!(shadow_cfg(1, 0, true).jitter_samples, 0);
    assert_eq!(shadow_cfg(9, 9, true).high.x, 1024);
}

#[test]
fn far_plane_corners() {
    let f = far_plane(std::f32::consts::FRAC_PI_2, 2.0, 100.0);
    assert!((f.top + 100.0).abs() < 1e-3);
    assert_eq!((f.bottom, f.left), (-f.top, -f.right));
    assert!((f.right / f.bottom - 2.0).abs() < 1e-6);
}

#[test]
fn accum_quad_uv_flips_y_and_normalises() {
    let screen = Vector2f { x: 100.0, y: 50.0 };
    let (min, max) = accum_quad_uv(v2l(10, 5), v2l(20, 10), screen, true);
    assert_eq!((min, max), ([10.0, 35.0], [30.0, 45.0]));
    let (min, max) = accum_quad_uv(v2l(10, 5), v2l(20, 10), screen, false);
    assert_eq!((min, max), ([0.1, 0.7], [0.3, 0.9]));
}

#[test]
fn debug_texture_index_map() {
    let t = |i| debug_gbuffer_texture(i, [3, 0, 2]);
    assert_eq!(
        t(2),
        DebugTex {
            kind: dbg::BUFFER,
            a: 2,
            b: 0
        }
    );
    assert_eq!(t(4).kind, dbg::ACCUM);
    assert_eq!(
        t(7),
        DebugTex {
            kind: dbg::H3_SSAO,
            a: 1,
            b: 0
        }
    );
    assert_eq!(
        t(10),
        DebugTex {
            kind: dbg::SHADOW,
            a: 0,
            b: 0
        }
    );
    assert_eq!(t(11).kind, dbg::NULL);
    assert_eq!(
        t(12),
        DebugTex {
            kind: dbg::SHADOW,
            a: 2,
            b: 0
        }
    );
    assert_eq!(t(13).kind, dbg::DIR_SHADOW);
    assert_eq!(t(15).kind, dbg::REFLECTION);
    assert_eq!(
        t(22),
        DebugTex {
            kind: dbg::TEMP,
            a: 1,
            b: 12
        }
    );
    assert_eq!(
        t(100 + 32 + 1),
        DebugTex {
            kind: dbg::SHADOW,
            a: 2,
            b: 1
        }
    );
    assert_eq!(t(100 + 32 + 2).kind, dbg::NULL);
    assert_eq!(t(100 + 16 * 3).kind, dbg::NULL);
}

#[test]
fn glare_grid_shape() {
    let idx = glare_indices();
    assert_eq!(idx.len(), GLARE_INDICES);
    assert_eq!(idx.iter().copied().max(), Some(399));
    assert_eq!(&idx[..6], &[20, 0, 21, 21, 0, 1]);
}

#[test]
fn ssao_mips_round_to_power_of_two() {
    let m = ssao_mips(v2l(640, 360));
    assert_eq!(m.mip_size, v2l(512, 256));
    assert_eq!((m.mips, m.levels), (5, 10));
}

#[test]
fn dof_table_shape() {
    assert_eq!(dof_rows(4), 1);
    assert_eq!(dof_rows(16), 2);
    assert_eq!(dof_rows(-3), 1);
    let t = dof_table(16, 1280.0);
    assert_eq!(t.len(), 64 * 2 * 4);
    assert!(t[..4].iter().all(|&v| v == 0.0));
    assert!(t[4..8].iter().all(|&v| (0.0..=1.0).contains(&v)));
}

#[test]
fn screen_position_vars_unit_target() {
    let far = far_plane(std::f32::consts::FRAC_PI_2, 1.0, 10.0);
    let (to_far, inv) = screen_position_vars(&far, v2l(100, 100), v2l(0, 0), v2l(100, 100));
    assert_eq!(inv, [0.01, 0.01]);
    assert!((to_far[0] - 0.2).abs() < 1e-6);
    assert!((to_far[2] - far.left).abs() < 1e-6);
}
