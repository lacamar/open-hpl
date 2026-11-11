use super::base::{self, BaseState, EffectsIn, LightData, MoveIn};
use super::lever::{self, LeverParams, LeverState};
use super::multislider::{self, MultiParams, MultiState};
use super::object::{self, JointData, ObjectState};
use super::ops::{Hinge, Slider};
use super::swingdoor::{self, DoorJoint, DoorState, Env};
use super::wheel::{self, WheelParams, WheelState};
use super::{Op, Ops, Probe, op};
use crate::math::{IDENTITY, to_rad, v3};
use crate::types::{Matrixf, Vector3f};
use std::cell::Cell;

#[derive(Default)]
struct Mock {
    hinge: Hinge,
    slider: Slider,
    push: Vector3f,
    collides: bool,
    in_fov: bool,
}

impl Probe for Mock {
    fn move_push(&mut self, _: Vector3f) -> Vector3f {
        self.push
    }
    fn outside_collides(&mut self, _: i32) -> bool {
        self.collides
    }
    fn insanity_in_fov(&mut self) -> bool {
        self.in_fov
    }
    fn hinge(&mut self, _: i32) -> Hinge {
        self.hinge
    }
    fn slider(&mut self) -> Slider {
        self.slider
    }
}

fn run<R>(f: impl FnOnce(&mut Ops) -> R) -> (R, Vec<Op>) {
    let log = std::cell::RefCell::new(Vec::new());
    let mut push = |o: &Op| log.borrow_mut().push(*o);
    let r = f(&mut Ops(&mut push));
    (r, log.into_inner())
}

fn codes(ops: &[Op]) -> Vec<i32> {
    ops.iter().map(|o| o.code).collect()
}

#[test]
fn damage_toughness() {
    assert_eq!(base::give_damage(100.0, 3, 10.0, 0), (100.0, 0.0));
    assert_eq!(base::give_damage(100.0, 3, 10.0, 2), (95.0, 5.0));
    assert_eq!(base::give_damage(100.0, 3, 10.0, 5), (90.0, 10.0));
}

#[test]
fn effects_toggle_and_fade() {
    let mut s = BaseState::new();
    let lights = [LightData {
        radius: 4.0,
        ..LightData::default()
    }];
    let env = EffectsIn {
        active: false,
        fade: true,
        n_ps: 1,
        n_sounds: 1,
        on_time: 2.0,
        off_time: 4.0,
        off_radius: -1.0,
        on_sound: false,
        off_sound: true,
    };
    let (_, ops) = run(|o| base::effects_set_active(&mut s, &env, &lights, o));
    assert_eq!(
        codes(&ops),
        [
            op::FX_PS_KILL_ALL,
            op::FX_LIGHT_OFF,
            op::FX_SOUND_FADE_OUT,
            op::FX_CHANGE_SOUND
        ]
    );
    assert_eq!(ops[1].f, [4.0, 4.0, 0.0, 0.0]);
    assert!(!s.effects_active && s.effects_fading);
    let (_, ops) = run(|o| base::effects_set_active(&mut s, &env, &lights, o));
    assert!(ops.is_empty());
    let (_, ops) = run(|o| base::effects_update(&mut s, 2.0, 2.0, 4.0, &lights, true, o));
    assert_eq!(s.effects_alpha, 0.5);
    assert_eq!(codes(&ops), [op::FX_BILLBOARDS, op::FX_MESH_ILLUM]);
}

#[test]
fn linear_move_arrives_and_stops() {
    let mut s = BaseState::new();
    let mut probe = Mock::default();
    let (_, ops) = run(|o| s.move_linear_to(v3(0.001, 0.0, 0.0), 10.0, 5.0, 0.0, true, o));
    assert_eq!(codes(&ops), [op::SND_MOVE_START, op::ON_START_MOVE]);
    let inp = MoveIn {
        dt: 0.1,
        body: IDENTITY,
        check_collision: false,
        loop_present: false,
        loop_alive: false,
        loop_sound: false,
    };
    let (_, ops) = run(|o| base::update_moving(&mut s, &inp, &mut probe, o));
    assert!(!s.moving);
    assert_eq!(ops.len(), 1);
    assert_eq!(ops[0].code, op::STATIC_LINEAR_MOVE);
    assert!((ops[0].v.x - 0.01).abs() < 1e-6);
}

#[test]
fn outside_player_pushes_and_releases() {
    let mut probe = Mock {
        collides: true,
        ..Mock::default()
    };
    let bodies = [base::OutsideBody {
        collide_character: false,
        default_collide_character: true,
        pos: v3(1.0, 0.0, 0.0),
        vel: v3(0.0, 0.0, 0.0),
    }];
    let (done, ops) = run(|o| base::outside_player(&bodies, v3(0.0, 0.0, 0.0), &mut probe, o));
    assert!(!done);
    assert_eq!(ops[0].code, op::OUTSIDE_IMPULSE);
    assert_eq!(ops[0].v, v3(0.5, 0.0, 0.0));
    probe.collides = false;
    let (done, ops) = run(|o| base::outside_player(&bodies, v3(0.0, 0.0, 0.0), &mut probe, o));
    assert!(done);
    assert_eq!(codes(&ops), [op::OUTSIDE_COLLIDE_CHAR]);
}

#[test]
fn object_stuck_state_hinge_quirk() {
    let mut s = ObjectState::default();
    let hinge = object::joint_data(0, -1.0, 1.0);
    let slider = object::joint_data(1, 0.0, 2.0);
    let (_, ops) = run(|o| object::set_stuck_state(&mut s, 1, &[hinge, slider], o));
    assert_eq!(
        codes(&ops),
        [
            op::SET_JOINT_MINMAX,
            op::JOINT_WAKE,
            op::SET_JOINT_MINMAX,
            op::JOINT_WAKE
        ]
    );
    assert_eq!(ops[0].f[0], ops[0].f[1]);
    assert_ne!(ops[2].f[0], ops[2].f[1]);
    let none = JointData::default();
    assert_eq!(none.kind, 0);
}

#[test]
fn object_impact_energy() {
    let b = object::ImpactBody {
        mass: 2.0,
        center: v3(0.0, 0.0, 0.0),
        vel_at_impact: v3(0.0, 0.0, -3.0),
    };
    let still = object::ImpactBody::default();
    let e = object::impact_energy(&[b, still], v3(0.0, 0.0, -1.0), v3(0.0, 0.0, 1.0));
    assert_eq!(e, 6.0);
}

fn door_probe(angle: f32) -> Mock {
    Mock {
        hinge: Hinge {
            angle,
            max_angle: 1.5,
            min_angle: 0.0,
            pin_dir: v3(0.0, 1.0, 0.0),
            child: IDENTITY,
            ..Hinge::default()
        },
        ..Mock::default()
    }
}

#[test]
fn door_closes_near_zero_and_locks() {
    let joints = [DoorJoint {
        max_angle: 1.5,
        min_angle: 0.0,
    }];
    let env = Env {
        joints: &joints,
        n_joints: 1,
    };
    let mut s = DoorState::new();
    let mut probe = door_probe(to_rad(2.0));
    let (_, ops) = run(|o| swingdoor::update(&mut s, &env, 0.1, false, &mut probe, o));
    assert!(s.closed);
    assert_eq!(*codes(&ops).last().unwrap(), op::PLAY_SOUND);
    s.closed = false;
    let (_, ops) = run(|o| swingdoor::set_locked(&mut s, &env, true, true, o));
    assert!(s.locked && s.closed);
    assert_eq!(ops.last().unwrap().i, op::SND_LOCK_ON);
    assert_eq!(swingdoor::door_state(&env, &mut probe), -1);
    probe.hinge.angle = 1.4;
    assert_eq!(swingdoor::door_state(&env, &mut probe), 1);
}

#[test]
fn door_timed_open_reaches_goal() {
    let joints = [DoorJoint {
        max_angle: 1.0,
        min_angle: 0.0,
    }];
    let env = Env {
        joints: &joints,
        n_joints: 2,
    };
    let mut s = DoorState::new();
    let mut probe = door_probe(0.0);
    probe.hinge.max_angle = 1.0;
    let (_, _) = run(|o| swingdoor::set_open_amount(&mut s, &env, 1.0, 2.0, true, &mut probe, o));
    assert!(s.is_updating_open_amount);
    assert_eq!(s.open_speed, 0.5);
    let (_, ops) = run(|o| swingdoor::update(&mut s, &env, 3.0, false, &mut probe, o));
    assert!(!s.is_updating_open_amount);
    assert!(ops.iter().any(|o| o.code == op::DOOR_BODY_MATRIX));
}

#[test]
fn wheel_limits_and_stuck() {
    let mut p = WheelParams {
        min_limit: to_rad(-360.0),
        max_limit: to_rad(360.0),
        min_limit_range: to_rad(5.0),
        max_limit_range: to_rad(5.0),
        max_limit_stuck: true,
        skip_middle_state: true,
        ..WheelParams::default()
    };
    p.slow_down_rotation = false;
    let mut s = WheelState {
        angle: to_rad(359.0),
        prev_angle: to_rad(359.0),
        ..WheelState::default()
    };
    let mut probe = Mock::default();
    let (_, ops) = run(|o| wheel::update(&mut s, &p, 0.016, false, &mut probe, o));
    assert_eq!(s.stuck_state, 1);
    assert_eq!(s.current_state, 1);
    let c = codes(&ops);
    assert!(
        c.contains(&op::CONN_STATE) && c.contains(&op::CONN_LIMIT) && c.contains(&op::CONN_TURN)
    );
    let (_, _) = run(|o| wheel::set_stuck_state(&mut s, &p, 0, true, &mut probe, o));
    assert_eq!(s.stuck_state, 0);
}

#[test]
fn lever_states_and_middle() {
    let p = LeverParams {
        min_limit_range: 0.1,
        max_limit_range: 0.1,
        auto_move_to_angle: false,
        ..LeverParams::default()
    };
    let mut s = LeverState::default();
    let (_, _) = run(|o| lever::setup(&mut s, &p, -1.0, 1.0, o));
    assert_eq!(s.middle_angle, 0.0);
    let mut probe = Mock::default();
    probe.hinge.angle = 0.95;
    let (_, ops) = run(|o| lever::update(&mut s, &p, 0.016, false, &mut probe, o));
    assert_eq!(s.current_state, 1);
    assert_eq!(
        codes(&ops),
        [op::CONN_STATE, op::LIMIT_SOUND, op::CONN_LIMIT]
    );
}

#[test]
fn multislider_states_and_sticking() {
    let states = multislider::compute_states(0.0, 1.0, 3);
    assert_eq!(states, [0.0, 0.5, 1.0]);
    let p = MultiParams {
        stick_to_state_max_dist: 0.1,
        ..MultiParams::default()
    };
    let mut s = MultiState::new();
    let mut probe = Mock::default();
    probe.slider.distance = 0.55;
    let (_, ops) = run(|o| multislider::update(&mut s, &p, &states, 0.016, false, &mut probe, o));
    assert_eq!(s.current_state, 1);
    assert_eq!(codes(&ops), [op::CHANGE_SOUND, op::RUN_CHANGE_CALLBACK]);
    assert!(multislider::compute_states(0.0, 1.0, -2).is_empty());
}

#[test]
fn insanity_vision_counts_down() {
    let p = object::ObjectParams::default();
    let i = object::ObjectInstance {
        is_insanity_vision: true,
        vision_max_sanity: 30.0,
        ..object::ObjectInstance::default()
    };
    let mut s = ObjectState::default();
    let mut probe = Mock::default();
    let a = object::UpdateIn {
        dt: 1.0,
        has_mesh: true,
        sanity: 10.0,
        interacted: false,
    };
    let (_, ops) = run(|o| object::update(&mut s, &p, &i, &a, &mut probe, o));
    assert_eq!(s.insanity_vision_count, 2.0);
    assert_eq!(codes(&ops), [op::INSANITY_VISIBLE]);
    assert_eq!(ops[0].i, 1);
}

#[test]
fn identity_matrix_is_unit() {
    let m: Matrixf = IDENTITY;
    let c = Cell::new(0);
    c.set(i32::from(m.m[3][3] == 1.0));
    assert_eq!(c.get(), 1);
}

#[test]
#[ignore = "prints struct sizes for the header's static_asserts"]
fn print_sizes() {
    use std::mem::size_of;
    macro_rules! p {
        ($($t:ty),*) => { $(println!("{} {}", stringify!($t), size_of::<$t>());)* };
    }
    p!(
        super::base::BaseParams,
        super::base::BaseInstance,
        super::base::LightData,
        super::base::Linear,
        super::base::Angular,
        super::base::BaseState,
        super::base::EffectsIn,
        super::base::MoveIn,
        super::base::OutsideBody,
        super::object::GrabData,
        super::object::PushData,
        super::object::MoveData,
        super::object::ObjectParams,
        super::object::ObjectInstance,
        super::object::ObjectState,
        super::object::UpdateIn,
        super::object::ImpactBody,
        super::object::JointData,
        super::swingdoor::DoorParams,
        super::swingdoor::DoorState,
        super::swingdoor::DoorJoint,
        super::wheel::WheelParams,
        super::wheel::WheelState,
        super::lever::LeverParams,
        super::lever::LeverState,
        super::lamp::LampParams,
        super::lamp::LampState,
        super::lamp::LampLink,
        super::multislider::MultiParams,
        super::multislider::MultiState,
        super::moveobject::MoveParams,
        super::moveobject::MoveObjState,
        super::ops::Hinge,
        super::ops::Slider
    );
}

#[test]
#[ignore = "probe"]
fn nan_sign_probe() {
    use crate::types::{Quaternion, Vector3f};
    unsafe extern "C" {
        safe fn ohpl_math_matrix_quaternion(q: &Quaternion, out: &mut Matrixf);
        safe fn ohpl_quaternion_from_angle_axis(q: &mut Quaternion, angle: f32, axis: &Vector3f);
    }
    for a in [
        f32::from_bits(0x7fc00000),
        f32::from_bits(0xffc00000),
        f32::INFINITY,
        -f32::INFINITY,
    ] {
        let mut q = Quaternion::default();
        ohpl_quaternion_from_angle_axis(&mut q, a, &v3(0.0, 1.0, 0.0));
        let mut m = Matrixf::default();
        ohpl_math_matrix_quaternion(&q, &mut m);
        println!(
            "{:08x} -> q {:08x} {:08x} m00 {:08x}",
            a.to_bits(),
            q.w.to_bits(),
            q.v.y.to_bits(),
            m.m[0][0].to_bits()
        );
    }
}
