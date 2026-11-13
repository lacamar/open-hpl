//! `cWorld` entity registry and the `.map` loader's static-combine planner. Entities stay C++
//! objects and cross as raw addresses; lists keep `std::list` push_back/erase order.

use std::collections::BTreeMap;

pub const KINDS: usize = 17;
const BODY_INDEX_LIMIT: i32 = 50_000;

pub const STATIC_MESH: usize = 2;
pub const DYNAMIC_MESH: usize = 1;

#[derive(Default)]
pub struct Reg {
    lists: [Vec<usize>; KINDS],
    areas: BTreeMap<Vec<u8>, usize>,
}

impl Reg {
    fn list(&self, kind: usize) -> &[usize] {
        self.lists.get(kind).map_or(&[], Vec::as_slice)
    }

    pub fn add(&mut self, kind: usize, p: usize) {
        if let Some(l) = self.lists.get_mut(kind) {
            l.push(p);
        }
    }

    /// `STLFindAndDelete` / `std::list::erase` of the first match.
    pub fn remove_first(&mut self, kind: usize, p: usize) -> bool {
        let Some(l) = self.lists.get_mut(kind) else {
            return false;
        };
        match l.iter().position(|&x| x == p) {
            Some(i) => {
                l.remove(i);
                true
            }
            None => false,
        }
    }

    pub fn remove_set(&mut self, kind: usize, set: &[usize]) {
        if let Some(l) = self.lists.get_mut(kind) {
            l.retain(|x| !set.contains(x));
        }
    }

    /// `MakeMeshEntityDynamic`: first static match erased, then appended to the dynamic list
    /// whether or not it was found.
    pub fn make_dynamic(&mut self, p: usize) {
        self.remove_first(STATIC_MESH, p);
        self.add(DYNAMIC_MESH, p);
    }

    pub fn clear(&mut self, kind: usize) {
        if let Some(l) = self.lists.get_mut(kind) {
            l.clear();
        }
    }

    pub fn len(&self, kind: usize) -> usize {
        self.list(kind).len()
    }

    pub fn get(&self, kind: usize, i: usize) -> usize {
        self.list(kind).get(i).copied().unwrap_or(0)
    }

    pub fn index_of(&self, kind: usize, p: usize) -> i32 {
        index(self.list(kind).iter().position(|&x| x == p))
    }

    /// First match, or the last one with `last` (`CreateAINodeContainer`). 0 when absent.
    pub fn find_name<'a>(
        &self,
        kind: usize,
        key: &[u8],
        last: bool,
        mut name: impl FnMut(usize) -> &'a [u8],
    ) -> usize {
        let mut found = 0;
        for &p in self.list(kind) {
            if name(p) == key {
                found = p;
                if !last {
                    break;
                }
            }
        }
        found
    }

    pub fn find_id(&self, kind: usize, id: i32, mut uid: impl FnMut(usize) -> i32) -> usize {
        self.list(kind)
            .iter()
            .copied()
            .find(|&p| uid(p) == id)
            .unwrap_or(0)
    }

    /// Smallest name, first on ties.
    pub fn first_by_name<'a>(&self, kind: usize, mut name: impl FnMut(usize) -> &'a [u8]) -> usize {
        let mut best: Option<(usize, &[u8])> = None;
        for &p in self.list(kind) {
            let n = name(p);
            if best.is_none_or(|(_, b)| n < b) {
                best = Some((p, n));
            }
        }
        best.map_or(0, |(p, _)| p)
    }

    /// `std::map::insert`: an existing key keeps its value.
    pub fn area_insert(&mut self, name: &[u8], p: usize) -> bool {
        match self.areas.entry(name.to_vec()) {
            std::collections::btree_map::Entry::Vacant(v) => {
                v.insert(p);
                true
            }
            std::collections::btree_map::Entry::Occupied(_) => false,
        }
    }

    pub fn area_get(&self, name: &[u8]) -> usize {
        self.areas.get(name).copied().unwrap_or(0)
    }

    pub fn area_clear(&mut self) {
        self.areas.clear();
    }
}

fn index(i: Option<usize>) -> i32 {
    i.and_then(|i| i32::try_from(i).ok()).unwrap_or(-1)
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct MeshItem {
    pub material: usize,
    pub shadow: bool,
    pub combine: bool,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct BodyItem {
    pub phys_material: usize,
    pub shadow: bool,
    pub char_collider: bool,
    pub collides: bool,
    pub index_num: i32,
}

/// Inclusive `[first, last]` runs `CombineAndCreateMeshesAndPhysics` combines, over objects
/// already sorted.
pub fn plan_meshes(items: &[MeshItem]) -> Vec<(i32, i32)> {
    let mut out = Vec::new();
    let mut first = 0usize;
    for (i, it) in items.iter().enumerate() {
        let split = match items.get(i + 1) {
            Some(next) => next.material != it.material || next.shadow != it.shadow || !next.combine,
            None => true,
        };
        if split {
            out.push((to_i32(first), to_i32(i)));
            first = i + 1;
        }
    }
    out
}

/// Body runs. `mesh_count` is `vMeshObjects.size()`, which the original's "last element" test
/// uses instead of the body vector's own size (equal whenever bodies are created).
pub fn plan_bodies(items: &[BodyItem], mesh_count: usize) -> Vec<(i32, i32)> {
    let mut out = Vec::new();
    let mut first = 0usize;
    let mut count = 0i32;
    for (i, it) in items.iter().enumerate() {
        if it.collides {
            count = count.wrapping_add(it.index_num);
        }
        let split = if i + 1 < mesh_count {
            match items.get(i + 1) {
                Some(next) => {
                    count > BODY_INDEX_LIMIT
                        || next.char_collider != it.char_collider
                        || next.phys_material != it.phys_material
                        || next.shadow != it.shadow
                }
                None => true,
            }
        } else {
            true
        };
        if split {
            out.push((to_i32(first), to_i32(i)));
            first = i + 1;
            if i + 1 < mesh_count {
                count = 0;
            }
        }
    }
    out
}

fn to_i32(v: usize) -> i32 {
    i32::try_from(v).unwrap_or(i32::MAX)
}
