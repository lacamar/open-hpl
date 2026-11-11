//! Order of `cGuiRenderObjectCompare`, the `std::multiset` comparator: z ascending, then clip
//! region, material and texture addresses descending, equal keys in insertion order
//! (`multiset::insert` places a new element after its equals).
//!
//! The (clip, material, texture) triples are ranked through a small dictionary so each object
//! gets a 64-bit key, and a stable LSD radix sort replaces the tree. The order is total: a NaN z
//! sorts after every number, where the C++ comparator is not a strict weak ordering.

use std::collections::HashMap;
use std::hash::BuildHasherDefault;

pub struct Key {
    pub z: f32,
    pub clip: usize,
    pub material: usize,
    pub texture: usize,
}

/// Lists up to this long are insertion sorted.
const SMALL: usize = 48;

/// Multiplicative hash of the three address words.
#[derive(Default)]
struct FxHasher(u64);

impl std::hash::Hasher for FxHasher {
    fn finish(&self) -> u64 {
        self.0
    }

    fn write(&mut self, bytes: &[u8]) {
        for &b in bytes {
            self.write_u64(u64::from(b));
        }
    }

    fn write_usize(&mut self, n: usize) {
        self.write_u64(n as u64);
    }

    fn write_u64(&mut self, n: u64) {
        self.0 = (self.0.rotate_left(5) ^ n).wrapping_mul(0x517c_c1b7_2722_0a95);
    }
}

#[derive(Clone, Copy, PartialEq, Eq, Hash)]
struct Triple {
    clip: usize,
    material: usize,
    texture: usize,
}

#[derive(Default)]
pub struct Sorter {
    ids: HashMap<Triple, u32, BuildHasherDefault<FxHasher>>,
    distinct: Vec<Triple>,
    object_ids: Vec<u32>,
    by_rank: Vec<u32>,
    rank_of_id: Vec<u32>,
    counts: Vec<usize>,
    entries: Vec<(u64, u32)>,
    spare: Vec<(u64, u32)>,
}

/// Monotone map of f32 to u32; `-0.0` and `0.0` share a key as they compare equal.
fn z_key(z: f32) -> u32 {
    let bits = (z + 0.0).to_bits();
    if bits >> 31 == 1 {
        !bits
    } else {
        bits | 1 << 31
    }
}

impl Sorter {
    /// Object indices in render order.
    pub fn order(&mut self, keys: &[Key]) -> impl Iterator<Item = usize> {
        self.rank(keys);
        self.radix_sort();
        self.entries.iter().map(|&(_, i)| i as usize)
    }

    fn rank(&mut self, keys: &[Key]) {
        self.ids.clear();
        self.distinct.clear();
        self.object_ids.clear();
        let mut last: Option<(Triple, u32)> = None;
        for k in keys {
            let t = Triple {
                clip: k.clip,
                material: k.material,
                texture: k.texture,
            };
            let id = match last {
                Some((lt, id)) if lt == t => id,
                _ => {
                    let next = u32::try_from(self.distinct.len()).unwrap_or(u32::MAX);
                    let id = *self.ids.entry(t).or_insert(next);
                    if id == next {
                        self.distinct.push(t);
                    }
                    last = Some((t, id));
                    id
                }
            };
            self.object_ids.push(id);
        }
        // Descending addresses: clip, then material, then texture.
        let by_rank = &mut self.by_rank;
        by_rank.clear();
        by_rank.extend((0..self.distinct.len()).map(|i| u32::try_from(i).unwrap_or(u32::MAX)));
        let distinct = &self.distinct;
        by_rank.sort_unstable_by_key(|&id| {
            distinct
                .get(id as usize)
                .map(|t| std::cmp::Reverse((t.clip, t.material, t.texture)))
        });
        self.rank_of_id.clear();
        self.rank_of_id.resize(by_rank.len(), 0);
        for (rank, &id) in by_rank.iter().enumerate() {
            if let Some(slot) = self.rank_of_id.get_mut(id as usize) {
                *slot = u32::try_from(rank).unwrap_or(u32::MAX);
            }
        }
        self.entries.clear();
        for (i, (k, &id)) in keys.iter().zip(&self.object_ids).enumerate() {
            let rank = self.rank_of_id.get(id as usize).copied().unwrap_or(0);
            let index = u32::try_from(i).unwrap_or(u32::MAX);
            self.entries
                .push((u64::from(z_key(k.z)) << 32 | u64::from(rank), index));
        }
    }

    fn insertion_sort(&mut self) {
        for i in 1..self.entries.len() {
            let mut j = i;
            while j > 0
                && matches!((self.entries.get(j - 1), self.entries.get(j)), (Some(a), Some(b)) if a.0 > b.0)
            {
                self.entries.swap(j - 1, j);
                j -= 1;
            }
        }
    }

    #[expect(
        clippy::indexing_slicing,
        reason = "digit and byte values are below 256, scatter positions below `n` by the histogram"
    )]
    fn radix_sort(&mut self) {
        let n = self.entries.len();
        if self
            .entries
            .windows(2)
            .all(|w| matches!(w, [a, b] if a.0 <= b.0))
        {
            return;
        }
        if n <= SMALL {
            self.insertion_sort();
            return;
        }
        let counts = &mut self.counts;
        counts.clear();
        counts.resize(8 * 256, 0);
        for &(key, _) in &self.entries {
            for d in 0..8 {
                counts[d * 256 + (key >> (8 * d) & 0xff) as usize] += 1;
            }
        }
        self.spare.clear();
        self.spare.resize(n, (0, 0));
        for d in 0..8 {
            let hist = &counts[d * 256..(d + 1) * 256];
            if hist.contains(&n) {
                continue;
            }
            let mut offset = [0usize; 256];
            let mut sum = 0;
            for (o, &c) in offset.iter_mut().zip(hist) {
                *o = sum;
                sum += c;
            }
            for &e in &self.entries {
                let b = (e.0 >> (8 * d) & 0xff) as usize;
                self.spare[offset[b]] = e;
                offset[b] += 1;
            }
            std::mem::swap(&mut self.entries, &mut self.spare);
        }
    }
}
