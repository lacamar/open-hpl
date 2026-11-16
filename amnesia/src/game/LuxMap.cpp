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

#include "LuxMap.h"
#include "LuxEntity.h"
#include "LuxAreaNodes.h"
#include "LuxEnemy.h"
#include "LuxProp_Lamp.h"

void cLuxMap::HostEntityCall(iLuxEntity *apEntity, int alWhich)
{
	if(alWhich == 0) apEntity->BeforeEntityDestruction();
	else apEntity->DestroyMe();
}

void cLuxMap::AddPlayerStart(cLuxNode_PlayerStart *apNode)
{
	ohpl_a7_start_add(mpA7, apNode, OHPL_A7_S(apNode->GetName()));
}

cLuxNode_PlayerStart * cLuxMap::GetPlayerStart(const tString & asName)
{
	cLuxNode_PlayerStart *pNode = static_cast<cLuxNode_PlayerStart*>(ohpl_a7_start_find(mpA7, OHPL_A7_S(asName)));
	if(pNode == NULL) Warning("Could not find player start node '%s'\n", asName.c_str());
	return pNode;
}

void cLuxMap::AddPosNode(cLuxNode_Pos *apNode)
{
	ohpl_a7_pos_add(mpA7, apNode, OHPL_A7_S(apNode->GetName()));
}

cLuxNode_Pos *cLuxMap::GetPosNode(const tString & asName)
{
	cLuxNode_Pos *pNode = static_cast<cLuxNode_Pos*>(ohpl_a7_pos_find(mpA7, OHPL_A7_S(asName)));
	if(pNode == NULL) Warning("Could not find pos node '%s'\n", asName.c_str());
	return pNode;
}

//-----------------------------------------------------------------------

void cLuxMap::AddUseItemCallback(	const tString& asName, const tString& asItem, const tString& asEntity,
									const tString& asFunction, bool abAutoCallback)
{
	cLuxUseItemCallback *pCallback = hplNew(cLuxUseItemCallback, ());

	pCallback->msName = asName;
	pCallback->msItem = asItem;
	pCallback->msEntity = asEntity;
	pCallback->msFunction = asFunction;
	pCallback->mbAutoDestroy = abAutoCallback;
	
	ohpl_a7_use_add(mpA7, pCallback, OHPL_A7_S(asName), OHPL_A7_S(asItem), OHPL_A7_S(asEntity));
}

void cLuxMap::RemoveUseItemCallback( const tString& asName)
{
	if(ohpl_a7_use_remove_name(mpA7, OHPL_A7_S(asName))==false)
		Warning("Could not find use item callback '%s'!\n", asName.c_str());
}

void cLuxMap::CopyTimers(std::vector<cLuxEventTimer> &avOut)
{
	for(size_t i=0; i<ohpl_a7_timer_len(mpA7); ++i)
	{
		cLuxEventTimer timer;
		bool bDestroy;
		ohpl_a7_timer_at(mpA7, i, ohpl_ga5::Collect, &timer.msName, &timer.msFunction, &timer.mfCount, &bDestroy);
		timer.mbDestroyMe = bDestroy;
		avOut.push_back(timer);
	}
}

void cLuxMap::SetTimers(const std::vector<cLuxEventTimer> &avIn)
{
	ohpl_a7_timers_clear(mpA7);
	for(size_t i=0; i<avIn.size(); ++i)
	{
		const cLuxEventTimer &t = avIn[i];
		ohpl_a7_timer_push(mpA7, OHPL_A7_S(t.msName), OHPL_A7_S(t.msFunction), t.mfCount, t.mbDestroyMe);
	}
}

void cLuxMap::CopyUseItemCallbacks(std::vector<cLuxUseItemCallback> &avOut)
{
	for(size_t i=0; i<ohpl_a7_use_len(mpA7); ++i) avOut.push_back(*static_cast<cLuxUseItemCallback*>(ohpl_a7_use_at(mpA7, i)));
}

void cLuxMap::SetUseItemCallbacks(const std::vector<cLuxUseItemCallback> &avIn)
{
	ohpl_a7_use_clear(mpA7);
	for(size_t i=0; i<avIn.size(); ++i)
	{
		cLuxUseItemCallback *pCallback = hplNew(cLuxUseItemCallback, ());
		*pCallback = avIn[i];
		ohpl_a7_use_add(mpA7, pCallback, OHPL_A7_S(pCallback->msName), OHPL_A7_S(pCallback->msItem), OHPL_A7_S(pCallback->msEntity));
	}
}


cLuxScriptVar* cLuxMap::GetVar(const tString &asName)
{
	tLuxScriptVarMapIt it = m_mapVars.find(asName);
	if(it != m_mapVars.end()) return &(it->second);

	m_mapVars.insert(tLuxScriptVarMap::value_type(asName, cLuxScriptVar(asName)));
	it = m_mapVars.find(asName);
	return &(it->second);
}
