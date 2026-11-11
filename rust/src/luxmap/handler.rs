//! `LuxMapHandler.cpp`: map-change and current-map plans, pause bookkeeping.

use crate::string;

/// `FileToMapName`: lowercase file name without extension.
pub fn map_name(file: &[u8]) -> Vec<u8> {
    string::to_lower(&string::set_file_ext(string::file_name(file), b""))
}

/// `ChangeMap`: the map file always gets the `map` extension.
pub fn change_file(name: &[u8]) -> Vec<u8> {
    string::set_file_ext(name, b"map")
}

/// `CheckMapChange` entry: waits for the fade, then clears the request.
pub fn change_proceeds(active: bool, fading: bool) -> bool {
    active && !fading
}

/// The requested map differs from the current one.
pub fn is_new_map(current: &[u8], file: &[u8]) -> bool {
    current != map_name(file).as_slice()
}

pub const EVENT_ESCAPE_ARTIST: i32 = 1;
pub const EVENT_DESCENDANT: i32 = 2;
pub const EVENT_SANITY: i32 = 4;

/// Achievements and the hard-mode sanity refill tied to a specific map pair.
pub fn pair_events(last: &[u8], new: &[u8], hard_mode: bool) -> i32 {
    let mut e = 0;
    if last == b"08_cellar_maze" && new == b"09_back_hall" {
        e |= EVENT_ESCAPE_ARTIST;
    }
    if last == b"14_elevator" && new == b"15_prison_south" {
        e |= EVENT_DESCENDANT;
    }
    if hard_mode && last == b"27_torture_chancel_redux" && new == b"28_inner_sanctum" {
        e |= EVENT_SANITY;
    }
    e
}

pub fn load_seconds(start_ms: u64, now_ms: u64) -> f32 {
    now_ms.wrapping_sub(start_ms) as f32 / 1000.0
}

/// A map without saved data is entered for the first time.
pub fn first_time(saved_exists: bool) -> bool {
    !saved_exists
}

pub const SET_LEAVE: u8 = 1;
pub const SET_DESTROY_WORLD_ENTITIES: u8 = 2;
pub const SET_MAP_LEAVE: u8 = 3;
pub const SET_ASSIGN: u8 = 4;
pub const SET_CREATE_WORLD_ENTITIES: u8 = 5;
pub const SET_MAP_ENTER: u8 = 6;
pub const SET_PLACE_PLAYER: u8 = 7;
pub const SET_AUTO_CHECKPOINT: u8 = 8;
pub const SET_ENTER: u8 = 9;
pub const SET_VIEW_WORLD: u8 = 10;
pub const SET_VIEW_NO_WORLD: u8 = 11;

/// `SetCurrentMap`: the calls, in order. `SET_ASSIGN` is `mpCurrentMap = apMap`.
pub fn set_current_plan(has_current: bool, has_new: bool, same: bool) -> Vec<u8> {
    if same {
        return Vec::new();
    }
    let mut p = Vec::new();
    if has_current {
        p.extend([SET_LEAVE, SET_DESTROY_WORLD_ENTITIES, SET_MAP_LEAVE]);
    }
    p.push(SET_ASSIGN);
    if has_new {
        p.extend([
            SET_CREATE_WORLD_ENTITIES,
            SET_MAP_ENTER,
            SET_PLACE_PLAYER,
            SET_AUTO_CHECKPOINT,
            SET_ENTER,
            SET_VIEW_WORLD,
        ]);
    } else {
        p.push(SET_VIEW_NO_WORLD);
    }
    p
}

/// `PauseSoundsAndMusic`: true when the pause has to be applied.
pub fn pause_sounds(paused: &mut bool) -> bool {
    if *paused {
        return false;
    }
    *paused = true;
    true
}

/// `ResumeSoundsAndMusic`: true when the resume has to be applied.
pub fn resume_sounds(paused: &mut bool) -> bool {
    let was = *paused;
    *paused = false;
    was
}

/// `SetShowCommentary(false)` stops the commentary that is playing.
pub fn commentary_stops(show: bool) -> bool {
    !show
}
