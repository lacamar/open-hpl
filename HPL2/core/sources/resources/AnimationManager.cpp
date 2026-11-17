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

#include "resources/AnimationManager.h"
#include "system/String.h"
#include "system/System.h"
#include "resources/Resources.h"
#include "graphics/Mesh.h"
#include "graphics/Animation.h"
#include "system/LowLevelSystem.h"
#include "resources/MeshLoaderHandler.h"
#include "resources/FileSearcher.h"
#include "resources/RustResMgr.h"


namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cAnimationManager::cAnimationManager(cGraphics* apGraphic,cResources *apResources)
		: iResourceManager(apResources->GetFileSearcher(), apResources->GetLowLevel(),
							apResources->GetLowLevelSystem())
	{
		mpGraphics = apGraphic;
		mpResources = apResources;
	}

	cAnimationManager::~cAnimationManager()
	{
		ohpl_h3f_shutdown(mpRs, H3Ops(), OHPL_H3S_ANIMATION);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cAnimation* cAnimationManager::CreateAnimation(const tString& asName)
	{
		cRmExts exts(*mpResources->GetMeshLoaderHandler()->GetSupportedTypes());
		return H3Res<cAnimation>(ohpl_h3f_model(mpRs, H3Ops(), false, RM_STR(asName), exts.h3(), exts.size(), 0));
	}

	//-----------------------------------------------------------------------

	void cAnimationManager::Unload(iResourceBase* apResource)
	{

	}
	//-----------------------------------------------------------------------

	void cAnimationManager::Destroy(iResourceBase* apResource)
	{
		ohpl_h3f_destroy_standard(mpRs, H3Ops(), apResource);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------


	//-----------------------------------------------------------------------

	void* cAnimationManager::H3Create(OhplH3Create &aCreate)
	{
		return static_cast<iResourceBase*>(mpResources->GetMeshLoaderHandler()->LoadAnimation(H3Wide(aCreate.path)));
	}
}
