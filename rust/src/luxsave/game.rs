//! `LuxSavedGameTypes.cpp`: handler/player records of the save.
//!
//! Kept behaviour: `FromMusicHandler` stores search-music enemies in the attacker list and never
//! fills the searcher list; the searcher warning prints `mvAttackerIDs[i]`; `ToPlayer` copies
//! `mbSanityDrainDisabled` from the player into the record. Defined where the original read out of
//! bounds: music entries beyond the live count are skipped and the searcher warning id is 0 when
//! there is no attacker `i`.

use super::env::{Env, Op, SET_ATTACK, SET_SEARCH};

fn idx(n: i32) -> impl Iterator<Item = i32> {
    0..n.max(0)
}

pub fn music_from(e: &mut dyn Env) {
    if e.t(Op::MusicHasSong) {
        e.c(Op::MusicFromSong);
    } else {
        e.c(Op::MusicFromNoSong);
    }
    e.c(Op::MusicFromMisc);
    e.c(Op::MusicFromEntries);
    for set in [SET_ATTACK, SET_SEARCH] {
        for i in idx(e.c1(Op::MusicEnemyN, set)) {
            let id = e.call(Op::MusicEnemyId, set, i);
            e.c1(Op::MusicAddAttackerId, id);
        }
    }
}

pub fn music_to(e: &mut dyn Env) {
    if e.t(Op::MusicHasSavedSong) && e.t(Op::MusicPlay) && e.t(Op::MusicHasCurrentSong) {
        e.c(Op::MusicApplySong);
    }
    e.c(Op::MusicToMisc);
    let live = e.c(Op::MusicLiveEntryN);
    for i in idx(e.c(Op::MusicSavedEntryN)) {
        if i < live {
            e.c1(Op::MusicEntryTo, i);
        }
    }
    let attackers: Vec<i32> = idx(e.c1(Op::MusicSavedIdN, SET_ATTACK))
        .map(|i| e.call(Op::MusicSavedId, SET_ATTACK, i))
        .collect();
    for &id in &attackers {
        if e.t1(Op::MusicFindEnemy, id) {
            e.call(Op::MusicInsertEnemy, id, SET_ATTACK);
        } else {
            e.c1(Op::MusicWarn, id);
        }
    }
    for i in idx(e.c1(Op::MusicSavedIdN, SET_SEARCH)) {
        let id = e.call(Op::MusicSavedId, SET_SEARCH, i);
        if e.t1(Op::MusicFindEnemy, id) {
            e.call(Op::MusicInsertEnemy, id, SET_SEARCH);
        } else {
            let shown = usize::try_from(i)
                .ok()
                .and_then(|k| attackers.get(k))
                .copied()
                .unwrap_or(0);
            e.c1(Op::MusicWarn, shown);
        }
    }
}

pub fn insanity_to(e: &mut dyn Env) {
    e.c(Op::InsToCount);
    let saved = e.c(Op::InsSavedEventN);
    if saved == e.c(Op::InsLiveEventN) {
        for i in idx(saved) {
            e.c1(Op::InsEventTo, i);
        }
    }
    e.c(Op::InsClearSets);
    for i in idx(e.c(Op::InsSavedSetN)) {
        e.c1(Op::InsInsertSet, i);
    }
}

pub fn inventory_to(e: &mut dyn Env) {
    e.c(Op::InvToDisabled);
    if e.c(Op::InvEquippedIndex) > 0 {
        e.c(Op::InvEquip);
    } else {
        e.c(Op::InvEquipNone);
    }
    e.c(Op::InvToItems);
    e.c(Op::InvToCallbacks);
}

pub fn hands_from(e: &mut dyn Env) {
    if e.t(Op::HandsHasObject) {
        e.c(Op::HandsFromObject);
    } else {
        e.c(Op::HandsFromNoObject);
    }
    e.c(Op::HandsFromRest);
}

pub fn hands_to(e: &mut dyn Env) {
    e.c(Op::HandsToState);
    if e.t(Op::HandsHasSavedObject) {
        e.c(Op::HandsFindObject);
        if e.t(Op::HandsHasCurrentObject) {
            e.c(Op::HandsAttachObject);
        }
    }
    e.c(Op::HandsToRest);
    if e.t(Op::HandsIdleCondition) {
        e.c(Op::HandsIdleAfterLoad);
    }
}

pub fn player_from(e: &mut dyn Env) {
    if e.t(Op::PlayerStateSaved) {
        e.c(Op::PlayerFromState);
    } else {
        e.c(Op::PlayerFromNoState);
    }
    e.c(Op::PlayerFromBlock);
}

pub fn player_to(e: &mut dyn Env) {
    e.c(Op::PlayerToStateBefore);
    if e.t(Op::PlayerHasStateData) {
        e.c(Op::PlayerLoadStateBefore);
    }
    e.c(Op::PlayerChangeState);
    if e.t(Op::PlayerHasStateData) {
        e.c(Op::PlayerLoadStateAfter);
    }
    e.c(Op::PlayerToBlock1);
    if e.t(Op::PlayerSpawnPsActive) {
        e.c(Op::PlayerSpawnPsStart);
    } else {
        e.c(Op::PlayerSpawnPsStop);
    }
    e.c(Op::PlayerToBlock2);
    if e.t(Op::PlayerHeadPosAddsMatch) {
        e.c(Op::PlayerToHeadPosAdds);
    }
    for i in idx(e.c(Op::PlayerSavedTerrorN)) {
        let id = e.c1(Op::PlayerSavedTerrorId, i);
        if e.t1(Op::PlayerFindEnemy, id) {
            e.c1(Op::PlayerInsertTerror, id);
        } else {
            e.c1(Op::PlayerWarnTerror, id);
        }
    }
    e.c(Op::PlayerToBlock3);
}
