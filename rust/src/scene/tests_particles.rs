use super::billboard::{self, FIXED_AXIS, ModelIn, POINT};
use super::particles::*;
use crate::math::{IDENTITY, v3};
use crate::types::{Color, Matrixf, Vector2f, Vector2l, Vector3f};
use std::mem::size_of;

const WHITE: Color = Color {
    r: 1.0,
    g: 1.0,
    b: 1.0,
    a: 1.0,
};

fn params() -> Params {
    Params {
        draw_type: DRAW_DYNAMIC_POINT,
        particles_per_second: 10.0,
        min_life_span: 1.0,
        max_life_span: 1.0,
        collision_update_rate: 10,
        min_start_size: Vector2f { x: 1.0, y: 1.0 },
        max_start_size: Vector2f { x: 1.0, y: 1.0 },
        start_rel_size: 1.0,
        middle_rel_size: 1.0,
        end_rel_size: 1.0,
        middle_rel_size_time: 0.5,
        min_start_color: WHITE,
        max_start_color: WHITE,
        start_rel_color: WHITE,
        middle_rel_color: WHITE,
        end_rel_color: WHITE,
        max_frame_time: 1.0,
        ..Params::default()
    }
}

fn no_hit(_: &Vector3f, _: &Vector3f) -> Option<(Vector3f, Vector3f)> {
    None
}

fn gfx_in() -> GfxIn {
    GfxIn {
        hpl3_color: false,
        add_blend: false,
        fade_at_distance: false,
        inverts_cull: false,
        ps_color: WHITE,
        brightness: 1.0,
        light_color: WHITE,
        light_brightness: 1.0,
        fade_center: v3(0.0, 0.0, 0.0),
        origin: v3(0.0, 0.0, 0.0),
        min_fade_start: 0.0,
        min_fade_end: 0.0,
        max_fade_start: 0.0,
        max_fade_end: 0.0,
        draw_size: Vector2f { x: 1.0, y: 1.0 },
        world: IDENTITY,
        view: IDENTITY,
        ps_world: IDENTITY,
        axis_right: v3(1.0, 0.0, 0.0),
        axis_forward: v3(0.0, 0.0, 1.0),
        pos_stride: 4,
    }
}

fn emitter(p: Params, max: u32, respawn: bool) -> Emitter {
    Emitter::new(p, max, respawn, Vector2l { x: 1, y: 1 }, 1)
}

#[test]
fn ffi_layouts_match_the_cpp_header() {
    assert_eq!(size_of::<Params>(), 404);
    assert_eq!(size_of::<GfxIn>(), 312);
    assert_eq!(size_of::<Particle>(), 176);
    assert_eq!(size_of::<ModelIn>(), 108);
    assert_eq!(size_of::<EnvVelIn>(), 56);
}

#[test]
fn k2pi_is_the_cpp_literal() {
    assert_eq!(std::f32::consts::TAU.to_bits(), 0x40c9_0fdb);
}

#[test]
fn spawn_rate_and_pool_cap() {
    let mut e = emitter(params(), 3, true);
    e.update_motion(0.1, &IDENTITY, &mut no_hit);
    assert_eq!(e.num_particles(), 1);
    e.update_motion(1.0, &IDENTITY, &mut no_hit);
    assert_eq!(e.num_particles(), 3);
}

#[test]
fn nonrespawning_emitter_dies_after_its_last_particle() {
    let mut p = params();
    p.min_life_span = 0.25;
    p.max_life_span = 0.25;
    let mut e = emitter(p, 1, false);
    assert!(e.is_dying());
    e.update_motion(0.1, &IDENTITY, &mut no_hit);
    assert_eq!((e.num_particles(), e.is_dead()), (1, false));
    e.update_motion(0.3, &IDENTITY, &mut no_hit);
    assert_eq!((e.num_particles(), e.is_dead()), (0, true));
}

#[test]
fn kill_stops_respawning() {
    let mut p = params();
    p.min_life_span = 0.25;
    p.max_life_span = 0.25;
    let mut e = emitter(p, 1, true);
    assert!(!e.is_dying());
    e.update_motion(0.1, &IDENTITY, &mut no_hit);
    e.update_motion(0.3, &IDENTITY, &mut no_hit);
    assert_eq!(e.num_particles(), 1);
    e.kill();
    assert!(e.is_dying());
    e.kill_instantly();
    assert!(e.is_dead());
}

#[test]
fn start_offset_delays_the_first_spawn() {
    let mut p = params();
    p.start_time_offset = 0.5;
    let mut e = emitter(p, 4, true);
    e.update_motion(0.3, &IDENTITY, &mut no_hit);
    e.update_motion(0.3, &IDENTITY, &mut no_hit);
    assert_eq!(e.num_particles(), 0);
    e.update_motion(0.1, &IDENTITY, &mut no_hit);
    assert_eq!(e.num_particles(), 1);
}

#[test]
fn gravity_vector_and_bounce() {
    let mut p = params();
    p.gravity_type = GRAVITY_VECTOR;
    p.gravity_acc = v3(0.0, -10.0, 0.0);
    p.collides = true;
    p.collision_update_rate = 100;
    p.min_bounce_amount = 0.5;
    p.max_bounce_amount = 0.5;
    p.min_collision_max = 1;
    p.max_collision_max = 1;
    let mut e = emitter(p, 1, false);
    let mut hits = 0;
    let mut floor = |_: &Vector3f, _: &Vector3f| {
        hits += 1;
        Some((v3(0.0, 1.0, 0.0), v3(0.0, 0.0, 0.0)))
    };
    e.update_motion(0.1, &IDENTITY, &mut floor);
    assert_eq!(hits, 1);
    let Some(a) = e.particle(0) else {
        panic!("slot 0")
    };
    // one bounce allowed: the particle is killed by the hit
    assert!(a.life <= 0.0 && a.bounce_count == 0);
    assert_eq!(a.pos, v3(0.0, 0.0, 0.0));
    // vel (0,-1,0) reflected on +Y and scaled by 0.5
    assert_eq!(a.vel, v3(0.0, 0.5, 0.0));
}

#[test]
fn sub_div_table_ignores_a_y_only_split() {
    let e = Emitter::new(params(), 1, true, Vector2l { x: 1, y: 4 }, 1);
    assert_eq!(e.sub_div_count(), 0);
    let e = Emitter::new(params(), 1, true, Vector2l { x: 2, y: 2 }, 1);
    assert_eq!(e.sub_div_count(), 4);
}

#[test]
fn builds_camera_facing_quads() {
    let mut e = emitter(params(), 2, true);
    e.update_motion(0.1, &IDENTITY, &mut no_hit);
    let (mut pos, mut col, mut tex) = (vec![0.0; 32], vec![0.0; 32], vec![]);
    let r = e.build_vertices(&gfx_in(), &mut pos, &mut col, &mut tex);
    assert_eq!(r, Gfx::Drawn);
    // particle at origin, size 1: corners at (+1,-1) (-1,-1) (-1,+1) (+1,+1)
    assert_eq!(&pos[..3], &[1.0, -1.0, 0.0]);
    assert_eq!(&pos[4..7], &[-1.0, -1.0, 0.0]);
    assert_eq!(&pos[8..11], &[-1.0, 1.0, 0.0]);
    assert_eq!(&col[..4], &[1.0, 1.0, 1.0, 1.0]);
}

#[test]
fn invisible_system_alpha_hides_the_emitter() {
    let mut e = emitter(params(), 2, true);
    e.update_motion(0.1, &IDENTITY, &mut no_hit);
    let mut g = gfx_in();
    g.ps_color.a = 0.0;
    let r = e.build_vertices(&g, &mut [0.0; 32], &mut [0.0; 32], &mut []);
    assert_eq!(r, Gfx::Hidden);
}

#[test]
fn fade_at_distance_scales_alpha_between_end_and_start() {
    let mut e = emitter(params(), 1, true);
    e.update_motion(0.1, &IDENTITY, &mut no_hit);
    let mut g = gfx_in();
    g.fade_at_distance = true;
    g.fade_center = v3(0.0, 0.0, 3.0);
    g.min_fade_start = 5.0;
    g.min_fade_end = 1.0;
    let mut col = [0.0; 16];
    assert_eq!(
        e.build_vertices(&g, &mut [0.0; 16], &mut col, &mut []),
        Gfx::Drawn
    );
    // distance 3 between end 1 and start 5: (3-1)/(5-1)
    assert_eq!(col[3], 0.5);
    g.min_fade_end = 3.5;
    assert_eq!(
        e.build_vertices(&g, &mut [0.0; 16], &mut col, &mut []),
        Gfx::Hidden
    );
}

#[test]
fn bounds_cover_particles_and_draw_size() {
    let mut p = params();
    p.min_start_vel = v3(2.0, 0.0, 0.0);
    p.max_start_vel = v3(2.0, 0.0, 0.0);
    let mut e = emitter(p, 1, true);
    e.update_motion(0.1, &IDENTITY, &mut no_hit);
    let none = v3(0.0, 0.0, 0.0);
    let (mn, mx) = e.bounds(none, none, none, Vector2f { x: 1.0, y: 2.0 });
    assert_eq!(mn, v3(-1.0, -2.0, -1.0));
    assert!((mx.x - 1.2).abs() < 1e-6 && mx.y == 2.0 && mx.z == 1.0);
}

#[test]
fn billboard_fixed_axis_keeps_the_world_matrix() {
    let mut w = IDENTITY;
    w.m[0][3] = 4.0;
    let i = ModelIn {
        kind: FIXED_AXIS,
        forward_offset: 1.0,
        world: w,
        cam_origin: v3(0.0, 0.0, 9.0),
        view_up: v3(0.0, 1.0, 0.0),
        axis: v3(0.0, 1.0, 0.0),
    };
    assert_eq!(billboard::model_matrix(&i), (w, false));
}

#[test]
fn billboard_point_faces_the_camera() {
    let i = ModelIn {
        kind: POINT,
        forward_offset: 0.0,
        world: IDENTITY,
        cam_origin: v3(0.0, 0.0, 5.0),
        view_up: v3(0.0, 1.0, 0.0),
        axis: v3(0.0, 1.0, 0.0),
    };
    let (m, warn): (Matrixf, bool) = billboard::model_matrix(&i);
    assert!(!warn);
    // right = up x forward = (1,0,0), up = forward x right = (0,1,0), forward = +Z toward camera
    assert_eq!(m.m, IDENTITY.m);
}

#[test]
fn billboard_axis_warns_when_the_camera_is_on_the_axis() {
    let i = ModelIn {
        kind: 1,
        forward_offset: 0.0,
        world: IDENTITY,
        cam_origin: v3(0.0, 5.0, 0.0),
        view_up: v3(0.0, 1.0, 0.0),
        axis: v3(0.0, 1.0, 0.0),
    };
    assert!(billboard::model_matrix(&i).1);
}

#[test]
fn billboard_vertices() {
    let mut pos = [9.0; 16];
    billboard::vertex_positions(Vector2f { x: 2.0, y: 4.0 }, &mut pos);
    assert_eq!(&pos[..8], &[1.0, -2.0, 0.0, 9.0, -1.0, -2.0, 0.0, 9.0]);
    let mut col = [0.0; 16];
    let c = Color {
        r: 0.5,
        g: 1.0,
        b: 0.25,
        a: 0.5,
    };
    billboard::vertex_colors(c, 2.0, 0.5, true, &mut col);
    // hdr squares rgb: 0.25 * (0.5 * 2), alpha keeps the unsquared colour alpha * halo
    assert_eq!(&col[..4], &[0.25, 1.0, 0.0625, 0.25]);
    assert_eq!(&col[12..], &col[..4]);
}

#[test]
fn env_velocity_vectors_seed_slot_zero_and_keep_it_without_randomness() {
    let i = EnvVelIn {
        gravity_vel: v3(0.0, -0.1, 0.0),
        wind_vel: v3(1.0, 0.0, 0.0),
        rotate_vel: v3(0.0, 0.5, 0.0),
        ..EnvVelIn::default()
    };
    let z = [v3(0.0, 0.0, 0.0); ENV_MAX_ITER];
    let (mut g, mut w, mut r) = (z, z, z);
    env_velocity_vectors(&i, &mut g, &mut w, &mut r);
    assert_eq!(
        (g[0], w[0], r[0]),
        (i.gravity_vel, i.wind_vel, i.rotate_vel)
    );
    assert!(g.iter().all(|v| *v == i.gravity_vel));
    assert!(w.iter().all(|v| *v == i.wind_vel));
}

#[test]
fn env_update_integrates_and_wraps_large_offsets() {
    let z = [v3(0.0, 0.0, 0.0); ENV_MAX_ITER];
    let mut g = z;
    let mut w = z;
    if let (Some(a), Some(b)) = (g.get_mut(0), w.get_mut(0)) {
        *a = v3(0.0, -1.0, 0.0);
        *b = v3(20000.0, 0.0, 0.0);
    }
    let (mut off, mut rot) = (z, z);
    env_update(1, 1.0, [&g, &w, &z], &mut off, &mut rot);
    assert_eq!(off.first().map(|o| o.y), Some(-1.0));
    // 20000 wraps to |20000| mod 10000 = 0
    assert_eq!(off.first().map(|o| o.x), Some(0.0));
    assert_eq!(off.get(1), Some(&v3(0.0, 0.0, 0.0)));
}
