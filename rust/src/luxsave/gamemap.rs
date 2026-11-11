//! `LuxSavedGame.cpp`: per-map game state kept in the save (entity flags, callbacks, lights,
//! sounds, particle systems) and how a load applies it back.

use super::env::{
    ENT_ENEMY, ENT_PROP, Env, Op, PROP_CHEST, PROP_ITEM, PROP_LAMP, PROP_LEVEL_DOOR, PROP_OBJECT,
    PROP_SWING_DOOR,
};
use super::map::{F_NO_ENTITY_PARENT, F_NO_PARENT, F_SAVED};
use std::collections::BTreeSet;

const F_FREE: i32 = F_SAVED | F_NO_ENTITY_PARENT | F_NO_PARENT;

pub const LIST_DISABLED: i32 = 0;
pub const LIST_LOCKED_DOORS: i32 = 1;
pub const LIST_LOCKED_LEVEL_DOORS: i32 = 2;
pub const LIST_ACTIVE_ITEMS: i32 = 3;
pub const LIST_OPEN_CHESTS: i32 = 4;
pub const LIST_UNBROKEN_CONTAINERS: i32 = 5;
pub const LIST_BROKEN_WITH_ACTIVE_ITEM: i32 = 6;
pub const LIST_UNLIT_LAMPS: i32 = 7;

pub const FUNC_LOOK: i32 = 0;
pub const FUNC_INTERACT: i32 = 1;
pub const FUNC_CALLBACK: i32 = 2;
pub const FUNC_STATE_CHANGE: i32 = 3;

fn idx(n: i32) -> impl Iterator<Item = i32> {
    0..n.max(0)
}

pub fn gamemap_from(e: &mut dyn Env) {
    e.c(Op::GmFromHead);
    let n = e.c(Op::MapLightN);
    for i in idx(n) {
        if e.c1(Op::MapLightFlags, i) & F_FREE == F_FREE {
            e.c1(Op::MapLightFrom, i);
        }
    }
    let n = e.c(Op::MapSoundN);
    for i in idx(n) {
        if e.c1(Op::MapSoundFlags, i) & F_FREE == F_FREE {
            e.c1(Op::MapSoundFrom, i);
        }
    }
    let n = e.c(Op::MapPsN);
    for i in idx(n) {
        if e.c1(Op::MapPsFlags, i) & F_FREE == F_FREE && !e.t1(Op::MapPsDying, i) {
            e.c1(Op::MapPsFrom, i);
        }
    }
    e.c(Op::GmSnapshot);
    for i in idx(e.c(Op::GmEntN)) {
        entity_from_flags(e, i);
    }
}

fn entity_from_flags(e: &mut dyn Env, i: i32) {
    if e.t1(Op::GmEntFullSave, i) {
        e.c1(Op::GmEntFullFrom, i);
        return;
    }
    let mut added = false;
    if e.t1(Op::GmEntHasCollide, i) || e.c1(Op::GmEntConnNum, i) > 0 {
        e.c1(Op::GmEntSaveEntity, i);
        added = true;
    }
    for f in [FUNC_LOOK, FUNC_INTERACT, FUNC_CALLBACK, FUNC_STATE_CHANGE] {
        if e.t2(Op::GmEntHasFunc, i, f) {
            e.call(Op::GmEntAddFunc, i, f);
        }
    }
    if !e.t1(Op::GmEntActive, i) {
        e.c1(Op::GmEntAddDisabled, i);
    }
    let ty = e.c1(Op::GmEntType, i);
    if ty == ENT_ENEMY {
        e.c1(Op::GmEntSaveEnemy, i);
    } else if ty == ENT_PROP {
        if !added && (e.t1(Op::GmPropHasAttached, i) || e.c1(Op::GmPropInteractConnNum, i) > 0) {
            e.c1(Op::GmEntSaveEntity, i);
        }
        match e.c1(Op::GmPropType, i) {
            PROP_SWING_DOOR => {
                if e.t1(Op::GmLocked, i) {
                    e.call(Op::GmAddList, i, LIST_LOCKED_DOORS);
                }
            }
            PROP_LEVEL_DOOR => {
                if e.t1(Op::GmLocked, i) {
                    e.call(Op::GmAddList, i, LIST_LOCKED_LEVEL_DOORS);
                }
            }
            PROP_ITEM => {
                if e.c1(Op::GmItemContainerId, i) >= 0 {
                    e.call(Op::GmAddList, i, LIST_BROKEN_WITH_ACTIVE_ITEM);
                } else {
                    e.call(Op::GmAddList, i, LIST_ACTIVE_ITEMS);
                }
            }
            PROP_OBJECT => {
                if e.t1(Op::GmObjectHasContained, i) {
                    e.call(Op::GmAddList, i, LIST_UNBROKEN_CONTAINERS);
                }
            }
            PROP_CHEST => {
                if !e.t1(Op::GmLocked, i) {
                    e.call(Op::GmAddList, i, LIST_OPEN_CHESTS);
                }
            }
            PROP_LAMP if !e.t1(Op::GmLampLit, i) => {
                e.call(Op::GmAddList, i, LIST_UNLIT_LAMPS);
            }
            _ => {}
        }
    }
}

fn list_set(e: &mut dyn Env, which: i32) -> BTreeSet<i32> {
    let n = e.c1(Op::GmListN, which);
    idx(n).map(|i| e.call(Op::GmListGet, which, i)).collect()
}

pub fn gamemap_to(e: &mut dyn Env) {
    e.c(Op::GmToHead);
    if e.t(Op::MapSkyIsNone) {
        e.c(Op::MapSkyNull);
    } else {
        e.c(Op::MapSkyLoad);
    }
    e.c(Op::MapToFog);
    e.c(Op::GmToVarsCallbacksTimers);
    for i in idx(e.c(Op::GmSavedEntityN)) {
        if e.t1(Op::GmSavedEntityFound, i) {
            e.c1(Op::GmToEntity, i);
        } else {
            e.c1(Op::GmSavedEntityError, i);
        }
    }
    for i in idx(e.c(Op::GmSavedEnemyN)) {
        if e.t1(Op::GmSavedEnemyFound, i) {
            e.c1(Op::GmToEnemy, i);
        } else {
            e.c1(Op::GmSavedEnemyError, i);
        }
    }
    e.c(Op::GmPlayerCallbacksTo);
    e.c(Op::GmSnapshot);
    for i in idx(e.c(Op::GmEntN)) {
        if !e.t1(Op::GmEntFullSave, i) {
            e.c1(Op::GmEntClearFuncs, i);
        }
    }
    for f in [FUNC_LOOK, FUNC_INTERACT, FUNC_CALLBACK, FUNC_STATE_CHANGE] {
        for j in idx(e.c1(Op::GmFuncN, f)) {
            if e.t2(Op::GmFuncFound, f, j) {
                e.call(Op::GmFuncApply, f, j);
            }
        }
    }
    for i in idx(e.c(Op::GmSavedLightN)) {
        if e.t1(Op::MapFindLight, i) {
            e.c1(Op::MapLightTo, i);
        } else {
            e.c1(Op::MapLightWarn, i);
        }
    }
    for i in idx(e.c(Op::GmSavedSoundN)) {
        let found = if e.c1(Op::GmSoundId, i) >= 0 {
            e.t1(Op::GmFindSound, i)
        } else {
            e.t1(Op::MapCreateSound, i)
        };
        if found {
            e.c1(Op::MapSoundTo, i);
        } else {
            e.c1(Op::GmSoundWarn, i);
        }
    }
    let mut saved_ps = BTreeSet::new();
    for i in idx(e.c(Op::GmSavedPsN)) {
        let id = e.c1(Op::GmPsId, i);
        if id >= 0 {
            if e.t1(Op::GmFindPs, i) {
                e.c1(Op::MapPsTo, i);
            }
            saved_ps.insert(id);
        } else if e.t1(Op::MapCreatePs, i) {
            e.c1(Op::MapPsTo, i);
        } else {
            e.c1(Op::MapPsWarn, i);
        }
    }
    for j in idx(e.c(Op::GmLivePsN)) {
        if e.c1(Op::GmLivePsFlags, j) & F_FREE == F_FREE {
            let id = e.c1(Op::GmLivePsId, j);
            if id >= 0 && !saved_ps.contains(&id) {
                e.c1(Op::GmLivePsKill, j);
            }
        }
    }
    full_entities(e);
    apply_sets(e);
    e.c(Op::MapLoadBegin);
    for i in idx(e.c(Op::MapLoadedN)) {
        e.c1(Op::MapSetupEntity, i);
        if e.t1(Op::MapLoadedActive, i) {
            e.c1(Op::MapLoadedUpdate, i);
        }
    }
    e.c(Op::MapLoadEnd);
}

fn full_entities(e: &mut dyn Env) {
    e.c(Op::MapLoadBegin);
    let n = e.c(Op::GmFullN);
    for i in idx(n) {
        let found = e.t1(Op::GmFullFind, i);
        if found || e.t1(Op::GmFullCreate, i) {
            e.c1(Op::GmFullLoad, i);
        }
    }
    e.c(Op::GmSnapshot);
    let ids: Vec<i32> = idx(n).map(|k| e.c1(Op::GmFullId, k)).collect();
    for j in idx(e.c(Op::GmEntN)) {
        if e.t1(Op::GmEntFullSave, j) {
            let id = e.c1(Op::GmEntId, j);
            if !ids.contains(&id) {
                e.c1(Op::GmEntDestroy, j);
            }
        }
    }
    e.c(Op::GmUpdateDestroyed);
    e.c(Op::MapLoadEnd);
}

fn apply_sets(e: &mut dyn Env) {
    let disabled = list_set(e, LIST_DISABLED);
    let doors = list_set(e, LIST_LOCKED_DOORS);
    let level_doors = list_set(e, LIST_LOCKED_LEVEL_DOORS);
    let items = list_set(e, LIST_ACTIVE_ITEMS);
    let chests = list_set(e, LIST_OPEN_CHESTS);
    let unbroken = list_set(e, LIST_UNBROKEN_CONTAINERS);
    let broken = list_set(e, LIST_BROKEN_WITH_ACTIVE_ITEM);
    let unlit = list_set(e, LIST_UNLIT_LAMPS);
    e.c(Op::GmSnapshot);
    for i in idx(e.c(Op::GmEntN)) {
        let id = e.c1(Op::GmEntId, i);
        let ty = e.c1(Op::GmEntType, i);
        if e.t1(Op::GmEntFullSave, i) || ty == ENT_ENEMY {
            continue;
        }
        e.call(Op::GmEntSetActive, i, i32::from(!disabled.contains(&id)));
        if ty != ENT_PROP {
            continue;
        }
        match e.c1(Op::GmPropType, i) {
            PROP_SWING_DOOR => {
                e.call(Op::GmSetLocked, i, i32::from(doors.contains(&id)));
            }
            PROP_LEVEL_DOOR => {
                e.call(
                    Op::GmSetLevelDoorLocked,
                    i,
                    i32::from(level_doors.contains(&id)),
                );
            }
            PROP_ITEM => {
                if e.t1(Op::GmEntSaved, i) && !items.contains(&id) {
                    e.c1(Op::GmEntDestroy, i);
                }
            }
            PROP_OBJECT => {
                if e.t1(Op::GmObjectHasContained, i)
                    && !unbroken.contains(&id)
                    && !broken.contains(&id)
                {
                    e.c1(Op::GmObjectClear, i);
                }
            }
            PROP_CHEST => {
                e.call(Op::GmSetChestLocked, i, i32::from(!chests.contains(&id)));
            }
            PROP_LAMP => {
                e.call(Op::GmSetLampLit, i, i32::from(!unlit.contains(&id)));
            }
            _ => {}
        }
    }
}

/// `cLuxSavedGameEntity::FromEntity`.
pub fn entity_from(e: &mut dyn Env) {
    e.c(Op::GeFromId);
    if e.t(Op::GeIsProp) {
        e.c(Op::GeFromProp);
    }
    e.c(Op::GeFromConns);
    e.c(Op::GeFromCollide);
}

/// `cLuxSavedGameEntity::ToEntity`.
pub fn entity_to(e: &mut dyn Env) {
    if e.t(Op::GeIsProp) {
        e.c(Op::GeToAttached);
        for i in idx(e.c(Op::GeSavedInteractN)) {
            if e.t1(Op::GeCreateInteract, i) {
                e.c1(Op::GeAddInteract, i);
            }
        }
    }
    e.c(Op::GeToConns);
    e.c(Op::GeToCollide);
}

/// `cLuxSavedGameEnemy::ToEnemy`.
pub fn enemy_to(e: &mut dyn Env) {
    e.c(Op::GenToHead);
    if e.t(Op::GenHasPathFinder) {
        for i in idx(e.c(Op::GenSavedNodeN)) {
            if e.t1(Op::GenFindNode, i) {
                e.c1(Op::GenAddNode, i);
            }
        }
    }
}

/// `cLuxSavedGameMapCollection::GetSavedMap`: first map whose name equals `name`.
pub fn find_map(names: &[&[u8]], name: &[u8]) -> Option<usize> {
    names.iter().position(|n| *n == name)
}
