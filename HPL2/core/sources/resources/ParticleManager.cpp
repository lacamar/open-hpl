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

#include "resources/ParticleManager.h"

#include "resources/Resources.h"
#include "resources/FileSearcher.h"
#include "graphics/Graphics.h"
#include "scene/ParticleSystem.h"
#include "system/LowLevelSystem.h"
#include "system/String.h"

#include "impl/tinyXML/tinyxml.h"
#include "resources/RustResMgr.h"

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cParticleManager::cParticleManager(cGraphics* apGraphics,cResources *apResources)
		: iResourceManager(apResources->GetFileSearcher(), apResources->GetLowLevel(),
							apResources->GetLowLevelSystem())
	{
		mpGraphics = apGraphics;
		mpResources = apResources;
	}

	cParticleManager::~cParticleManager()
	{
		if(mpRs) { ohpl_h3f_shutdown(mpRs, H3Ops(), OHPL_H3S_PARTICLE); return; }
		tResourceBaseMapIt it = m_mapResources.begin();
		for(;it != m_mapResources.end(); ++it)
		{
			iResourceBase* pResource = it->second;
			while(pResource->HasUsers()) pResource->DecUserCount();
		}

		DestroyUnused(0);

		Log(" Done with particles\n");

		
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cParticleSystem* cParticleManager::CreatePS(const tString& asName,const tString& asType, cVector3f avSize)
	{
		if(mpRs) return static_cast<cParticleSystem*>(ohpl_h3f_particle_by_type(mpRs, H3Ops(), RM_STR(asName), RM_STR(asType), avSize.x, avSize.y, avSize.z));
		cParticleSystemData *pData = NULL;

		cRmEnv env(mpFileSearcher, this);
		cRmOut byFile;
		ohpl_rm_particle_by_file(env.raw(), RM_STR(asType), byFile.raw());
		const tString& sFile = byFile.msName;
		const tWString& sPath = byFile.msPath;

		BeginLoad(asName);

		pData = static_cast<cParticleSystemData*>(byFile.Res());
		if(pData == NULL)
		{
			if(sPath == _W(""))
			{
				Error("Couldn't find particle system file '%s'\n",sFile.c_str());
				EndLoad();
				return NULL;
			}
			
			cParticleSystemData *pPSData = hplNew( cParticleSystemData, (sFile,	mpResources,mpGraphics) );

			if(pPSData->LoadFromFile(sPath)==false)
			{
				Error("Can't load data from particle system file '%s'\n",cString::To8Char(sPath).c_str());
				hplDelete(pPSData);
				EndLoad();
				return NULL;
			}

			AddData(pPSData);

			pData = pPSData;
		}
				

		pData->IncUserCount();
        cParticleSystem* pPS = pData->Create(asName,avSize);
		pPS->SetDataName(asType);
		pPS->SetDataSize(avSize);
		pPS->SetParticleManager(this);

		EndLoad();

		return pPS;		
	}
    
	//-----------------------------------------------------------------------

	cParticleSystem* cParticleManager::CreatePS(const tString& asName, const tString& asDataName, cXmlElement* apElement, cVector3f avSize)
	{
		if(mpRs) return static_cast<cParticleSystem*>(ohpl_h3f_particle_by_element(mpRs, H3Ops(), RM_STR(asName), RM_STR(asDataName), apElement, avSize.x, avSize.y, avSize.z));
		cParticleSystemData *pPSData = NULL;

		BeginLoad(asName);

		cRmEnv env(mpFileSearcher, this);
		pPSData = static_cast<cParticleSystemData*>(static_cast<iResourceBase*>(ohpl_rm_particle_by_data(env.raw(), RM_STR(asDataName))));

		if(pPSData==NULL)
		{
            pPSData = hplNew( cParticleSystemData,(asDataName,mpResources,mpGraphics) );
			if(pPSData->LoadFromElement(apElement)==false)
			{
				Error("Can't load particle system data '%s'\n",asDataName.c_str());
				hplDelete(pPSData);
				EndLoad();
				return NULL;
			}

			AddData(pPSData);
		}

		pPSData->IncUserCount();
        cParticleSystem* pPS = pPSData->Create(asName,avSize);
		pPS->SetDataName(asDataName);
		pPS->SetDataSize(avSize);
		pPS->SetParticleManager(this);

		EndLoad();

		return pPS;
	}

	//-----------------------------------------------------------------------	

	void cParticleManager::AddData(cParticleSystemData *apData)
	{
		if(mpRs) { ohpl_h3f_particle_add_data(mpRs, H3Ops(), static_cast<iResourceBase*>(apData)); return; }
		AddResource(apData);
	}

	//-----------------------------------------------------------------------

	void cParticleManager::Preload(const tString& asFile)
	{
		if(mpRs) { ohpl_h3f_particle_preload(mpRs, H3Ops(), RM_STR(asFile)); return; }
		cRmEnv env(mpFileSearcher, this);
		cRmOut byFile;
		ohpl_rm_particle_by_file(env.raw(), RM_STR(asFile), byFile.raw());
		const tString& sFile = byFile.msName;
		const tWString& sPath = byFile.msPath;
		
		cParticleSystemData *pData = static_cast<cParticleSystemData*>(byFile.Res());
		if(pData == NULL)
		{
			if(sPath == _W(""))
			{
				Error("Couldn't find particle system file '%s'\n",sFile.c_str());
				return;
			}

			cParticleSystemData *pPSData = hplNew( cParticleSystemData, (sFile, mpResources,mpGraphics) );

			if(pPSData->LoadFromFile(sPath)==false)
			{
				Error("Can't load data from particle system file '%s'\n",cString::To8Char(sPath).c_str());
				hplDelete(pPSData);
				return;
			}

			AddData(pPSData);
		}
	}

	//-----------------------------------------------------------------------

	void cParticleManager::Unload(iResourceBase* apResource)
	{

	}
	//-----------------------------------------------------------------------

	void cParticleManager::Destroy(iResourceBase* apResource)
	{
		if(mpRs) { ohpl_h3f_particle_destroy(mpRs, H3Ops(), apResource); return; }
		ohpl_rm_destroy_particle(apResource->GetMeta());
	}
	
	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------


	//-----------------------------------------------------------------------

	void* cParticleManager::H3Create(OhplH3Create &aCreate)
	{
		if(aCreate.kind == OHPL_H3_PS_CREATE)
		{
			cVector3f vSize(aCreate.f[0], aCreate.f[1], aCreate.f[2]);
			cParticleSystemData *pData = H3Res<cParticleSystemData>(aCreate.ptr);
			cParticleSystem *pPS = pData->Create(H3Name(aCreate.name), vSize);
			pPS->SetDataName(H3Name(aCreate.name2));
			pPS->SetDataSize(vSize);
			pPS->SetParticleManager(this);
			return pPS;
		}
		cParticleSystemData *pPSData = hplNew( cParticleSystemData, (H3Name(aCreate.name), mpResources, mpGraphics) );
		bool bOk = aCreate.kind == OHPL_H3_PS_DATA_FILE ? pPSData->LoadFromFile(H3Wide(aCreate.path)) : pPSData->LoadFromElement(static_cast<cXmlElement*>(aCreate.ptr));
		if(bOk==false)
		{
			hplDelete(pPSData);
			return NULL;
		}
		return static_cast<iResourceBase*>(pPSData);
	}
}
