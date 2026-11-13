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

#ifndef LUX_PLAYER_STATE_INTERACT_PUSH_H
#define LUX_PLAYER_STATE_INTERACT_PUSH_H

//----------------------------------------------

#include "LuxPlayerState_Interact.h"

//----------------------------------------------

class cLuxInteractData_Push;

//------------------------------------

class cLuxPlayerState_InteractPush_SaveData : public iLuxPlayerState_Interact_SaveData
{
	kSerializableClassInit(cLuxPlayerState_InteractPush_SaveData)
public:

};

//----------------------------------------------

class cLuxPlayerState_InteractPush : private OhplA3Holder<OhplA3Push>, public iLuxPlayerState_Interact
{
typedef iLuxPlayerState_Interact super_class;
public:	
	cLuxPlayerState_InteractPush(cLuxPlayer *apPlayer);
	virtual ~cLuxPlayerState_InteractPush();

	void OnEnterState(eLuxPlayerState aPrevState);
	void OnLeaveState(eLuxPlayerState aNewState);

	void Update(float afTimeStep);
	void PostUpdate(float afTimeStep);

	bool OnDoAction(eLuxPlayerAction aAction,bool abPressed);

	void OnScroll(float afAmount);

	bool OnMove(eCharDir aDir, float afMul);

	bool OnAddYaw(float afAmount);
	bool OnAddPitch(float afAmount);

	bool OnRun(bool abPressed){ return true;}
	bool OnJump(bool abPressed){ return false;}

	cGuiGfxElement* GetCrosshair();

	void OnSaveBody(iPhysicsBody *apBody, float &afMass, bool &abCollideCharacter){}

	float DrawDebug(cGuiSet *apSet,iFontData *apFont, float afStartY);

	/////////////////////////////////
	//Save data stuff
	virtual bool IsSaved(){ return false; }
	iLuxPlayerState_SaveData* CreateSaveData();

	void SaveToSaveData(iLuxPlayerState_SaveData* apSaveData);
	void LoadFromSaveDataBeforeEnter(cLuxMap *apMap,iLuxPlayerState_SaveData* apSaveData);
	void LoadFromSaveDataAfterEnter(cLuxMap *apMap, iLuxPlayerState_SaveData* apSaveData);

protected:
	void ClearMoveVars();

	cLuxInteractData_Push *&mpPushData = mR.data;

	float (&mfTotalMoveMul)[2] = mR.totalMoveMul;
	bool (&mbMoving)[2] = mR.moving;

	float (&mfMaxSpeedInDir)[2] = mR.maxSpeedInDir;

	OhplGa2bPid3 &mVelocityPid = mR.velocityPid;
	OhplGa2bPid3 &mStopVelocityPid = mR.stopPid;
	
	cVector3f &mvLocalAttachPos = mR.localAttach;

	cVector3f &mvPushDir = mR.pushDir;
	float &mfMaxAttachPointDist = mR.maxAttachDist;

	float &mfWalkMaxSpeed = mR.walkMaxSpeed;
	float &mfWalkForce = mR.walkForce;
	float &mfRunMaxSpeed = mR.runMaxSpeed;
	float &mfRunForce = mR.runForce;
	float &mfCrouchMaxSpeed = mR.crouchMaxSpeed;
	float &mfCrouchForce = mR.crouchForce;

	float &mfPitchMinRange = mR.pitchMinRange;
	float &mfPitchMaxRange = mR.pitchMaxRange;
	float &mfYawRange = mR.yawRange;

	float &mfPrevMinPitchLimit = mR.prevMinPitch;
	float &mfPrevMaxPitchLimit = mR.prevMaxPitch;

	float &mfMaxForce = mR.maxForce;
};

//----------------------------------------------


#endif // LUX_PLAYER_STATE_INTERACT_GRAB_H
