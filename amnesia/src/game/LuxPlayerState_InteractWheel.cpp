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

#include "LuxPlayerState_InteractWheel.h"

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

cLuxPlayerState_InteractWheel::cLuxPlayerState_InteractWheel(cLuxPlayer *apPlayer) : iLuxPlayerState_InteractRotateBase(apPlayer, eLuxPlayerState_InteractWheel)
{
	mRotatePid.p = 5.0f;
	mRotatePid.i = 0.0f;
	mRotatePid.d = 0.1f;
}

//-----------------------------------------------------------------------

cLuxPlayerState_InteractWheel::~cLuxPlayerState_InteractWheel()
{
	
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxPlayerState_InteractWheel::EnterRotateBase(eLuxPlayerState aPrevState)
{
	mlstMoveAdds.clear();
}

//-----------------------------------------------------------------------

void cLuxPlayerState_InteractWheel::LeaveRotateBase(eLuxPlayerState aNewState)
{

}

//-----------------------------------------------------------------------

float cLuxPlayerState_InteractWheel::GetSpeedAdd(cCamera *apCam)
{
	mlstMoveAdds.push_back(mvMouseAdd);
	if(ohpl_ga2b_wheel_pop(mlstMoveAdds.size())) mlstMoveAdds.pop_front();

	std::vector<cVector2f> vHistory(mlstMoveAdds.begin(), mlstMoveAdds.end());
	cVector3f vPinDir = mpCurrentJoint->GetPinDir();
	cVector3f vFwd = mpPlayer->GetCamera()->GetForward();
	return ohpl_ga2b_wheel_speed_add(vHistory.data(), vHistory.size(), &mvMouseAdd, &vPinDir, &vFwd);
}

//-----------------------------------------------------------------------

void cLuxPlayerState_InteractWheel::OnThrow()
{

}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// SAVE DATA STUFF
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

kSerializeRustClassP(cLuxPlayerState_InteractWheel_SaveData, iLuxPlayerState_InteractRotateBase_SaveData)

//-----------------------------------------------------------------------

iLuxPlayerState_SaveData* cLuxPlayerState_InteractWheel::CreateSaveData()
{
	return hplNew(cLuxPlayerState_InteractWheel_SaveData, ());
}

//-----------------------------------------------------------------------
