//! Save-object and save-data registries (`std::multimap<int, T*>` ordered by key, then insertion)
//! and the save-object id counter. Objects are opaque addresses; SerializeClass lives in `system`.

use std::collections::BTreeMap;
use std::ops::Bound;

#[derive(Default)]
pub struct Registry {
    map: BTreeMap<(i32, u64), usize>,
    seq: u64,
}

/// Position of a live iterator: the last entry handed out.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Cursor {
    pub key: i32,
    pub seq: u64,
    pub started: bool,
}

impl Registry {
    pub fn add(&mut self, key: i32, obj: usize) {
        self.map.insert((key, self.seq), obj);
        self.seq = self.seq.wrapping_add(1);
    }

    /// First object stored under `key`.
    pub fn get(&self, key: i32) -> Option<usize> {
        self.map
            .range((key, 0)..=(key, u64::MAX))
            .next()
            .map(|(_, v)| *v)
    }

    pub fn len(&self) -> usize {
        self.map.len()
    }

    pub fn is_empty(&self) -> bool {
        self.map.is_empty()
    }

    pub fn clear(&mut self) {
        self.map.clear();
    }

    fn after(&self, c: &Cursor) -> Option<((i32, u64), usize)> {
        let lo = if c.started {
            Bound::Excluded((c.key, c.seq))
        } else {
            Bound::Unbounded
        };
        self.map
            .range((lo, Bound::Unbounded))
            .next()
            .map(|(k, v)| (*k, *v))
    }

    pub fn has_next(&self, c: &Cursor) -> bool {
        self.after(c).is_some()
    }

    pub fn peek(&self, c: &Cursor) -> Option<usize> {
        self.after(c).map(|(_, v)| v)
    }

    pub fn next(&self, c: &mut Cursor) -> Option<usize> {
        let ((key, seq), v) = self.after(c)?;
        *c = Cursor {
            key,
            seq,
            started: true,
        };
        Some(v)
    }
}

/// `iSaveObject::iSaveObject`: hand out the counter, wrapping to 0 when it turns negative.
pub fn next_id(counter: &mut i32) -> i32 {
    let id = *counter;
    *counter = counter.wrapping_add(1);
    if *counter < 0 {
        *counter = 0;
    }
    id
}

/// `SetUpAll`: visits every object in order, `visit` returning the id read before its setup;
/// returns the largest id seen (at least 0). Objects added by a visit are visited.
///
/// # Safety
/// `reg` is valid; `visit` may re-enter the registry through `reg`.
pub unsafe fn set_up_all(reg: *mut Registry, visit: &mut dyn FnMut(usize) -> i32) -> i32 {
    let mut max = 0;
    let mut c = Cursor::default();
    // SAFETY: the borrow ends before `visit` runs.
    while let Some(obj) = unsafe { (*reg).next(&mut c) } {
        max = max.max(visit(obj));
    }
    max
}
