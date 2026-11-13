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

#include "LuxPlayerState.h"

#include "LuxPlayer.h"
#include "LuxProp.h"
#include "LuxMapHandler.h"
#include "LuxMap.h"
#include "LuxMapHelper.h"
#include "LuxEffectRenderer.h"
#include "LuxInputHandler.h"
#include "LuxPlayerHands.h"
#include "LuxPlayerHelpers.h"
#include "LuxMoveState_Normal.h"
#include "LuxArea_Ladder.h"
#include "LuxHelpFuncs.h"
#include "LuxMessageHandler.h"
#include "LuxInventory.h"
#include "impl/RustBackends.h"

//////////////////////////////////////////////////////////////////////////
// SETUP VARIABLES
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

///////////////////////
//General
tString cLuxPlayerStateVars::msHandObject = "";

///////////////////////
//Interaction
cVector3f cLuxPlayerStateVars::mvInteractPos;
iPhysicsBody *cLuxPlayerStateVars::mpInteractBody = NULL;
iLuxProp *cLuxPlayerStateVars::mpInteractProp = NULL;

///////////////////////
//Use Item
cLuxInventory_Item *cLuxPlayerStateVars::mpInventoryItem  = NULL;

///////////////////////
//Ladder
cLuxArea_Ladder *cLuxPlayerStateVars::mpLadder = NULL;
cVector3f cLuxPlayerStateVars::mvLadderStartPos = 0;
//-----------------------------------------------------------------------

void cLuxPlayerStateVars::SetupInteraction(iPhysicsBody *apBody, const cVector3f &avInteractPos)
{
	mpInteractBody = apBody;
	
	iLuxEntity *pEntity = (iLuxEntity*)apBody->GetUserData();
	if(pEntity && pEntity->GetEntityType() == eLuxEntityType_Prop)
	{
		mpInteractProp = static_cast<iLuxProp*>(pEntity);
	}

	mvInteractPos = avInteractPos;
}

//-----------------------------------------------------------------------



//-----------------------------------------------------------------------

static cLuxMap *gpA3CbMap;
static tString gsA3CbName;

static const char *const gA3LadderSounds[4] = { "attach", "up", "down", "dettach" };

int32_t iLuxPlayerState::A3Host(void *apCtx, uint32_t alOp, OhplA3Io *apIo)
{
	iLuxPlayerState *pState = (iLuxPlayerState*)apCtx;
	cLuxPlayer *pPlayer = pState->mpPlayer;
	OhplA3Io &io = *apIo;
	iPhysicsBody *pBody = (iPhysicsBody*)io.p[0];
	iLuxEntity *pEntity = (iLuxEntity*)io.p[0];
	iLuxProp *pProp = (iLuxProp*)io.p[0];
	cVector3f v(io.f[0], io.f[1], io.f[2]);
	cVector3f v2(io.f[3], io.f[4], io.f[5]);

	switch(alOp)
	{
	case kOhplA3Op_PREV_STATE: return pState->mPreviousState;
	case kOhplA3Op_CHANGE_STATE: pPlayer->ChangeState((eLuxPlayerState)io.i[0]); break;
	case kOhplA3Op_CHANGE_MOVE_STATE: pPlayer->ChangeMoveState((eLuxMoveState)io.i[0]); break;
	case kOhplA3Op_SET_INTERACT_MUL: pPlayer->SetInteractionMoveSpeedMul(io.f[0]); break;
	case kOhplA3Op_DEFAULT_MASS: io.f[0] = pPlayer->GetDefaultMass(); break;
	case kOhplA3Op_HAND_DRAWN: pPlayer->SetCurrentHandObjectDrawn(io.i[0] != 0); break;
	case kOhplA3Op_SET_FOCUS:
		pPlayer->SetEntityInFocus((iLuxEntity*)io.p[0]);
		pPlayer->SetBodyInFocus((iPhysicsBody*)io.p[1]);
		pPlayer->SetCurrentFocusDistance(io.f[0]);
		break;
	case kOhplA3Op_SET_FOCUS_TEXT:
		if(pEntity) pPlayer->SetFocusText(pEntity->GetFocusText());
		else pPlayer->SetFocusText(_W(""));
		break;
	case kOhplA3Op_ICON_STYLE_DEFAULT: return pPlayer->GetFocusIconStyle() == eLuxFocusIconStyle_Default;
	case kOhplA3Op_MOVE_ALLOWS_INTERACTION: return pPlayer->GetCurrentMoveStateData()->GetAllowsInteraction();
	case kOhplA3Op_HANDS_DO_ACTION: pPlayer->GetHands()->DoAction((eLuxPlayerAction)io.i[0], io.i[1] != 0); break;
	case kOhplA3Op_CUR_MOVE_STATE: return pPlayer->GetCurrentMoveState();
	case kOhplA3Op_MOVE_NORMAL_GET:
	{
		cLuxMoveState_Normal *pMove = static_cast<cLuxMoveState_Normal*>(pPlayer->GetMoveStateData(eLuxMoveState_Normal));
		return io.i[0] == 0 ? pMove->IsRunning() : pMove->IsCrouching();
	}
	case kOhplA3Op_MOVE_NORMAL_CMD:
	{
		cLuxMoveState_Normal *pMove = static_cast<cLuxMoveState_Normal*>(pPlayer->GetMoveStateData(eLuxMoveState_Normal));
		if(io.i[0] == 0) pMove->SetCrouch(false);
		else if(io.i[0] == 1) pMove->ResetJumping();
		else pMove->Jump();
		break;
	}
	case kOhplA3Op_LANTERN_ACTIVE: return pPlayer->GetHelperLantern()->IsActive();
	case kOhplA3Op_LANTERN_SET: pPlayer->GetHelperLantern()->SetActive(io.i[0] != 0, true, true, false); break;

	case kOhplA3Op_CAM_V3:
	{
		cCamera *pCam = pPlayer->GetCamera();
		cVector3f r = io.i[0] == 0 ? pCam->GetPosition() : io.i[0] == 1 ? pCam->GetForward() : io.i[0] == 2 ? pCam->GetUp() : pCam->GetRight();
		io.f[0] = r.x; io.f[1] = r.y; io.f[2] = r.z;
		break;
	}
	case kOhplA3Op_CAM_ANGLES:
		io.f[0] = pPlayer->GetCamera()->GetPitch();
		io.f[1] = pPlayer->GetCamera()->GetYaw();
		io.f[2] = pPlayer->GetCamera()->GetRoll();
		break;
	case kOhplA3Op_CAM_PITCH_LIMITS:
		io.f[0] = pPlayer->GetCamera()->GetPitchMinLimit();
		io.f[1] = pPlayer->GetCamera()->GetPitchMaxLimit();
		break;
	case kOhplA3Op_CAM_ADD_PITCH: pPlayer->GetCamera()->AddPitch(io.f[0]); break;
	case kOhplA3Op_CAM_ADD_YAW: pPlayer->GetCamera()->AddYaw(io.f[0]); break;
	case kOhplA3Op_CAM_SET_PITCH_LIMITS: pPlayer->GetCamera()->SetPitchLimits(io.f[0], io.f[1]); break;
	case kOhplA3Op_CAM_SET_YAW_LIMITS: pPlayer->GetCamera()->SetYawLimits(io.f[0], io.f[1]); break;

	case kOhplA3Op_CB_V3:
	{
		iCharacterBody *pChar = pPlayer->GetCharacterBody();
		cVector3f r = io.i[0] == 0 ? pChar->GetFeetPosition() : io.i[0] == 1 ? pChar->GetForward() : io.i[0] == 2 ? pChar->GetRight() :
					io.i[0] == 3 ? pChar->GetPosition() : pChar->GetSize();
		io.f[0] = r.x; io.f[1] = r.y; io.f[2] = r.z;
		break;
	}
	case kOhplA3Op_CB_SET_POS: pPlayer->GetCharacterBody()->SetPosition(v); break;
	case kOhplA3Op_CB_SET_YAW: pPlayer->GetCharacterBody()->SetYaw(io.f[0]); break;
	case kOhplA3Op_CB_SET_MASS: pPlayer->GetCharacterBody()->SetMass(io.f[0]); break;
	case kOhplA3Op_CB_FLAG:
		if(io.i[0] == 0) pPlayer->GetCharacterBody()->SetGravityActive(io.i[1] != 0);
		else if(io.i[0] == 1) pPlayer->GetCharacterBody()->SetTestCollision(io.i[1] != 0);
		else pPlayer->GetCharacterBody()->SetDisableDiagSpeedBoost(io.i[1] != 0);
		break;
	case kOhplA3Op_CB_CMD:
		if(io.i[0] == 0) pPlayer->GetCharacterBody()->StopMovement();
		else pPlayer->GetCharacterBody()->ResetClimbing();
		break;
	case kOhplA3Op_CB_FITS: return pPlayer->GetCharacterBody()->CheckCharacterFits(v, false, 0, NULL, kEpsilonf);
	case kOhplA3Op_CB_RAY: return pPlayer->GetCharacterBody()->CheckRayIntersection(v, v2, NULL, NULL);
	case kOhplA3Op_CB_MOVE: pPlayer->GetCharacterBody()->Move((eCharDir)io.i[0], io.f[0]); break;
	case kOhplA3Op_CB_MAX_SPEED:
		pPlayer->GetCharacterBody()->SetMaxPositiveMoveSpeed((eCharDir)io.i[0], io.f[0]);
		pPlayer->GetCharacterBody()->SetMaxNegativeMoveSpeed((eCharDir)io.i[0], io.f[1]);
		break;
	case kOhplA3Op_CB_SHAPE_COLLIDE:
	{
		iPhysicsWorld *pWorld = gpBase->mpMapHandler->GetCurrentMap()->GetWorld()->GetPhysicsWorld();
		iCharacterBody *pChar = pPlayer->GetCharacterBody();
		cMatrixf mtxNext;
		for(int r=0; r<4; ++r) for(int c=0; c<4; ++c) mtxNext.m[r][c] = io.f[3 + r*4 + c];
		cCollideData collideData;
		collideData.SetMaxSize(1);
		return pWorld->CheckShapeCollision(pChar->GetCurrentShape(), cMath::MatrixTranslate(v), pBody->GetShape(), mtxNext, collideData, 1, false);
	}
	case kOhplA3Op_CB_EXISTS: return pPlayer->GetCharacterBody() != NULL;

	case kOhplA3Op_BODY_MASS: io.f[0] = pBody->GetMass(); break;
	case kOhplA3Op_BODY_V3:
	{
		cVector3f r = io.i[0] == 0 ? pBody->GetLinearVelocity() : io.i[0] == 1 ? pBody->GetAngularVelocity() :
					io.i[0] == 2 ? pBody->GetLocalPosition() : pBody->GetMassCentre();
		io.f[0] = r.x; io.f[1] = r.y; io.f[2] = r.z;
		break;
	}
	case kOhplA3Op_BODY_MATRIX:
	{
		cMatrixf m = io.i[0] == 0 ? pBody->GetLocalMatrix() : pBody->GetInertiaMatrix();
		for(int r=0; r<4; ++r) for(int c=0; c<4; ++c) io.f[r*4 + c] = m.m[r][c];
		break;
	}
	case kOhplA3Op_BODY_VEL_AT:
	{
		cVector3f r = pBody->GetVelocityAtPosition(v);
		io.f[0] = r.x; io.f[1] = r.y; io.f[2] = r.z;
		break;
	}
	case kOhplA3Op_BODY_FLAG: return io.i[0] == 0 ? pBody->GetGravity() : pBody->GetCollideCharacter();
	case kOhplA3Op_BODY_SET_V3:
		if(io.i[0] == 0) pBody->SetLinearVelocity(v);
		else pBody->SetAngularVelocity(v);
		break;
	case kOhplA3Op_BODY_SET_MASS: pBody->SetMass(io.f[0]); break;
	case kOhplA3Op_BODY_SET_FLAG:
		if(io.i[0] == 0) pBody->SetGravity(io.i[1] != 0);
		else pBody->SetCollideCharacter(io.i[1] != 0);
		break;
	case kOhplA3Op_BODY_ADD:
		switch(io.i[0])
		{
		case 0: pBody->AddForce(v); break;
		case 1: pBody->AddTorque(v); break;
		case 2: pBody->AddImpulse(v); break;
		case 3: pBody->AddForceAtPosition(v, v2); break;
		default: pBody->AddImpulseAtPosition(v, v2); break;
		}
		break;
	case kOhplA3Op_BODY_JOINT_NUM: return pBody->GetJointNum();
	case kOhplA3Op_BODY_JOINT: io.p[0] = pBody->GetJoint(io.i[0]); break;
	case kOhplA3Op_BODY_USER_DATA: io.p[0] = pBody->GetUserData(); break;
	case kOhplA3Op_JOINT_V3:
	{
		iPhysicsJoint *pJoint = (iPhysicsJoint*)io.p[0];
		cVector3f r = io.i[0] == 0 ? pJoint->GetPinDir() : pJoint->GetPivotPoint();
		io.f[0] = r.x; io.f[1] = r.y; io.f[2] = r.z;
		break;
	}
	case kOhplA3Op_JOINT_BODY:
	{
		iPhysicsJoint *pJoint = (iPhysicsJoint*)io.p[0];
		io.p[0] = io.i[0] == 0 ? pJoint->GetChildBody() : pJoint->GetParentBody();
		break;
	}

	case kOhplA3Op_ENT_IS_PROP: return pEntity->GetEntityType() == eLuxEntityType_Prop;
	case kOhplA3Op_ENT_MAX_FOCUS: io.f[0] = pEntity->GetMaxFocusDistance(); break;
	case kOhplA3Op_ENT_CAN_INTERACT: return pEntity->CanInteract((iPhysicsBody*)io.p[1]);
	case kOhplA3Op_ENT_DISABLED: return pEntity->GetInteractionDisabled();
	case kOhplA3Op_ENT_CROSSHAIR: return pEntity->GetFocusCrosshair((iPhysicsBody*)io.p[1], v);
	case kOhplA3Op_ENT_ON_INTERACT: pEntity->OnInteract((iPhysicsBody*)io.p[1], v); break;
	case kOhplA3Op_ENT_INTERACT_CB: pEntity->RunInteractCallbackFunc(); break;
	case kOhplA3Op_ENT_FOCUS_DRAW: pEntity->InFocusDraw((cGuiSet*)io.p[1], io.f[0]); break;

	case kOhplA3Op_PROP_IS_ITEM: return pProp->GetPropType() == eLuxPropType_Item;
	case kOhplA3Op_PROP_SET_INTERACTED: pProp->SetIsInteractedWith(io.i[0] != 0); break;
	case kOhplA3Op_PROP_SET_DISABLE_COLL: pProp->SetDisableCollisionUntilOutSidePlayer(io.i[0] != 0); break;
	case kOhplA3Op_PROP_BODY_NUM: return pProp->GetBodyNum();
	case kOhplA3Op_PROP_BODY: io.p[0] = pProp->GetBody(io.i[0]); break;
	case kOhplA3Op_PROP_CONNECTED: return pProp->ShowOutlinesOnConnectedBodies();
	case kOhplA3Op_PROP_EFFECT_MESH: io.p[0] = pProp->GetEffectMeshEntity(); break;
	case kOhplA3Op_PROP_DATA:
		if(io.i[0] == 0) io.p[0] = pProp->GetGrabData();
		else if(io.i[0] == 1) io.p[0] = pProp->GetPushData();
		else if(io.i[0] == 2) io.p[0] = pProp->GetSlideData();
		else io.p[0] = pProp->GetMoveBaseData();
		break;
	case kOhplA3Op_DATA_F:
		if(io.i[0] == 0)
		{
			cLuxInteractData_Grab *pD = (cLuxInteractData_Grab*)io.p[0];
			cVector3f r;
			switch(io.i[1])
			{
			case 0: io.f[0] = pD->mbGrabUseDepth; break;
			case 1: io.f[0] = pD->mbGrabUseOffset; break;
			case 2: io.f[0] = pD->mbUseRotation; break;
			case 3: io.f[0] = pD->mfGrabDepth; break;
			case 4: io.f[0] = pD->mfGrabDepthInc; break;
			case 5: io.f[0] = pD->mfGrabMinDepth; break;
			case 6: io.f[0] = pD->mfGrabMaxDepth; break;
			case 7: io.f[0] = pD->mfGrabThrowImpulse; break;
			case 8: io.f[0] = pD->mfGrabMassMul; break;
			case 9: io.f[0] = pD->mfForceMul; break;
			case 10: io.f[0] = pD->mfTorqueMul; break;
			case 11: r = pD->mvGrabPositionOffset; io.f[0] = r.x; io.f[1] = r.y; io.f[2] = r.z; break;
			default: r = pD->mvGrabRotationOffset; io.f[0] = r.x; io.f[1] = r.y; io.f[2] = r.z; break;
			}
		}
		else if(io.i[0] == 1)
		{
			cLuxInteractData_Push *pD = (cLuxInteractData_Push*)io.p[0];
			io.f[0] = io.i[1] == 0 ? pD->mbPushAtPoint : io.i[1] == 1 ? pD->mfPushForceMul : pD->mfPushImpulse;
		}
		else if(io.i[0] == 2)
		{
			cLuxInteractData_Slide *pD = (cLuxInteractData_Slide*)io.p[0];
			io.f[0] = io.i[1] == 0 ? pD->mfSlideMaxSpeed : io.i[1] == 1 ? pD->mfSlideSlowDownFactor : io.i[1] == 2 ? pD->mfSlideSpeedFactor : pD->mfSlideThrowImpulse;
		}
		else
		{
			iLuxInteractData_RotateBase *pD = (iLuxInteractData_RotateBase*)io.p[0];
			io.f[0] = io.i[1] == 0 ? pD->mfMoveMaxSpeed : io.i[1] == 1 ? pD->mfMoveSlowDownFactor : io.i[1] == 2 ? pD->mfMoveSpeedFactor : pD->mfMoveThrowImpulse;
		}
		break;
	case kOhplA3Op_PROP_AS_ENTITY: io.p[0] = static_cast<iLuxEntity*>(pProp); break;

	case kOhplA3Op_MESH_SUB_NUM: return ((cMeshEntity*)io.p[0])->GetSubMeshEntityNum();
	case kOhplA3Op_MESH_SUB_PARENT: io.p[0] = ((cMeshEntity*)io.p[0])->GetSubMeshEntity(io.i[0])->GetEntityParent(); break;
	case kOhplA3Op_MESH_SUB_OUTLINE: gpBase->mpEffectRenderer->AddOutlineObject(((cMeshEntity*)io.p[0])->GetSubMeshEntity(io.i[0])); break;
	case kOhplA3Op_FX_CLEAR_OUTLINE: gpBase->mpEffectRenderer->ClearOutlineObjects(); break;

	case kOhplA3Op_CLOSEST_ENTITY:
	{
		float fDist;
		iPhysicsBody *pFound;
		iLuxEntity *pEnt;
		gpBase->mpMapHelper->GetClosestEntity(v, v2, io.f[6], &fDist, &pFound, &pEnt);
		io.f[0] = fDist; io.p[0] = pFound; io.p[1] = pEnt;
		break;
	}
	case kOhplA3Op_MAP_EXISTS: return gpBase->mpMapHandler->GetCurrentMap() != NULL;
	case kOhplA3Op_USE_CB_GET:
		io.p[2] = gpBase->mpMapHandler->GetCurrentMap()->GetUseItemCallback(((cLuxInventory_Item*)io.p[0])->GetName(), ((iLuxEntity*)io.p[1])->GetName());
		break;
	case kOhplA3Op_USE_CB_RUN:
	{
		cLuxUseItemCallback *pCb = (cLuxUseItemCallback*)io.p[0];
		bool bAutoDestroy = pCb->mbAutoDestroy;
		gpA3CbMap = gpBase->mpMapHandler->GetCurrentMap();
		gsA3CbName = pCb->msName;
		gpA3CbMap->RunScript(pCb->msFunction + "(\"" + pCb->msItem + "\", \"" + pCb->msEntity + "\")");
		return bAutoDestroy;
	}
	case kOhplA3Op_USE_CB_REMOVE: gpA3CbMap->RemoveUseItemCallback((cLuxUseItemCallback*)io.p[0], gsA3CbName); break;
	case kOhplA3Op_MESSAGE:
		if(io.i[0] == 0) gpBase->mpMessageHandler->SetMessage(kTranslate("Inventory","UseItemDoesNotWork"), 0);
		else gpBase->mpMessageHandler->SetMessage(kTranslate("Inventory","UseItemHasNoObject"), 0);
		break;
	case kOhplA3Op_GUI_DRAW_GFX:
		gpBase->mpGameHudSet->DrawGfx((cGuiGfxElement*)io.p[0], v, cVector2f(io.f[3], io.f[4]), cColor(io.f[5]), eGuiMaterial_Additive);
		break;

	case kOhplA3Op_IN_SMOOTH_MOUSE: return gpBase->mpInputHandler->GetSmoothMouse();
	case kOhplA3Op_IN_RESET_SMOOTH: gpBase->mpInputHandler->ResetSmoothMousePos(); break;
	case kOhplA3Op_IN_SMOOTH_POS:
	{
		cVector2f r = gpBase->mpInputHandler->GetSmoothMousePos(cVector2f(io.f[0], io.f[1]));
		io.f[0] = r.x; io.f[1] = r.y;
		break;
	}
	case kOhplA3Op_IN_INVERT_PITCH:
	{
		cLuxInputHandler *pIn = gpBase->mpInputHandler;
#ifdef USE_GAMEPAD
		if(pIn->IsGamepadPresent())
		{
#if USE_SDL2
			if(cMath::Abs(pIn->GetGamepad()->GetAxisValue(eGamepadAxis_RightY)) > 0.0f)
#else
			if(cMath::Abs(pIn->GetGamepad()->GetAxisValue(eGamepadAxis_3)) > 0.0f)
#endif
				return pIn->GetInvertGamepadLook();
			return pIn->GetInvertMouse();
		}
#endif
		return pIn->GetInvertMouse();
	}
	case kOhplA3Op_IN_ROTATE_TRIGGERED: return gpBase->mpEngine->GetInput()->IsTriggerd(eLuxAction_Rotate);

	case kOhplA3Op_LADDER_V3:
	{
		cLuxArea_Ladder *pLadder = (cLuxArea_Ladder*)io.p[0];
		if(io.i[0] == 0) io.f[0] = pLadder->GetMaxY();
		else if(io.i[0] == 1) io.f[0] = pLadder->GetMinY();
		else
		{
			cVector3f r = io.i[0] == 2 ? pLadder->GetForward() : pLadder->GetStartRotation();
			io.f[0] = r.x; io.f[1] = r.y; io.f[2] = r.z;
		}
		break;
	}
	case kOhplA3Op_LADDER_SOUND:
		gpBase->mpHelpFuncs->PlayGuiSoundData("player_ladder_" + ((cLuxArea_Ladder*)io.p[0])->GetMaterial() + "_" + gA3LadderSounds[io.i[0]], eSoundEntryType_World);
		break;

	case kOhplA3Op_DEBUG_ROW:
	{
		cGuiSet *pSet = (cGuiSet*)io.p[0];
		iFontData *pFont = (iFontData*)io.p[1];
		float fY = io.f[0];
		cVector3f vDbg(io.f[1], io.f[2], io.f[3]);
		switch(io.i[0])
		{
		case 0:
		{
			tWString sBodyName = io.p[2] ? cString::To16Char(((iPhysicsBody*)io.p[2])->GetName()) : _W("None");
			pSet->DrawFont(pFont, cVector3f(5, fY, 10), 14, cColor(1,1), _W("Focus Body: %ls Dist: %f"), sBodyName.c_str(), io.f[4]);
			break;
		}
		case 1: pSet->DrawFont(pFont, cVector3f(5, fY, 5), 12, cColor(1,1), _W("CurrentVel: %ls"), cString::To16Char(vDbg.ToString()).c_str()); break;
		case 2: pSet->DrawFont(pFont, cVector3f(5, fY, 5), 12, cColor(1,1), _W("WantedVel: %ls"), cString::To16Char(vDbg.ToString()).c_str()); break;
		case 3: pSet->DrawFont(pFont, cVector3f(5, fY, 0), 12, cColor(1,1), _W("SideSpeed: %f Force: %ls"), io.f[4], cString::To16Char(vDbg.ToString()).c_str()); break;
		case 4: pSet->DrawFont(pFont, cVector3f(5, fY, 5), 12, cColor(1,1), _W("Speed: %f"), io.f[4]); break;
		case 5: pSet->DrawFont(pFont, cVector3f(5, fY, 5), 12, cColor(1,1), _W("Angle: %f"), cMath::ToDeg(((iPhysicsJoint*)io.p[2])->GetAngle())); break;
		case 6: pSet->DrawFont(pFont, cVector3f(5, fY, 5), 12, cColor(1,1), _W("AngularVel: (%ls)"), cString::To16Char(vDbg.ToString()).c_str()); break;
		default: io.f[0] = ((iLuxProp*)io.p[2])->OnInteractDebugDraw(pSet, pFont, fY); break;
		}
		break;
	}
	case kOhplA3Op_CFG_FLOAT: io.f[0] = gpBase->mpGameCfg->GetFloat((const char*)io.p[0], (const char*)io.p[1], io.f[0]); break;
	case kOhplA3Op_VARS_ITEM_TAKE:
		io.p[0] = cLuxPlayerStateVars::mpInventoryItem;
		cLuxPlayerStateVars::mpInventoryItem = NULL;
		break;
	case kOhplA3Op_VARS_INTERACT:
		io.p[0] = cLuxPlayerStateVars::mpInteractProp;
		io.p[1] = cLuxPlayerStateVars::mpInteractBody;
		io.f[0] = cLuxPlayerStateVars::mvInteractPos.x; io.f[1] = cLuxPlayerStateVars::mvInteractPos.y; io.f[2] = cLuxPlayerStateVars::mvInteractPos.z;
		break;
	case kOhplA3Op_VARS_LADDER:
		io.p[0] = cLuxPlayerStateVars::mpLadder;
		io.f[0] = cLuxPlayerStateVars::mvLadderStartPos.x; io.f[1] = cLuxPlayerStateVars::mvLadderStartPos.y; io.f[2] = cLuxPlayerStateVars::mvLadderStartPos.z;
		break;
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

iLuxPlayerState::iLuxPlayerState(cLuxPlayer *apPlayer, eLuxPlayerState aType, OhplA3Base *apA3)
{
	mpPlayer = apPlayer;
	mpA3 = apA3;

	mType = aType;

	cGui *pGui = gpBase->mpEngine->GetGui();
	mpDefaultCrosshairGfx = pGui->CreateGfxImage("hud_crosshair_default.tga",eGuiMaterial_Alpha);
	mpSimpleInteractCrosshairGfx = pGui->CreateGfxImage("hud_crosshair_active.tga",eGuiMaterial_Alpha);
	
	mPreviousState = eLuxPlayerState_LastEnum;
}

//-----------------------------------------------------------------------

iLuxPlayerState::~iLuxPlayerState()
{
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------


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

kBeginSerializeBaseVirtual(iLuxPlayerState_SaveData)
kSerializeVar(mlPreviousState, eSerializeType_Int32)
kEndSerialize()

//-----------------------------------------------------------------------

void iLuxPlayerState::SaveToSaveData(iLuxPlayerState_SaveData* apSaveData)
{
	apSaveData->mlPreviousState = mPreviousState;
}

//-----------------------------------------------------------------------

void iLuxPlayerState::LoadFromSaveDataBeforeEnter(cLuxMap *apMap, iLuxPlayerState_SaveData* apSaveData)
{

}

//-----------------------------------------------------------------------

void iLuxPlayerState::LoadFromSaveDataAfterEnter(cLuxMap *apMap, iLuxPlayerState_SaveData* apSaveData)
{
	mPreviousState = (eLuxPlayerState)apSaveData->mlPreviousState;
}

//-----------------------------------------------------------------------


