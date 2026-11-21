//! C++ memory layout of the Amnesia save classes (`LuxSavedEngineTypes.h`, `LuxSavedGameTypes.h`,
//! `LuxSavedGame.h`): member order and types, in declaration order. Offsets and sizes of the
//! serialised members come from these mirrors; `cshim/layout_check.cpp` asserts the class sizes.

#![allow(non_snake_case, non_camel_case_types, dead_code)]

use std::mem::{offset_of, size_of};

type Str = [u64; 4];
type V2 = [f32; 2];
type V2l = [i32; 2];
type V3 = [f32; 3];
type Col = [f32; 4];
type Mtx = [f32; 16];
type Cont = [usize; 4];
type IdPair = [u64; 2];
type SavedMap = [u64; 76];

macro_rules! mirror {
    ($($cls:ident { $($f:ident : $t:ty),* $(,)? })*) => {
        $(
            #[repr(C)]
            pub struct $cls { pub vtable: usize, $(pub $f: $t),* }
        )*

        pub fn size(class: &[u8]) -> Option<usize> {
            $( if class == stringify!($cls).as_bytes() { return Some(size_of::<$cls>()); } )*
            None
        }

        pub fn field(class: &[u8], name: &[u8]) -> Option<(usize, usize)> {
            $(
                if class == stringify!($cls).as_bytes() {
                    $( if name == stringify!($f).as_bytes() {
                        return Some((offset_of!($cls, $f), size_of::<$t>()));
                    } )*
                    return None;
                }
            )*
            None
        }

        pub const CLASSES: &[&str] = &[$(stringify!($cls)),*];
    };
}

mirror! {
    cEngineCharacterBody_SaveData { mbActive: u8, mfMass: f32, mvPosition: V3, mfYaw: f32, mfPitch: f32, mfSpeedForward: f32, mfSpeedRight: f32, mvForceVelocity: V3, mlActiveSize: i32 }
    cEngineVerletParticle_SaveData { mvPosition: V3, mvPrevPosition: V3, mvSmoothPosition: V3, mfInvMass: f32 }
    cEngineRope_SaveData { mvParticles: Cont, msName: Str, mlUniqueGfxID: i32, msPhysicsName: Str, mlUniquePhysicsID: i32, mvStartPos: V3, mvEndPos: V3, mStartBody: IdPair, mEndBody: IdPair, mvStartBodyLocalPos: V3, mvEndBodyLocalPos: V3, mlMaxIterations: i32, mfTotalLength: f32, mfMinTotalLength: f32, mfMaxTotalLength: f32, mfSegmentLength: f32, mfDamping: f32, mfStrength: f32, mfStiffness: f32, msMaterial: Str, mfParticleRadius: f32, mlMaxSegments: i32, mfRadius: f32, mfLengthTileAmount: f32, mfLengthTileSize: f32, msSound: Str, mfSoundStartSpeed: f32, mfSoundStopSpeed: f32, mbMotorActive: u8, mfMotorWantedLength: f32, mfMotorSpeedMul: f32, mfMotorMinSpeed: f32, mfMotorMaxSpeed: f32, mbAutoMove: u8, mfAutoMoveSpeed: f32, mfAutoMoveAcc: f32, mfAutoMoveMaxSpeed: f32 }
    cEngineBillboard_SaveData { mColor: Col }
    cEngineAnimationState_SaveData { mbActive: u8, mbLoop: u8, mfWeight: f32, mfFadeStep: f32, mfTimePos: f32, mfSpeed: f32 }
    cEngineSubMeshEntity_SaveData { mbActive: u8, mbVisible: u8, mfIlluminationAmount: f32, m_mtxTransform: Mtx }
    cEngineMeshEntity_SaveData { mbActive: u8, mbVisible: u8, mfIlluminationAmount: f32, m_mtxTransform: Mtx, mvAnimations: Cont, mvSubMeshEntities: Cont }
    cEngineBody_SaveData { mfMass: f32, mbActive: u8, mbCollideCharacter: u8, mvLinearVelocity: V3, mvAngularVelocity: V3, m_mtxTransform: Mtx }
    cEngineJointController_SaveData { msName: Str, mfDestValue: f32, mbActive: u8 }
    cEngineJoint_SaveData { msName: Str, mlID: i32, mfMinLimit: f32, mfMaxLimit: f32, mbBroken: u8, msOnMinCallback: Str, msOnMaxCallback: Str, mvControllers: Cont }
    cEnginePSEmitter_SaveData { mbActive: u8 }
    cEnginePS_SaveData { msName: Str, mlID: i32, msType: Str, mvSize: V3, m_mtxTransform: Mtx, mColor: Col, mbFadeAtDistance: u8, mfMinFadeDistanceStart: f32, mfMinFadeDistanceEnd: f32, mfMaxFadeDistanceStart: f32, mfMaxFadeDistanceEnd: f32, mbActive: u8, mbVisible: u8, mvEmitterActive: Cont }
    cEngineBeam_SaveData { msName: Str, mlID: i32, msFile: Str, mvStartPos: V3, mvEndPos: V3 }
    cEngineSound_SaveData { msName: Str, mlID: i32, mbActive: u8, mbStopped: u8, msSoundDataName: Str, mfMinDistance: f32, mfMaxDistance: f32, mfVolume: f32, mvLocalPosition: V3, mbRemoveWhenOver: u8 }
    cEngineLight_SaveData { msName: Str, mlID: i32, mbActive: u8, mbVisible: u8, mbOnlyAffectInSector: u8, mDiffuseColor: Col, mfFarAttenuation: f32, mbFlickering: u8, msFlickerOffSound: Str, msFlickerOnSound: Str, msFlickerOffPS: Str, msFlickerOnPS: Str, mfFlickerOnMinLength: f32, mfFlickerOffMinLength: f32, mfFlickerOnMaxLength: f32, mfFlickerOffMaxLength: f32, mFlickerOffColor: Col, mfFlickerOffRadius: f32, mbFlickerFade: u8, mfFlickerOnFadeMinLength: f32, mfFlickerOnFadeMaxLength: f32, mfFlickerOffFadeMinLength: f32, mfFlickerOffFadeMaxLength: f32 }
    cLuxLoadScreenHandler_SaveData { msLoadTextCat: Str, msLoadTextEntry: Str, msLoadTextImage: Str, mlTextRandomNum: i32, mlPrevTextNumBitFlags: i32 }
    cLuxInsanityHandler_Event_SaveData { mbUsed: u8 }
    cLuxInsanityHandler_Set_SaveData { msName: Str }
    cLuxInsanityHandler_SaveData { mfNewEventCount: f32, mvEvents: Cont, mvDisabledSets: Cont }
    cLuxHintHandler_Hint_SaveData { msName: Str }
    cLuxHintHandler_SaveData { mlstHintsGiven: Cont, mlstHintsBlocked: Cont }
    cLuxGlobalDataHandler_SaveData { mlstVars: Cont }
    cLuxEffectHandler_SoundMul_SaveData { mbActive: u8, mfVal: f32, mfDest: f32, mfSpeed: f32, mTypes: u32, mbDestroyAtDest: u8 }
    cLuxEffectHandler_SaveData { mbFade_Active: u8, mfFade_GoalAlpha: f32, mfFade_Alpha: f32, mfFade_FadeSpeed: f32, mbImageTrail_Active: u8, mfImageTrail_Amount: f32, mfImageTrail_AmountGoal: f32, mfImageTrail_FadeSpeed: f32, mbSepiaColor_Active: u8, mfSepiaColor_Amount: f32, mfSepiaColor_AmountGoal: f32, mfSepiaColor_FadeSpeed: f32, mbRadialBlur_Active: u8, mfRadialBlur_Size: f32, mfRadialBlur_BlurStartDist: f32, mfRadialBlur_SizeGoal: f32, mfRadialBlur_FadeSpeed: f32, msVoiceOverCallback: Str, mvVoiceData: Cont, mbVoiceActive: u8, mvGlobalSoundVolumeMul: Cont, mvGlobalSoundSpeedMul: Cont }
    cLuxMusicHandler_Music_SaveData { msFile: Str, mfVolume: f32, mbLoop: u8, mbResume: u8, mbSpecialEffect: u8 }
    cLuxMusicHandler_SaveData { msCurrentMusic: Str, mfCurrentMusicVolume: f32, mfCurrentMusicVolumeAdd: f32, mfCurrentMusicMaxVolume: f32, mfCurrentMusicTime: f32, mbCurrentMusicLoop: u8, mlCurrentMaxPrio: i32, mbEnemyClosePlaying: u8, mbAttackPlaying: u8, mbSearchPlaying: u8, mvMusic: Cont, mvAttackerIDs: Cont, mvSearcherIDs: Cont }
    cLuxJournal_DiaryCont_SaveData { msType: Str, mvDiaries: Cont }
    cLuxJournal_SaveData { mlLastReadTextCat: i32, mlLastReadTextEntry: i32, mlLastReadTextType: i32, mvNotes: Cont, mvDiaryConts: Cont, mvQuestNotes: Cont }
    cLuxInventory_Item_SaveData { msName: Str, mlType: i32, msSubType: Str, msVal: Str, msExtraVal: Str, msImageFile: Str, mfAmount: f32, mlCount: i32, msGameNameEntry: Str, msGameDescEntry: Str }
    cLuxInventory_SaveData { mbDisabled: u8, mlEquippedHandItem: i32, mvItems: Cont, mvCombineCallbacks: Cont }
    cLuxPlayerHands_SaveData { mlState: i32, msCurrentHandObject: Str, msCurrentAnim: Str, mMesh: cEngineMeshEntity_SaveData, mfHandObjectChargeCount: f32, mlHandObjectState: i32, mbHandObjectAttackDown: u8, mbHandObjectInteractDown: u8, mfHandObjectAlpha: f32 }
    cLuxFlashbackData_SaveData { msFile: Str, msCallback: Str }
    cLuxPlayer_SaveData { mlState: i32, mpStateData: usize, mbActive: u8, mbUsesDragFootsteps: u8, mfHealth: f32, mfTerror: f32, mfSanity: f32, mfLampOil: f32, mlCoins: i32, mlTinderboxes: i32, mbIsInWater: u8, msWaterStepSound: Str, mfWaterSpeedMul: f32, mbCrouching: u8, mbJumpDisabled: u8, mbCrouchDisabled: u8, mbSanityDrainDisabled: u8, mfInteractionMoveSpeedMul: f32, mfEventMoveSpeedMul: f32, mfEventRunSpeedMul: f32, mfScriptMoveSpeedMul: f32, mfScriptRunSpeedMul: f32, mfHurtMoveSpeedMul: f32, mfInsanityCollapseSpeedMul: f32, mfScriptJumpForceMul: f32, mfLookSpeedMul: f32, mfAspectMul: f32, mfFOVMul: f32, mfAspectMulGoal: f32, mfFOVMulGoal: f32, mfAspectMulSpeed: f32, mfFOVMulSpeed: f32, mfRoll: f32, mfRollGoal: f32, mfRollSpeedMul: f32, mfRollMaxSpeed: f32, mbFadingPitch: u8, mfPitchGoal: f32, mfPitchSpeedMul: f32, mfPitchMaxSpeed: f32, mvCamAnimPos: V3, mvCamAnimPosGoal: V3, mfCamAnimPosSpeedMul: f32, mfCamAnimPosMaxSpeed: f32, mvHeadSpinSpeed: V2, mbScriptShowFocusIconAndCrossHair: u8, mbNoFallDamage: u8, mbLanternOn: u8, mbLanternDisabled: u8, msDeathHintCat: Str, msDeathHintEntry: Str, mbSpawnPSActive: u8, mbSpawnPSFile: Str, mbFlashbackActive: u8, mfFlashbackStartCount: f32, mfFlashDelay: f32, msFlashbackFile: Str, msFlashbackCallback: Str, mlstFlashbackQueue: Cont, mbInsanityCollapse_Active: u8, mlInsanityCollapse_State: i32, mfInsanityCollapse_HeightAdd: f32, mfInsanityCollapse_Roll: f32, mfInsanityCollapse_T: f32, mfSanity_AtLowSanityCount: f32, mbLookAt_Active: u8, mvLookAt_TargetPos: V3, mvLookAt_CurrentSpeed: V3, mfLookAt_SpeedMul: f32, mfLookAt_MaxSpeed: f32, msLookAt_AtTargetCallback: Str, mfLookAt_DestFovMul: f32, mfLookAt_Fov: f32, mfLookAt_FovSpeed: f32, mfLookAt_FovMaxSpeed: f32, mfDarkness_Active: u8, mvCameraAngles: V3, mCharBody: cEngineCharacterBody_SaveData, mfPitchMaxLimit: f32, mfPitchMinLimit: f32, mfYawMaxLimit: f32, mfYawMinLimit: f32, mvHeadPosAdds: Cont, mvTerrorEnemyIDs: Cont, mPlayerHands: cLuxPlayerHands_SaveData, mlstCollideCallbacks: Cont }
    cLuxSaveGame_SaveData { mbHardmode: u8, msMapFolder: Str, mMap: SavedMap, mInventory: cLuxInventory_SaveData, mJournal: cLuxJournal_SaveData, mPlayer: cLuxPlayer_SaveData, mMusicHandler: cLuxMusicHandler_SaveData, mEffectHandler: cLuxEffectHandler_SaveData, mGlobalDataHandler: cLuxGlobalDataHandler_SaveData, mHintHandler: cLuxHintHandler_SaveData, mInsanityHandler: cLuxInsanityHandler_SaveData, mLoadScreenHandler: cLuxLoadScreenHandler_SaveData, mpSavedMaps: usize }
    cLuxSavedGameEntity { mlID: i32, mvAttachedProps: Cont, mvInteractConnections: Cont, mvConnections: Cont, mlstCollideCallbacks: Cont }
    cLuxSavedGameEnemy { mlID: i32, mbActive: u8, mvPatrolNodes: Cont }
    cLuxSavedGameEntityFunc { mlID: i32, msFunc: Str, mbAutoRemove: u8 }
    cLuxSavedGameMap { msName: Str, msDisplayNameEntry: Str, mlNumberOfQuests: i32, mlTotalCompletionAmount: i32, mlCurrentCompletionAmount: i32, mbSkyBoxActive: u8, msSkyboxTexture: Str, mSkyBoxColor: Col, mbFogActive: u8, mbFogCulling: u8, mfFogStart: f32, mfFogEnd: f32, mfFogFalloffExp: f32, mFogColor: Col, mlstFullEntities: Cont, mlstCollideAndConnectEntities: Cont, mlstEnemies: Cont, mlstLights: Cont, mlstSounds: Cont, mlstPS: Cont, mlstTimers: Cont, mlstVars: Cont, mvUseItemCallbacks: Cont, mlstPlayerCollideCallbacks: Cont, mlstEntityFunc_Look: Cont, mlstEntityFunc_Interact: Cont, mlstEntityFunc_Callback: Cont, mlstEntityFunc_StateChange: Cont, mlstDisabledEntities: Cont, mlstLockedDoors: Cont, mlstLockedLevelDoors: Cont, mlstActiveItems: Cont, mlstOpenChests: Cont, mlstUnbrokenItemContainers: Cont, mlstBrokenContainersWithActiveItem: Cont, mlstUnlitLamps: Cont }
    cLuxSavedGameMapCollection { mlstMaps: Cont }
}
