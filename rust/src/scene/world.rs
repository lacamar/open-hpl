//! `cWorld` decisions: name/id/pointer lookups over the entity lists, force-field selection,
//! start-position choice, skybox colour, physics bounds and the pre-update step count. Entities
//! and containers stay C++ objects and cross as indices into arrays the caller built.

use crate::types::{Color, Vector3f};

pub const DESTROY_STATIC: u32 = 1;
pub const DESTROY_PHYSICS: u32 = 2;
const SKIP_STATIC_ENTITIES: u32 = 1;
const SKIP_PHYSICS: u32 = 2;

fn index(i: Option<usize>) -> i32 {
    i.and_then(|i| i32::try_from(i).ok()).unwrap_or(-1)
}

pub fn find_by_name<'a>(mut names: impl Iterator<Item = &'a [u8]>, key: &[u8]) -> i32 {
    index(names.position(|n| n == key))
}

/// `CreateAINodeContainer` keeps looping after a match, so the last duplicate wins.
pub fn find_last_by_name<'a>(names: impl Iterator<Item = &'a [u8]>, key: &[u8]) -> i32 {
    let mut found = None;
    for (i, n) in names.enumerate() {
        if n == key {
            found = Some(i);
        }
    }
    index(found)
}

pub fn find_by_id(mut ids: impl Iterator<Item = i32>, id: i32) -> i32 {
    index(ids.position(|i| i == id))
}

pub fn find_ptr(mut ptrs: impl Iterator<Item = usize>, p: usize) -> i32 {
    index(ptrs.position(|x| x == p))
}

/// Index of the lexicographically smallest name, first one on ties.
pub fn first_by_name<'a>(names: impl Iterator<Item = &'a [u8]>) -> i32 {
    let mut best: Option<(usize, &[u8])> = None;
    for (i, n) in names.enumerate() {
        if best.is_none_or(|(_, b)| n < b) {
            best = Some((i, n));
        }
    }
    index(best.map(|(i, _)| i))
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct FieldIn {
    pub min: Vector3f,
    pub max: Vector3f,
    pub radius: f32,
}

fn aabb_hit(a: &FieldIn, min: Vector3f, max: Vector3f) -> bool {
    !(a.max.x < min.x
        || a.max.y < min.y
        || a.max.z < min.z
        || max.x < a.min.x
        || max.y < a.min.y
        || max.z < a.min.z)
}

/// Fields touching the box, biggest radius first, at most four. `out` receives indices into
/// `fields`; returns the original `lNum`, which saturates at 4.
pub fn force_fields(fields: &[FieldIn], min: Vector3f, max: Vector3f, out: &mut [u32; 4]) -> i32 {
    let radius = |out: &[u32; 4], i: usize| -> f32 {
        out.get(i)
            .and_then(|&k| fields.get(k as usize))
            .map_or(0.0, |f| f.radius)
    };
    let mut num = 0usize;
    for (k, f) in fields.iter().enumerate() {
        if !aabb_hit(f, min, max) {
            continue;
        }
        let mut i = if num < 4 {
            num += 1;
            num - 1
        } else {
            4
        };
        while i > 0 && radius(out, i - 1) < f.radius {
            if i < 4
                && let Some(prev) = out.get(i - 1).copied()
                && let Some(slot) = out.get_mut(i)
            {
                *slot = prev;
            }
            i -= 1;
        }
        if let Some(slot) = out.get_mut(i) {
            *slot = k as u32;
        }
    }
    num as i32
}

/// `UpdateSkyBoxVBColor`'s vertex colour.
pub fn skybox_color(sky: Color, hpl3: bool, brightness: f32) -> Color {
    let mut c = sky;
    if hpl3 {
        c = Color {
            r: c.r * c.r,
            g: c.g * c.g,
            b: c.b * c.b,
            a: c.a * 1.0,
        };
    }
    Color {
        r: c.r * brightness,
        g: c.g * brightness,
        b: c.b * brightness,
        a: c.a,
    }
}

/// Static root bounds grown by the 10 m border `Compile` gives the physics world.
pub fn physics_bounds(min: Vector3f, max: Vector3f) -> (Vector3f, Vector3f) {
    let g = |v: Vector3f, d: f32| Vector3f {
        x: v.x + d,
        y: v.y + d,
        z: v.z + d,
    };
    (g(min, -10.0), g(max, 10.0))
}

/// Number of `PreUpdate` iterations. The C++ loop never ends when the step cannot shrink the
/// remaining time (step <= 0 or below float resolution); that case ends the loop instead.
pub fn pre_update_steps(total: f32, step: f32) -> u32 {
    let mut left = total;
    let mut n = 0u32;
    while left > 0.0 {
        n = n.saturating_add(1);
        let next = left - step;
        if next >= left {
            break;
        }
        left = next;
    }
    n
}

pub fn destroy_plan(flags: u32) -> u32 {
    let mut plan = 0;
    if flags & SKIP_STATIC_ENTITIES == 0 {
        plan |= DESTROY_STATIC;
    }
    if flags & SKIP_PHYSICS == 0 {
        plan |= DESTROY_PHYSICS;
    }
    plan
}
