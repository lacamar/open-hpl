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

struct MEnt {
    id: i32,
    local: Matrixf,
    world: Matrixf,
    tu: bool,
    ubv: bool,
    apply: bool,
    count: i32,
    parent_node: usize,
    parent_entity: usize,
    children: Vec<usize>,
    node_children: Vec<usize>,
    has_cb: bool,
    read: bool,
    bvt: Matrixf,
}

struct MNode {
    st: NodeState,
    children: Vec<usize>,
    entities: Vec<usize>,
}

fn ent(p: usize) -> *mut MEnt {
    p as *mut MEnt
}

fn node(p: usize) -> *mut MNode {
    p as *mut MNode
}

fn fill(list: &[usize], out: *mut usize, cap: usize) -> usize {
    for (i, &x) in list.iter().enumerate().take(cap) {
        // SAFETY: the host contract gives `cap` writable entries.
        unsafe { *out.add(i) = x };
    }
    list.len()
}

unsafe extern "C" fn h_ent_view(e: usize, v: *mut EntView) {
    let p = ent(e);
    // SAFETY: `e` is a live boxed `MEnt`; `v` is valid.
    unsafe {
        *v = EntView {
            local: addr_of_mut!((*p).local),
            world: addr_of_mut!((*p).world),
            transform_updated: addr_of_mut!((*p).tu),
            update_bv: addr_of_mut!((*p).ubv),
            apply_bv: addr_of_mut!((*p).apply),
            count: addr_of_mut!((*p).count),
            parent_node: (*p).parent_node,
            parent_entity: (*p).parent_entity,
        };
    }
}

unsafe extern "C" fn h_ent_children(e: usize, out: *mut usize, cap: usize) -> usize {
    // SAFETY: live `MEnt`.
    unsafe { fill(&(*ent(e)).children, out, cap) }
}

unsafe extern "C" fn h_ent_node_children(e: usize, out: *mut usize, cap: usize) -> usize {
    // SAFETY: live `MEnt`.
    unsafe { fill(&(*ent(e)).node_children, out, cap) }
}

unsafe extern "C" fn h_ent_on_updated(e: usize) {
    // SAFETY: live `MEnt`.
    let (id, read) = unsafe { ((*ent(e)).id, (*ent(e)).read) };
    log(format!("e u {id}"));
    if read {
        // SAFETY: as above.
        let w = unsafe {
            ent_update_world(&HOST, e);
            (*ent(e)).world
        };
        log(format!("= r{}", words(&w)));
    }
}

unsafe extern "C" fn h_ent_callbacks(e: usize) {
    // SAFETY: live `MEnt`.
    let (id, cb) = unsafe { ((*ent(e)).id, (*ent(e)).has_cb) };
    if cb {
        log(format!("e c {id}"));
    }
}

unsafe extern "C" fn h_node_state(n: usize) -> *mut NodeState {
    // SAFETY: live `MNode`.
    unsafe { addr_of_mut!((*node(n)).st) }
}

unsafe extern "C" fn h_node_children(n: usize, out: *mut usize, cap: usize) -> usize {
    // SAFETY: live `MNode`.
    unsafe { fill(&(*node(n)).children, out, cap) }
}

unsafe extern "C" fn h_node_entities(n: usize, out: *mut usize, cap: usize) -> usize {
    // SAFETY: live `MNode`.
    unsafe { fill(&(*node(n)).entities, out, cap) }
}

static HOST: Host = Host {
    ent_view: h_ent_view,
    ent_children: h_ent_children,
    ent_node_children: h_ent_node_children,
    ent_on_updated: h_ent_on_updated,
    ent_callbacks: h_ent_callbacks,
    node_state: h_node_state,
    node_children: h_node_children,
    node_entities: h_node_entities,
};

const IDENTITY_Q: Quaternion = Quaternion {
    v: Vector3f {
        x: 0.0,
        y: 0.0,
        z: 0.0,
    },
    w: 1.0,
};

fn new_ent(id: i32) -> usize {
    Box::into_raw(Box::new(MEnt {
        id,
        local: IDENTITY,
        world: IDENTITY,
        tu: true,
        ubv: true,
        apply: true,
        count: 0,
        parent_node: 0,
        parent_entity: 0,
        children: Vec::new(),
        node_children: Vec::new(),
        has_cb: false,
        read: false,
        bvt: IDENTITY,
    })) as usize
}

fn new_node() -> usize {
    Box::into_raw(Box::new(MNode {
        st: NodeState {
            local: IDENTITY,
            world: IDENTITY,
            pre: IDENTITY,
            post: IDENTITY,
            rotation: IDENTITY_Q,
            scale: Vector3f {
                x: 1.0,
                y: 1.0,
                z: 1.0,
            },
            translation: Vector3f::default(),
            parent: 0,
            entity_parent: 0,
            transform_updated: true,
            use_pre: false,
            use_post: false,
        },
        children: Vec::new(),
        entities: Vec::new(),
    })) as usize
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
    fn erase_ent_child(&mut self, parent: usize, child: usize) {
        // SAFETY: live objects of this scene.
        unsafe {
            (*ent(parent)).children.retain(|&c| c != child);
            (*ent(child)).parent_entity = 0;
        }
    }

    fn erase_node_child(&mut self, parent: usize, child: usize) {
        // SAFETY: live objects of this scene.
        unsafe {
            (*ent(parent)).node_children.retain(|&c| c != child);
            (*node(child)).st.entity_parent = 0;
        }
    }

    fn op(&mut self, name: &str, a: &mut Args) {
        let n = |s: &Self, i: usize| s.nodes.get(i).copied().unwrap();
        let e = |s: &Self, i: usize| s.ents.get(i).copied().unwrap();
        match name {
            "nsp" => {
                let (c, p) = (n(self, a.u()), n(self, a.u()));
                // SAFETY: live objects.
                unsafe {
                    (*node(c)).st.parent = p;
                    (*node(p)).children.push(c);
                }
            }
            "nae" => {
                let (nn, ee) = (n(self, a.u()), e(self, a.u()));
                // SAFETY: live objects.
                unsafe {
                    if (*ent(ee)).parent_node != 0 {
                        log("= b 0".into());
                    } else {
                        (*node(nn)).entities.push(ee);
                        (*ent(ee)).parent_node = nn;
                        log("= b 1".into());
                    }
                }
            }
            "eac" => {
                let (p, c) = (e(self, a.u()), e(self, a.u()));
                // SAFETY: live objects.
                unsafe {
                    let old = (*ent(c)).parent_entity;
                    if old != 0 {
                        self.erase_ent_child(old, c);
                    }
                    (*ent(p)).children.push(c);
                    (*ent(c)).parent_entity = p;
                    ent_set_transform_updated(&HOST, c, true);
                }
            }
            "enc" => {
                let (ee, nn) = (e(self, a.u()), n(self, a.u()));
                // SAFETY: live objects.
                unsafe {
                    let old = (*node(nn)).st.entity_parent;
                    if old != 0 {
                        self.erase_node_child(old, nn);
                    }
                    (*ent(ee)).node_children.push(nn);
                    (*node(nn)).st.entity_parent = ee;
                    node_set_world_transform_updated(&HOST, nn);
                }
            }
            "nre" => {
                let (nn, ee) = (n(self, a.u()), e(self, a.u()));
                // SAFETY: live objects.
                unsafe {
                    let list = &mut (*node(nn)).entities;
                    if let Some(i) = list.iter().position(|&x| x == ee) {
                        (*ent(ee)).parent_node = 0;
                        list.remove(i);
                        log("= b 1".into());
                    } else {
                        log("= b 0".into());
                    }
                }
            }
            "erc" => {
                let (p, c) = (e(self, a.u()), e(self, a.u()));
                self.erase_ent_child(p, c);
            }
            "ern" => {
                let (ee, nn) = (e(self, a.u()), n(self, a.u()));
                self.erase_node_child(ee, nn);
            }
            "nrc" => {
                let (p, c) = (n(self, a.u()), n(self, a.u()));
                // SAFETY: live objects.
                unsafe {
                    let list = &mut (*node(p)).children;
                    if let Some(i) = list.iter().position(|&x| x == c) {
                        list.remove(i);
                    }
                }
            }
            "ep" | "ewp" => {
                let ee = e(self, a.u());
                let v = a.v();
                // SAFETY: live object.
                unsafe {
                    if name == "ep" {
                        ent_set_position(&HOST, ee, v);
                    } else {
                        ent_set_world_position(&HOST, ee, v);
                    }
                }
            }
            "em" | "ewm" => {
                let ee = e(self, a.u());
                let m = a.m();
                // SAFETY: live object.
                unsafe {
                    if name == "em" {
                        ent_set_matrix(&HOST, ee, &m);
                    } else {
                        ent_set_world_matrix(&HOST, ee, &m);
                    }
                }
            }
            "np" | "nwp" => {
                let nn = n(self, a.u());
                let v = a.v();
                // SAFETY: live object.
                unsafe {
                    if name == "np" {
                        node_set_position(&HOST, nn, v);
                    } else {
                        node_set_world_position(&HOST, nn, v);
                    }
                }
            }
            "nm" => {
                let nn = n(self, a.u());
                let f = a.i() != 0;
                let m = a.m();
                // SAFETY: live object.
                unsafe { node_set_matrix(&HOST, nn, &m, f) };
            }
            "nwm" => {
                let nn = n(self, a.u());
                let m = a.m();
                // SAFETY: live object.
                unsafe { node_set_world_matrix(&HOST, nn, &m) };
            }
            "nar" => {
                let nn = n(self, a.u());
                let v = a.v();
                let o = a.i();
                // SAFETY: live object.
                unsafe { node_add_rotation_euler(h_node_state(nn), v, o) };
            }
            "naq" => {
                let nn = n(self, a.u());
                let q = Quaternion { v: a.v(), w: a.f() };
                // SAFETY: live object.
                unsafe { node_add_rotation(h_node_state(nn), &q) };
            }
            "nas" | "nat" => {
                let nn = n(self, a.u());
                let v = a.v();
                // SAFETY: live object.
                unsafe {
                    if name == "nas" {
                        node_add_scale(h_node_state(nn), v);
                    } else {
                        node_add_translation(h_node_state(nn), v);
                    }
                }
            }
            "num" => {
                let nn = n(self, a.u());
                let f = a.i() != 0;
                // SAFETY: live object.
                unsafe { node_update_matrix(&HOST, nn, f) };
            }
            "npt" => {
                let nn = n(self, a.u());
                let (post, on) = (a.i() != 0, a.i() != 0);
                let m = a.m();
                // SAFETY: live object.
                unsafe {
                    let st = &mut (*node(nn)).st;
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
                unsafe { node_apply_anim(&HOST, nn, post, f) };
            }
            "eca" | "ecr" => {
                let ee = e(self, a.u());
                // SAFETY: live object.
                unsafe { (*ent(ee)).has_cb = name == "eca" };
            }
            "ebv" => {}
            "eap" | "ere" => {
                let ee = e(self, a.u());
                let f = a.i() != 0;
                // SAFETY: live object.
                unsafe {
                    if name == "eap" {
                        (*ent(ee)).apply = f;
                    } else {
                        (*ent(ee)).read = f;
                    }
                }
            }
            "etu" => {
                let ee = e(self, a.u());
                let f = a.i() != 0;
                // SAFETY: live object.
                unsafe { ent_set_transform_updated(&HOST, ee, f) };
            }
            "qew" | "qewp" | "qel" => {
                let ee = e(self, a.u());
                // SAFETY: live object.
                unsafe {
                    if name != "qel" {
                        ent_update_world(&HOST, ee);
                    }
                    let p = ent(ee);
                    match name {
                        "qew" => log(format!("= w{}", words(&(*p).world))),
                        "qel" => log(format!("= l{}", words(&(*p).local))),
                        _ => log(format!(
                            "= p{}",
                            vwords(Vector3f {
                                x: (*p).world.m[0][3],
                                y: (*p).world.m[1][3],
                                z: (*p).world.m[2][3]
                            })
                        )),
                    }
                }
            }
            "qnw" | "qnwp" => {
                let nn = n(self, a.u());
                // SAFETY: live object.
                unsafe {
                    node_update_world(&HOST, nn);
                    let w = (*node(nn)).st.world;
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
                    if ent_bv_dirty(&HOST, ee) {
                        (*ent(ee)).bvt = (*ent(ee)).world;
                    }
                    log(format!("= bvt{}", words(&(*ent(ee)).bvt)));
                }
            }
            "qs" => {
                let ee = e(self, a.u());
                // SAFETY: live object.
                unsafe {
                    log(format!(
                        "= s {} {}",
                        i32::from((*ent(ee)).tu),
                        (*ent(ee)).count
                    ));
                }
            }
            "end" => {
                let mut h = Fnv::new();
                // SAFETY: live objects.
                unsafe {
                    for (i, &nn) in self.nodes.iter().enumerate() {
                        let mut o = Fnv::new();
                        o.m(&(*node(nn)).st.local);
                        node_update_world(&HOST, nn);
                        o.m(&(*node(nn)).st.world);
                        log(format!("h n {i} {:016x}", o.0));
                        h.m(&(*node(nn)).st.local);
                        h.m(&(*node(nn)).st.world);
                    }
                    for (i, &ee) in self.ents.iter().enumerate() {
                        if ent_bv_dirty(&HOST, ee) {
                            (*ent(ee)).bvt = (*ent(ee)).world;
                        }
                        let p = ent(ee);
                        let mut o = Fnv::new();
                        for h in [&mut h, &mut o] {
                            h.m(&(*p).local);
                            h.m(&(*p).world);
                            h.u32(u32::from((*p).tu));
                            h.u32((*p).count as u32);
                            h.u32(u32::from((*p).ubv));
                        }
                        log(format!(
                            "h e {i} {:016x} {} {}",
                            o.0,
                            i32::from((*p).tu),
                            (*p).count
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
                drop(Box::from_raw(ent(e)));
            }
            for &n in &self.nodes {
                drop(Box::from_raw(node(n)));
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
