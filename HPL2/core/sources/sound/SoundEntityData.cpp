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

#include "sound/SoundEntityData.h"

#include "system/LowLevelSystem.h"
#include "resources/Resources.h"
#include "system/String.h"
#include "system/Platform.h"
#include "math/Math.h"

#include "sound/Sound.h"
#include "sound/SoundHandler.h"
#include "sound/SoundChannel.h"

#include "resources/Resources.h"
#include "resources/SoundManager.h"
#include "resources/LowLevelResources.h"
#include "sound/RustSound.h"


namespace hpl {

	bool cSoundEntityData::mbLazyLoad = false;

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cSoundEntityData::cSoundEntityData(const tString& asName, cResources *apResources, cSound *apSound) : iResourceBase(asName, _W(""),0)
	{
		mpResources = apResources;
		mpSound = apSound;
		
		mbFadeStart = false;
		mbFadeStop = false;

		mfVolume = 1;
		mfMaxDistance =0;
		mfMinDistance=0;

		mbStream  = false;
		mbLoop = false;
		mbUse3D = true;

		mfRandom = 1;
		mfInterval =0;

		for(int i=0; i<3; i++)
		{
			mlPrevious[i] = -1;
		}
	}

	//-----------------------------------------------------------------------

	cSoundEntityData::~cSoundEntityData()
	{

	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	static tString gsEmptyString = "";

	const tString& cSoundEntityData::GetSoundName(eSoundEntityType aType, int alIdx)
	{
		int lIdx = ohpl_snd_name_index((int)mvSoundNameVecs[aType].size(), alIdx);
		if(lIdx < 0) return gsEmptyString;

		return mvSoundNameVecs[aType][lIdx];
	}

	const tString& cSoundEntityData::GetRandomSoundName(eSoundEntityType aType, bool abSkipPrevious)
	{
		int lRand = ohpl_snd_random_index((int)mvSoundNameVecs[aType].size(), &mlPrevious[aType], abSkipPrevious);
		if(lRand < 0) return gsEmptyString;

		return mvSoundNameVecs[aType][lRand];
	}

	//-----------------------------------------------------------------------

	int cSoundEntityData::GetSoundNum(eSoundEntityType aType)
	{
		return (int) mvSoundNameVecs[aType].size();
	}
	
	//-----------------------------------------------------------------------

	void cSoundEntityData::SetSoundName(const tString& asName, eSoundEntityType aType, int alIdx)
	{
		int lIdx = ohpl_snd_name_index((int)mvSoundNameVecs[aType].size(), alIdx);
		if(lIdx < 0) return;

		mvSoundNameVecs[aType][lIdx] = asName;
	}

	void cSoundEntityData::AddSoundName(const tString& asName, eSoundEntityType aType)
	{
		mvSoundNameVecs[aType].push_back(asName);			
	}


	//-----------------------------------------------------------------------

	void cSoundEntityData::PreloadSoundsOfType(eSoundEntityType aType)
	{
		for(size_t i=0; i<mvSoundNameVecs[aType].size(); ++i)
		{
			tString& sName = mvSoundNameVecs[aType][i];

			//No need to remove pointer as this is done when creating a channel!
			mpResources->GetSoundManager()->CreateSoundData(sName, false);
		}
	}

	void cSoundEntityData::PreloadSounds()
	{
		if(mbStream || mbLazyLoad) return;
		
		for(int i=0; i<3; ++i) PreloadSoundsOfType( (eSoundEntityType)i );
	}

	//-----------------------------------------------------------------------

	static void AddSoundNameCallback(void *apCtx, uint32_t alType, const unsigned char *asName, size_t alLen)
	{
		static_cast<tStringVec*>(apCtx)[alType].push_back(tString((const char*)asName, alLen));
	}

	bool cSoundEntityData::CreateFromFile(const tWString &asFile)
	{
		SetFullPath(asFile);

		FILE *pFile = cPlatform::OpenFile(asFile, _W("rb"));
		OhplXmlDocPtr doc;
		bool bLoaded = false;
		if(pFile)
		{
			bLoaded = OhplXmlLoadFile(doc.p, pFile);
			fclose(pFile);
		}

		OhplSntSink sink = { mvSoundNameVecs, AddSoundNameCallback };
		OhplSoundProps props;
		int lResult = ohpl_snt_load(doc.p, bLoaded, &sink, &props);

		if(lResult == 1 || lResult == 2)
		{
			int lRow=0, lCol=0;
			tString sDesc;
			if(lResult == 1) ohpl_xml_doc_error_info(doc.p, &lRow, &lCol, OhplXmlAssign, &sDesc);
			else sDesc = "No root element";
			Log("Failed parsing of XML document %s in line %d, column %d: %s\n", cString::To8Char(asFile).c_str(), lRow, lCol, sDesc.c_str());
			Error("Couldn't load sound entity data '%s'!\n",cString::To8Char(asFile).c_str());
			return false;
		}
		if(lResult == 3)
		{
			Error("Couldn't find SOUNDS element in '%s'!\n",asFile.c_str());
			return false;
		}
		if(lResult == 4)
		{
			Error("Couldn't find PROPERTIES element in '%s'!\n",asFile.c_str());
			return false;
		}

		mbUse3D = props.use_3d;
		mbLoop = props.looping;
		mbStream = props.stream;
		mbBlockable = props.blockable;
		mfBlockVolumeMul = props.block_volume_mul;
		mfVolume = props.volume;
		mfMaxDistance = props.max_distance;
		mfMinDistance = props.min_distance;
		mbFadeStart = props.fade_start;
		mbFadeStop = props.fade_stop;
		mfRandom = props.random;
		mfInterval = props.interval;
		mlPriority = props.priority;

        return true;
	}

	
	//-----------------------------------------------------------------------

}
