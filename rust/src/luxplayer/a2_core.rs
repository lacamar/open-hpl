//! Player core state owned by Rust and embedded in `cLuxPlayer` (members stay as references): vitals, camera fades,
//! terror, head spin and focus alpha. The drivers run the T0 kernels and call the host back for what is still C++.

use super::camera::{self, CamState};
use super::ffi::{
    VITALS_ADD_HEALTH, VITALS_ADD_LAMP_OIL, VITALS_ADD_SANITY, VITALS_GIVE_DAMAGE,
    VITALS_LOWER_SANITY, VITALS_SET_HEALTH, VITALS_SET_SANITY,
};
use super::ops::{Op, Ops};
use super::vitals::{self, Vitals};
use crate::types::Vector2f;
use std::ffi::c_void;

pub mod q {
    pub const CAM_PITCH: u32 = 1;
    pub const PITCH_MIN: u32 = 2;
    pub const PITCH_MAX: u32 = 3;
    pub const FREE_CAM: u32 = 4;
    pub const HARD_MODE: u32 = 5;
    pub const TERROR_ENEMIES: u32 = 6;
    pub const FOCUS_HIDE: u32 = 7;
    pub const RAND_YAW: u32 = 8;
    pub const RAND_PITCH: u32 = 9;
}

pub mod hop {
    pub const HEAD_SPIN_CAM: u32 = 200;
    pub const TERROR_SOUND: u32 = 201;
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Core {
    pub cam: CamState,
    pub health: f32,
    pub sanity: f32,
    pub lamp_oil: f32,
    pub terror: f32,
    pub head_spin: Vector2f,
    pub focus_alpha: f32,
    pub head_spin_damage_speed: f32,
    pub head_spin_deacc: f32,
    pub terror_inc: f32,
    pub terror_dec: f32,
}

const _: () = assert!(std::mem::size_of::<Core>() == 128 + 4 * 4 + 8 + 5 * 4);

#[repr(C)]
pub struct Host {
    pub ctx: *mut c_void,
    pub op: extern "C" fn(*mut c_void, u32, i32, f32, f32, f32, f32),
    pub query: extern "C" fn(*mut c_void, u32) -> f32,
}

impl Host {
    fn q(&self, k: u32) -> f32 {
        (self.query)(self.ctx, k)
    }

    fn qb(&self, k: u32) -> bool {
        self.q(k) != 0.0
    }

    fn op(&self, o: &Op) {
        (self.op)(self.ctx, o.kind, o.i, o.a, o.b, o.c, o.d);
    }

    fn hop(&self, kind: u32, a: f32, b: f32) {
        self.op(&Op {
            kind,
            i: 0,
            a,
            b,
            c: 0.0,
            d: 0.0,
        });
    }
}

/// # Safety
/// `c` is valid and not borrowed elsewhere; the host may call back into the player, so no reference outlives a host call.
pub unsafe fn vitals_run(c: *mut Core, h: &Host, which: i32, amount: f32, ty: i32, flags: u32) {
    // SAFETY: caller contract.
    let v = unsafe {
        Vitals {
            health: (*c).health,
            sanity: (*c).sanity,
            lamp_oil: (*c).lamp_oil,
            hard_mode: u32::from(h.qb(q::HARD_MODE)),
        }
    };
    let mut o = Ops::default();
    let on = flags & 1 != 0;
    match which {
        VITALS_GIVE_DAMAGE => vitals::give_damage(&v, amount, ty, on, flags & 2 != 0, &mut o),
        VITALS_SET_HEALTH => vitals::set_health(&v, amount, &mut o),
        VITALS_SET_SANITY => vitals::set_sanity(&v, amount, &mut o),
        VITALS_LOWER_SANITY => vitals::lower_sanity(&v, amount, on, &mut o),
        VITALS_ADD_HEALTH => vitals::add_health(&v, amount, &mut o),
        VITALS_ADD_SANITY => vitals::add_sanity(&v, amount, on, &mut o),
        VITALS_ADD_LAMP_OIL => vitals::add_lamp_oil(&v, amount, &mut o),
        _ => {}
    }
    for op in o.ops.iter().take(o.n as usize) {
        match op.kind {
            // SAFETY: caller contract.
            vitals::k::SET_HEALTH => unsafe { (*c).health = op.a },
            // SAFETY: caller contract.
            vitals::k::SET_SANITY => unsafe { (*c).sanity = op.a },
            // SAFETY: caller contract.
            vitals::k::SET_OIL => unsafe { (*c).lamp_oil = op.a },
            vitals::k::SPIN_HEAD => {
                // SAFETY: caller contract.
                unsafe { spin_head(c, h, (*c).head_spin_damage_speed) };
            }
            // SAFETY: caller contract.
            vitals::k::KILL => unsafe { vitals_run(c, h, VITALS_SET_HEALTH, 0.0, 0, 0) },
            _ => h.op(op),
        }
    }
}

/// # Safety
/// As `vitals_run`.
pub unsafe fn spin_head(c: *mut Core, h: &Host, speed: f32) {
    let yaw = h.q(q::RAND_YAW);
    let pitch = h.q(q::RAND_PITCH);
    // SAFETY: caller contract.
    unsafe { (*c).head_spin = camera::spin_head(yaw, pitch, speed) };
}

/// # Safety
/// As `vitals_run`.
pub unsafe fn update_camera(c: *mut Core, h: &Host, dt: f32) {
    // SAFETY: caller contract.
    let mut s = unsafe { (*c).cam };
    s.free_cam = u32::from(h.qb(q::FREE_CAM));
    s.cam_pitch = h.q(q::CAM_PITCH);
    s.pitch_min = h.q(q::PITCH_MIN);
    s.pitch_max = h.q(q::PITCH_MAX);
    let mut o = Ops::default();
    camera::update_camera(&mut s, dt, &mut o);
    // SAFETY: caller contract; only the fields the original wrote back.
    unsafe {
        let d = &mut (*c).cam;
        d.fov_mul = s.fov_mul;
        d.aspect_mul = s.aspect_mul;
        d.roll = s.roll;
        d.lean_roll = s.lean_roll;
        d.fading_pitch = s.fading_pitch;
        d.cam_anim_pos = s.cam_anim_pos;
    }
    for op in o.ops.iter().take(o.n as usize) {
        h.op(op);
    }
}

/// # Safety
/// As `vitals_run`.
pub unsafe fn update_head_spin(c: *mut Core, h: &Host, dt: f32) {
    // SAFETY: caller contract.
    let (spin, deacc) = unsafe { ((*c).head_spin, (*c).head_spin_deacc) };
    let Some((next, pitch, yaw)) = camera::update_head_spin(spin, deacc, dt) else {
        return;
    };
    // SAFETY: caller contract.
    unsafe { (*c).head_spin = next };
    h.hop(hop::HEAD_SPIN_CAM, pitch, yaw);
}

/// # Safety
/// As `vitals_run`.
pub unsafe fn update_terror(c: *mut Core, h: &Host, dt: f32) {
    let present = h.qb(q::TERROR_ENEMIES);
    // SAFETY: caller contract.
    let t = unsafe {
        (*c).terror =
            camera::terror_step((*c).terror, present, (*c).terror_inc, (*c).terror_dec, dt);
        (*c).terror
    };
    h.hop(hop::TERROR_SOUND, t, 0.0);
}

/// # Safety
/// As `vitals_run`.
pub unsafe fn update_focus_alpha(c: *mut Core, h: &Host, dt: f32) {
    let hide = h.qb(q::FOCUS_HIDE);
    // SAFETY: caller contract.
    unsafe { (*c).focus_alpha = camera::update_focus_alpha((*c).focus_alpha, hide, dt) };
}

/// # Safety
/// `c` valid, `h` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2_vitals(
    c: *mut Core,
    h: *const Host,
    which: i32,
    amount: f32,
    ty: i32,
    flags: u32,
) {
    // SAFETY: caller contract.
    unsafe { vitals_run(c, &*h, which, amount, ty, flags) };
}

/// # Safety
/// As `ohpl_a2_vitals`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2_spin_head(c: *mut Core, h: *const Host, speed: f32) {
    // SAFETY: caller contract.
    unsafe { spin_head(c, &*h, speed) };
}

/// # Safety
/// As `ohpl_a2_vitals`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2_update_camera(c: *mut Core, h: *const Host, dt: f32) {
    // SAFETY: caller contract.
    unsafe { update_camera(c, &*h, dt) };
}

/// # Safety
/// As `ohpl_a2_vitals`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2_update_head_spin(c: *mut Core, h: *const Host, dt: f32) {
    // SAFETY: caller contract.
    unsafe { update_head_spin(c, &*h, dt) };
}

/// # Safety
/// As `ohpl_a2_vitals`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2_update_terror(c: *mut Core, h: *const Host, dt: f32) {
    // SAFETY: caller contract.
    unsafe { update_terror(c, &*h, dt) };
}

/// # Safety
/// As `ohpl_a2_vitals`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2_update_focus_alpha(c: *mut Core, h: *const Host, dt: f32) {
    // SAFETY: caller contract.
    unsafe { update_focus_alpha(c, &*h, dt) };
}
