//! Helper state owned by Rust and embedded in the `cLuxPlayer*` helper classes (members stay as references):
//! insanity collapse, camera sway, hurt, look-at, flashback, light level and darkness. The drivers run the
//! `helpers` kernels on a local copy and call the host back for the engine objects that are still C++.

use super::helpers::{
    self, CamDir, Collapse, Darkness, DarknessIn, Flashback, Hurt, Lantern, LanternSet, LookAt,
    k as ok,
};
use super::ops::{Op, Ops};
use crate::types::{Vector2f, Vector3f};
use std::ffi::c_void;

pub mod bq {
    pub const HEALTH: u32 = 1;
    pub const DEAD: u32 = 2;
    pub const SPEED: u32 = 3;
    pub const SANITY: u32 = 4;
    pub const CAM_PITCH: u32 = 5;
    pub const CAM_YAW: u32 = 6;
    pub const RAND: u32 = 7;
    pub const VOICE_DONE: u32 = 8;
    pub const QUEUE_NONEMPTY: u32 = 9;
    pub const HAS_CALLBACK: u32 = 10;
    pub const LANTERN_ACTIVE: u32 = 11;
    pub const HARD_MODE: u32 = 12;
    pub const FLASHBACK_ACTIVE: u32 = 13;
    pub const DRAIN_DISABLED: u32 = 14;
    pub const EMOTION_FLASH: u32 = 15;
    pub const RAND_INT: u32 = 19;
    pub const OIL: u32 = 16;
    pub const ALLOWED: u32 = 17;
    pub const HAS_ITEM: u32 = 18;
}

pub mod bf {
    pub const GOAL_ANGLE: u32 = 1;
    pub const BODY: u32 = 2;
    pub const LIGHTS: u32 = 3;
    pub const DARK_LEVELS: u32 = 4;
}

pub mod bop {
    pub const ADD_HEALTH: u32 = 300;
    pub const AMBIENT_POS: u32 = 301;
    pub const LANTERN_MATRIX: u32 = 302;
}

pub const CAMDIR_CAP: usize = 256;

const _: () = {
    assert!(std::mem::size_of::<CamDirState>() == 32 + 4 + 8 * CAMDIR_CAP);
    assert!(std::mem::size_of::<HurtState>() == 64);
    assert!(std::mem::size_of::<LookAtState>() == 52);
    assert!(std::mem::size_of::<LightState>() == 16);
    assert!(std::mem::size_of::<DarkState>() == 40);
    assert!(std::mem::size_of::<LanternState>() == 36);
};

#[repr(C)]
pub struct HostB {
    pub ctx: *mut c_void,
    pub op: extern "C" fn(*mut c_void, u32, i32, f32, f32, f32, f32),
    pub query: extern "C" fn(*mut c_void, u32, f32, f32) -> f32,
    pub fill: extern "C" fn(*mut c_void, u32, *mut f32),
}

impl HostB {
    fn q(&self, k: u32) -> f32 {
        (self.query)(self.ctx, k, 0.0, 0.0)
    }

    fn qb(&self, k: u32) -> bool {
        self.q(k) != 0.0
    }

    fn op(&self, o: &Op) {
        (self.op)(self.ctx, o.kind, o.i, o.a, o.b, o.c, o.d);
    }

    fn hop(&self, kind: u32, a: f32) {
        (self.op)(self.ctx, kind, 0, a, 0.0, 0.0, 0.0);
    }

    fn fill<const N: usize>(&self, k: u32, buf: &mut [f32; N]) {
        (self.fill)(self.ctx, k, buf.as_mut_ptr());
    }

    fn replay(&self, o: &Ops) {
        for op in o.ops.iter().take(o.n as usize) {
            self.op(op);
        }
    }
}

// Insanity collapse

/// # Safety
/// `s` valid for reads and writes and not borrowed; the host may call back into the helper, so no reference outlives a host call.
pub unsafe fn collapse_update(s: *mut Collapse, h: &HostB, dt: f32) {
    // SAFETY: caller contract.
    let mut st = unsafe { *s };
    let mut o = Ops::default();
    helpers::collapse_update(&mut st, dt, &mut o);
    // SAFETY: caller contract.
    unsafe { *s = st };
    for op in o.ops.iter().take(o.n as usize) {
        if op.kind == ok::RAND_COUNT {
            let v = (h.query)(h.ctx, bq::RAND, op.a, op.b);
            // SAFETY: caller contract.
            unsafe { (*s).random_count = v };
        } else {
            h.op(op);
        }
    }
}

/// The variable part of `Reset`, `Start` (`active`) and `Stop` (which keeps `random_count`).
///
/// # Safety
/// `s` valid.
pub unsafe fn collapse_clear(s: *mut Collapse, active: bool, reset_random: bool) {
    // SAFETY: caller contract.
    let s = unsafe { &mut *s };
    s.active = u32::from(active);
    s.state = 0;
    s.height_add = 0.0;
    s.roll = 0.0;
    s.t = 0.0;
    if reset_random {
        s.random_count = 0.0;
    }
}

// Camera sway

#[repr(C)]
#[derive(Clone, Copy)]
pub struct CamDirState {
    pub k: CamDir,
    pub count: u32,
    pub list: [Vector2f; CAMDIR_CAP],
}

fn list_apply(s: &mut CamDirState, o: &Ops) {
    for op in o.ops.iter().take(o.n as usize) {
        match op.kind {
            ok::LIST_CLEAR => s.count = 0,
            ok::LIST_PUSH => {
                if s.count as usize >= CAMDIR_CAP {
                    s.list.copy_within(1.., 0);
                    s.count = CAMDIR_CAP as u32 - 1;
                }
                if let Some(slot) = s.list.get_mut(s.count as usize) {
                    *slot = Vector2f { x: op.a, y: op.b };
                    s.count += 1;
                }
            }
            ok::LIST_POP_FRONT if s.count > 0 => {
                s.list.copy_within(1..s.count as usize, 0);
                s.count -= 1;
            }
            _ => {}
        }
    }
}

/// # Safety
/// As `collapse_update`.
pub unsafe fn camdir_update(s: *mut CamDirState, h: &HostB, dt: f32) {
    let sanity = h.q(bq::SANITY);
    // SAFETY: caller contract.
    let mut st = unsafe { *s };
    let mut o = Ops::default();
    let n = (st.count as usize).min(CAMDIR_CAP);
    let list: Vec<Vector2f> = st.list.get(..n).unwrap_or(&[]).to_vec();
    helpers::camdir_update(&mut st.k, &list, sanity, dt, &mut o);
    list_apply(&mut st, &o);
    // SAFETY: caller contract.
    unsafe { *s = st };
}

/// # Safety
/// `s` valid.
pub unsafe fn camdir_add(s: *mut CamDirState, yaw: bool, x: f32) -> f32 {
    // SAFETY: caller contract.
    let s = unsafe { &mut *s };
    helpers::camdir_add_and_get(&mut s.k, s.count == 0, yaw, x)
}

/// # Safety
/// `s` valid.
pub unsafe fn camdir_reset(s: *mut CamDirState) {
    // SAFETY: caller contract.
    let s = unsafe { &mut *s };
    s.k.sway_active = 0;
    s.count = 0;
    s.k.sway_add = Vector2f::default();
    s.k.next_add = Vector2f::default();
    s.k.sway_alpha = 0.0;
}

// Hurt

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct HurtState {
    pub k: Hurt,
    pub health_regain_speed: f32,
    pub health_regain_limit: f32,
}

/// # Safety
/// As `collapse_update`.
pub unsafe fn hurt_update(s: *mut HurtState, h: &HostB, dt: f32) {
    // SAFETY: caller contract.
    let (limit, speed) = unsafe { ((*s).health_regain_limit, (*s).health_regain_speed) };
    if let Some(a) = helpers::hurt_regain(h.q(bq::HEALTH), limit, speed, dt) {
        h.hop(bop::ADD_HEALTH, a);
    }
    let health = h.q(bq::HEALTH);
    let dead = h.qb(bq::DEAD);
    let vel = (h.query)(h.ctx, bq::SPEED, dt, 0.0);
    // SAFETY: caller contract.
    let mut st = unsafe { (*s).k };
    let mut o = Ops::default();
    let mut rnd = |lo: i32, hi: i32| (h.query)(h.ctx, bq::RAND_INT, lo as f32, hi as f32) as i32;
    helpers::hurt_update_with(&mut st, health, dead, vel, dt, &mut o, &mut rnd);
    // SAFETY: caller contract.
    unsafe { (*s).k = st };
    h.replay(&o);
}

/// # Safety
/// `s` valid.
pub unsafe fn hurt_reset(s: *mut HurtState) {
    // SAFETY: caller contract.
    let k = unsafe { &mut (*s).k };
    k.alpha = 0.0;
    k.pant_count = 0.0;
    k.pant_pos_add = 0.0;
    k.pant_pos_add_vel = 0.0;
    k.pant_pos_add_dir = 1.0;
    k.current_noise = 0;
    k.noise_update_count = 0.0;
}

// Look-at

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct LookAtState {
    pub k: LookAt,
    pub active: u32,
    pub target_pos: Vector3f,
    pub dest_fov_mul: f32,
    pub fov: f32,
    pub fov_speed: f32,
    pub fov_max_speed: f32,
}

/// # Safety
/// As `collapse_update`.
pub unsafe fn lookat_update(s: *mut LookAtState, h: &HostB, dt: f32) {
    // SAFETY: caller contract.
    let (active, t) = unsafe { ((*s).active, (*s).target_pos) };
    if active == 0 {
        return;
    }
    let mut g = [t.x, t.y, t.z, 0.0, 0.0, 0.0];
    h.fill(bf::GOAL_ANGLE, &mut g);
    let pitch = h.q(bq::CAM_PITCH);
    let yaw = h.q(bq::CAM_YAW);
    let goal = Vector3f {
        x: g[3],
        y: g[4],
        z: g[5],
    };
    // SAFETY: caller contract.
    let mut st = unsafe { (*s).k };
    let mut o = Ops::default();
    helpers::lookat_update(&mut st, goal, pitch, yaw, dt, &mut o);
    // SAFETY: caller contract.
    unsafe { (*s).k = st };
    h.replay(&o);
}

/// # Safety
/// `s` valid.
pub unsafe fn lookat_reset(s: *mut LookAtState) {
    // SAFETY: caller contract.
    let s = unsafe { &mut *s };
    s.active = 0;
    s.k.max_speed = 9999.0;
    s.k.speed_mul = 1.0;
    s.target_pos = Vector3f {
        x: 0.0,
        y: 1.0,
        z: 0.0,
    };
    s.k.current_speed = Vector3f::default();
    s.dest_fov_mul = 1.0;
    s.fov = 1.0;
    s.fov_speed = 1.0;
    s.fov_max_speed = 1.0;
}

/// # Safety
/// `s` valid.
pub unsafe fn lookat_set_target(
    s: *mut LookAtState,
    target: Vector3f,
    speed_mul: f32,
    max_speed: f32,
) {
    // SAFETY: caller contract.
    let s = unsafe { &mut *s };
    s.target_pos = target;
    s.k.speed_mul = speed_mul;
    s.k.max_speed = max_speed;
}

/// # Safety
/// `s` valid.
pub unsafe fn lookat_set_active(s: *mut LookAtState, active: bool) {
    // SAFETY: caller contract.
    let s = unsafe { &mut *s };
    s.active = u32::from(active);
    if !active {
        s.k.current_speed = Vector3f::default();
    }
}

// Flashback

/// # Safety
/// As `collapse_update`.
pub unsafe fn flashback_update(s: *mut Flashback, h: &HostB, dt: f32) {
    // SAFETY: caller contract.
    let active = unsafe { (*s).active } != 0;
    let voice_done = active && h.qb(bq::VOICE_DONE);
    let queued = h.qb(bq::QUEUE_NONEMPTY);
    let has_cb = h.qb(bq::HAS_CALLBACK);
    // SAFETY: caller contract.
    let mut st = unsafe { *s };
    let mut o = Ops::default();
    helpers::flashback_update(&mut st, queued, voice_done, has_cb, dt, &mut o);
    // SAFETY: caller contract.
    unsafe { *s = st };
    h.replay(&o);
}

/// # Safety
/// `s` valid.
pub unsafe fn flashback_reset(s: *mut Flashback) {
    // SAFETY: caller contract.
    let s = unsafe { &mut *s };
    s.flash_delay = 0.0;
    s.active = 0;
}

// Light level

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct LightState {
    pub extended: f32,
    pub normal: f32,
    pub update_count: f32,
    pub radius_add: f32,
}

/// # Safety
/// As `collapse_update`.
pub unsafe fn light_update(s: *mut LightState, h: &HostB, dt: f32) {
    // SAFETY: caller contract.
    let due = unsafe { helpers::light_timer(&mut (*s).update_count, dt) };
    if !due {
        return;
    }
    let mut body = [0.0f32; 9];
    h.fill(bf::BODY, &mut body);
    let v = |i: usize| Vector3f {
        x: body.get(i).copied().unwrap_or(0.0),
        y: body.get(i + 1).copied().unwrap_or(0.0),
        z: body.get(i + 2).copied().unwrap_or(0.0),
    };
    let pos = helpers::light_test_positions(v(0), v(3), v(6));
    let lantern = h.qb(bq::LANTERN_ACTIVE);
    let mut lv = [0.0f32; 26];
    for (d, x) in lv.iter_mut().zip(pos.iter().flat_map(|p| [p.x, p.y, p.z])) {
        *d = x;
    }
    // SAFETY: caller contract.
    lv[15] = unsafe { (*s).radius_add };
    h.fill(bf::LIGHTS, &mut lv);
    let mut ext = [0.0f32; 5];
    let mut normal = [0.0f32; 5];
    for (d, x) in ext.iter_mut().zip(lv.iter().skip(16)) {
        *d = *x;
    }
    for (d, x) in normal.iter_mut().zip(lv.iter().skip(21)) {
        *d = *x;
    }
    let (e, n) = helpers::light_combine(lantern, &ext, &normal);
    // SAFETY: caller contract.
    unsafe {
        (*s).extended = e;
        (*s).normal = n;
    }
}

/// # Safety
/// `s` valid.
pub unsafe fn light_reset(s: *mut LightState) {
    // SAFETY: caller contract.
    let s = unsafe { &mut *s };
    s.extended = 1.0;
    s.normal = 1.0;
    s.update_count = 0.0;
}

// Darkness

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct DarkState {
    pub k: Darkness,
    pub active: u32,
}

/// # Safety
/// As `collapse_update`.
pub unsafe fn dark_update(s: *mut DarkState, h: &HostB, dt: f32) {
    // SAFETY: caller contract.
    if unsafe { (*s).active } == 0 {
        return;
    }
    let mut lv = [0.0f32; 2];
    h.fill(bf::DARK_LEVELS, &mut lv);
    h.hop(bop::AMBIENT_POS, 0.0);
    let i = DarknessIn {
        ext_level: lv[0],
        normal_level: lv[1],
        hard_mode: h.qb(bq::HARD_MODE),
        flashback_active: h.qb(bq::FLASHBACK_ACTIVE),
        drain_disabled: h.qb(bq::DRAIN_DISABLED),
        emotion_flash: h.qb(bq::EMOTION_FLASH),
    };
    // SAFETY: caller contract.
    let mut st = unsafe { (*s).k };
    let mut o = Ops::default();
    helpers::darkness_update(&mut st, &i, dt, &mut o);
    // SAFETY: caller contract.
    unsafe { (*s).k = st };
    h.replay(&o);
}

/// # Safety
/// `s` valid.
pub unsafe fn dark_reset(s: *mut DarkState) {
    // SAFETY: caller contract.
    let s = unsafe { &mut *s };
    s.active = 1;
    s.k.ambient_on = 0;
    s.k.in_darkness = 0;
    s.k.loop_sound_count = 0.0;
    s.k.sanity_loss_mul = 0.0;
    s.k.show_hint_timer = 0.0;
}

/// # Safety
/// `s` valid.
pub unsafe fn dark_map_enter(s: *mut DarkState) {
    // SAFETY: caller contract.
    let s = unsafe { &mut *s };
    s.k.ambient_on = 0;
    s.k.in_darkness = 0;
}

/// # Safety
/// As `collapse_update`.
pub unsafe fn dark_set_active(s: *mut DarkState, h: &HostB, active: bool) {
    // SAFETY: caller contract.
    let on = unsafe {
        (*s).active = u32::from(active);
        (*s).k.ambient_on != 0
    };
    if active {
        return;
    }
    if on {
        // SAFETY: caller contract.
        unsafe { (*s).k.ambient_on = 0 };
        h.op(&Op {
            kind: ok::AMBIENT_OFF,
            ..Op::default()
        });
    }
    // SAFETY: caller contract.
    unsafe {
        (*s).k.loop_sound_count = 0.0;
        (*s).k.in_darkness = 0;
        (*s).k.sanity_loss_mul = 0.0;
    }
}

// Lantern

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct LanternState {
    pub k: Lantern,
    pub disabled: u32,
}

/// # Safety
/// As `collapse_update`.
pub unsafe fn lantern_update(s: *mut LanternState, h: &HostB, dt: f32) {
    // SAFETY: caller contract.
    let active = unsafe { (*s).k.active } != 0;
    let emotion = active && h.qb(bq::EMOTION_FLASH);
    let oil = h.q(bq::OIL);
    // SAFETY: caller contract.
    let mut st = unsafe { (*s).k };
    let mut o = Ops::default();
    if !helpers::lantern_update(&mut st, oil, emotion, dt, &mut o) {
        return;
    }
    // SAFETY: caller contract; the original wrote back the alpha only.
    unsafe { (*s).k.alpha = st.alpha };
    h.replay(&o);
    h.hop(bop::LANTERN_MATRIX, 0.0);
}

/// # Safety
/// As `collapse_update`.
pub unsafe fn lantern_set_active(
    s: *mut LanternState,
    h: &HostB,
    active: bool,
    use_effects: bool,
    check_oil_items: bool,
    check_allowed: bool,
) {
    // SAFETY: caller contract.
    let (current, disabled) = unsafe { ((*s).k.active != 0, (*s).disabled != 0) };
    let p = LanternSet {
        active,
        use_effects,
        check_oil_items,
        check_allowed,
        current,
        allowed: !check_allowed || h.qb(bq::ALLOWED),
        has_item: !check_oil_items || h.qb(bq::HAS_ITEM),
        disabled,
        oil: h.q(bq::OIL),
    };
    let mut o = Ops::default();
    helpers::lantern_set_active(&p, &mut o);
    for op in o.ops.iter().take(o.n as usize) {
        if op.kind == ok::LANTERN_SET_ACTIVE {
            // SAFETY: caller contract.
            unsafe { (*s).k.active = u32::from(op.i != 0) };
        } else {
            h.op(op);
        }
    }
}

/// # Safety
/// As `collapse_update`.
pub unsafe fn lantern_set_disabled(s: *mut LanternState, h: &HostB, disabled: bool) {
    // SAFETY: caller contract.
    unsafe { (*s).disabled = u32::from(disabled) };
    if disabled {
        // SAFETY: caller contract.
        unsafe { lantern_set_active(s, h, false, true, true, true) };
    }
}

/// # Safety
/// `s` valid.
pub unsafe fn lantern_reset(s: *mut LanternState) {
    // SAFETY: caller contract.
    let s = unsafe { &mut *s };
    s.disabled = 0;
    s.k.active = 0;
    s.k.alpha = 0.0;
}

macro_rules! ffi {
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

ffi!(ohpl_a2b_collapse_update, collapse_update, Collapse);
ffi!(ohpl_a2b_camdir_update, camdir_update, CamDirState);
ffi!(ohpl_a2b_hurt_update, hurt_update, HurtState);
ffi!(ohpl_a2b_lookat_update, lookat_update, LookAtState);
ffi!(ohpl_a2b_flashback_update, flashback_update, Flashback);
ffi!(ohpl_a2b_light_update, light_update, LightState);
ffi!(ohpl_a2b_dark_update, dark_update, DarkState);
ffi!(ohpl_a2b_lantern_update, lantern_update, LanternState);

/// # Safety
/// `s` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2b_collapse_clear(
    s: *mut Collapse,
    active: bool,
    reset_random: bool,
) {
    // SAFETY: caller contract.
    unsafe { collapse_clear(s, active, reset_random) };
}

/// # Safety
/// `s` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2b_camdir_add(s: *mut CamDirState, yaw: bool, x: f32) -> f32 {
    // SAFETY: caller contract.
    unsafe { camdir_add(s, yaw, x) }
}

/// # Safety
/// `s` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2b_camdir_reset(s: *mut CamDirState) {
    // SAFETY: caller contract.
    unsafe { camdir_reset(s) };
}

/// # Safety
/// `s` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2b_hurt_reset(s: *mut HurtState) {
    // SAFETY: caller contract.
    unsafe { hurt_reset(s) };
}

/// # Safety
/// `s` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2b_lookat_reset(s: *mut LookAtState) {
    // SAFETY: caller contract.
    unsafe { lookat_reset(s) };
}

/// # Safety
/// `s` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2b_lookat_set_target(
    s: *mut LookAtState,
    x: f32,
    y: f32,
    z: f32,
    speed_mul: f32,
    max_speed: f32,
) {
    // SAFETY: caller contract.
    unsafe { lookat_set_target(s, Vector3f { x, y, z }, speed_mul, max_speed) };
}

/// # Safety
/// `s` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2b_lookat_set_active(s: *mut LookAtState, active: bool) {
    // SAFETY: caller contract.
    unsafe { lookat_set_active(s, active) };
}

/// # Safety
/// `s` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2b_flashback_reset(s: *mut Flashback) {
    // SAFETY: caller contract.
    unsafe { flashback_reset(s) };
}

/// # Safety
/// `s` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2b_light_reset(s: *mut LightState) {
    // SAFETY: caller contract.
    unsafe { light_reset(s) };
}

/// # Safety
/// `s` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2b_light_map_enter(s: *mut LightState) {
    // SAFETY: caller contract.
    unsafe { (*s).update_count = 0.0 };
}

/// # Safety
/// `s` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2b_dark_reset(s: *mut DarkState) {
    // SAFETY: caller contract.
    unsafe { dark_reset(s) };
}

/// # Safety
/// `s` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2b_dark_map_enter(s: *mut DarkState) {
    // SAFETY: caller contract.
    unsafe { dark_map_enter(s) };
}

/// # Safety
/// `s` and `h` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2b_dark_set_active(
    s: *mut DarkState,
    h: *const HostB,
    active: bool,
) {
    // SAFETY: caller contract.
    unsafe { dark_set_active(s, &*h, active) };
}

/// # Safety
/// `s` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2b_dark_in_darkness(s: *const DarkState, ext: f32) -> bool {
    // SAFETY: caller contract.
    ext <= unsafe { (*s).k.min_darkness_light_level }
}

/// # Safety
/// `s` and `h` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2b_lantern_set_active(
    s: *mut LanternState,
    h: *const HostB,
    active: bool,
    use_effects: bool,
    check_oil_items: bool,
    check_allowed: bool,
) {
    // SAFETY: caller contract.
    unsafe { lantern_set_active(s, &*h, active, use_effects, check_oil_items, check_allowed) };
}

/// # Safety
/// `s` and `h` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2b_lantern_set_disabled(
    s: *mut LanternState,
    h: *const HostB,
    disabled: bool,
) {
    // SAFETY: caller contract.
    unsafe { lantern_set_disabled(s, &*h, disabled) };
}

/// # Safety
/// `s` valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_a2b_lantern_reset(s: *mut LanternState) {
    // SAFETY: caller contract.
    unsafe { lantern_reset(s) };
}
