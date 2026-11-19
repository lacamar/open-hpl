//! State of `cSomaLuxEntity` that is plain data: flags, timers, fades, vectors, matrices, colours,
//! script ids and the move/rotate drivers. The C++ shell owns one box and reads every field through
//! reference members, so the layout is shared (`RustSomaCore.h`, size asserted on both sides).

#![allow(non_snake_case)]

use super::motion::{Move, Rotate};
use crate::math::IDENTITY;
use crate::types::{Color, Matrixf, Vector3f};

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Id {
    pub a: u8,
    pub b: i32,
    pub c: i32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct Core {
    pub mbSpawned: bool,
    pub meType: i32,
    pub mID: Id,
    pub mlParentType: i32,
    pub mParentID: Id,
    pub mbActive: bool,
    pub mbSleeping: bool,
    pub mbAutoSleep: bool,
    pub mbShowMesh: bool,
    pub mbIsDoor: bool,
    pub mbIsClosedDoor: bool,
    pub mbInteractionDisabled: bool,
    pub mfMaxInteractDistance: f32,
    pub mfVariableUpdateRate: f32,
    pub mfVariableUpdateAcc: f32,
    pub mbInteractedWith: bool,
    pub m_mtxOnLoad: Matrixf,
    pub mvScale: Vector3f,
    pub mvMeshScaleMul: Vector3f,
    pub mbGlobalSpaceAnim: bool,
    pub mvSize: Vector3f,
    pub mbCheckOutsidePlayer: bool,
    pub mbCharCollisionUntilStopped: bool,
    pub mbCharCollisionUntilStoppedStarted: bool,
    pub mvBodyOffset: Vector3f,
    pub mbInteractCallbackAutoRemove: bool,
    pub mbAnimMessageCallbackAutoRemove: bool,
    pub mbLookAtCallbackAutoRemove: bool,
    pub mbLookAtCheckCenter: bool,
    pub mbLookAtCheckRay: bool,
    pub mfLookAtMaxDistance: f32,
    pub mfLookAtDelay: f32,
    pub mbForceLookAtCheck: bool,
    pub mfLookAtCheckTimer: f32,
    pub mfLookAtCountdown: f32,
    pub mbLookedAt: bool,
    pub mbEffectsActive: bool,
    pub mbStaticPhysics: bool,
    pub mbAllowMapTransfer: bool,
    pub mfEffectsAlpha: f32,
    pub mfEffectsFadeSpeed: f32,
    pub mfEffectsOffAlpha: f32,
    pub mIllumBaseColor: Color,
    pub mbIllumBaseSet: bool,
    pub mInteractLeaderID: Id,
    pub mfInteractConnectionState: f32,
    pub mEffectBaseColor: Color,
    pub mEffectColorFrom: Color,
    pub mEffectColorTo: Color,
    pub mfEffectColorTime: f32,
    pub mfEffectColorT: f32,
    pub mfMeshFadeAlpha: f32,
    pub mfMeshFadeSpeed: f32,
    pub mfLiquidAmount: f32,
    pub mfLiquidTarget: f32,
    pub mfLiquidSpeed: f32,
    pub mfHealth: f32,
    pub mbBroken: bool,
    pub mbBreakable: bool,
    pub mbScriptCollision: bool,
    pub mbProcDirty: bool,
    pub mbStaticMoveSound: bool,
    pub mlStaticMoveLoopID: i32,
    pub mbCameraInLiquid: bool,
    pub mbPlayerInLiquid: bool,
    pub mfLiquidTime: f32,
    pub mfLiquidSurfaceY: f32,
    pub m_mtxLiquidPlaced: Matrixf,
    pub mbCheckCollision: bool,
    pub mbCheckCenterInArea: bool,
    pub mbCheckDynamic: bool,
    pub mbCheckStatic: bool,
    pub mbCheckCharacters: bool,
    pub mfTimeSinceCheck: f32,
    pub mMove: Move,
    pub mRotate: Rotate,
    pub mlCurrentAnim: i32,
    pub mlGuiDraws: i32,
    pub mlGuiCalls: i32,
    pub mbGuiActive: bool,
    pub mfGuiFade: f32,
    pub mfGuiFadeSpeed: f32,
    pub mfGuiFPS: f32,
    pub mfGuiTimeAcc: f32,
    pub mbGuiSetUseInput: bool,
    pub mbGuiUpdateWhenOutOfView: bool,
    pub mbGuiDirty: bool,
    pub mfGuiCacheAtScreenSize: f32,
    pub mbGuiCacheForced: bool,
    pub mvGuiOrigin: Vector3f,
    pub mvGuiRight: Vector3f,
    pub mvGuiDown: Vector3f,
    pub mlAnimLoopSoundID: i32,
}

impl Default for Core {
    fn default() -> Self {
        Self {
            mbSpawned: false,
            meType: 0,
            mID: Id::default(),
            mlParentType: -1,
            mParentID: Id::default(),
            mbActive: true,
            mbSleeping: false,
            mbAutoSleep: true,
            mbShowMesh: true,
            mbIsDoor: false,
            mbIsClosedDoor: false,
            mbInteractionDisabled: false,
            mfMaxInteractDistance: -1.0,
            mfVariableUpdateRate: 0.0,
            mfVariableUpdateAcc: 0.0,
            mbInteractedWith: false,
            m_mtxOnLoad: IDENTITY,
            mvScale: Vector3f {
                x: 1.0,
                y: 1.0,
                z: 1.0,
            },
            mvMeshScaleMul: Vector3f {
                x: 1.0,
                y: 1.0,
                z: 1.0,
            },
            mbGlobalSpaceAnim: false,
            mvSize: Vector3f {
                x: 1.0,
                y: 1.0,
                z: 1.0,
            },
            mbCheckOutsidePlayer: false,
            mbCharCollisionUntilStopped: false,
            mbCharCollisionUntilStoppedStarted: false,
            mvBodyOffset: Vector3f::default(),
            mbInteractCallbackAutoRemove: false,
            mbAnimMessageCallbackAutoRemove: false,
            mbLookAtCallbackAutoRemove: true,
            mbLookAtCheckCenter: true,
            mbLookAtCheckRay: true,
            mfLookAtMaxDistance: -1.0,
            mfLookAtDelay: 0.0,
            mbForceLookAtCheck: false,
            mfLookAtCheckTimer: 0.0,
            mfLookAtCountdown: -1.0,
            mbLookedAt: false,
            mbEffectsActive: true,
            mbStaticPhysics: false,
            mbAllowMapTransfer: true,
            mfEffectsAlpha: 1.0,
            mfEffectsFadeSpeed: 0.0,
            mfEffectsOffAlpha: 0.0,
            mIllumBaseColor: Color {
                r: 1.0,
                g: 1.0,
                b: 1.0,
                a: 1.0,
            },
            mbIllumBaseSet: false,
            mInteractLeaderID: Id::default(),
            mfInteractConnectionState: 0.0,
            mEffectBaseColor: Color {
                r: 1.0,
                g: 1.0,
                b: 1.0,
                a: 1.0,
            },
            mEffectColorFrom: Color {
                r: 0.0,
                g: 0.0,
                b: 0.0,
                a: 1.0,
            },
            mEffectColorTo: Color {
                r: 0.0,
                g: 0.0,
                b: 0.0,
                a: 1.0,
            },
            mfEffectColorTime: 0.0,
            mfEffectColorT: 0.0,
            mfMeshFadeAlpha: 1.0,
            mfMeshFadeSpeed: 0.0,
            mfLiquidAmount: 0.0,
            mfLiquidTarget: 0.0,
            mfLiquidSpeed: 0.0,
            mfHealth: 100.0,
            mbBroken: false,
            mbBreakable: false,
            mbScriptCollision: false,
            mbProcDirty: false,
            mbStaticMoveSound: false,
            mlStaticMoveLoopID: -1,
            mbCameraInLiquid: false,
            mbPlayerInLiquid: false,
            mfLiquidTime: 0.0,
            mfLiquidSurfaceY: 0.0,
            m_mtxLiquidPlaced: IDENTITY,
            mbCheckCollision: false,
            mbCheckCenterInArea: false,
            mbCheckDynamic: true,
            mbCheckStatic: false,
            mbCheckCharacters: false,
            mfTimeSinceCheck: 0.0,
            mMove: Move::default(),
            mRotate: Rotate {
                goal: IDENTITY,
                ..Rotate::default()
            },
            mlCurrentAnim: -1,
            mlGuiDraws: 0,
            mlGuiCalls: 0,
            mbGuiActive: true,
            mfGuiFade: 1.0,
            mfGuiFadeSpeed: 0.0,
            mfGuiFPS: 30.0,
            mfGuiTimeAcc: 0.0,
            mbGuiSetUseInput: true,
            mbGuiUpdateWhenOutOfView: false,
            mbGuiDirty: true,
            mfGuiCacheAtScreenSize: 0.0,
            mbGuiCacheForced: false,
            mvGuiOrigin: Vector3f::default(),
            mvGuiRight: Vector3f::default(),
            mvGuiDown: Vector3f::default(),
            mlAnimLoopSoundID: -1,
        }
    }
}

pub const CORE_SIZE: usize = 664;
const _: () = assert!(std::mem::size_of::<Core>() == CORE_SIZE);

#[unsafe(no_mangle)]
pub extern "C" fn ohpl_se_core_new() -> *mut Core {
    Box::into_raw(Box::new(Core::default()))
}

/// # Safety
/// `p` came from `ohpl_se_core_new` and is not used afterwards.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ohpl_se_core_free(p: *mut Core) {
    if !p.is_null() {
        // SAFETY: caller contract.
        drop(unsafe { Box::from_raw(p) });
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn defaults_match_the_cpp_initialisers() {
        let c = Core::default();
        assert!(c.mbActive && c.mbAutoSleep && c.mbShowMesh && c.mbCheckDynamic && c.mbGuiActive);
        assert_eq!(
            (c.mfHealth, c.mfGuiFPS, c.mlParentType, c.mlCurrentAnim),
            (100.0, 30.0, -1, -1)
        );
        assert_eq!(c.m_mtxOnLoad, IDENTITY);
        assert_eq!(c.mRotate.goal, IDENTITY);
        assert_eq!(c.mEffectColorFrom.a, 1.0);
    }
}
