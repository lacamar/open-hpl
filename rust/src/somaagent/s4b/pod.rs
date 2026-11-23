//! Entity pods (`Core`, `Names`) registered at agent creation: the agent logic reads them directly.

use crate::math::{Vector3f, v3};
use crate::somaent::ent_core::Core;
use crate::somaent::names::Names;
use std::cell::RefCell;
use std::collections::BTreeMap;

thread_local! {
    static PODS: RefCell<BTreeMap<usize, (usize, usize)>> = const { RefCell::new(BTreeMap::new()) };
}

pub fn register(ent: usize, core: usize, names: usize) {
    PODS.with(|p| p.borrow_mut().insert(ent, (core, names)));
}

pub fn forget(ent: usize) {
    PODS.with(|p| p.borrow_mut().remove(&ent));
}

fn pair(ent: usize) -> Option<(usize, usize)> {
    PODS.with(|p| p.borrow().get(&ent).copied())
}

fn core(ent: usize) -> Option<*mut Core> {
    pair(ent).map(|p| p.0 as *mut Core).filter(|p| !p.is_null())
}

pub fn read<R>(ent: usize, f: impl FnOnce(&Core) -> R) -> Option<R> {
    // SAFETY: the pod outlives the registration (the entity destructor forgets it first).
    core(ent).map(|c| f(unsafe { &*c }))
}

pub fn write<R>(ent: usize, f: impl FnOnce(&mut Core) -> R) -> Option<R> {
    // SAFETY: as `read`; the game thread holds no other reference to the pod during `f`.
    core(ent).map(|c| f(unsafe { &mut *c }))
}

pub fn name(ent: usize) -> Vec<u8> {
    pair(ent)
        .filter(|p| p.1 != 0)
        // SAFETY: as `read`.
        .map(|p| unsafe { &*(p.1 as *const Names) }.name().to_vec())
        .unwrap_or_default()
}

pub fn active(ent: usize) -> bool {
    read(ent, |c| c.mbActive).unwrap_or(false)
}

pub fn global_space_anim(ent: usize) -> bool {
    read(ent, |c| c.mbGlobalSpaceAnim).unwrap_or(false)
}

pub fn scale(ent: usize) -> Vector3f {
    read(ent, |c| c.mvScale).unwrap_or_default()
}

pub fn current_anim(ent: usize) -> i32 {
    read(ent, |c| c.mlCurrentAnim).unwrap_or(-1)
}

/// Translation, `m[0][2]` and `m[2][2]` of the load matrix.
pub fn on_load(ent: usize) -> (Vector3f, f32, f32) {
    read(ent, |c| {
        let m = &c.m_mtxOnLoad.m;
        (v3(m[0][3], m[1][3], m[2][3]), m[0][2], m[2][2])
    })
    .unwrap_or_default()
}

/// `which` 0 is the door flag, 1 the closed-door flag; `set` stores `value` first.
pub fn door_flag(ent: usize, which: i32, set: Option<bool>) -> bool {
    write(ent, |c| {
        let f = if which == 0 {
            &mut c.mbIsDoor
        } else {
            &mut c.mbIsClosedDoor
        };
        if let Some(v) = set {
            *f = v;
        }
        *f
    })
    .unwrap_or(false)
}

#[cfg(test)]
pub fn test_pod(ent: usize) {
    let core = Box::into_raw(Box::new(Core::default())) as usize;
    let mut n = Names::default();
    n.set_name(b"ag");
    register(ent, core, Box::into_raw(Box::new(n)) as usize);
}
