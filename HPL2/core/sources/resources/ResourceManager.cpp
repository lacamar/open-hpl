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

#include "resources/ResourceManager.h"

#include "system/String.h"
#include "system/Platform.h"
#include "system/EngineDiagnostics.h"

#include "resources/LowLevelResources.h"
#include "resources/FileSearcher.h"
#include "resources/ResourceBase.h"

#include "system/LowLevelSystem.h"

#include <algorithm>

namespace hpl {

	int iResourceManager::mlTabCount=0;
	static unsigned long glOuterStart=0;
	static tString gsOuterName;
	void (*iResourceManager::mpLoadTickCallback)() = NULL;

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	iResourceManager::iResourceManager(cFileSearcher *apFileSearcher, 
										iLowLevelResources *apLowLevelResources,
										iLowLevelSystem *apLowLevelSystem)
	{
		mpFileSearcher = apFileSearcher;
		mpLowLevelResources = apLowLevelResources;
		mpLowLevelSystem = apLowLevelSystem;
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	iResourceBase* iResourceManager::GetResource(const tWString& asFullPath)
	{
		unsigned int lHash = cString::GetHashW(asFullPath);
		return m_mapResources.find_if(lHash, [&](iResourceBase *pResource){ return pResource->GetFullPath() == asFullPath; });
	}

	//-----------------------------------------------------------------------

	cResourceBaseIterator iResourceManager::GetResourceBaseIterator()
	{
		return cResourceBaseIterator(&m_mapResources);
	}
	
	//-----------------------------------------------------------------------

	static void ResourceUseKey(void*, void *apRes, unsigned int *apUsers, unsigned long *apTime)
	{
		iResourceBase *pRes = static_cast<iResourceBase*>(apRes);
		*apUsers = pRes->GetUserCount();
		*apTime = pRes->GetTime();
	}

	static void CollectResources(void *apCtx, void *const *apRes, size_t alCount)
	{
		iResourceBase *const *ppFirst = reinterpret_cast<iResourceBase *const *>(apRes);
		static_cast<std::vector<iResourceBase*>*>(apCtx)->assign(ppFirst, ppFirst + alCount);
	}

    void iResourceManager::DestroyUnused(int alMaxToKeep)
	{
		if((int)m_mapResources.size() <= alMaxToKeep) return;

		//Sorted according to num of users and then time.
		std::vector<iResourceBase*> vResources;
		ohpl_resmap_eviction_order(m_mapResources.raw(), ResourceUseKey, NULL, CollectResources, &vResources);

		for(size_t i=alMaxToKeep; i<vResources.size(); ++i)
		{
			iResourceBase *pRes = vResources[i];

			if(pRes->HasUsers()==false)
			{
				RemoveResource(pRes);
				hplDelete(pRes);
			}
		}
	}

	//-----------------------------------------------------------------------

	void iResourceManager::DestroyAll()
	{
		tResourceBaseMapIt it = m_mapResources.begin();
		while(it != m_mapResources.end())
		{
			//Log("Start destroy...");
			
			iResourceBase* pResource = it->second;
			
			//Log(" res: %d ...", pResource);
			//Log(" res: '%s' / '%s': %d ...",pResource->GetName().c_str(), cString::To8Char(pResource->GetFullPath()).c_str(),pResource->GetUserCount());

			while(pResource->HasUsers()) pResource->DecUserCount();
			
			Destroy(pResource);

			it = m_mapResources.begin();
			
			//Log(" Done!\n");
		}
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PROTECTED METHODS
	//////////////////////////////////////////////////////////////////////////
	
	//-----------------------------------------------------------------------

	void iResourceManager::BeginLoad(const tString& asFile)
	{
		mlTimeStart = cPlatform::GetApplicationTime();
		
		//Log("Begin resource: %s\n",asFile.c_str());

		if(mlTabCount==0) { glOuterStart = mlTimeStart; gsOuterName = asFile; }
		mlTabCount++;
	}
	
	//-----------------------------------------------------------------------

	void iResourceManager::EndLoad()
	{
		mlTabCount--;
		if(mlTabCount==0) cEngineDiagnostics::AddLoadTime(gsOuterName, (double)(cPlatform::GetApplicationTime() - glOuterStart));
		if(mlTabCount==0 && mpLoadTickCallback) mpLoadTickCallback();
	}

	//-----------------------------------------------------------------------
	
	iResourceBase* iResourceManager::FindLoadedResource(const tString &asName, tWString &asFilePath,int *apEqualCount)
	{
		asFilePath = mpFileSearcher->GetFilePath(asName, apEqualCount);
		iResourceBase* pResource = GetResource(asFilePath);
		if(pResource!=NULL)
		{
			asFilePath = _W("");
		}

		return pResource;
	}

	//-----------------------------------------------------------------------
	
	tString iResourceManager::GetTabs()
	{
		tString sTabs ="";
		for(int i=0; i<mlTabCount; ++i) sTabs+="  ";
		return sTabs;
	}

	void iResourceManager::AddResource(iResourceBase* apResource, bool abLog, bool abAddToSet)
	{
		tString sName = cString::ToLowerCase(apResource->GetName());
		
		if(abAddToSet)
		{
			int lHash = cString::GetHashW(apResource->GetFullPath());
			m_mapResources.insert(lHash, apResource);
		}

		//Log("Adding %d, '%s' hash: %u\n",apResource,cString::To8Char(apResource->GetFullPath()).c_str(), lHash);
		
		if(abLog && iResourceBase::GetLogCreateAndDelete())
		{
			unsigned long lTime = cPlatform::GetApplicationTime() - mlTimeStart;
            Log("%sLoaded resource %s in %d ms\n",GetTabs().c_str(), apResource->GetName().c_str(),lTime);
			apResource->SetLogDestruction(true);
		}
		
		//Log("End resource: %s\n",apResource->GetName().c_str());
	}
	
	//-----------------------------------------------------------------------

	void iResourceManager::RemoveResource(iResourceBase* apResource)
	{
		//Log("Removing resource name: '%s' path: '%s' ", apResource->GetName().c_str(), cString::To8Char(apResource->GetFullPath()).c_str());

		unsigned int lHash = cString::GetHashW(apResource->GetFullPath());

		m_mapResources.erase(lHash, apResource);
 	}

	//-----------------------------------------------------------------------


}
