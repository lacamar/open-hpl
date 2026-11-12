//! Replays `tests/golden/H14/h14_vectors.txt`: every `ohpl_h14_*` call a C++ scene run made
//! (inputs, host answers in order, outputs). The scene dumps recorded with it are bit-identical
//! between the C++ path and the Rust path (`h14_scenes.txt`).

use super::char_aux::{Aux, BodyHost, CamIn, CamOut, Conn, EntIn, Grav};
use super::character::CharState;
use super::host::H;
use crate::types::{Matrixf, Vector3f};
use std::cell::RefCell;
use std::collections::{HashMap, VecDeque};
use std::mem::{MaybeUninit, size_of};

const VECTORS: &str = include_str!("../../tests/golden/H14/h14_vectors.txt");
const SCENES: &str = include_str!("../../tests/golden/H14/h14_scenes.txt");

fn unhex(s: &str) -> Vec<u8> {
    if s == "-" {
        return Vec::new();
    }
    (0..s.len() / 2)
        .map(|i| u8::from_str_radix(&s[2 * i..2 * i + 2], 16).unwrap())
        .collect()
}

fn read<T: Copy>(s: &str) -> T {
    let b = unhex(s);
    assert_eq!(b.len(), size_of::<T>());
    let mut v = MaybeUninit::<T>::zeroed();
    // SAFETY: `T` is a plain repr(C) struct and `b` holds exactly its bytes.
    unsafe {
        std::ptr::copy_nonoverlapping(b.as_ptr(), v.as_mut_ptr().cast::<u8>(), b.len());
        v.assume_init()
    }
}

fn bytes<T>(v: &T) -> &[u8] {
    // SAFETY: reading the bytes of a live value.
    unsafe { std::slice::from_raw_parts((v as *const T).cast::<u8>(), size_of::<T>()) }
}

fn f32_of(s: &str) -> f32 {
    read::<f32>(s)
}

struct Entry {
    name: String,
    args: Vec<u8>,
    ret: Vec<u8>,
}

struct Mock(RefCell<VecDeque<Entry>>);

impl Mock {
    fn new(calls: &str) -> Self {
        let q = if calls == "-" {
            VecDeque::new()
        } else {
            calls
                .split(';')
                .map(|e| {
                    let mut p = e.split(',');
                    Entry {
                        name: p.next().unwrap().to_string(),
                        args: unhex(p.next().unwrap()),
                        ret: unhex(p.next().unwrap()),
                    }
                })
                .collect()
        };
        Mock(RefCell::new(q))
    }

    fn pop(&self, name: &str, args: &[u8]) -> Vec<u8> {
        let e = self
            .0
            .borrow_mut()
            .pop_front()
            .expect("host call not recorded");
        assert_eq!(e.name, name);
        assert_eq!(e.args, args, "{name} arguments");
        e.ret
    }

    fn done(&self) {
        assert!(self.0.borrow().is_empty(), "recorded host calls left over");
    }

    fn ret<T: Copy>(&self, name: &str, args: &[u8]) -> T {
        let r = self.pop(name, args);
        assert_eq!(r.len(), size_of::<T>());
        let mut v = MaybeUninit::<T>::zeroed();
        // SAFETY: as in `read`.
        unsafe {
            std::ptr::copy_nonoverlapping(r.as_ptr(), v.as_mut_ptr().cast::<u8>(), r.len());
            v.assume_init()
        }
    }
}

fn vbytes(a: Vector3f) -> Vec<u8> {
    bytes(&a).to_vec()
}

impl BodyHost for Mock {
    fn mass(&self, _: H) -> f32 {
        self.ret("mass", &[])
    }
    fn world_matrix(&self, _: H) -> Matrixf {
        self.ret("worldMatrix", &[])
    }
    fn local_matrix(&self, _: H) -> Matrixf {
        self.ret("localMatrix", &[])
    }
    fn world_position(&self, _: H) -> Vector3f {
        self.ret("worldPosition", &[])
    }
    fn inertia(&self, _: H) -> Matrixf {
        self.ret("inertia", &[])
    }
    fn torque_from_force(&self, _: H, f: Vector3f, p: Vector3f) -> Vector3f {
        self.ret("torqueFromForce", &[vbytes(f), vbytes(p)].concat())
    }
    fn velocity_at_position(&self, _: H, p: Vector3f) -> Vector3f {
        self.ret("velocityAtPosition", &vbytes(p))
    }
    fn add_force(&self, _: H, f: Vector3f) {
        self.pop("addForce", &vbytes(f));
    }
    fn add_torque(&self, _: H, t: Vector3f) {
        self.pop("addTorque", &vbytes(t));
    }
    fn gravity_attachment_rotation(&self, _: H) -> bool {
        self.ret::<u8>("gravityAttachmentRotation", &[]) != 0
    }
    fn gravity_attachment_velocity(&self, _: H) -> bool {
        self.ret::<u8>("gravityAttachmentVelocity", &[]) != 0
    }
    fn gravity_attachment_axes(&self, _: H) -> i32 {
        self.ret("gravityAttachmentAxes", &[])
    }
}

fn state_eq(got: &CharState, want: &CharState) {
    assert_eq!(bytes(got)[..363], bytes(want)[..363], "character state");
}

#[test]
fn h14_vectors_replay_bit_exact() {
    let mut auxes: HashMap<&str, Aux> = HashMap::new();
    let mut counts: HashMap<&str, usize> = HashMap::new();
    for line in VECTORS.lines() {
        let f: Vec<&str> = line.split(' ').collect();
        let (tag, id) = (f[0], f[1]);
        *counts.entry(tag).or_default() += 1;
        match tag {
            "new" => {
                auxes.insert(id, Aux::new());
            }
            "free" => {
                auxes.remove(id);
            }
            "reset" => auxes.get_mut(id).unwrap().reset_pids(),
            "clear" => auxes.get_mut(id).unwrap().clear_smoothing(),
            "clearcam" => auxes.get_mut(id).unwrap().clear_camera(),
            "setup" => auxes
                .get_mut(id)
                .unwrap()
                .setup_safe_positions(f32_of(f[2]), f32_of(f[3])),
            "safeup" => auxes
                .get_mut(id)
                .unwrap()
                .update_safe_positions(f32_of(f[2]), read(f[3])),
            "safepos" => {
                let got = auxes.get(id).unwrap().safe_position(f32_of(f[2]));
                assert_eq!(got.is_some(), read::<u8>(f[3]) != 0);
                if let Some(p) = got {
                    assert_eq!(bytes(&p), unhex(f[4]).as_slice());
                }
            }
            "bodypos" => {
                let conn: Conn = read(f[2]);
                let mock = Mock::new(f[3]);
                let got = super::char_aux::body_connection_pos(&mock, &conn);
                mock.done();
                assert_eq!(bytes(&got), unhex(f[4]).as_slice());
            }
            "charpos" => {
                let st: CharState = read(f[2]);
                let conn: Conn = read(f[3]);
                let got = super::char_aux::char_connection_pos(&st, &conn);
                assert_eq!(bytes(&got), unhex(f[4]).as_slice());
            }
            "bodyconn" => {
                let st: CharState = read(f[2]);
                let conn: Conn = read(f[3]);
                let mock = Mock::new(f[6]);
                auxes.get_mut(id).unwrap().update_body_connection(
                    &mock,
                    &st,
                    &conn,
                    read::<u8>(f[4]) != 0,
                    f32_of(f[5]),
                );
                mock.done();
            }
            "charconn" => {
                let mut st: CharState = read(f[2]);
                let conn: Conn = read(f[3]);
                let mock = Mock::new(f[5]);
                let recorded: bool = read::<u8>(f[6]) != 0;
                assert_eq!(conn.affect_char, recorded);
                if recorded {
                    let force = auxes.get_mut(id).unwrap().update_character_connection(
                        &mock,
                        &mut st,
                        &conn,
                        f32_of(f[4]),
                    );
                    assert_eq!(bytes(&force), unhex(f[7]).as_slice());
                }
                mock.done();
                state_eq(&st, &read(f[8]));
            }
            "grav" => {
                let mut st: CharState = read(f[2]);
                let mut g: Grav = read(f[3]);
                let mock = Mock::new(f[4]);
                Aux::update_gravity_attachment(&mock, &mut st, &mut g);
                mock.done();
                state_eq(&st, &read(f[5]));
                let want: Grav = read(f[6]);
                assert_eq!(bytes(&g)[..93], bytes(&want)[..93], "gravity attachment");
            }
            "cam" => {
                let got: CamOut = auxes
                    .get_mut(id)
                    .unwrap()
                    .update_camera(&read::<CamIn>(f[2]));
                let want: CamOut = read(f[3]);
                assert_eq!(got.set, want.set);
                if want.set {
                    assert_eq!(bytes(&got)[..16], bytes(&want)[..16]);
                }
            }
            "ent" => {
                let got = auxes
                    .get_mut(id)
                    .unwrap()
                    .update_entity(&read::<EntIn>(f[2]));
                assert_eq!(bytes(&got), unhex(f[3]).as_slice());
            }
            other => panic!("unknown record {other}"),
        }
    }
    for tag in ["bodyconn", "charconn", "grav", "cam", "ent", "safeup"] {
        assert!(
            counts.get(tag).copied().unwrap_or(0) > 0,
            "{tag} not replayed"
        );
    }
}

#[test]
fn h14_scene_dump_lists_every_scene() {
    let ends = SCENES.lines().filter(|l| l.starts_with("end ")).count();
    assert_eq!(ends, 13);
    assert!(SCENES.contains("end connect_all steps 480 hash 32ea6a1cae7dbaab"));
}

#[test]
fn rope_plan_counts_segments() {
    use super::rope::{RopePlan, rope_plan};
    assert_eq!(
        rope_plan(1.0, 0.25),
        RopePlan {
            wanted: 3,
            first_segment: 0.25
        }
    );
    assert_eq!(rope_plan(1.1, 0.25).wanted, 4);
    assert_eq!(
        rope_plan(2.0, 0.0),
        RopePlan {
            wanted: 0,
            first_segment: 2.0
        }
    );
}
