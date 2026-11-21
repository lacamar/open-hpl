//! `iLuxEnemyLoader::AfterLoad` variable reading: base properties, hard-mode scaling, attack data.
#![allow(clippy::indexing_slicing)] // fixed-size config arrays, indices are 0/1

use crate::math::to_rad;
use crate::resources::vars::Vars;

const BIPED: usize = 0;
const WALK: usize = 0;
const RUN: usize = 1;

pub(super) const SLOT_DANGER_MUSIC: i32 = 0;
pub(super) const SLOT_MUSIC: i32 = 1;
pub(super) const SLOT_HIT_PS: i32 = 3;
pub(super) const SLOT_HIT_SOUND: i32 = 7;
pub(super) const SLOT_AMBIENT: i32 = 11;
pub(super) const SLOT_ATTACK: i32 = 14;

#[repr(C)]
#[derive(Default)]
pub struct AeAttack {
    pub min_damage: f32,
    pub max_damage: f32,
    pub force: f32,
    pub max_impulse: f32,
    pub strength: i32,
    pub size: [f32; 3],
    pub offset: [f32; 3],
}

#[repr(C)]
#[derive(Default)]
pub struct AeBody {
    pub size: [f32; 3],
    pub mass: f32,
    pub accurate_climbing: bool,
    pub max_no_slide_slope: f32,
    pub max_push_mass: f32,
    pub push_force: f32,
    pub char_max_push_mass: f32,
    pub char_push_force: f32,
    pub max_step_size: f32,
    pub step_climb_speed: f32,
    pub offset_rot: [f32; 3],
    pub offset_trans: [f32; 3],
}

#[repr(C)]
#[derive(Default)]
pub struct AeCfg {
    pub health: f32,
    pub sight_range: f32,
    pub darkness_sight_range: f32,
    pub hear_volume: f32,
    pub fov: f32,
    pub fov_x_mul: f32,
    pub max_regen_health: f32,
    pub regen_health_speed: f32,
    pub activation_distance: f32,
    pub danger_music_prio: i32,
    pub music_prio: [i32; 2],
    pub toughness: i32,
    pub auto_remove_at_path_end: bool,
    pub search_max_angle: f32,
    pub search_min_dist_mul: f32,
    pub search_max_dist_mul: f32,
    pub search_time: f32,
    pub patrol_max_angle: f32,
    pub patrol_min_dist: f32,
    pub patrol_max_dist: f32,
    pub def_forward_speed: [[f32; 2]; 2],
    pub def_backward_speed: [[f32; 2]; 2],
    pub def_forward_acc: [[f32; 2]; 2],
    pub def_forward_deacc: [[f32; 2]; 2],
    pub turn_speed_mul: f32,
    pub turn_max_speed: f32,
    pub turn_min_break_angle: f32,
    pub turn_break_mul: f32,
    pub move_speed_anim_mul: f32,
    pub stopped_to_walk: [f32; 2],
    pub walk_to_stopped: [f32; 2],
    pub walk_to_run: [f32; 2],
    pub run_to_walk: [f32; 2],
    pub water_step_walk: f32,
    pub water_step_run: f32,
    pub water_step_misc: f32,
    pub normal_attack_distance: f32,
    pub ambient_min: [f32; 4],
    pub ambient_max: [f32; 4],
    pub body: AeBody,
    pub normal: AeAttack,
    pub break_door: AeAttack,
}

const _: () = assert!(std::mem::size_of::<AeCfg>() == 408);

type Put = unsafe extern "C" fn(*mut std::ffi::c_void, i32, *const u8, usize);

struct Rd<'a> {
    v: &'a Vars,
    put: Put,
    ctx: *mut std::ffi::c_void,
}

impl Rd<'_> {
    fn f(&self, n: &str, d: f32) -> f32 {
        self.v.get_float(n.as_bytes(), d)
    }
    fn i(&self, n: &str, d: i32) -> i32 {
        self.v.get_int(n.as_bytes(), d)
    }
    fn s(&self, slot: i32, n: &str, d: &[u8]) {
        let s = self.v.get(n.as_bytes()).unwrap_or(d);
        // SAFETY: `put` and `ctx` come from the caller of `ohpl_ae_load_base`.
        unsafe { (self.put)(self.ctx, slot, s.as_ptr(), s.len()) }
    }
    fn vec<const N: usize>(&self, n: &str, d: f32) -> [f32; N] {
        let mut o = [0.0; N];
        if self.v.get_floats(n.as_bytes(), &mut o) {
            o
        } else {
            [d; N]
        }
    }
    fn attack(&self, prefix: &str, slot: i32, hard: bool) -> AeAttack {
        let dmg = self.vec::<2>(&format!("{prefix}AttackDamage"), 0.0);
        let force = self.vec::<2>(&format!("{prefix}AttackForce"), 0.0);
        self.s(slot, &format!("{prefix}AttackHitSound"), b"");
        self.s(slot + 1, &format!("{prefix}DamageType"), b"bloodsplat");
        let mul = if hard { 2.0 } else { 1.0 };
        AeAttack {
            min_damage: dmg[0] * mul,
            max_damage: dmg[1] * mul,
            force: force[0],
            max_impulse: force[1],
            strength: self.i(&format!("{prefix}AttackStrength"), 0),
            size: self.vec(&format!("{prefix}DamageSize"), 0.0),
            offset: self.vec(&format!("{prefix}DamageOffset"), 0.0),
        }
    }
}

fn scale(c: &mut AeCfg) {
    c.sight_range *= 1.15;
    c.darkness_sight_range *= 1.15;
    c.hear_volume *= 0.6;
    c.fov *= 1.1;
    c.fov_x_mul *= 1.1;
    c.search_max_angle *= 1.1;
    c.search_min_dist_mul *= 1.25;
    c.search_max_dist_mul *= 1.25;
    c.search_time *= 2.5;
    c.patrol_max_angle *= 1.25;
    c.patrol_min_dist *= 1.5;
    c.patrol_max_dist *= 1.5;
    for (m, k) in [(WALK, 1.1), (RUN, 1.2)] {
        c.def_forward_speed[BIPED][m] *= k;
        c.def_backward_speed[BIPED][m] *= k;
        c.def_forward_acc[BIPED][m] *= k;
        c.def_forward_deacc[BIPED][m] *= k;
    }
    c.turn_speed_mul *= 1.35;
    c.water_step_walk *= 1.5;
    c.water_step_run *= 1.5;
}

/// # Safety
/// `v` and `out` are live; `put(ctx, slot, bytes, len)` copies the bytes before it returns.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ae_load_base(
    v: *const Vars,
    hard_mode: bool,
    out: *mut AeCfg,
    put: Put,
    ctx: *mut std::ffi::c_void,
) {
    // SAFETY: caller contract.
    let (v, out) = unsafe { (&*v, &mut *out) };
    let r = Rd { v, put, ctx };
    let c = out;
    c.health = r.f("Health", 100.0);
    c.toughness = r.i("Toughness", 0);
    c.sight_range = r.f("SightRange", 0.0);
    c.darkness_sight_range = r.f("DarknessSightRange", 0.0);
    c.hear_volume = r.f("HearVolume", 0.0);
    c.fov = to_rad(r.f("FOV", 0.0));
    c.fov_x_mul = r.f("FOVXMul", 1.0);
    c.max_regen_health = r.f("MaxRegenHealth", 100.0);
    c.regen_health_speed = r.f("RegenHealthSpeed", 0.0);
    c.activation_distance = r.f("ActivationDistance", 0.0);
    r.s(SLOT_DANGER_MUSIC, "DangerMusic", b"");
    c.danger_music_prio = r.i("DangerMusicPrio", 0);
    r.s(SLOT_MUSIC, "SearchMusic", b"");
    c.music_prio[0] = r.i("SearchMusicPrio", 0);
    r.s(SLOT_MUSIC + 1, "AttackMusic", b"");
    c.music_prio[1] = r.i("AttackMusicPrio", 0);
    c.auto_remove_at_path_end = r.v.get_bool(b"AutoRemoveAtPathEnd", true);
    c.search_max_angle = to_rad(r.f("PlayerSearchMaxAngle", 0.0));
    c.search_min_dist_mul = r.f("PlayerSearchMinDistMul", 0.0);
    c.search_max_dist_mul = r.f("PlayerSearchMaxDistMul", 0.0);
    c.search_time = r.f("PlayerSearchTime", 0.0);
    c.patrol_max_angle = to_rad(r.f("PlayerPatrolMaxAngle", 0.0));
    c.patrol_min_dist = r.f("PlayerPatrolMinDist", 0.0);
    c.patrol_max_dist = r.f("PlayerPatrolMaxDist", 0.0);

    let b = &mut c.body;
    b.size = r.vec("Body_Size", 1.0);
    b.mass = r.f("Body_Mass", 1.0);
    b.accurate_climbing = r.v.get_bool(b"Body_AccurateClimbing", false);
    b.max_no_slide_slope = to_rad(r.f("Body_MaxNoSlideSlopeAngle", 0.0));
    b.max_push_mass = r.f("Body_MaxPushMass", 0.0);
    b.push_force = r.f("Body_PushForce", 0.0);
    b.char_max_push_mass = r.f("Body_CharacterMaxPushMass", 0.0);
    b.char_push_force = r.f("Body_CharacterPushForce", 0.0);
    b.max_step_size = r.f("Body_MaxStepSize", 0.0);
    b.step_climb_speed = r.f("Body_StepClimbSpeed", 0.0);
    b.offset_rot = r.vec::<3>("Body_OffsetRot", 0.0).map(to_rad);
    b.offset_trans = r.vec("Body_OffsetTrans", 0.0);

    for (m, p) in [(WALK, "Walk"), (RUN, "Run")] {
        c.def_forward_speed[BIPED][m] = r.f(&format!("{p}_ForwardSpeed"), 0.0);
        c.def_backward_speed[BIPED][m] = r.f(&format!("{p}_BackwardSpeed"), 0.0);
        c.def_forward_acc[BIPED][m] = r.f(&format!("{p}_ForwardAcc"), 0.0);
        c.def_forward_deacc[BIPED][m] = r.f(&format!("{p}_ForwardDeacc"), 0.0);
    }
    c.def_forward_speed[1] = [0.0; 2];
    c.def_backward_speed[1] = [0.0; 2];
    c.def_forward_acc[1] = [0.0; 2];
    c.def_forward_deacc[1] = [0.0; 2];

    c.turn_speed_mul = r.f("TurnSpeedMul", 1.0);
    c.turn_max_speed = r.f("TurnMaxSpeed", 1.0);
    c.turn_min_break_angle = to_rad(r.f("TurnMinBreakAngle", 1.0));
    c.turn_break_mul = r.f("TurnBreakMul", 1.0);
    c.move_speed_anim_mul = r.f("MoveSpeedAnimMul", 1.0);
    c.stopped_to_walk = [r.f("StoppedToWalkSpeed", 0.0), 0.0];
    c.walk_to_stopped = [r.f("WalkToStoppedSpeed", 0.0), 0.0];
    c.walk_to_run = [r.f("WalkToRunSpeed", 0.0), 0.0];
    c.run_to_walk = [r.f("RunToWalkSpeed", 0.0), 0.0];
    c.water_step_walk = r.f("WaterStepSpeedWalk", 0.0);
    c.water_step_run = r.f("WaterStepSpeedRun", 0.0);
    c.water_step_misc = r.f("WaterStepSpeedMisc", 0.0);

    // Sword, Club, Bullet (reads the Club variables), Dud
    for (k, n) in ["Sword", "Club", "Club", "Dud"].into_iter().enumerate() {
        r.s(SLOT_HIT_PS + k as i32, &format!("HitPS_{n}"), b"");
        r.s(SLOT_HIT_SOUND + k as i32, &format!("HitSound_{n}"), b"");
    }
    c.normal_attack_distance = r.f("NormalAttackDistance", 0.0);

    if hard_mode {
        scale(c);
    }

    c.normal = r.attack("Normal", SLOT_ATTACK, hard_mode);
    c.break_door = r.attack("BreakDoor", SLOT_ATTACK + 2, hard_mode);

    for (k, n) in ["Idle", "Alert", "Hunt"].into_iter().enumerate() {
        r.s(SLOT_AMBIENT + k as i32, &format!("AmbientSound_{n}"), b"");
        c.ambient_min[k + 1] = r.f(&format!("AmbientMinTime_{n}"), 0.0);
        c.ambient_max[k + 1] = r.f(&format!("AmbientMaxTime_{n}"), 0.0);
    }
}

#[repr(C)]
#[derive(Default)]
pub struct AePig {
    pub groggy_damage_count: f32,
    pub alert_to_hunt_distance: f32,
    pub alert_to_instant_hunt_distance: f32,
    pub hunt_pause_min_time: f32,
    pub hunt_pause_max_time: f32,
    pub increase_alert_speed_distance: f32,
    pub increased_alert_speed_mul: f32,
    pub alert_run_towards_to_hunt_limit: f32,
    pub alert_run_towards_check_distance: f32,
    pub idle_extra_time_min: f32,
    pub idle_extra_time_max: f32,
    pub idle_extra_num: i32,
    pub is_tesla: bool,
}

const _: () = assert!(std::mem::size_of::<AePig>() == 52);

/// Grunt (`quadruped == false`) and ManPig variables of `LoadVariables`.
///
/// # Safety
/// `v`, `cfg` and `out` are live.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_ae_load_pig(
    v: *const Vars,
    manpig: bool,
    cfg: *mut AeCfg,
    out: *mut AePig,
) {
    // SAFETY: caller contract.
    let (v, c, o) = unsafe { (&*v, &mut *cfg, &mut *out) };
    let g = |n: &str, d: f32| v.get_float(n.as_bytes(), d);
    if manpig {
        const QUAD: usize = 1;
        for (m, p) in [(WALK, "Walk"), (RUN, "Run")] {
            c.def_forward_speed[QUAD][m] = g(&format!("Quadruped_{p}_ForwardSpeed"), 0.0);
            c.def_backward_speed[QUAD][m] = g(&format!("Quadruped_{p}_BackwardSpeed"), 0.0);
            c.def_forward_acc[QUAD][m] = g(&format!("Quadruped_{p}_ForwardAcc"), 0.0);
            c.def_forward_deacc[QUAD][m] = g(&format!("Quadruped_{p}_ForwardDeacc"), 0.0);
        }
        c.stopped_to_walk[QUAD] = g("Quadruped_StoppedToWalkSpeed", 0.0);
        c.walk_to_stopped[QUAD] = g("Quadruped_WalkToStoppedSpeed", 0.0);
        c.walk_to_run[QUAD] = g("Quadruped_WalkToRunSpeed", 0.0);
        c.run_to_walk[QUAD] = g("Quadruped_RunToWalkSpeed", 0.0);
        o.is_tesla = v.get_bool(b"IsTelsa", false);
    } else {
        o.idle_extra_time_min = g("IdleExtraTimeMin", 2.0);
        o.idle_extra_time_max = g("IdleExtraTimeMax", 6.0);
        o.idle_extra_num = v.get_int(b"IdleExtraNum", 3);
    }
    o.groggy_damage_count = g("GroggyDamageCount", 0.0);
    o.alert_to_hunt_distance = g("AlertToHuntDistance", 0.0);
    o.alert_to_instant_hunt_distance = g("AlertToInstantHuntDistance", 0.0);
    o.hunt_pause_min_time = g("HuntPauseMinTime", 0.0);
    o.hunt_pause_max_time = g("HuntPauseMaxTime", 0.0);
    o.increase_alert_speed_distance = g("IncreaseAlertSpeedDistance", 0.0);
    o.increased_alert_speed_mul = g("IncreasedAlertSpeedMul", 0.0);
    o.alert_run_towards_to_hunt_limit = g("AlertRunTowardsToHuntLimit", 0.0);
    o.alert_run_towards_check_distance = g("AlertRunTowardsCheckDistance", 0.0);
}

#[cfg(test)]
mod tests {
    use super::*;

    unsafe extern "C" fn sink(c: *mut std::ffi::c_void, slot: i32, p: *const u8, n: usize) {
        // SAFETY: test passes a `Vec<(i32, Vec<u8>)>`.
        let v = unsafe { &mut *(c as *mut Vec<(i32, Vec<u8>)>) };
        v.push((slot, unsafe { std::slice::from_raw_parts(p, n) }.to_vec()));
    }

    fn load(vars: &Vars, hard: bool) -> (AeCfg, Vec<(i32, Vec<u8>)>) {
        let mut cfg = AeCfg::default();
        let mut got: Vec<(i32, Vec<u8>)> = Vec::new();
        // SAFETY: all pointers are live locals.
        unsafe { ohpl_ae_load_base(vars, hard, &mut cfg, sink, (&raw mut got).cast()) };
        (cfg, got)
    }

    #[test]
    fn defaults_and_hard_mode() {
        let mut v = Vars::default();
        v.set(b"SightRange", b"10");
        v.set(b"Walk_ForwardSpeed", b"2");
        v.set(b"NormalAttackDamage", b"5 7");
        v.set(b"HitSound_Club", b"club.snt");
        let (c, got) = load(&v, false);
        assert_eq!((c.health, c.sight_range, c.fov_x_mul), (100.0, 10.0, 1.0));
        assert_eq!((c.normal.min_damage, c.normal.max_damage), (5.0, 7.0));
        assert_eq!(c.body.size, [1.0; 3]);
        assert!(c.auto_remove_at_path_end);
        let sound = |slot: i32| got.iter().find(|g| g.0 == slot).map(|g| g.1.clone());
        assert_eq!(sound(SLOT_HIT_SOUND + 1).as_deref(), Some(&b"club.snt"[..]));
        assert_eq!(sound(SLOT_HIT_SOUND + 2).as_deref(), Some(&b"club.snt"[..]));
        assert_eq!(sound(SLOT_ATTACK + 1).as_deref(), Some(&b"bloodsplat"[..]));
        assert_eq!(got.len(), 18);

        let (h, _) = load(&v, true);
        assert_eq!(h.sight_range, 10.0 * 1.15);
        assert_eq!(h.def_forward_speed[0][0], 2.0 * 1.1);
        assert_eq!((h.normal.min_damage, h.normal.max_damage), (10.0, 14.0));
    }

    #[test]
    fn pig_variables() {
        let mut v = Vars::default();
        v.set(b"Quadruped_Run_ForwardAcc", b"3");
        v.set(b"IsTelsa", b"true");
        let mut cfg = AeCfg::default();
        let mut pig = AePig::default();
        // SAFETY: live locals.
        unsafe { ohpl_ae_load_pig(&v, true, &mut cfg, &mut pig) };
        assert_eq!(cfg.def_forward_acc[1][1], 3.0);
        assert!(pig.is_tesla);
        // SAFETY: live locals.
        unsafe { ohpl_ae_load_pig(&v, false, &mut cfg, &mut pig) };
        assert_eq!((pig.idle_extra_time_min, pig.idle_extra_num), (2.0, 3));
    }
}
