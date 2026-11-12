//! `cLuxMainMenu_StartGame`, `_LoadGame` and `_CustomStory(List)`: selection, sort and layout rules.

use super::menu::{WIN_CUSTOM, WIN_LAST};
use crate::luxsave::names::Date;
use std::cmp::Ordering;

/// Window to show after the OK result of an exit popup; None when cancelled.
pub fn exit_target(custom_story_active: bool, ok: bool) -> Option<i32> {
    if !ok {
        return None;
    }
    Some(if custom_story_active {
        WIN_CUSTOM
    } else {
        WIN_LAST
    })
}

/// `switch(aData.mlVal)` on the two UI buttons: 0 neither, 1 primary, 2 secondary.
pub fn ui_switch(val: i32) -> i32 {
    match val {
        1 => 1,
        2 => 2,
        _ => 0,
    }
}

/// `PressNormalMode`/`PressHardMode`: (normal button gold, hard button gold).
pub fn mode_gold(hard: bool) -> (bool, bool) {
    (!hard, hard)
}

/// Saved game files newest first (`multimap<cDate, _, greater>`: equal dates keep file order).
pub fn sort_saves(dates: &[Date]) -> Vec<usize> {
    let mut idx: Vec<usize> = (0..dates.len()).collect();
    idx.sort_by(|&a, &b| {
        let (Some(da), Some(db)) = (dates.get(a), dates.get(b)) else {
            return Ordering::Equal;
        };
        if da.after(db) {
            Ordering::Less
        } else if db.after(da) {
            Ordering::Greater
        } else {
            Ordering::Equal
        }
    });
    idx
}

/// Entry to load for the selected row; C++ indexed the file list unchecked.
pub fn load_index(selected: i32, count: usize) -> Option<usize> {
    usize::try_from(selected).ok().filter(|&i| i < count)
}

pub const PRESS_NO_SELECTION: i32 = 0;
pub const PRESS_SELECTED: i32 = 1;

pub fn press_ok(selected: i32) -> i32 {
    if selected < 0 {
        PRESS_NO_SELECTION
    } else {
        PRESS_SELECTED
    }
}

/// Custom story window button width: widest label plus 20.
pub fn button_width(lengths: &[f32]) -> f32 {
    let mut w = 0.0f32;
    for l in lengths {
        if w < *l {
            w = *l;
        }
    }
    w + 20.0
}

/// Buttons 0 and 2 need a saved game. Fills `active` and `y`, returns the count.
pub fn story_buttons(
    save_exists: bool,
    start_y: f32,
    heights: &[f32],
    active: &mut [bool],
    y: &mut [f32],
) -> usize {
    let mut pos = start_y;
    let mut n = 0;
    for (i, h) in heights.iter().enumerate() {
        let on = !matches!(i, 0 | 2) || save_exists;
        if let Some(a) = active.get_mut(i) {
            *a = on;
        }
        n += 1;
        if on {
            if let Some(d) = y.get_mut(i) {
                *d = pos;
            }
            pos += *h + 3.0 * 2.0;
        }
    }
    n
}

pub const FB_NAME: u32 = 1;
pub const FB_AUTHOR: u32 = 2;
pub const FB_DESC: u32 = 4;

pub fn story_fallbacks(name_empty: bool, author_empty: bool, desc_empty: bool) -> u32 {
    u32::from(name_empty) * FB_NAME
        + u32::from(author_empty) * FB_AUTHOR
        + u32::from(desc_empty) * FB_DESC
}

pub const FOLDER_CUSTOM: u32 = 1;
pub const FOLDER_PROFILE: u32 = 2;

/// `PressStart`: the profile folder is only checked when the custom folder already exists.
pub fn start_folders(custom_exists: bool, profile_exists: bool) -> u32 {
    if !custom_exists {
        FOLDER_CUSTOM
    } else if !profile_exists {
        FOLDER_PROFILE
    } else {
        0
    }
}
