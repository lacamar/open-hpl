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

#include "resources/ResourceLoaderHandler.h"

#include "resources/ResourceLoader.h"
#include "system/String.h"
#include "system/LowLevelSystem.h"
#include "impl/RustBackends.h"
#include <cstring>

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	iResourceLoaderHandler::iResourceLoaderHandler() : mbRust(OhplBackendRustRequested()), mpLoaders(ohpl_loaders_new())
	{
		memset(&mHOps, 0, sizeof(mHOps));
		mHOps.ctx = this;
		mHOps.log = [](void*, int32_t, const uint8_t *apMsg, size_t alLen){ Error("%s", tString(reinterpret_cast<const char*>(apMsg), alLen).c_str()); };
		mHOps.call = [](void *c, int32_t alWhat, void *apLoader, OhplH3WStr aFile, int64_t alArg, void *apArg) -> void*
		{
			return static_cast<iResourceLoaderHandler*>(c)->H4Call(alWhat, apLoader, aFile, alArg, apArg);
		};
	}

	iResourceLoaderHandler::~iResourceLoaderHandler()
	{
		for(size_t i=0; i<ohpl_loaders_len(mpLoaders); ++i)
		{
			iResourceLoader *pLoader = static_cast<iResourceLoader*>(ohpl_loaders_at(mpLoaders, i));
			hplDelete(pLoader);
		}
		ohpl_loaders_free(mpLoaders);
	}
	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	void iResourceLoaderHandler::AddLoader(iResourceLoader *apLoader)
	{
		ohpl_loaders_add(mpLoaders, apLoader, apLoader->mpExtensions);

		//call implemented method to set it up.
		SetupLoader(apLoader);

		//Add all supported types from the loader
		for(size_t i=0; i<ohpl_exts_len(apLoader->mpExtensions); ++i)
		{
			size_t lLen;
			const char *pExt = ohpl_exts_at(apLoader->mpExtensions, i, &lLen);
			mvSupportedTypes.push_back(tString(pExt, lLen));
		}
	}

	
	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PROTECTED METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	iResourceLoader* iResourceLoaderHandler::GetLoaderForFile(const tWString& asFileName)
	{
		if(mbRust) return static_cast<iResourceLoader*>(ohpl_h4_handler_find(mpLoaders, &mHOps, reinterpret_cast<const uint8_t*>(cString::To8Char(asFileName).data()), cString::To8Char(asFileName).size()));
		return GetLoaderForFile(cString::To8Char(asFileName));
	}

	//-----------------------------------------------------------------------

	iResourceLoader* iResourceLoaderHandler::GetLoaderForFile(const tString& asFileName)
	{
		if(mbRust) return static_cast<iResourceLoader*>(ohpl_h4_handler_find(mpLoaders, &mHOps, RM_STR(asFileName)));
		tString sLowExt = cString::ToLowerCase(cString::GetFileExt(asFileName));

		iResourceLoader *pLoader = static_cast<iResourceLoader*>(ohpl_loaders_find(mpLoaders, sLowExt.data(), sLowExt.size()));
		if(pLoader) return pLoader;

		Error("No loader for file extension '%s' found!\n",sLowExt.c_str());
		return NULL;
	}

	//-----------------------------------------------------------------------

}
