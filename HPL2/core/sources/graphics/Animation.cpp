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

#include "graphics/Animation.h"

#include "math/Math.h"
#include "graphics/AnimationTrack.h"
#include "impl/RustBackends.h"
#include "graphics/RustGraphicsH6.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cAnimation::cAnimation(const tString &asName, const tWString &asFullPath, const tString &asFile) : iResourceBase(asName, asFullPath,0)
	{
		msAnimName = "";
		msFileName = asFile;
		mpRs = OhplBackendRustRequested() ? ohpl_h6_anim_new() : NULL;
	}

	//-----------------------------------------------------------------------

	cAnimation::~cAnimation()
	{
		STLDeleteAll(mvTracks);
		if(mpRs) ohpl_h6_anim_free(mpRs);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	float cAnimation::GetLength()
	{
		return mfLength;
	}

	//-----------------------------------------------------------------------

	void cAnimation::SetLength(float afTime)
	{
		mfLength = afTime;
	}

	//-----------------------------------------------------------------------

	cAnimationTrack* cAnimation::CreateTrack(const tString &asName, tAnimTransformFlag aFlags)
	{
		cAnimationTrack *pTrack = hplNew( cAnimationTrack,(asName, aFlags, this) );

		mvTracks.push_back(pTrack);
		if(mpRs) ohpl_h6_anim_add_track(mpRs, asName.data(), asName.size());
		return pTrack;
	}
	
	//-----------------------------------------------------------------------

	cAnimationTrack* cAnimation::GetTrack(int alIndex)
	{
		return mvTracks[alIndex];
	}

	//-----------------------------------------------------------------------

	cAnimationTrack* cAnimation::GetTrackByName(const tString &asName)
	{
		if(mpRs)
		{
			int lIdx = ohpl_h6_anim_track_by_name(mpRs, asName.data(), asName.size());
			return lIdx < 0 ? NULL : mvTracks[lIdx];
		}
		for(size_t i=0; i< mvTracks.size(); ++i)
		{
			if(asName == tString(mvTracks[i]->GetName()))
			{
				return mvTracks[i];
			}
		}

		return NULL;
	}
	
	//-----------------------------------------------------------------------
	
	void cAnimation::ReserveTrackNum(int alNum)
	{
		mvTracks.reserve(alNum);
	}
	
	//-----------------------------------------------------------------------

	int cAnimation::GetTrackNum()
	{
		return (int)mvTracks.size();
	}

	//-----------------------------------------------------------------------

	void cAnimation::SmoothAllTracks(float afAmount, float afPow, int alSamples,bool abTranslation, bool abRotation)
	{
		for(size_t i=0; i< mvTracks.size(); ++i)
		{
			mvTracks[i]->Smooth(afAmount, afPow, alSamples, abTranslation, abRotation);
		}
	}

	//-----------------------------------------------------------------------


	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	//-----------------------------------------------------------------------
}
