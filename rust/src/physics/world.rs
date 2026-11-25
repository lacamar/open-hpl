//! `cPhysics` impact throttling and the body filter / push merge of `CheckShapeWorldCollision`.

use crate::types::Vector3f;

#[derive(Debug, Default)]
pub struct ImpactCounts(Vec<f32>);

impl ImpactCounts {
    pub fn can_play(&self, max: i32) -> bool {
        i32::try_from(self.0.len()).is_ok_and(|n| n < max)
    }

    pub fn add(&mut self) {
        self.0.push(0.0);
    }

    pub fn update(&mut self, time_step: f32, duration: f32) {
        self.0.retain_mut(|c| {
            *c += time_step;
            !matches!(
                (*c).partial_cmp(&duration),
                Some(std::cmp::Ordering::Greater)
            )
        });
    }

    pub fn len(&self) -> usize {
        self.0.len()
    }

    pub fn is_empty(&self) -> bool {
        self.0.is_empty()
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct BodyFilter {
    pub active: bool,
    pub is_character: bool,
    pub is_skip_body: bool,
    pub mass: f32,
    pub collide_character: bool,
    pub collide: bool,
    pub push_strength: i32,
    pub flags_match: bool,
    pub query_collide_character: bool,
    pub query_is_character: bool,
    pub skip_static: bool,
    pub min_push_strength: i32,
}

/// True when the body takes part in the shape query.
pub fn body_passes(f: &BodyFilter) -> bool {
    f.active
        && !(f.is_character && !f.query_collide_character)
        && !f.is_skip_body
        && !(f.skip_static && f.mass == 0.0 && !f.is_character)
        && !(f.query_is_character && !f.collide_character)
        && !(!f.query_is_character && !f.collide)
        && f.min_push_strength <= f.push_strength
        && f.flags_match
}

/// Keeps, per axis, the push of largest magnitude.
pub fn merge_push(push: Vector3f, normal: Vector3f, depth: f32) -> Vector3f {
    let v = Vector3f {
        x: normal.x * depth,
        y: normal.y * depth,
        z: normal.z * depth,
    };
    let pick = |cur: f32, new: f32| if cur.abs() < new.abs() { new } else { cur };
    Vector3f {
        x: pick(push.x, v.x),
        y: pick(push.y, v.y),
        z: pick(push.z, v.z),
    }
}
