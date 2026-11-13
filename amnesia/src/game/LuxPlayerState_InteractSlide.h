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

#ifndef LUX_PLAYER_STATE_INTERACT_SLIDE_H
#define LUX_PLAYER_STATE_INTERACT_SLIDE_H

//----------------------------------------------

#include "LuxPlayerState_Interact.h"

//----------------------------------------------

class cLuxPlayer;

class cLuxInteractData_Slide;

//------------------------------------

class cLuxPlayerState_InteractSlide_SaveData : public iLuxPlayerState_Interact_SaveData
{
	kSerializableClassInit(cLuxPlayerState_InteractSlide_SaveData)
public:
};

//------------------------------------


class cLuxPlayerState_InteractSlide : private OhplA3Holder<OhplA3Slide>, public iLuxPlayerState_Interact
{
typedef iLuxPlayerState_Interact super_class;
public:	
	cLuxPlayerState_InteractSlide(cLuxPlayer *apPlayer);
	virtual ~cLuxPlayerState_InteractSlide();

	void OnEnterState(eLuxPlayerState aPrevState);
	void OnLeaveState(eLuxPlayerState aNewState);

	void Update(float afTimeStep);
	void PostUpdate(float afTimeStep);

	bool OnDoAction(eLuxPlayerAction aAction,bool abPressed);

	bool OnAddYaw(float afAmount);
	bool OnAddPitch(float afAmount);

	bool OnMove(eCharDir aDir, float afMul);

	cGuiGfxElement* GetCrosshair();

	void OnSaveBody(iPhysicsBody *apBody, float &afMass, bool &abCollideCharacter);

	float DrawDebug(cGuiSet *apSet,iFontData *apFont, float afStartY);
	
	void RenderSolid(cRendererCallbackFunctions* apFunctions);

	/////////////////////////////////
	//Save data stuff
	virtual bool IsSaved(){ return true; }
	iLuxPlayerState_SaveData* CreateSaveData();

	void SaveToSaveData(iLuxPlayerState_SaveData* apSaveData);
	void LoadFromSaveDataBeforeEnter(cLuxMap *apMap,iLuxPlayerState_SaveData* apSaveData);
	void LoadFromSaveDataAfterEnter(cLuxMap *apMap, iLuxPlayerState_SaveData* apSaveData);

protected:
	cLuxInteractData_Slide *&mpSlideData = mR.data;

	OhplGa2bPid3 &mForcePid = mR.forcePid;

	iPhysicsJoint *&mpCurrentJoint = mR.joint;
	bool &mbHasGravity = mR.hasGravity;

	float &mfMaxDistance = mR.maxDistance;
	cVector3f &mvLocalInteractPos = mR.localInteractPos;

	cVector2f &mvMouseAdd = mR.mouseAdd;
	cVector2f &mvLastMouseAdd = mR.lastMouseAdd;
	cVector3f &mvLastForce = mR.lastForce;

	float &mfSlideSpeed = mR.slideSpeed;

	float &mfMoveToMouseAddFactor = mR.moveToMouseFactor;
	float &mfMaxForce = mR.maxForce;
	
};

//----------------------------------------------


#endif // LUX_PLAYER_STATE_INTERACT_SLIDE_H
