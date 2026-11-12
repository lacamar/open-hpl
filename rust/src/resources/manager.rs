//! `iResourceManager` state (H3): resource set keyed by full-path hash, eviction candidates, load-tab accounting.
//! Resources and their metas stay C++ objects and are opaque pointers here.

use crate::resources::{MapEntry, ResMap, ResMeta, eviction_order};
use crate::string;
use std::collections::BTreeMap;
use std::ffi::c_void;
use std::sync::{Mutex, MutexGuard, PoisonError};

struct Info {
    path: Vec<u32>,
    meta: *const ResMeta,
}

#[derive(Default)]
pub struct Manager {
    map: ResMap,
    info: BTreeMap<usize, Info>,
}

impl Manager {
    pub fn len(&self) -> usize {
        self.map.len()
    }

    pub fn is_empty(&self) -> bool {
        self.map.is_empty()
    }

    pub fn at(&mut self, i: usize) -> *mut MapEntry {
        self.map.at(i)
    }

    pub fn map_mut(&mut self) -> &mut ResMap {
        &mut self.map
    }

    /// # Safety
    /// `meta` stays valid until `remove(res)`.
    pub unsafe fn add(&mut self, res: *mut c_void, path: &[u32], meta: *const ResMeta) {
        self.map.insert(string::hash_wide(path), res);
        self.info.insert(
            res as usize,
            Info {
                path: path.to_vec(),
                meta,
            },
        );
    }

    pub fn remove(&mut self, res: *mut c_void) -> bool {
        let Some(info) = self.info.get(&(res as usize)) else {
            return false;
        };
        let removed = self.map.remove(string::hash_wide(&info.path), res);
        if removed {
            self.info.remove(&(res as usize));
        }
        removed
    }

    pub fn get(&self, path: &[u32]) -> *mut c_void {
        self.map.find(string::hash_wide(path), |r| {
            self.info.get(&(r as usize)).is_some_and(|i| i.path == path)
        })
    }

    /// Resources past `keep` in eviction order (most users first, then newest); empty when within budget.
    pub fn eviction_tail(&self, keep: i64) -> Vec<*mut c_void> {
        let keep = usize::try_from(keep).unwrap_or(0);
        if self.map.len() <= keep {
            return Vec::new();
        }
        let order = eviction_order(&self.map, |r| {
            self.info
                .get(&(r as usize))
                // SAFETY: `add` contract.
                .map_or((0, 0), |i| unsafe {
                    ((*i.meta).user_count, (*i.meta).time)
                })
        });
        order.into_iter().skip(keep).collect()
    }

    pub fn first(&mut self) -> *mut c_void {
        let e = self.map.at(0);
        if e.is_null() {
            std::ptr::null_mut()
        } else {
            // SAFETY: `at` returns a live entry or null.
            unsafe { (*e).second }
        }
    }
}

#[derive(Default)]
struct Tabs {
    count: i32,
    outer_start: u64,
    outer_name: Vec<u8>,
}

static TABS: Mutex<Tabs> = Mutex::new(Tabs {
    count: 0,
    outer_start: 0,
    outer_name: Vec::new(),
});

fn tabs() -> MutexGuard<'static, Tabs> {
    TABS.lock().unwrap_or_else(PoisonError::into_inner)
}

pub fn begin_load(name: &[u8], now: u64) {
    let mut t = tabs();
    if t.count == 0 {
        t.outer_start = now;
        t.outer_name = name.to_vec();
    }
    t.count += 1;
}

pub struct Outer {
    pub name: Vec<u8>,
    pub elapsed: u64,
}

/// `Some` when the outermost load ends: C++ records the load time and fires the tick callback.
pub fn end_load(now: u64) -> Option<Outer> {
    let mut t = tabs();
    t.count -= 1;
    (t.count == 0).then(|| Outer {
        name: t.outer_name.clone(),
        elapsed: now.wrapping_sub(t.outer_start),
    })
}

pub fn tab_count() -> i32 {
    tabs().count
}

#[cfg(test)]
pub static TEST_LOCK: Mutex<()> = Mutex::new(());

#[cfg(test)]
pub fn reset_tabs() {
    *tabs() = Tabs::default();
}

#[cfg(test)]
mod tests {
    use super::*;

    fn w(s: &str) -> Vec<u32> {
        s.chars().map(u32::from).collect()
    }

    #[test]
    fn lookup_remove_and_eviction() {
        let mut metas = [ResMeta::new(0, 1), ResMeta::new(0, 2), ResMeta::new(0, 3)];
        metas[0].user_count = 1;
        let ptrs: Vec<*mut c_void> = (1..=3usize).map(|i| i as *mut c_void).collect();
        let paths = [w("a/x.png"), w("b/x.png"), w("a/x.png")];
        let mut m = Manager::default();
        for ((p, path), meta) in ptrs.iter().zip(&paths).zip(&metas) {
            // SAFETY: metas outlive the manager.
            unsafe { m.add(*p, path, meta) };
        }
        assert_eq!(m.len(), 3);
        assert_eq!(m.get(&w("b/x.png")), ptrs[1]);
        assert_eq!(m.get(&w("a/x.png")), ptrs[0]);
        assert!(m.get(&w("zz")).is_null());
        assert!(m.eviction_tail(3).is_empty());
        assert_eq!(m.eviction_tail(1), vec![ptrs[2], ptrs[1]]);
        assert!(m.remove(ptrs[0]));
        assert!(!m.remove(ptrs[0]));
        assert_eq!(m.get(&w("a/x.png")), ptrs[2]);
        assert!(!m.first().is_null());
    }

    #[test]
    fn load_tabs_report_outermost_only() {
        let _g = TEST_LOCK.lock().unwrap_or_else(PoisonError::into_inner);
        reset_tabs();
        begin_load(b"outer", 10);
        begin_load(b"inner", 20);
        assert_eq!(tab_count(), 2);
        assert!(end_load(30).is_none());
        let o = end_load(50);
        assert_eq!(
            o.map(|o| (o.name, o.elapsed)),
            Some((b"outer".to_vec(), 40))
        );
    }
}
