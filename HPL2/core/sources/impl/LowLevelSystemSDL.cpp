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

//#include <vld.h>
//Use this to check for memory leaks!


#define _UNICODE

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include <fstream>
#include <string>
#include <vector>

#include <unistd.h>

#include "impl/LowLevelSystemSDL.h"
#include "impl/SqScript.h"
#include "system/Platform.h"

#include "SDL2/SDL.h"


#include "system/String.h"
#include "impl/RustImpl.h"
#include "impl/RustBackends.h"
#include "impl/RustL1.h"
#include "impl/RustL7.h"

#ifdef __linux__
#include <string.h>
#endif

namespace hpl {

	//-----------------------------------------------------------------------
	
	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cLowLevelSystemSDL::cLowLevelSystemSDL()
	{
		mpRustEngine = ohpl_l7_engine_new();
		if(ohpl_l7_engine_raw(mpRustEngine)==NULL)
		{
			Error("Failed to start angel script!\n");
		}

		mlHandleCount = 0;

		Log("-------- THE HPL ENGINE LOG ------------\n");
		//Log("Engine build ID %s\n\n", 
		//	GetBuildID_HPL2_0());

		//const char* pASLibOptions = asGetLibraryOptions();
		//Log(" AngelScript options: '%s'\n", pASLibOptions);
	}

	//-----------------------------------------------------------------------

	cLowLevelSystemSDL::~cLowLevelSystemSDL()
	{
		/*Release all runnings contexts */
		
		ohpl_l7_engine_free(mpRustEngine);

		//perhaps not the best thing to skip :)
		//if(gpLogWriter)	hplDelete(gpLogWriter);
		//gpLogWriter = NULL;
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////
	
	//-----------------------------------------------------------------------

	
	iScript* cLowLevelSystemSDL::CreateScript(const tString& asName)
	{
		return hplNew( cSqScript, (asName,mlHandleCount++,mpRustEngine) );
	}

	//-----------------------------------------------------------------------

	bool cLowLevelSystemSDL::AddScriptFunc(const tString& asFuncDecl, void* pFunc)
	{
		if(!ohpl_l7_add_func(mpRustEngine, asFuncDecl.c_str(), pFunc))
		{
			Error("Couldn't add func '%s'\n",asFuncDecl.c_str());
			return false;
		}

		return true;
	}
	
	//-----------------------------------------------------------------------

	bool cLowLevelSystemSDL::AddScriptVar(const tString& asVarDecl, void *pVar)
	{
		if(!ohpl_l7_add_var(mpRustEngine, asVarDecl.c_str(), pVar))
		{
			Error("Couldn't add var '%s'\n",asVarDecl.c_str());
			return false;
		}

		return true;
	}
	
	//-----------------------------------------------------------------------
	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////
	
	//-----------------------------------------------------------------------

	//-----------------------------------------------------------------------
}
