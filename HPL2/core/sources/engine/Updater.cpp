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

#include "engine/Updater.h"

#include "engine/Updateable.h"
#include "system/LowLevelSystem.h"
#include "system/Platform.h"

namespace hpl {

	static void RunVisit(void *apCtx, size_t alUpdateable, int alOp, int alMsg, float afX, const uint8_t *apName, size_t alNameLen)
	{
		iUpdateable *pUpdateable = reinterpret_cast<iUpdateable*>(alUpdateable);
		switch(alOp)
		{
		case 0:
			pUpdateable->RunMessage((eUpdateableMessage)alMsg, afX);
			break;
		case 1:
		{
			START_TIMING_EX(pUpdateable->GetName().c_str(),game)
			pUpdateable->RunMessage(eUpdateableMessage_Update, afX);
			STOP_TIMING(game)
			break;
		}
		case 2:
			pUpdateable->OnLeaveContainer(tString((const char*)apName, alNameLen));
			break;
		case 3:
			pUpdateable->OnEnterContainer(tString((const char*)apName, alNameLen));
			break;
		}
	}

	static_assert(eUpdateableMessage_Update == 5, "ohpl_en updater MSG_UPDATE");

	//-----------------------------------------------------------------------

	cUpdater::cUpdater(iLowLevelSystem *apLowLevelSystem)
	{
		mpHandle = ohpl_en_upd_new();
		mpLowLevelSystem = apLowLevelSystem;
	}

	cUpdater::~cUpdater()
	{
		ohpl_en_upd_free(mpHandle);
	}

	//-----------------------------------------------------------------------

	void cUpdater::BroadcastMessageToAll(eUpdateableMessage aMessage, float afX)
	{
		ohpl_en_upd_broadcast(mpHandle, aMessage, afX, RunVisit, NULL);
	}

	//-----------------------------------------------------------------------

	void cUpdater::RunMessage(eUpdateableMessage aMessage, float afX)
	{
		ohpl_en_upd_run_message(mpHandle, aMessage, afX, RunVisit, NULL);
	}

	//-----------------------------------------------------------------------

	bool cUpdater::SetContainer(tString asContainer)
	{
		return ohpl_en_upd_set_container(mpHandle, OhplEnP(asContainer), asContainer.size(), RunVisit, NULL);
	}

	//-----------------------------------------------------------------------

	tString cUpdater::GetCurrentContainerName()
	{
		tString sName;
		ohpl_en_upd_current_name(mpHandle, OhplEnAssign, &sName);
		return sName;
	}

	//-----------------------------------------------------------------------

	bool cUpdater::AddContainer(tString asName)
	{
		ohpl_en_upd_add_container(mpHandle, OhplEnP(asName), asName.size());
		return true;
	}

	//-----------------------------------------------------------------------

	bool cUpdater::AddUpdate(tString asContainer, iUpdateable* apUpdate)
	{
		int lRes = ohpl_en_upd_add_update(mpHandle, OhplEnP(asContainer), asContainer.size(), reinterpret_cast<size_t>(apUpdate));
		if(lRes == 2)
		{
			Error("Couldn't add NULL updatable!");
			return false;
		}
		return lRes == 1;
	}

	//-----------------------------------------------------------------------

	bool cUpdater::AddGlobalUpdate(iUpdateable* apUpdate)
	{
		ohpl_en_upd_add_global(mpHandle, reinterpret_cast<size_t>(apUpdate));
		return true;
	}

	//-----------------------------------------------------------------------
}
