//! `cLuxPlayerSanity`: low-sanity timer, hit zoom, enemy-seen effect, losing-sanity sounds and
//! insanity visuals. The enemy/critter scan and sound/effect calls stay C++.

use super::hw;
use super::ops::Ops;
use crate::math::sin;

pub mod k {
    pub const PLAYER_SET_SANITY: u32 = 200;
    pub const LOWER_SANITY: u32 = 201;
    pub const ENEMY_UNSEEN_BLUR: u32 = 202;
    pub const HINT_ENEMY_SEEN: u32 = 203;
    pub const BLUR_ENEMY: u32 = 204;
    pub const IMAGE_TRAIL: u32 = 205;
    pub const FADE_ASPECT: u32 = 206;
    pub const FADE_FOV: u32 = 207;
    pub const INSANITY_ACTIVE: u32 = 208;
    pub const INSANITY_FX: u32 = 209;
    pub const PLAY_HEARTBEAT: u32 = 210;
    pub const PLAY_DRAIN: u32 = 211;
    pub const HINT_SANITY_LOW: u32 = 212;
    pub const RAND_PANT: u32 = 213;
    pub const PLAY_PANT: u32 = 214;
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Sanity {
    pub hit_alpha: f32,
    pub hit_active: u32,
    pub sanity_lost_count: f32,
    pub at_low_sanity_count: f32,
    pub hit_is_updated: u32,
    pub sanity_lost_is_updated: u32,
    pub pant_count: f32,
    pub check_enemy_seen_count: f32,
    pub sanity_effect_updated: u32,
    pub t: f32,
    pub insane_wave_alpha: f32,
    pub sanity_drain_count: f32,
    pub sanity_drain_volume: f32,
    pub sanity_heartbeat_count: f32,
    pub seen_enemy_count: f32,
    pub enemy_is_seen: u32,
    pub show_hint_timer: f32,
    pub hit_zoom_in_speed: f32,
    pub hit_zoom_out_speed: f32,
    pub hit_zoom_in_fov_mul: f32,
    pub hit_zoom_in_aspect_mul: f32,
    pub sanity_regain_speed: f32,
    pub sanity_regain_limit: f32,
    pub sanity_very_low_limit: f32,
    pub sanity_effects_start: f32,
    pub sanity_wave_alpha_mul: f32,
    pub sanity_wave_speed_mul: f32,
    pub sanity_low_limit: f32,
    pub sanity_low_limit_max_time: f32,
    pub sanity_low_new_sanity_amount: f32,
    pub check_near_enemy_interval: f32,
}

/// Start of `Update`: time, flag reset and the low-sanity timer.
pub fn sanity_begin(s: &mut Sanity, sanity: f32, dt: f32, o: &mut Ops) {
    s.t += dt;
    s.hit_is_updated = 0;
    s.sanity_lost_is_updated = 0;
    if sanity < s.sanity_low_limit {
        s.at_low_sanity_count += dt;
        if s.at_low_sanity_count > s.sanity_low_limit_max_time {
            s.at_low_sanity_count = 0.0;
            o.kf(k::PLAYER_SET_SANITY, s.sanity_low_new_sanity_amount);
        }
    } else if s.at_low_sanity_count > 0.0 {
        s.at_low_sanity_count -= dt;
        if s.at_low_sanity_count < 0.0 {
            s.at_low_sanity_count = 0.0;
        }
    }
}

/// `UpdateCheckEnemySeen` timer: true when the scan is due.
pub fn sanity_scan_due(s: &mut Sanity, dt: f32) -> bool {
    if s.check_enemy_seen_count > 0.0 {
        s.check_enemy_seen_count -= dt;
        return false;
    }
    s.check_enemy_seen_count = s.check_near_enemy_interval;
    true
}

/// End of `UpdateCheckEnemySeen` once the scan found `seen_enemy`.
pub fn sanity_scan_result(s: &mut Sanity, seen_enemy: bool, enemy_decrease: f32, o: &mut Ops) {
    if seen_enemy {
        o.kf(k::LOWER_SANITY, enemy_decrease);
        s.enemy_is_seen = 1;
    } else if s.enemy_is_seen != 0 {
        s.enemy_is_seen = 0;
        o.kf(k::ENEMY_UNSEEN_BLUR, 0.12 / 2.0);
    }
}

pub fn sanity_enemy_seen_effect(s: &mut Sanity, dt: f32, o: &mut Ops) {
    if s.enemy_is_seen != 0 {
        if s.seen_enemy_count < 1.0 {
            s.seen_enemy_count = dt.mul_add(0.3, s.seen_enemy_count);
            if s.seen_enemy_count > 1.0 {
                s.seen_enemy_count = 1.0;
                o.k(k::HINT_ENEMY_SEEN);
            }
        }
        let pulse = sin(s.t * 2.5).mul_add(0.5, 0.5).mul_add(0.5, 0.5);
        o.kff(k::BLUR_ENEMY, 0.2, 0.12 * s.seen_enemy_count * pulse);
    } else if s.seen_enemy_count > 0.0 {
        s.seen_enemy_count = hw::msub(dt, 0.15, s.seen_enemy_count);
        if s.seen_enemy_count < 0.0 {
            s.seen_enemy_count = 0.0;
        }
    }
}

pub fn sanity_update_hit(s: &mut Sanity, dead: bool, dt: f32, o: &mut Ops) {
    if dead {
        return;
    }
    if s.hit_alpha <= 0.0 && s.hit_active == 0 {
        return;
    }
    s.hit_is_updated = 1;
    if s.hit_active != 0 {
        s.hit_alpha = dt.mul_add(s.hit_zoom_in_speed, s.hit_alpha);
        if s.hit_alpha >= 1.0 {
            s.hit_alpha = 1.0;
            s.hit_active = 0;
        }
    } else {
        s.hit_alpha = hw::msub(s.hit_zoom_out_speed, dt, s.hit_alpha);
        if s.hit_alpha < 0.0 {
            s.hit_alpha = 0.0;
        }
    }
    o.kff(k::IMAGE_TRAIL, s.hit_alpha * 0.9, 100.0);
    o.kff(
        k::FADE_ASPECT,
        hw::msub(s.hit_alpha, s.hit_zoom_in_aspect_mul, 1.0),
        100.0,
    );
    o.kff(
        k::FADE_FOV,
        hw::msub(s.hit_alpha, s.hit_zoom_in_fov_mul, 1.0),
        100.0,
    );
    s.sanity_effect_updated = 1;
}

fn insanity_visuals(s: &mut Sanity, sanity: f32, dt: f32, o: &mut Ops) {
    if sanity > s.sanity_effects_start && s.sanity_drain_volume <= 0.0 && s.insane_wave_alpha <= 0.0
    {
        o.ki(k::INSANITY_ACTIVE, 0);
        return;
    }
    o.ki(k::INSANITY_ACTIVE, 1);
    let mut goal = 1.0 - sanity / s.sanity_effects_start;
    if goal < 0.0 {
        goal = 0.0;
    }
    if goal < s.insane_wave_alpha {
        s.insane_wave_alpha -= dt;
        if s.insane_wave_alpha < goal {
            s.insane_wave_alpha = goal;
        }
    } else {
        s.insane_wave_alpha += dt;
        if s.insane_wave_alpha > goal {
            s.insane_wave_alpha = goal;
        }
    }
    let zoom_mul = ((sin(s.t * 2.0) + 1.0) * 0.5).mul_add(0.4, 0.6);
    o.kfff(
        k::INSANITY_FX,
        s.insane_wave_alpha * s.sanity_wave_alpha_mul,
        s.insane_wave_alpha * s.sanity_wave_speed_mul,
        hw::mul(zoom_mul, s.sanity_drain_volume),
    );
}

fn losing_sanity(s: &mut Sanity, sanity: f32, dead: bool, dt: f32, o: &mut Ops) {
    if s.sanity_lost_count <= 0.0 {
        s.sanity_drain_count = 0.0;
        s.sanity_heartbeat_count = 0.0;
        s.sanity_drain_volume = hw::msub(dt, 0.5, s.sanity_drain_volume);
        if s.sanity_drain_volume < 0.0 {
            s.sanity_drain_volume = 0.0;
        }
        return;
    }
    let norm = sanity / 100.0;
    s.sanity_lost_is_updated = 1;
    s.sanity_lost_count -= dt;
    s.sanity_drain_volume = dt.mul_add(0.1, s.sanity_drain_volume);
    if s.sanity_drain_volume > 1.0 {
        s.sanity_drain_volume = 1.0;
    }
    let speed_mul = (1.0 - norm).mul_add(2.0, 1.0);
    s.sanity_heartbeat_count = (dt * speed_mul).mul_add(0.1, s.sanity_heartbeat_count);
    if s.sanity_heartbeat_count >= 1.0 {
        s.sanity_heartbeat_count = 0.0;
        let vol = hw::msub(norm, 0.5, 1.0) * s.sanity_drain_volume;
        if !dead {
            o.kf(k::PLAY_HEARTBEAT, vol);
        }
    }
    s.sanity_drain_count = (dt * speed_mul).mul_add(0.33, s.sanity_drain_count);
    if s.sanity_drain_count >= 1.0 {
        s.sanity_drain_count = 0.0;
        let kind = if sanity > 75.0 {
            0
        } else if sanity > 50.0 {
            1
        } else {
            2
        };
        if !dead {
            o.push(k::PLAY_DRAIN, kind, s.sanity_drain_volume, 0.0, 0.0, 0.0);
        }
    }
}

fn low_sanity(s: &mut Sanity, sanity: f32, dead: bool, dt: f32, o: &mut Ops) {
    if sanity > s.sanity_very_low_limit {
        return;
    }
    if s.show_hint_timer <= 0.0 {
        s.show_hint_timer = 3.0;
        o.k(k::HINT_SANITY_LOW);
    } else {
        s.show_hint_timer -= dt;
    }
    if s.sanity_lost_is_updated == 0 {
        o.kff(k::IMAGE_TRAIL, 1.6, 3.0);
        s.sanity_effect_updated = 1;
    }
    if s.pant_count < 0.0 {
        o.kff(k::RAND_PANT, 0.5, 5.0);
        if !dead {
            o.k(k::PLAY_PANT);
        }
    } else {
        s.pant_count -= dt;
    }
}

/// `UpdateInsaneEffects`. `RAND_PANT` carries the draw bounds: C++ draws in replay order and
/// stores the result in `pant_count`.
pub fn sanity_insane_effects(s: &mut Sanity, sanity: f32, dead: bool, dt: f32, o: &mut Ops) {
    if s.hit_is_updated != 0 {
        return;
    }
    if s.sanity_effect_updated != 0 {
        o.kff(k::IMAGE_TRAIL, 0.0, 1.0);
        o.kff(k::FADE_ASPECT, 1.0, 1.0);
        o.kff(k::FADE_FOV, 1.0, 1.0);
        s.sanity_effect_updated = 0;
    }
    insanity_visuals(s, sanity, dt, o);
    if dead {
        return;
    }
    losing_sanity(s, sanity, dead, dt, o);
    low_sanity(s, sanity, dead, dt, o);
    if sanity < s.sanity_regain_limit {
        o.kf(
            k::PLAYER_SET_SANITY,
            dt.mul_add(s.sanity_regain_speed, sanity),
        );
    }
}
