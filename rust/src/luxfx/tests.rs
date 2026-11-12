use super::effects::{self, Amount, Emotion, Fade, Flash, Shake, k as ek};
use super::insanity::{self, Bug, BugsParams, Steps, Stream, Timing};
use super::misc::{self, Completion, Date, Vitals};
use super::music::{self, Enemy, k as mk};
use super::ops::Ops;
use super::postfx::{self, Clock};
use super::renderer::{self, Glow, Outline, op};
use super::voice::{self, AddIn, TextsIn, UpdateIn};
use crate::math::{c_srand, rand_rectf};
use crate::types::{Matrixf, Vector3f};

fn kinds(o: &Ops) -> Vec<u32> {
    o.as_slice().iter().map(|x| x.kind).collect()
}

#[test]
fn amount_fades_down_and_stops() {
    let mut s = Amount {
        amount: 1.0,
        goal: 0.0,
        speed: 2.0,
        active: 1,
    };
    let mut o = Ops::default();
    amount_step(&mut s, 0.25, &mut o);
    assert_eq!(s.amount, 0.5);
    assert_eq!(s.active, 1);
    assert_eq!(kinds(&o), [ek::AMOUNT_PARAMS]);
    let mut o = Ops::default();
    amount_step(&mut s, 0.5, &mut o);
    assert_eq!((s.amount, s.active), (0.0, 0));
    assert_eq!(kinds(&o), [ek::AMOUNT_PARAMS, ek::AMOUNT_OFF]);
}

fn amount_step(s: &mut Amount, dt: f32, o: &mut Ops) {
    effects::amount_update(s, dt, o);
}

#[test]
fn amount_fades_up_to_goal() {
    let mut s = Amount {
        amount: 0.0,
        goal: 0.15,
        speed: 3.0,
        active: 1,
    };
    let mut o = Ops::default();
    effects::amount_update(&mut s, 1.0, &mut o);
    assert_eq!((s.amount, s.active), (0.15, 0));
}

#[test]
fn fade_out_then_in() {
    let mut f = Fade::default();
    effects::fade_reset(&mut f);
    effects::fade_out(&mut f, 2.0);
    assert!(effects::fade_is_fading(&f));
    effects::fade_update(&mut f, 1.0);
    assert_eq!(f.alpha, 0.5);
    effects::fade_update(&mut f, 5.0);
    assert_eq!(f.alpha, 1.0);
    assert!(!effects::fade_is_fading(&f));
    effects::fade_in(&mut f, 0.0);
    assert_eq!((f.alpha, f.active), (0.0, 1));
    effects::fade_direct(&mut f, 0.0);
    assert_eq!(f.active, 0);
}

#[test]
fn flash_runs_three_steps() {
    let mut s = Flash::default();
    effects::flash_start(&mut s, 0.5, 0.0, 1.0);
    assert_eq!(s.white_speed, 1.0 / 0.000001f32);
    effects::flash_update(&mut s, 1.0);
    assert_eq!((s.alpha, s.step, s.count), (1.0, 1, 1.0));
    effects::flash_update(&mut s, 1.0);
    assert_eq!(s.step, 2);
    effects::flash_update(&mut s, 2.0);
    assert_eq!(s.active, 0);
}

#[test]
fn emotion_text_time_then_exit_ops() {
    let mut s = Emotion::default();
    effects::emotion_start(&mut s, 10);
    assert_eq!(s.text_time, 3.0 + 0.15 * 10.0);
    let mut o = Ops::default();
    effects::emotion_update(&mut s, 2.0, &mut o);
    assert_eq!(s.step, 1);
    effects::emotion_action(&mut s, false);
    assert_eq!(s.text_time, 3.0 + 0.15 * 10.0);
    effects::emotion_action(&mut s, true);
    assert_eq!(s.text_time, 0.0);
    let mut o = Ops::default();
    effects::emotion_update(&mut s, 0.1, &mut o);
    assert_eq!(s.step, 2);
    assert_eq!(
        kinds(&o),
        [ek::EMO_FOV_OUT, ek::EMO_UNPAUSE, ek::EMO_RADIAL_OUT]
    );
}

#[test]
fn shake_drops_finished_and_draws_in_order() {
    let live = Shake {
        max_size: 2.0,
        size: 0.0,
        time: 1.0,
        fade_in_time: 0.0,
        max_fade_in_time: 1.0,
        fade_out_time: 1.0,
        max_fade_out_time: 1.0,
    };
    let done = Shake {
        time: 0.0,
        fade_out_time: 0.0,
        ..live
    };
    let mut v = [done, live];
    let mut o = Ops::default();
    c_srand(3);
    let (n, active) = effects::shake_update(&mut v, 0.5, &mut o);
    assert_eq!((n, active), (1, true));
    assert_eq!(v[0].time, 0.5);
    c_srand(3);
    let x = rand_rectf(-2.0, 2.0);
    assert_eq!(o.as_slice()[0].a, x);

    let mut last = [done];
    let mut o = Ops::default();
    let (n, active) = effects::shake_update(&mut last, 0.5, &mut o);
    assert_eq!((n, active), (0, false));
    assert_eq!(kinds(&o), [ek::SHAKE_END]);
}

#[test]
fn voice_interrupts_by_priority() {
    let mut o = Ops::default();
    let base = AddIn {
        active: 1,
        list_empty: 0,
        front_priority: 1,
        priority: 2,
        remove_interrupted: 1,
        voice_valid: 1,
        effect_valid: 0,
    };
    voice::add(&base, &mut o);
    assert_eq!(
        kinds(&o),
        [
            voice::k::FADE_VOICE,
            voice::k::VOICE_NULL,
            voice::k::EFFECT_NULL,
            voice::k::POP_FRONT,
            voice::k::PUSH_FRONT
        ]
    );
    let mut o = Ops::default();
    voice::add(
        &AddIn {
            priority: -1,
            ..base
        },
        &mut o,
    );
    assert_eq!(kinds(&o), [voice::k::PUSH_BACK]);
}

#[test]
fn voice_text_queue_counts_due_entries() {
    let mut d = [0.5, 0.0, 2.0];
    assert_eq!(voice::text_tick(&mut d, 0.5), 2);
    assert_eq!(d, [0.0, -0.5, 1.5]);
    let mut n = [f32::NAN, 0.0];
    assert_eq!(voice::text_tick(&mut n, 1.0), 0);
}

#[test]
fn voice_update_actions() {
    let mut o = Ops::default();
    let idle = UpdateIn {
        volume_mul: 0.5,
        voice_nonnull: 1,
        voice_valid: 1,
        effect_nonnull: 1,
        effect_valid: 0,
        list_empty: 1,
    };
    assert_eq!(voice::update_a(&idle, &mut o), voice::ACTION_RETURN);
    assert_eq!(kinds(&o), [voice::k::VOL_VOICE]);
    let mut o = Ops::default();
    let gone = UpdateIn {
        voice_valid: 0,
        voice_nonnull: 0,
        volume_mul: 1.0,
        ..idle
    };
    assert_eq!(voice::update_a(&gone, &mut o), voice::ACTION_FINALIZE);
    let busy = UpdateIn {
        list_empty: 0,
        ..gone
    };
    assert_eq!(voice::update_a(&busy, &mut o), voice::ACTION_PLAY);
    assert_eq!(voice::start_time(1.5, -1.0, 100.0), 1.5);
    assert_eq!(voice::start_time(1.5, 10.0, 12.0), 3.5);
}

#[test]
fn voice_texts_wrap_or_queue() {
    let mut o = Ops::default();
    let t = TextsIn {
        nonempty_mask: 0b101,
        delays: [0.0, 0.0, 4.0, 0.0, 0.0, 0.0, 0.0],
        list_size: 1,
        voice_null: 1,
        effect_null: 1,
    };
    assert!(voice::update_b(&t, &mut o));
    assert_eq!(
        kinds(&o),
        [voice::k::CLEAR_TEXT, voice::k::WRAP_NOW, voice::k::QUEUE]
    );
    assert_eq!(o.as_slice()[2].i, 2);
}

#[test]
fn rows_stack_from_the_bottom() {
    let r = voice::voice_rows(3, 600.0, 18.0);
    assert_eq!(
        r.iter().map(|r| r.y).collect::<Vec<_>>(),
        [520.0, 540.0, 560.0]
    );
    assert_eq!(r[0].x, 100.0);
    let s = voice::stream_rows(2);
    assert_eq!((s[0].x, s[0].y, s[1].y), (250.0, 540.0, 560.0));
    let e = voice::emotion_rows(1, 20.0);
    assert_eq!((e[0].align, e[0].y), (voice::ALIGN_CENTER, 289.0));
    assert_eq!(voice::emotion_rows(0, 20.0).len(), 0);
}

#[test]
fn flash_plan_culls_and_draws_twice() {
    let objs = [
        Glow {
            collides: 1,
            alpha: 0.5,
        },
        Glow {
            collides: 0,
            alpha: 1.0,
        },
    ];
    let p = renderer::plan_flash(&objs, 1.0, true);
    let draws = p.ops().iter().filter(|o| o.kind == op::DRAW).count();
    assert_eq!(draws, 2);
    let f: Vec<f32> = p
        .ops()
        .iter()
        .filter(|o| o.kind == op::PROG_FLOAT)
        .map(|o| o.f[0])
        .collect();
    assert_eq!(f, [0.5]);
    assert!(renderer::plan_flash(&objs, 1.0, false).ops().is_empty());
    assert!(renderer::plan_glow(&[], true).ops().is_empty());
}

#[test]
fn outline_plan_bounds_and_passes() {
    let o = Outline {
        collides: 1,
        has_alpha: 1,
        min: Vector3f {
            x: -1.0,
            y: -1.0,
            z: -1.0,
        },
        max: Vector3f {
            x: 1.0,
            y: 1.0,
            z: 1.0,
        },
        local_min: Vector3f {
            x: -1.0,
            y: -1.0,
            z: -1.0,
        },
        local_max: Vector3f {
            x: 1.0,
            y: 1.0,
            z: 1.0,
        },
        local_center: Vector3f::default(),
        world: Matrixf {
            m: [
                [1.0, 0.0, 0.0, 0.0],
                [0.0, 1.0, 0.0, 0.0],
                [0.0, 0.0, 1.0, 0.0],
                [0.0, 0.0, 0.0, 1.0],
            ],
        },
    };
    let p = renderer::plan_outline(&[o], 0xff);
    assert_eq!(p.clip_min.x, -1.04);
    assert_eq!(p.clip_max.z, 1.04);
    let k: Vec<u32> = p.ops().iter().map(|o| o.kind).collect();
    assert_eq!(k.iter().filter(|x| **x == op::DRAW).count(), 2);
    assert_eq!(k.iter().filter(|x| **x == op::QUAD_MAX_UV).count(), 4);
    let scale = p.ops().iter().find(|o| o.kind == op::MATRIX_TEMP).unwrap();
    assert_eq!(scale.f[0], 1.01);
    assert!(renderer::plan_outline(&[], 0xff).ops().is_empty());
    let none = renderer::plan_outline(&[o], 0);
    assert!(none.ops().iter().all(|o| o.kind != op::PROG_COLOR3));
}

#[test]
fn insanity_clock_wraps_once() {
    let mut c = Clock {
        wave_speed: 2.0,
        ..Clock::default()
    };
    postfx::update(&mut c, 1.0);
    assert_eq!((c.t, c.anim_count), (2.0, 0.15));
    c.anim_count = 2.95;
    postfx::update(&mut c, 1.0);
    assert!((c.anim_count - 0.1).abs() < 1e-6);
}

#[test]
fn insanity_plan_reads_textures_and_guards_range() {
    let c = Clock {
        anim_count: 2.5,
        ..Clock::default()
    };
    let p = postfx::plan(&c, [1280.0, 720.0], true);
    let tex: Vec<(i32, i32)> = p
        .ops()
        .iter()
        .filter(|o| o.kind == postfx::op::TEX)
        .map(|o| (o.a, o.b))
        .collect();
    assert_eq!(tex, [(0, 0), (1, 3), (2, 1), (3, 4)]);
    let big = Clock {
        anim_count: 12.0,
        ..Clock::default()
    };
    let p = postfx::plan(&big, [1.0, 1.0], false);
    let t1 = p
        .ops()
        .iter()
        .find(|o| o.kind == postfx::op::TEX && o.a == 1)
        .unwrap();
    assert_eq!(t1.b, postfx::NONE);
    assert!(p.ops().iter().all(|o| o.kind != postfx::op::FLOAT));
}

#[test]
fn bugs_fade_out_and_finish() {
    let p = BugsParams {
        min_size_mul: 1.0,
        max_size_mul: 1.0,
        wander_dist: 1.0,
        wander_radius: 1.0,
        swarm_mul: 1.0,
        max_speed: 10.0,
    };
    let mut bugs = [Bug::default(); 3];
    c_srand(1);
    insanity::bugs_start(&mut bugs, &p, [-10.0, -10.0], [810.0, 610.0]);
    assert!(bugs.iter().all(|b| b.alpha == 0.0 && b.size_mul == 1.0));
    let mut t = 0.0;
    assert!(insanity::bugs_update(&mut bugs, 3, &p, 0.1, &mut t));
    let mut t = 5.0;
    assert!(!insanity::bugs_update(&mut bugs, 3, &p, 0.1, &mut t));
    assert!(bugs.iter().all(|b| b.alpha > 0.0));
}

#[test]
fn stream_and_steps_state() {
    assert_eq!(
        insanity::stream_load(0.0, 0.0, 2.0),
        (0.001, 1.0 / 0.001, 0.5)
    );
    let mut s = Stream {
        sound_count: 0.5,
        fade_alpha: 0.0,
        fade_alpha_speed: 2.0,
        fade_out_speed: 4.0,
        fade_screen: 1,
    };
    assert!(!insanity::stream_a(&mut s, true, 0.25));
    assert!(insanity::stream_a(&mut s, true, 0.25));
    assert!(!insanity::stream_b(&mut s, true, 0.25));
    assert_eq!(s.fade_alpha_speed, -4.0);
    s.fade_alpha = 0.0;
    assert!(insanity::stream_b(&mut s, true, 0.5));

    let mut st = Steps {
        sound_count: 0.0,
        sound_count_max: 1.0,
        count: 2,
        pos: [3.0, 0.0, 4.0],
        time_mul: 0.5,
        dist_mul: 2.0,
    };
    assert_eq!(insanity::steps_update(&mut st, 0.1), (true, false));
    assert_eq!((st.sound_count, st.sound_count_max), (1.0, 0.5));
    assert!((st.pos[0] - 6.0).abs() < 1e-5);
    assert_eq!(insanity::steps_update(&mut st, 0.25), (false, false));
    st.sound_count = 0.0;
    assert_eq!(insanity::steps_update(&mut st, 0.0), (true, true));
}

#[test]
fn pick_consumes_the_random_range() {
    let dis = [0, 0, 1, 0];
    let max = [100.0; 4];
    let mut used = [1, 0, 0, 0];
    c_srand(9);
    let r = crate::math::rand_rectl(0, 2);
    c_srand(9);
    let p = insanity::pick(50.0, &dis, &mut used, &max);
    let enabled_unused = [1usize, 3];
    assert_eq!(
        p.index,
        r.checked_sub(0)
            .and_then(|i| enabled_unused.get(i as usize))
            .map_or(-1, |i| *i as i32)
    );
    let mut all_used = [1, 1, 0, 1];
    let p = insanity::pick(50.0, &dis, &mut all_used, &max);
    assert_eq!(p.reset_used, 1);
    assert_eq!(all_used, [0, 0, 0, 0]);
    let p = insanity::pick(150.0, &dis, &mut used, &max);
    assert_eq!((p.index, p.reset_used), (-1, 0));
}

#[test]
fn insanity_timing() {
    let t = Timing {
        max_sanity_long: 80.0,
        max_sanity_med: 50.0,
        max_sanity_short: 20.0,
        time_long: 90.0,
        time_med: 60.0,
        time_short: 30.0,
    };
    assert_eq!(insanity::wait_after(&t, 10.0), 30.0);
    assert_eq!(insanity::wait_after(&t, 30.0), 60.0);
    assert_eq!(insanity::wait_after(&t, 99.0), 90.0);
    assert_eq!(insanity::count_down(5.0, -1, 1.0), 4.0);
    assert_eq!(insanity::count_down(5.0, 0, 1.0), 5.0);
    assert!(insanity::new_event_possible(&t, 0.0, 70.0, -1));
    assert!(!insanity::new_event_possible(&t, 0.0, 90.0, -1));
}

#[test]
fn music_priority_stack() {
    assert_eq!(music::prio(12, 10), 10);
    assert_eq!(music::prio(-3, 10), -1);
    let mut out = music::PlayOut::default();
    assert!(!music::play(-1, 2, true, 1.0, 1.0, 1.0, &mut out));
    assert!(music::play(3, 2, false, 0.5, 0.5, 0.5, &mut out));
    assert_eq!((out.play, out.cur_prio, out.volume), (0, 3, 0.25));
    assert!(music::play(2, 2, false, 1.0, 1.0, 0.0, &mut out));
    assert_eq!(
        (out.play, out.cur_prio, out.fade_speed),
        (1, 2, f32::INFINITY)
    );
    let s = music::stop(2, 2, true, 0.5, 2.0);
    assert_eq!((s.clear, s.stop, s.fade_speed), (1, 1, 0.25));
    let s = music::stop(3, 2, true, 0.5, 2.0);
    assert_eq!((s.clear, s.stop), (1, 0));
    assert_eq!(
        music::stop(3, 2, false, 0.0, 2.0),
        music::StopOut::default()
    );

    let mut slots = [music::Slot::default(); 11];
    slots[4] = music::Slot {
        has_file: 1,
        looped: 1,
        resume: 0,
        volume: 1.0,
    };
    slots[7] = music::Slot {
        has_file: 1,
        looped: 0,
        resume: 0,
        volume: 1.0,
    };
    let mut o = Ops::default();
    assert_eq!(music::play_highest(&slots, 10, &mut o), 4);
    assert_eq!(kinds(&o), [mk::CLEAR_SLOT, mk::PLAY]);
    assert_eq!(o.as_slice()[0].i, 7);
    let mut o = Ops::default();
    assert_eq!(music::play_highest(&slots[..4], 3, &mut o), -1);
}

#[test]
fn music_enemy_timing() {
    assert_eq!(music::pick_danger(&[0, 1, 1], &[9, 2, 5]), 2);
    assert_eq!(music::pick_danger(&[0], &[1]), -1);
    assert_eq!(music::pick_enemy(&[1, 1], &[3, 3]), 0);
    assert_eq!(music::pick_enemy(&[1], &[-9999]), -1);

    let mut e = Enemy::default();
    let mut o = Ops::default();
    assert_eq!(
        music::enemy_begin(&mut e, 0, true, false, 0.05, &mut o),
        music::ENEMY_RETURN
    );
    assert_eq!(
        music::enemy_begin(&mut e, 0, true, false, 0.06, &mut o),
        music::ENEMY_SCAN
    );
    music::enemy_found(&mut e, 1, &mut o);
    assert_eq!((e.playing, e.play_count[1]), ([0, 1], 0.0));
    assert_eq!(kinds(&o), [mk::STOP_LOWER, mk::PLAY_ENEMY]);
    assert_eq!(o.as_slice()[0].i, 9);
    let mut o = Ops::default();
    e.close_playing = 1;
    music::enemy_found(&mut e, 0, &mut o);
    assert_eq!(kinds(&o), [mk::STOP_CLOSE, mk::PLAY_ENEMY]);

    let mut e = Enemy::default();
    e.playing[0] = 1;
    let mut o = Ops::default();
    assert_eq!(
        music::enemy_begin(&mut e, 0, true, true, 1.3, &mut o),
        music::ENEMY_DONE
    );
    assert_eq!((e.playing[0], e.stop_count[0]), (0, 0.0));
    assert_eq!(kinds(&o), [mk::STOP_ENEMY]);
    assert_eq!(
        music::enemy_begin(&mut e, 0, false, true, 1.3, &mut o),
        music::ENEMY_RETURN
    );
}

#[test]
fn music_danger_timing() {
    let mut e = Enemy::default();
    assert_eq!(music::danger_begin(true, &e, true), music::DANGER_RETURN);
    assert_eq!(music::danger_begin(false, &e, false), music::DANGER_RETURN);
    assert_eq!(
        music::danger_begin(false, &e, true),
        music::DANGER_SCAN_IDLE
    );
    let mut o = Ops::default();
    music::danger_idle(&mut e, true, 1.5, &mut o);
    assert!(o.as_slice().is_empty());
    music::danger_idle(&mut e, true, 1.0, &mut o);
    assert_eq!((kinds(&o), e.close_playing), (vec![mk::PLAY_DANGER], 1));
    assert_eq!(
        music::danger_begin(false, &e, true),
        music::DANGER_SCAN_PLAYING
    );
    let mut o = Ops::default();
    music::danger_playing(&mut e, false, 7.0, &mut o);
    assert_eq!((kinds(&o), e.close_playing), (vec![mk::STOP_DANGER], 0));
    assert!(music::song_over(2, true) && !music::song_over(-1, true));
}

#[test]
fn progress_log_line_and_header() {
    let v = Vitals {
        health: 99.5,
        sanity: 80.4,
        oil: 12.5,
        tinderboxes: 3,
        coins: 7,
    };
    let l = misc::proglog_line(1, 60 * 60 * 61 + 125, b"01_map.hpm", &v, b"a\nb");
    assert_eq!(
        String::from_utf8(l).unwrap(),
        "# Medium # 01:01:02 | 01_map.hpm | H:100 | S:80 | O:12 | T:3 | C:7 \n > a\n > b\n"
    );
    let cut = misc::proglog_line(9, 0, b"m", &v, b"x\0y");
    assert!(cut.starts_with(b"#  # 00:00:00 | m | ") && cut.ends_with(b"\n > x"));
}

#[test]
fn progress_log_names() {
    let d = Date {
        year: 2026,
        month: 10,
        month_day: 5,
        hours: 7,
        minutes: 8,
        seconds: 9,
    };
    let profile: Vec<u32> = "Ann".chars().map(u32::from).collect();
    let n = misc::proglog_file_name(&profile, &d, 3);
    let s: String = n.iter().filter_map(|c| char::from_u32(*c)).collect();
    assert_eq!(s, "ProgLog_Ann_2026_11_05_07_08_09_03.log");
    let h = misc::proglog_header(b"Ann", &d);
    assert_eq!(
        String::from_utf8(h).unwrap(),
        "--- Amnesia Progress Logger ----\nPlayer: Ann Date: 5/11 - 2026\n---------------------------------\n"
    );
    assert_eq!(misc::proglog_tick(true, i32::MAX), i32::MIN);
    assert_eq!(misc::proglog_tick(false, 4), 4);
}

#[test]
fn completion_is_gated_but_logic_holds() {
    let mut c = Completion {
        alpha: 0.5,
        ..Completion::default()
    };
    misc::completion_update(&mut c, 1.0);
    misc::completion_show(&mut c, 0.0, 0.5, 1.0);
    assert_eq!(
        c,
        Completion {
            alpha: 0.5,
            ..Completion::default()
        }
    );
    misc::completion_show_for_test(&mut c, 0.0, 0.5, 1.0);
    assert_eq!((c.active, c.new_amount, c.disable_delay), (1, 0.5, 2.0));
    c.delay = 0.5;
    misc::completion_step_for_test(&mut c, 0.25);
    assert_eq!(c.delay, 0.25);
    misc::completion_reset(&mut c);
    assert_eq!((c.active, c.alpha), (0, 0.0));
}

#[test]
fn achievement_gates() {
    assert_eq!(misc::unlock_gate(false, true), misc::UNLOCK_NOT_REGISTERED);
    assert_eq!(misc::unlock_gate(true, false), misc::UNLOCK_UNKNOWN);
    assert_eq!(misc::unlock_gate(true, true), misc::UNLOCK_OK);
}
