//! `cWorld` entity containers and the `.map` loader's static-combine planner. Entities stay C++
//! shell objects (their classes keep live C++ subclasses) held by address in per-kind Rust-owned
//! lists with `std::list` creation/erase order; Rust runs the destroy order through the kind's
//! `Foreign` destructor thunk, and C++ iterates through cursors.

use crate::obj::{Foreign, Handle, Link, List};
use std::collections::BTreeMap;

pub const KINDS: usize = 17;
/// Pseudo-kind selecting the area map's `Foreign` (names are map keys, so only `drop` is used).
pub const AREA: usize = KINDS;
const BODY_INDEX_LIMIT: i32 = 50_000;

pub const STATIC_MESH: usize = 2;
pub const DYNAMIC_MESH: usize = 1;

macro_rules! entity_ref {
    ($($variant:ident = $kind:expr),+ $(,)?) => {
        /// A position in one kind's list; the variant is the kind (`kOhplH13*` order).
        #[derive(Clone, Copy, Debug, PartialEq, Eq)]
        pub enum EntityRef {
            $($variant(Link<usize>)),+
        }

        impl EntityRef {
            pub fn new(kind: usize, link: Link<usize>) -> Option<Self> {
                match kind {
                    $($kind => Some(Self::$variant(link)),)+
                    _ => None,
                }
            }

            pub fn kind(self) -> usize {
                match self {
                    $(Self::$variant(_) => $kind),+
                }
            }

            pub fn link(self) -> Link<usize> {
                match self {
                    $(Self::$variant(l) => l),+
                }
            }
        }
    };
}

entity_ref! {
    Light = 0, DynMesh = 1, StaticMesh = 2, Billboard = 3, LensFlare = 4, Beam = 5,
    ParticleSystem = 6, GuiSet = 7, Rope = 8, Sound = 9, StartPos = 10, FogArea = 11,
    ForceField = 12, Dummy = 13, AiContainer = 14, AStar = 15, EnvParticles = 16,
}

impl EntityRef {
    pub fn to_bits(self) -> u64 {
        self.link().to_bits()
    }

    pub fn from_bits(kind: usize, bits: u64) -> Option<Self> {
        Self::new(kind, Handle::from_bits(bits)?)
    }
}

#[derive(Default)]
pub struct Reg {
    lists: [List<usize>; KINDS],
    foreign: [Foreign; KINDS + 1],
    areas: BTreeMap<Vec<u8>, usize>,
}

impl Reg {
    fn list(&self, kind: usize) -> Option<&List<usize>> {
        self.lists.get(kind)
    }

    pub fn set_foreign(&mut self, kind: usize, f: Foreign) {
        if let Some(slot) = self.foreign.get_mut(kind) {
            *slot = f;
        }
    }

    pub fn foreign(&self, kind: usize) -> Foreign {
        self.foreign.get(kind).copied().unwrap_or_default()
    }

    pub fn add(&mut self, kind: usize, p: usize) {
        if let Some(l) = self.lists.get_mut(kind) {
            l.push_back(p);
        }
    }

    fn position(&self, kind: usize, p: usize) -> Option<Link<usize>> {
        let l = self.list(kind)?;
        let mut at = l.first();
        while let Some(link) = at {
            if l.get(link) == Some(p) {
                return Some(link);
            }
            at = l.next(link);
        }
        None
    }

    /// `STLFindAndDelete` / `std::list::erase` of the first match.
    pub fn remove_first(&mut self, kind: usize, p: usize) -> bool {
        let Some(link) = self.position(kind, p) else {
            return false;
        };
        if let Some(l) = self.lists.get_mut(kind) {
            l.erase(link);
        }
        true
    }

    pub fn remove_set(&mut self, kind: usize, set: &[usize]) {
        let Some(l) = self.lists.get_mut(kind) else {
            return;
        };
        let mut at = l.first();
        while let Some(link) = at {
            at = if l.get(link).is_some_and(|p| set.contains(&p)) {
                l.erase(link)
            } else {
                l.next(link)
            };
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

    /// Creation-order copy; `STLDeleteAll` deletes while the list is still populated.
    pub fn snapshot(&self, kind: usize) -> Vec<usize> {
        self.list(kind)
            .map_or_else(Vec::new, |l| l.iter().collect())
    }

    pub fn len(&self, kind: usize) -> usize {
        self.list(kind).map_or(0, List::len)
    }

    pub fn get(&self, kind: usize, i: usize) -> usize {
        self.list(kind).and_then(|l| l.iter().nth(i)).unwrap_or(0)
    }

    pub fn index_of(&self, kind: usize, p: usize) -> i32 {
        index(self.list(kind).and_then(|l| l.iter().position(|x| x == p)))
    }

    pub fn begin(&self, kind: usize) -> Option<EntityRef> {
        EntityRef::new(kind, self.list(kind)?.first()?)
    }

    pub fn next(&self, at: EntityRef) -> Option<EntityRef> {
        EntityRef::new(at.kind(), self.list(at.kind())?.next(at.link())?)
    }

    /// 0 for an erased or cleared position.
    pub fn resolve(&self, at: EntityRef) -> usize {
        self.list(at.kind())
            .and_then(|l| l.get(at.link()))
            .unwrap_or(0)
    }

    /// `std::list::erase` at a cursor: the position after it.
    pub fn erase(&mut self, at: EntityRef) -> Option<EntityRef> {
        let next = self.lists.get_mut(at.kind())?.erase(at.link())?;
        EntityRef::new(at.kind(), next)
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
        for p in self.list(kind).into_iter().flat_map(List::iter) {
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
            .into_iter()
            .flat_map(List::iter)
            .find(|&p| uid(p) == id)
            .unwrap_or(0)
    }

    /// Smallest name, first on ties.
    pub fn first_by_name<'a>(&self, kind: usize, mut name: impl FnMut(usize) -> &'a [u8]) -> usize {
        let mut best: Option<(usize, &[u8])> = None;
        for p in self.list(kind).into_iter().flat_map(List::iter) {
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

    pub fn area_at(&self, i: usize, key: impl FnOnce(&[u8])) -> usize {
        self.areas.iter().nth(i).map_or(0, |(k, &v)| {
            key(k);
            v
        })
    }

    /// Area map in key order (`std::map` iteration): (name, entity).
    pub fn areas(&self) -> Vec<(Vec<u8>, usize)> {
        self.areas.iter().map(|(k, &v)| (k.clone(), v)).collect()
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
