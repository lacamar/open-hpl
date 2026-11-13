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

#include "resources/MeshLoaderHandler.h"

#include "resources/MeshLoader.h"
#include "system/String.h"
#include "system/LowLevelSystem.h"
#include "system/Platform.h"
#include "resources/RustResMgr.h"
#include "graphics/Mesh.h"
#include "resources/Resources.h"
#include "scene/Scene.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	cMeshLoaderHandler::cMeshLoaderHandler(cResources* apResources, cScene *apScene)
	{
		mpResources = apResources;
		mpScene = apScene;
	}
	
	//-----------------------------------------------------------------------

	cMeshLoaderHandler::~cMeshLoaderHandler()
	{
		
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	cMesh* cMeshLoaderHandler::LoadMesh(const tWString& asFile,tMeshLoadFlag aFlags)
	{
		return static_cast<cMesh*>(ohpl_h4_handler_run(mpLoaders, &mHOps, OHPL_H4_MESH_LOAD, reinterpret_cast<const uint32_t*>(asFile.data()), asFile.size(), (int64_t)aFlags, NULL));
	}

	//-----------------------------------------------------------------------

	bool cMeshLoaderHandler::SaveMesh(cMesh* apMesh,const tWString& asFile)
	{
		return ohpl_h4_handler_run(mpLoaders, &mHOps, OHPL_H4_MESH_SAVE, reinterpret_cast<const uint32_t*>(asFile.data()), asFile.size(), 0, apMesh) != NULL;
	}
	
	//-----------------------------------------------------------------------
	cAnimation* cMeshLoaderHandler::LoadAnimation(const tWString& asFile)
	{
		return static_cast<cAnimation*>(ohpl_h4_handler_run(mpLoaders, &mHOps, OHPL_H4_ANIMATION_LOAD, reinterpret_cast<const uint32_t*>(asFile.data()), asFile.size(), 0, NULL));
	}

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	
	void cMeshLoaderHandler::SetupLoader(iResourceLoader *apLoader)
	{
		iMeshLoader *pMeshLoader = static_cast<iMeshLoader*>(apLoader);

		pMeshLoader->mpMaterialManager = mpResources->GetMaterialManager();
		pMeshLoader->mpMeshManager = mpResources->GetMeshManager();
		pMeshLoader->mpAnimationManager = mpResources->GetAnimationManager();
	}

	//-----------------------------------------------------------------------

	void* cMeshLoaderHandler::H4Call(int alWhat, void *apLoader, const OhplH3WStr &aFile, int64_t alArg, void *apArg)
	{
		iMeshLoader *pLoader = static_cast<iMeshLoader*>(static_cast<iResourceLoader*>(apLoader));
		tWString sFile = H4Wide(aFile);
		switch(alWhat)
		{
		case OHPL_H4_MESH_LOAD: return pLoader->LoadMesh(sFile, (tMeshLoadFlag)alArg);
		case OHPL_H4_MESH_SAVE: return pLoader->SaveMesh(static_cast<cMesh*>(apArg), sFile) ? apLoader : NULL;
		case OHPL_H4_ANIMATION_LOAD: return pLoader->LoadAnimation(sFile);
		case OHPL_H4_MESH_SET_FULL_PATH: static_cast<cMesh*>(apLoader)->SetFullPath(sFile); return NULL;
		case OHPL_H4_MESH_CACHE_USABLE: return (cResources::GetForceCacheLoadingAndSkipSaving() && cPlatform::FileExists(sFile)) ? (void*)1 : NULL;
		}
		return NULL;
	}
}
