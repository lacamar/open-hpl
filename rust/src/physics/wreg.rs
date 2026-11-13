//! `iPhysicsWorld` object lists, lookups, destruction order and the per-step update sequence
//! (`OHPL_IMPL=rust`). The objects stay C++; every access goes through one host `op`.
//! No registry lock is held across an op: ops re-enter this module (DestroyShape, DestroyJoint, ...).

use super::book::{self, Lists};
use super::world::{self, BodyFilter};
use crate::types::{Matrixf, Vector3f};

pub const SHAPE: usize = 0;
pub const BODY: usize = 1;
pub const CHAR: usize = 2;
pub const JOINT: usize = 3;
pub const CTRL: usize = 4;
pub const ROPE: usize = 5;

pub const WO_NAME: i32 = 1;
pub const WO_DELETE: i32 = 2;
pub const WO_BODY_DESTROY: i32 = 3;
pub const WO_BODY_IN_UPDATE: i32 = 4;
pub const WO_BODY_SET_IN_UPDATE: i32 = 5;
pub const WO_BODY_BEFORE: i32 = 6;
pub const WO_BODY_AFTER: i32 = 7;
pub const WO_CLEAR_CONTACTS: i32 = 8;
pub const WO_CTRL_UPDATE: i32 = 9;
pub const WO_ROPE_ACTIVE: i32 = 10;
pub const WO_ROPE_BEFORE: i32 = 11;
pub const WO_ROPE_AFTER: i32 = 12;
pub const WO_CHAR_UPDATE: i32 = 13;
pub const WO_SIMULATE: i32 = 14;
pub const WO_JOINT_UPDATE: i32 = 15;
pub const WO_JOINT_BREAKAGE: i32 = 16;
pub const WO_SHAPE_DEC_USER: i32 = 17;
pub const WO_SHAPE_HAS_USERS: i32 = 18;
pub const WO_ROPE_UID: i32 = 19;
pub const WO_MAT_NEEDS_PRELOAD: i32 = 20;
pub const WO_MAT_PRELOAD: i32 = 21;
pub const WO_BODIES_IN_BV: i32 = 22;
pub const WO_TEMP_LEN: i32 = 23;
pub const WO_TEMP_AT: i32 = 24;
pub const WO_BODY_MASS: i32 = 25;
pub const WO_BV_NEW: i32 = 26;
pub const WO_BV_FREE: i32 = 27;
pub const WO_BV_INTERSECTS: i32 = 28;
pub const WO_BODY_ENABLE: i32 = 29;
pub const WO_BODY_FADE: i32 = 30;
pub const WO_JOINT_FADE: i32 = 31;
pub const WO_CONTACTS_LEN: i32 = 32;
pub const WO_RENDER_CONTACT: i32 = 33;
pub const WO_QUERY_FILTER: i32 = 34;
pub const WO_CD_NEW: i32 = 35;
pub const WO_CD_FREE: i32 = 36;
pub const WO_CD_COLLIDE: i32 = 37;
pub const WO_CD_LEN: i32 = 38;
pub const WO_CD_POINT: i32 = 39;
pub const WO_CALLBACK: i32 = 40;

/// Argument/result block of one host op; the fields each op reads are listed in `RustPhysBook.h`.
#[repr(C)]
pub struct Wa {
    pub p: [usize; 6],
    pub f: [f32; 2],
    pub i: [i32; 2],
    pub s: *const u8,
    pub n: usize,
    pub rf: f32,
    pub rv: [f32; 3],
}

const _: () = assert!(std::mem::size_of::<Wa>() == 96 && std::mem::size_of::<Query>() == 24);

pub type WorldOp = unsafe extern "C" fn(i32, *mut Wa) -> usize;

/// What `CheckShapeWorldCollision` filters bodies by.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Query {
    pub skip_body: usize,
    pub flags: u32,
    pub collide_character: bool,
    pub is_character: bool,
    pub skip_static: bool,
    pub min_push: i32,
}

#[derive(Clone, Copy)]
struct Ops {
    op: WorldOp,
    me: usize,
}

impl Ops {
    fn raw(self, code: i32, p: [usize; 6], f0: f32, i0: i32) -> Wa {
        let mut a = Wa {
            p,
            f: [f0, 0.0],
            i: [i0, 0],
            s: std::ptr::null(),
            n: 0,
            rf: 0.0,
            rv: [0.0; 3],
        };
        // SAFETY: the op was registered with the world and outlives it; `a` is a live block.
        let r = unsafe { (self.op)(code, &mut a) };
        a.p[5] = r;
        a
    }

    fn r(self, code: i32, p: [usize; 6]) -> usize {
        self.raw(code, p, 0.0, 0).p[5]
    }

    fn name(self, kind: usize, h: usize) -> &'static [u8] {
        let a = self.raw(WO_NAME, [kind, h, 0, 0, 0, 0], 0.0, 0);
        // SAFETY: the host returns the object's name, alive until the object is destroyed.
        unsafe { super::host::Str { p: a.s, n: a.n }.bytes() }
    }

    fn in_update(self, b: usize) -> bool {
        self.r(WO_BODY_IN_UPDATE, [b, 0, 0, 0, 0, 0]) != 0
    }

    fn set_in_update(self, b: usize, v: bool) {
        self.raw(WO_BODY_SET_IN_UPDATE, [b, 0, 0, 0, 0, 0], 0.0, i32::from(v));
    }

    fn temp_len(self) -> usize {
        self.r(WO_TEMP_LEN, [self.me, 0, 0, 0, 0, 0])
    }

    fn temp_at(self, i: usize) -> usize {
        self.r(WO_TEMP_AT, [self.me, i, 0, 0, 0, 0])
    }
}

fn ops(book: u64) -> Option<Ops> {
    book::world_with(book, |w| w.op.map(|op| Ops { op, me: w.me })).flatten()
}

impl Lists {
    fn of(&mut self, kind: usize) -> Option<&mut Vec<usize>> {
        match kind {
            SHAPE => Some(&mut self.shapes),
            BODY => Some(&mut self.bodies),
            CHAR => Some(&mut self.chars),
            JOINT => Some(&mut self.joints),
            CTRL => Some(&mut self.ctrls),
            ROPE => Some(&mut self.ropes),
            _ => None,
        }
    }
}

pub fn new(op: WorldOp, me: usize) -> u64 {
    let b = book::world_new();
    book::world_with(b, |w| {
        w.op = Some(op);
        w.me = me;
    });
    b
}

pub fn add(book: u64, kind: usize, h: usize) {
    book::world_with(book, |w| {
        if let Some(l) = w.lists.of(kind) {
            l.push(h);
        }
    });
}

/// `std::map::insert`: an existing key keeps its value.
pub fn add_material(book: u64, name: &[u8], h: usize) {
    book::world_with(book, |w| {
        w.lists.mats.entry(name.to_vec()).or_insert(h);
    });
}

fn snapshot(book: u64, kind: usize) -> Vec<usize> {
    book::world_with(book, |w| w.lists.of(kind).map(|l| l.clone()))
        .flatten()
        .unwrap_or_default()
}

pub fn len(book: u64, kind: usize) -> usize {
    book::world_with(book, |w| w.lists.of(kind).map_or(0, |l| l.len())).unwrap_or(0)
}

pub fn at(book: u64, kind: usize, i: usize) -> usize {
    book::world_with(book, |w| w.lists.of(kind).and_then(|l| l.get(i).copied()))
        .flatten()
        .unwrap_or(0)
}

pub fn material_len(book: u64) -> usize {
    book::world_with(book, |w| w.lists.mats.len()).unwrap_or(0)
}

/// Name bytes (valid until the next material insert) and object of the i-th material.
pub fn material_at(book: u64, i: usize) -> Option<(*const u8, usize, usize)> {
    book::world_with(book, |w| {
        w.lists
            .mats
            .iter()
            .nth(i)
            .map(|(k, v)| (k.as_ptr(), k.len(), *v))
    })
    .flatten()
}

fn erase_first(book: u64, kind: usize, h: usize) -> bool {
    book::world_with(book, |w| {
        w.lists.of(kind).is_some_and(|l| {
            l.iter().position(|&x| x == h).is_some_and(|p| {
                l.remove(p);
                true
            })
        })
    })
    .unwrap_or(false)
}

pub fn contains(book: u64, kind: usize, h: usize) -> bool {
    book::world_with(book, |w| w.lists.of(kind).is_some_and(|l| l.contains(&h))).unwrap_or(false)
}

pub fn find_by_name(book: u64, kind: usize, name: &[u8]) -> usize {
    let Some(ops) = ops(book) else { return 0 };
    snapshot(book, kind)
        .into_iter()
        .find(|&h| ops.name(kind, h) == name)
        .unwrap_or(0)
}

/// `STLFindAndDelete`: the object is deleted even when it was not in the list.
pub fn find_and_delete(book: u64, kind: usize, h: usize) {
    erase_first(book, kind, h);
    if let Some(ops) = ops(book) {
        ops.raw(WO_DELETE, [kind, h, 0, 0, 0, 0], 0.0, 0);
    }
}

pub fn destroy_shape(book: u64, shape: usize) {
    let Some(ops) = ops(book) else { return };
    ops.r(WO_SHAPE_DEC_USER, [shape, 0, 0, 0, 0, 0]);
    if ops.r(WO_SHAPE_HAS_USERS, [shape, 0, 0, 0, 0, 0]) == 0 {
        find_and_delete(book, SHAPE, shape);
    }
}

pub fn update_add(book: u64, body: usize) {
    let Some(ops) = ops(book) else { return };
    if ops.in_update(body) {
        return;
    }
    book::world_with(book, |w| w.insert(body));
    ops.set_in_update(body, true);
}

pub fn update_remove(book: u64, body: usize) {
    let Some(ops) = ops(book) else { return };
    if !ops.in_update(body) {
        return;
    }
    book::world_with(book, |w| w.remove(body));
    ops.set_in_update(body, false);
}

pub fn destroy_body(book: u64, body: usize) {
    let Some(ops) = ops(book) else { return };
    if ops.in_update(body) {
        update_remove(book, body);
    }
    if contains(book, BODY, body) {
        ops.r(WO_BODY_DESTROY, [body, 0, 0, 0, 0, 0]);
        erase_first(book, BODY, body);
    }
}

pub fn get_material(book: u64, name: &[u8]) -> usize {
    let Some(ops) = ops(book) else { return 0 };
    let Some(m) = book::world_with(book, |w| w.lists.mats.get(name).copied()).flatten() else {
        return 0;
    };
    if ops.r(WO_MAT_NEEDS_PRELOAD, [m, 0, 0, 0, 0, 0]) != 0 {
        ops.r(WO_MAT_PRELOAD, [m, 0, 0, 0, 0, 0]);
    }
    m
}

pub fn rope_from_unique_id(book: u64, id: i32) -> usize {
    let Some(ops) = ops(book) else { return 0 };
    snapshot(book, ROPE)
        .into_iter()
        .find(|&r| ops.r(WO_ROPE_UID, [r, 0, 0, 0, 0, 0]) as u32 as i32 == id)
        .unwrap_or(0)
}

/// Walks a list like a `std::list` iterator: objects appended during the walk are visited,
/// the one under the cursor may be erased through [`Cur::erase`].
struct Cur {
    kind: usize,
    i: usize,
    cur: usize,
}

impl Cur {
    fn new(kind: usize) -> Cur {
        Cur { kind, i: 0, cur: 0 }
    }

    fn next(&mut self, book: u64) -> Option<usize> {
        book::world_with(book, |w| {
            let l = w.lists.of(self.kind)?;
            if self.cur != 0 {
                if l.get(self.i) == Some(&self.cur) {
                    self.i += 1;
                } else if let Some(p) = l.iter().position(|&x| x == self.cur) {
                    self.i = p + 1;
                }
            }
            let h = *l.get(self.i)?;
            self.cur = h;
            Some(h)
        })
        .flatten()
    }

    fn erase(&self, book: u64) {
        erase_first(book, self.kind, self.cur);
    }
}

fn update_set_next(book: u64, after: Option<usize>) -> Option<usize> {
    book::world_with(book, |w| w.next_after(after)).flatten()
}

pub fn update(book: u64, dt: f32) {
    let Some(ops) = ops(book) else { return };
    let me = ops.me;
    ops.r(WO_CLEAR_CONTACTS, [me, 0, 0, 0, 0, 0]);

    let mut cur = Cur::new(CTRL);
    while let Some(c) = cur.next(book) {
        ops.raw(WO_CTRL_UPDATE, [c, 0, 0, 0, 0, 0], dt, 0);
    }
    let mut cur = Cur::new(ROPE);
    while let Some(r) = cur.next(book) {
        if ops.r(WO_ROPE_ACTIVE, [r, 0, 0, 0, 0, 0]) != 0 {
            ops.raw(WO_ROPE_BEFORE, [r, 0, 0, 0, 0, 0], dt, 0);
        }
    }
    let mut cur = Cur::new(CHAR);
    while let Some(c) = cur.next(book) {
        ops.raw(WO_CHAR_UPDATE, [c, 0, 0, 0, 0, 0], dt, 0);
    }

    let mut drop_list = Vec::new();
    let mut b = update_set_next(book, None);
    while let Some(body) = b {
        if ops.raw(WO_BODY_BEFORE, [body, 0, 0, 0, 0, 0], dt, 0).p[5] == 0 {
            drop_list.push(body);
        }
        b = update_set_next(book, Some(body));
    }
    for body in drop_list {
        update_remove(book, body);
    }

    ops.raw(WO_SIMULATE, [me, 0, 0, 0, 0, 0], dt, 0);

    let mut cur = Cur::new(JOINT);
    while let Some(j) = cur.next(book) {
        if ops.r(WO_JOINT_UPDATE, [j, 0, 0, 0, 0, 0]) == 0 {
            continue;
        }
        if ops.r(WO_JOINT_BREAKAGE, [j, 0, 0, 0, 0, 0]) != 0 {
            cur.erase(book);
            ops.raw(WO_DELETE, [JOINT, j, 0, 0, 0, 0], 0.0, 0);
        }
    }

    let mut b = update_set_next(book, None);
    while let Some(body) = b {
        ops.raw(WO_BODY_AFTER, [body, 0, 0, 0, 0, 0], dt, 0);
        b = update_set_next(book, Some(body));
    }

    let mut cur = Cur::new(ROPE);
    while let Some(r) = cur.next(book) {
        ops.raw(WO_ROPE_AFTER, [r, 0, 0, 0, 0, 0], dt, 0);
    }
}

pub fn enable_bodies_in_bv(book: u64, bv: usize) {
    let Some(ops) = ops(book) else { return };
    ops.r(WO_BODIES_IN_BV, [ops.me, bv, 0, 0, 0, 0]);
    let mut i = 0;
    while i < ops.temp_len() {
        let body = ops.temp_at(i);
        i += 1;
        let mass = ops.raw(WO_BODY_MASS, [body, 0, 0, 0, 0, 0], 0.0, 0).rf;
        if mass > 0.0 && ops.r(WO_BV_INTERSECTS, [bv, body, 0, 0, 0, 0]) != 0 {
            ops.r(WO_BODY_ENABLE, [body, 0, 0, 0, 0, 0]);
        }
    }
}

pub fn fadeout_all_loop_sounds(book: u64, speed: f32) {
    let Some(ops) = ops(book) else { return };
    for b in snapshot(book, BODY) {
        ops.raw(WO_BODY_FADE, [ops.me, b, 0, 0, 0, 0], speed, 0);
    }
    for j in snapshot(book, JOINT) {
        ops.raw(WO_JOINT_FADE, [ops.me, j, 0, 0, 0, 0], speed, 0);
    }
}

pub fn render_contacts(book: u64, low: usize, colors: usize) {
    let Some(ops) = ops(book) else { return };
    let n = ops.r(WO_CONTACTS_LEN, [ops.me, 0, 0, 0, 0, 0]);
    for i in 0..n {
        ops.r(WO_RENDER_CONTACT, [ops.me, low, i, colors, 0, 0]);
    }
}

fn delete_all(book: u64, ops: Ops, kind: usize) {
    for h in snapshot(book, kind) {
        ops.raw(WO_DELETE, [kind, h, 0, 0, 0, 0], 0.0, 0);
    }
    book::world_with(book, |w| {
        if let Some(l) = w.lists.of(kind) {
            l.clear();
        }
    });
}

pub fn destroy_all(book: u64) {
    let Some(ops) = ops(book) else { return };
    delete_all(book, ops, CHAR);
    for body in snapshot(book, BODY) {
        ops.r(WO_BODY_DESTROY, [body, 0, 0, 0, 0, 0]);
    }
    book::world_with(book, |w| {
        w.lists.bodies.clear();
        w.clear();
    });
    delete_all(book, ops, ROPE);
    delete_all(book, ops, SHAPE);
    delete_all(book, ops, JOINT);
    delete_all(book, ops, CTRL);
    let mats: Vec<usize> =
        book::world_with(book, |w| w.lists.mats.values().copied().collect()).unwrap_or_default();
    for m in mats {
        ops.raw(WO_DELETE, [6, m, 0, 0, 0, 0], 0.0, 0);
    }
    book::world_with(book, |w| w.lists.mats.clear());
}

/// `iPhysicsWorld::CheckShapeWorldCollision`.
///
/// # Safety
/// `push` is null or writable; `mtx` is a valid matrix for the call.
pub unsafe fn shape_world_collision(
    book: u64,
    push: *mut Vector3f,
    shape: usize,
    mtx: *const Matrixf,
    q: &Query,
    callback: usize,
) -> bool {
    let Some(ops) = ops(book) else { return false };
    // SAFETY: caller contract.
    if let Some(p) = unsafe { push.as_mut() } {
        *p = Vector3f::default();
    }
    let have_push = !push.is_null();
    let bv = ops.r(WO_BV_NEW, [ops.me, shape, mtx as usize, 0, 0, 0]);
    ops.r(WO_BODIES_IN_BV, [ops.me, bv, 0, 0, 0, 0]);
    let cd = ops.r(WO_CD_NEW, [0; 6]);
    let mut collide = false;
    let mut i = 0;
    while i < ops.temp_len() {
        let body = ops.temp_at(i);
        i += 1;
        let mut filter = BodyFilter {
            active: false,
            is_character: false,
            is_skip_body: false,
            mass: 0.0,
            collide_character: false,
            collide: false,
            push_strength: 0,
            flags_match: false,
            query_collide_character: false,
            query_is_character: false,
            skip_static: false,
            min_push_strength: 0,
        };
        ops.r(
            WO_QUERY_FILTER,
            [
                body,
                q as *const Query as usize,
                &mut filter as *mut BodyFilter as usize,
                0,
                0,
                0,
            ],
        );
        if !world::body_passes(&filter) {
            continue;
        }
        if ops.r(WO_BV_INTERSECTS, [bv, body, 0, 0, 0, 0]) == 0 {
            continue;
        }
        let hit = ops.r(WO_CD_COLLIDE, [cd, ops.me, shape, mtx as usize, body, 0]) != 0;
        if hit && have_push {
            if callback != 0 {
                ops.r(WO_CALLBACK, [callback, body, cd, 0, 0, 0]);
            }
            let n = ops.r(WO_CD_LEN, [cd, 0, 0, 0, 0, 0]);
            for k in 0..n {
                let a = ops.raw(WO_CD_POINT, [cd, k, 0, 0, 0, 0], 0.0, 0);
                let normal = Vector3f {
                    x: a.rv[0],
                    y: a.rv[1],
                    z: a.rv[2],
                };
                // SAFETY: `have_push` implies a non-null, writable `push`.
                if let Some(p) = unsafe { push.as_mut() } {
                    *p = world::merge_push(*p, normal, a.rf);
                }
            }
        }
        if hit {
            collide = true;
        }
    }
    ops.r(WO_CD_FREE, [cd, 0, 0, 0, 0, 0]);
    ops.r(WO_BV_FREE, [bv, 0, 0, 0, 0, 0]);
    collide
}
