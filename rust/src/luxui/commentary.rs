//! `cLuxCommentaryIcon`: spin state, interact toggle and focus text. Entity, mesh, particle and
//! sound-effect handling stay C++.

use crate::math::{PI, wrap};
use crate::string;

pub const ROT_SPEED: f32 = 2.5;

pub const PS_NONE: i32 = 0;
pub const PS_CREATE: i32 = 1;
pub const PS_KILL: i32 = 2;

/// New Y rotation after `dt`.
pub fn spin(angle_y: f32, playing: bool, dt: f32) -> f32 {
    let mut y = angle_y;
    if playing {
        y = dt.mul_add(ROT_SPEED, y);
    } else if y != 0.0 {
        y = wrap(y, 0.0, PI);
        y = dt.mul_add(ROT_SPEED, y);
        if y > PI {
            y = 0.0;
        }
    }
    y
}

/// What `SetPlayingSound` does to the particle system for a state change.
pub fn set_playing(current: bool, new: bool) -> i32 {
    if current == new {
        PS_NONE
    } else if new {
        PS_CREATE
    } else {
        PS_KILL
    }
}

pub fn focus_text(talker: &[u8], topic: &[u8]) -> Vec<u8> {
    let mut s = talker.to_vec();
    s.extend_from_slice(b"\n\"");
    s.extend_from_slice(topic);
    s.push(b'"');
    s
}

pub fn comment_file(file: &[u8]) -> Vec<u8> {
    string::set_file_ext(file, b"comment")
}
