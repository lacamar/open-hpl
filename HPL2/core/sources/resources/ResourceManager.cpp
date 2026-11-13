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

#include "resources/RustResMgr.h"

#include <algorithm>
#include <cstring>

namespace hpl {

	void (*iResourceManager::mpLoadTickCallback)() = NULL;

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	iResourceManager::iResourceManager(cFileSearcher *apFileSearcher, 
										iLowLevelResources *apLowLevelResources,
										iLowLevelSystem *apLowLevelSystem)
		: mpRs(ohpl_h3_mgr_new()),
		  m_mapResources(ohpl_h3_mgr_map(mpRs))
	{
		mpFileSearcher = apFileSearcher;
		mpLowLevelResources = apLowLevelResources;
		mpLowLevelSystem = apLowLevelSystem;
		memset(&mOps, 0, sizeof(mOps));

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
			int lCount = apCount ? *apCount : 0;
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
		ohpl_h3_mgr_free(mpRs);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	iResourceBase* iResourceManager::GetResource(const tWString& asFullPath)
	{
		return static_cast<iResourceBase*>(ohpl_h3_mgr_get(mpRs, reinterpret_cast<const uint32_t*>(asFullPath.data()), asFullPath.size()));
	}

	//-----------------------------------------------------------------------

	cResourceBaseIterator iResourceManager::GetResourceBaseIterator()
	{
		return cResourceBaseIterator(&m_mapResources);
	}
	
	//-----------------------------------------------------------------------

	void iResourceManager::DestroyUnused(int alMaxToKeep)
	{
		ohpl_h3f_destroy_unused(mpRs, &mOps, alMaxToKeep);
	}

	//-----------------------------------------------------------------------

	void iResourceManager::DestroyAll()
	{
		ohpl_h3f_destroy_all(mpRs, &mOps);
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PROTECTED METHODS
	//////////////////////////////////////////////////////////////////////////
	
	//-----------------------------------------------------------------------

	void iResourceManager::BeginLoad(const tString& asFile)
	{
		ohpl_h3f_begin(mpRs, &mOps, RM_STR(asFile));
	}
	
	//-----------------------------------------------------------------------

	void iResourceManager::EndLoad()
	{
		ohpl_h3f_end(mpRs, &mOps);
	}

	//-----------------------------------------------------------------------
	
	iResourceBase* iResourceManager::FindLoadedResource(const tString &asName, tWString &asFilePath,int *apEqualCount)
	{
		return static_cast<iResourceBase*>(ohpl_h3f_find_loaded(mpRs, &mOps, RM_STR(asName), apEqualCount,
			[](void *c, const uint32_t *p, size_t n){ static_cast<tWString*>(c)->assign(reinterpret_cast<const wchar_t*>(p), n); }, &asFilePath));
	}

	//-----------------------------------------------------------------------
	
	tString iResourceManager::GetTabs()
	{
		tString sTabs;
		ohpl_h3f_tabs(mpRs, &mOps, [](void *c, const uint8_t *p, size_t n){ static_cast<tString*>(c)->assign(reinterpret_cast<const char*>(p), n); }, &sTabs);
		return sTabs;
	}

	void iResourceManager::AddResource(iResourceBase* apResource, bool abLog, bool abAddToSet)
	{
		ohpl_h3f_add(mpRs, &mOps, apResource, abLog, abAddToSet);
	}
	
	//-----------------------------------------------------------------------

	void iResourceManager::RemoveResource(iResourceBase* apResource)
	{
		ohpl_h3_mgr_remove(mpRs, apResource);
 	}

	//-----------------------------------------------------------------------


}
