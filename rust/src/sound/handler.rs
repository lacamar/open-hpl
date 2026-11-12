//! `cSoundHandler` state: the ordered entry list, entry records, id counter, silent flag and the
//! per-type global volume/speed tables. The C++ shell keeps the channels, resources and callbacks.

use super::entry::EntryState;
use crate::obj::{Arena, Handle};
use std::ffi::c_void;

pub struct Rec {
    pub state: EntryState,
    pub shell: *mut c_void,
    pub id: i32,
    pub name: Vec<u8>,
}

pub type EntryHandle = Handle<Box<Rec>>;

pub struct Handler {
    arena: Arena<Box<Rec>>,
    order: Vec<EntryHandle>,
    id_count: i32,
    silent: bool,
    global_volume: [f32; 32],
    global_speed: [f32; 32],
}

/// The shell's per-entry work; `update` may re-enter the handler (callbacks start or stop sounds).
pub trait Shells {
    fn update(&mut self, shell: *mut c_void, dt: f32) -> bool;
    fn delete(&mut self, shell: *mut c_void);
}

fn type_slot(entry_type: u32) -> Option<usize> {
    (entry_type != 0).then(|| entry_type.trailing_zeros() as usize)
}

impl Default for Handler {
    fn default() -> Self {
        Self {
            arena: Arena::default(),
            order: Vec::new(),
            id_count: 0,
            silent: false,
            global_volume: [1.0; 32],
            global_speed: [1.0; 32],
        }
    }
}

impl Handler {
    pub fn silent(&self) -> bool {
        self.silent
    }

    pub fn set_silent(&mut self, silent: bool) {
        self.silent = silent;
    }

    pub fn set_globals(&mut self, volume: &[f32; 32], speed: &[f32; 32]) {
        self.global_volume = *volume;
        self.global_speed = *speed;
    }

    pub fn global_volume(&self, entry_type: u32) -> f32 {
        type_slot(entry_type)
            .and_then(|i| self.global_volume.get(i))
            .copied()
            .unwrap_or(1.0)
    }

    pub fn global_speed(&self, entry_type: u32) -> f32 {
        type_slot(entry_type)
            .and_then(|i| self.global_speed.get(i))
            .copied()
            .unwrap_or(1.0)
    }

    pub fn id_count(&self) -> i32 {
        self.id_count
    }

    /// Creates the record; it joins the list with `push_back`.
    pub fn new_entry(
        &mut self,
        shell: *mut c_void,
        name: &[u8],
        state: EntryState,
    ) -> Option<EntryHandle> {
        let rec = Box::new(Rec {
            state,
            shell,
            id: self.id_count,
            name: name.to_vec(),
        });
        self.arena.insert(rec)
    }

    pub fn state_ptr(&mut self, h: EntryHandle) -> *mut EntryState {
        self.arena
            .get_mut(h)
            .map_or(std::ptr::null_mut(), |r| &raw mut r.state)
    }

    pub fn entry_id(&self, h: EntryHandle) -> i32 {
        self.arena.get(h).map_or(-1, |r| r.id)
    }

    pub fn push_back(&mut self, h: EntryHandle) {
        self.order.push(h);
        self.id_count = self.id_count.wrapping_add(1);
    }

    /// Drops the record and its list slot.
    pub fn free_entry(&mut self, h: EntryHandle) {
        self.order.retain(|x| *x != h);
        self.arena.remove(h);
    }

    pub fn len(&self) -> usize {
        self.order.len()
    }

    pub fn is_empty(&self) -> bool {
        self.order.is_empty()
    }

    pub fn shell_at(&self, i: usize) -> *mut c_void {
        self.order
            .get(i)
            .and_then(|h| self.arena.get(*h))
            .map_or(std::ptr::null_mut(), |r| r.shell)
    }

    /// First entry (list order) with this lowercase name.
    pub fn find(&self, name: &[u8]) -> *mut c_void {
        self.order
            .iter()
            .filter_map(|h| self.arena.get(*h))
            .find(|r| r.name == name)
            .map_or(std::ptr::null_mut(), |r| r.shell)
    }

    pub fn is_valid(&self, shell: *mut c_void, id: i32) -> bool {
        self.order
            .iter()
            .filter_map(|h| self.arena.get(*h))
            .find(|r| r.shell == shell)
            .is_some_and(|r| r.id == id)
    }

    pub fn first_shell(&self) -> *mut c_void {
        self.shell_at(0)
    }
}

/// Updates the entries in list order, deleting finished ones. Entries pushed during an update are
/// visited in the same pass, as with the original `std::list` walk.
///
/// # Safety
/// `h` is live and no reference to it is held; `shells` may re-enter `h` through the same pointer.
pub unsafe fn update_all(h: *mut Handler, dt: f32, shells: &mut dyn Shells) {
    let mut i = 0;
    loop {
        // SAFETY: caller contract; the borrow ends before `shells` runs.
        let (shell, cur) = {
            let hh = unsafe { &*h };
            let Some(cur) = hh.order.get(i).copied() else {
                break;
            };
            (hh.arena.get(cur).map(|r| r.shell), cur)
        };
        let Some(shell) = shell else {
            i += 1;
            continue;
        };
        if shells.update(shell, dt) {
            i += 1;
            continue;
        }
        shells.delete(shell);
        // SAFETY: as above.
        let hh = unsafe { &mut *h };
        if hh.order.get(i) == Some(&cur) {
            hh.free_entry(cur);
        }
    }
}

/// Calls `f` on each entry whose type intersects `mask`, in list order.
///
/// # Safety
/// `h` is live and no reference to it is held; `f` may re-enter `h`.
pub unsafe fn for_each(h: *mut Handler, mask: u32, f: &mut dyn FnMut(*mut c_void)) {
    let mut i = 0;
    loop {
        // SAFETY: caller contract.
        let shell = {
            let hh = unsafe { &*h };
            let Some(cur) = hh.order.get(i) else { break };
            hh.arena
                .get(*cur)
                .filter(|r| r.state.entry_type & mask != 0)
                .map(|r| r.shell)
        };
        if let Some(shell) = shell {
            f(shell);
        }
        i += 1;
    }
}
