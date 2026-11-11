//! `cSurfaceData` impact, slide and roll effects.

use super::body::rotation;
use super::host::{H, Host, Str};
use crate::math::{fma, matrix_mul_vec};
use crate::types::Vector3f;

pub const ROLL_AXIS_X: u32 = 1;
pub const ROLL_AXIS_Y: u32 = 2;
pub const ROLL_AXIS_Z: u32 = 4;

#[repr(C)]
#[derive(Clone, Copy)]
pub struct SurfaceParams {
    pub this: H,
    pub physics: H,
    pub min_scrape_speed: f32,
    pub min_scrape_freq: f32,
    pub min_scrape_freq_speed: f32,
    pub max_scrape_freq: f32,
    pub max_scrape_freq_speed: f32,
    pub middle_scrape_speed: f32,
    pub min_scrape_contacts: i32,
    pub min_roll_speed: f32,
    pub min_roll_freq: f32,
    pub min_roll_volume: f32,
    pub min_roll_freq_speed: f32,
    pub max_roll_freq: f32,
    pub max_roll_volume: f32,
    pub max_roll_freq_speed: f32,
    pub middle_roll_speed: f32,
    pub roll_axis_flags: u32,
    pub scrape_sound: Str,
    pub roll_sound: Str,
}

fn max(a: f32, b: f32) -> f32 {
    if a > b { a } else { b }
}

fn min(a: f32, b: f32) -> f32 {
    if a < b { a } else { b }
}

fn lerp_to(t: f32, to: f32) -> f32 {
    fma(t, to, 1.0 - t)
}

/// Pitch for a scrape at `abs_speed`.
pub fn scrape_freq(p: &SurfaceParams, abs_speed: f32) -> f32 {
    if abs_speed >= p.middle_scrape_speed {
        if abs_speed >= p.max_scrape_freq_speed {
            p.max_scrape_freq
        } else {
            let t = (abs_speed - p.middle_scrape_speed)
                / (p.max_scrape_freq_speed - p.middle_scrape_speed);
            lerp_to(t, p.max_scrape_freq)
        }
    } else if abs_speed <= p.min_scrape_freq_speed {
        p.min_scrape_freq
    } else {
        let t =
            (p.middle_scrape_speed - abs_speed) / (p.middle_scrape_speed - p.min_scrape_freq_speed);
        lerp_to(t, p.min_scrape_freq)
    }
}

/// Pitch and volume for a roll at `abs_speed`.
pub fn roll_freq_volume(p: &SurfaceParams, abs_speed: f32) -> (f32, f32) {
    if abs_speed >= p.middle_roll_speed {
        if abs_speed >= p.max_roll_freq_speed {
            (p.max_roll_freq, p.max_roll_volume)
        } else {
            let t =
                (abs_speed - p.middle_roll_speed) / (p.max_roll_freq_speed - p.middle_roll_speed);
            (lerp_to(t, p.max_roll_freq), lerp_to(t, p.max_roll_volume))
        }
    } else if abs_speed <= p.min_roll_freq_speed {
        (p.min_roll_freq, p.min_roll_volume)
    } else {
        let t = (p.middle_roll_speed - abs_speed) / (p.middle_roll_speed - p.min_roll_freq_speed);
        (lerp_to(t, p.min_roll_freq), lerp_to(t, p.min_roll_volume))
    }
}

fn first_at_or_below<Ho: Host>(h: &Ho, s: H, speed: f32) -> Option<i32> {
    (0..h.surf_impact_count(s)).find(|&i| h.surf_impact_min_speed(s, i) <= speed)
}

pub fn on_impact<Ho: Host>(h: &Ho, p: &SurfaceParams, speed: f32, pos: Vector3f, body: H) {
    if !h.can_play_impact(p.physics) {
        return;
    }
    let world = h.body_world_world(body);
    if world == 0 {
        return;
    }
    let Some(idx) = first_at_or_below(h, p.this, speed) else {
        return;
    };
    // SAFETY: the host returns a string that stays valid for the call.
    let sound = unsafe { h.surf_impact_sound(p.this, idx).bytes() };
    if !sound.is_empty() {
        h.add_impact(p.physics);
        let e = h.world_create_sound(world, Str::new(b"Impact"), Str::new(sound));
        if e != 0 {
            h.ent_set_saved_false(e);
            h.ent_set_position(e, pos);
            h.ent_set_param_named(e, Str::new(b"mass"), h.body_mass(body));
            h.ent_set_param_named(e, Str::new(b"speed"), speed);
        }
    }
}

pub fn on_slide<Ho: Host>(
    h: &Ho,
    p: &SurfaceParams,
    speed: f32,
    pos: Vector3f,
    contacts: i32,
    body: H,
    against: H,
) {
    let mut speed = speed;
    if contacts < p.min_scrape_contacts {
        return;
    }
    let scrape_body = h.body_scrape_body(body);
    if scrape_body != 0 && against != scrape_body {
        return;
    }
    let world = h.body_world_world(body);
    if world == 0 {
        return;
    }
    if h.world_silent(world) {
        return;
    }

    let entity = h.body_scrape_entity(body);
    if entity != 0 && !h.world_sound_exists(world, entity, h.body_scrape_entity_id(body)) {
        h.body_set_scrape_entity(body, 0);
    }

    if h.body_mass(body) != 0.0 {
        if h.body_prev_scrape_matrix(body) == h.body_local_matrix(body) {
            speed = 0.0;
        }
        h.body_set_prev_scrape_matrix(body, &h.body_local_matrix(body));
    }

    let entity = h.body_scrape_entity(body);
    if entity != 0 {
        let fmin = max(p.min_scrape_speed - 0.7, 0.02);
        if speed.abs() < fmin {
            h.ent_fade_out(entity, 4.3);
            h.body_set_scrape_entity(body, 0);
            h.body_set_scrape_body(body, 0);
        } else {
            h.body_set_has_slide(body, true);
            let freq = scrape_freq(p, speed.abs());
            h.ent_set_param_index(entity, 0, speed);
            let entry = h.ent_main_entry(entity);
            if entry != 0 {
                h.entry_set_speed_mul(entry, freq);
                h.ent_set_position(entity, pos);
            }
        }
    } else if p.min_scrape_speed <= speed.abs() && p.scrape_sound.n != 0 {
        h.body_set_has_slide(body, true);
        let e = h.world_create_sound(world, Str::new(b"Scrape"), p.scrape_sound);
        if e != 0 {
            h.ent_fade_in(e, 3.3);
            h.ent_set_position(e, pos);
            h.ent_set_param_index(e, 0, speed);
            h.ent_set_saved_false(e);
            h.body_set_scrape_entity(body, e);
            h.body_set_scrape_body(body, against);
        }
    }

    if h.body_mass(body) == 0.0 && h.body_has_slide(body) && !h.body_in_update_list(body) {
        h.body_add_to_update_list(body);
    }
}

pub fn create_impact_effect<Ho: Host>(
    h: &Ho,
    p: &SurfaceParams,
    speed: f32,
    pos: Vector3f,
    second: H,
    physics_world: H,
) {
    if speed == 0.0 {
        return;
    }
    let world = h.pworld_world(physics_world);
    if world == 0 {
        return;
    }
    if h.world_silent(world) {
        return;
    }

    let a = first_at_or_below(h, p.this, speed);
    let b = if second != p.this && second != 0 {
        first_at_or_below(h, second, speed)
    } else {
        None
    };

    let ps = match (a, b) {
        (Some(a), None) => h.surf_impact_ps(p.this, a),
        (None, Some(b)) => h.surf_impact_ps(second, b),
        (Some(a), Some(b)) => {
            if h.surf_impact_ps_prio(p.this, a) >= h.surf_impact_ps_prio(second, b) {
                h.surf_impact_ps(p.this, a)
            } else {
                h.surf_impact_ps(second, b)
            }
        }
        (None, None) => return,
    };
    if ps.n != 0 {
        h.world_create_impact_ps(world, ps, pos);
    }
}

pub fn update_roll_effect<Ho: Host>(h: &Ho, p: &SurfaceParams, body: H) {
    if !h.body_use_surface_effects(body) {
        return;
    }
    if p.roll_sound.n == 0 || p.roll_axis_flags == 0 {
        return;
    }

    let ang = matrix_mul_vec(
        &rotation(&h.body_local_matrix(body)),
        h.body_angular_velocity(body),
    );
    let mut rolling = 0.0f32;
    if p.roll_axis_flags & ROLL_AXIS_X != 0 {
        rolling = ang.x.abs();
    }
    if p.roll_axis_flags & ROLL_AXIS_Y != 0 && rolling < ang.y.abs() {
        rolling = ang.y.abs();
    }
    if p.roll_axis_flags & ROLL_AXIS_Z != 0 && rolling < ang.z.abs() {
        rolling = ang.z.abs();
    }

    if rolling == 0.0 && h.body_roll_entity(body) == 0 {
        return;
    }

    let world = h.body_world_world(body);
    if world == 0 {
        return;
    }
    if h.world_silent(world) {
        return;
    }

    if !h.world_sound_exists(world, h.body_roll_entity(body), h.body_roll_entity_id(body)) {
        h.body_set_roll_entity(body, 0);
    }

    let entity = h.body_roll_entity(body);
    if entity != 0 {
        let fmin = max(p.min_roll_speed - 0.7, 0.02);
        if rolling < fmin || !h.body_has_collision(body) {
            h.ent_fade_out(entity, 4.3);
            h.body_set_roll_entity(body, 0);
        } else {
            let (freq, volume) = roll_freq_volume(p, rolling);
            h.ent_set_param_index(entity, 0, rolling);
            let entry = h.ent_main_entry(entity);
            if entry != 0 {
                h.entry_set_speed_mul(entry, freq);
                h.entry_fade_volume_mul_to(entry, min(volume * h.ent_volume(entity), 1.0), 4.0);
                let target = h.body_roll_entity(body);
                h.ent_set_position(target, h.body_world_position(body));
            }
        }
    } else if p.min_roll_speed <= rolling && h.body_has_collision(body) {
        let e = h.world_create_sound(world, Str::new(b"Roll"), p.roll_sound);
        if e != 0 {
            h.ent_fade_in(e, 3.3);
            h.ent_set_position(e, h.body_world_position(body));
            h.ent_set_param_index(e, 0, rolling);
            h.ent_set_saved_false(e);
            h.body_set_roll_entity(body, e);
        }
    }
}
