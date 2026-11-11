//! `LuxSavedEngineTypes.cpp`: gather/apply flow of the engine object records.
//!
//! Kept behaviour: `FromRope` stores `GetAutoMoveAcc()` as the max speed. Defined where the
//! original read out of bounds: rope particle apply and particle-system emitter kill stop at the
//! live count.

use super::env::{
    Env, JOINT_BALL, JOINT_HINGE, JOINT_SCREW, JOINT_SLIDER, KIND_SPEED, KIND_VOLUME, Op,
    VOL_SKIPPED,
};

fn idx(n: i32) -> impl Iterator<Item = i32> {
    0..n.max(0)
}

pub fn light_from(e: &mut dyn Env) {
    let has_parent = e.t(Op::LightHasParent);
    e.c(Op::LightFromBase);
    if !has_parent {
        if e.t(Op::LightIsFading) && !e.t(Op::LightFlickerActive) {
            e.c(Op::LightWriteDest);
        }
        if e.t(Op::LightFlickerActive) {
            e.c(Op::LightColorFlickerOn);
        } else {
            e.c(Op::LightColorCurrent);
        }
        e.c(Op::LightFlickerProps);
    }
}

pub fn light_to(e: &mut dyn Env) {
    let has_parent = e.t(Op::LightHasParent);
    if !has_parent {
        e.c(Op::LightSetActiveVisible);
    }
    if !has_parent {
        e.c(Op::LightSetColorFlicker);
    }
}

pub fn sound_from(e: &mut dyn Env) {
    e.c(Op::SoundFromHead);
    let stopped = e.t(Op::SoundIsStopped) || e.t(Op::SoundIsFadingOut);
    e.c1(Op::SoundSetStopped, i32::from(stopped));
    e.c(Op::SoundFromTail);
}

pub fn sound_to(e: &mut dyn Env) {
    e.c(Op::SoundToHead);
    if e.t(Op::SoundSavedStopped) {
        e.c(Op::SoundStop);
    }
    e.c(Op::SoundToTail);
}

pub fn ps_from(e: &mut dyn Env) {
    if !e.t(Op::PsPresent) {
        e.c(Op::PsFromNull);
        return;
    }
    e.c(Op::PsFromHead);
    let n = e.c(Op::PsLiveEmitterN);
    e.c1(Op::PsResizeEmitters, n);
    for i in idx(n) {
        let gone = e.t1(Op::PsEmitterDying, i) || e.t1(Op::PsEmitterDead, i);
        e.call(Op::PsSetEmitter, i, i32::from(!gone));
    }
}

pub fn ps_to(e: &mut dyn Env) {
    if !e.t(Op::PsPresent) {
        return;
    }
    e.c(Op::PsToHead);
    let saved = e.c(Op::PsSavedEmitterN);
    let live = e.c(Op::PsLiveEmitterN);
    for i in idx(saved) {
        if !e.t1(Op::PsSavedEmitterActive, i) && i < live {
            e.c1(Op::PsKillEmitter, i);
        }
    }
}

pub fn joint_from(e: &mut dyn Env) {
    if !e.t(Op::JointExists) {
        e.c1(Op::JointSetBroken, 1);
        return;
    }
    e.c(Op::JointFromBase);
    let n = e.c(Op::JointLiveCtrlN);
    for i in idx(n) {
        e.c1(Op::JointCtrlFrom, i);
    }
    let ty = e.c(Op::JointType);
    if matches!(ty, JOINT_BALL | JOINT_HINGE | JOINT_SLIDER | JOINT_SCREW) {
        e.c1(Op::JointFromLimits, ty);
    }
}

pub fn joint_to(e: &mut dyn Env) {
    let n = e.c(Op::JointSavedCtrlN);
    for i in idx(n) {
        if e.t1(Op::JointFindCtrl, i) {
            e.c1(Op::JointCtrlTo, i);
        }
    }
    let ty = e.c(Op::JointType);
    if matches!(ty, JOINT_BALL | JOINT_HINGE | JOINT_SLIDER | JOINT_SCREW) {
        e.c1(Op::JointToLimits, ty);
    }
}

pub fn mesh_from(e: &mut dyn Env) {
    e.c(Op::MeshFromBase);
    let n = e.c(Op::MeshLiveAnimN);
    e.c1(Op::MeshResizeAnims, n);
    for i in idx(n) {
        e.c1(Op::MeshAnimFrom, i);
    }
    let n = e.c(Op::MeshLiveSubN);
    e.c1(Op::MeshResizeSubs, n);
    for i in idx(n) {
        e.c1(Op::MeshSubFrom, i);
    }
}

pub fn mesh_to(e: &mut dyn Env) {
    e.c(Op::MeshToBase);
    let saved = e.c(Op::MeshSavedAnimN);
    if saved == e.c(Op::MeshLiveAnimN) {
        for i in idx(saved) {
            e.c1(Op::MeshAnimTo, i);
        }
    }
    let saved = e.c(Op::MeshSavedSubN);
    if saved == e.c(Op::MeshLiveSubN) {
        for i in idx(saved) {
            e.c1(Op::MeshSubTo, i);
        }
    }
}

pub fn rope_from(e: &mut dyn Env) {
    e.c(Op::RopeFromBase);
    let n = e.c(Op::RopeLivePartN);
    for i in idx(n) {
        e.c1(Op::RopePartFrom, i);
    }
}

pub fn rope_to(e: &mut dyn Env) {
    e.c(Op::RopeToBase);
    let live = e.c(Op::RopeLivePartN);
    let saved = e.c(Op::RopeSavedPartN);
    for i in idx(live.min(saved)) {
        e.c1(Op::RopePartTo, i);
    }
    if e.t(Op::RopeFindStartBody) {
        e.c(Op::RopeAttachStart);
    }
    if e.t(Op::RopeFindEndBody) {
        e.c(Op::RopeAttachEnd);
    }
}

pub fn effects_from(e: &mut dyn Env) {
    e.c(Op::EffectFromBase);
    let n = e.c(Op::EffectVolumeCount);
    e.c1(Op::EffectResizeVolumes, n);
    for i in idx(n) {
        if e.call(Op::EffectVolumeKind, i, 0) == VOL_SKIPPED {
            continue;
        }
        for kind in [KIND_VOLUME, KIND_SPEED] {
            if e.t2(Op::EffectEntryPresent, kind, i) {
                e.call(Op::EffectEntryFrom, kind, i);
            } else {
                e.call(Op::EffectEntryInactive, kind, i);
            }
        }
    }
}

pub fn effects_to(e: &mut dyn Env) {
    e.c(Op::EffectToBase);
    for kind in [KIND_VOLUME, KIND_SPEED] {
        let n = e.c1(Op::EffectSavedCount, kind);
        for i in idx(n) {
            if e.call(Op::EffectVolumeKind, i, 0) == VOL_SKIPPED {
                continue;
            }
            if e.t2(Op::EffectEntryActive, kind, i) {
                e.call(Op::EffectEntryApply, kind, i);
            } else {
                e.call(Op::EffectEntryDestroy, kind, i);
            }
        }
    }
}
