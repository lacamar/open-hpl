use super::entc::{self, CollideHost, ConnHost, EntityCb, LookHost};
use super::entity::{LookIn, Los};
use super::opc as c;
use super::ops::Pod;
use super::propsc::{self, ItemHost, ItemState, NpcBone, NpcHost, NpcState, Skeleton};
use super::{Op, Ops};
use crate::math::{IDENTITY, v3};
use crate::types::{Matrixf, Vector3f};

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
fn interact_callback_clears_after_the_script() {
    let mut cb = EntityCb {
        interact_remove: true,
        ..EntityCb::default()
    };
    let (_, ops) = run(|o| entc::run_interact(Pod::new(&mut cb), true, o));
    assert_eq!(codes(&ops), [c::ENT_INTERACT, c::ENT_CLEAR_INTERACT]);
    cb.interact_remove = false;
    let (_, ops) = run(|o| entc::run_interact(Pod::new(&mut cb), true, o));
    assert_eq!(codes(&ops), [c::ENT_INTERACT]);
    let (_, ops) = run(|o| entc::run_interact(Pod::new(&mut cb), false, o));
    assert!(ops.is_empty());
}

#[test]
fn set_active_reports_a_change_only() {
    let mut a = true;
    assert!(!entc::set_active(&mut a, true));
    assert!(entc::set_active(&mut a, false));
    assert!(!a);
}

#[test]
fn attach_needs_sound_and_entity() {
    assert_eq!(
        entc::play_sound_attach(false, true, true),
        entc::ATTACH_NONE
    );
    assert_eq!(
        entc::play_sound_attach(true, false, true),
        entc::ATTACH_NONE
    );
    assert_eq!(
        entc::play_sound_attach(true, true, true),
        entc::ATTACH_CHILD
    );
    assert_eq!(
        entc::play_sound_attach(true, true, false),
        entc::ATTACH_POSITION
    );
}

struct Bodies(i32, bool);

impl LookHost for Bodies {
    fn body_num(&mut self) -> i32 {
        self.0
    }
    fn body(&mut self, _: i32) -> Option<LookIn> {
        self.1.then(|| LookIn {
            cam_fwd: v3(0.0, 0.0, 1.0),
            bv_min: v3(-1.0, -1.0, 1.0),
            bv_max: v3(1.0, 1.0, 3.0),
            bv_center: v3(0.0, 0.0, 2.0),
            bv_radius: 1.0,
            ..LookIn::default()
        })
    }
}

struct Seen;

impl Los for Seen {
    fn line_of_sight(&mut self, _: Vector3f, _: Vector3f) -> bool {
        true
    }
}

#[test]
fn look_at_runs_enter_then_leave() {
    let mut cb = EntityCb {
        lookat_remove: true,
        ..EntityCb::default()
    };
    let pod = Pod::new(&mut cb);
    let (_, ops) =
        run(|o| entc::update_look_at(pod, true, 0.1, &mut Bodies(1, true), &mut Seen, o));
    assert_eq!(codes(&ops), [c::ENT_LOOKAT_ENTER]);
    assert_eq!(ops[0].i, 1);
    assert!(cb.is_looked_at);
    assert!((cb.lookat_count - 0.3).abs() < 1e-6);
    let pod = Pod::new(&mut cb);
    let (_, ops) =
        run(|o| entc::update_look_at(pod, true, 0.1, &mut Bodies(1, false), &mut Seen, o));
    assert!(ops.is_empty());
    cb.lookat_count = 0.0;
    let pod = Pod::new(&mut cb);
    let (_, ops) =
        run(|o| entc::update_look_at(pod, true, 0.1, &mut Bodies(1, false), &mut Seen, o));
    assert_eq!(codes(&ops), [c::ENT_LOOKAT_LEAVE]);
    assert!(!cb.is_looked_at);
}

struct Conns(Vec<(i32, bool, bool)>);

impl ConnHost for Conns {
    fn conn_num(&mut self) -> i32 {
        i32::try_from(self.0.len()).unwrap_or(0)
    }
    fn conn(&mut self, i: i32) -> (i32, bool) {
        let c = self.0[usize::try_from(i).unwrap_or(0)];
        (c.0, c.1)
    }
    fn conn_has_callback(&mut self, i: i32) -> bool {
        self.0[usize::try_from(i).unwrap_or(0)].2
    }
}

#[test]
fn connections_filter_invert_and_script() {
    let mut h = Conns(vec![(0, false, false), (1, true, true), (-1, false, true)]);
    let (_, ops) = run(|o| entc::connection_state_change(1, true, &mut h, o));
    assert_eq!(
        codes(&ops),
        [
            c::ENT_CONN_CHANGE_CB,
            c::ENT_CONN_SEND,
            c::ENT_CONN_SEND,
            c::ENT_CONN_SCRIPT
        ]
    );
    assert_eq!((ops[2].i, ops[2].j), (1, -1));
}

struct Collide(Vec<(bool, bool)>);

impl CollideHost for Collide {
    fn body_num(&mut self) -> i32 {
        i32::try_from(self.0.len()).unwrap_or(0)
    }
    fn bv_intersects(&mut self, i: i32) -> bool {
        self.0[usize::try_from(i).unwrap_or(0)].0
    }
    fn shape_collides(&mut self, i: i32) -> bool {
        self.0[usize::try_from(i).unwrap_or(0)].1
    }
}

#[test]
fn collision_needs_bv_overlap_first() {
    assert!(!entc::collides_with_player(&mut Collide(vec![(
        false, true
    )])));
    assert!(entc::collides_with_player(&mut Collide(vec![
        (false, true),
        (true, true)
    ])));
    assert!(!entc::collides_with_player(&mut Collide(vec![])));
}

#[test]
fn chest_lock_orders_the_hinge_limits() {
    let mut locked = false;
    let (_, ops) = run(|o| propsc::chest_set_locked(Pod::new(&mut locked), true, &[1.5], o));
    assert!(locked);
    assert_eq!(
        codes(&ops),
        [
            c::CHEST_HINGE_MIN,
            c::CHEST_HINGE_MAX,
            c::CHEST_HINGE_STICKY,
            c::CHEST_ADD_FORCE
        ]
    );
    let (_, ops) = run(|o| propsc::chest_set_locked(Pod::new(&mut locked), false, &[1.5], o));
    assert_eq!(ops[0].code, c::CHEST_HINGE_MAX);
    assert_eq!(ops[0].f[0], 1.5);
    let (_, ops) = run(|o| propsc::chest_set_locked(Pod::new(&mut locked), false, &[1.5], o));
    assert!(ops.is_empty());
}

#[test]
fn chest_interact_messages() {
    assert_eq!(
        propsc::chest_interact(false, 5, 9),
        propsc::CHEST_NO_INTERACT
    );
    assert_eq!(propsc::chest_interact(true, 5, 4), propsc::CHEST_NO_COINS);
    assert_eq!(propsc::chest_interact(true, 5, 5), propsc::CHEST_QUESTION);
}

#[test]
fn chest_press_pays_then_unlocks() {
    let (_, ops) = run(|o| propsc::chest_press(true, 7, o));
    assert_eq!(ops[0].i, -7);
    assert_eq!(ops[1].code, c::CHEST_UNLOCK);
    assert_eq!(ops.len(), 5);
    assert!(run(|o| propsc::chest_press(false, 7, o)).1.is_empty());
}

struct Item {
    pickup: bool,
    subs: Vec<bool>,
}

impl ItemHost for Item {
    fn show_pickup(&mut self) -> bool {
        self.pickup
    }
    fn sub_mesh_num(&mut self) -> i32 {
        i32::try_from(self.subs.len()).unwrap_or(0)
    }
    fn sub_mesh_visible(&mut self, i: i32) -> bool {
        self.subs[usize::try_from(i).unwrap_or(0)]
    }
}

#[test]
fn item_pickup_sequence() {
    let mut s = ItemState::default();
    let mut host = Item {
        pickup: true,
        subs: vec![],
    };
    let (_, ops) = run(|o| propsc::item_interact(Pod::new(&mut s), &mut host, o));
    assert_eq!(
        codes(&ops),
        [
            c::ITEM_ADD,
            c::ITEM_DESTROY,
            c::ITEM_PICK_SOUND,
            c::ITEM_PICK_MESSAGE,
            c::ITEM_CALLBACK
        ]
    );
    assert!(s.remove_prop);
}

#[test]
fn item_flash_follows_the_player() {
    let mut s = ItemState::default();
    let mut host = Item {
        pickup: false,
        subs: vec![true, false, true],
    };
    let (_, ops) = run(|o| {
        propsc::item_update(
            Pod::new(&mut s),
            0.4,
            v3(0.0, 0.0, 0.0),
            v3(1.0, 0.0, 1.0),
            &mut host,
            o,
        );
    });
    assert_eq!(s.flash_alpha, 0.4);
    assert_eq!(ops.len(), 2);
    let (_, ops) = run(|o| {
        propsc::item_update(
            Pod::new(&mut s),
            0.5,
            v3(0.0, 0.0, 0.0),
            v3(9.0, 0.0, 0.0),
            &mut host,
            o,
        );
    });
    assert!(s.flash_alpha.abs() < 1e-6);
    assert!(ops.is_empty());
}

#[test]
fn npc_wake_cycle() {
    let mut s = NpcState::new();
    let (_, ops) = run(|o| propsc::npc_set_awake(Pod::new(&mut s), false, true, o));
    assert!(!s.awake && s.playing_wake_anim);
    assert_eq!(ops[0].i, c::NPC_ANIM_SLEEP);
    let (_, ops) = run(|o| propsc::npc_update_wake(Pod::new(&mut s), 3, o));
    assert!(ops.is_empty());
    let (_, ops) = run(|o| propsc::npc_update_wake(Pod::new(&mut s), -1, o));
    assert!(!s.playing_wake_anim);
    assert_eq!(ops[0].i, c::NPC_ANIM_SLEEP_IDLE);
}

struct Npc {
    area: bool,
    checks: u32,
}

impl NpcHost for Npc {
    fn check_area_collision(&mut self) -> bool {
        self.checks += 1;
        self.area
    }
    fn mesh_world(&mut self) -> Matrixf {
        IDENTITY
    }
    fn mesh_position(&mut self) -> Vector3f {
        v3(0.0, 0.0, 0.0)
    }
    fn player_position(&mut self) -> Vector3f {
        v3(1.0, 0.0, 1.0)
    }
}

#[test]
fn npc_area_check_runs_twice_a_second() {
    let mut s = NpcState::new();
    let mut h = Npc {
        area: true,
        checks: 0,
    };
    propsc::npc_update_area(Pod::new(&mut s), true, 0.2, &mut h);
    propsc::npc_update_area(Pod::new(&mut s), true, 0.2, &mut h);
    assert_eq!(h.checks, 1);
    assert!(s.player_in_area);
    propsc::npc_update_area(Pod::new(&mut s), false, 9.0, &mut h);
    assert_eq!(h.checks, 1);
}

#[test]
fn npc_head_turns_towards_the_player_and_rotates_bones() {
    let mut s = NpcState::new();
    s.player_in_area = true;
    let p = propsc::NpcParams {
        head_move_speed_mul: 2.0,
        head_move_max_speed: 1.0,
        max_head_angle: 1.0,
    };
    let bones = [
        NpcBone {
            idx: 3,
            mul: 1.0,
            rot_vec: v3(0.0, 1.0, 0.0),
        },
        NpcBone {
            idx: -2,
            mul: 1.0,
            rot_vec: v3(0.0, 1.0, 0.0),
        },
    ];
    let mut h = Npc {
        area: true,
        checks: 0,
    };
    let (_, ops) = run(|o| propsc::npc_update_head(Pod::new(&mut s), &p, &bones, 0.1, &mut h, o));
    assert!(s.head_angle > 0.0);
    assert_eq!(codes(&ops), [c::NPC_BONE_ROTATE]);
    assert_eq!(ops[0].i, 3);
}

struct Skel(i32);

impl Skeleton for Skel {
    fn present(&mut self) -> bool {
        true
    }
    fn bone_index(&mut self, _: &[u8]) -> i32 {
        self.0
    }
    fn bone_world(&mut self, _: i32) -> Matrixf {
        IDENTITY
    }
}

struct NoVars;

impl super::Vars for NoVars {
    fn string(&self, name: &str, _: &[u8]) -> Vec<u8> {
        match name {
            "MoveHeadBones" => b"a, b".to_vec(),
            "MoveHeadBoneMuls" => b"0.5 1".to_vec(),
            _ => Vec::new(),
        }
    }
    fn boolean(&self, _: &str, d: bool) -> bool {
        d
    }
    fn float(&self, _: &str, d: f32) -> f32 {
        d
    }
    fn int(&self, _: &str, d: i32) -> i32 {
        d
    }
    fn vec3(&self, _: &str, d: Vector3f) -> Vector3f {
        d
    }
    fn color(&self, _: &str, d: crate::types::Color) -> crate::types::Color {
        d
    }
    fn put(&self, _: i32, _: &[u8]) {}
}

#[test]
fn npc_bones_keep_an_invalid_index_for_the_error_op() {
    let (_, ops) = run(|o| propsc::npc_load(&NoVars, &mut Skel(-2), o));
    assert_eq!(
        codes(&ops),
        [c::NPC_BONES_RESIZE, c::NPC_ERROR_BONE, c::NPC_ERROR_BONE]
    );
    assert_eq!(ops[1].j, -2);
    let (_, ops) = run(|o| propsc::npc_load(&NoVars, &mut Skel(4), o));
    assert_eq!(ops[2].code, c::NPC_BONE_SET);
    assert_eq!((ops[2].j, ops[2].f[0]), (4, 1.0));
    assert_eq!(ops[2].v, v3(0.0, 1.0, 0.0));
}
