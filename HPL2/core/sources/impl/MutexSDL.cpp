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

#include "impl/MutexSDL.h"
#include "impl/RustBackends.h"
#include "impl/RustL1.h"

#if USE_SDL2
#include "SDL2/SDL.h"
#else
#include "SDL/SDL.h"
#endif

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	cMutexSDL::cMutexSDL()
	{
		mpMutexHandle = NULL;
		mpRust = OhplBackendRustRequested() ? ohpl_l1_mutex_new() : NULL;
		if(mpRust == NULL) mpMutexHandle = SDL_CreateMutex();
	}

	//-----------------------------------------------------------------------

	cMutexSDL::~cMutexSDL()
	{
		if(mpRust) ohpl_l1_mutex_free(mpRust);
		if(mpMutexHandle)
			SDL_DestroyMutex(mpMutexHandle);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	bool cMutexSDL::Lock()
	{
		if(mpRust) return ohpl_l1_mutex_lock(mpRust);
		return SDL_LockMutex(mpMutexHandle)!=-1;
	}
	
	bool cMutexSDL::Unlock()
	{
		if(mpRust) return ohpl_l1_mutex_unlock(mpRust);
		return SDL_UnlockMutex(mpMutexHandle)!=-1;
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------


	//-----------------------------------------------------------------------

}

