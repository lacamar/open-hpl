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

#include "LuxSavedMap.h"
#include "RustLuxSave.h"
#include <vector>

#include "LuxMapHandler.h"
#include "LuxMap.h"
#include "LuxEntity.h"

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxSavedMap::cLuxSavedMap()
{
	
}

//-----------------------------------------------------------------------

cLuxSavedMap::~cLuxSavedMap()
{
	cContainerListIterator<iLuxEntity_SaveData*> it = mlstEntities.GetIterator();
	while(it.HasNext())
	{
		iLuxEntity_SaveData *pSaveEntity = it.Next();
		hplDelete( pSaveEntity );
	}
	mlstEntities.Clear();
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

namespace
{

struct MapFromEnv
{
	cLuxSavedMap *mpData;
	cLuxMap *mpMap;
	cWorld *mpWorld;
	std::vector<iLight*> mvLights;
	std::vector<cSoundEntity*> mvSounds;
	std::vector<cParticleSystem*> mvPS;
	std::vector<iLuxEntity*> mvEntities;
};

}

int cLuxSavedMap::Ga6FromCall(void *apCtx, unsigned alOp, int alA, int alB)
{
	MapFromEnv &e = *static_cast<MapFromEnv*>(apCtx);
	cLuxSavedMap &d = *e.mpData;
	cLuxMap *apMap = e.mpMap;
	cWorld *pWorld = e.mpWorld;
	switch(alOp)
	{
	case eOhplGa6Op_MapFromHead:
		d.msFileName = apMap->GetFileName();
		d.msDisplayNameEntry = apMap->GetDisplayNameEntry();
		d.msLanternLitCallback = apMap->msLanternLitCallback;
		d.mlNumberOfQuests = apMap->mlNumberOfQuests;
		d.mlTotalCompletionAmount = apMap->mlTotalCompletionAmount;
		d.mlCurrentCompletionAmount = apMap->mlCurrentCompletionAmount;
		d.msCheckPointName = apMap->msCheckPointName;
		d.msCheckPointStartPos = apMap->msCheckPointStartPos;
		d.msCheckPointCallback = apMap->msCheckPointCallback;
		d.mlCheckPointCount = apMap->mlCheckPointCount;
		d.msCheckPointMusic = apMap->msCheckPointMusic;
		d.mlCheckPointMusicPrio = apMap->mlCheckPointMusicPrio;
		d.mbCheckPointMusicResume = apMap->mbCheckPointMusicResume;
		d.mfCheckPointMusicVolume = apMap->mfCheckPointMusicVolume;
		d.mbSkyBoxActive = pWorld->GetSkyBoxActive();
		d.msSkyboxTexture = pWorld->GetSkyBoxTexture() ? pWorld->GetSkyBoxTexture()->GetName() : "";
		d.mSkyBoxColor = pWorld->GetSkyBoxColor();
		d.mbFogActive = pWorld->GetFogActive();
		d.mbFogCulling = pWorld->GetFogCulling();
		d.mfFogStart = pWorld->GetFogStart();
		d.mfFogEnd = pWorld->GetFogEnd();
		d.mfFogFalloffExp = pWorld->GetFogFalloffExp();
		d.mFogColor = pWorld->GetFogColor();
		return 0;
	case eOhplGa6Op_MapLightN:
		{
			cLightListIterator lightIt = pWorld->GetLightIterator();
			while(lightIt.HasNext()) e.mvLights.push_back(lightIt.Next());
		}
		return (int)e.mvLights.size();
	case eOhplGa6Op_MapLightFlags:
		{
			iLight *pLight = e.mvLights[alA];
			return (pLight->IsSaved() ? eOhplGa6Flag_Saved : 0) |
				(pLight->GetEntityParent() == NULL ? eOhplGa6Flag_NoEntityParent : 0) |
				(pLight->GetParent() == NULL ? eOhplGa6Flag_NoParent : 0);
		}
	case eOhplGa6Op_MapLightFrom:
		{
			cEngineLight_SaveData saveLight{};
			saveLight.FromLight(e.mvLights[alA]);
			d.mlstLights.Add(saveLight);
		}
		return 0;
	case eOhplGa6Op_MapSoundN:
		{
			cSoundEntityIterator soundIt = pWorld->GetSoundEntityIterator();
			while(soundIt.HasNext()) e.mvSounds.push_back(soundIt.Next());
		}
		return (int)e.mvSounds.size();
	case eOhplGa6Op_MapSoundFlags:
		{
			cSoundEntity *pSound = e.mvSounds[alA];
			return (pSound->IsSaved() ? eOhplGa6Flag_Saved : 0) |
				(pSound->GetEntityParent() == NULL ? eOhplGa6Flag_NoEntityParent : 0) |
				(pSound->GetParent() == NULL ? eOhplGa6Flag_NoParent : 0);
		}
	case eOhplGa6Op_MapSoundLoop:
		return e.mvSounds[alA]->GetData()->GetLoop();
	case eOhplGa6Op_MapSoundFrom:
		{
			cEngineSound_SaveData saveSound{};
			saveSound.FromSound(e.mvSounds[alA]);
			d.mlstSounds.Add(saveSound);
		}
		return 0;
	case eOhplGa6Op_MapPsN:
		{
			cParticleSystemIterator psIt = pWorld->GetParticleSystemIterator();
			while(psIt.HasNext()) e.mvPS.push_back(psIt.Next());
		}
		return (int)e.mvPS.size();
	case eOhplGa6Op_MapPsFlags:
		{
			cParticleSystem *pPS = e.mvPS[alA];
			return (pPS->IsSaved() ? eOhplGa6Flag_Saved : 0) |
				(pPS->GetEntityParent() == NULL ? eOhplGa6Flag_NoEntityParent : 0) |
				(pPS->GetParent() == NULL ? eOhplGa6Flag_NoParent : 0);
		}
	case eOhplGa6Op_MapPsDying:
		return e.mvPS[alA]->IsDying();
	case eOhplGa6Op_MapPsFrom:
		{
			cEnginePS_SaveData savePS{};
			savePS.FromPS(e.mvPS[alA]);
			d.mlstPS.Add(savePS);
		}
		return 0;
	case eOhplGa6Op_MapEntN:
		{
			cLuxEntityIterator entityIt = apMap->GetEntityIterator();
			while(entityIt.HasNext()) e.mvEntities.push_back(entityIt.Next());
		}
		return (int)e.mvEntities.size();
	case eOhplGa6Op_MapEntSaved:
		return e.mvEntities[alA]->IsSaved();
	case eOhplGa6Op_MapEntDestroyMe:
		return e.mvEntities[alA]->GetDestroyMe();
	case eOhplGa6Op_MapEntFrom:
		{
			iLuxEntity *pEntity = e.mvEntities[alA];
			iLuxEntity_SaveData *pSaveData = pEntity->CreateSaveData();
			pEntity->SaveToSaveData(pSaveData);
			d.mlstEntities.Add(pSaveData);
		}
		return 0;
	case eOhplGa6Op_MapFromTail:
		{
			tLuxScriptVarMapIt it = apMap->m_mapVars.begin();
			for(; it != apMap->m_mapVars.end(); ++it)
			{
				d.mlstVars.Add(it->second);
			}
		}
		{
			tLuxEventTimerListIt it = apMap->mlstTimers.begin();
			for(; it != apMap->mlstTimers.end(); ++it)
			{
				cLuxEventTimer *pTimer = *it;
				d.mlstTimers.Add(*pTimer);
			}
		}
		{
			cRopeEntityIterator ropeIt = apMap->GetWorld()->GetRopeEntityIterator();
			while(ropeIt.HasNext())
			{
				cRopeEntity *pRope = ropeIt.Next();
				cEngineRope_SaveData saveRope{};
				saveRope.FromRope(pRope);
				d.mlstRopes.Add(saveRope);
			}
		}
		{
			tLuxUseItemCallbackListIt it = apMap->mlstUseItemCallbacks.begin();
			for(; it != apMap->mlstUseItemCallbacks.end(); ++it)
			{
				cLuxUseItemCallback *pCallback = *it;
				d.mvUseItemCallbacks.Add(*pCallback);
			}
		}
		return 0;
	}
	return 0;
}

namespace
{

struct MapToEnv
{
	cLuxSavedMap *mpData;
	cLuxMap *mpMap;
	cWorld *mpWorld;
	std::vector<cEngineLight_SaveData*> mvSavedLights;
	std::vector<cEngineSound_SaveData*> mvSavedSounds;
	std::vector<cEnginePS_SaveData*> mvSavedPS;
	std::vector<iLuxEntity_SaveData*> mvSavedEntities;
	std::vector<cEngineRope_SaveData*> mvSavedRopes;
	iLight *mpLight;
	cSoundEntity *mpSound;
	cParticleSystem *mpPS;
	iLuxEntity *mpEntity;
	cRopeEntity *mpRope;
	std::vector<iLuxEntity*> mvLoaded;
};

}

int cLuxSavedMap::Ga6ToCall(void *apCtx, unsigned alOp, int alA, int alB)
{
	MapToEnv &e = *static_cast<MapToEnv*>(apCtx);
	cLuxSavedMap &d = *e.mpData;
	cLuxMap *apMap = e.mpMap;
	cWorld *pWorld = e.mpWorld;
	switch(alOp)
	{
	case eOhplGa6Op_MapToHead:
		apMap->SetDisplayNameEntry(d.msDisplayNameEntry);
		apMap->msLanternLitCallback = d.msLanternLitCallback;
		apMap->mlNumberOfQuests = d.mlNumberOfQuests;
		apMap->mlTotalCompletionAmount = d.mlTotalCompletionAmount;
		apMap->mlCurrentCompletionAmount = d.mlCurrentCompletionAmount;
		apMap->msCheckPointName = d.msCheckPointName;
		apMap->msCheckPointStartPos = d.msCheckPointStartPos;
		apMap->msCheckPointCallback = d.msCheckPointCallback;
		apMap->mlCheckPointCount = d.mlCheckPointCount;
		apMap->msCheckPointMusic = d.msCheckPointMusic;
		apMap->mlCheckPointMusicPrio = d.mlCheckPointMusicPrio;
		apMap->mbCheckPointMusicResume = d.mbCheckPointMusicResume;
		apMap->mfCheckPointMusicVolume = d.mfCheckPointMusicVolume;
		pWorld->SetSkyBoxActive(d.mbSkyBoxActive);
		return 0;
	case eOhplGa6Op_MapSkyIsNone:
		return d.msSkyboxTexture == "";
	case eOhplGa6Op_MapSkyNull:
		pWorld->SetSkyBox(NULL, true);
		return 0;
	case eOhplGa6Op_MapSkyLoad:
		{
			iTexture *pTexture = gpBase->mpEngine->GetResources()->GetTextureManager()->CreateCubeMap(d.msSkyboxTexture, true);
			pWorld->SetSkyBox(pTexture, true);
		}
		return 0;
	case eOhplGa6Op_MapToFog:
		pWorld->SetSkyBoxColor(d.mSkyBoxColor);
		pWorld->SetFogActive(d.mbFogActive);
		pWorld->SetFogCulling(d.mbFogCulling);
		pWorld->SetFogStart(d.mfFogStart);
		pWorld->SetFogEnd(d.mfFogEnd);
		pWorld->SetFogFalloffExp(d.mfFogFalloffExp);
		pWorld->SetFogColor(d.mFogColor);
		return 0;
	case eOhplGa6Op_MapSavedLightN:
		{
			cContainerListIterator<cEngineLight_SaveData> it = d.mlstLights.GetIterator();
			while(it.HasNext()) e.mvSavedLights.push_back(&it.Next());
		}
		return (int)e.mvSavedLights.size();
	case eOhplGa6Op_MapFindLight:
		e.mpLight = pWorld->GetLightFromUniqueID(e.mvSavedLights[alA]->mlID);
		return e.mpLight != NULL;
	case eOhplGa6Op_MapLightTo:
		e.mvSavedLights[alA]->ToLight(e.mpLight);
		return 0;
	case eOhplGa6Op_MapLightWarn:
		Warning("Could not find saved light '%s'\n", e.mvSavedLights[alA]->msName.c_str());
		return 0;
	case eOhplGa6Op_MapSavedSoundN:
		{
			cContainerListIterator<cEngineSound_SaveData> it = d.mlstSounds.GetIterator();
			while(it.HasNext()) e.mvSavedSounds.push_back(&it.Next());
		}
		return (int)e.mvSavedSounds.size();
	case eOhplGa6Op_MapCreateSound:
		{
			cEngineSound_SaveData &saveSound = *e.mvSavedSounds[alA];
			e.mpSound = pWorld->CreateSoundEntity(saveSound.msName, saveSound.msSoundDataName, saveSound.mbRemoveWhenOver);
		}
		return e.mpSound != NULL;
	case eOhplGa6Op_MapSoundTo:
		e.mvSavedSounds[alA]->ToSound(e.mpSound);
		return 0;
	case eOhplGa6Op_MapSoundWarn:
		Warning("Could not create sound '%s'\n", e.mvSavedSounds[alA]->msSoundDataName.c_str());
		return 0;
	case eOhplGa6Op_MapSavedPsN:
		{
			cContainerListIterator<cEnginePS_SaveData> it = d.mlstPS.GetIterator();
			while(it.HasNext()) e.mvSavedPS.push_back(&it.Next());
		}
		return (int)e.mvSavedPS.size();
	case eOhplGa6Op_MapCreatePs:
		{
			cEnginePS_SaveData &savePS = *e.mvSavedPS[alA];
			e.mpPS = pWorld->CreateParticleSystem(savePS.msName, savePS.msType, 1);
		}
		return e.mpPS != NULL;
	case eOhplGa6Op_MapPsTo:
		e.mvSavedPS[alA]->ToPS(e.mpPS);
		return 0;
	case eOhplGa6Op_MapPsWarn:
		Warning("Could not create particle system '%s'\n", e.mvSavedPS[alA]->msType.c_str());
		return 0;
	case eOhplGa6Op_MapLoadBegin:
		gpBase->mpCurrentMapLoading = apMap;
		return 0;
	case eOhplGa6Op_MapLoadEnd:
		gpBase->mpCurrentMapLoading = NULL;
		return 0;
	case eOhplGa6Op_MapSavedEntN:
		{
			cContainerListIterator<iLuxEntity_SaveData*> it = d.mlstEntities.GetIterator();
			while(it.HasNext()) e.mvSavedEntities.push_back(it.Next());
		}
		return (int)e.mvSavedEntities.size();
	case eOhplGa6Op_MapCreateEntity:
		e.mpEntity = e.mvSavedEntities[alA]->CreateEntity(apMap);
		return e.mpEntity != NULL;
	case eOhplGa6Op_MapEntLoad:
		e.mpEntity->LoadFromSaveData(e.mvSavedEntities[alA]);
		e.mpEntity->SetSaveData(e.mvSavedEntities[alA]);
		e.mvLoaded.push_back(e.mpEntity);
		return 0;
	case eOhplGa6Op_MapSavedRopeN:
		{
			cContainerListIterator<cEngineRope_SaveData> it = d.mlstRopes.GetIterator();
			while(it.HasNext()) e.mvSavedRopes.push_back(&it.Next());
		}
		return (int)e.mvSavedRopes.size();
	case eOhplGa6Op_MapRopeCreate:
		e.mpRope = e.mvSavedRopes[alA]->CreateRope(apMap);
		return 0;
	case eOhplGa6Op_MapRopeTo:
		e.mvSavedRopes[alA]->ToRope(e.mpRope, apMap);
		return 0;
	case eOhplGa6Op_MapAfterLoad:
		apMap->AfterWorldLoadEntitySetup();
		return 0;
	case eOhplGa6Op_MapVarsTimers:
		{
			apMap->m_mapVars.clear();
			cContainerListIterator<cLuxScriptVar> it = d.mlstVars.GetIterator();
			while(it.HasNext())
			{
				cLuxScriptVar& scriptVar = it.Next();
				apMap->m_mapVars.insert(tLuxScriptVarMap::value_type(scriptVar.msName, scriptVar));
			}
		}
		{
			STLDeleteAll(apMap->mlstTimers);
			cContainerListIterator<cLuxEventTimer> it = d.mlstTimers.GetIterator();
			while(it.HasNext())
			{
				cLuxEventTimer& savedTimer = it.Next();
				cLuxEventTimer *pTimer = hplNew(cLuxEventTimer,());
				*pTimer = savedTimer;
				apMap->mlstTimers.push_back(pTimer);
			}
		}
		return 0;
	case eOhplGa6Op_MapLoadedN:
		return (int)e.mvLoaded.size();
	case eOhplGa6Op_MapSetupEntity:
		e.mvLoaded[alA]->SetupSaveData(e.mvLoaded[alA]->GetSaveData());
		return 0;
	case eOhplGa6Op_MapLoadedActive:
		return e.mvLoaded[alA]->IsActive();
	case eOhplGa6Op_MapLoadedUpdate:
		e.mvLoaded[alA]->UpdateLogic(0.001f);
		return 0;
	case eOhplGa6Op_MapUseItemCallbacks:
		{
			STLDeleteAll(apMap->mlstUseItemCallbacks);
			for(size_t i=0; i<d.mvUseItemCallbacks.Size(); ++i)
			{
				cLuxUseItemCallback *pCallback = hplNew(cLuxUseItemCallback, ());
				*pCallback = d.mvUseItemCallbacks[i];
				apMap->mlstUseItemCallbacks.push_back(pCallback);
			}
		}
		return 0;
	}
	return 0;
}

void cLuxSavedMap::FromMap(cLuxMap *apMap)
{
	MapFromEnv env = { this, apMap, apMap->GetWorld() };
	ohpl_ga6_map_from(&env, cLuxSavedMap::Ga6FromCall);
}

//-----------------------------------------------------------------------

void cLuxSavedMap::ToMap(cLuxMap *apMap)
{
	MapToEnv env = { this, apMap, apMap->GetWorld() };
	env.mpLight = NULL; env.mpSound = NULL; env.mpPS = NULL; env.mpEntity = NULL; env.mpRope = NULL;
	ohpl_ga6_map_to(&env, cLuxSavedMap::Ga6ToCall);
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// SERIALIZABLE
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

kBeginSerializeBase(cLuxSavedMap)
kSerializeVar(msFileName, eSerializeType_String)
kSerializeVar(msDisplayNameEntry, eSerializeType_String)

kSerializeVar(msLanternLitCallback, eSerializeType_String)

kSerializeVar(mlNumberOfQuests, eSerializeType_Int32)
kSerializeVar(mlTotalCompletionAmount, eSerializeType_Int32)
kSerializeVar(mlCurrentCompletionAmount, eSerializeType_Int32)

kSerializeVar(mbSkyBoxActive, eSerializeType_Bool)
kSerializeVar(msSkyboxTexture, eSerializeType_String)
kSerializeVar(mSkyBoxColor, eSerializeType_Color)

kSerializeVar(mbFogActive, eSerializeType_Bool)
kSerializeVar(mbFogCulling, eSerializeType_Bool)
kSerializeVar(mfFogStart, eSerializeType_Float32)
kSerializeVar(mfFogEnd, eSerializeType_Float32)
kSerializeVar(mfFogFalloffExp, eSerializeType_Float32)
kSerializeVar(mFogColor, eSerializeType_Color)

kSerializeVar(msCheckPointName, eSerializeType_String)
kSerializeVar(msCheckPointStartPos, eSerializeType_String)
kSerializeVar(msCheckPointCallback, eSerializeType_String)
kSerializeVar(mlCheckPointCount, eSerializeType_Int32)
kSerializeVar(msCheckPointMusic, eSerializeType_String)
kSerializeVar(mlCheckPointMusicPrio, eSerializeType_Int32)
kSerializeVar(mbCheckPointMusicResume, eSerializeType_Bool)
kSerializeVar(mfCheckPointMusicVolume, eSerializeType_Float32)

kSerializeClassContainer(mlstLights, cEngineLight_SaveData, eSerializeType_Class)
kSerializeClassContainer(mlstRopes, cEngineRope_SaveData, eSerializeType_Class)
kSerializeClassContainer(mlstSounds, cEngineSound_SaveData, eSerializeType_Class)
kSerializeClassContainer(mlstPS, cEnginePS_SaveData, eSerializeType_Class)

kSerializeClassContainer(mlstEntities,iLuxEntity_SaveData, eSerializeType_ClassPointer)

kSerializeClassContainer(mlstTimers, cLuxEventTimer,  eSerializeType_Class)
kSerializeClassContainer(mlstVars, cLuxScriptVar,  eSerializeType_Class)

kSerializeClassContainer(mvUseItemCallbacks,cLuxUseItemCallback, eSerializeType_Class)
kEndSerialize()


//-----------------------------------------------------------------------

