//! `cProgramComboManager` cache: programs keyed by feature flags, shaders keyed by the flags that
//! reach their stage, both reference counted. Creation and destruction stay with the C++ caller;
//! values are opaque handles.

use std::collections::BTreeMap;

pub const VERTEX_BIT: u32 = 0x1;

#[derive(Clone, Copy)]
struct Entry {
    handle: usize,
    users: i32,
}

#[derive(Default)]
struct Mode {
    feature_stages: Vec<u32>,
    combinations: i32,
    programs: BTreeMap<u32, Entry>,
    vertex: BTreeMap<u32, Entry>,
    fragment: BTreeMap<u32, Entry>,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Release {
    NotFound,
    Kept,
    /// The entry is gone; the caller destroys the handle.
    Destroyed(usize),
}

#[derive(Default)]
pub struct ComboCache {
    modes: Vec<Mode>,
}

fn flag_bit(bit: usize) -> u32 {
    1u32.wrapping_shl(bit as u32)
}

fn release(map: &mut BTreeMap<u32, Entry>, key: u32) -> Release {
    let Some(e) = map.get_mut(&key) else {
        return Release::NotFound;
    };
    e.users -= 1;
    if e.users > 0 {
        return Release::Kept;
    }
    let handle = e.handle;
    map.remove(&key);
    Release::Destroyed(handle)
}

impl ComboCache {
    pub fn new(modes: usize) -> Self {
        Self {
            modes: (0..modes).map(|_| Mode::default()).collect(),
        }
    }

    fn mode(&mut self, mode: usize) -> Option<&mut Mode> {
        self.modes.get_mut(mode)
    }

    /// Stores the stage mask of every feature; returns the combination count `(int)(2^n + 0.49)`.
    pub fn setup(&mut self, mode: usize, feature_stages: &[u32]) -> i32 {
        let Some(m) = self.mode(mode) else { return 0 };
        m.feature_stages = feature_stages.to_vec();
        m.combinations = (2f64.powf(feature_stages.len() as f64) + 0.49) as i32;
        m.combinations
    }

    pub fn combinations(&self, mode: usize) -> i32 {
        self.modes.get(mode).map_or(0, |m| m.combinations)
    }

    /// Features of `flags` whose stage mask contains `stage`.
    pub fn valid_bits(&self, mode: usize, flags: u32, stage: u32) -> u32 {
        let Some(m) = self.modes.get(mode) else {
            return 0;
        };
        m.feature_stages
            .iter()
            .enumerate()
            .filter(|&(i, &s)| flags & flag_bit(i) != 0 && s & stage != 0)
            .fold(0, |acc, (i, _)| acc | flag_bit(i))
    }

    /// Cache hit: counts a user and returns the handle (0 for a program that failed to build).
    pub fn program_acquire(&mut self, mode: usize, flags: u32) -> Option<usize> {
        let m = self.mode(mode)?;
        let e = m.programs.get_mut(&flags)?;
        e.users = e.users.wrapping_add(1);
        Some(e.handle)
    }

    pub fn program_insert(&mut self, mode: usize, flags: u32, handle: usize) {
        if let Some(m) = self.mode(mode) {
            m.programs
                .entry(flags)
                .or_insert(Entry { handle, users: 1 });
        }
    }

    pub fn program_release(&mut self, mode: usize, flags: u32) -> Release {
        self.mode(mode)
            .map_or(Release::NotFound, |m| release(&mut m.programs, flags))
    }

    fn stage_map(m: &mut Mode, stage: u32) -> &mut BTreeMap<u32, Entry> {
        if stage == VERTEX_BIT {
            &mut m.vertex
        } else {
            &mut m.fragment
        }
    }

    pub fn shader_acquire(&mut self, mode: usize, stage: u32, bits: u32) -> Option<usize> {
        let m = self.mode(mode)?;
        let e = Self::stage_map(m, stage).get_mut(&bits)?;
        e.users = e.users.wrapping_add(1);
        Some(e.handle)
    }

    pub fn shader_insert(&mut self, mode: usize, stage: u32, bits: u32, handle: usize) {
        if let Some(m) = self.mode(mode) {
            Self::stage_map(m, stage)
                .entry(bits)
                .or_insert(Entry { handle, users: 1 });
        }
    }

    pub fn shader_release(&mut self, mode: usize, stage: u32, bits: u32) -> Release {
        self.mode(mode).map_or(Release::NotFound, |m| {
            release(Self::stage_map(m, stage), bits)
        })
    }

    /// Removes the program with the smallest key, for ordered teardown.
    pub fn pop_program(&mut self, mode: usize) -> Option<usize> {
        self.mode(mode)?.programs.pop_first().map(|(_, e)| e.handle)
    }

    pub fn pop_shader(&mut self, mode: usize, stage: u32) -> Option<usize> {
        let m = self.mode(mode)?;
        Self::stage_map(m, stage).pop_first().map(|(_, e)| e.handle)
    }

    pub fn program_count(&self, mode: usize) -> usize {
        self.modes.get(mode).map_or(0, |m| m.programs.len())
    }

    pub fn shader_count(&self, mode: usize, stage: u32) -> usize {
        self.modes.get(mode).map_or(0, |m| {
            if stage == VERTEX_BIT {
                m.vertex.len()
            } else {
                m.fragment.len()
            }
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn counts_users_and_destroys_at_zero() {
        let mut c = ComboCache::new(1);
        assert_eq!(c.program_acquire(0, 5), None);
        c.program_insert(0, 5, 77);
        assert_eq!(c.program_acquire(0, 5), Some(77));
        assert_eq!(c.program_release(0, 5), Release::Kept);
        assert_eq!(c.program_release(0, 5), Release::Destroyed(77));
        assert_eq!(c.program_release(0, 5), Release::NotFound);
    }

    #[test]
    fn failed_build_is_cached_as_null() {
        let mut c = ComboCache::new(1);
        c.program_insert(0, 1, 0);
        assert_eq!(c.program_acquire(0, 1), Some(0));
    }

    #[test]
    fn valid_bits_filter_by_stage() {
        let mut c = ComboCache::new(2);
        assert_eq!(c.setup(1, &[1, 2, 3, 2]), 16);
        assert_eq!(c.valid_bits(1, 0b1111, 1), 0b0101);
        assert_eq!(c.valid_bits(1, 0b1111, 2), 0b1110);
        assert_eq!(c.valid_bits(1, 0b0010, 1), 0);
        assert_eq!(c.valid_bits(9, 0b1111, 1), 0);
    }

    #[test]
    fn shaders_are_split_by_stage() {
        let mut c = ComboCache::new(1);
        c.shader_insert(0, VERTEX_BIT, 3, 10);
        c.shader_insert(0, 2, 3, 20);
        assert_eq!(c.shader_acquire(0, VERTEX_BIT, 3), Some(10));
        assert_eq!(c.shader_acquire(0, 2, 3), Some(20));
        assert_eq!(c.pop_shader(0, 2), Some(20));
        assert_eq!(c.shader_count(0, 2), 0);
        assert_eq!(c.shader_count(0, VERTEX_BIT), 1);
    }

    #[test]
    fn pops_programs_in_key_order() {
        let mut c = ComboCache::new(1);
        for (k, h) in [(9, 1), (2, 2), (5, 3)] {
            c.program_insert(0, k, h);
        }
        assert_eq!(
            (c.pop_program(0), c.pop_program(0), c.pop_program(0)),
            (Some(2), Some(3), Some(1))
        );
        assert_eq!(c.pop_program(0), None);
    }
}
