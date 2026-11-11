use super::deferred::*;
use super::deferred_pass::*;

fn light(idx: i32, shadows: bool) -> PassLight {
    PassLight {
        idx,
        kind: 0,
        cast_shadows: shadows,
        inside_near: false,
        blend_func: blend::ADD,
        diffuse: [0.5, 0.25, 2.0, 1.0],
        brightness: 2.0,
        distance_fade: 0.5,
    }
}

fn run(mut p: Plan, answer: bool) -> Vec<Op> {
    let mut v = Vec::new();
    while let Some(o) = p.next_op() {
        if matches!(
            o,
            Op::P3(P3Op::ShadowSetup(_)) | Op::Step(Step::RenderBoxWeighted)
        ) {
            p.feed(Feed::Bool(answer));
        }
        v.push(o);
    }
    v
}

fn input(pass: i32) -> PassIn {
    PassIn {
        pass,
        stencil_dirty: true,
        depth_cull: true,
        far_plane: 10.0,
        far_left: -1.0,
        far_right: 1.0,
        far_top: 2.0,
        far_bottom: -2.0,
        ..PassIn::default()
    }
}

fn draws(v: &[Op]) -> usize {
    v.iter()
        .filter(|o| matches!(o, Op::Dl(DlOp::DrawCurrent)))
        .count()
}

#[test]
fn empty_list_emits_nothing() {
    for p in 0..5 {
        assert!(run(plan_light_pass(input(p), &[]), true).is_empty());
    }
}

#[test]
fn stencil_front_batches_eight_lights_and_clears_between() {
    let lights: Vec<_> = (0..9).map(|i| light(i, false)).collect();
    let v = run(
        plan_light_pass(input(pass::STENCIL_FRONT_RENDER_BACK), &lights),
        false,
    );
    let clears = v
        .iter()
        .filter(|o| matches!(o, Op::P3(P3Op::ClearStencil)))
        .count();
    // dirty clear at the start and one between the two batches
    assert_eq!(clears, 2);
    assert_eq!(draws(&v), 18);
    assert!(matches!(
        v.last(),
        Some(Op::P3(P3Op::Log {
            kind: logkind::END,
            ..
        }))
    ));
    let masks: Vec<i32> = v
        .iter()
        .filter_map(|o| match o {
            Op::P3(P3Op::StencilWriteMask(m)) => Some(*m),
            _ => None,
        })
        .collect();
    assert_eq!(&masks[..8], &[1, 2, 4, 8, 16, 32, 64, 128]);
    assert_eq!(masks[8], 0xFF);
    assert_eq!(masks[9], 1);
}

#[test]
fn shadow_branch_follows_the_answer() {
    let lights = [light(3, true)];
    let yes = run(plan_light_pass(input(pass::RENDER_BACK), &lights), true);
    let no = run(plan_light_pass(input(pass::RENDER_BACK), &lights), false);
    assert!(yes.contains(&Op::P3(P3Op::ShadowMap(3))));
    assert!(!no.contains(&Op::P3(P3Op::ShadowMap(3))));
    assert_eq!(yes.len(), no.len() + 3);
}

#[test]
fn occlusion_pairs_wrap_the_front_draw_only() {
    let mut i = input(pass::STENCIL_FRONT_RENDER_BACK);
    i.occlusion_test = true;
    let v = run(plan_light_pass(i, &[light(0, false)]), false);
    assert!(v.contains(&Op::P3(P3Op::PairBegin(0))));
    assert!(v.contains(&Op::P3(P3Op::PairEnd)));
    i.pass = pass::STENCIL_BACK_SCREEN_QUAD;
    let v = run(plan_light_pass(i, &[light(0, false)]), false);
    assert!(!v.contains(&Op::P3(P3Op::PairEnd)));
}

#[test]
fn box_light_color_is_squared_in_hdr_and_scaled() {
    let mut i = input(pass::BOX_RENDER_BACK);
    i.hdr = true;
    i.box_ssao = true;
    let v = run(plan_light_pass(i, &[light(1, false)]), false);
    let c = v.iter().find_map(|o| match o {
        Op::Vec4 { prog, v, .. } => Some((*prog, *v)),
        _ => None,
    });
    assert_eq!(c, Some((pslot::BOX_PROG1, [0.25, 0.0625, 4.0, 1.0])));
    assert!(v.contains(&Op::Tex { unit: 5, tex: 4 }));
    assert!(v.contains(&Op::State {
        which: State::Blend,
        val: st::BLEND_ADD
    }));
    assert_eq!(draws(&v), 1);
}

#[test]
fn box_replace_blend_turns_blending_off() {
    let mut l = light(0, false);
    l.blend_func = blend::REPLACE;
    let v = run(plan_light_pass(input(pass::BOX_RENDER_BACK), &[l]), false);
    assert!(v.contains(&Op::State {
        which: State::Blend,
        val: st::BLEND_NONE
    }));
    l.blend_func = 2;
    let v = run(plan_light_pass(input(pass::BOX_RENDER_BACK), &[l]), false);
    let blends = v
        .iter()
        .filter(|o| {
            matches!(
                o,
                Op::State {
                    which: State::Blend,
                    ..
                }
            )
        })
        .count();
    // only the trailing additive reset
    assert_eq!(blends, 1);
}

#[test]
fn back_screen_pass_restores_projection_and_marks_stencil_dirty() {
    let v = run(
        plan_light_pass(
            input(pass::STENCIL_BACK_SCREEN_QUAD),
            &[light(0, false), light(1, false)],
        ),
        false,
    );
    assert!(v.contains(&Op::NormalFrustumProjection));
    assert!(v.contains(&Op::P3(P3Op::ScissorOff)));
    assert!(v.contains(&Op::FlatProjectionMinMax {
        min: [-1.0, -2.0, -15.0],
        max: [1.0, 2.0, 15.0]
    }));
    assert!(v.contains(&Op::P3(P3Op::StencilDirty(true))));
    assert_eq!(draws(&v), 4);
}

#[test]
fn render_lights_skips_box_passes_when_weighted_handled_them() {
    let steps = |handled| {
        run(plan_render_lights(3), handled)
            .into_iter()
            .filter_map(|o| match o {
                Op::Step(Step::LightPass(n)) => Some(n),
                _ => None,
            })
            .collect::<Vec<_>>()
    };
    assert_eq!(steps(true), [0, 1, 2]);
    assert_eq!(steps(false), [3, 4, 0, 1, 2]);
    let v = run(plan_render_lights(9), true);
    let texs = v.iter().filter(|o| matches!(o, Op::Tex { .. })).count();
    assert_eq!(texs, 5);
    assert!(v.contains(&Op::TexRangeNull { first: 9 }));
}
