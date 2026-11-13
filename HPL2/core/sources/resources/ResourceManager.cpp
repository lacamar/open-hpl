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

#include "impl/RustBackends.h"
#include "resources/RustResMgr.h"

#include <algorithm>
#include <cstring>

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
		: mpRs(OhplBackendRustRequested() ? ohpl_h3_mgr_new() : NULL),
		  m_mapResources(mpRs ? cResourceMap(ohpl_h3_mgr_map(mpRs)) : cResourceMap())
	{
		mpFileSearcher = apFileSearcher;
		mpLowLevelResources = apLowLevelResources;
		mpLowLevelSystem = apLowLevelSystem;
		memset(&mOps, 0, sizeof(mOps));
		if(mpRs == NULL) return;

		mOps.ctx = this;
		mOps.log = [](void*, int32_t alLevel, const uint8_t *apMsg, size_t alLen)
		{
			tString sMsg(reinterpret_cast<const char*>(apMsg), alLen);
			if(alLevel == 2) Error("%s", sMsg.c_str());
			else if(alLevel == 1) Warning("%s", sMsg.c_str());
			else Log("%s", sMsg.c_str());
		};
		mOps.now = [](void*) -> uint64_t { return cPlatform::GetApplicationTime(); };
		mOps.file_path = [](void *c, const uint8_t *p, size_t n, int32_t *apCount, size_t *apLen) -> const uint32_t*
		{
			int lCount = 0;
			const tWString &sPath = static_cast<iResourceManager*>(c)->mpFileSearcher->GetFilePath(tString(reinterpret_cast<const char*>(p), n), apCount ? &lCount : NULL);
			if(apCount) *apCount = lCount;
			*apLen = sPath.size();
			return reinterpret_cast<const uint32_t*>(sPath.data());
		};
		mOps.info = [](void*, void *apObj, OhplH3Info *apInfo)
		{
			iResourceBase *pRes = static_cast<iResourceBase*>(apObj);
			apInfo->name = OhplH3Str{reinterpret_cast<const uint8_t*>(pRes->GetName().data()), pRes->GetName().size()};
			apInfo->path = OhplH3WStr{reinterpret_cast<const uint32_t*>(pRes->GetFullPath().data()), pRes->GetFullPath().size()};
			apInfo->meta = pRes->GetMeta();
		};
		mOps.destroy = [](void*, void *apObj){ hplDelete(static_cast<iResourceBase*>(apObj)); };
		mOps.load_done = [](void*, const uint8_t *apName, size_t alLen, uint64_t alElapsed)
		{
			cEngineDiagnostics::AddLoadTime(tString(reinterpret_cast<const char*>(apName), alLen), (double)alElapsed);
			if(mpLoadTickCallback) mpLoadTickCallback();
		};
		mOps.create = [](void *c, OhplH3Create *apCreate) -> void* { return static_cast<iResourceManager*>(c)->H3Create(*apCreate); };
		mOps.call = [](void *c, int32_t alWhat, void *apObj, int64_t alArg) -> int64_t
		{
			iResourceManager *pThis = static_cast<iResourceManager*>(c);
			if(alWhat == OHPL_H3C_DESTROY_VIRTUAL){ pThis->Destroy(static_cast<iResourceBase*>(apObj)); return 0; }
			return pThis->H3Call(alWhat, apObj, alArg);
		};
		mOps.text = [](void *c, int32_t alWhat, void *apObj, OhplH3WStr aPath, void (*apSink)(void*, const uint8_t*, size_t), void *apCtx)
		{
			static_cast<iResourceManager*>(c)->H3Text(alWhat, apObj, aPath, apSink, apCtx);
		};
	}

	iResourceManager::~iResourceManager()
	{
		if(mpRs) ohpl_h3_mgr_free(mpRs);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	iResourceBase* iResourceManager::GetResource(const tWString& asFullPath)
	{
		if(mpRs) return static_cast<iResourceBase*>(ohpl_h3_mgr_get(mpRs, reinterpret_cast<const uint32_t*>(asFullPath.data()), asFullPath.size()));
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
		if(mpRs) { ohpl_h3f_destroy_unused(mpRs, &mOps, alMaxToKeep); return; }

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
		if(mpRs) { ohpl_h3f_destroy_all(mpRs, &mOps); return; }

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
		if(mpRs) { ohpl_h3f_begin(mpRs, &mOps, RM_STR(asFile)); return; }
		mlTimeStart = cPlatform::GetApplicationTime();
		
		//Log("Begin resource: %s\n",asFile.c_str());

		if(mlTabCount==0) { glOuterStart = mlTimeStart; gsOuterName = asFile; }
		mlTabCount++;
	}
	
	//-----------------------------------------------------------------------

	void iResourceManager::EndLoad()
	{
		if(mpRs) { ohpl_h3f_end(mpRs, &mOps); return; }
		mlTabCount--;
		if(mlTabCount==0) cEngineDiagnostics::AddLoadTime(gsOuterName, (double)(cPlatform::GetApplicationTime() - glOuterStart));
		if(mlTabCount==0 && mpLoadTickCallback) mpLoadTickCallback();
	}

	//-----------------------------------------------------------------------
	
	iResourceBase* iResourceManager::FindLoadedResource(const tString &asName, tWString &asFilePath,int *apEqualCount)
	{
		if(mpRs)
		{
			iResourceBase *pFound = static_cast<iResourceBase*>(ohpl_h3f_find_loaded(mpRs, &mOps, RM_STR(asName), apEqualCount,
				[](void *c, const uint32_t *p, size_t n){ static_cast<tWString*>(c)->assign(reinterpret_cast<const wchar_t*>(p), n); }, &asFilePath));
			return pFound;
		}
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
		if(mpRs)
		{
			tString sRsTabs;
			ohpl_h3f_tabs(mpRs, &mOps, [](void *c, const uint8_t *p, size_t n){ static_cast<tString*>(c)->assign(reinterpret_cast<const char*>(p), n); }, &sRsTabs);
			return sRsTabs;
		}
		tString sTabs ="";
		for(int i=0; i<mlTabCount; ++i) sTabs+="  ";
		return sTabs;
	}

	void iResourceManager::AddResource(iResourceBase* apResource, bool abLog, bool abAddToSet)
	{
		if(mpRs) { ohpl_h3f_add(mpRs, &mOps, apResource, abLog, abAddToSet); return; }

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

		if(mpRs) { ohpl_h3_mgr_remove(mpRs, apResource); return; }

		unsigned int lHash = cString::GetHashW(apResource->GetFullPath());

		m_mapResources.erase(lHash, apResource);
 	}

	//-----------------------------------------------------------------------


}
