use super::critter::{self, CritterHost, CritterProps, CritterState, Flock, Member};
use super::imgui::{self, Actions, Fade, Focus, Layout, NavEntry, Prev, State, WidgetIn};
use super::imgui_widgets::{self, ButtonFlags, MultiIn};
use super::loaders::{self, ElementId, EntityKind};
use super::postfx;
use super::screens::{self, SplashIn};
use super::tonemap::{self, ExposureArea, ToneState};
use crate::math::{IDENTITY, v3};
use crate::types::Vector3f;

struct Floor;

impl CritterHost for Floor {
    fn cast(&mut self, start: Vector3f, end: Vector3f) -> Option<(f32, Vector3f)> {
        if end.y < 0.0 && start.y > 0.0 {
            Some((start.y - 0.0, v3(0.0, 1.0, 0.0)))
        } else {
            Some((-1.0, v3(0.0, 0.0, 0.0)))
        }
    }
}

#[test]
fn critter_safe_normalize_and_speed() {
    let n = critter::safe_normalize(v3(3.0, 0.0, 4.0));
    assert!((n.x - 0.6).abs() < 1e-6 && (n.z - 0.8).abs() < 1e-6);
    assert_eq!(
        critter::safe_normalize(v3(0.0, 0.0, 0.0)),
        v3(0.0, 0.0, 0.0)
    );
    assert_eq!(critter::change_max_speed(1.0, 2.0, 1.0, 0.25), 1.25);
    assert_eq!(critter::change_max_speed(1.0, 0.5, 1.0, 1.0), 0.5);
}

#[test]
fn critter_falls_and_lands() {
    let mut s = CritterState::default();
    let mut p = CritterProps {
        max_velocity: 1.0,
        max_turn_speed: 6.0,
        turn_speed_mul: 10.0,
        max_gravity_velocity: 5.0,
        ..CritterProps::default()
    };
    let step = critter::update(&mut s, &mut p, &mut Floor, v3(0.0, 0.2, 0.0), 0.1);
    assert!(step.pos.y < 0.2 && p.gravity_vel[1] < 0.0 || p.gravity_vel == [0.0; 3]);
    assert_eq!(step.play_anim, 2);
    let again = critter::update(&mut s, &mut p, &mut Floor, step.pos, 0.1);
    assert_eq!(again.play_anim, 0);
}

#[test]
fn critter_flock_without_members_is_center_pull() {
    let f = Flock {
        center_mul: 2.0,
        center_y_mul: 0.0,
        separation: 1.0,
        alignment: 1.0,
        cohesion: 1.0,
        max_checks: 3,
        dt: 0.5,
    };
    let a = critter::flocking_add(
        v3(0.0, 0.0, 0.0),
        v3(0.0, 0.0, 0.0),
        Some(v3(1.0, 5.0, 1.0)),
        &[],
        &f,
    );
    assert_eq!(a, v3(1.0, 0.0, 1.0));
    let m = [Member {
        is_self: true,
        pos: v3(2.0, 0.0, 0.0),
        vel: v3(0.0, 0.0, 0.0),
    }];
    let b = critter::flocking_add(v3(0.0, 0.0, 0.0), v3(0.0, 0.0, 0.0), None, &m, &f);
    assert_eq!(b, v3(1.0, 0.0, 0.0));
}

#[test]
fn tone_step_reaches_goal() {
    let mut v = 0.0;
    let mut fade = tonemap::Fade::default();
    tonemap::step(&mut v, &mut fade, 1.0, 2.0, 1.0);
    assert_eq!(v, 0.5);
    tonemap::step(&mut v, &mut fade, 1.0, 2.0, 1.0);
    assert_eq!(v, 1.0);
}

#[test]
fn tone_area_lookup_and_layers() {
    let area = ExposureArea {
        inv_transform: IDENTITY,
        half_size: [1.0, 1.0, 1.0],
        exposure: 2.0,
        white_cut: 3.0,
        transition_time: 1.0,
    };
    let areas = [area];
    assert_eq!(tonemap::area_at(&areas, v3(0.5, -0.5, 1.0)), Some(0));
    assert_eq!(tonemap::area_at(&areas, v3(1.5, 0.0, 0.0)), None);
    assert_eq!(tonemap::layer_insert_index(&[1, 2, 2, 5], 2), 3);
    let mut s = ToneState::default();
    tonemap::map_loaded(&mut s, 1.0, 4.0, areas.first());
    assert_eq!((s.world_exposure, s.exposure, s.white_cut), (1.0, 2.0, 3.0));
    assert!(tonemap::update_grading(&mut s, true, true, 2.0).promote_target);
}

#[test]
fn gamma_screen_layout() {
    assert_eq!(screens::gamma_wrap_width(1920.0), 700.0);
    assert_eq!(screens::gamma_wrap_width(300.0), 300.0);
    let l = screens::gamma_layout([1000.0, 800.0], Some([200.0, 100.0]), 700.0, 2);
    assert_eq!(l.checker_pos, [400.0, 310.0]);
    assert_eq!(l.slider_pos[1], 440.0);
    assert_eq!(l.button_pos[1], 495.0);
    assert_eq!(screens::gamma_to_slider(1.0), 41);
    assert!((screens::slider_to_gamma(100, 100) - 2.0).abs() < 1e-6);
    assert_eq!(screens::mouse_edge(true, false), 1);
    assert_eq!(screens::mouse_edge(false, true), 2);
}

#[test]
fn splash_timing() {
    let (e, done) = screens::splash_update(7.6, 0.0);
    assert!(!done && e == 7.6);
    assert!(screens::splash_update(7.6, 0.1).1);
    assert!(!screens::scripts_may_run(false, 3.0) && screens::scripts_may_run(false, 3.7));
    assert_eq!(screens::boot_bar_fraction(0.0), 0.60);
    assert_eq!(screens::boot_bar_fraction(3.0), 1.0);
    assert_eq!(screens::brain_frame(0.0), Some(0));
    assert_eq!(screens::brain_frame(26.0 / 15.0), Some(24));
    assert_eq!(screens::brain_frame(-1.0), None);
    let p = screens::splash_plan(&SplashIn {
        screen: [1024.0, 768.0],
        vw: 1024.0,
        t: 1.1,
        premenu_image: Some([512.0, 384.0]),
    });
    assert_eq!(p.alpha, 0.5);
    assert_eq!(p.premenu_size, [1024.0, 768.0]);
}

#[test]
fn loader_decisions() {
    assert_eq!(
        loaders::kind_from_entity_type(b"Agent_Foo"),
        EntityKind::Agent
    );
    assert_eq!(
        loaders::kind_from_entity_type(b"critter_x"),
        EntityKind::Critter
    );
    assert_eq!(loaders::kind_from_entity_type(b"Critter"), EntityKind::Prop);
    assert!(!loaders::is_dynamic(false, false, false, &[0.0, -1.0]));
    assert!(loaders::is_dynamic(false, false, false, &[0.0, 2.0]));
    assert_eq!(
        loaders::element_id(&[300, 4, 5], 9),
        ElementId { a: 44, b: 4, c: 5 }
    );
    assert_eq!(
        loaders::element_id(&[1, 2], 9),
        ElementId { a: 0, b: 0, c: 9 }
    );
    assert_eq!(
        loaders::socket_bone_name(b"SOCKET_hand"),
        Some(&b"hand"[..])
    );
    assert_eq!(loaders::socket_bone_name(b"sock"), None);
}

#[test]
fn postfx_activity_and_blur() {
    let mut p = [0.0f32; postfx::PARAMS];
    assert_eq!(postfx::set_activity(0, &p, true), (false, false));
    p[0] = 1.0;
    assert_eq!(postfx::set_activity(0, &p, false), (true, true));
    assert_eq!(postfx::set_activity(5, &p, false), (false, false));
    assert_eq!(postfx::trail_alpha(0.3, 0.016, true), 1.0);
    let levels = postfx::blur_levels([100, 50]);
    assert_eq!(levels.len(), 5);
    assert_eq!((levels[4].w, levels[4].h, levels[4].div), (7, 4, 16));
    assert!(postfx::blur_plan(1e9, 50, &levels).passthrough);
    let plan = postfx::blur_plan(8.0, 50, &levels);
    assert!(!plan.passthrough && plan.n_ops > 0);
    let w = postfx::blur_weights(10.0, 100, 50);
    let sum: f32 = w.weights[0] + 2.0 * w.weights[1..].iter().sum::<f32>();
    assert!((sum - 1.0).abs() < 1e-5);
}

#[test]
fn postfx_tentacle_fades() {
    let mut f = postfx::TentacleFades::default();
    f.set_material(2, 3, 1, 2.0);
    assert_eq!(f.blend().0, Some(1));
    f.advance(1.0);
    assert_eq!(f.blend(), (Some(1), 0.5));
    f.advance(1.0);
    assert_eq!((f.mat, f.blend().0), (1, None));
    f.set_material(2, 3, 5, 1.0);
    assert_eq!(f.fades.len(), 0);
    assert_eq!(postfx::tentacle_target(3840, 2160), [1920, 1080]);
}

fn actions<'a>(d: &'a mut [bool; 10], t: &'a mut [bool; 10], u: &'a mut [bool; 10]) -> Actions<'a> {
    Actions {
        down: d,
        triggered: t,
        used: u,
    }
}

#[test]
fn imgui_defaults_fades_and_layout() {
    let mut s = State::default();
    assert_eq!(imgui::default_or_current_int(&mut s, 3), 3);
    s.int = 5;
    assert_eq!(imgui::default_or_current_int(&mut s, 3), 5);
    assert_eq!(imgui::default_or_current_int(&mut s, 4), 4);
    let mut f: Fade = imgui::fade_make(&s, 0, [1.0, 0.0, 0.0, 0.0], 2.0, 0);
    assert!(imgui::fade_blocked(Some(&f), false));
    assert!(!imgui::fade_blocked(Some(&f), true));
    assert_eq!(imgui::fade_advance(&mut f, 1.0), Some(0.5));
    let mut st = State::default();
    imgui::fade_apply(&f, 0.5, &mut st);
    assert!(st.set_float);

    let (mut pos, mut size) = ([1.0, 2.0, 0.0], [-1.0, 5.0]);
    imgui::layout(
        &mut pos,
        &mut size,
        [10.0, 20.0],
        [100.0, 0.0, 0.0],
        true,
        6,
    );
    assert_eq!((pos, size), ([96.0, 2.0 - 2.5, 0.0], [10.0, 5.0]));

    let mut l = Layout {
        ty: 3,
        spacing: [1.0, 2.0],
        ..Layout::default()
    };
    imgui::advance(&mut l, [0.0, 10.0, 0.0], [5.0, 6.0]);
    assert_eq!(l.cursor[1], 18.0);
}

#[test]
fn imgui_nav_moves_focus() {
    let nav = [
        NavEntry {
            id: 1,
            pos: [0, 0],
            size: [10, 10],
            wrap: 3,
            group: 1,
        },
        NavEntry {
            id: 2,
            pos: [0, 20],
            size: [10, 10],
            wrap: 3,
            group: 1,
        },
        NavEntry {
            id: 3,
            pos: [30, 0],
            size: [10, 10],
            wrap: 3,
            group: 1,
        },
    ];
    let mut f = Focus {
        focus: 1,
        ..Focus::default()
    };
    let (mut d, mut t, mut u) = ([false; 10], [false; 10], [false; 10]);
    t[5] = true;
    update(&nav, &mut f, &mut d, &mut t, &mut u);
    assert_eq!(f.focus, 2);
    let (mut d, mut t, mut u) = ([false; 10], [false; 10], [false; 10]);
    t[5] = true;
    update(&nav, &mut f, &mut d, &mut t, &mut u);
    assert_eq!(f.focus, 1);
}

fn update(
    nav: &[NavEntry],
    f: &mut Focus,
    d: &mut [bool; 10],
    t: &mut [bool; 10],
    u: &mut [bool; 10],
) {
    imgui::update_ui_movement(nav, f, &mut actions(d, t, u));
}

#[test]
fn imgui_widget_focus_and_button() {
    let (mut d, mut t, mut u) = ([false; 10], [false; 10], [false; 10]);
    t[1] = true;
    let mut f = Focus {
        wrap_mode: 3,
        group_flags: 1,
        ..Focus::default()
    };
    let mut st = State::default();
    let mut prev = Prev::default();
    let i = WidgetIn {
        use_input: true,
        show_mouse: true,
        has_focus: true,
        first_run: false,
        use_ui_pos: true,
        mouse_rel: [0.0, 0.0],
        exp_hori: [0.0, 0.0],
        exp_vert: [0.0, 0.0],
        pos: [0.0, 0.0, 0.0],
        size: [10.0, 10.0],
        mouse: [5.0, 5.0],
    };
    let mut a = actions(&mut d, &mut t, &mut u);
    let o = imgui::widget_base(7, &i, &mut f, &mut a, &mut st, &mut prev);
    assert!(o.in_focus && o.mouse_over && o.has_nav && f.focus == 7);
    let r = imgui_widgets::button(&mut st, &mut a, true, true, 1, 0.1, ButtonFlags::default());
    assert!(r.clicked && r.result && st.int == 1);
}

#[test]
fn imgui_multi_select_wraps() {
    let (mut d, mut t, mut u) = ([false; 10], [false; 10], [false; 10]);
    t[7] = true;
    let mut st = State::default();
    let mut a = actions(&mut d, &mut t, &mut u);
    let i = MultiIn {
        over: true,
        mouse_over: false,
        num: 3,
        mouse_x: 0.0,
        pos_x: 0.0,
        size_x: 1.0,
    };
    imgui_widgets::multi_select(&mut st, &mut a, &i);
    assert_eq!(st.int, 2);
}
