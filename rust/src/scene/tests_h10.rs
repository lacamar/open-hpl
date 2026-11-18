//! Replay of `tests/golden/H10/h10_scenes.txt` (recorded from the C++ path of
//! `HPL2/tests/SceneH10Tests.cpp`) against `h10.rs`, with a mock of the C++ list shell.

use super::h10::*;
use crate::math::IDENTITY;
use crate::types::{Matrixf, Quaternion, Vector2f, Vector3f};
use std::cell::RefCell;
use std::ptr::addr_of_mut;

const GOLDEN: &str = include_str!("../../tests/golden/H10/h10_scenes.txt");

thread_local! {
    static LOG: RefCell<Vec<String>> = const { RefCell::new(Vec::new()) };
}

fn log(s: String) {
    LOG.with(|l| l.borrow_mut().push(s));
}

fn words(m: &Matrixf) -> String {
    m.m.iter()
        .flatten()
        .map(|f| format!(" {:08x}", f.to_bits()))
        .collect()
}

fn vwords(v: Vector3f) -> String {
    format!(
        " {:08x} {:08x} {:08x}",
        v.x.to_bits(),
        v.y.to_bits(),
        v.z.to_bits()
    )
}

struct Side {
    id: i32,
    pod: usize,
    has_cb: bool,
    read: bool,
    bvt: Matrixf,
}

thread_local! {
    static SIDE: RefCell<Vec<Side>> = const { RefCell::new(Vec::new()) };
}

fn side<R>(owner: usize, f: impl FnOnce(&mut Side) -> R) -> R {
    SIDE.with(|s| f(&mut s.borrow_mut()[owner - 1]))
}

fn ent(p: usize) -> *mut EntBox {
    p as *mut EntBox
}

fn node(p: usize) -> *mut NodeBox {
    p as *mut NodeBox
}

fn sd(p: usize) -> usize {
    // SAFETY: `p` is a live pod of this scene.
    unsafe { (*ent(p)).s.owner }
}

unsafe extern "C" fn h_ent_on_updated(e: usize) {
    let (id, read, pod) = side(e, |s| (s.id, s.read, s.pod));
    log(format!("e u {id}"));
    if read {
        // SAFETY: live pod.
        let w = unsafe {
            ent_update_world(&HOST, ent(pod));
            (*ent(pod)).s.world
        };
        log(format!("= r{}", words(&w)));
    }
}

unsafe extern "C" fn h_ent_callbacks(e: usize) {
    let (id, cb) = side(e, |s| (s.id, s.has_cb));
    if cb {
        log(format!("e c {id}"));
    }
}

unsafe extern "C" fn h_node_delete(_: usize) {}

static HOST: Host = Host {
    ent_on_updated: h_ent_on_updated,
    ent_callbacks: h_ent_callbacks,
    node_delete: h_node_delete,
};

fn new_ent(id: i32) -> usize {
    let owner = SIDE.with(|s| {
        let mut s = s.borrow_mut();
        s.push(Side {
            id,
            pod: 0,
            has_cb: false,
            read: false,
            bvt: IDENTITY,
        });
        s.len()
    });
    let p = ent_new(owner);
    side(owner, |s| s.pod = p as usize);
    p as usize
}

fn new_node() -> usize {
    node_new(0, false) as usize
}

struct Fnv(u64);

impl Fnv {
    fn new() -> Self {
        Fnv(0xcbf29ce484222325)
    }
    fn u32(&mut self, v: u32) {
        for i in 0..4 {
            self.0 = (self.0 ^ u64::from((v >> (8 * i)) & 255)).wrapping_mul(0x100000001b3);
        }
    }
    fn f(&mut self, v: f32) {
        self.u32(v.to_bits());
    }
    fn m(&mut self, m: &Matrixf) {
        for r in &m.m {
            for &x in r {
                self.f(x);
            }
        }
    }
}

fn bits(t: &str) -> f32 {
    f32::from_bits(u32::from_str_radix(t, 16).unwrap())
}

struct Args<'a>(std::slice::Iter<'a, &'a str>);

impl Args<'_> {
    fn i(&mut self) -> i32 {
        self.0.next().unwrap().parse().unwrap()
    }
    fn u(&mut self) -> usize {
        self.0.next().unwrap().parse().unwrap()
    }
    fn f(&mut self) -> f32 {
        bits(self.0.next().unwrap())
    }
    fn v(&mut self) -> Vector3f {
        Vector3f {
            x: self.f(),
            y: self.f(),
            z: self.f(),
        }
    }
    fn m(&mut self) -> Matrixf {
        let mut m = IDENTITY;
        for r in 0..4 {
            for c in 0..4 {
                m.m[r][c] = self.f();
            }
        }
        m
    }
}

struct EntityScene {
    nodes: Vec<usize>,
    ents: Vec<usize>,
}

impl EntityScene {
    fn op(&mut self, name: &str, a: &mut Args) {
        let n = |s: &Self, i: usize| s.nodes.get(i).copied().unwrap();
        let e = |s: &Self, i: usize| s.ents.get(i).copied().unwrap();
        match name {
            "nsp" => {
                let (c, p) = (n(self, a.u()), n(self, a.u()));
                // SAFETY: live objects.
                unsafe { node_set_parent(node(c), node(p)) };
            }
            "nae" => {
                let (nn, ee) = (n(self, a.u()), e(self, a.u()));
                // SAFETY: live objects.
                let ok = unsafe { node_add_entity(node(nn), ent(ee)) };
                log(format!("= b {}", i32::from(ok)));
            }
            "eac" => {
                let (p, c) = (e(self, a.u()), e(self, a.u()));
                // SAFETY: live objects.
                unsafe { ent_add_child(&HOST, ent(p), ent(c)) };
            }
            "enc" => {
                let (ee, nn) = (e(self, a.u()), n(self, a.u()));
                // SAFETY: live objects.
                unsafe { ent_add_node_child(&HOST, ent(ee), node(nn)) };
            }
            "nre" => {
                let (nn, ee) = (n(self, a.u()), e(self, a.u()));
                // SAFETY: live objects.
                let ok = unsafe { node_remove_entity(node(nn), ent(ee)) };
                log(format!("= b {}", i32::from(ok)));
            }
            "erc" => {
                let (p, c) = (e(self, a.u()), e(self, a.u()));
                // SAFETY: live objects.
                unsafe { ent_remove_child(ent(p), ent(c)) };
            }
            "ern" => {
                let (ee, nn) = (e(self, a.u()), n(self, a.u()));
                // SAFETY: live objects.
                unsafe { ent_remove_node_child(ent(ee), node(nn)) };
            }
            "nrc" => {
                let (p, c) = (n(self, a.u()), n(self, a.u()));
                // SAFETY: live objects.
                unsafe { node_remove_child(node(p), node(c)) };
            }
            "ep" | "ewp" => {
                let ee = e(self, a.u());
                let v = a.v();
                // SAFETY: live object.
                unsafe {
                    if name == "ep" {
                        ent_set_position(&HOST, ent(ee), v);
                    } else {
                        ent_set_world_position(&HOST, ent(ee), v);
                    }
                }
            }
            "em" | "ewm" => {
                let ee = e(self, a.u());
                let m = a.m();
                // SAFETY: live object.
                unsafe {
                    if name == "em" {
                        ent_set_matrix(&HOST, ent(ee), &m);
                    } else {
                        ent_set_world_matrix(&HOST, ent(ee), &m);
                    }
                }
            }
            "np" | "nwp" => {
                let nn = n(self, a.u());
                let v = a.v();
                // SAFETY: live object.
                unsafe {
                    if name == "np" {
                        node_set_position(&HOST, node(nn), v);
                    } else {
                        node_set_world_position(&HOST, node(nn), v);
                    }
                }
            }
            "nm" => {
                let nn = n(self, a.u());
                let f = a.i() != 0;
                let m = a.m();
                // SAFETY: live object.
                unsafe { node_set_matrix(&HOST, node(nn), &m, f) };
            }
            "nwm" => {
                let nn = n(self, a.u());
                let m = a.m();
                // SAFETY: live object.
                unsafe { node_set_world_matrix(&HOST, node(nn), &m) };
            }
            "nar" => {
                let nn = n(self, a.u());
                let v = a.v();
                let o = a.i();
                // SAFETY: live object.
                unsafe { node_add_rotation_euler(addr_of_mut!((*node(nn)).s), v, o) };
            }
            "naq" => {
                let nn = n(self, a.u());
                let q = Quaternion { v: a.v(), w: a.f() };
                // SAFETY: live object.
                unsafe { node_add_rotation(addr_of_mut!((*node(nn)).s), &q) };
            }
            "nas" | "nat" => {
                let nn = n(self, a.u());
                let v = a.v();
                // SAFETY: live object.
                unsafe {
                    if name == "nas" {
                        node_add_scale(addr_of_mut!((*node(nn)).s), v);
                    } else {
                        node_add_translation(addr_of_mut!((*node(nn)).s), v);
                    }
                }
            }
            "num" => {
                let nn = n(self, a.u());
                let f = a.i() != 0;
                // SAFETY: live object.
                unsafe { node_update_matrix(&HOST, node(nn), f) };
            }
            "npt" => {
                let nn = n(self, a.u());
                let (post, on) = (a.i() != 0, a.i() != 0);
                let m = a.m();
                // SAFETY: live object.
                unsafe {
                    let st = &mut (*node(nn)).s;
                    if post {
                        st.post = m;
                        st.use_post = on;
                    } else {
                        st.pre = m;
                        st.use_pre = on;
                    }
                }
            }
            "nap" => {
                let nn = n(self, a.u());
                let (post, f) = (a.i() != 0, a.i() != 0);
                // SAFETY: live object.
                unsafe { node_apply_anim(&HOST, node(nn), post, f) };
            }
            "eca" | "ecr" => {
                let ee = e(self, a.u());
                // SAFETY: live object.
                side(sd(ee), |s| s.has_cb = name == "eca");
            }
            "ebv" => {}
            "eap" | "ere" => {
                let ee = e(self, a.u());
                let f = a.i() != 0;
                if name == "eap" {
                    // SAFETY: live object.
                    unsafe { (*ent(ee)).s.apply_bv = f };
                } else {
                    side(sd(ee), |s| s.read = f);
                }
            }
            "etu" => {
                let ee = e(self, a.u());
                let f = a.i() != 0;
                // SAFETY: live object.
                unsafe { ent_set_transform_updated(&HOST, ent(ee), f) };
            }
            "qew" | "qewp" | "qel" => {
                let ee = e(self, a.u());
                // SAFETY: live object.
                unsafe {
                    if name != "qel" {
                        ent_update_world(&HOST, ent(ee));
                    }
                    let p = ent(ee);
                    match name {
                        "qew" => log(format!("= w{}", words(&(*p).s.world))),
                        "qel" => log(format!("= l{}", words(&(*p).s.local))),
                        _ => log(format!(
                            "= p{}",
                            vwords(Vector3f {
                                x: (*p).s.world.m[0][3],
                                y: (*p).s.world.m[1][3],
                                z: (*p).s.world.m[2][3]
                            })
                        )),
                    }
                }
            }
            "qnw" | "qnwp" => {
                let nn = n(self, a.u());
                // SAFETY: live object.
                unsafe {
                    node_update_world(&HOST, node(nn));
                    let w = (*node(nn)).s.world;
                    if name == "qnw" {
                        log(format!("= w{}", words(&w)));
                    } else {
                        log(format!(
                            "= p{}",
                            vwords(Vector3f {
                                x: w.m[0][3],
                                y: w.m[1][3],
                                z: w.m[2][3]
                            })
                        ));
                    }
                }
            }
            "qbv" => {
                let ee = e(self, a.u());
                // SAFETY: live object.
                unsafe {
                    if ent_bv_dirty(&HOST, ent(ee)) {
                        let w = (*ent(ee)).s.world;
                        side(sd(ee), |s| s.bvt = w);
                    }
                    let bvt = side(sd(ee), |s| s.bvt);
                    log(format!("= bvt{}", words(&bvt)));
                }
            }
            "qs" => {
                let ee = e(self, a.u());
                // SAFETY: live object.
                unsafe {
                    log(format!(
                        "= s {} {}",
                        i32::from((*ent(ee)).s.transform_updated),
                        (*ent(ee)).s.count
                    ));
                }
            }
            "end" => {
                let mut h = Fnv::new();
                // SAFETY: live objects.
                unsafe {
                    for (i, &nn) in self.nodes.iter().enumerate() {
                        let mut o = Fnv::new();
                        o.m(&(*node(nn)).s.local);
                        node_update_world(&HOST, node(nn));
                        o.m(&(*node(nn)).s.world);
                        log(format!("h n {i} {:016x}", o.0));
                        h.m(&(*node(nn)).s.local);
                        h.m(&(*node(nn)).s.world);
                    }
                    for (i, &ee) in self.ents.iter().enumerate() {
                        if ent_bv_dirty(&HOST, ent(ee)) {
                            let w = (*ent(ee)).s.world;
                            side(sd(ee), |s| s.bvt = w);
                        }
                        let p = ent(ee);
                        let mut o = Fnv::new();
                        for h in [&mut h, &mut o] {
                            h.m(&(*p).s.local);
                            h.m(&(*p).s.world);
                            h.u32(u32::from((*p).s.transform_updated));
                            h.u32((*p).s.count as u32);
                            h.u32(u32::from((*p).s.update_bv));
                        }
                        log(format!(
                            "h e {i} {:016x} {} {}",
                            o.0,
                            i32::from((*p).s.transform_updated),
                            (*p).s.count
                        ));
                    }
                }
                log(format!("h {:016x}", h.0));
            }
            other => panic!("unknown op {other}"),
        }
    }
}

impl Drop for EntityScene {
    fn drop(&mut self) {
        // SAFETY: every handle came from `Box::into_raw` and is freed once.
        unsafe {
            for &e in &self.ents {
                ent_free(ent(e));
            }
            for &n in &self.nodes {
                node_free(&HOST, node(n));
            }
        }
    }
}

struct CamScene {
    s: CamState,
    node_pos: Vector3f,
}

impl CamScene {
    fn new() -> Self {
        let aspect = 4.0f32 / 3.0;
        let s = CamState {
            position: Vector3f::default(),
            fov: (70.0f32 / 360.0f32) * std::f32::consts::TAU,
            aspect,
            far: 1000.0,
            near: 0.05,
            view_size: Vector2f {
                x: 2.0 * aspect,
                y: 2.0,
            },
            projection_type: 0,
            pitch: 0.0,
            yaw: 0.0,
            roll: 0.0,
            pitch_min: -crate::math::PI2,
            pitch_max: crate::math::PI2,
            yaw_min: 0.0,
            yaw_max: 0.0,
            rotate_mode: 0,
            move_mode: 1,
            view: IDENTITY,
            projection: IDENTITY,
            mv: IDENTITY,
            prev_view: IDENTITY,
            prev_projection: IDENTITY,
            rotation: IDENTITY,
            inf_far: false,
            view_updated: true,
            projection_updated: true,
            move_updated: true,
            frustum_updated: true,
        };
        CamScene {
            s,
            node_pos: Vector3f::default(),
        }
    }

    fn hash(&self) -> String {
        let s = &self.s;
        let mut h = Fnv::new();
        h.f(s.position.x);
        h.f(s.position.y);
        h.f(s.position.z);
        for f in [s.fov, s.aspect, s.far, s.near, s.view_size.x, s.view_size.y] {
            h.f(f);
        }
        h.u32(s.projection_type as u32);
        for f in [
            s.pitch,
            s.yaw,
            s.roll,
            s.pitch_min,
            s.pitch_max,
            s.yaw_min,
            s.yaw_max,
        ] {
            h.f(f);
        }
        h.u32(s.rotate_mode as u32);
        h.u32(s.move_mode as u32);
        h.u32(u32::from(s.inf_far));
        h.m(&s.prev_view);
        h.m(&s.prev_projection);
        h.m(&s.rotation);
        h.f(self.node_pos.x);
        h.f(self.node_pos.y);
        h.f(self.node_pos.z);
        format!("h {:016x}", h.0)
    }

    fn op(&mut self, name: &str, a: &mut Args) {
        let s = &mut self.s;
        match name {
            "cpos" => {
                a.u();
                s.set_position(a.v());
                self.node_pos = self.s.position;
            }
            "cpitch" => s.set_pitch(a.f()),
            "cyaw" => s.set_yaw(a.f()),
            "croll" => s.set_roll(a.f()),
            "capitch" => s.add_pitch(a.f()),
            "cayaw" => s.add_yaw(a.f()),
            "caroll" => s.add_roll(a.f()),
            "cmove" => {
                let axis = a.i();
                s.move_along(axis, a.f());
                self.node_pos = self.s.position;
            }
            "cfov" => s.set_fov(a.f()),
            "caspect" => s.set_aspect(a.f()),
            "cfar" => s.set_far(a.f()),
            "cnear" => s.set_near(a.f()),
            "cproj" => s.set_projection_type(a.i()),
            "cortho" => s.set_ortho_view_size(Vector2f { x: a.f(), y: a.f() }),
            "cinf" => s.set_inf_far(a.i() != 0),
            "crot" => s.set_rotate_mode(a.i()),
            "cmm" => s.set_move_mode(a.i()),
            "creset" => s.reset_rotation(),
            "crow" => {
                let row = a.u();
                s.set_rotation_row(row, a.v());
            }
            "crm" => s.set_rotation_matrix(&a.m()),
            "clim" => {
                let (lo, hi) = (a.f(), a.f());
                s.pitch_min = lo;
                s.pitch_max = hi;
                s.yaw_min = lo * 2.0;
                s.yaw_max = hi * 2.0;
            }
            "cqv" => log(format!("= v{}", words(s.view_matrix()))),
            "cqp" => log(format!("= p{}", words(s.projection_matrix()))),
            "cqm" => {
                s.update_move();
                log(format!("= m{}", words(&s.mv)));
            }
            "cqd" => {
                let f = s.view_row(2, true);
                let r = s.view_row(0, false);
                let u = s.view_row(1, false);
                log(format!("= d{}{}{}", vwords(f), vwords(r), vwords(u)));
            }
            "cqf" => {
                let f = s.frustum_begin();
                if f & 1 != 0 {
                    s.projection_matrix();
                    s.view_matrix();
                    s.frustum_end(f & 2 != 0);
                }
                log(format!("= fi {}", i32::from(s.inf_far)));
            }
            "cqu" => {
                s.view_matrix();
            }
            "cqt" => log(format!(
                "= t {}",
                i32::from(self.node_pos.x == s.position.x)
            )),
            "cend" => {}
            other => panic!("unknown camera op {other}"),
        }
    }
}

#[test]
fn replay_golden() {
    let mut scene: Option<EntityScene> = None;
    let mut cam: Option<CamScene> = None;
    let mut block: Vec<String> = Vec::new();
    let mut pending: Option<String> = None;
    let (mut ops, mut scenes) = (0, 0);

    let check = |op: &Option<String>, expected: &mut Vec<String>| {
        if let Some(op) = op {
            let actual = LOG.with(|l| std::mem::take(&mut *l.borrow_mut()));
            assert_eq!(&actual, expected, "op `{op}`");
        }
        expected.clear();
    };

    for line in GOLDEN.lines().chain(std::iter::once("stop")) {
        let (tag, rest) = line.split_once(' ').unwrap_or((line, ""));
        match tag {
            "o" => {
                check(&pending, &mut block);
                let toks: Vec<&str> = rest.split(' ').collect();
                let mut a = Args(toks.get(1..).unwrap_or(&[]).iter());
                let name = toks.first().copied().unwrap();
                if let Some(s) = scene.as_mut() {
                    s.op(name, &mut a);
                } else if let Some(c) = cam.as_mut() {
                    c.op(name, &mut a);
                    if name != "cend" {
                        log(c.hash());
                    }
                }
                ops += 1;
                pending = Some(line.to_string());
            }
            "=" | "e" | "h" => block.push(line.to_string()),
            "~" => {}
            _ => {
                check(&pending, &mut block);
                pending = None;
                scene = None;
                cam = None;
                if tag == "scene" {
                    let f: Vec<usize> = rest.split(' ').map(|x| x.parse().unwrap()).collect();
                    scenes += 1;
                    drop(scene.take());
                    SIDE.with(|s| s.borrow_mut().clear());
                    scene = Some(EntityScene {
                        nodes: (0..f[1]).map(|_| new_node()).collect(),
                        ents: (0..f[2]).map(|i| new_ent(i as i32)).collect(),
                    });
                } else if tag == "cscene" {
                    scenes += 1;
                    cam = Some(CamScene::new());
                }
            }
        }
    }
    assert!(ops > 10_000 && scenes >= 40, "{ops} ops, {scenes} scenes");
}
