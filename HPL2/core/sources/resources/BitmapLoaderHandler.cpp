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

#include "resources/BitmapLoaderHandler.h"

#include "system/String.h"
#include "system/LowLevelSystem.h"
#include "resources/Resources.h"
#include "graphics/Graphics.h"

#include "graphics/Bitmap.h"
#include "resources/BitmapLoader.h"


namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	cBitmapLoaderHandler::cBitmapLoaderHandler(cResources* apResources, cGraphics* apGraphics)
	{
		mpResources = apResources;
		mpGraphics = apGraphics;
	}
	
	//-----------------------------------------------------------------------

	cBitmapLoaderHandler::~cBitmapLoaderHandler()
	{
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	cBitmap* cBitmapLoaderHandler::LoadBitmap(const tWString& asFile, tBitmapLoadFlag aFlags)
	{
		if(mbRust) return static_cast<cBitmap*>(ohpl_h4_handler_run(mpLoaders, &mHOps, OHPL_H4_BITMAP_LOAD, reinterpret_cast<const uint32_t*>(asFile.data()), asFile.size(), (int64_t)aFlags, NULL));
		iBitmapLoader *pBitmapLoader = static_cast<iBitmapLoader*>(GetLoaderForFile(asFile));

		if(pBitmapLoader)
		{
			cBitmap* pBitmap = pBitmapLoader->LoadBitmap(asFile, aFlags);

			//Set name of the file loaded.
			if(pBitmap) pBitmap->SetFileName(cString::GetFileNameW(asFile));

			return pBitmap;
		}
		else
		{
			return NULL;
		}
	}

	//-----------------------------------------------------------------------

	bool cBitmapLoaderHandler::SaveBitmap(cBitmap* apBitmap, const tWString& asFile, tBitmapSaveFlag aFlags)
	{
		if(mbRust) return ohpl_h4_handler_run(mpLoaders, &mHOps, OHPL_H4_BITMAP_SAVE, reinterpret_cast<const uint32_t*>(asFile.data()), asFile.size(), (int64_t)aFlags, apBitmap) != NULL;
		iBitmapLoader *pBitmapLoader = static_cast<iBitmapLoader*>(GetLoaderForFile(asFile));
		
		if(pBitmapLoader)
		{
			return pBitmapLoader->SaveBitmap(apBitmap,asFile,aFlags);
		}
		return false;
	}
	
	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cBitmapLoaderHandler::SetupLoader(iResourceLoader *apLoader)
	{
		iBitmapLoader *pBitmapLoader = static_cast<iBitmapLoader*>(apLoader);

		pBitmapLoader->mpLowLevelGraphics = mpGraphics->GetLowLevel();
	}
	
	//-----------------------------------------------------------------------

	void* cBitmapLoaderHandler::H4Call(int alWhat, void *apLoader, const OhplH3WStr &aFile, int64_t alArg, void *apArg)
	{
		iBitmapLoader *pLoader = static_cast<iBitmapLoader*>(static_cast<iResourceLoader*>(apLoader));
		tWString sFile = H4Wide(aFile);
		switch(alWhat)
		{
		case OHPL_H4_BITMAP_LOAD: return pLoader->LoadBitmap(sFile, (tBitmapLoadFlag)alArg);
		case OHPL_H4_BITMAP_SAVE: return pLoader->SaveBitmap(static_cast<cBitmap*>(apArg), sFile, (tBitmapSaveFlag)alArg) ? apLoader : NULL;
		case OHPL_H4_BITMAP_SET_FILE_NAME: static_cast<cBitmap*>(apLoader)->SetFileName(sFile); return NULL;
		}
		return NULL;
	}
}
