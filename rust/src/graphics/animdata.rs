//! `cAnimationTrack` key-frame ownership, `cAnimation` track lookup, `cMesh` sub-mesh/animation
//! name indices.

use super::anim::{self, Frames, KeyFrame};
use std::collections::BTreeMap;

/// Key frames are heap blocks whose addresses C++ keeps and writes through.
pub struct Track {
    frames: Vec<*mut KeyFrame>,
    pub max_frame_time: f32,
}

impl Default for Track {
    fn default() -> Self {
        Self::new()
    }
}

impl Drop for Track {
    fn drop(&mut self) {
        self.clear();
    }
}

impl Track {
    pub fn new() -> Self {
        Self {
            frames: Vec::new(),
            max_frame_time: 0.0,
        }
    }

    fn view(&self) -> Frames<'_> {
        // SAFETY: every pointer came from `Box::into_raw` and lives until `clear`/drop.
        unsafe {
            Frames::new(std::slice::from_raw_parts(
                self.frames.as_ptr().cast(),
                self.frames.len(),
            ))
        }
    }

    pub fn len(&self) -> usize {
        self.frames.len()
    }

    pub fn is_empty(&self) -> bool {
        self.frames.is_empty()
    }

    pub fn frame(&self, i: usize) -> *mut KeyFrame {
        self.frames.get(i).copied().unwrap_or(std::ptr::null_mut())
    }

    pub fn reserve(&mut self, n: usize) {
        self.frames.reserve(n.saturating_sub(self.frames.len()));
    }

    /// `CreateKeyFrame`; the maximum is not lowered by `clear`, as in C++.
    pub fn create(&mut self, time: f32) -> *mut KeyFrame {
        let (pos, upd) = anim::insert_position(self.view(), self.max_frame_time, time);
        let p = Box::into_raw(Box::new(KeyFrame {
            time,
            ..KeyFrame::default()
        }));
        self.frames.insert(pos, p);
        if upd {
            self.max_frame_time = time;
        }
        p
    }

    pub fn clear(&mut self) {
        for p in self.frames.drain(..) {
            // SAFETY: allocated by `create` and removed from the list here.
            drop(unsafe { Box::from_raw(p) });
        }
    }

    pub fn interpolate(&self, length: f32, time: f32) -> KeyFrame {
        anim::interpolate(self.view(), length, self.max_frame_time, time)
    }

    pub fn frames_at_time(&self, length: f32, time: f32) -> (usize, usize, f32) {
        anim::frames_at_time(self.view(), length, self.max_frame_time, time)
    }
}

#[derive(Default)]
pub struct AnimInfo {
    pub length: f32,
    names: Vec<Vec<u8>>,
}

impl AnimInfo {
    pub fn add_track(&mut self, name: &[u8]) {
        self.names.push(name.to_vec());
    }

    pub fn track_by_name(&self, name: &[u8]) -> i32 {
        self.names
            .iter()
            .position(|n| n == name)
            .map_or(-1, |i| i as i32)
    }
}

/// `std::map`/`std::multimap` insert keeps the first entry for a name.
#[derive(Default)]
pub struct MeshInfo {
    sub_names: Vec<Vec<u8>>,
    sub_map: BTreeMap<Vec<u8>, i32>,
    anim_count: i32,
    anim_map: BTreeMap<Vec<u8>, i32>,
}

impl MeshInfo {
    pub fn add_sub(&mut self, name: &[u8]) {
        self.sub_map
            .entry(name.to_vec())
            .or_insert(self.sub_names.len() as i32);
        self.sub_names.push(name.to_vec());
    }

    pub fn sub_index(&self, name: &[u8]) -> i32 {
        self.sub_names
            .iter()
            .position(|n| n == name)
            .map_or(-1, |i| i as i32)
    }

    pub fn sub_by_name(&self, name: &[u8]) -> i32 {
        self.sub_map.get(name).copied().unwrap_or(-1)
    }

    pub fn add_animation(&mut self, name: &[u8]) {
        self.anim_map
            .entry(name.to_vec())
            .or_insert(self.anim_count);
        self.anim_count += 1;
    }

    pub fn animation_index(&self, name: &[u8]) -> i32 {
        self.anim_map.get(name).copied().unwrap_or(-1)
    }

    pub fn clear_animations(&mut self) {
        self.anim_count = 0;
        self.anim_map.clear();
    }
}
