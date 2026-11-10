use super::beam;
use super::lensflare::{self, EntIn, FlareIn, Kind, ViewIn, VisState};
use super::light::*;
use super::rope::{self, RopeOut, RopeParams};
use crate::math::IDENTITY;
use crate::types::{Color, Vector2f, Vector3f};

fn v3(x: f32, y: f32, z: f32) -> Vector3f {
    Vector3f { x, y, z }
}

fn col(r: f32, g: f32, b: f32, a: f32) -> Color {
    Color { r, g, b, a }
}

#[derive(Default)]
struct Rec {
    events: Vec<String>,
}

impl LightHost for Rec {
    fn ease(&mut self, t: f32) -> f32 {
        t
    }
    fn set_brightness(&mut self, v: f32) {
        self.events.push(format!("brightness {v}"));
    }
    fn set_radius(&mut self, v: f32) {
        self.events.push(format!("radius {v}"));
    }
    fn set_diffuse(&mut self, c: Color) {
        self.events.push(format!("diffuse {}", c.r));
    }
    fn assign_diffuse(&mut self, _: Color) {}
    fn raw_radius(&mut self, v: f32) {
        self.events.push(format!("raw {v}"));
    }
    fn flicker_sound(&mut self, on: bool) {
        self.events.push(format!("sound {on}"));
    }
    fn flicker_event(&mut self, on: bool) {
        self.events.push(format!("event {on}"));
    }
}

fn live() -> LightLive {
    LightLive {
        diffuse: col(1.0, 1.0, 1.0, 1.0),
        radius: 10.0,
        brightness: 1.0,
    }
}

#[test]
fn fade_reaches_destination_and_snaps() {
    let mut a = LightAnim::default();
    let mut l = live();
    fade_to(&mut a, &l, col(0.0, 0.0, 0.0, 1.0), 2.0, 1.0);
    let mut host = Rec::default();
    update_light(&mut a, &mut l, false, false, &mut host, 0.5);
    assert_eq!(l.radius, 6.0);
    assert_eq!(l.diffuse.r, 0.5);
    update_light(&mut a, &mut l, false, false, &mut host, 0.5);
    assert_eq!(a.fade_time, 0.0);
    assert_eq!((l.radius, l.diffuse.r), (2.0, 0.0));
    assert_eq!(host.events.last().map(String::as_str), Some("raw 2"));
}

#[test]
fn brightness_fade_clears_when_done() {
    let mut a = LightAnim::default();
    let mut l = live();
    fade_brightness_to(&mut a, &l, 0.0, 2.0);
    let mut host = Rec::default();
    update_light(&mut a, &mut l, false, false, &mut host, 1.0);
    assert_eq!(l.brightness, 0.5);
    update_light(&mut a, &mut l, false, false, &mut host, 5.0);
    assert_eq!((l.brightness, a.bright_fade_time), (0.0, 0.0));
}

#[test]
fn zero_time_fade_uses_minimum() {
    let mut a = LightAnim::default();
    fade_to(&mut a, &live(), col(0.0, 0.0, 0.0, 0.0), 0.0, 0.0);
    assert_eq!(a.fade_time, 0.0001);
    fade_brightness_to(&mut a, &live(), 0.0, -1.0);
    assert_eq!(a.bright_fade_time, 0.0001);
}

#[test]
fn flicker_toggles_off_then_on_with_sounds() {
    let mut a = LightAnim::default();
    let mut l = live();
    let cfg = FlickerCfg {
        off_color: col(0.0, 0.0, 0.0, 1.0),
        off_radius: 1.0,
        on_min_length: 1.0,
        on_max_length: 1.0,
        off_min_length: 1.0,
        off_max_length: 1.0,
        ..FlickerCfg::default()
    };
    set_flicker(&mut a, &l, &cfg);
    a.flickering = true;
    let mut host = Rec::default();
    update_light(&mut a, &mut l, true, true, &mut host, 1.0);
    assert!(host.events.is_empty());
    update_light(&mut a, &mut l, true, true, &mut host, 0.5);
    assert_eq!(
        host.events,
        ["diffuse 0", "radius 1", "sound false", "event false"]
    );
    assert!(!a.flicker_on);
    host.events.clear();
    update_light(&mut a, &mut l, true, true, &mut host, 1.0);
    update_light(&mut a, &mut l, false, true, &mut host, 0.0);
    assert_eq!(host.events, ["diffuse 1", "radius 10", "event true"]);
}

#[test]
fn spot_projection_matches_hand_worked_matrix() {
    let m = spot_projection(std::f32::consts::FRAC_PI_2, 1.0, 1.0, 0.0, 3.0);
    let near = |a: f32, b: f32| (a - b).abs() < 1e-5;
    assert!(near(m.m[0][0], 1.0) && near(m.m[1][1], 1.0));
    assert!(near(m.m[2][2], -2.0) && near(m.m[2][3], -3.0) && m.m[3][2] == -1.0);
    assert_eq!(spot_near(0.5, 0.0), 0.5);
    assert_eq!(spot_near(0.5, 0.1), 0.1);
}

#[test]
fn spot_view_proj_maps_clip_space_to_texture_space() {
    let m = spot_view_proj(&IDENTITY, &IDENTITY);
    assert_eq!((m.m[0][0], m.m[0][3], m.m[3][3]), (0.5, 0.5, 1.0));
}

#[test]
fn box_fade_runs_to_completion() {
    let from = [v3(0.0, 0.0, 0.0); 9];
    let target = [v3(2.0, 4.0, 6.0); 9];
    let mut bands = from;
    let mut t = 0.0;
    let speed = box_fade_speed(2.0);
    assert!(!box_fade_step(
        &mut bands, &from, &target, &mut t, speed, 1.0
    ));
    assert_eq!(bands[0], v3(1.0, 2.0, 3.0));
    assert!(box_fade_step(
        &mut bands, &from, &target, &mut t, speed, 1.0
    ));
    assert_eq!(bands[8], target[8]);
    let mut t = 0.0;
    assert!(box_fade_step(
        &mut bands,
        &from,
        &target,
        &mut t,
        box_fade_speed(0.0),
        0.0
    ));
}

#[test]
fn visibility_snaps_first_resets_after_gap_and_ignores_repeat() {
    let mut s = VisState {
        target: 0.8,
        vis: 1.0,
        vel: 0.0,
        frame: -1,
    };
    lensflare::update_visibility(&mut s, 0.016, 5);
    assert_eq!((s.vis, s.frame), (0.8, 5));
    s.target = 0.0;
    lensflare::update_visibility(&mut s, 0.016, 5);
    assert_eq!(s.vis, 0.8);
    lensflare::update_visibility(&mut s, 0.016, 9);
    assert!(s.vis < 0.05 && s.vis >= 0.0);
}

#[test]
fn occlusion_target_is_visible_fraction_of_clipped_area() {
    let full = lensflare::occlusion_target(100.0, 100.0, v3(-0.5, -0.5, 0.0), v3(0.5, 0.5, 0.0));
    assert!((full - 1.0).abs() < 1e-3);
    let half = lensflare::occlusion_target(100.0, 100.0, v3(0.0, -0.5, 0.0), v3(2.0, 0.5, 0.0));
    assert!((half - 0.5).abs() < 1e-3);
    let empty = lensflare::occlusion_target(1.0, 1.0, v3(1.0, 0.0, 0.0), v3(1.0, 1.0, 0.0));
    assert_eq!(empty, 0.0);
}

fn flare() -> (FlareIn, EntIn, ViewIn) {
    let f = FlareIn {
        active: [false, true, true],
        color: [col(1.0, 1.0, 1.0, 1.0); 3],
        size: [Vector2f { x: 0.5, y: 0.5 }; 3],
        range_max: Vector2f { x: -1.0, y: -1.0 },
        range_min: Vector2f { x: -1.0, y: -1.0 },
        glare_range: Vector2f { x: -1.0, y: -1.0 },
        atlas_grid: crate::types::Vector2l { x: 2, y: 2 },
        multi_iris_seed: 1234,
        multi_iris_count: 5,
        size_by_dist: 0.5,
        outer_cos: -1.0,
        inner_cos: -1.0,
        glare_cos: 1.0,
        glare_brightness: 0.5,
        glare_stare_at: 0.2,
        fade: 1.0,
        brightness: 1.0,
        shrink_when_occluded: true,
        mul_iris_with_glare: false,
    };
    let e = EntIn {
        pos: v3(0.0, 0.0, -5.0),
        axis: v3(0.0, 0.0, 1.0),
    };
    let mut v = ViewIn {
        view: IDENTITY,
        proj: IDENTITY,
        aspect: 1.0,
        ..ViewIn::default()
    };
    v.proj.m[3] = [0.0, 0.0, -1.0, 0.0];
    (f, e, v)
}

#[test]
fn inactive_types_have_no_colour_or_size() {
    let (f, e, v) = flare();
    assert_eq!(
        lensflare::color_and_brightness(&f, &e, &v, Kind::Anamorphic, 1.0),
        Color::default()
    );
    assert_eq!(
        lensflare::size_and_rotation(&f, &e, &v, Kind::Anamorphic, 1.0),
        IDENTITY
    );
    assert_eq!(
        lensflare::size_and_rotation(&f, &e, &v, Kind::MultiIris, 1.0),
        IDENTITY
    );
    assert!(lensflare::color_and_brightness(&f, &e, &v, Kind::Halo, 1.0).a > 0.0);
}

#[test]
fn type_color_is_none_when_occluded() {
    let (f, e, v) = flare();
    assert!(lensflare::type_color(&f, &e, &v, Kind::Halo, 0.0).is_none());
    assert!(lensflare::type_color(&f, &e, &v, Kind::Halo, 1.0).is_some());
}

#[test]
fn multi_iris_fills_requested_quads_only() {
    let (f, e, v) = flare();
    let (mut pos, mut col, mut uv) = (vec![-1.0; 16 * 3], vec![-1.0; 16 * 3], vec![-1.0; 12 * 3]);
    let n = lensflare::multi_iris(&f, &e, &v, 1.0, &mut pos, &mut col, &mut uv);
    assert_eq!(n, 3);
    assert!(pos.iter().step_by(4).skip(1).take(1).all(|x| x.is_finite()));
    assert!(pos.chunks(4).all(|p| p[3] == 1.0));
    let again = {
        let (mut p2, mut c2, mut u2) = (vec![0.0; 80], vec![0.0; 80], vec![0.0; 60]);
        lensflare::multi_iris(&f, &e, &v, 1.0, &mut p2, &mut c2, &mut u2);
        p2
    };
    assert_eq!(&again[..48], &pos[..48]);
}

#[test]
fn beam_geometry_spans_start_to_end() {
    let g = beam::geometry(v3(0.0, 0.0, 0.0), v3(0.0, 0.0, 4.0), [1.0, 2.0], true);
    assert_eq!((g.axis, g.mid), (v3(0.0, 0.0, 1.0), v3(0.0, 0.0, 2.0)));
    assert_eq!(g.pos[0], [0.5, -2.0, 0.0]);
    assert_eq!(g.uv[2], [0.0, -2.0]);
    let flat = beam::geometry(v3(0.0, 0.0, 0.0), v3(0.0, 0.0, 4.0), [1.0, 2.0], false);
    assert_eq!(flat.uv[3], [1.0, 0.0]);
}

#[test]
fn beam_bounds_are_padded_box() {
    let (min, max) = beam::bounds(v3(1.0, 5.0, 0.0), v3(3.0, 2.0, 0.0), 0.5);
    assert_eq!((min, max), (v3(0.5, 1.5, -0.5), v3(3.5, 5.5, 0.5)));
}

#[test]
fn beam_edge_colors_premultiply_alpha() {
    let mut out = [0.0; 8];
    beam::set_edge_colors(col(1.0, 0.5, 0.25, 0.5), true, &mut out);
    assert_eq!(out, [0.5, 0.25, 0.125, 0.5, 0.5, 0.25, 0.125, 0.5]);
}

#[test]
fn beam_model_matrix_translates_to_midpoint() {
    let m = beam::model_matrix(
        &IDENTITY,
        v3(0.0, 0.0, 0.0),
        v3(0.0, 0.0, 10.0),
        v3(0.0, 1.0, 0.0),
        v3(1.0, 2.0, 3.0),
    );
    assert_eq!((m.m[0][3], m.m[1][3], m.m[2][3]), (1.0, 2.0, 3.0));
    assert_eq!((m.m[1][1], m.m[0][0].abs()), (1.0, 1.0));
}

#[test]
fn rope_quads_follow_points() {
    let pts = [v3(0.0, 0.0, 0.0), v3(0.0, 1.0, 0.0), v3(0.0, 2.0, 0.0)];
    let p = RopeParams {
        forward: v3(0.0, 0.0, 1.0),
        radius: 0.1,
        segment_length: 1.0,
        tile_amount: 2.0,
        tile_size: 1.0,
    };
    let (mut pos, mut uv, mut n, mut t) = ([0.0; 64], [0.0; 48], [0.0; 48], [0.0; 64]);
    let out = RopeOut {
        pos: &mut pos,
        uv: &mut uv,
        normal: &mut n,
        tangent: &mut t,
    };
    assert_eq!(rope::quads(&pts, &p, out), 2);
    assert_eq!(
        &pos[..16],
        &[
            0.1, 0.0, 0.0, 1.0, -0.1, 0.0, 0.0, 1.0, -0.1, 1.0, 0.0, 1.0, 0.1, 1.0, 0.0, 1.0
        ]
    );
    assert_eq!(&uv[..3], &[1.0, 2.0, 0.0]);
    assert_eq!(&n[..3], &[0.0, 0.0, 1.0]);
    assert_eq!(&t[..4], &[1.0, 0.0, 0.0, 1.0]);
}

#[test]
fn rope_quads_stop_at_buffer_capacity_and_empty_input() {
    let pts = [v3(0.0, 0.0, 0.0), v3(0.0, 1.0, 0.0), v3(0.0, 2.0, 0.0)];
    let p = RopeParams {
        forward: v3(0.0, 0.0, 1.0),
        radius: 0.1,
        segment_length: 1.0,
        tile_amount: 1.0,
        tile_size: 1.0,
    };
    let (mut pos, mut uv, mut n, mut t) = ([0.0; 16], [0.0; 12], [0.0; 12], [0.0; 16]);
    let out = RopeOut {
        pos: &mut pos,
        uv: &mut uv,
        normal: &mut n,
        tangent: &mut t,
    };
    assert_eq!(rope::quads(&pts, &p, out), 1);
    let out = RopeOut {
        pos: &mut pos,
        uv: &mut uv,
        normal: &mut n,
        tangent: &mut t,
    };
    assert_eq!(rope::quads(&[], &p, out), 0);
}

#[test]
fn rope_bounds_pad_by_radius() {
    let (min, max) = rope::bounds(&[v3(0.0, 1.0, 2.0), v3(-1.0, 3.0, 0.0)], 0.5);
    assert_eq!((min, max), (v3(-1.5, 0.5, -0.5), v3(0.5, 3.5, 2.5)));
    let (min, max) = rope::bounds(&[], 1.0);
    assert!(min.x > max.x);
}
