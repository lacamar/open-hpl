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

#ifndef HPL_RESOURCEMANAGER_H
#define HPL_RESOURCEMANAGER_H

#include <map>
#include <cstring>
#include "resources/ResourceBase.h"
#include "system/SystemTypes.h"
#include "resources/RustResources.h"
#include "resources/RustResourcesH3.h"

namespace hpl {

	class iLowLevelResources;
	class iLowLevelSystem;
	class cFileSearcher;
	class iResourceBase;

	typedef cResourceMap tResourceBaseMap;
	typedef tResourceBaseMap::iterator tResourceBaseMapIt;

	typedef std::list<iResourceBase*> tResourceBaseList;
	typedef tResourceBaseList::iterator tResourceBaseListIt;

	typedef cSTLMapIterator<iResourceBase*, tResourceBaseMap, tResourceBaseMapIt> cResourceBaseIterator;
	
	//------------------------------------------

	class iResourceManager
	{
	public:
		iResourceManager(cFileSearcher *apFileSearcher, iLowLevelResources *apLowLevelResources,
						iLowLevelSystem *apLowLevelSystem);
		virtual ~iResourceManager();

		iResourceBase* GetResource(const tWString& asFullPath);
		
		cResourceBaseIterator GetResourceBaseIterator();

		void DestroyUnused(int alMaxToKeep);

		virtual void Destroy(iResourceBase* apResource)=0;
		virtual void DestroyAll();

		virtual void Unload(iResourceBase* apResource)=0;
		
		virtual void Update(float afTimeStep){}

		static void (*mpLoadTickCallback)();
		
	protected:
		OhplH3Mgr *mpRs;
		OhplH3Ops mOps;
		tResourceBaseMap m_mapResources;

		const OhplH3Ops* H3Ops() { return &mOps; }
		static tWString H3Wide(const OhplH3WStr &aStr){ return tWString(reinterpret_cast<const wchar_t*>(aStr.p), aStr.n); }
		static tString H3Name(const OhplH3Str &aStr){ return tString(reinterpret_cast<const char*>(aStr.p), aStr.n); }
		static int64_t H3Bits(float afX){ int32_t lBits; memcpy(&lBits, &afX, sizeof(lBits)); return lBits; }
		static float H3Float(int64_t alBits){ int32_t lBits = (int32_t)alBits; float fX; memcpy(&fX, &lBits, sizeof(fX)); return fX; }
		template<class T> static T* H3Res(void *apObj){ return static_cast<T*>(static_cast<iResourceBase*>(apObj)); }
		virtual void* H3Create(OhplH3Create &aCreate){ return NULL; }
		virtual int64_t H3Call(int alWhat, void *apObj, int64_t alArg){ return 0; }
		virtual void H3Text(int alWhat, void *apObj, const OhplH3WStr &aPath, void (*apSink)(void*, const uint8_t*, size_t), void *apCtx){}
		
		cFileSearcher *mpFileSearcher;
		iLowLevelResources *mpLowLevelResources;
		iLowLevelSystem *mpLowLevelSystem;

		void BeginLoad(const tString& asFile);
		void EndLoad();
		
		unsigned long mlTimeStart;

		/**
		 * Checks if a resource alllready is in the manager, else searches the resources.
		 * \param &asName Name of the resource.
		 * \param &asFilePath If the file is not in the manager, the path is put here. "" if there is no such file.
		 * \return A pointer to the resource. NULL if not in manager.
		 */
		iResourceBase* FindLoadedResource(const tString &asName, tWString &asFilePath, int *apEqualCount=NULL);
		void AddResource(iResourceBase* apResource, bool abLog=true, bool abAddToSet=true);
		void RemoveResource(iResourceBase* apResource);

		tString GetTabs();
		static int mlTabCount;

	};

};
#endif // HPL_RESOURCEMANAGER_H
