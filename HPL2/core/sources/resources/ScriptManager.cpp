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

#include "resources/ScriptManager.h"
#include "system/String.h"
#include "system/System.h"
#include "resources/Resources.h"
#include "system/Script.h"
#include "system/LowLevelSystem.h"
#include "resources/RustResMgr.h"



namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cScriptManager::cScriptManager(cSystem* apSystem,cResources *apResources)
		: iResourceManager(apResources->GetFileSearcher(), apResources->GetLowLevel(),
							apResources->GetLowLevelSystem())
	{
		mpSystem = apSystem;
		mpResources = apResources;
	}

	cScriptManager::~cScriptManager()
	{
		if(mpRs) { ohpl_h3f_shutdown(mpRs, H3Ops(), OHPL_H3S_SCRIPT); return; }
		DestroyAll();
		Log(" Done with scripts\n");
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	iScript* cScriptManager::CreateScript(const tString& asName, tString *apCompileMessages)
	{
		if(mpRs) return H3Res<iScript>(ohpl_h3f_script(mpRs, H3Ops(), RM_STR(asName), apCompileMessages));
		iScript* pScript;

		BeginLoad(asName);

		cRmEnv env(mpFileSearcher, this);
		cRmOut out;
		ohpl_rm_resolve_simple(env.raw(), 2, RM_STR(asName), out.raw());
		const tString& asNewName = out.msName;
		const tWString& sPath = out.msPath;

		pScript = static_cast<iScript*>(out.Res());

		if(pScript==NULL && sPath!=_W(""))
		{
			pScript = mpSystem->GetLowLevel()->CreateScript(asNewName);
			if(pScript->CreateFromFile(sPath, apCompileMessages)==false){
				hplDelete(pScript);
				EndLoad();
				return NULL;
			}

			AddResource(pScript);
		}

		if(pScript)pScript->IncUserCount();
		else Error("Couldn't create script '%s'\n",asNewName.c_str());

		EndLoad();
		return pScript;
	}

	//-----------------------------------------------------------------------

	void cScriptManager::Unload(iResourceBase* apResource)
	{

	}
	//-----------------------------------------------------------------------

	void cScriptManager::Destroy(iResourceBase* apResource)
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

	void* cScriptManager::H3Create(OhplH3Create &aCreate)
	{
		iScript *pScript = mpSystem->GetLowLevel()->CreateScript(H3Name(aCreate.name));
		if(pScript->CreateFromFile(H3Wide(aCreate.path), static_cast<tString*>(aCreate.ptr))==false)
		{
			hplDelete(pScript);
			return NULL;
		}
		return static_cast<iResourceBase*>(pScript);
	}
}
