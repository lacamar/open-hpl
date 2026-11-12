use super::area::*;
use super::handler::*;
use super::helper::*;
use super::map::*;
use crate::math::{Matrixf, Vector3f, v3};

#[test]
fn timers() {
    assert_eq!(timer_count(0.0), 0.001);
    assert_eq!(timer_count(-3.0), 0.001);
    assert_eq!(timer_count(2.0), 2.0);
    assert!(timer_due(0.0, false));
    assert!(!timer_due(0.0, true));
    assert!(!timer_due(0.1, false));
    assert!(callback_remove_ok(b"", b"x"));
    assert!(!callback_remove_ok(b"a", b"b"));
}

#[test]
fn free_ids() {
    assert_eq!(free_entity_id(&[]), 0);
    assert_eq!(free_entity_id(&[0, 1, 2]), 3);
    assert_eq!(free_entity_id(&[0, 2, 3]), 1);
    assert_eq!(free_entity_id(&[1]), 0);
}

#[test]
fn completion() {
    let mut cur = 3;
    let (p, n) = completion_add(&mut cur, 10, 2);
    assert_eq!((p, n, cur), (0.3, 0.5, 5));
    let mut cur = 3;
    assert_eq!(completion_add(&mut cur, 0, 2), (0.0, 0.0));
    assert_eq!(completion_value(COMPLETE_CHEST, &[1, 2, 3, 4, 5]), 4);
    assert_eq!(completion_value(COMPLETE_NONE, &[1, 2, 3, 4, 5]), 0);
}

#[test]
fn sticky_decisions() {
    assert_eq!(sticky_inside(false, true), INSIDE_FORGET_LAST);
    assert_eq!(sticky_inside(false, false), INSIDE_SKIP);
    assert_eq!(sticky_inside(true, true), INSIDE_SKIP);
    assert_eq!(sticky_inside(true, false), INSIDE_PROCEED);
    assert_eq!(sticky_name(b"", b"x"), (false, false));
    assert_eq!(sticky_name(b"key", b"big_key_1"), (false, true));
    assert_eq!(sticky_name(b"key", b"door"), (true, false));
    assert!(sticky_after_callback(true, false));
    assert!(!sticky_after_callback(false, false));
}

#[test]
fn sticky_attach_modes() {
    let mut i = StickyAttachIn {
        pose_time: 0.0,
        move_body: true,
        rotate_body: true,
        ..StickyAttachIn::default()
    };
    i.start.m = [
        [1.0, 0.0, 0.0, 0.0],
        [0.0, 1.0, 0.0, 0.0],
        [0.0, 0.0, 1.0, 0.0],
        [0.0, 0.0, 0.0, 1.0],
    ];
    i.area_world = i.start;
    let o = sticky_attach(&i);
    assert!(o.done && o.set_time == 1.0 && o.mode == ATTACH_SET_MATRIX);
    i.move_body = false;
    i.rotate_body = false;
    let o = sticky_attach(&i);
    assert!(o.done && o.mode == ATTACH_NONE);
    i.move_body = true;
    i.pose_time = 2.0;
    i.step = 0.5;
    i.set_time = 0.0;
    i.area_pos = v3(4.0, 0.0, 0.0);
    let o = sticky_attach(&i);
    assert_eq!(o.set_time, 0.25);
    assert_eq!(o.pos.x, 1.0);
    assert!(!o.done);
}

#[test]
fn ticks() {
    let mut c = 1.0;
    assert!(!tick(&mut c, 0.5));
    assert!(tick_fixed(&mut c, 0.5, 0.1));
    assert_eq!(c, 0.1);
    let mut n = f32::NAN;
    assert!(tick(&mut n, 1.0));
    let mut e = 0.5;
    examine_tick(&mut e, 0.2);
    assert_eq!(e, 0.3);
    let mut z = 0.0f32;
    examine_tick(&mut z, 1.0);
    assert_eq!(z, 0.0);
}

#[test]
fn examine() {
    let i = ExamineIn {
        has_cat: true,
        has_insane_cat: true,
        has_insane_entry: true,
        has_sound: true,
        has_insane_sound: false,
    };
    let o = examine_interact(i, 10.0, 50.0, 0.0);
    assert!(o.show_text && o.insane_text && o.play_sound && !o.insane_sound);
    let o = examine_interact(i, 90.0, 50.0, 0.2);
    assert!(o.show_text && !o.insane_text && !o.play_sound);
}

#[test]
fn ladder_search_quirk() {
    let mut tries = 0;
    let r = ladder_find_start(v3(0.0, 0.0, 0.0), 5.0, &mut |_| {
        tries += 1;
        tries == 3
    });
    assert!((r.map_or(0.0, |p| p.y) - 0.3).abs() < 1e-5);
    let r = ladder_find_start(v3(0.0, 0.0, 0.0), 5.0, &mut |_| true);
    assert_eq!(r.map(|p| p.y), Some(0.0));
    assert!(ladder_find_start(v3(0.0, 0.0, 0.0), 1.0, &mut |_| false).is_none());
    assert!(ladder_find_start(v3(0.0, f32::NAN, 0.0), 1.0, &mut |_| false).is_none());
}

#[test]
fn rope() {
    assert_eq!(rope_max_segments(10.0, 0.5), 24);
    assert_eq!(rope_max_segments(1.0, 0.0), i32::MAX.wrapping_add(4));
    assert_eq!(rope_max_segments(f32::NAN, 1.0), 4);
    assert_eq!(rope_after_world(true, true), ROPE_RELINK);
    assert_eq!(rope_after_world(true, false), ROPE_RETURN);
    assert_eq!(rope_after_world(false, true), ROPE_CREATE);
}

#[test]
fn liquid() {
    assert_eq!(liquid_char(true, false, true, false), CHAR_ENTER);
    assert_eq!(liquid_char(true, false, false, true), CHAR_LEAVE);
    assert_eq!(liquid_char(false, false, true, false), CHAR_NONE);
    let b = liquid_body(
        BuoyIn {
            inside: true,
            allowed: true,
            active: false,
            has_waves: true,
        },
        1.0,
        4.0,
    );
    assert!(b.setup && b.waves && !b.deactivate);
    assert!(liquid_body(BuoyIn::default(), 0.0, 0.0).deactivate);
    assert!(body_skipped(true, true, 0.0, false));
    assert!(!body_skipped(true, true, 0.0, true));
    let p = liquid_surface_plane(1.0, 2.0);
    assert_eq!((p.a, p.b, p.c, p.d), (0.0, 1.0, 0.0, -2.0));
}

#[test]
fn calls() {
    assert_eq!(call1(b"F", b"area"), b"F(\"area\")");
    assert_eq!(call2(b"F", b"a", b"b"), b"F(\"a\",\"b\")");
    assert_eq!(checkpoint_call(b"cb", b"_auto", 3), b"cb(\"_auto\", 3)");
    assert_eq!(
        compile_errors(b"ok\nERR a\n\nxx ERR b\n"),
        b" ERR a\n xx ERR b\n"
    );
}

#[test]
fn sound_broadcast() {
    assert_eq!(sound_volume(1.0, 0.0, 10.0, 1.0), None);
    assert_eq!(sound_volume(5.0, 0.0, 10.0, 2.0), Some(1.0));
    assert_eq!(sound_volume(20.0, 10.0, 10.0, 1.0), Some(0.0));
    assert_eq!(sound_volume(5.0, 10.0, 10.0, 1.0), Some(1.0));
    assert!(sound_prefix_matches(b"enemy_grunt", b"enemy"));
    assert!(!sound_prefix_matches(b"en", b"enemy"));
}

#[test]
fn hard_mode() {
    let every = hard_tinderbox_every(0);
    assert!((every - 0.7).abs() < 1e-6);
    let mut c = 0.0;
    let removed = (0..10)
        .filter(|_| hard_remove(&mut c, HARD_OIL_EVERY))
        .count();
    assert_eq!(removed, 6);
}

#[test]
fn flags_and_scripts() {
    assert_eq!(
        world_load_flags(false, true, false, true, [1, 2, 4, 8]),
        1 | 2 | 8
    );
    assert_eq!(script_choice(true, true, true), SCRIPT_PLAIN);
    assert_eq!(script_choice(false, true, true), SCRIPT_COMPRESSED);
    assert_eq!(script_choice(false, false, true), SCRIPT_NONE);
    assert_eq!(
        enter_plan(true, true, true),
        ENTER_START | ENTER_COMPLETION | ENTER_ENTER
    );
    assert_eq!(enter_plan(true, true, false), ENTER_ENTER);
    assert_eq!(enter_plan(false, true, true), 0);
    assert_eq!(commentary_sync(true, true), None);
    assert_eq!(commentary_sync(false, true), Some(true));
}

#[test]
fn handler_plans() {
    assert_eq!(map_name(b"maps/08_Cellar_Maze.map"), b"08_cellar_maze");
    assert_eq!(change_file(b"x.hpm"), b"x.map");
    assert!(!is_new_map(b"a", b"dir/A.map"));
    assert!(is_new_map(b"a", b"b.map"));
    assert_eq!(
        pair_events(b"08_cellar_maze", b"09_back_hall", false),
        EVENT_ESCAPE_ARTIST
    );
    assert_eq!(
        pair_events(b"27_torture_chancel_redux", b"28_inner_sanctum", false),
        0
    );
    assert_eq!(
        pair_events(b"27_torture_chancel_redux", b"28_inner_sanctum", true),
        EVENT_SANITY
    );
    assert!(set_current_plan(false, false, true).is_empty());
    assert_eq!(
        set_current_plan(false, false, false),
        [SET_ASSIGN, SET_VIEW_NO_WORLD]
    );
    assert_eq!(set_current_plan(true, true, false).len(), 10);
    let mut p = false;
    assert!(pause_sounds(&mut p) && !pause_sounds(&mut p));
    assert!(resume_sounds(&mut p) && !resume_sounds(&mut p));
    assert_eq!(load_seconds(1000, 3500), 2.5);
    assert!(change_proceeds(true, false));
    assert!(!change_proceeds(true, true));
    assert!(!change_proceeds(false, false));
}

#[test]
fn helper_filters() {
    let sub = SubMesh {
        parent_is_body: true,
        has_material: true,
        translucent: false,
        shadow_caster: false,
    };
    assert!(los_before(false, true, false, false, true, &[sub]));
    assert!(!los_before(false, true, true, true, true, &[sub]));
    assert!(los_before(false, true, true, true, false, &[]));
    assert!(!los_before(true, true, false, false, false, &[]));
    assert_eq!(closest_entity_before(true, true, false, false), CLOSEST_ASK);
    assert_eq!(
        closest_entity_before(false, false, true, true),
        CLOSEST_REJECT
    );
    let f = DamageFilter {
        active: true,
        collide: true,
        is_char: true,
        is_player_body: true,
        ..DamageFilter::default()
    };
    assert!(damage_skips(f));
    assert!(!damage_skips(DamageFilter {
        check_player: true,
        ..f
    }));
    let pts = [v3(0.0, 0.0, 0.0), v3(2.0, 4.0, 6.0)];
    assert_eq!(hit_position(&pts), v3(1.0, 2.0, 3.0));
    let d = hit_direction(v3(0.0, 3.0, 0.0), v3(0.0, 0.0, 0.0));
    assert_eq!(d, v3(0.0, 1.0, 0.0));
    assert_eq!(prop_impulse(d, 10.0, 2.0, 3.0).y, 3.0);
    assert_eq!(max_rgb(0.1, 0.5, 0.3), 0.5);
    assert_eq!(light_falloff_add(0.5, 1.0, 5.0, 4.0, 1.0), 0.5);
    assert_eq!(light_kind(false, true, true), LIGHT_NEEDS_LOS);
}

#[test]
fn nodes() {
    let mut m = Matrixf::default();
    m.m[0][0] = 1.0;
    m.m[1][1] = 1.0;
    m.m[2][2] = 1.0;
    m.m[0][3] = 3.0;
    let s = player_start(&m);
    assert_eq!(
        s.pos,
        Vector3f {
            x: 3.0,
            y: 0.05,
            z: 0.0
        }
    );
    let l = ladder_setup(&m, 1.0, 2.0);
    assert_eq!((l.max_y, l.min_y), (2.0, 0.0));
}
