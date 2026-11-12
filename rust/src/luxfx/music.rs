//! `cLuxMusicHandler`: priority stack and enemy/danger music timing. The slot strings, enemy sets
//! and the engine music handler stay C++; steps return ordered ops.

use super::ops::Ops;

pub mod k {
    pub const PLAY: u32 = 1;
    pub const CLEAR_SLOT: u32 = 3;
    pub const PLAY_DANGER: u32 = 4;
    pub const STOP_DANGER: u32 = 5;
    pub const STOP_CLOSE: u32 = 6;
    pub const STOP_LOWER: u32 = 7;
    pub const PLAY_ENEMY: u32 = 8;
    pub const STOP_ENEMY: u32 = 9;
}

pub const ENEMY_KINDS: usize = 2;

/// Clamps a priority to the stack; -1 for a negative one, which the C++ indexed out of bounds.
pub fn prio(p: i32, max: i32) -> i32 {
    if p < 0 {
        -1
    } else if p > max {
        max
    } else {
        p
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Slot {
    pub has_file: u32,
    pub looped: u32,
    pub resume: u32,
    pub volume: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct PlayOut {
    pub play: u32,
    pub volume: f32,
    pub fade_speed: f32,
    pub cur_prio: i32,
}

/// `Play` after the priority was clamped and `same_file` compared the slot with the new file.
/// Returns false when nothing happens; otherwise the slot is stored with `volume`.
pub fn play(
    cur_prio: i32,
    p: i32,
    same_file: bool,
    volume: f32,
    volume_mul: f32,
    fade_time: f32,
    out: &mut PlayOut,
) -> bool {
    if same_file {
        return false;
    }
    let v = volume * volume_mul;
    out.volume = v;
    out.cur_prio = cur_prio;
    out.play = 0;
    if cur_prio <= p {
        out.play = 1;
        out.fade_speed = v / fade_time;
        out.cur_prio = p;
    }
    true
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct StopOut {
    pub clear: u32,
    pub stop: u32,
    pub fade_speed: f32,
}

/// `Stop` on a clamped priority; `stop` also means "play the highest priority next".
pub fn stop(cur_prio: i32, p: i32, has_file: bool, slot_volume: f32, fade_time: f32) -> StopOut {
    let mut o = StopOut::default();
    if has_file {
        o.clear = 1;
        if p == cur_prio {
            o.stop = 1;
            o.fade_speed = slot_volume / fade_time;
        }
    }
    o
}

/// `PlayHighestPriority`: ops clear dropped one-shot slots and end with the slot to start.
/// Returns the new current priority, or -1.
pub fn play_highest(slots: &[Slot], max_prio: i32, o: &mut Ops) -> i32 {
    let mut i = max_prio;
    while i >= 0 {
        if let Some(s) = slots.get(i as usize).filter(|s| s.has_file != 0) {
            if s.looped != 0 {
                o.ki(k::PLAY, i);
                return i;
            }
            o.ki(k::CLEAR_SLOT, i);
        }
        i -= 1;
    }
    -1
}

pub fn song_over(cur_prio: i32, no_song: bool) -> bool {
    cur_prio >= 0 && no_song
}

/// Index of the candidate with the highest priority; the first wins ties. Candidates without a
/// music file are skipped.
pub fn pick_danger(nonempty: &[u32], prios: &[i32]) -> i32 {
    let mut best = -1i32;
    let mut best_prio = -1i32;
    for (i, (n, p)) in nonempty.iter().zip(prios.iter()).enumerate() {
        if *n != 0 && (best < 0 || best_prio < *p) {
            best = i as i32;
            best_prio = *p;
        }
    }
    best
}

/// Index of the candidate with the highest priority above -9999; the first wins ties.
pub fn pick_enemy(nonempty: &[u32], prios: &[i32]) -> i32 {
    let mut best = -1i32;
    let mut max = -9999i32;
    for (i, (n, p)) in nonempty.iter().zip(prios.iter()).enumerate() {
        if *n != 0 && *p > max {
            best = i as i32;
            max = *p;
        }
    }
    best
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Enemy {
    pub playing: [u32; ENEMY_KINDS],
    pub play_count: [f32; ENEMY_KINDS],
    pub stop_count: [f32; ENEMY_KINDS],
    pub close_playing: u32,
    pub close_count: f32,
    pub gone_count: f32,
}

pub const DANGER_RETURN: i32 = 0;
pub const DANGER_SCAN_IDLE: i32 = 1;
pub const DANGER_SCAN_PLAYING: i32 = 2;

/// `UpdateDangerMusic` entry checks: what the C++ scans the enemies for.
pub fn danger_begin(hard_mode: bool, e: &Enemy, has_map: bool) -> i32 {
    if hard_mode || e.playing.iter().any(|p| *p != 0) || !has_map {
        return DANGER_RETURN;
    }
    if e.close_playing == 0 {
        DANGER_SCAN_IDLE
    } else {
        DANGER_SCAN_PLAYING
    }
}

/// Idle danger scan done: `found` is whether an enemy in range has danger music.
pub fn danger_idle(e: &mut Enemy, found: bool, dt: f32, o: &mut Ops) {
    if found {
        e.gone_count = 0.0;
        e.close_count += dt;
        if e.close_count > 2.0 {
            o.k(k::PLAY_DANGER);
            e.close_playing = 1;
        }
    }
}

/// Playing danger scan done: `found` is whether any enemy is still in range.
pub fn danger_playing(e: &mut Enemy, found: bool, dt: f32, o: &mut Ops) {
    if !found {
        e.close_count = 0.0;
        e.gone_count += dt;
        if e.gone_count > 6.0 {
            o.k(k::STOP_DANGER);
            e.close_playing = 0;
        }
    }
}

pub const ENEMY_RETURN: i32 = 0;
pub const ENEMY_SCAN: i32 = 1;
pub const ENEMY_DONE: i32 = 2;

/// `UpdateEnemyMusic`: `ENEMY_SCAN` asks the C++ to pick the best music of the enemy set;
/// stop ops are already in `o` for `ENEMY_DONE`.
pub fn enemy_begin(
    e: &mut Enemy,
    t: usize,
    has_map: bool,
    set_empty: bool,
    dt: f32,
    o: &mut Ops,
) -> i32 {
    if !has_map {
        return ENEMY_RETURN;
    }
    if e.playing.iter().skip(t + 1).any(|p| *p != 0) {
        return ENEMY_RETURN;
    }
    let (Some(playing), Some(play_count), Some(stop_count)) = (
        e.playing.get(t).copied(),
        e.play_count.get(t).copied(),
        e.stop_count.get(t).copied(),
    ) else {
        return ENEMY_RETURN;
    };
    if playing == 0 && !set_empty {
        if let Some(c) = e.stop_count.get_mut(t) {
            *c = 0.0;
        }
        let pc = play_count + dt;
        if let Some(c) = e.play_count.get_mut(t) {
            *c = pc;
        }
        if pc > 0.1 {
            return ENEMY_SCAN;
        }
    } else if playing != 0 && set_empty {
        if let Some(c) = e.play_count.get_mut(t) {
            *c = 0.0;
        }
        let sc = stop_count + dt;
        if let Some(c) = e.stop_count.get_mut(t) {
            *c = sc;
        }
        if sc > 1.2 {
            if let Some(p) = e.playing.get_mut(t) {
                *p = 0;
            }
            o.ki(k::STOP_ENEMY, 8 + t as i32);
            if let Some(c) = e.stop_count.get_mut(t) {
                *c = 0.0;
            }
        }
        return ENEMY_DONE;
    }
    ENEMY_RETURN
}

/// A music was found for enemy kind `t`: ops stop the close music and lower kinds, then play.
pub fn enemy_found(e: &mut Enemy, t: usize, o: &mut Ops) {
    if e.close_playing != 0 {
        e.close_playing = 0;
        o.k(k::STOP_CLOSE);
    }
    for i in 0..t {
        if let Some(p) = e.playing.get_mut(i) {
            *p = 0;
        }
        o.ki(k::STOP_LOWER, 8 + t as i32);
    }
    o.ki(k::PLAY_ENEMY, 8 + t as i32);
    if let Some(p) = e.playing.get_mut(t) {
        *p = 1;
    }
    if let Some(c) = e.play_count.get_mut(t) {
        *c = 0.0;
    }
}
