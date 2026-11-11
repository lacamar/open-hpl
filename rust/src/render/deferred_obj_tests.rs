//! Unit tests of the part E plans: object passes, sky box, translucent and world reflection.

use super::deferred::*;
use super::deferred_fx::x;
use super::deferred_obj::*;
use crate::math::IDENTITY;
use crate::types::Vector3f;

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

fn id(o: &Op) -> Option<i32> {
    match o {
        Op::P4(p) => Some(p.id),
        _ => None,
    }
}

fn ids(v: &[Op]) -> Vec<i32> {
    v.iter().filter_map(id).collect()
}

fn count(v: &[Op], i: i32) -> usize {
    ids(v).iter().filter(|x| **x == i).count()
}

fn p4_of(o: &Op) -> P4 {
    match o {
        Op::P4(p) => *p,
        _ => P4::default(),
    }
}

#[test]
fn z_and_gbuffer_loops() {
    let v = run(plan_z(3), |_| None);
    assert_eq!(count(&v, y::OBJ_Z), 3);
    assert_eq!(id(&v[0]), Some(x::PASS_BEGIN));
    assert_eq!(id(v.last().unwrap()), Some(x::PASS_END));
    assert_eq!(
        run(plan_z(-2), |_| None).len(),
        2 + pass_ops(Pass::ZBegin).len()
    );

    let v = run(plan_gbuffer(2), |_| None);
    for i in [
        y::OBJ_PROGRAM,
        y::OBJ_TEXTURES,
        y::OBJ_MATRIX_PTR,
        y::OBJ_VBUF,
        y::OBJ_DRAW_MAT,
    ] {
        assert_eq!(count(&v, i), 2);
    }
    assert!(matches!(
        v[v.len() - 2],
        Op::State {
            which: State::DepthFunc,
            ..
        }
    ));
}

#[test]
fn decals_skip_when_empty_and_ask_blend() {
    let none = DecalIn {
        count: 0,
        depth_in_normal_alpha: false,
    };
    assert!(run(plan_decals(none), |_| None).is_empty());
    let on = DecalIn {
        count: 2,
        depth_in_normal_alpha: true,
    };
    let v = run(plan_decals(on), |o| {
        (id(o) == Some(y::ASK_OBJ_BLEND)).then_some(Feed::Int(6))
    });
    let raws: Vec<_> = v
        .iter()
        .filter(|o| id(o) == Some(x::BLEND_RAW))
        .map(|o| p4_of(o).a)
        .collect();
    assert_eq!(raws, vec![6, 6]);
    assert!(v.contains(&state(State::Channel, st::CHANNEL_RGB)));
    let off = DecalIn {
        count: 1,
        depth_in_normal_alpha: false,
    };
    let v = run(plan_decals(off), |_| Some(Feed::Int(0)));
    assert!(v.contains(&state(State::Channel, st::CHANNEL_RGBA)));
}

#[test]
fn illumination_binds_unit_zero() {
    assert!(run(plan_illumination(0), |_| None).is_empty());
    let v = run(plan_illumination(2), |_| None);
    assert_eq!(count(&v, y::OBJ_TEX_UNIT), 2);
    assert_eq!(
        v.iter()
            .filter(|o| matches!(o, Op::TexRangeNull { first: 1 }))
            .count(),
        1
    );
}

#[test]
fn skybox_matrix_scale_and_translation() {
    let inactive = SkyIn {
        world_ok: true,
        active: false,
        ..SkyIn::default()
    };
    assert!(run(plan_skybox(inactive), |_| None).is_empty());
    let sky = SkyIn {
        world_ok: true,
        active: true,
        far_plane: 30.0,
        origin: Vector3f {
            x: 1.0,
            y: 2.0,
            z: 3.0,
        },
        ..SkyIn::default()
    };
    let mut p = plan_skybox(sky);
    let mut arr = None;
    while let Some(o) = p.next_op() {
        if id(&o) == Some(y::SKY_MATRIX) {
            arr = Some(p4_of(&o).a);
        }
    }
    let m = p.array(arr.unwrap()).to_vec();
    let side = (900.0f32 / 3.0).sqrt() * 0.95;
    assert_eq!(m[0], side);
    assert_eq!(m[5], side);
    assert_eq!((m[3], m[7], m[11]), (1.0, 2.0, 3.0));
}

#[test]
fn content_passes_draw_every_gbuffer_texture() {
    let n = |t| {
        count(
            &run(plan_gbuffer_content([8.0, 4.0], t), |_| None),
            y::QUAD2,
        )
    };
    assert_eq!((n(3), n(4), n(5)), (3, 4, 4));
    let v = run(plan_reflection_content([8.0, 4.0], 2), |_| None);
    let q = v
        .iter()
        .find(|o| id(o) == Some(y::QUAD2))
        .map(p4_of)
        .unwrap();
    assert_eq!(&q.f[4..6], &[4.0, 2.0]);
}

fn cam() -> CameraIn {
    let mut c = CameraIn::default();
    c.f.view = IDENTITY;
    c.f.proj = IDENTITY;
    c.f.perspective = true;
    c.f.fov = 1.0;
    c.f.near = 0.1;
    c.f.aspect = 1.5;
    c.far_plane = 100.0;
    c.forward = Vector3f {
        x: 0.0,
        y: 0.0,
        z: 1.0,
    };
    c
}

fn trans(count: i32) -> TransIn {
    TransIn {
        count,
        debug_skip: -1,
        underwater: true,
        fog_active: true,
        rt_size: crate::types::Vector2l { x: 640, y: 480 },
        camera: cam(),
        ..TransIn::default()
    }
}

#[derive(Clone, Default)]
struct Obj {
    dof: bool,
    refraction: bool,
    reflection: bool,
    fog: bool,
    illum: bool,
    edge: bool,
    render_type: u32,
    update: bool,
}

fn answer(objs: Vec<Obj>) -> impl Fn(&Op) -> Option<Feed> {
    move |o| {
        let p = p4_of(o);
        let obj = |i: i32| objs.get(i as usize).cloned().unwrap_or_default();
        Some(match id(o)? {
            y::ASK_WORLD_FOG => Feed::Int(7),
            y::ASK_COMPILED_FOG => Feed::Int(7),
            y::ASK_TRANS_INFO => {
                let b = obj(p.a);
                let mut w = vec![0u32; info::WORDS];
                for (k, v) in [
                    (info::AFFECTED_BY_DOF, b.dof),
                    (info::HAS_REFRACTION, b.refraction),
                    (info::HAS_WORLD_REFLECTION, b.reflection),
                    (info::AFFECTED_BY_FOG, b.fog),
                    (info::HAS_TRANSLUCENT_ILLUMINATION, b.illum),
                    (info::USE_REFRACTION_EDGE_CHECK, b.edge),
                    (info::DEPTH_TEST, true),
                ] {
                    w[k] = u32::from(v);
                }
                w[info::BLEND] = 4;
                w[info::RENDER_TYPE] = b.render_type;
                w[info::BV_CENTER + 2] = (-10.0f32).to_bits();
                w[info::BV_RADIUS] = 1.0f32.to_bits();
                Feed::Words(w)
            }
            y::ASK_FOG_VIS => Feed::Floats(vec![0.5, 1.0, 1.0, 1.0, 1.0, 2.0]),
            y::ASK_UPDATE_GRAPHICS => Feed::Bool(obj(p.a).update),
            y::ASK_RETRIEVE_QUERY | y::ASK_PLANE_VISIBLE => Feed::Bool(true),
            y::ASK_CLIP_RECT_OBJ => Feed::Words(vec![10, 20, 30, 40]),
            y::ASK_BV_MIN_MAX => Feed::Floats(vec![-100.0, -100.0, -100.0, 100.0, 100.0, -0.01]),
            y::ASK_REFL_INFO => Feed::Floats(vec![0.0, 1.0, 1.0]),
            y::ASK_REFL_CLEARED => Feed::Bool(false),
            y::ASK_REFL_TARGET => Feed::Words(vec![0, 0, 640, 480, 0, 0, 0, 640, 480, 2, 320, 240]),
            y::ASK_REFL_SURFACE => {
                let mut f = vec![0.0; 28];
                for (i, v) in [
                    (0, 1.0),
                    (5, 1.0),
                    (10, 1.0),
                    (15, 1.0),
                    (18, 1.0),
                    (22, -1.0),
                    (23, -1.0),
                    (24, -11.0),
                    (25, 1.0),
                    (26, 1.0),
                    (27, -9.0),
                ] {
                    f[i] = v;
                }
                Feed::Floats(f)
            }
            _ => return None,
        })
    }
}

fn live() -> Obj {
    Obj {
        update: true,
        fog: true,
        ..Obj::default()
    }
}

#[test]
fn translucent_empty_list_has_no_pass() {
    assert!(run(plan_translucent(trans(0)), answer(vec![])).is_empty());
}

#[test]
fn translucent_plain_object_draws_once() {
    let v = run(plan_translucent(trans(1)), answer(vec![live()]));
    assert_eq!(count(&v, y::OBJ_DRAW_MAT), 1);
    assert_eq!(count(&v, y::OBJ_COMPILE), 0);
    let draw = v
        .iter()
        .find(|o| id(o) == Some(y::OBJ_DRAW_MAT))
        .map(p4_of)
        .unwrap();
    assert_eq!(draw.a, mode::DIFFUSE_FOG);
    let mut i = trans(1);
    i.fog_active = true;
    let mut p = plan_translucent(i);
    let mut modes = Vec::new();
    while let Some(o) = p.next_op() {
        if let Some(f) = answer(vec![live()])(&o) {
            p.feed(f);
        }
        if id(&o) == Some(y::OBJ_PROGRAM) {
            modes.push(p4_of(&o).a);
        }
    }
    assert_eq!(modes, vec![mode::DIFFUSE_FOG]);
    let unfogged = Obj {
        fog: false,
        ..live()
    };
    let v = run(plan_translucent(trans(1)), answer(vec![unfogged]));
    let draw = v
        .iter()
        .find(|o| id(o) == Some(y::OBJ_DRAW_MAT))
        .map(p4_of)
        .unwrap();
    assert_eq!(draw.a, mode::DIFFUSE);
}

#[test]
fn translucent_stale_compile_and_debug_skip() {
    let v = run(plan_translucent(trans(1)), |o| {
        if id(o) == Some(y::ASK_COMPILED_FOG) {
            return Some(Feed::Int(3));
        }
        answer(vec![live()])(o)
    });
    assert_eq!(count(&v, y::OBJ_COMPILE), 1);
    let mut i = trans(2);
    i.debug_skip = 0;
    let v = run(plan_translucent(i), answer(vec![live(), live()]));
    assert_eq!(count(&v, y::OBJ_DRAW_MAT), 1);
}

#[test]
fn translucent_failed_update_skips_the_draw() {
    let dead = Obj {
        update: false,
        ..live()
    };
    let v = run(plan_translucent(trans(2)), answer(vec![dead, live()]));
    assert_eq!(count(&v, y::OBJ_DRAW_MAT), 1);
    assert_eq!(count(&v, y::OBJ_MODEL_MATRIX), 1);
}

#[test]
fn translucent_fog_areas_blend_alpha_or_colour() {
    let temp = |hdr: bool| {
        let mut i = trans(1);
        i.fog_area_num = 2;
        i.hdr = hdr;
        let v = run(plan_translucent(i), answer(vec![live()]));
        v.iter()
            .find(|o| id(o) == Some(y::SET_TEMP_FOG))
            .map(p4_of)
            .unwrap()
            .f
    };
    let sdr = temp(false);
    assert_eq!(&sdr[..5], &[0.25, 0.0, 0.0, 0.0, 0.0]);
    let hdr = temp(true);
    assert_eq!(hdr[0], 1.0);
    assert!(hdr[4] > 0.0);
}

#[test]
fn translucent_depth_of_field_gate() {
    let draws = |pass: i32, o: Obj| {
        let mut i = trans(1);
        i.dof_pass = pass;
        i.dof_start = 1.0;
        i.dof_end = 5.0;
        i.dof_falloff = 1.0;
        count(&run(plan_translucent(i), answer(vec![o])), y::OBJ_DRAW_MAT)
    };
    // an unaffected object only draws in the focused pass
    let plain = live();
    assert_eq!(
        (
            draws(0, plain.clone()),
            draws(1, plain.clone()),
            draws(2, plain)
        ),
        (1, 0, 1)
    );
    // an object far behind the focus end only draws in the blurred pass
    let far = Obj {
        dof: true,
        ..live()
    };
    assert_eq!((draws(1, far.clone()), draws(2, far)), (1, 0));
}

#[test]
fn translucent_refraction_copies_and_reuses_the_frame() {
    let refr = Obj {
        refraction: true,
        render_type: 3,
        ..live()
    };
    let v = run(
        plan_translucent(trans(2)),
        answer(vec![refr.clone(), refr.clone()]),
    );
    // a volume filling the view leaves a whole-target drawn rect behind
    assert_eq!(count(&v, y::COPY_REFRACTION_FULL), 2);
    let edge = Obj { edge: true, ..refr };
    let v = run(plan_translucent(trans(1)), answer(vec![edge]));
    assert_eq!(count(&v, y::COPY_REFRACTION_RECT), 1);
    assert_eq!(count(&v, y::QUAD_CLIP), 1);
}

#[test]
fn translucent_world_reflection_runs_a_nested_render() {
    let sub = Obj {
        reflection: true,
        render_type: enums::RENDERABLE_SUB_MESH,
        ..live()
    };
    let mut i = trans(1);
    i.render_world_reflection = true;
    let v = run(plan_translucent(i), answer(vec![sub.clone()]));
    for k in [
        y::REFL_BEGIN,
        y::REFL_FRUSTUM,
        y::REFL_RENDER,
        y::REFL_END,
        y::WAIT_RETRIEVE_QUERIES,
    ] {
        assert_eq!(count(&v, k), 1, "{k}");
    }
    let r = ids(&v).iter().position(|k| *k == y::REFL_RENDER).unwrap();
    let e = ids(&v).iter().position(|k| *k == y::REFL_END).unwrap();
    assert!(r < e);
    // reflections are not drawn inside a reflection
    let mut inner = trans(1);
    inner.is_reflection = true;
    let v = run(plan_translucent(inner), answer(vec![sub.clone()]));
    assert_eq!(count(&v, y::OBJ_DRAW_MAT), 0);
    // without world reflection support the texture is cleared once
    let mut off = trans(1);
    off.render_world_reflection = false;
    let v = run(plan_translucent(off), answer(vec![sub]));
    assert_eq!(count(&v, y::REFL_BEGIN), 0);
    assert_eq!(count(&v, y::REFL_SET_CLEARED), 1);
}

#[test]
fn rect_intersection_matches_the_c_edge_cases() {
    assert!(rect_intersects([0, 0, 4, 4], [3, 3, 4, 4]));
    assert!(!rect_intersects([0, 0, 4, 4], [4, 0, 4, 4]));
    // negative sizes are flipped around their origin
    assert!(rect_intersects([4, 4, -4, -4], [0, 0, 1, 1]));
    assert!(!rect_intersects([4, 4, -4, -4], [4, 4, 1, 1]));
    // empty rects never intersect what lies outside them
    assert!(!rect_intersects([0, 0, 0, 0], [5, 5, 1, 1]));
}

#[test]
fn refractive_particles_do_not_invalidate_the_copy() {
    let particle = Obj {
        refraction: true,
        render_type: enums::RENDERABLE_PARTICLE_EMITTER,
        ..live()
    };
    let v = run(
        plan_translucent(trans(2)),
        answer(vec![particle.clone(), particle]),
    );
    assert_eq!(count(&v, y::COPY_REFRACTION_FULL), 1);
}

fn max_dist_planes(normal_y: f32) -> Vec<[f32; 4]> {
    let sub = Obj {
        reflection: true,
        render_type: enums::RENDERABLE_SUB_MESH,
        ..live()
    };
    let mut i = trans(1);
    i.render_world_reflection = true;
    let base = answer(vec![sub]);
    let v = run(plan_translucent(i), |o| match id(o) {
        Some(y::ASK_REFL_INFO) => Some(Feed::Floats(vec![5.0, 1.0, 1.0])),
        Some(y::ASK_PLANE_BV) => Some(Feed::Int(0)),
        Some(y::ASK_REFL_SURFACE) => {
            let mut f = vec![0.0; 28];
            for (k, v) in [
                (0, 1.0),
                (5, 1.0),
                (10, 1.0),
                (15, 1.0),
                (17, normal_y),
                (18, 1.0),
            ] {
                f[k] = v;
            }
            Some(Feed::Floats(f))
        }
        _ => base(o),
    });
    v.iter()
        .filter(|o| id(o) == Some(y::REFL_ADD_PLANE))
        .map(|o| {
            let f = p4_of(o).f;
            [f[0], f[1], f[2], f[3]]
        })
        .collect()
}

#[test]
fn max_reflection_distance_plane_has_an_aligned_special_case() {
    let aligned = max_dist_planes(0.0);
    assert!(!aligned.is_empty());
    let tilted = max_dist_planes(0.5);
    assert_eq!(tilted.len(), aligned.len());
    assert_ne!(tilted.first(), aligned.first());
}
