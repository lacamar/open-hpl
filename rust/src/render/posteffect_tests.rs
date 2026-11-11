use super::posteffect::*;
use crate::types::{Vector2f, Vector2l};

fn ops(mut p: Plan) -> Vec<Op> {
    std::iter::from_fn(|| p.next_op()).collect()
}

fn v2(x: f32, y: f32) -> Vector2f {
    Vector2f { x, y }
}

#[test]
fn chain_alternates_buffers_over_active_effects() {
    let steps = chain(&[true, false, true, true, false]);
    let got: Vec<_> = steps.iter().map(|s| (s.index, s.buffer)).collect();
    assert_eq!(got, [(0, 0), (2, 1), (3, 0)]);
    assert!(chain(&[false, false]).is_empty());
    assert!(chain(&[]).is_empty());
}

#[test]
fn insert_pos_is_after_equal_priorities_highest_first() {
    let cases: [(&[i32], i32, usize); 5] = [
        (&[], 5, 0),
        (&[10, 5, 5, 1], 5, 3),
        (&[10, 5, 5, 1], 7, 1),
        (&[10, 5, 5, 1], 0, 4),
        (&[10, 5, 5, 1], 11, 0),
    ];
    for (prios, prio, want) in cases {
        assert_eq!(insert_pos(prios, prio), want, "{prios:?} {prio}");
    }
}

#[test]
fn full_screen_target_has_unit_uv_window() {
    let (pos, size) = uv_pos_size(
        v2(1280.0, 720.0),
        v2(1280.0, 720.0),
        Vector2l { x: 0, y: 0 },
        Vector2l { x: 1280, y: 720 },
    );
    assert_eq!((pos, size), (v2(0.0, 0.0), v2(1280.0, 720.0)));
    let (min, max) = quad_uv(v2(1280.0, 720.0), true, pos, size);
    assert_eq!((min, max), (v2(0.0, 0.0), v2(1280.0, 720.0)));
    let (min, max) = quad_uv(v2(1280.0, 720.0), false, pos, size);
    assert_eq!((min, max), (v2(0.0, 0.0), v2(1.0, 1.0)));
}

#[test]
fn round_target_adds_half_and_truncates() {
    let (p, s) = round_target(v2(0.49, 1.5), v2(2.5, 3.2));
    assert_eq!((p.x, p.y, s.x, s.y), (0, 2, 3, 3));
}

#[test]
fn bloom_blur_iterations_repeat_second_buffer_pass() {
    let mk = |iterations| BloomIn {
        blur_size: 1.0,
        iterations,
        rgb_to_intensity: [0.3, 0.58, 0.12],
        programs_ok: true,
    };
    let quads = |n| {
        ops(plan_bloom(&mk(n)))
            .iter()
            .filter(|o| matches!(o, Op::Quad1 { .. }))
            .count()
    };
    assert_eq!((quads(0), quads(1), quads(2), quads(4)), (2, 2, 4, 8));
    let none = BloomIn {
        programs_ok: false,
        ..mk(2)
    };
    let p = plan_bloom(&none);
    assert_eq!(p.result, bloom::TEX_INPUT);
    assert!(ops(p).is_empty());
}

#[test]
fn image_trail_first_frame_clears_and_uses_full_alpha() {
    let inp = ImageTrailIn {
        amount: 0.3,
        frame_time: 1.0 / 60.0,
        program_ok: true,
    };
    let mut clear = true;
    let first = ops(plan_image_trail(&inp, &mut clear));
    assert!(!clear);
    assert!(first.contains(&Op::ClearColor));
    assert!(first.contains(&Op::Float {
        prog: 0,
        id: 0,
        v: 1.0
    }));
    let second = ops(plan_image_trail(&inp, &mut clear));
    assert!(!second.contains(&Op::ClearColor));
    let want = (-(60.0f32 * 0.3) * 0.015).exp();
    assert!(
        second
            .iter()
            .any(|o| matches!(o, Op::Float { v, .. } if (v - want).abs() < 1e-6))
    );
}

#[test]
fn color_conv_fade_picks_program_and_clamps_alpha() {
    let mk = |fade_alpha| ColorConvIn {
        fade_alpha,
        programs_ok: [true, true],
    };
    let plain = ops(plan_color_conv(&mk(1.0)));
    assert!(plain.contains(&Op::Program {
        prog: color_conv::PROG_PLAIN
    }));
    assert!(!plain.iter().any(|o| matches!(o, Op::Float { .. })));
    let neg = ops(plan_color_conv(&mk(-0.5)));
    assert!(neg.contains(&Op::Float {
        prog: color_conv::PROG_FADE,
        id: 0,
        v: 0.0
    }));
}

#[test]
fn radial_blur_uniforms_scale_with_target_width() {
    let p = plan_radial_blur(&RadialBlurIn {
        size: 0.5,
        blur_start_dist: 0.25,
        target_size: Vector2l { x: 800, y: 600 },
        program_ok: true,
    });
    assert_eq!(p.result, radial_blur::TEX_FINAL);
    let o = ops(p);
    assert!(o.contains(&Op::Float {
        prog: 0,
        id: 0,
        v: 400.0
    }));
    assert!(o.contains(&Op::Vec2 {
        prog: 0,
        id: 1,
        v: [400.0, 300.0]
    }));
}

#[test]
fn final_copy_draws_to_render_target() {
    let o = ops(plan_final_copy());
    assert_eq!(o.len(), 5);
    assert_eq!(o[2], Op::CompFbPos { fb: 0 });
}
