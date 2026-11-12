use super::move_normal::{
    BobCfg, BobIn, BobState, FallCfg, JumpState, LedgeCfg, MoveMulIn, SpeedCfg, bounce_starts,
    fall_damage, head_bob, jump_tick, ledge_update, move_limits, move_speed_mul, movement_update,
    speed_multipliers,
};
use super::states_base::{
    self as sb, ACT_ATTACK, ACT_HOLSTER, ACT_INTERACT, ACT_LANTERN, PS_HAND, PS_NONE, PS_NORMAL,
};
use super::states_grab::{
    GrabEnterIn, GrabEnterOut, PushBegin, PushBeginOut, grab_enter, grab_scroll, leave_limit,
    push_begin, push_pick,
};
use super::states_ladder::{
    BOTTOM_CHECK_RAYS, BOTTOM_NO, BOTTOM_YES, LadderEnterIn, LadderEnterOut, LadderState,
    SOUND_DOWN, SOUND_UP, ladder_at_bottom, ladder_attach, ladder_enter, ladder_move_post,
    ladder_move_pre,
};
use super::states_rotate::{rotate_slow, wheel_pop, wheel_speed_add};
use super::states_vec::Pid3;
use crate::math::v3;
use crate::types::{Matrixf, Vector2f, Vector3f};

fn pid() -> Pid3 {
    let mut p = Pid3::default();
    p.init(3, 2.0, 0.0, 0.5);
    p
}

#[test]
fn pid_output_is_proportional_then_adds_the_derivative() {
    let mut p = pid();
    let first = p.output(v3(1.0, 0.0, 0.0), 0.5);
    assert_eq!(first.x, 2.0);
    let second = p.output(v3(2.0, 0.0, 0.0), 0.5);
    assert_eq!(second.x, 2.0 * 2.0 + 0.5 * ((2.0 - 1.0) / 0.5));
    p.reset();
    assert_eq!((p.error_num, p.last_num), (0, -1));
    assert_eq!(p.steps, [0.0; 20]);
}

#[test]
fn empty_pid_buffer_outputs_zero_instead_of_indexing() {
    let mut p = Pid3::default();
    assert_eq!(p.output(v3(1.0, 2.0, 3.0), 0.1), Vector3f::default());
}

#[test]
fn move_speed_mul_applies_water_last_and_only_in_water() {
    let mut i = MoveMulIn {
        interaction: 0.5,
        script: 1.0,
        event: 1.0,
        hurt: 1.0,
        collapse: 1.0,
        water: 0.5,
        in_water: 0,
    };
    assert_eq!(move_speed_mul(&i), 0.5);
    i.in_water = 1;
    assert_eq!(move_speed_mul(&i), 0.25);
}

#[test]
fn speed_multipliers_follow_ground_crouch_run_and_air() {
    let c = SpeedCfg {
        crouch_fwd: 0.5,
        crouch_back: 0.4,
        crouch_side: 0.3,
        run_fwd: 2.0,
        run_back: 1.5,
        run_side: 1.2,
        air_fwd: 0.2,
        air_back: 0.1,
        air_side: 0.05,
    };
    let walk = speed_multipliers(&c, 1.0, 1.0, true, false, false);
    assert_eq!((walk.fwd, walk.default_fwd), (1.0, 1.0));
    let crouch = speed_multipliers(&c, 1.0, 1.0, true, true, true);
    assert_eq!((crouch.fwd, crouch.default_fwd), (0.5, 0.5));
    let run = speed_multipliers(&c, 1.0, 0.5, true, false, true);
    assert_eq!((run.fwd, run.default_fwd), (1.5, 2.0));
    let air = speed_multipliers(&c, 3.0, 1.0, false, true, true);
    assert_eq!((air.fwd, air.side, air.default_fwd), (0.2, 0.05, 0.2));
    let l = move_limits(4.0, 2.0, 3.0, &run);
    assert_eq!(
        (l.fwd_pos, l.fwd_neg, l.side_neg),
        (6.0, -2.0 * run.back, -3.0 * run.side)
    );
}

#[test]
fn standing_up_while_running_happens_once_per_move_start() {
    let mut was = false;
    assert!(movement_update(true, true, &mut was, true, false, false));
    assert!(was);
    assert!(!movement_update(true, true, &mut was, true, false, false));
    assert!(!movement_update(false, true, &mut was, true, false, false));
    assert!(!was);
}

#[test]
fn held_jump_counts_up_and_ends_at_the_maximum() {
    let mut s = JumpState {
        count: 0.0,
        max: 0.3,
        jumping: 1,
    };
    assert!(jump_tick(&mut s, 0.2));
    assert_eq!(s.jumping, 1);
    assert!(jump_tick(&mut s, 0.2));
    assert_eq!((s.count, s.jumping), (0.3, 0));
    assert!(!jump_tick(&mut s, 0.2));
}

#[test]
fn fall_damage_picks_the_hardest_tier_and_needs_positive_damage() {
    let c = FallCfg {
        speed_min: 5.0,
        damage_min: 10.0,
        speed_med: 8.0,
        damage_med: 30.0,
        speed_max: 11.0,
        damage_max: 60.0,
    };
    assert_eq!(fall_damage(&c, -4.0), (0, 0.0));
    assert_eq!(fall_damage(&c, -6.0), (1, 10.0));
    assert_eq!(fall_damage(&c, -9.0), (2, 30.0));
    assert_eq!(fall_damage(&c, -12.0), (3, 60.0));
    let zero = FallCfg {
        damage_min: 0.0,
        ..c
    };
    assert_eq!(fall_damage(&zero, -6.0).0, 0);
    assert!(bounce_starts(0.5, -9.0, 5.0, false));
    assert!(!bounce_starts(0.5, -9.0, 5.0, true));
}

#[test]
fn head_bob_steps_when_the_sine_crosses_its_lowest_point() {
    let c = BobCfg {
        walk_max: Vector2f { x: 0.01, y: 0.02 },
        run_max: Vector2f { x: 0.02, y: 0.03 },
        crouch_max: Vector2f { x: 0.005, y: 0.01 },
        walk_min_speed: 1.0,
        walk_max_speed: 2.0,
        max_forward_speed: 3.0,
        ..BobCfg::default()
    };
    let mut s = BobState {
        default_fwd_mul: 1.0,
        ..BobState::default()
    };
    let i = BobIn {
        velocity: v3(1.5, 0.0, 0.0),
        on_ground: 1,
        moved_last_update: 1,
        ..BobIn::default()
    };
    let mut steps = 0;
    for _ in 0..200 {
        if head_bob(&mut s, &c, &i, 0.02) {
            steps += 1;
        }
    }
    assert!(steps > 0 && s.bobbing == 1);
    let idle = BobIn::default();
    for _ in 0..200 {
        head_bob(&mut s, &c, &idle, 0.02);
    }
    assert_eq!(s.bobbing, 0);
    assert_eq!(s.head_bob, Vector3f::default());
}

#[test]
fn ledge_update_returns_control_inside_the_give_distance() {
    let c = LedgeCfg {
        give_control_dist: 0.1,
        pitch_factor: 2.0,
        max_pitch_speed: 1.0,
    };
    assert_eq!(ledge_update(&c, v3(0.05, 0.0, 0.0), 0.3, 0.1), None);
    assert!((ledge_update(&c, v3(1.0, 0.0, 0.0), 0.3, 0.1).unwrap_or(0.0) + 0.06).abs() < 1e-6);
    assert_eq!(ledge_update(&c, v3(1.0, 0.0, 0.0), 9.0, 0.5), Some(-0.5));
}

#[test]
fn action_tables_route_holster_attack_and_interact() {
    assert_eq!(sb::normal_action(ACT_HOLSTER, true).change, PS_HAND);
    assert_eq!(sb::normal_action(ACT_HOLSTER, false).pass, 1);
    assert_eq!(sb::hand_object_action(ACT_HOLSTER, true).change, PS_NORMAL);
    let a = sb::hand_object_action(ACT_ATTACK, false);
    assert_eq!((a.pass, a.forward_to_hands, a.change), (0, 1, PS_NONE));
    let u = sb::use_item_action(ACT_INTERACT, true);
    assert_eq!((u.use_item, u.change, u.pass), (1, PS_NORMAL, 0));
    assert_eq!(sb::interact_action(ACT_INTERACT, false), 1);
    assert_eq!(sb::interact_action(ACT_ATTACK, true), 2);
    assert_eq!(sb::interact_action(ACT_ATTACK, false), 0);
    assert_eq!(sb::ladder_action(ACT_LANTERN, false), (false, true));
    assert_eq!(sb::ladder_action(ACT_INTERACT, true), (true, false));
    assert_eq!(
        sb::default_base_interact(ACT_INTERACT, true, true),
        (false, true)
    );
    assert!(sb::hand_object_hides(PS_NORMAL) && !sb::hand_object_hides(sb::PS_HAND));
}

#[test]
fn crosshair_pick_falls_back_to_default_and_simple_styles() {
    assert_eq!(sb::crosshair_pick(3, 0, 7, false, true), 3);
    assert_eq!(
        sb::crosshair_pick(3, 0, 7, false, false),
        sb::CROSSHAIR_SIMPLE_GFX
    );
    assert_eq!(
        sb::crosshair_pick(7, 0, 7, false, true),
        sb::CROSSHAIR_DEFAULT_GFX
    );
    assert_eq!(
        sb::crosshair_pick(3, 0, 7, true, true),
        sb::CROSSHAIR_DEFAULT_GFX
    );
    assert!(sb::focus_in_range(true, 1.0, 2.0) && !sb::focus_in_range(true, f32::NAN, 2.0));
    assert!(sb::outline_gate(true, true, f32::NAN, 1.0));
}

#[test]
fn use_item_plans_and_draw_gates_follow_the_focus() {
    assert_eq!(sb::use_item_max_focus(1.0, 2.0), 2.0);
    assert_eq!(
        sb::use_item_focus(false, true, true, 0.0, 9.0),
        sb::PLAN_NONE
    );
    assert_eq!(
        sb::use_item_focus(true, true, true, 1.0, 2.0),
        sb::PLAN_CALLBACK
    );
    assert_eq!(
        sb::use_item_focus(true, true, false, 1.0, 2.0),
        sb::PLAN_NO_OBJECT
    );
    assert_eq!(sb::use_item_plan(false), sb::PLAN_DOES_NOT_WORK);
    assert_eq!(sb::use_item_draw(true, 5.0, 2.0, false, false), sb::DRAW_NO);
    assert_eq!(sb::use_item_draw(true, 1.0, 2.0, true, true), sb::DRAW_NO);
    assert_eq!(sb::use_item_draw(true, 1.0, 2.0, true, false), sb::DRAW_YES);
    assert_eq!(
        sb::use_item_draw(true, 1.0, 2.0, false, false),
        sb::DRAW_CHECK_CALLBACK
    );
}

fn ident() -> Matrixf {
    crate::math::IDENTITY
}

#[test]
fn grab_enter_slows_the_player_by_carried_mass() {
    let i = GrabEnterIn {
        cam_pos: v3(0.0, 0.0, 2.0),
        body_matrix: ident(),
        use_offset: 1,
        use_depth: 1,
        depth: 1.2,
        max_depth: 2.0,
        proper_mass_sum: 15.0,
        min_slow_mass: 10.0,
        max_slow_mass: 20.0,
        min_slow_mul: 0.5,
        ..GrabEnterIn::default()
    };
    let mut o = GrabEnterOut::default();
    grab_enter(&i, &mut o);
    assert_eq!((o.slow, o.depth), (1, 1.2));
    assert!((o.slow_mul - 0.75).abs() < 1e-6);
    assert!((o.max_distance - (2.0 * 1.1 + 0.2)).abs() < 1e-6);
    let heavy = GrabEnterIn {
        proper_mass_sum: 25.0,
        ..i
    };
    grab_enter(&heavy, &mut o);
    assert_eq!(o.slow_mul, 0.5);
    let light = GrabEnterIn {
        proper_mass_sum: 5.0,
        ..i
    };
    grab_enter(&light, &mut o);
    assert_eq!(o.slow, 0);
}

#[test]
fn grab_depth_is_clamped_and_leave_speed_is_capped() {
    assert_eq!(grab_scroll(1.0, 10.0, 0.1, 0.5, 1.5), 1.5);
    assert_eq!(grab_scroll(1.0, -10.0, 0.1, 0.5, 1.5), 0.5);
    assert_eq!(leave_limit(v3(3.0, 0.0, 0.0), 6.0), None);
    let v = leave_limit(v3(0.0, 8.0, 0.0), 4.0).unwrap_or_default();
    assert!((v.y - 4.0).abs() < 1e-6);
}

#[test]
fn push_direction_is_zero_until_moving_then_normalised() {
    let mut i = PushBegin {
        at_point: 0,
        body_matrix: ident(),
        max_attach_dist: 5.0,
        forward: v3(0.0, 0.0, -1.0),
        right: v3(1.0, 0.0, 0.0),
        ..PushBegin::default()
    };
    let mut o = PushBeginOut::default();
    push_begin(&i, &mut o);
    assert_eq!((o.too_far, o.no_movement), (0, 1));
    i.moving = [1, 1];
    i.total_move_mul = [2.0, 2.0];
    push_begin(&i, &mut o);
    assert_eq!(o.no_movement, 0);
    assert!((o.push_dir.x - std::f32::consts::FRAC_1_SQRT_2).abs() < 1e-5);
    i.feet = v3(100.0, 0.0, 0.0);
    push_begin(&i, &mut o);
    assert_eq!(o.too_far, 1);
    assert_eq!(
        push_pick(true, true, false, (1.0, 2.0), (3.0, 4.0), (5.0, 6.0)),
        (3.0, 4.0)
    );
    assert_eq!(
        push_pick(false, true, true, (1.0, 2.0), (3.0, 4.0), (5.0, 6.0)),
        (1.0, 2.0)
    );
}

#[test]
fn rotate_speed_decays_to_zero_and_resets_without_mouse() {
    let mut s = 1.0;
    rotate_slow(&mut s, Vector2f { x: 1.0, y: 0.0 }, 2.0, 0.1);
    assert!((s - 0.8).abs() < 1e-6);
    rotate_slow(&mut s, Vector2f::default(), 2.0, 0.1);
    assert_eq!(s, 0.0);
    let mut n = -1.0;
    rotate_slow(&mut n, Vector2f { x: 1.0, y: 0.0 }, 100.0, 1.0);
    assert_eq!(n, 0.0);
}

#[test]
fn wheel_needs_four_samples_and_a_circular_gesture() {
    assert!(!wheel_pop(10) && wheel_pop(11));
    let pin = v3(0.0, 0.0, 1.0);
    let fwd = v3(0.0, 0.0, 1.0);
    let few = vec![Vector2f { x: 1.0, y: 0.0 }; 3];
    assert_eq!(
        wheel_speed_add(&few, Vector2f { x: 1.0, y: 0.0 }, pin, fwd),
        0.0
    );
    let line: Vec<Vector2f> = (0..8)
        .map(|k| Vector2f {
            x: k as f32 + 1.0,
            y: 0.0,
        })
        .collect();
    // a gesture with no movement divides 0 by 0, and NaN passes the distance test
    assert_eq!(
        wheel_speed_add(&line, Vector2f { x: 3.0, y: 4.0 }, pin, fwd),
        5.0
    );
    let arc: Vec<Vector2f> = (0..8)
        .map(|k| {
            let a = k as f32 * 0.3;
            Vector2f {
                x: a.cos(),
                y: a.sin(),
            }
        })
        .collect();
    let s = wheel_speed_add(&arc, Vector2f { x: 3.0, y: 4.0 }, pin, fwd);
    assert_eq!(s.abs(), 5.0);
}

#[test]
fn ladder_attach_time_depends_on_height_and_ends_the_phase() {
    let mut o = LadderEnterOut::default();
    let i = LadderEnterIn {
        pos: v3(0.0, 5.0, 0.0),
        start: v3(1.0, 5.0, 0.0),
        max_y: 4.0,
        ..LadderEnterIn::default()
    };
    ladder_enter(&i, &mut o);
    assert_eq!(o.time, 1.2);
    ladder_enter(&LadderEnterIn { max_y: 9.0, ..i }, &mut o);
    assert_eq!(o.time, 0.5);
    assert!((o.pos_add.x - 2.0).abs() < 1e-6);
    let mut s = LadderState {
        time_count: 0.3,
        pos_add: o.pos_add,
        ..LadderState::default()
    };
    let mut a = super::states_ladder::LadderAttachOut::default();
    ladder_attach(&mut s, 0.2, &mut a);
    assert_eq!(a.done, 0);
    ladder_attach(&mut s, 0.2, &mut a);
    assert_eq!(a.done, 1);
}

#[test]
fn ladder_climb_sounds_fire_per_step_length_and_on_release() {
    let mut s = LadderState {
        move_mul: 1.0,
        ..LadderState::default()
    };
    assert_eq!(ladder_move_pre(&mut s, v3(0.0, 1.0, 0.0), 2.0, 1.0, 0.1), 0);
    assert!((s.char_pos.y - 1.2).abs() < 1e-6);
    assert_eq!(ladder_move_post(&mut s, 2.0, 1.0, 0.3, 0.1), 0);
    assert_eq!(ladder_move_post(&mut s, 2.0, 1.0, 0.3, 0.1), SOUND_UP);
    assert_eq!(s.played_sound, 1);
    s.move_mul = 0.0;
    s.step_count = -0.1;
    s.played_sound = 0;
    assert_eq!(
        ladder_move_pre(&mut s, v3(0.0, 1.0, 0.0), 2.0, 1.0, 0.1),
        SOUND_DOWN
    );
    assert_eq!(ladder_at_bottom(0.0, 0.0, 1.0, 0.0), BOTTOM_NO);
    assert_eq!(ladder_at_bottom(-1.0, 0.0, 1.0, 0.0), BOTTOM_YES);
    assert_eq!(ladder_at_bottom(-1.0, 3.0, 1.0, 0.0), BOTTOM_CHECK_RAYS);
}
