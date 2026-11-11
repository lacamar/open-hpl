//! Unit tests of the part D plans: fog, depth of field, night vision, HPL3 SSAO upsample,
//! directional and weighted box lights.

use super::deferred::*;
use super::deferred_box::plan_box_weighted;
use super::deferred_dir::{BoxIn, BoxLightIn, DirIn, plan_directional};
use super::deferred_fog::{FogAreaIn, FogParams, FogWorld, plan_fog, plan_full_screen_fog};
use super::deferred_fx::*;
use crate::math::IDENTITY;

fn run(mut p: Plan, answer: impl Fn(&Op) -> Option<Feed>) -> Vec<Op> {
    let mut v = Vec::new();
    while let Some(o) = p.next_op() {
        if let Some(f) = answer(&o) {
            p.feed(f);
        }
        v.push(o);
    }
    v
}

fn no_answers(_: &Op) -> Option<Feed> {
    None
}

fn is_p4(o: &Op, id: i32) -> bool {
    matches!(o, Op::P4(p) if p.id == id)
}

fn count_p4(v: &[Op], id: i32) -> usize {
    v.iter().filter(|o| is_p4(o, id)).count()
}

fn world() -> FogWorld {
    FogWorld {
        fog_active: 1,
        area_num: 1,
        frustum_far: 100.0,
        view: IDENTITY,
        inv_view: IDENTITY,
        ..FogWorld::default()
    }
}

fn params() -> FogParams {
    FogParams {
        start: 1.0,
        end: 9.0,
        falloff: 2.0,
        color: [0.5, 0.25, 1.0, 1.0],
        brightness: 1.0,
        noise_size: 1.0,
        ..FogParams::default()
    }
}

#[test]
fn fog_depth_texture_modes() {
    let ops = |din, bind| run(plan_fog_depth(din, 2, bind, 3), no_answers);
    assert_eq!(ops(false, true), vec![p4(x::TEX_GBUFFER, 3, 2)]);
    assert!(ops(false, false).is_empty());
    assert_eq!(
        ops(true, true),
        vec![p4(x::RED_FROM_ALPHA, 1, 0), p4(x::TEX_GBUFFER, 3, 1)]
    );
    assert_eq!(ops(true, false), vec![p4(x::RED_FROM_ALPHA, 0, 0)]);
}

#[test]
fn night_vision_needs_program_and_visibility() {
    let i = NightIn {
        program_ok: 1,
        visible: 1,
        color: [2.0, 3.0, 4.0, 0.5],
        brightness: 0.5,
        ..NightIn::default()
    };
    let hidden = [NightIn { program_ok: 0, ..i }, NightIn { visible: 0, ..i }];
    for h in hidden {
        assert!(run(plan_night_vision(&h), no_answers).is_empty());
    }
    let v = run(plan_night_vision(&i), no_answers);
    assert!(v.contains(&Op::Vec4 {
        prog: nslot::NV_PROG,
        id: v4::AV_LIGHT_COLOR,
        v: [2.0, 4.5, 8.0, 0.5],
    }));
    assert_eq!(v.last(), Some(&state(State::DepthTest, 1)));
}

#[test]
fn hpl3_upsample_halves_texel_and_skips_when_not_rendered() {
    let i = Hpl3In {
        rendered: 0,
        ..Hpl3In::default()
    };
    assert!(run(plan_apply_hpl3_ssao(&i), no_answers).is_empty());
    let i = Hpl3In {
        rendered: 1,
        far_plane: 7.0,
        size_div: 4,
        tex_w: 8,
        tex_h: 16,
        ..Hpl3In::default()
    };
    let v = run(plan_apply_hpl3_ssao(&i), no_answers);
    assert!(v.contains(&Op::Float {
        prog: hslot::UPSAMPLE_PROG,
        id: v4::AF_SIZE_DIV,
        v: 0.25,
    }));
    assert!(v.contains(&Op::Quad {
        min: [-0.0625, -0.03125],
        max: [0.9375, 0.96875],
    }));
}

#[test]
fn dof_asks_first_and_draws_three_passes() {
    let i = DofIn {
        screen_w: 64,
        screen_h: 32,
        screen_fx: 64.0,
        screen_fy: 32.0,
        focus_start: 2.0,
        focus_end: 6.0,
        falloff: 1.0,
        near_plane: 0.5,
        far_plane: 50.0,
        ..DofIn::default()
    };
    let off = run(plan_depth_of_field(i), |o| {
        matches!(o, Op::Ask(Query::DepthOfField)).then_some(Feed::Bool(false))
    });
    assert_eq!(off, vec![Op::Ask(Query::DepthOfField)]);
    let on = run(plan_depth_of_field(i), |o| {
        matches!(o, Op::Ask(Query::DepthOfField)).then_some(Feed::Bool(true))
    });
    assert_eq!(count_p4(&on, x::PASS_BEGIN), 1);
    assert_eq!(count_p4(&on, x::PASS_END), 1);
    let quads = on.iter().filter(|o| matches!(o, Op::Quad { .. })).count();
    assert_eq!(quads, 2);
    assert_eq!(count_p4(&on, x::DRAW_ACCUM_QUAD), 1);
    assert!(on.contains(&Op::Vec4 {
        prog: dslot::FOCUS_PROG,
        id: v4::AV_FOCUS_START_END,
        v: [1.5, 2.5, 6.0, 8.0],
    }));
    assert_eq!(
        count_p4(&on, x::FILTER_SAVE),
        count_p4(&on, x::FILTER_RESTORE)
    );
}

fn fog_answers(program_ok: bool, noise: bool) -> impl Fn(&Op) -> Option<Feed> {
    move |o| match o {
        Op::P4(p) if p.id == x::GEN_FOG_PROGRAM => Some(Feed::Bool(program_ok)),
        Op::P4(p) if p.id == x::ENSURE_FOG_NOISE => Some(Feed::Bool(noise)),
        _ => None,
    }
}

#[test]
fn full_screen_fog_underwater_draws_two_passes() {
    let mut w = world();
    let off = run(
        plan_full_screen_fog(FogWorld { fog_active: 0, ..w }, params()),
        no_answers,
    );
    assert!(off.is_empty());
    let one = run(plan_full_screen_fog(w, params()), fog_answers(true, false));
    assert_eq!(count_p4(&one, x::QUAD_PLAIN), 1);
    w.fog_underwater = 1;
    let two = run(plan_full_screen_fog(w, params()), fog_answers(true, false));
    assert_eq!(count_p4(&two, x::QUAD_PLAIN), 2);
    let none = run(plan_full_screen_fog(w, params()), fog_answers(false, false));
    assert_eq!(count_p4(&none, x::QUAD_PLAIN), 0);
    assert_eq!(count_p4(&none, x::GEN_FOG_PROGRAM), 2);
}

#[test]
fn fog_program_flags_follow_the_world() {
    let flags = |w: FogWorld, p: FogParams| {
        run(plan_full_screen_fog(w, p), fog_answers(true, true))
            .iter()
            .find_map(|o| match o {
                Op::P4(p) if p.id == x::GEN_FOG_PROGRAM => Some(p.b as u32),
                _ => None,
            })
            .unwrap_or(u32::MAX)
    };
    use super::deferred_fog::fogf;
    assert_eq!(flags(world(), params()), 0);
    let noisy = FogParams {
        noise_strength: 1.0,
        ..params()
    };
    assert_eq!(flags(world(), noisy), fogf::NOISE);
    let sky = FogWorld {
        sky_tex: 1,
        sky_active: 1,
        sky_separate: 1,
        sec_active: 1,
        ..world()
    };
    let wants = FogParams {
        skybox_wanted: 1,
        lighten: 1,
        ..params()
    };
    assert_eq!(
        flags(sky, wants),
        fogf::SKYBOX | fogf::SEPARATE_SKYBOX | fogf::SECONDARY | fogf::LIGHTEN
    );
    let uw = FogWorld {
        fog_underwater: 1,
        ..world()
    };
    assert_eq!(flags(uw, params()), fogf::UNDERWATER);
}

fn area(inside: bool, underwater: bool) -> FogAreaIn {
    FogAreaIn {
        inside_near: i32::from(inside),
        backside_inside: 1,
        underwater: i32::from(underwater),
        params: FogParams {
            is_area: 1,
            ..params()
        },
        model: IDENTITY,
        ..FogAreaIn::default()
    }
}

fn fog_run(w: FogWorld, areas: Vec<FogAreaIn>) -> Vec<Op> {
    run(plan_fog(w), move |o| match o {
        Op::P4(p) if p.id == x::GEN_FOG_PROGRAM => Some(Feed::Bool(true)),
        Op::P4(p) if p.id == x::ASK_FOGS => Some(Feed::Fogs(areas.clone())),
        _ => None,
    })
}

#[test]
fn fog_without_areas_only_clears_the_render_data() {
    let w = FogWorld {
        area_num: 0,
        ..world()
    };
    assert_eq!(
        run(plan_fog(w), no_answers),
        vec![p4(x::CLEAR_FOG_DATA, 0, 0)]
    );
}

#[test]
fn fog_areas_draw_per_pass_and_cast_rays_when_outside() {
    let v = fog_run(world(), vec![area(true, false), area(false, true)]);
    let draws = v
        .iter()
        .filter(|o| matches!(o, Op::Dl(DlOp::DrawCurrent)))
        .count();
    assert_eq!(draws, 3);
    let rays = v
        .iter()
        .filter(|o| matches!(o, Op::Vec3 { id, .. } if *id == v4::AV_RAY_CAST_START))
        .count();
    assert_eq!(rays, 2);
    assert_eq!(count_p4(&v, x::FOG_AREA_MATRIX), 3);
    assert_eq!(count_p4(&v, x::PASS_BEGIN), 1);
    assert_eq!(count_p4(&v, x::PASS_END), 1);
}

#[test]
fn fog_ray_start_is_the_camera_in_box_space() {
    let mut a = area(false, false);
    a.model.m[0][3] = 4.0;
    let v = fog_run(world(), vec![a]);
    let start = v.iter().find_map(|o| match o {
        Op::Vec3 { id, v, .. } if *id == v4::AV_RAY_CAST_START => Some(*v),
        _ => None,
    });
    assert_eq!(start, Some([-4.0, 0.0, 0.0]));
}

fn dir_in() -> DirIn {
    DirIn {
        active: 1,
        brightness: 1.0,
        diffuse: [1.0; 4],
        cast_specular: 1,
        cast_diffuse: 1,
        cast_shadows: 1,
        render_shadows: 1,
        direction: [0.0, -1.0, 0.0],
        frustum_far: 100.0,
        frustum_near: 0.5,
        fov: 1.0,
        aspect: 1.5,
        view: IDENTITY,
        inv_view: IDENTITY,
        shadow_distance: -1.0,
        slice_log_term: 0.5,
        caster_distance: 10.0,
        bias_mul: 1.0,
        slope_bias_mul: 1.0,
        casters_affected: 3,
        gbuffer_textures: 3,
        ..DirIn::default()
    }
}

fn dir_run(i: DirIn, program_ok: bool, slice_ok: bool) -> Vec<Op> {
    run(plan_directional(i), move |o| match o {
        Op::GenProgram { .. } => Some(Feed::Bool(program_ok)),
        Op::P4(p) if p.id == x::ASK_DIR_SHADOW_SIZE => Some(Feed::Vec3([512.0, 512.0, 0.0])),
        Op::P4(p) if p.id == x::INT_ASK => Some(Feed::Bool(slice_ok)),
        _ => None,
    })
}

#[test]
fn directional_light_is_skipped_when_inactive_or_dark() {
    for i in [
        DirIn {
            active: 0,
            ..dir_in()
        },
        DirIn {
            brightness: 0.0,
            ..dir_in()
        },
        DirIn {
            brightness: -1.0,
            ..dir_in()
        },
    ] {
        assert!(dir_run(i, true, true).is_empty());
    }
}

#[test]
fn directional_shadow_renders_four_slices_before_the_program() {
    let v = dir_run(dir_in(), true, true);
    assert_eq!(count_p4(&v, x::DIR_SHADOW_CREATE), 1);
    assert_eq!(count_p4(&v, x::DIR_FRUSTUM), 4);
    assert_eq!(count_p4(&v, x::DIR_CASTERS), 4);
    assert_eq!(count_p4(&v, x::DIR_RENDER), 4);
    let program = v.iter().position(|o| matches!(o, Op::GenProgram { .. }));
    let last_render = v.iter().rposition(|o| is_p4(o, x::DIR_RENDER));
    assert!(program > last_render);
    assert_eq!(count_p4(&v, x::TEX_BUFFER), 3);
    let draws = v
        .iter()
        .filter(|o| matches!(o, Op::Dl(DlOp::DrawCurrent)))
        .count();
    assert_eq!(draws, 1);
    let existing = dir_run(
        DirIn {
            shadow_exists: 1,
            ..dir_in()
        },
        true,
        true,
    );
    assert_eq!(count_p4(&existing, x::DIR_SHADOW_CREATE), 0);
}

#[test]
fn directional_without_shadows_or_program() {
    let plain = dir_run(
        DirIn {
            render_shadows: 0,
            ..dir_in()
        },
        true,
        true,
    );
    assert_eq!(count_p4(&plain, x::DIR_FRUSTUM), 0);
    assert!(
        plain
            .iter()
            .any(|o| matches!(o, Op::GenProgram { flags, .. } if *flags == (1 << 12) | (1 << 2)))
    );
    let missing = dir_run(
        DirIn {
            render_shadows: 0,
            ..dir_in()
        },
        false,
        true,
    );
    assert_eq!(missing.len(), 1);
}

#[test]
fn directional_slice_uniforms_only_when_the_program_has_them() {
    let has = |ok| {
        dir_run(dir_in(), true, ok).iter().any(
            |o| matches!(o, Op::Float { id, .. } if *id == super::deferred_dir::v5::AF_SLICE_DIV),
        )
    };
    assert!(has(true));
    assert!(!has(false));
}

fn box_light(id: u64, blend: i32, prio: i32) -> BoxLightIn {
    BoxLightIn {
        id,
        idx: id as i32,
        blend_func: blend,
        prio,
        brightness: 1.0,
        fade: 1.0,
        diffuse: [1.0; 4],
        ..BoxLightIn::default()
    }
}

fn box_in() -> BoxIn {
    BoxIn {
        buffer_ok: 1,
        resolve_ok: 1,
        prog_ok: 0x3f,
        gbuffer_textures: 2,
        view: IDENTITY,
        ..BoxIn::default()
    }
}

fn model_views(v: &[Op]) -> Vec<i32> {
    v.iter()
        .filter_map(|o| match o {
            Op::Dl(DlOp::SetModelView(i)) => Some(*i),
            _ => None,
        })
        .collect()
}

#[test]
fn box_weighted_handled_flag() {
    for (i, handled) in [
        (box_in(), true),
        (
            BoxIn {
                buffer_ok: 0,
                ..box_in()
            },
            false,
        ),
        (
            BoxIn {
                resolve_ok: 0,
                ..box_in()
            },
            false,
        ),
        (
            BoxIn {
                is_reflection: 1,
                ..box_in()
            },
            false,
        ),
    ] {
        let p = plan_box_weighted(&i, &[box_light(1, 0, 0)]);
        assert_eq!(p.res.handled, handled);
    }
    let empty = plan_box_weighted(&box_in(), &[]);
    assert!(empty.res.handled);
    assert!(run(empty, no_answers).is_empty());
}

#[test]
fn box_weighted_sorts_by_blend_rank_priority_then_id() {
    let lights = [
        box_light(9, 2, 0),
        box_light(8, 1, 0),
        box_light(7, 0, 5),
        box_light(6, 0, 1),
        box_light(5, 0, 1),
    ];
    let v = run(plan_box_weighted(&box_in(), &lights), no_answers);
    assert_eq!(model_views(&v), vec![5, 6, 7, 9, 8]);
}

#[test]
fn box_weighted_skips_lights_without_a_program() {
    let mut i = box_in();
    i.prog_ok = 0b11_1110;
    let lights = [box_light(1, 0, 0), box_light(2, 0, 0)];
    let mut l2 = lights;
    l2[1].spherical = 1;
    let v = run(plan_box_weighted(&i, &l2), no_answers);
    assert_eq!(model_views(&v), vec![2]);
}

#[test]
fn box_weighted_underwater_tints_only_the_flat_lights() {
    let mut i = box_in();
    i.underwater = 1;
    i.fog_color = [2.0, 2.0, 2.0, 1.0];
    let mut l = box_light(1, 0, 0);
    l.ambient_sky = [1.0; 4];
    let v = run(plan_box_weighted(&i, &[l]), no_answers);
    let sky = v.iter().find_map(|o| match o {
        Op::Vec3 { id, v, .. } if *id == super::deferred_dir::v5::AV_AMBIENT_COLOR_SKY => Some(*v),
        _ => None,
    });
    let tinted = 4.0 * (1.0f32 / 12.0);
    assert_eq!(sky, Some([tinted; 3]));
    l.spherical = 1;
    let v = run(plan_box_weighted(&i, &[l]), no_answers);
    let sky = v.iter().find_map(|o| match o {
        Op::Vec3 { id, v, .. } if *id == super::deferred_dir::v5::AV_AMBIENT_COLOR_SKY => Some(*v),
        _ => None,
    });
    assert_eq!(sky, Some([1.0, 1.0, 1.0]));
}

use super::deferred_misc::{
    EnvIn, FlareUpdateIn, GlareFlare, GlareIn, IrisFlare, plan_environment_particles,
    plan_multi_iris_and_glare, plan_update_lens_flares,
};

fn env_in() -> EnvIn {
    EnvIn {
        active: 1,
        num: 2,
        behind_focus: 0,
        focus_start: 1.0,
        focus_end: 3.0,
        falloff: 1.0,
        proj: IDENTITY,
    }
}

fn env_run(i: EnvIn, dof: bool, systems: [[f32; 3]; 2]) -> Vec<Op> {
    run(plan_environment_particles(i), move |o| match o {
        Op::Ask(Query::DepthOfField) => Some(Feed::Bool(dof)),
        Op::P4(p) if p.id == x::ENV_VISIBLE => Some(Feed::Vec3(
            systems.get(p.a as usize).copied().unwrap_or([0.0; 3]),
        )),
        _ => None,
    })
}

#[test]
fn environment_particles_early_outs() {
    let none = [[0.0; 3]; 2];
    assert!(
        env_run(
            EnvIn {
                active: 0,
                ..env_in()
            },
            false,
            none
        )
        .is_empty()
    );
    assert!(env_run(EnvIn { num: 0, ..env_in() }, false, none).is_empty());
    let skipped = env_run(env_in(), false, none);
    assert_eq!(skipped, vec![Op::Ask(Query::DepthOfField)]);
}

#[test]
fn environment_particles_draw_each_iteration_of_visible_systems() {
    let v = env_run(
        EnvIn {
            behind_focus: 1,
            ..env_in()
        },
        false,
        [[1.0, 1.0, 3.0], [1.0, 0.0, 2.0]],
    );
    assert_eq!(count_p4(&v, x::ENV_ITERATION), 3);
    assert_eq!(count_p4(&v, x::ENV_SETUP_BASE), 1);
    let setup = v.iter().find_map(|o| match o {
        Op::P4(p) if p.id == x::ENV_SETUP_BASE => Some(p.f),
        _ => None,
    });
    assert_eq!(
        setup.map(|f| [f[0], f[1], f[2], f[3]]),
        Some([0.0, 0.0, 0.0, 1.0])
    );
    assert_eq!(v.last(), Some(&p4(x::SET_VERTEX_BUFFER_NONE, 0, 0)));
}

#[test]
fn environment_particles_dof_params_follow_the_projection() {
    let v = env_run(env_in(), true, [[1.0, 1.0, 1.0], [0.0; 3]]);
    let f = v.iter().find_map(|o| match o {
        Op::P4(p) if p.id == x::ENV_SETUP_BASE => Some(p.f),
        _ => None,
    });
    let f = f.unwrap_or_default();
    assert_eq!([f[0], f[2], f[3]], [-3.0, 1.0, -1.0]);
    assert_eq!(f[1], 1.0 / ((-1.0 - -3.0) * 0.5));
}

#[test]
fn lens_flare_update_skips_reflections() {
    let i = FlareUpdateIn {
        is_reflection: 1,
        count: 3,
        frame_time: 0.5,
        frame: 7,
    };
    assert!(run(plan_update_lens_flares(i), no_answers).is_empty());
    let v = run(
        plan_update_lens_flares(FlareUpdateIn {
            is_reflection: 0,
            ..i
        }),
        no_answers,
    );
    assert_eq!(count_p4(&v, x::FLARE_RETRIEVE), 3);
    assert_eq!(count_p4(&v, x::FLARE_UPDATE), 3);
}

fn glare_in() -> GlareIn {
    GlareIn {
        iris_ok: 1,
        glare_ok: 1,
        flare_count: 2,
        multi_count: 1,
        aspect: 1.0,
        ..GlareIn::default()
    }
}

fn glare_run(
    i: GlareIn,
    flares: Vec<GlareFlare>,
    iris: Vec<IrisFlare>,
    color: [f32; 7],
) -> Vec<Op> {
    run(
        plan_multi_iris_and_glare(i, flares, iris, vec![0.5; GLARE_VERTS * 4]),
        move |o| {
            matches!(o, Op::P4(p) if p.id == x::ASK_GLARE).then(|| Feed::Floats(color.to_vec()))
        },
    )
}

#[test]
fn glare_needs_programs_and_flares() {
    for i in [
        GlareIn {
            iris_ok: 0,
            ..glare_in()
        },
        GlareIn {
            glare_ok: 0,
            ..glare_in()
        },
        GlareIn {
            flare_count: 0,
            ..glare_in()
        },
    ] {
        assert!(glare_run(i, Vec::new(), Vec::new(), [0.0; 7]).is_empty());
    }
}

#[test]
fn glare_draws_only_when_the_summed_colour_is_visible() {
    let flares = vec![
        GlareFlare {
            vis: 1.0,
            any_active: 1,
        },
        GlareFlare {
            vis: 0.0,
            any_active: 1,
        },
    ];
    let lit = glare_run(
        glare_in(),
        flares.clone(),
        Vec::new(),
        [1.0, 0.5, 0.25, 1.0, 0.5, 0.5, 0.0],
    );
    assert_eq!(count_p4(&lit, x::ASK_GLARE), 1);
    assert_eq!(count_p4(&lit, x::GLARE_UPDATE_DATA), 1);
    let dark = glare_run(
        glare_in(),
        flares,
        Vec::new(),
        [0.0, 0.0, 0.001, 1.0, 0.5, 0.5, 0.0],
    );
    assert_eq!(count_p4(&dark, x::GLARE_WRITE), 1);
    assert_eq!(count_p4(&dark, x::GLARE_UPDATE_DATA), 0);
}

#[test]
fn glare_iris_skips_missing_textures_and_fully_fogged_flares() {
    let iris = |tex_ok, vis| IrisFlare {
        tex_ok,
        blend: 2,
        vis,
        pos: [0.0, 0.0, -10.0],
    };
    let draws = |i: GlareIn, f: IrisFlare| {
        glare_run(i, Vec::new(), vec![f], [0.0; 7])
            .iter()
            .filter(|o| is_p4(o, x::FLARE_IRIS_BUFFER))
            .count()
    };
    assert_eq!(draws(glare_in(), iris(1, 0.5)), 1);
    assert_eq!(draws(glare_in(), iris(0, 0.5)), 0);
    assert_eq!(draws(glare_in(), iris(1, 0.0)), 0);
    let fogged = GlareIn {
        fog_active: 1,
        fog_start: 0.0,
        fog_end: 5.0,
        fog_falloff: 1.0,
        fog_alpha: 1.0,
        ..glare_in()
    };
    assert_eq!(draws(fogged, iris(1, 0.5)), 0);
}

use super::deferred_ssao3::{Hpl3RenderIn, plan_hpl3_ssao};

fn h3_in() -> Hpl3RenderIn {
    Hpl3RenderIn {
        programs_ok: 1,
        ssao_active: 1,
        size_div: 2,
        fov: 1.0,
        aspect: 1.5,
        far_plane: 100.0,
        tex_w: 64,
        tex_h: 32,
        mip_w: 16,
        mip_h: 8,
        screen_fx: 128.0,
        screen_fy: 64.0,
        mip_buffers: 3,
        time: 255.5,
        first_frame: 1,
        frame_time: 1.0,
        prev_view: IDENTITY,
        view: IDENTITY,
        proj: IDENTITY,
        ..Hpl3RenderIn::default()
    }
}

#[test]
fn hpl3_ssao_is_skipped_without_programs_reflection_or_ssao() {
    for i in [
        Hpl3RenderIn {
            programs_ok: 0,
            ..h3_in()
        },
        Hpl3RenderIn {
            is_reflection: 1,
            ..h3_in()
        },
        Hpl3RenderIn {
            ssao_active: 0,
            ..h3_in()
        },
    ] {
        let p = plan_hpl3_ssao(&i);
        assert_eq!(p.res.h3.rendered, 0);
        assert_eq!(p.res.h3.first_frame, 1);
        assert_eq!(p.res.h3.time, 255.5);
        assert!(run(p, no_answers).is_empty());
    }
}

#[test]
fn hpl3_ssao_draws_mips_render_blurs_and_temporal() {
    let p = plan_hpl3_ssao(&h3_in());
    assert_eq!(p.res.h3.rendered, 1);
    assert_eq!(p.res.h3.first_frame, 0);
    assert_eq!(p.res.h3.time, 0.5);
    let v = run(p, no_answers);
    let quads = v.iter().filter(|o| matches!(o, Op::Quad { .. })).count();
    assert_eq!(quads, 3 + 1 + 2 + 1 + 2);
    assert_eq!(count_p4(&v, x::SET_FB3), 3 + 1 + 2 + 1 + 2);
    assert_eq!(count_p4(&v, x::PASS_BEGIN), 1);
    assert_eq!(count_p4(&v, x::PASS_END), 1);
}

#[test]
fn hpl3_ssao_first_frame_uses_identity_and_extra_blur() {
    let v = run(plan_hpl3_ssao(&h3_in()), no_answers);
    let amount = v.iter().find_map(|o| match o {
        Op::Float { id, v, .. } if *id == super::deferred_ssao3::var::AF_TEMPORAL_BLUR_AMOUNT => {
            Some(*v)
        }
        _ => None,
    });
    assert_eq!(amount, Some(1.0));
    let later = Hpl3RenderIn {
        first_frame: 0,
        frame_time: 0.0,
        ..h3_in()
    };
    let v = run(plan_hpl3_ssao(&later), no_answers);
    let amount = v.iter().find_map(|o| match o {
        Op::Float { id, v, .. } if *id == super::deferred_ssao3::var::AF_TEMPORAL_BLUR_AMOUNT => {
            Some(*v)
        }
        _ => None,
    });
    assert_eq!(amount, Some(1.0 / 60.0));
}
