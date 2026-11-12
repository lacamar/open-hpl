use super::critter::{self, CritterState, Query, Reply};
use super::entity::{self, Los};
use super::hand::{self, Hands, LightAttrs};
use super::interact;
use super::smallprops;
use super::{Op, Ops};
use crate::math::v3;
use crate::types::{Vector2f, Vector3f};

struct NoQuery;

impl Query for NoQuery {
    fn ask(&mut self, _: i32, _: Vector3f, _: Vector3f, _: i32) -> Reply {
        Reply::default()
    }
}

fn run<R>(f: impl FnOnce(&mut Ops) -> R) -> (R, Vec<Op>) {
    let log = std::cell::RefCell::new(Vec::new());
    let mut push = |o: &Op| log.borrow_mut().push(*o);
    let r = f(&mut Ops(&mut push));
    (r, log.into_inner())
}

#[test]
fn fwd_ring_keeps_twenty() {
    let mut s = CritterState::new();
    for i in 0..30 {
        s.vel = v3(1.0 + i as f32, 0.0, 0.0);
        critter::mesh_basis(&mut s);
    }
    assert_eq!(s.n_fwd, 20);
    assert_eq!(s.n_up, 30);
    for _ in 0..30 {
        critter::mesh_basis(&mut s);
    }
    assert_eq!(s.n_up, 40);
}

#[test]
fn ortho_replaces_parallel_forward() {
    let up = v3(0.0, 1.0, 0.0);
    let (right, fwd) = critter::create_ortho(up, up);
    assert_eq!((right, fwd), (v3(0.0, 0.0, 0.0), v3(0.0, 0.0, 0.0)));
}

#[test]
fn anim_switches_walk_idle() {
    let mut s = CritterState::new();
    s.vel = v3(1.0, 0.0, 0.0);
    let (_, ops) = run(|o| critter::anim_step(&mut s, o));
    assert_eq!(s.anim_state, 1);
    assert_eq!(ops.len(), 1);
    s.vel = v3(0.0, 0.0, 0.0);
    let (_, ops) = run(|o| critter::anim_step(&mut s, o));
    assert_eq!(s.anim_state, 0);
    assert_eq!(ops.len(), 2);
}

#[test]
fn in_range_is_fifteen_metres() {
    let o = v3(0.0, 0.0, 0.0);
    assert!(critter::in_range(v3(15.0, 0.0, 0.0), o));
    assert!(!critter::in_range(v3(15.1, 0.0, 0.0), o));
}

#[test]
fn spider_damage_below_threshold_is_ignored() {
    let mut c = CritterState::new();
    let mut s = critter::SpiderState::default();
    let (_, ops) = run(|o| critter::spider_on_damage(&mut c, &mut s, 2.0, o));
    assert!(ops.is_empty());
    let (_, ops) = run(|o| critter::spider_on_damage(&mut c, &mut s, 2.5, o));
    assert_eq!(ops.len(), 1);
    assert_eq!(s.state, critter::ST_HIT);
}

#[test]
fn flee_unused_without_query() {
    let _ = NoQuery.ask(0, v3(0.0, 0.0, 0.0), v3(0.0, 0.0, 0.0), 0);
}

#[test]
fn light_load_clamps_zero_fade() {
    let p = hand::light_load(&LightAttrs {
        pin_dir: v3(0.0, 0.0, 2.0),
        sway_limits: Vector2f { x: 180.0, y: 90.0 },
        ..LightAttrs::default()
    });
    assert!((p.fade_in_speed - 1000.0).abs() < 0.01);
    assert_eq!(p.pin_dir.z, 1.0);
    assert!((p.sway_limits.x - std::f32::consts::PI).abs() < 1e-6);
}

#[test]
fn light_fades_out_to_zero_and_in_to_done() {
    let p = hand::light_load(&LightAttrs {
        fade_in_time: 1.0,
        fade_out_time: 1.0,
        ..LightAttrs::default()
    });
    let f = hand::light_fade(&p, true, 0.3, 1.0);
    assert_eq!((f.alpha, f.update, f.done), (0.0, true, false));
    let f = hand::light_fade(&p, false, 0.5, 1.0);
    assert_eq!((f.alpha, f.update, f.done), (1.0, true, true));
    let f = hand::light_fade(&p, false, 1.0, 1.0);
    assert!(!f.update);
}

#[test]
fn melee_charge_swing_cycle() {
    let mut h = Hands::default();
    let a = hand::melee_do_action(hand::HANDS_IDLE, true, &mut h);
    assert!(a.ret && a.charge);
    hand::melee_charge(&mut h);
    assert!(h.attack_down);
    let a = hand::melee_anim_over(&mut h);
    assert!(a.charge_speed && h.obj_state == 1);
    assert!(!hand::melee_update_charge(&mut h, 1.0, 0.5));
    assert!(hand::melee_update_charge(&mut h, 1.0, 0.5));
    assert_eq!(hand::melee_damage_mul(&h), 2.0);
    let a = hand::melee_do_action(hand::HANDS_HAND_OBJECT, false, &mut h);
    assert!(a.swing && !h.attack_down);
    assert!(hand::melee_swing(&mut h));
    let a = hand::melee_anim_over(&mut h);
    assert!(a.ret && h.obj_state == 0);
}

#[test]
fn rope_and_move_limits() {
    assert_eq!(
        interact::rope_limit_length(true, 1, 0, false, 1.0, 5.0),
        None
    );
    assert_eq!(
        interact::rope_limit_length(false, 0, 0, false, 1.0, 5.0),
        None
    );
    assert_eq!(
        interact::rope_limit_length(false, 1, -1, false, 1.0, 5.0),
        None
    );
    assert_eq!(
        interact::rope_limit_length(false, 1, 0, true, 1.0, 5.0),
        Some(1.0)
    );
    assert_eq!(
        interact::rope_limit_length(false, 2, 0, false, 1.0, 5.0),
        Some(0.0)
    );
    assert_eq!(interact::move_limit_state(false, -1, 0, false), Some(0.0));
    assert_eq!(interact::move_limit_state(false, -1, 0, true), Some(1.0));
    let mut needs = true;
    assert!(interact::move_update_gate(true, false, &mut needs));
    assert!(!needs);
    assert!(!interact::move_update_gate(true, false, &mut needs));
    assert_eq!(interact::move_turn_state(0.25, true), 0.75);
}

#[test]
fn button_and_door() {
    let mut on = false;
    assert_eq!(smallprops::button_set(&mut on, true), Some(1));
    assert_eq!(smallprops::button_set(&mut on, true), None);
    assert_eq!(smallprops::button_set(&mut on, false), Some(-1));
    assert!(smallprops::button_can_interact(true, false, true));
    assert!(!smallprops::button_can_interact(true, true, false));
    assert_eq!(smallprops::button_conn_state(0), None);
    let a = smallprops::door_interact(true, 0.0);
    assert!(a.locked_feedback && a.count == 2.0 && !a.change_map);
    assert!(smallprops::door_interact(false, 0.0).change_map);
    assert_eq!(smallprops::door_tick(1.0, 0.25), 0.75);
    assert_eq!(smallprops::door_tick(0.0, 0.25), 0.0);
}

#[test]
fn oil_barrel_fill() {
    assert_eq!(smallprops::oil_amount(100.0), 10.0);
    assert_eq!(smallprops::oil_amount(-50.0), 100.0);
    let f = smallprops::oil_fill(0.0, 10.0);
    assert_eq!(f.result, smallprops::OIL_EMPTY);
    let f = smallprops::oil_fill(10.0, 99.5);
    assert_eq!(f.result, smallprops::OIL_FULL);
    let f = smallprops::oil_fill(30.0, 90.0);
    assert_eq!((f.lamp, f.barrel, f.filled, f.tier), (100.0, 20.0, 10.0, 0));
    let f = smallprops::oil_fill(30.0, 10.0);
    assert_eq!((f.lamp, f.barrel, f.tier), (40.0, 0.0, 2));
    assert_eq!(smallprops::oil_focus_tier(0.0), 0);
    assert_eq!(smallprops::oil_focus_tier(60.0), 3);
}

#[test]
fn photocell_switches_with_hysteresis() {
    let mut lit = false;
    assert_eq!(smallprops::photo_switch(0.5, &mut lit, 0.6, 0.2), 0);
    assert_eq!(smallprops::photo_switch(0.7, &mut lit, 0.6, 0.2), 1);
    assert_eq!(smallprops::photo_switch(0.3, &mut lit, 0.6, 0.2), 0);
    assert_eq!(smallprops::photo_switch(0.1, &mut lit, 0.6, 0.2), -1);
    let mut c = 0.0;
    assert!(!smallprops::photo_tick(&mut c, 0.01));
    assert!(smallprops::photo_tick(&mut c, 0.03));
    assert_eq!(c, 0.0);
    assert_eq!(smallprops::photo_level(&[0.1, 0.5, 0.2]), 0.5);
    assert!((smallprops::photo_illum_step(0.9, 0.2, 0.5) - 0.4).abs() < 1e-6);
    assert_eq!(smallprops::photo_illum_step(0.1, 0.2, 0.5), 0.2);
}

struct Sight(bool);

impl Los for Sight {
    fn line_of_sight(&mut self, _: Vector3f, _: Vector3f) -> bool {
        self.0
    }
}

#[test]
fn entity_connections_and_look_at() {
    assert_eq!(entity::conn_state(1, 0, true), Some(-1));
    assert_eq!(entity::conn_state(1, -1, false), None);
    assert_eq!(entity::conn_state(-1, -1, false), Some(-1));
    let mut count = 0.0;
    assert!(entity::look_timer(&mut count, 0.1));
    assert_eq!(count, 0.3);
    assert!(!entity::look_timer(&mut count, 0.1));
    let look = entity::LookIn {
        cam_pos: v3(0.0, 0.0, 0.0),
        cam_fwd: v3(0.0, 0.0, -1.0),
        cam_up: v3(0.0, 1.0, 0.0),
        cam_right: v3(1.0, 0.0, 0.0),
        bv_min: v3(-1.0, -1.0, -6.0),
        bv_max: v3(1.0, 1.0, -4.0),
        bv_center: v3(0.0, 0.0, -5.0),
        bv_radius: 1.5,
    };
    assert!(entity::look_body(&look, &mut Sight(true)));
    assert!(!entity::look_body(&look, &mut Sight(false)));
    let far = entity::LookIn {
        bv_center: v3(0.0, 0.0, -80.0),
        ..look
    };
    assert!(!entity::look_body(&far, &mut Sight(true)));
}
