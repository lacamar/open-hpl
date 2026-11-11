use super::help::{self, CMD_BAD, CMD_BUTTON, CMD_LOOK, OP_CHAR, OP_COMMAND};
use super::lookup::{self, Wild};
use super::tables;
use super::types::{self, AlphaFader};
use super::vars;

fn wide(s: &str) -> Vec<u32> {
    s.chars().map(u32::from).collect()
}

#[test]
fn wildcard_requires_every_part_in_any_order() {
    let w = Wild::new(b"*door*key*");
    assert!(w.matches(b"key_door"));
    assert!(w.matches(b"a_door_key"));
    assert!(!w.matches(b"door"));
    assert!(Wild::new(b"*").matches(b""));
    assert!(lookup::has_wildcard(b"a*"));
    assert!(!lookup::has_wildcard(b"a"));
}

#[test]
fn type_prep_defers_subtype_reads() {
    assert_eq!(lookup::type_prep(0, 5, 3), lookup::TYPE_YES);
    assert_eq!(lookup::type_prep(0, 1, -1), lookup::TYPE_NO);
    assert_eq!(lookup::type_prep(1, 1, -1), lookup::TYPE_YES);
    assert_eq!(lookup::type_prep(1, 1, 2), lookup::TYPE_NEED_AREA);
    assert_eq!(lookup::type_prep(2, 2, 2), lookup::TYPE_NEED_PROP);
}

#[test]
fn body_suffix_matches_after_entity_name() {
    assert!(lookup::body_matches(b"door_hinge", 4, b"hinge"));
    assert!(!lookup::body_matches(b"door_hinge", 4, b"hing"));
    assert!(lookup::body_matches(b"door", 4, b""));
    assert!(lookup::searches_bodies(b"x", 2));
    assert!(!lookup::searches_bodies(b"x", 1));
    assert!(!lookup::searches_bodies(b"", 2));
}

#[test]
fn var_arithmetic_uses_c_number_text() {
    assert_eq!(vars::add_int(b"7abc", 3), b"10");
    assert_eq!(vars::add_int(b"", i32::MAX), b"2147483647");
    assert_eq!(vars::add_int(b"1", i32::MAX), b"-2147483648");
    assert_eq!(vars::add_float(b"1.5", 1.0), b"2.50000000000000000000");
    assert_eq!(vars::get_int(b""), 0);
    assert_eq!(vars::get_float(b"2.5x"), 2.5);
    assert_eq!(vars::from_float(f32::NAN), b"nan");
}

#[test]
fn name_tables() {
    assert_eq!(tables::progress_level(b"HIGH"), 2);
    assert_eq!(tables::damage_type(b"Slash"), 2);
    assert_eq!(tables::crosshair(b"Ladder"), 6);
    assert_eq!(tables::crosshair(b"nope"), -1);
    assert_eq!(tables::pose(b"biped"), -1);
    assert_eq!(tables::pose(b"Quadruped"), 1);
    assert!(tables::achievement_hardmode(tables::achievement(
        b"Survivor"
    )));
    assert!(!tables::achievement_hardmode(tables::achievement(
        b"Quitter"
    )));
    assert_eq!(tables::achievement(b"survivor"), 0);
    assert_eq!(tables::quest_achievement(b"21FindOrb"), 10);
    assert!(tables::sound_is_entity(b"sfx/door"));
    assert!(tables::sound_is_entity(b"x.snt"));
    assert!(!tables::sound_is_entity(b"x.ogg"));
    assert!(tables::coord_is_local(b"LoCaL"));
}

#[test]
fn sanity_boosts() {
    assert_eq!(tables::sanity_boost(10.0), 90.0);
    assert_eq!(tables::sanity_boost(60.0), 20.0);
    assert_eq!(tables::sanity_boost(f32::NAN), 5.0);
    assert_eq!(tables::sanity_boost_small(30.0), 15.0);
}

#[test]
fn fade_speed_and_segments() {
    assert_eq!(tables::fade_speed(0.0), 0.0);
    assert_eq!(tables::fade_speed(4.0), 0.25);
    assert!(tables::inverse(0.0).is_infinite());
    assert_eq!(tables::rope_segments(10.0, 3.0), 7);
    assert_eq!(tables::rope_segments(f32::NAN, 1.0), 4);
    assert_eq!(
        tables::rope_segments(f32::INFINITY, 1.0),
        i32::MAX.wrapping_add(4)
    );
}

#[test]
fn local_force_reuses_y_for_up() {
    let v = tables::local_force([1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0], 2.0, 3.0);
    assert_eq!(v, [3.0, 2.0, 2.0]);
}

#[test]
fn parse_plan_orders_commands_and_chars() {
    let ops = help::parse_plan(&wide("a$ButUse b$Look$x$Bad "));
    let cmds: Vec<(u32, u32)> = ops
        .chunks(4)
        .filter(|o| o[0] == OP_COMMAND)
        .map(|o| (o[2], o[3]))
        .collect();
    assert_eq!(cmds, [(6, CMD_BUTTON), (4, CMD_LOOK), (3, CMD_BAD)]);
    assert_eq!(ops[..4], [OP_CHAR, u32::from(b'a'), 0, 0]);
    let tail_dropped = help::parse_plan(&wide("a$Bad"));
    assert_eq!(tail_dropped, [OP_CHAR, u32::from(b'a'), 0, 0]);
    assert_eq!(help::parse_plan(&wide("$But!"))[3], CMD_BUTTON);
    assert_eq!(help::parse_plan(&wide("$Bu!"))[3], CMD_BAD);
}

#[test]
fn string_duration_clamps_to_minimum() {
    assert_eq!(help::string_duration(1.0, 3.0, 0.5, 2), 3.0);
    assert_eq!(help::string_duration(1.0, 3.0, 0.5, 10), 6.0);
}

#[test]
fn collide_transitions() {
    let s = types::collide_step(false, 0, true);
    assert!(s.transition && s.fire && s.state == 1);
    let s = types::collide_step(true, 1, false);
    assert!(s.transition && !s.fire && s.state == -1);
    let s = types::collide_step(true, 1, true);
    assert!(!s.transition && !s.fire);
    assert_eq!(
        types::collide_command(b"f", b"A", b"B", -1),
        b"f(\"A\", \"B\", -1)"
    );
}

#[test]
fn alpha_fader_clamps() {
    let mut f = AlphaFader {
        alpha: 0.5,
        active: true,
        fade_in: 1.0,
        fade_out: 2.0,
    };
    f.update(1.0);
    assert_eq!(f.alpha, 1.0);
    f.active = false;
    f.update(1.0);
    assert_eq!(f.alpha, 0.0);
    f.reset();
    assert_eq!(f.alpha, 0.0);
}

#[test]
fn enemy_sound_gate() {
    let mut c = 5.0;
    types::enemy_sound_update(&mut c, 2.0);
    assert!(!types::enemy_sound_allowed(c));
    types::enemy_sound_update(&mut c, 3.0);
    assert!(types::enemy_sound_allowed(c));
    types::enemy_sound_update(&mut c, 3.0);
    assert_eq!(c, 0.0);
}
