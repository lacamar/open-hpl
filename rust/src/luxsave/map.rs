//! `LuxSavedMap.cpp`: which live objects a map save keeps, and the order a load rebuilds them.

use super::env::{Env, Op};

pub const F_SAVED: i32 = 1;
pub const F_NO_ENTITY_PARENT: i32 = 2;
pub const F_NO_PARENT: i32 = 4;
const F_FREE: i32 = F_SAVED | F_NO_ENTITY_PARENT | F_NO_PARENT;

fn idx(n: i32) -> impl Iterator<Item = i32> {
    0..n.max(0)
}

pub fn map_from(e: &mut dyn Env) {
    e.c(Op::MapFromHead);
    let n = e.c(Op::MapLightN);
    for i in idx(n) {
        if e.c1(Op::MapLightFlags, i) & F_FREE == F_FREE {
            e.c1(Op::MapLightFrom, i);
        }
    }
    let n = e.c(Op::MapSoundN);
    for i in idx(n) {
        if e.c1(Op::MapSoundFlags, i) & F_FREE == F_FREE && e.t1(Op::MapSoundLoop, i) {
            e.c1(Op::MapSoundFrom, i);
        }
    }
    let n = e.c(Op::MapPsN);
    for i in idx(n) {
        if e.c1(Op::MapPsFlags, i) & F_FREE == F_FREE && !e.t1(Op::MapPsDying, i) {
            e.c1(Op::MapPsFrom, i);
        }
    }
    let n = e.c(Op::MapEntN);
    for i in idx(n) {
        if e.t1(Op::MapEntSaved, i) && !e.t1(Op::MapEntDestroyMe, i) {
            e.c1(Op::MapEntFrom, i);
        }
    }
    e.c(Op::MapFromTail);
}

pub fn map_to(e: &mut dyn Env) {
    e.c(Op::MapToHead);
    if e.t(Op::MapSkyIsNone) {
        e.c(Op::MapSkyNull);
    } else {
        e.c(Op::MapSkyLoad);
    }
    e.c(Op::MapToFog);
    for i in idx(e.c(Op::MapSavedLightN)) {
        if e.t1(Op::MapFindLight, i) {
            e.c1(Op::MapLightTo, i);
        } else {
            e.c1(Op::MapLightWarn, i);
        }
    }
    for i in idx(e.c(Op::MapSavedSoundN)) {
        if e.t1(Op::MapCreateSound, i) {
            e.c1(Op::MapSoundTo, i);
        } else {
            e.c1(Op::MapSoundWarn, i);
        }
    }
    for i in idx(e.c(Op::MapSavedPsN)) {
        if e.t1(Op::MapCreatePs, i) {
            e.c1(Op::MapPsTo, i);
        } else {
            e.c1(Op::MapPsWarn, i);
        }
    }
    e.c(Op::MapLoadBegin);
    for i in idx(e.c(Op::MapSavedEntN)) {
        if e.t1(Op::MapCreateEntity, i) {
            e.c1(Op::MapEntLoad, i);
        }
    }
    e.c(Op::MapLoadEnd);
    for i in idx(e.c(Op::MapSavedRopeN)) {
        e.c1(Op::MapRopeCreate, i);
        e.c1(Op::MapRopeTo, i);
    }
    e.c(Op::MapAfterLoad);
    e.c(Op::MapVarsTimers);
    e.c(Op::MapLoadBegin);
    for i in idx(e.c(Op::MapLoadedN)) {
        e.c1(Op::MapSetupEntity, i);
        if e.t1(Op::MapLoadedActive, i) {
            e.c1(Op::MapLoadedUpdate, i);
        }
    }
    e.c(Op::MapLoadEnd);
    e.c(Op::MapUseItemCallbacks);
}
