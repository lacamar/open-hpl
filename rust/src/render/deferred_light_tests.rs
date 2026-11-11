use super::deferred::*;
use super::deferred_light::var;
use super::deferred_light::*;
use super::deferred_lightlist::*;
use crate::math::{IDENTITY, Matrixf};
use crate::types::{Vector2l, Vector3f};

fn v3(x: f32, y: f32, z: f32) -> Vector3f {
    Vector3f { x, y, z }
}

fn ops(mut p: Plan, mut answer: impl FnMut(&Op) -> Option<Feed>) -> (Vec<Op>, Plan) {
    let mut v = Vec::new();
    while let Some(o) = p.next_op() {
        if let Some(f) = answer(&o) {
            p.feed(f);
        }
        v.push(o);
    }
    (v, p)
}

fn flat(p: Plan) -> Vec<Op> {
    ops(p, |_| None).0
}

fn far() -> FarIn {
    FarIn {
        plane: 10.0,
        top: 3.0,
        bottom: -3.0,
        right: 4.0,
        left: -4.0,
    }
}

fn ssao_in() -> SsaoIn {
    SsaoIn {
        loaded: true,
        active: true,
        far: far(),
        gbuffer_depth_size: Vector2l { x: 640, y: 480 },
        ssao_size: Vector2l { x: 320, y: 240 },
        unpack_ok: true,
        render_ok: true,
        blur_ok: [true, true],
        scatter_mul: 0.2,
        scatter_min: 0.015,
        scatter_max: 0.13,
        depth_diff_mul: 1.5,
        skip_edge: 3.0,
        ..Default::default()
    }
}

fn point() -> LightIn {
    LightIn {
        kind: kind::POINT,
        radius: 5.0,
        brightness: 1.0,
        falloff_pow: 1.0,
        spot_falloff_pow: 1.0,
        diffuse: [0.5, 0.25, 1.0, 1.0],
        cast_specular: true,
        cast_diffuse: true,
        shadow_res: res::HIGH,
        world: IDENTITY,
        view_proj: IDENTITY,
        ..Default::default()
    }
}

fn spot() -> LightIn {
    LightIn {
        kind: kind::SPOT,
        tan_half_fov: 0.5,
        aspect: 1.0,
        cos_half_fov: 0.8,
        near_clip: 0.1,
        cast_shadows: true,
        bv_min: v3(-1.0, -1.0, -6.0),
        bv_max: v3(1.0, 1.0, -1.0),
        ..point()
    }
}

#[test]
fn ssao_skips() {
    assert!(
        plan_ssao(&SsaoIn {
            loaded: false,
            ..ssao_in()
        })
        .is_none()
    );
    assert!(
        plan_ssao(&SsaoIn {
            active: false,
            ..ssao_in()
        })
        .is_none()
    );
    let box_only = SsaoIn {
        in_box_light: true,
        box_lists_empty: true,
        ..ssao_in()
    };
    assert!(plan_ssao(&box_only).is_none());
    let box_lit = SsaoIn {
        box_lists_empty: false,
        ..box_only
    };
    assert!(plan_ssao(&box_lit).is_some());
}

#[test]
fn ssao_ops() {
    let o = flat(plan_ssao(&ssao_in()).unwrap_or_else(|| fixed(vec![])));
    assert_eq!(
        o[..8],
        [
            state(State::Channel, st::CHANNEL_RGBA),
            Op::TexRangeNull { first: 1 },
            state(State::Alpha, st::ALPHA_SOLID),
            state(State::Blend, st::BLEND_NONE),
            state(State::DepthWrite, 0),
            state(State::DepthTest, 0),
            Op::FlatProjectionMinMax {
                min: [-4.0, -3.0, -15.0],
                max: [4.0, 3.0, 15.0]
            },
            Op::SetFb {
                fb: 0,
                use_pos: false
            },
        ]
    );
    let quads = o.iter().filter(|o| matches!(o, Op::QuadAt { .. })).count();
    assert_eq!(quads, 4);
    assert_eq!(o.last(), Some(&Op::NormalFrustumProjection));
    let on_color = SsaoIn {
        on_color_buffer: true,
        ..ssao_in()
    };
    let o2 = flat(plan_ssao(&on_color).unwrap_or_else(|| fixed(vec![])));
    assert!(o2.contains(&Op::Quad {
        min: [0.0, 0.0],
        max: [320.0, 240.0]
    }));

    let box_mode = SsaoIn {
        on_color_buffer: false,
        gbuffer_64bit: true,
        render_ok: false,
        blur_ok: [false, true],
        ..ssao_in()
    };
    let o = flat(plan_ssao(&box_mode).unwrap_or_else(|| fixed(vec![])));
    assert!(!o.iter().any(|o| matches!(o, Op::Quad { .. })));
    let floats = o.iter().filter(|o| matches!(o, Op::Float { .. })).count();
    assert_eq!(floats, 2);
    assert!(o.contains(&Op::Float {
        prog: slot::prog::SSAO_UNPACK,
        id: var::AF_NEG_INV_FAR_PLANE,
        v: -0.1
    }));
}

#[test]
fn edge_smooth_ops() {
    let i = EdgeIn {
        loaded: true,
        use_edge_smooth: true,
        gbuffer_64bit: true,
        render_ok: false,
        far: far(),
        screen: Vector2l { x: 640, y: 480 },
        screen_f: [640.0, 480.0],
    };
    assert!(plan_edge_smooth(&EdgeIn { loaded: false, ..i }).is_none());
    assert!(
        plan_edge_smooth(&EdgeIn {
            use_edge_smooth: false,
            ..i
        })
        .is_none()
    );
    let o = flat(plan_edge_smooth(&i).unwrap_or_else(|| fixed(vec![])));
    assert!(o.contains(&Op::CopyFbToTex {
        tex: slot::tex::EDGE_TEMP,
        size: i.screen
    }));
    assert_eq!(
        o.iter().filter(|o| matches!(o, Op::Float { .. })).count(),
        1
    );
    assert_eq!(o.last(), Some(&Op::NormalFrustumProjection));
}

fn light_state(l: LightIn) -> LightState {
    LightState {
        light: l,
        view_render: Matrixf {
            m: [
                [1.0, 0.0, 0.0, 2.0],
                [0.0, 1.0, 0.0, 3.0],
                [0.0, 0.0, 1.0, -4.0],
                [0.0, 0.0, 0.0, 1.0],
            ],
        },
        view_transform: IDENTITY,
        inv_view: IDENTITY,
        distance_fade: 0.5,
        far_plane: 100.0,
        depth_texture_idx: 2,
        program_ok: true,
        ..Default::default()
    }
}

#[test]
fn light_variables_point() {
    let p = plan_light_variables(&light_state(point()));
    let o = flat(p);
    let l = slot::prog::LIGHT;
    assert_eq!(
        o[0],
        Op::Vec3 {
            prog: l,
            id: var::AV_LIGHT_POS,
            v: [2.0, 3.0, -4.0]
        }
    );
    assert_eq!(
        o[1],
        Op::Vec4 {
            prog: l,
            id: var::AV_LIGHT_POS_SCALE,
            v: [2.0, 3.0, -4.0, 0.2]
        }
    );
    assert_eq!(
        o[2],
        Op::Vec4 {
            prog: l,
            id: var::AV_LIGHT_COLOR,
            v: [0.25, 0.125, 0.5, 1.0]
        }
    );
    assert_eq!(o.len(), 7);

    let mut off = light_state(point());
    off.program_ok = false;
    assert!(flat(plan_light_variables(&off)).is_empty());
}

#[test]
fn light_color_alpha_flags() {
    let mut l = point();
    l.cast_specular = false;
    l.cast_diffuse = false;
    let mut s = light_state(l);
    s.hdr = true;
    let o = flat(plan_light_variables(&s));
    assert_eq!(
        o[2],
        Op::Vec4 {
            prog: slot::prog::LIGHT,
            id: var::AV_LIGHT_COLOR,
            v: [0.125, 0.03125, 0.5, -0.0]
        }
    );
    l.cast_specular = true;
    s.light = l;
    let o = flat(plan_light_variables(&s));
    assert!(matches!(o[2], Op::Vec4 { v, .. } if v[3] == -1.0));
}

#[test]
fn light_variables_spot_shadow() {
    let mut l = spot();
    l.has_gobo = true;
    l.gobo_specular = true;
    let mut s = light_state(l);
    s.cast_shadows = true;
    s.shadow_tex = true;
    s.shadow_size = Vector2l { x: 512, y: 256 };
    s.jitter = true;
    let o = flat(plan_light_variables(&s));
    assert!(o.contains(&Op::Dl(DlOp::ClearShadowTexture)));
    assert!(o.contains(&Op::Vec2 {
        prog: slot::prog::LIGHT,
        id: var::AV_SHADOW_MAP_SIZE,
        v: [512.0, 256.0]
    }));
    let mats = o.iter().filter(|o| matches!(o, Op::Matrix { .. })).count();
    assert_eq!(mats, 2);
    let p = plan_light_variables(&s);
    assert_eq!(p.array(0).len(), 16);
    s.jitter = false;
    let o = flat(plan_light_variables(&s));
    assert!(
        !o.iter()
            .any(|o| matches!(o, Op::Vec2 { id, .. } if *id == var::AV_SHADOW_MAP_OFFSET_MUL))
    );
}

#[test]
fn light_variables_underwater() {
    let mut s = light_state(point());
    s.underwater = true;
    s.fog_color = [0.5, 0.5, 0.5, 1.0];
    s.fog_start = 2.0;
    s.fog_end = 10.0;
    let o = flat(plan_light_variables(&s));
    assert!(o.contains(&Op::Vec2 {
        prog: slot::prog::LIGHT,
        id: var::AV_FOG_START_AND_LENGTH,
        v: [2.0, 8.0]
    }));
}

#[test]
fn light_program_flags_and_textures() {
    use feature::*;
    let mut l = spot();
    l.has_mask_box = true;
    l.translucency = 1.0;
    let mut s = light_state(l);
    s.cast_shadows = true;
    s.shadow_tex = true;
    s.jitter = true;
    s.underwater = true;
    let (o, _) = ops(plan_light_program(&s, LIGHT_SHAPES, 0), |o| {
        matches!(o, Op::GenProgram { .. }).then_some(Feed::Bool(true))
    });
    let Op::GenProgram { flags, mode, .. } = o[0] else {
        panic!("first op {:?}", o[0]);
    };
    assert_eq!(mode, 0);
    assert_eq!(
        flags,
        LIGHT_SHAPES
            | SPECULAR
            | BOX_MASK
            | TRANSLUCENCY
            | UNDERWATER_FOG
            | SPOT_LIGHT
            | DIVIDE_IN_FRAG
            | SHADOW_MAP
    );
    assert!(o.contains(&Op::Tex {
        unit: 7,
        tex: slot::tex::JITTER
    }));
    assert!(o.contains(&Op::Tex {
        unit: 5,
        tex: slot::tex::SPOT_FALLOFF
    }));
    assert!(o.contains(&Op::ScreenPositionVars {
        prog: slot::prog::LIGHT
    }));

    let (o, _) = ops(plan_light_program(&s, 0, 0), |o| {
        matches!(o, Op::GenProgram { .. }).then_some(Feed::Bool(false))
    });
    assert!(!o.iter().any(|o| matches!(o, Op::ScreenPositionVars { .. })));
    assert!(o.contains(&Op::Program {
        prog: slot::prog::LIGHT
    }));
}

#[test]
fn shadow_map_plan() {
    let run = |update: bool, n: i32| {
        ops(plan_light_shadow_map(n), |o| {
            matches!(o, Op::AskGeom { .. }).then_some(Feed::Bool(update))
        })
        .0
    };
    let o = run(true, 4);
    assert!(o.contains(&Op::Dl(DlOp::RenderShadowMap)));
    assert_eq!(o.iter().filter(|o| matches!(o, Op::Tex { .. })).count(), 4);
    let o = run(false, 9);
    assert!(!o.contains(&Op::Dl(DlOp::RenderShadowMap)));
    assert_eq!(o.iter().filter(|o| matches!(o, Op::Tex { .. })).count(), 5);
    assert_eq!(
        run(false, -1)
            .iter()
            .filter(|o| matches!(o, Op::Tex { .. }))
            .count(),
        0
    );
}

#[test]
fn shadow_resolution_and_shape() {
    assert_eq!(shadow_map_resolution(res::HIGH, res::HIGH), res::HIGH);
    assert_eq!(shadow_map_resolution(res::HIGH, res::MEDIUM), res::MEDIUM);
    assert_eq!(shadow_map_resolution(res::LOW, res::MEDIUM), res::LOW);
    assert_eq!(shadow_map_resolution(res::HIGH, res::LOW), res::LOW);
    assert_eq!(light_shape(kind::POINT, 2), 2);
    assert_eq!(light_shape(kind::SPOT, 0), shape::PYRAMID);
    assert_eq!(light_shape(kind::BOX, 0), shape::NONE);
}

#[test]
fn sort_keys_order() {
    let a = SortKey::of(&point(), false);
    let b = SortKey::of(&point(), true);
    assert!(default_less(&a, &b) && !default_less(&b, &a));
    assert!(!default_less(&a, &a));
    let s = SortKey::of(&spot(), false);
    assert!(default_less(&a, &s));
    let mut spec = point();
    spec.diffuse[3] = 0.0;
    assert!(default_less(&SortKey::of(&spec, true), &a));
    let (mut g1, mut g2) = (spot(), spot());
    g1.gobo = 5;
    g2.gobo = 9;
    assert!(default_less(
        &SortKey::of(&g1, false),
        &SortKey::of(&g2, false)
    ));
    let (mut x, mut y) = (SortKey::default(), SortKey::default());
    x.box_prio = 2;
    y.box_prio = 1;
    assert!(box_less(&y, &x) && !box_less(&x, &y));
    assert!(list_less(list::BOX_RENDER_BACK, &y, &x));
}

fn frustum() -> FrustumIn {
    FrustumIn {
        view: IDENTITY,
        proj: IDENTITY,
        perspective: true,
        fov: 1.0,
        near: 0.1,
        aspect: 1.5,
        origin: v3(0.0, 0.0, 0.0),
    }
}

fn setup_in() -> SetupIn {
    SetupIn {
        test_large_lights: true,
        use_occlusion_culling: true,
        render_shadows: true,
        rt_size: Vector2l { x: 640, y: 480 },
        min_large_normalized_area: 0.04,
        max_shadow_res: res::HIGH,
        shadow_dist_none: 40.0,
        shadow_dist_low: 20.0,
        shadow_dist_medium: 10.0,
        tan_half_fov: 0.5,
        sample_limit: 0,
        frustum: frustum(),
    }
}

fn setup_answers(near: bool, hit: [f32; 3]) -> impl FnMut(&Op) -> Option<Feed> {
    move |o| match o {
        Op::AskGeom {
            q: Geom::LineIntersection,
            ..
        } => Some(Feed::Vec3(hit)),
        Op::AskGeom { .. } => Some(Feed::Bool(near)),
        _ => None,
    }
}

#[test]
fn setup_lights_prelude_and_queries() {
    let mut lights = vec![point(), point()];
    lights[0].world = Matrixf {
        m: [
            [1.0, 0.0, 0.0, 0.0],
            [0.0, 1.0, 0.0, 0.0],
            [0.0, 0.0, 1.0, -3.0],
            [0.0, 0.0, 0.0, 1.0],
        ],
    };
    lights[1].id = 7;
    lights[1].world = lights[0].world;
    let pairs = [
        PairIn {
            light: 7,
            samples: 5,
        },
        PairIn {
            light: 8,
            samples: 0,
        },
    ];
    let (o, p) = ops(
        plan_setup_lights(setup_in(), lights, &pairs),
        setup_answers(false, [0.0; 3]),
    );
    assert_eq!(o[0], Op::Dl(DlOp::ClearPairs));
    assert_eq!(o[1], Op::Dl(DlOp::ClearTempLights));
    assert_eq!(o[2], Op::Dl(DlOp::SetMinLargeArea(12288)));
    assert_eq!(o.last(), Some(&Op::Dl(DlOp::Flush)));
    let begins = o
        .iter()
        .filter(|o| matches!(o, Op::Dl(DlOp::QueryBegin(_))))
        .count();
    assert_eq!(begins, 1, "light id 7 was visible last frame");
    assert!(o.contains(&Op::Dl(DlOp::QueryBegin(0))));
    assert_eq!(p.res.lights.lights.len(), 2);
    let l0 = p.res.lights.lights[0];
    assert!(!l0.inside_near_plane);
    assert!(
        l0.area > 12288,
        "a radius-5 sphere at depth 3 covers the screen: {}",
        l0.area
    );
}

#[test]
fn setup_lights_without_queries_and_inside() {
    let mut i = setup_in();
    i.test_large_lights = false;
    let (o, _) = ops(
        plan_setup_lights(i, vec![point()], &[]),
        setup_answers(false, [0.0; 3]),
    );
    assert!(!o.contains(&Op::Dl(DlOp::Flush)));
    assert!(!o.contains(&Op::Dl(DlOp::ClearPairs)));
    assert!(!o.iter().any(|o| matches!(o, Op::Dl(DlOp::AcquireQuery(_)))));

    let (o, p) = ops(
        plan_setup_lights(setup_in(), vec![point()], &[]),
        setup_answers(true, [0.0; 3]),
    );
    assert!(!o.iter().any(|o| matches!(o, Op::Dl(DlOp::AcquireQuery(_)))));
    assert!(p.res.lights.lights[0].inside_near_plane);
}

#[test]
fn setup_lights_shadow_distance_bands() {
    let band = |dist: f32, wanted: i32, max: i32| {
        let mut l = spot();
        l.shadow_res = wanted;
        let mut i = setup_in();
        i.max_shadow_res = max;
        let (_, p) = ops(
            plan_setup_lights(i, vec![l], &[]),
            setup_answers(false, [0.0, 0.0, dist]),
        );
        let o = p.res.lights.lights[0];
        (o.cast_shadows, o.shadow_res)
    };
    assert_eq!(band(5.0, res::HIGH, res::HIGH), (true, res::HIGH));
    assert_eq!(band(15.0, res::HIGH, res::HIGH), (true, res::MEDIUM));
    assert_eq!(band(15.0, res::MEDIUM, res::HIGH), (true, res::LOW));
    assert_eq!(band(25.0, res::HIGH, res::HIGH), (true, res::LOW));
    assert_eq!(band(25.0, res::LOW, res::HIGH), (false, res::LOW));
    assert_eq!(band(50.0, res::HIGH, res::HIGH), (false, res::HIGH));
    assert_eq!(band(5.0, res::HIGH, res::MEDIUM), (true, res::MEDIUM));

    let mut l = spot();
    l.cast_shadows = false;
    let (_, p) = ops(
        plan_setup_lights(setup_in(), vec![l], &[]),
        setup_answers(true, [0.0; 3]),
    );
    assert!(!p.res.lights.lights[0].cast_shadows);

    let (_, p) = ops(
        plan_setup_lights(setup_in(), vec![spot()], &[]),
        setup_answers(true, [0.0; 3]),
    );
    assert_eq!(
        (
            p.res.lights.lights[0].cast_shadows,
            p.res.lights.lights[0].shadow_res
        ),
        (true, res::HIGH)
    );
}

#[test]
fn setup_lights_nan_and_box() {
    let mut nan = point();
    nan.radius = f32::NAN;
    let mut b = point();
    b.kind = kind::BOX;
    let mut d = point();
    d.kind = 3;
    let (o, p) = ops(
        plan_setup_lights(setup_in(), vec![nan, b, d], &[]),
        setup_answers(false, [0.0; 3]),
    );
    assert_eq!(p.res.lights.lights.len(), 3);
    let made = o
        .iter()
        .filter(|o| matches!(o, Op::Dl(DlOp::Light(_))))
        .count();
    assert_eq!(made, 3);
    assert_eq!(p.res.lights.lights[1], LightOut::default());
}

fn init_in() -> InitIn {
    InitIn {
        view: IDENTITY,
        last_fov: -1.0,
        last_far_plane: -1.0,
        fov: 1.0,
        far_plane: 20.0,
        far_left: -8.0,
        far_bottom: -6.0,
        far_right: 8.0,
        far_top: 6.0,
        min_large_area: 100,
        sample_limit: 0,
    }
}

fn init_light(l: LightIn, inside: bool, area: i32, query: bool) -> InitLight {
    InitLight {
        light: l,
        inside_near: inside,
        has_query: query,
        area,
    }
}

#[test]
fn init_lights_classification() {
    let mut p1 = point();
    p1.id = 1;
    let mut p2 = point();
    p2.id = 2;
    let mut p3 = point();
    p3.id = 3;
    let mut s = spot();
    s.id = 4;
    let mut b1 = point();
    b1.kind = kind::BOX;
    b1.id = 5;
    b1.box_prio = 3;
    b1.box_size = v3(2.0, 2.0, 2.0);
    b1.world_pos = v3(1.0, 2.0, 3.0);
    let mut b2 = b1;
    b2.id = 6;
    b2.box_prio = 1;
    let lights = vec![
        init_light(p1, true, 0, false),
        init_light(p2, false, 500, false),
        init_light(p3, false, 5, false),
        init_light(s, false, 0, false),
        init_light(b1, false, 0, false),
        init_light(b2, false, 0, false),
    ];
    let (o, p) = ops(plan_init_lights(init_in(), lights), |o| match o {
        Op::AskGeom { idx: 5, .. } => Some(Feed::Bool(true)),
        Op::AskGeom { .. } => Some(Feed::Bool(false)),
        _ => None,
    });
    let l = &p.res.lights;
    assert_eq!(l.lists[list::RENDER_BACK], [2, 0]);
    assert_eq!(l.lists[list::STENCIL_FRONT_RENDER_BACK], [1, 3]);
    assert_eq!(l.lists[list::BOX_RENDER_BACK], [5]);
    assert_eq!(l.lists[list::BOX_STENCIL_FRONT_RENDER_BACK], [4]);
    assert_eq!(l.rendered, 6);
    assert_eq!(o[0], Op::Dl(DlOp::InvView));
    assert!(matches!(o[1], Op::Dl(DlOp::UpdateFullscreenQuad { .. })));
    assert_eq!(o.last(), Some(&Op::Dl(DlOp::Lists)));
    assert_eq!(l.lights[4].render.m[0][3], 1.0 * 1.0 + 0.0);
    assert_eq!(l.lights[4].render.m[0][0], 2.0);
}

#[test]
fn init_lights_queries_and_quad_update() {
    let mut a = point();
    a.id = 1;
    let mut b = point();
    b.id = 2;
    let mut c = point();
    c.id = 3;
    let lights = vec![
        init_light(a, false, 500, true),
        init_light(b, false, 500, true),
        init_light(c, false, 500, true),
    ];
    let mut i = init_in();
    i.last_fov = i.fov;
    i.last_far_plane = i.far_plane;
    let (o, p) = ops(plan_init_lights(i, lights), |o| match o {
        Op::AskGeom {
            q: Geom::QueryFetch,
            idx,
            ..
        } => Some(Feed::Bool(*idx != 2)),
        Op::AskGeom {
            q: Geom::QuerySamples,
            idx,
            ..
        } => Some(Feed::Int(if *idx == 0 { 0 } else { 9 })),
        _ => None,
    });
    assert!(
        !o.iter()
            .any(|o| matches!(o, Op::Dl(DlOp::UpdateFullscreenQuad { .. })))
    );
    assert_eq!(p.res.lights.lists[list::STENCIL_FRONT_RENDER_BACK], [1, 2]);
    assert_eq!(p.res.lights.rendered, 2);
    let released = o
        .iter()
        .filter(|o| matches!(o, Op::Dl(DlOp::ReleaseQuery(_))))
        .count();
    assert_eq!(released, 3);
    assert!(o.contains(&Op::Dl(DlOp::QueryLog {
        idx: 0,
        samples: 0,
        visible: false
    })));
    assert!(o.contains(&Op::Dl(DlOp::QueryLog {
        idx: 1,
        samples: 9,
        visible: true
    })));
    assert!(
        !o.iter()
            .any(|o| matches!(o, Op::Dl(DlOp::QueryLog { idx: 2, .. })))
    );
}

#[test]
fn init_lights_sorts_with_std_sort_order() {
    let mut lights = Vec::new();
    for n in 0..40u64 {
        let mut l = point();
        l.id = 1000 - n;
        lights.push(init_light(l, false, 5, false));
    }
    let (_, p) = ops(plan_init_lights(init_in(), lights), |_| None);
    let list = &p.res.lights.lists[list::RENDER_BACK];
    assert_eq!(list.len(), 40);
    let all_equal_class = list
        .iter()
        .copied()
        .collect::<std::collections::BTreeSet<_>>()
        .len();
    assert_eq!(all_equal_class, 40);
}

#[test]
fn plan_matrix_arrays_are_row_major() {
    let mut l = spot();
    l.has_gobo = true;
    let mut s = light_state(l);
    s.light.view_proj = Matrixf {
        m: [
            [1.0, 2.0, 3.0, 4.0],
            [5.0, 6.0, 7.0, 8.0],
            [9.0, 10.0, 11.0, 12.0],
            [13.0, 14.0, 15.0, 16.0],
        ],
    };
    let p = plan_light_variables(&s);
    let arr = p.array(0).to_vec();
    assert_eq!(arr[0], 1.0);
    assert_eq!(arr[4], 5.0);
    assert_eq!(arr[15], 16.0);
}
