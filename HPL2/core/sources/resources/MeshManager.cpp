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

#include "resources/MeshManager.h"
#include "system/String.h"
#include "system/System.h"
#include "resources/Resources.h"
#include "graphics/Mesh.h"
#include "system/LowLevelSystem.h"
#include "resources/MeshLoaderHandler.h"
#include "resources/FileSearcher.h"
#include "resources/RustResMgr.h"
#include "graphics/SubMesh.h"
#include "graphics/VertexBuffer.h"


namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cMeshManager::cMeshManager(cGraphics* apGraphic,cResources *apResources)
		: iResourceManager(apResources->GetFileSearcher(), apResources->GetLowLevel(),
							apResources->GetLowLevelSystem())
	{
		mpGraphics = apGraphic;
		mpResources = apResources;

		msFastloadMaterial = "";
		mbUseFastloadMaterial = false;
	}

	cMeshManager::~cMeshManager()
	{
		if(mpRs) { ohpl_h3f_shutdown(mpRs, H3Ops(), OHPL_H3S_MESH); return; }
		DestroyAll();

		Log(" Done with meshes\n");
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cMesh* cMeshManager::CreateMesh(const tString& asName, tMeshLoadFlag aFlag)
	{
		if(mpRs)
		{
			cRmExts exts(*mpResources->GetMeshLoaderHandler()->GetSupportedTypes());
			return H3Res<cMesh>(ohpl_h3f_model(mpRs, H3Ops(), true, RM_STR(asName), exts.h3(), exts.size(), (int32_t)aFlag));
		}
		cMesh* pMesh;

		BeginLoad(asName);

		cRmEnv env(mpFileSearcher, this);
		cRmExts exts(*mpResources->GetMeshLoaderHandler()->GetSupportedTypes());
		cRmOut out;
		if(ohpl_rm_resolve_model(env.raw(), RM_STR(asName), exts.data(), exts.size(), out.raw())==0)
		{
			Error("Couldn't find mesh file '%s' in any supported format!\n",asName.c_str());
			EndLoad();
			return NULL;
		}
		const tString& asNewName = out.msName;
		const tWString& sPath = out.msPath;

		pMesh = static_cast<cMesh*>(out.Res());

		if(pMesh==NULL && sPath!=_W(""))
		{
			pMesh = mpResources->GetMeshLoaderHandler()->LoadMesh(sPath,aFlag);
			if(pMesh == NULL)
			{
				EndLoad();
				return NULL;
			}
			AddResource(pMesh);
		}

		if(pMesh)pMesh->IncUserCount();
		else Error("Couldn't load mesh '%s'\n",asNewName.c_str());

		EndLoad();
		return pMesh;
	}

	//-----------------------------------------------------------------------

	iVertexBuffer* cMeshManager::CreateVertexBufferFromMesh(const tString& asName, tVertexElementFlag alVtxToCopy)
	{
		cMesh *pMesh = CreateMesh(asName);
		if(pMesh==NULL) return NULL;

		iVertexBuffer *pVtxBuffer = pMesh->GetSubMesh(0)->GetVertexBuffer()->CreateCopy(eVertexBufferType_Hardware,
																						eVertexBufferUsageType_Static,
																						alVtxToCopy);
		Destroy(pMesh);
		return pVtxBuffer;
	}

	//-----------------------------------------------------------------------

	void cMeshManager::Unload(iResourceBase* apResource)
	{

	}
	//-----------------------------------------------------------------------

	void cMeshManager::Destroy(iResourceBase* apResource)
	{
		if(mpRs) { ohpl_h3f_destroy_standard(mpRs, H3Ops(), apResource); return; }
		if(ohpl_rm_destroy_standard(apResource->GetMeta())){
			RemoveResource(apResource);
			hplDelete(apResource);
		}
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------


	//-----------------------------------------------------------------------

	void* cMeshManager::H3Create(OhplH3Create &aCreate)
	{
		return static_cast<iResourceBase*>(mpResources->GetMeshLoaderHandler()->LoadMesh(H3Wide(aCreate.path), (tMeshLoadFlag)aCreate.i[0]));
	}
}
