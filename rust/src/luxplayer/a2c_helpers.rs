//! Drivers of the last player helpers (`LuxPlayerHelpers.cpp`): sanity, death, lean, HUD effect and
//! the spawn particle systems. State is POD embedded in the C++ helper (spawn positions live in a boxed
//! Rust list); the engine objects behind each step are reached through the host.

use super::a2b_helpers::{HostB, bq};
use super::helpers::{self, Death};
use super::ops::Ops;
use super::sanity::{self, Sanity};
use crate::hw;
use crate::types::Vector3f;
use std::ffi::c_void;

const _: () = {
    assert!(std::mem::size_of::<Lean>() == 28);
    assert!(std::mem::size_of::<Splash>() == 60);
    assert!(std::mem::size_of::<Hud>() == 20 + 60 * SPLASH_CAP);
};

pub mod cq {
    pub const HARD_ACTIVE: u32 = 20;
    pub const PERMA: u32 = 21;
    pub const SCAN: u32 = 22;
    pub const LEAN_COLLIDE: u32 = 23;
    pub const HUD_IMAGES: u32 = 24;
}

pub mod cf {
    pub const HUD_IMAGE_SIZE: u32 = 20;
    pub const HUD_CENTER: u32 = 21;
}

pub mod cop {
    pub const LEAN_ROLL: u32 = 320;
    pub const LEAN_HEAD: u32 = 321;
}

/// Runs one kernel step on a local copy, stores it, then replays the ops (which may call back into the helper).
///
/// # Safety
/// `s` valid and not borrowed; no reference outlives a host call.
unsafe fn stage<T: Copy>(
    s: *mut T,
    h: &HostB,
    f: impl FnOnce(&mut T, &mut Ops),
    pant: Option<fn(*mut T) -> *mut f32>,
) {
    // SAFETY: caller contract.
    let mut st = unsafe { *s };
    let mut o = Ops::default();
    f(&mut st, &mut o);
    // SAFETY: caller contract.
    unsafe { *s = st };
    for op in o.ops.iter().take(o.n as usize) {
        match pant {
            Some(field) if op.kind == sanity::k::RAND_PANT => {
                let v = (h.query)(h.ctx, bq::RAND, op.a, op.b);
                // SAFETY: caller contract; `field` points into `*s`.
                unsafe { *field(s) = v };
            }
            _ => h.op(op),
        }
    }
}

fn pant_field(s: *mut Sanity) -> *mut f32 {
    // SAFETY: only a field address is formed.
    unsafe { &raw mut (*s).pant_count }
}

/// `cLuxPlayerSanity::Update`; `SCAN` runs the critter and enemy scan and answers whether an enemy was seen.
///
/// # Safety
/// As `stage`.
pub unsafe fn sanity_update(s: *mut Sanity, h: &HostB, dt: f32) {
    let dead = || h.qb(bq::DEAD);
    // SAFETY: caller contract, for every call below.
    unsafe {
        stage(
            s,
            h,
            |st, o| sanity::sanity_begin(st, h.q(bq::SANITY), dt, o),
            Some(pant_field),
        );
        let mut due = false;
        stage(s, h, |st, _| due = sanity::sanity_scan_due(st, dt), None);
        if due {
            let seen = h.qb(cq::SCAN);
            let decrease = (*s).near_enemy_decrease;
            stage(
                s,
                h,
                |st, o| sanity::sanity_scan_result(st, seen, decrease, o),
                None,
            );
        }
        stage(
            s,
            h,
            |st, o| sanity::sanity_enemy_seen_effect(st, dt, o),
            None,
        );
        let d = dead();
        stage(s, h, |st, o| sanity::sanity_update_hit(st, d, dt, o), None);
        let (sanity, d) = (h.q(bq::SANITY), dead());
        stage(
            s,
            h,
            |st, o| sanity::sanity_insane_effects(st, sanity, d, dt, o),
            Some(pant_field),
        );
    }
}

/// `cLuxPlayerSanity::Reset`; the two update flags keep their value.
pub fn sanity_reset(s: &mut Sanity) {
    s.hit_alpha = 0.0;
    s.hit_active = 0;
    s.sanity_lost_count = 0.0;
    s.pant_count = 1.0;
    s.check_enemy_seen_count = 0.0;
    s.t = 0.0;
    s.insane_wave_alpha = 0.0;
    s.sanity_drain_count = 0.0;
    s.sanity_drain_volume = 0.0;
    s.sanity_heartbeat_count = 0.0;
    s.seen_enemy_count = 0.0;
    s.enemy_is_seen = 0;
    s.sanity_effect_updated = 0;
    s.at_low_sanity_count = 0.0;
    s.show_hint_timer = 0.0;
}

/// `cLuxPlayerDeath::Update`.
///
/// # Safety
/// As `stage`.
pub unsafe fn death_update(s: *mut Death, h: &HostB, dt: f32) {
    // SAFETY: caller contract, for every access below.
    unsafe {
        if (*s).active == 0 {
            return;
        }
        let perma = h.qb(cq::PERMA);
        stage(s, h, |st, o| helpers::death_phase1(st, perma, dt, o), None);
        let hard = h.qb(cq::HARD_ACTIVE);
        stage(s, h, |st, o| helpers::death_phase2(st, hard, dt, o), None);
        let state = (*s).state;
        let gone = state == 4 && h.qb(bq::VOICE_DONE);
        stage(s, h, |_, o| helpers::death_phase3(state, gone, o), None);
    }
}

/// The variable part of `cLuxPlayerDeath::Reset`.
///
/// # Safety
/// `s` valid.
pub unsafe fn death_reset(s: *mut Death) {
    // SAFETY: caller contract.
    let s = unsafe { &mut *s };
    s.height_add = 0.0;
    s.roll = 0.0;
    s.text_alpha1 = 0.0;
    s.white_count = 0.0;
    s.fade_alpha = 0.0;
    s.text_on_screen_count = 0.0;
    s.state = 0;
    s.t = 0.0;
    s.active = 0;
}

/// The variable part of `cLuxPlayerDeath::Start`, run after the engine side.
///
/// # Safety
/// `s` valid.
pub unsafe fn death_start(s: *mut Death, crouching: bool) {
    // SAFETY: caller contract.
    let s = unsafe { &mut *s };
    s.height_add = 0.0;
    s.roll = 0.0;
    s.min_height_add = if crouching {
        s.height_add_goal_crouch
    } else {
        s.height_add_goal
    };
    s.state = 0;
    s.fade_alpha = 0.0;
    s.white_count = 0.0;
}

// Lean

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Lean {
    pub dir: f32,
    pub dir_add: f32,
    pub movement: f32,
    pub rotation: f32,
    pub max_movement: f32,
    pub max_rotation: f32,
    pub pressed: u32,
}

/// `cLuxPlayerLean::Update`; `LEAN_COLLIDE` (a = movement) tests the head shape at that offset.
///
/// # Safety
/// As `stage`.
pub unsafe fn lean_update(s: *mut Lean, h: &HostB, dt: f32) {
    if h.qb(bq::DEAD) {
        return;
    }
    // SAFETY: caller contract, for every access below.
    let mut st = unsafe { *s };
    if st.pressed != 0 {
        let dir = st.dir + st.dir_add;
        st.pressed = 0;
        let goal_pos = st.max_movement * dir;
        let goal_rot = st.max_rotation * -dir;

        let prev_movement = st.movement;
        let mut move_speed = goal_pos - st.movement;
        if move_speed.abs() < 0.1 {
            move_speed = 0.1 * dir;
        }
        st.movement = (move_speed * dt).mul_add(3.0, st.movement);
        if goal_pos < 0.0 && st.movement < goal_pos {
            st.movement = goal_pos;
        }
        if goal_pos > 0.0 && st.movement > goal_pos {
            st.movement = goal_pos;
        }

        let prev_rotation = st.rotation;
        let mut rot_speed = goal_rot - st.rotation;
        if rot_speed.abs() < 0.13 {
            rot_speed = 0.13 * -dir;
        }
        st.rotation = (rot_speed * dt).mul_add(2.0, st.rotation);
        if goal_rot < 0.0 && st.rotation < goal_rot {
            st.rotation = goal_rot;
        }
        if goal_rot > 0.0 && st.rotation > goal_rot {
            st.rotation = goal_rot;
        }

        let reverse_movement = prev_movement - st.movement;
        let reverse_rotation = prev_rotation - st.rotation;
        let mut count = 0;
        while (h.query)(h.ctx, cq::LEAN_COLLIDE, st.movement, 0.0) != 0.0 {
            st.movement += reverse_movement;
            st.rotation += reverse_rotation;
            if reverse_movement < 0.0 && st.movement < 0.0 {
                st.movement = 0.0;
                st.rotation = 0.0;
                break;
            }
            if reverse_movement > 0.0 && st.movement > 0.0 {
                st.movement = 0.0;
                st.rotation = 0.0;
                break;
            }
            count += 1;
            if count > 10 {
                st.movement = 0.0;
                st.rotation = 0.0;
                break;
            }
        }
        // SAFETY: caller contract.
        unsafe { *s = st };
        (h.op)(h.ctx, cop::LEAN_ROLL, 0, st.rotation, 5.0, 3.0, 0.0);
        (h.op)(h.ctx, cop::LEAN_HEAD, 0, st.movement, 2.0, 0.05, 0.0);
    } else if st.movement != 0.0 || st.rotation != 0.0 {
        st.rotation = 0.0;
        st.movement = 0.0;
        st.dir = 0.0;
        st.dir_add = 0.0;
        // SAFETY: caller contract.
        unsafe { *s = st };
        (h.op)(h.ctx, cop::LEAN_ROLL, 0, 0.0, 4.0, 2.0, 0.0);
        (h.op)(h.ctx, cop::LEAN_HEAD, 0, 0.0, 1.3, 0.1, 0.0);
    } else {
        // SAFETY: caller contract.
        unsafe { *s = st };
    }
}

pub fn lean_reset(s: &mut Lean) {
    s.dir = 0.0;
    s.dir_add = 0.0;
    s.movement = 0.0;
    s.rotation = 0.0;
    s.pressed = 0;
}

pub fn lean_set(s: &mut Lean, mul: f32) {
    s.dir = mul.clamp(-1.0, 1.0);
    if mul.abs() > 0.0 {
        s.dir_add = 0.0;
        s.pressed = 1;
    }
}

pub fn lean_add(s: &mut Lean, add: f32) {
    s.dir_add = (s.dir_add + add).clamp(-1.0, 1.0);
    s.pressed = u32::from(s.dir_add.abs() > 0.0);
}

// HUD effect

pub const SPLASH_CAP: usize = 64;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Splash {
    pub image: u32,
    pub pos: Vector3f,
    pub size: [f32; 2],
    pub alpha: f32,
    pub alpha_mul: f32,
    pub pos_vel: Vector3f,
    pub size_vel: [f32; 2],
    pub alpha_vel: f32,
    pub alpha_move_start: f32,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Hud {
    pub flash_alpha_speed: f32,
    pub flash_alpha_out_speed: f32,
    pub flash_alpha: f32,
    pub flash_active: u32,
    pub count: u32,
    pub splashes: [Splash; SPLASH_CAP],
}

impl Default for Hud {
    fn default() -> Self {
        Self {
            flash_alpha_speed: 0.0,
            flash_alpha_out_speed: 0.0,
            flash_alpha: 0.0,
            flash_active: 0,
            count: 0,
            splashes: [Splash::default(); SPLASH_CAP],
        }
    }
}

impl Hud {
    fn push(&mut self, splash: Splash) {
        if let Some(slot) = self.splashes.get_mut(self.count as usize) {
            *slot = splash;
            self.count += 1;
        }
    }
}

pub fn hud_reset(s: &mut Hud) {
    s.count = 0;
    s.flash_alpha = 0.0;
    s.flash_active = 0;
}

pub fn hud_flash(s: &mut Hud, in_time: f32, out_time: f32) {
    s.flash_alpha = 0.0;
    s.flash_alpha_speed = if in_time == 0.0 {
        100000.0
    } else {
        1.0 / in_time
    };
    s.flash_alpha_out_speed = if out_time == 0.0 {
        -100000.0
    } else {
        -1.0 / out_time
    };
    s.flash_active = 1;
}

pub fn hud_update(s: &mut Hud, dt: f32) {
    let mut keep = 0;
    let n = (s.count as usize).min(SPLASH_CAP);
    for i in 0..n {
        let Some(mut sp) = s.splashes.get(i).copied() else {
            break;
        };
        if sp.alpha < sp.alpha_move_start {
            sp.pos.x = sp.pos_vel.x.mul_add(dt, sp.pos.x);
            sp.pos.y = sp.pos_vel.y.mul_add(dt, sp.pos.y);
            sp.pos.z = sp.pos_vel.z.mul_add(dt, sp.pos.z);
            sp.size[0] = sp.size_vel[0].mul_add(dt, sp.size[0]);
            sp.size[1] = sp.size_vel[1].mul_add(dt, sp.size[1]);
        }
        sp.alpha = hw::msub(sp.alpha_vel, dt, sp.alpha);
        if sp.alpha < 0.0 {
            continue;
        }
        if let Some(slot) = s.splashes.get_mut(keep) {
            *slot = sp;
        }
        keep += 1;
    }
    s.count = keep as u32;
    if s.flash_active == 0 {
        return;
    }
    s.flash_alpha = s.flash_alpha_speed.mul_add(dt, s.flash_alpha);
    if s.flash_alpha > 1.0 && s.flash_alpha_speed > 0.0 {
        s.flash_alpha = 1.0;
        s.flash_alpha_speed = s.flash_alpha_out_speed;
    }
    if s.flash_alpha < 0.0 && s.flash_alpha_speed < 0.0 {
        s.flash_alpha = 0.0;
        s.flash_active = 0;
    }
}

/// `AddDamageSplash`: `image` is `type << 8 | index`; the host knows the image counts and sizes.
///
/// # Safety
/// `s` valid and not borrowed.
pub unsafe fn hud_add_splash(s: *mut Hud, h: &HostB, damage_type: i32) {
    let count = (h.query)(h.ctx, cq::HUD_IMAGES, damage_type as f32, 0.0) as i32;
    if count <= 0 {
        return;
    }
    let pick = (h.query)(h.ctx, bq::RAND_INT, 0.0, (count - 1) as f32) as i32;
    let mut size = [damage_type as f32, pick as f32];
    h.fill(cf::HUD_IMAGE_SIZE, &mut size);
    let mut center = [0.0_f32; 2];
    h.fill(cf::HUD_CENTER, &mut center);
    let x = (h.query)(h.ctx, bq::RAND, 0.0, center[0] - size[0]);
    let y = (h.query)(h.ctx, bq::RAND, 0.0, center[1] - size[1]);
    // SAFETY: caller contract.
    let s = unsafe { &mut *s };
    for i in 0..2 {
        let (alpha_mul, vel, size_vel) = if i == 0 {
            (0.3, 1.0, 8.0)
        } else {
            (0.7, 6.0, 16.0)
        };
        s.push(Splash {
            image: ((damage_type as u32) << 8) | pick as u32,
            pos: Vector3f { x, y, z: 1.0 },
            size,
            alpha: 1.0,
            alpha_mul,
            pos_vel: Vector3f {
                x: 0.0,
                y: vel,
                z: 0.0,
            },
            size_vel: [0.0, size_vel],
            alpha_vel: 0.4,
            alpha_move_start: 0.8,
        });
    }
}

// Spawn particle systems

#[repr(C)]
pub struct SpsHost {
    pub ctx: *mut c_void,
    pub create: extern "C" fn(*mut c_void, *const Vector3f) -> usize,
    pub exists: extern "C" fn(*mut c_void, usize) -> bool,
    pub kill: extern "C" fn(*mut c_void, usize),
    pub destroy: extern "C" fn(*mut c_void, usize),
    pub rand: extern "C" fn(*mut c_void, f32, f32) -> f32,
    pub feet: extern "C" fn(*mut c_void, *mut Vector3f),
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct SpsCfg {
    pub radius: f32,
    pub density: f32,
    pub height_from_feet: f32,
    pub height_add_min: f32,
    pub height_add_max: f32,
}

struct SpawnPos {
    ps: usize,
    pos: Vector3f,
}

#[derive(Default)]
pub struct Sps {
    cfg: SpsCfg,
    active: bool,
    spawns: Vec<SpawnPos>,
}

impl SpsHost {
    fn feet(&self) -> Vector3f {
        let mut v = Vector3f::default();
        (self.feet)(self.ctx, &mut v);
        v
    }

    fn rand(&self, min: f32, max: f32) -> f32 {
        (self.rand)(self.ctx, min, max)
    }

    fn create(&self, pos: &Vector3f) -> usize {
        (self.create)(self.ctx, pos)
    }
}

impl Sps {
    pub fn set_active(&mut self, active: bool) {
        self.active = active;
    }

    pub fn is_active(&self) -> bool {
        self.active
    }

    pub fn count(&self) -> usize {
        self.spawns.len()
    }

    pub fn start(&mut self, h: &SpsHost, cfg: SpsCfg) {
        self.cfg = cfg;
        self.active = true;
        self.respawn(h);
    }

    /// `Reset`: destroys the systems that still exist when a map is loaded.
    pub fn reset(&mut self, h: &SpsHost, map: bool) {
        self.active = false;
        if map {
            for sp in &self.spawns {
                if (h.exists)(h.ctx, sp.ps) {
                    (h.destroy)(h.ctx, sp.ps);
                }
            }
        }
        self.spawns.clear();
    }

    pub fn respawn(&mut self, h: &SpsHost) {
        for sp in &self.spawns {
            if sp.ps != 0 && (h.exists)(h.ctx, sp.ps) {
                (h.kill)(h.ctx, sp.ps);
            }
        }
        self.spawns.clear();
        self.generate(h);
    }

    fn generate(&mut self, h: &SpsHost) {
        let c = self.cfg;
        let player = h.feet();
        let step = 1.0 / c.density;
        let start = Vector3f {
            x: -c.radius + step * 0.5,
            y: 0.0,
            z: -c.radius + step * 0.5,
        };
        let mut local = start;
        while local.z < c.radius {
            local.x = start.x;
            while local.x < c.radius {
                let range = (1.0 / c.density) * 0.2;
                let rx = h.rand(-range, range);
                let ry = h.rand(0.0, 0.0);
                let rz = h.rand(-range, range);
                let mut fin = Vector3f {
                    x: local.x + rx,
                    y: local.y + ry,
                    z: local.z + rz,
                };
                fin.y += c.height_from_feet + h.rand(c.height_add_min, c.height_add_max);
                let pos = Vector3f {
                    x: player.x + fin.x,
                    y: player.y + fin.y,
                    z: player.z + fin.z,
                };
                let ps = h.create(&pos);
                if ps != 0 {
                    self.spawns.push(SpawnPos { ps, pos });
                }
                local.x += step;
            }
            local.z += step;
        }
    }

    pub fn update(&mut self, h: &SpsHost) {
        if !self.active {
            return;
        }
        let c = self.cfg;
        let player = h.feet();
        for i in 0..self.spawns.len() {
            let Some(sp) = self.spawns.get(i) else { break };
            let mut cur = Vector3f {
                x: sp.pos.x - player.x,
                y: sp.pos.y - player.y,
                z: sp.pos.z - player.z,
            };
            if !(cur.x.abs() > c.radius || cur.z.abs() > c.radius) {
                continue;
            }
            let ps = sp.ps;
            if ps != 0 && (h.exists)(h.ctx, ps) {
                (h.kill)(h.ctx, ps);
            }
            let (mut add_x, mut add_z, mut mul_x, mut mul_z) = (0.0, 0.0, 1.0, 1.0);
            if cur.x > c.radius {
                add_x = cur.x - c.radius;
                mul_x = -1.0;
            } else if cur.x < -c.radius {
                add_x = cur.x + c.radius;
                mul_x = -1.0;
            }
            if cur.z > c.radius {
                add_z = cur.z - c.radius;
                mul_z = -1.0;
            } else if cur.z < -c.radius {
                add_z = cur.z + c.radius;
                mul_z = -1.0;
            }
            cur.x = cur.x * mul_x + add_x * 2.0;
            cur.z = cur.z * mul_z + add_z * 2.0;
            cur.y = player.y;
            cur.y += c.height_from_feet + h.rand(c.height_add_min, c.height_add_max);
            let pos = Vector3f {
                x: player.x + cur.x,
                y: player.y + cur.y,
                z: player.z + cur.z,
            };
            let ps = h.create(&pos);
            if let Some(sp) = self.spawns.get_mut(i) {
                sp.pos = pos;
                sp.ps = ps;
            }
        }
    }
}

macro_rules! drive {
    ($name:ident, $f:ident, $t:ty) => {
        /// # Safety
        /// `s` and `h` valid; `s` not borrowed.
        #[unsafe(no_mangle)]
        pub unsafe extern "C" fn $name(s: *mut $t, h: *const HostB, dt: f32) {
            // SAFETY: caller contract.
            unsafe { $f(s, &*h, dt) };
        }
    };
}

drive!(ohpl_a2c_sanity_update, sanity_update, Sanity);
drive!(ohpl_a2c_lean_update, lean_update, Lean);

/// # Safety
/// As `sanity_update`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2c_death_update(s: *mut Death, h: *const HostB, dt: f32) {
    // SAFETY: caller contract.
    unsafe { death_update(s, &*h, dt) };
}

/// # Safety
/// `s` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2c_sanity_reset(s: *mut Sanity) {
    // SAFETY: caller contract.
    sanity_reset(unsafe { &mut *s });
}

/// # Safety
/// `s` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2c_death_reset(s: *mut Death) {
    // SAFETY: caller contract.
    unsafe { death_reset(s) };
}

/// # Safety
/// `s` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2c_death_start(s: *mut Death, crouching: bool) {
    // SAFETY: caller contract.
    unsafe { death_start(s, crouching) };
}

/// # Safety
/// `s` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2c_lean_reset(s: *mut Lean) {
    // SAFETY: caller contract.
    lean_reset(unsafe { &mut *s });
}

/// # Safety
/// `s` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2c_lean_set(s: *mut Lean, mul: f32) {
    // SAFETY: caller contract.
    lean_set(unsafe { &mut *s }, mul);
}

/// # Safety
/// `s` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2c_lean_add(s: *mut Lean, add: f32) {
    // SAFETY: caller contract.
    lean_add(unsafe { &mut *s }, add);
}

/// # Safety
/// `s` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2c_hud_reset(s: *mut Hud) {
    // SAFETY: caller contract.
    hud_reset(unsafe { &mut *s });
}

/// # Safety
/// `s` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2c_hud_flash(s: *mut Hud, in_time: f32, out_time: f32) {
    // SAFETY: caller contract.
    hud_flash(unsafe { &mut *s }, in_time, out_time);
}

/// # Safety
/// `s` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2c_hud_update(s: *mut Hud, dt: f32) {
    // SAFETY: caller contract.
    hud_update(unsafe { &mut *s }, dt);
}

/// # Safety
/// `s` and `h` valid; `s` not borrowed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2c_hud_add_splash(s: *mut Hud, h: *const HostB, damage_type: i32) {
    // SAFETY: caller contract.
    unsafe { hud_add_splash(s, &*h, damage_type) };
}

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_a2c_sps_new() -> *mut Sps {
    Box::into_raw(Box::default())
}

/// # Safety
/// `s` from `ohpl_a2c_sps_new`, not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2c_sps_free(s: *mut Sps) {
    if !s.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(s) });
    }
}

/// # Safety
/// `s` valid and not borrowed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2c_sps_start(s: *mut Sps, h: *const SpsHost, cfg: SpsCfg) {
    // SAFETY: caller contract; the host may call back into other helpers only.
    unsafe { (*s).start(&*h, cfg) };
}

/// # Safety
/// As `ohpl_a2c_sps_start`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2c_sps_reset(s: *mut Sps, h: *const SpsHost, map: bool) {
    // SAFETY: caller contract.
    unsafe { (*s).reset(&*h, map) };
}

/// # Safety
/// As `ohpl_a2c_sps_start`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2c_sps_respawn(s: *mut Sps, h: *const SpsHost) {
    // SAFETY: caller contract.
    unsafe { (*s).respawn(&*h) };
}

/// # Safety
/// As `ohpl_a2c_sps_start`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2c_sps_update(s: *mut Sps, h: *const SpsHost) {
    // SAFETY: caller contract.
    unsafe { (*s).update(&*h) };
}

/// # Safety
/// `s` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2c_sps_set_active(s: *mut Sps, active: bool) {
    // SAFETY: caller contract.
    unsafe { (*s).set_active(active) };
}

/// # Safety
/// `s` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2c_sps_active(s: *const Sps) -> bool {
    // SAFETY: caller contract.
    unsafe { (*s).is_active() }
}

/// # Safety
/// `s` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2c_sps_count(s: *const Sps) -> usize {
    // SAFETY: caller contract.
    unsafe { (*s).count() }
}
