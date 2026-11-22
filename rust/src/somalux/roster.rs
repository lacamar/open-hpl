//! Entity roster of `cSomaLuxMap`: insertion-ordered entity list, lower-case name map, wildcard
//! cache and the destroyed list. Entities stay C++ objects; the roster holds their addresses plus
//! the Rust `Core`/`Names` boxes that carry the id and the name.

use crate::somaent::ent_core::{Core, Id};
use crate::somaent::names::Names;
use crate::string::{matches_wildcard, to_lower};
use std::collections::HashMap;

#[derive(Clone, Copy)]
struct Ent {
    ent: usize,
    core: *const Core,
    names: *const Names,
}

impl Ent {
    fn name(&self) -> &[u8] {
        // SAFETY: the entity outlives its roster entry; C++ frees the roster before the entities.
        unsafe { &*self.names }.name()
    }
    fn id(&self) -> Id {
        // SAFETY: as above.
        unsafe { &*self.core }.mID
    }
}

#[derive(Default)]
pub struct Roster {
    live: Vec<usize>,
    ents: Vec<Ent>,
    by_name: HashMap<Vec<u8>, usize>,
    wild: HashMap<Vec<u8>, Vec<usize>>,
    destroyed: Vec<Ent>,
    destroyed_ptrs: Vec<usize>,
    flushed: usize,
    scratch: Vec<usize>,
}

impl Roster {
    fn add(&mut self, e: Ent) {
        self.live.push(e.ent);
        self.by_name.insert(to_lower(e.name()), e.ent);
        self.ents.push(e);
        self.wild.clear();
    }

    fn remove(&mut self, ent: usize) -> bool {
        let Some(i) = self.live.iter().position(|&p| p == ent) else {
            return false;
        };
        let e = self.ents.remove(i);
        self.live.remove(i);
        self.wild.clear();
        let key = to_lower(e.name());
        if self.by_name.get(&key) == Some(&ent) {
            self.by_name.remove(&key);
        }
        self.destroyed_ptrs.push(ent);
        self.destroyed.push(e);
        true
    }

    fn by_name(&self, name: &[u8]) -> usize {
        if let Some(&p) = self.by_name.get(&to_lower(name)) {
            return p;
        }
        if name.contains(&b'*') {
            return self
                .ents
                .iter()
                .find(|e| matches_wildcard(name, e.name()))
                .map_or(0, |e| e.ent);
        }
        0
    }

    fn by_id(&self, id: Id) -> usize {
        let live = self.ents.iter().find(|e| e.id() == id);
        let dead = || {
            self.destroyed
                .get(self.flushed..)?
                .iter()
                .find(|e| e.id() == id)
        };
        live.or_else(dead).map_or(0, |e| e.ent)
    }

    fn children(&mut self, pat: &[u8]) -> &[usize] {
        if !pat.contains(&b'*') {
            self.scratch.clear();
            if let Some(&p) = self.by_name.get(&to_lower(pat)) {
                self.scratch.push(p);
            }
            return &self.scratch;
        }
        let ents = &self.ents;
        self.wild.entry(pat.to_vec()).or_insert_with(|| {
            ents.iter()
                .filter(|e| matches_wildcard(pat, e.name()))
                .map(|e| e.ent)
                .collect()
        })
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_so_roster_new() -> *mut Roster {
    Box::into_raw(Box::default())
}

/// # Safety
/// `r` came from `ohpl_so_roster_new` and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_so_roster_free(r: *mut Roster) {
    if !r.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(r) });
    }
}

/// # Safety
/// `r` is live; `core` and `names` are the entity's boxes and outlive the roster entry.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_so_roster_add(
    r: *mut Roster,
    ent: usize,
    core: *const Core,
    names: *const Names,
) {
    // SAFETY: caller contract.
    unsafe { &mut *r }.add(Ent { ent, core, names });
}

/// True when `ent` was live; it moves to the destroyed list.
///
/// # Safety
/// `r` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_so_roster_remove(r: *mut Roster, ent: usize) -> bool {
    // SAFETY: caller contract.
    unsafe { &mut *r }.remove(ent)
}

/// Live entity addresses; `len` receives the count. Valid until the next change.
///
/// # Safety
/// `r` is live; `len` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_so_roster_live(r: *const Roster, len: *mut usize) -> *const usize {
    // SAFETY: caller contract.
    let (r, len) = unsafe { (&*r, &mut *len) };
    *len = r.live.len();
    r.live.as_ptr()
}

/// Destroyed entity addresses, in order.
///
/// # Safety
/// `r` is live; `len` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_so_roster_destroyed(
    r: *const Roster,
    len: *mut usize,
) -> *const usize {
    // SAFETY: caller contract.
    let (r, len) = unsafe { (&*r, &mut *len) };
    *len = r.destroyed_ptrs.len();
    r.destroyed_ptrs.as_ptr()
}

/// Ids of entities destroyed so far stop resolving.
///
/// # Safety
/// `r` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_so_roster_flush(r: *mut Roster) {
    // SAFETY: caller contract.
    let r = unsafe { &mut *r };
    r.flushed = r.destroyed.len();
}

/// # Safety
/// `r` is live; `s` points at `len` readable bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_so_roster_by_name(
    r: *const Roster,
    s: *const u8,
    len: usize,
) -> usize {
    // SAFETY: caller contract.
    let (r, s) = unsafe { (&*r, std::slice::from_raw_parts(s, len)) };
    r.by_name(s)
}

/// # Safety
/// `r` is live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_so_roster_by_id(r: *const Roster, a: u8, b: i32, c: i32) -> usize {
    // SAFETY: caller contract.
    unsafe { &*r }.by_id(Id { a, b, c })
}

/// Entities named by a plain name or a wildcard (cached); valid until the next change.
///
/// # Safety
/// `r` is live; `s` points at `len` readable bytes; `n` is writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_so_roster_children(
    r: *mut Roster,
    s: *const u8,
    len: usize,
    n: *mut usize,
) -> *const usize {
    // SAFETY: caller contract.
    let (r, s, n) = unsafe { (&mut *r, std::slice::from_raw_parts(s, len), &mut *n) };
    let v = r.children(s);
    *n = v.len();
    v.as_ptr()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn lookup_wildcard_and_flush() {
        let mut core = [Box::new(Core::default()), Box::new(Core::default())];
        let mut n = [Box::new(Names::default()), Box::new(Names::default())];
        for (i, c) in core.iter_mut().enumerate() {
            c.mID = Id {
                a: 1,
                b: i as i32,
                c: 0,
            };
        }
        n[0].set_name(b"Door_A");
        n[1].set_name(b"door_b");
        let mut r = Roster::default();
        for i in 0..2 {
            r.add(Ent {
                ent: 10 + i,
                core: &*core[i],
                names: &*n[i],
            });
        }
        assert_eq!(r.by_name(b"DOOR_a"), 10);
        assert_eq!(r.by_name(b"*_b"), 11);
        assert_eq!(r.children(b"*oor_*"), &[10, 11]);
        assert!(r.remove(10));
        assert!(!r.remove(10));
        assert_eq!(r.by_name(b"door_a"), 0);
        assert_eq!(r.children(b"*oor_*"), &[11]);
        assert_eq!(r.by_id(Id { a: 1, b: 0, c: 0 }), 10);
        r.flushed = r.destroyed.len();
        assert_eq!(r.by_id(Id { a: 1, b: 0, c: 0 }), 0);
    }
}
