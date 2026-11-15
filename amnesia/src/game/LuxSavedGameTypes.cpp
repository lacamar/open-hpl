/*
 * Copyright © 2009-2020 Frictional Games
 * 
 * This file is part of Amnesia: The Dark Descent.
 * 
 * Amnesia: The Dark Descent is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version. 

 * Amnesia: The Dark Descent is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Amnesia: The Dark Descent.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "LuxSavedGameTypes.h"
#include "RustLuxSave.h"
#include <vector>

#include "LuxMap.h"
#include "LuxPlayer.h"
#include "LuxPlayerHelpers.h"
#include "LuxPlayerHands.h"
#include "LuxHandObject.h"
#include "LuxPlayerState.h"
#include "LuxInventory.h"
#include "LuxMapHandler.h"
#include "LuxMusicHandler.h"
#include "LuxEnemy.h"
#include "LuxJournal.h"
#include "LuxEffectHandler.h"
#include "LuxGlobalDataHandler.h"
#include "LuxHintHandler.h"
#include "LuxInsanityHandler.h"
#include "LuxLoadScreenHandler.h"
#include "LuxMoveState_Normal.h"


//////////////////////////////////////////////////////////////////////////
// LOAD SCREEN HANDLER
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

namespace
{

template<class E>
int Ga6Tramp(void *apCtx, unsigned alOp, int alA, int alB)
{
	E *e = static_cast<E*>(apCtx);
	return e->mpSelf->Ga6Op(e, alOp, alA, alB);
}

}

void cLuxLoadScreenHandler_SaveData::FromLoadScreenHandler(cLuxLoadScreenHandler *apData)
{
	msLoadTextCat = apData->GetLoadTextCat();
	msLoadTextEntry = apData->GetLoadTextEntry();
	msLoadTextImage = apData->GetLoadTextImage ();
	mlTextRandomNum = apData->GetLoadTextEntryRandomNum();
	mlPrevTextNumBitFlags = apData->GetPrevTextNumBitFlags();
}

//-----------------------------------------------------------------------

void cLuxLoadScreenHandler_SaveData::ToLoadScreenHandler(cLuxMap *apMap, cLuxLoadScreenHandler *apData)
{
	apData->SetupLoadText(msLoadTextCat, msLoadTextEntry, mlTextRandomNum, msLoadTextImage);
	apData->SetPrevTextNumBitFlags(mlPrevTextNumBitFlags);
}

//-----------------------------------------------------------------------

kBeginSerializeBase(cLuxLoadScreenHandler_SaveData)

kSerializeVar(msLoadTextCat, eSerializeType_String)
kSerializeVar(msLoadTextEntry, eSerializeType_String)
kSerializeVar(msLoadTextImage, eSerializeType_String)
kSerializeVar(mlTextRandomNum, eSerializeType_Int32)
kSerializeVar(mlPrevTextNumBitFlags, eSerializeType_Int32)

kEndSerialize()

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// HINT HANDLER
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxInsanityHandler_SaveData::FromInsanityHandler(cLuxInsanityHandler *apData)
{
	mfNewEventCount = apData->mfNewEventCount;

	//Events
    mvEvents.Resize(apData->mvEvents.size());
	for(size_t i=0; i<mvEvents.Size(); ++i)
	{
		mvEvents[i].mbUsed = apData->mvEvents[i]->IsUsed();
	}

	//Disabled Events
	for(tStringSetIt it = apData->m_setDisabledSets.begin(); it != apData->m_setDisabledSets.end(); ++it)
	{
		mvDisabledSets.Add(cLuxInsanityHandler_Set_SaveData(*it));
	}
}

namespace
{

struct InsanityEnv
{
	cLuxInsanityHandler_SaveData *mpSelf;
	cLuxInsanityHandler *mpData;
};

}

int cLuxInsanityHandler_SaveData::Ga6Op(void *apEnv, unsigned alOp, int alA, int alB)
{
	InsanityEnv &e = *static_cast<InsanityEnv*>(apEnv);
	cLuxInsanityHandler *apData = e.mpData;
	switch(alOp)
	{
	case eOhplGa6Op_InsToCount:
		apData->mfNewEventCount = mfNewEventCount;
		return 0;
	case eOhplGa6Op_InsSavedEventN:
		return (int)mvEvents.Size();
	case eOhplGa6Op_InsLiveEventN:
		return (int)apData->mvEvents.size();
	case eOhplGa6Op_InsEventTo:
		apData->mvEvents[alA]->SetUsed(mvEvents[alA].mbUsed);
		return 0;
	case eOhplGa6Op_InsClearSets:
		apData->m_setDisabledSets.clear();
		return 0;
	case eOhplGa6Op_InsSavedSetN:
		return (int)mvDisabledSets.Size();
	case eOhplGa6Op_InsInsertSet:
		apData->m_setDisabledSets.insert(mvDisabledSets[alA].msName);
		return 0;
	}
	return 0;
}

void cLuxInsanityHandler_SaveData::ToInsanityHandler(cLuxMap *apMap, cLuxInsanityHandler *apData)
{
	InsanityEnv env = { this, apData };
	ohpl_ga6_insanity_to(&env, Ga6Tramp<InsanityEnv>);
}

//-----------------------------------------------------------------------

kBeginSerializeBase(cLuxInsanityHandler_Event_SaveData)

kSerializeVar(mbUsed, eSerializeType_Bool)

kEndSerialize()

kBeginSerializeBase(cLuxInsanityHandler_Set_SaveData)

kSerializeVar(msName, eSerializeType_String)

kEndSerialize()

kBeginSerializeBase(cLuxInsanityHandler_SaveData)

kSerializeVar(mfNewEventCount, eSerializeType_Float32)
kSerializeClassContainer(mvEvents, cLuxInsanityHandler_Event_SaveData, eSerializeType_Class)
kSerializeClassContainer(mvDisabledSets, cLuxInsanityHandler_Set_SaveData, eSerializeType_Class)

kEndSerialize()

//////////////////////////////////////////////////////////////////////////
// HINT HANDLER
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------
void cLuxHintHandler_SaveData::FromHintHandler(cLuxHintHandler *apData)
{
	tStringVec vNames;
	for(int blocked=0; blocked<2; ++blocked)
	{
		vNames.clear();
		apData->GetNames(blocked != 0, vNames);
		for(size_t i=0; i<vNames.size(); ++i)
		{
			cLuxHintHandler_Hint_SaveData hint;
			hint.msName = vNames[i];
			(blocked ? mlstHintsBlocked : mlstHintsGiven).Add(hint);
		}
	}
}

//-----------------------------------------------------------------------

void cLuxHintHandler_SaveData::ToHintHandler(cLuxMap *apMap, cLuxHintHandler *apData)
{
	tStringVec vNames;
	cContainerListIterator<cLuxHintHandler_Hint_SaveData> it = mlstHintsGiven.GetIterator();
	while(it.HasNext()) vNames.push_back(it.Next().msName);
	apData->SetNames(false, vNames);

	vNames.clear();
	it = mlstHintsBlocked.GetIterator();
	while(it.HasNext()) vNames.push_back(it.Next().msName);
	apData->SetNames(true, vNames);
}

//-----------------------------------------------------------------------

kBeginSerializeBase(cLuxHintHandler_Hint_SaveData)

kSerializeVar(msName, eSerializeType_String)

kEndSerialize()

kBeginSerializeBase(cLuxHintHandler_SaveData)

kSerializeClassContainer(mlstHintsGiven, cLuxHintHandler_Hint_SaveData, eSerializeType_Class)
kSerializeClassContainer(mlstHintsBlocked, cLuxHintHandler_Hint_SaveData, eSerializeType_Class)

kEndSerialize()

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// GLOBAL DATA HANDLER
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxGlobalDataHandler_SaveData::FromGlobalDataHandler(cLuxGlobalDataHandler *apData)
{
	tLuxScriptVarMapIt it = apData->m_mapVars.begin();
	for(; it != apData->m_mapVars.end(); ++it)
	{
		mlstVars.Add(it->second);
	}
}

//-----------------------------------------------------------------------

void cLuxGlobalDataHandler_SaveData::ToGlobalDataHandler(cLuxMap *apMap, cLuxGlobalDataHandler *apData)
{
	apData->m_mapVars.clear();
	cContainerListIterator<cLuxScriptVar> it = mlstVars.GetIterator();
	while(it.HasNext())
	{
		cLuxScriptVar& scriptVar = it.Next();
		apData->m_mapVars.insert(tLuxScriptVarMap::value_type(scriptVar.msName, scriptVar));
	}
}

//-----------------------------------------------------------------------

kBeginSerializeBase(cLuxGlobalDataHandler_SaveData)

kSerializeClassContainer(mlstVars, cLuxScriptVar,  eSerializeType_Class)

kEndSerialize()

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// EFFECT HANDLER
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxEffectHandler_SoundMul_SaveData::FromEntry(cMultipleSettingsHandler::cGSEntry* apEntry)
{
	if(apEntry)
	{
		mbActive = true;
		mfVal = apEntry->GetVal();
		mfDest = apEntry->GetDest();
		mfSpeed = apEntry->GetSpeed();
		mTypes = apEntry->GetTypes();
		mbDestroyAtDest = apEntry->GetDestroyAtDest();
	}
	else
	{
		mbActive = false;
	}
}

void cLuxEffectHandler_SoundMul_SaveData::ToEntry(int alIdx, cMultipleSettingsHandler *apHandler)
{
	if(mbActive)
	{
		cMultipleSettingsHandler::cGSEntry* pEntry = apHandler->GetEntry(alIdx);
		pEntry->SetVal(mfVal);
		pEntry->SetDest(mfDest);
		pEntry->SetSpeed(mfSpeed);
		pEntry->SetTypes(mTypes);
		pEntry->SetDestroyAtDest(mbDestroyAtDest);
	}
	else
	{
		apHandler->DestroyEntry(alIdx);
	}
}

//-----------------------------------------------------------------------

namespace
{

struct EffectsEnv
{
	cLuxEffectHandler_SaveData *mpSelf;
	cLuxEffectHandler *mpEffects;
	cLuxMap *mpMap;
	cMultipleSettingsHandler::cGSEntry *mpEntry;
};

}

int cLuxEffectHandler_SaveData::Ga6Op(void *apEnv, unsigned alOp, int alA, int alB)
{
	EffectsEnv &e = *static_cast<EffectsEnv*>(apEnv);
	cLuxEffectHandler *apEffects = e.mpEffects;
	cLuxMap *apMap = e.mpMap;
	cSoundHandler *pSoundHandler = gpBase->mpEngine->GetSound()->GetSoundHandler();
	cContainerVec<cLuxEffectHandler_SoundMul_SaveData> &vMul = alA == eOhplGa6Kind_Volume ? mvGlobalSoundVolumeMul : mvGlobalSoundSpeedMul;
	cMultipleSettingsHandler *pMulHandler = alA == eOhplGa6Kind_Volume ? pSoundHandler->GetGlobalVolumeSettingsHandler() : pSoundHandler->GetGlobalSpeedSettingsHandler();
	switch(alOp)
	{
	case eOhplGa6Op_EffectFromBase:
		{
			uint64_t lH = apEffects->GetHandle();
			OhplA12aFxState st;
			ohpl_a12a_fx_get_state(lH, &st);

			mbFade_Active = st.fade.active;
			mfFade_GoalAlpha = st.fade.goal;
			mfFade_Alpha = st.fade.alpha;
			mfFade_FadeSpeed = st.fade.speed;

			mbImageTrail_Active = st.trail.active;
			mfImageTrail_Amount = st.trail.amount;
			mfImageTrail_AmountGoal = st.trail.goal;
			mfImageTrail_FadeSpeed = st.trail.speed;

			mbSepiaColor_Active = st.sepia.active;
			mfSepiaColor_Amount = st.sepia.amount;
			mfSepiaColor_AmountGoal = st.sepia.goal;
			mfSepiaColor_FadeSpeed = st.sepia.speed;

			mbRadialBlur_Active = st.radial.active;
			mfRadialBlur_Size = st.radial.amount;
			mfRadialBlur_BlurStartDist = st.radialStart;
			mfRadialBlur_SizeGoal = st.radial.goal;
			mfRadialBlur_FadeSpeed = st.radial.speed;

			mbVoiceActive = st.voiceActive != 0;
			msVoiceOverCallback.clear();
			ohpl_a12a_fx_voice_cb_get(lH, [](void *apCtx, const unsigned char *apP, size_t alN){ ((tString*)apCtx)->assign((const char*)apP, alN); }, &msVoiceOverCallback);
			mvVoiceData.Clear();
			for(size_t i=0; i<ohpl_a12a_fx_voice_count(lH); ++i)
			{
				OhplA12aVoiceSave v;
				if(ohpl_a12a_fx_voice_get(lH, i, &v) == 0) continue;
				cLuxVoiceData data;
				tWString *vTexts[7] = { &data.msText, &data.msText2, &data.msText3, &data.msText4, &data.msText5, &data.msText6, &data.msText7 };
				float *vDelays[7] = { &data.mfTextDelay, &data.mfText2Delay, &data.mfText3Delay, &data.mfText4Delay, &data.mfText5Delay, &data.mfText6Delay, &data.mfText7Delay };
				for(int t=0; t<7; ++t)
				{
					vTexts[t]->assign((const wchar_t*)v.texts[t].p, v.texts[t].n);
					*vDelays[t] = v.delays[t];
				}
				data.msVoiceFile.assign((const char*)v.voice.p, v.voice.n);
				data.msEffectFile.assign((const char*)v.effect.p, v.effect.n);
				data.mbUsePosition = v.usePos != 0;
				data.mvPosition = cVector3f(v.pos[0], v.pos[1], v.pos[2]);
				data.mfMinDistance = v.minDist;
				data.mfMaxDistance = v.maxDist;
				data.mlPriority = v.priority;
				data.mfCurrentTime = v.currentTime;
				data.mfInterruptedAt = v.interruptedAt;
				mvVoiceData.Add(data);
			}
		}
		return 0;
	case eOhplGa6Op_EffectVolumeCount:
		return (int)eLuxGlobalVolumeType_LastEnum;
	case eOhplGa6Op_EffectResizeVolumes:
		mvGlobalSoundVolumeMul.Resize(alA);
		mvGlobalSoundSpeedMul.Resize(alA);
		return 0;
	case eOhplGa6Op_EffectVolumeKind:
		return (alA == eLuxGlobalVolumeType_Commentary || alA == eLuxGlobalVolumeType_DebugMenu) ? eOhplGa6Vol_Skipped : eOhplGa6Vol_Normal;
	case eOhplGa6Op_EffectEntryPresent:
		e.mpEntry = pMulHandler->GetEntry(alB, false);
		return e.mpEntry != NULL;
	case eOhplGa6Op_EffectEntryFrom:
		vMul[alB].FromEntry(e.mpEntry);
		return 0;
	case eOhplGa6Op_EffectEntryInactive:
		vMul[alB].FromEntry(NULL);
		return 0;
	case eOhplGa6Op_EffectToBase:
		{
			uint64_t lH = apEffects->GetHandle();
			OhplA12aFxState st;
			ohpl_a12a_fx_get_state(lH, &st);

			st.fade.active = mbFade_Active;
			st.fade.goal = mfFade_GoalAlpha;
			st.fade.alpha = mfFade_Alpha;
			st.fade.speed = mfFade_FadeSpeed;

			gpBase->mpMapHandler->GetPostEffect_ImageTrail()->SetActive(mbImageTrail_Active);
			st.trail.active = mbImageTrail_Active;
			st.trail.amount = mfImageTrail_Amount;
			st.trail.goal = mfImageTrail_AmountGoal;
			st.trail.speed = mfImageTrail_FadeSpeed;

			gpBase->mpMapHandler->GetPostEffect_Sepia()->SetActive(mbSepiaColor_Active);
			st.sepia.active = mbSepiaColor_Active;
			st.sepia.amount = mfSepiaColor_Amount;
			st.sepia.goal = mfSepiaColor_AmountGoal;
			st.sepia.speed = mfSepiaColor_FadeSpeed;

			gpBase->mpMapHandler->GetPostEffect_RadialBlur()->SetActive(mbRadialBlur_Active);
			st.radial.active = mbRadialBlur_Active;
			st.radial.amount = mfRadialBlur_Size;
			st.radialStart = mfRadialBlur_BlurStartDist;
			st.radial.goal = mfRadialBlur_SizeGoal;
			st.radial.speed = mfRadialBlur_FadeSpeed;

			st.voiceActive = mbVoiceActive;
			ohpl_a12a_fx_set_state(lH, &st);
			ohpl_a12a_fx_voice_cb_set(lH, msVoiceOverCallback.data(), msVoiceOverCallback.size());
			ohpl_a12a_fx_voice_clear(lH);
			for(size_t i=0; i<mvVoiceData.Size(); ++i)
			{
				cLuxVoiceData &data = mvVoiceData[i];
				OhplA12aVoiceSave v;
				memset(&v, 0, sizeof(v));
				tWString *vTexts[7] = { &data.msText, &data.msText2, &data.msText3, &data.msText4, &data.msText5, &data.msText6, &data.msText7 };
				float vDelays[7] = { data.mfTextDelay, data.mfText2Delay, data.mfText3Delay, data.mfText4Delay, data.mfText5Delay, data.mfText6Delay, data.mfText7Delay };
				for(int t=0; t<7; ++t)
				{
					v.texts[t].p = (const unsigned*)vTexts[t]->data();
					v.texts[t].n = vTexts[t]->size();
					v.delays[t] = vDelays[t];
				}
				v.voice.p = (const unsigned char*)data.msVoiceFile.data(); v.voice.n = data.msVoiceFile.size();
				v.effect.p = (const unsigned char*)data.msEffectFile.data(); v.effect.n = data.msEffectFile.size();
				v.usePos = data.mbUsePosition;
				v.pos[0] = data.mvPosition.x; v.pos[1] = data.mvPosition.y; v.pos[2] = data.mvPosition.z;
				v.minDist = data.mfMinDistance;
				v.maxDist = data.mfMaxDistance;
				v.priority = data.mlPriority;
				v.currentTime = data.mfCurrentTime;
				v.interruptedAt = data.mfInterruptedAt;
				ohpl_a12a_fx_voice_push(lH, &v);
			}
		}
		return 0;
	case eOhplGa6Op_EffectSavedCount:
		return (int)vMul.Size();
	case eOhplGa6Op_EffectEntryActive:
		return vMul[alB].mbActive;
	case eOhplGa6Op_EffectEntryApply:
	case eOhplGa6Op_EffectEntryDestroy:
		vMul[alB].ToEntry(alB, pMulHandler);
		return 0;
	}
	return 0;
}

void cLuxEffectHandler_SaveData::FromEffectHandler(cLuxEffectHandler *apEffects)
{
	EffectsEnv env = { this, apEffects, NULL, NULL };
	ohpl_ga6_effects_from(&env, Ga6Tramp<EffectsEnv>);
}

void cLuxEffectHandler_SaveData::ToEffectHandler(cLuxMap *apMap, cLuxEffectHandler *apEffects)
{
	EffectsEnv env = { this, apEffects, apMap, NULL };
	ohpl_ga6_effects_to(&env, Ga6Tramp<EffectsEnv>);
}

//-----------------------------------------------------------------------

kBeginSerializeBase(cLuxEffectHandler_SoundMul_SaveData)
kSerializeVar(mbActive, eSerializeType_Bool)
kSerializeVar(mfVal, eSerializeType_Float32)
kSerializeVar(mfDest, eSerializeType_Float32)
kSerializeVar(mfSpeed, eSerializeType_Float32)
kSerializeVar(mTypes, eSerializeType_Int32)
kSerializeVar(mbDestroyAtDest, eSerializeType_Bool)
kEndSerialize()


kBeginSerializeBase(cLuxEffectHandler_SaveData)
kSerializeVar(mbFade_Active, eSerializeType_Bool)
kSerializeVar(mfFade_GoalAlpha, eSerializeType_Float32)
kSerializeVar(mfFade_Alpha, eSerializeType_Float32)
kSerializeVar(mfFade_FadeSpeed, eSerializeType_Float32)

kSerializeVar(mbImageTrail_Active, eSerializeType_Bool)
kSerializeVar(mfImageTrail_Amount, eSerializeType_Float32)
kSerializeVar(mfImageTrail_AmountGoal, eSerializeType_Float32)
kSerializeVar(mfImageTrail_FadeSpeed, eSerializeType_Float32)

kSerializeVar(mbSepiaColor_Active, eSerializeType_Bool)
kSerializeVar(mfSepiaColor_Amount, eSerializeType_Float32)
kSerializeVar(mfSepiaColor_AmountGoal, eSerializeType_Float32)
kSerializeVar(mfSepiaColor_FadeSpeed, eSerializeType_Float32)

kSerializeVar(mbRadialBlur_Active, eSerializeType_Bool)
kSerializeVar(mfRadialBlur_Size, eSerializeType_Float32)
kSerializeVar(mfRadialBlur_BlurStartDist, eSerializeType_Float32)
kSerializeVar(mfRadialBlur_SizeGoal, eSerializeType_Float32)
kSerializeVar(mfRadialBlur_FadeSpeed, eSerializeType_Float32)

kSerializeVar(msVoiceOverCallback, eSerializeType_String)
kSerializeVar(mbVoiceActive, eSerializeType_Bool)
kSerializeClassContainer(mvVoiceData, cLuxVoiceData, eSerializeType_Class)

kSerializeClassContainer(mvGlobalSoundVolumeMul, cLuxEffectHandler_SoundMul_SaveData, eSerializeType_Class)
kSerializeClassContainer(mvGlobalSoundSpeedMul, cLuxEffectHandler_SoundMul_SaveData, eSerializeType_Class)

kEndSerialize()
//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// MUSIC HANDLER
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

namespace
{

struct MusicEnv
{
	cLuxMusicHandler_SaveData *mpSelf;
	cLuxMusicHandler *mpMusic;
	cLuxMap *mpMap;
	cMusicEntry *mpEntry;
	iLuxEntity *mpEntity;
	std::vector<iLuxEnemy*> mvEnemies[2];
};

}

int cLuxMusicHandler_SaveData::Ga6Op(void *apEnv, unsigned alOp, int alA, int alB)
{
	MusicEnv &e = *static_cast<MusicEnv*>(apEnv);
	cLuxMusicHandler *apMusic = e.mpMusic;
	cLuxMap *apMap = e.mpMap;
	cMusicEntry *pMusicEntry = e.mpEntry;
	eLuxEnemyMusic setKind = alA == eOhplGa6Set_Attack ? eLuxEnemyMusic_Attack : eLuxEnemyMusic_Search;
	switch(alOp)
	{
	case eOhplGa6Op_MusicHasSong:
		e.mpEntry = apMusic->mpMusicHandler->GetCurrentSong();
		return e.mpEntry != NULL;
	case eOhplGa6Op_MusicFromSong:
		{
					msCurrentMusic = pMusicEntry->msFileName;
					mfCurrentMusicMaxVolume = pMusicEntry->mfMaxVolume;
					mfCurrentMusicVolume = pMusicEntry->mfVolume;
					mfCurrentMusicVolumeAdd = pMusicEntry->mfVolumeAdd;
					mfCurrentMusicTime = pMusicEntry->mpStream->GetElapsedTime();
					mbCurrentMusicLoop = pMusicEntry->mbLoop;
		}
		return 0;
	case eOhplGa6Op_MusicFromNoSong:
		msCurrentMusic = "";
		mfCurrentMusicVolume = 0;
		mfCurrentMusicVolumeAdd = 0;
		mfCurrentMusicMaxVolume = 0;
		mfCurrentMusicTime = 0;
		mbCurrentMusicLoop = false;
		return 0;
	case eOhplGa6Op_MusicFromMisc:
		{
				mlCurrentMaxPrio = apMusic->mlCurrentMaxPrio;
				mbEnemyClosePlaying = apMusic->mbEnemyClosePlaying;
				mbAttackPlaying = apMusic->mbEnemyPlaying[eLuxEnemyMusic_Attack];
				mbSearchPlaying = apMusic->mbEnemyPlaying[eLuxEnemyMusic_Search];

				///////////////////
				// Music
		}
		return 0;
	case eOhplGa6Op_MusicFromEntries:
		{
				mvMusic.Resize(apMusic->mvGameMusic.size());
				for(size_t i=0; i<mvMusic.Size(); ++i)
				{
					mvMusic[i].msFile = apMusic->mvGameMusic[i].msFile;
					mvMusic[i].mbLoop = apMusic->mvGameMusic[i].mbLoop;
					mvMusic[i].mbResume = apMusic->mvGameMusic[i].mbResume;
					mvMusic[i].mfVolume = apMusic->mvGameMusic[i].mfVolume;
					mvMusic[i].mbSpecialEffect = apMusic->mvGameMusic[i].mbSpecialEffect;
				}

				///////////////////
				// Enemies

				//Attackers
		}
		return 0;
	case eOhplGa6Op_MusicEnemyN:
		{
			e.mvEnemies[alA].clear();
			tLuxEnemySetIt it = apMusic->m_setEnemies[setKind].begin();
			for(; it != apMusic->m_setEnemies[setKind].end(); ++it)
				e.mvEnemies[alA].push_back(*it);
			return (int)e.mvEnemies[alA].size();
		}
	case eOhplGa6Op_MusicEnemyId:
		return e.mvEnemies[alA][alB]->GetID();
	case eOhplGa6Op_MusicAddAttackerId:
		mvAttackerIDs.Add(alA);
		return 0;
	case eOhplGa6Op_MusicHasSavedSong:
		return msCurrentMusic != "";
	case eOhplGa6Op_MusicPlay:
		return apMusic->mpMusicHandler->Play(msCurrentMusic,mfCurrentMusicMaxVolume,0.1f, mbCurrentMusicLoop, false);
	case eOhplGa6Op_MusicHasCurrentSong:
		e.mpEntry = apMusic->mpMusicHandler->GetCurrentSong();
		return e.mpEntry != NULL;
	case eOhplGa6Op_MusicApplySong:
		pMusicEntry->mfMaxVolume = mfCurrentMusicMaxVolume;
		pMusicEntry->mfVolume = mfCurrentMusicVolume;
		pMusicEntry->mfVolumeAdd = mfCurrentMusicVolumeAdd;
		pMusicEntry->mpStream->SetElapsedTime(mfCurrentMusicTime);
		pMusicEntry->mbLoop = mbCurrentMusicLoop;
		return 0;
	case eOhplGa6Op_MusicToMisc:
		{
				apMusic->mlCurrentMaxPrio = mlCurrentMaxPrio;
				apMusic->mbEnemyClosePlaying = mbEnemyClosePlaying;
				apMusic->mbEnemyPlaying[eLuxEnemyMusic_Attack] = mbAttackPlaying;
				apMusic->mbEnemyPlaying[eLuxEnemyMusic_Search] = mbSearchPlaying;


				///////////////////
				// Music
		}
		return 0;
	case eOhplGa6Op_MusicLiveEntryN:
		return (int)apMusic->mvGameMusic.size();
	case eOhplGa6Op_MusicSavedEntryN:
		return (int)mvMusic.Size();
	case eOhplGa6Op_MusicEntryTo:
		apMusic->mvGameMusic[alA].msFile = mvMusic[alA].msFile;
		apMusic->mvGameMusic[alA].mbLoop = mvMusic[alA].mbLoop;
		apMusic->mvGameMusic[alA].mbResume = mvMusic[alA].mbResume;
		apMusic->mvGameMusic[alA].mfVolume = mvMusic[alA].mfVolume;
		apMusic->mvGameMusic[alA].mbSpecialEffect = mvMusic[alA].mbSpecialEffect;
		return 0;
	case eOhplGa6Op_MusicSavedIdN:
		return (int)(alA == eOhplGa6Set_Attack ? mvAttackerIDs.Size() : mvSearcherIDs.Size());
	case eOhplGa6Op_MusicSavedId:
		return alA == eOhplGa6Set_Attack ? mvAttackerIDs[alB] : mvSearcherIDs[alB];
	case eOhplGa6Op_MusicFindEnemy:
		e.mpEntity = apMap->GetEntityByID(alA);
		return e.mpEntity && e.mpEntity->GetEntityType() == eLuxEntityType_Enemy;
	case eOhplGa6Op_MusicInsertEnemy:
		apMusic->m_setEnemies[alB == eOhplGa6Set_Attack ? eLuxEnemyMusic_Attack : eLuxEnemyMusic_Search].insert(static_cast<iLuxEnemy*>(e.mpEntity));
		return 0;
	case eOhplGa6Op_MusicWarn:
		Warning("Could not find entity with id %d\n", alA);
		return 0;
	}
	return 0;
}

void cLuxMusicHandler_SaveData::FromMusicHandler(cLuxMusicHandler *apMusic)
{
	MusicEnv env = { this, apMusic, NULL, NULL, NULL };
	ohpl_ga6_music_from(&env, Ga6Tramp<MusicEnv>);
}

//-----------------------------------------------------------------------

void cLuxMusicHandler_SaveData::ToMusicHandler(cLuxMap *apMap, cLuxMusicHandler *apMusic)
{
	MusicEnv env = { this, apMusic, apMap, NULL, NULL };
	ohpl_ga6_music_to(&env, Ga6Tramp<MusicEnv>);
}

//-----------------------------------------------------------------------

kBeginSerializeBase(cLuxMusicHandler_Music_SaveData)
kSerializeVar(msFile, eSerializeType_String)
kSerializeVar(mfVolume, eSerializeType_Float32)
kSerializeVar(mbLoop, eSerializeType_Bool)
kSerializeVar(mbResume, eSerializeType_Bool)
kSerializeVar(mbSpecialEffect, eSerializeType_Bool)
kEndSerialize()

//-----------------------------------------------------------------------

kBeginSerializeBase(cLuxMusicHandler_SaveData)
kSerializeVar(msCurrentMusic, eSerializeType_String)
kSerializeVar(mfCurrentMusicVolume, eSerializeType_Float32)
kSerializeVar(mfCurrentMusicVolumeAdd, eSerializeType_Float32)
kSerializeVar(mfCurrentMusicMaxVolume, eSerializeType_Float32)
kSerializeVar(mfCurrentMusicTime, eSerializeType_Float32)
kSerializeVar(mbCurrentMusicLoop, eSerializeType_Bool)

kSerializeVar(mlCurrentMaxPrio, eSerializeType_Int32)
kSerializeVar(mbEnemyClosePlaying, eSerializeType_Bool)
kSerializeVar(mbAttackPlaying, eSerializeType_Bool)
kSerializeVar(mbSearchPlaying, eSerializeType_Bool)

kSerializeClassContainer(mvMusic,  cLuxMusicHandler_Music_SaveData, eSerializeType_Class)
kSerializeVarContainer(mvAttackerIDs,eSerializeType_Int32)
kSerializeVarContainer(mvSearcherIDs,eSerializeType_Int32)
kEndSerialize()

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// JOURNAL DIARY CONTAINER
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

kBeginSerializeBase(cLuxJournal_DiaryCont_SaveData)
kSerializeVar(msType, eSerializeType_String)
kSerializeClassContainer(mvDiaries, cLuxDiary, eSerializeType_Class)
kEndSerialize()


//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// JOURNAL
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxJournal_SaveData::cLuxJournal_SaveData()
{

}

//-----------------------------------------------------------------------

cLuxJournal_SaveData::~cLuxJournal_SaveData()
{

}

//-----------------------------------------------------------------------

void cLuxJournal_SaveData::FromJournal(cLuxJournal *apJournal)
{
	/////////////
	//Vars
	mlLastReadTextCat = apJournal->mlLastReadTextCat;
	mlLastReadTextEntry = apJournal->mlLastReadTextEntry;
	mlLastReadTextType = apJournal->mlLastReadTextType;

	/////////////
	//Notes
    mvNotes.Resize(apJournal->mvNotes.size());
	for(size_t i=0; i<mvNotes.Size(); ++i)
	{
		mvNotes[i] = *apJournal->mvNotes[i];
	}

	/////////////
	//Diaries
	mvDiaryConts.Resize(apJournal->mvDiaryContainers.size());
	for(size_t cont=0; cont<mvDiaryConts.Size(); ++cont)
	{
		mvDiaryConts[cont].msType = apJournal->mvDiaryContainers[cont]->msType;
		mvDiaryConts[cont].mvDiaries.Resize(apJournal->mvDiaryContainers[cont]->mvDiaries.size());
		for(size_t i=0; i<mvDiaryConts[cont].mvDiaries.Size(); ++i)
		{
			mvDiaryConts[cont].mvDiaries[i] = *apJournal->mvDiaryContainers[cont]->mvDiaries[i];
		}
	}

	/////////////
	//Quest notes
	mvQuestNotes.Resize(apJournal->mvQuestNotes.size());
	for(size_t i=0; i<mvQuestNotes.Size(); ++i)
	{
		mvQuestNotes[i] = *apJournal->mvQuestNotes[i];
	}
}

void cLuxJournal_SaveData::ToJournal(cLuxMap *apMap, cLuxJournal *apJournal)
{
	/////////////
	//Vars
	apJournal->mlLastReadTextCat = mlLastReadTextCat;
	apJournal->mlLastReadTextEntry = mlLastReadTextEntry;
	apJournal->mlLastReadTextType = mlLastReadTextType;

	/////////////
	//Notes
	apJournal->mvNotes.resize(mvNotes.Size());
	for(size_t i=0; i<mvNotes.Size(); ++i)
	{
		apJournal->mvNotes[i] = hplNew(cLuxNote, ());
		*apJournal->mvNotes[i] = mvNotes[i];
	}

	/////////////
	//Diaries
	apJournal->mvDiaryContainers.resize(mvDiaryConts.Size());
	for(size_t cont=0; cont<mvDiaryConts.Size(); ++cont)
	{
		apJournal->mvDiaryContainers[cont] = hplNew(cLuxDiaryContainer, ());
		apJournal->mvDiaryContainers[cont]->msType = mvDiaryConts[cont].msType;
		apJournal->mvDiaryContainers[cont]->mvDiaries.resize(mvDiaryConts[cont].mvDiaries.Size());
		for(size_t i=0; i<mvDiaryConts[cont].mvDiaries.Size(); ++i)
		{
			apJournal->mvDiaryContainers[cont]->mvDiaries[i] = hplNew(cLuxDiary, ());
			*apJournal->mvDiaryContainers[cont]->mvDiaries[i] = mvDiaryConts[cont].mvDiaries[i];
		}
	}

	/////////////
	//QuestNotes
	apJournal->mvQuestNotes.resize(mvQuestNotes.Size());
	for(size_t i=0; i<mvQuestNotes.Size(); ++i)
	{
		apJournal->mvQuestNotes[i] = hplNew(cLuxQuestNote, ());
		*apJournal->mvQuestNotes[i] = mvQuestNotes[i];
	}
}

//-----------------------------------------------------------------------

kBeginSerializeBase(cLuxJournal_SaveData)
kSerializeVar(mlLastReadTextCat, eSerializeType_Int32)
kSerializeVar(mlLastReadTextEntry, eSerializeType_Int32)
kSerializeVar(mlLastReadTextType, eSerializeType_Int32)
kSerializeClassContainer(mvNotes, cLuxNote, eSerializeType_Class)
kSerializeClassContainer(mvDiaryConts, cLuxJournal_DiaryCont_SaveData, eSerializeType_Class)
kSerializeClassContainer(mvQuestNotes, cLuxQuestNote, eSerializeType_Class)
kEndSerialize()

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// INVENTORY ITEM
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxInventory_Item_SaveData::FromItem(cLuxInventory_Item *apItem)
{
	msName = OhplInvName(apItem);
	msSubType = OhplInvSubType(apItem);
	mlType = OhplInvType(apItem);
	msVal = OhplInvStringVal(apItem);
	msExtraVal = OhplInvExtraVal(apItem);

	msImageFile = OhplInvImageName(apItem);

	mfAmount = OhplInvAmount(apItem);
	mlCount = OhplInvCount(apItem);
	msGameNameEntry = OhplInvNameEntry(apItem);
	msGameDescEntry = OhplInvDescEntry(apItem);
}

//-----------------------------------------------------------------------

kBeginSerializeBase(cLuxInventory_Item_SaveData)
kSerializeVar(msName, eSerializeType_String)
kSerializeVar(mlType, eSerializeType_Int32)
kSerializeVar(msSubType, eSerializeType_String)
kSerializeVar(msVal, eSerializeType_String)
kSerializeVar(msExtraVal, eSerializeType_String)

kSerializeVar(msImageFile, eSerializeType_String)

kSerializeVar(mfAmount, eSerializeType_Float32)
kSerializeVar(mlCount, eSerializeType_Int32)
kSerializeVar(msGameNameEntry, eSerializeType_String)
kSerializeVar(msGameDescEntry, eSerializeType_String)
kEndSerialize()

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// INVENTORY 
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxInventory_SaveData::cLuxInventory_SaveData()
{

}

cLuxInventory_SaveData::~cLuxInventory_SaveData()
{

}

//-----------------------------------------------------------------------

void cLuxInventory_SaveData::FromInventory(cLuxInventory *apInventory)
{
	///////////////////////////
	//Variables
	mbDisabled = apInventory->GetDisabled();
	mlEquippedHandItem = apInventory->GetItemIndex(apInventory->GetEquippedHandItem());

	///////////////////////////
	//Items
	mvItems.Resize(apInventory->GetItemNum());
	for(size_t i=0; i<mvItems.Size(); ++i)
	{
		mvItems[i].FromItem(apInventory->GetItem((int)i));
	}

	///////////////////////////
	//Combine callbacks
	{
		for(int i=0; i<ohpl_inv_combine_num(); ++i)
		{
			cLuxCombineItemsCallback callback;
			ohpl_inv_combine_str(i, 0, OhplA11aStrSink, &callback.msName);
			ohpl_inv_combine_str(i, 1, OhplA11aStrSink, &callback.msItemA);
			ohpl_inv_combine_str(i, 2, OhplA11aStrSink, &callback.msItemB);
			ohpl_inv_combine_str(i, 3, OhplA11aStrSink, &callback.msFunction);
			callback.mbAutoDestroy = ohpl_inv_combine_auto(i) != 0;
			mvCombineCallbacks.Add(callback);
		}
	}
}

//-----------------------------------------------------------------------

namespace
{

struct InventoryEnv
{
	cLuxInventory_SaveData *mpSelf;
	cLuxInventory *mpData;
};

}

int cLuxInventory_SaveData::Ga6Op(void *apEnv, unsigned alOp, int alA, int alB)
{
	InventoryEnv &e = *static_cast<InventoryEnv*>(apEnv);
	cLuxInventory *apInventory = e.mpData;
	switch(alOp)
	{
	case eOhplGa6Op_InvToDisabled:
		apInventory->SetDisabled(mbDisabled);
		return 0;
	case eOhplGa6Op_InvEquippedIndex:
		return mlEquippedHandItem;
	case eOhplGa6Op_InvEquip:
		{
			cLuxInventory_Item *pItem = apInventory->GetItem(mlEquippedHandItem);
			apInventory->SetEquippedHandItem(pItem);
		}
		return 0;
	case eOhplGa6Op_InvEquipNone:
		apInventory->SetEquippedHandItem(NULL);
		return 0;
	case eOhplGa6Op_InvToItems:
		{
				for(size_t i=0; i<mvItems.Size(); ++i)
				{
					cLuxInventory_Item_SaveData& saveItem = mvItems[i];

					cLuxInventory_Item *pItem = apInventory->AddItem(	saveItem.msName, (eLuxItemType)saveItem.mlType, saveItem.msSubType,
																		saveItem.msImageFile, saveItem.mfAmount, saveItem.msVal, saveItem.msExtraVal);
					ohpl_inv_item_set_entries((uintptr_t)pItem, OhplA11aS(saveItem.msGameNameEntry), OhplA11aS(saveItem.msGameDescEntry));
					ohpl_inv_item_set_count((uintptr_t)pItem, saveItem.mlCount);
				}

				///////////////////////////
				//Combine callbacks
		}
		return 0;
	case eOhplGa6Op_InvToCallbacks:
		{
				for(size_t i=0; i<mvCombineCallbacks.Size(); ++i)
				{
					cLuxCombineItemsCallback &callback = mvCombineCallbacks[i];
					apInventory->AddCombineCallback(callback.msName, callback.msItemA, callback.msItemB, callback.msFunction, callback.mbAutoDestroy);
				}
		}
		return 0;
	}
	return 0;
}

void cLuxInventory_SaveData::ToInventory(cLuxMap *apMap, cLuxInventory *apInventory)
{
	InventoryEnv env = { this, apInventory };
	ohpl_ga6_inventory_to(&env, Ga6Tramp<InventoryEnv>);
}

//-----------------------------------------------------------------------

kBeginSerializeBase(cLuxInventory_SaveData)
kSerializeVar(mbDisabled, eSerializeType_Bool)
kSerializeVar(mlEquippedHandItem, eSerializeType_Int32)
kSerializeClassContainer(mvItems, cLuxInventory_Item_SaveData, eSerializeType_Class)
kSerializeClassContainer(mvCombineCallbacks, cLuxCombineItemsCallback, eSerializeType_Class)
kEndSerialize()

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PLAYER HANDS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

namespace
{

struct HandsEnv
{
	cLuxPlayerHands_SaveData *mpSelf;
	cLuxPlayerHands *mpHands;
	cLuxMap *mpMap;
};

}

int cLuxPlayerHands_SaveData::Ga6Op(void *apEnv, unsigned alOp, int alA, int alB)
{
	HandsEnv &e = *static_cast<HandsEnv*>(apEnv);
	cLuxPlayerHands *apPlayerHands = e.mpHands;
	cLuxMap *apMap = e.mpMap;
	switch(alOp)
	{
	case eOhplGa6Op_HandsHasObject:
		return apPlayerHands->mpCurrentHandObject != NULL;
	case eOhplGa6Op_HandsFromObject:
		msCurrentHandObject = apPlayerHands->mpCurrentHandObject->GetName();
		return 0;
	case eOhplGa6Op_HandsFromNoObject:
		msCurrentHandObject = "";
		return 0;
	case eOhplGa6Op_HandsFromRest:
		mlState = apPlayerHands->mHandState;
		{
				msCurrentAnim = apPlayerHands->msCurrentAnim;

			    mMesh.FromMeshEntity(apPlayerHands->mpHandsEntity);

				mfHandObjectChargeCount = apPlayerHands->mfHandObjectChargeCount;
				mlHandObjectState = apPlayerHands->mlHandObjectState;
				mbHandObjectAttackDown = apPlayerHands->mbHandObjectAttackDown;
				mbHandObjectInteractDown = apPlayerHands->mbHandObjectInteractDown;
				mfHandObjectAlpha = apPlayerHands->mfHandObjectAlpha;
		}
		return 0;
	case eOhplGa6Op_HandsToState:
		apPlayerHands->mHandState = (eLuxHandsState)mlState;
		return 0;
	case eOhplGa6Op_HandsHasSavedObject:
		return msCurrentHandObject != "";
	case eOhplGa6Op_HandsFindObject:
		apPlayerHands->mpCurrentHandObject = apPlayerHands->GetHandObject(msCurrentHandObject);
		return 0;
	case eOhplGa6Op_HandsHasCurrentObject:
		return apPlayerHands->mpCurrentHandObject != NULL;
	case eOhplGa6Op_HandsAttachObject:
		apPlayerHands->CreateAndAttachHandObject(apMap, apPlayerHands->mpCurrentHandObject);
		return 0;
	case eOhplGa6Op_HandsToRest:
		{
				apPlayerHands->msCurrentAnim = msCurrentAnim;

				mMesh.ToMeshEntity(apPlayerHands->mpHandsEntity);

				apPlayerHands->mfHandObjectChargeCount = mfHandObjectChargeCount;
				apPlayerHands->mlHandObjectState = mlHandObjectState;
				apPlayerHands->mbHandObjectAttackDown = mbHandObjectAttackDown;
				apPlayerHands->mbHandObjectInteractDown = mbHandObjectInteractDown;
				apPlayerHands->mfHandObjectAlpha = mfHandObjectAlpha;

				//If hand object is active, set state as idle
		}
		return 0;
	case eOhplGa6Op_HandsIdleCondition:
		return apPlayerHands->mHandState == eLuxHandsState_HandObject && apPlayerHands->mpCurrentHandObject;
	case eOhplGa6Op_HandsIdleAfterLoad:
		apPlayerHands->PlayAnim(apPlayerHands->mpCurrentHandObject->GetAnimIdle(),true);
		apPlayerHands->mHandState = eLuxHandsState_Idle;
		return 0;
	}
	return 0;
}

void cLuxPlayerHands_SaveData::FromPlayerHands(cLuxPlayerHands *apPlayerHands)
{
	HandsEnv env = { this, apPlayerHands, NULL };
	ohpl_ga6_hands_from(&env, Ga6Tramp<HandsEnv>);
}

void cLuxPlayerHands_SaveData::ToPlayerHands(cLuxMap *apMap, cLuxPlayerHands *apPlayerHands)
{
	HandsEnv env = { this, apPlayerHands, apMap };
	ohpl_ga6_hands_to(&env, Ga6Tramp<HandsEnv>);
}

//-----------------------------------------------------------------------

kBeginSerializeBase(cLuxPlayerHands_SaveData)
kSerializeVar(mlState, eSerializeType_Int32)
kSerializeVar(msCurrentHandObject, eSerializeType_String)
kSerializeVar(msCurrentAnim, eSerializeType_String)
kSerializeVar(mMesh, eSerializeType_Class)

kSerializeVar(mfHandObjectChargeCount, eSerializeType_Float32)
kSerializeVar(mlHandObjectState, eSerializeType_Int32)
kSerializeVar(mbHandObjectAttackDown, eSerializeType_Bool)
kSerializeVar(mbHandObjectInteractDown, eSerializeType_Bool)
kSerializeVar(mfHandObjectAlpha, eSerializeType_Float32)
kEndSerialize()

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// PLAYER
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPlayer_SaveData::cLuxPlayer_SaveData()
{
	mpStateData = NULL;

	// Default values on new variables, to not break old saves
	mfDarkness_Active = true;
	mfScriptJumpForceMul = 1.0f;
}

cLuxPlayer_SaveData::~cLuxPlayer_SaveData()
{
	if(mpStateData) hplDelete(mpStateData);
}

//-----------------------------------------------------------------------

namespace
{

struct PlayerEnv
{
	cLuxPlayer_SaveData *mpSelf;
	cLuxPlayer *mpPlayer;
	cLuxMap *mpMap;
	iLuxPlayerState *mpState;
	eLuxPlayerState mState;
	iLuxEntity *mpEntity;
};

}

int cLuxPlayer_SaveData::Ga6Op(void *apEnv, unsigned alOp, int alA, int alB)
{
	PlayerEnv &e = *static_cast<PlayerEnv*>(apEnv);
	cLuxPlayer *apPlayer = e.mpPlayer;
	cLuxMap *apMap = e.mpMap;
	iLuxPlayerState *pState = e.mpState;
	switch(alOp)
	{
	case eOhplGa6Op_PlayerStateSaved:
		e.mpState = apPlayer->GetCurrentStateData();
		return e.mpState->IsSaved();
	case eOhplGa6Op_PlayerFromState:
		mlState = apPlayer->GetCurrentState();
		mpStateData = pState->CreateSaveData();
		pState->SaveToSaveData(mpStateData);
		return 0;
	case eOhplGa6Op_PlayerFromNoState:
		mlState = eLuxPlayerState_Normal;
		mpStateData = NULL;
		return 0;
	case eOhplGa6Op_PlayerFromBlock:
		{
				mbActive = apPlayer->mbActive;

			    mbUsesDragFootsteps = apPlayer->mbUsesDragFootsteps;

				mfHealth = apPlayer->mfHealth;
				mfSanity = apPlayer->mfSanity;
				mfLampOil = apPlayer->mfLampOil;

				mfTerror = apPlayer->mfTerror;
				mlCoins = apPlayer->mlCoins;
				mlTinderboxes = apPlayer->mlTinderboxes;

				mvHeadSpinSpeed = apPlayer->mvHeadSpinSpeed;

				mbLanternOn = apPlayer->GetHelperLantern()->IsActive();
				mbLanternDisabled = apPlayer->GetHelperLantern()->GetDisabled();

				msDeathHintCat = apPlayer->GetHelperDeath()->GetHintCat();
				msDeathHintEntry = apPlayer->GetHelperDeath()->GetHintEntry();

				cLuxMoveState_Normal *pMoveNormal = static_cast<cLuxMoveState_Normal*>(apPlayer->GetMoveStateData(eLuxMoveState_Normal));
			    mbCrouching = pMoveNormal->mbCrouching;

				//////////////////////
				/// SpawnPS
				mbSpawnPSActive = apPlayer->GetHelperSpawnPS()->IsActive();
				mbSpawnPSFile = apPlayer->GetHelperSpawnPS()->GetFileName();
	
				//////////////////////
				/// Flashback
				mbFlashbackActive = apPlayer->GetHelperFlashback()->mbActive;
				mfFlashbackStartCount =	apPlayer->GetHelperFlashback()->mfFlashbackStartCount;
				mfFlashDelay = apPlayer->GetHelperFlashback()->mfFlashDelay;
				msFlashbackFile = apPlayer->GetHelperFlashback()->msFlashbackFile;
				msFlashbackCallback = apPlayer->GetHelperFlashback()->msCallback;
				mlstFlashbackQueue.Clear();
				tLuxFlashbackDataListIt flashIt = apPlayer->GetHelperFlashback()->mlstFlashbackQueue.begin();
				for(; flashIt != apPlayer->GetHelperFlashback()->mlstFlashbackQueue.end(); ++flashIt)
				{
					cLuxFlashbackData& flashData = *flashIt;
					cLuxFlashbackData_SaveData saveData;
					saveData.msFile = flashData.msFile;
					saveData.msCallback = flashData.msCallback;

					mlstFlashbackQueue.Add(saveData);
				}

				//////////////////////
				/// Insanity collapse
				mbInsanityCollapse_Active = apPlayer->GetInsanityCollapse()->mbActive;
				mlInsanityCollapse_State = apPlayer->GetInsanityCollapse()->mlState;
				mfInsanityCollapse_HeightAdd = apPlayer->GetInsanityCollapse()->mfHeightAdd;
				mfInsanityCollapse_Roll = apPlayer->GetInsanityCollapse()->mfRoll;
				mfInsanityCollapse_T = apPlayer->GetInsanityCollapse()->mfT;;

				//////////////////////
				/// Sanity
				mfSanity_AtLowSanityCount = apPlayer->GetHelperSanity()->mfAtLowSanityCount;

				//////////////////////
				// Look At
				mbLookAt_Active = apPlayer->GetHelperLookAt()->mbActive;
				mvLookAt_TargetPos = apPlayer->GetHelperLookAt()->mvTargetPos;
				mvLookAt_CurrentSpeed = apPlayer->GetHelperLookAt()->mvCurrentSpeed;
				mfLookAt_SpeedMul = apPlayer->GetHelperLookAt()->mfSpeedMul;
				mfLookAt_MaxSpeed = apPlayer->GetHelperLookAt()->mfMaxSpeed;
				msLookAt_AtTargetCallback = apPlayer->GetHelperLookAt()->msAtTargetCallback;
				mfLookAt_DestFovMul = apPlayer->GetHelperLookAt()->mfDestFovMul;
				mfLookAt_Fov = apPlayer->GetHelperLookAt()->mfFov;
				mfLookAt_FovSpeed = apPlayer->GetHelperLookAt()->mfFovSpeed;
				mfLookAt_FovMaxSpeed = apPlayer->GetHelperLookAt()->mfFovMaxSpeed;

				//////////////////////
				// Darkness
				mfDarkness_Active = apPlayer->GetHelperInDarkness()->mbActive;

				//////////////////////
				/// Misc vars
				kCopyFromVar(apPlayer, mbIsInWater);
				kCopyFromVar(apPlayer, msWaterStepSound);
				kCopyFromVar(apPlayer, mfWaterSpeedMul);

				kCopyFromVar(apPlayer, mbJumpDisabled);
				kCopyFromVar(apPlayer, mbCrouchDisabled);
				kCopyFromVar(apPlayer, mbSanityDrainDisabled);

				kCopyFromVar(apPlayer, mfInteractionMoveSpeedMul);
				kCopyFromVar(apPlayer, mfEventMoveSpeedMul);
				kCopyFromVar(apPlayer, mfEventRunSpeedMul);
				kCopyFromVar(apPlayer, mfScriptMoveSpeedMul);
				kCopyFromVar(apPlayer, mfScriptRunSpeedMul);
				kCopyFromVar(apPlayer, mfLookSpeedMul);
				kCopyFromVar(apPlayer, mfHurtMoveSpeedMul);
				kCopyFromVar(apPlayer, mfInsanityCollapseSpeedMul);

				kCopyFromVar(apPlayer, mfScriptJumpForceMul);

				kCopyFromVar(apPlayer, mfAspectMul);
				kCopyFromVar(apPlayer, mfFOVMul);
				kCopyFromVar(apPlayer, mfAspectMulGoal);
				kCopyFromVar(apPlayer, mfFOVMulGoal);
				kCopyFromVar(apPlayer, mfAspectMulSpeed);
				kCopyFromVar(apPlayer, mfFOVMulSpeed);

				kCopyFromVar(apPlayer, mfRoll);
				kCopyFromVar(apPlayer, mfRollGoal);
				kCopyFromVar(apPlayer, mfRollSpeedMul);
				kCopyFromVar(apPlayer, mfRollMaxSpeed);

			    kCopyFromVar( apPlayer, mbFadingPitch );
			    kCopyFromVar( apPlayer, mfPitchGoal );
			    kCopyFromVar( apPlayer, mfPitchSpeedMul );
			    kCopyFromVar( apPlayer, mfPitchMaxSpeed );

				kCopyFromVar(apPlayer, mvCamAnimPos);
				kCopyFromVar(apPlayer, mvCamAnimPosGoal);
				kCopyFromVar(apPlayer, mfCamAnimPosSpeedMul);
				kCopyFromVar(apPlayer, mfCamAnimPosMaxSpeed);

				kCopyFromVar(apPlayer, mbNoFallDamage);

				kCopyFromVar(apPlayer, mbScriptShowFocusIconAndCrossHair);

				//TODO: Get the correct move mode!?

				//////////////////////
				///Body
				mCharBody.FromBody(apPlayer->mpCharBody);
	
				//////////////////////
				///Camera
				cCamera *pCam = apPlayer->GetCamera();
				mvCameraAngles = cVector3f(pCam->GetPitch(),pCam->GetYaw(), pCam->GetRoll());

				mfPitchMaxLimit = pCam->GetPitchMaxLimit();
				mfPitchMinLimit = pCam->GetPitchMinLimit();
				mfYawMaxLimit = pCam->GetYawMaxLimit();
				mfYawMinLimit = pCam->GetYawMinLimit();

				mvHeadPosAdds.Resize(apPlayer->mvHeadPosAdds.size());
				for(size_t i=0; i<mvHeadPosAdds.Size(); ++i)
					mvHeadPosAdds[i] = apPlayer->mvHeadPosAdds[i];

				///////////////////
				// Terror Enemies
				tLuxEnemySetIt it = apPlayer->m_setTerrorEnemies.begin();
				for(; it != apPlayer->m_setTerrorEnemies.end(); ++it)
				{
					iLuxEnemy *pEnemy = *it;
					mvTerrorEnemyIDs.Add(pEnemy->GetID());
				}

				//////////////////////
				///Player hands
				mPlayerHands.FromPlayerHands(apPlayer->GetHands());

				//////////////////////
				///Collide callbacks
				for(tLuxCollideCallbackListIt it = apPlayer->mlstCollideCallbacks.begin(); it != apPlayer->mlstCollideCallbacks.end(); ++it)
				{
					cLuxCollideCallback *pCallback = *it;
					cLuxCollideCallback_SaveData saveCallback;
					saveCallback.FromCallback(pCallback);

					mlstCollideCallbacks.Add(saveCallback);
				}
		}
		return 0;
	case eOhplGa6Op_PlayerToStateBefore:
		e.mState = (eLuxPlayerState)mlState;
		e.mpState = apPlayer->GetStateData(e.mState);
		return 0;
	case eOhplGa6Op_PlayerHasStateData:
		return mpStateData != NULL;
	case eOhplGa6Op_PlayerLoadStateBefore:
		pState->LoadFromSaveDataBeforeEnter(apMap, mpStateData);
		return 0;
	case eOhplGa6Op_PlayerChangeState:
		apPlayer->ChangeState(e.mState);
		return 0;
	case eOhplGa6Op_PlayerLoadStateAfter:
		pState->LoadFromSaveDataAfterEnter(apMap, mpStateData);
		return 0;
	case eOhplGa6Op_PlayerToBlock1:
		{
				apPlayer->mbActive = mbActive;

			    apPlayer->mbUsesDragFootsteps = mbUsesDragFootsteps;

				apPlayer->mfHealth = mfHealth;
				apPlayer->mfSanity = mfSanity;
				apPlayer->mfLampOil = mfLampOil;
	
				apPlayer->mfTerror = mfTerror;
				apPlayer->mlCoins = mlCoins;
				apPlayer->mlTinderboxes = mlTinderboxes;

				apPlayer->mvHeadSpinSpeed = mvHeadSpinSpeed;

				apPlayer->GetHelperLantern()->SetActive(mbLanternOn, false, false);
				apPlayer->GetHelperLantern()->SetDisabled(mbLanternDisabled);

				apPlayer->GetHelperDeath()->SetHint(msDeathHintCat, msDeathHintEntry);

				cLuxMoveState_Normal *pMoveNormal = static_cast<cLuxMoveState_Normal*>(apPlayer->GetMoveStateData(eLuxMoveState_Normal));
				pMoveNormal->mbCrouching = mbCrouching;

				//////////////////////
				/// SpawnPS
		}
		return 0;
	case eOhplGa6Op_PlayerSpawnPsActive:
		return mbSpawnPSActive;
	case eOhplGa6Op_PlayerSpawnPsStart:
		apPlayer->GetHelperSpawnPS()->Start(mbSpawnPSFile);
		return 0;
	case eOhplGa6Op_PlayerSpawnPsStop:
		apPlayer->GetHelperSpawnPS()->Stop();
		return 0;
	case eOhplGa6Op_PlayerToBlock2:
		{
				apPlayer->GetHelperFlashback()->mbActive = mbFlashbackActive;
				apPlayer->GetHelperFlashback()->mfFlashbackStartCount = mfFlashbackStartCount;
				apPlayer->GetHelperFlashback()->mfFlashDelay = mfFlashDelay;
				apPlayer->GetHelperFlashback()->msFlashbackFile = msFlashbackFile;
				apPlayer->GetHelperFlashback()->msCallback = msFlashbackCallback;
	
				apPlayer->GetHelperFlashback()->mlstFlashbackQueue.clear();
				cContainerListIterator<cLuxFlashbackData_SaveData> flashIt =mlstFlashbackQueue.GetIterator();
				while(flashIt.HasNext())
				{
					cLuxFlashbackData_SaveData saveData = flashIt.Next();
					apPlayer->GetHelperFlashback()->mlstFlashbackQueue.push_back(cLuxFlashbackData(saveData.msFile,saveData.msCallback) );
				}

				//////////////////////
				/// Insanity collapse
				apPlayer->GetInsanityCollapse()->mbActive = mbInsanityCollapse_Active;
				apPlayer->GetInsanityCollapse()->mlState = mlInsanityCollapse_State;
				apPlayer->GetInsanityCollapse()->mfHeightAdd = mfInsanityCollapse_HeightAdd;
				apPlayer->GetInsanityCollapse()->mfRoll = mfInsanityCollapse_Roll;
				apPlayer->GetInsanityCollapse()->mfT = mfInsanityCollapse_T;

				//////////////////////
				/// Sanity
				apPlayer->GetHelperSanity()->mfAtLowSanityCount = mfSanity_AtLowSanityCount;

				//////////////////////
				// Look At
				apPlayer->GetHelperLookAt()->mbActive = mbLookAt_Active;
				apPlayer->GetHelperLookAt()->mvTargetPos = mvLookAt_TargetPos;
				apPlayer->GetHelperLookAt()->mvCurrentSpeed = mvLookAt_CurrentSpeed;
				apPlayer->GetHelperLookAt()->mfSpeedMul = mfLookAt_SpeedMul;
				apPlayer->GetHelperLookAt()->mfMaxSpeed = mfLookAt_MaxSpeed;
				apPlayer->GetHelperLookAt()->msAtTargetCallback = msLookAt_AtTargetCallback;
				apPlayer->GetHelperLookAt()->mfDestFovMul = mfLookAt_DestFovMul;
				apPlayer->GetHelperLookAt()->mfFov = mfLookAt_Fov;
				apPlayer->GetHelperLookAt()->mfFovSpeed = mfLookAt_FovSpeed;
				apPlayer->GetHelperLookAt()->mfFovMaxSpeed = mfLookAt_FovMaxSpeed;
	
				//////////////////////
				// Darkness
				apPlayer->GetHelperInDarkness()->SetActive(mfDarkness_Active);

				//////////////////////
				/// Misc vars
				kCopyToVar(apPlayer, mbIsInWater);
				kCopyToVar(apPlayer, msWaterStepSound);
				kCopyToVar(apPlayer, mfWaterSpeedMul);

				kCopyToVar(apPlayer, mbJumpDisabled);
				kCopyToVar(apPlayer, mbCrouchDisabled);
				kCopyFromVar(apPlayer, mbSanityDrainDisabled);

				kCopyToVar(apPlayer, mfInteractionMoveSpeedMul);
				kCopyToVar(apPlayer, mfEventMoveSpeedMul);
				kCopyToVar(apPlayer, mfEventRunSpeedMul);
				kCopyToVar(apPlayer, mfScriptMoveSpeedMul);
				kCopyToVar(apPlayer, mfScriptRunSpeedMul);
				kCopyToVar(apPlayer, mfLookSpeedMul);
				kCopyToVar(apPlayer, mfHurtMoveSpeedMul);
				kCopyToVar(apPlayer, mfInsanityCollapseSpeedMul);

				kCopyToVar(apPlayer, mfScriptJumpForceMul);

				kCopyToVar(apPlayer, mfAspectMul);
				kCopyToVar(apPlayer, mfFOVMul);
				kCopyToVar(apPlayer, mfAspectMulGoal);
				kCopyToVar(apPlayer, mfFOVMulGoal);
				kCopyToVar(apPlayer, mfAspectMulSpeed);
				kCopyToVar(apPlayer, mfFOVMulSpeed);

				kCopyToVar(apPlayer, mfRoll);
				kCopyToVar(apPlayer, mfRollGoal);
				kCopyToVar(apPlayer, mfRollSpeedMul);
				kCopyToVar(apPlayer, mfRollMaxSpeed);

			    kCopyToVar( apPlayer, mbFadingPitch );
			    kCopyToVar( apPlayer, mfPitchGoal );
			    kCopyToVar( apPlayer, mfPitchSpeedMul );
			    kCopyToVar( apPlayer, mfPitchMaxSpeed );

				kCopyToVar(apPlayer, mvCamAnimPos);
				kCopyToVar(apPlayer, mvCamAnimPosGoal);
				kCopyToVar(apPlayer, mfCamAnimPosSpeedMul);
				kCopyToVar(apPlayer, mfCamAnimPosMaxSpeed);

				kCopyToVar(apPlayer, mbNoFallDamage);

				kCopyToVar(apPlayer, mbScriptShowFocusIconAndCrossHair);

				//TODO: Set the correct move mode!


				//////////////////////
				///Body
				mCharBody.ToBody(apPlayer->mpCharBody);


				//////////////////////
				///Camera
				cCamera *pCam = apPlayer->GetCamera();
				pCam->SetPitchLimits(mfPitchMinLimit, mfPitchMaxLimit);
				pCam->SetYawLimits(mfYawMinLimit, mfYawMaxLimit);
	
				pCam->SetPitch(mvCameraAngles.x);
				pCam->SetYaw(mvCameraAngles.y);
				pCam->SetRoll(mvCameraAngles.z);
		}
		return 0;
	case eOhplGa6Op_PlayerHeadPosAddsMatch:
		return apPlayer->mvHeadPosAdds.size() == mvHeadPosAdds.Size();
	case eOhplGa6Op_PlayerToHeadPosAdds:
		for(size_t i=0; i<apPlayer->mvHeadPosAdds.size(); ++i)
			apPlayer->mvHeadPosAdds[i] = mvHeadPosAdds[i];
		return 0;
	case eOhplGa6Op_PlayerSavedTerrorN:
		return (int)mvTerrorEnemyIDs.Size();
	case eOhplGa6Op_PlayerSavedTerrorId:
		return mvTerrorEnemyIDs[alA];
	case eOhplGa6Op_PlayerFindEnemy:
		e.mpEntity = apMap->GetEntityByID(alA);
		return e.mpEntity && e.mpEntity->GetEntityType() == eLuxEntityType_Enemy;
	case eOhplGa6Op_PlayerInsertTerror:
		apPlayer->m_setTerrorEnemies.insert(static_cast<iLuxEnemy*>(e.mpEntity));
		return 0;
	case eOhplGa6Op_PlayerWarnTerror:
		Warning("Could not find entity with id %d\n", alA);
		return 0;
	case eOhplGa6Op_PlayerToBlock3:
		{
				mPlayerHands.ToPlayerHands(apMap, apPlayer->GetHands());

				//////////////////////
				///Collide callbacks
				cContainerListIterator<cLuxCollideCallback_SaveData> it = mlstCollideCallbacks.GetIterator();
				while(it.HasNext())
				{
					cLuxCollideCallback_SaveData& saveCallback = it.Next();
					cLuxCollideCallback *pCallback = hplNew(cLuxCollideCallback, ());

					saveCallback.ToCallback(apMap, apPlayer, pCallback);
					apPlayer->mlstCollideCallbacks.push_back(pCallback);
				}
		}
		return 0;
	}
	return 0;
}

void cLuxPlayer_SaveData::FromPlayer(cLuxPlayer *apPlayer)
{
	PlayerEnv env = { this, apPlayer, NULL, NULL, eLuxPlayerState_Normal, NULL };
	ohpl_ga6_player_from(&env, Ga6Tramp<PlayerEnv>);
}

//-----------------------------------------------------------------------

void cLuxPlayer_SaveData::ToPlayer(cLuxMap *apMap,cLuxPlayer *apPlayer)
{
	PlayerEnv env = { this, apPlayer, apMap, NULL, eLuxPlayerState_Normal, NULL };
	ohpl_ga6_player_to(&env, Ga6Tramp<PlayerEnv>);
}

//-----------------------------------------------------------------------

kBeginSerializeBase(cLuxFlashbackData_SaveData)

kSerializeVar(msFile,		eSerializeType_String)
kSerializeVar(msCallback,	eSerializeType_String)

kEndSerialize()


kBeginSerializeBase(cLuxPlayer_SaveData)

kSerializeVar(mlState, eSerializeType_Int32)
kSerializeVar(mpStateData, eSerializeType_ClassPointer)

kSerializeVar(mbActive, eSerializeType_Bool)

kSerializeVar( mbUsesDragFootsteps, eSerializeType_Bool )

kSerializeVar(mfHealth, eSerializeType_Float32)
kSerializeVar(mfSanity, eSerializeType_Float32)
kSerializeVar(mfLampOil, eSerializeType_Float32)
kSerializeVar(mfTerror, eSerializeType_Float32)
kSerializeVar(mlCoins, eSerializeType_Int32)
kSerializeVar(mlTinderboxes, eSerializeType_Int32)

kSerializeVar(mbIsInWater, eSerializeType_Bool)
kSerializeVar(msWaterStepSound, eSerializeType_String)
kSerializeVar(mfWaterSpeedMul, eSerializeType_Float32)

kSerializeVar(mbJumpDisabled, eSerializeType_Bool)
kSerializeVar(mbCrouchDisabled, eSerializeType_Bool)
kSerializeVar(mbSanityDrainDisabled, eSerializeType_Bool)

kSerializeVar(mbCrouching, eSerializeType_Bool)

kSerializeVar(mfInteractionMoveSpeedMul, eSerializeType_Float32)
kSerializeVar(mfEventMoveSpeedMul, eSerializeType_Float32)
kSerializeVar(mfEventRunSpeedMul, eSerializeType_Float32)
kSerializeVar(mfScriptMoveSpeedMul, eSerializeType_Float32)
kSerializeVar(mfScriptRunSpeedMul, eSerializeType_Float32)
kSerializeVar(mfLookSpeedMul, eSerializeType_Float32)
kSerializeVar(mfHurtMoveSpeedMul, eSerializeType_Float32)
kSerializeVar(mfInsanityCollapseSpeedMul, eSerializeType_Float32)

kSerializeVar(mfScriptJumpForceMul, eSerializeType_Float32)

kSerializeVar(mfAspectMul, eSerializeType_Float32)
kSerializeVar(mfFOVMul, eSerializeType_Float32)
kSerializeVar(mfAspectMulGoal, eSerializeType_Float32)
kSerializeVar(mfFOVMulGoal, eSerializeType_Float32)
kSerializeVar(mfAspectMulSpeed, eSerializeType_Float32)
kSerializeVar(mfFOVMulSpeed, eSerializeType_Float32)

kSerializeVar(mfRoll, eSerializeType_Float32)
kSerializeVar(mfRollGoal, eSerializeType_Float32)
kSerializeVar(mfRollSpeedMul, eSerializeType_Float32)
kSerializeVar(mfRollMaxSpeed, eSerializeType_Float32)

kSerializeVar(mbFadingPitch, eSerializeType_Bool )
kSerializeVar(mfPitchGoal, eSerializeType_Float32 )
kSerializeVar(mfPitchSpeedMul, eSerializeType_Float32 )
kSerializeVar(mfPitchMaxSpeed, eSerializeType_Float32 )

kSerializeVar(mvCamAnimPos, eSerializeType_Vector3f)
kSerializeVar(mvCamAnimPosGoal, eSerializeType_Vector3f)
kSerializeVar(mfCamAnimPosSpeedMul, eSerializeType_Float32)
kSerializeVar(mfCamAnimPosMaxSpeed, eSerializeType_Float32)

kSerializeVar(mvHeadSpinSpeed, eSerializeType_Vector2f)

kSerializeVar(mbNoFallDamage, eSerializeType_Bool)

kSerializeVar(mbScriptShowFocusIconAndCrossHair, eSerializeType_Bool)

kSerializeVar(mbLanternOn, eSerializeType_Bool)
kSerializeVar(mbLanternDisabled, eSerializeType_Bool)

kSerializeVar(msDeathHintCat, eSerializeType_String)
kSerializeVar(msDeathHintEntry, eSerializeType_String)

kSerializeVar(mbSpawnPSActive, eSerializeType_Bool)
kSerializeVar(mbSpawnPSFile, eSerializeType_String)

kSerializeVar(mbFlashbackActive, eSerializeType_Bool)
kSerializeVar(mfFlashbackStartCount, eSerializeType_Float32)
kSerializeVar(mfFlashDelay, eSerializeType_Float32)
kSerializeVar(msFlashbackFile, eSerializeType_String)
kSerializeVar(msFlashbackCallback, eSerializeType_String)
kSerializeClassContainer(mlstFlashbackQueue,cLuxFlashbackData_SaveData, eSerializeType_Class)

kSerializeVar(mbInsanityCollapse_Active, eSerializeType_Bool)
kSerializeVar(mlInsanityCollapse_State, eSerializeType_Int32)
kSerializeVar(mfInsanityCollapse_HeightAdd, eSerializeType_Float32)
kSerializeVar(mfInsanityCollapse_Roll, eSerializeType_Float32)
kSerializeVar(mfInsanityCollapse_T, eSerializeType_Float32)

kSerializeVar(mfSanity_AtLowSanityCount, eSerializeType_Float32)

kSerializeVar(mbLookAt_Active, eSerializeType_Bool)
kSerializeVar(mvLookAt_TargetPos, eSerializeType_Vector3f)
kSerializeVar(mvLookAt_CurrentSpeed, eSerializeType_Vector3f)
kSerializeVar(mfLookAt_SpeedMul, eSerializeType_Float32)
kSerializeVar(mfLookAt_MaxSpeed, eSerializeType_Float32)
kSerializeVar(msLookAt_AtTargetCallback, eSerializeType_String)
kSerializeVar(mfLookAt_DestFovMul, eSerializeType_Float32)
kSerializeVar(mfLookAt_Fov, eSerializeType_Float32)
kSerializeVar(mfLookAt_FovSpeed, eSerializeType_Float32)
kSerializeVar(mfLookAt_FovMaxSpeed, eSerializeType_Float32)
	
kSerializeVar(mfDarkness_Active, eSerializeType_Bool)

kSerializeVar(mCharBody, eSerializeType_Class)

kSerializeVar(mvCameraAngles, eSerializeType_Vector3f)

kSerializeVar(mfPitchMaxLimit, eSerializeType_Float32)
kSerializeVar(mfPitchMinLimit, eSerializeType_Float32)
kSerializeVar(mfYawMaxLimit, eSerializeType_Float32)
kSerializeVar(mfYawMinLimit, eSerializeType_Float32)

kSerializeClassContainer(mvHeadPosAdds, cLuxHeadPosAdd, eSerializeType_Class)

kSerializeVarContainer(mvTerrorEnemyIDs, eSerializeType_Int32)

kSerializeVar(mPlayerHands, eSerializeType_Class)

kSerializeClassContainer(mlstCollideCallbacks, cLuxCollideCallback_SaveData, eSerializeType_Class)

kEndSerialize()

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// SAVED GAME
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------
cLuxSaveGame_SaveData::cLuxSaveGame_SaveData()
{
	mpSavedMaps = NULL;
	mbHardmode = false;
}

//-----------------------------------------------------------------------

kBeginSerializeBase(cLuxSaveGame_SaveData)

////////////////////////
// HARDMODE

kSerializeVar(mbHardmode, eSerializeType_Bool)

////////////////////////

kSerializeVar(msMapFolder, eSerializeType_String)
kSerializeVar(mMap, eSerializeType_Class)
kSerializeVar(mInventory, eSerializeType_Class)
kSerializeVar(mJournal, eSerializeType_Class)
kSerializeVar(mPlayer, eSerializeType_Class)
kSerializeVar(mMusicHandler, eSerializeType_Class)
kSerializeVar(mEffectHandler, eSerializeType_Class)
kSerializeVar(mGlobalDataHandler, eSerializeType_Class)
kSerializeVar(mHintHandler, eSerializeType_Class)
kSerializeVar(mInsanityHandler, eSerializeType_Class)
kSerializeVar(mLoadScreenHandler, eSerializeType_Class)

kSerializeVar(mpSavedMaps, eSerializeType_ClassPointer)


kEndSerialize()

//-----------------------------------------------------------------------



