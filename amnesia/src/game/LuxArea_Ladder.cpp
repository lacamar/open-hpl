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

#include "LuxArea_Ladder.h"
#include "RustLuxMap.h"

#include "LuxMap.h"
#include "LuxPlayer.h"
#include "LuxPlayerState.h"
#include "LuxEffectHandler.h"
#include "LuxHelpFuncs.h"
#include "LuxCompletionCountHandler.h"
#include "LuxMessageHandler.h"
#include "LuxMapHandler.h"
#include "LuxMap.h"


//////////////////////////////////////////////////////////////////////////
// LOADER
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxAreaLoader_Ladder::cLuxAreaLoader_Ladder(const tString& asName) : iLuxAreaLoader(asName)
{

}

cLuxAreaLoader_Ladder::~cLuxAreaLoader_Ladder()
{

}

//-----------------------------------------------------------------------

iLuxArea *cLuxAreaLoader_Ladder::CreateArea(const tString& asName, int alID, cLuxMap *apMap)
{
	cLuxArea_Ladder *pArea = hplNew(cLuxArea_Ladder, (asName, alID, apMap));
	return pArea;
}

//-----------------------------------------------------------------------

void cLuxAreaLoader_Ladder::LoadVariables(iLuxArea *apArea, cWorld *apWorld)
{
	cLuxArea_Ladder *pLadderArea = static_cast<cLuxArea_Ladder*>(apArea);
	
	pLadderArea->msMaterial = GetVarString("Material","metal");
}

void cLuxAreaLoader_Ladder::SetupArea(iLuxArea *apArea, cWorld *apWorld)
{

}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cLuxArea_Ladder::cLuxArea_Ladder(const tString &asName, int alID, cLuxMap *apMap)  : iLuxArea(asName,alID,apMap, eLuxAreaType_Ladder)
{
	mfMaxFocusDistance = gpBase->mpGameCfg->GetFloat("Player_Interaction","Ladder_MaxFocusDist",0);
}

//-----------------------------------------------------------------------

cLuxArea_Ladder::~cLuxArea_Ladder()
{
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cLuxArea_Ladder::SetupAfterLoad(cWorld *apWorld)
{
	/////////////////////////////////
	//Generate some variables based on the area body
	cMatrixf mtxWorld = mpBody->GetWorldMatrix();
	OhplGa5LadderSetup setup;
	ohpl_ga5_ladder_setup(reinterpret_cast<const OhplGa5Mtx*>(&mtxWorld), mpBody->GetWorldPosition().y, mpBody->GetShape()->GetSize().y, &setup);
	mvForward = cVector3f(setup.forward.x, setup.forward.y, setup.forward.z);
	mfMaxY = setup.maxY;
	mfMinY = setup.minY;
}

//-----------------------------------------------------------------------

void cLuxArea_Ladder::OnUpdate(float afTimeStep)
{
}

//-----------------------------------------------------------------------

bool cLuxArea_Ladder::CanInteract(iPhysicsBody *apBody)
{
	return true;
}

//-----------------------------------------------------------------------

static bool LadderFits(void *apCtx, const OhplGa5Vec3 *apPos)
{
	iCharacterBody *pCharBody = static_cast<iCharacterBody*>(apCtx);
	return pCharBody->CheckCharacterFits(cVector3f(apPos->x, apPos->y, apPos->z), false,0,NULL,kEpsilonf);
}

bool cLuxArea_Ladder::OnInteract(iPhysicsBody *apBody, const cVector3f &avPos)
{
	///////////////////////////////
	//Check if the pos in front of the ladder is free.
	iCharacterBody *pCharBody = gpBase->mpPlayer->GetCharacterBody();
	iPhysicsWorld *pWorld = gpBase->mpMapHandler->GetCurrentMap()->GetPhysicsWorld();

	cVector3f vStartPos = GetStartPosition();

	//Check higher and higher positions until a free is found.
	//If not found, the skip
	if(ohpl_ga5_ladder_find_start(reinterpret_cast<OhplGa5Vec3*>(&vStartPos), mfMaxY, LadderFits, pCharBody)==false)
	{
		//TODO: Message?
		return false;
	}

	////////////////////////////
	// Change state
	cLuxPlayerStateVars::SetupLadder(this, vStartPos);
	gpBase->mpPlayer->ChangeState(eLuxPlayerState_Ladder);

	return true;
}

//-----------------------------------------------------------------------

eLuxFocusCrosshair cLuxArea_Ladder::GetFocusCrosshair(iPhysicsBody *apBody, const cVector3f &avPos)
{
	return eLuxFocusCrosshair_Ladder;	
}

//-----------------------------------------------------------------------

cVector3f cLuxArea_Ladder::GetStartRotation()
{
	return cMath::GetAngleFromPoints3D(cVector3f(0,0,0), GetForward()*-1);
}

//-----------------------------------------------------------------------

cVector3f cLuxArea_Ladder::GetStartPosition()
{
	iCharacterBody *pCharBody = gpBase->mpPlayer->GetCharacterBody();
	cVector3f vCharSize = pCharBody->GetShape(0)->GetSize();
	cVector3f vPos = pCharBody->GetPosition();
	cVector3f vLadderPos = mpBody->GetWorldPosition();
	cVector3f vForward = GetForward();

	cVector3f vResult;
	ohpl_ga5_ladder_start_pos(reinterpret_cast<const OhplGa5Vec3*>(&vLadderPos), reinterpret_cast<const OhplGa5Vec3*>(&vForward), reinterpret_cast<const OhplGa5Vec3*>(&vCharSize), vPos.y, mfMaxY, mfMinY, reinterpret_cast<OhplGa5Vec3*>(&vResult));
	return vResult;
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

kBeginSerialize(cLuxArea_Ladder_SaveData, iLuxArea_SaveData)

kSerializeVar(msMaterial, eSerializeType_String)

kEndSerialize()

//-----------------------------------------------------------------------

iLuxArea* cLuxArea_Ladder_SaveData::CreateArea(cLuxMap *apMap)
{
	return hplNew(cLuxArea_Ladder, (msName, mlID, apMap));
}

//-----------------------------------------------------------------------

iLuxEntity_SaveData* cLuxArea_Ladder::CreateSaveData()
{
	return hplNew(cLuxArea_Ladder_SaveData, ());
}

//-----------------------------------------------------------------------

void cLuxArea_Ladder::SaveToSaveData(iLuxEntity_SaveData* apSaveData)
{
	super_class::SaveToSaveData(apSaveData);
	cLuxArea_Ladder_SaveData *pData = static_cast<cLuxArea_Ladder_SaveData*>(apSaveData);

    kCopyToVar(pData, msMaterial);
}

//-----------------------------------------------------------------------

void cLuxArea_Ladder::LoadFromSaveData(iLuxEntity_SaveData* apSaveData)
{
	super_class::LoadFromSaveData(apSaveData);
	cLuxArea_Ladder_SaveData *pData = static_cast<cLuxArea_Ladder_SaveData*>(apSaveData);

	kCopyFromVar(pData, msMaterial);
}

//-----------------------------------------------------------------------

void cLuxArea_Ladder::SetupSaveData(iLuxEntity_SaveData *apSaveData)
{
	super_class::SetupSaveData(apSaveData);

}

//-----------------------------------------------------------------------

