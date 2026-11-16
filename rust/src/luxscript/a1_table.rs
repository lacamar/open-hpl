//! Registration table: declarations in the order of the original `InitScriptFunctions`.

use super::{a1_native as native, fns};
use std::ffi::c_void;

pub fn table() -> Vec<(&'static str, &'static str, *const c_void)> {
    vec![
        (
            "void Print(const string &in asString)",
            "Print",
            fns::misc::Print as *const c_void,
        ),
        (
            "void AddDebugMessage(const string &in asString, bool abCheckForDuplicates)",
            "AddDebugMessage",
            fns::misc::AddDebugMessage as *const c_void,
        ),
        (
            "void ProgLog(const string &in asLevel, const string &in asMessage)",
            "ProgLog",
            fns::misc::ProgLog as *const c_void,
        ),
        (
            "bool ScriptDebugOn()",
            "ScriptDebugOn",
            fns::misc::ScriptDebugOn as *const c_void,
        ),
        (
            "float RandFloat(float afMin, float afMax)",
            "RandFloat",
            fns::misc::RandFloat as *const c_void,
        ),
        (
            "int RandInt(int alMin, int alMax)",
            "RandInt",
            fns::misc::RandInt as *const c_void,
        ),
        (
            "bool StringContains(const string &in asString, const string &in asSubString)",
            "StringContains",
            native::f_StringContains as *const c_void,
        ),
        (
            "string& StringSub(const string &in asString, int alStart, int alCount)",
            "StringSub",
            fns::misc::StringSub as *const c_void,
        ),
        (
            "void AddTimer(const string &in asName, float afTime, const string &in asFunction)",
            "AddTimer",
            fns::misc::AddTimer as *const c_void,
        ),
        (
            "void RemoveTimer(const string &in asName)",
            "RemoveTimer",
            fns::misc::RemoveTimer as *const c_void,
        ),
        (
            "float GetTimerTimeLeft(const string &in asName)",
            "GetTimerTimeLeft",
            fns::misc::GetTimerTimeLeft as *const c_void,
        ),
        (
            "void SetLocalVarInt(const string &in asName, int alVal)",
            "SetLocalVarInt",
            fns::misc::SetLocalVarInt as *const c_void,
        ),
        (
            "void SetLocalVarFloat(const string &in asName, float afVal)",
            "SetLocalVarFloat",
            fns::misc::SetLocalVarFloat as *const c_void,
        ),
        (
            "void SetLocalVarString(const string &in asName, const string &in asVal)",
            "SetLocalVarString",
            fns::misc::SetLocalVarString as *const c_void,
        ),
        (
            "void AddLocalVarInt(const string &in asName, int alVal)",
            "AddLocalVarInt",
            fns::misc::AddLocalVarInt as *const c_void,
        ),
        (
            "void AddLocalVarFloat(const string &in asName, float afVal)",
            "AddLocalVarFloat",
            fns::misc::AddLocalVarFloat as *const c_void,
        ),
        (
            "void AddLocalVarString(const string &in asName, const string &in asVal)",
            "AddLocalVarString",
            fns::misc::AddLocalVarString as *const c_void,
        ),
        (
            "int GetLocalVarInt(const string &in asName)",
            "GetLocalVarInt",
            fns::misc::GetLocalVarInt as *const c_void,
        ),
        (
            "float GetLocalVarFloat(const string &in asName)",
            "GetLocalVarFloat",
            fns::misc::GetLocalVarFloat as *const c_void,
        ),
        (
            "string& GetLocalVarString(const string &in asName)",
            "GetLocalVarString",
            fns::misc::GetLocalVarString as *const c_void,
        ),
        (
            "void SetGlobalVarInt(const string &in asName, int alVal)",
            "SetGlobalVarInt",
            fns::misc::SetGlobalVarInt as *const c_void,
        ),
        (
            "void SetGlobalVarFloat(const string &in asName, float afVal)",
            "SetGlobalVarFloat",
            fns::misc::SetGlobalVarFloat as *const c_void,
        ),
        (
            "void SetGlobalVarString(const string &in asName, const string &in asVal)",
            "SetGlobalVarString",
            fns::misc::SetGlobalVarString as *const c_void,
        ),
        (
            "void AddGlobalVarInt(const string &in asName, int alVal)",
            "AddGlobalVarInt",
            fns::misc::AddGlobalVarInt as *const c_void,
        ),
        (
            "void AddGlobalVarFloat(const string &in asName, float afVal)",
            "AddGlobalVarFloat",
            fns::misc::AddGlobalVarFloat as *const c_void,
        ),
        (
            "void AddGlobalVarString(const string &in asName, const string &in asVal)",
            "AddGlobalVarString",
            fns::misc::AddGlobalVarString as *const c_void,
        ),
        (
            "int GetGlobalVarInt(const string &in asName)",
            "GetGlobalVarInt",
            fns::misc::GetGlobalVarInt as *const c_void,
        ),
        (
            "float GetGlobalVarFloat(const string &in asName)",
            "GetGlobalVarFloat",
            fns::misc::GetGlobalVarFloat as *const c_void,
        ),
        (
            "string& GetGlobalVarString(const string &in asName)",
            "GetGlobalVarString",
            fns::misc::GetGlobalVarString as *const c_void,
        ),
        (
            "void StartCredits(const string &in asMusic, bool abLoopMusic, const string &in asTextCat, const string &in asTextEntry, int alEndNum)",
            "StartCredits",
            fns::misc::StartCredits as *const c_void,
        ),
        (
            "void AddKeyPart(int alKeyPart)",
            "AddKeyPart",
            fns::misc::AddKeyPart as *const c_void,
        ),
        (
            "void StartDemoEnd()",
            "StartDemoEnd",
            fns::misc::StartDemoEnd as *const c_void,
        ),
        (
            "void AutoSave()",
            "AutoSave",
            fns::misc::AutoSave as *const c_void,
        ),
        (
            "void CheckPoint(const string &in asName,const string &in asStartPos ,const string &in asCallback, const string &in asDeathHintCat, const string &in asDeathHintEntry)",
            "CheckPoint",
            fns::misc::CheckPoint as *const c_void,
        ),
        (
            "void CheckPoint(const string &in asName,const string &in asStartPos ,const string &in asCallback, const string &in asDeathHintCat, const string &in asDeathHintEntry, bool abAmfpExtraArg)",
            "CheckPointAmfp",
            fns::misc::CheckPointAmfp as *const c_void,
        ),
        (
            "void AddHint(const string &in asEntity, const string &in asUnknown)",
            "AmfpStub_AddHint",
            fns::stubs::AmfpStub_AddHint as *const c_void,
        ),
        (
            "bool GetEntityActive(const string &in asName)",
            "AmfpStub_GetEntityActive",
            fns::misc::AmfpStub_GetEntityActive as *const c_void,
        ),
        (
            "void SetEnemyMoveType(const string &in asEnemyName, const string &in asMoveType)",
            "AmfpStub_SetEnemyMoveType",
            fns::stubs::AmfpStub_SetEnemyMoveType as *const c_void,
        ),
        (
            "void SetJournalDisabled(bool abDisabled)",
            "AmfpStub_SetJournalDisabled",
            fns::stubs::AmfpStub_SetJournalDisabled as *const c_void,
        ),
        (
            "void SetLampFlickerActive(const string &in asName, bool abActive)",
            "AmfpStub_SetLampFlickerActive",
            fns::stubs::AmfpStub_SetLampFlickerActive as *const c_void,
        ),
        (
            "void SetLanternFlickerActive(bool abActive)",
            "AmfpStub_SetLanternFlickerActive",
            fns::stubs::AmfpStub_SetLanternFlickerActive as *const c_void,
        ),
        (
            "void SetManPigType(const string &in asEnemyName, const string &in asType)",
            "AmfpStub_SetManPigType",
            fns::stubs::AmfpStub_SetManPigType as *const c_void,
        ),
        (
            "void SetParticleSystemActive(const string &in asName, bool abActive)",
            "AmfpStub_SetParticleSystemActive",
            fns::stubs::AmfpStub_SetParticleSystemActive as *const c_void,
        ),
        (
            "void SetPhysicsAutoDisable(const string &in asName, bool abActive)",
            "AmfpStub_SetPhysicsAutoDisable",
            fns::stubs::AmfpStub_SetPhysicsAutoDisable as *const c_void,
        ),
        (
            "void SetPlayerInfection(float afAmount)",
            "AmfpStub_SetPlayerInfection",
            fns::stubs::AmfpStub_SetPlayerInfection as *const c_void,
        ),
        (
            "void ShowScreenImage(const string &in asFile, float afX, float afY, float afZ, bool abFadeIn, float afA, float afB, float afC)",
            "AmfpStub_ShowScreenImage",
            fns::stubs::AmfpStub_ShowScreenImage as *const c_void,
        ),
        (
            "void StartPhoneRinging(const string &in asName)",
            "AmfpStub_StartPhoneRinging",
            fns::stubs::AmfpStub_StartPhoneRinging as *const c_void,
        ),
        (
            "void StopPhoneRinging(const string &in asName)",
            "AmfpStub_StopPhoneRinging",
            fns::stubs::AmfpStub_StopPhoneRinging as *const c_void,
        ),
        (
            "void AddEffectVoice2(const string &in asVoiceFile, const string &in asEffectFile, const string &in asTextCat, const string &in asTextEntry, float afTextDelay, const string &in asText2Entry, float afText2Delay, bool abUsePostion,  const string &in asPosEnitity, float afMinDistance, float afMaxDistance)",
            "AddEffectVoice2",
            fns::fx::AddEffectVoice2 as *const c_void,
        ),
        (
            "void AddEffectVoice3(const string &in asVoiceFile, const string &in asEffectFile, const string &in asTextCat, const string &in asTextEntry, float afTextDelay, const string &in asText2Entry, float afText2Delay, const string &in asText3Entry, float afText3Delay, bool abUsePostion,  const string &in asPosEnitity, float afMinDistance, float afMaxDistance)",
            "AddEffectVoice3",
            fns::fx::AddEffectVoice3 as *const c_void,
        ),
        (
            "void AddEffectVoice5(const string &in asVoiceFile, const string &in asEffectFile, const string &in asTextCat, const string &in asTextEntry, float afTextDelay, const string &in asText2Entry, float afText2Delay, const string &in asText3Entry, float afText3Delay, const string &in asText4Entry, float afText4Delay, const string &in asText5Entry, float afText5Delay, bool abUsePostion, const string &in asPosEnitity, float afMinDistance, float afMaxDistance)",
            "AddEffectVoice5",
            fns::fx::AddEffectVoice5 as *const c_void,
        ),
        (
            "void AddEffectVoice6(const string &in asVoiceFile, const string &in asEffectFile, const string &in asTextCat, const string &in asTextEntry, float afTextDelay, const string &in asText2Entry, float afText2Delay, const string &in asText3Entry, float afText3Delay, const string &in asText4Entry, float afText4Delay, const string &in asText5Entry, float afText5Delay, const string &in asText6Entry, float afText6Delay, bool abUsePostion, const string &in asPosEnitity, float afMinDistance, float afMaxDistance)",
            "AddEffectVoice6",
            fns::fx::AddEffectVoice6 as *const c_void,
        ),
        (
            "void AddEffectVoice7(const string &in asVoiceFile, const string &in asEffectFile, const string &in asTextCat, const string &in asTextEntry, float afTextDelay, const string &in asText2Entry, float afText2Delay, const string &in asText3Entry, float afText3Delay, const string &in asText4Entry, float afText4Delay, const string &in asText5Entry, float afText5Delay, const string &in asText6Entry, float afText6Delay, const string &in asText7Entry, float afText7Delay, bool abUsePostion, const string &in asPosEnitity, float afMinDistance, float afMaxDistance)",
            "AddEffectVoice7",
            fns::fx::AddEffectVoice7 as *const c_void,
        ),
        (
            "void AddPlayerInfection(float afInfection)",
            "AmfpStub_AddPlayerInfection",
            fns::stubs::AmfpStub_AddPlayerInfection as *const c_void,
        ),
        (
            "void AttachAreaToProp(const string &in asAreaName, const string &in asProp, int alBody)",
            "AttachAreaToProp",
            fns::prop::AttachAreaToProp as *const c_void,
        ),
        (
            "void AttachPropToBone(const string &in asChildEntityName, const string &in asParentEntityName, const string &in asParentBoneName, float fPosX, float fPosY, float fPosZ, float fRotX, float fRotY, float fRotZ)",
            "AttachPropToBone",
            fns::prop::AttachPropToBone as *const c_void,
        ),
        (
            "void ChangeEnemyPose(const string &in asName, const string &in asPoseType)",
            "AmfpStub_ChangeEnemyPose",
            fns::stubs::AmfpStub_ChangeEnemyPose as *const c_void,
        ),
        (
            "void DestroyParticleSystemInstantly(const string &in asName)",
            "DestroyParticleSystemInstantly",
            fns::world::DestroyParticleSystemInstantly as *const c_void,
        ),
        (
            "void DetachPropFromBone(const string &in asChildEntityName)",
            "DetachPropFromBone",
            fns::prop::DetachPropFromBone as *const c_void,
        ),
        (
            "void FadeLampTo(const string &in asName, float afR, float afG, float afB, float afA, float afRadius, float afTime)",
            "FadeLampTo",
            fns::prop::FadeLampTo as *const c_void,
        ),
        (
            "void FadePlayerPitchTo(float afX, float afSpeedMul, float afMaxSpeed)",
            "FadePlayerPitchTo",
            fns::player::FadePlayerPitchTo as *const c_void,
        ),
        (
            "void ForceEnemyWaitState(const string &in asName)",
            "ForceEnemyWaitState",
            fns::enemy::ForceEnemyWaitState as *const c_void,
        ),
        (
            "float GetEnemyPlayerDistance(const string &in asEnemyName)",
            "GetEnemyPlayerDistance",
            fns::enemy::GetEnemyPlayerDistance as *const c_void,
        ),
        (
            "int GetEnemyState(const string &in asName)",
            "GetEnemyState",
            fns::enemy::GetEnemyState as *const c_void,
        ),
        (
            "bool GetJournalDisabled()",
            "AmfpStub_GetJournalDisabled",
            fns::misc::AmfpStub_GetJournalDisabled as *const c_void,
        ),
        (
            "bool GetPlayerCanSeeEnemy(const string &in asEnemyName)",
            "GetPlayerCanSeeEnemy",
            fns::enemy::GetPlayerCanSeeEnemy as *const c_void,
        ),
        (
            "float GetPlayerInfection()",
            "AmfpStub_GetPlayerInfection",
            fns::misc::AmfpStub_GetPlayerInfection as *const c_void,
        ),
        (
            "void GiveInfectionDamage(float afAmount, bool abUseEffect)",
            "AmfpStub_GiveInfectionDamage",
            fns::stubs::AmfpStub_GiveInfectionDamage as *const c_void,
        ),
        (
            "void HangUpPhone(const string &in asName)",
            "AmfpStub_HangUpPhone",
            fns::stubs::AmfpStub_HangUpPhone as *const c_void,
        ),
        (
            "void HideScreenImageImmediately()",
            "AmfpStub_HideScreenImageImmediately",
            fns::stubs::AmfpStub_HideScreenImageImmediately as *const c_void,
        ),
        (
            "void HideScreenImageWithFade(float afFadeOut)",
            "AmfpStub_HideScreenImageWithFade",
            fns::stubs::AmfpStub_HideScreenImageWithFade as *const c_void,
        ),
        (
            "void PauseCurrentPropAnimation(const string &in asProp, float afFadeTime)",
            "PauseCurrentAnimation",
            fns::prop::PauseCurrentAnimation as *const c_void,
        ),
        (
            "void PlayCurrentPropAnimation(const string &in asProp, float afFadeTime, bool abLoop)",
            "PlayCurrentAnimation",
            fns::prop::PlayCurrentAnimation as *const c_void,
        ),
        (
            "void PlayEnemyAnimation(const string &in asEnemyName, const string &in asAnimName, bool abLoop, float afFadeTime)",
            "PlayEnemyAnimation",
            fns::enemy::PlayEnemyAnimation as *const c_void,
        ),
        (
            "void PlayScriptedAnimation(const string &in asEnemyName, const string &in asAnimationName, bool abLoopAnimation)",
            "PlayScriptedAnimation",
            fns::enemy::PlayScriptedAnimation as *const c_void,
        ),
        (
            "void PlaySoundAtPosition(const string &in asSoundName, const string &in asSoundFile, float afX, float afY, float afZ, float afFadeSpeed, bool abSaveSound)",
            "PlaySoundAtPosition",
            fns::world::PlaySoundAtPosition as *const c_void,
        ),
        (
            "void ReleasePlayerFromLimbo()",
            "AmfpStub_ReleasePlayerFromLimbo",
            fns::stubs::AmfpStub_ReleasePlayerFromLimbo as *const c_void,
        ),
        (
            "void RemoveAllHints()",
            "AmfpStub_RemoveAllHints",
            fns::stubs::AmfpStub_RemoveAllHints as *const c_void,
        ),
        (
            "void SendEnemyTimeOut(const string &in asEnemyName, float afTimeOut)",
            "SendEnemyTimeOut",
            fns::enemy::SendEnemyTimeOut as *const c_void,
        ),
        (
            "void SetButtonCanBeSwitchedOff(const string &in asName, bool abCanBeSwitchedOff)",
            "SetButtonCanBeSwitchedOff",
            fns::prop::SetButtonCanBeSwitchedOff as *const c_void,
        ),
        (
            "void SetButtonCanBeSwitchedOn(const string &in asName, bool abCanBeSwitchedOn)",
            "SetButtonCanBeSwitchedOn",
            fns::prop::SetButtonCanBeSwitchedOn as *const c_void,
        ),
        (
            "void SetEnemyActivationDistance(const string &in asName, float afX)",
            "SetEnemyActivationDistance",
            fns::enemy::SetEnemyActivationDistance as *const c_void,
        ),
        (
            "void SetEnemyBlind(const string &in asName, bool abX)",
            "SetEnemyBlind",
            fns::enemy::SetEnemyBlind as *const c_void,
        ),
        (
            "void SetEnemyDeaf(const string &in asName, bool abX)",
            "SetEnemyDeaf",
            fns::enemy::SetEnemyDeaf as *const c_void,
        ),
        (
            "void SetInfectionFauxMode(bool abFauxMode)",
            "AmfpStub_SetInfectionFauxMode",
            fns::stubs::AmfpStub_SetInfectionFauxMode as *const c_void,
        ),
        (
            "void SetLampCanBeLitByPlayer(const string &in asName, bool abLit)",
            "SetLampCanBeLitByPlayer",
            fns::prop::SetLampCanBeLitByPlayer as *const c_void,
        ),
        (
            "void SetPlayerUsesDragFootsteps(bool abX)",
            "SetPlayerUsesDragFootsteps",
            fns::player::SetPlayerUsesDragFootsteps as *const c_void,
        ),
        (
            "void SetPropAnimationPosition(const string &in asProp, float afPos)",
            "SetPropAnimationPosition",
            fns::prop::SetPropAnimationPosition as *const c_void,
        ),
        (
            "void SetPropAnimationSpeed(const string &in asProp, float afSpeed)",
            "SetPropAnimationSpeed",
            fns::prop::SetPropAnimationSpeed as *const c_void,
        ),
        (
            "void SetSwingDoorOpenAmount(const string &in asName, float afOpenAmount, float afDuration, bool abOpenTowardsMaxAngle)",
            "SetSwingDoorOpenAmount",
            fns::prop::SetSwingDoorOpenAmount as *const c_void,
        ),
        (
            "void StopAnimationAndContinue(const string &in asEnemyName, float afTimeOut)",
            "StopAnimationAndContinue",
            fns::enemy::StopAnimationAndContinue as *const c_void,
        ),
        (
            "void StopPropAnimation(const string &in asProp)",
            "StopPropAnimation",
            fns::prop::StopPropAnimation as *const c_void,
        ),
        (
            "void CreateEntityAtArea(const string &in asEntityName, const string &in asEntityFile, const string &in asAreaName, bool abFullGameSave, float afPosX, float afPosY, float afPosZ, float afRotX, float afRotY, float afRotZ)",
            "CreateEntityAtAreaExt",
            fns::entity::CreateEntityAtAreaExt as *const c_void,
        ),
        (
            "void AddEffectVoice4(const string &in asVoiceFile, const string &in asEffectFile, const string &in asTextCat, const string &in asTextEntry, float afTextDelay, const string &in asText2Entry, float afText2Delay, const string &in asText3Entry, float afText3Delay, const string &in asText4Entry, float afText4Delay, bool abUsePostion, const string &in asPosEnitity, float afMinDistance, float afMaxDistance)",
            "AddEffectVoice4",
            fns::fx::AddEffectVoice4 as *const c_void,
        ),
        (
            "void AddEffectVoice(const string &in asVoiceFile, const string &in asEffectFile, const string &in asTextCat, const string &in asTextEntry, bool abUsePostion,  const string &in asPosEnitity, float afMinDistance, float afMaxDistance, int alPriority, bool abStopInterrupted)",
            "AddEffectVoiceExt",
            fns::fx::AddEffectVoiceExt as *const c_void,
        ),
        (
            "void ChangeMap(const string &in asMapName, const string &in asStartPos, const string &in asStartSound, const string &in asEndSound)",
            "ChangeMap",
            fns::misc::ChangeMap as *const c_void,
        ),
        (
            "void ClearSavedMaps()",
            "ClearSavedMaps",
            fns::misc::ClearSavedMaps as *const c_void,
        ),
        (
            "void CreateDataCache()",
            "CreateDataCache",
            fns::misc::CreateDataCache as *const c_void,
        ),
        (
            "void DestroyDataCache()",
            "DestroyDataCache",
            fns::misc::DestroyDataCache as *const c_void,
        ),
        (
            "void SetMapDisplayNameEntry(const string &in asNameEntry)",
            "SetMapDisplayNameEntry",
            fns::misc::SetMapDisplayNameEntry as *const c_void,
        ),
        (
            "void SetSkyBoxActive(bool abActive)",
            "SetSkyBoxActive",
            fns::world::SetSkyBoxActive as *const c_void,
        ),
        (
            "void SetSkyBoxTexture(const string &in asTexture)",
            "SetSkyBoxTexture",
            fns::world::SetSkyBoxTexture as *const c_void,
        ),
        (
            "void SetSkyBoxColor(float afR, float afG, float afB, float afA)",
            "SetSkyBoxColor",
            fns::world::SetSkyBoxColor as *const c_void,
        ),
        (
            "void UnlockAchievement(const string &in asName)",
            "UnlockAchievement",
            fns::misc::UnlockAchievement as *const c_void,
        ),
        (
            "void SetFogActive(bool abActive)",
            "SetFogActive",
            fns::world::SetFogActive as *const c_void,
        ),
        (
            "void SetFogColor(float afR, float afG, float afB, float afA)",
            "SetFogColor",
            fns::world::SetFogColor as *const c_void,
        ),
        (
            "void SetFogProperties(float afStart, float afEnd, float afFalloffExp, bool abCulling)",
            "SetFogProperties",
            fns::world::SetFogProperties as *const c_void,
        ),
        (
            "void SetupLoadScreen(const string &in asTextCat, const string &in asTextEntry, int alRandomNum, const string &in asImageFile)",
            "SetupLoadScreen",
            fns::misc::SetupLoadScreen as *const c_void,
        ),
        (
            "void FadeIn(float afTime)",
            "FadeIn",
            fns::fx::FadeIn as *const c_void,
        ),
        (
            "void FadeOut(float afTime)",
            "FadeOut",
            fns::fx::FadeOut as *const c_void,
        ),
        (
            "void FadeImageTrailTo(float afAmount, float afSpeed)",
            "FadeImageTrailTo",
            fns::fx::FadeImageTrailTo as *const c_void,
        ),
        (
            "void FadeSepiaColorTo(float afAmount, float afSpeed)",
            "FadeSepiaColorTo",
            fns::fx::FadeSepiaColorTo as *const c_void,
        ),
        (
            "void FadeRadialBlurTo(float afSize, float afSpeed)",
            "FadeRadialBlurTo",
            fns::fx::FadeRadialBlurTo as *const c_void,
        ),
        (
            "void SetRadialBlurStartDist(float afStartDist)",
            "SetRadialBlurStartDist",
            fns::fx::SetRadialBlurStartDist as *const c_void,
        ),
        (
            "void StartEffectFlash(float afFadeIn, float afWhite, float afFadeOut)",
            "StartEffectFlash",
            fns::fx::StartEffectFlash as *const c_void,
        ),
        (
            "void StartEffectEmotionFlash(const string &in asTextCat, const string &in asTextEntry, const string &in asSound)",
            "StartEffectEmotionFlash",
            fns::fx::StartEffectEmotionFlash as *const c_void,
        ),
        (
            "void SetInDarknessEffectsActive(bool abX)",
            "SetInDarknessEffectsActive",
            fns::player::SetInDarknessEffectsActive as *const c_void,
        ),
        (
            "void AddEffectVoice(const string &in asVoiceFile, const string &in asEffectFile, const string &in asTextCat, const string &in asTextEntry, bool abUsePostion,  const string &in asPosEnitity, float afMinDistance, float afMaxDistance)",
            "AddEffectVoice",
            fns::fx::AddEffectVoice as *const c_void,
        ),
        (
            "void StopAllEffectVoices(float afFadeOutTime)",
            "StopAllEffectVoices",
            fns::fx::StopAllEffectVoices as *const c_void,
        ),
        (
            "bool GetEffectVoiceActive()",
            "GetEffectVoiceActive",
            fns::fx::GetEffectVoiceActive as *const c_void,
        ),
        (
            "void SetEffectVoiceOverCallback(const string &in asFunc)",
            "SetEffectVoiceOverCallback",
            fns::fx::SetEffectVoiceOverCallback as *const c_void,
        ),
        (
            "void StartScreenShake(float afAmount, float afTime, float afFadeInTime,float afFadeOutTime)",
            "StartScreenShake",
            fns::fx::StartScreenShake as *const c_void,
        ),
        (
            "bool GetFlashbackIsActive()",
            "GetFlashbackIsActive",
            fns::player::GetFlashbackIsActive as *const c_void,
        ),
        (
            "void SetInsanitySetEnabled(const string &in asSet, bool abX)",
            "SetInsanitySetEnabled",
            fns::fx::SetInsanitySetEnabled as *const c_void,
        ),
        (
            "void StartRandomInsanityEvent()",
            "StartRandomInsanityEvent",
            fns::fx::StartRandomInsanityEvent as *const c_void,
        ),
        (
            "void StartInsanityEvent(const string &in asEventName)",
            "StartInsanityEvent",
            fns::fx::StartInsanityEvent as *const c_void,
        ),
        (
            "void StopCurrentInsanityEvent()",
            "StopCurrentInsanityEvent",
            fns::fx::StopCurrentInsanityEvent as *const c_void,
        ),
        (
            "void InsanityEventIsActive()",
            "InsanityEventIsActive",
            fns::fx::InsanityEventIsActive as *const c_void,
        ),
        (
            "void StartPlayerSpawnPS(const string &in asSPSFile)",
            "StartPlayerSpawnPS",
            fns::player::StartPlayerSpawnPS as *const c_void,
        ),
        (
            "void StopPlayerSpawnPS()",
            "StartPlayerSpawnPS",
            fns::player::StartPlayerSpawnPS as *const c_void,
        ),
        (
            "void PlayGuiSound(const string &in asSoundFile, float afVolume)",
            "PlayGuiSound",
            fns::misc::PlayGuiSound as *const c_void,
        ),
        (
            "void SetPlayerActive(bool abActive)",
            "SetPlayerActive",
            fns::player::SetPlayerActive as *const c_void,
        ),
        (
            "void ChangePlayerStateToNormal()",
            "ChangePlayerStateToNormal",
            fns::player::ChangePlayerStateToNormal as *const c_void,
        ),
        (
            "void SetPlayerCrouching(bool abCrouch)",
            "SetPlayerCrouching",
            fns::player::SetPlayerCrouching as *const c_void,
        ),
        (
            "void AddPlayerBodyForce(float afX, float afY, float afZ, bool abUseLocalCoords)",
            "AddPlayerBodyForce",
            fns::player::AddPlayerBodyForce as *const c_void,
        ),
        (
            "void ShowPlayerCrossHairIcons(bool abX)",
            "ShowPlayerCrossHairIcons",
            fns::player::ShowPlayerCrossHairIcons as *const c_void,
        ),
        (
            "void SetPlayerPos(float afX, float afY, float afZ)",
            "SetPlayerPos",
            fns::player::SetPlayerPos as *const c_void,
        ),
        (
            "float GetPlayerPosX()",
            "GetPlayerPosX",
            fns::player::GetPlayerPosX as *const c_void,
        ),
        (
            "float GetPlayerPosY()",
            "GetPlayerPosY",
            fns::player::GetPlayerPosY as *const c_void,
        ),
        (
            "float GetPlayerPosZ()",
            "GetPlayerPosZ",
            fns::player::GetPlayerPosZ as *const c_void,
        ),
        (
            "void SetPlayerSanity(float afSanity)",
            "SetPlayerSanity",
            fns::player::SetPlayerSanity as *const c_void,
        ),
        (
            "void AddPlayerSanity(float afSanity)",
            "AddPlayerSanity",
            fns::player::AddPlayerSanity as *const c_void,
        ),
        (
            "float GetPlayerSanity()",
            "GetPlayerSanity",
            fns::player::GetPlayerSanity as *const c_void,
        ),
        (
            "void SetPlayerHealth(float afHealth)",
            "SetPlayerHealth",
            fns::player::SetPlayerHealth as *const c_void,
        ),
        (
            "void AddPlayerHealth(float afHealth)",
            "AddPlayerHealth",
            fns::player::AddPlayerHealth as *const c_void,
        ),
        (
            "float GetPlayerHealth()",
            "GetPlayerHealth",
            fns::player::GetPlayerHealth as *const c_void,
        ),
        (
            "void SetPlayerLampOil(float afOil)",
            "SetPlayerLampOil",
            fns::player::SetPlayerLampOil as *const c_void,
        ),
        (
            "void AddPlayerLampOil(float afOil)",
            "AddPlayerLampOil",
            fns::player::AddPlayerLampOil as *const c_void,
        ),
        (
            "float GetPlayerLampOil()",
            "GetPlayerLampOil",
            fns::player::GetPlayerLampOil as *const c_void,
        ),
        (
            "float GetPlayerSpeed()",
            "GetPlayerSpeed",
            fns::player::GetPlayerSpeed as *const c_void,
        ),
        (
            "float GetPlayerYSpeed()",
            "GetPlayerYSpeed",
            fns::player::GetPlayerYSpeed as *const c_void,
        ),
        (
            "void MovePlayerForward(float afAmount)",
            "MovePlayerForward",
            fns::player::MovePlayerForward as *const c_void,
        ),
        (
            "void SetPlayerPermaDeathSound(const string &in asSound)",
            "SetPlayerPermaDeathSound",
            fns::player::SetPlayerPermaDeathSound as *const c_void,
        ),
        (
            "void SetSanityDrainDisabled(bool abX)",
            "SetSanityDrainDisabled",
            fns::player::SetSanityDrainDisabled as *const c_void,
        ),
        (
            "void GiveSanityBoost()",
            "GiveSanityBoost",
            fns::player::GiveSanityBoost as *const c_void,
        ),
        (
            "void GiveSanityBoostSmall()",
            "GiveSanityBoostSmall",
            fns::player::GiveSanityBoostSmall as *const c_void,
        ),
        (
            "void GiveSanityDamage(float afAmount, bool abUseEffect)",
            "GiveSanityDamage",
            fns::player::GiveSanityDamage as *const c_void,
        ),
        (
            "void GivePlayerDamage(float afAmount, const string &in asType, bool abSpinHead, bool abLethal)",
            "GivePlayerDamage",
            fns::player::GivePlayerDamage as *const c_void,
        ),
        (
            "void FadePlayerFOVMulTo(float afX, float afSpeed)",
            "FadePlayerFOVMulTo",
            fns::player::FadePlayerFOVMulTo as *const c_void,
        ),
        (
            "void FadePlayerAspectMulTo(float afX, float afSpeed)",
            "FadePlayerAspectMulTo",
            fns::player::FadePlayerAspectMulTo as *const c_void,
        ),
        (
            "void FadePlayerRollTo(float afX, float afSpeedMul, float afMaxSpeed)",
            "FadePlayerRollTo",
            fns::player::FadePlayerRollTo as *const c_void,
        ),
        (
            "void MovePlayerHeadPos(float afX, float afY, float afZ, float afSpeed, float afSlowDownDist)",
            "MovePlayerHeadPos",
            fns::player::MovePlayerHeadPos as *const c_void,
        ),
        (
            "void StartPlayerLookAt(const string &in asEntityName, float afSpeedMul, float afMaxSpeed,const string &in asAtTargetCallback)",
            "StartPlayerLookAt",
            fns::player::StartPlayerLookAt as *const c_void,
        ),
        (
            "void StopPlayerLookAt()",
            "StopPlayerLookAt",
            fns::player::StopPlayerLookAt as *const c_void,
        ),
        (
            "void SetPlayerMoveSpeedMul(float afMul)",
            "SetPlayerMoveSpeedMul",
            fns::player::SetPlayerMoveSpeedMul as *const c_void,
        ),
        (
            "void SetPlayerRunSpeedMul(float afMul)",
            "SetPlayerRunSpeedMul",
            fns::player::SetPlayerRunSpeedMul as *const c_void,
        ),
        (
            "void SetPlayerLookSpeedMul(float afMul)",
            "SetPlayerLookSpeedMul",
            fns::player::SetPlayerLookSpeedMul as *const c_void,
        ),
        (
            "void SetPlayerJumpForceMul(float afMul)",
            "SetPlayerJumpForceMul",
            fns::player::SetPlayerJumpForceMul as *const c_void,
        ),
        (
            "void SetPlayerJumpDisabled(bool abX)",
            "SetPlayerJumpDisabled",
            fns::player::SetPlayerJumpDisabled as *const c_void,
        ),
        (
            "void SetPlayerCrouchDisabled(bool abX)",
            "SetPlayerCrouchDisabled",
            fns::player::SetPlayerCrouchDisabled as *const c_void,
        ),
        (
            "void SetPlayerFallDamageDisabled(bool abX)",
            "SetPlayerFallDamageDisabled",
            fns::player::SetPlayerFallDamageDisabled as *const c_void,
        ),
        (
            "void TeleportPlayer(const string &in asStartPosName)",
            "TeleportPlayer",
            fns::player::TeleportPlayer as *const c_void,
        ),
        (
            "void SetLanternActive(bool abX, bool abUseEffects)",
            "SetLanternActive",
            fns::player::SetLanternActive as *const c_void,
        ),
        (
            "bool GetLanternActive()",
            "GetLanternActive",
            fns::player::GetLanternActive as *const c_void,
        ),
        (
            "void SetLanternDisabled(bool abX)",
            "SetLanternDisabled",
            fns::player::SetLanternDisabled as *const c_void,
        ),
        (
            "void SetLanternLitCallback(const string &in asCallback)",
            "SetLanternLitCallback",
            fns::player::SetLanternLitCallback as *const c_void,
        ),
        (
            "void SetMessage(const string &in asTextCategory, const string &in asTextEntry, float afTime)",
            "SetMessage",
            fns::fx::SetMessage as *const c_void,
        ),
        (
            "void SetDeathHint(const string &in asTextCategory, const string &in asTextEntry)",
            "SetDeathHint",
            fns::player::SetDeathHint as *const c_void,
        ),
        (
            "void DisableDeathStartSound()",
            "DisableDeathStartSound",
            fns::player::DisableDeathStartSound as *const c_void,
        ),
        (
            "void AddNote(const string &in asNameAndTextEntry, const string &in asImage)",
            "AddNote",
            fns::fx::AddNote as *const c_void,
        ),
        (
            "void AddDiary(const string &in asNameAndTextEntry, const string &in asImage)",
            "AddDiary",
            fns::fx::AddDiary as *const c_void,
        ),
        (
            "void ReturnOpenJournal(bool abOpenJournal)",
            "ReturnOpenJournal",
            fns::fx::ReturnOpenJournal as *const c_void,
        ),
        (
            "void AddQuest(const string &in asName, const string &in asNameAndTextEntry)",
            "AddQuest",
            fns::fx::AddQuest as *const c_void,
        ),
        (
            "void CompleteQuest(const string &in asName, const string &in asNameAndTextEntry)",
            "CompleteQuest",
            fns::fx::CompleteQuest as *const c_void,
        ),
        (
            "bool QuestIsCompleted(const string &in asName)",
            "QuestIsCompleted",
            fns::fx::QuestIsCompleted as *const c_void,
        ),
        (
            "bool QuestIsAdded(const string &in asName)",
            "QuestIsAdded",
            fns::fx::QuestIsAdded as *const c_void,
        ),
        (
            "void SetNumberOfQuestsInMap(int alNumberOfQuests)",
            "SetNumberOfQuestsInMap",
            fns::fx::SetNumberOfQuestsInMap as *const c_void,
        ),
        (
            "void GiveHint(const string &in asName, const string &in asMessageCat, const string &in asMessageEntry, float afTimeShown)",
            "GiveHint",
            fns::fx::GiveHint as *const c_void,
        ),
        (
            "void RemoveHint(const string &in asName)",
            "RemoveHint",
            fns::fx::RemoveHint as *const c_void,
        ),
        (
            "void BlockHint(const string &in asName)",
            "BlockHint",
            fns::fx::BlockHint as *const c_void,
        ),
        (
            "void UnBlockHint(const string &in asName)",
            "UnBlockHint",
            fns::fx::UnBlockHint as *const c_void,
        ),
        (
            "void ExitInventory()",
            "ExitInventory",
            fns::fx::ExitInventory as *const c_void,
        ),
        (
            "void SetInventoryDisabled(bool abX)",
            "SetInventoryDisabled",
            fns::fx::SetInventoryDisabled as *const c_void,
        ),
        (
            "void SetInventoryMessage(const string &in asTextCategory, const string &in asTextEntry, float afTime)",
            "SetInventoryMessage",
            fns::fx::SetInventoryMessage as *const c_void,
        ),
        (
            "void GiveItem(const string &in asName, const string &in asType, const string &in asSubTypeName, const string &in asImageName, float afAmount)",
            "GiveItem",
            fns::fx::GiveItem as *const c_void,
        ),
        (
            "void GiveItemFromFile(const string &in asName, const string &in asFileName)",
            "GiveItemFromFile",
            fns::fx::GiveItemFromFile as *const c_void,
        ),
        (
            "void RemoveItem(const string &in asName)",
            "RemoveItem",
            fns::fx::RemoveItem as *const c_void,
        ),
        (
            "bool HasItem(const string &in asName)",
            "HasItem",
            fns::fx::HasItem as *const c_void,
        ),
        (
            "void AddCombineCallback(const string &in asName, const string &in asItemA, const string &in asItemB, const string &in asFunction, bool abAutoDestroy)",
            "AddCombineCallback",
            fns::fx::AddCombineCallback as *const c_void,
        ),
        (
            "void RemoveCombineCallback(const string &in asName)",
            "RemoveCombineCallback",
            fns::fx::RemoveCombineCallback as *const c_void,
        ),
        (
            "void AddUseItemCallback(const string &in asName, const string &in asItem, const string &in asEntity, const string &in asFunction, bool abAutoDestroy)",
            "AddUseItemCallback",
            fns::fx::AddUseItemCallback as *const c_void,
        ),
        (
            "void RemoveUseItemCallback(const string &in asName)",
            "RemoveUseItemCallback",
            fns::fx::RemoveUseItemCallback as *const c_void,
        ),
        (
            "void PreloadParticleSystem(const string &in asPSFile)",
            "PreloadParticleSystem",
            fns::world::PreloadParticleSystem as *const c_void,
        ),
        (
            "void PreloadSound(const string &in asSoundFile)",
            "PreloadSound",
            fns::world::PreloadSound as *const c_void,
        ),
        (
            "void CreateParticleSystemAtEntity(const string &in asPSName, const string &in asPSFile, const string &in asEntity, bool abSavePS)",
            "CreateParticleSystemAtEntity",
            fns::world::CreateParticleSystemAtEntity as *const c_void,
        ),
        (
            "void CreateParticleSystemAtEntityExt(\tconst string &in asPSName, const string &in asPSFile, const string &in asEntity, bool abSavePS, float afR, float afG, float afB, float afA, bool abFadeAtDistance, float afFadeMinEnd, float afFadeMinStart, float afFadeMaxStart, float afFadeMaxEnd)",
            "CreateParticleSystemAtEntityExt",
            fns::world::CreateParticleSystemAtEntityExt as *const c_void,
        ),
        (
            "void DestroyParticleSystem(const string &in asName)",
            "DestroyParticleSystem",
            fns::world::DestroyParticleSystem as *const c_void,
        ),
        (
            "void PlaySoundAtEntity(const string &in asSoundName, const string &in asSoundFile, const string &in asEntity, float afFadeSpeed, bool abSaveSound)",
            "PlaySoundAtEntity",
            fns::world::PlaySoundAtEntity as *const c_void,
        ),
        (
            "void FadeInSound(const string &in asSoundName, float afFadeTime, bool abPlayStart)",
            "FadeInSound",
            fns::world::FadeInSound as *const c_void,
        ),
        (
            "void StopSound(const string &in asSoundName, float afFadeTime)",
            "StopSound",
            fns::world::StopSound as *const c_void,
        ),
        (
            "void PlayMusic(const string &in asMusicFile, bool abLoop, float afVolume, float afFadeTime, int alPrio, bool abResume)",
            "PlayMusic",
            fns::fx::PlayMusic as *const c_void,
        ),
        (
            "void StopMusic(float afFadeTime, int alPrio)",
            "StopMusic",
            fns::fx::StopMusic as *const c_void,
        ),
        (
            "void FadeGlobalSoundVolume(float afDestVolume, float afTime)",
            "FadeGlobalSoundVolume",
            fns::world::FadeGlobalSoundVolume as *const c_void,
        ),
        (
            "void FadeGlobalSoundSpeed(float afDestSpeed, float afTime)",
            "FadeGlobalSoundSpeed",
            fns::world::FadeGlobalSoundSpeed as *const c_void,
        ),
        (
            "void SetLightVisible(const string &in asLightName, bool abVisible)",
            "SetLightVisible",
            fns::world::SetLightVisible as *const c_void,
        ),
        (
            "void FadeLightTo(const string &in asLightName, float afR, float afG, float afB, float afA, float afRadius, float afTime)",
            "FadeLightTo",
            fns::world::FadeLightTo as *const c_void,
        ),
        (
            "void SetLightFlickerActive(const string &in asLightName, bool abActive)",
            "SetLightFlickerActive",
            fns::world::SetLightFlickerActive as *const c_void,
        ),
        (
            "void SetEntityActive(const string &in asName, bool abActive)",
            "SetEntityActive",
            fns::entity::SetEntityActive as *const c_void,
        ),
        (
            "void SetEntityVisible(const string &in asName, bool abVisible)",
            "SetEntityVisible",
            fns::entity::SetEntityVisible as *const c_void,
        ),
        (
            "bool GetEntityExists(const string &in asName)",
            "GetEntityExists",
            fns::entity::GetEntityExists as *const c_void,
        ),
        (
            "void SetEntityPos(const string &in asName, float afX, float afY, float afZ)",
            "SetEntityPos",
            fns::entity::SetEntityPos as *const c_void,
        ),
        (
            "float GetEntityPosX(const string &in asName)",
            "GetEntityPosX",
            fns::entity::GetEntityPosX as *const c_void,
        ),
        (
            "float GetEntityPosY(const string &in asName)",
            "GetEntityPosY",
            fns::entity::GetEntityPosY as *const c_void,
        ),
        (
            "float GetEntityPosZ(const string &in asName)",
            "GetEntityPosZ",
            fns::entity::GetEntityPosZ as *const c_void,
        ),
        (
            "void SetEntityCustomFocusCrossHair(const string &in asName, const string &in asCrossHair)",
            "SetEntityCustomFocusCrossHair",
            fns::entity::SetEntityCustomFocusCrossHair as *const c_void,
        ),
        (
            "void CreateEntityAtArea(const string &in asEntityName, const string &in asEntityFile, const string &in asAreaName, bool abFullGameSave)",
            "CreateEntityAtArea",
            fns::entity::CreateEntityAtArea as *const c_void,
        ),
        (
            "void ReplaceEntity(const string &in asName, const string &in asBodyName, const string &in asNewEntityName, const string &in asNewEntityFile, bool abFullGameSave)",
            "ReplaceEntity",
            fns::entity::ReplaceEntity as *const c_void,
        ),
        (
            "void PlaceEntityAtEntity(const string &in asName, const string &in asTargetEntity, const string &in asTargetBodyName, bool abUseRotation)",
            "PlaceEntityAtEntity",
            fns::entity::PlaceEntityAtEntity as *const c_void,
        ),
        (
            "void SetEntityPlayerLookAtCallback(const string &in asName, const string &in asCallback, bool abRemoveWhenLookedAt)",
            "SetEntityPlayerLookAtCallback",
            fns::entity::SetEntityPlayerLookAtCallback as *const c_void,
        ),
        (
            "void SetEntityPlayerInteractCallback(const string &in asName, const string &in asCallback, bool abRemoveOnInteraction)",
            "SetEntityPlayerInteractCallback",
            fns::entity::SetEntityPlayerInteractCallback as *const c_void,
        ),
        (
            "void SetEntityCallbackFunc(const string &in asName, const string &in asCallback)",
            "SetEntityCallbackFunc",
            fns::entity::SetEntityCallbackFunc as *const c_void,
        ),
        (
            "void SetEntityConnectionStateChangeCallback(const string &in asName, const string &in asCallback)",
            "SetEntityConnectionStateChangeCallback",
            fns::entity::SetEntityConnectionStateChangeCallback as *const c_void,
        ),
        (
            "void SetEntityInteractionDisabled(const string &in asName, bool abDisabled)",
            "SetEntityInteractionDisabled",
            fns::entity::SetEntityInteractionDisabled as *const c_void,
        ),
        (
            "bool GetEntitiesCollide(const string &in asEntityA, const string &in asEntityB)",
            "GetEntitiesCollide",
            fns::entity::GetEntitiesCollide as *const c_void,
        ),
        (
            "void SetPropEffectActive(const string &in asName, bool abActive, bool abFadeAndPlaySounds)",
            "SetPropEffectActive",
            fns::prop::SetPropEffectActive as *const c_void,
        ),
        (
            "void SetPropActiveAndFade(const string &in asName, bool abActive, float afFadeTime)",
            "SetPropActiveAndFade",
            fns::prop::SetPropActiveAndFade as *const c_void,
        ),
        (
            "void SetPropStaticPhysics(const string &in asName, bool abX)",
            "SetPropStaticPhysics",
            fns::prop::SetPropStaticPhysics as *const c_void,
        ),
        (
            "bool GetPropIsInteractedWith(const string &in asName)",
            "GetPropIsInteractedWith",
            fns::prop::GetPropIsInteractedWith as *const c_void,
        ),
        (
            "void RotatePropToSpeed(const string &in asName, float afAcc, float afGoalSpeed, float afAxisX, float afAxisY, float afAxisZ, bool abResetSpeed, const string &in asOffsetArea)",
            "RotatePropToSpeed",
            fns::prop::RotatePropToSpeed as *const c_void,
        ),
        (
            "void StopPropMovement(const string &in asName)",
            "StopPropMovement",
            fns::prop::StopPropMovement as *const c_void,
        ),
        (
            "void AddAttachedPropToProp(const string &in asPropName, const string &in asAttachName, const string &in asAttachFile, float fPosX, float fPosY, float fPosZ, float fRotX, float fRotY, float fRot)",
            "AddAttachedPropToProp",
            fns::prop::AddAttachedPropToProp as *const c_void,
        ),
        (
            "void AttachPropToProp(const string &in asPropName, const string &in asAttachName, const string &in asAttachFile, float fPosX, float fPosY, float fPosZ, float fRotX, float fRotY, float fRot)",
            "AttachPropToProp",
            fns::prop::AttachPropToProp as *const c_void,
        ),
        (
            "void RemoveAttachedPropFromProp(const string &in asPropName, const string &in asAttachName)",
            "RemoveAttachedPropFromProp",
            fns::prop::RemoveAttachedPropFromProp as *const c_void,
        ),
        (
            "void SetLampLit(const string &in asName, bool abLit, bool abEffects)",
            "SetLampLit",
            fns::prop::SetLampLit as *const c_void,
        ),
        (
            "void SetSwingDoorLocked(const string &in asName, bool abLocked, bool abEffects)",
            "SetSwingDoorLocked",
            fns::prop::SetSwingDoorLocked as *const c_void,
        ),
        (
            "void SetSwingDoorClosed(const string &in asName, bool abClosed, bool abEffects)",
            "SetSwingDoorClosed",
            fns::prop::SetSwingDoorClosed as *const c_void,
        ),
        (
            "void SetSwingDoorDisableAutoClose(const string &in asName, bool abDisableAutoClose)",
            "SetSwingDoorDisableAutoClose",
            fns::prop::SetSwingDoorDisableAutoClose as *const c_void,
        ),
        (
            "void SetLevelDoorLocked(const string &in asName, bool abLocked)",
            "SetLevelDoorLocked",
            fns::prop::SetLevelDoorLocked as *const c_void,
        ),
        (
            "void SetLevelDoorLockedSound(const string &in asName, const string &in asSound)",
            "SetLevelDoorLockedSound",
            fns::prop::SetLevelDoorLockedSound as *const c_void,
        ),
        (
            "void SetLevelDoorLockedText(const string &in asName, const string &in asTextCat, const string &in asTextEntry)",
            "SetLevelDoorLockedText",
            fns::prop::SetLevelDoorLockedText as *const c_void,
        ),
        (
            "bool GetSwingDoorLocked(const string &in asName)",
            "GetSwingDoorLocked",
            fns::prop::GetSwingDoorLocked as *const c_void,
        ),
        (
            "bool GetSwingDoorClosed(const string &in asName)",
            "GetSwingDoorClosed",
            fns::prop::GetSwingDoorClosed as *const c_void,
        ),
        (
            "int GetSwingDoorState(const string &in asName)",
            "GetSwingDoorState",
            fns::prop::GetSwingDoorState as *const c_void,
        ),
        (
            "void SetPropObjectStuckState(const string &in asName, int alState)",
            "SetPropObjectStuckState",
            fns::prop::SetPropObjectStuckState as *const c_void,
        ),
        (
            "void SetWheelAngle(const string &in asName, float afAngle, bool abAutoMove)",
            "SetWheelAngle",
            fns::prop::SetWheelAngle as *const c_void,
        ),
        (
            "void SetWheelStuckState(const string &in asName, int alState, bool abEffects)",
            "SetWheelStuckState",
            fns::prop::SetWheelStuckState as *const c_void,
        ),
        (
            "void SetLeverStuckState(const string &in asName, int alState, bool abEffects)",
            "SetLeverStuckState",
            fns::prop::SetLeverStuckState as *const c_void,
        ),
        (
            "void SetWheelInteractionDisablesStuck(const string &in asName, bool abX)",
            "SetWheelInteractionDisablesStuck",
            fns::prop::SetWheelInteractionDisablesStuck as *const c_void,
        ),
        (
            "void SetLeverInteractionDisablesStuck(const string &in asName, bool abX)",
            "SetLeverInteractionDisablesStuck",
            fns::prop::SetLeverInteractionDisablesStuck as *const c_void,
        ),
        (
            "int GetLeverState(const string &in asName)",
            "GetLeverState",
            fns::prop::GetLeverState as *const c_void,
        ),
        (
            "void SetMultiSliderStuckState(const string &in asName, int alStuckState, bool abEffects)",
            "SetMultiSliderStuckState",
            fns::prop::SetMultiSliderStuckState as *const c_void,
        ),
        (
            "void SetMultiSliderCallback(const string &in asName, const string &in asCallback)",
            "SetMultiSliderCallback",
            fns::prop::SetMultiSliderCallback as *const c_void,
        ),
        (
            "void SetButtonSwitchedOn(const string &in asName, bool abSwitchedOn, bool abEffects)",
            "SetButtonSwitchedOn",
            fns::prop::SetButtonSwitchedOn as *const c_void,
        ),
        (
            "void SetAllowStickyAreaAttachment(bool abX)",
            "SetAllowStickyAreaAttachment",
            fns::prop::SetAllowStickyAreaAttachment as *const c_void,
        ),
        (
            "void AttachPropToStickyArea(const string &in asAreaName, const string &in asProp)",
            "AttachPropToStickyArea",
            fns::prop::AttachPropToStickyArea as *const c_void,
        ),
        (
            "void AttachBodyToStickyArea(const string &in asAreaName, const string &in asBody)",
            "AttachBodyToStickyArea",
            fns::prop::AttachBodyToStickyArea as *const c_void,
        ),
        (
            "void DetachFromStickyArea(const string &in asAreaName)",
            "DetachFromStickyArea",
            fns::prop::DetachFromStickyArea as *const c_void,
        ),
        (
            "void SetNPCAwake(const string &in asName, bool abAwake, bool abEffects)",
            "SetNPCAwake",
            fns::prop::SetNPCAwake as *const c_void,
        ),
        (
            "void SetNPCFollowPlayer(const string &in asName, bool abX)",
            "SetNPCFollowPlayer",
            fns::prop::SetNPCFollowPlayer as *const c_void,
        ),
        (
            "void SetEnemyDisabled(const string &in asName, bool abDisabled)",
            "SetEnemyDisabled",
            fns::enemy::SetEnemyDisabled as *const c_void,
        ),
        (
            "void SetEnemyIsHallucination(const string &in asName, bool abX)",
            "SetEnemyIsHallucination",
            fns::enemy::SetEnemyIsHallucination as *const c_void,
        ),
        (
            "void FadeEnemyToSmoke(const string &in asName, bool abPlaySound)",
            "FadeEnemyToSmoke",
            fns::enemy::FadeEnemyToSmoke as *const c_void,
        ),
        (
            "void SetEnemyDisableTriggers(const string &in asName, bool abX)",
            "SetEnemyDisableTriggers",
            fns::enemy::SetEnemyDisableTriggers as *const c_void,
        ),
        (
            "void ShowEnemyPlayerPosition(const string &in asName)",
            "ShowEnemyPlayerPosition",
            fns::enemy::ShowEnemyPlayerPosition as *const c_void,
        ),
        (
            "void AlertEnemyOfPlayerPresence(const string &in asName)",
            "AlertEnemyOfPlayerPresence",
            fns::enemy::AlertEnemyOfPlayerPresence as *const c_void,
        ),
        (
            "void AddEnemyPatrolNode(const string &in asEnemyName, const string &in asNodeName, float afWaitTime, const string &in asAnimation)",
            "AddEnemyPatrolNode",
            fns::enemy::AddEnemyPatrolNode as *const c_void,
        ),
        (
            "void AddEnemyPatrolNode(const string &in asEnemyName, const string &in asNodeName, float afWaitTime, const string &in asAnimation, bool abAmfpExtraArg)",
            "AddEnemyPatrolNodeAmfp",
            fns::enemy::AddEnemyPatrolNodeAmfp as *const c_void,
        ),
        (
            "void ClearEnemyPatrolNodes(const string &in asEnemyName)",
            "ClearEnemyPatrolNodes",
            fns::enemy::ClearEnemyPatrolNodes as *const c_void,
        ),
        (
            "void SetEnemySanityDecreaseActive(const string &in asName, bool abX)",
            "SetEnemySanityDecreaseActive",
            fns::enemy::SetEnemySanityDecreaseActive as *const c_void,
        ),
        (
            "void TeleportEnemyToNode(const string &in asEnemyName, const string &in asNodeName, bool abChangeY)",
            "TeleportEnemyToNode",
            fns::enemy::TeleportEnemyToNode as *const c_void,
        ),
        (
            "void TeleportEnemyToEntity(const string &in asEnemyName, const string &in asTargetEntity, const string &in asTargetBody, bool abChangeY)",
            "TeleportEnemyToEntity",
            fns::enemy::TeleportEnemyToEntity as *const c_void,
        ),
        (
            "void ChangeManPigPose(const string &in asName, const string &in asPoseType)",
            "ChangeManPigPose",
            fns::enemy::ChangeManPigPose as *const c_void,
        ),
        (
            "void SetTeslaPigFadeDisabled(const string &in asName, bool abX)",
            "SetTeslaPigFadeDisabled",
            fns::enemy::SetTeslaPigFadeDisabled as *const c_void,
        ),
        (
            "void SetTeslaPigSoundDisabled(const string &in asName, bool abX)",
            "SetTeslaPigSoundDisabled",
            fns::enemy::SetTeslaPigSoundDisabled as *const c_void,
        ),
        (
            "void SetTeslaPigEasyEscapeDisabled(const string &in asName, bool abX)",
            "SetTeslaPigEasyEscapeDisabled",
            fns::enemy::SetTeslaPigEasyEscapeDisabled as *const c_void,
        ),
        (
            "void ForceTeslaPigSighting(const string &in asName)",
            "ForceTeslaPigSighting",
            fns::enemy::ForceTeslaPigSighting as *const c_void,
        ),
        (
            "string& GetEnemyStateName(const string &in asName)",
            "GetEnemyStateName",
            fns::enemy::GetEnemyStateName as *const c_void,
        ),
        (
            "void SetPropHealth(const string &in asName, float afHealth)",
            "SetPropHealth",
            fns::prop::SetPropHealth as *const c_void,
        ),
        (
            "void AddPropHealth(const string &in asName, float afHealth)",
            "AddPropHealth",
            fns::prop::AddPropHealth as *const c_void,
        ),
        (
            "float GetPropHealth(const string &in asName)",
            "GetPropHealth",
            fns::prop::GetPropHealth as *const c_void,
        ),
        (
            "void ResetProp(const string &in asName)",
            "ResetProp",
            fns::prop::ResetProp as *const c_void,
        ),
        (
            "void PlayPropAnimation(const string &in asProp, const string &in asAnimation, float afFadeTime, bool abLoop, const string &in asCallback)",
            "PlayPropAnimation",
            fns::prop::PlayPropAnimation as *const c_void,
        ),
        (
            "void SetMoveObjectState(const string &in asName, float afState)",
            "SetMoveObjectState",
            fns::prop::SetMoveObjectState as *const c_void,
        ),
        (
            "void SetMoveObjectStateExt(const string &in asName, float afState, float afAcc, float afMaxSpeed, float afSlowdownDist, bool abResetSpeed)",
            "SetMoveObjectStateExt",
            fns::prop::SetMoveObjectStateExt as *const c_void,
        ),
        (
            "void AddPropForce(const string &in asName, float afX, float afY, float afZ, const string &in asCoordSystem)",
            "AddPropForce",
            fns::entity::AddPropForce as *const c_void,
        ),
        (
            "void AddPropImpulse(const string &in asName, float afX, float afY, float afZ, const string &in asCoordSystem)",
            "AddPropImpulse",
            fns::entity::AddPropImpulse as *const c_void,
        ),
        (
            "void AddBodyForce(const string &in asName, float afX, float afY, float afZ, const string &in asCoordSystem)",
            "AddBodyForce",
            fns::entity::AddBodyForce as *const c_void,
        ),
        (
            "void AddBodyImpulse(const string &in asName, float afX, float afY, float afZ, const string &in asCoordSystem)",
            "AddBodyImpulse",
            fns::entity::AddBodyImpulse as *const c_void,
        ),
        (
            "void BreakJoint(const string &in asName)",
            "BreakJoint",
            fns::entity::BreakJoint as *const c_void,
        ),
        (
            "void SetBodyMass(const string &in asName, float afMass)",
            "SetBodyMass",
            fns::entity::SetBodyMass as *const c_void,
        ),
        (
            "float GetBodyMass(const string &in asName)",
            "GetBodyMass",
            fns::entity::GetBodyMass as *const c_void,
        ),
        (
            "void AddEntityCollideCallback(const string &in asParentName, const string &in asChildName, const string &in asFunction, bool abDeleteOnCollide, int alStates)",
            "AddEntityCollideCallback",
            fns::entity::AddEntityCollideCallback as *const c_void,
        ),
        (
            "void RemoveEntityCollideCallback(const string &in asParentName, const string &in asChildName)",
            "RemoveEntityCollideCallback",
            fns::entity::RemoveEntityCollideCallback as *const c_void,
        ),
        (
            "void InteractConnectPropWithRope(const string &in asName, const string &in asLeverName, const string &in asPropName, bool abInteractOnly, float afSpeedMul,float afMinSpeed, float afMaxSpeed, bool abInvert, int alStatesUsed)",
            "InteractConnectPropWithRope",
            fns::entity::InteractConnectPropWithRope as *const c_void,
        ),
        (
            "void InteractConnectPropWithMoveObject(const string &in asName, const string &in asPropName, const string &in asMoveObjectName, bool abInteractOnly,bool abInvert, int alStatesUsed)",
            "InteractConnectPropWithMoveObject",
            fns::entity::InteractConnectPropWithMoveObject as *const c_void,
        ),
        (
            "void ConnectEntities(const string &in asName, const string &in asMainEntity, const string &in asConnectEntity, bool abInvertStateSent, int alStatesUsed, const string &in asCallbackFunc)",
            "ConnectEntities",
            fns::entity::ConnectEntities as *const c_void,
        ),
        (
            "float MathSin(float afX)",
            "ScriptSin",
            native::f_ScriptSin as *const c_void,
        ),
        (
            "float MathCos(float afX)",
            "ScriptCos",
            native::f_ScriptCos as *const c_void,
        ),
        (
            "float MathTan(float afX)",
            "ScriptTan",
            native::f_ScriptTan as *const c_void,
        ),
        (
            "float MathAsin(float afX)",
            "ScriptAsin",
            native::f_ScriptAsin as *const c_void,
        ),
        (
            "float MathAcos(float afX)",
            "ScriptAcos",
            native::f_ScriptAcos as *const c_void,
        ),
        (
            "float MathAtan(float afX)",
            "ScriptAtan",
            native::f_ScriptAtan as *const c_void,
        ),
        (
            "float MathAtan2(float afX, float afY)",
            "ScriptAtan2",
            native::f_ScriptAtan2 as *const c_void,
        ),
        (
            "float MathSqrt(float afX)",
            "ScriptSqrt",
            native::f_ScriptSqrt as *const c_void,
        ),
        (
            "float MathPow(float afBase, float afExp)",
            "ScriptPow",
            native::f_ScriptPow as *const c_void,
        ),
        (
            "float MathMin(float afA, float afB)",
            "ScriptMin",
            native::f_ScriptMin as *const c_void,
        ),
        (
            "float MathMax(float afA, float afB)",
            "ScriptMax",
            native::f_ScriptMax as *const c_void,
        ),
        (
            "float MathClamp(float afX, float afMin, float afMax)",
            "ScriptClamp",
            native::f_ScriptClamp as *const c_void,
        ),
        (
            "float MathAbs(float afX)",
            "ScriptAbs",
            native::f_ScriptAbs as *const c_void,
        ),
        (
            "int StringToInt(const string &in asString)",
            "ScriptStringToInt",
            native::f_ScriptStringToInt as *const c_void,
        ),
        (
            "float StringToFloat(const string &in asString)",
            "ScriptStringToFloat",
            native::f_ScriptStringToFloat as *const c_void,
        ),
        (
            "bool StringToBool(const string &in asString)",
            "ScriptStringToBool",
            native::f_ScriptStringToBool as *const c_void,
        ),
    ]
}
