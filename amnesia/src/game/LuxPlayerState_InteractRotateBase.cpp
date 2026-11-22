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

#include "LuxPlayerState_InteractRotateBase.h"

#include "LuxPlayer.h"
#include "LuxMapHandler.h"
#include "LuxMap.h"
#include "LuxMapHelper.h"
#include "LuxProp.h"
#include "LuxMoveState_Normal.h"
#include "LuxInputHandler.h"

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

iLuxPlayerState_InteractRotateBase::iLuxPlayerState_InteractRotateBase(cLuxPlayer *apPlayer, eLuxPlayerState aState) 
: iLuxPlayerState_Interact(apPlayer, aState, &mR.base)
{
	OhplA3Host h = A3HostTable();
	ohpl_a3_init(&mR.base, &h, aState);
}

//-----------------------------------------------------------------------

iLuxPlayerState_InteractRotateBase::~iLuxPlayerState_InteractRotateBase()
{
	ohpl_a3_free(&mR.base);
	
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void iLuxPlayerState_InteractRotateBase::OnEnterState(eLuxPlayerState aPrevState)
{
	OhplA3Host h = A3HostTable();
	ohpl_a3_enter(mpA3, &h, aPrevState);
}

//-----------------------------------------------------------------------

void iLuxPlayerState_InteractRotateBase::OnLeaveState(eLuxPlayerState aNewState)
{
	OhplA3Host h = A3HostTable();
	ohpl_a3_leave(mpA3, &h, aNewState);
}

//-----------------------------------------------------------------------

//-----------------------------------------------------------------------

void iLuxPlayerState_InteractRotateBase::Update(float afTimeStep)
{
	OhplA3Host h = A3HostTable();
	ohpl_a3_update(mpA3, &h, afTimeStep);
}


bool iLuxPlayerState_InteractRotateBase::OnDoAction(eLuxPlayerAction aAction,bool abPressed)
{
	OhplA3Host h = A3HostTable();
	return ohpl_a3_do_action(mpA3, &h, aAction, abPressed);
}

//-----------------------------------------------------------------------

void iLuxPlayerState_InteractRotateBase::OnScroll(float afAmount)
{
	
}

//-----------------------------------------------------------------------

bool iLuxPlayerState_InteractRotateBase::OnMove(eCharDir aDir, float afMul)
{
	OhplA3Host h = A3HostTable();
	return ohpl_a3_on_move(mpA3, &h, aDir, afMul);
}

//-----------------------------------------------------------------------

bool iLuxPlayerState_InteractRotateBase::OnAddYaw(float afAmount)
{
	OhplA3Host h = A3HostTable();
	return ohpl_a3_add_yaw(mpA3, &h, afAmount);
}

bool iLuxPlayerState_InteractRotateBase::OnAddPitch(float afAmount)
{
	OhplA3Host h = A3HostTable();
	return ohpl_a3_add_pitch(mpA3, &h, afAmount);
}

//-----------------------------------------------------------------------

cGuiGfxElement* iLuxPlayerState_InteractRotateBase::GetCrosshair()
{
	return NULL;
}

//-----------------------------------------------------------------------

float iLuxPlayerState_InteractRotateBase::DrawDebug(cGuiSet *apSet,iFontData *apFont, float afStartY)
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

kSerializeRustClassV(iLuxPlayerState_InteractRotateBase_SaveData, iLuxPlayerState_Interact_SaveData)

//-----------------------------------------------------------------------
