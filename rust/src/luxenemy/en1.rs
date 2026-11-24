//! Data of the Grunt and WaterLurker shell `cLuxEnemyRs`: loader variables, the pointers the
//! Rust handlers read, and the save data state fix-up. The shell keeps a pointer to a [`Pod`].

use super::ctx::{Ctx, P};
use super::loader::{AeCfg, AePig, ohpl_ae_load_pig};
use super::waterlurker::WlCtx;
use crate::resources::vars::Vars;
use std::ptr::null_mut;

const STATE_PIG_START: i32 = 17;
const STATE_LAST: i32 = 21;
const SPLASH: [&str; 4] = ["Walk", "Run", "Eat", "Attack"];

pub struct Pod {
    wait_time: f32,
    alert_run_towards_count: f32,
    pig: AePig,
    wl: WlCtx,
    height: f32,
    eat_damage: f32,
    after_attack: f32,
    /// Slots 0-2 (Grunt): notice, give up, enabled sound.
    grunt: [Vec<u8>; 3],
    /// Slots 3-6 (WaterLurker): splash particle systems.
    ps: [Vec<u8>; 4],
    /// Slots 7-10 (WaterLurker): splash sounds.
    snd: [Vec<u8>; 4],
}

/// # Safety
/// `ctx` is the live `OhplGa3Ctx` of the enemy; `sanity` points at its sanity flag.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en1_new(wl: bool, ctx: *mut Ctx, sanity: P<bool>) -> *mut Pod {
    let mut p = Box::new(Pod {
        wait_time: 0.0,
        alert_run_towards_count: 0.0,
        pig: AePig::default(),
        wl: WlCtx {
            player_detection_height: null_mut(),
            after_attack_pause: null_mut(),
            causes_sanity_decrease: sanity,
        },
        height: 0.0,
        eat_damage: 0.0,
        after_attack: 0.0,
        grunt: Default::default(),
        ps: Default::default(),
        snd: Default::default(),
    });
    // SAFETY: caller contract; the box is never moved, so the pointers stay valid until `ohpl_en1_free`.
    let c = unsafe { &mut *ctx };
    if wl {
        p.wl.player_detection_height = &mut p.height;
        p.wl.after_attack_pause = &mut p.after_attack;
    } else {
        c.wait_time = &mut p.wait_time;
        c.alert_run_towards_count = &mut p.alert_run_towards_count;
        c.groggy_damage_count = &mut p.pig.groggy_damage_count;
        c.alert_to_hunt_distance = &mut p.pig.alert_to_hunt_distance;
        c.alert_to_instant_hunt_distance = &mut p.pig.alert_to_instant_hunt_distance;
        c.hunt_pause_min_time = &mut p.pig.hunt_pause_min_time;
        c.hunt_pause_max_time = &mut p.pig.hunt_pause_max_time;
        c.increase_alert_speed_distance = &mut p.pig.increase_alert_speed_distance;
        c.increased_alert_speed_mul = &mut p.pig.increased_alert_speed_mul;
        c.alert_run_towards_to_hunt_limit = &mut p.pig.alert_run_towards_to_hunt_limit;
        c.alert_run_towards_check_distance = &mut p.pig.alert_run_towards_check_distance;
        c.idle_extra_time_min = &mut p.pig.idle_extra_time_min;
        c.idle_extra_time_max = &mut p.pig.idle_extra_time_max;
        c.idle_extra_num = &mut p.pig.idle_extra_num;
    }
    Box::into_raw(p)
}

/// # Safety
/// `p` came from `ohpl_en1_new` and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en1_free(p: *mut Pod) {
    if !p.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(p) });
    }
}

/// `LoadVariables` of the Grunt or the WaterLurker.
///
/// # Safety
/// `p`, `v` and `cfg` are live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en1_load(p: *mut Pod, wl: bool, v: *const Vars, cfg: *mut AeCfg) {
    // SAFETY: caller contract.
    let (p, v) = unsafe { (&mut *p, &*v) };
    let s = |n: &str| v.get(n.as_bytes()).unwrap_or_default().to_vec();
    if !wl {
        for (d, n) in p
            .grunt
            .iter_mut()
            .zip(["NoticeSound", "GiveUpNoticeSound", "EnabledSound"])
        {
            *d = s(n);
        }
        // SAFETY: `cfg` live, `p.pig` is a field of the live pod.
        unsafe { ohpl_ae_load_pig(v, false, cfg, &mut p.pig) };
        return;
    }
    p.height = v.get_float(b"PlayerDetectionHeight", 0.6);
    p.eat_damage = v.get_float(b"EatDamage", 0.0);
    p.after_attack = v.get_float(b"AfterAttackPauseTime", 0.0);
    for ((n, ps), snd) in SPLASH.iter().zip(&mut p.ps).zip(&mut p.snd) {
        *ps = s(&format!("SplashPS_{n}"));
        *snd = s(&format!("SplashSound_{n}"));
    }
}

/// The string in a slot (grunt, then splash particle systems, then splash sounds); the bytes stay valid until the next load.
///
/// # Safety
/// `p` live, `len` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en1_str(p: *const Pod, slot: usize, len: *mut usize) -> *const u8 {
    // SAFETY: caller contract.
    let (p, len) = unsafe { (&*p, &mut *len) };
    let s = p
        .grunt
        .iter()
        .chain(&p.ps)
        .chain(&p.snd)
        .nth(slot)
        .map_or(&[][..], Vec::as_slice);
    *len = s.len();
    s.as_ptr()
}

/// # Safety
/// `p` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en1_wl_ctx(p: *const Pod) -> *const WlCtx {
    // SAFETY: caller contract.
    unsafe { &(*p).wl }
}

/// # Safety
/// `p` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en1_height(p: *const Pod) -> f32 {
    // SAFETY: caller contract.
    unsafe { (*p).height }
}

/// # Safety
/// `p` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en1_set_height(p: *mut Pod, h: f32) {
    // SAFETY: caller contract.
    unsafe { (*p).height = h }
}

/// # Safety
/// `p` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en1_eat_damage(p: *const Pod) -> f32 {
    // SAFETY: caller contract.
    unsafe { (*p).eat_damage }
}

/// `InRangeOfFood`: the food is within the detection height of the feet.
///
/// # Safety
/// `p` live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en1_in_range_of_food(p: *const Pod, y_dist: f32) -> bool {
    // SAFETY: caller contract.
    y_dist.abs() <= unsafe { (*p).height }
}

/// Saves of the other pig types hold states of the pig range; these enemies cannot be in it.
///
/// # Safety
/// The three pointers are live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_en1_fix_states(cur: *mut i32, next: *mut i32, prev: *mut i32) {
    for s in [cur, next, prev] {
        // SAFETY: caller contract.
        let s = unsafe { &mut *s };
        if *s >= STATE_PIG_START {
            *s = STATE_LAST;
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::mem::MaybeUninit;

    fn ctx() -> Ctx {
        // SAFETY: `Ctx` holds raw pointers only; all zero is a valid value.
        unsafe { MaybeUninit::<Ctx>::zeroed().assume_init() }
    }

    #[test]
    fn grunt_ctx_points_into_pod_and_loads() {
        let mut v = Vars::default();
        v.set(b"NoticeSound", b"n.snt");
        v.set(b"GroggyDamageCount", b"2.5");
        let (mut c, mut cfg, mut sanity) = (ctx(), AeCfg::default(), true);
        // SAFETY: live locals, pod freed once.
        unsafe {
            let p = ohpl_en1_new(false, &mut c, &mut sanity);
            ohpl_en1_load(p, false, &v, &mut cfg);
            assert_eq!(*c.groggy_damage_count, 2.5);
            assert_eq!(*c.idle_extra_num, 3);
            let mut n = 0;
            let s = ohpl_en1_str(p, 0, &mut n);
            assert_eq!(std::slice::from_raw_parts(s, n), b"n.snt");
            ohpl_en1_free(p);
        }
    }

    #[test]
    fn lurker_loads_and_checks_food_range() {
        let mut v = Vars::default();
        v.set(b"SplashSound_Eat", b"eat.snt");
        let (mut c, mut cfg, mut sanity) = (ctx(), AeCfg::default(), true);
        // SAFETY: live locals, pod freed once.
        unsafe {
            let p = ohpl_en1_new(true, &mut c, &mut sanity);
            ohpl_en1_load(p, true, &v, &mut cfg);
            assert_eq!(ohpl_en1_height(p), 0.6);
            assert!(ohpl_en1_in_range_of_food(p, -0.6));
            assert!(!ohpl_en1_in_range_of_food(p, 0.61));
            let w = &*ohpl_en1_wl_ctx(p);
            ohpl_en1_set_height(p, 1.5);
            assert_eq!(*w.player_detection_height, 1.5);
            let mut n = 0;
            let s = ohpl_en1_str(p, 9, &mut n);
            assert_eq!(std::slice::from_raw_parts(s, n), b"eat.snt");
            ohpl_en1_free(p);
        }
    }

    #[test]
    fn pig_states_are_reset() {
        let (mut a, mut b, mut c) = (3, 17, 20);
        // SAFETY: live locals.
        unsafe { ohpl_en1_fix_states(&mut a, &mut b, &mut c) };
        assert_eq!((a, b, c), (3, STATE_LAST, STATE_LAST));
    }
}
