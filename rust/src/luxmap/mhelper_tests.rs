//! Map helper against a fake host: ray filters, nearest-hit keeping, light traversal order, damage flow.

use super::mhelper::{self, DamageArgs, Ray};
use super::mhost::{self, A, body, op};
use crate::types::Vector3f;
use std::sync::Mutex;

static LOG: Mutex<Vec<String>> = Mutex::new(Vec::new());

fn log(s: String) {
    LOG.lock().unwrap().push(s);
}

const ENTITY: usize = 0x500;
const PROP: usize = 0x100;
const CHAR: usize = 0x200;

static SUBS: [[bool; 4]; 1] = [[true, true, false, true]];
static NODES: [usize; 3] = [0x11, 0x20, 0x21];

unsafe extern "C" fn fake(o: i32, a: *mut A) {
    let a = unsafe { &mut *a };
    match o {
        op::CUR_MAP => {
            a.r = 1;
            a.p[0] = 0x77;
            a.p[1] = 0x88;
        }
        op::BODY_INFO => match a.p[0] {
            PROP => {
                a.i[0] = body::COLLIDE | body::ACTIVE | body::BLOCKS_LIGHT;
                a.f[0] = 2.0;
                a.r = ENTITY;
            }
            CHAR => {
                a.i[0] = body::IS_CHAR | body::COLLIDE | body::ACTIVE | body::PLAYER_BODY;
                a.f[0] = 80.0;
                a.p[1] = ENTITY;
            }
            _ => {}
        },
        op::BODY_SUBMESHES => {
            a.i[0] = 1;
            a.i[1] = 1;
            a.r = SUBS.as_ptr() as usize;
        }
        op::ENT_INFO => {
            a.i[0] = 1;
            a.i[1] = 0;
        }
        op::BODY_POS => a.f[0] = 5.0,
        op::SHAPE_BODIES => {
            a.i[0] = 2;
            static BODIES: [usize; 2] = [PROP, CHAR];
            a.r = BODIES.as_ptr() as usize;
        }
        op::SHAPE_COLLIDE => {
            a.i[0] = 1;
            a.i[1] = 1;
            a.i[2] = 2;
            static PTS: [f32; 6] = [1.0, 2.0, 3.0, 3.0, 4.0, 5.0];
            a.r = PTS.as_ptr() as usize;
        }
        op::BODY_IMPULSE => log(format!("impulse {:?}", &a.f[..6])),
        op::BODY_PUSH => log(format!("push {:?}", &a.f[..3])),
        op::BODY_HIT => a.i[0] = 0,
        op::ENT_DAMAGE => log("entity damage".into()),
        op::PLAYER_DAMAGE => log("player damage".into()),
        op::RAY_CAST => {
            // the attack ray finds nothing; the sight ray sees body 0x100 first
            let ray = a.p[1] as *mut Ray;
            if a.i[0] == mhelper::RAY_LOS {
                unsafe {
                    if mhelper::ray_before(ray, PROP) {
                        mhelper::ray_hit(ray, PROP, 1.0, Vector3f::default());
                    }
                }
            }
        }
        op::LIGHT_ROOT => a.r = NODES[0] + a.i[0] as usize,
        op::NODE_INFO => {
            // node 0x11 has children 0x20 (leaf) and 0x21 (leaf), 0x10 is empty
            if a.p[0] == 0x11 {
                a.i[0] = 2;
                a.r = NODES[1..].as_ptr() as usize;
            } else {
                a.i[1] = 1;
                static OBJ: [usize; 1] = [0x900];
                a.r = OBJ.as_ptr() as usize;
            }
            a.f[3..6].copy_from_slice(&[10.0; 3]);
            a.f[0..3].copy_from_slice(&[-10.0; 3]);
        }
        op::LIGHT_TEST => {
            a.i[0] = 1;
            a.i[1] = 1;
        }
        op::LIGHT_PROPS => {
            a.f[..3].copy_from_slice(&[0.5, 0.25, 0.1]);
            a.i[0] = 1;
        }
        op::PLAYER_AMB_LIGHT => a.r = 0xdead,
        _ => {}
    }
}

fn args() -> DamageArgs {
    DamageArgs {
        shape: 1,
        transform: [0.0; 16],
        origin: Vector3f::default(),
        min_damage: 3.0,
        max_damage: 3.0,
        force: 20.0,
        max_impulse: 5.0,
        strength: 1,
        hit_speed: 1.0,
        damage_type: 0,
        weapon_hit_type: 0,
        check_enemies: true,
        check_player: false,
        check_props: true,
        lethal_for_player: false,
    }
}

#[test]
fn line_of_sight_blocks_on_solid_prop() {
    let _g = mhost::TEST_LOCK.lock().unwrap_or_else(|e| e.into_inner());
    mhost::set_host(Some(fake));
    let v = Vector3f::default();
    assert!(!mhelper::check_line_of_sight(v, v, false));
    // shadow check: the one sub mesh is a shadow caster whose parent is not the body
    assert!(!mhelper::check_line_of_sight(v, v, true));
}

#[test]
fn shape_damage_pushes_prop_and_skips_unchecked_player() {
    let _g = mhost::TEST_LOCK.lock().unwrap_or_else(|e| e.into_inner());
    mhost::set_host(Some(fake));
    LOG.lock().unwrap().clear();
    let (hit, player) = mhelper::shape_damage(&args());
    assert!(hit && !player);
    let l = LOG.lock().unwrap();
    // mean contact (2,3,4), origin 0: normalized direction * min(20/2, 5)
    assert!(l[0].starts_with("impulse"));
    assert_eq!(l.iter().filter(|s| *s == "entity damage").count(), 1);
    assert!(!l.iter().any(|s| s == "player damage"));
}

#[test]
fn light_level_adds_box_lights_in_tree_order() {
    let _g = mhost::TEST_LOCK.lock().unwrap_or_else(|e| e.into_inner());
    mhost::set_host(Some(fake));
    let l = mhelper::light_level_at_pos(Vector3f::default(), None, 0.0);
    // static root 0x11: two leaves with one light each; dynamic root 0x12: one light; 0.5 per box light
    assert_eq!(l, 1.5);
    assert_eq!(
        mhelper::light_level_at_pos(Vector3f::default(), Some(&[0x900]), 0.0),
        0.0
    );
}
