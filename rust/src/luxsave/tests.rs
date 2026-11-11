use super::engine;
use super::env::{Env, JOINT_BALL, Op};
use super::game;
use super::gamemap;
use super::map;
use super::names::{self, Date, NameState};
use std::collections::HashMap;

struct Script {
    vals: HashMap<(u32, i32, i32), i32>,
    any: HashMap<u32, i32>,
    log: Vec<(Op, i32, i32)>,
}

impl Script {
    fn new() -> Script {
        Script {
            vals: HashMap::new(),
            any: HashMap::new(),
            log: Vec::new(),
        }
    }

    fn set(&mut self, op: Op, v: i32) -> &mut Script {
        self.any.insert(op as u32, v);
        self
    }

    fn set_at(&mut self, op: Op, a: i32, b: i32, v: i32) -> &mut Script {
        self.vals.insert((op as u32, a, b), v);
        self
    }

    fn ops(&self) -> Vec<Op> {
        self.log.iter().map(|x| x.0).collect()
    }
}

impl Env for Script {
    fn call(&mut self, op: Op, a: i32, b: i32) -> i32 {
        self.log.push((op, a, b));
        self.vals
            .get(&(op as u32, a, b))
            .or_else(|| self.any.get(&(op as u32)))
            .copied()
            .unwrap_or(0)
    }
}

fn w(s: &str) -> Vec<u32> {
    s.bytes().map(u32::from).collect()
}

fn date(y: i32, mo: i32, d: i32, h: i32, mi: i32, s: i32) -> Date {
    Date {
        seconds: s,
        minutes: mi,
        hours: h,
        month_day: d,
        month: mo,
        year: y,
    }
}

#[test]
fn op_numbers_match_header() {
    let rs = include_str!("env.rs");
    let h = include_str!("../../../amnesia/src/game/RustLuxSave.h");
    let in_rs: Vec<(String, u32)> = rs
        .lines()
        .filter_map(|l| {
            let l = l.strip_prefix("    ")?.strip_suffix(',')?;
            let (n, v) = l.split_once(" = ")?;
            Some((n.to_string(), v.parse().ok()?))
        })
        .collect();
    let in_h: Vec<(String, u32)> = h
        .lines()
        .filter_map(|l| {
            let l = l.trim().strip_prefix("eOhplGa6Op_")?.strip_suffix(',')?;
            let (n, v) = l.split_once(" = ")?;
            Some((n.to_string(), v.parse().ok()?))
        })
        .collect();
    assert!(in_rs.len() > 200);
    assert_eq!(in_rs, in_h);
}

#[test]
fn save_name_counts_within_one_second() {
    let mut st = NameState::default();
    let d = date(2026, 10, 5, 7, 8, 9);
    let a = names::save_name(&mut st, &w("AutoSave"), &w("level"), &d);
    assert_eq!(a, w("AutoSave_level_2026_11_5_7_8_9_0.sav"));
    let b = names::save_name(&mut st, &w("AutoSave"), &w("level"), &d);
    assert_eq!(b, w("AutoSave_level_2026_11_5_7_8_9_0.sav"));
    let c = names::save_name(&mut st, &w("AutoSave"), &w("level"), &d);
    assert_eq!(c, w("AutoSave_level_2026_11_5_7_8_9_1.sav"));
    let e = names::save_name(&mut st, &w("AutoSave"), &[], &date(2026, 10, 5, 7, 8, 10));
    assert_eq!(e, w("AutoSave_NULL_2026_11_5_7_8_10_2.sav"));
    let f = names::save_name(
        &mut st,
        &w("AutoSave"),
        &w("x"),
        &date(2026, 10, 5, 7, 8, 10),
    );
    assert_eq!(f, w("AutoSave_x_2026_11_5_7_8_10_0.sav"));
}

#[test]
fn save_name_count_wraps_at_100() {
    let mut st = NameState {
        count: 99,
        latest: date(1, 2, 3, 4, 5, 6),
    };
    let d = date(1, 2, 3, 4, 5, 6);
    names::save_name(&mut st, &[], &w("m"), &d);
    assert_eq!(st.count, 0);
}

#[test]
fn proper_name_formats_nine_parts() {
    let mut tr = |t: &[u32]| {
        let mut o = w("T:");
        o.extend_from_slice(t);
        o
    };
    let n = names::proper_name(&w("AutoSave_lvl_2026_11_5_7_8_9_0.sav"), &mut tr);
    assert_eq!(n, w("AutoSave - T:lvl - 5/11-2026 07:08:09"));
    let bad = names::proper_name(&w("AutoSave_lvl.sav"), &mut tr);
    assert_eq!(bad, w("INVALID SAVE FILE SYNTAX"));
}

#[test]
fn newest_keeps_first_of_equals() {
    let a = date(2026, 1, 1, 1, 1, 1);
    let b = date(2026, 1, 1, 1, 1, 2);
    assert_eq!(names::newest(&[]), None);
    assert_eq!(names::newest(&[a, b, b]), Some(1));
    assert_eq!(names::newest(&[b, a, b]), Some(0));
}

#[test]
fn rotation_removes_oldest_until_below_max() {
    let d = |s| date(2026, 1, 1, 1, 1, s);
    let dates = [d(5), d(1), d(3), d(1), d(9)];
    assert_eq!(names::rotation_victims(&dates, 3), vec![1, 3, 2]);
    assert_eq!(names::rotation_victims(&dates, 6), Vec::<usize>::new());
    assert_eq!(names::rotation_victims(&dates, 0), vec![1, 3, 2, 0, 4]);
    assert_eq!(names::rotation_victims(&dates, -1), vec![1, 3, 2, 0, 4]);
    assert_eq!(names::rotation_victims(&[], 0), Vec::<usize>::new());
}

#[test]
fn hardmode_gate() {
    assert!(names::hardmode_file_ok(&w("HardMode_a_1.sav")));
    assert!(names::hardmode_file_ok(&w("xHardModex")));
    assert!(!names::hardmode_file_ok(&w("AutoSave_1.sav")));
    assert!(!names::hardmode_file_ok(&w("Hard")));
}

#[test]
fn find_map_first_equal() {
    let n: [&[u8]; 3] = [b"a", b"b", b"b"];
    assert_eq!(gamemap::find_map(&n, b"b"), Some(1));
    assert_eq!(gamemap::find_map(&n, b"c"), None);
    assert_eq!(gamemap::find_map(&[], b""), None);
}

#[test]
fn light_from_parentless_fading_writes_dest_first() {
    let mut s = Script::new();
    s.set(Op::LightIsFading, 1);
    engine::light_from(&mut s);
    assert_eq!(
        s.ops(),
        vec![
            Op::LightHasParent,
            Op::LightFromBase,
            Op::LightIsFading,
            Op::LightFlickerActive,
            Op::LightWriteDest,
            Op::LightFlickerActive,
            Op::LightColorCurrent,
            Op::LightFlickerProps
        ]
    );
}

#[test]
fn light_from_flickering_uses_on_color_without_write() {
    let mut s = Script::new();
    s.set(Op::LightIsFading, 1).set(Op::LightFlickerActive, 1);
    engine::light_from(&mut s);
    let ops = s.ops();
    assert!(!ops.contains(&Op::LightWriteDest));
    assert!(ops.contains(&Op::LightColorFlickerOn));
}

#[test]
fn light_with_parent_only_names_the_base() {
    let mut s = Script::new();
    s.set(Op::LightHasParent, 1);
    engine::light_from(&mut s);
    assert_eq!(s.ops(), vec![Op::LightHasParent, Op::LightFromBase]);
    let mut s = Script::new();
    s.set(Op::LightHasParent, 1);
    engine::light_to(&mut s);
    assert_eq!(s.ops(), vec![Op::LightHasParent]);
}

#[test]
fn sound_stopped_short_circuits() {
    let mut s = Script::new();
    s.set(Op::SoundIsStopped, 1);
    engine::sound_from(&mut s);
    assert!(!s.ops().contains(&Op::SoundIsFadingOut));
    assert!(s.log.contains(&(Op::SoundSetStopped, 1, 0)));
}

#[test]
fn ps_from_null_and_emitters() {
    let mut s = Script::new();
    engine::ps_from(&mut s);
    assert_eq!(s.ops(), vec![Op::PsPresent, Op::PsFromNull]);
    let mut s = Script::new();
    s.set(Op::PsPresent, 1).set(Op::PsLiveEmitterN, 3);
    s.set_at(Op::PsEmitterDying, 1, 0, 1)
        .set_at(Op::PsEmitterDead, 2, 0, 1);
    engine::ps_from(&mut s);
    let set: Vec<_> = s
        .log
        .iter()
        .filter(|x| x.0 == Op::PsSetEmitter)
        .map(|x| (x.1, x.2))
        .collect();
    assert_eq!(set, vec![(0, 1), (1, 0), (2, 0)]);
}

#[test]
fn ps_to_skips_emitters_past_the_live_count() {
    let mut s = Script::new();
    s.set(Op::PsPresent, 1)
        .set(Op::PsSavedEmitterN, 4)
        .set(Op::PsLiveEmitterN, 2);
    s.set_at(Op::PsSavedEmitterActive, 0, 0, 1);
    engine::ps_to(&mut s);
    let kills: Vec<_> = s
        .log
        .iter()
        .filter(|x| x.0 == Op::PsKillEmitter)
        .map(|x| x.1)
        .collect();
    assert_eq!(kills, vec![1]);
}

#[test]
fn joint_broken_returns_early() {
    let mut s = Script::new();
    engine::joint_from(&mut s);
    assert_eq!(s.ops(), vec![Op::JointExists, Op::JointSetBroken]);
    let mut s = Script::new();
    s.set(Op::JointExists, 1).set(Op::JointType, JOINT_BALL);
    engine::joint_from(&mut s);
    assert!(s.log.contains(&(Op::JointFromLimits, JOINT_BALL, 0)));
    let mut s = Script::new();
    s.set(Op::JointExists, 1).set(Op::JointType, 255);
    engine::joint_from(&mut s);
    assert!(!s.ops().contains(&Op::JointFromLimits));
}

#[test]
fn mesh_to_needs_equal_counts() {
    let mut s = Script::new();
    s.set(Op::MeshSavedAnimN, 2)
        .set(Op::MeshLiveAnimN, 3)
        .set(Op::MeshSavedSubN, 1)
        .set(Op::MeshLiveSubN, 1);
    engine::mesh_to(&mut s);
    let ops = s.ops();
    assert!(!ops.contains(&Op::MeshAnimTo));
    assert_eq!(ops.iter().filter(|o| **o == Op::MeshSubTo).count(), 1);
}

#[test]
fn rope_to_stops_at_the_shorter_particle_list() {
    let mut s = Script::new();
    s.set(Op::RopeLivePartN, 5)
        .set(Op::RopeSavedPartN, 2)
        .set(Op::RopeFindEndBody, 1);
    engine::rope_to(&mut s);
    let ops = s.ops();
    assert_eq!(ops.iter().filter(|o| **o == Op::RopePartTo).count(), 2);
    assert!(!ops.contains(&Op::RopeAttachStart));
    assert!(ops.contains(&Op::RopeAttachEnd));
}

#[test]
fn effects_skip_commentary_and_debug_volumes() {
    let mut s = Script::new();
    s.set(Op::EffectVolumeCount, 4).set(Op::EffectSavedCount, 4);
    s.set_at(Op::EffectVolumeKind, 2, 0, 1);
    s.set_at(Op::EffectEntryPresent, 0, 1, 1);
    s.set_at(Op::EffectEntryActive, 1, 3, 1);
    engine::effects_from(&mut s);
    let from: Vec<_> = s
        .log
        .iter()
        .filter(|x| matches!(x.0, Op::EffectEntryFrom | Op::EffectEntryInactive))
        .map(|x| (x.0, x.1, x.2))
        .collect();
    assert_eq!(from.len(), 6);
    assert!(from.contains(&(Op::EffectEntryFrom, 0, 1)));
    assert!(from.contains(&(Op::EffectEntryInactive, 1, 1)));
    assert!(!from.iter().any(|x| x.2 == 2));
    let mut s = Script::new();
    s.set(Op::EffectSavedCount, 4);
    s.set_at(Op::EffectVolumeKind, 2, 0, 1);
    s.set_at(Op::EffectEntryActive, 1, 3, 1);
    engine::effects_to(&mut s);
    let apply = s.log.iter().filter(|x| x.0 == Op::EffectEntryApply).count();
    let destroy = s
        .log
        .iter()
        .filter(|x| x.0 == Op::EffectEntryDestroy)
        .count();
    assert_eq!((apply, destroy), (1, 5));
}

#[test]
fn music_from_files_searchers_under_attackers() {
    let mut s = Script::new();
    s.set_at(Op::MusicEnemyN, 0, 0, 1)
        .set_at(Op::MusicEnemyN, 1, 0, 2);
    s.set_at(Op::MusicEnemyId, 0, 0, 10)
        .set_at(Op::MusicEnemyId, 1, 0, 20)
        .set_at(Op::MusicEnemyId, 1, 1, 21);
    game::music_from(&mut s);
    let ids: Vec<_> = s
        .log
        .iter()
        .filter(|x| x.0 == Op::MusicAddAttackerId)
        .map(|x| x.1)
        .collect();
    assert_eq!(ids, vec![10, 20, 21]);
}

#[test]
fn music_to_searcher_warning_shows_the_attacker_id() {
    let mut s = Script::new();
    s.set_at(Op::MusicSavedIdN, 0, 0, 1)
        .set_at(Op::MusicSavedIdN, 1, 0, 2);
    s.set_at(Op::MusicSavedId, 0, 0, 7)
        .set_at(Op::MusicSavedId, 1, 0, 8)
        .set_at(Op::MusicSavedId, 1, 1, 9);
    s.set_at(Op::MusicFindEnemy, 7, 0, 1);
    game::music_to(&mut s);
    let warns: Vec<_> = s
        .log
        .iter()
        .filter(|x| x.0 == Op::MusicWarn)
        .map(|x| x.1)
        .collect();
    assert_eq!(warns, vec![7, 0]);
    assert!(s.log.contains(&(Op::MusicInsertEnemy, 7, 0)));
}

#[test]
fn music_entries_past_the_live_count_are_skipped() {
    let mut s = Script::new();
    s.set(Op::MusicSavedEntryN, 3).set(Op::MusicLiveEntryN, 1);
    game::music_to(&mut s);
    assert_eq!(
        s.ops().iter().filter(|o| **o == Op::MusicEntryTo).count(),
        1
    );
}

#[test]
fn inventory_equips_only_above_zero() {
    let mut s = Script::new();
    s.set(Op::InvEquippedIndex, 0);
    game::inventory_to(&mut s);
    assert!(s.ops().contains(&Op::InvEquipNone));
    let mut s = Script::new();
    s.set(Op::InvEquippedIndex, 1);
    game::inventory_to(&mut s);
    assert!(s.ops().contains(&Op::InvEquip));
}

#[test]
fn player_to_runs_state_hooks_around_the_change() {
    let mut s = Script::new();
    s.set(Op::PlayerHasStateData, 1);
    game::player_to(&mut s);
    let ops = s.ops();
    let pos = |o: Op| ops.iter().position(|x| *x == o);
    assert!(pos(Op::PlayerLoadStateBefore) < pos(Op::PlayerChangeState));
    assert!(pos(Op::PlayerChangeState) < pos(Op::PlayerLoadStateAfter));
    assert!(ops.contains(&Op::PlayerSpawnPsStop));
}

#[test]
fn map_from_filters_by_flags() {
    let mut s = Script::new();
    s.set(Op::MapLightN, 3);
    s.set_at(Op::MapLightFlags, 0, 0, 7)
        .set_at(Op::MapLightFlags, 1, 0, 3)
        .set_at(Op::MapLightFlags, 2, 0, 7);
    s.set(Op::MapSoundN, 2);
    s.set_at(Op::MapSoundFlags, 0, 0, 7)
        .set_at(Op::MapSoundFlags, 1, 0, 7);
    s.set_at(Op::MapSoundLoop, 1, 0, 1);
    s.set(Op::MapEntN, 2);
    s.set_at(Op::MapEntSaved, 0, 0, 1)
        .set_at(Op::MapEntSaved, 1, 0, 1)
        .set_at(Op::MapEntDestroyMe, 1, 0, 1);
    map::map_from(&mut s);
    let sel = |op: Op| -> Vec<i32> { s.log.iter().filter(|x| x.0 == op).map(|x| x.1).collect() };
    assert_eq!(sel(Op::MapLightFrom), vec![0, 2]);
    assert_eq!(sel(Op::MapSoundFrom), vec![1]);
    assert_eq!(sel(Op::MapEntFrom), vec![0]);
}

#[test]
fn map_to_warns_for_missing_lights_and_builds_the_rest() {
    let mut s = Script::new();
    s.set(Op::MapSavedLightN, 2)
        .set_at(Op::MapFindLight, 1, 0, 1);
    s.set(Op::MapSavedSoundN, 1);
    s.set(Op::MapSavedEntN, 2)
        .set_at(Op::MapCreateEntity, 1, 0, 1);
    s.set(Op::MapSkyIsNone, 1);
    map::map_to(&mut s);
    let ops = s.ops();
    assert!(ops.contains(&Op::MapSkyNull) && !ops.contains(&Op::MapSkyLoad));
    assert!(s.log.contains(&(Op::MapLightWarn, 0, 0)));
    assert!(s.log.contains(&(Op::MapLightTo, 1, 0)));
    assert!(s.log.contains(&(Op::MapSoundWarn, 0, 0)));
    assert!(s.log.contains(&(Op::MapEntLoad, 1, 0)));
    assert!(!s.log.contains(&(Op::MapEntLoad, 0, 0)));
}

#[test]
fn gamemap_to_flags_entities_from_the_lists() {
    use super::env::{ENT_ENEMY, ENT_PROP, PROP_CHEST, PROP_ITEM, PROP_LAMP};
    let mut s = Script::new();
    s.set(Op::GmEntN, 5);
    s.set_at(Op::GmEntType, 0, 0, ENT_PROP)
        .set_at(Op::GmPropType, 0, 0, PROP_CHEST);
    s.set_at(Op::GmEntType, 1, 0, ENT_ENEMY);
    s.set_at(Op::GmEntType, 2, 0, ENT_PROP)
        .set_at(Op::GmPropType, 2, 0, PROP_ITEM)
        .set_at(Op::GmEntSaved, 2, 0, 1);
    s.set_at(Op::GmEntType, 3, 0, ENT_PROP)
        .set_at(Op::GmPropType, 3, 0, PROP_LAMP);
    s.set_at(Op::GmEntType, 4, 0, ENT_PROP)
        .set_at(Op::GmPropType, 4, 0, PROP_ITEM)
        .set_at(Op::GmEntSaved, 4, 0, 1);
    s.set_at(Op::GmEntId, 0, 0, 10)
        .set_at(Op::GmEntId, 2, 0, 12)
        .set_at(Op::GmEntId, 3, 0, 13)
        .set_at(Op::GmEntId, 4, 0, 14);
    s.set_at(Op::GmEntFullSave, 4, 0, 0);
    s.set_at(Op::GmListN, gamemap::LIST_OPEN_CHESTS, 0, 1)
        .set_at(Op::GmListGet, gamemap::LIST_OPEN_CHESTS, 0, 10);
    s.set_at(Op::GmListN, gamemap::LIST_ACTIVE_ITEMS, 0, 1)
        .set_at(Op::GmListGet, gamemap::LIST_ACTIVE_ITEMS, 0, 14);
    s.set_at(Op::GmListN, gamemap::LIST_UNLIT_LAMPS, 0, 1)
        .set_at(Op::GmListGet, gamemap::LIST_UNLIT_LAMPS, 0, 13);
    s.set_at(Op::GmListN, gamemap::LIST_DISABLED, 0, 1).set_at(
        Op::GmListGet,
        gamemap::LIST_DISABLED,
        0,
        12,
    );
    gamemap::gamemap_to(&mut s);
    let has = |op: Op, a: i32, b: i32| s.log.contains(&(op, a, b));
    assert!(has(Op::GmSetChestLocked, 0, 0));
    assert!(has(Op::GmEntSetActive, 2, 0));
    assert!(has(Op::GmEntDestroy, 2, 0));
    assert!(!has(Op::GmEntDestroy, 4, 0));
    assert!(has(Op::GmSetLampLit, 3, 0));
    assert!(!s.log.iter().any(|x| x.0 == Op::GmEntSetActive && x.1 == 1));
}

#[test]
fn gamemap_from_collects_in_order() {
    use super::env::{ENT_PROP, PROP_SWING_DOOR};
    let mut s = Script::new();
    s.set(Op::GmEntN, 2);
    s.set_at(Op::GmEntFullSave, 0, 0, 1);
    s.set_at(Op::GmEntType, 1, 0, ENT_PROP)
        .set_at(Op::GmPropType, 1, 0, PROP_SWING_DOOR);
    s.set_at(Op::GmEntActive, 1, 0, 0)
        .set_at(Op::GmLocked, 1, 0, 1)
        .set_at(Op::GmPropHasAttached, 1, 0, 1);
    gamemap::gamemap_from(&mut s);
    let ops = s.ops();
    assert!(ops.contains(&Op::GmEntFullFrom));
    assert_eq!(ops.iter().filter(|o| **o == Op::GmEntSaveEntity).count(), 1);
    assert!(s.log.contains(&(Op::GmEntAddDisabled, 1, 0)));
    assert!(
        s.log
            .contains(&(Op::GmAddList, 1, gamemap::LIST_LOCKED_DOORS))
    );
}

#[test]
fn entity_flows() {
    let mut s = Script::new();
    s.set(Op::GeIsProp, 1)
        .set(Op::GeSavedInteractN, 2)
        .set_at(Op::GeCreateInteract, 1, 0, 1);
    gamemap::entity_to(&mut s);
    assert!(s.log.contains(&(Op::GeAddInteract, 1, 0)));
    assert!(!s.log.contains(&(Op::GeAddInteract, 0, 0)));
    let mut s = Script::new();
    gamemap::entity_from(&mut s);
    assert!(!s.ops().contains(&Op::GeFromProp));
}

#[test]
fn enemy_to_without_pathfinder_keeps_no_nodes() {
    let mut s = Script::new();
    s.set(Op::GenSavedNodeN, 2);
    gamemap::enemy_to(&mut s);
    assert_eq!(s.ops(), vec![Op::GenToHead, Op::GenHasPathFinder]);
    let mut s = Script::new();
    s.set(Op::GenHasPathFinder, 1)
        .set(Op::GenSavedNodeN, 2)
        .set_at(Op::GenFindNode, 1, 0, 1);
    gamemap::enemy_to(&mut s);
    assert!(s.log.contains(&(Op::GenAddNode, 1, 0)));
    assert!(!s.log.contains(&(Op::GenAddNode, 0, 0)));
}
