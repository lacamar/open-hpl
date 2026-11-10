//! `cSoundHandler::Play`: the distance priority and the channel set-up sequence.

use super::channel::Channel;
use crate::math::{fma, matrix_mul_vec};
use crate::types::{Matrixf, Vector3f};

pub const ENTRY_TYPE_WORLD: u32 = 1;

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct PlayParams {
    pub pos: Vector3f,
    pub min_dist: f32,
    pub max_dist: f32,
    pub priority_modifier: i32,
    pub entry_type: u32,
    pub looping: bool,
    pub relative: bool,
    pub is_3d: bool,
    pub silent: bool,
}

fn dist(a: Vector3f, b: Vector3f) -> f32 {
    let dx = b.x - a.x;
    let dy = b.y - a.y;
    let dz = b.z - a.z;
    fma(dz, dz, fma(dx, dx, dy * dy)).sqrt()
}

pub fn distance_priority(
    pos: Vector3f,
    listener: Vector3f,
    min_dist: f32,
    max_dist: f32,
    is_3d: bool,
    relative: bool,
) -> i32 {
    if !is_3d || relative {
        return 0;
    }
    let d = dist(pos, listener);
    if d >= max_dist {
        0
    } else if d >= min_dist {
        10
    } else {
        100
    }
}

pub(super) fn setup_channel<C: Channel>(
    ch: &mut C,
    p: &PlayParams,
    dist_priority: i32,
    listener: &Matrixf,
) {
    ch.set_looping(p.looping);
    ch.set_min_distance(p.min_dist);
    ch.set_max_distance(p.max_dist);
    ch.set_3d(p.is_3d);
    if p.is_3d && !p.relative {
        ch.set_priority(dist_priority);
        ch.set_priority_modifier(p.priority_modifier);
    } else {
        ch.set_priority(p.priority_modifier);
    }

    if p.entry_type == ENTRY_TYPE_WORLD {
        ch.set_affected_by_env(true);
    }

    ch.set_position_is_relative(p.relative);
    if p.relative {
        ch.set_rel_position(p.pos);
        ch.set_position(matrix_mul_vec(listener, p.pos));
    } else {
        ch.set_position(p.pos);
    }

    ch.set_volume(0.0);

    if p.silent {
        ch.set_looping(false);
        ch.stop();
    }
}
