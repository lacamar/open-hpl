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

#include "LuxPlayerState_InteractPush.h"

#include "LuxPlayer.h"
#include "LuxMapHandler.h"
#include "LuxMap.h"
#include "LuxMapHelper.h"
#include "LuxProp.h"
#include "LuxMoveState_Normal.h"

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxPlayerState_InteractPush::cLuxPlayerState_InteractPush(cLuxPlayer *apPlayer) : iLuxPlayerState_Interact(apPlayer, eLuxPlayerState_InteractPush, &mR.base)
{
	OhplA3Host h = A3HostTable();
	ohpl_a3_init(&mR.base, &h, eLuxPlayerState_InteractPush);
}

//-----------------------------------------------------------------------

cLuxPlayerState_InteractPush::~cLuxPlayerState_InteractPush()
{
	
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxPlayerState_InteractPush::OnEnterState(eLuxPlayerState aPrevState)
{
	OhplA3Host h = A3HostTable();
	ohpl_a3_enter(mpA3, &h, aPrevState);
}

//-----}

//-----------------------------------------------------------------------

void cLuxPlayerState_InteractPush::OnLeaveState(eLuxPlayerState aNewState)
{
	OhplA3Host h = A3HostTable();
	ohpl_a3_leave(mpA3, &h, aNewState);
}

//-----------------------------------------------------------------------

void cLuxPlayerState_InteractPush::Update(float afTimeStep)
{
	OhplA3Host h = A3HostTable();
	ohpl_a3_update(mpA3, &h, afTimeStep);
}

void cLuxPlayerState_InteractPush::PostUpdate(float afTimeStep)
{
	OhplA3Host h = A3HostTable();
	ohpl_a3_post_update(mpA3, &h, afTimeStep);
}

//-----------------------------------------------------------------------

bool cLuxPlayerState_InteractPush::OnDoAction(eLuxPlayerAction aAction,bool abPressed)
{
	OhplA3Host h = A3HostTable();
	return ohpl_a3_do_action(mpA3, &h, aAction, abPressed);
}

//-----------------------------------------------------------------------

void cLuxPlayerState_InteractPush::OnScroll(float afAmount)
{
	
}

//-----------------------------------------------------------------------

bool cLuxPlayerState_InteractPush::OnMove(eCharDir aDir, float afMul)
{
	OhplA3Host h = A3HostTable();
	return ohpl_a3_on_move(mpA3, &h, aDir, afMul);
}

//-----------------------------------------------------------------------

bool cLuxPlayerState_InteractPush::OnAddYaw(float afAmount)
{
	return true;
}

bool cLuxPlayerState_InteractPush::OnAddPitch(float afAmount)
{
	return true;
}

//-----------------------------------------------------------------------

cGuiGfxElement* cLuxPlayerState_InteractPush::GetCrosshair()
{
	return NULL;
}

//-----------------------------------------------------------------------

float cLuxPlayerState_InteractPush::DrawDebug(cGuiSet *apSet,iFontData *apFont, float afStartY)
{
	OhplA3Host h = A3HostTable();
	return ohpl_a3_draw_debug(mpA3, &h, apSet, apFont, afStartY);
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// SAVE DATA STUFF
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

kBeginSerialize(cLuxPlayerState_InteractPush_SaveData, iLuxPlayerState_Interact_SaveData)
kEndSerialize()

//-----------------------------------------------------------------------

iLuxPlayerState_SaveData* cLuxPlayerState_InteractPush::CreateSaveData()
{
	return hplNew(cLuxPlayerState_InteractPush_SaveData, ());
}

//-----------------------------------------------------------------------


void cLuxPlayerState_InteractPush::SaveToSaveData(iLuxPlayerState_SaveData* apSaveData)
{
	///////////////////////
	// Init
	super_class::SaveToSaveData(apSaveData);
	cLuxPlayerState_InteractPush_SaveData *pData = static_cast<cLuxPlayerState_InteractPush_SaveData*>(apSaveData);


	///////////////////////
	// Save vars
}

//-----------------------------------------------------------------------

void cLuxPlayerState_InteractPush::LoadFromSaveDataBeforeEnter(cLuxMap *apMap, iLuxPlayerState_SaveData* apSaveData)
{
	///////////////////////
	// Init
	super_class::LoadFromSaveDataBeforeEnter(apMap,apSaveData);
	cLuxPlayerState_InteractPush_SaveData *pData = static_cast<cLuxPlayerState_InteractPush_SaveData*>(apSaveData);

	///////////////////////
	// Load vars
}

//-----------------------------------------------------------------------

void cLuxPlayerState_InteractPush::LoadFromSaveDataAfterEnter(cLuxMap *apMap, iLuxPlayerState_SaveData* apSaveData)
{
	///////////////////////
	// Init
	super_class::LoadFromSaveDataAfterEnter(apMap,apSaveData);
	cLuxPlayerState_InteractPush_SaveData *pData = static_cast<cLuxPlayerState_InteractPush_SaveData*>(apSaveData);

	///////////////////////
	// Load vars
}

//-----------------------------------------------------------------------





