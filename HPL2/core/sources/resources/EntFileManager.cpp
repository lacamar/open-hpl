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
}
