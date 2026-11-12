//! `LuxMap.cpp`: timers and callbacks bookkeeping, entity ids, completion, checkpoints, enemy
//! sound broadcast and load-time decisions. The C++ lists stay (saves read them); this decides.

use crate::math::{clamp, fma};
use crate::string;

/// `AddTimer`: zero or negative times are bumped.
pub fn timer_count(time: f32) -> f32 {
    if time > 0.0 { time } else { 0.001 }
}

/// `UpdateTimers` first pass over every timer count.
pub fn timers_step(counts: &mut [f32], step: f32) {
    for c in counts {
        *c -= step;
    }
}

pub fn timer_due(count: f32, destroy_me: bool) -> bool {
    count <= 0.0 && !destroy_me
}

/// `RemoveTimer` while timers are being updated only marks them.
pub fn timer_remove_marks(updating: bool) -> bool {
    updating
}

/// `RemoveUseItemCallback(callback, name)`: a non-empty name must match the callback's own.
pub fn callback_remove_ok(name: &[u8], callback_name: &[u8]) -> bool {
    name.is_empty() || name == callback_name
}

pub fn use_item_matches(item: &[u8], entity: &[u8], want_item: &[u8], want_entity: &[u8]) -> bool {
    item == want_item && entity == want_entity
}

/// Lowest unused id of the ascending id list.
pub fn free_entity_id(ids: &[i32]) -> i32 {
    let mut next = 0i32;
    for &id in ids {
        if id != next {
            return next;
        }
        next = next.wrapping_add(1);
    }
    next
}

pub fn completion_fraction(current: i32, total: i32) -> f32 {
    if total == 0 {
        0.0
    } else {
        current as f32 / total as f32
    }
}

/// `AddCompletionAmount`: `(previous, new)` fractions; the current amount wraps.
pub fn completion_add(current: &mut i32, total: i32, amount: i32) -> (f32, f32) {
    let previous = completion_fraction(*current, total);
    *current = current.wrapping_add(amount);
    (previous, completion_fraction(*current, total))
}

pub fn quest_completion(total: i32, quests: i32, value: i32) -> i32 {
    total.wrapping_add(quests.wrapping_mul(value))
}

pub const COMPLETE_NONE: i32 = 0;
pub const COMPLETE_PUZZLE: i32 = 1;
pub const COMPLETE_NOTE: i32 = 2;
pub const COMPLETE_DIARY: i32 = 3;
pub const COMPLETE_CHEST: i32 = 4;
pub const COMPLETE_FLASHBACK: i32 = 5;

/// Handler values, in the order of the `COMPLETE_*` kinds from 1.
pub fn completion_value(kind: i32, values: &[i32; 5]) -> i32 {
    match kind {
        COMPLETE_PUZZLE..=COMPLETE_FLASHBACK => {
            values.get((kind - 1) as usize).copied().unwrap_or(0)
        }
        _ => 0,
    }
}

pub fn dissolve_speed(time: f32) -> f32 {
    1.0 / time
}

/// `(alpha, remove)` after one step.
pub fn dissolve_step(alpha: f32, speed: f32, step: f32) -> (f32, bool) {
    let a = fma(-speed, step, alpha);
    (a, a <= 0.0)
}

/// `UpdateCheckCommentaryIconActive`: the new icon state, when it changes.
pub fn commentary_sync(icons_active: bool, show: bool) -> Option<bool> {
    (icons_active != show).then_some(show)
}

/// A looping, non-effect music entry is what a checkpoint resumes.
pub fn checkpoint_music_pick(file: &[u8], looped: bool, special_effect: bool) -> bool {
    !file.is_empty() && looped && !special_effect
}

pub fn checkpoint_music_volume(volume: f32, volume_mul: f32) -> f32 {
    if volume_mul > 0.0 {
        volume / volume_mul
    } else {
        volume
    }
}

/// `LoadCheckPoint` last step: `<callback>("<name>", <count>)`.
pub fn checkpoint_call(callback: &[u8], name: &[u8], count: i32) -> Vec<u8> {
    let mut v = callback.to_vec();
    v.extend_from_slice(b"(\"");
    v.extend_from_slice(name);
    v.extend_from_slice(b"\", ");
    v.extend_from_slice(&string::int_to_string(count, 0));
    v.push(b')');
    v
}

/// `BroadcastEnemySoundMessage`: the volume an enemy hears, or none when too close. With the
/// handler passing the max distance as the min too, the ratio divides by zero (kept).
pub fn sound_volume(dist: f32, min: f32, max: f32, volume: f32) -> Option<f32> {
    if dist < 2.0 {
        return None;
    }
    let heard = 1.0 - clamp((dist - min) / (max - min), 0.0, 1.0);
    Some(heard * volume)
}

pub fn sound_prefix_matches(type_name: &[u8], name: &[u8]) -> bool {
    type_name.starts_with(name)
}

/// Hard mode: share of tinderboxes to remove per box, from how many the player has.
pub fn hard_tinderbox_every(tinderboxes: i32) -> f32 {
    let (min, max) = (7.0f32 / 10.0, 9.5f32 / 10.0);
    let t = tinderboxes as f32 / 6.0;
    fma(t, max, (1.0 - t) * min)
}

pub const HARD_OIL_EVERY: f32 = 6.5 / 10.0;

/// Fraction accumulator: true when this item is removed.
pub fn hard_remove(counter: &mut f32, every: f32) -> bool {
    *counter += every;
    if *counter >= 1.0 {
        *counter -= 1.0;
        true
    } else {
        false
    }
}

pub fn world_load_flags(
    load_entities: bool,
    fast_physics: bool,
    fast_static: bool,
    fast_entity: bool,
    flags: [i32; 4],
) -> i32 {
    let mut f = 0;
    for (on, bit) in [!load_entities, fast_physics, fast_static, fast_entity]
        .into_iter()
        .zip(flags)
    {
        if on {
            f |= bit;
        }
    }
    f
}

pub const SCRIPT_NONE: i32 = 0;
pub const SCRIPT_PLAIN: i32 = 1;
pub const SCRIPT_COMPRESSED: i32 = 2;

pub fn script_choice(plain_exists: bool, compressed_enabled: bool, compressed_exists: bool) -> i32 {
    if plain_exists {
        SCRIPT_PLAIN
    } else if compressed_enabled && compressed_exists {
        SCRIPT_COMPRESSED
    } else {
        SCRIPT_NONE
    }
}

/// The compile-message lines that mention an error, each as ` <line>\n`.
pub fn compile_errors(messages: &[u8]) -> Vec<u8> {
    let mut out = Vec::new();
    for row in string::split(messages, Some(b"\n"), true) {
        if string::first_pos(row, b"ERR") >= 0 {
            out.push(b' ');
            out.extend_from_slice(row);
            out.push(b'\n');
        }
    }
    out
}

pub const ENTER_START: i32 = 1;
pub const ENTER_COMPLETION: i32 = 2;
pub const ENTER_ENTER: i32 = 4;

/// `OnEnter` script part, as bits.
pub fn enter_plan(run_script: bool, has_script: bool, first_time: bool) -> i32 {
    if !run_script || !has_script {
        return 0;
    }
    (if first_time {
        ENTER_START | ENTER_COMPLETION
    } else {
        0
    }) | ENTER_ENTER
}

/// `RunScript`: only the current map runs scripts.
pub fn run_script_allowed(has_script: bool, is_current: bool) -> bool {
    has_script && is_current
}

pub const LIST_ENEMY: i32 = 1;
pub const LIST_STICKY: i32 = 2;

/// Extra lists an entity joins (`AddEntity`) and leaves (`UpdateToBeDesotroyedEntities`).
pub fn entity_lists(is_enemy: bool, is_area: bool, is_sticky: bool) -> i32 {
    if is_enemy {
        LIST_ENEMY
    } else if is_area && is_sticky {
        LIST_STICKY
    } else {
        0
    }
}
