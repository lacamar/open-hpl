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

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	iResourceLoaderHandler::iResourceLoaderHandler() : mpLoaders(ohpl_loaders_new())
	{
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
		return GetLoaderForFile(cString::To8Char(asFileName));
	}

	//-----------------------------------------------------------------------

	iResourceLoader* iResourceLoaderHandler::GetLoaderForFile(const tString& asFileName)
	{
		tString sLowExt = cString::ToLowerCase(cString::GetFileExt(asFileName));

		iResourceLoader *pLoader = static_cast<iResourceLoader*>(ohpl_loaders_find(mpLoaders, sLowExt.data(), sLowExt.size()));
		if(pLoader) return pLoader;

		Error("No loader for file extension '%s' found!\n",sLowExt.c_str());
		return NULL;
	}

	//-----------------------------------------------------------------------

}
