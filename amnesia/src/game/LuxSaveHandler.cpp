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

#include "LuxSaveHandler.h"
#include "RustLuxSave.h"
#include <vector>

#include "LuxMapHandler.h"
#include "LuxMap.h"
#include "LuxEntity.h"
#include "LuxProp.h"
#include "LuxPlayer.h"
#include "LuxInventory.h"
#include "LuxJournal.h"
#include "LuxMusicHandler.h"
#include "LuxMessageHandler.h"
#include "LuxSavedGameTypes.h"
#include "LuxEffectHandler.h"
#include "LuxGlobalDataHandler.h"
#include "LuxHintHandler.h"
#include "LuxHelpFuncs.h"
#include "LuxInsanityHandler.h"
#include "LuxProgressLogHandler.h"
#include "LuxLoadScreenHandler.h"
#include "LuxSavedGame.h"

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

namespace
{

void Ga6Sink(void *apCtx, const unsigned *apData, size_t alSize)
{
	static_cast<tWString*>(apCtx)->assign((const wchar_t*)apData, alSize);
}

void Ga6Translate(void *apCtx, const unsigned *apData, size_t alSize, OhplGa6Sink32 apSink, void *apSinkCtx)
{
	tWString sToken((const wchar_t*)apData, alSize);
	tWString sOut = kTranslate("Levels", cString::To8Char(sToken));
	apSink(apSinkCtx, (const unsigned*)sOut.data(), sOut.size());
}

OhplGa6Date Ga6Date(const cDate &aDate)
{
	OhplGa6Date date = { aDate.seconds, aDate.minutes, aDate.hours, aDate.month_day, aDate.month, aDate.year };
	return date;
}

}

cLuxSaveHandlerThreadClass::cLuxSaveHandlerThreadClass()
{
	mpThread = NULL;
	mpSaveMutex = cPlatform::CreateMutEx();
}

cLuxSaveHandlerThreadClass::~cLuxSaveHandlerThreadClass()
{
	if(IsRunning())
	{
		mpThread->Stop();
		hplDelete(mpThread);
	}
	hplDelete(mpSaveMutex);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

bool cLuxSaveHandlerThreadClass::IsRunning()
{
	return mpThread!=NULL;
}

//-----------------------------------------------------------------------

void cLuxSaveHandlerThreadClass::SetUpThread()
{
	mpThread = cPlatform::CreateThread(this);
	mpThread->SetPriority(eThreadPrio_Low);
	mpThread->SetUpdateRate(1.0f);
	mpThread->Start();
}

//-----------------------------------------------------------------------

void cLuxSaveHandlerThreadClass::Save(cLuxSaveGame_SaveData* apSaveData, const tWString& asFile)
{
	if(apSaveData==NULL)
		return;

	mpSaveMutex->Lock();

	mvSaveData.push_back(apSaveData);
	mvSaveFileNames.push_back(asFile);

	mpSaveMutex->Unlock();
}

//-----------------------------------------------------------------------

void cLuxSaveHandlerThreadClass::ProcessPendingSaves()
{
	std::vector<cLuxSaveGame_SaveData*> vSaveDataCopy;
	std::vector<tWString> vSaveFileNamesCopy;

	mpSaveMutex->Lock();
	if(mvSaveData.empty()==false)
	{
		vSaveDataCopy = mvSaveData;
		vSaveFileNamesCopy = mvSaveFileNames;

		mvSaveData.clear();
		mvSaveFileNames.clear();
	}
	mpSaveMutex->Unlock();

	// Force a wait until all saves are processed
	iMutex* pMutex = gpBase->mpMapHandler->mpSavedGameMutex;
	pMutex->Lock();
	{
		for(int i=0;i<(int)vSaveDataCopy.size();++i)
		{
			cLuxSaveGame_SaveData* pData = vSaveDataCopy[i];
			const tWString& sFile = vSaveFileNamesCopy[i];

			//Need to set saved maps before saving!
			pData->mpSavedMaps = gpBase->mpMapHandler->GetSavedMapCollection();

			cSerializeClass::SaveToFile(pData,sFile,"SaveGame");

			hplDelete(pData);
		}
	}
	pMutex->Unlock();
}

//-----------------------------------------------------------------------

void cLuxSaveHandlerThreadClass::UpdateThread()
{
	ProcessPendingSaves();
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

namespace
{

struct SaveHost
{
	cLuxSaveHandler *mpHandler;
	cLuxSaveGame_SaveData *mpData = NULL;
	tWString msResult;
	tWStringVec mvFiles;
	tWString msFolder;
	bool mbModified = false;
	cLuxMap *mpOldMap = NULL;
	tString msOldMapFolder;
	cLuxModelCache mCache;
};

void CopyUnits(unsigned *apBuf, size_t alSize, const tWString &asStr)
{
	for(size_t i=0; i<alSize && i<asStr.size(); ++i) apBuf[i] = (unsigned)asStr[i];
}

void PutDate(unsigned *apBuf, const cDate &aDate)
{
	OhplGa6Date d = Ga6Date(aDate);
	const int v[6] = { d.seconds, d.minutes, d.hours, d.month_day, d.month, d.year };
	for(int i=0; i<6; ++i) apBuf[i] = (unsigned)v[i];
}

void SaveNow(cLuxSaveGame_SaveData *apData, const tWString &asFile)
{
	apData->mpSavedMaps = gpBase->mpMapHandler->GetSavedMapCollection();
	cSerializeClass::SaveToFile(apData,asFile,"SaveGame");
	hplDelete(apData);
}

long long LoadStep(SaveHost *h, long long alStep)
{
	cLuxMap *pCurrentMap = gpBase->mpMapHandler->GetCurrentMap();
	switch(alStep)
	{
	case eOhplGa6HStep_DrawMenuScreen: gpBase->mpLoadScreenHandler->DrawMenuScreen(); break;
	case eOhplGa6HStep_DestroyDataCache: gpBase->mpMapHandler->DestroyDataCache(); break;
	case eOhplGa6HStep_HasCurrentMap:
		h->mpOldMap = pCurrentMap;
		return pCurrentMap!=NULL;
	case eOhplGa6HStep_DestroyWorldEntities:
		gpBase->RunModuleMessage(eLuxUpdateableMessage_DestroyWorldEntities, pCurrentMap);
		break;
	case eOhplGa6HStep_ResetModules:
		gpBase->mpPlayer->Reset();
		gpBase->mpInventory->Reset();
		gpBase->mpJournal->Reset();
		gpBase->mpMusicHandler->Reset();
		gpBase->mpMessageHandler->Reset();
		gpBase->mpEffectHandler->Reset();
		gpBase->mpGlobalDataHandler->Reset();
		gpBase->mpHintHandler->Reset();
		gpBase->mpInsanityHandler->Reset();
		gpBase->mpLoadScreenHandler->Reset();
		gpBase->mpMapHandler->GetSavedMapCollection()->Reset();
		break;
	case eOhplGa6HStep_StopSounds:
		gpBase->mpEngine->GetSound()->GetSoundHandler()->StopAll(eSoundEntryType_World | eSoundEntryType_Gui);
		gpBase->mpEngine->GetSound()->GetMusicHandler()->Stop(100.0f);
		break;
	case eOhplGa6HStep_CacheCreate: h->mCache.Create(); break;
	case eOhplGa6HStep_SetMapFolder:
		h->msOldMapFolder = gpBase->mpMapHandler->GetMapFolder();
		gpBase->mpMapHandler->SetMapFolder(h->mpData->msMapFolder);
		break;
	case eOhplGa6HStep_LoadScripts:
		gpBase->mpInventory->LoadScript();
		gpBase->mpGlobalDataHandler->LoadScript();
		break;
	case eOhplGa6HStep_NeedsNewMap:
		return	h->mpOldMap==NULL ||
				h->msOldMapFolder != gpBase->mpMapHandler->GetMapFolder() ||
				h->mpOldMap->GetFileName() != h->mpData->mMap.msFileName;
	case eOhplGa6HStep_LoadNewMap:
	{
		cLuxMap *pNewMap = gpBase->mpMapHandler->LoadMap(h->mpData->mMap.msFileName, false);
		if(pNewMap==NULL) FatalError("Could not load quicksave map '%s'\n", h->mpData->mMap.msFileName.c_str());
		if(h->mpOldMap) gpBase->mpMapHandler->DestroyMap(h->mpOldMap, false);
		gpBase->mpMapHandler->SetCurrentMap(pNewMap, false, false,"");
		break;
	}
	case eOhplGa6HStep_ClearWorldDynamics:
		pCurrentMap->GetWorld()->DestroyAllSoundEntities();
		pCurrentMap->GetWorld()->DestroyAllParticleSystems();
		break;
	case eOhplGa6HStep_DestroyEntities: pCurrentMap->DestroyAllEntities(); break;
	case eOhplGa6HStep_KillDyingParticles:
	{
		cParticleSystemIterator partIt = pCurrentMap->GetWorld()->GetParticleSystemIterator();
		while(partIt.HasNext())
		{
			cParticleSystem *pPS = partIt.Next();
			if(pPS->IsDying()) pPS->KillInstantly();
		}
		break;
	}
	case eOhplGa6HStep_CreateWorldEntities:
		gpBase->RunModuleMessage(eLuxUpdateableMessage_CreateWorldEntities, pCurrentMap);
		break;
	case eOhplGa6HStep_ApplySave:
	{
		cLuxSaveGame_SaveData *apSave = h->mpData;
		apSave->mMap.ToMap(pCurrentMap);
		apSave->mHintHandler.ToHintHandler(pCurrentMap, gpBase->mpHintHandler);
		apSave->mPlayer.ToPlayer(pCurrentMap, gpBase->mpPlayer);
		apSave->mInventory.ToInventory(pCurrentMap, gpBase->mpInventory);
		apSave->mJournal.ToJournal(pCurrentMap, gpBase->mpJournal);
		apSave->mMusicHandler.ToMusicHandler(pCurrentMap, gpBase->mpMusicHandler);
		apSave->mEffectHandler.ToEffectHandler(pCurrentMap, gpBase->mpEffectHandler);
		apSave->mGlobalDataHandler.ToGlobalDataHandler(pCurrentMap, gpBase->mpGlobalDataHandler);
		apSave->mInsanityHandler.ToInsanityHandler(pCurrentMap, gpBase->mpInsanityHandler);
		apSave->mLoadScreenHandler.ToLoadScreenHandler(pCurrentMap, gpBase->mpLoadScreenHandler);
		break;
	}
	case eOhplGa6HStep_SavedMaps: gpBase->mpMapHandler->SetSavedMapCollection(h->mpData->mpSavedMaps); break;
	case eOhplGa6HStep_HardMode: gpBase->mbHardMode = h->mpData->mbHardmode; break;
	case eOhplGa6HStep_Cleanup: gpBase->mpHelpFuncs->CleanupData(); break;
	case eOhplGa6HStep_CacheDestroy: h->mCache.Destroy(); break;
	}
	return 0;
}

long long SaveHostCall(void *apCtx, unsigned alOp, long long alA, unsigned *apBuf, size_t alN)
{
	SaveHost *h = (SaveHost*)apCtx;
	switch(alOp)
	{
	case eOhplGa6H_Log:
	{
		tString s = cString::To8Char(tWString((const wchar_t*)apBuf, alN));
		switch(alA)
		{
		case eOhplGa6HLog_BeginSave: Log("-------- BEGIN SAVE TO: %s ---------\n", s.c_str()); break;
		case eOhplGa6HLog_EndSave: Log("-------- END SAVE ---------\n"); break;
		case eOhplGa6HLog_BeginLoad: Log("-------- BEGIN LOAD FROM %s ---------\n", s.c_str()); break;
		case eOhplGa6HLog_EndLoad: Log("-------- END LOAD ---------\n"); break;
		case eOhplGa6HLog_Loading: Log("LOADING SAVE GAME!\n"); break;
		case eOhplGa6HLog_NoSaveFile: Error("Could not find a save file in '%s'!\n", s.c_str()); break;
		case eOhplGa6HLog_NoHardmodeFile: Error("Could not find a hard mode save file in '%s'!\n", s.c_str()); break;
		}
		return 0;
	}
	case eOhplGa6H_HardMode: return gpBase->mbHardMode;
	case eOhplGa6H_MapDisplayName:
		h->msResult = cString::To16Char(gpBase->mpMapHandler->GetCurrentMap()->GetDisplayNameEntry());
		return (long long)h->msResult.size();
	case eOhplGa6H_CopyResult: CopyUnits(apBuf, alN, h->msResult); return 0;
	case eOhplGa6H_NowDate: PutDate(apBuf, cPlatform::GetDate()); return 0;
	case eOhplGa6H_ProfilePath:
		h->msResult = gpBase->msProfileSavePath;
		return (long long)h->msResult.size();
	case eOhplGa6H_ScanSaves:
	{
		tWStringList lstFiles;
		h->msFolder = gpBase->msProfileSavePath;
		cPlatform::FindFilesInDir(lstFiles, h->msFolder, _W("*.sav"));
		h->mvFiles.assign(lstFiles.begin(), lstFiles.end());
		h->mbModified = alA != 0;
		return (long long)h->mvFiles.size();
	}
	case eOhplGa6H_ScanName:
		h->msResult = h->mvFiles[alA];
		return (long long)h->msResult.size();
	case eOhplGa6H_ScanDate:
		PutDate(apBuf, h->mbModified ? cPlatform::FileModifiedDate(h->msFolder + h->mvFiles[alA])
									: cPlatform::FileCreationDate(h->msFolder + h->mvFiles[alA]));
		return 0;
	case eOhplGa6H_RemoveScanned: cPlatform::RemoveFile(h->msFolder + h->mvFiles[alA]); return 0;
	case eOhplGa6H_CreateData: return (long long)(intptr_t)h->mpHandler->CreateSaveGameData();
	case eOhplGa6H_ThreadRunning: return h->mpHandler->GetThreadClass()->IsRunning();
	case eOhplGa6H_ThreadSave:
		h->mpHandler->GetThreadClass()->Save((cLuxSaveGame_SaveData*)(intptr_t)alA, tWString((const wchar_t*)apBuf, alN));
		return 0;
	case eOhplGa6H_SaveNow: SaveNow((cLuxSaveGame_SaveData*)(intptr_t)alA, tWString((const wchar_t*)apBuf, alN)); return 0;
	case eOhplGa6H_Snapshot:
	{
		tWString sFile((const wchar_t*)apBuf, alN);
		tWString sFileExt = cString::GetFileExtW(sFile);
		tWString sFileName = cString::SubW(sFile,0, sFile.size()-(sFileExt.size()+1)) +  _W(".jpg");

		cEngine *pEngine = gpBase->mpEngine;

		cBitmap *pBmp = pEngine->GetGraphics()->GetLowLevel()->CopyFrameBufferToBitmap();
		pEngine->GetResources()->GetBitmapLoaderHandler()->SaveBitmap(pBmp,sFileName,0);
		hplDelete(pBmp);
		return 0;
	}
	case eOhplGa6H_NewData:
		h->mpData = hplNew(cLuxSaveGame_SaveData, ());
		return (long long)(intptr_t)h->mpData;
	case eOhplGa6H_LoadFile:
		cSerializeClass::LoadFromFile((cLuxSaveGame_SaveData*)(intptr_t)alA, tWString((const wchar_t*)apBuf, alN));
		return 0;
	case eOhplGa6H_FreeData: hplDelete((cLuxSaveGame_SaveData*)(intptr_t)alA); h->mpData = NULL; return 0;
	case eOhplGa6H_ThreadProcess: h->mpHandler->GetThreadClass()->ProcessPendingSaves(); return 0;
	case eOhplGa6H_ResetProgressLog: gpBase->mpProgressLogHandler->CreateAndResetLogFile(); return 0;
	case eOhplGa6H_Step: return LoadStep(h, alA);
	}
	return 0;
}

}

cLuxSaveHandler::cLuxSaveHandler() : iLuxUpdateable("LuxSaveHandler")
{
	mpRust = ohpl_ga6h_new(gpBase->mpGameCfg->GetInt("Saving","MaxAutoSaves",20));
}

cLuxSaveHandler::~cLuxSaveHandler()
{
	ohpl_ga6h_free(mpRust);
}

void cLuxSaveHandler::OnStart()
{
}

void cLuxSaveHandler::Update(float afTimeStep)
{
}

void cLuxSaveHandler::Reset()
{
}

void cLuxSaveHandler::SaveGameToFile(const tWString& asFile, bool abSaveSnapshot)
{
	SaveHost host = { this };
	ohpl_ga6h_save_to_file(mpRust, &host, SaveHostCall, (const unsigned*)asFile.data(), asFile.size(), abSaveSnapshot);
}

void cLuxSaveHandler::LoadGameFromFile(const tWString& asFile)
{
	SaveHost host = { this };
	ohpl_ga6h_load_from_file(mpRust, &host, SaveHostCall, (const unsigned*)asFile.data(), asFile.size());
}

bool cLuxSaveHandler::AutoSave() { return Run(eOhplGa6HRun_AutoSave); }
bool cLuxSaveHandler::AutoLoad(bool abResetProgressLogger) { return Run(abResetProgressLogger ? eOhplGa6HRun_AutoLoadReset : eOhplGa6HRun_AutoLoad); }
bool cLuxSaveHandler::SaveFileExists() { return Run(eOhplGa6HRun_FileExists); }
bool cLuxSaveHandler::HardModeSave() { return Run(eOhplGa6HRun_HardModeSave); }

bool cLuxSaveHandler::Run(unsigned alWhat)
{
	SaveHost host = { this };
	return ohpl_ga6h_run(mpRust, &host, SaveHostCall, alWhat);
}

cLuxSaveGame_SaveData *cLuxSaveHandler::CreateSaveGameData()
{
	cLuxSaveGame_SaveData *pSave = hplNew(cLuxSaveGame_SaveData, ());

	/////////////////////
	// HARDMODE

	pSave->mbHardmode = gpBase->mbHardMode;

	////////////////////

	pSave->msMapFolder = gpBase->mpMapHandler->GetMapFolder();

	pSave->mInventory.FromInventory(gpBase->mpInventory);
	pSave->mJournal.FromJournal(gpBase->mpJournal);
    pSave->mPlayer.FromPlayer(gpBase->mpPlayer);
	pSave->mMap.FromMap(gpBase->mpMapHandler->GetCurrentMap());
	pSave->mMusicHandler.FromMusicHandler(gpBase->mpMusicHandler);
	pSave->mEffectHandler.FromEffectHandler(gpBase->mpEffectHandler);
	pSave->mGlobalDataHandler.FromGlobalDataHandler(gpBase->mpGlobalDataHandler);
	pSave->mHintHandler.FromHintHandler(gpBase->mpHintHandler);
	pSave->mInsanityHandler.FromInsanityHandler(gpBase->mpInsanityHandler);
	pSave->mLoadScreenHandler.FromLoadScreenHandler(gpBase->mpLoadScreenHandler);

	return pSave;
}

//-----------------------------------------------------------------------

tWString cLuxSaveHandler::GetProperSaveName(const tWString &asFile)
{
	tWString sProperName;
	ohpl_ga6_proper_name((const unsigned*)asFile.data(), asFile.size(), Ga6Translate, NULL, Ga6Sink, &sProperName);
	return sProperName;
}

//-----------------------------------------------------------------------

