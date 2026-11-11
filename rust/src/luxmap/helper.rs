//! `LuxMapHelper.cpp`: ray-callback filters, shape-damage decisions and light-level math.

use crate::math::{Vector3f, fma, normalize3, v3};

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct SubMesh {
    pub parent_is_body: bool,
    pub has_material: bool,
    pub translucent: bool,
    pub shadow_caster: bool,
}

/// `cLuxLineOfSightCallback::BeforeIntersect`.
pub fn los_before(
    is_char: bool,
    collide: bool,
    check_shadow: bool,
    blocks_light: bool,
    has_mesh_entity: bool,
    subs: &[SubMesh],
) -> bool {
    if is_char || !collide || (check_shadow && !blocks_light) {
        return false;
    }
    if !has_mesh_entity {
        return true;
    }
    subs.iter().any(|s| {
        s.parent_is_body && s.has_material && !s.translucent && (!check_shadow || s.shadow_caster)
    })
}

pub const CLOSEST_REJECT: i32 = 0;
pub const CLOSEST_ACCEPT: i32 = 1;
/// The caller asks the entity whether it can be interacted with and uses that answer.
pub const CLOSEST_ASK: i32 = 2;

/// `cLuxClosestEntityCallback::BeforeIntersect`.
pub fn closest_entity_before(
    has_entity: bool,
    entity_active: bool,
    collide: bool,
    is_char: bool,
) -> i32 {
    if has_entity {
        if !entity_active {
            return CLOSEST_REJECT;
        }
        if !collide {
            return CLOSEST_ASK;
        }
        CLOSEST_ACCEPT
    } else if is_char || !collide {
        CLOSEST_REJECT
    } else {
        CLOSEST_ACCEPT
    }
}

/// Both closest-hit callbacks keep the nearer hit.
pub fn closer(dist: f32, closest: f32) -> bool {
    dist < closest
}

pub fn char_collider_before(collide_character: bool, is_char: bool) -> bool {
    collide_character && !is_char
}

pub fn attack_ray_before(is_char: bool, collide: bool, is_skip_body: bool) -> bool {
    !(is_char || !collide || is_skip_body)
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct DamageFilter {
    pub active: bool,
    pub collide: bool,
    pub has_user_data: bool,
    pub check_props: bool,
    pub is_char: bool,
    pub is_player_body: bool,
    pub check_player: bool,
    pub check_enemies: bool,
}

/// `ShapeDamage`: true when the body is passed over.
pub fn damage_skips(f: DamageFilter) -> bool {
    if !f.active || !f.collide {
        return true;
    }
    if f.has_user_data && !f.check_props {
        return true;
    }
    if f.is_char {
        return if f.is_player_body {
            !f.check_player
        } else {
            !f.check_enemies
        };
    }
    false
}

/// Mean of the contact points (`0/0` when there are none, as the C++).
pub fn hit_position(points: &[Vector3f]) -> Vector3f {
    let mut s = v3(0.0, 0.0, 0.0);
    for p in points {
        s = v3(s.x + p.x, s.y + p.y, s.z + p.z);
    }
    let n = points.len() as f32;
    v3(s.x / n, s.y / n, s.z / n)
}

pub fn hit_direction(hit: Vector3f, origin: Vector3f) -> Vector3f {
    let mut d = v3(hit.x - origin.x, hit.y - origin.y, hit.z - origin.z);
    normalize3(&mut d);
    d
}

/// Velocity added to a character body: `dir * force * 0.1`.
pub fn char_push(dir: Vector3f, force: f32) -> Vector3f {
    let f = |c: f32| c * force * 0.1;
    v3(f(dir.x), f(dir.y), f(dir.z))
}

/// Impulse on a prop: `dir * min(force / mass, max_impulse)`.
pub fn prop_impulse(dir: Vector3f, force: f32, mass: f32, max_impulse: f32) -> Vector3f {
    let a = force / mass;
    let i = if a < max_impulse { a } else { max_impulse };
    v3(dir.x * i, dir.y * i, dir.z * i)
}

pub fn takes_push(mass: f32, is_char: bool) -> bool {
    mass > 0.0 || is_char
}

pub const TARGET_NONE: i32 = 0;
pub const TARGET_ENTITY: i32 = 1;
pub const TARGET_PLAYER: i32 = 2;

pub fn damage_target(has_entity: bool, is_player_body: bool) -> i32 {
    if has_entity {
        TARGET_ENTITY
    } else if is_player_body {
        TARGET_PLAYER
    } else {
        TARGET_NONE
    }
}

pub fn max_rgb(r: f32, g: f32, b: f32) -> f32 {
    let mut a = r;
    if a < g {
        a = g;
    }
    if a < b {
        a = b;
    }
    a
}

pub const LIGHT_BOX: i32 = 0;
/// Spot light that casts shadows: the line of sight to it must be clear first.
pub const LIGHT_NEEDS_LOS: i32 = 1;
pub const LIGHT_FALLOFF: i32 = 2;

pub fn light_kind(is_box: bool, is_spot: bool, casts_shadows: bool) -> i32 {
    if is_box {
        LIGHT_BOX
    } else if is_spot && casts_shadows {
        LIGHT_NEEDS_LOS
    } else {
        LIGHT_FALLOFF
    }
}

/// Spot and point contribution added to the running level: brightness faded linearly to the
/// (extended) radius.
pub fn light_falloff_add(level: f32, amount: f32, dist: f32, radius: f32, radius_add: f32) -> f32 {
    let mut t = 1.0 - dist / (radius + radius_add);
    if t < 0.0 {
        t = 0.0;
    }
    fma(amount, t, level)
}
