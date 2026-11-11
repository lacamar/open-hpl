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

cLuxSaveHandler::cLuxSaveHandler() : iLuxUpdateable("LuxSaveHandler")
{
	mbInitialized = false;
	mbStartThread = false;

	mlMaxAutoSaves =  gpBase->mpGameCfg->GetInt("Saving","MaxAutoSaves",20);
	mlSaveNameCount =0;
}

//-----------------------------------------------------------------------

cLuxSaveHandler::~cLuxSaveHandler()
{
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxSaveHandler::OnStart()
{

}

//-----------------------------------------------------------------------

void cLuxSaveHandler::Update(float afTimeStep)
{
}

//-----------------------------------------------------------------------

void cLuxSaveHandler::Reset()
{
	if(mbInitialized==false)
	{
		mbInitialized = true;
		if(mbStartThread) mSaveHandlerThreadClass.SetUpThread();
	}
}

//-----------------------------------------------------------------------

void cLuxSaveHandler::SaveGameToFile(const tWString& asFile, bool abSaveSnapshot)
{
	Log("-------- BEGIN SAVE TO: %s ---------\n", cString::To8Char(asFile).c_str());

	cLuxSaveGame_SaveData* pData = CreateSaveGameData();

	if(mSaveHandlerThreadClass.IsRunning())
		mSaveHandlerThreadClass.Save(pData, asFile);
	else
	{
		pData->mpSavedMaps = gpBase->mpMapHandler->GetSavedMapCollection();
		cSerializeClass::SaveToFile(pData,asFile,"SaveGame");
		hplDelete(pData);
	}


	// Save snapshot?
	if(abSaveSnapshot)
	{
		tWString sFileExt = cString::GetFileExtW(asFile);
		tWString sFileName = cString::SubW(asFile,0, asFile.size()-(sFileExt.size()+1)) +  _W(".jpg");
		
		cEngine *pEngine = gpBase->mpEngine;
		
		cBitmap *pBmp = pEngine->GetGraphics()->GetLowLevel()->CopyFrameBufferToBitmap();
		pEngine->GetResources()->GetBitmapLoaderHandler()->SaveBitmap(pBmp,sFileName,0);
		hplDelete(pBmp);
	}

	Log("-------- END SAVE ---------\n");

}

//-----------------------------------------------------------------------

void cLuxSaveHandler::LoadGameFromFile(const tWString& asFile)
{
	Log("-------- BEGIN LOAD FROM %s ---------\n", cString::To8Char(asFile).c_str());

	cLuxSaveGame_SaveData * pSaveGame = hplNew(cLuxSaveGame_SaveData, ());

	//cSerializeClass::SetLog(true);
	cSerializeClass::LoadFromFile(pSaveGame,asFile);
	//cSerializeClass::SetLog(false);
	
	LoadSaveGameData(pSaveGame);

	hplDelete(pSaveGame);

	Log("-------- END LOAD ---------\n");
}

//-----------------------------------------------------------------------

bool cLuxSaveHandler::AutoSave()
{
	//////////////////////
	// HARDMODE
	if (gpBase->mbHardMode)
		return true;


	DeleteOldestSaveFiles(gpBase->msProfileSavePath, mlMaxAutoSaves);
	
	SaveGameToFile(gpBase->msProfileSavePath+GetSaveName(_W("AutoSave")));

	return true;
}


//-----------------------------------------------------------------------

bool cLuxSaveHandler::AutoLoad(bool abResetProgressLogger)
{
	// Wait until any pending save is done.
	if(mSaveHandlerThreadClass.IsRunning())
		mSaveHandlerThreadClass.ProcessPendingSaves();
	
	//Get newest file (if any!)
	tWString sFile = GetNewestSaveFile(gpBase->msProfileSavePath);
	if(sFile == _W(""))
	{
		Error("Could not find a save file in '%s'!\n", cString::To8Char(gpBase->msProfileSavePath).c_str());
		return false;
	}
	
	////////////////////////////////
	// HARDMODE
	if (gpBase->mbHardMode)
	{
		if (!ohpl_ga6_hardmode_file_ok((const unsigned*)sFile.data(), sFile.size()))
		{
			Error("Could not find a hard mode save file in '%s'!\n", cString::To8Char(gpBase->msProfileSavePath).c_str());
			return false;
		}
	}


	//Reset the progress logger.
	if(abResetProgressLogger)
	{
		gpBase->mpProgressLogHandler->CreateAndResetLogFile();
	}


	LoadGameFromFile(gpBase->msProfileSavePath+sFile);
		
	return true;
}

//-----------------------------------------------------------------------

bool cLuxSaveHandler::SaveFileExists()
{
	if(gpBase->msProfileSavePath==_W("")) return false;

	return GetNewestSaveFile(gpBase->msProfileSavePath)!=_W("");
}

//-----------------------------------------------------------------------

//////////////////////
// HARDMODE
bool cLuxSaveHandler::HardModeSave()
{
	SaveGameToFile(gpBase->msProfileSavePath + GetSaveName(_W("HardMode")));
	return true;
}

//-----------------------------------------------------------------------

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

void cLuxSaveHandler::LoadSaveGameData(cLuxSaveGame_SaveData *apSave)
{
	Log("LOADING SAVE GAME!\n");

	cLuxMap *pOldMap = NULL;
	cLuxMap *pCurrentMap = gpBase->mpMapHandler->GetCurrentMap();

	//////////////////////////////////////
	// Draw loading screen
	gpBase->mpLoadScreenHandler->DrawMenuScreen();

	//////////////////////////////////////
	// Destroy cache data
	gpBase->mpMapHandler->DestroyDataCache();

	///////////////////
	// Reset stuff (A better way to do this? Some weak reset that can be broadcasted?)
	if(pCurrentMap)
	{
		gpBase->RunModuleMessage(eLuxUpdateableMessage_DestroyWorldEntities, pCurrentMap);

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

		gpBase->mpEngine->GetSound()->GetSoundHandler()->StopAll(eSoundEntryType_World | eSoundEntryType_Gui);
		gpBase->mpEngine->GetSound()->GetMusicHandler()->Stop(100.0f);
	}


	///////////////////
	// Save cache
	cLuxModelCache cache;
	cache.Create();

	///////////////////
	// Setup variables
	tString msOldMapFolder = gpBase->mpMapHandler->GetMapFolder();
    gpBase->mpMapHandler->SetMapFolder(apSave->msMapFolder);

	///////////////////
	// Load script files

	gpBase->mpInventory->LoadScript();
	gpBase->mpGlobalDataHandler->LoadScript();
	
	///////////////////
	// Load new map
	if(	pCurrentMap==NULL || 
		msOldMapFolder != gpBase->mpMapHandler->GetMapFolder() || 
		pCurrentMap->GetFileName() != apSave->mMap.msFileName)
	{
		cLuxMap *pNewMap = gpBase->mpMapHandler->LoadMap(apSave->mMap.msFileName, false);
		if(pNewMap==NULL) FatalError("Could not load quicksave map '%s'\n", apSave->mMap.msFileName.c_str());
	
		// Destroy old and set new
		if(pCurrentMap) gpBase->mpMapHandler->DestroyMap(pCurrentMap, false);
		gpBase->mpMapHandler->SetCurrentMap(pNewMap, false, false,"");

		pCurrentMap = pNewMap;

		pCurrentMap->GetWorld()->DestroyAllSoundEntities();
		pCurrentMap->GetWorld()->DestroyAllParticleSystems();
	}
	/////////////////////
	//Clear current
	else
	{
		pCurrentMap->GetWorld()->DestroyAllSoundEntities();
		pCurrentMap->GetWorld()->DestroyAllParticleSystems();

		pCurrentMap->DestroyAllEntities();
		
		//Destroy all to be destroyed particle systems
		cParticleSystemIterator partIt = pCurrentMap->GetWorld()->GetParticleSystemIterator();
		while(partIt.HasNext())
		{
			cParticleSystem *pPS = partIt.Next();
            if(pPS->IsDying()) pPS->KillInstantly();		
		}

		//TODO: Destroy all special in player (???)
		
		// Create map data
		gpBase->RunModuleMessage(eLuxUpdateableMessage_CreateWorldEntities, pCurrentMap);
	}
	
	///////////////////
	// Load save data
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
							 
	///////////////////
	// Load saved maps
	gpBase->mpMapHandler->SetSavedMapCollection(apSave->mpSavedMaps);
	
	///////////////////
	//Hardmode
	gpBase->mbHardMode = apSave->mbHardmode;

	//////////////////////////////////
	//Clean up
	gpBase->mpHelpFuncs->CleanupData();
	
	///////////////////
	// Destroy cache
	cache.Destroy();

}

//-----------------------------------------------------------------------

tWString cLuxSaveHandler::GetProperSaveName(const tWString &asFile)
{
	tWString sProperName;
	ohpl_ga6_proper_name((const unsigned*)asFile.data(), asFile.size(), Ga6Translate, NULL, Ga6Sink, &sProperName);
	return sProperName;
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

tWString cLuxSaveHandler::GetSaveName(const tWString &asPrefix)
{
	cLuxMap *pCurrentMap = gpBase->mpMapHandler->GetCurrentMap();
	tString sMapName = pCurrentMap->GetDisplayNameEntry();
	cDate currentDate = cPlatform::GetDate();

	tWString sMap = cString::To16Char(sMapName);
	OhplGa6NameState state = { mlSaveNameCount, Ga6Date(mLatestSaveDate) };
	OhplGa6Date date = Ga6Date(currentDate);
	tWString sFileName;
	ohpl_ga6_save_name(&state, (const unsigned*)asPrefix.data(), asPrefix.size(), (const unsigned*)sMap.data(), sMap.size(), &date, Ga6Sink, &sFileName);

	mlSaveNameCount = state.count;
	mLatestSaveDate = currentDate;

	return sFileName;
}

void cLuxSaveHandler::DeleteOldestSaveFiles(const tWString &asFolder, int alMax)
{
	tWStringList lstFiles;
	cPlatform::FindFilesInDir(lstFiles, asFolder, _W("*.sav"));

	std::vector<tWString> vFiles(lstFiles.begin(), lstFiles.end());
	std::vector<OhplGa6Date> vDates;
	for(size_t i=0; i<vFiles.size(); ++i)
	{
		vDates.push_back(Ga6Date(cPlatform::FileCreationDate(asFolder + vFiles[i])));
	}

	std::vector<int> vVictims(vFiles.size() + 1);
	size_t lCount = ohpl_ga6_rotation_victims(vDates.data(), vDates.size(), alMax, vVictims.data());
	for(size_t i=0; i<lCount; ++i)
	{
		cPlatform::RemoveFile(asFolder + vFiles[vVictims[i]]);
	}
}

tWString cLuxSaveHandler::GetNewestSaveFile(const tWString &asFolder)
{
	tWStringList lstFiles;
	cPlatform::FindFilesInDir(lstFiles, asFolder, _W("*.sav"));

	if(lstFiles.empty()) return _W("");

	std::vector<tWString> vFiles(lstFiles.begin(), lstFiles.end());
	std::vector<OhplGa6Date> vDates;
	for(size_t i=0; i<vFiles.size(); ++i)
	{
		vDates.push_back(Ga6Date(cPlatform::FileModifiedDate(asFolder + vFiles[i])));
	}

	int lNewest = ohpl_ga6_newest(vDates.data(), vDates.size());
	return lNewest < 0 ? _W("") : vFiles[lNewest];
}

