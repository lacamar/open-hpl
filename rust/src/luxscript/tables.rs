//! Name tables and small math of the script API.

use crate::math;
use crate::string;

pub(crate) fn progress_level(s: &[u8]) -> i32 {
    match string::to_lower(s).as_slice() {
        b"medium" => 1,
        b"high" => 2,
        _ => 0,
    }
}

pub(crate) fn damage_type(s: &[u8]) -> i32 {
    match string::to_lower(s).as_slice() {
        b"claws" => 1,
        b"slash" => 2,
        _ => 0,
    }
}

/// -1 for an unknown name (C++ logs and falls back to Default).
pub(crate) fn crosshair(s: &[u8]) -> i32 {
    match string::to_lower(s).as_slice() {
        b"default" => 0,
        b"grab" => 1,
        b"push" => 2,
        b"ignite" => 3,
        b"pick" => 4,
        b"leveldoor" => 5,
        b"ladder" => 6,
        _ => -1,
    }
}

pub(crate) fn pose(s: &[u8]) -> i32 {
    match s {
        b"Biped" => 0,
        b"Quadruped" => 1,
        _ => -1,
    }
}

pub(crate) const ACH_NONE: i32 = 0;

/// Script achievement names: 1..=7 are the UnlockAchievement names, 8..=10 the quest ones.
pub(crate) fn achievement(s: &[u8]) -> i32 {
    match s {
        b"Benefactor" => 1,
        b"Survivor" => 2,
        b"Sacrifice" => 3,
        b"Quitter" => 4,
        b"Egotist" => 5,
        b"Altruist" => 6,
        b"Vacillator" => 7,
        _ => ACH_NONE,
    }
}

pub(crate) fn achievement_hardmode(id: i32) -> bool {
    (1..=3).contains(&id)
}

pub(crate) fn quest_achievement(s: &[u8]) -> i32 {
    match s {
        b"02Web" => 8,
        b"SewerFlooded" => 9,
        b"21FindOrb" => 10,
        _ => ACH_NONE,
    }
}

pub(crate) fn sound_is_entity(file: &[u8]) -> bool {
    let ext = string::file_ext(file);
    ext.is_empty() || ext == b"snt"
}

pub(crate) fn coord_is_local(s: &[u8]) -> bool {
    string::to_lower(s) == b"local"
}

pub(crate) fn sanity_boost(s: f32) -> f32 {
    if s < 25.0 {
        100.0 - s
    } else if s < 50.0 {
        90.0 - s
    } else if s < 75.0 {
        80.0 - s
    } else {
        5.0
    }
}

pub(crate) fn sanity_boost_small(s: f32) -> f32 {
    if s < 25.0 {
        20.0
    } else if s < 50.0 {
        15.0
    } else if s < 75.0 {
        10.0
    } else {
        5.0
    }
}

pub(crate) fn fade_speed(t: f32) -> f32 {
    if t == 0.0 { 0.0 } else { 1.0 / t }
}

pub(crate) fn inverse(t: f32) -> f32 {
    1.0 / t
}

/// `forward*z + right*y + up*y` (the C++ reuses y for the up axis), with gcc's fused chain.
pub(crate) fn local_force(f: [f32; 3], r: [f32; 3], u: [f32; 3], y: f32, z: f32) -> [f32; 3] {
    let c = |f: f32, r: f32, u: f32| math::fma(u, y, math::fma(f, z, r * y));
    [
        c(f[0], r[0], u[0]),
        c(f[1], r[1], u[1]),
        c(f[2], r[2], u[2]),
    ]
}

pub(crate) fn rope_segments(total: f32, segment: f32) -> i32 {
    ((total / segment) as i32).wrapping_add(4)
}

pub(crate) fn min(a: f32, b: f32) -> f32 {
    if a < b { a } else { b }
}

pub(crate) fn max(a: f32, b: f32) -> f32 {
    if a > b { a } else { b }
}
