use super::game;
use super::pause::{self, Op, Pause};
use super::player::{self, DamageId, Fade, MessageKind, MoveVec};
use super::scriptable::{self, Fader, Timer, TimerAdd};
use super::transfer::{self, AfterLoad, Body, Preload, Restore, Take, Taken};
use crate::math::{IDENTITY, matrix_mul_vec, matrix_translate, v3};

#[test]
fn hash_matches_the_official_fnv_variant() {
    assert_eq!(scriptable::hash64(b""), 0);
    assert_eq!(
        scriptable::hash64(b"a"),
        (0x1465_0fb0_739d_0383u64 ^ 0x61).wrapping_mul(0x0000_0100_0000_01b3_u64)
    );
    let neg = scriptable::hash64(&[0xff]);
    assert_eq!(
        neg,
        (0x1465_0fb0_739d_0383u64 ^ u64::MAX).wrapping_mul(0x0000_0100_0000_01b3_u64)
    );
}

#[test]
fn easing_endpoints_and_default() {
    assert_eq!(scriptable::easing(0, 0.3), 0.3);
    assert_eq!(scriptable::easing(99, 0.3), 0.3);
    for k in 1..=30 {
        assert!(scriptable::easing(k, 0.0).abs() < 1e-5, "kind {k} at 0");
        assert!(
            (scriptable::easing(k, 1.0) - 1.0).abs() < 1e-5,
            "kind {k} at 1"
        );
    }
}

#[test]
fn one_shot_timer_fires_once_and_repeat_survives() {
    let mut once = Timer {
        id: 1,
        length: 1.0,
        time_left: 1.0,
        repeat: false,
        paused: false,
    };
    let mut rep = Timer {
        id: 2,
        length: 1.0,
        time_left: 1.0,
        repeat: true,
        paused: false,
    };
    assert_eq!(scriptable::step_timer(&mut once, 0.4), (false, true));
    assert_eq!(scriptable::step_timer(&mut once, 0.6), (true, false));
    assert_eq!(scriptable::step_timer(&mut rep, 1.5), (true, true));
    assert!((rep.time_left - 0.5).abs() < 1e-6);
    rep.paused = true;
    assert_eq!(scriptable::step_timer(&mut rep, 9.0), (false, true));
    let mut nan = Timer {
        id: 3,
        length: 1.0,
        time_left: f32::NAN,
        repeat: false,
        paused: false,
    };
    assert_eq!(scriptable::step_timer(&mut nan, 1.0), (false, true));
}

#[test]
fn fader_runs_to_goal_and_reverses() {
    let mut f = [Fader {
        id: 1,
        value: 0.0,
        start: 0.0,
        goal: 1.0,
        speed: 1.0,
        reverse_at_end: true,
        paused: false,
    }];
    scriptable::update_faders(&mut f, 0.5);
    assert_eq!(f[0].value, 0.5);
    scriptable::update_faders(&mut f, 0.5);
    assert_eq!(
        (f[0].value, f[0].goal, f[0].start, f[0].reverse_at_end),
        (1.0, 0.0, 1.0, false)
    );
    assert_eq!(scriptable::fade_speed(0.0, 2.0, 0.0), 1e9);
    assert_eq!(scriptable::fade_speed(0.0, -2.0, 4.0), 0.5);
}

#[test]
fn timer_value_and_has_passed() {
    assert_eq!(scriptable::timer_value(None, 2.0, 4.0, 0, false), 4.0);
    assert_eq!(
        scriptable::timer_value(Some((2.0, 1.0)), 0.0, 1.0, 0, false),
        0.5
    );
    assert_eq!(
        scriptable::timer_value(Some((2.0, 1.0)), -1.0, 0.0, 0, true),
        0.5
    );
    assert!(scriptable::timer_time_has_passed(None, 100.0));
    assert!(!scriptable::timer_time_has_passed(Some((2.0, 1.5)), 1.0));
    assert_eq!(
        scriptable::fader_value(Some(2.0), 0.0, 10.0, 0, false),
        10.0
    );
    assert_eq!(scriptable::timer_add(true, false), TimerAdd::Skip);
    assert_eq!(scriptable::timer_add(true, true), TimerAdd::Overwrite);
    assert_eq!(scriptable::timer_add(false, false), TimerAdd::Push);
}

#[test]
fn same_map_ignores_dir_ext_and_case() {
    assert!(transfer::same_map(
        b"maps/01_03_Upsilon.hpm",
        b"01_03_upsilon"
    ));
    assert!(!transfer::same_map(b"01_03", b"01_04"));
    assert!(transfer::same_map(b"", b""));
}

#[test]
fn transfer_needs_a_preloaded_map_and_no_start_position() {
    let b = transfer::begin(true, false, true, b"b.hpm", b"B.hpm");
    assert!(b.use_transfer && b.preloaded && b.lookup_area);
    // open-hpl-dbi: a start position drops the transfer
    let b = transfer::begin(false, false, true, b"b.hpm", b"b.hpm");
    assert!(!b.use_transfer && b.preloaded && !b.lookup_area);
    let b = transfer::begin(true, false, true, b"", b"b.hpm");
    assert!(b.use_transfer && !b.preloaded && !b.lookup_area);
    let b = transfer::begin(true, false, true, b"a.hpm", b"b.hpm");
    assert!(!b.preloaded && !b.lookup_area);
    assert!(!transfer::begin(true, false, false, b"b", b"b").lookup_area);
}

#[test]
fn transfer_keeps_pose_relative_to_the_area() {
    let old = matrix_translate(v3(10.0, 0.0, 0.0));
    let body = Body {
        feet: v3(11.0, 2.0, 3.0),
        yaw: -1.0,
        active_size: 1,
    };
    let rel = transfer::capture(&old, &body, Some(0.25));
    assert_eq!((rel.pitch, rel.active_size), (0.25, 1));
    let new = matrix_translate(v3(-5.0, 1.0, 0.0));
    let a = transfer::arrive(&new, &rel);
    assert_eq!((a.feet.x, a.feet.y, a.feet.z), (-4.0, 3.0, 3.0));
    assert!(a.crouching);
    assert!(
        (a.yaw - (transfer::start_yaw(&new) + (-1.0 - transfer::start_yaw(&old)))).abs() < 1e-5
    );
    assert_eq!(transfer::capture(&old, &body, None).pitch, 0.0);
    assert!(transfer::carries(
        &rel.inv,
        v3(10.0, 0.0, 0.0),
        v3(2.0, 2.0, 2.0)
    ));
    assert!(!transfer::carries(
        &rel.inv,
        v3(12.5, 0.0, 0.0),
        v3(2.0, 2.0, 2.0)
    ));
    assert_eq!(matrix_mul_vec(&IDENTITY, v3(1.0, 2.0, 3.0)).y, 2.0);
}

#[test]
fn after_load_and_restore_decisions() {
    assert_eq!(
        transfer::after_load(false, true, true),
        AfterLoad::LoadFailed
    );
    assert_eq!(transfer::after_load(true, true, true), AfterLoad::Transfer);
    assert_eq!(transfer::after_load(true, false, true), AfterLoad::Done);
    assert_eq!(transfer::after_load(true, true, false), AfterLoad::Done);
    assert_eq!(transfer::restore_prop(false, true, true), Restore::Skip);
    assert_eq!(transfer::restore_prop(true, false, true), Restore::Skip);
    assert_eq!(
        transfer::restore_prop(true, true, true),
        Restore::MakeDynamicThenSet
    );
    assert_eq!(transfer::restore_prop(true, true, false), Restore::Set);
}

#[test]
fn preload_starts_throttles_and_pays_debt() {
    let mut st = Preload::default();
    let t = transfer::preload_tick(&mut st, b"b.hpm", b"", false, 1);
    assert!(t.start && !t.drop_old && t.step && t.budget_ms == 4);
    transfer::preload_stepped(&mut st, 10, 4);
    assert_eq!(st.debt_ms, 6.0);
    let t = transfer::preload_tick(&mut st, b"b.hpm", b"b.hpm", true, 1);
    assert!(!t.step);
    let t = transfer::preload_tick(&mut st, b"b.hpm", b"b.hpm", true, 2);
    assert!(!t.step);
    assert_eq!(st.debt_ms, 2.0);
    let t = transfer::preload_tick(&mut st, b"b.hpm", b"b.hpm", true, 3);
    assert!(!t.step);
    let t = transfer::preload_tick(&mut st, b"b.hpm", b"b.hpm", true, 4);
    assert!(t.step);
    let t = transfer::preload_tick(&mut st, b"c.hpm", b"b.hpm", true, 5);
    assert!(t.drop_old && t.start);
    st.prio = 0;
    assert!(!transfer::preload_tick(&mut st, b"c.hpm", b"c.hpm", true, 9).step);
    assert!(transfer::preload_ready(b"b.hpm", false, b"B"));
    assert!(!transfer::preload_ready(b"b.hpm", true, b"B"));
    assert_eq!(transfer::take_plan(b"", b"b"), Take::None);
    assert_eq!(
        transfer::take_plan(b"a", b"b"),
        Take::Finish { same: false }
    );
    assert_eq!(transfer::taken(true, true), Taken::Use);
    assert_eq!(
        transfer::taken(true, false),
        Taken::Discard {
            destroy_world: true
        }
    );
    assert_eq!(
        transfer::taken(false, true),
        Taken::Discard {
            destroy_world: false
        }
    );
}

#[test]
fn pause_pauses_each_group_once_and_resume_clears() {
    let mut st = Pause::default();
    assert!(pause::set_paused(&mut st, false, false, true, true, true, 7).is_empty());
    let ops = pause::set_paused(&mut st, false, true, true, true, true, 7);
    assert_eq!(
        ops,
        [
            Op::SetScriptPaused(true),
            Op::WorldActive(false),
            Op::MusicPause,
            Op::SoundsPause(7)
        ]
    );
    let ops = pause::set_paused(&mut st, true, true, true, true, true, 7);
    assert_eq!(ops, [Op::SetScriptPaused(true), Op::WorldActive(false)]);
    let ops = pause::set_paused(&mut st, true, false, true, true, true, 7);
    assert_eq!(ops.first(), Some(&Op::SetScriptPaused(false)));
    assert_eq!(ops.last(), Some(&Op::ReapplyVoicePauses));
    assert!(!st.music && !st.sounds && !st.ambience);
    let ops = pause::set_paused(&mut st, false, true, false, true, false, 1);
    assert_eq!(ops[2..], [Op::SoundsPause(1), Op::SoundscapePaused(false)]);
    let ops = pause::set_paused(&mut st, true, true, false, false, true, 1);
    assert_eq!(ops[2..], [Op::SoundscapePaused(true)]);
}

#[test]
fn saving_lock_counts_down() {
    let mut st = Pause::default();
    pause::disable_saving(&mut st, 1.0);
    pause::tick_saving(&mut st, 0.6);
    assert!(!st.saving_enabled);
    pause::tick_saving(&mut st, 0.6);
    assert!(st.saving_enabled);
    pause::disable_saving(&mut st, 0.0);
    pause::tick_saving(&mut st, 1.0);
    assert!(!st.saving_enabled);
}

#[test]
fn frame_plans() {
    let f = pause::frame_plan(true, true, true, true, false);
    assert!(f.game_input && f.game_update && f.map_update && !f.update_when_paused);
    let f = pause::frame_plan(true, true, true, true, true);
    assert!(!f.map_update && f.update_when_paused && f.game_paused);
    assert_eq!(
        pause::post_plan(false, false, false, true, true, true),
        (false, false)
    );
    assert_eq!(
        pause::post_plan(true, false, true, true, true, true),
        (false, false)
    );
    assert_eq!(
        pause::post_plan(true, false, false, true, true, false),
        (true, false)
    );
}

#[test]
fn game_routing() {
    assert_eq!(game::route_action(true), game::Route::ModulesOnly);
    assert!(!game::analog_allowed(true));
    assert!(game::visits_effect(false, false) && !game::visits_effect(false, true));
    assert_eq!(
        game::string_duration(2.5, 1.5, 0.07, 10),
        2.5f32.max(1.5 + 10.0 * 0.07)
    );
    assert_eq!(game::pointer_mode(-1, false, false), Some(true));
    assert_eq!(game::pointer_mode(1, true, false), Some(false));
    assert_eq!(game::pointer_mode(1, true, true), None);
    assert!(game::pointer_moved(false, 0, 1) && !game::pointer_moved(false, 0, 0));
    assert_eq!(game::action_edge(300, 256, true, false), game::Edge::None);
    assert_eq!(game::action_edge(3, 256, true, false), game::Edge::Down);
    assert_eq!(game::action_edge(3, 256, false, true), game::Edge::Up);
    assert_eq!(game::analog_fire(1, 2, 0.5, true), Some((2, 0.5)));
    assert_eq!(game::analog_fire(1, 3, 0.5, true), None);
    assert_eq!(game::analog_fire(-1, 0, 0.5, true), None);
    assert_eq!(game::analog_fire(1, 0, 0.0, true), None);
}

#[test]
fn fade_towards_snaps_when_close() {
    let mut f = Fade {
        value: 0.0,
        goal: 1.0,
        speed: 1.0,
        max_speed: 0.0,
        speed_mul: 0.0,
    };
    assert_eq!(player::fade_towards(&mut f, 0.25), 0.25);
    assert_eq!(player::fade_towards(&mut f, 10.0), 1.0);
    let mut g = Fade {
        value: 0.0,
        goal: 1.0,
        speed: 0.0,
        ..Fade::default()
    };
    assert_eq!(player::fade_towards(&mut g, 0.1), 1.0);
}

#[test]
fn camera_move_vector_arrives_and_stops() {
    let mut m = MoveVec {
        goal: v3(1.0, 0.0, 0.0),
        acc: 100.0,
        max_speed: 100.0,
        moving: true,
        ..MoveVec::default()
    };
    player::step_move_vec(&mut m, 0.001);
    assert!(m.moving && m.value.x > 0.0);
    player::step_move_vec(&mut m, 1.0);
    assert!(!m.moving && m.value.x == 1.0 && m.speed == 0.0);
}

#[test]
fn keyed_multipliers() {
    assert_eq!(player::fov_mul(2.0, &[0.5, 3.0]), 3.0);
    assert_eq!(player::visibility_range_mul(&[]), 1.0);
    assert_eq!(player::visibility_max_range(&[-1.0, 5.0, 3.0]), 3.0);
    assert_eq!(player::visibility_max_range(&[-1.0]), -1.0);
}

#[test]
fn automove_arrives_or_moves() {
    let fwd = v3(0.0, 0.0, -1.0);
    let right = v3(1.0, 0.0, 0.0);
    assert_eq!(
        player::step_automove(v3(0.0, 5.0, 0.01), v3(0.0, 0.0, 0.0), fwd, right, 1.0),
        player::Automove::Arrived
    );
    match player::step_automove(v3(0.0, 0.0, -2.0), v3(0.0, 0.0, 0.0), fwd, right, 1.0) {
        player::Automove::Move { forward, right } => assert_eq!((forward, right), (1.0, 0.0)),
        other => panic!("{other:?}"),
    }
}

#[test]
fn messages_and_states() {
    assert_eq!(player::message_kind(b"void Reset()"), MessageKind::Reset);
    assert_eq!(
        player::message_kind(b"void OnMapEnter(cLuxMap @apMap)"),
        MessageKind::MapEnter
    );
    assert_eq!(player::message_kind(b"void Other()"), MessageKind::Other);
    assert!(player::drops_state_on_reset(MessageKind::Reset, true, 3));
    assert!(!player::drops_state_on_reset(MessageKind::Reset, true, 0));
    assert!(!player::drops_state_on_reset(
        MessageKind::MapEnter,
        true,
        3
    ));
    assert_eq!(
        player::state_change(false, false),
        player::StateChange::Missing
    );
    assert_eq!(player::state_change(true, true), player::StateChange::Same);
    assert_eq!(player::place_plan(false, false), None);
    assert!(player::place_plan(true, false).is_some_and(|p| p.set_feet_and_yaw));
    assert!(player::place_plan(true, true).is_some_and(|p| !p.set_feet_and_yaw));
    assert!(player::clears_camera_adds(false) && !player::clears_camera_adds(true));
}

#[test]
fn health_and_damage_ids() {
    assert_eq!(player::set_health(1.0, 1.0, true, -1.0), (-1.0, true));
    assert_eq!(player::set_health(-1.0, 1.0, true, -2.0), (-2.0, false));
    assert_eq!(player::set_health(1.0, 1.0, false, -1.0), (-1.0, false));
    assert_eq!(player::set_health(0.5, 1.0, true, 9.0), (1.0, false));
    assert_eq!(player::damage_id(&[1, 2], -1), DamageId::Untracked);
    assert_eq!(player::damage_id(&[1, 2], 2), DamageId::Reject);
    assert_eq!(player::damage_id(&[1, 2], 3), DamageId::Push);
    assert_eq!(
        player::damage_id(&[0, 1, 2, 3, 4, 5, 6, 7], 8),
        DamageId::EvictPush
    );
    assert_eq!(player::health_after_damage(1.0, 3.0, 0.2), 0.2);
    assert_eq!(player::health_after_script_damage(1.0, 0.5, 0.0), 0.5);
    assert_eq!(player::health_after_script_damage(1.0, 0.5, 2.0), 1.0);
    assert_eq!(player::health_after_script_damage(1.0, 3.0, -1.0), 0.0);
    assert_eq!(player::add_health(1.0, -5.0, 0.0), 0.0);
}

#[test]
fn mouse_smoothing_weights_recent_samples() {
    assert_eq!(player::mouse_vector(10, -20, 2.0, 100.0), (0.2, -0.4));
    let (x, y) = player::smooth_mouse(&[(1.0, 0.0), (0.0, 1.0)], 0.5);
    assert!((x - 1.0 / 1.5).abs() < 1e-6 && (y - 0.5 / 1.5).abs() < 1e-6);
    assert!(player::pops_oldest_sample(8, 7) && !player::pops_oldest_sample(7, 7));
}

#[test]
fn move_speed_settings_mirror_the_speeds() {
    let s = player::MoveSpeeds {
        max_forward: 4.0,
        max_backward: 2.0,
        max_sideway: 3.0,
        max_forward_mul: 0.5,
        max_backward_mul: 1.0,
        max_sideway_mul: 2.0,
        forward_acc: 7.0,
        ..Default::default()
    };
    let v = player::move_speed_settings(&s);
    assert_eq!(&v[..5], &[2.0, -2.0, 6.0, -6.0, 7.0]);
}

#[test]
fn angle_delta_wraps() {
    assert!((player::angle_delta(0.0, 7.0) - (7.0 - 2.0 * std::f32::consts::PI)).abs() < 1e-4);
    assert!(player::angle_delta(1.0, 1.0).abs() < 1e-6);
}
