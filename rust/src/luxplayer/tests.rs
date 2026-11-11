use super::camera::{self, CamState, HeadPosAdd};
use super::hands;
use super::helpers::{
    self, Collapse, Darkness, DarknessIn, Death, Flashback, Hurt, Lantern, LanternSet, LookAt,
};
use super::ops::Ops;
use super::sanity::{self, Sanity};
use super::vitals::{self, Vitals, k as vk};
use crate::math::v3;
use crate::types::Vector2f;

fn kinds(o: &Ops) -> Vec<u32> {
    o.as_slice().iter().map(|x| x.kind).collect()
}

fn vit(health: f32, sanity: f32, hard: bool) -> Vitals {
    Vitals {
        health,
        sanity,
        lamp_oil: 50.0,
        hard_mode: u32::from(hard),
    }
}

#[test]
fn damage_is_clamped_when_not_lethal_and_ignored_when_dead() {
    let mut o = Ops::default();
    vitals::give_damage(&vit(20.0, 50.0, false), 30.0, 2, true, false, &mut o);
    assert_eq!(
        kinds(&o),
        [
            vk::SET_HEALTH,
            vk::DAMAGE_SPLASH,
            vk::HIT_FLASH,
            vk::SPIN_HEAD,
            vk::SET_HEALTH
        ]
    );
    assert_eq!(o.ops.get(4).map(|x| x.a), Some(10.0));
    let mut o = Ops::default();
    vitals::give_damage(&vit(20.0, 50.0, false), 30.0, 0, false, true, &mut o);
    assert_eq!(o.as_slice().last().map(|x| x.kind), Some(vk::DEATH_START));
    let mut o = Ops::default();
    vitals::give_damage(&vit(0.0, 50.0, false), 30.0, 0, false, true, &mut o);
    assert_eq!(o.n, 0);
}

#[test]
fn sanity_loss_collapses_and_hard_mode_kills() {
    let mut o = Ops::default();
    vitals::lower_sanity(&vit(50.0, 3.0, true), 5.0, true, &mut o);
    assert_eq!(
        kinds(&o),
        [
            vk::SET_SANITY,
            vk::COLLAPSE_START,
            vk::KILL,
            vk::SANITY_LOST
        ]
    );
    let mut o = Ops::default();
    vitals::lower_sanity(&vit(50.0, 30.0, true), 5.0, false, &mut o);
    assert_eq!(kinds(&o), [vk::SET_SANITY]);
    let mut o = Ops::default();
    vitals::set_health(&vit(0.0, 1.0, false), 0.0, &mut o);
    assert_eq!(o.n, 0);
    let mut o = Ops::default();
    vitals::add_sanity(&vit(50.0, 100.0, false), 5.0, true, &mut o);
    assert_eq!(o.n, 0);
}

#[test]
fn fov_fade_snaps_near_the_goal_and_pitch_respects_limits() {
    let mut s = CamState {
        fov: 1.0,
        fov_mul: 1.0,
        fov_mul_goal: 1.002,
        fov_mul_speed: 1.0,
        pitch_min: -1.0,
        pitch_max: 1.0,
        ..CamState::default()
    };
    let mut o = Ops::default();
    camera::update_camera(&mut s, 1.0 / 60.0, &mut o);
    assert_eq!(s.fov_mul, 1.002);
    s.free_cam = 1;
    s.fov_mul_goal = 2.0;
    let mut o = Ops::default();
    camera::update_camera(&mut s, 1.0, &mut o);
    assert_eq!((s.fov_mul, o.n), (1.002, 0));
    assert_eq!(camera::clamp_pitch_goal(3.0, -1.0, 1.0), 1.0);
    assert_eq!(camera::clamp_pitch_goal(3.0, 0.0, 0.0), 3.0);
}

#[test]
fn head_offset_reaching_its_goal_stops_moving() {
    let mut adds = [HeadPosAdd {
        moving: 1,
        goal: v3(0.0, 1.0, 0.0),
        speed: 1.0,
        slowdown_dist: 0.0,
        add: v3(0.0, 1.0, 0.0),
    }];
    let (total, sum) = camera::update_head_pos_add(&mut adds, v3(0.0, 1.5, 0.0), 0.1);
    assert_eq!(adds[0].moving, 0);
    assert_eq!(total.y, 2.5);
    assert_eq!(sum.y, 1.0);
}

#[test]
fn spin_and_terror_and_speed_average() {
    let v = camera::spin_head(0.0, 0.0, 8.0);
    assert_eq!((v.x, v.y), (8.0, 0.0));
    assert!(camera::update_head_spin(Vector2f::default(), 40.0, 0.1).is_none());
    let (s, p, y) = camera::update_head_spin(Vector2f { x: 8.0, y: 0.0 }, 40.0, 0.1).unwrap();
    assert_eq!((s.x, p, y), (4.0, 0.0, 0.8));
    assert_eq!(camera::terror_step(0.995, true, 0.33, 0.2, 1.0), 1.0);
    assert_eq!(camera::terror_step(0.1, false, 0.33, 0.2, 1.0), 0.0);
    assert!(camera::prev_speeds_pop(61, 60));
    assert!(!camera::prev_speeds_pop(60, 60));
    assert!(camera::avg_speed(&[]).is_nan());
    assert_eq!(camera::avg_speed(&[1.0, 3.0]), 2.0);
}

#[test]
fn collapse_wakes_after_sleeping_and_ends_when_height_is_back() {
    let mut s = Collapse {
        active: 1,
        state: 1,
        height_add: -0.8,
        height_add_goal: -0.8,
        height_add_awake_speed: 0.4,
        sleep_time: 1.0,
        t: 1.5,
        random_count: 5.0,
        awaken_sanity: 40.0,
        wake_up_speed_mul: 0.5,
        ..Collapse::default()
    };
    let mut o = Ops::default();
    helpers::collapse_update(&mut s, 0.1, &mut o);
    assert_eq!(s.state, 2);
    assert!(kinds(&o).contains(&helpers::k::PLAYER_SET_SANITY));
    for _ in 0..100 {
        let mut o = Ops::default();
        helpers::collapse_update(&mut s, 0.1, &mut o);
    }
    assert_eq!(s.active, 0);
}

#[test]
fn sway_list_changes_come_back_as_ops() {
    let mut s = helpers::CamDir {
        start_max_sanity: 15.0,
        max_positions: 2,
        ..helpers::CamDir::default()
    };
    let list = [Vector2f { x: 1.0, y: 1.0 }, Vector2f { x: 3.0, y: 3.0 }];
    let mut o = Ops::default();
    helpers::camdir_update(&mut s, &list, 10.0, 0.1, &mut o);
    assert_eq!(
        kinds(&o),
        [helpers::k::LIST_PUSH, helpers::k::LIST_POP_FRONT]
    );
    assert_eq!(s.sway_add.x, 1.5);
    assert_eq!(s.sway_active, 1);
}

#[test]
fn hurt_noise_and_pant_follow_health() {
    let mut s = Hurt {
        effect_start_health: 50.0,
        min_move_mul: 0.75,
        max_pant_count: 15.0,
        pant_speed: 1.0,
        pant_size: 0.1,
        noise_freq: 10.0,
        noise_count: 8,
        pant_pos_add_dir: 1.0,
        ..Hurt::default()
    };
    let mut o = Ops::default();
    helpers::hurt_update(&mut s, 60.0, false, 0.0, 0.1, &mut o);
    assert_eq!(o.n, 0);
    helpers::hurt_update(&mut s, 10.0, false, 0.0, 0.1, &mut o);
    assert_eq!(
        o.ops.first().map(|x| x.kind),
        Some(helpers::k::SET_HURT_MOVE_SPEED_MUL)
    );
    assert!(helpers::hurt_regain(10.0, 20.0, 1.0, 0.5).is_some());
    assert!(helpers::hurt_regain(30.0, 20.0, 1.0, 0.5).is_none());
}

#[test]
fn look_at_reports_arrival() {
    let mut s = LookAt {
        speed_mul: 1.0,
        max_speed: 9999.0,
        ..LookAt::default()
    };
    let mut o = Ops::default();
    helpers::lookat_update(&mut s, v3(0.0, 0.0, 0.0), 0.0, 0.0, 0.1, &mut o);
    assert_eq!(
        kinds(&o),
        [
            helpers::k::ADD_PITCH,
            helpers::k::ADD_YAW,
            helpers::k::SYNC_BODY_YAW,
            helpers::k::AT_TARGET
        ]
    );
}

#[test]
fn flashback_ends_with_the_voices_and_queues_next() {
    let mut s = Flashback {
        active: 1,
        radial_blur_size: 0.09,
        ..Flashback::default()
    };
    let mut o = Ops::default();
    helpers::flashback_update(&mut s, false, true, true, 0.1, &mut o);
    assert_eq!((s.active, s.start_count), (0, 6.0));
    assert_eq!(
        o.ops.first().map(|x| (x.kind, x.i)),
        Some((helpers::k::FLASHBACK_END, 1))
    );
    let mut s = Flashback {
        start_count: 0.05,
        ..Flashback::default()
    };
    let mut o = Ops::default();
    helpers::flashback_update(&mut s, true, false, false, 0.1, &mut o);
    assert_eq!(kinds(&o), [helpers::k::FLASHBACK_START_QUEUED]);
}

#[test]
fn lantern_oil_runs_out_and_activation_is_gated() {
    let mut s = Lantern {
        active: 1,
        alpha: 1.0,
        lower_oil_speed: 10.0,
        fade_light_oil_amount: 10.0,
        color: [1.0; 4],
    };
    let mut o = Ops::default();
    assert!(helpers::lantern_update(&mut s, 0.5, false, 0.1, &mut o));
    assert_eq!(
        kinds(&o),
        [
            helpers::k::LANTERN_COLOR,
            helpers::k::LANTERN_OUT_OF_OIL,
            helpers::k::LANTERN_SET_OIL
        ]
    );
    let mut idle = Lantern::default();
    assert!(!helpers::lantern_update(
        &mut idle,
        5.0,
        false,
        0.1,
        &mut Ops::default()
    ));
    let p = LanternSet {
        active: true,
        use_effects: true,
        check_oil_items: true,
        check_allowed: true,
        current: false,
        allowed: true,
        has_item: true,
        disabled: false,
        oil: 0.0,
    };
    let mut o = Ops::default();
    helpers::lantern_set_active(&p, &mut o);
    assert_eq!(
        kinds(&o),
        [
            helpers::k::LANTERN_HINT_NO_OIL,
            helpers::k::LANTERN_SOUND_OUT
        ]
    );
    let mut o = Ops::default();
    helpers::lantern_set_active(&LanternSet { oil: 5.0, ..p }, &mut o);
    assert_eq!(o.n, 4);
}

#[test]
fn light_level_combines_the_maximum() {
    let mut count = 0.0;
    assert!(helpers::light_timer(&mut count, 0.1));
    assert!(!helpers::light_timer(&mut count, 0.1));
    assert_eq!(count, 0.4);
    let (e, n) = helpers::light_combine(true, &[0.2; 5], &[0.9, 0.1, 0.1, 0.1, 0.1]);
    assert_eq!((e, n), (1.0, 1.0));
    let (e, n) = helpers::light_combine(
        false,
        &[0.2, 0.5, 0.1, 0.1, 0.1],
        &[0.9, 0.1, 0.1, 0.1, 0.1],
    );
    assert_eq!((e, n), (0.5, 0.9));
}

#[test]
fn darkness_drains_sanity_and_hints_once() {
    let mut s = Darkness {
        min_darkness_light_level: 0.15,
        ambient_light_min_light_level: 0.2,
        loop_sound_startup_time: 1.0,
        sanity_loss_per_second: 0.2,
        sanity_loss_mul: 0.5,
        ..Darkness::default()
    };
    let i = DarknessIn {
        ext_level: 0.0,
        normal_level: 0.0,
        hard_mode: true,
        flashback_active: false,
        drain_disabled: false,
        emotion_flash: false,
    };
    let mut o = Ops::default();
    helpers::darkness_update(&mut s, &i, 0.1, &mut o);
    assert_eq!(
        kinds(&o),
        [
            helpers::k::AMBIENT_ON,
            helpers::k::LOWER_SANITY,
            helpers::k::HINT_DARKNESS
        ]
    );
    assert_eq!((s.in_darkness, s.show_hint_timer), (1, 3.0));
}

#[test]
fn death_phases_hand_over_in_order() {
    let mut s = Death {
        active: 1,
        state: 1,
        fade_alpha: 1.0,
        text_alpha1: 1.0,
        text_on_screen_count: 6.0,
        show_hint: 1,
        fade_out_time: 3.0,
        height_add_speed: 1.2,
        ..Death::default()
    };
    let mut o = Ops::default();
    helpers::death_phase1(&mut s, false, 0.1, &mut o);
    assert_eq!(s.state, 2);
    let tail: Vec<u32> = kinds(&o).into_iter().rev().take(3).collect();
    assert_eq!(
        tail,
        [
            helpers::k::DEATH_AWAKEN_SOUND,
            helpers::k::DEATH_FLASH,
            helpers::k::DEATH_RESET_GAME
        ]
    );
    let mut o = Ops::default();
    helpers::death_phase2(&mut s, true, 0.6, &mut o);
    assert_eq!(s.state, 3);
    let mut o = Ops::default();
    helpers::death_phase2(&mut s, true, 10.0, &mut o);
    assert_eq!(
        kinds(&o),
        [helpers::k::DEATH_RESET, helpers::k::DEATH_AUTOLOAD]
    );
    let mut o = Ops::default();
    helpers::death_phase3(4, true, &mut o);
    assert_eq!(kinds(&o), [helpers::k::DEATH_EXIT]);
}

#[test]
fn sanity_low_timer_hit_zoom_and_scan() {
    let mut s = Sanity {
        sanity_low_limit: 30.0,
        sanity_low_limit_max_time: 300.0,
        sanity_low_new_sanity_amount: 40.0,
        at_low_sanity_count: 299.95,
        check_near_enemy_interval: 0.3,
        hit_zoom_in_speed: 3.5,
        ..Sanity::default()
    };
    let mut o = Ops::default();
    sanity::sanity_begin(&mut s, 10.0, 0.1, &mut o);
    assert_eq!(kinds(&o), [sanity::k::PLAYER_SET_SANITY]);
    assert!(sanity::sanity_scan_due(&mut s, 0.1));
    assert!(!sanity::sanity_scan_due(&mut s, 0.1));
    s.hit_active = 1;
    let mut o = Ops::default();
    sanity::sanity_update_hit(&mut s, false, 0.1, &mut o);
    assert_eq!((s.hit_is_updated, s.sanity_effect_updated, o.n), (1, 1, 3));
    let mut o = Ops::default();
    sanity::sanity_update_hit(&mut s, true, 0.1, &mut o);
    assert_eq!(o.n, 0);
}

#[test]
fn insane_effects_reset_after_a_hit() {
    let mut s = Sanity {
        sanity_effects_start: 65.0,
        sanity_effect_updated: 1,
        ..Sanity::default()
    };
    let mut o = Ops::default();
    sanity::sanity_insane_effects(&mut s, 90.0, false, 0.1, &mut o);
    assert_eq!(s.sanity_effect_updated, 0);
    assert_eq!(
        kinds(&o),
        [
            sanity::k::IMAGE_TRAIL,
            sanity::k::FADE_ASPECT,
            sanity::k::FADE_FOV,
            sanity::k::INSANITY_ACTIVE
        ]
    );
    s.hit_is_updated = 1;
    let mut o = Ops::default();
    sanity::sanity_insane_effects(&mut s, 90.0, false, 0.1, &mut o);
    assert_eq!(o.n, 0);
}

#[test]
fn hands_state_machine_and_gates() {
    let mut o = Ops::default();
    assert_eq!(
        hands::hands_update(hands::DISABLED, true, true, true, &mut o),
        (hands::DISABLED, false)
    );
    assert_eq!(
        hands::hands_update(hands::HOLSTER, true, true, false, &mut o),
        (hands::DRAW, true)
    );
    assert_eq!(
        hands::hands_update(hands::HOLSTER, false, true, false, &mut o),
        (hands::DISABLED, true)
    );
    assert_eq!(
        hands::hands_update(hands::HAND_OBJECT, true, true, true, &mut o),
        (hands::IDLE, false)
    );
    assert_eq!(
        hands::hands_update(hands::DRAW, false, true, false, &mut o),
        (hands::DRAW, true)
    );
    assert!(hands::hands_action_gate(hands::IDLE, true));
    assert!(!hands::hands_action_gate(hands::DRAW, true));
    assert_eq!(hands::hands_switch_plan(true, true), hands::PLAN_HOLSTER);
    assert_eq!(hands::hands_switch_plan(false, true), hands::PLAN_DRAW);
    assert!(!hands::rotations_pop(1, -1));
    assert!(hands::rotations_pop(7, 6));
    assert!(!hands::rotations_pop(5, 6));
}

#[test]
fn hands_rotation_weights_newest_most() {
    let r = hands::hands_final_rotation(&[v3(0.0, 0.0, 0.0), v3(1.0, 1.0, 1.0)]);
    assert!((r.x - 0.8).abs() < 1e-6);
    assert!(hands::hands_final_rotation(&[]).x.is_nan());
}
