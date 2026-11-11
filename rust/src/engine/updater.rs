//! `cUpdater`: global list plus named containers, one current container. Updatables are opaque
//! addresses; the visitor performs the virtual call. A visit may re-enter the updater, so the
//! traversals take a raw pointer and hold no borrow across a visit. Lists are re-read after every
//! visit, so an updatable added mid-run is still reached, as with `std::list`.

use std::collections::BTreeMap;
use std::ops::Bound;

pub const MSG_UPDATE: i32 = 5;

#[derive(Default)]
pub struct Updater {
    globals: Vec<usize>,
    containers: BTreeMap<Vec<u8>, Vec<usize>>,
    /// `mpCurrentUpdates`: key of the active list.
    current: Option<Vec<u8>>,
    /// `msCurrentUpdates`: set before the leave messages go out.
    name: Vec<u8>,
}

#[derive(Clone, Copy, PartialEq, Debug)]
pub enum Visit<'a> {
    Message(i32, f32),
    TimedUpdate(f32),
    Leave(&'a [u8]),
    Enter(&'a [u8]),
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum AddResult {
    NoContainer,
    Added,
    Null,
}

pub type Cb<'a> = &'a mut dyn FnMut(usize, Visit<'_>);

impl Updater {
    pub fn add_container(&mut self, name: &[u8]) {
        self.containers.entry(name.to_vec()).or_default();
    }

    pub fn add_update(&mut self, container: &[u8], upd: usize) -> AddResult {
        if upd == 0 {
            return AddResult::Null;
        }
        match self.containers.get_mut(container) {
            Some(list) => {
                list.push(upd);
                AddResult::Added
            }
            None => AddResult::NoContainer,
        }
    }

    pub fn add_global(&mut self, upd: usize) {
        self.globals.push(upd);
    }

    pub fn current_name(&self) -> &[u8] {
        if self.current.is_some() {
            &self.name
        } else {
            &[]
        }
    }

    fn at(&self, list: Option<&[u8]>, i: usize) -> Option<usize> {
        match list {
            None => self.globals.get(i).copied(),
            Some(n) => self.containers.get(n)?.get(i).copied(),
        }
    }

    fn next_key(&self, after: Option<&Vec<u8>>) -> Option<Vec<u8>> {
        let lo = after.map_or(Bound::Unbounded, |k| Bound::Excluded(k.clone()));
        self.containers
            .range((lo, Bound::Unbounded))
            .next()
            .map(|(k, _)| k.clone())
    }
}

/// # Safety
/// `u` is valid; `cb` may re-enter through the same pointer.
unsafe fn at(u: *mut Updater, list: Option<&[u8]>, i: usize) -> Option<usize> {
    // SAFETY: short shared borrow, dropped before returning.
    unsafe { (*u).at(list, i) }
}

/// # Safety
/// `u` is valid; `cb` may re-enter through the same pointer.
unsafe fn current(u: *mut Updater) -> Option<Vec<u8>> {
    // SAFETY: short shared borrow, dropped before returning.
    unsafe { (*u).current.clone() }
}

/// `RunMessage`: globals, then the current container, stopping if a visit switched container.
///
/// # Safety
/// `u` is valid; `cb` may re-enter through the same pointer.
pub unsafe fn run_message(u: *mut Updater, msg: i32, x: f32, cb: Cb<'_>) {
    let visit = || {
        if msg == MSG_UPDATE {
            Visit::TimedUpdate(x)
        } else {
            Visit::Message(msg, x)
        }
    };
    let mut i = 0;
    // SAFETY: caller contract.
    while let Some(w) = unsafe { at(u, None, i) } {
        cb(w, visit());
        i += 1;
    }
    // SAFETY: caller contract.
    let Some(cur) = (unsafe { current(u) }) else {
        return;
    };
    let mut i = 0;
    // SAFETY: caller contract.
    while let Some(w) = unsafe { at(u, Some(&cur), i) } {
        cb(w, visit());
        // SAFETY: caller contract.
        if unsafe { current(u) }.as_deref() != Some(cur.as_slice()) {
            break;
        }
        i += 1;
    }
}

/// `BroadcastMessageToAll`: globals, then every container in key order.
///
/// # Safety
/// `u` is valid; `cb` may re-enter through the same pointer.
pub unsafe fn broadcast(u: *mut Updater, msg: i32, x: f32, cb: Cb<'_>) {
    let mut i = 0;
    // SAFETY: caller contract.
    while let Some(w) = unsafe { at(u, None, i) } {
        cb(w, Visit::Message(msg, x));
        i += 1;
    }
    let mut key: Option<Vec<u8>> = None;
    // SAFETY: caller contract.
    while let Some(k) = unsafe { (*u).next_key(key.as_ref()) } {
        let mut i = 0;
        // SAFETY: caller contract.
        while let Some(w) = unsafe { at(u, Some(&k), i) } {
            cb(w, Visit::Message(msg, x));
            i += 1;
        }
        key = Some(k);
    }
}

/// `SetContainer`: false when unknown; leave messages go to the old list, enter messages to the new.
///
/// # Safety
/// `u` is valid; `cb` may re-enter through the same pointer.
pub unsafe fn set_container(u: *mut Updater, name: &[u8], cb: Cb<'_>) -> bool {
    // SAFETY: caller contract.
    let (known, same, prev) = unsafe {
        (
            (*u).containers.contains_key(name),
            (*u).current.as_deref() == Some(name),
            (*u).current.clone(),
        )
    };
    if !known {
        return false;
    }
    if same {
        return true;
    }
    // SAFETY: caller contract.
    let old = unsafe { std::mem::replace(&mut (*u).name, name.to_vec()) };
    if let Some(prev) = prev {
        let mut i = 0;
        // SAFETY: caller contract.
        while let Some(w) = unsafe { at(u, Some(&prev), i) } {
            cb(w, Visit::Leave(name));
            i += 1;
        }
    }
    // SAFETY: caller contract.
    unsafe { (*u).current = Some(name.to_vec()) };
    let mut i = 0;
    // SAFETY: caller contract.
    while let Some(w) = unsafe { at(u, Some(name), i) } {
        cb(w, Visit::Enter(&old));
        i += 1;
    }
    true
}
