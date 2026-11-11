use super::effects::{self, LIQUID_MOVED, LIQUID_SNAP};
use super::geom::{aabb_to_obb, obb_overlap, point_in_obb, ray_hits_obb};
use super::logic::{self, ANIM_FINISH_CALLBACK, ANIM_NEXT};
use super::motion::{Move, ROTATE_ALIGN, Rotate};
use super::procanim::ProcAnim;
use super::props::{Op, Vars, apply_instance_vars};
use crate::math::{IDENTITY, v3};
use std::collections::HashMap;

#[test]
fn move_reaches_goal_and_stops() {
    let mut m = Move::default();
    assert!(m.step(0.1, v3(0.0, 0.0, 0.0)).is_none());
    m.start(v3(1.0, 0.0, 0.0), 10.0, 2.0, 0.0, true);
    let mut pos = v3(0.0, 0.0, 0.0);
    let mut done = false;
    for _ in 0..100 {
        let (p, d) = m.step(0.1, pos).unwrap();
        pos = p;
        if d {
            done = true;
            break;
        }
    }
    assert!(done && pos == v3(1.0, 0.0, 0.0) && !m.moving && m.speed == 0.0);
}

#[test]
fn rotate_speed_mode_and_align() {
    let mut r = Rotate::default();
    r.at_speed(1.0, 0.0, v3(0.0, 1.0, 0.0), true, v3(0.0, 0.0, 0.0));
    assert!(r.step(0.1, &IDENTITY).is_none());
    let mut goal = IDENTITY;
    goal.m[0][0] = 0.0;
    goal.m[0][2] = 1.0;
    goal.m[2][0] = -1.0;
    goal.m[2][2] = 0.0;
    r.align_to(&goal, 100.0, 5.0, 0.0, true, v3(0.0, 0.0, 0.0));
    assert_eq!(r.mode, ROTATE_ALIGN);
    let mut m = IDENTITY;
    let mut done = false;
    for _ in 0..200 {
        let o = r.step(0.05, &m).unwrap();
        m = o.matrix;
        done |= o.done;
        if o.done {
            break;
        }
    }
    assert!(done && r.mode == 0);
    assert!((m.m[0][2] - 1.0).abs() < 1e-4);
}

#[test]
fn effects_fade_and_liquid() {
    let mut alpha = 1.0;
    assert_eq!(effects::fade_setup(false, 0.0, 0.25, &mut alpha), 0.0);
    assert_eq!(alpha, 0.25);
    let mut speed = effects::fade_setup(true, 2.0, 0.25, &mut alpha);
    assert_eq!(speed, 0.5);
    let mut mesh = 1.0;
    let f = effects::update_fades(&mut mesh, 0.0, true, &mut alpha, &mut speed, 0.25, 1.0);
    assert_eq!(f, effects::UPDATE_EFFECTS);
    assert_eq!(alpha, 0.75);
    let mut amount = 0.0;
    assert_eq!(
        effects::liquid_update(0.1, &mut amount, 1.0, 1.0),
        LIQUID_MOVED
    );
    assert_eq!(
        effects::liquid_update(10.0, &mut amount, 1.0, 1.0),
        LIQUID_SNAP
    );
}

#[test]
fn proc_anim_runs_to_end() {
    let mut a = ProcAnim::default();
    a.play(1.0, false, 0.0, 0.0);
    assert!(a.active && a.amount == 1.0 && a.speed == 1.0);
    let mut frames = 0;
    while a.step(0.25) {
        frames += 1;
        assert!(frames < 10);
    }
    assert_eq!(a.time, 1.0);
}

#[test]
fn logic_decisions() {
    assert_eq!(logic::damage_amount(10.0, 0, 2), 0.0);
    assert_eq!(logic::damage_amount(10.0, 1, 2), 5.0);
    assert_eq!(logic::damage_amount(10.0, 2, 2), 10.0);
    assert_eq!(logic::connection_state(2, 1, false), None);
    assert_eq!(logic::connection_state(2, 0, true), Some(-2));
    assert_eq!(
        logic::anim_update_action(true, 0, false, true, true),
        ANIM_NEXT
    );
    assert_eq!(
        logic::anim_update_action(true, 0, false, false, true),
        ANIM_FINISH_CALLBACK
    );
    assert!(logic::body_name_matches(b"ent_door", b"door"));
    assert!(logic::body_name_matches(b"a/door", b"door"));
    assert!(!logic::body_name_matches(b"ent_door", b""));
    assert!(logic::break_energy_exceeds(10.0, 5.0, 0.0, 0.0, 0.0, 100.0));
}

#[test]
fn obb_tests() {
    let a = aabb_to_obb(v3(0.0, 0.0, 0.0), v3(1.0, 1.0, 1.0));
    let b = aabb_to_obb(v3(0.5, 0.5, 0.5), v3(2.0, 2.0, 2.0));
    let c = aabb_to_obb(v3(3.0, 3.0, 3.0), v3(4.0, 4.0, 4.0));
    assert!(obb_overlap(&a, &b) && !obb_overlap(&a, &c));
    assert!(point_in_obb(&a, v3(0.5, 0.5, 0.5)) && !point_in_obb(&a, v3(2.0, 0.5, 0.5)));
    let t = ray_hits_obb(&a, v3(-1.0, 0.5, 0.5), v3(1.0, 0.0, 0.0), 10.0).unwrap();
    assert_eq!(t, 1.0);
}

struct Map(HashMap<&'static str, &'static str>);

impl Vars for Map {
    fn string(&self, name: &str) -> Vec<u8> {
        self.0.get(name).map_or(vec![], |s| s.as_bytes().to_vec())
    }
    fn boolean(&self, name: &str, def: bool) -> bool {
        self.0.get(name).map_or(def, |s| *s == "true")
    }
    fn float(&self, name: &str, def: f32) -> f32 {
        self.0.get(name).and_then(|s| s.parse().ok()).unwrap_or(def)
    }
    fn int(&self, name: &str, def: i32) -> i32 {
        self.0.get(name).and_then(|s| s.parse().ok()).unwrap_or(def)
    }
}

#[test]
fn instance_vars_ops() {
    let inst = Map(HashMap::from([
        ("PlayerInteractCallback", "OnUse"),
        ("CC_Entities", "Player; box"),
        ("CC_Funcs", "OnEnter"),
        ("ConnectedEntity", "a,b"),
        ("InteractionDisabled", "true"),
    ]));
    let vars = Map(HashMap::from([("Health", "50")]));
    let mut ops = vec![];
    apply_instance_vars(&inst, &vars, &mut |op| ops.push(format!("{op:?}")));
    assert_eq!(ops.len(), 8);
    assert!(ops[0].starts_with("InteractCallback"));
    assert!(ops[1].contains("to_player: true") && ops[2].contains("to_player: false"));
    assert_eq!(ops[7], format!("{:?}", Op::InteractionDisabled));
}
