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

#include "impl/OcclusionQueryOGL.h"
#include "impl/LowLevelGraphicsSDL.h"

#include <GL/glew.h>


namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cOcclusionQueryOGL::cOcclusionQueryOGL()
	{
		ohpl_glq_create(&mRs);
		mlQueryId = mRs.id;
		mlLastSampleCount = 0;
		mbResultsAreFetched = true;
	}

	//-----------------------------------------------------------------------

	cOcclusionQueryOGL::~cOcclusionQueryOGL()
	{
		ohpl_glq_destroy(&mRs);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cOcclusionQueryOGL::Begin()
	{
		ohpl_glq_begin(&mRs);
		mbResultsAreFetched = false;
	}

	void cOcclusionQueryOGL::End()
	{
		ohpl_glq_end(&mRs);
	}

	bool cOcclusionQueryOGL::FetchResults()
	{
		bool bDone = ohpl_glq_fetch(&mRs);
		mbResultsAreFetched = mRs.fetched;
		mlLastSampleCount = mRs.lastSamples;
		return bDone;
	}

	unsigned int cOcclusionQueryOGL::GetSampleCount()
	{
		return mlLastSampleCount;
	}

	//-----------------------------------------------------------------------
}
