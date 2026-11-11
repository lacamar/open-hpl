//! `LuxPlayerHelpers.cpp`: insanity collapse, camera sway, hurt, look-at, flashback, lantern, light
//! level, darkness and death.

use super::hw;
use super::ops::Ops;
use crate::math::{clamp, rand_rectl, to_rad, v3, wrap};
use crate::types::{Vector2f, Vector3f};

pub mod k {
    pub const SET_HEAD_ADD: u32 = 30;
    pub const FADE_ROLL: u32 = 31;
    pub const SET_COLLAPSE_SPEED_MUL: u32 = 32;
    pub const SET_CROUCH_DISABLED: u32 = 33;
    pub const SET_JUMP_DISABLED: u32 = 34;
    pub const PLAY_RANDOM_SOUND: u32 = 35;
    pub const RAND_COUNT: u32 = 36;
    pub const WAKE_FX: u32 = 37;
    pub const LOOP_FADE_OUT: u32 = 38;
    pub const PLAYER_SET_SANITY: u32 = 39;
    pub const PLAY_AWAKEN: u32 = 45;

    pub const LIST_CLEAR: u32 = 40;
    pub const LIST_PUSH: u32 = 41;
    pub const LIST_POP_FRONT: u32 = 42;

    pub const SET_HURT_MOVE_SPEED_MUL: u32 = 50;
    pub const PLAY_PANT: u32 = 51;
    pub const SET_HURT_HEAD_ADD: u32 = 52;

    pub const ADD_PITCH: u32 = 60;
    pub const ADD_YAW: u32 = 61;
    pub const SYNC_BODY_YAW: u32 = 62;
    pub const AT_TARGET: u32 = 63;

    pub const FLASH_MID: u32 = 70;
    pub const FLASHBACK_BEGIN: u32 = 71;
    pub const FLASHBACK_END: u32 = 72;
    pub const FLASHBACK_START_QUEUED: u32 = 73;

    pub const LANTERN_COLOR: u32 = 100;
    pub const LANTERN_OUT_OF_OIL: u32 = 101;
    pub const LANTERN_SET_ACTIVE: u32 = 102;
    pub const LANTERN_HINT_NO_ITEM: u32 = 103;
    pub const LANTERN_HINT_NO_OIL: u32 = 104;
    pub const LANTERN_SOUND_OUT: u32 = 105;
    pub const LANTERN_SOUND_ON: u32 = 106;
    pub const LANTERN_SOUND_OFF: u32 = 107;
    pub const LANTERN_HAND: u32 = 108;
    pub const LANTERN_CALLBACK: u32 = 109;
    pub const LANTERN_SET_OIL: u32 = 110;

    pub const AMBIENT_OFF: u32 = 120;
    pub const AMBIENT_ON: u32 = 121;
    pub const LOWER_SANITY: u32 = 122;
    pub const HINT_DARKNESS: u32 = 123;

    pub const DEATH_PERMA_SOUND: u32 = 131;
    pub const DEATH_RESET_GAME: u32 = 132;
    pub const DEATH_FLASH: u32 = 133;
    pub const DEATH_AWAKEN_SOUND: u32 = 134;
    pub const DEATH_RESET: u32 = 135;
    pub const DEATH_AUTOLOAD: u32 = 136;
    pub const DEATH_EXIT: u32 = 137;
}

// Insanity collapse

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Collapse {
    pub active: u32,
    pub state: i32,
    pub height_add: f32,
    pub roll: f32,
    pub t: f32,
    pub random_count: f32,
    pub height_add_goal: f32,
    pub height_add_collapse_speed: f32,
    pub height_add_awake_speed: f32,
    pub roll_collapse_speed: f32,
    pub sleep_time: f32,
    pub wake_up_speed_mul: f32,
    pub awaken_sanity: f32,
    pub sleep_random_min: f32,
    pub sleep_random_max: f32,
}

pub fn collapse_update(s: &mut Collapse, dt: f32, o: &mut Ops) {
    if s.active == 0 {
        return;
    }
    if s.state == 0 || s.state == 1 {
        if s.height_add > s.height_add_goal {
            s.height_add = hw::msub(s.height_add_collapse_speed, dt, s.height_add);
            if s.height_add < s.height_add_goal {
                s.height_add = s.height_add_goal;
                s.state = 1;
            }
            o.kf(k::SET_HEAD_ADD, s.height_add);
        }
        s.roll = to_rad(s.roll_collapse_speed).mul_add(dt, s.roll);
        if s.roll > to_rad(35.0) {
            s.roll = to_rad(35.0);
        }
        o.kfff(k::FADE_ROLL, s.roll, 10.0, 10.0);
    }
    if s.state == 1 {
        s.t += dt;
        s.random_count -= dt;
        if s.random_count < 0.0 {
            o.k(k::PLAY_RANDOM_SOUND);
            o.kff(k::RAND_COUNT, s.sleep_random_min, s.sleep_random_max);
        }
        if s.t > s.sleep_time {
            o.k(k::WAKE_FX);
            o.kf(k::SET_COLLAPSE_SPEED_MUL, s.wake_up_speed_mul);
            o.kfff(k::FADE_ROLL, 0.0, 5.0, 6.0);
            o.k(k::LOOP_FADE_OUT);
            o.kf(k::PLAYER_SET_SANITY, s.awaken_sanity);
            o.k(k::PLAY_AWAKEN);
            s.state = 2;
        }
    } else if s.state == 2 && s.height_add < 0.0 {
        s.height_add = s.height_add_awake_speed.mul_add(dt, s.height_add);
        if s.height_add > 0.0 {
            s.height_add = 0.0;
            s.active = 0;
            o.kf(k::SET_COLLAPSE_SPEED_MUL, 1.0);
            o.ki(k::SET_CROUCH_DISABLED, 0);
            o.ki(k::SET_JUMP_DISABLED, 0);
        }
        o.kf(k::SET_HEAD_ADD, s.height_add);
    }
}

// Camera sway

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct CamDir {
    pub start_max_sanity: f32,
    pub sway_active: u32,
    pub sway_add: Vector2f,
    pub max_positions: i32,
    pub sway_alpha: f32,
    pub next_add: Vector2f,
}

pub fn camdir_update(s: &mut CamDir, list: &[Vector2f], sanity: f32, dt: f32, o: &mut Ops) {
    let mut v: Vec<Vector2f> = list.to_vec();
    if s.sway_active == 0 && sanity <= s.start_max_sanity {
        s.sway_active = 1;
    } else if s.sway_active != 0 && sanity > s.start_max_sanity {
        s.sway_active = 0;
    }
    if s.sway_active != 0 && s.sway_alpha < 1.0 {
        s.sway_alpha = dt.mul_add(0.1, s.sway_alpha);
        if s.sway_alpha > 1.0 {
            s.sway_alpha = 1.0;
        }
    } else if s.sway_active == 0 && s.sway_alpha > 0.0 {
        s.sway_alpha = hw::msub(dt, 0.2, s.sway_alpha);
        if s.sway_alpha < 0.0 {
            s.sway_alpha = 0.0;
            v.clear();
            o.k(k::LIST_CLEAR);
            s.next_add = Vector2f::default();
            s.sway_add = Vector2f::default();
        }
    }
    if s.sway_active == 0 && v.is_empty() && s.sway_alpha <= 0.0 {
        return;
    }
    v.push(s.next_add);
    o.push(k::LIST_PUSH, 0, s.next_add.x, s.next_add.y, 0.0, 0.0);
    s.next_add = Vector2f::default();
    if v.len() as i32 > s.max_positions {
        if !v.is_empty() {
            v.remove(0);
        }
        o.k(k::LIST_POP_FRONT);
    }
    let mut total = Vector2f::default();
    for e in &v {
        total.x += e.x;
        total.y += e.y;
    }
    let n = v.len() as f32;
    s.sway_add = Vector2f {
        x: total.x / n,
        y: total.y / n,
    };
}

/// `AddAndGetYawAdd` (`yaw`) and `AddAndGetPitchAdd`.
pub fn camdir_add_and_get(s: &mut CamDir, list_empty: bool, yaw: bool, x: f32) -> f32 {
    let sway = if yaw { s.sway_add.x } else { s.sway_add.y };
    let mut r = x;
    if !list_empty {
        r = sway.mul_add(s.sway_alpha, hw::mul(1.0 - s.sway_alpha, x));
    }
    if s.sway_alpha > 0.0 {
        if yaw {
            s.next_add.x = x;
        } else {
            s.next_add.y = x;
        }
    }
    r
}

// Hurt

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Hurt {
    pub effect_start_health: f32,
    pub min_move_mul: f32,
    pub max_pant_count: f32,
    pub pant_speed: f32,
    pub pant_size: f32,
    pub current_noise: i32,
    pub noise_update_count: f32,
    pub noise_freq: f32,
    pub pant_count: f32,
    pub pant_pos_add: f32,
    pub pant_pos_add_vel: f32,
    pub pant_pos_add_dir: f32,
    pub alpha: f32,
    pub noise_count: i32,
}

/// `Hurt::Update` after the health regain; `speed` is the body velocity length.
pub fn hurt_update(s: &mut Hurt, health: f32, dead: bool, speed: f32, dt: f32, o: &mut Ops) {
    if s.alpha <= 0.0 && health > s.effect_start_health {
        return;
    }
    if dead {
        return;
    }
    let wanted = (1.0 - (health / s.effect_start_health)).clamp(0.0, 1.0);
    if s.alpha < wanted {
        s.alpha += dt;
        if s.alpha > wanted {
            s.alpha = wanted;
        }
    } else if s.alpha > wanted {
        s.alpha -= dt;
        if s.alpha < wanted {
            s.alpha = wanted;
        }
    }
    o.kf(
        k::SET_HURT_MOVE_SPEED_MUL,
        s.min_move_mul.mul_add(s.alpha, 1.0 - s.alpha),
    );
    s.noise_update_count -= dt;
    if s.noise_update_count <= 0.0 {
        let max = s.noise_count - 1;
        s.current_noise = s.current_noise.wrapping_add(rand_rectl(1, max));
        if s.current_noise > max {
            s.current_noise = s.current_noise.wrapping_sub(max.wrapping_add(1));
        }
        s.noise_update_count = 1.0 / s.noise_freq;
    }
    if speed < 0.05 {
        if s.pant_count > 0.0 {
            s.pant_count -= dt;
        }
    } else {
        s.pant_count += dt;
        let max = s.max_pant_count * s.alpha;
        if s.pant_count > max {
            s.pant_count = max;
        }
    }
    if s.pant_count > 0.0 && speed < 0.05 {
        s.pant_pos_add_vel = s.pant_pos_add_dir.mul_add(dt, s.pant_pos_add_vel);
        s.pant_pos_add_vel = clamp(s.pant_pos_add_vel, -1.0, 1.0);
        s.pant_pos_add = (dt * s.pant_pos_add_vel).mul_add(s.pant_speed, s.pant_pos_add);
        if s.pant_pos_add_dir > 0.0 {
            if s.pant_pos_add > s.pant_size {
                s.pant_pos_add_dir = -s.pant_pos_add_dir;
                let a = s.alpha + 0.3;
                o.kf(k::PLAY_PANT, if a < 1.0 { a } else { 1.0 });
            }
        } else if s.pant_pos_add < -s.pant_size * 0.5 {
            s.pant_pos_add_dir = -s.pant_pos_add_dir;
        }
    } else if s.pant_pos_add != 0.0 {
        let dir = if s.pant_pos_add > 0.0 { -1.0f32 } else { 1.0 };
        s.pant_pos_add_vel = if dir < 0.0 {
            hw::add(-dt, s.pant_pos_add_vel)
        } else {
            dir.mul_add(dt, s.pant_pos_add_vel)
        };
        s.pant_pos_add_dir = 1.0;
        let max = (s.pant_pos_add / s.pant_size).abs();
        s.pant_pos_add_vel = clamp(s.pant_pos_add_vel, -max, max);
        s.pant_pos_add = (dt * s.pant_pos_add_vel).mul_add(s.pant_speed, s.pant_pos_add);
        if (dir > 0.0 && s.pant_pos_add > 0.0) || (dir < 0.0 && s.pant_pos_add < 0.0) {
            s.pant_pos_add = 0.0;
        }
    }
    o.kf(k::SET_HURT_HEAD_ADD, s.pant_pos_add);
}

/// The health regain that opens `Hurt::Update`: the amount to add, if any.
pub fn hurt_regain(health: f32, limit: f32, speed: f32, dt: f32) -> Option<f32> {
    (health < limit).then_some(speed * dt)
}

// Look-at

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct LookAt {
    pub current_speed: Vector3f,
    pub speed_mul: f32,
    pub max_speed: f32,
}

fn angle_dist_rad(a1: f32, a2: f32) -> f32 {
    let max = std::f32::consts::TAU;
    let a1 = wrap(a1, 0.0, max);
    let a2 = wrap(a2, 0.0, max);
    if a1 == a2 {
        return 0.0;
    }
    let d1 = a2 - a1;
    let mut d2 = max - d1.abs();
    if d1 > 0.0 {
        d2 = -d2;
    }
    if d1.abs() < d2.abs() { d1 } else { d2 }
}

fn fmin(a: f32, b: f32) -> f32 {
    if a < b { a } else { b }
}

/// `LookAt::Update`; `goal` is the `(pitch, yaw)` angle to the target from the camera. The
/// original left `z` of the wanted speed uninitialised; it is 0 here.
pub fn lookat_update(s: &mut LookAt, goal: Vector3f, pitch: f32, yaw: f32, dt: f32, o: &mut Ops) {
    let dx = angle_dist_rad(pitch, goal.x);
    let dy = angle_dist_rad(yaw, goal.y);
    let wx = fmin(dx * s.speed_mul, s.max_speed);
    let wy = fmin(dy * s.speed_mul, s.max_speed);
    let diff = v3(
        wx - s.current_speed.x,
        wy - s.current_speed.y,
        0.0 - s.current_speed.z,
    );
    s.current_speed.x = (diff.x * dt).mul_add(10.0, s.current_speed.x);
    s.current_speed.y = (diff.y * dt).mul_add(10.0, s.current_speed.y);
    s.current_speed.z = (diff.z * dt).mul_add(10.0, s.current_speed.z);
    o.kf(k::ADD_PITCH, s.current_speed.x * dt);
    o.kf(k::ADD_YAW, s.current_speed.y * dt);
    o.k(k::SYNC_BODY_YAW);
    let total = dx.mul_add(dx, dy * dy);
    if f64::from(total) < 0.01 {
        o.k(k::AT_TARGET);
    }
}

// Flashback

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Flashback {
    pub radial_blur_size: f32,
    pub radial_blur_start_dist: f32,
    pub move_speed_mul: f32,
    pub run_speed_mul: f32,
    pub flash_delay: f32,
    pub active: u32,
    pub start_count: f32,
}

/// `Flashback::Update`; `queued` is a non-empty queue, `voice_done` the voice player's state and
/// `has_callback` a non-empty callback.
pub fn flashback_update(
    s: &mut Flashback,
    queued: bool,
    voice_done: bool,
    has_callback: bool,
    dt: f32,
    o: &mut Ops,
) {
    if s.active == 0 {
        if queued {
            s.start_count -= dt;
            if s.start_count < 0.0 {
                o.k(k::FLASHBACK_START_QUEUED);
            }
        }
        return;
    }
    if s.flash_delay > 0.0 {
        s.flash_delay -= dt;
        if s.flash_delay < 0.0 {
            o.kff(k::FLASH_MID, s.move_speed_mul, s.run_speed_mul);
        }
    }
    if s.start_count > 0.0 {
        s.start_count -= dt;
        if s.start_count <= 0.0 {
            o.push(
                k::FLASHBACK_BEGIN,
                0,
                s.radial_blur_start_dist,
                s.radial_blur_size,
                s.radial_blur_size / 3.5,
                1.0 / 3.5,
            );
        }
    } else if voice_done {
        let fade = 4.0f32;
        o.push(
            k::FLASHBACK_END,
            i32::from(has_callback),
            1.0 / fade,
            s.radial_blur_size / fade,
            0.0,
            0.0,
        );
        s.active = 0;
        s.start_count = 6.0;
    }
}

// Lantern

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Lantern {
    pub color: [f32; 4],
    pub fade_light_oil_amount: f32,
    pub lower_oil_speed: f32,
    pub active: u32,
    pub alpha: f32,
}

/// `Lantern::Update` (the light matrix stays in C++).
pub fn lantern_update(
    s: &mut Lantern,
    oil: f32,
    emotion_flash: bool,
    dt: f32,
    o: &mut Ops,
) -> bool {
    if s.active == 0 && s.alpha <= 0.0 {
        return false;
    }
    if s.active != 0 {
        s.alpha += dt;
        if s.alpha > 1.0 {
            s.alpha = 1.0;
        }
    } else if s.alpha > 0.0 {
        s.alpha = hw::msub(dt, 2.0, s.alpha);
        if s.alpha < 0.0 {
            s.alpha = 0.0;
        }
    }
    let mut c = s.color;
    if oil < s.fade_light_oil_amount {
        let f = oil / s.fade_light_oil_amount;
        c = [c[0] * f, c[1] * f, c[2] * f, c[3] * f];
    }
    o.push(
        k::LANTERN_COLOR,
        0,
        c[0] * s.alpha,
        c[1] * s.alpha,
        c[2] * s.alpha,
        c[3] * s.alpha,
    );
    if s.active != 0 && !emotion_flash {
        let mut oil = hw::msub(s.lower_oil_speed, dt, oil);
        if oil <= 0.0 {
            oil = 0.0;
            o.k(k::LANTERN_OUT_OF_OIL);
        }
        o.kf(k::LANTERN_SET_OIL, oil);
    }
    true
}

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct LanternSet {
    pub active: bool,
    pub use_effects: bool,
    pub check_oil_items: bool,
    pub check_allowed: bool,
    pub current: bool,
    pub allowed: bool,
    pub has_item: bool,
    pub disabled: bool,
    pub oil: f32,
}

pub fn lantern_set_active(p: &LanternSet, o: &mut Ops) {
    if p.current == p.active {
        return;
    }
    if p.check_allowed && !p.allowed {
        return;
    }
    if p.check_oil_items && !p.has_item {
        o.k(k::LANTERN_HINT_NO_ITEM);
        return;
    }
    if p.active && p.disabled {
        return;
    }
    if p.check_oil_items && p.active && p.oil <= 0.0 {
        if p.use_effects {
            o.k(k::LANTERN_HINT_NO_OIL);
            o.k(k::LANTERN_SOUND_OUT);
        }
        return;
    }
    o.ki(k::LANTERN_SET_ACTIVE, i32::from(p.active));
    if p.use_effects {
        o.k(if p.active {
            k::LANTERN_SOUND_ON
        } else {
            k::LANTERN_SOUND_OFF
        });
    }
    o.ki(k::LANTERN_HAND, i32::from(p.active));
    o.ki(k::LANTERN_CALLBACK, i32::from(p.active));
}

// Light level

/// The five sample positions of `LightLevel::Update`.
pub fn light_test_positions(pos: Vector3f, size: Vector3f, forward: Vector3f) -> [Vector3f; 5] {
    let h = size.y - 0.1;
    let (tx, ty, tz) = (forward.x * size.z, forward.y * size.z, forward.z * size.z);
    [
        pos,
        v3(pos.x + 0.0, pos.y + h, pos.z + 0.0),
        v3(pos.x - 0.0, pos.y - h, pos.z - 0.0),
        v3(
            tx.mul_add(0.8, pos.x),
            ty.mul_add(0.8, pos.y),
            tz.mul_add(0.8, pos.z),
        ),
        v3(
            tx.mul_add(0.8, pos.x - 0.0),
            ty.mul_add(0.8, pos.y - h),
            tz.mul_add(0.8, pos.z - 0.0),
        ),
    ]
}

/// The update timer: true when the light level is to be sampled now.
pub fn light_timer(count: &mut f32, dt: f32) -> bool {
    if *count <= 0.0 {
        *count = 1.0 / 2.0;
        true
    } else {
        *count -= dt;
        false
    }
}

pub fn light_combine(lantern: bool, ext: &[f32; 5], normal: &[f32; 5]) -> (f32, f32) {
    let (mut e, mut n) = (0.0f32, 0.0f32);
    if lantern {
        e += 1.0;
        n += 1.0;
    }
    for (&x, &y) in ext.iter().zip(normal.iter()) {
        e = if x > e { x } else { e };
        n = if y > n { y } else { n };
    }
    (e, n)
}

// Darkness

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Darkness {
    pub min_darkness_light_level: f32,
    pub ambient_light_min_light_level: f32,
    pub loop_sound_startup_time: f32,
    pub sanity_loss_per_second: f32,
    pub ambient_on: u32,
    pub in_darkness: u32,
    pub loop_sound_count: f32,
    pub sanity_loss_mul: f32,
    pub show_hint_timer: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct DarknessIn {
    pub ext_level: f32,
    pub normal_level: f32,
    pub hard_mode: bool,
    pub flashback_active: bool,
    pub drain_disabled: bool,
    pub emotion_flash: bool,
}

pub fn darkness_update(s: &mut Darkness, i: &DarknessIn, dt: f32, o: &mut Ops) {
    if i.normal_level > s.ambient_light_min_light_level {
        if s.ambient_on != 0 {
            s.ambient_on = 0;
            o.k(k::AMBIENT_OFF);
        }
    } else if s.ambient_on == 0 {
        s.ambient_on = 1;
        o.ki(k::AMBIENT_ON, i32::from(i.hard_mode));
    }
    if i.ext_level > s.min_darkness_light_level {
        s.loop_sound_count -= dt;
        if s.loop_sound_count <= 0.0 {
            s.loop_sound_count = 0.0;
        }
        s.in_darkness = 0;
        s.sanity_loss_mul = hw::msub(dt, 0.3, s.sanity_loss_mul);
        if s.sanity_loss_mul < 0.0 {
            s.sanity_loss_mul = 0.0;
        }
    } else {
        s.sanity_loss_mul = dt.mul_add(0.1, s.sanity_loss_mul);
        if s.sanity_loss_mul > 1.0 {
            s.sanity_loss_mul = 1.0;
        }
        if !i.flashback_active && !i.drain_disabled && !i.emotion_flash {
            o.kf(
                k::LOWER_SANITY,
                s.sanity_loss_per_second * dt * s.sanity_loss_mul,
            );
            if s.show_hint_timer <= 0.0 && s.sanity_loss_mul > 0.05 {
                s.show_hint_timer = 3.0;
                o.k(k::HINT_DARKNESS);
            } else {
                s.show_hint_timer -= dt;
            }
        }
        s.loop_sound_count += dt;
        if s.loop_sound_count >= s.loop_sound_startup_time {
            s.loop_sound_count = s.loop_sound_startup_time;
        }
        s.in_darkness = 1;
    }
}

// Death

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Death {
    pub active: u32,
    pub show_hint: u32,
    pub state: i32,
    pub height_add_speed: f32,
    pub roll_speed: f32,
    pub fade_out_time: f32,
    pub height_add: f32,
    pub roll: f32,
    pub t: f32,
    pub min_height_add: f32,
    pub fade_alpha: f32,
    pub text_alpha1: f32,
    pub text_on_screen_count: f32,
    pub white_count: f32,
}

/// Fade-out of `Death::Update`: collapse, fade, text and the hand-over to the reset.
pub fn death_phase1(s: &mut Death, perma: bool, dt: f32, o: &mut Ops) {
    s.t += dt;
    if s.state == 0 || s.state == 1 {
        if s.height_add > s.min_height_add {
            s.height_add = hw::msub(s.height_add_speed, dt, s.height_add);
            if s.height_add < s.min_height_add {
                s.height_add = s.min_height_add;
            }
            o.kf(k::SET_HEAD_ADD, s.height_add);
            s.state = 1;
        }
        s.roll = to_rad(s.roll_speed).mul_add(dt, s.roll);
        if s.roll > to_rad(65.0) {
            s.roll = to_rad(65.0);
        }
        o.kfff(k::FADE_ROLL, s.roll, 10.0, 10.0);
    }
    if s.state == 1 {
        s.fade_alpha = dt.mul_add(1.0 / s.fade_out_time, s.fade_alpha);
        if s.fade_alpha > 1.0 {
            s.fade_alpha = 1.0;
            if perma {
                s.state = 4;
                o.k(k::DEATH_PERMA_SOUND);
            }
        }
        if s.fade_alpha > 0.75 && !perma {
            s.text_alpha1 = ((1.0 - s.text_alpha1) * 0.5).mul_add(dt, s.text_alpha1);
            if s.text_alpha1 > 1.0 {
                s.text_alpha1 = 1.0;
            }
        }
        if s.text_alpha1 > 1.0 {
            s.text_alpha1 = 1.0;
        }
        if s.text_alpha1 > 0.9 && s.fade_alpha == 1.0 {
            s.text_on_screen_count += dt;
            if s.text_on_screen_count > 5.5 || s.show_hint == 0 {
                s.state = 2;
                o.k(k::DEATH_RESET_GAME);
                o.k(k::DEATH_FLASH);
                o.k(k::DEATH_AWAKEN_SOUND);
            }
        }
    }
}

/// White flash and fade-in of `Death::Update`; `hard_mode_active` is hard mode with an active player.
pub fn death_phase2(s: &mut Death, hard_mode_active: bool, dt: f32, o: &mut Ops) {
    if s.state == 2 {
        s.white_count += dt;
        if s.white_count >= 0.5 {
            s.state = 3;
        }
    }
    if s.state == 3 {
        s.text_alpha1 = hw::msub(dt, 0.85, s.text_alpha1);
        s.fade_alpha = hw::msub(dt, 0.75, s.fade_alpha);
        if s.fade_alpha < 0.0 {
            s.fade_alpha = 0.0;
            s.active = 0;
            o.k(k::DEATH_RESET);
            if hard_mode_active {
                o.k(k::DEATH_AUTOLOAD);
            }
        }
    }
}

/// Perma-death voice wait and hard-mode reload of `Death::Update`.
pub fn death_phase3(state: i32, voice_gone: bool, o: &mut Ops) {
    if state == 4 && voice_gone {
        o.k(k::DEATH_EXIT);
    }
    if state == 5 {
        o.k(k::DEATH_AUTOLOAD);
    }
}
