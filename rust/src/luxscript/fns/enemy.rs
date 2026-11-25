//! Enemy functions.

use super::super::{a1::Arg, tables};
use super::entity::{body_in, lossy, set_each, vec_of};
use super::{
    Ent, P, RStr, S, TYPE_ANY, TYPE_ENEMY, call, each, entities, entity, error, ops, s, sf,
};

const STATE_WAIT: i32 = 2;
const STATE_SEARCH: i32 = 6;
const STATE_HUNT: i32 = 9;
const STATE_MELEE_SHORT: i32 = 12;
const STATE_MELEE_LONG: i32 = 13;
const STATE_BREAK_DOOR: i32 = 15;

const MSG_TIME_OUT: i32 = 0;
const MSG_STOP_PATROL_ANIM: i32 = 1;

fn find_enemy(name: &[u8]) -> Option<Ent> {
    let e = entity(name, TYPE_ENEMY, -1);
    if e.is_none() {
        error(&format!("Can't find enemy '{}'!\n", lossy(name)));
    }
    e
}

sf!(ForceEnemyWaitState(name: S) {
    each(name.0, TYPE_ENEMY, -1, |e| { call(ops::ENEMY_CHANGE_STATE, &[e.a(), Arg::i(STATE_WAIT)]); });
});

sf!(GetEnemyPlayerDistance(name: S) -> f32 {
    find_enemy(name.0).map_or(0.0, |e| call(ops::ENEMY_PLAYER_DISTANCE, &[e.a()]).as_f())
});

sf!(GetEnemyState(name: S) -> i32 {
    find_enemy(name.0).map_or(0, |e| call(ops::ENEMY_STATE, &[e.a()]).as_i())
});

sf!(GetPlayerCanSeeEnemy(name: S) -> bool {
    find_enemy(name.0).is_some_and(|e| call(ops::ENEMY_SEEN, &[e.a()]).as_b())
});

sf!(GetEnemyStateName(name: S) -> RStr {
    match find_enemy(name.0) {
        Some(e) => RStr(call(ops::ENEMY_STATE_NAME, &[e.a()]).p),
        None => super::null_str(),
    }
});

sf!(PlayEnemyAnimation(name: S, anim: S, looped: bool, fade: f32) {
    each(name.0, TYPE_ENEMY, -1, |e| {
        call(ops::ENEMY_CLEAR_PATROL, &[e.a()]);
        call(ops::ENEMY_PLAY_ANIM, &[e.a(), s(anim.0), Arg::b(looped), Arg::f(fade)]);
    });
});

set_each!(
    PlayScriptedAnimation(anim: S, looped: bool),
    TYPE_ENEMY,
    -1,
    ops::ENEMY_SCRIPTED_ANIM
);

sf!(SendEnemyTimeOut(name: S, time: f32) {
    each(name.0, TYPE_ENEMY, -1, |e| {
        call(ops::ENEMY_MESSAGE, &[e.a(), Arg::i(MSG_TIME_OUT), Arg::f(time)]);
    });
});

sf!(StopAnimationAndContinue(name: S, time: f32) {
    each(name.0, TYPE_ENEMY, -1, |e| {
        call(ops::ENEMY_MESSAGE, &[e.a(), Arg::i(MSG_STOP_PATROL_ANIM), Arg::f(time)]);
    });
});

set_each!(
    SetEnemyActivationDistance(dist: f32),
    TYPE_ENEMY,
    -1,
    ops::ENEMY_ACTIVATION_DISTANCE
);
set_each!(SetEnemyBlind(on: bool), TYPE_ENEMY, -1, ops::ENEMY_BLIND);
set_each!(SetEnemyDeaf(on: bool), TYPE_ENEMY, -1, ops::ENEMY_DEAF);
set_each!(
    SetEnemyDisabled(on: bool),
    TYPE_ENEMY,
    -1,
    ops::ENEMY_DISABLED
);
set_each!(
    SetEnemyIsHallucination(on: bool),
    TYPE_ENEMY,
    -1,
    ops::ENEMY_HALLUCINATION
);
set_each!(
    FadeEnemyToSmoke(sound: bool),
    TYPE_ENEMY,
    -1,
    ops::ENEMY_FADE_TO_SMOKE
);
set_each!(
    SetEnemyDisableTriggers(on: bool),
    TYPE_ENEMY,
    -1,
    ops::ENEMY_DISABLE_TRIGGERS
);
set_each!(
    ClearEnemyPatrolNodes(),
    TYPE_ENEMY,
    -1,
    ops::ENEMY_CLEAR_PATROL
);
set_each!(
    SetEnemySanityDecreaseActive(on: bool),
    TYPE_ENEMY,
    -1,
    ops::ENEMY_SANITY_DECREASE
);

// the original's state test is a chain of `!=` joined by `||`, so it always passes
sf!(ShowEnemyPlayerPosition(name: S) {
    each(name.0, TYPE_ENEMY, -1, |e| {
        call(ops::ENEMY_SHOW_PLAYER_POS, &[e.a()]);
        let state = call(ops::ENEMY_STATE, &[e.a()]).as_i();
        if state != STATE_HUNT || state != STATE_MELEE_LONG || state != STATE_MELEE_SHORT || state != STATE_BREAK_DOOR {
            call(ops::ENEMY_CHANGE_STATE, &[e.a(), Arg::i(STATE_HUNT)]);
        }
    });
});

sf!(AlertEnemyOfPlayerPresence(name: S) {
    each(name.0, TYPE_ENEMY, -1, |e| {
        call(ops::ENEMY_SHOW_PLAYER_POS, &[e.a()]);
        let state = call(ops::ENEMY_STATE, &[e.a()]).as_i();
        if state != STATE_HUNT && state != STATE_MELEE_LONG && state != STATE_MELEE_SHORT && state != STATE_BREAK_DOOR {
            call(ops::ENEMY_CHANGE_STATE, &[e.a(), Arg::i(STATE_SEARCH)]);
        }
    });
});

fn add_patrol_node(name: &[u8], node: &[u8], wait: f32, anim: &[u8]) {
    each(name, TYPE_ENEMY, -1, |e| {
        let en = super::entity::ent_name(e);
        if call(ops::ENEMY_HAS_NODES, &[e.a()]).p.is_null() {
            error(&format!(
                "There is no node container in enemy '{}'! (probably no nodes in map!)\n",
                lossy(en)
            ));
            return;
        }
        let n = call(ops::ENEMY_NODE, &[e.a(), s(node)]).p;
        if n.is_null() {
            error(&format!(
                "Could not find node '{}' for enemy '{}'\n",
                lossy(node),
                lossy(en)
            ));
            return;
        }
        call(
            ops::ENEMY_ADD_PATROL,
            &[e.a(), Arg::ptr(n), Arg::f(wait), s(anim)],
        );
    });
}

sf!(AddEnemyPatrolNode(name: S, node: S, wait: f32, anim: S) {
    add_patrol_node(name.0, node.0, wait, anim.0);
});

sf!(AddEnemyPatrolNodeAmfp(name: S, node: S, wait: f32, anim: S, extra: bool) {
    let _ = extra;
    add_patrol_node(name.0, node.0, wait, anim.0);
});

fn feet(e: Ent) -> [f32; 3] {
    let v = vec_of(ops::ENEMY_FEET, e.0);
    [v.x, v.y, v.z]
}

fn set_feet(e: Ent, p: [f32; 3]) {
    call(
        ops::ENEMY_SET_FEET,
        &[e.a(), Arg::f(p[0]), Arg::f(p[1]), Arg::f(p[2])],
    );
}

sf!(TeleportEnemyToNode(name: S, node: S, change_y: bool) {
    each(name.0, TYPE_ENEMY, -1, |e| {
        let n = call(ops::ENEMY_NODE, &[e.a(), s(node.0)]).p;
        if n.is_null() {
            error(&format!(
                "Could not find node '{}' for enemy '{}'\n",
                lossy(node.0),
                lossy(super::entity::ent_name(e))
            ));
            return;
        }
        let v = vec_of(ops::NODE_POSITION, n);
        let mut p = [v.x, v.y, v.z];
        if !change_y {
            p[1] = feet(e)[1];
        }
        set_feet(e, p);
    });
});

sf!(TeleportEnemyToEntity(name: S, target: S, target_body: S, change_y: bool) {
    let Some(list) = entities(name.0, TYPE_ENEMY, -1) else { return };
    for e in list {
        let t = entity(target.0, TYPE_ANY, -1);
        let Some(t) = t else {
            error(&format!(
                "Could not find entity '{}' for enemy '{}' to be teleported to.\n",
                lossy(target.0),
                lossy(super::entity::ent_name(e))
            ));
            continue;
        };
        let mut p = if call(ops::ENT_TYPE, &[t.a()]).as_i() == TYPE_ENEMY {
            feet(t)
        } else {
            let Some(b) = body_in(Some(t), target_body.0) else { return };
            let v = vec_of(ops::BODY_WORLD_POS, b);
            [v.x, v.y, v.z]
        };
        if !change_y {
            p[1] = feet(e)[1];
        }
        set_feet(e, p);
    }
});

sf!(ChangeManPigPose(name: S, pose: S) {
    let p = tables::pose(pose.0);
    if p < 0 {
        error(&format!(
            "Could not set pose '{}' for enemy '{}'. Pose does not exist!\n",
            lossy(pose.0),
            lossy(name.0)
        ));
        return;
    }
    each(name.0, TYPE_ENEMY, -1, |e| {
        if call(ops::ENT_AS_MANPIG, &[e.a()]).as_b() {
            call(ops::MANPIG_POSE, &[e.a(), Arg::i(p)]);
        }
    });
});

fn manpig(name: &[u8], op: ops::Op, arg: Option<bool>) {
    each(name, TYPE_ENEMY, -1, |e| {
        if call(ops::ENT_AS_MANPIG, &[e.a()]).as_b() {
            match arg {
                Some(b) => call(op, &[e.a(), Arg::b(b)]),
                None => call(op, &[e.a()]),
            };
        }
    });
}

sf!(SetTeslaPigFadeDisabled(name: S, off: bool) {
    manpig(name.0, ops::TESLA_FADE_DISABLED, Some(off));
});

sf!(SetTeslaPigSoundDisabled(name: S, off: bool) {
    manpig(name.0, ops::TESLA_SOUND_DISABLED, Some(off));
});

sf!(SetTeslaPigEasyEscapeDisabled(name: S, off: bool) {
    manpig(name.0, ops::TESLA_EASY_ESCAPE_DISABLED, Some(off));
});

sf!(ForceTeslaPigSighting(name: S) {
    manpig(name.0, ops::TESLA_FORCE_SIGHTING, None);
});

#[allow(unused_imports)]
use P as _;
