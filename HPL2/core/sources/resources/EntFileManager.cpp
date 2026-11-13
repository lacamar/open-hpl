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

#include "resources/EntFileManager.h"

#include "system/String.h"
#include "system/LowLevelSystem.h"
#include "resources/Resources.h"
#include "resources/LowLevelResources.h"
#include "resources/XmlDocument.h"
#include "resources/RustResMgr.h"


namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// ENT FILE
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------
	
	cEntFile::cEntFile(const tString& asName, const tWString& asFullPath, cResources *apResources) : iResourceBase(asName, asFullPath, 0)
	{
		mpXmlDoc = apResources->GetLowLevel()->CreateXmlDocument(asName);
	}
	cEntFile::~cEntFile()
	{
		hplDelete(mpXmlDoc);
	}

	bool cEntFile::CreateFromFile()
	{
		return mpXmlDoc->CreateFromFile(GetFullPath());
	}

	//-----------------------------------------------------------------------


	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cEntFileManager::cEntFileManager(cResources *apResources)
		: iResourceManager(apResources->GetFileSearcher(), apResources->GetLowLevel(), apResources->GetLowLevelSystem())
	{
		mpResources = apResources;
	}

	cEntFileManager::~cEntFileManager()
	{
		if(mpRs) { ohpl_h3f_shutdown(mpRs, H3Ops(), OHPL_H3S_ENT_FILE); return; }
		DestroyAll();
		Log(" Done with ent files\n");
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cEntFile* cEntFileManager::CreateEntFile(const tString& asName)
	{
		if(mpRs) return H3Res<cEntFile>(ohpl_h3f_ent_file(mpRs, H3Ops(), RM_STR(asName)));
		cEntFile* pEntFile;

		BeginLoad(asName);

		cRmEnv env(mpFileSearcher, this);
		cRmOut out;
		ohpl_rm_resolve_simple(env.raw(), 0, RM_STR(asName), out.raw());
		const tString& asNewName = out.msName;
		const tWString& sPath = out.msPath;

		pEntFile = static_cast<cEntFile*>(out.Res());

		if(pEntFile==NULL && sPath!=_W(""))
		{
			pEntFile = hplNew(cEntFile, (asNewName, sPath.c_str(), mpResources));

			if(pEntFile->CreateFromFile()==false)
			{
				Error("Couldn't load entity file '%s'!\n",cString::To8Char(sPath).c_str());
				hplDelete(pEntFile);
				EndLoad();
				return NULL;
			}

			AddResource(pEntFile);
		}

		if(pEntFile)
			pEntFile->IncUserCount();
		else
			Error("Couldn't create ent file '%s'\n",asNewName.c_str());

		EndLoad();
		return pEntFile;
	}

	//-----------------------------------------------------------------------

	void cEntFileManager::Unload(iResourceBase* apResource)
	{

	}
	//-----------------------------------------------------------------------

	void cEntFileManager::Destroy(iResourceBase* apResource)
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

	void* cEntFileManager::H3Create(OhplH3Create &aCreate)
	{
		cEntFile *pEntFile = hplNew(cEntFile, (H3Name(aCreate.name), H3Wide(aCreate.path), mpResources));
		if(pEntFile->CreateFromFile()==false)
		{
			hplDelete(pEntFile);
			return NULL;
		}
		return static_cast<iResourceBase*>(pEntFile);
	}
}
