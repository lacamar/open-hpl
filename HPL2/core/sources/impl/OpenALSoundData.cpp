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

#include "impl/OpenALSoundData.h"
#include "impl/OpenALSoundChannel.h"

#include "system/LowLevelSystem.h"
#include "system/String.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	cOpenALSoundData::cOpenALSoundData(const tString& asName, bool abStream) : iSoundData(asName,_W(""), abStream)
	{
		mpRust = NULL;
//		mpSoundData = NULL;
	}
	
	//-----------------------------------------------------------------------

	cOpenALSoundData::~cOpenALSoundData()
	{
		if(mpRust) ohpl_l5_data_free(mpRust);
	}
	
	//-----------------------------------------------------------------------

	bool cOpenALSoundData::CreateFromFile(const tWString &asFile)
	{
		SetFullPath(asFile);

		OhplL5InstallLog();
		if(mpRust) ohpl_l5_data_free(mpRust);
		mpRust = ohpl_l5_data_new(mbStream, mbLoopStream);
		return ohpl_l5_data_create(mpRust, (const uint32_t*)asFile.c_str(), asFile.size());
	}
	
	//-----------------------------------------------------------------------

	iSoundChannel* cOpenALSoundData::CreateChannel(int alPriority)
	{
		if(mpRust==NULL) return NULL;
		int lRustHandle = ohpl_l5_data_channel(mpRust, alPriority);
		if(lRustHandle==-1) return NULL;
		return hplNew( cOpenALSoundChannel, (this,lRustHandle, mpSoundManger) );
	}

	//-----------------------------------------------------------------------

	bool cOpenALSoundData::IsStereo()
	{
		return mpRust && ohpl_l5_data_is_stereo(mpRust);
	}

	//-----------------------------------------------------------------------

}
