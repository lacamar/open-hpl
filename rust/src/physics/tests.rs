use super::body;
use super::controller::{self, CtrlParams};
use super::host::Str;
use super::host::mock_support::{Mock, Val};
use super::joint::{self, JointState, MoveParams};
use super::pid::Pid;
use super::surface::{self, SurfaceParams};
use super::surface_xml;
use super::world::{self, BodyFilter, ImpactCounts};
use crate::math::{IDENTITY, v3};
use crate::types::Vector3f;

fn near(a: f32, b: f32) -> bool {
    (a - b).abs() < 1e-5
}

fn surface_params() -> SurfaceParams {
    SurfaceParams {
        this: 1,
        physics: 2,
        min_scrape_speed: 0.6,
        min_scrape_freq: 0.7,
        min_scrape_freq_speed: 1.0,
        max_scrape_freq: 2.0,
        max_scrape_freq_speed: 3.0,
        middle_scrape_speed: 2.0,
        min_scrape_contacts: 2,
        min_roll_speed: 0.7,
        min_roll_freq: 0.7,
        min_roll_volume: 0.5,
        min_roll_freq_speed: 1.0,
        max_roll_freq: 2.0,
        max_roll_volume: 1.5,
        max_roll_freq_speed: 3.0,
        middle_roll_speed: 2.0,
        roll_axis_flags: 1,
        scrape_sound: Str::new(b"scrape"),
        roll_sound: Str::new(b"roll"),
    }
}

fn move_params() -> MoveParams {
    MoveParams {
        min_speed: 0.5,
        min_freq: 0.5,
        min_freq_speed: 1.0,
        min_volume: 0.25,
        max_freq: 2.0,
        max_freq_speed: 4.0,
        max_volume: 1.0,
        middle_speed: 2.0,
        middle_volume: 0.5,
        speed_type: 0,
        move_sound: Str::new(b"move"),
    }
}

fn count(m: &Mock, prefix: &str) -> usize {
    m.calls(prefix).len()
}

#[test]
fn pid_integral_ring_and_derivative() {
    let mut pid = Pid::new();
    pid.set_error_num(3);
    pid.i = 1.0;
    let out: Vec<f32> = [2.0, 4.0, 8.0, 1.0]
        .iter()
        .map(|&e| pid.output(e, 0.5))
        .collect();
    assert_eq!(out, [1.0, 3.0, 7.0, 6.5]);

    pid.reset();
    assert_eq!(pid.output(2.0, 0.5), 1.0);

    let mut pid = Pid::new();
    pid.set_error_num(4);
    pid.d = 1.0;
    assert_eq!(pid.output(2.0, 0.5), 0.0);
    assert_eq!(pid.output(4.0, 0.5), 4.0);
}

#[test]
fn pid_without_buffer_outputs_zero() {
    let mut pid = Pid::new();
    pid.p = 3.0;
    pid.set_error_num(0);
    assert_eq!(pid.output(5.0, 0.1), 0.0);
    pid.set_error_num(-4);
    assert_eq!(pid.output(5.0, 0.1), 0.0);
}

#[test]
fn scrape_and_roll_curves() {
    let p = surface_params();
    assert!(near(surface::scrape_freq(&p, 0.5), 0.7));
    assert!(near(surface::scrape_freq(&p, 1.5), 0.85));
    assert!(near(surface::scrape_freq(&p, 2.5), 1.5));
    assert!(near(surface::scrape_freq(&p, 3.5), 2.0));
    let (f, v) = surface::roll_freq_volume(&p, 2.5);
    assert!(near(f, 1.5) && near(v, 1.25));
    let (f, v) = surface::roll_freq_volume(&p, 0.1);
    assert!(near(f, 0.7) && near(v, 0.5));
}

#[test]
fn impact_picks_first_threshold_below_speed() {
    let m = Mock::new();
    m.set("can_play_impact", vec![Val::B(true)]);
    m.set("body_world_world", vec![Val::H(5)]);
    m.set("surf_impact_count", vec![Val::I(2)]);
    m.set("surf_impact_min_speed", vec![Val::F(3.0), Val::F(1.0)]);
    m.set("surf_impact_sound", vec![Val::S(b"soft".to_vec())]);
    m.set("world_create_sound", vec![Val::H(9)]);
    m.set("body_mass", vec![Val::F(4.0)]);
    surface::on_impact(&m, &surface_params(), 2.0, v3(0.0, 0.0, 0.0), 7);
    assert_eq!(count(&m, "add_impact"), 1);
    assert_eq!(count(&m, "ent_set_param_named"), 2);
    assert!(m.calls("surf_impact_sound")[0].contains("(1, 1)"));

    let m = Mock::new();
    m.set("can_play_impact", vec![Val::B(true)]);
    m.set("body_world_world", vec![Val::H(5)]);
    m.set("surf_impact_count", vec![Val::I(2)]);
    m.set("surf_impact_min_speed", vec![Val::F(3.0), Val::F(1.0)]);
    surface::on_impact(&m, &surface_params(), 0.5, v3(0.0, 0.0, 0.0), 7);
    assert_eq!(count(&m, "add_impact"), 0);

    let m = Mock::new();
    surface::on_impact(&m, &surface_params(), 2.0, v3(0.0, 0.0, 0.0), 7);
    assert_eq!(m.log.borrow().len(), 1);
}

#[test]
fn slide_stops_and_starts_scrape_sounds() {
    let m = Mock::new();
    m.set("body_world_world", vec![Val::H(5)]);
    m.set("body_scrape_entity", vec![Val::H(7)]);
    m.set("world_sound_exists", vec![Val::B(true)]);
    surface::on_slide(&m, &surface_params(), 0.01, v3(0.0, 0.0, 0.0), 3, 10, 11);
    assert_eq!(count(&m, "ent_fade_out"), 1);
    assert_eq!(count(&m, "body_set_scrape_entity"), 1);
    assert_eq!(count(&m, "body_set_scrape_body"), 1);

    let m = Mock::new();
    m.set("body_world_world", vec![Val::H(5)]);
    m.set("world_create_sound", vec![Val::H(11)]);
    surface::on_slide(&m, &surface_params(), 1.0, v3(0.0, 0.0, 0.0), 3, 10, 12);
    assert_eq!(count(&m, "world_create_sound"), 1);
    assert!(m.calls("body_set_scrape_entity")[0].contains("(10, 11)"));
    assert!(m.calls("body_set_scrape_body")[0].contains("(10, 12)"));

    let m = Mock::new();
    surface::on_slide(&m, &surface_params(), 1.0, v3(0.0, 0.0, 0.0), 1, 10, 12);
    assert!(m.log.borrow().is_empty());
}

#[test]
fn impact_effect_prefers_higher_ps_priority() {
    let m = Mock::new();
    m.set("pworld_world", vec![Val::H(5)]);
    m.set("surf_impact_count", vec![Val::I(1)]);
    m.set("surf_impact_min_speed", vec![Val::F(0.0)]);
    m.set("surf_impact_ps_prio", vec![Val::I(5), Val::I(10)]);
    m.set("surf_impact_ps", vec![Val::S(b"ps".to_vec())]);
    surface::create_impact_effect(&m, &surface_params(), 1.0, v3(0.0, 0.0, 0.0), 2, 3);
    assert!(m.calls("surf_impact_ps ")[0].contains("(2, 0)"));
    assert_eq!(count(&m, "world_create_impact_ps"), 1);

    let m = Mock::new();
    surface::create_impact_effect(&m, &surface_params(), 0.0, v3(0.0, 0.0, 0.0), 2, 3);
    assert!(m.log.borrow().is_empty());
}

#[test]
fn roll_sound_starts_when_fast_and_colliding() {
    let m = Mock::new();
    m.set("body_use_surface_effects", vec![Val::B(true)]);
    m.set("body_angular_velocity", vec![Val::V(v3(1.5, 0.0, 0.0))]);
    m.set("body_world_world", vec![Val::H(5)]);
    m.set("body_has_collision", vec![Val::B(true)]);
    m.set("world_create_sound", vec![Val::H(8)]);
    surface::update_roll_effect(&m, &surface_params(), 10);
    assert!(
        m.calls("body_set_roll_entity")
            .last()
            .is_some_and(|c| c.contains("(10, 8)"))
    );

    let m = Mock::new();
    m.set("body_use_surface_effects", vec![Val::B(true)]);
    m.set("body_angular_velocity", vec![Val::V(v3(0.0, 9.0, 0.0))]);
    surface::update_roll_effect(&m, &surface_params(), 10);
    assert_eq!(count(&m, "world_create_sound"), 0);
}

#[test]
fn move_sound_curve_and_auto_sleep() {
    let p = move_params();
    assert_eq!(joint::calc_sound_freq(&p, 0.0), (0.5, 0.25));
    assert_eq!(joint::calc_sound_freq(&p, 9.0), (2.0, 1.0));
    let (f, v) = joint::calc_sound_freq(&p, 3.0);
    assert!(near(f, 1.5) && near(v, 0.75));

    let mut steps = 0;
    let near_limit: Vec<bool> = (0..4)
        .map(|_| joint::limit_auto_sleep(true, 0.02, 2, &mut steps, 0.0, 1.0, 0.01))
        .collect();
    assert_eq!(near_limit, [false, false, true, true]);
    assert!(!joint::limit_auto_sleep(
        true, 0.02, 2, &mut steps, 0.0, 1.0, 0.5
    ));
    assert_eq!(steps, 0);
    assert!(!joint::limit_auto_sleep(
        false, 0.02, 0, &mut steps, 0.0, 1.0, 0.0
    ));
}

#[test]
fn breakage_needs_force_or_broken_flag() {
    let sound = Str::new(b"break");
    let m = Mock::new();
    assert!(!joint::check_breakage(&m, 1, false, true, 5.0, sound));
    assert!(m.log.borrow().is_empty());

    let m = Mock::new();
    m.set("joint_force_size", vec![Val::F(10.0)]);
    m.set("joint_world_world", vec![Val::H(5)]);
    m.set("world_create_sound", vec![Val::H(9)]);
    assert!(joint::check_breakage(&m, 1, true, false, 5.0, sound));
    assert_eq!(count(&m, "world_create_sound"), 1);

    let m = Mock::new();
    m.set("joint_force_size", vec![Val::F(1.0)]);
    assert!(!joint::check_breakage(&m, 1, true, false, 5.0, sound));
    assert!(joint::check_breakage(&m, 1, true, true, 5.0, sound));
}

#[test]
fn limit_ends_active_controllers_once() {
    let effect = joint::LimitEffect {
        sound: Str::new(b""),
        min_speed: 0.0,
        max_speed: 1.0,
    };
    let m = Mock::new();
    m.set("joint_controller_count", vec![Val::I(2)]);
    m.set("joint_controller_at", vec![Val::H(100), Val::H(101)]);
    m.set("ctrl_is_active", vec![Val::B(true)]);
    m.set("ctrl_end_type", vec![Val::I(joint::END_ON_MAX)]);
    joint::on_limit(&m, 1, true, &effect);
    assert_eq!(count(&m, "joint_callback_max"), 1);
    assert_eq!(count(&m, "joint_warn_missing_controller"), 2);
    assert!(m.calls("joint_set_has_collided")[0].contains("true"));

    let m = Mock::new();
    m.set("joint_has_collided", vec![Val::B(true)]);
    joint::on_limit(&m, 1, true, &effect);
    assert_eq!(count(&m, "joint_callback_max"), 0);
    assert_eq!(count(&m, "joint_controller_count"), 0);
}

#[test]
fn physics_update_starts_move_sound_after_four_fast_steps() {
    let m = Mock::new();
    m.set("body_enabled", vec![Val::B(true)]);
    m.set("pworld_world", vec![Val::H(5)]);
    m.set("body_linear_velocity", vec![Val::V(v3(3.0, 0.0, 0.0))]);
    m.set("world_create_sound", vec![Val::H(9)]);
    m.set("ent_creation_id", vec![Val::I(42)]);
    let mut s = JointState {
        parent: 0,
        child: 2,
        world: 3,
        pivot: v3(0.0, 0.0, 0.0),
        pin_dir: v3(0.0, 1.0, 0.0),
        local_pivot: v3(0.0, 0.0, 0.0),
        local_pin_dir: v3(0.0, 1.0, 0.0),
        prev_child: IDENTITY,
        prev_parent: IDENTITY,
        sound: 0,
        sound_id: -1,
        speed_count: 0,
    };
    let mut p = move_params();
    p.speed_type = 0;
    for step in 0..4 {
        s.prev_child.m[0][0] = 5.0 + step as f32;
        assert!(joint::on_physics_update(&m, &p, &mut s));
    }
    assert_eq!((s.sound, s.sound_id, s.speed_count), (9, 42, 0));
    assert_eq!(count(&m, "world_create_sound"), 1);

    s.sound = 0;
    let frozen = Mock::new();
    assert!(!joint::on_physics_update(&frozen, &p, &mut s));
}

#[test]
fn spring_controller_applies_clamped_force_and_ends_on_dest() {
    let m = Mock::new();
    m.set("body_linear_velocity", vec![Val::V(v3(3.0, 0.0, 0.0))]);
    let p = CtrlParams {
        this: 50,
        body: 1,
        joint: 2,
        active: true,
        paused: false,
        log: false,
        mul_mass: false,
        kind: 1,
        input: 2,
        input_axis: 0,
        output: 0,
        output_axis: 1,
        end: 0,
        a: 2.0,
        b: 0.5,
        c: 0.0,
        dest: 1.0,
        max_output: 0.0,
        next: Str::new(b"next"),
    };
    let mut pid = Pid::new();
    // SAFETY: `pid` is live and not borrowed.
    unsafe { controller::update(&m, &p, &mut pid, 0.1) };
    assert!(m.calls("body_add_force")[0].contains("y: -5.5"));

    let m2 = Mock::new();
    m2.set("body_linear_velocity", vec![Val::V(v3(3.0, 0.0, 0.0))]);
    let clamped = CtrlParams {
        max_output: 3.0,
        ..p
    };
    // SAFETY: as above.
    unsafe { controller::update(&m2, &clamped, &mut pid, 0.1) };
    assert!(m2.calls("body_add_force")[0].contains("y: -3.0"));

    let m3 = Mock::new();
    m3.set("body_linear_velocity", vec![Val::V(v3(3.0, 0.0, 0.0))]);
    m3.set("joint_get_controller", vec![Val::H(60)]);
    let on_dest = CtrlParams {
        end: 1,
        dest: 3.0,
        ..p
    };
    // SAFETY: as above.
    unsafe { controller::update(&m3, &on_dest, &mut pid, 0.1) };
    assert_eq!(count(&m3, "ctrl_deactivate"), 1);
    assert!(m3.calls("ctrl_set_active")[0].contains("(60, true)"));

    let idle = Mock::new();
    let paused = CtrlParams { paused: true, ..p };
    // SAFETY: as above.
    unsafe { controller::update(&idle, &paused, &mut pid, 0.1) };
    assert!(idle.log.borrow().is_empty());
}

#[test]
fn velocity_at_position_adds_rotation_term() {
    let m = Mock::new();
    m.set("body_linear_velocity", vec![Val::V(v3(1.0, 0.0, 0.0))]);
    m.set("body_angular_velocity", vec![Val::V(v3(0.0, 0.0, 1.0))]);
    let v = body::velocity_at_position(&m, 1, v3(1.0, 0.0, 0.0));
    assert_eq!(v, v3(1.0, 1.0, 0.0));
}

#[test]
fn body_update_list_decisions() {
    let m = Mock::new();
    m.set("body_mass", vec![Val::F(2.0)]);
    m.set("body_enabled", vec![Val::B(true)]);
    assert!(body::update_before_simulate(&m, 1));
    assert!(m.calls("body_set_has_slide")[0].contains("false"));

    let m = Mock::new();
    assert!(!body::update_before_simulate(&m, 1));

    let m = Mock::new();
    m.set("body_scrape_entity", vec![Val::H(3)]);
    m.set("body_world_world", vec![Val::H(5)]);
    m.set("world_sound_exists", vec![Val::B(true)]);
    body::update_after_simulate(&m, 1, 0.016);
    assert!(m.calls("ent_fade_out")[0].contains("5.2"));
    assert_eq!(count(&m, "body_set_scrape_entity"), 1);

    let m = Mock::new();
    m.set("body_has_slide", vec![Val::B(true)]);
    body::update_after_simulate(&m, 1, 0.016);
    assert!(m.calls("body_set_slide_count")[0].contains("8"));
}

#[test]
fn static_move_only_for_static_bodies() {
    let m = Mock::new();
    m.set("body_mass", vec![Val::F(1.0)]);
    body::static_linear_move(&m, 1, v3(1.0, 0.0, 0.0));
    assert_eq!(m.log.borrow().len(), 1);

    let m = Mock::new();
    m.set("body_total_static_linear", vec![Val::V(v3(1.0, 1.0, 1.0))]);
    body::static_linear_move(&m, 1, v3(1.0, 2.0, 3.0));
    assert!(m.calls("body_set_total_static_linear")[0].contains("y: 3.0"));
    assert_eq!(count(&m, "body_add_to_update_list"), 1);
}

#[test]
fn shape_query_filter_and_push_merge() {
    let base = BodyFilter {
        active: true,
        is_character: false,
        is_skip_body: false,
        mass: 1.0,
        collide_character: true,
        collide: true,
        push_strength: 2,
        flags_match: true,
        query_collide_character: true,
        query_is_character: false,
        skip_static: false,
        min_push_strength: 0,
    };
    assert!(world::body_passes(&base));
    assert!(!world::body_passes(&BodyFilter {
        active: false,
        ..base
    }));
    assert!(!world::body_passes(&BodyFilter {
        is_skip_body: true,
        ..base
    }));
    assert!(!world::body_passes(&BodyFilter {
        min_push_strength: 3,
        ..base
    }));
    assert!(!world::body_passes(&BodyFilter {
        flags_match: false,
        ..base
    }));
    assert!(!world::body_passes(&BodyFilter {
        skip_static: true,
        mass: 0.0,
        ..base
    }));
    assert!(world::body_passes(&BodyFilter {
        skip_static: true,
        mass: 0.0,
        is_character: true,
        ..base
    }));
    assert!(!world::body_passes(&BodyFilter {
        collide: false,
        ..base
    }));
    assert!(world::body_passes(&BodyFilter {
        collide: false,
        query_is_character: true,
        ..base
    }));

    let pushed: Vector3f = world::merge_push(v3(0.5, -2.0, 0.0), v3(1.0, 1.0, 1.0), 1.0);
    assert_eq!(pushed, v3(1.0, -2.0, 1.0));
}

#[test]
fn impact_throttle_expires_by_duration() {
    let mut c = ImpactCounts::default();
    assert!(c.can_play(1));
    c.add();
    assert!(!c.can_play(1));
    c.update(0.3, 0.4);
    assert_eq!(c.len(), 1);
    c.update(0.2, 0.4);
    assert!(c.is_empty());
}

#[derive(Debug, Default)]
struct Seen {
    name: Vec<u8>,
    elasticity: f32,
    priority: i32,
    friction_mode: i32,
    elasticity_mode: i32,
    min_scrape_speed: f32,
    axis: u32,
    impacts: Vec<(f32, Vec<u8>, i32)>,
    hits: usize,
}

fn parse(xml: &str) -> (bool, Vec<Seen>) {
    let mut out = Vec::new();
    let ok = surface_xml::parse(xml.as_bytes(), true, |d| {
        // SAFETY: the sink gets live pointers for the duration of the call.
        unsafe {
            out.push(Seen {
                name: d.name.bytes().to_vec(),
                elasticity: d.elasticity,
                priority: d.priority,
                friction_mode: d.friction_mode,
                elasticity_mode: d.elasticity_mode,
                min_scrape_speed: d.min_scrape_speed,
                axis: d.roll_axis_flags,
                impacts: std::slice::from_raw_parts(d.impacts, d.n_impacts)
                    .iter()
                    .map(|i| (i.min_speed, i.sound.bytes().to_vec(), i.ps_prio))
                    .collect(),
                hits: d.n_hits,
            });
        }
    });
    (ok, out)
}

#[test]
fn surface_file_defaults_modes_and_lists() {
    let (ok, mats) = parse(
        r#"<Materials>
<Material Name="Wood" Elasticity="0.25" Priority="3" FrictionMode="MULTIPLY" ElasticityMode="bogus" RollAxis="X, z">
  <Impact MinSpeed="2" SoundName="hard"/>
  <Impact SoundName="soft"/>
  <Hit/>
</Material>
<Material/>
<Material Name="Bare" MinScrapeSpeed="abc"/>
</Materials>"#,
    );
    assert!(ok);
    assert_eq!(mats.len(), 2);
    let wood = &mats[0];
    assert_eq!(wood.name, b"Wood");
    assert_eq!((wood.elasticity, wood.priority), (0.25, 3));
    assert_eq!((wood.friction_mode, wood.elasticity_mode), (2, 0));
    assert_eq!(wood.min_scrape_speed, 0.7);
    assert_eq!(wood.axis, 5);
    assert_eq!(wood.impacts[0], (2.0, b"hard".to_vec(), 10));
    assert_eq!(wood.impacts[1].0, 1.0);
    assert_eq!(wood.hits, 1);
    let bare = &mats[1];
    assert_eq!(bare.elasticity, 0.5);
    assert_eq!(bare.min_scrape_speed, 0.0);
}

#[test]
fn surface_file_without_root_element_or_valid_xml() {
    let (ok, mats) = parse("<!-- nothing -->");
    assert!(ok && mats.is_empty());
    assert!(!parse("<Materials><Material Name=\"a\">").0);
    assert!(!parse("").0);
    assert!(!surface_xml::parse(b"<a/>", false, |_| {}));
}

#[test]
fn body_book_replaces_host_slots() {
    let bk = super::book::body_new();
    super::book::body_with(bk, |k| {
        k.static_movement = true;
        k.total_linear = v3(1.0, 0.0, 0.0);
        k.has_slide = true;
    });

    let book_run = Mock::new();
    assert!(body::update_before_simulate_bk(&book_run, 1, bk));
    body::update_after_simulate_bk(&book_run, 1, 0.5, bk);

    let k = super::book::body_get(bk).unwrap_or_else(|| panic!("book"));
    assert!(!k.has_slide && !k.has_collision && k.slide_count == 0);
    assert_eq!(k.total_linear, v3(0.0, 0.0, 0.0));
    assert_eq!(count(&book_run, "body_set_position"), 1);
    for slot in [
        "body_has_slide",
        "body_set_has_slide",
        "body_set_has_collision",
        "body_static_movement",
        "body_set_static_movement",
        "body_total_static",
        "body_set_total_static",
        "body_slide_count",
        "body_set_slide_count",
    ] {
        assert!(
            book_run.log.borrow().iter().all(|l| !l.starts_with(slot)),
            "{slot} leaked to host"
        );
    }

    let mock_run = Mock::new();
    mock_run.set("body_static_movement", vec![Val::B(true)]);
    mock_run.set("body_total_static_linear", vec![Val::V(v3(1.0, 0.0, 0.0))]);
    mock_run.set("body_has_slide", vec![Val::B(true)]);
    assert!(body::update_before_simulate(&mock_run, 1));
    body::update_after_simulate(&mock_run, 1, 0.5);
    assert_eq!(
        book_run.calls("body_set_position"),
        mock_run.calls("body_set_position")
    );

    super::book::body_free(bk);
    assert!(super::book::body_get(bk).is_none());
}

#[test]
fn joint_book_keeps_step_counters() {
    let bk = super::book::joint_new();
    for _ in 0..2 {
        assert!(!super::ffi::ohpl_h15_joint_limit_auto_sleep(
            bk, true, 0.1, 2, 0.0, 1.0, 0.05
        ));
    }
    assert!(super::ffi::ohpl_h15_joint_limit_auto_sleep(
        bk, true, 0.1, 2, 0.0, 1.0, 0.05
    ));
    assert!(!super::ffi::ohpl_h15_joint_limit_auto_sleep(
        bk, true, 0.1, 2, 0.0, 1.0, 0.5
    ));
    assert_eq!(
        super::book::joint_get(bk).map(|k| k.limit_step_count),
        Some(0)
    );
    super::book::joint_free(bk);
}

struct WorldCtx {
    book: u64,
    seen: Vec<usize>,
    removed: Vec<usize>,
}

unsafe extern "C" fn world_before(c: *mut std::ffi::c_void, b: usize) -> bool {
    // SAFETY: the test passes a live WorldCtx.
    let c = unsafe { &mut *c.cast::<WorldCtx>() };
    c.seen.push(b);
    if b == 20 {
        super::ffi::ohpl_h15_world_update_insert(c.book, 25);
        super::ffi::ohpl_h15_world_update_insert(c.book, 5);
    }
    b != 30
}

unsafe extern "C" fn world_remove(c: *mut std::ffi::c_void, b: usize) {
    // SAFETY: the test passes a live WorldCtx.
    let c = unsafe { &mut *c.cast::<WorldCtx>() };
    c.removed.push(b);
    super::ffi::ohpl_h15_world_update_remove(c.book, b);
}

unsafe extern "C" fn world_after(c: *mut std::ffi::c_void, b: usize) {
    // SAFETY: the test passes a live WorldCtx.
    unsafe { &mut *c.cast::<WorldCtx>() }.seen.push(b);
}

#[test]
fn world_update_set_matches_std_set_walk() {
    let bk = super::ffi::ohpl_h15_world_new();
    for b in [30, 10, 20] {
        assert!(super::ffi::ohpl_h15_world_update_insert(bk, b));
    }
    assert!(!super::ffi::ohpl_h15_world_update_insert(bk, 10));
    let mut c = WorldCtx {
        book: bk,
        seen: vec![],
        removed: vec![],
    };
    // SAFETY: c outlives the calls.
    unsafe {
        super::ffi::ohpl_h15_world_update_before(
            bk,
            (&raw mut c).cast(),
            world_before,
            world_remove,
        );
    }
    // 5 was inserted below the cursor: std::set would not visit it either.
    assert_eq!(c.seen, [10, 20, 25, 30]);
    assert_eq!(c.removed, [30]);
    c.seen.clear();
    // SAFETY: as above.
    unsafe { super::ffi::ohpl_h15_world_update_after(bk, (&raw mut c).cast(), world_after) };
    assert_eq!(c.seen, [5, 10, 20, 25]);
    super::ffi::ohpl_h15_world_update_clear(bk);
    super::ffi::ohpl_h15_world_free(bk);
    assert!(!super::ffi::ohpl_h15_world_update_insert(bk, 1));
}
