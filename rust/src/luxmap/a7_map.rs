//! `cLuxMap` state under `OHPL_IMPL=rust`: per-map arena owning the entity lists and name/id
//! multimaps, the enemy/sticky/to-destroy lists, event timers, use-item callbacks, dissolve
//! entities, lamp connections and start/pos nodes. C++ objects (entities, nodes, connections)
//! stay C++ shells held by address; Rust owns order and lifecycle and runs their destructors
//! through `Foreign::drop` with no borrow of the map held.

use super::map;
use crate::obj::{Foreign, Link, List};
use crate::string;
use std::collections::BTreeMap;

pub const K_ENTITY: usize = 0;
pub const K_DISSOLVE: usize = 1;
pub const K_LAMP: usize = 2;
pub const K_START: usize = 3;
pub const K_POS: usize = 4;
pub const K_USEITEM: usize = 5;
const KINDS: usize = 6;

pub const W_ENTITIES: u32 = 0;
pub const W_ENEMIES: u32 = 1;
pub const W_DOOMED: u32 = 2;
pub const W_STICKY: u32 = 3;

/// An entity as `iLuxEntity*` (`p`, what the main list holds) and as its own static type
/// (`typed`, what the enemy and sticky lists hold; the address can differ).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Entity {
    pub p: usize,
    pub typed: usize,
}

#[derive(Clone, Debug, PartialEq)]
pub struct Timer {
    pub name: Vec<u8>,
    pub func: Vec<u8>,
    pub count: f32,
    pub destroy_me: bool,
}

#[derive(Clone, Debug, PartialEq)]
pub struct UseItem {
    pub ptr: usize,
    pub name: Vec<u8>,
    pub item: Vec<u8>,
    pub entity: Vec<u8>,
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Dissolve {
    pub obj: usize,
    pub speed: f32,
    pub alpha: f32,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Lamp {
    pub conn: usize,
    pub light: usize,
}

/// A multimap: key order, insertion order within a key (what `std::multimap` iterates and what
/// `find` returns first).
#[derive(Default)]
struct Multi<K: Ord> {
    m: BTreeMap<K, Vec<usize>>,
}

impl<K: Ord> Multi<K> {
    fn insert(&mut self, k: K, v: usize) {
        self.m.entry(k).or_default().push(v);
    }

    fn first_of(&self, k: &K) -> Option<usize> {
        self.m.get(k).and_then(|v| v.first().copied())
    }

    fn first(&self) -> Option<usize> {
        self.m.values().next().and_then(|v| v.first().copied())
    }

    fn values(&self) -> impl Iterator<Item = usize> + '_ {
        self.m.values().flatten().copied()
    }

    fn clear(&mut self) {
        self.m.clear();
    }
}

impl<K: Ord + Clone> Multi<K> {
    fn erase_value(&mut self, p: usize) {
        let hit = self
            .m
            .iter()
            .find_map(|(k, v)| v.iter().position(|&x| x == p).map(|i| (k.clone(), i)));
        if let Some((k, i)) = hit
            && let Some(v) = self.m.get_mut(&k)
        {
            v.remove(i);
            if v.is_empty() {
                self.m.remove(&k);
            }
        }
    }
}

#[derive(Default)]
pub struct A7Map {
    ents: List<usize>,
    by_name: Multi<Vec<u8>>,
    by_id: Multi<i32>,
    enemies: List<usize>,
    stickies: List<usize>,
    doomed: List<usize>,
    latest: usize,
    timers: Vec<Timer>,
    updating_timers: bool,
    deleting_all: bool,
    commentary_icons: bool,
    use_items: Vec<UseItem>,
    dissolve: Vec<Dissolve>,
    lamps: Vec<Lamp>,
    starts: Multi<Vec<u8>>,
    start_order: Vec<usize>,
    pos_nodes: Multi<Vec<u8>>,
    foreign: [Foreign; KINDS],
}

impl A7Map {
    pub fn set_foreign(&mut self, kind: usize, f: Foreign) {
        if let Some(slot) = self.foreign.get_mut(kind) {
            *slot = f;
        }
    }

    pub fn foreign(&self, kind: usize) -> Foreign {
        self.foreign.get(kind).copied().unwrap_or_default()
    }

    fn list(&self, which: u32) -> &List<usize> {
        match which {
            W_ENEMIES => &self.enemies,
            W_DOOMED => &self.doomed,
            W_STICKY => &self.stickies,
            _ => &self.ents,
        }
    }

    fn list_mut(&mut self, which: u32) -> &mut List<usize> {
        match which {
            W_ENEMIES => &mut self.enemies,
            W_DOOMED => &mut self.doomed,
            W_STICKY => &mut self.stickies,
            _ => &mut self.ents,
        }
    }

    // ---- entities

    /// `AddEntity`.
    pub fn add_entity(
        &mut self,
        e: Entity,
        name: &[u8],
        id: i32,
        is_enemy: bool,
        is_area: bool,
        is_sticky: bool,
    ) {
        let Entity { p, typed } = e;
        self.by_name.insert(string::to_lower(name), p);
        self.by_id.insert(id, p);
        self.ents.push_back(p);
        self.latest = p;
        let lists = map::entity_lists(is_enemy, is_area, is_sticky);
        if lists & map::LIST_ENEMY != 0 {
            self.enemies.push_back(typed);
        } else if lists & map::LIST_STICKY != 0 {
            self.stickies.push_back(typed);
        }
    }

    /// A plain address list (`cLuxMapHandler`'s maps): no name or id lookups.
    pub fn list_add(&mut self, p: usize) {
        self.ents.push_back(p);
    }

    /// `STLFindAndDelete` unlink half.
    pub fn list_remove(&mut self, p: usize) {
        find_remove(&mut self.ents, p);
    }

    pub fn find_name(&self, name: &[u8]) -> usize {
        self.by_name.first_of(&string::to_lower(name)).unwrap_or(0)
    }

    pub fn find_id(&self, id: i32) -> usize {
        self.by_id.first_of(&id).unwrap_or(0)
    }

    pub fn exists(&self, p: usize) -> bool {
        self.ents.iter().any(|x| x == p)
    }

    pub fn latest(&self) -> usize {
        self.latest
    }

    pub fn reset_latest(&mut self) {
        self.latest = 0;
    }

    pub fn entity_num(&self) -> usize {
        self.ents.len()
    }

    /// `GetFreeEntityID`: the id multimap flattened in key order, duplicates kept.
    pub fn free_entity_id(&self) -> i32 {
        let mut ids = Vec::new();
        for (k, v) in &self.by_id.m {
            ids.extend(std::iter::repeat_n(*k, v.len()));
        }
        map::free_entity_id(&ids)
    }

    pub fn doom_push(&mut self, p: usize) {
        self.doomed.push_back(p);
    }

    /// The unlink half of `UpdateToBeDesotroyedEntities` for one entity.
    pub fn unlink_entity(&mut self, e: Entity, is_enemy: bool, is_area: bool, is_sticky: bool) {
        let Entity { p, typed } = e;
        find_remove(&mut self.ents, p);
        self.by_name.erase_value(p);
        self.by_id.erase_value(p);
        let lists = map::entity_lists(is_enemy, is_area, is_sticky);
        if lists & map::LIST_ENEMY != 0 {
            find_remove(&mut self.enemies, typed);
        } else if lists & map::LIST_STICKY != 0 {
            find_remove(&mut self.stickies, typed);
        }
    }

    pub fn cursor_begin(&self, which: u32) -> u64 {
        self.list(which).first().map_or(0, |l| l.to_bits())
    }

    pub fn cursor_get(&self, which: u32, cur: u64) -> usize {
        link(cur).and_then(|l| self.list(which).get(l)).unwrap_or(0)
    }

    pub fn cursor_next(&self, which: u32, cur: u64) -> u64 {
        link(cur)
            .and_then(|l| self.list(which).next(l))
            .map_or(0, |l| l.to_bits())
    }

    pub fn list_clear(&mut self, which: u32) {
        self.list_mut(which).clear();
    }

    pub fn list_snapshot(&self, which: u32) -> Vec<usize> {
        self.list(which).iter().collect()
    }

    pub fn list_len(&self, which: u32) -> usize {
        self.list(which).len()
    }

    /// Maps and side lists cleared after `DestroyAllEntities` deleted the entities.
    pub fn after_destroy_all(&mut self) {
        self.ents.clear();
        self.by_id.clear();
        self.by_name.clear();
        self.doomed.clear();
        self.latest = 0;
        self.enemies.clear();
        self.stickies.clear();
        self.commentary_icons = false;
    }

    pub fn set_deleting_all(&mut self, v: bool) {
        self.deleting_all = v;
    }

    pub fn deleting_all(&self) -> bool {
        self.deleting_all
    }

    pub fn commentary_icons(&self) -> bool {
        self.commentary_icons
    }

    /// `UpdateCheckCommentaryIconActive` decision; `None` when nothing changes.
    pub fn commentary_sync(&mut self, show: bool) -> Option<bool> {
        let new = map::commentary_sync(self.commentary_icons, show)?;
        self.commentary_icons = new;
        Some(new)
    }

    // ---- timers

    pub fn timer_add(&mut self, name: &[u8], time: f32, func: &[u8]) {
        self.timers.push(Timer {
            name: name.to_vec(),
            func: func.to_vec(),
            count: map::timer_count(time),
            destroy_me: false,
        });
    }

    pub fn timer_push_raw(&mut self, t: Timer) {
        self.timers.push(t);
    }

    pub fn timers_clear(&mut self) {
        self.timers.clear();
    }

    pub fn timer_len(&self) -> usize {
        self.timers.len()
    }

    pub fn timer_at(&self, i: usize) -> Option<&Timer> {
        self.timers.get(i)
    }

    pub fn timer_get(&self, name: &[u8]) -> Option<f32> {
        self.timers.iter().find(|t| t.name == name).map(|t| t.count)
    }

    pub fn timer_remove(&mut self, name: &[u8]) {
        let updating = map::timer_remove_marks(self.updating_timers);
        if updating {
            for t in self.timers.iter_mut().filter(|t| t.name == name) {
                t.destroy_me = true;
            }
        } else {
            self.timers.retain(|t| t.name != name);
        }
    }

    pub fn timers_begin(&mut self, step: f32) {
        self.updating_timers = true;
        let mut counts: Vec<f32> = self.timers.iter().map(|t| t.count).collect();
        map::timers_step(&mut counts, step);
        for (t, c) in self.timers.iter_mut().zip(counts) {
            t.count = c;
        }
    }

    /// Advances `cursor` to the next due timer; its name and function when there is one.
    pub fn timer_poll(&self, cursor: &mut usize) -> Option<(Vec<u8>, Vec<u8>)> {
        while let Some(t) = self.timers.get(*cursor) {
            if map::timer_due(t.count, t.destroy_me) {
                return Some((t.name.clone(), t.func.clone()));
            }
            *cursor += 1;
        }
        None
    }

    /// Erases the timer the cursor stopped on (the script has run).
    pub fn timer_finish(&mut self, cursor: usize) {
        if cursor < self.timers.len() {
            self.timers.remove(cursor);
        }
    }

    pub fn timers_end(&mut self) {
        self.timers.retain(|t| !t.destroy_me);
        self.updating_timers = false;
    }

    // ---- use-item callbacks

    pub fn use_item_add(&mut self, u: UseItem) {
        self.use_items.push(u);
    }

    pub fn use_item_len(&self) -> usize {
        self.use_items.len()
    }

    pub fn use_item_at(&self, i: usize) -> Option<&UseItem> {
        self.use_items.get(i)
    }

    /// `RemoveUseItemCallback(name)`: the removed object, or `None` when no entry has the name.
    pub fn use_item_remove_name(&mut self, name: &[u8]) -> Option<usize> {
        let i = self.use_items.iter().position(|u| u.name == name)?;
        Some(self.use_items.remove(i).ptr)
    }

    /// `RemoveUseItemCallback(ptr, name)`: `Some(ptr)` when the entry was unlinked.
    pub fn use_item_remove_ptr(&mut self, ptr: usize, name: &[u8]) -> Option<usize> {
        let i = self.use_items.iter().position(|u| u.ptr == ptr)?;
        let ok = map::callback_remove_ok(name, &self.use_items.get(i)?.name);
        ok.then(|| self.use_items.remove(i).ptr)
    }

    pub fn use_item_find(&self, item: &[u8], entity: &[u8]) -> usize {
        self.use_items
            .iter()
            .find(|u| map::use_item_matches(&u.item, &u.entity, item, entity))
            .map_or(0, |u| u.ptr)
    }

    pub fn use_items_take(&mut self) -> Vec<usize> {
        self.use_items.drain(..).map(|u| u.ptr).collect()
    }

    // ---- dissolve entities

    pub fn dissolve_add(&mut self, obj: usize, time: f32) {
        self.dissolve.push(Dissolve {
            obj,
            speed: map::dissolve_speed(time),
            alpha: 1.0,
        });
    }

    /// One `UpdateDissolveEntities` pass: `(obj, alpha, remove)` per entity in order. Removed
    /// entries are unlinked here; the caller applies the alpha then deletes `obj`.
    pub fn dissolve_step(&mut self, step: f32) -> Vec<(usize, f32, bool)> {
        let mut out = Vec::with_capacity(self.dissolve.len());
        for d in &mut self.dissolve {
            let (a, remove) = map::dissolve_step(d.alpha, d.speed, step);
            d.alpha = a;
            out.push((d.obj, a, remove));
        }
        let mut gone = out.iter().map(|o| o.2);
        self.dissolve.retain(|_| !gone.next().unwrap_or(false));
        out
    }

    pub fn dissolve_take(&mut self) -> Vec<usize> {
        self.dissolve.drain(..).map(|d| d.obj).collect()
    }

    // ---- lamp light connections

    pub fn lamp_find(&self, light: usize) -> usize {
        self.lamps
            .iter()
            .find(|l| l.light == light)
            .map_or(0, |l| l.conn)
    }

    pub fn lamp_add(&mut self, conn: usize, light: usize) {
        self.lamps.push(Lamp { conn, light });
    }

    pub fn lamp_snapshot(&self) -> Vec<usize> {
        self.lamps.iter().map(|l| l.conn).collect()
    }

    pub fn lamp_take(&mut self) -> Vec<usize> {
        self.lamps.drain(..).map(|l| l.conn).collect()
    }

    // ---- start / pos nodes

    pub fn start_add(&mut self, p: usize, name: &[u8]) {
        self.starts.insert(name.to_vec(), p);
        self.start_order.push(p);
    }

    pub fn start_find(&self, name: &[u8]) -> usize {
        self.starts.first_of(&name.to_vec()).unwrap_or(0)
    }

    pub fn start_first(&self) -> usize {
        self.starts.first().unwrap_or(0)
    }

    pub fn start_num(&self) -> usize {
        self.start_order.len()
    }

    pub fn start_at(&self, i: usize) -> usize {
        self.start_order.get(i).copied().unwrap_or(0)
    }

    pub fn start_take(&mut self) -> Vec<usize> {
        let v = self.starts.values().collect();
        self.starts.clear();
        v
    }

    pub fn pos_add(&mut self, p: usize, name: &[u8]) {
        self.pos_nodes.insert(name.to_vec(), p);
    }

    pub fn pos_find(&self, name: &[u8]) -> usize {
        self.pos_nodes.first_of(&name.to_vec()).unwrap_or(0)
    }

    pub fn pos_take(&mut self) -> Vec<usize> {
        let v = self.pos_nodes.values().collect();
        self.pos_nodes.clear();
        v
    }
}

fn link(cur: u64) -> Option<Link<usize>> {
    crate::obj::Handle::from_bits(cur)
}

/// `STLFindAndRemove` on a list.
fn find_remove(l: &mut List<usize>, p: usize) {
    let mut at = l.first();
    while let Some(cur) = at {
        if l.get(cur) == Some(p) {
            l.erase(cur);
            return;
        }
        at = l.next(cur);
    }
}
