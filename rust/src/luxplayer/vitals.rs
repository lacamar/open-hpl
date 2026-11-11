//! `cLuxPlayer` health, sanity and lamp oil rules. Member writes come back as ops so that the
//! C++ callbacks between them (death, collapse) see the same state as the original.

use super::ops::Ops;

pub mod k {
    pub const SET_HEALTH: u32 = 1;
    pub const SET_SANITY: u32 = 2;
    pub const SET_OIL: u32 = 3;
    pub const DAMAGE_SPLASH: u32 = 4;
    pub const HIT_FLASH: u32 = 5;
    pub const SPIN_HEAD: u32 = 6;
    pub const DEATH_START: u32 = 7;
    pub const COLLAPSE_START: u32 = 8;
    pub const SANITY_LOST: u32 = 9;
    pub const SANITY_GAIN_FLASH: u32 = 10;
    pub const KILL: u32 = 11;
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Vitals {
    pub health: f32,
    pub sanity: f32,
    pub lamp_oil: f32,
    pub hard_mode: u32,
}

pub fn give_damage(
    v: &Vitals,
    amount: f32,
    damage_type: i32,
    spin: bool,
    lethal: bool,
    o: &mut Ops,
) {
    if v.health <= 0.0 {
        return;
    }
    let mut health = v.health - amount;
    o.kf(k::SET_HEALTH, health);
    o.ki(k::DAMAGE_SPLASH, damage_type);
    o.k(k::HIT_FLASH);
    if spin {
        o.k(k::SPIN_HEAD);
    }
    if !lethal && health < 10.0 {
        health = 10.0;
        o.kf(k::SET_HEALTH, health);
    }
    if health <= 0.0 {
        o.k(k::DEATH_START);
    }
}

pub fn set_health(v: &Vitals, x: f32, o: &mut Ops) {
    if v.health <= 0.0 && x <= 0.0 {
        return;
    }
    o.kf(k::SET_HEALTH, x);
    if x <= 0.0 {
        o.k(k::DEATH_START);
    }
}

pub fn set_sanity(v: &Vitals, x: f32, o: &mut Ops) {
    o.kf(k::SET_SANITY, x);
    if v.hard_mode != 0 && x <= 0.0 {
        o.k(k::KILL);
    }
}

pub fn lower_sanity(v: &Vitals, amount: f32, use_effect: bool, o: &mut Ops) {
    if v.health <= 0.0 {
        return;
    }
    let mut sanity = v.sanity - amount;
    if sanity < 0.0 {
        sanity = 0.0;
        o.kf(k::SET_SANITY, sanity);
        o.k(k::COLLAPSE_START);
        if v.hard_mode != 0 {
            o.k(k::KILL);
        }
    } else {
        o.kf(k::SET_SANITY, sanity);
    }
    if use_effect {
        o.k(k::SANITY_LOST);
    }
}

pub fn add_health(v: &Vitals, x: f32, o: &mut Ops) {
    if (v.health >= 100.0 && x > 0.0) || v.health < 0.0 {
        return;
    }
    let mut health = v.health + x;
    if health > 100.0 {
        health = 100.0;
    }
    o.kf(k::SET_HEALTH, health);
    if health <= 0.0 {
        o.k(k::DEATH_START);
    }
}

pub fn add_sanity(v: &Vitals, x: f32, show_effect: bool, o: &mut Ops) {
    if v.sanity >= 100.0 && x > 0.0 {
        return;
    }
    let mut sanity = v.sanity + x;
    if sanity > 100.0 {
        sanity = 100.0;
    }
    if sanity < 0.0 {
        sanity = 0.0;
        o.kf(k::SET_SANITY, sanity);
        o.k(k::COLLAPSE_START);
        if v.hard_mode != 0 {
            o.k(k::KILL);
        }
    } else {
        o.kf(k::SET_SANITY, sanity);
    }
    if x > 0.0 && show_effect {
        o.k(k::SANITY_GAIN_FLASH);
    }
}

pub fn add_lamp_oil(v: &Vitals, x: f32, o: &mut Ops) {
    if v.lamp_oil >= 100.0 && x > 0.0 {
        return;
    }
    let oil = (v.lamp_oil + x).clamp(0.0, 100.0);
    o.kf(k::SET_OIL, oil);
}
